"""Compile generated CFFI bindings against fake SDK/User implementations, never a robot."""

from pathlib import Path
import os
import shlex
import subprocess
import sys
import sysconfig
import tempfile
import textwrap
import unittest


DEMO = Path(__file__).resolve().parents[1]
ROOT = DEMO.parent

FAKE_API = r'''
#include "AheadForm_Task.h"
#include <unistd.h>
unsigned long long GetTimeUsCnt_UpToNow(void) { return 1234567890123ULL; }
static int results[9] = {0, 0, 0, 0, 0, 0, 0, 1, 0};
static int calls[9];
static int replyResult = AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED;
static AHEADFORM_ALL_CTRL *context;
static int concurrent, maxConcurrent;
void Probe_SetResult(int index, int value) { results[index] = value; }
void Probe_SetReplyResult(int value) { replyResult = value; }
int Probe_Calls(int index) { return calls[index]; }
int Probe_MaxConcurrent(void) { return maxConcurrent; }
int Probe_ContextMatches(AHEADFORM_ALL_CTRL *value) { return context == value; }
int AheadForm_Init(AHEADFORM_ALL_CTRL *value, const char *path)
{ calls[0]++; context = value; return results[0]; }
int AheadForm_Deinit(AHEADFORM_ALL_CTRL *value)
{ calls[1]++; return results[1]; }
int Audio_QueryVolumeAndWaitReply(AHEADFORM_AUDIO_CTRL *value, AHEADFORM_VOLUME_REPLY *reply, int timeout)
{ calls[2]++; return results[2]; }
int Audio_GetVolumeFromReply(const AHEADFORM_VOLUME_REPLY *reply, AHEADFORM_VOLUME *volume)
{ calls[3]++; volume->micVolumePercent = 25; volume->spkVolumePercent = 75; return results[3]; }
int OperatingMode_ReadStateSync(AHEADFORM_OPERATING_MODE_CTRL *value, AHEADFORM_OPERATING_MODE_STATE *state, int timeout)
{ calls[4]++; state->mode = AHEADFORM_OPERATING_MODE_DEV; return results[4]; }
int OperatingMode_BuildRequest(AHEADFORM_OPERATING_MODE_CTRL *value, ENUM_AHEADFORM_OPERATING_MODE mode, AHEADFORM_OPERATING_MODE_ASK *request)
{ calls[5]++; return results[5]; }
int OperatingMode_SubmitAsyncRequestAndWaitReply(AHEADFORM_OPERATING_MODE_CTRL *value, const AHEADFORM_OPERATING_MODE_ASK *request, AHEADFORM_OPERATING_MODE_REPLY *reply, int timeout)
{ calls[6]++; reply->result = replyResult; return results[6]; }
int Status_ReadLatestNeckState(AHEADFORM_STATUS *value, AHEADFORM_NECK_STATE *state)
{
    calls[7]++;
    if (results[7] != 1) return results[7];
    memset(state, 0, sizeof(*state));
    state->ReceiveTime_us = 1234567890123ULL;
    state->lastAcceptedTargetId = 9876543210123ULL;
    state->state = AHEADFORM_NECK_STATE_MOTION_TARGET_ENABLED;
    state->isReady = 1; state->isPoseValid = 1; state->isTargetReached = 1;
    state->stPose.pitch_rad = 0.25; state->stPose.yaw_rad = -0.5; state->stPose.roll_rad = 0.75;
    return 1;
}
int Head_Send1NeckPose(AHEADFORM_HEAD_CTRL *value, float pitch, float yaw, float roll)
{
    int active = __sync_add_and_fetch(&concurrent, 1);
    if (active > maxConcurrent) maxConcurrent = active;
    usleep(10000);
    __sync_sub_and_fetch(&concurrent, 1);
    calls[8]++;
    return results[8];
}
'''

