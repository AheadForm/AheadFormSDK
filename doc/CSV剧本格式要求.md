# CSV 剧本格式要求

本文以当前 SDK 的 `Head_CheckMotionCsv()`、`Head_LoadMotionCsv()` 读取规则为准，适用于 C/C++ 或 Python User 菜单的 `PlayScript` 客户端播放。实现见 [HeadMotionFile.c](../CodeProject/API/AheadForm_Task/RobotCtrl/head/HeadMotionFile.c)，播放示例见 [SDK 快速上手](AheadFormSDK快速上手.md#3-指令菜单说明)。控制台上传和机器人端播放器可能另有约束，以对应项目说明为准。

## 必需字段（共 60 列）

字段名大小写必须完全一致；列顺序不限，不要重复列名。

| 字段 | 含义与填写要求 |
| --- | --- |
| `timestamp` | 相对时间，单位秒。建议从 0 开始，按帧率递增；30fps 时第 i 帧为 i/30（i 从 0 开始）。 |
| `neckPitch`、`neckYaw`、`neckRoll` | 脖子俯仰、偏航、侧倾目标角。 |
| `eyePitchLeft`、`eyeYawLeft` | 左眼球俯仰、偏航目标角。 |
| `eyePitchRight`、`eyeYawRight` | 右眼球俯仰、偏航目标角。 |
| 下列 52 个 Blendshape 字段 | 表情权重，按 0～1 提供；不用的通道也必须保留，可填 0。 |

```text
eyeBlinkLeft eyeLookDownLeft eyeLookInLeft eyeLookOutLeft eyeLookUpLeft eyeSquintLeft
eyeWideLeft eyeBlinkRight eyeLookDownRight eyeLookInRight eyeLookOutRight eyeLookUpRight
eyeSquintRight eyeWideRight jawForward jawLeft jawRight jawOpen
mouthClose mouthFunnel mouthPucker mouthLeft mouthRight mouthSmileLeft
mouthSmileRight mouthFrownLeft mouthFrownRight mouthDimpleLeft mouthDimpleRight mouthStretchLeft
mouthStretchRight mouthRollLower mouthRollUpper mouthShrugLower mouthShrugUpper mouthPressLeft
mouthPressRight mouthLowerDownLeft mouthLowerDownRight mouthUpperUpLeft mouthUpperUpRight browDownLeft
browDownRight browInnerUp browOuterUpLeft browOuterUpRight cheekPuff cheekSquintLeft
cheekSquintRight noseSneerLeft noseSneerRight tongueOut
```

SDK 的颈部和眼球角度均按弧度（rad）传递；CSV 不自带单位标记，SDK 不会根据列名自动换算。机器人端也须采用匹配的角度约定，不要把角度值（degree）直接填入这些列。角度填 0 表示中性目标角，不表示保持当前姿态。

## 可选字段

| 字段 | 处理方式 |
| --- | --- |
| `neckX`、`neckY`、`neckZ` | 头部位置分量，可省略；省略时为 0。使用时需与机器人坐标和单位约定一致。 |

`leftEyeRoll`、`rightEyeRoll` 等其他额外列不参与当前播放器控制，可删除。`headPitch/headYaw/headRoll` 和 `leftEyePitch/leftEyeYaw/rightEyePitch/rightEyeYaw` 不是必需字段的别名，不能替代上表列名。

## 基本约束

- 使用 UTF-8、英文逗号分隔；可带 UTF-8 BOM，支持 LF 或 CRLF 换行。首行为表头，后续每行一帧，至少一帧。
- 表头使用精确列名，不加前后空格；当前解析器不支持带引号字段或单元格内逗号。
- 数值不得为空或为 NaN/Inf；每行列数与表头一致。保留的额外列也必须是有效数值，即使 SDK 不使用该列。
- 每帧 `timestamp` 在 0～600 秒内且不倒退；最后一帧时间至少为 0.001 秒。允许相同时间戳，建议首帧从 0 开始；单文件不超过 32 MiB。
- 无需在 CSV 中填写音频字段；`PlayScript csvFilePath [audioFilePath]` 的可选音频参数支持 MP3 或 PCM WAV。

## SDK 读取与播放说明

`Head_LoadMotionCsv()` 将时间换算为微秒并减去首帧时间，返回的 `duration_us` 是归一化后的末帧时间。`Head_CalcInterpolateMotionPose()` 按请求时间插值；时间相同的帧取最后一行，超过末帧后保持末帧。CSV 采样率不必与发送周期相同，当前 User 播放示例按 20 ms 周期处理；指定音频时，首帧 PCM 提交后延迟 150 ms 启动动作。

加载 API 的帧容量由调用方提供的缓冲区决定；当前 User 示例最多保存 32,768 个动作帧，解码 PCM 缓冲区为 20 MiB，超出会返回缓冲区错误。API 不会因此自动扩大缓冲区。C/C++ 菜单的文件路径不能含空格；Python 交互输入可用引号包住含空格路径，相对路径均按启动时的工作目录解释。
