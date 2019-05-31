#ifndef CYCLIC_BUFFER_H
#define CYCLIC_BUFFER_H

#include "../utils/utils.h"

#define MAX_PATH_SIZE 128

typedef struct BufferNode BufferNode;  // forward declaration

typedef struct CyclicBuffer {
    BufferNode** buffer;  // array of BufferNode* elements
    int startIndex, endIndex;
    // curSize: size of buffer at a given moment
    // maxSize: maximum buffer size
    unsigned int curSize, maxSize;
} CyclicBuffer;

struct BufferNode {
    char filePath[MAX_PATH_SIZE];  // filePath: can have 3 different values -> 1. path of a file :  indicates that current buffer node is a file node
                                   //                                          2. ","  : indicates that current buffer node is a client node
                                   //                                          3. ",," : indicates that the thread that will handle this buffer node must exit

    time_t version;  // version: a timestamp which is the modification time of a file
    uint32_t ip;     // ip: ip address of the client that is represented by this buffer node
    int portNumber;  // portNumber: port number of the client that is represented by this buffer node
};

// initializes the cyclicBuffer struct
void initBuffer(CyclicBuffer* cyclicBuffer, int bufferSize);

// initializes and returns a new buffer node
// if filePath is NULL, it gives the string "," as filePath's value
BufferNode* initBufferNode(char* filePath, time_t version, uint32_t ip, int portNum);

// returns 1 if cyclicBuffer is empty
// returns 0 if cyclicBuffer is not empty
int cyclicBufferEmpty(CyclicBuffer* cyclicBuffer);

// returns 1 if cyclicBuffer is full
// returns 0 if cyclicBuffer is not full
int cyclicBufferFull(CyclicBuffer* cyclicBuffer);

// adds a new node to cyclic buffer if it is not empty
// returns 1 if the new node was neither created nor inserted
// returns 0 if the new node was created and inserted
int addNodeToCyclicBuffer(CyclicBuffer* cyclicBuffer, char* filePath, time_t version, uint32_t ip, int portNum);

// removes a node from the start of the cyclic buffer and returns it by copying its contents to a newly allocated buffer node
BufferNode* getNodeFromCyclicBuffer(CyclicBuffer* cyclicBuffer);

// frees bufferNode
void freeBufferNode(BufferNode** bufferNode);

// frees the whole cyclicBuffer struct
void freeBuffer(CyclicBuffer* cyclicBuffer);

#endif