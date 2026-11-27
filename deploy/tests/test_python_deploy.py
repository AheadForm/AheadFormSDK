"""Python deployment regression tests; no system packages or robot operations."""
from pathlib import Path
from contextlib import ExitStack, redirect_stderr, redirect_stdout
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "deploy"))
import dds
import deploy
import python_env


class PythonDeployTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.args = deploy.parser().parse_args(["--build-only", "--non-interactive"])
        self.config = dict(id="test", cross=False, settings={key: "" for key in dds.KEYS})
        self.config["settings"].update(CC="cc", CXX="c++", AR="ar", RANLIB="ranlib")
        self.installer = Mock()
        self.root_patch = patch.object(dds, "ROOT", self.root)
        self.root_patch.start()
        self.addCleanup(self.root_patch.stop)

    def test_default_includes_python_and_skip_does_not_touch_environment(self):
        self.assertFalse(self.args.skip_python)
        self.args.skip_python = True
        with patch("python_env.probe") as probe, patch("dds.run") as run:
            self.assertIsNone(python_env.prepare(self.installer, self.args, self.config))
        probe.assert_not_called()
        run.assert_not_called()
        self.installer.install.assert_not_called()

    def test_cross_can_prepare_host_cffi_before_target_python_is_available(self):
        self.config["cross"] = True
        with patch.dict(os.environ, {}, clear=True), \
             patch("python_env.probe", return_value=(True, "")), patch("dds.run") as run:
            python = python_env.prepare(self.installer, self.args, self.config)
        self.assertEqual(python, self.root / "python_demo/.venv/bin/python")
        run.assert_not_called()

    def test_host_pip_cannot_inherit_target_compiler_or_install_destination(self):
        with patch.dict(os.environ, {"CC": "arm-vendor-gcc", "CXX": "arm-vendor-g++",
                                    "PKG_CONFIG_LIBDIR": "/target", "SYSROOT": "/sysroot",
                                    "PIP_TARGET": "/outside", "PIP_USER": "1",
                                    "PYTHONPATH": "/target/python", "PATH": "/host/bin"}, clear=True):
            environment = python_env.host_environment()
        self.assertEqual(environment, {"PATH": "/host/bin"})

    def test_ready_environment_is_reused_without_pip_or_package_install(self):
        with patch("python_env.probe", return_value=(True, "")), patch("dds.run") as run:
            python = python_env.prepare(self.installer, self.args, self.config)
            python_env.prepare(self.installer, self.args, self.config)
        self.assertEqual(python, self.root / "python_demo/.venv/bin/python")
        run.assert_not_called()
        self.installer.install.assert_not_called()

    def test_incomplete_venv_is_repaired_without_deleting_files(self):
        sentinel = self.root / "python_demo/.venv/customer-file"
        sentinel.parent.mkdir(parents=True)
        sentinel.write_text("keep")
        answers = [(False, "incomplete"), (False, "missing ensurepip"), (True, ""),
                   (True, ""), (True, ""), (True, ""), (True, "")]
        with patch("python_env.probe", side_effect=answers), patch("dds.run") as run:
            python_env.prepare(self.installer, self.args, self.config)
        self.installer.install.assert_called_once()
        self.assertEqual(run.call_args.args[0][1:3], ["-m", "venv"])
        self.assertEqual(sentinel.read_text(), "keep")

    def test_offline_requirements_install_uses_private_interpreter_and_no_index(self):
        self.args.python_wheelhouse = str(self.root)
        answers = [(True, ""), (True, ""), (True, ""), (False, "missing cffi"), (True, "")]
        with patch("python_env.probe", side_effect=answers), patch("dds.run") as run:
            python = python_env.prepare(self.installer, self.args, self.config)
        command = run.call_args.args[0]
        self.assertEqual(command[0], python)
        self.assertIn("--require-virtualenv", command)
        self.assertIn("--no-index", command)
        self.assertEqual(command[-2:], ["--find-links", str(self.root)])
        self.installer.install.assert_not_called()

    def test_failed_pip_can_be_retried_without_success_marker(self):
        answers = [(True, ""), (True, ""), (True, ""), (False, "missing cffi")]
        with patch("python_env.probe", side_effect=answers), \
             patch("dds.run", side_effect=subprocess.CalledProcessError(1, ["pip"])), \
             patch("python_env.subprocess.run", return_value=Mock(returncode=0)):
            with self.assertRaisesRegex(RuntimeError, "rerun deployment"):
                python_env.prepare(self.installer, self.args, self.config)
        self.test_ready_environment_is_reused_without_pip_or_package_install()

    def test_cffi_source_build_installs_missing_host_libffi_then_retries(self):
        answers = [(True, ""), (True, ""), (True, ""), (False, "missing cffi"), (True, "")]
        with patch("python_env.probe", side_effect=answers), \
             patch("dds.run", side_effect=[subprocess.CalledProcessError(1, ["pip"]), Mock()]) as run, \
             patch("python_env.subprocess.run", return_value=Mock(returncode=1)):
            python_env.prepare(self.installer, self.args, self.config)
        self.installer.install.assert_called_once()
        self.assertEqual(run.call_count, 2)

    def test_python_prepare_interrupt_keeps_last_success_settings_and_selection(self):
        cache = self.root / ".deps"
        cache.mkdir()
        settings = cache / "deploy-settings.json"
        original = '{"version":1,"settings":{"CC":"old"}}'
        settings.write_text(original)
        (cache / "active.mk").write_text("DDS_BUILD_ID := old\n")
        with patch.object(dds, "CACHE", cache), \
             patch("deploy.ensure_dependencies", return_value=self.config), \
             patch("deploy.configured_inputs", return_value={}), \
             patch("python_env.prepare", side_effect=deploy.DeploymentInterrupted()), \
             patch("dds.build"), patch("dds.link_probe"), patch("dds.run"), \
             patch("dds.capture", return_value="libc.so"), \
             patch("deploy.dependency_summary", return_value="dependency details"):
            with self.assertRaises(deploy.DeploymentInterrupted):
                deploy.deploy(self.args, self.installer)
        self.assertEqual(settings.read_text(), original)
        self.assertEqual((cache / "active.mk").read_text(), "DDS_BUILD_ID := old\n")

    def deployment_mocks(self, stack):
        cache = self.root / ".deps"
        stack.enter_context(patch.object(dds, "CACHE", cache))
        stack.enter_context(patch("deploy.ensure_dependencies", return_value=self.config))
        stack.enter_context(patch("deploy.configured_inputs", return_value={}))
        stack.enter_context(patch("dds.build"))
        stack.enter_context(patch("dds.link_probe"))
        run = stack.enter_context(patch("dds.run"))
        stack.enter_context(patch("dds.capture", return_value="libc.so"))
        packages = {
            ("--modversion", "CycloneDDS"): "11.0.1",
            ("--modversion", "gstreamer-1.0"): "1.24.0",
            ("--modversion", "gstreamer-app-1.0"): "1.24.1",
            ("--cflags", "CycloneDDS"): "-I" + str(cache / "test/prefix/include"),
            ("--cflags", "gstreamer-app-1.0"): "-I/customer/gst/include",
            ("--libs", "gstreamer-app-1.0"): "-L/customer/gst/lib -lgstapp-1.0",
        }
        stack.enter_context(patch("dds.pkg", side_effect=lambda config, args, private=False: packages[tuple(args)]))
        output, errors = io.StringIO(), io.StringIO()
        stack.enter_context(redirect_stdout(output))
        stack.enter_context(redirect_stderr(errors))
        return cache, run, output, errors

    def test_optional_python_errors_publish_cpp_success_and_can_retry(self):
        failures = [RuntimeError("CFFI unavailable"), OSError("Python packages cannot be installed"),
                    ValueError("unsupported Python configuration"), subprocess.CalledProcessError(2, ["pip"])]
        for error in failures:
            with self.subTest(error=error), ExitStack() as stack:
                cache, run, output, errors = self.deployment_mocks(stack)
                selection = self.root / "python_demo/.build/active"
                selection.parent.mkdir(parents=True, exist_ok=True)
                selection.write_text("previous-python-build\n")
                sentinel = selection.parent / "cached-object"
                sentinel.write_text("keep for retry")
                prepare = stack.enter_context(patch("python_env.prepare", return_value=Path("python")))
                prepare.side_effect = error
                deploy.deploy(self.args, self.installer)
                self.assertEqual(json.loads((cache / "deploy-settings.json").read_text())["settings"],
                                 self.config["settings"])
                self.assertIn("DDS_BUILD_ID := test", (cache / "active.mk").read_text())
                self.assertIn("C/C++ 客户（环境已就绪）", output.getvalue())
                self.assertIn("Python/CFFI 环境准备失败", output.getvalue())
                self.assertNotIn("bindings are ready", output.getvalue())
                self.assertIn(str(error), errors.getvalue())
                summary = output.getvalue().split("1. 编译")[-1]
                self.assertIn("C/C++ 客户（环境已就绪）", summary)
                self.assertIn("Python/CFFI 环境准备失败", summary)
                self.assertIn(str(error), summary)
                self.assertIn("修复后重试", summary)
                result = output.getvalue().rsplit("部署结果：\n", 1)[1].split("\n后续操作指南：", 1)[0]
                self.assertTrue(result.startswith("  C/C++ 依赖环境：准备成功。\n"))
                self.assertIn("  Python/CFFI 环境：准备失败。\n", result)
                self.assertTrue(result.rstrip().endswith("原因：" + str(error)))
                self.assertEqual(selection.read_text(), "previous-python-build\n")
                self.assertEqual(sentinel.read_text(), "keep for retry")
                run.assert_not_called()  # Deployment does not clean/build SDK artifacts.
                prepare.side_effect = None
                deploy.deploy(self.args, self.installer)
                self.assertIn("  Python 客户：\n    cd ", output.getvalue())
                result = output.getvalue().rsplit("部署结果：\n", 1)[1].split("\n后续操作指南：", 1)[0]
                self.assertEqual(result.strip(),
                                 "C/C++ 依赖环境：准备成功。\n  Python/CFFI 环境：准备成功。")

    def test_default_deployment_succeeds_without_robot_and_prints_configuration_path(self):
        self.args = deploy.parser().parse_args(["--non-interactive"])
        with ExitStack() as stack:
            cache, run, output, _ = self.deployment_mocks(stack)
            stack.enter_context(patch("python_env.prepare", return_value=Path("python")))
            configure = stack.enter_context(patch("deploy.configure_robot"))
            deploy.deploy(self.args, self.installer)
            configure.assert_not_called()
            run.assert_not_called()
            self.assertTrue((cache / "deploy-settings.json").is_file())
            self.assertIn("  Python 客户：\n    cd ", output.getvalue())
            self.assertNotIn("--list", output.getvalue())
            self.assertNotIn("QueryVolume", output.getvalue())
            self.assertNotIn("未构建", output.getvalue())
            self.assertNotIn("工具链已保存：", output.getvalue())
            self.assertNotIn(str(cache / "test/config.mk"), output.getvalue())
            self.assertIn("修改编译器或编译参数后，请在对应构建目录"
                          "先执行 make clean，再执行 make。", output.getvalue())
            self.assertNotIn("&& make clean", output.getvalue())
            self.assertIn(str(self.root / "deploy/deps.mk"), output.getvalue())
            self.assertIn(str(cache / "test/prefix/lib/libddsc.a"), output.getvalue())
            self.assertEqual([line for line in output.getvalue().splitlines()
                              if line[:1].isdigit()], ["1. 编译（按客户语言选择一项）",
                                                      "2. 运行 SDK 之前请检查修改配置文件", "3. SDK 运行方法"])
            self.assertIn("cd " + str(self.root / "CodeProject") + " && make\n", output.getvalue())
            self.assertIn("cd " + str(self.root / "python_demo") + " && make\n", output.getvalue())
            self.assertIn("./AheadFormSDK", output.getvalue())
            self.assertIn(".venv/bin/python demo_user.py", output.getvalue())
            self.assertIn(str(self.root / "CodeProject/AheadFormSDK_Config.ini"), output.getvalue())
            result = output.getvalue().split("部署结果：\n", 1)[1].split("\n后续操作指南：", 1)[0]
            self.assertEqual(result.strip(),
                             "C/C++ 依赖环境：准备成功。\n  Python/CFFI 环境：准备成功。")
            for field in ("strNetworkInterface", "domain_id", "strRobotIPv4"):
                self.assertNotIn(field, output.getvalue())
            self.assertNotIn("修改后重新部署", output.getvalue())
            self.assertIn("CycloneDDS 11.0.1（SDK 私有静态库）", output.getvalue())
            self.assertIn("GStreamer：Core 1.24.0 / App 1.24.1", output.getvalue())
            self.assertIn("gstreamer_cflags = -I/customer/gst/include", output.getvalue())
            self.assertIn("gstreamer_libs = -L/customer/gst/lib -lgstapp-1.0", output.getvalue())
            self.assertNotIn(".pc 目录：", output.getvalue())
            text = output.getvalue()
            selected, instructions = text.split("\n后续操作指南：", 1)
            headings = ("Selected toolchain and deploy:", "部署结果：", "后续操作指南：",
                        "1. 编译", "2. 运行 SDK", "3. SDK 运行方法")
            for heading in headings:
                self.assertEqual(text.count(heading), 1)
            positions = [text.index(heading) for heading in headings]
            self.assertEqual(positions, sorted(positions))
            self.assertIn("Selected toolchain and deploy:\n  CC = cc\n  CXX = c++\n"
                          "  AR = ar\n  RANLIB = ranlib\n  DDS：", selected)
            self.assertLess(selected.index("DDS："), selected.index("GStreamer："))
            for field in ("DDS：", "GStreamer：", "dds_cflags =", "dds_libs =",
                          "gstreamer_cflags =", "gstreamer_libs ="):
                self.assertEqual(text.count(field), 1)
                self.assertNotIn(field, instructions)

    def test_build_only_and_cross_ignore_optional_robot_configuration(self):
        self.args = deploy.parser().parse_args(["--robot", "192.168.110.51", "--domain", "0", "--skip-python"])
        for cross, build_only in ((False, False), (False, True), (True, False)):
            self.config["cross"] = cross
            self.args.build_only = build_only
            with self.subTest(cross=cross, build_only=build_only), ExitStack() as stack:
                _, _, output, _ = self.deployment_mocks(stack)
                configure = stack.enter_context(patch("deploy.configure_robot"))
                deploy.deploy(self.args, self.installer)
                self.assertEqual(configure.call_count, int(not cross and not build_only))
                self.assertIn(str(self.root / "CodeProject/AheadFormSDK_Config.ini"), output.getvalue())

    def test_cross_deploy_defers_target_python_configuration_to_manual_make(self):
        self.config["cross"] = True
        with ExitStack() as stack, patch.dict(os.environ, {}, clear=True):
            cache, run, output, errors = self.deployment_mocks(stack)
            stack.enter_context(patch("python_env.prepare", return_value=Path("python")))
            deploy.deploy(self.args, self.installer)
            self.assertIn("make PYTHON_CONFIG=", output.getvalue())
            self.assertIn("SDK 运行方法（在目标设备上执行）", output.getvalue())
            self.assertIn("python3 demo_user.py", output.getvalue())
            self.assertNotIn(".venv/bin/python demo_user.py", output.getvalue())
            self.assertNotIn("准备失败", output.getvalue())
            self.assertIn("Python/CFFI 环境：准备成功（宿主环境）。", output.getvalue())
            self.assertTrue((cache / "active.mk").exists())
            run.assert_not_called()

    def test_invalid_wheelhouse_does_not_block_cpp_deployment(self):
        self.args.python_wheelhouse = str(self.root / "missing-wheels")
        with ExitStack() as stack:
            cache, _, output, errors = self.deployment_mocks(stack)
            deploy.deploy(self.args, self.installer)
            self.assertIn("existing directory", errors.getvalue())
            self.assertIn("C/C++ 客户（环境已就绪）", output.getvalue())
            self.assertTrue((cache / "deploy-settings.json").exists())

    def test_explicit_skip_never_prepares_or_builds_python(self):
        self.args.skip_python = True
        with ExitStack() as stack:
            _, _, output, _ = self.deployment_mocks(stack)
            prepare = stack.enter_context(patch("python_env.prepare"))
            deploy.deploy(self.args, self.installer)
            prepare.assert_not_called()
            self.assertIn("Python 客户：已跳过", output.getvalue())
            result = output.getvalue().split("部署结果：\n", 1)[1].split("\n后续操作指南：", 1)[0]
            self.assertEqual(result.strip(),
                             "C/C++ 依赖环境：准备成功。\n  Python/CFFI 环境：按 --skip-python 跳过。")

    def test_optional_phase_does_not_swallow_cancellation(self):
        for error in (deploy.DeploymentInterrupted(), KeyboardInterrupt(), SystemExit(2)):
            with self.subTest(error=type(error).__name__), ExitStack() as stack:
                cache, _, _, _ = self.deployment_mocks(stack)
                stack.enter_context(patch("python_env.prepare", side_effect=error))
                with self.assertRaises(type(error)):
                    deploy.deploy(self.args, self.installer)
                self.assertFalse((cache / "deploy-settings.json").exists())

    def test_cpp_failure_is_fatal_before_optional_python_phase(self):
        with ExitStack() as stack:
            cache, run, _, _ = self.deployment_mocks(stack)
            stack.enter_context(patch("dds.link_probe", side_effect=RuntimeError("DDS link probe failed")))
            prepare = stack.enter_context(patch("python_env.prepare"))
            with self.assertRaisesRegex(RuntimeError, "DDS link probe failed"):
                deploy.deploy(self.args, self.installer)
            prepare.assert_not_called()
            self.assertFalse((cache / "deploy-settings.json").exists())

    def test_deployment_and_plain_make_flags_share_stamp_but_quoted_changes_rebuild(self):
        stamp = self.root / "build-settings"
        command = [sys.executable, str(ROOT / "python_demo/build_bindings.py"), "stamp", str(stamp)]
        environment = os.environ.copy()
        environment["AF_BUILD_CFLAGS"] = "-DNAME='a b' -pthread"
        subprocess.run(command, env=environment, check=True)
        original, timestamp = stamp.read_bytes(), stamp.stat().st_mtime_ns
        environment["AF_BUILD_CFLAGS"] = "  -DNAME='a b'   -pthread  "
        subprocess.run(command, env=environment, check=True)
        self.assertEqual(stamp.read_bytes(), original)
        self.assertEqual(stamp.stat().st_mtime_ns, timestamp)
        environment["AF_BUILD_CFLAGS"] = "-DNAME='a  b' -pthread"
        subprocess.run(command, env=environment, check=True)
        self.assertNotEqual(stamp.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
