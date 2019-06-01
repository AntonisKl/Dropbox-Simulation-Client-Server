#include "server.h"

void handleExit(int exitNum) {
    // close sockets
    close(newSocketFd);
    close(mySocketFd);

    // free allocated memory
    freeList(&clientsList);

    exit(exitNum);
}

void handleArgs(int argc, char** argv, int* portNum) {
    // validate argument count
    if (argc != 3) {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    // validate input arguments
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

void tryWrite(int socketFd, void* buffer, int bufferSize) {
    // write data to socket
    if (write(socketFd, buffer, bufferSize) == -1) {
        perror("Send error");
        handleExit(1);
    }

    return;
}

int tryRead(int socketFd, void* buffer, int bufferSize) {
    int returnValue, tempBufferSize = bufferSize, progress = 0;

    // read data from socket until all the bufferSize bytes are read
    returnValue = read(socketFd, buffer, tempBufferSize);
    while (returnValue < tempBufferSize && returnValue != 0) {  // rare case
        // not all desired bytes were read, so read the remaining bytes and add them to buffer
        if (returnValue == -1) {
            perror("Read error");
            // exit according to callingMode's value
            handleExit(1);
        }

        tempBufferSize -= returnValue;
        progress += returnValue;
        returnValue = read(socketFd, buffer + progress, tempBufferSize);  // read remaining bytes that aren't written yet
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

    handleExit(0);
}

void handleIncomingMessage(int socketFd, char* message) {
    if (strcmp(message, LOG_ON) == 0) {
        printLn("Got LOG_ON message");

        int curPortNum, curPortNumToSend;               // curPortNumToSend: port number to send without executing ntohs()
        struct in_addr curIpStruct, curIpStructToSend;  // curIpStructToSend: ip struct to send without executing ntohl()
        // get new client's ip
        tryRead(socketFd, &curIpStruct.s_addr, 4);
        // get new client's port number
        tryRead(socketFd, &curPortNum, 4);

        curIpStructToSend.s_addr = curIpStruct.s_addr;
        curPortNumToSend = curPortNum;
        curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
        curPortNum = ntohs(curPortNum);
        printf("LOG_ON IP: %s, port: %d\n", inet_ntoa(curIpStruct), curPortNum);

        int clientSocketFd;               // socket descriptor of the connection with the client with which the server is trying to connect
        struct sockaddr_in clientAddr;    // struct that holds information about the client with which the server is trying to connect
        clientAddr.sin_family = AF_INET;  // IPv4
        // send new client's ip and port number to the rest of the connected clients
        ClientInfo* curClientInfo = clientsList->firstNode;
        while (curClientInfo != NULL) {
            if (curClientInfo->ipStruct.s_addr == curIpStruct.s_addr && curClientInfo->portNumber == curPortNum)
                continue;

            clientAddr.sin_port = curClientInfo->portNumber;
            clientAddr.sin_addr.s_addr = curClientInfo->ipStruct.s_addr;

            printf("connecting to peer with ip: %s and port: %d\n", inet_ntoa(curClientInfo->ipStruct), curClientInfo->portNumber);
            if (connectToPeer(&clientSocketFd, &clientAddr) == 1)
                handleExit(1);

            // send USER_ON message
            tryWrite(clientSocketFd, USER_ON, MAX_MESSAGE_SIZE);
            // send new client's ip
            tryWrite(clientSocketFd, &curIpStructToSend.s_addr, 4);
            // send new client's port number
            tryWrite(clientSocketFd, &curPortNumToSend, 4);

            // close current client's socket
            close(clientSocketFd);

            curClientInfo = curClientInfo->nextClientInfo;
        }
        // add new client's node to clients list
        addNodeToList(clientsList, initClientInfo(curIpStruct, curPortNum));
    } else if (strcmp(message, GET_CLIENTS) == 0) {
        printLn("Got GET_CLIENTS message");

        // send CLIENT_LIST message
        tryWrite(socketFd, CLIENT_LIST, MAX_MESSAGE_SIZE);

        unsigned int listSendingSize = clientsList->size;
        // send clients list size
        tryWrite(socketFd, &listSendingSize, 4);

        uint32_t ipToSend;  // converted ip to send to client
        int portNumToSend;  // converted port number to send to client
        // send all connected clients to currently connected client
        ClientInfo* curClientInfo = clientsList->firstNode;
        while (curClientInfo != NULL) {
            ipToSend = htonl(curClientInfo->ipStruct.s_addr);
            portNumToSend = htons(curClientInfo->portNumber);

            // send ip
            tryWrite(socketFd, &ipToSend, 4);
            // send port number
            tryWrite(socketFd, &portNumToSend, 4);

            curClientInfo = curClientInfo->nextClientInfo;
        }
    } else if (strcmp(message, LOG_OFF) == 0) {
        printLn("Got LOG_OFF message");
        int curPortNum, curPortNumToSend;               // curPortNumToSend: port number to send without executing ntohs()
        struct in_addr curIpStruct, curIpStructToSend;  // curIpStructToSend: ip struct to send without executing ntohl()
        tryRead(socketFd, &curIpStruct.s_addr, 4);
        tryRead(socketFd, &curPortNum, 4);

        curIpStructToSend.s_addr = curIpStruct.s_addr;
        curPortNumToSend = curPortNum;
        curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
        curPortNum = ntohs(curPortNum);
        printf("LOG_OFF IP: %s, port: %d\n", inet_ntoa(curIpStruct), curPortNum);

        // delete logged off client from clients list
        if (deleteNodeFromList(clientsList, &curPortNum, &curIpStruct.s_addr) == -1) {
            // send ERROR_IP_PORT_NOT_FOUND_IN_LIST message to current connected client
            tryWrite(socketFd, ERROR_IP_PORT_NOT_FOUND_IN_LIST, MAX_MESSAGE_SIZE);
            // do not proceed with handling of the current message
            return;
        }

        int clientSocketFd;
        struct sockaddr_in clientAddr;
        clientAddr.sin_family = AF_INET;

        // send the ip and port number of the logged off client to the rest of the connected clients
        ClientInfo* curClientInfo = clientsList->firstNode;
        while (curClientInfo != NULL) {
            clientAddr.sin_port = curClientInfo->portNumber;
            clientAddr.sin_addr.s_addr = curClientInfo->ipStruct.s_addr;

            if (connectToPeer(&clientSocketFd, &clientAddr) == 1)
                handleExit(1);

            // send USER_OFF message
            tryWrite(clientSocketFd, USER_OFF, MAX_MESSAGE_SIZE);
            // send ip address
            tryWrite(clientSocketFd, &curIpStructToSend.s_addr, 4);
            // send port number
            tryWrite(clientSocketFd, &curPortNumToSend, 4);

            // close current client's socket
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

    // add only SIGINT signal
    sigemptyset(&sigAction.sa_mask);
    sigaddset(&sigAction.sa_mask, SIGINT);

    if (sigaction(SIGINT, &sigAction, NULL) == -1) {
        perror("Error: cannot handle SIGINT");  // should not happen
    }

    int portNum;  // server's port number
    // incomingAddr: holds information about connection with a client
    // myAddr: holds information about server
    struct sockaddr_in myAddr, incomingAddr;

    handleArgs(argc, argv, &portNum);

    // init clients list to fill it later
    clientsList = initList(CLIENTS);

    // create the server
    if (createServer(&mySocketFd, &myAddr, portNum, MAX_CONNECTIONS)) {
        handleExit(1);
    }

    printf(ANSI_COLOR_GREEN "Started server with ip %s" ANSI_COLOR_RESET "\n", inet_ntoa(myAddr.sin_addr));

    int incomingAddrLen = sizeof(incomingAddr);

    fd_set activeSocketsSet, readSocketsSet;  // socket descriptor sets used for selecting

    // initialize the set of active sockets
    FD_ZERO(&activeSocketsSet);
    FD_SET(mySocketFd, &activeSocketsSet);

    while (1) {
        printLn("Listening for incoming connections");

        // block until data arrive on one or more active sockets
        readSocketsSet = activeSocketsSet;
        if (select(FD_SETSIZE, &readSocketsSet, NULL, NULL, NULL) < 0) {
            perror("Select error");
            handleExit(1);
        }

        // service all the sockets in the set
        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (!FD_ISSET(i, &readSocketsSet))  // socket not ready
                continue;

            if (!(i == mySocketFd)) {  // already-connected socket
                printLn("Handling existing connection");

                // data arrived on an already connected socket
                char message[MAX_MESSAGE_SIZE];
                if (tryRead(i, message, MAX_MESSAGE_SIZE) == 1) {
                    close(i);
                    FD_CLR(i, &activeSocketsSet);
                    continue;
                }
                handleIncomingMessage(i, message);
                continue;
            }

            // socket is server socket, so accept new connection
            if ((newSocketFd = accept(mySocketFd, (struct sockaddr*)&incomingAddr, (socklen_t*)&incomingAddrLen)) < 0) {
                perror("Accept error");
                handleExit(1);
            }

            printLn("Accepted new incoming connection");

            // add the new socket descriptor to active sockets' set
            FD_SET(newSocketFd, &activeSocketsSet);
        }
    }
}