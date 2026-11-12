# SDK 剧本播放快速上手

本文面向首次使用 AheadFormSDK 的客户。按第 1～7 节完成环境准备、部署、配置、编译和剧本播放；第 8 节列出全部 22 条菜单指令及使用方法，第 9～11 节提供文件格式、API 入口和排障参考。

SDK 在客户电脑上读取动作 CSV 和同名 MP3，通过 DDS 发送脸部、眼睛和脖子动作，通过 RTP/UDP 发送音频。剧本保留在客户电脑，不复制到机器人。仓库不附带客户剧本。

## 1. 支持环境与准备事项

| 项目 | 要求 |
| --- | --- |
| 已验证系统 | Ubuntu 22.04 x86_64 |
| 其它系统 | Ubuntu 24.04 尚未验证；其它 Linux 发行版、架构及 WSL 尚未验证；当前不提供 Windows/macOS 原生运行方式 |
| 编译器 | 客户电脑本机 GCC，无需 ARM64 交叉编译器或 sysroot |
| 网络 | 电脑与已启动的机器人使用可互通的有线局域网；虚拟机桥接主机有线网卡 |
| 安装权限 | 首次依赖安装需要联网及 sudo 权限；日常启动 SDK 使用普通用户 |
| 客户文件 | 符合第 9 节格式的 CSV 和同目录、同名 MP3 |

准备好机器人 IPv4 和机器人 DDS Bridge 的**对外通信域**。本文使用机器人 `192.168.110.51`、对外域 `50`、本地网卡 `ens33` 举例，请替换成实际值。对外通信域不能直接用机器人内部域或 Bridge 进程域代替。

在 Linux 终端检查到机器人的路由：

```bash
ip -4 route get 192.168.110.51
ip -4 addr show
ping -c 3 192.168.110.51
```

路由输出中的 `dev` 是本机网卡，`src` 是本机 IPv4。例如 `dev ens33 src 192.168.110.129`。虚拟机应使用桥接有线网络，避免 NAT、无线网卡或其它接口承载机器人通信。能 ping 通仅证明基本 IP 连通，DDS 还需要对应端点和组播通路。

## 2. 下载工程

以下命令在 **Linux 终端**执行，示例把工程放在当前用户的主目录：

```bash
sudo apt-get update
sudo apt-get install -y git
cd ~
git clone https://github.com/AheadForm/AheadFormSDK.git
cd ~/AheadFormSDK
```

工程和客户文件的位置如下。以下 `customer` 代表客户的真实 Linux 用户名；运行 `echo "$HOME"` 可以查看自己的主目录。

```text
/home/customer/
├── AheadFormSDK/                       # 下载的工程
│   ├── README.md
│   ├── scripts/
│   │   └── deploy_ubuntu.sh
│   ├── doc/
│   │   └── SDK剧本播放快速上手.md
│   └── CodeProject/
│       ├── Makefile
│       ├── AheadFormSDK_Config.ini     # 有效配置
│       ├── AheadFormSDK                # 编译生成，不随仓库分发
│       ├── API/                        # SDK 源码与公开头文件
│       └── User/src/                   # 菜单的 API 调用示例
└── AheadFormScripts/                   # 客户自行选择的剧本目录
    ├── show.csv
    └── show.mp3
```

## 3. 使用部署脚本安装依赖并构建

在 Linux 终端、工程根目录执行：

```bash
cd ~/AheadFormSDK
bash scripts/deploy_ubuntu.sh --robot 192.168.110.51 --domain 50
```

| 参数 | 含义 |
| --- | --- |
| `--robot IPv4` | 机器人实际地址；省略时为 `192.168.110.51` |
| `--domain 数字` | DDS Bridge 对外通信域，范围 0～232；省略时为 `50` |
| `--interface 网卡名` | 可选，本机有线网卡；省略时按机器人路由自动选择，指定时必须与该路由一致 |
| `--skip-packages` | 仅跳过 apt 安装，仍检查网络和插件、必要时安装 CycloneDDS、生成配置并构建 |
| `--help` | 显示参数说明 |

例如显式指定本地网卡：

```bash
cd ~/AheadFormSDK
bash scripts/deploy_ubuntu.sh --robot 192.168.110.51 --domain 50 --interface ens33
```

脚本依次完成：

