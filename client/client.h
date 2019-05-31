#ifndef CLIENT_H
#define CLIENT_H

#include "../cyclic_buffer/cyclic_buffer.h"
#include "../list/list.h"
#include "../utils/utils.h"

// parameters
#define MAX_CONNECTIONS 100 // maximum open connections that are allowed for the client's server
#define FILE_CHUNK_SIZE 100 // size of file chunk to send

// usefull define
#define MAX_CLIENT_NAME_SIZE 28  // format: 111.111.111.111_1234

pthread_mutex_t cyclicBufferMutex = PTHREAD_MUTEX_INITIALIZER, clientListMutex = PTHREAD_MUTEX_INITIALIZER; // mutexes for cyclic buffer and clients list access
// cyclicBufferFullCond: cyclic buffer is currently full
// cyclicBufferEmptyCond: cyclic buffer is currently empty
pthread_cond_t cyclicBufferFullCond = PTHREAD_COND_INITIALIZER, cyclicBufferEmptyCond = PTHREAD_COND_INITIALIZER; // condition variables for cyclic buffer access

pthread_t bufferFillerThreadId; // id of buffer filler thread
pthread_t* threadIds = NULL;// ids of worker threads

// serverSocketFd: socket descriptor for the socket that is used to communicate with the server
// mySocketFd: socket descriptor for the socket that is used to create this client's server
// workerThreadsNum: the number of worker threads that will be created
// portNum: the number of this client's server/listening port
// serverAddr: holds information about the connection of this client with the server
// localIpAddr: holds information about the private IP of this client
// clientName: this client's name -> format: 100.100.100.100_10 (10 represents this client's listening port)
// bufferFillerThreadCreated == 1: buffer filler thread was created
// bufferFillerThreadCreated == 0: buffer filler thread wasn't created
int serverSocketFd, mySocketFd, workerThreadsNum, portNum;
struct sockaddr_in serverAddr;
struct in_addr localIpAddr;
char clientName[MAX_CLIENT_NAME_SIZE], bufferFillerThreadCreated = 0;

// clientsList: list that holds information for all the clients in the system
// filesList: list that holds information for all of this client's local files
// cyclicBuffer: struct that holds information about the cyclic buffer
List *clientsList, *filesList;
CyclicBuffer cyclicBuffer;

// the 2 variables below are used by buffer filler thread
// nextClientIp: ip of the next client that will be inserted in the cyclic buffer
// nextClientPortNum: port number of the next client that will be inserted in the cyclic buffer
uint32_t nextClientIp;
int nextClientPortNum;

// handles the exit of all the sub-threads, the main thread and frees allocated memory
void handleExit(int exitNum);

// handles input arguments
void handleArgs(int argc, char** argv, char** dirName, int* portNum, int* workerThreadsNum, int* bufferSize, int* serverPort, struct sockaddr_in* serverAddr);

// recursively traverses the directory with name inputDirName and all of its subdirectories and
// adds to fileList all entries of the regular files and directories of the directory with name inputDirName
// inputDirName: in each recursive call of the function it represents the path until the current file
void populateFileList(List* fileList, char* inputDirName, char* rootDirName);

// calls write, performs error handling and exits according to callingMode's value (MAIN_THREAD or SECONDARY_THREAD)
void tryWrite(int socketFd, void* buffer, int bufferSize, CallingMode callingMode);

// calls read, performs error handling and exits according to callingMode's value (MAIN_THREAD or SECONDARY_THREAD)
// returns 0 if data of the required size were read
// returns 1 if EOF was read
int tryRead(int socketId, void* buffer, int bufferSize, CallingMode callingMode);

// builds client name -> format: ip_portNum
void buildClientName(char (*clientName)[], uint32_t ip, int portNum);

// handles SIGINT signal by calling handleExit(1) function and is used only by the main thread
void handleSigIntMainThread(int signal);

// buffer filler thread's job
// description: Fills cyclic buffer with the remaining clients' nodes that were not inserted initially due to lack of space.
void* bufferFillerThreadJob(void* a);
// worker thread's job
// description: Handles client buffer nodes and file buffer nodes. It uses mutexes due to concurrent access to cyclic buffer and clients list.
void* workerThreadJob(void* id);

// according to message's value, it does the required task to handle the incoming message
void handleIncomingMessage(int socketFd, char* message, char* dirName);

#endif