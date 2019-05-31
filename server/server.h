#ifndef SERVER_H
#define SERVER_H

#include "../list/list.h"
#include "../utils/utils.h"

#define MAX_CONNECTIONS 50 // maximum open connections that are allowed for the server

int mySocketFd, newSocketFd;
List* clientsList = NULL;

// frees allocated memory, closes sockets etc. and exits
void handleExit(int exitNum);

// checks and stores input arguments
void handleArgs(int argc, char** argv, int* portNum);

// calls write, performs error handling and exits according to callingMode's value (MAIN_THREAD or SECONDARY_THREAD)
void tryWrite(int socketFd, void* buffer, int bufferSize);

// calls read, performs error handling and exits according to callingMode's value (MAIN_THREAD or SECONDARY_THREAD)
// returns 0 if data of the required size were read
// returns 1 if EOF was read
int tryRead(int socketFd, void* buffer, int bufferSize);

// according to message's value, it does the required task to handle the incoming message
void handleIncomingMessage(int socketFd, char* message);

#endif