1. 安装 GCC、make、pkg-config、CMake、网络工具、GStreamer 开发库及 MP3/RTP/H.264 等插件。
2. 检查机器人路由、所选有线网卡及 ping 连通性。
3. 检查 CycloneDDS 11.0.1；缺少指定版本时下载固定源码归档、校验 SHA-256、编译安装。
4. 检查动态库和 GStreamer 插件，备份已有 INI，再生成有效配置。
5. 使用本机 GCC 构建，检查动态库是否缺失，输出启动目录和命令。

成功时应看到 `CREATE AheadFormSDK`、`Deployment complete:` 以及本机网卡、IPv4、机器人地址和对外域。脚本完成后程序位于 `~/AheadFormSDK/CodeProject/AheadFormSDK`，脚本不会自动进入交互菜单。

**再次运行部署脚本会备份并覆盖有效配置**，备份名形如 `AheadFormSDK_Config.ini.bak.时间戳`。已自定义配置且只想重新编译时，使用第 5 节的 make 命令。

依赖已安装时可以运行以下命令，但它仍会重新生成配置：

```bash
cd ~/AheadFormSDK
bash scripts/deploy_ubuntu.sh --robot 192.168.110.51 --domain 50 --skip-packages
```

## 4. 配置文件的位置与修改方式

有效配置必须位于：

```text
~/AheadFormSDK/CodeProject/AheadFormSDK_Config.ini
```

在 Linux 终端打开：

```bash
cd ~/AheadFormSDK
nano CodeProject/AheadFormSDK_Config.ini
```

在 nano 中修改后，按 Ctrl+O、回车保存，Ctrl+X 退出。配置示例：

```ini
[dds]
strNetworkInterface = ens33
domain_id = 50

[head]
sourceName = aheadform_sdk

[status]
faultRecordCntMax = 128

[media]
strMulticastAddress = 239.255.42.1
strRobotIPv4 = 192.168.110.51
videoPort = 5600
micPort = 5602
spkPort = 5604
videoPayloadType = 96
micPayloadType = 96
spkPayloadType = 96
jitterLatency_ms = 60
rtpMtu = 1400
```

| 字段 | 配置方法 |
| --- | --- |
| `[dds].strNetworkInterface` | 运行 SDK 的客户电脑网卡；DDS 和媒体共用，不填机器人的网卡 |
| `[dds].domain_id` | 机器人 DDS Bridge 对外通信域，与部署一致 |
| `[head].sourceName` | SDK 动作来源名，默认 `aheadform_sdk`；1～32 字符，小写字母开头，后续使用小写字母、数字、下划线 |
| `[status].faultRecordCntMax` | 一帧当前故障集合最多保存的记录数，示例 128；与 FIFO 可保存的帧数不同 |
| `[media].strMulticastAddress` | 视频、麦克风接收所用组播地址，与机器人媒体发送配置一致 |
| `[media].strRobotIPv4` | 机器人扬声器 RTP 音频接收目标 IPv4 |
| `videoPort`、`micPort`、`spkPort` | 视频接收、麦克风接收、扬声器发送端口，与机器人配置一致 |
| `videoPayloadType`、`micPayloadType`、`spkPayloadType` | 对应 RTP Payload Type，与机器人配置一致，示例均为 96 |
| `jitterLatency_ms` | 接收端抖动缓冲延迟，单位毫秒 |
| `rtpMtu` | RTP 打包大小限制，单位字节 |

保留全部 section 和必需键；未知键、缺失键、重复键或非法值会报错。**修改配置后退出并重新启动 SDK，无需重新编译。**

程序实际从**启动时的当前工作目录**读取 `AheadFormSDK_Config.ini`，不会自动按可执行文件所在目录查找。因此按第 6 节进入 `CodeProject` 后启动。当前菜单程序没有 `--config` 参数。

## 5. 手动编译

首次部署脚本已完成编译。修改源码后，或需要重新构建时，在 Linux 终端执行：

```bash
cd ~/AheadFormSDK/CodeProject
make clean
make -j2 CC=gcc
ldd ./AheadFormSDK
```

编译成功应输出 `CREATE AheadFormSDK`；`ldd` 输出不应出现 `not found`。Makefile 使用本机 GCC，通过 pkg-config 获取 CycloneDDS 和 GStreamer 依赖，自动发现源码和头文件目录。

已经构建完成且未修改源码时，无需每次 make。若此前使用过交叉编译环境，请在干净终端中构建，避免继承旧 sysroot、`PKG_CONFIG_LIBDIR` 或 `LD_LIBRARY_PATH`。

## 6. 启动与进入指令菜单

