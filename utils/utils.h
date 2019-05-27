#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
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
#include <sys/time.h>
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

#define LOG_ON "LOG_ON\0"
#define LOG_OFF "LOG_OFF\0"
#define GET_FILE_LIST "GET_FILE_LIST\0"
#define FILE_LIST "FILE_LIST\0"
#define GET_FILE "GET_FILE\0"
#define GET_CLIENTS "GET_CLIENTS\0"
#define FILE_NOT_FOUND "FILE_NOT_FOUND\0"
#define FILE_UP_TO_DATE "FILE_UP_TO_DATE\0"
#define FILE_SIZE "FILE_SIZE\0"
#define USER_ON "USER_ON\0"
#define CLIENT_LIST "CLIENT_LIST\0"
#define USER_OFF "USER_OFF\0"
#define ERROR_IP_PORT_NOT_FOUND_IN_LIST "ERROR_IP_PORT_NOT_FOUND_IN_LIST\0"

// USEFUL SIZES
#define MAX_STRING_INT_SIZE 12  // including '\0' character
#define MAX_MESSAGE_SIZE 32     // including '\0' character

typedef enum CallingMode {
    SECONDARY_THREAD,
    MAIN_THREAD
} CallingMode;

void printErrorLn(char* s);

void printLn(char* s);

char fileExists(char* fileName);

char dirExists(char* dirName);

void createDir(char* dirPath);

void _mkdir(const char* dir);

void createAndWriteToFile(char* fileName, char* contents);

void removeFileName(char* path);

struct in_addr getLocalIp();

int connectToPeer(int* socketFd, struct sockaddr_in* peerAddr);

int createServer(int* socketFd, struct sockaddr_in* socketAddr, int portNum, int maxConnectionsNum);

int selectSocket(int socketFd1, int socketFd2);

#endif