#ifndef CLIENT_H
#define CLIENT_H

#include "../cyclic_buffer/cyclic_buffer.h"
#include "../list/list.h"
#include "../utils/utils.h"

#define MAX_CONNECTIONS 100
#define FILE_CHUNK_SIZE 100
#define MAX_CLIENT_NAME_SIZE 28 // format: 111.111.111.111_1234
// #define MAX_IP_STRING_SIZE 16

pthread_mutex_t cyclicBufferMutex = PTHREAD_MUTEX_INITIALIZER, clientListMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cyclicBufferFullCond = PTHREAD_COND_INITIALIZER, cyclicBufferEmptyCond = PTHREAD_COND_INITIALIZER;

pthread_t bufferFillerThreadId;
pthread_t* threadIds = NULL;
char* threadsRunning;

// typedef struct CyclicBuffer CyclicBuffer;

int serverSocketFd, mySocketFd, workerThreadsNum;
struct sockaddr_in serverAddr;
struct in_addr localIpAddr;
int portNum;
char bufferFillerThreadCreated = 0, threadsShouldExit = 0;

List *clientsList, *filesList;
CyclicBuffer cyclicBuffer;

uint32_t nextClientIp;
int nextClientPortNum;

void handleExit(int exitNum);

void handleArgs(int argc, char** argv, char** dirName, int* portNum, int* workerThreadsNum, int* bufferSize, int* serverPort, struct sockaddr_in* serverAddr);

// recursively traverses the directory with name inputDirName and all of its subdirectories and
// adds to fileList all entries of the regular files and directories of the directory with name inputDirName
// indent: it is used only for printing purposes
// inputDirName: in each recursive call of the function it represents the path until the current file
void populateFileList(List* fileList, char* inputDirName, int indent);
void trySend(int socketFd, void* buffer, int bufferSize, CallingMode callingMode);
void tryInitAndSend(int* socketFd, void* buffer, int bufferSize, CallingMode callingMode, struct sockaddr_in* addr, int portNum);

void tryRead(int socketId, void* buffer, int bufferSize, CallingMode callingMode);

void buildClientName(char (*clientName)[], uint32_t ip, int portNum);

void handleSigIntMainThread(int signal);

void handleSigIntSecondaryThread(int signal);

void* bufferFillerThreadJob(void* a);
void* workerThreadJob(void* id);

#endif