确认机器人已启动，在 Linux 终端执行：

```bash
cd ~/AheadFormSDK/CodeProject
./AheadFormSDK
```

初始化需要音量请求/回复和完整动作端点建立外部匹配，成功后打印：

```text
input help will print cmdInfo...
```

随后在 **SDK 程序内**输入下面的指令，再按回车：

```text
help
```

程序显示全部菜单项。每行执行一条指令，指令及参数用空格分隔；`cmd[名称] succeed.` 表示该菜单处理函数成功返回，具体业务结果还应按对应指令的输出判断。

本文 `bash` 代码块属于 Linux 终端，`text` 指令块属于 SDK 菜单。不要在 Linux 终端直接输入 `PlayScript`，也不要在 SDK 菜单直接输入 `cd`、`make` 或 `./AheadFormSDK`。

路径可以是绝对路径，也可以是相对启动目录的路径，建议客户使用绝对路径。包含空格时加单引号或双引号；菜单不自动展开 `~`、`$HOME` 或 shell 表达式。

## 7. 播放客户自己的 CSV + MP3

### 7.1 准备文件

文件可以放在客户 Linux 电脑的任意可读目录，不要求放在工程或可执行文件目录。建议在 Linux 终端创建专用目录：

```bash
mkdir -p ~/AheadFormScripts
```

将客户的 `show.csv` 和 `show.mp3` 放入该目录，保持同目录、同文件名主体。仓库不提供这两个文件。准备后，在 Linux 终端确认真实路径：

```bash
ls -l ~/AheadFormScripts/show.csv ~/AheadFormScripts/show.mp3
realpath ~/AheadFormScripts/show.csv
```

例如得到 `/home/customer/AheadFormScripts/show.csv`，后续菜单输入该实际路径。`customer` 应替换为真实用户名。

### 7.2 查看机器人状态

进入 SDK 菜单后输入：

```text
ReadStatus hardware latest
```

输出中的 `online`、`ready`、`fault`、`estop` 和 `source` 分别表示整机在线、就绪、故障、急停及当前动作来源。播放前应确保机器人已就绪、急停已解除，并等待其它动作来源释放；空闲时 `source=` 为空。

若 `source=face_fusion_brain`，可在 SDK 菜单请求退出交互模式、切换到 DEV 开发模式：

```text
OperatingMode 1
```

确认回复为 `operating mode requestId=..., result=1.`，表示远端报告切换成功。`result=2` 表示失败；请求超时不能视为切换成功，应检查机器人对应服务以及 `/system/operating_mode/control`、`/system/operating_mode/control/reply` 的 DDS Bridge 通路。

收到成功回复后，再次执行 `ReadStatus hardware latest`，等待动作源释放、`source=` 为空，再启动剧本。需要恢复交互时使用 `OperatingMode 2`。`PlayScript` 不会自动退出交互模式；`StopScript` 只停止 SDK 自己的剧本，不执行模式切换。

若只显示统计而没有状态样本，等待下一帧再查询，并检查状态 Topic 的外部通路。播放需要最近一秒内的整机状态。

**这条菜单指令不直接打印脸部和脖子运动许可。** 播放线程会检查许可、急停及安全停止状态；出现 `robot motion blocked: facePermit=... neckPermit=... estop=... safeStop=...` 时，根据字段和机器人当前状态处理。其它程序持有动作来源时会打印 `robot motion owned by ...`；按机器人既有流程结束该来源，不要改成其它程序的来源名称争抢控制。

### 7.3 启动播放

在 SDK 菜单输入：

```text
PlayScript /home/customer/AheadFormScripts/show.csv
```

只需要一个 CSV 参数，自动配对 `/home/customer/AheadFormScripts/show.mp3`。不接受第二个 MP3 参数。含空格路径示例：

```text
PlayScript "/home/customer/my scripts/show.csv"
```

菜单显示 `cmd[PlayScript] succeed.` 仅表示后台任务已启动。CSV 校验、MP3 解码和发送异步进行，后续是否播放成功应通过 `ScriptStatus` 和日志判断；菜单可以继续接收指令。当前不支持 `./AheadFormSDK /path/to/show.csv` 启动播放，也没有暂停、继续或循环播放指令。

### 7.4 查看进度与完成结果

播放期间或结束后，在 SDK 菜单输入：

```text
ScriptStatus
```

输出结构如下，数值随当前任务变化：

