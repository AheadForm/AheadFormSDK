# AheadFormSDK 需求与总体架构设计

本文为当前公开 SDK 的客户版架构说明，按发布源码整理。它说明已实现能力、模块职责和接入约束；操作步骤见 [SDK 剧本播放快速上手](SDK剧本播放快速上手.md)，API 接入概览见 [AheadFormSDK 快速上手](AheadFormSDK快速上手.md)。

## 1. 运行位置与目标

SDK 运行在客户 Linux 电脑上，提供 C API 和终端菜单。机器人负责动作仲裁、硬件执行和音频输出，客户电脑负责本地剧本加载与发送。

```text
客户程序 / SDK 终端菜单
    ├── CSV → Head / Playback → DDS → 机器人 DDS Bridge → 动作仲裁与执行
    ├── MP3 → Media / Playback → RTP/UDP → 机器人音频接收与输出
    └── 状态和控制回复 ← DDS Bridge ← 机器人业务服务
```

客户剧本文件保留在客户电脑，SDK 不向机器人上传剧本文件。内部测试素材不包含在公开仓库中，客户按公开格式准备自己的 CSV 与同名 MP3。

已验证环境为 Ubuntu 22.04 x86_64、本机 GCC。依赖与部署步骤以操作指南为准；能够 ping 通机器人不足以证明 DDS 和 RTP 通路均已可用。

## 2. 当前能力与边界

| 领域 | 当前能力 |
| --- | --- |
| 配置与生命周期 | INI 解析、共享 DDS 资源、业务模块初始化和销毁、初始化期间的必需 DDS 端点发现 |
| 动作 | 完整动作、脸部、脖子和舵机目标发送；规定格式的 CSV 加载与采样；预置动作请求 |
| 媒体 | 音量查询与设置、设备和通道控制、视频/麦克风接收、扬声器 PCM 发送、MP3 解码与 WAV 音频发送示例 |
| 状态 | 整机及各硬件域状态、故障集合与事件、最新或未读样本读取、JSON Lines 导出 |
| 剧本 | 本地 CSV 与同名 MP3 异步协同发送、进度查询、停止、完成及失败状态 |
| 整机控制 | 工作模式、安全控制和硬件域控制请求；远端结果取决于机器人服务和 Bridge 部署 |

当前本地剧本入口不提供暂停、继续、循环、远端资源剧本编排或多任务 taskId。Record 模块当前只有初始化和销毁骨架，不提供完整多轨录制能力。视频/麦克风收发能力不代表已经实现录制产品。

## 3. 模块与公开入口

源码位于 `CodeProject`：

| 模块 | 职责 |
| --- | --- |
| User | 参数解析、菜单、公开 API 调用组合和结果展示 |
| API/AheadForm_Task/Task | 总句柄生命周期、整机控制、Playback 调度；Record 保留模块边界 |
| API/AheadForm_Task/ParseInitConfig | 配置读取与校验 |
| API/AheadForm_Task/RobotCtrl/head | 动作对象、CSV 读取与采样、动作发送和预置动作请求 |
| API/AheadForm_Task/RobotCtrl/media | 媒体控制、解码、RTP 管线和媒体帧缓存 |
| API/AheadForm_Task/RobotCtrl/status | 状态读取与导出 |
| API/AheadForm_Task/RobotCtrl/topic | Topic 注册、消息转换、Reader/Writer、请求匹配与接收缓存 |
| API/common | 时间、线程、DDS、FIFO、错误日志等基础能力 |

[AheadForm_Task.h](../CodeProject/API/AheadForm_Task/Task/inc/AheadForm_Task.h) 定义总句柄 `AHEADFORM_ALL_CTRL`。总句柄持有配置、共享 Topics、Head/Media/Status 子句柄、Playback 和 Record，并管理整机控制所需的 Reader/Writer。

AheadForm_Init 依次解析配置、初始化共享通信和各业务资源。AheadForm_Deinit 停止并等待播放线程，结束各业务资源后销毁共享通信。应用不能覆盖仍在使用的活动句柄；自行注册的 Reader/Writer 须先结束使用并注销。

具体函数声明及并发、借用约束以相应公开头文件为准。

## 4. 本地剧本数据流

