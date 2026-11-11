#ifndef __SPSC_COVERABLE_RING_H__
#define __SPSC_COVERABLE_RING_H__

#include "PublicDef.h"

#define SPSC_COVERABLE_RING_ELEM_CNT_MAX 1129

typedef enum
{
    SPSC_COVERABLE_RING_EMPTY = ERRCODE_COMM_RING_EMPTY,
    SPSC_COVERABLE_RING_ELEM_WR_BUSY = ERRCODE_COMM_RING_ELEM_WR_BUSY,
    SPSC_COVERABLE_RING_ELEM_MISSED = ERRCODE_COMM_RING_ELEM_MISSED,
    SPSC_COVERABLE_RING_OP_FAIL = ERRCODE_COMM_RING_OP_FAIL,
    SPSC_COVERABLE_RING_OP_SUCCEED = RET_SUCCESS
} ENUM_SPSC_COVERABLE_RING_STATE;

typedef struct
{
    unsigned int sizeOf1Elem;
    unsigned int maxElemCnt;
    unsigned char *pElemAryBuf;
    unsigned long long nextWriteSeq; //Global stream sequence for the next Writer write, not a slot index.
    unsigned long long nextReadSeq;  //Global stream sequence for the Reader's next read.
    unsigned long long missedElemCnt; //Total elements missed by the Reader after falling behind.
    unsigned long long szVersionOfWrite[SPSC_COVERABLE_RING_ELEM_CNT_MAX]; //Write version of each slot: odd means writing, even means stable.
} SPSC_COVERABLE_RING;

int SPSC_CoverableRing_Init(SPSC_COVERABLE_RING *pstRing, void *pElemAryBuf, unsigned int elemAryBufSize, unsigned int sizeOf1Elem);
int SPSC_CoverableRing_Deinit(SPSC_COVERABLE_RING *pstRing);
int SPSC_CoverableRing_Clear(SPSC_COVERABLE_RING *pstRing);
int SPSC_CoverableRing_Writer_PushElem(SPSC_COVERABLE_RING *pstRing, const void *pElem);
const void* SPSC_CoverableRing_Writer_PushElemAndGetAddr(SPSC_COVERABLE_RING *pstRing, const void *pElem);
ENUM_SPSC_COVERABLE_RING_STATE SPSC_CoverableRing_Reader_PopElem(SPSC_COVERABLE_RING *pstRing, void *pElem);
int SPSC_CoverableRing_Reader_ReadLatest1Elem(SPSC_COVERABLE_RING *pstRing, void *pElem);
int SPSC_CoverableRing_Reader_DropOldElems(SPSC_COVERABLE_RING *pstRing, unsigned int retainedFreshElemCnt);
unsigned int SPSC_CoverableRing_Reader_GetReadableElemCnt(SPSC_COVERABLE_RING *pstRing);
unsigned long long SPSC_CoverableRing_Reader_GetMissedElemCnt(SPSC_COVERABLE_RING *pstRing);

#endif
