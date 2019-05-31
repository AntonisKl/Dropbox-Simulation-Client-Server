#ifndef SERVER_H
#define SERVER_H

#include "../list/list.h"
#include "../utils/utils.h"

#define MAX_CONNECTIONS 10

int mySocketFd, newSocketFd;
List* clientsList = NULL;

void handleExit(int exitNum);

void handleArgs(int argc, char** argv, int* portNum);

void trySend(int socketFd, void* buffer, int bufferSize, CallingMode callingMode);

int tryRead(int socketId, void* buffer, int bufferSize, CallingMode callingMode);

void handleIncomingMessage(int socketFd, char* message);

#endif