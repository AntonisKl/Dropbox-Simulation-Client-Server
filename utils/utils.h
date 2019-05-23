#ifndef UTILS_H
#define UTILS_H

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h> /* superset of previous */
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
#include <arpa/inet.h>
#include <sys/socket.h>

// COLOR CODES
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"

// USEFUL SIZES
#define MAX_STRING_INT_SIZE 12
#define MAX_FIFO_FILE_NAME (MAX_STRING_INT_SIZE * 2 - 1) + 9
#define MAX_FILE_LIST_NODE_STRING_SIZE MAX_STRING_INT_SIZE + 1 + (2 * PATH_MAX) + 1
#define EMPTY_FILE_LIST_STRING "**"
#define MAX_TEMP_FILELIST_FILE_NAME_SIZE 15 + MAX_STRING_INT_SIZE + 1  // "tmp/" + "TempFileList" + int
#define MIN_KEY_DETAILS_FILE_SIZE 119
#define MAX_TMP_ENCRYPTED_FILE_PATH_SIZE 8 + MAX_STRING_INT_SIZE

void printErrorLn(char* s);

#endif