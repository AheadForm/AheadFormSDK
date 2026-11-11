#include "SPSC_CoverableRing.h"

static unsigned long long SPSC_CoverableRing_Reader_SkipOverwrittenElemsAndGetNextReadSeq(SPSC_COVERABLE_RING *pstRing, unsigned long long nextWriteSeq);
//Caller has already filtered out odd write-busy versions; an even version mismatch means this read sequence was overwritten.
static void SPSC_CoverableRing_Reader_SkipOverwrittenElemsAndUpdateMissedElemCnt(SPSC_COVERABLE_RING *pstRing, unsigned long long overwrittenReadSeq);

int SPSC_CoverableRing_Init(SPSC_COVERABLE_RING *pstRing, void *pElemAryBuf, unsigned int elemAryBufSize, unsigned int sizeOf1Elem)
{
    unsigned int i, maxElemCnt;

    if (pstRing == NULL)
        return ERR_ARGV_1;

    if (pElemAryBuf == NULL)
        return ERR_ARGV_2;

    if (elemAryBufSize == 0)
        return ERR_ARGV_3;

    if (sizeOf1Elem == 0)
        return ERR_ARGV_4;

    maxElemCnt = elemAryBufSize / sizeOf1Elem;
    if (maxElemCnt < 2)
        return ERR_ARGV_3;

    if (maxElemCnt > SPSC_COVERABLE_RING_ELEM_CNT_MAX)
        return ERR_ARGV_3;

    pstRing->sizeOf1Elem = sizeOf1Elem;
    pstRing->maxElemCnt = maxElemCnt;
    pstRing->pElemAryBuf = (unsigned char*)pElemAryBuf;
    __atomic_store_n(&pstRing->nextWriteSeq, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&pstRing->nextReadSeq, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&pstRing->missedElemCnt, 0, __ATOMIC_SEQ_CST);
    for (i = 0; i < SPSC_COVERABLE_RING_ELEM_CNT_MAX; i++)
        __atomic_store_n(&pstRing->szVersionOfWrite[i], 0, __ATOMIC_SEQ_CST);

    return RET_SUCCESS;
}

int SPSC_CoverableRing_Deinit(SPSC_COVERABLE_RING *pstRing)
{
    unsigned int i;

    if (pstRing == NULL)
        return ERR_ARGV_1;

    pstRing->sizeOf1Elem = 0;
    pstRing->maxElemCnt = 0;
    pstRing->pElemAryBuf = NULL;
    __atomic_store_n(&pstRing->nextWriteSeq, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&pstRing->nextReadSeq, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&pstRing->missedElemCnt, 0, __ATOMIC_SEQ_CST);
    for (i = 0; i < SPSC_COVERABLE_RING_ELEM_CNT_MAX; i++)
        __atomic_store_n(&pstRing->szVersionOfWrite[i], 0, __ATOMIC_SEQ_CST);

    return RET_SUCCESS;
}

int SPSC_CoverableRing_Clear(SPSC_COVERABLE_RING *pstRing)
{
    unsigned int i;

    if (pstRing == NULL)
        return ERR_ARGV_1;

    __atomic_store_n(&pstRing->nextWriteSeq, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&pstRing->nextReadSeq, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&pstRing->missedElemCnt, 0, __ATOMIC_SEQ_CST);
    for (i = 0; i < SPSC_COVERABLE_RING_ELEM_CNT_MAX; i++)
        __atomic_store_n(&pstRing->szVersionOfWrite[i], 0, __ATOMIC_SEQ_CST);

    return RET_SUCCESS;
}

