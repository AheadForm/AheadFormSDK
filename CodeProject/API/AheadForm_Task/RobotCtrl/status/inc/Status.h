#ifndef __AHEADFORM_STATUS_H__
#define __AHEADFORM_STATUS_H__

#include "AheadFormTopics.h"

#define AHEADFORM_STATUS_TOPIC_CNT (AHEADFORM_TOPIC_ID_FAULT_EVENT - AHEADFORM_TOPIC_ID_HARDWARE_STATE + 1)

typedef struct
{
	AHEADFORM_TOPICS *pstTopics;
	AHEADFORM_TOPIC_READER *szpstReader[AHEADFORM_STATUS_TOPIC_CNT];
} AHEADFORM_STATUS;

typedef struct
{
	unsigned int exportedSampleCnt;
	unsigned int sourceSeqRollbackCnt;
	unsigned long long missedElemCnt;
	int lastReceiveResult;
} AHEADFORM_STATUS_EXPORT_RESULT;

//Topics owns all sample storage. Init registers independent long-lived Readers; Deinit does not destroy Topics.
int Status_Init(AHEADFORM_STATUS *pstStatus, AHEADFORM_TOPICS *pstTopics);
int Status_Deinit(AHEADFORM_STATUS *pstStatus);
/*
	Status topics continuously push data; no query is sent. Latest reads do not advance Reader progress.
	Register a separate Reader for independent sequential consumption, IO multiplexing or per-sample callbacks.
	Reader consumption/statistics are lock-free under the existing SPMC stable-copy contract.
*/
AHEADFORM_TOPIC_READER* Status_RegisterHardwareStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Status_ReadLatestHardwareState(AHEADFORM_STATUS *pstStatus, AHEADFORM_HARDWARE_STATE_SAMPLE *pstSample);
AHEADFORM_TOPIC_READER* Status_RegisterFaceStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Status_ReadLatestFaceState(AHEADFORM_STATUS *pstStatus, AHEADFORM_FACE_STATE_SAMPLE *pstSample);
AHEADFORM_TOPIC_READER* Status_RegisterNeckStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Status_ReadLatestNeckState(AHEADFORM_STATUS *pstStatus, AHEADFORM_NECK_STATE_SAMPLE *pstSample);
AHEADFORM_TOPIC_READER* Status_RegisterMediaStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Status_ReadLatestMediaState(AHEADFORM_STATUS *pstStatus, AHEADFORM_MEDIA_STATE_SAMPLE *pstSample);
AHEADFORM_TOPIC_READER* Status_RegisterPowerStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Status_ReadLatestPowerState(AHEADFORM_STATUS *pstStatus, AHEADFORM_POWER_STATE_SAMPLE *pstSample);
AHEADFORM_TOPIC_READER* Status_RegisterSensorStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Status_ReadLatestSensorState(AHEADFORM_STATUS *pstStatus, AHEADFORM_SENSOR_STATE_SAMPLE *pstSample);
AHEADFORM_TOPIC_READER* Status_RegisterFaultSetReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
AHEADFORM_TOPIC_READER* Status_RegisterFaultEventReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Status_ReadLatestFaultEvent(AHEADFORM_STATUS *pstStatus, AHEADFORM_FAULT_EVENT_SAMPLE *pstSample);
int Status_UnregisterReader(AHEADFORM_STATUS *pstStatus, AHEADFORM_TOPIC_READER *pstReader);
//The required byte size is TopicReader_GetElemSize(); failure never writes past caller capacity.
int Status_ReadLatestFaultSet(AHEADFORM_STATUS *pstStatus, AHEADFORM_FAULT_SET_SAMPLE *pstSample, unsigned int sampleBufSize);
int StatusReader_ReadLatestFaultSet(AHEADFORM_TOPIC_READER *pstReader, AHEADFORM_FAULT_SET_SAMPLE *pstSample, unsigned int sampleBufSize);
int StatusReader_PopFaultSet(AHEADFORM_TOPIC_READER *pstReader, AHEADFORM_FAULT_SET_SAMPLE *pstSample, unsigned int sampleBufSize);
/*
	Export a finite snapshot of at most sampleCntMax unread samples as JSON Lines, using and advancing this Reader.
	The result reports FIFO loss, source sequence rollback and the latest receive/conversion result, including on failure.
	Consumed samples are not retained for later replay. Do not consume this Reader concurrently while exporting.
*/
int Status_ExportUnread(AHEADFORM_TOPIC_READER *pstReader, const char *filePath, unsigned int sampleCntMax, AHEADFORM_STATUS_EXPORT_RESULT *pstResult);

#endif
