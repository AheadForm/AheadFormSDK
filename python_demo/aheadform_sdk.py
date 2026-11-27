"""CFFI access to the existing SDK. Importing never initializes a robot."""

from dataclasses import dataclass
from enum import IntEnum
from functools import wraps
import importlib.util
from importlib.machinery import EXTENSION_SUFFIXES
import json
import math
import os
from pathlib import Path
import sys
import threading

DEMO_ROOT = Path(__file__).resolve().parent
DEFAULT_CONFIG = DEMO_ROOT.parent / "CodeProject" / "AheadFormSDK_Config.ini"
RET_SUCCESS = 0
RET_SKIPPED = 1
RET_EXIT = 2
_lock = threading.RLock()
_loaded_directory = None
_api_module = None
_user_module = None
_active = None
_terminal = False


class SDKError(RuntimeError):
    def __init__(self, operation, code, fatal=False):
        self.code = int(code)
        self.unsigned_code = self.code & 0xFFFFFFFF
        self.fatal = fatal
        message = "{} failed: 0x{:08X}".format(operation, self.unsigned_code)
        if fatal:
            message += "; stop this process; do not retry Init/Deinit"
        super().__init__(message)


class OperatingMode(IntEnum):
    UNKNOWN = 0
    DEV = 1
    INTERACTIVE = 2


@dataclass(frozen=True)
class Volume:
    microphone_percent: float
    speaker_percent: float


@dataclass(frozen=True)
class NeckState:
    receive_time_us: int
    last_accepted_target_id: int
    pitch_rad: float
    yaw_rad: float
    roll_rad: float
    state: int
    err_code: int
    is_ready: bool
    is_fault_active: bool
    is_target_reached: bool
    is_pose_valid: bool


def _text(value):
    if isinstance(value, (bytes, os.PathLike)):
        encoded = os.fsencode(value)
    elif isinstance(value, str):
        encoded = value.encode("utf-8")
    else:
        raise TypeError("expected str, bytes or a path")
    if b"\0" in encoded:
        raise ValueError("C strings must not contain NUL")
    return encoded


def _library_directory(directory):
    if directory is not None:
        return Path(directory).resolve()
    try:
        selection = (DEMO_ROOT / ".build" / "active").read_text().strip()
    except FileNotFoundError:
        raise FileNotFoundError("Build first: make -C {} api".format(DEMO_ROOT)) from None
    if not selection or Path(selection).name != selection or selection in (".", ".."):
        raise RuntimeError("Invalid Python build selection; rebuild python_demo")
    return (DEMO_ROOT / ".build" / selection).resolve()