```text
script state=... result=0x........ elapsed=...s duration=...s motion=已发/总数 PCM=已发/总数 first-motion-delay=... us
```

| 字段 | 含义 |
| --- | --- |
| `state` | 0 空闲，1 加载，2 播放，3 已发送完毕，4 已停止，5 失败 |
| `result` | 任务结果码；失败时结合错误日志定位 |
| `elapsed`、`duration` | 已用时间与计划发送时间，单位秒 |
| `motion`、`PCM` | 动作帧和 PCM 帧的已发送数量/总数量 |
| `first-motion-delay` | 动作首帧相对音频首帧提交的时间差，单位微秒；尚未发送首帧时可为 0 |

`SendScript()` 结束日志中的 `motion=0 PCM=0` 表示已成功提交的动作帧和 PCM 帧数均为零；尚未发送首帧时，`first-motion-delay=0`。结合 `state`、`result` 和完整日志定位失败阶段。

当 `state=5 result=0x90000400` 时，表示 CSV 校验或 MP3 加载、解码阶段发生文件格式错误。在 SDK 菜单查询：

```text
ScriptStatus
errlog
```

结合本次播放的完整终端输出和 `errlog` 中的函数名、行号区分原因：

- 指向 `Head_LoadMotionCsv` 或 `ParseMotionRow`：按第 9.1 节检查 CSV 的 63 列表头、大小写、空值、数值范围和时间戳；如有 `rejected ... near line ...`，结合该提示排查。
- 指向 `Media_LoadMp3Pcm`：检查 CSV 同目录、同名的 `.mp3` 是否存在、可读且内容能正确解码；保留 `decode MP3(...) failed` 等错误信息。插件缺失时按下方常见问题检查 GStreamer 环境。

播放线程先校验 CSV、解码 MP3，再检查动作源和运动许可。因此，需要先修复文件加载错误；修复后若报告 `robot motion owned by ...` 或 `robot motion blocked: ...`，再按第 7.2 节处理模式、动作源和许可。

音频在电脑上解码为 16 kHz、单声道、S16LE PCM，每 20 ms 发送 640 字节，末帧不足补零。首帧 PCM 成功提交后建立时间起点，动作首帧安排在其后 **150 ms**，之后按 CSV 时间戳发送。某条轨道先结束时，另一条继续发送；两条通路均结束后任务才完成。

`state=3` 表示 SDK 本地发送结束，不表示机器人机械动作和扬声器输出已物理结束。150 ms 是发送时间补偿，实际观感仍受网络、机器人缓冲及执行影响。

### 7.5 按需停止与退出

仅在需要提前停止时，在 SDK 菜单输入：

```text
StopScript
```

成功返回前会等待播放线程退出并清理资源，随后可以用 `ScriptStatus` 确认状态。停止后再次运行 `PlayScript` 从文件开头开始；Stop 不是暂停。

结束 SDK 时，在菜单输入：

```text
exit
```

退出会先停止播放再清理资源。Stop 和退出仅停止后续本地发送，不能撤回机器人已收到的动作或播放缓冲中的音频。

## 8. 全部菜单指令

### 8.1 指令速查

以下均在 SDK 菜单内输入。`<名称>` 表示需要替换的参数，`[参数]` 表示可选参数，竖线表示候选值，不应原样输入。

