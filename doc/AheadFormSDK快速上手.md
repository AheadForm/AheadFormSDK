# AheadFormSDK 快速上手

下文以机器人 IP `192.168.110.51` 和外部 DDS 域 ID `50` 为例，部署时请将 `--robot` 和 `--domain` 换成现场实际值。

## 1. SDK 快速上手操作步骤

将 `AheadFormSDK` 复制到 Linux 客户端的运行目录，给机器人上电并确认客户端可通过有线网络访问机器人。进入 `deploy` 执行部署；如需指定网卡，在部署命令末尾加上 `--interface 网卡名`。

```sh
cd "/你的运行目录/AheadFormSDK"
cd deploy
bash ./deploy_ubuntu.sh --robot 192.168.110.51 --domain 50
```

部署会覆盖原配置。部署成功后，打开 `../CodeProject/AheadFormSDK_Config.ini`，核对网卡、外部 DDS 域和机器人 IP，按需调整媒体参数。然后进入 `CodeProject` 编译：

```sh
cd ../CodeProject
make
```

确认机器人已上电至少 10 秒，在当前目录启动 SDK：

```sh
./AheadFormSDK
```

启动后输入 `help` 查看菜单，再输入要运行的指令；想要退出SDK进程时输入 `exit` 退出。

## 2. 环境要求和配置

推荐已验证的 Ubuntu 22.04，使用虚拟机时将网卡设为桥接模式。[部署脚本](../deploy/deploy_ubuntu.sh)会安装或检查 GCC、Make、`pkg-config`、CycloneDDS 11.0.1、GStreamer Core 和 App 1.20 或更高版本，以及音视频所需的插件。

如需手动调整配置，`strNetworkInterface` 填客户端连接机器人的网卡，`domain_id` 填机器人的外部 DDS 域，`strRobotIPv4` 填机器人 IP。脚本将原配置备份为同目录的 `AheadFormSDK_Config.ini.bak.*`。启动时，程序从当前工作目录读取 `AheadFormSDK_Config.ini`。

## 3. 指令菜单说明

启动 SDK 后，在 `>>` 提示处每次输入一条指令并按回车。下表与 `help` 菜单顺序一致。`[]` 表示可选参数，输入时不带方括号；`mic/spk` 这类参数只填其中一项。文件路径和资源名请使用不含空格的名称。

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
| `PlayScript csvFilePath [audioFilePath]` | 播放 Linux 客户端上的文件。动作 CSV 必填，音频可选 MP3 或 PCM WAV；播放期间菜单等待该指令返回。 | 自行准备符合格式要求的文件。在菜单中输入 `PlayScript /home/customer/AheadFormScripts/show.csv`，或输入 `PlayScript /home/customer/AheadFormScripts/show.csv /home/customer/AheadFormScripts/voice.mp3` 加入音频；请将路径换成实际文件的路径。 |
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
