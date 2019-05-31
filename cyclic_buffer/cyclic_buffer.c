#include "cyclic_buffer.h"

void initBuffer(CyclicBuffer* cyclicBuffer, int bufferSize) {
    cyclicBuffer->buffer = malloc(bufferSize * sizeof(BufferNode));
    cyclicBuffer->maxSize = bufferSize;
    cyclicBuffer->curSize = 0;
    cyclicBuffer->startIndex = 0;
    cyclicBuffer->endIndex = -1;  // end index is -1 at start, the fist node will be added to index 0 of the buffer

    return;
}

BufferNode* initBufferNode(char* filePath, time_t version, uint32_t ip, int portNum) {
    BufferNode* bufferNode = (BufferNode*)malloc(sizeof(BufferNode));
    if (filePath != NULL) {
        strcpy(bufferNode->filePath, filePath);
    } else {
        strcpy(bufferNode->filePath, ",");  // "," indicates client buffer node
    }

    bufferNode->version = version;
    bufferNode->ip = ip;
    bufferNode->portNumber = portNum;
    return bufferNode;
}

int cyclicBufferEmpty(CyclicBuffer* cyclicBuffer) {
    return cyclicBuffer->curSize == 0;
}

int cyclicBufferFull(CyclicBuffer* cyclicBuffer) {
    return cyclicBuffer->curSize == cyclicBuffer->maxSize;
}

int addNodeToCyclicBuffer(CyclicBuffer* cyclicBuffer, char* filePath, time_t version, uint32_t ip, int portNum) {
    if (cyclicBufferFull(cyclicBuffer))  // cannot add node to buffer
        return 1;

    // proceed end index and insert new node
    cyclicBuffer->endIndex = (cyclicBuffer->endIndex + 1) % cyclicBuffer->maxSize;
    cyclicBuffer->buffer[cyclicBuffer->endIndex] = initBufferNode(filePath, version, ip, portNum);

    // update current size
    cyclicBuffer->curSize++;

    return 0;
}

BufferNode* getNodeFromCyclicBuffer(CyclicBuffer* cyclicBuffer) {
    if (cyclicBufferEmpty(cyclicBuffer))
        return NULL;

    // get a buffer node by copying its contents to a new one and then update start index
    BufferNode* curBufferNode = cyclicBuffer->buffer[cyclicBuffer->startIndex];
    BufferNode* bufferNodeToReturn = initBufferNode(curBufferNode->filePath, curBufferNode->version, curBufferNode->ip, curBufferNode->portNumber);
    cyclicBuffer->startIndex = (cyclicBuffer->startIndex + 1) % cyclicBuffer->maxSize;

    // update current size
    cyclicBuffer->curSize--;

    // free old buffer node
    freeBufferNode(&curBufferNode);

    return bufferNodeToReturn;// return copied buffer node
}

void freeBufferNode(BufferNode** bufferNode) {
    free(*bufferNode);
    (*bufferNode) = NULL;

    return;
}

void freeBuffer(CyclicBuffer* cyclicBuffer) {
    if (cyclicBuffer != NULL) {
        BufferNode* curBufferNode;
        while ((curBufferNode = getNodeFromCyclicBuffer(cyclicBuffer)) != NULL) {
            freeBufferNode(&curBufferNode);
        }
        free(cyclicBuffer->buffer);
        cyclicBuffer->buffer = NULL;
    }
}