`AheadForm_PlayScript` 复制 CSV 路径并创建后台线程。菜单返回成功表示线程已启动，应用通过 `AheadForm_GetScriptPlaybackInfo` 的 state、result 和帧计数判断后续结果。

后台依次执行：

1. 加载并校验 CSV，按列名映射动作数据。
2. 将同目录、同名 MP3 解码为 16 kHz、单声道 S16LE PCM。
3. 检查最近一秒内的整机状态、当前动作源、脸部/脖子运动许可、急停和安全停止。
4. 启动扬声器发送，每 20 ms 提交 640 字节 PCM；首帧提交后建立本地单调时钟起点。
5. 将首帧动作安排在音频首帧提交后 150 ms，后续动作按 CSV 时间戳调度。
6. 两条轨道发送结束，或遇到停止/错误时，结束媒体发送并释放加载资源。

CSV 恰好包含规定的 63 列，列名区分大小写：timestamp、istracked、52 个表情字段及 9 个头眼姿态字段。首帧时间为 0，后续严格递增；姿态单位为弧度。eyeRollLeft、eyeRollRight 参与校验但没有对应 DDS 发送字段。完整字段表见操作指南第 9 节。

两条轨道长度不同时，较长轨道继续发送。完整文件及解码后的音频预先保存在客户电脑内存中，长剧本需要相应内存。

## 5. 通信与动作源

主要 Topic 示例：

| 用途 | Topic |
| --- | --- |
| 完整动作发布 | /aheadform/control/aheadmotion |
| 脖子、脸部单独控制 | /aheadform/control/neck、/aheadform/control/face |
| 整机状态订阅 | /hw/state |
| 各硬件域状态订阅 | /hw/state/face、/hw/state/neck、/hw/state/media、/hw/state/power、/hw/state/sensor |
| 故障集合与事件 | /hw/fault/active、/hw/fault/events |
| 音量请求与回复 | /hw/media/volume/ask、/hw/media/volume/reply |
| 模式请求与回复 | /system/operating_mode/control、/system/operating_mode/control/reply |

完整注册表和 QoS 见 [AheadFormTopics.c](../CodeProject/API/AheadForm_Task/RobotCtrl/topic/src/AheadFormTopics.c)。DDS 网卡、对外 domain 与机器人 Bridge 配置相符；音频目标地址、端口和 payload type 与机器人 RTP 接收配置相符。

动作来源由配置 `[head].sourceName` 指定，默认示例为 `aheadform_sdk`。机器人仍拥有仲裁权，SDK 本地发布成功不等于机器人接受或执行。

当 `face_fusion_brain` 占用动作源时，SDK 不自动抢占。可请求 `OperatingMode 1` 切换 DEV；确认远端 result=1，并等待最新状态显示动作源释放后再播放。请求超时不能认定模式已切换。PlayScript 和 StopScript 不执行模式切换。

## 6. 状态、错误与并发约束

- 播放 state 为 0 空闲、1 加载、2 播放、3 本地发送完成、4 已停止、5 失败。失败时读取 result，并结合完整日志与 errlog。
- 0x90000400 表示文件格式错误，可能发生在 CSV 校验或 MP3 加载、解码阶段；文件加载先于动作源检查。
- motion 和 PCM 表示已成功提交的帧数；两个计数为零时，结合错误结果定位尚未完成发送的原因。
- ReadStatus 读取持续订阅的本地缓存。latest 不推进未读消费位置；oldest/all/export 会消费未读数据。FIFO missed 是缓存消费统计，不能直接作为网络丢包数。
- Init、Play、Stop、Deinit 由应用串行调用；Info 可在初始化成功后、销毁前并发查询，销毁前结束客户查询线程。
- 播放期间独占扬声器发送资源，不并发执行 SendWav 或直接操作扬声器发送接口。
- 请求发布、匹配到回复和远端业务成功是不同结果，应按具体接口检查远端回复字段。
- Stop 等待本地播放线程结束，只停止后续发送；已到机器人缓冲的数据仍可能继续输出。
- 150 ms 是本地发送调度补偿，不构成实际声音与机械运动的同步精度保证。

历史实测及测量限制见 [剧本播放验证记录](剧本播放验证记录.md)。
