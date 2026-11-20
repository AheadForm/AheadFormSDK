#ifndef __AHEADFORM_VIDEO_H__
#define __AHEADFORM_VIDEO_H__

#include "ParseInitConfig.h"
#include "SPMCRingFifo.h"

#include <gst/gst.h>
#include <pthread.h>

typedef struct
{
	unsigned long long frameSeq;
	unsigned long long pts_ns;
	unsigned long long ReceiveTime_us;
	unsigned int dataLen;
	unsigned int width;
	unsigned int height;
	unsigned char isKeyFrame;
	unsigned char isDiscontinuous;
} AHEADFORM_VIDEO_FRAME_INFO;

typedef struct VIDEO_READER_NODE
{
	SPMC_RING_FIFO_READER stFifoReader;
	unsigned char *pElemBuf;
	int rxDataUpdateEventfd;
	unsigned long long lastMissedFrameCnt;
	unsigned char isWaitingKeyFrame;
	struct VIDEO_READER_NODE *pNext;
} AHEADFORM_VIDEO_READER;

typedef struct
{
	AHEADFORM_MEDIA_CFG stCfg;
	char strNetworkInterface[AHEADFORM_NETWORK_INTERFACE_LEN_MAX];
	GstElement *pPipeline;
	SPMC_RING_FIFO stFifo;
	unsigned char *pElemAryBuf;
	unsigned char *pElemBuf;
	AHEADFORM_VIDEO_READER *pstReaders;
	pthread_mutex_t readerListMutex;
	unsigned char isDiscontinuous;
	unsigned long long nextFrameSeq;
	int lastError;
} AHEADFORM_VIDEO_CTRL;

/*
	pstCfg must be successfully parsed; Init copies the required configuration.
	Video does not require Topics. Lifecycle and registration calls are serialized by the caller.
	Runtime APIs require successful Init; on Init failure, the application reports the error and exits without Deinit.
	After teardown begins, Deinit errors are terminal; do not retry Deinit or use the partially released handle.
	Do not copy active handles or modify SDK runtime members.
*/
int Video_Init(AHEADFORM_VIDEO_CTRL *pstVideoCtrl, const AHEADFORM_INIT_CONFIG *pstCfg);
int Video_Deinit(AHEADFORM_VIDEO_CTRL *pstVideoCtrl);

/*
	Start allocates the FIFO on first use; Stop retains its data, Readers and sequence.
	Start success does not mean a first frame has arrived.
	Stop must run outside GStreamer callbacks; failed Stop preserves pipeline ownership.
*/
int Video_StartCapture(AHEADFORM_VIDEO_CTRL *pstVideoCtrl);
int Video_StopCapture(AHEADFORM_VIDEO_CTRL *pstVideoCtrl);
int Video_GetCaptureError(const AHEADFORM_VIDEO_CTRL *pstVideoCtrl);

/*
	Register after the first successful Start; each Reader starts at the current writer position.
	The SDK owns rxDataUpdateEventfd. Clear it before draining; its count is only an update hint.
	One sequential consumer per Reader; stop consuming and waiting before unregister.
	Deinit returns ERRCODE_RESOURCE_BUSY before teardown while customer Readers remain.
*/
AHEADFORM_VIDEO_READER* Video_RegisterReader(AHEADFORM_VIDEO_CTRL *pstVideoCtrl);
int Video_UnregisterReader(AHEADFORM_VIDEO_CTRL *pstVideoCtrl, AHEADFORM_VIDEO_READER *pstReader);

/*
	Output is one Annex-B H.264 AU, not a decoded image.
	dataBufSize must be at least configured videoFrameSizeMax; capacity failure does not consume.
	Return 1, 0 when no deliverable frame remains, or an error; only 1 makes outputs valid.
	New Readers and Readers with known loss skip until an IDR containing SPS/PPS.
	pts_ns is stream time or ULLONG_MAX; ReceiveTime_us is local monotonic arrival time.
*/
int Video_Reader_PopFrame(AHEADFORM_VIDEO_READER *pstReader, unsigned char *pRetDataBuf, size_t dataBufSize, AHEADFORM_VIDEO_FRAME_INFO *pstRetInfo);
int Video_Reader_DropOldFrames(AHEADFORM_VIDEO_READER *pstReader, unsigned int retainedFreshFrameCnt);
unsigned int Video_Reader_GetReadableFrameCnt(AHEADFORM_VIDEO_READER *pstReader);
unsigned long long Video_Reader_GetMissedFrameCnt(AHEADFORM_VIDEO_READER *pstReader);

#endif
