#ifndef __AHEADFORM_STATUS_H__
#define __AHEADFORM_STATUS_H__

#include "AheadFormTopics.h"

typedef struct
{
	AHEADFORM_TOPICS *pstTopics;
	AHEADFORM_TOPIC_READER *pstSensorStateReader;
	AHEADFORM_TOPIC_READER *pstNeckStateReader;
	AHEADFORM_TOPIC_READER *pstFaultPoolReader;
} AHEADFORM_STATUS;

/*
	pstTopics must be successfully initialized first.
	Serialize lifecycle operations and release existing resources before reinitialization.
	Stop and unregister customer Readers before deinitializing Status.
*/
int Status_Init(AHEADFORM_STATUS *pstStatus, AHEADFORM_TOPICS *pstTopics);
int Status_Deinit(AHEADFORM_STATUS *pstStatus);

/*
	Return 1 when a sample is copied, 0 when the cache is empty, or an error code.
	A return value of 0 or an error leaves the output state object unchanged.
	Latest reads do not advance Reader cursors, clear eventfd or publish a request.
	Cached data does not prove that the robot is currently online.
*/
int Status_ReadLatestSensorState(AHEADFORM_STATUS *pstStatus, AHEADFORM_SENSOR_STATE *pstSensorState);

/*
	Output needs rdStateCnt elements; selected states are oldest first.
	Nonzero rdStateCnt must be < AHEADFORM_SENSOR_STATE_FIFO_ELEM_CNT / 2; otherwise ERR_ARGV_3 before copying.
	Return < 0: ERR_*; >= 0: copied count. Only positive returns make output usable; 0 leaves it unchanged.
	Client threads only, outside Reader callbacks/matching functions; may wait for topicMutex, not new samples.
	No cursor, notification or missed-count changes, requests or file I/O.
*/
int Status_ReadSensorRecentState(AHEADFORM_STATUS *pstStatus, AHEADFORM_SENSOR_STATE *pstSensorStateAry, unsigned int rdStateCnt);

/*
	The Sensor Latest/Recent contracts above also apply to Neck and FaultPool.
	Nonzero Recent counts must be < the corresponding FIFO capacity / 2 (at most 499).
*/
int Status_ReadLatestNeckState(AHEADFORM_STATUS *pstStatus, AHEADFORM_NECK_STATE *pstNeckState);
int Status_ReadNeckRecentState(AHEADFORM_STATUS *pstStatus, AHEADFORM_NECK_STATE *pstNeckStateAry, unsigned int rdStateCnt);

/*
	Latest copies the complete list from the last successfully received sample.
	Return 0 means no cached snapshot; return 1 with faultCnt == 0 means a valid empty list.
	ReceiveTime_us is local monotonic time; cached snapshots are not automatically expired or cleared.
*/
int Status_ReadLatestFaultPool(AHEADFORM_STATUS *pstStatus, AHEADFORM_FAULT_POOL *pstFaultPool);

/*
	rdPoolCnt counts snapshots, not fault records.
	Output needs rdPoolCnt pool objects; selected snapshots are oldest first.
*/
int Status_ReadRecentFaultPools(AHEADFORM_STATUS *pstStatus, AHEADFORM_FAULT_POOL *pstFaultPoolAry, unsigned int rdPoolCnt);

/*
	Only SENSOR_STATE, NECK_STATE and FAULT_POOL Topic IDs are accepted.
	Customer Readers have independent cursors; unregister waits for in-flight callbacks.
	Internal Status Readers must not be unregistered by customers.
*/
AHEADFORM_TOPIC_READER* Status_RegisterReader(AHEADFORM_STATUS *pstStatus, ENUM_AHEADFORM_TOPIC_ID topicId, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Status_UnregisterReader(AHEADFORM_STATUS *pstStatus, AHEADFORM_TOPIC_READER *pstReader);

#endif
