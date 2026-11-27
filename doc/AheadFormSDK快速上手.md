# AheadFormSDK 快速上手

部署用于准备 SDK 构建与运行环境，无需指定或连接机器人。下文配置中的机器人 IP `192.168.110.51` 和外部 DDS 域 ID `50` 只是示例，运行前请按目标机器人修改。

各目录用途见[文档与代码清单](文档与代码清单.md)；Python 客户见[Python 接入说明](../python_demo/doc/Python接入说明.md)，可只使用 API，也可复用 User 业务。

## 1. SDK 快速上手操作步骤

将 SDK 目录复制到 Linux 客户端的运行目录，在 SDK 根目录执行部署。此时机器人可以不在线，也不需要填写机器人 IP、DDS 域或网卡。

```sh
cd "/你的运行目录/AheadFormSDK"
bash deploy/deploy.sh
```

脚本先显示当前 C 编译器。本机编译且无特殊要求时，直接回车或等待 2 分钟采用当前值；需要指定时填写 `gcc`、`clang` 或编译器可执行文件的完整路径。配套 C++ 编译器及静态库工具自动确定，通常无需填写；交叉编译所需参数见[指定编译器与交叉编译](依赖部署与交叉编译.md#4-指定编译器)。脚本仅准备依赖：构建私有 DDS，在 `python_demo/.venv` 安装 CFFI，并把确认后的编译器及参数写入 `.deps/<工具链标识>/config.mk`，由 `deploy/deps.mk` 自动读取；不构建 SDK 主程序、API 库或 Python 扩展，不修改系统 Python 包；只需 C/C++ 时加 `--skip-python`。中断后可重新执行同一部署命令。

部署结尾显示当前 DDS/GStreamer 版本、编译与链接参数，并分别给出 C/C++ 客户、Python 客户的编译和运行命令；按使用语言选择一项即可，无须两套都编译。运行前的配置提醒只显示 `CodeProject/AheadFormSDK_Config.ini` 的完整路径。打开该文件，按目标机器人填写网卡、外部 DDS 域和机器人 IP，并按需调整媒体参数（见[配置字段说明](#配置字段说明)）。默认部署保留原配置；也可选填 `--robot`、`--domain`、`--interface` 自动更新对应字段，详见[可选机器人参数](依赖部署与交叉编译.md#1-默认部署)。切换机器人时修改配置并重启 SDK 即可，无需重新部署或编译。

进入程序目录并手动构建：

```sh
cd CodeProject
make
```

确认机器人已上电至少 10 秒，在当前目录启动 SDK：

```sh
./AheadFormSDK
```

看到 `>>` 后，输入 `help` 并按回车。菜单显示完并再次出现 `>>` 后，输入要运行的指令。输入 `exit` 退出。

## 2. 环境要求和配置

SDK 面向 GNU/Linux，推荐已验证的 Ubuntu 22.04；使用虚拟机时将网卡设为桥接模式。[部署脚本](../deploy/deploy.sh)检查 C/C++ 编译器、Make、CMake ≥ 3.16、Python ≥ 3.8、`pkg-config`、GStreamer Core/App ≥ 1.20 及所需插件。已有依赖满足要求时不安装；缺少时，APT、DNF、Zypper 系统会安装或升级必要包，包括可能更新已有的 GStreamer 1.x。其他发行版请先准备缺项，再执行脚本。

CycloneDDS 11.0.1 源码已放在 SDK 根目录的 `ThirdParty`，部署时使用所选编译器构建，产物保存在项目私有的 `.deps/<工具链标识>/`；SDK 静态链接这份 DDS，不安装系统 DDS，不修改全局环境及动态库搜索配置。GStreamer、工具链、构建工具和 Python 依赖包不随 SDK 提供；全部准备好时，可加 `--skip-packages` 断网构建。首次离线安装 Python 包时，另加 `--python-wheelhouse /本地包目录`；`--skip-packages` 本身不禁止 pip 联网。原 `deploy_ubuntu.sh` 仍可用，会转发到统一入口。

默认部署不检查机器人网络、不修改配置。依赖已提前准备完整时，可禁止系统包安装：

```sh
bash deploy/deploy.sh --skip-packages
```

**同一套工具链和依赖环境只需成功部署一次，日常修改源码不必重新部署。** 编译器设置会保存在项目本地 `.mk` 配置中，后续 `make` 自动读取，无需再次输入。需要固定工具链或参数时，可修改 [`deploy/deps.mk`](../deploy/deps.mk) 顶部的覆盖项，再重新部署以准备匹配的 DDS；已有构建产物时，先 `make clean` 再 `make`；Python 接入也应先清理再构建。完整部署选项见[依赖部署与交叉编译](依赖部署与交叉编译.md)。

### 修改 SDK 后的编译

修改或新增源码、头文件后，先退出正在运行的 SDK，再从 SDK 根目录执行：

```sh
make -C CodeProject
```

也可进入 `CodeProject` 后直接执行 `make`。构建成功后，在 `CodeProject` 中运行 `./AheadFormSDK` 启动新版程序。普通 `make` 只增量构建 SDK，不重新构建 DDS。

主 Makefile 自动收集 **`CodeProject` 下除 `API` 以外所有目录及子目录中的 `.c`、`.cpp`**，例如 `User/feature.c`、`User/sub/extension.cpp` 或自行创建的 `Customer/extension.cpp`。新增这些目录、源码和头文件不需要修改 Makefile；修改已有 API 的 `.c` 或头文件，也会通过 API 子 Makefile 自动增量构建。`API` 保持纯 C，`.cpp` 请放在 API 以外的目录。整个程序只保留一个 `main()`，同一目录不得同时存在 `foo.c` 和 `foo.cpp`；C++ 调用 SDK 的声明方式见[移植方法](#5-移植方法)。

删除、重命名源码或将源码在 `.c`、`.cpp` 之间转换后，先清理再构建：

```sh
make -C CodeProject clean
make -C CodeProject
```

`clean` 会清理主程序和 API 的编译产物，保留 DDS 缓存，不需要因此重新部署。只修改 `AheadFormSDK_Config.ini` 时无需编译，重启 SDK 即可。只有更换编译器、目标架构、sysroot、编译/链接参数或 GStreamer 开发环境，或依赖配置、DDS 缓存丢失/失效时，才需重新运行部署脚本；缓存损坏时按报错加上 `--rebuild-dds`。

### 配置字段说明

SDK 启动时从当前工作目录读取 [`AheadFormSDK_Config.ini`](../CodeProject/AheadFormSDK_Config.ini)。默认部署不修改该文件；显式传入机器人参数时，仅在相应字段变化后把旧配置备份为同目录的 `AheadFormSDK_Config.ini.bak.*`，再更新。手动修改后重启 SDK 即可生效，无需重新编译或部署。请保留以下三个节及全部字段，节名和字段名区分大小写，每个字段只填写一次。

**`[dds]`：连接机器人**

| 字段 | 示例值 | 含义与填写方法 |
| --- | --- | --- |
| `strNetworkInterface` | `ens33` | 填写 Linux 客户端连接机器人的实际网卡名，DDS 和媒体通信都使用该网卡。运行 `ip route get 192.168.110.51`（把 IP 换成实际机器人 IP），填写输出中 `dev` 后的网卡名。若启动报网卡不存在，先检查此项。 |
| `domain_id` | `50` | 机器人 DDS Bridge 对客户端开放的 DDS 域 ID，向现场部署人员确认。应填外部域，不能填板端内部域；域不一致时 SDK 无法发现机器人。部署命令的 `--domain` 接受 `0～232`。 |

**`[head]`：动作来源**

| 字段 | 示例值 | 含义与填写方法 |
| --- | --- | --- |
| `sourceName` | `aheadform_sdk` | SDK 发送动作指令时使用的来源标识。没有现场命名要求时可沿用示例；长度 1～32 个字符，以小写字母开头，其余只允许小写字母、数字和下划线。 |

**`[media]`：音视频**

先按现场填写机器人 IP；组播地址、端口和 RTP 负载类型应与机器人媒体网关一致。三个开关用 `0` 或 `1`，控制 SDK 启动时是否开启对应通道。只使用菜单查询或 DDS 控制时，可以保持为 `0`。

| 字段 | 示例值 | 含义与填写方法 |
| --- | --- | --- |
| `isVideoCaptureOn` | `0` | 是否在启动时接收机器人视频；填 `1` 时，`videoFrameSizeMax` 必须大于 0。 |
| `isMicCaptureOn` | `0` | 是否在启动时接收机器人麦克风音频。 |
| `isSdkPlayAudioOn` | `0` | 是否在启动时开启 SDK 向机器人扬声器发送音频的通道。填 `0` 时，有音频的 `PlayScript` 仍会按需开启通道，前提是已填写实际机器人 IP。 |
| `multicastAddress` | `239.255.42.1` | 机器人视频和麦克风共用的 IPv4 组播地址。 |
| `strRobotIPv4` | `192.168.110.51` | SDK 向机器人扬声器发送音频时的目标 IPv4。使用 SDK 音频播放时必须填写可达的机器人单播地址；`0.0.0.0` 表示未设定。 |
| `videoPort` | `5600` | 客户端接收机器人视频的 UDP 端口，须与 `micPort` 不同。 |
| `micPort` | `5602` | 客户端接收机器人麦克风音频的 UDP 端口。 |
| `spkPort` | `5604` | 机器人接收扬声器音频的 UDP 端口。 |
| `videoPayloadType` | `96` | 机器人发送的 H.264 视频 RTP 负载类型编号。 |
| `micPayloadType` | `96` | 机器人发送的麦克风音频 RTP 负载类型编号。 |
| `spkPayloadType` | `96` | 机器人扬声器接收的 RTP 负载类型编号。 |
| `jitterBufferLatencyMax_ms` | `60` | 视频和麦克风接收端的 RTP 抖动缓冲时限，单位毫秒。网络抖动大时可适当调高，但接收延迟也会增加。 |
| `rtpMtu` | `1400` | RTP 单包大小设置，单位字节，可填 `652～65507`。用于视频和麦克风接收的缓冲分配，以及扬声器音频的发包上限。通常沿用示例，需调整时按现场网络的 MTU 预留协议头空间。 |
| `videoFrameSizeMax` | `0` | SDK 能缓存的单个 H.264 访问单元（AU，即一帧编码数据）的最大字节数。开启视频前按实测最大 AU 大小留出余量；超出会丢帧，填得过大则会增加内存占用。不接收视频可以填 `0`。 |

UDP 端口填 `1～65535`，RTP 负载类型编号填 `0～127`；同为 `96` 是示例中的三条独立媒体流各自使用的设置。

## 3. 指令菜单说明

下表与 `help` 菜单顺序一致。菜单里的 `<...>` 括住的是输入示例，例如看到 `<AudioDevice mic>` 时，只输入 `AudioDevice mic`，不用键入 `>>` 或尖括号。`[]` 表示可选参数，输入时不带方括号；`mic/spk` 这类参数只填其中一项。本节以 C/C++ 主程序的终端菜单为准：其输入按空格分隔，不支持用引号包住含空格参数，因此文件路径和资源名请使用不含空格的名称。Python 的 `demo_user.py` 交互菜单复用相同命令表，但支持带引号的含空格参数；具体输入方式见 [Python 接入说明](../python_demo/doc/Python接入说明.md#4-复用-user)。

| 指令 | 功能 | 输入示例 |
| --- | --- | --- |
| `help` | 显示完整指令菜单。 | `help` |
| `shell cmd` | 在运行 SDK 的 Linux 客户端执行 Shell 命令。 | `shell pwd`：查看当前工作目录。 |
| `errlog [storeFilePath]` | 打印 SDK 错误栈，或将其导出到指定文件。空栈时不生成文件。 | `errlog`：在终端查看；`errlog ./sdk-errors.log`：导出到当前目录。 |
| `WatchVolume seconds` | 在指定时长内立即查询一次机器人音量，随后每秒查询一次。时长填正整数。 | `WatchVolume 5`：观察音量 5 秒。 |
| `QueryVolume` | 查询一次机器人当前音量。 | `QueryVolume` |
| `AudioDevice mic/spk [devId]` | 列出机器人麦克风（`mic`）或扬声器（`spk`），`*` 标记当前设备；指定设备 ID 时切换。 | `AudioDevice mic`、`AudioDevice spk`：列出设备；`AudioDevice mic mic0`：选择列表中的 `mic0`，ID 以实机列表为准。 |
| `OperatingMode [dev/interactive]` | 查询或请求切换机器人工作模式。`dev` 为开发模式，`interactive` 为交互模式。 | `OperatingMode`：查询；`OperatingMode dev`、`OperatingMode interactive`：请求切换。 |
| `EmotePlayback [csvFileName] [audioFileName] [loop]` | 播放机器人上已有的动作（CSV）或音频（MP3、WAV），至少指定一个资源。可同时指定两种资源，顺序不限；最后加 `loop` 循环播放，循环须包含动作。资源名须带后缀，以机器人上的文件名为准。 | `EmotePlayback intro.csv`：播放动作；`EmotePlayback voice.mp3`：播放音频；`EmotePlayback intro.csv voice.mp3 loop`：循环播放两者。 |
| `EmotePlayCtrl pause/resume/stop` | 暂停、继续或停止由 `EmotePlayback` 启动的机器人端播放。 | `EmotePlayCtrl pause`、`EmotePlayCtrl resume`、`EmotePlayCtrl stop` |
| `PlayScript csvFilePath [audioFilePath]` | 播放 Linux 客户端上的文件。动作 CSV 必填，音频可选 MP3 或 PCM WAV；播放期间菜单等待该指令返回。 | 从 `CodeProject` 启动后，在菜单中输入 `PlayScript ../scripts/AheadForm_standard_demo_52.csv`，或输入 `PlayScript ../scripts/AheadForm_standard_demo_52.csv ../scripts/AheadForm_standard_demo.mp3` 加入配套音频。 |
| `Tx1MotionPose t_us pitch_rad yaw_rad roll_rad headX headY headZ leftPitch_rad leftYaw_rad rightPitch_rad rightYaw_rad bs0 … bs51` | 发送一帧完整动作，依次填写微秒时间戳、3 个颈部角度、3 个头部位置分量、4 个眼球角度和 52 个表情权重。 | 见下方完整输入示例。 |
| `Tx1Face leftPitch_rad leftYaw_rad rightPitch_rad rightYaw_rad bs0 … bs51` | 发送一帧面部动作，依次填写 4 个眼球角度和 52 个表情权重。 | 见下方完整输入示例。 |
| `Tx1NeckPose pitch_rad yaw_rad roll_rad` | 发送一帧颈部姿态，依次填写俯仰、偏航、侧倾角，单位为弧度。 | `Tx1NeckPose 0.1 0 0`：俯仰目标为 0.1 弧度，其余两轴目标为 0。 |
| `TxServosTargetLogicDeg deg0 … deg19` | 按舵机编号 0～19 发送一组脸部舵机目标逻辑角，单位为度。 | 见下方完整输入示例。 |
| `ReadRecentState sensor/neck/faults cnt` | 读取 SDK 最近缓存的传感器、颈部或故障池快照，`cnt` 为 1～499。 | `ReadRecentState neck 1`：查看最近一条颈部状态；`ReadRecentState faults 1`：查看最近一份故障池。 |
| `WatchState sensor/neck/faults seconds` | 在指定时长内显示新收到的传感器、颈部或故障池快照。时长填正整数。 | `WatchState neck 5`：观察颈部状态 5 秒。 |
| `exit` | 退出 SDK。 | `exit` |

测试剧本前，先输入 `OperatingMode` 查询模式；处于交互模式时，用 `OperatingMode dev` 请求切换开发模式，收到成功回复后再播放。若仍无动作，检查机器人仲裁日志是否出现 `reject motion from inactive source`，确认其他动作来源已释放控制权。`PlayScript` 的发送帧数、音频字节数及菜单的 `cmd[...] succeed.` 表示客户端处理完成，不单独证明机器人已经接收并执行；还需结合机器人状态、仲裁和媒体接收日志判断。Python 菜单同样复用 C 指令与结果打印，详见 [Python 接入说明](../python_demo/doc/Python接入说明.md)。

下方三条长指令分别给出了全部参数，每次复制一整行到菜单。颈部和眼球角度用弧度，表情权重填 0～1，舵机逻辑角用度。示例中的 `0` 是参数值，不表示保持当前姿态；动作目标请按机器人允许的范围调整。

```text
Tx1MotionPose 0 0.1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
Tx1Face 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
TxServosTargetLogicDeg 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
```

动作 CSV 的字段和单位见 [CSV 剧本格式要求](CSV剧本格式要求.md)。机器人未接入 IMU 时，`sensor` 没有快照或新数据是正常的。

## 4. 架构简单说明

`CodeProject/API` 提供 SDK 功能，包括连接机器人、控制动作和媒体、读取状态。客户代码通过这里的接口使用机器人功能。

`CodeProject/User` 负责把 API 组合成业务流程，指令菜单就是可运行的示例。在 [UserCmdMenu.c](../CodeProject/User/UserCmdMenu.c) 中按指令名查找实现，按需复制对应的业务函数和它依赖的辅助函数。例如，`PlayScript()` 解析菜单参数后调用 `PlaybackScriptOfUser()`，自己的程序可以直接复用播放流程。

## 5. 移植方法

**方法一：在 SDK 工程中开发。** 建议将自己的 `.c`、`.cpp` 和头文件放入 `CodeProject/User`，也可在 `CodeProject` 下新建 API 以外的业务目录。各目录均可使用多级子目录、混合放置 C/C++ 源码。主 Makefile 调用 API 子 Makefile 构建纯 C 静态库，并递归编译 API 目录以外的 `.c` 和 `.cpp`：`.c` 使用 C 编译器，`.cpp` 使用 C++ 编译器；存在 `.cpp` 时用 C++ 编译器共同链接，否则仍用 C 编译器。使用 `make -j` 时 API 和业务代码可并行构建，头文件搜索仍包含 API。新增文件无须修改 Makefile，构建命令见[修改 SDK 后的编译](#修改-sdk-后的编译)。整个程序只保留一个 `main()`；同一目录不得同时存在 `foo.c` 和 `foo.cpp`，不同目录可以使用相同文件名。

在 `.cpp` 中调用 SDK 的 C 接口时，将对应头文件放在 `extern "C"` 中，例如：

```cpp
extern "C"
{
#include "AheadForm_Task.h"
}
```

客户自己的 C/C++ 模块互调时，也需为相关 C 接口提供正确的 C 链接声明。

**方法二：在自有工程中链接静态库。** 在 SDK 根目录执行：

```sh
cd CodeProject/API
make -f subMakefile_buildApiLib
```

当前目录生成 `libAheadFormSDKApi.a`，只包含 API 自身的对象文件。由应用调用 [AheadForm_Init() 和 AheadForm_Deinit()](../CodeProject/API/AheadForm_Task/Task/AheadForm_Task.h) 完成初始化和释放。先运行过部署脚本后，可在当前目录查询客户工程需要的编译和链接参数：

```sh
make -s -f subMakefile_buildApiLib print-cflags
make -s -f subMakefile_buildApiLib print-libs
```

将第一条输出用于编译，将第二条输出放在调用方对象文件之后用于链接；它包含 API 库、私有 DDS 静态库、客户环境的 GStreamer 和系统库。不要另用系统 `pkg-config CycloneDDS` 替换私有 DDS 参数。客户工程必须使用同一工具链、ABI、sysroot 和配套 API 头文件。

相同工具链和编译参数下，主程序与独立 API 构建统一使用 API 子 Makefile 管理 API 对象，切换入口无须清理。删除、重命名源码或将源码在 `.c`、`.cpp` 之间转换后，从 SDK 根目录先执行 `make -C CodeProject clean`，再构建；更换工具链或编译参数时重新运行部署脚本，再手动清理并重建。主入口的 `make clean` 会同时清理主程序和 API 产物，保留私有 DDS 缓存。

**按需生成 API 动态库：** 默认 `make` 仍只构建静态 API 库和主程序。需要动态库时，在 SDK 根目录执行：

```sh
make -C CodeProject/API -f subMakefile_buildApiLib api_so
```

产物为 `CodeProject/API/libAheadFormSDKApi.so`；也可在 `CodeProject/API` 中执行 `make -f subMakefile_buildApiLib api_so`。此入口只构建动态库，不额外构建静态库或主程序。静态库使用普通 `.o`，动态库按需使用独立的 `.pic.o`，同一工具链下可以共存并分别增量更新。动态库静态链接私有 DDS，运行时仍需要匹配的 GStreamer 和系统库；C/C++ 客户也可链接此动态库，并自行安排私有库目录与运行搜索路径。原 `print-libs` 继续输出静态链接参数。

**Python 接入：** 默认部署会尝试在 `python_demo/.venv` 准备私有 Python/CFFI 环境，不构建 Python 扩展、不展示菜单。准备失败时继续完成 C/C++ 依赖部署；部署完成先显示两种环境的“部署结果”及具体失败原因，再显示“后续操作指南”。Python 环境准备成功后，从 SDK 根目录执行 `make -C python_demo` 构建 API/User 接入；只需 API 时执行 `make -C python_demo api`。构建成功后，进入 `python_demo` 并运行 `.venv/bin/python demo_user.py`，即可在 `>>` 下连续输入 User 指令；也支持单次命令。Python 复用标准 API 动态库和现有 User 业务。详细操作见 [Python 接入说明](../python_demo/doc/Python接入说明.md)。
