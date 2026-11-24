# AheadFormSDK 快速上手

下文以机器人 IP `192.168.110.51` 和外部 DDS 域 ID `50` 为例，部署时请将 `--robot` 和 `--domain` 换成现场实际值。

## 1. SDK 快速上手操作步骤

将 `AheadFormSDK` 复制到 Linux 客户端的运行目录，给机器人上电并确认客户端可通过有线网络访问机器人。进入 `deploy` 执行部署。脚本默认按通往机器人 IP 的路由选择网卡；如需指定，在部署命令末尾加上 `--interface 网卡名`。

```sh
cd "/你的运行目录/AheadFormSDK"
cd deploy
bash ./deploy_ubuntu.sh --robot 192.168.110.51 --domain 50
```

部署会覆盖原配置。部署成功后，打开 `../CodeProject/AheadFormSDK_Config.ini`，核对网卡、外部 DDS 域和机器人 IP，按需调整媒体参数（见[配置字段说明](#配置字段说明)）。然后进入 `CodeProject` 编译：

```sh
cd ../CodeProject
make
```

确认机器人已上电至少 10 秒，在当前目录启动 SDK：

```sh
./AheadFormSDK
```

看到 `>>` 后，输入 `help` 并按回车。菜单显示完并再次出现 `>>` 后，输入要运行的指令。输入 `exit` 退出。

## 2. 环境要求和配置

推荐已验证的 Ubuntu 22.04，使用虚拟机时将网卡设为桥接模式。[部署脚本](../deploy/deploy_ubuntu.sh)会安装或检查 GCC、Make、`pkg-config`、CycloneDDS 11.0.1、GStreamer Core 和 App 1.20 或更高版本，以及音视频所需的插件。

其他主流 glibc Linux 发行版可按目标架构手动安装依赖，使用同一份源码构建 32 位或 64 位程序。部署方式见 [README](../README.md)。

### 配置字段说明

SDK 启动时从当前工作目录读取 [`AheadFormSDK_Config.ini`](../CodeProject/AheadFormSDK_Config.ini)。部署脚本会把旧配置备份为同目录的 `AheadFormSDK_Config.ini.bak.*`，再写入新配置。手动修改请在部署完成后进行，保存后重启 SDK 即可生效，无需重新编译。请保留以下三个节及全部字段，节名和字段名区分大小写，每个字段只填写一次。

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

下表与 `help` 菜单顺序一致。菜单里的 `<...>` 括住的是输入示例，例如看到 `<AudioDevice mic>` 时，只输入 `AudioDevice mic`，不用键入 `>>` 或尖括号。`[]` 表示可选参数，输入时不带方括号；`mic/spk` 这类参数只填其中一项。文件路径和资源名请使用不含空格的名称。

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

**方法一：在 `User` 中开发。** 将自己的 `.c` 和 `.h` 放入 `CodeProject/User`，可使用多级子目录。主 Makefile 会递归收集 `.c` 和头文件搜索目录，并跟踪头文件依赖，增删文件无须编辑 Makefile。删除或重命名 `.c` 后，从 SDK 根目录先执行 `make -C CodeProject clean`，再构建，避免旧代码留在可执行文件中。整个工程只保留一个 `main()`。

**方法二：在自有工程中链接静态库。** 在 SDK 根目录执行：

```sh
cd CodeProject/API
make -f subMakefile_buildApiLib
```

当前目录生成 `libAheadFormSDKApi.a`。在自有工程中，将 API 下存放头文件的目录加入头文件搜索路径，由应用调用 [AheadForm_Init() 和 AheadForm_Deinit()](../CodeProject/API/AheadForm_Task/Task/AheadForm_Task.h) 完成初始化和释放。第三方头文件参数可用 `pkg-config --cflags CycloneDDS gstreamer-app-1.0` 获取。

链接时将静态库放在调用方对象文件之后，再加入 `pkg-config --libs CycloneDDS gstreamer-app-1.0` 给出的链接参数及 `-pthread -lm -lrt`。

同一源码树切换构建入口时，先清理共用的中间文件。切换到静态库，在 `CodeProject/API` 执行 `make -f subMakefile_buildApiLib clean`；切回主程序，从 SDK 根目录执行 `make -C CodeProject clean`。
