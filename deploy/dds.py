#!/usr/bin/env python3
"""Private CycloneDDS build and dependency queries; never installs system packages."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import signal
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
THIRD_PARTY = ROOT / "ThirdParty"
CACHE = ROOT / ".deps"
RECIPE = 2
KEYS = ("CC", "CXX", "AR", "RANLIB", "CROSS_COMPILE", "SYSROOT",
        "CPPFLAGS", "CFLAGS", "CXXFLAGS", "LDFLAGS", "LDLIBS",
        "PKG_CONFIG", "PKG_CONFIG_PATH", "PKG_CONFIG_LIBDIR", "PKG_CONFIG_SYSROOT_DIR")
PLUGINS = ("appsrc", "appsink", "udpsrc", "udpsink", "rtpjitterbuffer",
           "rtph264depay", "h264parse", "rtpL16depay", "rtpL16pay",
           "mpg123audiodec", "mpegaudioparse", "audioconvert", "audioresample",
           "filesrc", "decodebin", "typefind", "wavparse", "id3demux")
OPTIONS = ("-DBUILD_SHARED_LIBS=OFF", "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
           "-DBUILD_IDLC=OFF", "-DBUILD_TESTING=OFF", "-DBUILD_EXAMPLES=OFF",
           "-DBUILD_DDSPERF=OFF", "-DENABLE_SECURITY=OFF", "-DENABLE_SSL=OFF",
           "-DENABLE_ICEORYX=OFF", "-DENABLE_ICEORYX2=OFF", "-DENABLE_LTO=OFF")
SYSTEM_LIBS = ["-pthread", "-lm", "-lrt", "-ldl"]

def run(arguments, env=None, input=None, stdout=None, stderr=None):
    """Run a build/probe and stop its whole process group on interruption."""
    process = subprocess.Popen([str(x) for x in arguments], env=env, text=True,
                               stdin=subprocess.PIPE if input is not None else None,
                               stdout=stdout, stderr=stderr, start_new_session=True)
    try:
        output, errors = process.communicate(input)
    except BaseException:
        previous = {number: signal.getsignal(number) for number in (signal.SIGINT, signal.SIGTERM)}
        try:
            for number in previous:
                signal.signal(number, signal.SIG_IGN)
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.communicate()
        finally:
            for number, handler in previous.items():
                signal.signal(number, handler)
        raise
    result = subprocess.CompletedProcess(arguments, process.returncode, output, errors)
    result.check_returncode()
    return result


def capture(command, env=None, input=None):
    try:
        result = run(command, input=input, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as error:
        raise RuntimeError(shlex.join([str(x) for x in command]) + "\n" +
                           (error.stderr or error.stdout or "").strip()) from error
    return result.stdout.strip()

def command(value):
    words = shlex.split(value)
    if not words:
        raise RuntimeError("An empty tool command was specified.")
    executable = shutil.which(words[0])
    if not executable:
        raise RuntimeError("Tool not found: " + words[0])
    return [str(Path(executable).resolve())] + words[1:]

def compiler_values(env):
    prefix = env.get("CROSS_COMPILE", "")
    cc = env.get("CC") or (prefix + "gcc" if prefix else "cc")
    if env.get("CXX"):
        return cc, env["CXX"]
    words = shlex.split(cc)
    if not words:
        raise RuntimeError("Specify a C compiler with CC.")
    executable = Path(shutil.which(words[0]) or words[0])
    if executable.exists():
        executable = executable.resolve()
    name = executable.name
    if re.fullmatch(r".*clang(?:-[0-9.]+)?", name):
        companion = name.replace("clang", "clang++", 1)
    elif re.fullmatch(r".*gcc(?:-[0-9.]+)?", name):
        companion = name.replace("gcc", "g++", 1)
    elif words[0] == "cc" and not executable.exists():
        return cc, shlex.join(["c++"] + words[1:])
    else:
        raise RuntimeError("Cannot infer a companion C++ compiler for " + words[0] + "; set CXX explicitly.")
    candidate = executable.with_name(companion)
    if not candidate.exists() and words[0] in ("cc", "gcc", "clang"):
        # Only distribution-native tools may fall back to installable native aliases.
        settings = {key: env.get(key, "") for key in KEYS}
        if not executable.exists() or not target_info(command(cc), settings)[1]:
            return cc, shlex.join([{"cc": "c++", "gcc": "g++", "clang": "clang++"}[words[0]]] + words[1:])
    cxx = shlex.join([str(candidate)] + words[1:])
    return cc, cxx

def target_info(cc, settings, force_cross=False):
    flags = shlex.split(settings["CPPFLAGS"]) + shlex.split(settings["CFLAGS"])
    if settings["SYSROOT"]:
        flags += ["--sysroot=" + settings["SYSROOT"]]
    macros = capture(cc + flags + ["-dM", "-E", "-"], input="")
    if "#define __linux__ " not in macros:
        raise RuntimeError("The selected compiler must target Linux.")
    families = (("__aarch64__", "aarch64"), ("__arm__", "arm"),
                ("__x86_64__", "x86_64"), ("__i386__", "x86"),
                ("__riscv", "riscv"), ("__powerpc64__", "ppc64"),
                ("__s390x__", "s390x"), ("__loongarch64", "loongarch64"))
    family = next((name for macro, name in families if "#define " + macro + " " in macros),
                  capture(cc + flags + ["-dumpmachine"]).split("-")[0])
    machine = platform.machine().lower()
    host = {"amd64": "x86_64", "arm64": "aarch64"}.get(machine, machine)
    if re.fullmatch(r"i[3-6]86", machine):
        host = "x86"
    elif machine.startswith("armv") or machine == "arm":
        host = "arm"
    elif machine.startswith("riscv"):
        host = "riscv"
    elif machine.startswith(("ppc64", "powerpc64")):
        host = "ppc64"
    pointer = re.search(r"#define __SIZEOF_POINTER__ (\d+)", macros)
    abi_differs = pointer and int(pointer.group(1)) != struct.calcsize("P")
    cross = bool(force_cross or settings.get("CROSS_COMPILE") or family != host or abi_differs
                 or (settings["SYSROOT"] and Path(settings["SYSROOT"]).resolve() != Path("/")))
    return family, cross

def configuration(environment=None, force_cross=False):
    env = dict(os.environ if environment is None else environment)
    prefix = env.get("CROSS_COMPILE", "")
    cc_value, cxx_value = compiler_values(env)
    cc, cxx = command(cc_value), command(cxx_value)
    settings = {key: env.get(key, "") for key in KEYS}
    settings.update(CC=shlex.join(cc), CXX=shlex.join(cxx))
    settings["PKG_CONFIG"] = shlex.join(command(env.get("PKG_CONFIG") or "pkg-config"))
    for name in ("AR", "RANLIB"):
        tool = env.get(name) or capture(cc + ["-print-prog-name=" + name.lower()])
        resolved = command(tool)
        if len(resolved) != 1:
            raise RuntimeError(name + " must name a single executable.")
        settings[name] = resolved[0]
    if settings["SYSROOT"]:
        settings["SYSROOT"] = str(Path(settings["SYSROOT"]).expanduser().resolve())
        if not Path(settings["SYSROOT"]).is_dir():
            raise RuntimeError("SYSROOT directory does not exist: " + settings["SYSROOT"])
    family, cross = target_info(cc, settings, force_cross)
    manifest = (THIRD_PARTY / "manifest.json").read_bytes()
    identity = dict(settings=settings, cross=cross, recipe=RECIPE, options=OPTIONS,
                    compiler=capture(cc + ["--version"]),
                    cxx_compiler=capture(cxx + ["--version"]),
                    target=capture(cc + shlex.split(settings["CPPFLAGS"]) + shlex.split(settings["CFLAGS"]) + ["-dumpmachine"]),
                    manifest=hashlib.sha256(manifest).hexdigest())
    digest = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()[:16]
    return dict(id=family + "-" + digest, identity=identity, settings=settings, cross=cross)

def load(build_id):
    if not build_id or "/" in build_id or "\\" in build_id or build_id in (".", ".."):
        raise RuntimeError("DDS is not prepared. Run bash deploy/deploy.sh --build-only from the SDK root.")
    base = CACHE / build_id
    if not (base / "config.json").is_file() or not (base / "complete.json").is_file():
        raise RuntimeError("DDS cache is missing or incomplete; run bash deploy/deploy.sh --build-only again.")
    config = json.loads((base / "config.json").read_text())
    if config.get("id") != build_id:
        raise RuntimeError("DDS cache configuration is invalid; re-run deployment.")
    for rel in ("prefix/lib/libddsc.a", "prefix/include/dds/dds.h",
                "prefix/lib/pkgconfig/CycloneDDS.pc"):
        if not (base / rel).is_file():
            raise RuntimeError("Missing private DDS file: " + str(base / rel) + "; re-run deployment with --rebuild-dds.")
    marker = json.loads((base / "complete.json").read_text())
    archive = base / "prefix/lib/libddsc.a"
    if marker.get("archive_sha256") != hashlib.sha256(archive.read_bytes()).hexdigest():
        raise RuntimeError("Private DDS archive is damaged; run deployment with --rebuild-dds.")
    return config

def pkg(config, arguments, private=False):
    settings = config["settings"]
    env = os.environ.copy()
    for key in ("PKG_CONFIG_PATH", "PKG_CONFIG_LIBDIR", "PKG_CONFIG_SYSROOT_DIR"):
        env.pop(key, None)
        if settings[key]:
            env[key] = settings[key]
    options = []
    if private:
        prefix = CACHE / config["id"] / "prefix"
        env.update(PKG_CONFIG_PATH="", PKG_CONFIG_LIBDIR=str(prefix / "lib/pkgconfig"),
                   PKG_CONFIG_SYSROOT_DIR="")
        options = ["--define-prefix"]
    elif config["cross"] and not settings["PKG_CONFIG_LIBDIR"] and Path(shlex.split(settings["PKG_CONFIG"])[0]).name in ("pkg-config", "pkgconf"):
        raise RuntimeError("Cross builds require target GStreamer: set PKG_CONFIG_LIBDIR and "
                           "PKG_CONFIG_SYSROOT_DIR, or provide a target PKG_CONFIG wrapper.")
    # A customer cross wrapper may overwrite PKG_CONFIG_* internally. Query private
    # DDS with the host pkg-config, so that wrapper cannot select a system DDS.
    executable = command("pkg-config" if private else (settings["PKG_CONFIG"] or "pkg-config"))
    result = capture(executable + options + arguments, env=env)
    if config["cross"] and not private and "--libs" in arguments:
        extra = ["-Wl,-rpath-link," + token[2:] for token in shlex.split(result)
                 if token.startswith("-L")]
        result = shlex.join(shlex.split(result) + extra)
    return result

def validate_gstreamer(config):
    pkg(config, ["--exists", "gstreamer-1.0 >= 1.20", "gstreamer-app-1.0 >= 1.20"])

def query(config, kind):
    if kind == "dds-cflags":
        return pkg(config, ["--cflags", "CycloneDDS"], private=True)
    if kind == "dds-libs":
        return shlex.quote(str(CACHE / config["id"] / "prefix/lib/libddsc.a"))
    if kind == "gst-cflags":
        return pkg(config, ["--cflags", "gstreamer-app-1.0"])
    if kind == "gst-libs":
        return pkg(config, ["--libs", "gstreamer-app-1.0"])
    if kind == "cflags":
        return query(config, "dds-cflags") + " " + query(config, "gst-cflags")
    if kind == "libs":
        return " ".join((query(config, "dds-libs"), query(config, "gst-libs"),
                         shlex.join(SYSTEM_LIBS)))
    raise RuntimeError("Unknown query: " + kind)

def verify_sources():
    manifest = json.loads((THIRD_PARTY / "manifest.json").read_text())
    checksums = THIRD_PARTY / "SHA256SUMS"
    if hashlib.sha256(checksums.read_bytes()).hexdigest() != manifest["checksums_sha256"]:
        raise RuntimeError("ThirdParty/SHA256SUMS does not match the manifest.")
    names = set()
    for line in checksums.read_text().splitlines():
        expected, name = line.split("  ", 1)
        names.add(name)
        path = THIRD_PARTY / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise RuntimeError("DDS source checksum mismatch: " + name)
    source = THIRD_PARTY / manifest["directory"]
    actual = {str(path.relative_to(THIRD_PARTY)) for path in source.rglob("*") if path.is_file()}
    if actual != names:
        raise RuntimeError("DDS source file list differs from ThirdParty/SHA256SUMS.")
    return source

def atomic_text(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    try:
        with temporary.open("w", encoding="utf-8", newline="\n") as output:
            output.write(content)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)

def make_value(value):
    if "\n" in value or "\r" in value:
        raise RuntimeError("Multiline toolchain values are not supported.")
    return value.replace("$", "$$").replace("#", "\\#")

def write_settings(config):
    base = CACHE / config["id"]
    atomic_text(base / "config.json", json.dumps(config, indent=2) + "\n")
    lines = ["# Generated by deploy/deploy.sh; do not edit.",
             "DDS_CROSS := " + str(int(config["cross"]))]
    lines += ["DDS_DEFAULT_" + key + " := " + make_value(value)
              for key, value in config["settings"].items()]
    atomic_text(base / "config.mk", "\n".join(lines) + "\n")

def build(config, jobs, rebuild=False):
    source = verify_sources()
    base = CACHE / config["id"]
    base.mkdir(parents=True, exist_ok=True)
    import fcntl
    with (CACHE / (config["id"] + ".lock")).open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if not rebuild and (base / "complete.json").is_file():
            write_settings(config)
            load(config["id"])
            print("Reusing private DDS: " + config["id"], flush=True)
            return
        (base / "complete.json").unlink(missing_ok=True)
        if rebuild:
            for name in ("cmake", "prefix"):
                if (base / name).exists():
                    shutil.rmtree(base / name)
        work, prefix = base / "cmake", base / "prefix"
        cache = work / "CMakeCache.txt"
        if cache.exists() and "CMAKE_CACHEFILE_DIR:INTERNAL=" + str(work) not in cache.read_text():
            shutil.rmtree(work)
        settings = config["settings"]
        cmake = (["set(CMAKE_SYSTEM_NAME Linux)",
                  "set(CMAKE_SYSTEM_PROCESSOR " + config["id"].split("-")[0] + ")"]
                 if config["cross"] else [])
        def cmake_string(value):
            return '"' + value.replace("\\", "\\\\").replace('"', '\\"').replace(";", "\\;") + '"'
        for lang, key in (("C", "CC"), ("CXX", "CXX")):
            words = shlex.split(settings[key])
            cmake.append("set(CMAKE_" + lang + "_COMPILER " + cmake_string(words[0]) + ")")
            if len(words) > 1:
                cmake.append("set(CMAKE_" + lang + "_COMPILER_ARG1 " +
                             cmake_string(shlex.join(words[1:])) + ")")
        for key in ("AR", "RANLIB"):
            cmake.append("set(CMAKE_" + key + " " + cmake_string(settings[key]) + ")")
        if settings["SYSROOT"]:
            cmake.append("set(CMAKE_SYSROOT " + cmake_string(settings["SYSROOT"]) + ")")
        if config["cross"]:
            cmake += ["set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)",
                      "set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)",
                      "set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)",
                      "set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)"]
        atomic_text(base / "toolchain.cmake", "\n".join(cmake) + "\n")
        commands = [
            ["cmake", "-S", str(source), "-B", str(work), "-G", "Unix Makefiles",
             "-DCMAKE_TOOLCHAIN_FILE=" + str(base / "toolchain.cmake"),
             "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=" + str(prefix),
             "-DCMAKE_INSTALL_LIBDIR=lib",
             "-DCMAKE_C_FLAGS=" + settings["CPPFLAGS"] + " " + settings["CFLAGS"],
             "-DCMAKE_CXX_FLAGS=" + settings["CPPFLAGS"] + " " + settings["CXXFLAGS"],
             "-DCMAKE_EXE_LINKER_FLAGS=" + settings["LDFLAGS"]] + list(OPTIONS),
            ["cmake", "--build", str(work), "--parallel", str(jobs)],
            ["cmake", "--install", str(work)]]
        env = os.environ.copy()
        # CMake receives explicit toolchain and flags above.
        for key in ("CC", "CXX", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS"):
            env.pop(key, None)
        print("Building private DDS: " + config["id"], flush=True)
        log_path = base / "build.log"
        with log_path.open("w") as log:
            for args in commands:
                log.write("$ " + shlex.join(args) + "\n")
                log.flush()
                try:
                    run(args, stdout=log, stderr=subprocess.STDOUT, env=env)
                except subprocess.CalledProcessError as error:
                    raise RuntimeError("DDS build failed; see " + str(log_path)) from error
        write_settings(config)
        for rel in ("lib/libddsc.a", "include/dds/dds.h", "lib/pkgconfig/CycloneDDS.pc"):
            if not (prefix / rel).is_file():
                raise RuntimeError("DDS install did not produce " + rel)
        atomic_text(base / "complete.json", json.dumps({"recipe": RECIPE, "archive_sha256": hashlib.sha256((prefix / "lib/libddsc.a").read_bytes()).hexdigest()}) + "\n")

def link_probe(config):
    validate_gstreamer(config)
    base = CACHE / config["id"]
    source = base / "link_probe.c"
    source.write_text("#include <dds/dds.h>\n#include <gst/gst.h>\n"
                      "int main(void) { gst_init(0,0); dds_entity_t p=dds_create_participant("
                      "DDS_DOMAIN_DEFAULT,0,0); if(p>0) dds_delete(p); return p<0; }\n")
    settings = config["settings"]
    args = shlex.split(settings["CC"]) + shlex.split(settings["CPPFLAGS"])
    args += shlex.split(settings["CFLAGS"])
    if settings["SYSROOT"]:
        args += ["--sysroot=" + settings["SYSROOT"]]
    args += shlex.split(query(config, "cflags")) + [str(source), "-o", str(base / "link_probe")]
    args += shlex.split(settings["LDFLAGS"]) + shlex.split(query(config, "libs"))
    args += shlex.split(settings["LDLIBS"])
    capture(args)

def check(config):
    env = {key: os.environ.get("AHEADFORM_" + key, config["settings"][key]) for key in KEYS}
    actual = configuration(env, force_cross=config["cross"])
    if actual["id"] != config["id"]:
        raise RuntimeError("Toolchain/flags differ from the prepared DDS. Re-run deploy/deploy.sh "
                           "with the desired environment before building.")
    validate_gstreamer(config)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    query_parser = sub.add_parser("query")
    query_parser.add_argument("kind", choices=("dds-cflags", "dds-libs", "gst-cflags",
                                             "gst-libs", "cflags", "libs"))
    query_parser.add_argument("--id", required=True)
    check_parser = sub.add_parser("check")
    check_parser.add_argument("--id", required=True)
    sub.add_parser("verify-sources")
    args = parser.parse_args()
    if args.action == "verify-sources":
        verify_sources()
        print("DDS source checksums verified.")
    elif args.action == "query":
        print(query(load(args.id), args.kind))
    else:
        check(load(args.id))

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.SubprocessError) as error:
        print("Dependency error: " + str(error), file=sys.stderr)
        sys.exit(1)
