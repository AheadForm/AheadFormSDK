#!/usr/bin/env python3
"""Prepare private DDS/Python dependencies and save settings for manual SDK builds."""
import argparse
from contextlib import contextmanager
import fcntl
import ipaddress
import json
import os
from pathlib import Path
import re
import select
import signal
import shlex
import shutil
import subprocess
import sys
import termios
import time
import dds
import python_env

PACKAGES = {
    "apt": {
        "tools": ["build-essential", "cmake", "pkg-config", "findutils"],
        "clang": ["clang"],
        "python": ["python3-dev", "python3-venv", "libffi-dev"],
        "network": ["iproute2", "iputils-ping"],
        "media": ["libgstreamer1.0-dev", "libgstreamer-plugins-base1.0-dev",
                  "gstreamer1.0-tools", "gstreamer1.0-plugins-base",
                  "gstreamer1.0-plugins-good", "gstreamer1.0-plugins-bad"]},
    "dnf": {
        "tools": ["gcc", "gcc-c++", "make", "cmake", "pkgconf-pkg-config", "findutils"],
        "clang": ["clang"],
        "python": ["python3-devel", "python3-pip", "libffi-devel"],
        "network": ["iproute", "iputils"],
        "media": ["gstreamer1-devel", "gstreamer1-plugins-base-devel", "gstreamer1",
                  "gstreamer1-plugins-base", "gstreamer1-plugins-good",
                  "gstreamer1-plugins-good-extras", "gstreamer1-plugins-bad-free"]},
    "zypper": {
        "tools": ["gcc", "gcc-c++", "make", "cmake", "pkgconf-pkg-config", "findutils"],
        "clang": ["clang"],
        "python": ["python3-devel", "python3-pip", "libffi-devel"],
        "network": ["iproute2", "iputils"],
        "media": ["gstreamer-devel", "gstreamer-plugins-base-devel", "gstreamer-utils",
                  "gstreamer-plugins-base", "gstreamer-plugins-good", "gstreamer-plugins-bad"]}
}

PROMPT_TIMEOUT = 120.0
NATIVE_TOOLS = {"cc", "c++", "gcc", "g++", "clang", "clang++", "ar", "ranlib", "pkg-config", "pkgconf"}
TARGET_SETTINGS = ("SYSROOT", "PKG_CONFIG", "PKG_CONFIG_PATH", "PKG_CONFIG_LIBDIR", "PKG_CONFIG_SYSROOT_DIR")

class DeploymentInterrupted(Exception):
    def __init__(self, signum=signal.SIGINT):
        self.signum = signum

def interrupt_deployment(signum, frame):
    raise DeploymentInterrupted(signum)

