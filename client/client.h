#ifndef CLIENT_H
#define CLIENT_H

#include "../list/list.h"
#include "../utils/utils.h"

#define MAX_CONNECTIONS 10

#define MAX_PATH_SIZE 128
#define FILE_CHUNK_SIZE 100

pthread_mutex_t cyclicBufferMutex = PTHREAD_MUTEX_INITIALIZER, clientListMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cyclicBufferFullCond = PTHREAD_COND_INITIALIZER, cyclicBufferEmptyCond = PTHREAD_COND_INITIALIZER;

typedef struct CyclicBuffer CyclicBuffer;

List *clientsList, *filesList;
CyclicBuffer cyclicBuffer;

struct CyclicBuffer {
    char* buffer;
    int startIndex, endIndex;
    unsigned int curSize, maxSize;
};

typedef struct BufferNode {
    char filePath[MAX_PATH_SIZE];
    time_t version;
    uint32_t ip;
    int portNumber;
} BufferNode;

void handleExit(int exitNum);

void handleArgs(int argc, char** argv, char** dirName, int* portNum, int* workerThreadsNum, int* bufferSize, int* serverPort, struct sockaddr_in* serverAddr);

void* workerThreadJob(void* a);

// recursively traverses the directory with name inputDirName and all of its subdirectories and
// adds to fileList all entries of the regular files and directories of the directory with name inputDirName
// indent: it is used only for printing purposes
// inputDirName: in each recursive call of the function it represents the path until the current file
void populateFileList(List* fileList, char* inputDirName, int indent);

#endif