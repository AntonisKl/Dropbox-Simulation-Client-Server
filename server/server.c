#include "server.h"

void handleExit(int exitNum) {
    exit(exitNum);
}

void handleArgs(int argc, char** argv, int* portNum) {
    // validate argument count
    if (argc != 3) {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    // validate input arguments one by one
    if (strcmp(argv[1], "-p") == 0) {
        (*portNum) = atoi(argv[2]);
        if ((*portNum) < 0) {
            printErrorLn("Invalid arguments\nExiting...");
            handleExit(1);
        }
    } else {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    return;
}

void trySend(int socketFd, void* buffer, int bufferSize, CallingMode callingMode) {
    if (send(socketFd, buffer, bufferSize, 0) == -1) {  // send LOG_ON to server
        perror("Send error");
        if (callingMode == MAIN_THREAD)
            handleExit(1);
        else
            pthread_exit((void**)1);
    }

    return;
}

void tryRead(int socketId, void* buffer, int bufferSize, CallingMode callingMode) {
    int returnValue, tempBufferSize = bufferSize, progress = 0;

    // trySelect(fd);
    returnValue = read(socketId, buffer, tempBufferSize);
    while (returnValue < tempBufferSize && returnValue != 0) {  // rare case
        // not all desired bytes were read, so read the remaining bytes and add them to buffer
        if (returnValue == -1) {
            perror("Read error");
            if (callingMode == MAIN_THREAD)
                handleExit(1);
            else
                pthread_exit((void**)1);
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

int main(int argc, char** argv) {
    int portNum, mySocketFd, newSocketFd;
    struct sockaddr_in myAddr, incomingAddr;

    handleArgs(argc, argv, &portNum);

    List* clientsList = initList(CLIENTS);

    if (createServer(&mySocketFd, &myAddr, portNum, MAX_CONNECTIONS)) {
        handleExit(1);
    }

    printf(ANSI_COLOR_GREEN "Started server with ip %s" ANSI_COLOR_RESET "\n", inet_ntoa(myAddr.sin_addr));

    int incomingAddrLen = sizeof(incomingAddr);
    while (1) {
        printLn("Listening for incoming connections");
        if ((newSocketFd = accept(mySocketFd, (struct sockaddr*)&incomingAddr, (socklen_t*)&incomingAddrLen)) < 0) {
            perror("Accept error");
            handleExit(1);
        }
        printLn("Accepted an incoming connection");

        char message[MAX_MESSAGE_SIZE];
        tryRead(newSocketFd, &message, MAX_MESSAGE_SIZE, MAIN_THREAD);
        printf("Read message: %s\n", message);

        if (strcmp(message, LOG_ON) == 0) {
            printLn("Got LOG_ON message");

            int curPortNum;
            struct in_addr curIpStruct;
            tryRead(newSocketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);
            printLn("after read 1");

            tryRead(newSocketFd, &curPortNum, 4, MAIN_THREAD);
            printLn("after read 2");

            curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
            curPortNum = ntohs(curPortNum);

            int clientSocketFd;
            struct sockaddr_in clientAddr;
            ClientInfo* curClientInfo = clientsList->firstNode;
            while (curClientInfo != NULL) {
                clientAddr.sin_family = AF_INET;
                clientAddr.sin_port = curClientInfo->portNumber;
                clientAddr.sin_addr.s_addr = curClientInfo->ipStruct.s_addr;

                if (connectToPeer(&clientSocketFd, &clientAddr) == 1)
                    handleExit(1);

                trySend(clientSocketFd, USER_ON, MAX_MESSAGE_SIZE, MAIN_THREAD);
                trySend(clientSocketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);
                trySend(clientSocketFd, &curPortNum, 4, MAIN_THREAD);

                close(clientSocketFd);
                curClientInfo = curClientInfo->nextClientInfo;
            }
            // printf("ha");
            addNodeToList(clientsList, initClientInfo(curIpStruct, curPortNum));
        } else if (strcmp(message, GET_CLIENTS) == 0) {
            printLn("Got GET_CLIENTS message");

            trySend(newSocketFd, CLIENT_LIST, MAX_MESSAGE_SIZE, MAIN_THREAD);

            unsigned int listSendingSize = clientsList->size - 1;
            trySend(newSocketFd, &listSendingSize, 4, MAIN_THREAD);

            uint32_t ipToSend;
            int portNumToSend;
            ClientInfo* curClientInfo = clientsList->firstNode;
            while (curClientInfo != NULL) {
                if (curClientInfo->ipStruct.s_addr == incomingAddr.sin_addr.s_addr && curClientInfo->portNumber == incomingAddr.sin_port)
                    continue;

                ipToSend = htonl(curClientInfo->ipStruct.s_addr);
                portNumToSend = htons(curClientInfo->portNumber);

                trySend(newSocketFd, &ipToSend, 4, MAIN_THREAD);
                trySend(newSocketFd, &portNumToSend, 4, MAIN_THREAD);

                curClientInfo = curClientInfo->nextClientInfo;
            }
        } else if (strcmp(message, LOG_OFF) == 0) {
            int curPortNum;
            struct in_addr curIpStruct;
            tryRead(newSocketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);
            tryRead(newSocketFd, &curPortNum, 4, MAIN_THREAD);

            curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
            curPortNum = ntohs(curPortNum);

            if (deleteNodeFromList(clientsList, &incomingAddr.sin_port, &incomingAddr.sin_addr.s_addr) == -1) {
                trySend(newSocketFd, ERROR_IP_PORT_NOT_FOUND_IN_LIST, MAX_MESSAGE_SIZE, MAIN_THREAD);
                continue;
            }

            int clientSocketFd;
            struct sockaddr_in clientAddr;
            ClientInfo* curClientInfo = clientsList->firstNode;
            while (curClientInfo != NULL) {
                clientAddr.sin_family = AF_INET;
                clientAddr.sin_port = curClientInfo->portNumber;
                clientAddr.sin_addr.s_addr = curClientInfo->ipStruct.s_addr;

                if (connectToPeer(&clientSocketFd, &clientAddr) == 1)
                    handleExit(1);

                trySend(clientSocketFd, USER_OFF, MAX_MESSAGE_SIZE, MAIN_THREAD);
                trySend(clientSocketFd, &incomingAddr.sin_addr.s_addr, 4, MAIN_THREAD);
                trySend(clientSocketFd, &curPortNum, 4, MAIN_THREAD);

                close(clientSocketFd);
                curClientInfo = curClientInfo->nextClientInfo;
            }
        }

        close(newSocketFd);
    }
}