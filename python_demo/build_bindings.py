"""Generate CFFI C sources and read Python build metadata; Make owns compilation."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import sysconfig
import tempfile

ROOT = Path(__file__).resolve().parent


def write_if_changed(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file() and path.read_bytes() == data:
        return
    fd, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=str(path.parent))
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def cffi_version():
    import cffi
    if sys.version_info < (3, 8):
        raise ValueError("Python 3.8 or newer is required")
    expected = "1.17.1" if sys.version_info < (3, 10) else "2.1.1"
    if cffi.__version__ != expected:
        raise ValueError("Install requirements.txt in a private venv; expected CFFI " + expected)
    return cffi.__version__


def config_value(command, option):
    value = subprocess.check_output(command + [option], text=True).strip()
    if not value or "\n" in value or "\r" in value:
        if option != "--ldflags" or value:
            raise ValueError("Invalid target PYTHON_CONFIG response for " + option)
    return value


def configure(args):
    version = cffi_version()
    if args.cross and not args.python_config:
        raise ValueError("Cross compilation requires explicit target PYTHON_CONFIG; host Python headers must not be used")
    if args.python_config:
        command = shlex.split(args.python_config)
        includes = config_value(command, "--includes")
        ldflags = config_value(command, "--ldflags")
        suffix = config_value(command, "--extension-suffix")
    else:
        paths = dict.fromkeys(sysconfig.get_path(name) for name in ("include", "platinclude"))
        includes = shlex.join("-I" + path for path in paths if path)
        # Interpreter LDFLAGS can contain its own -O2/-g/-fwrapv; do not copy them.
        libdir = sysconfig.get_config_var("LIBDIR")
        ldflags = " ".join(filter(None, (shlex.quote("-L" + libdir) if libdir else "",
                                        sysconfig.get_config_var("LIBS"), sysconfig.get_config_var("SYSLIBS"))))
        suffix = sysconfig.get_config_var("EXT_SUFFIX")
    if not suffix or not re.fullmatch(r"\.[A-Za-z0-9_.-]+\.so", suffix):
        raise ValueError("PYTHON_CONFIG must return a target CPython extension suffix, e.g. .cpython-310-aarch64-linux-gnu.so")
    if not any(flag.startswith("-I") for flag in shlex.split(includes)):
        raise ValueError("Python development headers are missing from --includes")
    for value in (includes, ldflags, suffix):
        if any(char in value for char in ("\n", "\r", "#", "$")):
            raise ValueError("Unsupported character in Python build configuration")
    data = dict(cffi=version, includes=includes, ldflags=ldflags, suffix=suffix,
                generator_python=sys.version.split()[0], python_config=args.python_config,
                cross=args.cross)
    digest = hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()[:12]
    abi = suffix[1:-3] + "-" + digest
    data["abi"] = abi
    name = ROOT / ".build" / ("python-" + digest + ".mk")
    content = "# Generated Python development configuration; do not edit.\n"
    content += "python_abi := {}\npython_includes := {}\npython_ldflags := {}\npython_suffix := {}\n".format(abi, includes, ldflags, suffix)
    write_if_changed(name, content.encode())
    write_if_changed(name.with_suffix(".json"), (json.dumps(data, indent=2) + "\n").encode())
    print(name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    config = sub.add_parser("configure")
    config.add_argument("--cross", type=int, default=0)
    config.add_argument("--python-config", default="")
    generate = sub.add_parser("generate")
    generate.add_argument("module", choices=("api", "user"))
    generate.add_argument("output", type=Path)
    stamp = sub.add_parser("stamp")
    stamp.add_argument("output", type=Path)
    activate = sub.add_parser("activate")
    activate.add_argument("config", type=Path)
    activate.add_argument("directory", type=Path)
    args = parser.parse_args()
    if args.action == "configure":
        configure(args)
    elif args.action == "generate":
        cffi_version()
        from bindings import api_builder, user_builder
        builder = api_builder() if args.module == "api" else user_builder()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        builder.emit_c_code(str(args.output))
    elif args.action == "stamp":
        # Exported base toolchain and actual flags both affect compiled objects.
        # Make can add insignificant whitespace depending on a variable's origin.
        # Compare flag arguments while preserving spaces inside quoted arguments.
        settings = {key: shlex.split(value) if key.startswith("AF_BUILD_") else value
                    for key, value in os.environ.items()
                    if key.startswith(("AHEADFORM_", "AF_BUILD_"))}
        write_if_changed(args.output, (json.dumps(settings, sort_keys=True, indent=2) + "\n").encode())
    elif args.action == "activate":
        data = args.config.with_suffix(".json").read_bytes()
        write_if_changed(args.directory / "build-info.json", data)
        write_if_changed(ROOT / ".build" / "active", (args.directory.name + "\n").encode())


if __name__ == "__main__":
    try:
        main()
    except (ImportError, OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit("Python binding build: " + str(error))