@contextmanager
def deployment_lock():
    dds.CACHE.mkdir(parents=True, exist_ok=True)
    with (dds.CACHE / "deploy.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise RuntimeError("Another deployment is running for this SDK directory.") from error
        yield

def saved_settings():
    path = dds.CACHE / "deploy-settings.json"
    if path.is_file():
        data = json.loads(path.read_text(encoding="utf-8"))
        if data.get("version") != 1:
            raise RuntimeError("Unsupported saved deployment settings: " + str(path))
    else:
        active = dds.CACHE / "active.mk"
        if not active.is_file():
            return {}
        match = re.search(r"^DDS_BUILD_ID\s*:?=\s*([A-Za-z0-9_-]+)\s*$",
                          active.read_text(encoding="utf-8"), re.MULTILINE)
        if not match:
            raise RuntimeError("Invalid active DDS selection: " + str(active))
        path = dds.CACHE / match.group(1) / "config.json"
        data = json.loads(path.read_text(encoding="utf-8"))
    settings = data.get("settings")
    if not isinstance(settings, dict) or any(not isinstance(settings.get(key, ""), str) for key in dds.KEYS):
        raise RuntimeError("Invalid saved deployment settings: " + str(path))
    return {key: settings.get(key, "") for key in dds.KEYS}

def available_command(value):
    words = shlex.split(value)
    return bool(words and shutil.which(words[0]))

def validate_tool(value, allow_install=False):
    words = shlex.split(value)
    if not words:
        raise ValueError("必须指定工具命令。")
    if not shutil.which(words[0]) and not (allow_install and words[0] in NATIVE_TOOLS):
        raise ValueError("找不到工具，请填写已安装的命令或完整路径：" + words[0])

def validate_directory(value):
    if value and not Path(value).expanduser().is_dir():
        raise ValueError("目录不存在：" + value)

def validate_pc_directories(value):
    if not value:
        raise ValueError("交叉编译必须指定目标 GStreamer 的 .pc 目录，不能使用主机默认目录。")
    for directory in value.split(":"):
        if not directory or not Path(directory).is_dir():
            raise ValueError("无效的 .pc 目录：" + directory)

def prompt_value(name, description, current, validator, optional=False):
    while True:
        print("\n" + description + "（" + name + "）")
        print("当前值：" + (current if current else "未设置"))
        clearing = "；输入 - 清空" if optional else ""
        print("请输入新值；回车或等待 2 分钟保留当前值" + clearing + "：", end="", flush=True)
        fd = sys.stdin.fileno()
        deadline = time.monotonic() + PROMPT_TIMEOUT
        while True:
            try:
                ready, _, _ = select.select([fd], [], [], max(0, deadline - time.monotonic()))
                break
            except InterruptedError:
                if time.monotonic() >= deadline:
                    ready = []
                    break
        timed_out = not ready
        if timed_out:
            termios.tcflush(fd, termios.TCIFLUSH)
            print("\n等待超时。")
            text = ""
        else:
            data = os.read(fd, 65536)
            if not data:
                raise DeploymentInterrupted()
            text = data.decode(sys.stdin.encoding or "utf-8").strip()
        value = current if not text else ("" if text == "-" and optional else text)
        try:
            if not value and not optional:
                raise ValueError("没有可用默认值，必须指定此项。")
            validator(value)
        except (RuntimeError, OSError, ValueError, subprocess.SubprocessError) as error:
            print("参数无效：" + str(error), flush=True)
            if timed_out:
                raise RuntimeError("No valid default for " + name + "; specify it and run deployment again.") from error
            continue
        print("采用：" + (value if value else "未设置"), flush=True)
        return value

def compiler_identity(value):
    try:
        return shlex.join(dds.command(value))
    except (RuntimeError, OSError, ValueError):
        return value

def apply_environment(settings):
    for key in dds.KEYS:
        os.environ[key] = settings.get(key, "")

def configured_inputs(installer):
    """Let GNU Make evaluate explicit deps.mk overrides with its usual precedence."""
    if not shutil.which("make"):
        installer.install(["tools"], "GNU Make is required to read deploy/deps.mk.")
    environment = os.environ.copy()
    for key in ("MAKEFLAGS", "MFLAGS", "GNUMAKEFLAGS", "MAKELEVEL", "MAKEOVERRIDES"):
        environment.pop(key, None)
    script = ("import json,os; print(json.dumps({key:os.environ['AHEADFORM_'+key] "
              "for key in os.environ.get('AHEADFORM_CONFIG_INPUTS','').split()}))")
    makefile = (".PHONY: _aheadform_settings\n_aheadform_settings:\n\t@" +
                shlex.join([sys.executable, "-c", script]) + "\n")
    return json.loads(dds.capture([
        "make", "--silent", "--no-print-directory", "-f", str(dds.ROOT / "deploy/deps.mk"),
        "-f", "-", "SDK_ROOT=" + str(dds.ROOT), "_AHEADFORM_CONFIG_ONLY=1",
        "_aheadform_settings"], env=environment, input=makefile))

def prepare_environment(args, installer):
    previous = saved_settings()
    explicit = configured_inputs(installer)
    settings = dict(previous)
    settings.update(explicit)
    if "CROSS_COMPILE" in explicit and "CC" not in explicit:
        settings["CC"] = explicit["CROSS_COMPILE"] + "gcc" if explicit["CROSS_COMPILE"] else "cc"
    current = settings.get("CC") or "cc"
    target_changed = any(settings.get(key, "") != previous.get(key, "")
                         for key in ("CROSS_COMPILE", "SYSROOT", "CPPFLAGS", "CFLAGS", "CXXFLAGS"))
    interactive = not args.non_interactive and sys.stdin.isatty() and sys.stdout.isatty()

    def validate_cc(value):
        validate_tool(value, allow_install=True)
        if available_command(value):
            probe = {key: settings.get(key, "") for key in dds.KEYS}
            if target_changed or compiler_identity(value) != compiler_identity(previous.get("CC", "")):
                for key in ("SYSROOT", "CPPFLAGS", "CFLAGS"):
                    probe[key] = explicit.get(key, "")
            dds.target_info(dds.command(value), probe)

    cc = prompt_value("CC", "选择 C 编译器；配套工具将自动确定", current, validate_cc) if interactive else current
    validate_cc(cc)
    changed = bool(previous and (
        compiler_identity(cc) != compiler_identity(previous.get("CC", "")) or
        target_changed))
    if changed:
        for key in ("CXX", "AR", "RANLIB", "CPPFLAGS", "CFLAGS", "CXXFLAGS", "LDFLAGS", "LDLIBS") + TARGET_SETTINGS:
            if key not in explicit:
                settings.pop(key, None)
    # An edited CC supersedes a previous prefix; explicit CROSS_COMPILE remains authoritative.
    if compiler_identity(cc) != compiler_identity(previous.get("CC", "")) and "CROSS_COMPILE" not in explicit:
        settings.pop("CROSS_COMPILE", None)
    settings["CC"] = cc
    settings["PKG_CONFIG"] = settings.get("PKG_CONFIG") or "pkg-config"
    try:
        _, cxx = dds.compiler_values(settings)
    except RuntimeError:
        cxx = ""
    if not cxx or not available_command(cxx):
        try:
            validate_tool(cxx, allow_install=True)
        except (ValueError, RuntimeError):
            if not interactive:
                raise RuntimeError("Cannot determine a usable companion C++ compiler. Set CXX explicitly.")
            cxx = prompt_value("CXX", "无法自动确定配套 C++ 编译器，请指定同一工具链的 C++ 编译器",
                               cxx, lambda value: validate_tool(value, allow_install=True))
    settings["CXX"] = cxx
    apply_environment(settings)
    ensure_build_tools(installer, target_pkg=False)

    for key in ("AR", "RANLIB"):
        tool = settings.get(key, "")
        if not tool:
            try:
                tool = dds.capture(dds.command(cc) + ["-print-prog-name=" + key.lower()])
            except RuntimeError:
                tool = ""
        if not tool or not available_command(tool):
            if not interactive:
                raise RuntimeError("Cannot determine " + key + ". Set it explicitly using this toolchain.")
            tool = prompt_value(key, "无法自动确定配套静态库工具，请填写工具路径", tool, validate_tool)
        validate_tool(tool)
        if len(shlex.split(tool)) != 1:
            raise RuntimeError(key + " must name a single executable.")
        settings[key] = tool

    probe = {key: settings.get(key, "") for key in dds.KEYS}
    cross = dds.target_info(dds.command(cc), probe, args.cross)[1]
    if cross and interactive:
        old_sysroot = settings.get("SYSROOT", "")
        settings["SYSROOT"] = prompt_value(
            "SYSROOT", "目标系统根目录；未设置时沿用编译器自己的默认目录",
            old_sysroot, validate_directory, optional=True)
        if settings["SYSROOT"] != old_sysroot:
            for key in TARGET_SETTINGS[1:]:
                if key not in explicit:
                    settings.pop(key, None)
        settings["PKG_CONFIG"] = prompt_value(
            "PKG_CONFIG", "目标 GStreamer 的 pkg-config 命令或客户包装脚本",
            settings.get("PKG_CONFIG") or "pkg-config", validate_tool)
        name = Path(shlex.split(settings["PKG_CONFIG"])[0]).name
        if name in ("pkg-config", "pkgconf"):
            settings["PKG_CONFIG_LIBDIR"] = prompt_value(
                "PKG_CONFIG_LIBDIR", "目标 GStreamer 的 .pc 目录；多个目录用冒号分隔",
                settings.get("PKG_CONFIG_LIBDIR", ""), validate_pc_directories)
            settings["PKG_CONFIG_SYSROOT_DIR"] = prompt_value(
                "PKG_CONFIG_SYSROOT_DIR", "查询目标 GStreamer 时使用的 sysroot",
                settings.get("PKG_CONFIG_SYSROOT_DIR") or settings["SYSROOT"],
                validate_directory, optional=True)
    apply_environment(settings)
    args.cross = args.cross or cross

def run_package_command(arguments):
    # Keep package transactions outside the terminal foreground process group.
    # Only the deploy process receives Ctrl+C and defers it until this transaction ends.
    pending = []
    def defer(signum, frame):
        if not pending:
            print("\n已收到中断请求；等待当前系统包事务结束后退出，请勿强制终止。", flush=True)
        pending.append(signum)
    signals = (signal.SIGINT, signal.SIGTERM)
    previous = {signum: signal.getsignal(signum) for signum in signals}
    process = None
    try:
        for signum in signals:
            signal.signal(signum, defer)
        process = subprocess.Popen(arguments, stdin=subprocess.DEVNULL, preexec_fn=os.setpgrp)
        result = process.wait()
    finally:
        for signum, handler in previous.items():
            signal.signal(signum, handler)
    if pending:
        raise DeploymentInterrupted(pending[0])
    if result:
        raise subprocess.CalledProcessError(result, arguments)


def os_info(path=Path("/etc/os-release")):
    result = {}
    if path.is_file():
        for line in path.read_text().splitlines():
            if "=" in line and not line.lstrip().startswith("#"):
                key, value = line.split("=", 1)
                words = shlex.split(value)
                result[key] = words[0] if words else ""
    return result

def package_family(info, which=shutil.which):
    names = (info.get("ID", "") + " " + info.get("ID_LIKE", "")).split()
    for identities, executable, family in (
            (("debian", "ubuntu", "linuxmint"), "apt-get", "apt"),
            (("fedora", "rhel", "centos", "rocky", "almalinux"), "dnf", "dnf"),
            (("suse", "opensuse", "opensuse-leap", "opensuse-tumbleweed", "sles"), "zypper", "zypper")):
        if any(name in identities for name in names) and which(executable):
            return family
    return None

class Installer:
    def __init__(self, skip):
        self.skip = skip
        self.family = package_family(os_info())
        self.refreshed = False
        self.authorized = False

    def install(self, groups, reason):
        if self.skip:
            raise RuntimeError(reason + "\n--skip-packages was specified; prepare the missing dependencies.")
        if not self.family:
            raise RuntimeError(reason + "\nAutomatic installation supports APT, DNF and Zypper families. "
                               "On Arch/other systems prepare dependencies manually; no partial system upgrade is performed.")
        names = sorted({name for group in groups for name in PACKAGES[self.family][group]})
        privilege = [] if os.geteuid() == 0 else ["sudo"]
        if privilege and not shutil.which("sudo"):
            raise RuntimeError(reason + "\nsudo is unavailable; install these packages as administrator: " + " ".join(names))
        if privilege and not self.authorized:
            subprocess.run(privilege + ["-v"], check=True)
            self.authorized = True
        privilege = privilege + ["-n"] if privilege else []
        print("Installing/updating required packages: " + " ".join(names), flush=True)
        if not self.refreshed:
            refresh = {"apt": ["apt-get", "update"], "dnf": ["dnf", "makecache"],
                       "zypper": ["zypper", "--non-interactive", "refresh"]}[self.family]
            run_package_command(privilege + refresh)
            self.refreshed = True
        if self.family == "apt":
            args = ["env", "DEBIAN_FRONTEND=noninteractive", "apt-get", "install", "-y", "--no-remove", "--no-install-recommends"] + names
        elif self.family == "dnf":
            args = ["dnf", "install", "-y", "--setopt=install_weak_deps=False"] + names
        else:
            args = ["zypper", "--non-interactive", "install", "--no-recommends"] + names
        try:
            run_package_command(privilege + args)
        except subprocess.CalledProcessError as error:
            recovery = {"apt": "sudo dpkg --audit; if packages are interrupted, sudo dpkg --configure -a",
                        "dnf": "sudo dnf check", "zypper": "sudo zypper verify"}[self.family]
            raise RuntimeError("System package installation failed. Check the package-manager output; "
                               "do not remove lock files. Recovery/check: " + recovery) from error

def missing_tools(target_pkg=True):
    commands = ("make", "cmake", "find", "pkg-config")
    if target_pkg:
        commands += (os.environ.get("PKG_CONFIG") or "pkg-config",)
    result = [name for name in commands
              if not shutil.which(shlex.split(name)[0])]
    cc, cxx = dds.compiler_values(os.environ)
    result += [name for name in (cc, cxx) if not shutil.which(shlex.split(name)[0])]
    if shutil.which("cmake"):
        version = dds.capture(["cmake", "--version"]).splitlines()[0]
        match = re.search(r"(\d+)\.(\d+)", version)
        if not match or tuple(map(int, match.groups())) < (3, 16):
            result.append("cmake >= 3.16")
    return result

def missing_plugins():
    if not shutil.which("gst-inspect-1.0"):
        return list(dds.PLUGINS)
    return [name for name in dds.PLUGINS if subprocess.run(
        ["gst-inspect-1.0", name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode]

def ensure_build_tools(installer, target_pkg=True):
    tools = missing_tools(target_pkg)
    if tools:
        native = {"cc", "c++", "gcc", "g++", "clang", "clang++", "make", "cmake",
                  "pkg-config", "pkgconf", "find", "cmake >= 3.16"}
        unavailable = [name for name in tools if shlex.split(name)[0] not in native]
        if unavailable:
            raise RuntimeError("Customer tool not found: " + ", ".join(unavailable) +
                               ". Install/configure this toolchain before deployment.")
        groups = ["tools"] + (["clang"] if any("clang" in name for name in tools) else [])
        installer.install(groups, "Missing build tools: " + ", ".join(tools))
        tools = missing_tools(target_pkg)
        if tools:
            raise RuntimeError("Selected toolchain is unavailable: " + ", ".join(tools) +
                               ". Vendor/cross toolchains must be supplied by the customer.")

def ensure_dependencies(installer, args):
    build_only, force_cross = args.build_only, args.cross
    ensure_build_tools(installer)
    config = dds.configuration(force_cross=force_cross)
    if not build_only and not config["cross"]:
        validate_robot(args)
    if config["cross"]:
        dds.validate_gstreamer(config)
        print("Cross build: using target GStreamer; host runtime/plugin checks are skipped.", flush=True)
        return config
    try:
        dds.validate_gstreamer(config)
        media_error = ""
    except RuntimeError as error:
        media_error = str(error)
    plugins = missing_plugins()
    if media_error or plugins:
        installer.install(["media"], "GStreamer Core/App >= 1.20 or required plugins are missing.\n" +
                          (media_error or ", ".join(plugins)))
        dds.validate_gstreamer(config)
        plugins = missing_plugins()
        if plugins:
            raise RuntimeError("Required plugins remain unavailable from configured repositories: " +
                               ", ".join(plugins) + ". No third-party repository was enabled.")
    if not build_only and args.robot is not None and any(not shutil.which(name) for name in ("ip", "ping")):
        installer.install(["network"], "ip and ping are required for robot network checks.")
    return config

def update_config_text(text, updates):
    """Change specified INI values only, preserving comments and other fields."""
    section = ""
    remaining = dict(updates)
    lines = text.splitlines(keepends=True)
    for index, line in enumerate(lines):
        heading = re.match(r"\s*\[([^]]+)\]", line)
        if heading:
            section = heading.group(1).strip().lower()
            continue
        match = re.match(r"([ \t]*([^=;#\s]+)[ \t]*=[ \t]*)([^;#\r\n]*?)([ \t]*(?:[;#][^\r\n]*)?)(\r?\n)?$", line)
        if match:
            key = (section, match.group(2))
            if key in remaining:
                lines[index] = match.group(1) + str(remaining.pop(key)) + match.group(4) + (match.group(5) or "")
    if remaining:
        raise RuntimeError("Configuration fields not found: " + ", ".join(
            section + "." + key for section, key in remaining))
    return "".join(lines)

def validate_robot(args):
    if args.robot is None:
        return None
    try:
        robot = ipaddress.IPv4Address(args.robot)
    except ipaddress.AddressValueError as error:
        raise RuntimeError("Invalid robot IPv4: " + args.robot) from error
    if robot.is_multicast or robot.is_loopback or robot.is_unspecified or int(robot) == 0xffffffff:
        raise RuntimeError("Robot IPv4 must be a reachable unicast address.")
    return robot

def configure_robot(args):
    updates = {}
    if args.domain is not None:
        updates[("dds", "domain_id")] = args.domain
    if args.interface is not None:
        updates[("dds", "strNetworkInterface")] = args.interface
    robot = validate_robot(args)
    if robot is not None:
        route = json.loads(dds.capture(["ip", "-4", "-j", "route", "get", str(robot)]))[0]
        interface = args.interface if args.interface is not None else route["dev"]
        if interface != route["dev"] or interface == "lo" or Path("/sys/class/net", interface, "wireless").exists():
            raise RuntimeError("Use the wired interface routing to the robot: " + route["dev"])
        subprocess.run(["ping", "-I", interface, "-c", "1", "-W", "3", str(robot)],
                       check=True, stdout=subprocess.DEVNULL)
        updates[("dds", "strNetworkInterface")] = interface
        updates[("media", "strRobotIPv4")] = str(robot)
    elif args.interface is not None:
        interface = Path("/sys/class/net", args.interface)
        if (not args.interface or "/" in args.interface or args.interface == "lo" or
                not interface.is_dir() or (interface / "wireless").exists()):
            raise RuntimeError("Use an existing wired network interface: " + args.interface)
    if not updates:
        return
    path = dds.ROOT / "CodeProject/AheadFormSDK_Config.ini"
    original = path.read_bytes()
    updated = update_config_text(original.decode("utf-8"), updates)
    if updated.encode("utf-8") == original:
        print("Configuration is unchanged; no backup/write is needed.", flush=True)
        return
    import datetime
    backup = path.with_name(path.name + ".bak." +
                            datetime.datetime.now().strftime("%Y%m%d%H%M%S") + "." + str(os.getpid()))
    shutil.copy2(path, backup)
    temporary = path.with_name(path.name + ".tmp")
    try:
        temporary.write_bytes(updated.encode("utf-8"))
        shutil.copymode(path, temporary)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)
    print("Configuration backup: " + str(backup), flush=True)

def parser():
    result = argparse.ArgumentParser(
        description="Prepare private CycloneDDS and optional Python/CFFI dependencies; save settings for manual SDK builds.",
        epilog="Robot options are optional; without them, robot connectivity and configuration are unchanged. "
               "Review CodeProject/AheadFormSDK_Config.ini before running. "
               "Select compilers with CC/CXX or CROSS_COMPILE; cross builds need target GStreamer.")
    result.add_argument("--robot", help="optional robot IPv4; check connectivity and save its IP and routed wired interface")
    result.add_argument("--domain", type=int, help="optional external DDS domain ID (0..232); keep the INI value when omitted")
    result.add_argument("--interface", help="optional wired interface; inferred with --robot, otherwise keep the INI value")
    result.add_argument("--skip-packages", action="store_true", help="never invoke system package managers; pip may still use the network unless --python-wheelhouse is set")
    result.add_argument("--skip-python", action="store_true", help="prepare C/C++ dependencies only; skip the private Python/CFFI environment")
    result.add_argument("--python-wheelhouse", metavar="DIR", help="install Python requirements only from this local wheel/package directory (no package index)")
    result.add_argument("--build-only", action="store_true", help="skip robot network checks and configuration changes")
    result.add_argument("--non-interactive", action="store_true", help="use current/default toolchain settings without prompting")
    result.add_argument("--cross", action="store_true", help="force cross mode (automatically implies build-only)")
    result.add_argument("--rebuild-dds", action="store_true", help="rebuild the current private DDS cache")
    result.add_argument("--jobs", type=int, default=min(os.cpu_count() or 1, 4), help="parallel DDS build jobs (default: at most 4)")
    return result

def main():
    args = parser().parse_args()
    if args.jobs < 1:
        raise RuntimeError("--jobs must be positive.")
    if args.domain is not None and not 0 <= args.domain <= 232:
        raise RuntimeError("External DDS domain must be between 0 and 232.")
    if not sys.platform.startswith("linux"):
        raise RuntimeError("This deployment entry requires Linux.")
    if sys.version_info < (3, 8):
        raise RuntimeError("Python >= 3.8 is required; select a newer supported distribution or Python.")
    with deployment_lock():
        installer = Installer(args.skip_packages)
        prepare_environment(args, installer)
        deploy(args, installer)

def dependency_summary(config):
    dds_version = dds.pkg(config, ["--modversion", "CycloneDDS"], private=True)
    gst_version = dds.pkg(config, ["--modversion", "gstreamer-1.0"])
    app_version = dds.pkg(config, ["--modversion", "gstreamer-app-1.0"])
    lines = ["  DDS：CycloneDDS " + dds_version + "（SDK 私有静态库）",
             "    dds_cflags = " + dds.query(config, "dds-cflags"),
             "    dds_libs = " + dds.query(config, "dds-libs"),
             "  GStreamer：Core " + gst_version + " / App " + app_version,
             "    gstreamer_cflags = " + dds.query(config, "gst-cflags"),
             "    gstreamer_libs = " + dds.query(config, "gst-libs")]
    for key in ("PKG_CONFIG", "PKG_CONFIG_PATH", "PKG_CONFIG_LIBDIR", "PKG_CONFIG_SYSROOT_DIR"):
        if config["settings"][key]:
            lines.append("    " + key + " = " + config["settings"][key])
    return "\n".join(lines)


def deploy(args, installer):
    config = ensure_dependencies(installer, args)
    # A fixed assignment in deps.mk also wins during manual make. Do not publish
    # a conflicting interactive selection that manual builds could never use.
    for key, value in configured_inputs(installer).items():
        if value != os.environ.get(key, ""):
            raise RuntimeError("deploy/deps.mk fixes " + key + "=" + value +
                               "; edit that assignment or accept its displayed value, then rerun deployment.")
    dds.build(config, args.jobs, args.rebuild_dds)
    dds.link_probe(config)
    print("Selected toolchain and deploy:", flush=True)
    for key in ("CC", "CXX", "AR", "RANLIB"):
        print("  " + key + " = " + config["settings"][key], flush=True)
    print(dependency_summary(config), flush=True)
    sdk = dds.ROOT / "CodeProject"
    python = None
    python_error = ""
    if not args.skip_python:
        try:
            python = python_env.prepare(installer, args, config)
        except (RuntimeError, OSError, ValueError, subprocess.SubprocessError) as error:
            python = None
            python_error = str(error)
            print("\nPython support is unavailable: " + python_error +
                  "\nContinuing C/C++ deployment. Python caches are kept for retry.", file=sys.stderr, flush=True)
    robot_options = any(value is not None for value in (args.robot, args.domain, args.interface))
    if robot_options and not args.build_only and not config["cross"]:
        configure_robot(args)
    dds.atomic_text(dds.CACHE / "active.mk", "# Generated by deploy/deploy.sh; do not edit.\n"
                    "DDS_BUILD_ID := " + config["id"] + "\n")
    dds.atomic_text(dds.CACHE / "deploy-settings.json",
                    json.dumps({"version": 1, "settings": config["settings"]}, indent=2) + "\n")
    print("\n部署结果：", flush=True)
    print("  C/C++ 依赖环境：准备成功。", flush=True)
    if python:
        python_status = "准备成功（宿主环境）" if config["cross"] else "准备成功"
        print("  Python/CFFI 环境：" + python_status + "。", flush=True)
    elif args.skip_python:
        print("  Python/CFFI 环境：按 --skip-python 跳过。", flush=True)
    else:
        print("  Python/CFFI 环境：准备失败。", flush=True)
        print("    原因：" + python_error.replace("\n", "\n    "), flush=True)
    print("\n后续操作指南：", flush=True)
    demo = dds.ROOT / "python_demo"
    print("\n1. 编译（按客户语言选择一项）\n  编译与依赖配置：" + str(dds.ROOT / "deploy/deps.mk") +
          "\n  修改编译器或编译参数后，请在对应构建目录先执行 make clean，再执行 make。" +
          "\n  C/C++ 客户（环境已就绪）：\n    cd " +
          shlex.quote(str(sdk)) + " && make", flush=True)
    if python:
        print("  Python 客户：\n    cd " + shlex.quote(str(demo)) +
              (" && make PYTHON_CONFIG=/实际路径/目标-python-config" if config["cross"] else " && make"), flush=True)
    elif args.skip_python:
        print("  Python 客户：已跳过（--skip-python）。", flush=True)
    else:
        print("  Python 客户：Python/CFFI 环境准备失败，修复后重试。\n    " +
              python_error.replace("\n", "\n    "), flush=True)
    print("\n2. 运行 SDK 之前请检查修改配置文件\n  " + str(sdk / "AheadFormSDK_Config.ini"), flush=True)
    print("\n3. SDK 运行方法" + ("（在目标设备上执行）" if config["cross"] else ""), flush=True)
    run_root = Path("/目标路径/AheadFormSDK") if config["cross"] else dds.ROOT
    print("  C/C++ 客户：\n    cd " + shlex.quote(str(run_root / "CodeProject")) + " && ./AheadFormSDK", flush=True)
    if python:
        interpreter = "python3" if config["cross"] else ".venv/bin/python"
        print("  Python 客户：\n    cd " + shlex.quote(str(run_root / "python_demo")) +
              " && " + interpreter + " demo_user.py", flush=True)

if __name__ == "__main__":
    signal.signal(signal.SIGINT, interrupt_deployment)
    signal.signal(signal.SIGTERM, interrupt_deployment)
    try:
        main()
    except DeploymentInterrupted as error:
        print("\n部署已取消；重新运行相同命令可继续。", file=sys.stderr)
        sys.exit(128 + error.signum)
    except (RuntimeError, OSError, ValueError, subprocess.SubprocessError) as error:
        print("部署失败：" + str(error), file=sys.stderr)
        sys.exit(1)
