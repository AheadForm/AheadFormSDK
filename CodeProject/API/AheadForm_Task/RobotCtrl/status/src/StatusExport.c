#include "Status.h"

#include <math.h>

static void WriteJsonString(FILE *pFile, const char *strValue)
{
	const unsigned char *p = (const unsigned char*)strValue;

	fputc('"', pFile);
	for (; *p != 0; p++)
	{
		if (*p == '"' || *p == '\\')
			fprintf(pFile, "\\%c", *p);
		else if (*p < 32)
			fprintf(pFile, "\\u%04x", (unsigned int)*p);
		else
			fputc(*p, pFile);
	}
	fputc('"', pFile);
}

static void WriteJsonNumber(FILE *pFile, double value)
{
	if (isfinite(value))
		fprintf(pFile, "%.17g", value);
	else
		fputs("null", pFile);
}

static void WriteJsonNeckPose(FILE *pFile, const AHEADFORM_NECK_POSE *pstSample)
{
	fputc('{', pFile);
	fputs("\"pitch_rad\":", pFile);
	WriteJsonNumber(pFile, pstSample->pitch_rad);
	fputs(",\"yaw_rad\":", pFile);
	WriteJsonNumber(pFile, pstSample->yaw_rad);
	fputs(",\"roll_rad\":", pFile);
	WriteJsonNumber(pFile, pstSample->roll_rad);
	fputs(",\"headX\":", pFile);
	WriteJsonNumber(pFile, pstSample->headX);
	fputs(",\"headY\":", pFile);
	WriteJsonNumber(pFile, pstSample->headY);
	fputs(",\"headZ\":", pFile);
	WriteJsonNumber(pFile, pstSample->headZ);
	fputc('}', pFile);
}

