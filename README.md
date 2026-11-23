# AheadFormSDK

**Build Your Own Embodied AI.**  
让你的 AI 拥有真实的面孔、声音、眼神与表达能力。

AheadFormSDK 是首形科技（AheadForm）面向仿生机器人提供的开放软件开发工具包。它通过标准化的运动控制、剧本演绎、音视频和设备状态接口，连接客户自己的 AI 模型、数字角色、交互内容及业务系统与首形仿生机器人。

客户既可以使用首形提供的智能交互系统（视产品配置而定），也可以通过 SDK 自主开发上层应用，将自己的智能和内容带入真实世界。

## How It Works ｜ 整体架构

![AheadForm SDK Architecture](sdk_architecture.png)

SDK 是连接智能应用与机器人硬件的桥梁：客户应用通过 SDK 下发动作、剧本与音频，机器人通过 SDK 返回摄像头、麦克风及设备状态数据，形成感知与表达的闭环。

## Use Cases ｜ 应用场景

基于AheadFormSDK的开放接口，用户可进一步开发的应用方向：

| 应用方向 | 典型场景 |
| :--- | :--- |
| **AI Agent** · AI 实体化 | 将客户自己的 AI 助手接入实体机器人 |
| **Research & Education** · 科研教育 | 人机交互、具身智能、心理学与认知科学 |
| **Robotics Integration** · 本体集成 | 与人形机器人本体或其他控制系统进行适配 |
| **IP & Character** · IP 角色 | 打造具有独立人格、声音与表达风格的实体角色 |
| **Entertainment** · 文娱演出 | 唱歌、舞台表演、互动娱乐 |
| **Retail & Exhibition** · 商业展陈 | 商场迎宾、品牌展示、展馆讲解 |




## Getting Started | 快速开始

在客户的 Linux 电脑上用 `PlayScript` 播放本地剧本：动作 CSV 必填，可另指定 MP3 或 PCM WAV，无需同名。SDK 通过 DDS 发送脸部、眼睛和脖子动作，通过 RTP/UDP 发送音频。剧本文件保留在客户电脑上，不上传到机器人。播放机器人上已有的资源时，使用 `EmotePlayback`。

SDK 还提供音量查询、动作控制、状态与故障读取、音频设备管理等菜单示例。部署步骤和全部 17 条指令见 [AheadFormSDK 快速上手](doc/AheadFormSDK快速上手.md)。

## 文档索引

| 文档 | 用途 |
| --- | --- |
| [AheadFormSDK 快速上手](doc/AheadFormSDK快速上手.md) | 部署、配置、全部菜单指令和工程移植 |
| [AheadFormSDK 架构设计说明](doc/AheadFormSDK架构设计说明.md) | 模块职责、DDS 与媒体通路、错误码和构建方式 |
| [CSV 剧本格式要求](doc/CSV剧本格式要求.md) | 动作 CSV 的字段、单位和文件要求 |

公开仓库不附带内部测试素材。文档中的 `show.csv` 和 `voice.mp3` 是示例文件名，请自行准备符合格式要求的文件。

## 1. 准备环境

AheadFormSDK 支持采用 glibc 的主流 Linux 发行版，可针对 **32 位和 64 位 Linux** 从源码构建。请按发行版和目标架构安装对应的 CycloneDDS、GStreamer 和媒体插件。已在 Ubuntu 22.04 验证 x86_64 原生构建，以及 ARM64 和 ARM32（armhf）交叉构建。Alpine 等采用 musl 的发行版当前需要适配线程调度接口。当前不提供 Windows/macOS 原生运行方式。

电脑与已启动的机器人连接可互通的有线局域网；Linux 虚拟机使用桥接有线网卡。首次安装需联网和 sudo 权限，日常运行使用普通用户。

## 2. 下载、安装依赖并编译

以下命令适用于 **Ubuntu**，示例将工程放在用户主目录：

**机器人 IP `192.168.110.51` 和外部 DDS 域 ID `50` 仅为示例。** 执行部署前，请将 `--robot` 和 `--domain` 换成现场实际值。

```bash
sudo apt-get update
sudo apt-get install -y git
cd ~
git clone https://github.com/AheadForm/AheadFormSDK.git
cd ~/AheadFormSDK
bash deploy/deploy_ubuntu.sh --robot 192.168.110.51 --domain 50
```

脚本按到机器人的路由选择本地有线网卡；需要显式指定时，追加 `--interface ens33`，将 `ens33` 换成与路由一致的实际网卡名。

脚本会安装构建依赖和媒体插件，检查到机器人的网络，并在缺少指定版本时下载、校验和安装 CycloneDDS 11.0.1。构建要求 GStreamer Core 和 App 均为 1.20 或更高版本。**构建及动态库检查通过后，脚本会备份并覆盖现有配置。** `--skip-packages` 仅跳过 apt 安装。

在其他 Linux 发行版上，使用该发行版的包管理器安装目标架构对应的 GCC、GNU Make、`pkg-config`、GStreamer 开发包及媒体插件，并安装 CycloneDDS 11.0.1。GStreamer Core 和 App 都需达到 1.20。确认 `pkg-config` 能找到依赖后，在仓库根目录运行 `make -C CodeProject`。编译后按下一节填写现场配置。

