#ifndef SERVER_H
#define SERVER_H

#include "../list/list.h"
#include "../utils/utils.h"

#define MAX_CONNECTIONS 10

void handleExit(int exitNum);

void handleArgs(int argc, char** argv, int* portNum);

void trySend(int socketFd, void* buffer, int bufferSize, CallingMode callingMode);

void tryRead(int socketId, void* buffer, int bufferSize, CallingMode callingMode);

#endif