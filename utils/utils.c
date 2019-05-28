#include "utils.h"

void printErrorLn(char* s) {
    printf(ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", s);

    return;
}

void printLn(char* s) {
    printf(ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\n", s);

    return;
}

char fileExists(char* fileName) {
    if (access(fileName, F_OK) != -1) {
        // file exists
        return 1;
    }

    // file does not exist
    return 0;
}

char dirExists(char* dirName) {
    char returnValue = 0;
    DIR* dir = opendir(dirName);
    if (dir) {
        // directory exists
        returnValue = 1;
        closedir(dir);
    }

    // directory does not exist
    return returnValue;
}

void createDir(char* dirPath) {
    int pid = fork();
    if (pid == 0) {
        char* args[] = {"mkdir", "-p", dirPath, NULL};  // -p parameter is used to create the whole directory structure if it does not exist
        if (execvp(args[0], args) == -1) {
            perror("execvp failed");
            exit(1);
        }
    } else if (pid == -1) {
        perror("fork error");
        exit(1);
    } else {
        wait(NULL);
        printf("dir with name: %s created!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", dirPath);
    }

    return;
}

void _mkdir(const char* dir) {
    char tmp[256];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU);
            *p = '/';
        }
    mkdir(tmp, S_IRWXU);
}

void createAndWriteToFile(char* fileName, char* contents) {
    FILE* file = fopen(fileName, "w");  // overwrite if file exists
    if (file == NULL) {
        perror("fopen failed");
        exit(1);
    }

    fprintf(file, "%s", contents);
    fflush(file);

    if (fclose(file) == EOF) {
        perror("fclose failed");
        exit(1);
    }
    return;
}

void removeFileName(char* path) {
    char* const last = strrchr(path, '/');
    if (last != NULL)
        *last = '\0';
}

struct in_addr getLocalIp() {
    struct ifaddrs* ifAddrStruct = NULL;
    struct ifaddrs* ifa = NULL;
    void* tmpAddrPtr = NULL;

    getifaddrs(&ifAddrStruct);

    ifa = ifAddrStruct->ifa_next->ifa_next->ifa_next;

    if (ifa->ifa_addr->sa_family == AF_INET) {  // check it is IP4
        // is a valid IP4 Address
        tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
        char addressBuffer[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
        printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
    } else if (ifa->ifa_addr->sa_family == AF_INET6) {  // check it is IP6
        // is a valid IP6 Address
        tmpAddrPtr = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
        char addressBuffer[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
        printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
    }

    if (ifAddrStruct != NULL) freeifaddrs(ifAddrStruct);
    return ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
}

int connectToPeer(int* socketFd, struct sockaddr_in* peerAddr) {
    if (((*socketFd) = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }
    printf("Created socket\n");
    // serverAddr.sin_family = AF_INET;
    // serverAddr.sin_port = htons(serverPort);
    // peerAddr->sin_addr.s_addr = htonl(peerAddr->sin_addr.s_addr);
    // peerAddr->sin_port = htons(peerAddr->sin_port);
    int attempts = 1;
    char connected = 1;
    while (connect((*socketFd), (struct sockaddr*)peerAddr, sizeof(*peerAddr)) < 0) {
        perror("Socket connection failed");
        printf("Attempt: %d, retrying...\n", attempts);
        attempts++;
        if (attempts == (MAX_CONNECT_ATTEMPTS) + 1) {
            connected = 0;
            break;
        }
        // return 1;
        sleep(1);
    }

    if (connected) {
        printf("Connected to port %d and ip %s\n", peerAddr->sin_port, inet_ntoa(peerAddr->sin_addr));
        return 0;
    } else
        return 1;
}

int createServer(int* socketFd, struct sockaddr_in* socketAddr, int portNum, int maxConnectionsNum) {
    // Creating socket file descriptor
    if (((*socketFd) = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        // handleExit(1);
        return 1;
    }

    // // Forcefully attaching socket to the port 8080
    // if (setsockopt(serverSocketFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
    //                                               &opt, sizeof(opt)))
    // {
    //     perror("setsockopt");
    //     exit(EXIT_FAILURE);
    // }
    // memset(&socketAddr, 0, sizeof(socketAddr));
    socketAddr->sin_family = AF_INET;
    socketAddr->sin_addr = getLocalIp();
    ;
    socketAddr->sin_port = portNum;

    if (bind(*socketFd, (struct sockaddr*)socketAddr,
             sizeof(*socketAddr)) < 0) {
        perror("Bind error");
        return 1;
    }
    printf("ha\n");
    if (listen(*socketFd, maxConnectionsNum) < 0) {
        perror("Listen error");
        return 1;
    }

    return 0;
}

int selectSocket(int socketFd1, int socketFd2) {
    fd_set socketFds;
    // struct timeval timeout;
    int selectRet;

    /* Set time limit. */
    // timeout.tv_sec = 3;
    // timeout.tv_usec = 0;
    /* Create a descriptor set containing our two sockets.  */
    FD_ZERO(&socketFds);
    FD_SET(socketFd1, &socketFds);
    FD_SET(socketFd2, &socketFds);

    selectRet = select(sizeof(socketFds) * 8, &socketFds, NULL, NULL, NULL);  // last arg: timeout
    if (selectRet == -1) {
        perror("Select failed");
        return -1;
    }

    if (selectRet > 0) {
        if (FD_ISSET(socketFd1, &socketFds))
            return socketFd1;
        if (FD_ISSET(socketFd2, &socketFds))
            return socketFd2;
    }
    //    result = 0;
    //    if (selectRet > 0)
    //    {
    //       if (FD_ISSET(socketFd1, &socketFds)) result |= S1READY;
    //       if (FD_ISSET(socketFd2, &socketFds)) result |= S2READY;
    //    }

    return -1;
}

// int trySend(int socketId, char* buffer, int bufferSize) {
//     if (send(socketId, buffer, bufferSize, 0) == -1) {
//         perror("Send error");
//         return -1;
//     }
//     return 0;
// }

// void tryWrite(int fd, const void* buffer, int bufferSize) {
//     int returnValue, tempBufferSize = bufferSize, progress = 0;

//     returnValue = write(fd, buffer, tempBufferSize);
//     while (returnValue < tempBufferSize && returnValue != 0) {  // rare case
//         // not all desired bytes were written, so write the remaining bytes of buffer
//         if (returnValue == -1) {
//             perror("write error");
//             handleExit(1, SIGUSR2);
//         }

//         tempBufferSize -= returnValue;
//         progress += returnValue;
//         returnValue = write(fd, buffer + progress, tempBufferSize); // write remaining bytes that aren't written yet
//     }

//     return;
// }