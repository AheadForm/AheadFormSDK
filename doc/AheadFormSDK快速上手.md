# AheadFormSDK 快速上手

本文适用于当前公开 SDK，提供部署、播放和 C API 接入的简要步骤。完整配置、63 列 CSV 表头、全部菜单和排障说明见 [SDK 剧本播放快速上手](SDK剧本播放快速上手.md)。

## 1. 环境与部署

已验证 Ubuntu 22.04 x86_64、本机 GCC 和有线桥接网络。机器人应已启动，并开放相应 DDS Bridge 与 RTP 通路。

在 Linux 终端执行：

```bash
git clone https://github.com/AheadForm/AheadFormSDK.git
cd AheadFormSDK
bash scripts/deploy_ubuntu.sh --robot 192.168.110.51 --domain 50
cd CodeProject
./AheadFormSDK
```

将机器人 IP 和 DDS 对外域替换为实际值。部署脚本会备份并覆盖配置；完整参数与依赖说明见完整指南。菜单程序从当前工作目录读取 `AheadFormSDK_Config.ini`，应进入 `CodeProject` 后启动。

## 2. 准备客户自己的文件

公开仓库不提供内部剧本 CSV、MP3 或录制数据。以下 `show.csv`、`show.mp3` 均为占位名称，需替换为客户自行准备的文件。

- CSV 与 MP3 放在客户 Linux 电脑的同一个可读目录，文件名主体相同。
- CSV 恰好包含规定的 63 列：`timestamp`、`istracked`、52 个表情权重和 9 个头眼姿态字段。列名区分大小写，列顺序允许变化。
- `timestamp` 单位为秒，首帧为 0，后续严格递增；表情权重范围为 `[0,1]`，头眼角度单位为弧度。
- 不能缺列、重复列、增加其它列、留空值或包含空行。两个 eye roll 字段参与校验，但没有对应 DDS 发送字段。
- PlayScript 仅配对同名 `.mp3`，不接受 WAV 作为剧本配对音频。

完整表头与校验规则以完整指南第 9 节为准。

## 3. 使用 SDK 菜单播放

先在 SDK 的 `>>` 菜单输入：

```text
ReadStatus hardware latest
```

确认状态持续更新、机器人就绪、急停已解除，且其它动作源已释放。若显示 `source=face_fusion_brain`，可请求退出交互：

```text
OperatingMode 1
```

只有远端回复 `result=1` 才表示远端报告切换成功；超时不能视为成功。再次读取最新状态，等待 `source=` 为空并满足运动许可后播放。

```text
PlayScript /home/customer/AheadFormScripts/show.csv
ScriptStatus
```

将路径与用户名替换为实际值。菜单不会展开 `~` 或 `$HOME`。PlayScript 不自动切换模式；成功返回只表示后台任务启动，后续仍可能加载或发送失败。

需要提前停止时输入 `StopScript`，结束程序输入 `exit`。停止只终止 SDK 后续发送，不能撤回机器人已缓存的数据，也不会退出其它程序的交互模式。恢复交互可请求 `OperatingMode 2`，同样需要确认远端回复。

## 4. 在客户程序中调用 C API

公开入口为 [AheadForm_Task.h](../CodeProject/API/AheadForm_Task/Task/inc/AheadForm_Task.h)，播放状态定义见 [Playback.h](../CodeProject/API/AheadForm_Task/Task/inc/Playback.h)。

```c
int AheadForm_Init(AHEADFORM_ALL_CTRL *pstAheadForm, const char *iniConfigFilePath);
int AheadForm_PlayScript(AHEADFORM_ALL_CTRL *pstAheadForm, const char *csvFilePath);
int AheadForm_GetScriptPlaybackInfo(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_PLAYBACK_INFO *pstInfo);
int AheadForm_StopScript(AHEADFORM_ALL_CTRL *pstAheadForm);
int AheadForm_Deinit(AHEADFORM_ALL_CTRL *pstAheadForm);
```

调用顺序为初始化、启动播放、查询状态、按需停止、销毁。每一步检查返回值；初始化或业务失败需要输出错误日志，并按 API 生命周期结束已创建的资源。菜单中的实际调用示例见 [UserScriptPlayback.c](../CodeProject/User/src/UserScriptPlayback.c) 和 [main.c](../CodeProject/User/src/main.c)。

Init、Play、Stop、Deinit 由应用串行调用；Info 可以在初始化成功后、销毁前并发查询，销毁前须结束应用自己的查询线程。Play 会复制路径，后台预加载 CSV 和 MP3。播放期间独占扬声器发送资源，不并发调用 SendWav 或扬声器 Start/Write/Stop。

| state | 含义 |
| --- | --- |
| 0 | 空闲 |
| 1 | 加载中 |
| 2 | 播放中 |
| 3 | 本地发送完成 |
| 4 | 已停止 |
| 5 | 失败，检查 result 与日志 |

本地发送完成不等于机器人已物理执行完毕。当前播放入口不提供暂停、继续、循环或 taskId 参数。

## 5. 常见问题

| 现象 | 处理 |
| --- | --- |
| 菜单显示 succeed，但没有播放 | 查询 ScriptStatus，检查后台日志；succeed 仅表示任务已启动 |
| state=5、result=0x90000400 | 文件格式错误；结合完整输出和 errlog，区分 CSV 校验与 MP3 加载、解码失败 |
| motion=0、PCM=0 | 已发送帧数为零，结合 state/result 定位失败阶段 |
| motion owned by face_fusion_brain | 请求 OperatingMode 1，确认 result=1，并等待动作源释放 |
| motion blocked | 检查脸部/脖子许可、急停与安全停止字段；按机器人既有流程恢复 |
| OperatingMode 超时 | 检查机器人服务及对应请求、回复 Topic 的 Bridge 通路 |

历史实测结果与限制见 [剧本播放验证记录](剧本播放验证记录.md)。
