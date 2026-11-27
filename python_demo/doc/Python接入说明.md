# Python 接入说明

Python 使用 CFFI 编译模式（API mode），调用现有 `libAheadFormSDKApi.so`；可选 User 扩展直接复用现有命令表及 C/C++ 业务。不再维护手写 C 转发函数，也不保留 ctypes 接入。SDK 原有 GNU/Linux 平台边界不变，C/C++ 接入见 [SDK 快速上手](../../doc/AheadFormSDK快速上手.md#5-移植方法)。

## 1. 一次性准备与日常构建

先在 SDK 根目录成功部署一次：

```sh
bash deploy/deploy.sh
# 需要同时检查机器人网络、配置时改用现场参数，例如：
# bash deploy/deploy.sh --robot 192.168.110.51 --domain 50
```

默认部署不需要机器人在线，也不要求 IP、DDS 域或网卡参数，保留原 INI；部署末尾会打印 `CodeProject/AheadFormSDK_Config.ini` 的完整路径。执行业务示例前按目标机器人修改网卡、外部 DDS 域、机器人 IP 和媒体设置；切换机器人时修改配置并重启 Python 程序，无需重新部署。上面的 IP 和 DDS 域仅为可选自动配置的示例。本机构建无特殊要求时，在 C 编译器提示处回车或等待 2 分钟保留当前值；使用其他编译器时输入可执行文件名或完整路径。配套工具通常自动推导；交叉编译还须提供目标 sysroot 和 GStreamer 环境。详见[依赖部署与交叉编译](../../doc/依赖部署与交叉编译.md)。

默认部署构建或复用私有 DDS，准备所需工具和 GStreamer，再检查 Python 开发头文件、venv/pip，并在 `python_demo/.venv` 安装 `requirements.txt`；不构建 SDK 或 Python 扩展、不打印菜单。确认的编译器及参数会写入 `.deps/<工具链标识>/config.mk`，由 `deploy/deps.mk` 自动读取，也可在后者顶部明确填写覆盖项后重新部署。Python 环境或 CFFI 安装失败不阻止 C/C++ 依赖部署；核心依赖及所需配置成功时退出码为 0，先在“部署结果”中分行显示环境状态与具体失败原因，再显示“后续操作指南”。已有扩展及选择标记不被改动；补齐依赖后重跑，主动中断仍取消整个部署。手动 make 失败正常返回错误，不会破坏 C/C++ 依赖环境。

部署结尾给出 Python 编译命令后，手动构建再运行：

```sh
cd python_demo
. .venv/bin/activate
make -j4     # 首次及日常修改后，构建 API + User；仅用 API 时末尾加 api
python demo_user.py --list
```

`requirements.txt` 为 Python 3.8/3.9 选择 CFFI 1.17.1，为 Python 3.10 及以上选择 CFFI 2.1.1；解释器还须在该 CFFI 版本支持范围内。当前实际验证 CPython 3.10，其他解释器/版本不能视为已实测。构建端和运行端使用一致 CFFI 版本。部署使用运行脚本的 Python 创建私有环境；若客户使用的自定义 Python 无法由系统开发包补齐，请准备该版本对应的开发头文件和 venv/pip。

只需要 C/C++ 时可加 `--skip-python`，不会准备 Python 环境。之后需要 Python 时，重新运行不带此选项的部署命令即可。重复部署会复用有效虚拟环境及符合版本的 CFFI，不重复安装；创建环境或安装包被中断后可重跑部署；手动扩展编译被中断后可重新 make。`--skip-packages` 只禁止系统包管理器，不禁止 pip 联网；完全离线时另用 `--python-wheelhouse /本地包目录`，并预先准备系统依赖。离线步骤见下文。

| 命令（在 python_demo 内） | 行为 |
| --- | --- |
| `make PYTHON="$(command -v python)" api` | 仅构建 API 绑定，自动调用 API 子 Makefile 的 `api_so` |
| `make PYTHON="$(command -v python)"` / `make ... user` | 同时构建 API 和 User 绑定 |
| `make clean` | 删除本目录 `.build` 及标准 API 静态库、动态库和对象文件；保留 DDS 缓存及 `.venv`，不要求 CFFI 已安装 |
| `python -m unittest discover -s tests -v` | 本机编译替身并测试，不连接机器人 |

构建成功后，Makefile 末尾统一列出 CFFI 扩展的绝对路径，避免与链接后的成功消息重复；标准 API 动态库需要重建时，由 API 子 Makefile 单独报告一次绝对路径。运行提示采用 `运行方法： .venv/bin/python demo_user.py`，表示在 `python_demo` 目录执行该命令；`make api` 对应 `demo_api.py`，默认 `make` 或 `make user` 对应 `demo_user.py`。显式指定其他 `PYTHON` 时显示对应解释器；交叉构建提示在目标设备执行 `python3`。重复 make 无需重编时也显示这些提示。

本目录 Makefile 未指定 `PYTHON` 时优先使用 `.venv/bin/python`，没有该环境才使用 `python3`。表中显式指定的方式仍可用于客户自己的虚拟环境。激活 `.venv` 后可直接使用 `python` 运行示例；未激活时使用 `.venv/bin/python`，避免误用系统解释器。

**同一工具链与依赖只需成功部署一次。** 此后修改 API/User 源码、头文件或 `bindings.py` 后，重新执行相应 `make`；普通构建不会重新构建 DDS。原 `make -C CodeProject` 仍默认生成静态 API 库和 C/C++ 主程序，不构建 CFFI，也不需要本目录的 requirements；默认部署只额外准备 Python/CFFI 依赖，不构建扩展。

## 2. API 调用示例

在本目录运行：

```sh
python demo_api.py --query volume
```

此示例会初始化 SDK 并查询机器人；默认不切换模式、不发送动作。运行前核对配置、网络和机器人状态。`demo_user.py --list` 与 `demo_user.py help` 仅加载、枚举，不初始化机器人。

```python
from aheadform_sdk import AheadFormSDK, SDKError

try:
    with AheadFormSDK() as sdk:
        volume = sdk.query_volume(timeout_ms=200)
        print(volume.microphone_percent, volume.speaker_percent)
        print(sdk.get_operating_mode(timeout_ms=3000).name)
        state = sdk.read_latest_neck_state()
        if state is not None:
            print(state.is_pose_valid, state.pitch_rad, state.yaw_rad, state.roll_rad)
except SDKError as error:
    print("SDK 返回 0x{:08X}".format(error.unsigned_code))
    raise SystemExit(1)
```

默认 INI 为 SDK 内 `CodeProject/AheadFormSDK_Config.ini` 的绝对路径，不依赖启动目录。可用 `AheadFormSDK(config="/实际路径/config.ini")` 指定。INI 内部文件路径仍按原 SDK 规则解释，本模块不改变工作目录。

| 方法 | 返回或行为 |
| --- | --- |
| `open()` / `close()` | 初始化/释放；推荐 `with`，重复关闭不重复释放 |
| `query_volume(timeout_ms=200)` | 返回 `Volume`，同时检查等待结果及机器人回复 |
| `get_operating_mode(timeout_ms=3000)` | 返回 `OperatingMode` |
| `set_operating_mode("dev"或"interactive", timeout_ms=3000)` | 主动发送模式设置并检查回复 |
| `read_latest_neck_state()` | 返回 `NeckState`，暂无缓存返回 `None`，其他错误抛 `SDKError` |
| `send_neck_pose(pitch_rad, yaw_rad, roll_rad)` | 主动发送一帧颈部姿态，单位 rad；拒绝非有限 float |

颈部便捷结果是常用字段摘要，不含电机明细。`is_pose_valid` 为假时不能把姿态当作有效反馈；缓存非空也不等于机器人此刻在线。接收时间沿用 SDK 本地单调时钟。

## 3. 新增 C API 的维护方式

`bindings.py` 集中声明需要暴露的类型与函数；CFFI 根据原 SDK 头文件生成绑定，结构体的 `...;` 让 C 编译器计算完整大小、字段偏移，不在 Python 中手写完整布局。

新增公开 C API 后，确认其原始头文件已被 `api_builder()` 的 `#include` 包含，再补一条声明及所需类型。例如本文件已经声明：

```c
unsigned long long GetTimeUsCnt_UpToNow(void);
```

重新 `make api` 后即可通用调用，不需要增加 C 转发函数或 Python 便捷方法：

```python
from aheadform_sdk import AheadFormSDK

with AheadFormSDK() as sdk:
    now_us = sdk.bind("GetTimeUsCnt_UpToNow")()
    print(now_us)
    status = sdk.ffi.new("AHEADFORM_NECK_STATE *")
    result = sdk.bind("Status_ReadLatestNeckState")(sdk.get_handle("status"), status)
    if result == 1:
        print(status.ReceiveTime_us, status.stPose.pitch_rad)
```

`bind("函数名")` 的类型来自集中声明，不再接收 `restype`、`argtypes`。它保留原返回值含义，例如状态读取的 `1`/`0`；未知或未声明函数报错。新增类型可采用 `typedef struct { 需要访问的字段; ...; } TYPE;`，但字段和函数签名必须符合真实头文件。不是自动导出所有头文件或仅凭名字推导类型。声明改变后重新编译扩展。

`get_handle()` 返回 CFFI 指针，支持 `all`、`config`、`topics`、`head`、`media`、`audio`、`video`、`status`、`operating_mode`、`emote_play`。它是借用指针，只能在对应实例成功打开期间使用，不可自行释放；用 `sdk.ffi` 分配结果和参数。通用绑定也串行调用并检查实例就绪，不允许直接绑定总初始化/释放函数。

客户仍需遵循 C API 的线程、所有权、缓冲区和回调约定。不要用低层接口破坏内部 Reader 或生命周期；回调和异步使用的缓冲区必须保持存活，并在注销、确认停止后释放。不要在 SDK 回调里反向调用本类阻塞方法或关闭 SDK。任意 C++ 类不自动绑定；已注册 User 业务可经命令表复用，其他 C++ 接口须先设计 C 兼容入口。

## 4. 复用 User

```sh
make -j4 PYTHON="$(command -v python)"
python demo_user.py --list              # 只列出指令，不初始化机器人
python demo_user.py                     # 不带指令，进入交互菜单
```

启动后可在同一进程连续输入，指令名和参数沿用原 C 菜单：

```text
>> help
>> QueryVolume
>> exit
```

交互模式先初始化 SDK，成功后才显示输入说明和 `>>`，顺序与 C 入口一致；输入 `help` 显示原菜单。初始化失败直接报错退出，不显示菜单或接受指令。后续指令复用同一实例，不逐条初始化和释放。输入 `exit`、收到输入结束（终端通常为 Ctrl+D）或 Ctrl+C 时，输入循环结束并释放已成功初始化的 SDK；Ctrl+C 返回退出码 130。初始化或释放失败会报错并终止，不在当前进程重试。原 User 函数执行期间会等待其返回，长时间阻塞时可能不能立即响应 Ctrl+C。

空行忽略，未知指令、引号不匹配或普通指令错误会提示并继续输入；进入交互输入前已完成初始化，这些输入不会重复初始化。交互输入使用 `shlex` 拆分，含空格路径请加单引号或双引号，例如 `PlayScript "../scripts/my demo.csv"`。输入解析本身不执行 shell 展开；原有 `shell` 指令仍保留自身行为。

交互及单次指令执行后，示例调用原 C 菜单的 `PrintCmdResult()`，显示相同的 `>>cmd[指令] succeed.` 或错误与错误栈；`RET_SKIPPED` 沿用 C 菜单不打印结果的行为。这里的成功表示指令处理函数返回成功，不能单独证明机器人已经执行动作。`PlayScript` 的发送帧数和 PCM 字节数是客户端进度，机器人若被其他动作来源占用、仲裁拒绝或媒体接收未就绪，仍可能没有动作或声音。

测试剧本前先用 `OperatingMode` 查询模式；处于交互模式时，用 `OperatingMode dev` 请求退出，收到成功回复后再播放。若仍无动作，检查机器人仲裁日志中的 `reject motion from inactive source` 和 `active_source_name`，确认其他动作程序已释放控制权；不要通过改成其他程序的 `sourceName` 绕过仲裁。音频还需核对配置中的实际机器人 IP 和机器人端 RTP 接收服务。

仍支持单次指令，便于其他脚本调用；业务指令执行完成后释放 SDK 并退出。独立使用 `--help`、`--list` 或单次 `help`/`exit` 不初始化机器人：

```sh
python demo_user.py help
python demo_user.py QueryVolume
# 指定其他配置同样适用于交互或单次执行：
python demo_user.py --config /实际路径/config.ini
```

上述 `QueryVolume` 会初始化 SDK 并查询机器人，运行前核对配置和网络。调用业务示例（会播放动作和音频，先确认现场条件）：

```python
from aheadform_sdk import AheadFormSDK, SDKError, RET_SUCCESS, RET_SKIPPED, RET_EXIT

with AheadFormSDK(use_user=True) as sdk:
    code = sdk.execute("PlayScript", "../scripts/AheadForm_standard_demo_52.csv",
                       "../scripts/AheadForm_standard_demo.mp3")
    if code not in (RET_SUCCESS, RET_SKIPPED, RET_EXIT):
        raise SDKError("PlayScript", code)
```

路径按此示例从 `python_demo` 启动解释，也可传绝对路径。`execute()` 从原 `gUserInTaskAry` 查找命令并调用函数指针，大小写不敏感；参数独立传递，不经过 shell 拆分，含空格路径仍是一个参数。每个参数复制到独立可写缓冲区，并保持至处理函数返回；最大参数数从原 `MAX_ARG_CNT` 获取，包括命令名本身。返回原有符号 C 状态码；未知命令和参数过多分别返回原 SDK 错误码。

`commands()` 返回命令与说明。`help`、`exit` 允许未初始化时使用；`exit` 返回 `RET_EXIT`，不自动释放控制对象。其他业务须成功 `open()`。`execute()` 默认只返回原 C 状态码；需要 C 菜单结果打印时，传入 `report_result=True`，例如 `sdk.execute("QueryVolume", report_result=True)`。新增已注册指令后重新 `make`，无需添加 Python 分支；User 多级目录的 `.c`/`.cpp` 自动收集。排除原 `main.c`、`HandleUserInCmd.c`，不运行原 C 终端输入循环；`demo_user.py` 用 Python 输入循环调用同一个 `execute()`，并在离开循环时负责释放 SDK。未注册的任意函数不会自动成为 `execute()` 指令。

User 保留终端打印、阻塞、`shell` 和错误处理行为，**部分原有错误路径会直接 `exit()`，结束整个 Python 进程**。需要结构化结果优先使用 API；不适合放在服务主进程的 User 指令可放在独立工作进程。

## 5. 生命周期与错误

同一进程只允许一个活动控制对象，便捷方法、通用绑定、User 调用及开关共用一把锁。创建 Python 包装对象时不分配完整 SDK 控制对象；成功打开后由模块保持强引用，避免客户删除 Python 变量时 SDK 线程访问已释放内存。

正常退出显式 `close()` 或使用 `with`，不依赖 GC、析构函数或解释器退出时自动清理。只有成功释放后才移除活动引用，允许再次打开。初始化失败时原 SDK 无完整回滚约定，本模块不擅自释放；释放失败也不重试。这两种情况或初始化/释放被中断时，保留控制对象及配置缓冲区，将本进程标记为不可重试，记录错误后结束进程。

便捷方法失败抛 `SDKError`：`code` 保留有符号原值，`unsigned_code` 用于十六进制显示，`fatal` 标识不可重试的初始化/释放错误。`bind()` 和 `execute()` 保留原返回值，由调用方按对应接口判断。不要在 SDK 初始化后 `fork()` 并继续使用继承对象，应在各自独立进程中初始化。

## 6. 构建原理、产物与迁移

- API 子 Makefile 生成标准 `CodeProject/API/libAheadFormSDKApi.so`，内部静态链接私有 DDS；不在扩展里再加入一份 DDS。
- `build_bindings.py` 只读取 Python 配置、生成 CFFI C 源码和构建元数据，不通过 setuptools 选择编译器或编译 SDK。
- Python Makefile 使用部署保存的 `CC`/`CXX`、sysroot、编译链接设置编译生成代码和 User；`.c` 与 `.cpp` 分开编译，有 C++ 时使用 C++ 链接器。
- 本机从 `PYTHON` 读取头文件目录、链接库参数和扩展后缀，不复制解释器自身的优化级别。没有新增 `-O2` 或指定 C++ 标准。
- `.build/<DDS_BUILD_ID>-<Python ABI及配置标识>/` 保存 `_aheadform_api.<ABI>.so`、可选 `_aheadform_user.<ABI>.so`、生成 C、对象、依赖文件和构建信息；`.build/active` 指向当前成功构建。
- 声明、头文件、生成配置及构建参数变化会触发相应更新；User 源码清单变化会重新链接。重复无变化构建不编译、不链接。

API/User 扩展通过 CFFI `include()` 共享类型，都使用同一份标准 API 动态库。相对运行搜索路径 `$ORIGIN` 支持将两份扩展和 `libAheadFormSDKApi.so` 放在一起；`$ORIGIN/../../../CodeProject/API` 支持源码包现有目录关系，不修改系统动态库配置。

迁移时优先在匹配目标环境重建。直接交付产物时携带 `aheadform_sdk.py`、`build-info.json`、API 扩展和标准 API 动态库；使用 User 时再带 User 扩展。用 `AheadFormSDK(library_dir="/交付目录")` 指定同目录布局，或保留源码目录关系。目标须有兼容 Python ABI、同版 CFFI（含目标 `_cffi_backend`）、GStreamer/插件、系统 ABI/运行库；有 C++ 业务时还需匹配的 C++ 运行库。源码仓库不交付预编译扩展或第三方 Python 包。同一进程不能切换另一套已加载的构建，换版本需重启。

删除、重命名 API 源文件后，先 `make -C python_demo clean`，再重建；此清理入口同时调用 API 子 Makefile，删除标准 API 静态库、动态库和对象文件。本目录删除 User 源码自动重新链接，不残留旧对象。User 同目录不能同时存在同名 `foo.c`/`foo.cpp`。更换工具链或编译参数后先重新部署，再在 `python_demo` 执行 `make clean && make`，无须进入其他目录清理；API Makefile 不自动检测编译参数变化，不能仅改本目录 CC 绕过 DDS 配置检查。

## 7. 交叉编译

先按部署说明准备目标工具链、sysroot、私有 DDS 和目标 GStreamer。部署只准备宿主 Python/CFFI，不要求目标 Python 配置；手动构建扩展时再提供下面的 `PYTHON_CONFIG`。生成器使用宿主 Python/CFFI，生成的 C 由目标编译器编译。**必须显式提供可在宿主机执行的目标 `PYTHON_CONFIG` 包装脚本**，响应：

```text
--includes           目标 Python.h、pyconfig.h 等头文件的 -I 参数
--ldflags            目标 Python 扩展需要的链接参数
--extension-suffix   目标解释器的真实扩展后缀
```

不要直接执行依赖目标 CPU 的 Python 程序，也不要提供宿主机 `python3-config` 冒充目标配置。例如 ARM64 CPython 3.10 的包装脚本可以采用下面形式，所有路径、版本和后缀必须按真实目标更换：

```sh
#!/bin/sh
case "$1" in
  --includes) printf '%s\n' '-I/opt/target-sysroot/usr/include/python3.10 -I/opt/target-sysroot/usr/include/aarch64-linux-gnu/python3.10' ;;
  --ldflags) printf '%s\n' '-L/opt/target-sysroot/usr/lib/aarch64-linux-gnu -ldl -lm' ;;
  --extension-suffix) printf '%s\n' '.cpython-310-aarch64-linux-gnu.so' ;;
  *) exit 2 ;;
esac
```

```sh
make -C python_demo PYTHON=/宿主私有环境/bin/python PYTHON_CONFIG=/opt/tools/target-python-config api
```

依赖部署只准备宿主 CFFI，不要求目标 Python 配置；手动 Python make 缺少目标配置时报错停止，不用宿主机头文件替代。部署的 `DDS_CROSS` 标记用于识别交叉构建；不要手动伪造它以跳过检查。宿主生成端与目标运行端的 CFFI 版本必须一致：例如目标 Python 3.8/3.9 使用 1.17.1 时，准备对应受支持的宿主 Python/CFFI 环境生成，而不是拿默认 2.1.1 产物混用。目标扩展不能在宿主 Python 中加载测试，须到匹配目标验证。

## 8. Python 依赖的离线准备与排错

仅 CycloneDDS 源码随 SDK 离线提供；Python、CFFI、GStreamer 和工具链不随包提供。在能联网且与目标 Python/平台匹配的环境，提前准备 wheel：

```sh
python -m pip download --only-binary=:all: -r requirements.txt -d /介质/wheelhouse
# 离线机器，在私有 venv 内安装：
python -m pip install --no-index --find-links=/介质/wheelhouse -r requirements.txt
```

也可在 SDK 根目录让部署脚本自动创建环境并离线安装依赖，之后手动构建：

```sh
bash deploy/deploy.sh --build-only --skip-packages --python-wheelhouse /介质/wheelhouse
make -C python_demo
```

此时系统构建工具、venv/pip、匹配的 Python 开发头文件和 GStreamer 须已准备；本地包目录不访问包索引。CFFI 无兼容 wheel 时可能需要主机 libffi 开发文件及 Python 包构建依赖，断网前应一并准备。

wheelhouse 必须包含 CFFI 的传递依赖；目标平台没有兼容 wheel 时，先在匹配工具链环境构建 wheel，或准备完整源码及其构建依赖。交叉场景分别准备宿主生成环境和目标运行环境；可用 pip 的 `--platform`、`--python-version`、`--implementation`、`--abi` 下载目标 wheel，但要明确选择目标对应的 CFFI 固定版本、核对标签与所有依赖，不能复制宿主 `_cffi_backend` 或虚拟环境。Python 开发头文件、venv/pip 本身也须提前准备。

找不到扩展先执行对应 `make api`/`make`；缺少 CFFI 用当前 `PYTHON` 安装 requirements。`wrong ELF class`、导入错误或 ABI 不匹配时核对目标架构、Python 版本和扩展后缀，不能只重命名 `.so`。`undefined symbol` 时检查标准 API 库与声明/源码是否同版；头文件和链接参数来自匹配目标。CFFI 版本不一致时安装构建时版本再试。依赖检查失败按部署提示重新准备，不回退到系统 DDS。

实际测试与未验证边界见[验证记录](验证记录.md)。
