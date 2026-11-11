#include "SPMCRingFifo.h"

static unsigned int SPMCRingFifo_GetMaxUsableElemCnt(unsigned int maxElemCnt);
static unsigned long long SPMCRingFifo_Reader_SkipOverwrittenElems(SPMC_RING_FIFO_READER *pstReader, unsigned long long nextWriteSeq);


int SPMCRingFifo_Init(SPMC_RING_FIFO *pstFifo, void *pElemAryBuf, unsigned int sizeOf1Elem, unsigned int maxElemCnt)
{
	if (pstFifo == NULL)
		return ERR_ARGV_1;

	if (pElemAryBuf == NULL)
		return ERR_ARGV_2;

	if (sizeOf1Elem == 0)
		return ERR_ARGV_3;

	if (maxElemCnt < SPMC_RING_FIFO_MIN_ELEM_CNT || maxElemCnt > INT_MAX)
		return ERR_ARGV_4;

	pstFifo->sizeOf1Elem = sizeOf1Elem;
	pstFifo->maxElemCnt = maxElemCnt;
	pstFifo->pElemAryBuf = pElemAryBuf;
	__atomic_store_n(&pstFifo->nextWriteSeq, 0, __ATOMIC_SEQ_CST);
	return RET_SUCCESS;
}

int SPMCRingFifo_Deinit(SPMC_RING_FIFO *pstFifo)
{
	if (pstFifo == NULL)
		return ERR_ARGV_1;

	memset(pstFifo, 0, sizeof(*pstFifo));
	return RET_SUCCESS;
}

int SPMCRingFifo_Reader_Init(SPMC_RING_FIFO_READER *pstReader, SPMC_RING_FIFO *pstFifo)
{
	if (pstReader == NULL)
		return ERR_ARGV_1;

	if (pstFifo == NULL || pstFifo->pElemAryBuf == NULL)
		return ERR_ARGV_2;

	pstReader->pstFifo = pstFifo;
	__atomic_store_n(&pstReader->nextReadSeq, __atomic_load_n(&pstFifo->nextWriteSeq, __ATOMIC_SEQ_CST), __ATOMIC_SEQ_CST);
	__atomic_store_n(&pstReader->missedElemCnt, 0, __ATOMIC_SEQ_CST);
	return RET_SUCCESS;
}

