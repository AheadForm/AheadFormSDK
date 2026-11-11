# AheadFormSDK

在 Linux 电脑上读取本地动作 CSV 和同名 MP3，通过 DDS 发送脸部、眼睛和脖子动作，通过 RTP/UDP 发送音频。剧本文件保留在客户电脑上，不上传到机器人。

## 获取与部署

推荐 Ubuntu 22.04 / 24.04，使用本机 GCC。电脑与机器人连接同一有线局域网；虚拟机需要桥接有线网卡。首次部署需要网络和 sudo 权限。

```bash
sudo apt-get update
sudo apt-get install -y git
git clone https://github.com/AheadForm/AheadFormSDK.git
cd AheadFormSDK
bash scripts/deploy_ubuntu.sh --robot 192.168.110.51 --domain 50
```

将 `--robot` 改为机器人实际 IPv4，`--domain` 改为机器人 DDS Bridge 的对外通信域。脚本自动选择到机器人路由使用的本地有线网卡，也可以追加 `--interface ens33` 指定同一网卡。

脚本安装 GCC、GStreamer 开发库和 MP3/RTP/H.264 插件；缺少 CycloneDDS 11.0.1 时下载固定版本、校验并编译安装。随后检查机器人连通性、生成配置并编译 SDK。再次执行会备份并重新生成配置；备份不会提交到 Git。依赖已安装时可追加 `--skip-packages`，仍会检查网络、插件、CycloneDDS 并重新编译。

## 修改配置与手动编译

有效配置为 `CodeProject/AheadFormSDK_Config.ini`。部署后如需调整，直接编辑该文件：

| 配置 | 含义 |
| --- | --- |
| `[dds].strNetworkInterface` | SDK 电脑的本地网卡，DDS 与媒体共用；用 `ip -4 route get 机器人IP` 查看 |
| `[dds].domain_id` | 机器人 Bridge 对外通信域；示例为 `50`，不是机器人内部域 |
| `[head].sourceName` | SDK 动作来源名称，默认 `aheadform_sdk` |
| `[media].strRobotIPv4` | 机器人音频单播目标 IPv4 |
| `[media].spkPort` / `spkPayloadType` | 必须与机器人扬声器 RTP 接收配置一致，示例为 `5604` / `96` |

其它媒体参数应与机器人部署一致；各 section 的必需键都要保留。示例网卡 `ens33` 和 IP `192.168.110.51` 并非每台客户电脑的固定值。配置修改后重启 SDK 生效，无需重新编译。

已安装依赖时，手动编译并运行：

```bash
cd CodeProject
make clean
make -j2 CC=gcc
./AheadFormSDK
```

程序从当前工作目录读取 `AheadFormSDK_Config.ini`，因此按上述方式进入 `CodeProject` 后运行。

## 播放客户自己的剧本

仓库不包含客户 CSV 或 MP3。准备符合规定表头的 `/path/to/show.csv` 和同目录的 `/path/to/show.mp3`，在 SDK 菜单输入：

```text
ReadStatus hardware latest
PlayScript /path/to/show.csv
ScriptStatus
StopScript
exit
```

先确认最新状态中的动作源空闲、脸部和脖子运动许可有效。`PlayScript` 自动配对同名 MP3，异步加载并播放；用 `ScriptStatus` 查询结果。路径含空格时使用引号。`StopScript` 停止本地后续发送，`exit` 先清理播放再退出。

音频解码为 16 kHz、单声道 S16LE PCM，每 20 ms 发送一帧；首帧动作安排在音频首帧提交后的 **150 ms**，后续按 CSV 时间戳调度。完成表示 SDK 发送完毕，实际声音和机械动作还受机器人缓冲及执行影响。

仅支持规定的 63 列 CSV 与 MP3，不支持旧版 CSV schema 或其它剧本格式。完整表头、配置说明、公开 API 和排障见 [SDK 剧本播放快速上手](doc/SDK剧本播放快速上手.md)。

## 目录

- `CodeProject/`：SDK 源码、公开头文件、菜单调用示例、有效配置和 Makefile。
- `scripts/`：Ubuntu 依赖安装、配置生成和构建脚本。
- `doc/`：客户快速上手说明。

许可证见 [LICENSE](LICENSE)。
