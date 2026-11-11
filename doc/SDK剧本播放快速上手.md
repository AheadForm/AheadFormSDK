# SDK 剧本播放快速上手

本 SDK 在客户 Linux 电脑上读取规定格式的 CSV 和同名 MP3，通过 DDS 发送脸部、眼睛和脖子动作，通过 RTP/UDP 向机器人发送音频。CSV、MP3 不会复制到机器人。仓库不包含客户剧本，请自行准备符合本文表头的 CSV 和配套 MP3。

## 1. 环境与部署

推荐 Ubuntu 22.04 或 24.04，使用本机 GCC，无需 ARM64 交叉编译环境。电脑和机器人连接同一可互通的有线局域网；虚拟机使用桥接有线网络，不使用 NAT。首次安装需要访问 Ubuntu 软件源和 GitHub，并具有 sudo 权限。

获取工程，在 `AheadFormSDK` 仓库根目录执行部署脚本：

```bash
sudo apt-get update
sudo apt-get install -y git
git clone https://github.com/AheadForm/AheadFormSDK.git
cd AheadFormSDK
bash scripts/deploy_ubuntu.sh --robot 192.168.110.51 --domain 50
```

脚本安装 GCC、GStreamer 开发库及 MP3/RTP/H.264 插件，检查到机器人的路由，自动选择该路由使用的有线网卡，生成配置并编译。CycloneDDS 使用固定的 11.0.1 版本，缺少时下载、校验 SHA-256 后编译安装。脚本会备份原配置；再次运行将生成新的示例配置。

将 `--robot` 改为机器人实际地址，`--domain` 改为实际对外通信域。可追加 `--interface ens33`，指定与机器人路由一致的本地有线网卡。`bash scripts/deploy_ubuntu.sh --help` 查看参数。备份保留在本地，不提交到 Git。

已安装依赖时，可跳过 apt 安装阶段：

```bash
bash scripts/deploy_ubuntu.sh --robot 192.168.110.51 --domain 50 --skip-packages
```

示例环境为 Ubuntu 22.04 x86_64，网卡 `ens33`，机器人 `192.168.110.51`。`domain_id=50` 是该机器人 DDS Bridge 的**外部通信域**，不应直接使用机器人内部域或 Bridge 进程域代替。其它机器人按其部署的外部通信域填写。

## 2. 配置

有效配置文件为仓库中的 `CodeProject/AheadFormSDK_Config.ini`。程序读取当前工作目录下的 `AheadFormSDK_Config.ini`，因此运行前应先进入 `CodeProject`。配置如下：

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

`strNetworkInterface` 是运行 SDK 的电脑网卡，DDS 和 Media 共用；`strRobotIPv4` 是音频单播目标。配置键与结构体成员同名，未知、缺失、重复或非法配置会报错。`faultRecordCntMax` 是一帧当前故障集合可保存的记录数，与 FIFO 帧容量不同。

部署前可用以下命令查看到机器人的路由及本地地址：

```bash
ip -4 route get 192.168.110.51
ip -4 addr show
```

按实际环境修改网卡、对外 DDS 域和机器人 IPv4；媒体地址、端口与 Payload Type 必须和机器人配置一致。不要删掉其它必需键。配置修改后重启 SDK，无需重新编译。自定义配置后若再次运行部署脚本，脚本会备份并覆盖该配置。

SPMC、普通 Topic 和媒体 FIFO 均至少 1000 槽，音量 FIFO 为 1129 槽。视频每槽载荷固定 128 KiB，视频 FIFO 约 125 MiB，另有麦克风及状态缓存；内存分配失败会返回错误，不自动缩小容量。播放初始化不自动开启视频或麦克风接收。

## 3. 手动编译和运行

首次通过部署脚本安装依赖后，可使用本机 GCC 手动重新编译；不需要 ARM64 sysroot。打开新的终端，避免继承旧交叉编译环境。

```bash
cd CodeProject
make clean
make -j2 CC=gcc
ldd ./AheadFormSDK
./AheadFormSDK
```

`ldd` 输出不应出现 `not found`。Makefile 自动发现源码和头文件目录，依赖通过 `pkg-config` 获取；客户扩展程序可参考现有菜单源码调用公开 API。已经编译完成时只需在 `CodeProject` 中执行 `./AheadFormSDK`。

机器人需已启动，SDK 初始化等待音量请求/回复和完整动作端点建立外部匹配。机器人当前 Bridge 未开放的其它领域端点不作为本次初始化的硬依赖。

## 4. 播放、停止和状态

先查询机器人状态，确认当前动作源已释放、脸部和脖子运动获准：

```text
ReadStatus hardware latest
```

若状态查询提示没有新样本，等待机器人发布下一帧后重试。`source=` 应为空，且脸部和脖子运动许可有效；播放期间会显示 `aheadform_sdk`。若其它程序仍持有动作源，应通过机器人现有操作流程结束该动作源，再等待释放。不要把 SDK 的来源配置改成其它正在使用的动作源来争抢控制权。模式控制请求超时不能视为成功，播放前应以最新硬件状态和运动许可为准。

播放示例：

```text
PlayScript /path/to/show.csv
ScriptStatus
StopScript
exit
```

`PlayScript` 自动查找同目录、同文件名的 `.mp3`。例如 `/data/show.csv` 配对 `/data/show.mp3`。路径包含空格时加单引号或双引号：

```text
PlayScript "/home/customer/my scripts/show.csv"
```

命令成功表示播放任务已经启动；任务会异步加载和校验文件。用 `ScriptStatus` 查看真正的加载、发送结果。状态：`0=空闲`、`1=加载`、`2=播放`、`3=已发送完毕`、`4=已停止`、`5=失败`。失败时查看 `result` 和错误日志。

