#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h> /* superset of previous */
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// COLOR CODES
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"

#define LOG_ON "LOG_ON"
#define LOG_OFF "LOG_OFF"
#define GET_FILE_LIST "GET_FILE_LIST"
#define GET_FILE "GET_FILE"
#define GET_CLIENTS "GET_CLIENTS"

// USEFUL SIZES
#define MAX_STRING_INT_SIZE 12 // including '\0' character
#define MAX_MESSAGE_SIZE 15 // including '\0' character

void printErrorLn(char* s);

void tryRead(int socketId, void* buffer, int bufferSize);

#endif