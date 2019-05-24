#include "utils.h"

void printErrorLn(char* s) {
    printf(ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", s);

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

void tryRead(int socketId, void* buffer, int bufferSize) {
    int returnValue, tempBufferSize = bufferSize, progress = 0;

    // trySelect(fd);
    returnValue = read(socketId, buffer, tempBufferSize);
    while (returnValue < tempBufferSize && returnValue != 0) {  // rare case
        // not all desired bytes were read, so read the remaining bytes and add them to buffer
        if (returnValue == -1) {
            perror("Read error");
            return;
        }

        tempBufferSize -= returnValue;
        progress += returnValue;
        // trySelect(socketId);
        returnValue = read(socketId, buffer + progress, tempBufferSize);  // read remaining bytes that aren't written yet
    }

    // if (returnValue == 0) {  // 0 = EOF which means that writer failed and closed the pipe early
    //     handleExit(1, SIGUSR1);
    // }

    return;
}

int connectToPeer(int* socketFd, struct sockaddr_in* peerAddr) {
    if (((*socketFd) = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    // serverAddr.sin_family = AF_INET;
    // serverAddr.sin_port = htons(serverPort);

    if (connect((*socketFd), (struct sockaddr*)&peerAddr, sizeof(peerAddr)) < 0) {
        perror("\nSocket connection failed\n");
        return 1;
    }
    
    return 0;
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

    socketAddr->sin_family = AF_INET;
    socketAddr->sin_addr.s_addr = htonl(INADDR_ANY);
    socketAddr->sin_port = htons(portNum);

    if (bind(socketFd, (struct sockaddr*)&socketAddr,
             sizeof(socketAddr)) < 0) {
        perror("Bind error");
        return 1;
    }
    if (listen(socketFd, maxConnectionsNum) < 0) {
        perror("Listen error");
        return 1;
    }

    return 0;
}

int selectSocket(int socketFd1, int socketFd2) {
    fd_set socketFds;
    struct timeval timeout;
    int selectRet, result;

    /* Set time limit. */
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
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