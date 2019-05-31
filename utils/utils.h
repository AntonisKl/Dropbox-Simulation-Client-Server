#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
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
#include <time.h>
#include <unistd.h>
#include <utime.h>

#define MAX_CONNECT_ATTEMPTS 5 // maximum reconnection attempts in case something goes wrong while trying to connect to a peer

// defines for all messages
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

// useful sizes
#define MAX_STRING_INT_SIZE 12  // including '\0' character
#define MAX_MESSAGE_SIZE 32     // including '\0' character

// color codes
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"

// prints string s in red color followed by '\n'
void printErrorLn(char* s);

// prints string s followed by '\n'
void printLn(char* s);

// returns 1 if file with name fileName exists
// returns 0 if file with name fileName doesn't exist
char fileExists(char* fileName);

// returns 1 if directory with name dirName exists
// returns 0 if directory with name dirName doesn't exist
char dirExists(char* dirName);

// creates directory with name dir and all of its parent directories if they don't exist
void _mkdir(const char* dir);

// removes the file name from path
void removeFileName(char* path);

// returns current machine's private ip address
struct in_addr getPrivateIp();

// tries to create a connection to peer which has information that is stored in peerAddr variable
int connectToPeer(int* socketFd, struct sockaddr_in* peerAddr);

// tries to create a server in current machine in port portNum
int createServer(int* socketFd, struct sockaddr_in* socketAddr, int portNum, int maxConnectionsNum);

#endif