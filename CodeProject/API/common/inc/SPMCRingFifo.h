#ifndef __SPMCRINGFIFO_H__
#define __SPMCRINGFIFO_H__

#include "PublicDef.h"

#define SPMC_RING_FIFO_MIN_ELEM_CNT 1000

typedef struct
{
	unsigned int sizeOf1Elem;
	unsigned int maxElemCnt;
	unsigned char *pElemAryBuf;
	unsigned long long nextWriteSeq;
} SPMC_RING_FIFO;

typedef struct
{
	SPMC_RING_FIFO *pstFifo;
	unsigned long long nextReadSeq;
	unsigned long long missedElemCnt;
} SPMC_RING_FIFO_READER;

//Require capacity >= 1000, one producer and one consumer per Reader; effective unread limit is floor(3 * capacity / 4), and copied slots must remain stable.
int SPMCRingFifo_Init(SPMC_RING_FIFO *pstFifo, void *pElemAryBuf, unsigned int sizeOf1Elem, unsigned int maxElemCnt);
int SPMCRingFifo_Deinit(SPMC_RING_FIFO *pstFifo);
int SPMCRingFifo_Reader_Init(SPMC_RING_FIFO_READER *pstReader, SPMC_RING_FIFO *pstFifo);

//The returned slot is borrowed until the producer starts another write.
const void* SPMCRingFifo_Writer_PushElemAndGetAddr(SPMC_RING_FIFO *pstFifo, const void *pElem);

//Read operations return the actual element count, zero when empty, or an error code.
int SPMCRingFifo_ReadLatest1Elem(SPMC_RING_FIFO *pstFifo, void *pElem);
//Apply automatic trimming before popping one element.
int SPMCRingFifo_Reader_PopElem(SPMC_RING_FIFO_READER *pstReader, void *pElem);
//After trimming, return the actual count up to floor(3 * capacity / 4); zero does not advance the Reader.
int SPMCRingFifo_Reader_PopElems(SPMC_RING_FIFO_READER *pstReader, void *pElemAryBuf, unsigned int rdElemCnt);
//Retention is capped at floor(3 * capacity / 4); return the explicitly dropped count.
int SPMCRingFifo_Reader_DropOldElems(SPMC_RING_FIFO_READER *pstReader, unsigned int retainedFreshElemCnt);
//Return the next Pop's readable count for the current snapshot without advancing the Reader.
unsigned int SPMCRingFifo_Reader_GetReadableElemCnt(SPMC_RING_FIFO_READER *pstReader);
//Count overwritten elements and completed drops without advancing the Reader.
unsigned long long SPMCRingFifo_Reader_GetMissedElemCnt(SPMC_RING_FIFO_READER *pstReader);

#endif
