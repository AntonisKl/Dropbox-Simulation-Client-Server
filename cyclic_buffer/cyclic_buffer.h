#ifndef CYCLIC_BUFFER_H
#define CYCLIC_BUFFER_H

#include "../utils/utils.h"

#define MAX_PATH_SIZE 128

typedef struct BufferNode BufferNode;

typedef struct CyclicBuffer {
    BufferNode** buffer;
    int startIndex, endIndex;
    unsigned int curSize, maxSize;
} CyclicBuffer;

struct BufferNode {
    char filePath[MAX_PATH_SIZE];
    time_t version;
    uint32_t ip;
    int portNumber;
};

void initBuffer(CyclicBuffer* cyclicBuffer, int bufferSize);

BufferNode* initBufferNode(char* filePath, time_t version, uint32_t ip, int portNum);

int cyclicBufferEmpty(CyclicBuffer* cyclicBuffer);

int cyclicBufferFull(CyclicBuffer* cyclicBuffer);

int addNodeToCyclicBuffer(CyclicBuffer* cyclicBuffer, char* filePath, time_t version, uint32_t ip, int portNum);

BufferNode* getNodeFromCyclicBuffer(CyclicBuffer* cyclicBuffer);

void freeBufferNode(BufferNode** bufferNode);

void freeBufferNodes(CyclicBuffer* cyclicBuffer);

#endif