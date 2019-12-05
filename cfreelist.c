#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cfreelist.h"

typedef struct {
    volatile uint64_t *bitVector;
    uint8_t* msgBuffer;
    int bitVectorSize;
    int msgCount;
    uint64_t msgSize;
} FreeList;

FreeListRef CreateFreeList(uint8_t* msgBuffer, int msgCount, uint64_t msgSize) {
    fprintf(stderr, "create FreeList\n");
    FreeList* fl = (FreeList*)malloc(sizeof(FreeList));
	if (fl == NULL) {
		fprintf(stderr, "failed to allocate FreeList\n");
		return NULL;
	}

	memset(fl, 0, sizeof(FreeList));

    int bitVectorSize = msgCount / 64;
    if (msgCount % 64 != 0) {
        bitVectorSize += 1;
    }

    fl->bitVector = (volatile uint64_t *) malloc (bitVectorSize * sizeof(uint64_t));
    if (fl->bitVector == NULL) {
        fprintf(stderr, "failed to allocate FreeList->bitVector\n");
        return NULL;
    }

    // set bitVector bits to 1 to indicate the position is available
    for (int i = 0; i < bitVectorSize; i++) {
        fl->bitVector[i] = 0xffffffffffffffff;
    }

    fl->msgCount = msgCount;
    fl->msgSize = msgSize;
    fl->bitVectorSize = bitVectorSize;
    fl->msgBuffer = msgBuffer;

    if (bitVectorSize % 64 != 0) {
        // set those extra bits to 0 to indicate they are unavailable
        uint64_t mask = 0xffffffffffffffff;
        int trailingBitsCount = bitVectorSize * 64 - msgCount;
        mask = mask >> trailingBitsCount;
        fl->bitVector[bitVectorSize-1] &= mask;
    }

    return fl;
}

uint8_t* GetMessage(FreeListRef freeListRef) {
    fprintf(stderr, "c get message\n");
    int numRetires = 0, pos = 0;
    uint64_t oldBitVectorVal = 0, mask = 0, newBitVectorVal = 0;
    bool success = false;
    FreeList *fl = (FreeList *)freeListRef;

    while (1) {
        for (int i = 0; i < fl->bitVectorSize; i++) {
retry:
            oldBitVectorVal = fl->bitVector[i];
            if (oldBitVectorVal == 0) {
                continue;
            }

            pos = __builtin_ffsll(oldBitVectorVal) - 1;
            if (pos < 0) {
                fprintf(stderr, "pos cannot be negative %d, 0x%" PRIx64 "\n", pos, oldBitVectorVal);
                goto retry;
            }
            mask = (uint64_t)0x1 << pos;
            newBitVectorVal = oldBitVectorVal ^ mask;
            success = __sync_bool_compare_and_swap(&(fl->bitVector[i]), oldBitVectorVal, newBitVectorVal);
            if (success == false) {
                goto retry;
            }
            pos += (i << 6);

            return fl->msgBuffer + (uint64_t)pos * (uint64_t)fl->msgSize;
        }

        numRetires += 1;
        if (numRetires >= 3) {
            break;
        }
    }

    fprintf(stderr, "no available spot found\n");
    return NULL;
}

void PutMessage(FreeListRef flRef, uint8_t* msg) {
    fprintf(stderr, "c put message\n");
    FreeList *fl = (FreeList *)flRef;
    int pos = (msg - fl->msgBuffer) / fl->msgSize;
    int vecIdx = pos >> 6;
    int vecOffset = pos & 63;

    fl->bitVector[vecIdx] |= (uint64_t)0x1 << pos;
    return;
}

void DestroyFreeList(FreeListRef flRef) {
    fprintf(stderr, "destroy free list\n");
    FreeList *fl = (FreeList *)flRef;
    free((void *)fl->bitVector);
    free(fl);
}