const void* SPMCRingFifo_Writer_PushElemAndGetAddr(SPMC_RING_FIFO *pstFifo, const void *pElem)
{
	unsigned char *pElemAddr = NULL;
	unsigned long long nextWriteSeq;

	if (pstFifo == NULL || pstFifo->pElemAryBuf == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (pElem == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}
	nextWriteSeq = __atomic_load_n(&pstFifo->nextWriteSeq, __ATOMIC_SEQ_CST);
	pElemAddr = pstFifo->pElemAryBuf + (nextWriteSeq % pstFifo->maxElemCnt) * pstFifo->sizeOf1Elem;
	memcpy(pElemAddr, pElem, pstFifo->sizeOf1Elem);
	__atomic_store_n(&pstFifo->nextWriteSeq, nextWriteSeq + 1, __ATOMIC_SEQ_CST);
	return pElemAddr;
}

int SPMCRingFifo_ReadLatest1Elem(SPMC_RING_FIFO *pstFifo, void *pElem)
{
	unsigned long long nextWriteSeq;

	if (pstFifo == NULL || pstFifo->pElemAryBuf == NULL)
		return ERR_ARGV_1;

	if (pElem == NULL)
		return ERR_ARGV_2;

	nextWriteSeq = __atomic_load_n(&pstFifo->nextWriteSeq, __ATOMIC_SEQ_CST);
	if (nextWriteSeq == 0)
		return 0;

	memcpy(pElem, pstFifo->pElemAryBuf + ((nextWriteSeq - 1) % pstFifo->maxElemCnt) * pstFifo->sizeOf1Elem, pstFifo->sizeOf1Elem);
	return 1;
}

int SPMCRingFifo_Reader_PopElem(SPMC_RING_FIFO_READER *pstReader, void *pElem)
{
	return SPMCRingFifo_Reader_PopElems(pstReader, pElem, 1);
}

int SPMCRingFifo_Reader_PopElems(SPMC_RING_FIFO_READER *pstReader, void *pElemAryBuf, unsigned int rdElemCnt)
{
	unsigned long long nextWriteSeq, nextReadSeq;
	unsigned int firstElemCnt, slotId, maxUsableElemCnt, skippedElemCnt;

	if (pstReader == NULL || pstReader->pstFifo == NULL || pstReader->pstFifo->pElemAryBuf == NULL)
		return ERR_ARGV_1;

	if (pElemAryBuf == NULL)
		return ERR_ARGV_2;

	if (rdElemCnt == 0)
		return 0;

	nextWriteSeq = __atomic_load_n(&pstReader->pstFifo->nextWriteSeq, __ATOMIC_SEQ_CST);
	nextReadSeq = SPMCRingFifo_Reader_SkipOverwrittenElems(pstReader, nextWriteSeq);
	maxUsableElemCnt = SPMCRingFifo_GetMaxUsableElemCnt(pstReader->pstFifo->maxElemCnt);
	if (nextWriteSeq - nextReadSeq > maxUsableElemCnt)
	{
		skippedElemCnt = pstReader->pstFifo->maxElemCnt - maxUsableElemCnt;
		__atomic_add_fetch(&pstReader->missedElemCnt, skippedElemCnt, __ATOMIC_SEQ_CST);
		nextReadSeq += skippedElemCnt;
		__atomic_store_n(&pstReader->nextReadSeq, nextReadSeq, __ATOMIC_SEQ_CST);
	}

	if (nextReadSeq == nextWriteSeq)
		return 0;

	if (rdElemCnt > nextWriteSeq - nextReadSeq)
		rdElemCnt = (unsigned int)(nextWriteSeq - nextReadSeq);

	slotId = nextReadSeq % pstReader->pstFifo->maxElemCnt;
	firstElemCnt = pstReader->pstFifo->maxElemCnt - slotId;
	if (firstElemCnt > rdElemCnt)
		firstElemCnt = rdElemCnt;

	memcpy(pElemAryBuf, pstReader->pstFifo->pElemAryBuf + slotId * pstReader->pstFifo->sizeOf1Elem, firstElemCnt * pstReader->pstFifo->sizeOf1Elem);
	if (firstElemCnt < rdElemCnt)
		memcpy((unsigned char*)pElemAryBuf + firstElemCnt * pstReader->pstFifo->sizeOf1Elem, pstReader->pstFifo->pElemAryBuf, (rdElemCnt - firstElemCnt) * pstReader->pstFifo->sizeOf1Elem);

	__atomic_store_n(&pstReader->nextReadSeq, nextReadSeq + rdElemCnt, __ATOMIC_SEQ_CST);
	return rdElemCnt;
}

int SPMCRingFifo_Reader_DropOldElems(SPMC_RING_FIFO_READER *pstReader, unsigned int retainedFreshElemCnt)
{
	unsigned long long nextWriteSeq, nextReadSeq;
	unsigned int droppedElemCnt, maxUsableElemCnt;

	if (pstReader == NULL || pstReader->pstFifo == NULL || pstReader->pstFifo->pElemAryBuf == NULL)
		return ERR_ARGV_1;

	maxUsableElemCnt = SPMCRingFifo_GetMaxUsableElemCnt(pstReader->pstFifo->maxElemCnt);
	if (retainedFreshElemCnt > maxUsableElemCnt)
		retainedFreshElemCnt = maxUsableElemCnt;

	nextWriteSeq = __atomic_load_n(&pstReader->pstFifo->nextWriteSeq, __ATOMIC_SEQ_CST);
	nextReadSeq = SPMCRingFifo_Reader_SkipOverwrittenElems(pstReader, nextWriteSeq);
	if (nextWriteSeq - nextReadSeq <= retainedFreshElemCnt)
		return 0;

	droppedElemCnt = (unsigned int)(nextWriteSeq - nextReadSeq - retainedFreshElemCnt);
	__atomic_add_fetch(&pstReader->missedElemCnt, droppedElemCnt, __ATOMIC_SEQ_CST);
	__atomic_store_n(&pstReader->nextReadSeq, nextReadSeq + droppedElemCnt, __ATOMIC_SEQ_CST);
	return droppedElemCnt;
}

unsigned int SPMCRingFifo_Reader_GetReadableElemCnt(SPMC_RING_FIFO_READER *pstReader)
{
	unsigned long long nextWriteSeq, nextReadSeq;
	unsigned int readableElemCnt, maxUsableElemCnt;

	if (pstReader == NULL || pstReader->pstFifo == NULL || pstReader->pstFifo->pElemAryBuf == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return 0;
	}
	nextWriteSeq = __atomic_load_n(&pstReader->pstFifo->nextWriteSeq, __ATOMIC_SEQ_CST);
	nextReadSeq = __atomic_load_n(&pstReader->nextReadSeq, __ATOMIC_SEQ_CST);
	readableElemCnt = nextWriteSeq - nextReadSeq > pstReader->pstFifo->maxElemCnt ? pstReader->pstFifo->maxElemCnt : (unsigned int)(nextWriteSeq - nextReadSeq);
	maxUsableElemCnt = SPMCRingFifo_GetMaxUsableElemCnt(pstReader->pstFifo->maxElemCnt);
	if (readableElemCnt > maxUsableElemCnt)
		readableElemCnt -= pstReader->pstFifo->maxElemCnt - maxUsableElemCnt;

	return readableElemCnt;
}

unsigned long long SPMCRingFifo_Reader_GetMissedElemCnt(SPMC_RING_FIFO_READER *pstReader)
{
	unsigned long long nextWriteSeq, nextReadSeq, firstReadableSeq, missedElemCnt;

	if (pstReader == NULL || pstReader->pstFifo == NULL || pstReader->pstFifo->pElemAryBuf == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return 0;
	}
	nextWriteSeq = __atomic_load_n(&pstReader->pstFifo->nextWriteSeq, __ATOMIC_SEQ_CST);
	nextReadSeq = __atomic_load_n(&pstReader->nextReadSeq, __ATOMIC_SEQ_CST);
	missedElemCnt = __atomic_load_n(&pstReader->missedElemCnt, __ATOMIC_SEQ_CST);
	firstReadableSeq = nextWriteSeq > pstReader->pstFifo->maxElemCnt ? nextWriteSeq - pstReader->pstFifo->maxElemCnt : 0;
	if (nextReadSeq < firstReadableSeq)
		missedElemCnt += firstReadableSeq - nextReadSeq;

	return missedElemCnt;
}

static unsigned long long SPMCRingFifo_Reader_SkipOverwrittenElems(SPMC_RING_FIFO_READER *pstReader, unsigned long long nextWriteSeq)
{
	unsigned long long nextReadSeq, firstReadableSeq;

	nextReadSeq = __atomic_load_n(&pstReader->nextReadSeq, __ATOMIC_SEQ_CST);
	firstReadableSeq = nextWriteSeq > pstReader->pstFifo->maxElemCnt ? nextWriteSeq - pstReader->pstFifo->maxElemCnt : 0;
	if (nextReadSeq < firstReadableSeq)
	{
		__atomic_add_fetch(&pstReader->missedElemCnt, firstReadableSeq - nextReadSeq, __ATOMIC_SEQ_CST);
		nextReadSeq = firstReadableSeq;
		__atomic_store_n(&pstReader->nextReadSeq, nextReadSeq, __ATOMIC_SEQ_CST);
	}

	return nextReadSeq;
}

static unsigned int SPMCRingFifo_GetMaxUsableElemCnt(unsigned int maxElemCnt)
{
	return (unsigned int)((unsigned long long)maxElemCnt * 3 / 4);
}
