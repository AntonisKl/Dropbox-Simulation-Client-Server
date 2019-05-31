#include "cyclic_buffer.h"

void initBuffer(CyclicBuffer* cyclicBuffer, int bufferSize) {
    cyclicBuffer->buffer = malloc(bufferSize * sizeof(BufferNode));
    cyclicBuffer->maxSize = bufferSize;
    cyclicBuffer->curSize = 0;
    cyclicBuffer->startIndex = 0;
    cyclicBuffer->endIndex = -1;

    return;
}

BufferNode* initBufferNode(char* filePath, time_t version, uint32_t ip, int portNum) {
    // char filePath[MAX_PATH_SIZE];
    // time_t version;
    // struct in_addr ip;
    // int portNumber;
       // printf("init buffer node0\n");

    BufferNode* bufferNode = (BufferNode*)malloc(sizeof(BufferNode));
    if (filePath != NULL) {
        strcpy(bufferNode->filePath, filePath);
    } else {
        strcpy(bufferNode->filePath, ",");
    }
    
    bufferNode->version = version;
    bufferNode->ip = ip;
    bufferNode->portNumber = portNum;
   // printf("init buffer node\n");
    return bufferNode;
}

int cyclicBufferEmpty(CyclicBuffer* cyclicBuffer) {
    return cyclicBuffer->curSize == 0;
}

int cyclicBufferFull(CyclicBuffer* cyclicBuffer) {
    return cyclicBuffer->curSize == cyclicBuffer->maxSize;
}

// int calculateBufferIndex(int index) {
//     return index * sizeof(BufferNode);
// }

int addNodeToCyclicBuffer(CyclicBuffer* cyclicBuffer, char* filePath, time_t version, uint32_t ip, int portNum) {
    if (cyclicBufferFull(cyclicBuffer))
        return 1;

   // printf("buffer max size: %d\n", cyclicBuffer->maxSize);
    cyclicBuffer->endIndex = (cyclicBuffer->endIndex + 1)%cyclicBuffer->maxSize;
    // memcpy(cyclicBuffer->cyclicBuffer->buffer[cyclicBuffer->endIndex])
   // printf("hi cyclicBuffer->endIndex : %d\n", cyclicBuffer->endIndex );
    cyclicBuffer->buffer[cyclicBuffer->endIndex] = initBufferNode(filePath, version, ip, portNum);
       // printf("ho\n");

    cyclicBuffer->curSize++;

    return 0;
}

BufferNode* getNodeFromCyclicBuffer(CyclicBuffer* cyclicBuffer) {
    if (cyclicBufferEmpty(cyclicBuffer))
        return NULL;

    BufferNode* curBufferNode = cyclicBuffer->buffer[cyclicBuffer->startIndex];
    BufferNode* bufferNodeToReturn = initBufferNode(curBufferNode->filePath, curBufferNode->version, curBufferNode->ip, curBufferNode->portNumber);
    cyclicBuffer->startIndex = (cyclicBuffer->startIndex + 1)%cyclicBuffer->maxSize;
    cyclicBuffer->curSize--;

    freeBufferNode(&curBufferNode);

    return bufferNodeToReturn;
}

void freeBufferNode(BufferNode** bufferNode) {
    free(*bufferNode);
    (*bufferNode) = NULL;

    return;
}
