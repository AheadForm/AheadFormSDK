# AheadFormSDK 快速上手
## CSV 剧本播放
> 接口预览版，随交付版本更新。以下播放接口按目标交付形态约定，具体声明以交付版头文件为准。

使用者在客户端本地加载 CSV，驱动机器人表情、头部和双眼姿态。

## 1. 准备配置和文件

将 CSV 保留在客户端可访问的目录，在运行目录准备 `aheadform.ini`：

```ini
[dds]
interface = wlan0
domain_id = 11
```

`interface` 改为运行 SDK 的客户端实际网卡名；`domain_id` 与目标机器人 DDS Bridge 的对外 `domain_id` 一致，上述数值仅为示例，不叠加桥内部的 `external_domain_offset`。机器人端应已启动桥和仲裁服务，并允许外部动作源控制。

数据通路为：**输入CSV → SDK 本地播放 → DDS Bridge → 仲裁层 → 机器人执行**。SDK 负责将动作封装到 `/aheadform/control/aheadmotion`，客户无需自行创建 DDS Topic，也无需处理 Protobuf 通路中的 104 字节表情量化格式。

## 2. CSV 格式和单位

示例文件无需重排表头。SDK 按列名识别动作，映射到固定的 52 路 ARKit 顺序，不按 CSV 列位置直接复制。

| CSV 字段 | 约定 |
|---|---|
| `EyeBlinkLeft` 至 `TongueOut` 等完整 52 路表情列 | 归一化权重，范围 `[0,1]` |
| `HeadPitch`、`HeadYaw`、`HeadRoll` | 头部俯仰、偏航、侧倾，单位为弧度 `rad` |
| `LeftEyePitch`、`LeftEyeYaw`、`RightEyePitch`、`RightEyeYaw` | 左右眼俯仰、偏航，单位为弧度 `rad` |
| `LeftEyeRoll`、`RightEyeRoll` | 可以保留；当前 DDS 动作消息无对应字段，不下发 |
| `Timecode`、`BlendshapeCount` | 元数据，不用作播放时间或动作列数量的依据 |

角度不需要转成度。未提供的头部位移在完整动作消息中填 `0`。首版要求完整 52 路表情及上述 7 路头眼角度；缺列、空值、非有限数或表情权重越界时报错，不静默补齐缺失姿态。仅含表情的 test2 暂不列为首版可直接播放格式。

角度极性以**机器人自身视角**为准：

| 姿态 | 正值 | 负值 |
|---|---|---|
| 头部 Pitch | 低头，下巴向下 | 抬头，下巴向上 |
| 头部 Yaw | 向左转头 | 向右转头 |
| 头部 Roll | 向右侧倾，右耳靠近右肩 | 向左侧倾，左耳靠近左肩 |
| 左右眼 Pitch | 向下看 | 向上看 |
| 左右眼 Yaw | 向右看 | 向左看 |

播放时显式传入源帧率 `30.0`。首行数据时间为 `0`，第 `i` 行数据时间为 `i / fps` 秒，`i` 从 0 开始；不将此批文件中的 `Timecode` 当作秒时间戳。帧率必须为有限正数。SDK 在后台按时间轴调度，调用者无需逐行发送或自行延时。

## 3. 主要接口

包含 `AheadForm_Task.h`，使用总句柄 `AHEADFORM_ALL_CTRL`。所有接口成功返回 `RET_SUCCESS`，失败返回错误码；可调用 `PrintAllErrLog()` 查看已记录的原因。

```c
typedef enum
{
	AHEADFORM_PLAYBACK_STATE_PLAYING = 0,
	AHEADFORM_PLAYBACK_STATE_PAUSED,
	AHEADFORM_PLAYBACK_STATE_STOPPED,
	AHEADFORM_PLAYBACK_STATE_FINISHED,
	AHEADFORM_PLAYBACK_STATE_FAILED
} ENUM_AHEADFORM_PLAYBACK_STATE;

typedef struct
{
	ENUM_AHEADFORM_PLAYBACK_STATE state;
	double position_s;
	double duration_s;
	int errorCode;
} AHEADFORM_PLAYBACK_INFO;

int AheadForm_Init(AHEADFORM_ALL_CTRL *pstAheadForm, const char *iniConfigFilePath);
int AheadForm_Deinit(AHEADFORM_ALL_CTRL *pstAheadForm);
int AheadForm_PlayCsvScript(AHEADFORM_ALL_CTRL *pstAheadForm, const char *csvFilePath, double fps, unsigned long long *pTaskId);
int AheadForm_PausePlayback(AHEADFORM_ALL_CTRL *pstAheadForm, unsigned long long taskId);
int AheadForm_ResumePlayback(AHEADFORM_ALL_CTRL *pstAheadForm, unsigned long long taskId);
int AheadForm_StopPlayback(AHEADFORM_ALL_CTRL *pstAheadForm, unsigned long long taskId);
int AheadForm_GetPlaybackInfo(AHEADFORM_ALL_CTRL *pstAheadForm, unsigned long long taskId, AHEADFORM_PLAYBACK_INFO *pstInfo);
```