| 指令语法 | 用途 | 示例 |
| --- | --- | --- |
| `help` | 显示全部指令 | `help` |
| `PlayScript <CSV路径>` | 后台播放 CSV 与同名 MP3 | `PlayScript /home/customer/AheadFormScripts/show.csv` |
| `ScriptStatus` | 查询播放状态及进度 | `ScriptStatus` |
| `StopScript` | 停止本地剧本发送 | `StopScript` |
| `QueryVolume` | 查询一次麦克风、扬声器音量 | `QueryVolume` |
| `WatchVolume <秒数>` | 每秒查询音量并观察指定时长 | `WatchVolume 10` |
| `PresetMotion <action>` | 请求预设动作 | `PresetMotion 2` |
| `SendNeck <pitch_rad> <yaw_rad> <roll_rad>` | 发送单帧脖子姿态 | `SendNeck 0 0 0` |
| `SendServo <angle_deg_1> ... <angle_deg_20>` | 发送 20 路舵机目标 | 完整 20 参数示例见 8.4 |
| `MotionSample <CSV路径> <t_us> <motion或face或inspect>` | 取样、查看或发送 CSV 指定时刻的一帧 | `MotionSample /home/customer/AheadFormScripts/show.csv 1000000 inspect` |
| `ReadStatus <topic> <latest或oldest或all>`；`ReadStatus <topic> export <路径>` | 读取状态或导出未读数据 | `ReadStatus hardware latest` |
| `WatchStatus <hardware或events> <秒数>` | 连续观察整机状态或故障事件 | `WatchStatus events 10` |
| `AudioDevice <operation> <type> [deviceId]` | 查询或切换音频设备 | `AudioDevice 2 1` |
| `MediaChannel <video> <mic> <playback> <vad>` | 控制机器人媒体通道 | `MediaChannel 0 0 2 0` |
| `WatchMedia <video或mic> <秒数>` | 接收媒体并打印帧信息 | `WatchMedia mic 10` |
| `SendWav <WAV路径>` | 单独发送指定格式 WAV 音频 | `SendWav /home/customer/AheadFormScripts/test.wav` |
| `OperatingMode <mode>` | 请求工作模式切换 | `OperatingMode 1` |
| `SafetyControl <action> [facePermit neckPermit]` | 动作许可、安全停止、清故障或急停 | `SafetyControl 1 1 1` |
| `HardwareControl <action> <device>` | 请求机器人硬件域日志或 daemon 退出 | `HardwareControl 2 2` |
| `errlog [文件路径]` | 打印或保存 SDK 本地错误日志 | `errlog /home/customer/sdk-errors.log` |
| `shell <系统命令及参数>` | 在 SDK 电脑执行系统命令 | `shell pwd` |
| `exit` | 清理资源并退出程序 | `exit` |

这些是当前 SDK 已实现的菜单调用示例。相应功能还需要机器人服务、DDS Bridge 开放对应 Topic，或媒体 RTP 通路可达；并非所有指令都已在每一种机器人部署上实测。

除 PlayScript 后台播放外，菜单指令按顺序执行：请求回复类指令等待回复或超时；Watch 类占用菜单直至指定时长结束；SendWav 占用菜单直至发送或报错。等待期间，同一菜单中输入的后续指令要等当前处理函数返回才执行，因此不要在希望随时输入 StopScript 时启动长时间 Watch。

### 8.2 剧本控制

`PlayScript`、`ScriptStatus`、`StopScript` 的完整流程见第 7 节。播放期间不要同时使用 SendWav 或其它直接操作扬声器发送资源的接口，也不要同时发送其它动作覆盖剧本。

### 8.3 音量查询

`QueryVolume` 无参数，等待一次查询回复，输出麦克风和扬声器音量百分比。`WatchVolume 10` 表示连续 10 秒，每秒发送一次音量查询并打印回复；秒数为正整数。两者需要音量请求/回复 DDS 通路。

`WatchVolume` 期间菜单被占用，结束后返回。音量示例在源码中使用条件编译提供三种接收方法，客户使用菜单无需切换接收方法。

### 8.4 动作控制

- **PresetMotion**：`action=1` 为 SLEEP，`2` 为 CENTER。示例 `PresetMotion 2` 请求回到预设居中动作，输出请求号、来源和远端返回码；远端接受不等于动作已执行到位。
- **SendNeck**：三个有限数依次为 pitch、yaw、roll，单位**弧度**。示例 `SendNeck 0 0 0` 发送零姿态目标；成功返回表示本地发布成功。
- **SendServo**：必须一次提供 20 个有限数，按机器人规定的舵机通道顺序排列，单位**度**。数值是逻辑角度，实际限位由机器人处理。下面仅演示 20 个参数的写法，使用时替换为机器人通道定义及允许范围内的目标：

```text
SendServo 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
```

- **MotionSample**：加载第 9 节规定的 CSV，以十进制微秒 `t_us` 取样；例如 `1000000` 表示 1 秒。相邻动作帧间线性插值，超出片段末尾取末帧。`inspect` 仅打印帧数、取样时间和脖子姿态；`motion` 发送完整动作单帧；`face` 发送脸部和双眼单帧。它不调度连续播放，也不发送 MP3。

上述发送操作需要对应动作接收端点，受机器人工作模式、动作仲裁和运动许可约束。返回成功不能替代机器人执行反馈。

### 8.5 状态、故障读取与导出

ReadStatus 支持以下 `topic`：