FAKE_USER = r'''
#include "UserCmdMenu.h"
#include <stdlib.h>
extern int Probe_ContextMatches(AHEADFORM_ALL_CTRL *value);
int PrintCmdResult(char *command, int code)
{
    printf("USER_RESULT[%s]=0x%08X\n", command, (unsigned int)code);
    return 0;
}
static int Help(AHEADFORM_ALL_CTRL *value, int argc, char **argv) { return 0; }
static int Echo(AHEADFORM_ALL_CTRL *value, int argc, char **argv)
{
    if (argc != 2 || !Probe_ContextMatches(value)) return ERRCODE_ARGV;
    if (strcmp(argv[1], "file with spaces.csv") != 0) return ERRCODE_ARGV;
    argv[1][0] = 'X';
    return 0;
}
static int ExitOnError(AHEADFORM_ALL_CTRL *value, int argc, char **argv) { exit(19); }
static int Exit(AHEADFORM_ALL_CTRL *value, int argc, char **argv) { return RET_EXIT; }
const USERIN_TASK gUserInTaskAry[USERIN_CMD_CNT_MAX] = {
    {"help", Help, "help"}, {"Echo", Echo, "echo"},
    {"ExitOnError", ExitOnError, "fatal path"}, {"exit", Exit, "exit"}, {"", NULL}
};
'''


class PythonSDKTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="aheadform-python-test-")
        cls.directory = Path(cls.temp.name)
        cls.config = cls.directory / "test.ini"
        cls.config.write_text("; Fake SDK does not read robot configuration.\n")
        flags = subprocess.check_output(
            ["make", "--no-print-directory", "-s", "-f", "subMakefile_buildApiLib", "print-cflags"],
            cwd=ROOT / "CodeProject" / "API", text=True)
        flags = shlex.split(flags)
        compiler = shlex.split(os.environ.get("CC", "cc"))
        common = compiler + ["-fPIC", "-shared", "-Wall", "-Werror"] + flags
        common += ["-I" + str(ROOT / "CodeProject" / "User")]
        api_source = cls.directory / "fake_api.c"
        user_source = cls.directory / "fake_user.c"
        api_source.write_text(FAKE_API)
        user_source.write_text(FAKE_USER)
        subprocess.run(common + [str(api_source), "-Wl,-z,defs", "-Wl,-soname,libAheadFormSDKApi.so",
                                 "-o", str(cls.directory / "libAheadFormSDKApi.so")], check=True)
        sys.path.insert(0, str(DEMO))
        from bindings import api_builder, user_builder
        api = api_builder("""
            void Probe_SetResult(int, int);
            void Probe_SetReplyResult(int);
            int Probe_Calls(int);
            int Probe_MaxConcurrent(void);
            int Probe_ContextMatches(AHEADFORM_ALL_CTRL *);
        """)
        # Include probe prototypes through the fake header, without handwritten wrappers.
        prototypes = cls.directory / "probe.h"
        prototypes.write_text('#include "AheadForm_Task.h"\nvoid Probe_SetResult(int,int);\nvoid Probe_SetReplyResult(int);\nint Probe_Calls(int);\nint Probe_MaxConcurrent(void);\nint Probe_ContextMatches(AHEADFORM_ALL_CTRL *);\n')
        suffix = sysconfig.get_config_var("EXT_SUFFIX")
        includes = ["-I" + sysconfig.get_path("include")]
        for name, builder in (("_aheadform_api", api), ("_aheadform_user", user_builder(api))):
            source = cls.directory / (name + ".c")
            builder.emit_c_code(str(source))
            sources = [str(source)] + ([str(user_source)] if name.endswith("user") else [])
            subprocess.run(common + includes + ["-include", str(prototypes)] + sources +
                           ["-L" + str(cls.directory), "-lAheadFormSDKApi", "-Wl,-rpath,$ORIGIN",
                            "-o", str(cls.directory / (name + suffix))], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def run_case(self, body, expected_return=0, use_user=False):
        setup = """
            import gc
            import math
            import threading
            import weakref
            from aheadform_sdk import AheadFormSDK, SDKError, OperatingMode
            sdk = AheadFormSDK(config=CONFIG, use_user=USE_USER, library_dir=LIBRARY)
            ffi = sdk.ffi
            def probe(name):
                return getattr(sdk._lib, name)
            def set_result(index, value):
                sdk._lib.Probe_SetResult(index, int(ffi.cast('int', value)))
            calls = probe('Probe_Calls')
        """
        prefix = "CONFIG = {!r}\nLIBRARY = {!r}\nUSE_USER = {!r}\n".format(
            str(self.config), str(self.directory), use_user)
        process = subprocess.run([sys.executable, "-c", prefix + textwrap.dedent(setup) + textwrap.dedent(body)],
                                 cwd=self.directory, env=dict(os.environ, PYTHONPATH=str(DEMO) + os.pathsep + os.environ.get("PYTHONPATH", "")),
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=20)
        self.assertEqual(process.returncode, expected_return, process.stdout + process.stderr)
        return process

    def test_values_bindings_and_close(self):
        self.run_case("""
            with sdk:
                assert sdk.bind('GetTimeUsCnt_UpToNow')() == 1234567890123
                for name in ('all', 'config', 'topics', 'head', 'media', 'audio', 'video', 'status', 'operating_mode', 'emote_play'):
                    assert ffi.typeof(sdk.get_handle(name)).kind == 'pointer'
                volume = sdk.query_volume()
                assert volume.microphone_percent == 25 and volume.speaker_percent == 75
                assert sdk.get_operating_mode() == OperatingMode.DEV
                sdk.set_operating_mode('dev')
                state = sdk.read_latest_neck_state()
                assert state.receive_time_us == 1234567890123
                assert state.last_accepted_target_id == 9876543210123
                assert state.pitch_rad == 0.25 and state.yaw_rad == -0.5 and state.roll_rad == 0.75
                assert state.is_ready and state.is_pose_valid and state.is_target_reached
                function = sdk.bind('Head_Send1NeckPose')
                assert function(sdk.get_handle('head'), 0.1, 0.2, 0.3) == 0
                try:
                    sdk.send_neck_pose(math.nan, 0, 0)
                    assert False
                except ValueError:
                    pass
            sdk.close()
            assert calls(1) == 1
            try:
                function(ffi.NULL, 0, 0, 0)
                assert False
            except RuntimeError:
                pass
        """)

    def test_timeouts_replies_and_no_data(self):
        self.run_case("""
            with sdk:
                set_result(2, 0xA4000000)
                try:
                    sdk.query_volume()
                    assert False
                except SDKError as error:
                    assert error.unsigned_code == 0xA4000000 and not error.fatal
                assert calls(3) == 0
                set_result(2, 0)
                set_result(3, 0xC0000008)
                try:
                    sdk.query_volume()
                    assert False
                except SDKError as error:
                    assert error.unsigned_code == 0xC0000008
                set_result(7, 0)
                assert sdk.read_latest_neck_state() is None
                value = ffi.new('AHEADFORM_NECK_STATE *')
                value.ReceiveTime_us = 77
                assert sdk.bind('Status_ReadLatestNeckState')(sdk.get_handle('status'), value) == 0
                assert value.ReceiveTime_us == 77
                set_result(7, 0xC0000001)
                try:
                    sdk.read_latest_neck_state()
                    assert False
                except SDKError as error:
                    assert error.unsigned_code == 0xC0000001
                set_reply = probe('Probe_SetReplyResult')
                set_reply(2)
                try:
                    sdk.set_operating_mode('dev')
                    assert False
                except SDKError as error:
                    assert error.unsigned_code == 0xC0000008
        """)

    def test_failed_init_is_terminal_without_deinit(self):
        self.run_case("""
            set_result(0, 0xA0010000)
            try:
                sdk.open()
                assert False
            except SDKError as error:
                assert error.fatal
            sdk.close()
            try:
                sdk.open()
                assert False
            except RuntimeError:
                pass
            assert calls(0) == 1 and calls(1) == 0
        """)

    def test_failed_deinit_is_not_retried(self):
        self.run_case("""
            sdk.open()
            set_result(1, 0xA0400000)
            try:
                sdk.close()
                assert False
            except SDKError as error:
                assert error.fatal
            sdk.close()
            assert calls(1) == 1
            try:
                sdk.open()
                assert False
            except RuntimeError:
                pass
        """)

    def test_singleton_and_serial_calls(self):
        self.run_case("""
            other = AheadFormSDK(config=CONFIG, library_dir=LIBRARY)
            with sdk:
                try:
                    other.open()
                    assert False
                except RuntimeError:
                    pass
                workers = [threading.Thread(target=sdk.send_neck_pose, args=(0, 0, 0)) for i in range(4)]
                for worker in workers: worker.start()
                for worker in workers: worker.join()
                assert calls(8) == 4
                assert probe('Probe_MaxConcurrent')() == 1
            with other:
                assert other.get_operating_mode() == OperatingMode.DEV
        """)

    def test_user_arguments_table_and_status(self):
        self.run_case("""
            assert 'Echo' in sdk.commands()
            assert sdk.execute('HELP') == 0 and sdk.execute('exit') == 2
            assert calls(0) == 0
            try:
                sdk.execute('Echo', 'file with spaces.csv')
                assert False
            except RuntimeError:
                pass
            with sdk:
                argument = b'file with spaces.csv'
                assert sdk.execute('echo', argument) == 0
                assert argument == b'file with spaces.csv'
                assert sdk.execute('not-a-command') & 0xFFFFFFFF == 0x92000000
                assert sdk.execute('Echo', *(['x'] * 64)) & 0xFFFFFFFF == 0x90000001
                try:
                    sdk.execute('Echo', 'bad\\0argument')
                    assert False
                except ValueError:
                    pass
        """, use_user=True)

    def test_user_result_reporting_uses_c_menu_and_keeps_return_values(self):
        process = self.run_case("""
            assert sdk.execute('help') == 0  # Programmatic calls do not print a result by default.
            assert sdk.execute('HELP', report_result=True) == 0
            with sdk:
                assert sdk.execute('Echo', 'file with spaces.csv', report_result=True) == 0
                assert sdk.execute('Echo', report_result=True) & 0xFFFFFFFF == 0x90000040
                assert sdk.execute('Echo', *(['x'] * 64), report_result=True) & 0xFFFFFFFF == 0x90000001
                assert sdk.execute('unknown', report_result=True) & 0xFFFFFFFF == 0x92000000
                print('BEFORE_EXIT', flush=True)
                assert sdk.execute('exit', report_result=True) == 2
        """, use_user=True)
        results = [line for line in process.stdout.splitlines() if line.startswith('USER_RESULT[')]
        self.assertEqual(results, [
            'USER_RESULT[HELP]=0x00000000', 'USER_RESULT[Echo]=0x00000000',
            'USER_RESULT[Echo]=0x90000040', 'USER_RESULT[Echo]=0x90000001',
            'USER_RESULT[unknown]=0x92000000', 'USER_RESULT[exit]=0x00000002'])
        self.assertLess(process.stdout.index('USER_RESULT[unknown]'), process.stdout.index('BEFORE_EXIT'))

    def test_interactive_reuses_session_and_quoted_arguments(self):
        self.run_case("""
            import contextlib
            import io
            from unittest.mock import patch
            import demo_user
            errors = io.StringIO()
            with contextlib.redirect_stderr(errors), patch('builtins.input', side_effect=[
                'help', 'eChO "file with spaces.csv"', "Echo 'file with spaces.csv'", 'EXIT'
            ]) as prompt:
                assert demo_user._interactive(sdk) == 0
            assert all(call.args == ('>> ',) for call in prompt.call_args_list)
            assert prompt.call_count == 4 and errors.getvalue() == ''
            assert calls(0) == 1 and calls(1) == 1
        """, use_user=True)

    def test_interactive_recovers_from_invalid_input_and_command_result(self):
        self.run_case("""
            import contextlib
            import io
            from unittest.mock import patch
            import demo_user
            errors = io.StringIO()
            with contextlib.redirect_stderr(errors), patch('builtins.input', side_effect=[
                '  ', 'Echo "unclosed', 'unknown', 'Echo', 'Echo bad\\0argument',
                'Echo "file with spaces.csv"', 'exit'
            ]):
                assert demo_user._interactive(sdk) == 0
            message = errors.getvalue()
            assert 'unknown' in message and 'User ' in message and '0x' in message
            assert 'unclosed' not in message  # shlex reports the syntax, not a command execution.
            assert calls(0) == 1 and calls(1) == 1
        """, use_user=True)

    def test_interactive_initializes_before_menu_and_input(self):
        self.run_case("""
            from unittest.mock import patch
            import demo_user
            original_commands = sdk.commands
            for count, inputs in enumerate((
                ['help', 'exit'],
                ['', 'unknown', 'Echo "unclosed', EOFError],
                [KeyboardInterrupt],
            ), start=1):
                pending = iter(inputs)
                def check_ready(*args, **kwargs):
                    assert calls(0) == count and calls(1) == count - 1
                    sdk.get_handle()  # Initialization must have completed successfully.
                def read_commands():
                    check_ready()
                    return original_commands()
                def read_input(prompt):
                    check_ready()
                    assert prompt == '>> '
                    value = next(pending)
                    if isinstance(value, type) and issubclass(value, BaseException):
                        raise value
                    return value
                with patch.object(demo_user, 'print', side_effect=check_ready) as output, \\
                     patch.object(sdk, 'commands', side_effect=read_commands) as menu, \\
                     patch('builtins.input', side_effect=read_input) as prompt:
                    expected = 130 if inputs == [KeyboardInterrupt] else 0
                    assert demo_user._interactive(sdk) == expected
                assert output.called and prompt.called and menu.call_count == 1
                assert calls(0) == count and calls(1) == count
        """, use_user=True)

    def test_interactive_eof_and_interrupt_release_session(self):
        for exception, result in (('EOFError', 0), ('KeyboardInterrupt', 130)):
            with self.subTest(exception=exception):
                self.run_case("""
                    from unittest.mock import patch
                    import demo_user
                    with patch('builtins.input', side_effect=['Echo "file with spaces.csv"', EXCEPTION]):
                        assert demo_user._interactive(sdk) == RESULT
                    assert calls(0) == 1 and calls(1) == 1
                """.replace('EXCEPTION', exception).replace('RESULT', str(result)), use_user=True)

    def test_interactive_lifecycle_failure_stops_without_retry(self):
        for index in (0, 1):
            with self.subTest(failure='init' if index == 0 else 'deinit'):
                self.run_case("""
                    import contextlib
                    import io
                    from unittest.mock import patch
                    import demo_user
                    set_result(INDEX, 0xA0010000)
                    errors = io.StringIO()
                    output = io.StringIO()
                    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors), \\
                         patch.object(demo_user, 'AheadFormSDK', return_value=sdk), \\
                         patch.object(sdk, 'commands', wraps=sdk.commands) as menu, \\
                         patch.object(sdk, 'execute', wraps=sdk.execute) as execute, \\
                         patch('sys.argv', ['demo_user.py']), \\
                         patch('builtins.input', side_effect=['Echo "file with spaces.csv"', 'exit']) as prompt:
                        assert demo_user.main() == 1
                    assert errors.getvalue() and calls(0) == 1 and calls(1) == INDEX
                    assert bool(output.getvalue()) == bool(INDEX)
                    assert prompt.call_count == INDEX * 2
                    assert menu.call_count == INDEX and execute.call_count == INDEX * 2
                    sdk.close()
                    assert calls(1) == INDEX
                """.replace('INDEX', str(index)), use_user=True)

    def test_user_cli_keeps_list_and_single_command(self):
        self.run_case("""
            from unittest.mock import patch
            import demo_user
            with patch.object(demo_user, 'AheadFormSDK', return_value=sdk), patch('builtins.input') as prompt:
                for arguments in (['--list'], ['help'], ['exit']):
                    with patch('sys.argv', ['demo_user.py'] + arguments):
                        assert demo_user.main() == 0
                assert calls(0) == 0 and calls(1) == 0
                with patch('sys.argv', ['demo_user.py', 'Echo', 'file with spaces.csv']):
                    assert demo_user.main() == 0
                assert calls(0) == 1 and calls(1) == 1
                prompt.assert_not_called()
        """, use_user=True)

    def test_active_context_survives_python_references(self):
        self.run_case("""
            sdk.open()
            reference = weakref.ref(sdk)
            context = sdk.get_handle()
            del sdk
            gc.collect()
            owner = reference()
            assert owner is not None and owner._context == context
            assert owner._lib.Probe_ContextMatches(owner._context) == 1
            owner.close()
            assert owner._context is None
        """)

    def test_failed_context_is_retained(self):
        for failure in ('init', 'deinit'):
            self.run_case("""
                if FAILURE == 'init':
                    set_result(0, 0xA0010000)
                else:
                    sdk.open()
                    set_result(1, 0xA0400000)
                try:
                    sdk.open() if FAILURE == 'init' else sdk.close()
                    assert False
                except SDKError:
                    pass
                reference = weakref.ref(sdk)
                context = sdk._context
                del sdk
                gc.collect()
                owner = reference()
                assert owner is not None and owner._context == context
                other = AheadFormSDK(config=CONFIG, library_dir=LIBRARY)
                try:
                    other.open()
                    assert False
                except RuntimeError:
                    pass
            """.replace('FAILURE', repr(failure)))

    def test_original_user_exit_stays_in_child_process(self):
        self.run_case("""
            sdk.open()
            sdk.execute('ExitOnError')
            raise AssertionError('existing handler should terminate its process')
        """, expected_return=19, use_user=True)


if __name__ == "__main__":
    unittest.main()