`AheadForm_Init` 完成共享通信及业务初始化，无需另外初始化 Topics。`AheadForm_PlayCsvScript` 完成文件加载和格式检查后返回任务编号，由后台继续播放；只有成功时输出的任务编号有效，每个实例同时只运行一个播放任务，播放中或暂停时再次启动会报错。

暂停冻结本地时间轴，继续从暂停位置恢复；停止结束后续发送。`GetPlaybackInfo` 返回当前任务的本地位置和总时长，单位为秒；总时长为最后一帧的时间，即 `(数据行数 - 1) / fps`。状态为 `FAILED` 时 `errorCode` 为非零播放错误码，其余状态为 `RET_SUCCESS`。查询成功仍须检查播放状态，以区分任务失败和查询失败。

任务结束后可查询其最终状态，直至成功启动下一任务或销毁 SDK。任务编号仅用于所属实例；无效或已失效编号报错。`AheadForm_Deinit` 结束后台播放并等待任务退出后释放资源；初始化成功后，不论播放成功或失败，都应调用它。

## 4. 最小 C 示例

以下为 Linux/POSIX 调用示例。CSV 路径位于运行 SDK 的客户端；相对路径以程序工作目录为基准。示例每 100 ms 查询一次进度，该间隔与 CSV 的 30 fps 相互独立。

```c
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include "AheadForm_Task.h"

int main(void)
{
	AHEADFORM_ALL_CTRL stAheadForm = {0};
	AHEADFORM_PLAYBACK_INFO stInfo;
	struct timespec stPollInterval = {0, 100000000};
	unsigned long long taskId = 0;
	int ret, exitCode = EXIT_SUCCESS;

	if ( (ret = AheadForm_Init(&stAheadForm, "aheadform.ini")) != RET_SUCCESS )
	{
		PrintAllErrLog();
		return EXIT_FAILURE;
	}

	if ( (ret = AheadForm_PlayCsvScript(&stAheadForm, "test1_30fps/FF_NR_C_arkit61_lp.csv", 30.0, &taskId)) == RET_SUCCESS )
	{
		while (1)
		{
			if ( (ret = AheadForm_GetPlaybackInfo(&stAheadForm, taskId, &stInfo)) != RET_SUCCESS )
				break;

			printf("playback %.2f / %.2f s\r\n", stInfo.position_s, stInfo.duration_s);
			if (stInfo.state == AHEADFORM_PLAYBACK_STATE_FAILED)
			{
				ret = stInfo.errorCode;
				break;
			}

			if (stInfo.state == AHEADFORM_PLAYBACK_STATE_FINISHED || stInfo.state == AHEADFORM_PLAYBACK_STATE_STOPPED)
				break;

			if (nanosleep(&stPollInterval, NULL) < 0)
			{
				printf("%s() call nanosleep() failed: %s\r\n", __FUNCTION__, strerror(errno));
				exitCode = EXIT_FAILURE;
				break;
			}
		}
	}

	if (ret != RET_SUCCESS)
	{
		printf("SDK error: 0x%08x\r\n", (unsigned int)ret);
		PrintAllErrLog();
		exitCode = EXIT_FAILURE;
	}

	if ( (ret = AheadForm_Deinit(&stAheadForm)) != RET_SUCCESS )
	{
		PrintAllErrLog();
		exitCode = EXIT_FAILURE;
	}

	return exitCode;
}
```

暂停、继续和停止用于应用自己的控制流程。以下是三项独立操作，按用户操作选用；`pstAheadForm` 指向仍然有效的总句柄，`taskId` 来自成功启动的任务，每次调用均应检查返回值：

```c
//Pause a playing task.
if ( (ret = AheadForm_PausePlayback(pstAheadForm, taskId)) != RET_SUCCESS )
	return ret;

//Resume a paused task.
if ( (ret = AheadForm_ResumePlayback(pstAheadForm, taskId)) != RET_SUCCESS )
	return ret;

//Stop a playing or paused task.
if ( (ret = AheadForm_StopPlayback(pstAheadForm, taskId)) != RET_SUCCESS )
	return ret;
```

## 5. 常见问题

| 现象 | 检查方式 |
|---|---|
| 初始化或通信异常 | 核对 INI 路径、客户端网卡和机器人桥的对外 Domain；初始化成功本身不代表机器人已可控制 |
| CSV 启动失败 | 检查文件路径、完整 52 路表情与 7 路头眼列、有效数值及帧率，调用 `PrintAllErrLog()` 查看原因 |
| 本地进度正常但机器人不动 | 检查机器人运动许可、桥和仲裁服务，以及当前活动动作源；避免机器人端播放器或其他动作源与 SDK 同时争用动作流 |
| 状态为完成，机器人是否已执行完？ | 完成表示本地动作发送完毕；实际采用、限幅及物理执行结果以机器人反馈为准 |

暂停、停止及 SDK 销毁描述的是本地播放行为，不代表机器人急停或机械锁定。测试时由机器人端的运动许可和执行约束管理实际动作。
