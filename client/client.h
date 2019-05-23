#ifndef CLIENT_H
#define CLIENT_H

#include "../utils/utils.h"
#include "../list/list.h"

#define MAX_CONNECTIONS 10

List* clientsList;
char* buffer;

typedef struct BufferNode {
    char filePath[128];
    time_t version;
    struct in_addr ip;
    int portNumber;
} BufferNode;

void handleExit(int exitNum);

void handleArgs(int argc, char** argv, char** dirName, int* portNum, int* workerThreadsNum, int* bufferSize, int* serverPort, struct sockaddr_in* serverAddr);

void* workerThreadJob( void* a);

// recursively traverses the directory with name inputDirName and all of its subdirectories and
// adds to fileList all entries of the regular files and directories of the directory with name inputDirName
// indent: it is used only for printing purposes
// inputDirName: in each recursive call of the function it represents the path until the current file
void populateFileList(List* fileList, char* inputDirName, int indent);

#endif