## 3. 配置放在哪里、怎样修改

```text
~/AheadFormSDK/
├── deploy/deploy_ubuntu.sh
└── CodeProject/
    ├── Makefile
    ├── AheadFormSDK_Config.ini     ← 有效配置文件
    └── AheadFormSDK                ← 编译生成的程序
```

在 Linux 终端打开配置：

```bash
cd ~/AheadFormSDK
nano CodeProject/AheadFormSDK_Config.ini
```

| 配置 | 应填写的内容 |
| --- | --- |
| `[dds].strNetworkInterface` | 客户电脑的本地有线网卡，DDS 与媒体共用 |
| `[dds].domain_id` | 机器人 DDS Bridge 对外通信域，当前示例为 `50` |
| `[media].strRobotIPv4` | 机器人音频接收目标 IPv4，当前示例为 `192.168.110.51` |
| `[media].spkPort` / `spkPayloadType` | 与机器人扬声器 RTP 配置一致，示例 `5604` / `96` |

将 `ip -4 route get 192.168.110.51` 中的 IP 换成实际值，可查看到机器人的本地路由网卡。核对部署生成的 [AheadFormSDK_Config.ini](CodeProject/AheadFormSDK_Config.ini)，保留文件中的必需项，按现场配置调整。修改后重启程序即可，无需重新编译。完整字段和取值范围见[配置字段说明](doc/AheadFormSDK快速上手.md#配置字段说明)。

## 4. 手动编译与启动菜单

部署脚本已完成首次编译；修改源码后，在 Linux 终端重新编译：

```bash
cd ~/AheadFormSDK/CodeProject
make
ldd ./AheadFormSDK
```

重新链接成功时输出 `CREATE AheadFormSDK`；`ldd` 不应出现 `not found`。确认机器人已上电至少 10 秒，再启动 SDK：

```bash
cd ~/AheadFormSDK/CodeProject
./AheadFormSDK
```

**程序从当前工作目录读取 `AheadFormSDK_Config.ini`，因此需要进入 `CodeProject` 后运行。** 当前没有 `--config` 参数，也不能直接把 CSV 路径作为可执行程序的启动参数。

启动成功后看到 `>>` 提示，在 **SDK 菜单中**输入以下指令并回车：

```text
help
```

等到 `>>` 再次出现后，可继续输入指令。菜单中的 `<...>` 是示例，输入时只键入尖括号里的内容。

## 5. 放置并播放客户剧本

将符合规定格式的动作 CSV 放在 Linux 客户端的可读目录中。需要配音时，再准备一个 MP3 或 PCM WAV。文件不必放在 SDK 目录中。

在 SDK 菜单中，按需选择其中一条指令，将路径换成自己的文件：

```text
PlayScript /home/customer/AheadFormScripts/show.csv
PlayScript /home/customer/AheadFormScripts/show.csv /home/customer/AheadFormScripts/voice.mp3
```

文件路径请使用不含空格的名称；菜单不会展开 `~` 或 `$HOME`。如现场需要先切换到开发模式，在播放前输入 `OperatingMode dev`。`PlayScript` 返回后可继续输入下一条指令，或输入 `exit` 退出。

带音频时，示例默认在客户端提交首帧 PCM 后延迟 150 ms 开始发送动作。实际声画时序还受网络、缓冲和机器人执行影响。动作文件的字段和单位见 [CSV 剧本格式要求](doc/CSV剧本格式要求.md)。

播放机器人上已有的资源时，使用 `EmotePlayback`，传入机器人上的文件名；暂停、继续或停止这类播放使用 `EmotePlayCtrl`。

## 6. 其它主要功能

以下均为启动 SDK 后输入的菜单指令；完整参数和输入示例见《快速上手》第 3 章。

| 功能 | 指令 |
| --- | --- |
| 客户端剧本播放 | `PlayScript` |
| 机器人端资源播放 | `EmotePlayback`、`EmotePlayCtrl` |
| 音量查询 | `QueryVolume`、`WatchVolume` |
| 动作控制 | `Tx1MotionPose`、`Tx1Face`、`Tx1NeckPose`、`TxServosTargetLogicDeg` |
| 状态与故障 | `ReadRecentState`、`WatchState` |
| 音频设备 | `AudioDevice` |
| 工作模式 | `OperatingMode` |
| 帮助与诊断 | `help`、`errlog`、`shell`、`exit` |

各功能需要机器人相应服务及 DDS Bridge/RTP 通路。`WatchVolume`、`WatchState` 和 `PlayScript` 返回后才能输入下一条指令。麦克风与视频 Reader、PCM 发送通过 SDK API 使用。

SDK 源码与公开头文件位于 `CodeProject/API/`，菜单调用示例位于 `CodeProject/User/`，部署脚本位于 `deploy/`。在自己的工程中链接 API 静态库的方法见 [AheadFormSDK 快速上手](doc/AheadFormSDK快速上手.md)。许可证见 [LICENSE](LICENSE)。