音频在电脑上解码成 16 kHz、单声道、S16LE PCM，每 20 ms 发送 640 字节，末帧不足时补零。第一帧 PCM 成功提交后记录单调时钟起点，第一帧动作在其后 **150 ms** 开始，后续帧按 CSV 时间戳调度。动作比音频短时，音频继续发送到结束；两条通路都结束后任务才完成。这个延迟是发送时间补偿，网络、机器人缓冲及机械响应仍会影响实际观感。

`StopScript` 停止本地动作和音频发送，等待线程退出并释放资源；不会撤回已经到达机器人的动作或扬声器缓冲中的音频。`exit` 同样会先结束播放再清理 SDK。播放期间不要同时调用独立的扬声器启停/写入接口。

## 5. 规定的文件格式

CSV 必须包含且仅包含以下 63 列，列名区分大小写；SDK 按列名映射，允许这些规定列的顺序变化。以下给出完整标准表头，无需下载任何客户样例：

```text
timestamp,istracked,eyeBlinkLeft,eyeLookDownLeft,eyeLookInLeft,eyeLookOutLeft,eyeLookUpLeft,eyeSquintLeft,eyeWideLeft,eyeBlinkRight,eyeLookDownRight,eyeLookInRight,eyeLookOutRight,eyeLookUpRight,eyeSquintRight,eyeWideRight,jawForward,jawLeft,jawRight,jawOpen,mouthClose,mouthFunnel,mouthPucker,mouthLeft,mouthRight,mouthSmileLeft,mouthSmileRight,mouthFrownLeft,mouthFrownRight,mouthDimpleLeft,mouthDimpleRight,mouthStretchLeft,mouthStretchRight,mouthRollLower,mouthRollUpper,mouthShrugLower,mouthShrugUpper,mouthPressLeft,mouthPressRight,mouthLowerDownLeft,mouthLowerDownRight,mouthUpperUpLeft,mouthUpperUpRight,browDownLeft,browDownRight,browInnerUp,browOuterUpLeft,browOuterUpRight,cheekPuff,cheekSquintLeft,cheekSquintRight,noseSneerLeft,noseSneerRight,tongueOut,neckPitch,neckYaw,neckRoll,eyePitchLeft,eyeYawLeft,eyeRollLeft,eyePitchRight,eyeYawRight,eyeRollRight
```

- `timestamp`：秒，首帧为 0，后续严格递增；内部转换为微秒。
- `istracked`：0 或 1，读取并校验，不发送。
- 上述全部 52 个 blendshape 字段，有限数，范围 `[0,1]`。
- `neckPitch`、`neckYaw`、`neckRoll`，单位弧度。
- `eyePitchLeft`、`eyeYawLeft`、`eyeRollLeft`、`eyePitchRight`、`eyeYawRight`、`eyeRollRight`，单位弧度。

所有字段必须有有效数字，不能缺列、重复列、增加其它列或空行。眼睛 roll 会校验，但当前 DDS EyePose 无此字段，因此不发送。旧 `#AheadFormMotionCsv` 表头不被接受。只支持 MP3 配对音频，不支持 WAV 或其它剧本格式。编码文件解码会预先保存到电脑内存，长剧本需要预留对应 PCM 和动作数据空间。

## 6. 在客户程序中调用

公开入口在 `CodeProject/API/AheadForm_Task/Task/inc/AheadForm_Task.h`：

```c
int AheadForm_PlayScript(AHEADFORM_ALL_CTRL *pstAheadForm, const char *csvFilePath);
int AheadForm_StopScript(AHEADFORM_ALL_CTRL *pstAheadForm);
int AheadForm_GetScriptPlaybackInfo(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_PLAYBACK_INFO *pstInfo);
```

先 `AheadForm_Init()`，确认机器人动作源可用及运动许可，再调用 Play；轮询 Info 的 `state/result` 和动作、PCM 进度。Play 复制路径，客户的路径字符串只需在调用期间有效。Init、Play、Stop、Deinit 由客户串行调用，Info 可在初始化成功后、销毁前并发查询；Deinit 前结束客户自己的查询线程。完整菜单组合示例见 `CodeProject/User/src/UserScriptPlayback.c`。SDK 和客户程序须使用一致头文件重新编译。

## 7. 排障

- **初始化发现超时**：核对有线桥接、到机器人的路由、外部 DDS 域、防火墙和机器人 DDS Bridge。能 ping 通不等于 DDS 组播与端点匹配正常。
- **只听见声音、没有动作**：检查 `ReadStatus hardware latest` 的当前动作源、急停和运动许可；等待其它动作源释放。SDK 的发送成功不能替代机器人仲裁接受。
- **找不到 MP3/格式错误**：检查同名配对、文件权限和规定表头；`ScriptStatus` 将显示失败。
- **编译找不到头文件或库**：运行 `pkg-config --modversion CycloneDDS gstreamer-app-1.0`，CycloneDDS 应为 `11.0.1`；确认已执行部署脚本。
- **GStreamer 插件缺失或动态库找不到**：重新执行部署脚本；不要使用交叉编译环境的 `PKG_CONFIG_LIBDIR`、sysroot 或 `LD_LIBRARY_PATH`。用 `gst-inspect-1.0 mpg123audiodec` 检查 MP3 解码插件。
- **配置文件找不到**：进入 `CodeProject` 后运行程序，并保留该目录中的有效 INI。
- **停止后仍有极短声音**：可能是已进入网络或机器人播放缓冲的音频，Stop 只终止后续本地发送。