| 名称 | 内容 |
| --- | --- |
| `hardware` | 整机状态与当前动作来源 |
| `face` | 脸部硬件域状态 |
| `neck` | 脖子硬件域状态 |
| `media` | 媒体硬件域状态 |
| `power` | 电源硬件域状态 |
| `sensor` | 传感器硬件域状态 |
| `faults` | 当前故障集合 |
| `events` | 故障新增、变化、解除事件 |

`latest` 读取已收到的最新一帧，不推进未读消费位置；`oldest` 消费最旧的一帧未读数据；`all` 消费调用时可读的有限未读批次，不会无限等待新数据。已消费的数据不能通过这些指令回看。

示例按需执行，每行是独立操作：

```text
ReadStatus neck latest
ReadStatus events oldest
ReadStatus events all
ReadStatus events export /home/customer/fault-events.jsonl
```

`export` 把当前有限未读批次保存为 JSON Lines，并推进同一个 Reader 的消费位置。目标父目录必须存在；同名文件会被覆盖。输出 `exported`、`FIFO missed`、`source rollback` 和最近接收结果：它们分别表示导出数量、缓存遗漏、该导出批次检测到的源序列回退及接收/转换结果。有限缓存不构成完整历史日志；先执行 all 或 oldest 会影响随后可导出的数据。

普通读取输出的是状态摘要；需要完整字段可导出 JSON Lines 或调用公开 API。ReadStatus 没有发送查询请求，而是读取 SDK 持续订阅的数据。没有样本时可能仅打印统计；有样本也应检查接收时间，避免把旧缓存当作新的机器人状态。

WatchStatus 仅支持 `hardware`、`events`，例如 `WatchStatus hardware 10`。秒数为正整数，观察期间按接收的数据打印，结束后返回菜单；它使用独立 Reader，不消费 ReadStatus 的未读位置。

### 8.6 媒体设备、通道和收发

**AudioDevice** 的 `operation`：1 切换路由，2 查询设备列表，3 查询当前设备；`type`：0 麦克风，1 扬声器。查询仅输入两个数字，切换必须追加从实际设备列表获得的 deviceId：

```text
AudioDevice 2 1
AudioDevice 3 1
```

上面分别查询扬声器列表和当前扬声器。切换的语法示例为 `AudioDevice 1 1 <实际deviceId>`，不要把尖括号占位符直接输入。回复显示 `remote ret`、`queryDataValid`；有有效查询数据时打印设备 ID 和名称。它控制机器人设备，不是客户电脑设备。

**MediaChannel** 的四个参数依次控制机器人 video、mic、playback、vad；每项 0 保持、1 禁用、2 启用。例如 `MediaChannel 0 0 2 0` 仅请求启用播放通道。返回远端接受结果，实际通道状态还需观察机器人反馈。

**WatchMedia**：`WatchMedia video 10` 或 `WatchMedia mic 10`，启动相应本机接收通路，打印帧序号、字节数、PTS 及不连续信息，指定秒数结束后停止本次接收。它不显示视频窗口、不播放麦克风声音，也不自动保存文件。需机器人已启用对应发送通道，且组播、端口和 PT 配置一致。视频是 H.264 access unit，麦克风数据转换为 S16LE PCM。

**SendWav**：只接受 RIFF/WAVE、PCM、16 kHz、单声道、16 位小端音频；每 20 ms 发送一帧，尾帧补零，处理函数返回前菜单被占用。成功表示本地发送结束，不能保证机器人扬声器已全部播完。该命令是独立音频示例；PlayScript 的配对音频仍只接受 MP3。不要在剧本播放期间执行 SendWav。

### 8.7 整机控制

这些指令需要对应机器人控制服务和 Bridge 通路。请求超时不表示操作成功，也不能据此断定机器人一定没有收到请求。

**OperatingMode**：`OperatingMode 1` 请求 DEV，`OperatingMode 2` 请求 INTERACTIVE。回复打印 requestId 和 result；result=1 表示远端报告成功，2 表示失败。模式请求成功也应结合后续机器人状态判断动作条件。

**SafetyControl**：

| 输入 | 请求效果 |
| --- | --- |
| `SafetyControl 1 <facePermit> <neckPermit>` | 设置脸部、脖子运动许可；0 不许可，1 许可，例如 `SafetyControl 1 1 1` |
| `SafetyControl 2` | 请求机器人安全停止 |
| `SafetyControl 3` | 请求机器人清除故障 |
| `SafetyControl 4` | 请求机器人急停 |

