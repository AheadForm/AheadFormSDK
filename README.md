# AheadFormSDK

在客户 Linux 电脑上播放本地剧本：读取规定格式的动作 CSV 和同名 MP3，通过 DDS 发送脸部、眼睛和脖子动作，通过 RTP/UDP 发送音频。剧本文件保留在客户电脑，不上传到机器人。

SDK 还提供音量查询、动作控制、状态与故障读取、媒体设备和通道控制等菜单示例。完整步骤和全部 22 条指令见 [SDK 剧本播放快速上手](doc/SDK剧本播放快速上手.md)。

## 1. 准备环境

已验证 **Ubuntu 22.04 x86_64 + 本机 GCC**。Ubuntu 24.04 尚未验证；当前不提供 Windows/macOS 原生运行方式，也不需要 ARM64 交叉编译环境。

电脑与已启动的机器人连接可互通的有线局域网；Linux 虚拟机使用桥接有线网卡。首次安装需联网和 sudo 权限，日常运行使用普通用户。

## 2. 下载、安装依赖并编译

以下命令在 **Linux 终端**执行，示例将工程放在用户主目录：

```bash
sudo apt-get update
sudo apt-get install -y git
cd ~
git clone https://github.com/AheadForm/AheadFormSDK.git
cd ~/AheadFormSDK
bash scripts/deploy_ubuntu.sh --robot 192.168.110.51 --domain 50
```

将机器人 IP 和 DDS Bridge 对外通信域改成实际值。脚本按到机器人的路由选择本地有线网卡；需要显式指定时，追加 `--interface ens33`，网卡必须与路由一致。

脚本会安装 GCC、GStreamer 开发库和插件，缺少指定版本时编译安装 CycloneDDS 11.0.1，然后检查网络、备份并生成配置、编译 SDK。**再次运行会备份并覆盖现有配置。** `--skip-packages` 仅跳过 apt 阶段，仍会检查依赖、网络、生成配置并构建。参数详见快速上手文档。

## 3. 配置放在哪里、怎样修改

```text
~/AheadFormSDK/
├── scripts/deploy_ubuntu.sh
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

用 `ip -4 route get 192.168.110.51` 查看本地路由网卡。保留配置中全部必需键，完整 INI 和各字段解释见快速上手文档。修改配置后重启程序即可，无需重新编译。

## 4. 手动编译与启动菜单

部署脚本已完成首次编译；修改源码后，在 Linux 终端重新编译：

```bash
cd ~/AheadFormSDK/CodeProject
make clean
make -j2 CC=gcc
ldd ./AheadFormSDK
```

编译成功输出 `CREATE AheadFormSDK`；`ldd` 不应出现 `not found`。已经构建完成时直接启动：

```bash
cd ~/AheadFormSDK/CodeProject
./AheadFormSDK
```

**程序从当前工作目录读取 `AheadFormSDK_Config.ini`，因此需要进入 `CodeProject` 后运行。** 当前没有 `--config` 参数，也不能直接把 CSV 路径作为可执行程序的启动参数。

启动成功后看到 `input help will print cmdInfo...`，在 **SDK 菜单中**输入以下指令并回车：

```text
help
```

## 5. 放置并播放客户剧本

仓库不附带客户剧本。将符合规定格式的 CSV 与同名 MP3 放在客户电脑的同一个可读目录，例如：

```text
/home/customer/AheadFormScripts/
├── show.csv
└── show.mp3
```

把 `customer` 替换为实际 Linux 用户名。剧本不必放在工程、配置目录或机器人上。

先在 SDK 菜单输入 `ReadStatus hardware latest` 查看最新状态；等待已有动作源释放，并检查机器人就绪和急停状态。播放时 SDK 还会检查运动许可，失败原因见日志。

在 SDK 菜单中启动播放：

```text
PlayScript /home/customer/AheadFormScripts/show.csv
```

自动配对同目录 `show.mp3`。路径含空格时加引号；菜单不会展开 `~` 或 `$HOME`。播放异步进行，可以随后输入：

```text
ScriptStatus
```

`state=2` 表示播放中，`3` 表示本地发送完成，`5` 表示失败，应查看 `result` 和日志。需要提前停止时输入：

```text
StopScript
```

结束使用时输入 `exit`。Stop 停止后续本地发送，无法撤回机器人已经缓存的音频或动作。

仅支持规定的 **63 列 CSV + MP3**。动作首帧安排在音频首帧提交后的 **150 ms**；实际声音和机械响应还受网络、缓冲和执行影响。完整格式、状态字段和操作示例见 [快速上手文档](doc/SDK剧本播放快速上手.md)。

## 6. 其它主要功能

以下均为启动 SDK 后输入的菜单指令；完整参数、示例和前提见快速上手文档第 8 节。

| 功能 | 指令 |
| --- | --- |
| 剧本播放 | `PlayScript`、`ScriptStatus`、`StopScript` |
| 音量查询 | `QueryVolume`、`WatchVolume` |
| 动作控制 | `PresetMotion`、`SendNeck`、`SendServo`、`MotionSample` |
| 状态与故障 | `ReadStatus`、`WatchStatus` |
| 媒体设备、通道与收发 | `AudioDevice`、`MediaChannel`、`WatchMedia`、`SendWav` |
| 整机控制 | `OperatingMode`、`SafetyControl`、`HardwareControl` |
| 帮助与诊断 | `help`、`errlog`、`shell`、`exit` |

各功能需要机器人相应服务及 DDS Bridge/RTP 通路。菜单中存在指令不表示所有机器人部署均已开放对应通路。Watch 类指令和 SendWav 执行期间会占用菜单，返回后才能输入下一条指令。

SDK 源码与公开头文件位于 `CodeProject/API/`，菜单调用示例位于 `CodeProject/User/src/`，部署脚本位于 `scripts/`。许可证见 [LICENSE](LICENSE)。
