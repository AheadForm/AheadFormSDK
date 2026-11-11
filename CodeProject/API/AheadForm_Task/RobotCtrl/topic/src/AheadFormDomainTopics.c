#include "AheadFormTopics.h"
#include "AheadFormDdsMsg.h"
#include "OperatingModeMessages.h"

#include <time.h>

int TopicWriter_WriteDomainSample(AHEADFORM_TOPIC_WRITER *pstWriter, const void *pElem)
{
	struct timespec stTime;
	dds_return_t ddsRet;
	unsigned int i;

	if (pstWriter == NULL || pstWriter->pDdsWriter == NULL || *pstWriter->pDdsWriter <= 0)
		return ERR_ARGV_1;

	if (pElem == NULL)
		return ERR_ARGV_2;

	if (clock_gettime(CLOCK_REALTIME, &stTime) < 0)
	{
		printf("%s() call clock_gettime(CLOCK_REALTIME) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_CLOCK;
	}

	switch (pstWriter->topicId)
	{
		case AHEADFORM_TOPIC_ID_MOTION:
		{
			const AHEADFORM_MOTION_COMMAND *pstSample = (const AHEADFORM_MOTION_COMMAND*)pElem;
			arbitration_msgs_msg_AheadMotion stDds = {0};

			if (pstSample->sourceName[0] < 'a' || pstSample->sourceName[0] > 'z' || strnlen(pstSample->sourceName, sizeof(pstSample->sourceName)) == sizeof(pstSample->sourceName))
				return ERR_ARGV_2;

			if (strspn(pstSample->sourceName, "abcdefghijklmnopqrstuvwxyz0123456789_") != strlen(pstSample->sourceName))
				return ERR_ARGV_2;

			strcpy(stDds.source_name, pstSample->sourceName);
			stDds.timestamp = stTime.tv_sec + stTime.tv_nsec / 1000000000.0;
			stDds.version = 2;
			stDds.neck.pitch = pstSample->stFrame.stNeck.pitch_rad;
			stDds.neck.yaw = pstSample->stFrame.stNeck.yaw_rad;
			stDds.neck.roll = pstSample->stFrame.stNeck.roll_rad;
			stDds.neck.head_x = pstSample->stFrame.stNeck.headX;
			stDds.neck.head_y = pstSample->stFrame.stNeck.headY;
			stDds.neck.head_z = pstSample->stFrame.stNeck.headZ;
			stDds.eyes.left_eye_pitch = pstSample->stFrame.stEyes.leftPitch_rad;
			stDds.eyes.left_eye_yaw = pstSample->stFrame.stEyes.leftYaw_rad;
			stDds.eyes.right_eye_pitch = pstSample->stFrame.stEyes.rightPitch_rad;
			stDds.eyes.right_eye_yaw = pstSample->stFrame.stEyes.rightYaw_rad;
			memcpy(stDds.blendshapes, pstSample->stFrame.blendshapes, sizeof(stDds.blendshapes));
			for (i = 0; i < AHEADFORM_BLENDSHAPE_CNT; i++)
			{
				if (!isfinite(pstSample->stFrame.blendshapes[i]) || pstSample->stFrame.blendshapes[i] < 0 || pstSample->stFrame.blendshapes[i] > 1)
					return ERR_ARGV_2;
			}

			if (!isfinite(stDds.neck.pitch) || !isfinite(stDds.neck.yaw) || !isfinite(stDds.neck.roll) || !isfinite(stDds.neck.head_x) || !isfinite(stDds.neck.head_y) || !isfinite(stDds.neck.head_z) ||
				!isfinite(stDds.eyes.left_eye_pitch) || !isfinite(stDds.eyes.left_eye_yaw) || !isfinite(stDds.eyes.right_eye_pitch) || !isfinite(stDds.eyes.right_eye_yaw))
				return ERR_ARGV_2;
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_NECK:
		{
			const AHEADFORM_NECK_COMMAND *pstSample = (const AHEADFORM_NECK_COMMAND*)pElem;
			arbitration_msgs_msg_NeckControl stDds = {0};

			if (pstSample->sourceName[0] < 'a' || pstSample->sourceName[0] > 'z' || strnlen(pstSample->sourceName, sizeof(pstSample->sourceName)) == sizeof(pstSample->sourceName))
				return ERR_ARGV_2;

			if (strspn(pstSample->sourceName, "abcdefghijklmnopqrstuvwxyz0123456789_") != strlen(pstSample->sourceName))
				return ERR_ARGV_2;

			strcpy(stDds.source_name, pstSample->sourceName);
			stDds.timestamp = stTime.tv_sec + stTime.tv_nsec / 1000000000.0;
			stDds.command_seq = pstSample->commandSeq;
			stDds.pitch = pstSample->stNeck.pitch_rad;
			stDds.yaw = pstSample->stNeck.yaw_rad;
			stDds.roll = pstSample->stNeck.roll_rad;

			if (!isfinite(stDds.pitch) || !isfinite(stDds.yaw) || !isfinite(stDds.roll))
				return ERR_ARGV_2;
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_FACE:
		{
			const AHEADFORM_FACE_COMMAND *pstSample = (const AHEADFORM_FACE_COMMAND*)pElem;
			arbitration_msgs_msg_FaceControl stDds = {0};

			if (pstSample->sourceName[0] < 'a' || pstSample->sourceName[0] > 'z' || strnlen(pstSample->sourceName, sizeof(pstSample->sourceName)) == sizeof(pstSample->sourceName))
				return ERR_ARGV_2;

			if (strspn(pstSample->sourceName, "abcdefghijklmnopqrstuvwxyz0123456789_") != strlen(pstSample->sourceName))
				return ERR_ARGV_2;

			strcpy(stDds.source_name, pstSample->sourceName);
			stDds.timestamp = stTime.tv_sec + stTime.tv_nsec / 1000000000.0;
			stDds.command_seq = pstSample->commandSeq;
			stDds.eyes.left_eye_pitch = pstSample->stEyes.leftPitch_rad;
			stDds.eyes.left_eye_yaw = pstSample->stEyes.leftYaw_rad;
			stDds.eyes.right_eye_pitch = pstSample->stEyes.rightPitch_rad;
			stDds.eyes.right_eye_yaw = pstSample->stEyes.rightYaw_rad;
			memcpy(stDds.blendshapes, pstSample->blendshapes, sizeof(stDds.blendshapes));
			for (i = 0; i < AHEADFORM_BLENDSHAPE_CNT; i++)
			{
				if (!isfinite(pstSample->blendshapes[i]) || pstSample->blendshapes[i] < 0 || pstSample->blendshapes[i] > 1)
					return ERR_ARGV_2;
			}

			if (!isfinite(stDds.eyes.left_eye_pitch) || !isfinite(stDds.eyes.left_eye_yaw) || !isfinite(stDds.eyes.right_eye_pitch) || !isfinite(stDds.eyes.right_eye_yaw))
				return ERR_ARGV_2;
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_SERVO:
		{
			const AHEADFORM_SERVO_COMMAND *pstSample = (const AHEADFORM_SERVO_COMMAND*)pElem;
			arbitration_msgs_msg_FaceServoDirectTarget stDds = {0};

			stDds.command_seq = pstSample->commandSeq;
			stDds.stamp.sec = stTime.tv_sec;
			stDds.stamp.nanosec = stTime.tv_nsec;
			memcpy(stDds.target_logic_deg, pstSample->targetLogic_deg, sizeof(stDds.target_logic_deg));
			for (i = 0; i < AHEADFORM_FACE_SERVO_CNT; i++)
			{
				if (!isfinite(stDds.target_logic_deg[i]))
					return ERR_ARGV_2;
			}
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_PRESET_MOTION_ASK:
		{
			const AHEADFORM_PRESET_MOTION_ASK *pstSample = (const AHEADFORM_PRESET_MOTION_ASK*)pElem;
			arbitration_msgs_msg_PresetMotionAsk stDds = {0};

			if (pstSample->askId == 0 || pstSample->sourceName[0] == 0)
				return ERR_ARGV_2;

			stDds.stamp.sec = (*pstSample).stSourceTime.sec;
			stDds.stamp.nanosec = (*pstSample).stSourceTime.nanosec;
			stDds.ask_id = (*pstSample).askId;
			if (strnlen((*pstSample).sourceName, sizeof((*pstSample).sourceName)) == sizeof((*pstSample).sourceName))
				return ERR_ARGV_2;

			strcpy(stDds.source_name, (*pstSample).sourceName);
			stDds.action = (*pstSample).action;
			stDds.stamp.sec = stTime.tv_sec;
			stDds.stamp.nanosec = stTime.tv_nsec;
			if (stDds.action != AHEADFORM_PRESET_ACTION_SLEEP && stDds.action != AHEADFORM_PRESET_ACTION_CENTER)
				return ERR_ARGV_2;
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_AUDIO_DEVICE_ASK:
		{
			const AHEADFORM_AUDIO_DEVICE_ASK *pstSample = (const AHEADFORM_AUDIO_DEVICE_ASK*)pElem;
			arbitration_msgs_msg_MediaAudioDeviceAsk stDds = {0};

			if (pstSample->askId == 0 || pstSample->sourceName[0] == 0)
				return ERR_ARGV_2;

			stDds.stamp.sec = (*pstSample).stSourceTime.sec;
			stDds.stamp.nanosec = (*pstSample).stSourceTime.nanosec;
			stDds.ask_id = (*pstSample).askId;
			if (strnlen((*pstSample).sourceName, sizeof((*pstSample).sourceName)) == sizeof((*pstSample).sourceName))
				return ERR_ARGV_2;

			stDds.source_name = (char*)(*pstSample).sourceName;
			stDds.operation = (*pstSample).operation;
			stDds.device_type = (*pstSample).deviceType;
			stDds.has_mic_device = (*pstSample).hasMicDevice;
			if (strnlen((*pstSample).micDeviceId, sizeof((*pstSample).micDeviceId)) == sizeof((*pstSample).micDeviceId))
				return ERR_ARGV_2;

			stDds.mic_device_id = (char*)(*pstSample).micDeviceId;
			stDds.has_spk_device = (*pstSample).hasSpkDevice;
			if (strnlen((*pstSample).spkDeviceId, sizeof((*pstSample).spkDeviceId)) == sizeof((*pstSample).spkDeviceId))
				return ERR_ARGV_2;

			stDds.spk_device_id = (char*)(*pstSample).spkDeviceId;
			stDds.stamp.sec = stTime.tv_sec;
			stDds.stamp.nanosec = stTime.tv_nsec;
			if (stDds.operation < AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE || stDds.operation > AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT || stDds.device_type > AHEADFORM_AUDIO_DEVICE_SPK)
				return ERR_ARGV_2;

			if (stDds.operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE && ((!stDds.has_mic_device && !stDds.has_spk_device) || (stDds.has_mic_device && stDds.mic_device_id[0] == 0) || (stDds.has_spk_device && stDds.spk_device_id[0] == 0)))
				return ERR_ARGV_2;
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_MEDIA_CHANNEL_ASK:
		{
			const AHEADFORM_MEDIA_CHANNEL_ASK *pstSample = (const AHEADFORM_MEDIA_CHANNEL_ASK*)pElem;
			arbitration_msgs_msg_MediaChannelControlAsk stDds = {0};

			if (pstSample->askId == 0 || pstSample->sourceName[0] == 0)
				return ERR_ARGV_2;

			stDds.stamp.sec = (*pstSample).stSourceTime.sec;
			stDds.stamp.nanosec = (*pstSample).stSourceTime.nanosec;
			stDds.ask_id = (*pstSample).askId;
			if (strnlen((*pstSample).sourceName, sizeof((*pstSample).sourceName)) == sizeof((*pstSample).sourceName))
				return ERR_ARGV_2;

			stDds.source_name = (char*)(*pstSample).sourceName;
			stDds.video_ctrl = (*pstSample).videoCtrl;
			stDds.mic_ctrl = (*pstSample).micCtrl;
			stDds.playback_ctrl = (*pstSample).playbackCtrl;
			stDds.vad_ctrl = (*pstSample).vadCtrl;
			stDds.stamp.sec = stTime.tv_sec;
			stDds.stamp.nanosec = stTime.tv_nsec;
			if (stDds.video_ctrl > 2 || stDds.mic_ctrl > 2 || stDds.playback_ctrl > 2 || stDds.vad_ctrl > 2)
				return ERR_ARGV_2;
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_HARDWARE_CONTROL_ASK:
		{
			const AHEADFORM_HARDWARE_CONTROL_ASK *pstSample = (const AHEADFORM_HARDWARE_CONTROL_ASK*)pElem;
			arbitration_msgs_msg_HardwareControlAsk stDds = {0};

			if (pstSample->askId == 0 || pstSample->sourceName[0] == 0)
				return ERR_ARGV_2;

			stDds.stamp.sec = (*pstSample).stSourceTime.sec;
			stDds.stamp.nanosec = (*pstSample).stSourceTime.nanosec;
			stDds.ask_id = (*pstSample).askId;
			if (strnlen((*pstSample).sourceName, sizeof((*pstSample).sourceName)) == sizeof((*pstSample).sourceName))
				return ERR_ARGV_2;

			stDds.source_name = (char*)(*pstSample).sourceName;
			stDds.action = (*pstSample).action;
			stDds.device.value = (*pstSample).device;
			stDds.stamp.sec = stTime.tv_sec;
			stDds.stamp.nanosec = stTime.tv_nsec;
			if ((stDds.action != AHEADFORM_HARDWARE_ACTION_PRINT_ERROR_LOG && stDds.action != AHEADFORM_HARDWARE_ACTION_EXIT_DAEMON) || stDds.device.value < AHEADFORM_HARDWARE_DEVICE_FACE || stDds.device.value > AHEADFORM_HARDWARE_DEVICE_SENSOR)
				return ERR_ARGV_2;
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_SAFETY_CONTROL_ASK:
		{
			const AHEADFORM_SAFETY_CONTROL_ASK *pstSample = (const AHEADFORM_SAFETY_CONTROL_ASK*)pElem;
			arbitration_msgs_msg_SafetyControlAsk stDds = {0};

			if (pstSample->askId == 0 || pstSample->sourceName[0] == 0)
				return ERR_ARGV_2;

			stDds.stamp.sec = (*pstSample).stSourceTime.sec;
			stDds.stamp.nanosec = (*pstSample).stSourceTime.nanosec;
			stDds.ask_id = (*pstSample).askId;
			if (strnlen((*pstSample).sourceName, sizeof((*pstSample).sourceName)) == sizeof((*pstSample).sourceName))
				return ERR_ARGV_2;

			stDds.source_name = (char*)(*pstSample).sourceName;
			stDds.action = (*pstSample).action;
			stDds.face_motion_permitted = (*pstSample).faceMotionPermitted;
			stDds.neck_motion_permitted = (*pstSample).neckMotionPermitted;
			stDds.stamp.sec = stTime.tv_sec;
			stDds.stamp.nanosec = stTime.tv_nsec;
			if (stDds.action < AHEADFORM_SAFETY_ACTION_MOTION_PERMISSION || stDds.action > AHEADFORM_SAFETY_ACTION_ESTOP)
				return ERR_ARGV_2;
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_OPERATING_MODE_ASK:
		{
			const AHEADFORM_OPERATING_MODE_ASK *pstSample = (const AHEADFORM_OPERATING_MODE_ASK*)pElem;
			cloud_agent_msgs_msg_OperatingModeControl stDds = {0};

			if (pstSample->requestId == 0 || (pstSample->mode != AHEADFORM_OPERATING_MODE_DEV && pstSample->mode != AHEADFORM_OPERATING_MODE_INTERACTIVE))
				return ERR_ARGV_2;

			stDds.request_id = pstSample->requestId;
			stDds.mode = (cloud_agent_msgs_msg_OperatingMode)pstSample->mode;
			if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDds)) < 0 )
			{
				printf("%s() call dds_write(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, ddsRet);
				return ERR_DDS_PUBLISH_FAILED;
			}

			return RET_SUCCESS;
		}

		default:
			return ERR_TOPIC_DIRECTION_INVALID;
	}
}