只在 action=1 时提供两个许可参数。输出远端接受结果，完成状态还需检查机器人反馈。清故障请求不等于删除 SDK 本地缓存，也不保证故障原因已经解除；安全停止和急停的解除依照机器人的既有操作流程。

StopScript 仅停止电脑后续发送，SafetyControl 则是向机器人发控制请求，两者效果不同。

**HardwareControl**：`action=2` 请求打印指定硬件域错误日志，`3` 请求退出该域 daemon。`device`：1 脸部，2 脖子，3 媒体，4 电源，5 传感器。

例如 `HardwareControl 2 2` 请求脖子域打印错误日志；日志产生在机器人侧，不会自动回传为 SDK 的 errlog。输出的 remote ret 是远端接受结果。action=3 会影响机器人对应服务，不能作为日常播放步骤。

### 8.8 帮助、诊断与退出

- **help**：无参数，显示当前全部菜单及语法。
- **errlog**：无参数时打印本次 SDK 进程运行期间积累的本地错误日志；带文件路径时保存到该路径。父目录须存在；有日志时覆盖同名文件，没有日志时可直接成功返回而不创建文件。
- **shell**：在客户电脑执行系统命令并等待返回，例如 `shell pwd` 显示 SDK 工作目录；它不通过 SSH 操作机器人。菜单对参数先分词再组合交给系统 shell，复杂引号、空格路径和重定向请直接在独立 Linux 终端执行。菜单成功返回不能证明外部命令退出码为 0。
- **exit**：无参数，停止播放、释放 SDK 资源并退出，之后才能在该终端继续输入 Linux 命令。

## 9. 规定的剧本文件格式

### 9.1 CSV

CSV 必须包含且仅包含以下 63 列，列名区分大小写；按列名映射，允许规定列的顺序变化。完整表头：

```text
timestamp,istracked,eyeBlinkLeft,eyeLookDownLeft,eyeLookInLeft,eyeLookOutLeft,eyeLookUpLeft,eyeSquintLeft,eyeWideLeft,eyeBlinkRight,eyeLookDownRight,eyeLookInRight,eyeLookOutRight,eyeLookUpRight,eyeSquintRight,eyeWideRight,jawForward,jawLeft,jawRight,jawOpen,mouthClose,mouthFunnel,mouthPucker,mouthLeft,mouthRight,mouthSmileLeft,mouthSmileRight,mouthFrownLeft,mouthFrownRight,mouthDimpleLeft,mouthDimpleRight,mouthStretchLeft,mouthStretchRight,mouthRollLower,mouthRollUpper,mouthShrugLower,mouthShrugUpper,mouthPressLeft,mouthPressRight,mouthLowerDownLeft,mouthLowerDownRight,mouthUpperUpLeft,mouthUpperUpRight,browDownLeft,browDownRight,browInnerUp,browOuterUpLeft,browOuterUpRight,cheekPuff,cheekSquintLeft,cheekSquintRight,noseSneerLeft,noseSneerRight,tongueOut,neckPitch,neckYaw,neckRoll,eyePitchLeft,eyeYawLeft,eyeRollLeft,eyePitchRight,eyeYawRight,eyeRollRight
```

- `timestamp`：秒，首帧为 0，后续严格递增，内部转换为微秒。
- `istracked`：0 或 1，校验后不发送。
- 52 个 blendshape 字段：有限数，范围 `[0,1]`。
- 脖子及双眼姿态：有限数，单位**弧度**。
- `eyeRollLeft`、`eyeRollRight`：校验，但当前 DDS EyePose 没有对应字段，因此不发送。

不能缺列、重复列、增加其它列、留空值或包含空行。旧 `#AheadFormMotionCsv` 表头和其它 CSV schema 不被接受。

### 9.2 MP3 与资源

PlayScript 仅接受同目录、同名的 `.mp3` 配对，不支持 WAV 或其它剧本音频格式。MP3 在本机解码后转为规定发送格式，客户无需预先转为 PCM。

CSV 和解码后的音频会预先保存到客户电脑内存，长剧本需预留相应空间。视频和麦克风接收不会随剧本播放自动启动，但媒体缓存会在 SDK 初始化时准备。视频的 1000 槽、每槽 128 KiB 载荷约占 125 MiB，另有元数据、麦克风及其它缓存。分配失败会报错，不会自动降低容量。

## 10. 在客户程序中调用 API

公开入口在 [AheadForm_Task.h](../CodeProject/API/AheadForm_Task/Task/inc/AheadForm_Task.h)：

