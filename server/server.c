#include "server.h"

void handleExit(int exitNum) {
    close(newSocketFd);
    close(mySocketFd);

    freeList(&clientsList);
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

int tryRead(int socketId, void* buffer, int bufferSize, CallingMode callingMode) {
    int returnValue, tempBufferSize = bufferSize, progress = 0;

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
        returnValue = read(socketId, buffer + progress, tempBufferSize);  // read remaining bytes that aren't written yet
    }

    if (returnValue == 0)  // 0 = EOF which means that client closed the connection
        return 1;

    return 0;
}

void handleSigInt(int signal) {
    if (signal != SIGINT) {
        printErrorLn("Caught wrong signal instead of SIGINT");
    }
    printf("Caught SIGINT\n");

    handleExit(1);
}

void handleIncomingMessage(int socketFd, char* message) {
    if (strcmp(message, LOG_ON) == 0) {
        printLn("Got LOG_ON message");

        int curPortNum, curPortNumToSend;
        struct in_addr curIpStruct, curIpStructToSend;
        tryRead(socketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);

        tryRead(socketFd, &curPortNum, 4, MAIN_THREAD);

        curIpStructToSend.s_addr = curIpStruct.s_addr;
        curPortNumToSend = curPortNum;
        curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
        curPortNum = ntohs(curPortNum);
        printf("ip: %s, port: %d\n", inet_ntoa(curIpStruct), curPortNum);

        int clientSocketFd;
        struct sockaddr_in clientAddr;
        ClientInfo* curClientInfo = clientsList->firstNode;
        while (curClientInfo != NULL) {

            if (curClientInfo->ipStruct.s_addr == curIpStruct.s_addr && curClientInfo->portNumber == curPortNum) 
                continue;
            
            clientAddr.sin_family = AF_INET;
            clientAddr.sin_port = curClientInfo->portNumber;
            clientAddr.sin_addr.s_addr = curClientInfo->ipStruct.s_addr;

            if (connectToPeer(&clientSocketFd, &clientAddr) == 1)
                handleExit(1);

            trySend(clientSocketFd, USER_ON, MAX_MESSAGE_SIZE, MAIN_THREAD);
            trySend(clientSocketFd, &curIpStructToSend.s_addr, 4, MAIN_THREAD);
            trySend(clientSocketFd, &curPortNumToSend, 4, MAIN_THREAD);

            close(clientSocketFd);
            curClientInfo = curClientInfo->nextClientInfo;
        }
        addNodeToList(clientsList, initClientInfo(curIpStruct, curPortNum));
    } else if (strcmp(message, GET_CLIENTS) == 0) {
        printLn("Got GET_CLIENTS message");

        trySend(socketFd, CLIENT_LIST, MAX_MESSAGE_SIZE, MAIN_THREAD);

        unsigned int listSendingSize = clientsList->size;
        trySend(socketFd, &listSendingSize, 4, MAIN_THREAD);

        uint32_t ipToSend;
        int portNumToSend;
        ClientInfo* curClientInfo = clientsList->firstNode;
        while (curClientInfo != NULL) {
            ipToSend = htonl(curClientInfo->ipStruct.s_addr);
            portNumToSend = htons(curClientInfo->portNumber);

            trySend(socketFd, &ipToSend, 4, MAIN_THREAD);
            trySend(socketFd, &portNumToSend, 4, MAIN_THREAD);

            curClientInfo = curClientInfo->nextClientInfo;
        }
    } else if (strcmp(message, LOG_OFF) == 0) {
        int curPortNum, curPortNumToSend;
        struct in_addr curIpStruct, curIpStructToSend;
        tryRead(socketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);
        tryRead(socketFd, &curPortNum, 4, MAIN_THREAD);

        curIpStructToSend.s_addr = curIpStruct.s_addr;
        curPortNumToSend = curPortNum;
        curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
        curPortNum = ntohs(curPortNum);
        printLn("Got LOG_OFF message");

        printf("Got ip: %s, port: %d\n", inet_ntoa(curIpStruct), curPortNum);

        if (deleteNodeFromList(clientsList, &curPortNum, &curIpStruct.s_addr) == -1) {
            trySend(socketFd, ERROR_IP_PORT_NOT_FOUND_IN_LIST, MAX_MESSAGE_SIZE, MAIN_THREAD);
            return;
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
            trySend(clientSocketFd, &curIpStructToSend.s_addr, 4, MAIN_THREAD);
            trySend(clientSocketFd, &curPortNumToSend, 4, MAIN_THREAD);

            close(clientSocketFd);
            curClientInfo = curClientInfo->nextClientInfo;
        }
    }
    return;
}

int main(int argc, char** argv) {
    struct sigaction sigAction;

    // setup the sighub handler
    sigAction.sa_handler = &handleSigInt;

    // restart the system call, if at all possible
    sigAction.sa_flags = SA_RESTART;

    // add only SIGINT signal (SIGUSR1 and SIGUSR2 signals are handled by the client's forked subprocesses)
    sigemptyset(&sigAction.sa_mask);
    sigaddset(&sigAction.sa_mask, SIGINT);

    if (sigaction(SIGINT, &sigAction, NULL) == -1) {
        perror("Error: cannot handle SIGINT");  // Should not happen
    }

    int portNum;
    struct sockaddr_in myAddr, incomingAddr;

    handleArgs(argc, argv, &portNum);

    clientsList = initList(CLIENTS);

    if (createServer(&mySocketFd, &myAddr, portNum, MAX_CONNECTIONS)) {
        handleExit(1);
    }

    printf(ANSI_COLOR_GREEN "Started server with ip %s" ANSI_COLOR_RESET "\n", inet_ntoa(myAddr.sin_addr));

    int incomingAddrLen = sizeof(incomingAddr);

    fd_set activeSocketsSet, readSocketsSet;

    /* Initialize the set of active sockets. */
    FD_ZERO(&activeSocketsSet);
    FD_SET(mySocketFd, &activeSocketsSet);

    while (1) {
        printLn("Listening for incoming connections");

        /* Block until input arrives on one or more active sockets. */
        readSocketsSet = activeSocketsSet;
        if (select(FD_SETSIZE, &readSocketsSet, NULL, NULL, NULL) < 0) {
            perror("Select error");
                handleExit(1);
        }

        /* Service all the sockets with input pending. */
        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (!FD_ISSET(i, &readSocketsSet))
                continue;

            if (!(i == mySocketFd)) {
                printLn("Handling existing connection");

                /* Data arriving on an already-connected socket. */
                char message[MAX_MESSAGE_SIZE];
                if (tryRead(i, message, MAX_MESSAGE_SIZE, MAIN_THREAD) == 1) {
                    close(i);
                    FD_CLR(i, &activeSocketsSet);
                    continue;
                }
                printf("Read message: %s\n", message);
                handleIncomingMessage(i, message);
                continue;
            }

            // socket is server socket, so accept new connection
            if ((newSocketFd = accept(mySocketFd, (struct sockaddr*)&incomingAddr, (socklen_t*)&incomingAddrLen)) < 0) {
                perror("Accept error");
                handleExit(1);
            }

            printLn("Accepted new incoming connection");

            FD_SET(newSocketFd, &activeSocketsSet);
        }
    }
}