#!/usr/bin/env python3
"""Deployment interaction/retry regression tests; never installs packages or controls a robot."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import pty
import select
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "deploy"))
import dds
import deploy

def arguments(**changes):
    values = dict(non_interactive=True, build_only=True, cross=False, skip_packages=True,
                  rebuild_dds=False, jobs=2, robot=None, domain=None, interface=None,
                  skip_python=True, python_wheelhouse=None)
    values.update(changes)
    return argparse.Namespace(**values)

class Terminal:
    def __init__(self, code):
        self.pid, self.fd = pty.fork()
        if not self.pid:
            os.execv(sys.executable, [sys.executable, "-c",
                     "import sys;sys.path.insert(0," + repr(str(ROOT / "deploy")) + ");" + code])
        self.output = b""
        self.status = None
        self.cursor = 0

    def until(self, value, timeout=5):
        deadline = time.monotonic() + timeout
        needle = value.encode()
        start = self.cursor
        while time.monotonic() < deadline:
            position = self.output.find(needle, start)
            if position >= 0:
                self.cursor = position + len(needle)
                return self.output[start:self.cursor].decode(errors="replace")
            ready, _, _ = select.select([self.fd], [], [], min(.1, max(0, deadline - time.monotonic())))
            if ready:
                try:
                    data = os.read(self.fd, 65536)
                except OSError:
                    data = b""
                if not data:
                    break
                self.output += data
        raise AssertionError("Missing " + value + "\n" + self.output.decode(errors="replace"))

    def send(self, text):
        os.write(self.fd, text if isinstance(text, bytes) else text.encode())

    def finish(self, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                self.status = os.waitstatus_to_exitcode(status)
                return self.status
            time.sleep(.02)
        raise AssertionError("Terminal process did not exit")

    def close(self):
        if self.status is None:
            try:
                os.kill(self.pid, signal.SIGKILL)
                os.waitpid(self.pid, 0)
            except ProcessLookupError:
                pass
        os.close(self.fd)

class DeployTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name)

    def terminal(self, code):
        terminal = Terminal(code)
        self.addCleanup(terminal.close)
        return terminal

    def test_prompt_enter_and_changed_value(self):
        for entered, expected in [("\n", "cc"), ("gcc\n", "gcc")]:
            terminal = self.terminal("import deploy;v=deploy.prompt_value('CC','compiler','cc',lambda v:None);print('RESULT='+v,flush=True)")
            terminal.until("2 分钟")
            terminal.send(entered)
            terminal.until("RESULT=" + expected)
            self.assertEqual(terminal.finish(), 0)

    def test_prompt_clear_and_invalid_retry(self):
        terminal = self.terminal("import deploy;v=deploy.prompt_value('PATH','directory','/old',deploy.validate_directory,True);print('RESULT='+repr(v),flush=True)")
        terminal.until("2 分钟")
        terminal.send("/definitely/not/present\n")
        terminal.until("参数无效")
        time.sleep(.05)
        terminal.send("-\n")
        terminal.until("RESULT=''")
        self.assertEqual(terminal.finish(), 0)

    def test_prompt_real_120_seconds_flushes_partial_line(self):
        code = ("import deploy;v=deploy.prompt_value('CC','compiler','cc',lambda v:None);"
                "print('FIRST='+v,flush=True);"
                "v=deploy.prompt_value('SECOND','next','kept',lambda v:None);print('SECOND='+v,flush=True)")
        terminal = self.terminal(code)
        terminal.until("2 分钟")
        started = time.monotonic()
        terminal.send("unsubmitted-compiler")
        terminal.until("FIRST=cc", timeout=125)
        self.assertGreaterEqual(time.monotonic() - started, 119)
        self.assertLess(time.monotonic() - started, 125)
        terminal.until("2 分钟")
        terminal.send("\n")
        terminal.until("SECOND=kept")
        self.assertEqual(terminal.finish(), 0)

    def test_prompt_missing_default_times_out(self):
        terminal = self.terminal("import deploy;deploy.PROMPT_TIMEOUT=.15;deploy.prompt_value('CXX','compiler','',lambda v:None)")
        terminal.until("No valid default")
        self.assertNotEqual(terminal.finish(), 0)

    def test_ctrl_c_and_eof_cancel(self):
        for key in (b"\x03", b"\x04"):
            code = ("import deploy,signal;signal.signal(signal.SIGINT,deploy.interrupt_deployment);"
                    "\ntry: deploy.prompt_value('CC','compiler','cc',lambda v:None)"
                    "\nexcept deploy.DeploymentInterrupted: print('CANCELLED',flush=True)")
            terminal = self.terminal(code)
            terminal.until("2 分钟")
            terminal.send(key)
            terminal.until("CANCELLED")
            self.assertEqual(terminal.finish(), 0)

    def test_compiler_companions_match_target(self):
        for name in ("gcc", "aarch64-linux-gnu-gcc", "arm-linux-gnueabihf-gcc", "clang"):
            if not deploy.available_command(name):
                continue
            cc, cxx = dds.compiler_values({"CC": name})
            if deploy.available_command(cxx):
                self.assertEqual(dds.capture(dds.command(cc) + ["-dumpmachine"]),
                                 dds.capture(dds.command(cxx) + ["-dumpmachine"]))
            else:
                self.assertIn("g++", Path(cxx).name)
                self.assertIn(name.split("-gcc")[0], Path(cxx).name)
        with self.assertRaisesRegex(RuntimeError, "CXX"):
            dds.compiler_values({"CC": "/vendor/unusual-compiler"})
        self.assertEqual(dds.compiler_values({"CC": "vendor-cc", "CXX": "vendor-cxx"}),
                         ("vendor-cc", "vendor-cxx"))

    def test_missing_native_cpp_uses_installable_alias(self):
        for cc, resolved, expected in (("cc", "/missing/gcc-99", "c++"),
                                       ("gcc", "/missing/gcc-99", "g++"),
                                       ("clang", "/missing/clang-99", "clang++")):
            with patch("dds.shutil.which", return_value=resolved):
                self.assertEqual(dds.compiler_values({"CC": cc}), (cc, expected))

    def test_cross_wrapper_does_not_ask_for_internal_directories(self):
        if not deploy.available_command("aarch64-linux-gnu-gcc"):
            self.skipTest("ARM64 toolchain is not installed")
        wrapper = self.work / "customer-pkg-config"
        wrapper.write_text("#!/bin/sh\nexec pkg-config \"$@\"\n")
        wrapper.chmod(0o755)
        result, calls = self.prepare({}, {"CROSS_COMPILE": "aarch64-linux-gnu-",
                                         "PKG_CONFIG": str(wrapper)})
        self.assertEqual(calls, ["CC", "SYSROOT", "PKG_CONFIG"])
        self.assertEqual(result["PKG_CONFIG"], str(wrapper))

    def test_bootstrap_package_interrupt_and_retry(self):
        source = (ROOT / "deploy/deploy.sh").read_text()
        helper = source[source.index("bootstrap_package() {"):source.index("# Bootstrap Python")]
        for signum in (signal.SIGINT, signal.SIGTERM):
            ready, done = self.work / ("ready-" + str(signum)), self.work / ("done-" + str(signum))
            transaction = ("from pathlib import Path;import time;Path(" + repr(str(ready)) +
                           ").touch();time.sleep(.5);Path(" + repr(str(done)) + ").touch()")
            import shlex
            command = helper + "\nbootstrap_package " + shlex.join([sys.executable, "-c", transaction]) + "\n"
            process = subprocess.Popen(["bash", "-euc", command], start_new_session=True,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.addCleanup(lambda p=process: p.poll() is None and p.kill())
            self.wait_file(ready)
            os.killpg(process.pid, signum)
            process.communicate(timeout=5)
            self.assertEqual(process.returncode, 128 + signum)
            self.assertTrue(done.exists())
            retry = subprocess.run(["bash", "-euc", command], stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, timeout=5)
            self.assertEqual(retry.returncode, 0, retry.stderr.decode())

    def test_missing_native_companion_keeps_compiler_target_arguments(self):
        with patch("dds.shutil.which", return_value="/missing/clang-99"):
            self.assertEqual(dds.compiler_values({"CC": "clang --target=aarch64-linux-gnu"}),
                             ("clang --target=aarch64-linux-gnu", "clang++ --target=aarch64-linux-gnu"))
        with patch("deploy.shutil.which", return_value=None):
            deploy.validate_tool("clang --target=aarch64-linux-gnu", allow_install=True)
        custom = self.work / "vendor-cc"
        custom.touch()
        with patch("dds.shutil.which", return_value=str(custom)):
            with self.assertRaisesRegex(RuntimeError, "CXX"):
                dds.compiler_values({"CC": "cc"})

    def test_cross_compiler_alias_never_falls_back_to_native_cpp(self):
        with patch("dds.Path.exists", side_effect=[True, False, True]), \
             patch("dds.target_info", return_value=("arm", True)):
            cc, cxx = dds.compiler_values({"CC": "gcc"})
        self.assertEqual(cc, "gcc")
        self.assertNotEqual(cxx, "g++")

    def test_saved_settings_and_existing_active_migration(self):
        with patch.object(dds, "CACHE", self.work):
            self.assertEqual(deploy.saved_settings(), {})
            (self.work / "native-id").mkdir()
            settings = {"CC": "gcc", "CXX": "g++"}
            (self.work / "active.mk").write_text("DDS_BUILD_ID := native-id\n")
            (self.work / "native-id/config.json").write_text(json.dumps({"settings": settings}))
            self.assertEqual(deploy.saved_settings()["CC"], "gcc")
            (self.work / "deploy-settings.json").write_text(json.dumps({"version": 1, "settings": {"CC": "clang"}}))
            self.assertEqual(deploy.saved_settings()["CC"], "clang")
            (self.work / "deploy-settings.json").write_text('{"version":99}')
            with self.assertRaises(RuntimeError):
                deploy.saved_settings()

    def test_deps_mk_overrides_are_read_without_a_dds_cache(self):
        (self.work / "deploy").mkdir()
        source = (ROOT / "deploy/deps.mk").read_text()
        (self.work / "deploy/deps.mk").write_text(
            "CC := clang\nCFLAGS := -DNAME='a b'\nSYSROOT :=\n" + source)
        with patch.object(dds, "ROOT", self.work), \
             patch.dict(os.environ, {"PATH": os.environ["PATH"], "CC": "gcc", "AR": "ar"}, clear=True):
            settings = deploy.configured_inputs(deploy.Installer(True))
        self.assertEqual(settings, dict(CC="clang", CFLAGS="-DNAME='a b'", SYSROOT="", AR="ar"))

    def test_optional_deps_mk_settings_preserve_environment(self):
        (self.work / "deploy").mkdir()
        source = (ROOT / "deploy/deps.mk").read_text()
        (self.work / "deploy/deps.mk").write_text(
            "CXXFLAGS ?= -std=c++17\nPKG_CONFIG_LIBDIR ?= /default/pkgconfig\n" + source)
        for supplied, expected in (
            ({}, dict(CXXFLAGS="-std=c++17", PKG_CONFIG_LIBDIR="/default/pkgconfig")),
            (dict(CXXFLAGS="-std=c++20", PKG_CONFIG_LIBDIR="/customer/pkgconfig"),
             dict(CXXFLAGS="-std=c++20", PKG_CONFIG_LIBDIR="/customer/pkgconfig")),
        ):
            with self.subTest(environment=supplied), patch.object(dds, "ROOT", self.work), \
                 patch.dict(os.environ, dict(PATH=os.environ["PATH"], **supplied), clear=True):
                settings = deploy.configured_inputs(deploy.Installer(True))
            self.assertEqual(settings, expected)

    def test_confirmed_settings_are_written_to_make_configuration(self):
        (self.work / "deploy").mkdir()
        (self.work / "deploy/deps.mk").write_bytes((ROOT / "deploy/deps.mk").read_bytes())
        config = dict(id="confirmed", cross=False, settings={key: "" for key in dds.KEYS})
        config["settings"].update(CC="/tools/vendor-gcc", CXX="/tools/vendor-g++", AR="/tools/vendor-ar",
                                 RANLIB="/tools/vendor-ranlib", CFLAGS="-DNAME='a b'")
        with patch.object(dds, "ROOT", self.work), patch.object(dds, "CACHE", self.work / ".deps"), \
             patch("deploy.ensure_dependencies", return_value=config), \
             patch("deploy.configured_inputs", return_value={}), \
             patch("dds.build", side_effect=lambda *args: dds.write_settings(config)), patch("dds.link_probe"), \
             patch("deploy.dependency_summary", return_value="dependency details"):
            deploy.deploy(arguments(), deploy.Installer(True))
        saved = (self.work / ".deps/confirmed/config.mk").read_text()
        self.assertIn("DDS_DEFAULT_CC := /tools/vendor-gcc", saved)
        self.assertIn("DDS_DEFAULT_CXX := /tools/vendor-g++", saved)
        # This is the same include path used by all manual Makefile entry points.
        code = ("import os,json; print(json.dumps({k:os.environ['AHEADFORM_'+k] "
                "for k in ('CC','CXX','AR','RANLIB','CFLAGS')}))")
        import shlex
        makefile = "show:\n\t@" + shlex.join([sys.executable, "-c", code]) + "\n"
        output = subprocess.check_output([
            "make", "-s", "-f", str(self.work / "deploy/deps.mk"), "-f", "-",
            "SDK_ROOT=" + str(self.work), "_AHEADFORM_CONFIG_ONLY=1", "show"],
            input=makefile, text=True, env={"PATH": os.environ["PATH"]})
        self.assertEqual(json.loads(output), {key: config["settings"][key]
                                            for key in ("CC", "CXX", "AR", "RANLIB", "CFLAGS")})

    def prepare(self, previous, environment, args=None, answers=None):
        supplied = {"PATH": os.environ["PATH"]}
        supplied.update(environment)
        def respond(name, description, current, validator, optional=False):
            value = (answers or {}).get(name, current)
            validator(value)
            return value
        with patch.dict(os.environ, supplied, clear=True), patch("deploy.saved_settings", return_value=previous), \
             patch("deploy.ensure_build_tools"), patch("deploy.sys.stdin.isatty", return_value=True), \
             patch("deploy.sys.stdout.isatty", return_value=True), patch("deploy.prompt_value", side_effect=respond) as prompt:
            deploy.prepare_environment(args or arguments(non_interactive=False), deploy.Installer(True))
            result = {key: os.environ.get(key, "") for key in dds.KEYS}
            calls = [call.args[0] for call in prompt.call_args_list]
        return result, calls

    def test_native_only_prompts_for_c_compiler(self):
        result, calls = self.prepare({}, {})
        self.assertEqual(calls, ["CC"])
        self.assertTrue(result["CXX"])
        self.assertTrue(result["AR"])
        self.assertTrue(result["RANLIB"])

    def test_toolchain_change_discards_old_target_defaults(self):
        previous = dict(CC="arm-linux-gnueabihf-gcc", CXX="old-cxx", AR="old-ar",
                        RANLIB="old-ranlib", CROSS_COMPILE="arm-linux-gnueabihf-",
                        SYSROOT="/old/sysroot", PKG_CONFIG="/old/pkg-config",
                        PKG_CONFIG_LIBDIR="/old/lib/pkgconfig", CFLAGS="-mfpu=neon")
        result, calls = self.prepare(previous, {"CC": "gcc"})
        self.assertEqual(calls, ["CC"])
        for key in ("SYSROOT", "CROSS_COMPILE", "PKG_CONFIG_LIBDIR", "CFLAGS"):
            self.assertFalse(result[key])
        self.assertNotEqual(result["CXX"], "old-cxx")

    def test_explicit_tools_survive_and_noninteractive_never_prompts(self):
        cc, cxx = dds.compiler_values({"CC": "gcc"})
        ar = dds.capture(dds.command(cc) + ["-print-prog-name=ar"])
        result, calls = self.prepare({}, {"CC": cc, "CXX": cxx, "AR": ar},
                                     args=arguments())
        self.assertEqual(calls, [])
        self.assertEqual(result["CXX"], cxx)
        self.assertEqual(result["AR"], ar)

    def test_cross_prompts_for_target_directories(self):
        if not deploy.available_command("aarch64-linux-gnu-gcc"):
            self.skipTest("ARM64 toolchain is not installed")
        result, calls = self.prepare({}, {"CROSS_COMPILE": "aarch64-linux-gnu-"},
                                     answers={"PKG_CONFIG_LIBDIR": str(self.work)})
        self.assertEqual(calls, ["CC", "SYSROOT", "PKG_CONFIG", "PKG_CONFIG_LIBDIR", "PKG_CONFIG_SYSROOT_DIR"])
        self.assertIn("aarch64", result["CXX"])
        self.assertEqual(result["PKG_CONFIG_LIBDIR"], str(self.work))

    def test_abi_flag_change_does_not_reuse_old_target_directories(self):
        if not deploy.available_command("aarch64-linux-gnu-gcc"):
            self.skipTest("ARM64 toolchain is not installed")
        previous = dict(CC="aarch64-linux-gnu-gcc", CXX="old-cxx", AR="old-ar", RANLIB="old-ranlib",
                        CROSS_COMPILE="aarch64-linux-gnu-", CFLAGS="-O2", PKG_CONFIG_LIBDIR="/old/target/pkgconfig")
        result, calls = self.prepare(previous, {"CC": "aarch64-linux-gnu-gcc", "CFLAGS": "-O1"},
                                     answers={"PKG_CONFIG_LIBDIR": str(self.work)})
        self.assertEqual(result["CFLAGS"], "-O1")
        self.assertEqual(result["CROSS_COMPILE"], "aarch64-linux-gnu-")
        self.assertEqual(result["PKG_CONFIG_LIBDIR"], str(self.work))
        self.assertNotEqual(result["CXX"], "old-cxx")

    def test_unknown_c_compiler_asks_for_cpp_instead_of_using_host_default(self):
        vendor_cc = self.work / "vendor-cc"
        vendor_cc.write_text("#!/bin/sh\nexec gcc \"$@\"\n")
        vendor_cc.chmod(0o755)
        result, calls = self.prepare({}, {"CC": str(vendor_cc)}, answers={"CXX": "g++"})
        self.assertEqual(calls, ["CC", "CXX"])
        self.assertEqual(result["CXX"], "g++")

    def test_bootstrap_tools_defer_selected_pkg_config_until_after_prompts(self):
        with patch.dict(os.environ, {"PKG_CONFIG": "/missing/old-wrapper"}):
            self.assertNotIn("/missing/old-wrapper", deploy.missing_tools(target_pkg=False))
            self.assertIn("/missing/old-wrapper", deploy.missing_tools())

    def test_interrupted_ini_replace_is_safe_to_retry(self):
        (self.work / "CodeProject").mkdir()
        path = self.work / "CodeProject/AheadFormSDK_Config.ini"
        original = b"; comment\r\n[dds]\r\nstrNetworkInterface=old\r\ndomain_id=1\r\n[media]\r\nstrRobotIPv4=1.2.3.4\r\n"
        path.write_bytes(original)
        args = arguments(robot="192.168.110.51", domain=50)
        with patch.object(dds, "ROOT", self.work), patch("dds.capture", return_value='[{"dev":"test-wired"}]'), \
             patch("deploy.subprocess.run"):
            with patch("pathlib.Path.replace", side_effect=deploy.DeploymentInterrupted()):
                with self.assertRaises(deploy.DeploymentInterrupted):
                    deploy.configure_robot(args)
            self.assertEqual(path.read_bytes(), original)
            self.assertFalse(path.with_name(path.name + ".tmp").exists())
            deploy.configure_robot(args)
            updated = path.read_bytes()
            self.assertNotEqual(updated, original)
            backups = list(path.parent.glob("*.bak.*"))
            deploy.configure_robot(args)
            self.assertEqual(path.read_bytes(), updated)
            self.assertEqual(list(path.parent.glob("*.bak.*")), backups)

    def test_project_lock_rejects_second_deployment(self):
        with patch.object(dds, "CACHE", self.work):
            with deploy.deployment_lock():
                with self.assertRaisesRegex(RuntimeError, "Another deployment"):
                    with deploy.deployment_lock():
                        pass
            with deploy.deployment_lock():
                pass

    def test_deployment_without_robot_does_not_require_network_tools(self):
        for options in ({}, {"domain": 0}, {"interface": "test-wired"}):
            with self.subTest(options=options), \
                 patch("deploy.ensure_build_tools"), \
                 patch("dds.configuration", return_value={"cross": False}), \
                 patch("dds.validate_gstreamer"), patch("deploy.missing_plugins", return_value=[]), \
                 patch("deploy.shutil.which", return_value=None):
                installer = deploy.Installer(True)
                with patch.object(installer, "install") as install:
                    deploy.ensure_dependencies(installer, arguments(build_only=False, **options))
                    install.assert_not_called()

    def test_robot_validation_is_optional_and_does_not_require_domain(self):
        self.assertIsNone(deploy.validate_robot(arguments()))
        self.assertEqual(str(deploy.validate_robot(arguments(robot="192.168.110.51"))), "192.168.110.51")
        for address in ("", "invalid", "127.0.0.1", "0.0.0.0", "239.1.2.3", "255.255.255.255"):
            with self.subTest(address=address), self.assertRaises(RuntimeError):
                deploy.validate_robot(arguments(robot=address))

    def test_robot_options_update_only_selected_fields_and_remain_idempotent(self):
        source = b"; keep\r\n[dds]\r\nstrNetworkInterface=old\r\ndomain_id=10\r\n[media]\r\nstrRobotIPv4=1.2.3.4\r\n"
        cases = [({}, source),
                 ({"domain": 0}, source.replace(b"domain_id=10", b"domain_id=0")),
                 ({"interface": "test-wired"}, source.replace(b"strNetworkInterface=old", b"strNetworkInterface=test-wired")),
                 ({"robot": "192.168.110.51"}, source.replace(b"strRobotIPv4=1.2.3.4", b"strRobotIPv4=192.168.110.51")
                                                  .replace(b"strNetworkInterface=old", b"strNetworkInterface=test-wired"))]
        for index, (options, expected) in enumerate(cases):
            root = self.work / str(index)
            (root / "CodeProject").mkdir(parents=True)
            path = root / "CodeProject/AheadFormSDK_Config.ini"
            path.write_bytes(source)
            with self.subTest(options=options), patch.object(dds, "ROOT", root), \
                 patch("dds.capture", return_value='[{"dev":"test-wired"}]') as route, \
                 patch("deploy.subprocess.run") as ping, patch("pathlib.Path.is_dir", return_value=True):
                deploy.configure_robot(arguments(**options))
                self.assertEqual(path.read_bytes(), expected)
                timestamp = path.stat().st_mtime_ns
                deploy.configure_robot(arguments(**options))
                self.assertEqual(path.stat().st_mtime_ns, timestamp)
                self.assertEqual(len(list(path.parent.glob("*.bak.*"))), int(bool(options)))
                self.assertEqual(route.call_count, 2 if "robot" in options else 0)
                self.assertEqual(ping.call_count, 2 if "robot" in options else 0)

    def test_interface_only_rejects_unusable_interfaces_without_changing_config(self):
        with patch.object(dds, "ROOT", self.work), patch("dds.capture") as route, \
             patch("deploy.subprocess.run") as ping:
            for name in ("", "lo", "missing-interface", "/tmp"):
                with self.subTest(name=name), self.assertRaises(RuntimeError):
                    deploy.configure_robot(arguments(interface=name))
            route.assert_not_called()
            ping.assert_not_called()

    def test_config_update_is_idempotent_and_preserves_bytes(self):
        (self.work / "CodeProject").mkdir()
        path = self.work / "CodeProject/AheadFormSDK_Config.ini"
        source = b"# comment\r\n[dds]\r\nstrNetworkInterface = old ; keep\r\ndomain_id = 10\r\n[media]\r\nstrRobotIPv4 = 1.2.3.4\r\nother = keep\r\n"
        path.write_bytes(source)
        path.chmod(0o640)
        args = arguments(robot="192.168.110.51", domain=50)
        with patch.object(dds, "ROOT", self.work), patch("dds.capture", return_value='[{"dev":"test-wired"}]'), \
             patch("deploy.subprocess.run"):
            deploy.configure_robot(args)
            changed = path.read_bytes()
            mtime = path.stat().st_mtime_ns
            self.assertIn(b"# comment\r\n", changed)
            self.assertIn(b"; keep\r\n", changed)
            self.assertIn(b"other = keep\r\n", changed)
            self.assertEqual(path.stat().st_mode & 0o777, 0o640)
            deploy.configure_robot(args)
            self.assertEqual(path.stat().st_mtime_ns, mtime)
            self.assertEqual(len(list(path.parent.glob("*.bak.*"))), 1)
            self.assertFalse(path.with_name(path.name + ".tmp").exists())

    def test_atomic_write_failure_preserves_original(self):
        path = self.work / "settings.json"
        path.write_text("original")
        with patch("pathlib.Path.replace", side_effect=OSError("injected interruption")):
            with self.assertRaises(OSError):
                dds.atomic_text(path, "new")
        self.assertEqual(path.read_text(), "original")
        self.assertFalse(path.with_suffix(".tmp").exists())

    def test_rebuild_clears_success_marker_before_removing_products(self):
        base = self.work / "test-id"
        (base / "cmake").mkdir(parents=True)
        marker = base / "complete.json"
        marker.write_text("{}")
        with patch.object(dds, "CACHE", self.work), patch("dds.verify_sources", return_value=self.work), \
             patch("dds.shutil.rmtree", side_effect=deploy.DeploymentInterrupted()):
            with self.assertRaises(deploy.DeploymentInterrupted):
                dds.build({"id": "test-id"}, 1, rebuild=True)
        self.assertFalse(marker.exists())

    def test_failed_deployment_keeps_last_success(self):
        snapshot = self.work / "deploy-settings.json"
        snapshot.write_text('{"version":1,"settings":{"CC":"old"}}')
        previous = snapshot.read_bytes()
        config = dict(id="test", settings={key: "" for key in dds.KEYS}, cross=False)
        config["settings"].update(CC="cc", CXX="c++", AR="ar", RANLIB="ranlib")
        with patch.object(dds, "CACHE", self.work), patch("deploy.ensure_dependencies", return_value=config), \
             patch("deploy.configured_inputs", return_value={}), \
             patch("dds.build"), patch("dds.link_probe", side_effect=RuntimeError("injected failed DDS probe")):
            with self.assertRaisesRegex(RuntimeError, "failed DDS probe"):
                deploy.deploy(arguments(), deploy.Installer(True))
        self.assertEqual(snapshot.read_bytes(), previous)
        self.assertFalse((self.work / "active.mk").exists())

    def test_success_snapshot_only_after_configuration(self):
        snapshot = self.work / "deploy-settings.json"
        snapshot.write_text('{"version":1,"settings":{"CC":"old"}}')
        original = snapshot.read_bytes()
        config = dict(id="test", settings={key: "" for key in dds.KEYS}, cross=False)
        config["settings"].update(CC="cc", CXX="c++", AR="ar", RANLIB="ranlib")
        with patch.object(dds, "CACHE", self.work), patch("deploy.ensure_dependencies", return_value=config), \
             patch("deploy.configured_inputs", return_value={}), \
             patch("dds.build"), patch("dds.link_probe"), patch("dds.capture", return_value="libc.so"), \
             patch("dds.run"), patch("dds.target_info", return_value=("x86_64", False)), \
             patch("deploy.dependency_summary", return_value="dependency details"), \
             patch("deploy.configure_robot", side_effect=RuntimeError("network failure")):
            with self.assertRaisesRegex(RuntimeError, "network failure"):
                deploy.deploy(arguments(build_only=False, robot="192.168.110.51", domain=50), deploy.Installer(True))
        self.assertEqual(snapshot.read_bytes(), original)

    def test_package_operations_remain_targeted(self):
        for family in ("apt", "dnf", "zypper"):
            installer = deploy.Installer(False)
            installer.family = family
            with patch("deploy.os.geteuid", return_value=0), patch("deploy.run_package_command") as run:
                installer.install(["media"], "test")
                self.assertEqual(run.call_count, 2)
                command = run.call_args.args[0]
                self.assertNotIn("allowerasing", command)
                self.assertNotIn("dist-upgrade", command)
                if family == "apt":
                    self.assertIn("--no-remove", command)

    def wait_file(self, path, timeout=5):
        deadline = time.monotonic() + timeout
        while not path.exists() and time.monotonic() < deadline:
            time.sleep(.02)
        self.assertTrue(path.exists())

    def test_package_interrupt_is_deferred(self):
        ready, done = self.work / "ready", self.work / "done"
        transaction = "from pathlib import Path;import time;Path(" + repr(str(ready)) + ").touch();time.sleep(1);Path(" + repr(str(done)) + ").touch()"
        code = ("import sys,signal;sys.path.insert(0," + repr(str(ROOT / "deploy")) + ");import deploy;"
                "signal.signal(signal.SIGINT,deploy.interrupt_deployment);"
                "\ntry: deploy.run_package_command(" + repr([sys.executable, "-c", transaction]) + ")"
                "\nexcept deploy.DeploymentInterrupted as e: sys.exit(128+e.signum)")
        process = subprocess.Popen([sys.executable, "-c", code], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(lambda: process.poll() is None and process.kill())
        self.wait_file(ready)
        os.kill(process.pid, signal.SIGINT)
        self.assertIsNone(process.poll())
        process.communicate(timeout=5)
        self.assertEqual(process.returncode, 130)
        self.assertTrue(done.exists())

    def test_build_interrupt_stops_grandchildren(self):
        marker = self.work / "grandchild"
        child = ("import subprocess,sys,time;from pathlib import Path;"
                 "p=subprocess.Popen([sys.executable,'-c','import time;time.sleep(60)']);"
                 "Path(" + repr(str(marker)) + ").write_text(str(p.pid));time.sleep(60)")
        code = ("import sys,signal;sys.path.insert(0," + repr(str(ROOT / "deploy")) + ");import dds,deploy;"
                "signal.signal(signal.SIGTERM,deploy.interrupt_deployment);"
                "\ntry: dds.run(" + repr([sys.executable, "-c", child]) + ")"
                "\nexcept deploy.DeploymentInterrupted as e: sys.exit(128+e.signum)")
        process = subprocess.Popen([sys.executable, "-c", code], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.addCleanup(lambda: process.poll() is None and process.kill())
        self.wait_file(marker)
        os.kill(process.pid, signal.SIGTERM)
        process.communicate(timeout=8)
        self.assertEqual(process.returncode, 143)
        pid = int(marker.read_text())
        stat = Path("/proc") / str(pid) / "stat"
        self.assertTrue(not stat.exists() or stat.read_text().split()[2] == "Z")

if __name__ == "__main__":
    unittest.main(verbosity=2)