```c
int AheadForm_PlayScript(AHEADFORM_ALL_CTRL *pstAheadForm, const char *csvFilePath);
int AheadForm_StopScript(AHEADFORM_ALL_CTRL *pstAheadForm);
int AheadForm_GetScriptPlaybackInfo(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_PLAYBACK_INFO *pstInfo);
```

先调用 AheadForm_Init 并传入有效 INI 路径，再启动播放，通过 GetScriptPlaybackInfo 的 state/result 和帧进度判断结果。Play 会复制路径字符串，字符串只需在调用期间有效；运行仍需要机器人状态与运动条件满足。

Init、Play、Stop、Deinit 由客户串行调用；Info 可在初始化成功后、销毁前并发查询，Deinit 前应结束客户自己的查询线程。播放期间独占扬声器发送资源，不并发调用其 Start/Write/Stop。

菜单就是公开 API 的调用组合示例。剧本调用见 [UserScriptPlayback.c](../CodeProject/User/src/UserScriptPlayback.c)，请求回复示例见 [UserDomainRequests.c](../CodeProject/User/src/UserDomainRequests.c)，状态与媒体示例见 [UserDomainSamples.c](../CodeProject/User/src/UserDomainSamples.c)。SDK 和客户程序使用一致头文件重新编译。

## 11. 常见问题

| 现象 | 检查与处理 |
| --- | --- |
| 找不到 AheadFormSDK_Config.ini | 在 Linux 终端进入 `~/AheadFormSDK/CodeProject` 再运行 `./AheadFormSDK`；确认有效 INI 存在 |
| 找不到 AheadFormSDK 可执行文件 | 先完成部署脚本或第 5 节编译，程序生成在 CodeProject 内 |
| 编译缺头文件、库或 pkg-config 信息 | 先执行依赖部署脚本；终端运行 `pkg-config --modversion CycloneDDS gstreamer-app-1.0`，CycloneDDS 应为 11.0.1 |
| 运行报动态库或插件缺失 | 终端运行 `ldd ./AheadFormSDK`、`gst-inspect-1.0 mpg123audiodec`；按需要重新部署依赖，注意脚本会备份并覆盖配置 |
| 初始化发现超时 | 核对机器人已启动、有线桥接、路由网卡、外部 DDS 域、组播、防火墙及机器人 Bridge |
| PlayScript 指令不存在 | 确认在 SDK 菜单中输入，且运行的是本次构建的程序；用 help 查看 |
| PlayScript 返回成功但没有播放 | 菜单 succeed 只表示后台任务已启动，继续查 ScriptStatus；检查 CSV、同名 MP3、权限及后台加载错误 |
| state=5，result=0x90000400 | 文件加载格式错误；按第 7.4 节结合完整日志与 errlog 区分 CSV 校验和 MP3 加载、解码失败 |
| motion=0、PCM=0 | 已发送帧数均为零；结合 state、result 和完整日志定位失败阶段，说明见第 7.4 节 |
| 机器人持续交互或待机动作，source=face_fusion_brain | 按第 7.2 节执行 OperatingMode 1；确认远端 result=1，并等待动作源释放后播放；PlayScript、StopScript 不执行模式切换 |
| 找不到 MP3 | 检查 CSV 和 MP3 是否同目录同名，后缀是否为 .mp3；使用 realpath 得到的实际 CSV 路径 |
| 路径含空格或使用 ~ 无法打开 | 在 SDK 菜单用引号包围实际绝对路径，菜单不展开 ~、$HOME |
| ReadStatus 没有样本或状态陈旧 | 等待新状态，检查接收时间及该状态 Topic 的 Bridge 通路；缓存存在不表示持续收到新数据 |
| 播放报告 motion owned 或 motion blocked | 按第 7.2 节检查其它动作源、许可、急停和安全停止；根据机器人既有流程恢复 |
| 只听见声音、没有动作 | 查看机器人动作仲裁、工作模式、急停和执行状态；本地发布成功不等于机器人接受，硬件摘要不直接显示运动许可 |
| 某条控制命令超时 | 确认机器人对应服务及 Bridge 已开放所需请求/回复 Topic，不能将超时当成成功 |
| 输入 StopScript 没有立即执行 | 检查是否仍在执行 Watch 或 SendWav；菜单按顺序处理，需等前一条返回 |
| 停止后仍有短暂声音 | 可能是已进入网络或机器人缓冲的音频，Stop 仅终止后续本地发送 |