def _load_extension(name, directory):
    try:
        import cffi
    except ImportError as error:
        raise RuntimeError("CFFI is missing; install python_demo/requirements.txt in this Python environment") from error
    metadata = directory / "build-info.json"
    if metadata.is_file():
        info = json.loads(metadata.read_text())
        if cffi.__version__ != info["cffi"]:
            raise RuntimeError("CFFI version differs from build; install the matching requirements.txt")
        if info["suffix"] not in EXTENSION_SUFFIXES:
            raise RuntimeError("Extension does not match this Python ABI; rebuild for the selected interpreter")
    paths = [directory / (name + suffix) for suffix in EXTENSION_SUFFIXES
             if (directory / (name + suffix)).is_file()]
    if len(paths) != 1:
        raise FileNotFoundError("Build exactly one {} extension in {}".format(name, directory))
    path = paths[0].resolve()
    previous = sys.modules.get(name)
    if previous is not None:
        if Path(previous.__file__).resolve() != path:
            raise RuntimeError("A different SDK extension is already loaded; start a new Python process")
        return previous
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ImportError("Extension does not match this Python ABI: " + str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    sys.modules[name] = module
    return module


class AheadFormSDK:
    """One active control object per process; native calls through this class serialize."""

    def __init__(self, config=DEFAULT_CONFIG, use_user=False, library_dir=None):
        global _loaded_directory, _api_module, _user_module
        self.config = Path(config).resolve()
        self._state = "new"
        self._context = None
        self._config_buffer = None
        self._directory = _library_directory(library_dir)
        with _lock:
            if _loaded_directory is not None and _loaded_directory != self._directory:
                raise RuntimeError("A different SDK build is already loaded; start a new Python process")
            if _api_module is None:
                _api_module = _load_extension("_aheadform_api", self._directory)
                _loaded_directory = self._directory
            self.ffi = _api_module.ffi
            self._lib = _api_module.lib
            self._user = None
            if use_user:
                if _user_module is None:
                    _user_module = _load_extension("_aheadform_user", self._directory)
                self._user = _user_module.lib

    @staticmethod
    def _check(code, operation, fatal=False):
        if code != RET_SUCCESS:
            raise SDKError(operation, code, fatal)

    def _signed(self, code):
        return int(self.ffi.cast("int", code))

    def _require_ready(self):
        if self._state != "ready" or _active is not self or _terminal:
            raise RuntimeError("This SDK instance is not ready; open it successfully before use")

    def open(self, config=None):
        global _active, _terminal
        with _lock:
            if _terminal:
                raise RuntimeError("SDK Init/Deinit previously failed; stop this process")
            if self._state == "ready":
                return self
            if _active is not None:
                raise RuntimeError("Another SDK instance is already active")
            path = self.config if config is None else Path(config).resolve()
            if not path.is_file():
                raise FileNotFoundError(path)
            # Keep the complete object and config alive even if Init partially fails.
            self._context = self.ffi.new("AHEADFORM_ALL_CTRL *")
            self._config_buffer = self.ffi.new("char[]", _text(path))
            _active = self
            self._state = "failed"
            _terminal = True
            code = self._lib.AheadForm_Init(self._context, self._config_buffer)
            self._check(code, "AheadForm_Init", fatal=True)
            self.config = path
            self._state = "ready"
            _terminal = False
            return self

    def close(self):
        global _active, _terminal
        with _lock:
            if self._state != "ready":
                return
            # A failed or interrupted teardown is terminal; retain all managed memory.
            self._state = "failed"
            _terminal = True
            code = self._lib.AheadForm_Deinit(self._context)
            self._check(code, "AheadForm_Deinit", fatal=True)
            self._state = "closed"
            self._context = None
            self._config_buffer = None
            _active = None
            _terminal = False

    def __enter__(self):
        return self.open()

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def get_handle(self, module="all"):
        """Borrowed CFFI pointer, valid only until successful close; never free it."""
        fields = {
            "config": "stCfg", "topics": "stTopics",
            "head": "stRobotCtrl.stHeadCtrl", "media": "stRobotCtrl.stMediaCtrl",
            "audio": "stRobotCtrl.stMediaCtrl.stAudioCtrl",
            "video": "stRobotCtrl.stMediaCtrl.stVideoCtrl", "status": "stRobotCtrl.stStatus",
            "operating_mode": "stRobotCtrl.stOperatingModeCtrl",
            "emote_play": "stRobotCtrl.stEmotePlayCtrl",
        }
        with _lock:
            self._require_ready()
            if module == "all":
                return self._context
            if module not in fields:
                raise ValueError("Unknown SDK module: " + str(module))
            value = self._context[0]
            path = fields[module].split(".")
            for field in path[:-1]:
                value = getattr(value, field)
            return self.ffi.addressof(value, path[-1])

    def bind(self, name):
        """Use a function declared in bindings.py; return its native result unchanged."""
        if name in ("AheadForm_Init", "AheadForm_Deinit"):
            raise ValueError("Use open()/close() for SDK lifecycle")
        with _lock:
            native = getattr(self._lib, name)
            if self.ffi.typeof(native).kind not in ("function", "pointer"):
                raise TypeError("Not a declared C function: " + name)

        @wraps(native)
        def call(*args):
            with _lock:
                self._require_ready()
                return native(*args)
        return call

    def query_volume(self, timeout_ms=200):
        with _lock:
            handle = self.get_handle("audio")
            reply = self.ffi.new("AHEADFORM_VOLUME_REPLY *")
            volume = self.ffi.new("AHEADFORM_VOLUME *")
            self._check(self._lib.Audio_QueryVolumeAndWaitReply(handle, reply, timeout_ms), "query_volume")
            self._check(self._lib.Audio_GetVolumeFromReply(reply, volume), "query_volume")
            return Volume(volume.micVolumePercent, volume.spkVolumePercent)

    def get_operating_mode(self, timeout_ms=3000):
        with _lock:
            handle = self.get_handle("operating_mode")
            state = self.ffi.new("AHEADFORM_OPERATING_MODE_STATE *")
            self._check(self._lib.OperatingMode_ReadStateSync(handle, state, timeout_ms), "get_operating_mode")
            return OperatingMode(state.mode)

    def set_operating_mode(self, mode, timeout_ms=3000):
        if isinstance(mode, str):
            mode = OperatingMode[mode.upper()]
        mode = OperatingMode(mode)
        if mode == OperatingMode.UNKNOWN:
            raise ValueError("mode must be DEV or INTERACTIVE")
        with _lock:
            handle = self.get_handle("operating_mode")
            request = self.ffi.new("AHEADFORM_OPERATING_MODE_ASK *")
            reply = self.ffi.new("AHEADFORM_OPERATING_MODE_REPLY *")
            self._check(self._lib.OperatingMode_BuildRequest(handle, int(mode), request), "set_operating_mode")
            self._check(self._lib.OperatingMode_SubmitAsyncRequestAndWaitReply(handle, request, reply, timeout_ms), "set_operating_mode")
            if reply.result != self._lib.AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED:
                code = self._lib.ERRCODE_REQUEST_REJECTED if reply.result == self._lib.AHEADFORM_OPERATING_MODE_RESULT_FAILED else self._lib.ERRCODE_DATA_PACKET_FORMAT
                self._check(self._signed(code), "set_operating_mode")

    def read_latest_neck_state(self):
        with _lock:
            handle = self.get_handle("status")
            result = self.ffi.new("AHEADFORM_NECK_STATE *")
            code = self._lib.Status_ReadLatestNeckState(handle, result)
            if code == 0:
                return None
            if code != 1:
                raise SDKError("read_latest_neck_state", code)
            return NeckState(result.ReceiveTime_us, result.lastAcceptedTargetId,
                             result.stPose.pitch_rad, result.stPose.yaw_rad, result.stPose.roll_rad,
                             result.state, result.errCode, bool(result.isReady), bool(result.isFaultActive),
                             bool(result.isTargetReached), bool(result.isPoseValid))

    def send_neck_pose(self, pitch_rad, yaw_rad, roll_rad):
        values = [float(self.ffi.cast("float", value)) for value in (pitch_rad, yaw_rad, roll_rad)]
        if not all(math.isfinite(value) for value in values):
            raise ValueError("neck angles must be finite float values in radians")
        with _lock:
            handle = self.get_handle("head")
            self._check(self._lib.Head_Send1NeckPose(handle, *values), "send_neck_pose")

    def _commands(self):
        if self._user is None:
            raise RuntimeError("Create AheadFormSDK(use_user=True) to enable User commands")
        entries = []
        for entry in self._user.gUserInTaskAry:
            if entry.pFunction == self.ffi.NULL:
                break
            entries.append(entry)
        return entries

    def commands(self):
        """Read the existing command table without initializing the robot."""
        with _lock:
            return {self.ffi.string(entry.strCmd).decode("utf-8"):
                    self.ffi.string(entry.strDescription).decode("utf-8") if entry.strDescription != self.ffi.NULL else ""
                    for entry in self._commands()}

    def execute(self, command, *args, report_result=False):
        """Call a User handler; optionally print its result through the C menu."""
        encoded = [_text(command)] + [_text(arg if isinstance(arg, (bytes, os.PathLike)) else str(arg)) for arg in args]
        if not encoded[0]:
            raise ValueError("command must not be empty")
        with _lock:
            entries = self._commands()
            if encoded[0].lower() not in (b"help", b"exit"):
                self._require_ready()
            if len(encoded) > self._user.MAX_ARG_CNT:
                code = self._signed(self._lib.ERRCODE_ARGC)
            else:
                entry = next((item for item in entries if self.ffi.string(item.strCmd).lower() == encoded[0].lower()), None)
                if entry is None:
                    code = self._signed(self._lib.ERRCODE_ID_NOT_FOUND)
                else:
                    buffers = [self.ffi.new("char[]", value) for value in encoded]
                    argv = self.ffi.new("char *[]", buffers + [self.ffi.NULL])
                    context = self._context if self._state == "ready" else self.ffi.NULL
                    code = entry.pFunction(context, len(encoded), argv)
            if report_result:
                self._user.PrintCmdResult(self.ffi.new("char[]", encoded[0]), code)
                self._user.fflush(self._user.stdout)
            return code
