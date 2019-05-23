#ifndef CLIENT_H
#define CLIENT_H

#include "../utils/utils.h"

typedef struct BufferNode {
    char filePath[128];
    time_t version;
    struct in_addr ip;
    int portNumber;
} BufferNode;

void handleExit(int exitNum);

void handleArgs(int argc, char** argv, char** dirName, int* portNum, int* workerThreadsNum, int* bufferSize, int* serverPort, struct in_addr* serverIpStructP);

#endif