static void WriteJsonSourceTime(FILE *pFile, const AHEADFORM_SOURCE_TIME *pstSample)
{
	fputc('{', pFile);
	fputs("\"sec\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->sec);
	fputs(",\"nanosec\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->nanosec);
	fputc('}', pFile);
}

static void WriteJsonHardwareStateSample(FILE *pFile, const AHEADFORM_HARDWARE_STATE_SAMPLE *pstSample)
{
	fputc('{', pFile);
	fputs("\"ReceiveTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->ReceiveTime_us);
	fputs(",\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"arbiterAlive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->arbiterAlive);
	fputs(",\"arbiterErrorCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->arbiterErrorCode);
	fputs(",\"arbiterErrorReason\":", pFile);
	WriteJsonString(pFile, pstSample->arbiterErrorReason);
	fputs(",\"allOnline\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->allOnline);
	fputs(",\"allReady\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->allReady);
	fputs(",\"anyFaultActive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->anyFaultActive);
	fputs(",\"anyEstopLatched\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->anyEstopLatched);
	fputs(",\"activeMotionSourceName\":", pFile);
	WriteJsonString(pFile, pstSample->activeMotionSourceName);
	fputs(",\"faceMotionPermitted\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faceMotionPermitted);
	fputs(",\"neckMotionPermitted\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->neckMotionPermitted);
	fputs(",\"safeStopMotionBlockActive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->safeStopMotionBlockActive);
	fputc('}', pFile);
}

static void WriteJsonDeviceConnectionState(FILE *pFile, const AHEADFORM_DEVICE_CONNECTION_STATE *pstSample)
{
	fputc('{', pFile);
	fputs("\"handshaked\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->handshaked);
	fputs(",\"hasControl\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->hasControl);
	fputs(",\"statusPushGranted\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->statusPushGranted);
	fputs(",\"estopPermitGranted\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->estopPermitGranted);
	fputs(",\"daemonEndpoint\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->daemonEndpoint);
	fputs(",\"daemonState\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->daemonState);
	fputs(",\"helloErrCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->helloErrCode);
	fputs(",\"faultActive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faultActive);
	fputs(",\"daemonName\":", pFile);
	WriteJsonString(pFile, pstSample->daemonName);
	fputc('}', pFile);
}

static void WriteJsonDeviceOperationalStatus(FILE *pFile, const AHEADFORM_DEVICE_OPERATIONAL_STATUS *pstSample)
{
	fputc('{', pFile);
	fputs("\"online\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->online);
	fputs(",\"ready\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ready);
	fputs(",\"state\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->state);
	fputs(",\"errCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->errCode);
	fputs(",\"faultActive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faultActive);
	fputs(",\"ownerOnline\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ownerOnline);
	fputs(",\"canOk\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->canOk);
	fputs(",\"reached\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->reached);
	fputs(",\"lastAcceptedTargetId\":", pFile);
	fprintf(pFile, "%llu", pstSample->lastAcceptedTargetId);
	fputs(",\"targetHasExec_ms\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->targetHasExec_ms);
	fputs(",\"trajectoryPlanRun_ms\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->trajectoryPlanRun_ms);
	fputc('}', pFile);
}

static void WriteJsonDeviceState(FILE *pFile, const AHEADFORM_DEVICE_STATE *pstSample)
{
	fputc('{', pFile);
	fputs("\"stConnection\":", pFile);
	WriteJsonDeviceConnectionState(pFile, &pstSample->stConnection);
	fputs(",\"stOperationalStatus\":", pFile);
	WriteJsonDeviceOperationalStatus(pFile, &pstSample->stOperationalStatus);
	fputc('}', pFile);
}

static void WriteJsonFaceServoStatus(FILE *pFile, const AHEADFORM_FACE_SERVO_STATUS *pstSample)
{
	fputc('{', pFile);
	fputs("\"servoId\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->servoId);
	fputs(",\"targetLogic_deg\":", pFile);
	WriteJsonNumber(pFile, pstSample->targetLogic_deg);
	fputs(",\"motionRefLogic_deg\":", pFile);
	WriteJsonNumber(pFile, pstSample->motionRefLogic_deg);
	fputs(",\"feedbackTargetLogic_deg\":", pFile);
	WriteJsonNumber(pFile, pstSample->feedbackTargetLogic_deg);
	fputs(",\"currentLogic_deg\":", pFile);
	WriteJsonNumber(pFile, pstSample->currentLogic_deg);
	fputs(",\"logicSpeed_deg_s\":", pFile);
	WriteJsonNumber(pFile, pstSample->logicSpeed_deg_s);
	fputs(",\"temperatureC\":", pFile);
	WriteJsonNumber(pFile, pstSample->temperatureC);
	fputs(",\"voltageV\":", pFile);
	WriteJsonNumber(pFile, pstSample->voltageV);
	fputs(",\"currentA\":", pFile);
	WriteJsonNumber(pFile, pstSample->currentA);
	fputs(",\"feedbackAge_ms\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->feedbackAge_ms);
	fputs(",\"hlsPresentStatus\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->hlsPresentStatus);
	fputs(",\"faultCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faultCode);
	fputc('}', pFile);
}

static void WriteJsonFaceStatus(FILE *pFile, const AHEADFORM_FACE_STATUS *pstSample)
{
	unsigned int i;

	fputc('{', pFile);
	fputs("\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"sampleSeq\":", pFile);
	fprintf(pFile, "%llu", pstSample->sampleSeq);
	fputs(",\"msgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->msgId);
	fputs(",\"refMsgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->refMsgId);
	fputs(",\"daemonId\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->daemonId);
	fputs(",\"state\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->state);
	fputs(",\"errCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->errCode);
	fputs(",\"ready\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ready);
	fputs(",\"online\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->online);
	fputs(",\"faultActive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faultActive);
	fputs(",\"ownerOnline\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ownerOnline);
	fputs(",\"lastAcceptedTargetId\":", pFile);
	fprintf(pFile, "%llu", pstSample->lastAcceptedTargetId);
	fputs(",\"statusSeq\":", pFile);
	fprintf(pFile, "%llu", pstSample->statusSeq);
	fputs(",\"statusBuildTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->statusBuildTime_us);
	fputs(",\"faceCtrlFaultCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faceCtrlFaultCode);
	fputs(",\"allServoReady\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->allServoReady);
	fputs(",\"servoCount\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->servoCount);
	fputs(",\"szstServo\":", pFile);
	fputc('[', pFile);
	for (i = 0; i < AHEADFORM_FACE_SERVO_CNT; i++)
	{
		if (i != 0)
			fputc(',', pFile);

		WriteJsonFaceServoStatus(pFile, &pstSample->szstServo[i]);
	}
	fputc(']', pFile);
	fputc('}', pFile);
}

static void WriteJsonFaceStateSample(FILE *pFile, const AHEADFORM_FACE_STATE_SAMPLE *pstSample)
{
	fputc('{', pFile);
	fputs("\"ReceiveTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->ReceiveTime_us);
	fputs(",\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"stDeviceState\":", pFile);
	WriteJsonDeviceState(pFile, &pstSample->stDeviceState);
	fputs(",\"hasLatestSample\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->hasLatestSample);
	fputs(",\"stLatestSample\":", pFile);
	WriteJsonFaceStatus(pFile, &pstSample->stLatestSample);
	fputc('}', pFile);
}

static void WriteJsonNeckMotorSample(FILE *pFile, const AHEADFORM_NECK_MOTOR_SAMPLE *pstSample)
{
	fputc('{', pFile);
	fputs("\"motorId\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->motorId);
	fputs(",\"feedbackTimeout\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->feedbackTimeout);
	fputs(",\"lkMotorState\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->lkMotorState);
	fputs(",\"errorState\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->errorState);
	fputs(",\"sampleAge_ms\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->sampleAge_ms);
	fputs(",\"target_deg\":", pFile);
	WriteJsonNumber(pFile, pstSample->target_deg);
	fputs(",\"lastMotionRef_deg\":", pFile);
	WriteJsonNumber(pFile, pstSample->lastMotionRef_deg);
	fputs(",\"currentRaw_deg\":", pFile);
	WriteJsonNumber(pFile, pstSample->currentRaw_deg);
	fputs(",\"currentFiltered_deg\":", pFile);
	WriteJsonNumber(pFile, pstSample->currentFiltered_deg);
	fputs(",\"speed_deg_s\":", pFile);
	WriteJsonNumber(pFile, pstSample->speed_deg_s);
	fputs(",\"temperatureC\":", pFile);
	WriteJsonNumber(pFile, pstSample->temperatureC);
	fputs(",\"voltageV\":", pFile);
	WriteJsonNumber(pFile, pstSample->voltageV);
	fputs(",\"currentA\":", pFile);
	WriteJsonNumber(pFile, pstSample->currentA);
	fputc('}', pFile);
}

static void WriteJsonNeckStatus(FILE *pFile, const AHEADFORM_NECK_STATUS *pstSample)
{
	unsigned int i;

	fputc('{', pFile);
	fputs("\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"sampleSeq\":", pFile);
	fprintf(pFile, "%llu", pstSample->sampleSeq);
	fputs(",\"msgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->msgId);
	fputs(",\"refMsgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->refMsgId);
	fputs(",\"daemonId\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->daemonId);
	fputs(",\"state\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->state);
	fputs(",\"errCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->errCode);
	fputs(",\"ready\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ready);
	fputs(",\"online\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->online);
	fputs(",\"faultActive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faultActive);
	fputs(",\"ownerOnline\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ownerOnline);
	fputs(",\"canOk\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->canOk);
	fputs(",\"reached\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->reached);
	fputs(",\"lastAcceptedTargetId\":", pFile);
	fprintf(pFile, "%llu", pstSample->lastAcceptedTargetId);
	fputs(",\"statusSeq\":", pFile);
	fprintf(pFile, "%llu", pstSample->statusSeq);
	fputs(",\"statusBuildTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->statusBuildTime_us);
	fputs(",\"targetHasExec_ms\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->targetHasExec_ms);
	fputs(",\"trajectoryPlanRun_ms\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->trajectoryPlanRun_ms);
	fputs(",\"stPose\":", pFile);
	WriteJsonNeckPose(pFile, &pstSample->stPose);
	fputs(",\"szstMotor\":", pFile);
	fputc('[', pFile);
	for (i = 0; i < AHEADFORM_NECK_MOTOR_CNT; i++)
	{
		if (i != 0)
			fputc(',', pFile);

		WriteJsonNeckMotorSample(pFile, &pstSample->szstMotor[i]);
	}
	fputc(']', pFile);
	fputc('}', pFile);
}

static void WriteJsonNeckStateSample(FILE *pFile, const AHEADFORM_NECK_STATE_SAMPLE *pstSample)
{
	fputc('{', pFile);
	fputs("\"ReceiveTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->ReceiveTime_us);
	fputs(",\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"stDeviceState\":", pFile);
	WriteJsonDeviceState(pFile, &pstSample->stDeviceState);
	fputs(",\"hasLatestSample\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->hasLatestSample);
	fputs(",\"stLatestSample\":", pFile);
	WriteJsonNeckStatus(pFile, &pstSample->stLatestSample);
	fputc('}', pFile);
}

static void WriteJsonMediaChannelStatus(FILE *pFile, const AHEADFORM_MEDIA_CHANNEL_STATUS *pstSample)
{
	fputc('{', pFile);
	fputs("\"channelState\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->channelState);
	fputs(",\"deviceAvailable\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->deviceAvailable);
	fputs(",\"activeReaderMask\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->activeReaderMask);
	fputs(",\"readableFrameCount\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->readableFrameCount);
	fputs(",\"lastSequence\":", pFile);
	fprintf(pFile, "%llu", pstSample->lastSequence);
	fputs(",\"droppedFrameCount\":", pFile);
	fprintf(pFile, "%llu", pstSample->droppedFrameCount);
	fputc('}', pFile);
}

static void WriteJsonMediaStatus(FILE *pFile, const AHEADFORM_MEDIA_STATUS *pstSample)
{
	fputc('{', pFile);
	fputs("\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"sampleSeq\":", pFile);
	fprintf(pFile, "%llu", pstSample->sampleSeq);
	fputs(",\"msgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->msgId);
	fputs(",\"refMsgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->refMsgId);
	fputs(",\"daemonId\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->daemonId);
	fputs(",\"state\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->state);
	fputs(",\"errCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->errCode);
	fputs(",\"ready\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ready);
	fputs(",\"online\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->online);
	fputs(",\"faultActive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faultActive);
	fputs(",\"ownerOnline\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ownerOnline);
	fputs(",\"mediaTransClientConnected\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->mediaTransClientConnected);
	fputs(",\"pulseAudioAecReady\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->pulseAudioAecReady);
	fputs(",\"video\":", pFile);
	WriteJsonMediaChannelStatus(pFile, &pstSample->video);
	fputs(",\"mic\":", pFile);
	WriteJsonMediaChannelStatus(pFile, &pstSample->mic);
	fputs(",\"playback\":", pFile);
	WriteJsonMediaChannelStatus(pFile, &pstSample->playback);
	fputs(",\"vad\":", pFile);
	WriteJsonMediaChannelStatus(pFile, &pstSample->vad);
	fputc('}', pFile);
}

static void WriteJsonMediaStateSample(FILE *pFile, const AHEADFORM_MEDIA_STATE_SAMPLE *pstSample)
{
	fputc('{', pFile);
	fputs("\"ReceiveTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->ReceiveTime_us);
	fputs(",\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"stDeviceState\":", pFile);
	WriteJsonDeviceState(pFile, &pstSample->stDeviceState);
	fputs(",\"hasLatestSample\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->hasLatestSample);
	fputs(",\"stLatestSample\":", pFile);
	WriteJsonMediaStatus(pFile, &pstSample->stLatestSample);
	fputc('}', pFile);
}

static void WriteJsonPowerStatus(FILE *pFile, const AHEADFORM_POWER_STATUS *pstSample)
{
	unsigned int i;

	fputc('{', pFile);
	fputs("\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"sampleSeq\":", pFile);
	fprintf(pFile, "%llu", pstSample->sampleSeq);
	fputs(",\"msgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->msgId);
	fputs(",\"refMsgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->refMsgId);
	fputs(",\"statusBuildTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->statusBuildTime_us);
	fputs(",\"daemonId\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->daemonId);
	fputs(",\"state\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->state);
	fputs(",\"errCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->errCode);
	fputs(",\"ready\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ready);
	fputs(",\"online\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->online);
	fputs(",\"faultActive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faultActive);
	fputs(",\"ownerOnline\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ownerOnline);
	fputs(",\"requestUpperPowerOn\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->requestUpperPowerOn);
	fputs(",\"requestLowerPowerOn\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->requestLowerPowerOn);
	fputs(",\"upperPowerOn\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->upperPowerOn);
	fputs(",\"lowerPowerOn\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->lowerPowerOn);
	fputs(",\"requestMainPowerOff\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->requestMainPowerOff);
	fputs(",\"camWorkStateLedOn\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->camWorkStateLedOn);
	fputs(",\"powerSwitchReleased\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->powerSwitchReleased);
	fputs(",\"fanRotationSpeedPercent\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->fanRotationSpeedPercent);
	fputs(",\"tempLimitC\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->tempLimitC);
	fputs(",\"upperTemp_C\":", pFile);
	fputc('[', pFile);
	for (i = 0; i < AHEADFORM_POWER_TEMP_CNT; i++)
	{
		if (i != 0)
			fputc(',', pFile);

		fprintf(pFile, "%lld", (long long)pstSample->upperTemp_C[i]);
	}
	fputc(']', pFile);
	fputs(",\"upperBoardFeedbackAge_ms\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->upperBoardFeedbackAge_ms);
	fputs(",\"lowerBoardFeedbackAge_ms\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->lowerBoardFeedbackAge_ms);
	fputc('}', pFile);
}

static void WriteJsonPowerStateSample(FILE *pFile, const AHEADFORM_POWER_STATE_SAMPLE *pstSample)
{
	fputc('{', pFile);
	fputs("\"ReceiveTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->ReceiveTime_us);
	fputs(",\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"stDeviceState\":", pFile);
	WriteJsonDeviceState(pFile, &pstSample->stDeviceState);
	fputs(",\"hasLatestSample\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->hasLatestSample);
	fputs(",\"stLatestSample\":", pFile);
	WriteJsonPowerStatus(pFile, &pstSample->stLatestSample);
	fputc('}', pFile);
}

static void WriteJsonSensorStatus(FILE *pFile, const AHEADFORM_SENSOR_STATUS *pstSample)
{
	fputc('{', pFile);
	fputs("\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"sampleSeq\":", pFile);
	fprintf(pFile, "%llu", pstSample->sampleSeq);
	fputs(",\"msgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->msgId);
	fputs(",\"refMsgId\":", pFile);
	fprintf(pFile, "%llu", pstSample->refMsgId);
	fputs(",\"daemonId\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->daemonId);
	fputs(",\"state\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->state);
	fputs(",\"errCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->errCode);
	fputs(",\"ready\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ready);
	fputs(",\"online\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->online);
	fputs(",\"faultActive\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faultActive);
	fputs(",\"ownerOnline\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->ownerOnline);
	fputs(",\"statusSeq\":", pFile);
	fprintf(pFile, "%llu", pstSample->statusSeq);
	fputs(",\"statusBuildTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->statusBuildTime_us);
	fputs(",\"imuRpyFeedbackAge_ms\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->imuRpyFeedbackAge_ms);
	fputs(",\"roll_rad\":", pFile);
	WriteJsonNumber(pFile, pstSample->roll_rad);
	fputs(",\"pitch_rad\":", pFile);
	WriteJsonNumber(pFile, pstSample->pitch_rad);
	fputs(",\"yaw_rad\":", pFile);
	WriteJsonNumber(pFile, pstSample->yaw_rad);
	fputc('}', pFile);
}

static void WriteJsonSensorStateSample(FILE *pFile, const AHEADFORM_SENSOR_STATE_SAMPLE *pstSample)
{
	fputc('{', pFile);
	fputs("\"ReceiveTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->ReceiveTime_us);
	fputs(",\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"stDeviceState\":", pFile);
	WriteJsonDeviceState(pFile, &pstSample->stDeviceState);
	fputs(",\"hasLatestSample\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->hasLatestSample);
	fputs(",\"stLatestSample\":", pFile);
	WriteJsonSensorStatus(pFile, &pstSample->stLatestSample);
	fputc('}', pFile);
}

static void WriteJsonFaultRecord(FILE *pFile, const AHEADFORM_FAULT_RECORD *pstSample)
{
	fputc('{', pFile);
	fputs("\"device\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->device);
	fputs(",\"source\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->source);
	fputs(",\"objectId\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->objectId);
	fputs(",\"errCode\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->errCode);
	fputc('}', pFile);
}

static void WriteJsonFaultEventSample(FILE *pFile, const AHEADFORM_FAULT_EVENT_SAMPLE *pstSample)
{
	fputc('{', pFile);
	fputs("\"ReceiveTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->ReceiveTime_us);
	fputs(",\"stSourceTime\":", pFile);
	WriteJsonSourceTime(pFile, &pstSample->stSourceTime);
	fputs(",\"sourceSeq\":", pFile);
	fprintf(pFile, "%llu", pstSample->sourceSeq);
	fputs(",\"eventType\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->eventType);
	fputs(",\"stFault\":", pFile);
	WriteJsonFaultRecord(pFile, &pstSample->stFault);
	fputc('}', pFile);
}

static void WriteJsonFaultSetSample(FILE *pFile, const AHEADFORM_FAULT_SET_SAMPLE *pstSample)
{
	unsigned int i;

	fputc('{', pFile);
	fputs("\"ReceiveTime_us\":", pFile);
	fprintf(pFile, "%llu", pstSample->ReceiveTime_us);
	fputs(",\"sourceTime_sec\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->sourceTime_sec);
	fputs(",\"sourceTime_nanosec\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->sourceTime_nanosec);
	fputs(",\"faultRecordCnt\":", pFile);
	fprintf(pFile, "%lld", (long long)pstSample->faultRecordCnt);
	fputs(",\"szstFaultRecord\":", pFile);
	fputc('[', pFile);
	for (i = 0; i < pstSample->faultRecordCnt; i++)
	{
		if (i != 0)
			fputc(',', pFile);

		WriteJsonFaultRecord(pFile, &pstSample->szstFaultRecord[i]);
	}
	fputc(']', pFile);
	fputc('}', pFile);
}

int Status_ExportUnread(AHEADFORM_TOPIC_READER *pstReader, const char *filePath, unsigned int sampleCntMax, AHEADFORM_STATUS_EXPORT_RESULT *pstResult)
{
	FILE *pFile = NULL;
	void *pSample = NULL;
	unsigned int i, sampleCnt, elemSize;
	unsigned long long sourceSeq = 0, previousSeq = 0;
	unsigned char hasSourceSeq = 0, hasPreviousSeq = 0;
	int ret = RET_SUCCESS;

	if (pstReader == NULL || pstReader->topicId < AHEADFORM_TOPIC_ID_HARDWARE_STATE || pstReader->topicId > AHEADFORM_TOPIC_ID_FAULT_EVENT)
		return ERR_ARGV_1;

	if (filePath == NULL || filePath[0] == 0)
		return ERR_ARGV_2;

	if (sampleCntMax == 0)
		return ERR_ARGV_3;

	if (pstResult == NULL)
		return ERR_ARGV_4;

	memset(pstResult, 0, sizeof(*pstResult));
	pstResult->lastReceiveResult = TopicReader_GetLastReceiveResult(pstReader);
	elemSize = TopicReader_GetElemSize(pstReader);
	if (elemSize == 0)
		return ERR_SDK_NOT_READY;

	if ( (pSample = malloc(elemSize)) == NULL )
		return ERR_MEMORY_ALLOC;

	if ( (pFile = fopen(filePath, "wb")) == NULL )
	{
		printf("%s() call fopen(%s) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
		free(pSample);
		return ERR_FILE_OP_FAILED;
	}
	sampleCnt = TopicReader_GetReadableElemCnt(pstReader);
	if (sampleCnt > sampleCntMax)
		sampleCnt = sampleCntMax;

	for (i = 0; i < sampleCnt; i++)
	{
		ret = TopicReader_PopElem(pstReader, pSample);
		if (ret != 1)
		{
			if (ret == 0)
				ret = RET_SUCCESS;

			break;
		}
		hasSourceSeq = 0;
		fprintf(pFile, "{\"topicId\":%u,\"sample\":", (unsigned int)pstReader->topicId);
		switch (pstReader->topicId)
		{
			case AHEADFORM_TOPIC_ID_HARDWARE_STATE:
				WriteJsonHardwareStateSample(pFile, (const AHEADFORM_HARDWARE_STATE_SAMPLE*)pSample);
				break;

			case AHEADFORM_TOPIC_ID_FACE_STATE:
				WriteJsonFaceStateSample(pFile, (const AHEADFORM_FACE_STATE_SAMPLE*)pSample);
				hasSourceSeq = ((const AHEADFORM_FACE_STATE_SAMPLE*)pSample)->hasLatestSample;
				sourceSeq = ((const AHEADFORM_FACE_STATE_SAMPLE*)pSample)->hasLatestSample ? ((const AHEADFORM_FACE_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq : 0;
				break;

			case AHEADFORM_TOPIC_ID_NECK_STATE:
				WriteJsonNeckStateSample(pFile, (const AHEADFORM_NECK_STATE_SAMPLE*)pSample);
				hasSourceSeq = ((const AHEADFORM_NECK_STATE_SAMPLE*)pSample)->hasLatestSample;
				sourceSeq = ((const AHEADFORM_NECK_STATE_SAMPLE*)pSample)->hasLatestSample ? ((const AHEADFORM_NECK_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq : 0;
				break;

			case AHEADFORM_TOPIC_ID_MEDIA_STATE:
				WriteJsonMediaStateSample(pFile, (const AHEADFORM_MEDIA_STATE_SAMPLE*)pSample);
				hasSourceSeq = ((const AHEADFORM_MEDIA_STATE_SAMPLE*)pSample)->hasLatestSample;
				sourceSeq = ((const AHEADFORM_MEDIA_STATE_SAMPLE*)pSample)->hasLatestSample ? ((const AHEADFORM_MEDIA_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq : 0;
				break;

			case AHEADFORM_TOPIC_ID_POWER_STATE:
				WriteJsonPowerStateSample(pFile, (const AHEADFORM_POWER_STATE_SAMPLE*)pSample);
				hasSourceSeq = ((const AHEADFORM_POWER_STATE_SAMPLE*)pSample)->hasLatestSample;
				sourceSeq = ((const AHEADFORM_POWER_STATE_SAMPLE*)pSample)->hasLatestSample ? ((const AHEADFORM_POWER_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq : 0;
				break;

			case AHEADFORM_TOPIC_ID_SENSOR_STATE:
				WriteJsonSensorStateSample(pFile, (const AHEADFORM_SENSOR_STATE_SAMPLE*)pSample);
				hasSourceSeq = ((const AHEADFORM_SENSOR_STATE_SAMPLE*)pSample)->hasLatestSample;
				sourceSeq = ((const AHEADFORM_SENSOR_STATE_SAMPLE*)pSample)->hasLatestSample ? ((const AHEADFORM_SENSOR_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq : 0;
				break;

			case AHEADFORM_TOPIC_ID_FAULT_SET:
				WriteJsonFaultSetSample(pFile, (const AHEADFORM_FAULT_SET_SAMPLE*)pSample);
				break;

			case AHEADFORM_TOPIC_ID_FAULT_EVENT:
				WriteJsonFaultEventSample(pFile, (const AHEADFORM_FAULT_EVENT_SAMPLE*)pSample);
				hasSourceSeq = 1;
				sourceSeq = ((const AHEADFORM_FAULT_EVENT_SAMPLE*)pSample)->sourceSeq;
				break;

			default:
				ret = ERR_ARGV_1;
				break;
		}

		if (hasSourceSeq && hasPreviousSeq && sourceSeq < previousSeq)
			pstResult->sourceSeqRollbackCnt++;

		if (hasSourceSeq)
		{
			previousSeq = sourceSeq;
			hasPreviousSeq = 1;
		}

		fprintf(pFile, ",\"missedElemCnt\":%llu,\"sourceSeqRollbackCnt\":%u}\r\n", TopicReader_GetMissedElemCnt(pstReader), pstResult->sourceSeqRollbackCnt);
		if (ferror(pFile))
		{
			printf("%s() call fprintf(%s) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
			ret = ERR_FILE_OP_FAILED;
			break;
		}
		pstResult->exportedSampleCnt++;
		ret = RET_SUCCESS;
	}
	pstResult->missedElemCnt = TopicReader_GetMissedElemCnt(pstReader);
	pstResult->lastReceiveResult = TopicReader_GetLastReceiveResult(pstReader);
	if (fclose(pFile) != RET_SUCCESS)
	{
		printf("%s() call fclose(%s) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
		ret = ERR_FILE_OP_FAILED;
	}
	free(pSample);
	return ret;
}