int SPSC_CoverableRing_Writer_PushElem(SPSC_COVERABLE_RING *pstRing, const void *pElem)
{
    unsigned long long nextWriteSeq;
    unsigned long long versionWhenWriting, versionAfterWrite;
    unsigned int elemSlotId;

    if (pstRing == NULL || pstRing->pElemAryBuf == NULL)
        return ERR_ARGV_1;

    if (pElem == NULL)
        return ERR_ARGV_2;

    nextWriteSeq = __atomic_load_n(&pstRing->nextWriteSeq, __ATOMIC_SEQ_CST);
    elemSlotId = (unsigned int)(nextWriteSeq % pstRing->maxElemCnt);
    versionWhenWriting = (nextWriteSeq * 2) + 1;
    versionAfterWrite = (nextWriteSeq * 2) + 2;
    __atomic_store_n(&pstRing->szVersionOfWrite[elemSlotId], versionWhenWriting, __ATOMIC_SEQ_CST);
    memcpy(pstRing->pElemAryBuf + (elemSlotId * pstRing->sizeOf1Elem), pElem, pstRing->sizeOf1Elem);
    __atomic_store_n(&pstRing->szVersionOfWrite[elemSlotId], versionAfterWrite, __ATOMIC_SEQ_CST);
    __atomic_store_n(&pstRing->nextWriteSeq, nextWriteSeq + 1, __ATOMIC_SEQ_CST);
    return RET_SUCCESS;
}

const void* SPSC_CoverableRing_Writer_PushElemAndGetAddr(SPSC_COVERABLE_RING *pstRing, const void *pElem)
{
    unsigned long long nextWriteSeq;
    unsigned long long versionWhenWriting, versionAfterWrite;
    unsigned int elemSlotId;

    if (pstRing == NULL || pstRing->pElemAryBuf == NULL)
    {
        PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
        return NULL;
    }

    if (pElem == NULL)
    {
        PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
        return NULL;
    }
    nextWriteSeq = __atomic_load_n(&pstRing->nextWriteSeq, __ATOMIC_SEQ_CST);
    elemSlotId = (unsigned int)(nextWriteSeq % pstRing->maxElemCnt);
    versionWhenWriting = (nextWriteSeq * 2) + 1;
    versionAfterWrite = (nextWriteSeq * 2) + 2;
    __atomic_store_n(&pstRing->szVersionOfWrite[elemSlotId], versionWhenWriting, __ATOMIC_SEQ_CST);
    memcpy(pstRing->pElemAryBuf + (elemSlotId * pstRing->sizeOf1Elem), pElem, pstRing->sizeOf1Elem);
    __atomic_store_n(&pstRing->szVersionOfWrite[elemSlotId], versionAfterWrite, __ATOMIC_SEQ_CST);
    __atomic_store_n(&pstRing->nextWriteSeq, nextWriteSeq + 1, __ATOMIC_SEQ_CST);
    return pstRing->pElemAryBuf + (elemSlotId * pstRing->sizeOf1Elem);
}

ENUM_SPSC_COVERABLE_RING_STATE SPSC_CoverableRing_Reader_PopElem(SPSC_COVERABLE_RING *pstRing, void *pElem)
{
    unsigned long long nextWriteSeq, nextReadSeq;
    unsigned long long expectedVersionOfWrite, versionBeforeCopy, versionAfterCopy;
    unsigned int slotId;

    if (pstRing == NULL || pstRing->pElemAryBuf == NULL)
    {
        PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
        return SPSC_COVERABLE_RING_OP_FAIL;
    }

    if (pElem == NULL)
    {
        PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
        return SPSC_COVERABLE_RING_OP_FAIL;
    }
    nextWriteSeq = __atomic_load_n(&pstRing->nextWriteSeq, __ATOMIC_SEQ_CST);
    nextReadSeq = SPSC_CoverableRing_Reader_SkipOverwrittenElemsAndGetNextReadSeq(pstRing, nextWriteSeq);
    if (nextReadSeq >= nextWriteSeq)
        return SPSC_COVERABLE_RING_EMPTY;

    slotId = (unsigned int)(nextReadSeq % pstRing->maxElemCnt);
    expectedVersionOfWrite = (nextReadSeq * 2) + 2;
    versionBeforeCopy = __atomic_load_n(&pstRing->szVersionOfWrite[slotId], __ATOMIC_SEQ_CST);
    if ((versionBeforeCopy & 1) != 0)
        return SPSC_COVERABLE_RING_ELEM_WR_BUSY;

    if (versionBeforeCopy != expectedVersionOfWrite)
    {
        SPSC_CoverableRing_Reader_SkipOverwrittenElemsAndUpdateMissedElemCnt(pstRing, nextReadSeq);
        return SPSC_COVERABLE_RING_ELEM_MISSED;
    }
    memcpy(pElem, pstRing->pElemAryBuf + (slotId * pstRing->sizeOf1Elem), pstRing->sizeOf1Elem);
    versionAfterCopy = __atomic_load_n(&pstRing->szVersionOfWrite[slotId], __ATOMIC_SEQ_CST);
    if ((versionAfterCopy & 1) != 0)
        return SPSC_COVERABLE_RING_ELEM_WR_BUSY;

    if (versionAfterCopy != expectedVersionOfWrite)
    {
        SPSC_CoverableRing_Reader_SkipOverwrittenElemsAndUpdateMissedElemCnt(pstRing, nextReadSeq);
        return SPSC_COVERABLE_RING_ELEM_MISSED;
    }
    __atomic_store_n(&pstRing->nextReadSeq, nextReadSeq + 1, __ATOMIC_SEQ_CST);
    return SPSC_COVERABLE_RING_OP_SUCCEED;
}

