#pragma once

#include <inttypes.h>

typedef void* FreeListRef;

FreeListRef CreateFreeList(uint8_t* msgBuffer, int msgCount, uint64_t msgSize);
uint8_t* GetMessage(FreeListRef fl);
void PutMessage(FreeListRef fl, uint8_t* msg);
void DestroyFreeList(FreeListRef fl);