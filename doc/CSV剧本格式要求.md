# CSV 剧本格式要求

适用于 `AheadForm_console` 上传及其使用的 `emote_player`。

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

角度按接口约定使用弧度（rad）；眼球角度须与板端 `eye_pose_unit` 配置一致。CSV 不自带单位标记，播放器不会根据列名换算单位。角度填 0 表示中性目标角，不表示保持当前姿态。

## 可选字段

| 字段 | 处理方式 |
| --- | --- |
| `neckX`、`neckY`、`neckZ` | 头部位置分量，可省略；省略时为 0。使用时需与机器人坐标和单位约定一致。 |

`leftEyeRoll`、`rightEyeRoll` 等其他额外列不参与当前播放器控制，可删除。`headPitch/headYaw/headRoll` 和 `leftEyePitch/leftEyeYaw/rightEyePitch/rightEyeYaw` 不是必需字段的别名，不能替代上表列名。

## 基本约束

- 使用 UTF-8、英文逗号分隔；首行为表头，后续每行一帧，至少一帧。
- 数值不得为空或为 NaN/Inf；每行列数与表头一致。
- `timestamp` 非负且不倒退，最后一帧时间须在 0.001～600 秒之间；单文件不超过 32 MiB。
- 无需在 CSV 中填写音频字段；MP3 在控制台单独选择。