int SPSC_CoverableRing_Reader_ReadLatest1Elem(SPSC_COVERABLE_RING *pstRing, void *pElem)
{
    unsigned long long nextWriteSeq, latestReadSeq;
    unsigned long long expectedVersionOfWrite, versionBeforeCopy, versionAfterCopy;
    unsigned int slotId;

    if (pstRing == NULL || pstRing->pElemAryBuf == NULL)
        return ERR_ARGV_1;

    if (pElem == NULL)
        return ERR_ARGV_2;

    nextWriteSeq = __atomic_load_n(&pstRing->nextWriteSeq, __ATOMIC_SEQ_CST);
    if (nextWriteSeq == 0)
        return 0;

    latestReadSeq = nextWriteSeq - 1;
    slotId = (unsigned int)(latestReadSeq % pstRing->maxElemCnt);
    expectedVersionOfWrite = (latestReadSeq * 2) + 2;
    versionBeforeCopy = __atomic_load_n(&pstRing->szVersionOfWrite[slotId], __ATOMIC_SEQ_CST);
    if ((versionBeforeCopy & 1) != 0 || versionBeforeCopy != expectedVersionOfWrite)
        return 0;

    memcpy(pElem, pstRing->pElemAryBuf + (slotId * pstRing->sizeOf1Elem), pstRing->sizeOf1Elem);
    versionAfterCopy = __atomic_load_n(&pstRing->szVersionOfWrite[slotId], __ATOMIC_SEQ_CST);
    if ((versionAfterCopy & 1) != 0 || versionAfterCopy != expectedVersionOfWrite)
        return 0;

    return 1;
}

int SPSC_CoverableRing_Reader_DropOldElems(SPSC_COVERABLE_RING *pstRing, unsigned int retainedFreshElemCnt)
{
    unsigned long long nextWriteSeq, nextReadSeq;
    int readableElemCnt, droppedElemCnt;

    if (pstRing == NULL || pstRing->pElemAryBuf == NULL)
        return ERR_ARGV_1;

    nextWriteSeq = __atomic_load_n(&pstRing->nextWriteSeq, __ATOMIC_SEQ_CST);
    nextReadSeq = SPSC_CoverableRing_Reader_SkipOverwrittenElemsAndGetNextReadSeq(pstRing, nextWriteSeq);
    if (nextWriteSeq <= nextReadSeq)
        return 0;

    readableElemCnt = (int)(nextWriteSeq - nextReadSeq);
    if (retainedFreshElemCnt >= (unsigned int)readableElemCnt)
        return 0;

    droppedElemCnt = readableElemCnt - (int)retainedFreshElemCnt;
    __atomic_store_n(&pstRing->nextReadSeq, nextReadSeq + (unsigned long long)droppedElemCnt, __ATOMIC_SEQ_CST);
    __atomic_add_fetch(&pstRing->missedElemCnt, (unsigned long long)droppedElemCnt, __ATOMIC_SEQ_CST);
    return droppedElemCnt;
}

unsigned int SPSC_CoverableRing_Reader_GetReadableElemCnt(SPSC_COVERABLE_RING *pstRing)
{
    unsigned long long nextWriteSeq, nextReadSeq, readableElemCnt;

    if (pstRing == NULL || pstRing->pElemAryBuf == NULL)
    {
        PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
        return 0;
    }
    nextWriteSeq = __atomic_load_n(&pstRing->nextWriteSeq, __ATOMIC_SEQ_CST);
    nextReadSeq = __atomic_load_n(&pstRing->nextReadSeq, __ATOMIC_SEQ_CST);
    if (nextWriteSeq <= nextReadSeq)
        return 0;

    readableElemCnt = nextWriteSeq - nextReadSeq;
    if (readableElemCnt > pstRing->maxElemCnt)
        return pstRing->maxElemCnt;

    return (unsigned int)readableElemCnt;
}

unsigned long long SPSC_CoverableRing_Reader_GetMissedElemCnt(SPSC_COVERABLE_RING *pstRing)
{
    unsigned long long nextWriteSeq, nextReadSeq, firstReadableSeq, missedElemCnt;

    if (pstRing == NULL || pstRing->pElemAryBuf == NULL)
    {
        PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
        return 0;
    }
    missedElemCnt = __atomic_load_n(&pstRing->missedElemCnt, __ATOMIC_SEQ_CST);
    nextWriteSeq = __atomic_load_n(&pstRing->nextWriteSeq, __ATOMIC_SEQ_CST);
    nextReadSeq = __atomic_load_n(&pstRing->nextReadSeq, __ATOMIC_SEQ_CST);
    if (nextWriteSeq > pstRing->maxElemCnt)
        firstReadableSeq = nextWriteSeq - pstRing->maxElemCnt;
    else
        firstReadableSeq = 0;

    if (nextReadSeq < firstReadableSeq)
        missedElemCnt += firstReadableSeq - nextReadSeq;

    return missedElemCnt;
}

static unsigned long long SPSC_CoverableRing_Reader_SkipOverwrittenElemsAndGetNextReadSeq(SPSC_COVERABLE_RING *pstRing, unsigned long long nextWriteSeq)
{
    unsigned long long nextReadSeq, firstReadableSeq, overwrittenElemCnt;

    nextReadSeq = __atomic_load_n(&pstRing->nextReadSeq, __ATOMIC_SEQ_CST);
    if (nextWriteSeq > pstRing->maxElemCnt)
        firstReadableSeq = nextWriteSeq - pstRing->maxElemCnt;
    else
        firstReadableSeq = 0;

    if (nextReadSeq < firstReadableSeq)
    {
        overwrittenElemCnt = firstReadableSeq - nextReadSeq;
        __atomic_store_n(&pstRing->nextReadSeq, firstReadableSeq, __ATOMIC_SEQ_CST);
        __atomic_add_fetch(&pstRing->missedElemCnt, overwrittenElemCnt, __ATOMIC_SEQ_CST);
        nextReadSeq = firstReadableSeq;
    }

    return nextReadSeq;
}

//Caller has already filtered out odd write-busy versions; an even version mismatch means this read sequence was overwritten.
static void SPSC_CoverableRing_Reader_SkipOverwrittenElemsAndUpdateMissedElemCnt(SPSC_COVERABLE_RING *pstRing, unsigned long long overwrittenReadSeq)
{
    unsigned long long nextWriteSeq, fixedNextReadSeq;

    nextWriteSeq = __atomic_load_n(&pstRing->nextWriteSeq, __ATOMIC_SEQ_CST);
    fixedNextReadSeq = SPSC_CoverableRing_Reader_SkipOverwrittenElemsAndGetNextReadSeq(pstRing, nextWriteSeq);
    if (fixedNextReadSeq <= overwrittenReadSeq)
    {
        __atomic_store_n(&pstRing->nextReadSeq, overwrittenReadSeq + 1, __ATOMIC_SEQ_CST);
        __atomic_add_fetch(&pstRing->missedElemCnt, 1, __ATOMIC_SEQ_CST);
    }
}
