#include "client.h"

void handleExit(int exitNum) {
    int retVal;
    pthread_kill(bufferFillerThreadId, SIGUSR1);
    for (int i = 0; i < sizeof(threadIds) / sizeof(pthread_t); i++) {
        pthread_kill(threadIds[i], SIGUSR1);
    }

    pthread_join(bufferFillerThreadId, (void**)&retVal);
    if (retVal == 1) {
        printErrorLn("Buffer filler thread exited with an error");
    }

    for (int i = 0; i < sizeof(threadIds) / sizeof(pthread_t); i++) {
        pthread_join(threadIds[i], (void**)&retVal);
        if (retVal == 1) {
            printf(ANSI_COLOR_RED "Worker thread with id %lu exited with an error" ANSI_COLOR_RESET "\n", threadIds[i]);
        }
    }

    tryInitAndSend(&serverSocketFd, LOG_OFF, MAX_MESSAGE_SIZE, MAIN_THREAD, &serverAddr, serverAddr.sin_port);  // send LOG_OFF to server

    printf("exiting...\n");
    exit(exitNum);
}

void handleArgs(int argc, char** argv, char** dirName, int* portNum, int* workerThreadsNum, int* bufferSize, int* serverPort, struct sockaddr_in* serverAddr) {
    // validate argument count
    if (argc != 13) {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    // validate input arguments one by one
    if (strcmp(argv[1], "-d") == 0) {
        (*dirName) = argv[2];
    } else {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    if (strcmp(argv[3], "-p") == 0) {
        (*portNum) = atoi(argv[4]);
        if ((*portNum) < 0) {
            printErrorLn("Invalid arguments\nExiting...");
            handleExit(1);
        }
    } else {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    if (strcmp(argv[5], "-w") == 0) {
        (*workerThreadsNum) = atoi(argv[6]);
        if ((*workerThreadsNum) <= 0) {
            printErrorLn("Invalid arguments\nExiting...");
            handleExit(1);
        }
    } else {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    if (strcmp(argv[7], "-b") == 0) {
        (*bufferSize) = atoi(argv[8]);
        if ((*bufferSize) <= 0) {
            printErrorLn("Invalid arguments\nExiting...");
            handleExit(1);
        }
    } else {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    if (strcmp(argv[9], "-sp") == 0) {
        (*serverPort) = atoi(argv[10]);
        if ((*serverPort) < 0) {
            printErrorLn("Invalid arguments\nExiting...");
            handleExit(1);
        }
    } else {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    if (strcmp(argv[11], "-sip") == 0) {
        inet_aton(argv[12], &serverAddr->sin_addr);
    } else {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    return;
}

void populateFileList(List* fileList, char* inputDirName, int indent) {
    DIR* dir;
    struct dirent* entry;
    char isEmpty = 1;
    char path[PATH_MAX];
    struct stat curStat;

    if ((dir = opendir(inputDirName)) == NULL) {
        perror("Could not open directory");
        handleExit(1);
    }

    // traverse the whole directory recursively
    while ((entry = readdir(dir)) != NULL) {
        isEmpty = 0;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, PATH_MAX, "%s/%s", inputDirName, entry->d_name);  // append to inputDirName the current file's name
        stat(path, &curStat);

        if (!S_ISREG(curStat.st_mode)) {  // is a directory
            printf("dir: %s\n", path);
            populateFileList(fileList, path, indent + 2);  // continue traversing directory
        } else {                                           // is a file
            printf("file: %s\n", path);
            addNodeToList(fileList, initFile(path, curStat.st_size, curStat.st_mtime));  // add a REGULAR_FILE node to FileList
        }
    }

    if (isEmpty == 1) {
        printf("he\n");
        // snprintf(path, PATH_MAX, "%s/%s", inputDirName, entry->d_name);  // append to inputDirName the current file's name
        stat(inputDirName, &curStat);

        addNodeToList(fileList, initFile(inputDirName, -1, curStat.st_mtime));  // add a DIRECTORY node to FileList only if it is empty
    }

    closedir(dir);  // close current directory
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

void tryInitAndSend(int* socketFd, void* buffer, int bufferSize, CallingMode callingMode, struct sockaddr_in* addr, int portNum) {
    addr->sin_family = AF_INET;
    addr->sin_port = portNum;

    if (connectToPeer(socketFd, addr) == 1) {
        if (callingMode == MAIN_THREAD)
            handleExit(1);
        else
            pthread_exit((void**)1);
    }

    if (send(*socketFd, buffer, bufferSize, 0) == -1) {  // send LOG_ON to server
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

void buildClientName(char (*clientName)[], uint32_t ip, int portNum) {
    // char ipS[MAX_STRING_INT_SIZE], portNumS[MAX_STRING_INT_SIZE];
    // sprintf(ipS, "%u", ip);
    // sprintf(portNumS, "%d", portNum);
    sprintf(*clientName, "%u_%d", ip, portNum);

    return;
}

void handleSigIntMainThread(int signal) {
    if (signal != SIGINT) {
        printErrorLn("Client caught wrong signal instead of SIGINT");
    }
    printf("Client caught SIGINT\n");

    handleExit(1);
}

void handleSigUsrSecondaryThread(int signal) {
    if (signal != SIGUSR1) {
        printErrorLn("Secondary thread caught wrong signal instead of SIGUSR1");
    }
    printf("Secondary thread caught SIGUSR1\n");

    pthread_exit((void**)1);
}

void* bufferFillerThreadJob(void* a) {
    struct sigaction sigAction;
    // setup the sighub handler
    sigAction.sa_handler = &handleSigUsrSecondaryThread;

    // restart the system call, if at all possible
    sigAction.sa_flags = SA_RESTART;

    // add only SIGINT signal (SIGUSR1 and SIGUSR2 signals are handled by the client's forked subprocesses)
    sigemptyset(&sigAction.sa_mask);
    sigaddset(&sigAction.sa_mask, SIGUSR1);

    if (sigaction(SIGUSR1, &sigAction, NULL) == -1) {
        perror("Error: cannot handle SIGINT");  // Should not happen
    }

    ////// store client infos in buffer

    ClientInfo* curClientInfo;

    while (1) {
        // nextClientIp = curClientInfo->ipStruct.s_addr;
        // nextClientPortNum = curClientInfo->portNumber;

        // pthread_mutex_lock(&cyclicBufferMutex);

        // while (cyclicBufferFull(&cyclicBuffer)) {
        //     pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
        // }

        if (nextClientIp == -1) {
            // pthread_mutex_unlock(&clientListMutex);
            // pthread_mutex_unlock(&cyclicBufferMutex);
            break;
        }
        pthread_mutex_lock(&clientListMutex);

        curClientInfo = findNodeInList(clientsList, &nextClientPortNum, &nextClientIp);

        if (curClientInfo->nextClientInfo != NULL) {
            nextClientIp = curClientInfo->nextClientInfo->ipStruct.s_addr;
            nextClientPortNum = curClientInfo->nextClientInfo->portNumber;
        } else {
            nextClientIp = -1;
        }

        pthread_mutex_lock(&cyclicBufferMutex);
        while (cyclicBufferFull(&cyclicBuffer)) {
            pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
        }
        if (addNodeToCyclicBuffer(&cyclicBuffer, NULL, -1, curClientInfo->ipStruct.s_addr, curClientInfo->portNumber) == 1) {
            printErrorLn("Something's wrong\n");  ///////////////////////////////////////////////////////////////////////////////// should delete this
            pthread_mutex_unlock(&cyclicBufferMutex);
            pthread_mutex_unlock(&clientListMutex);
            break;
        }

        pthread_mutex_unlock(&cyclicBufferMutex);
        pthread_mutex_unlock(&clientListMutex);

        pthread_cond_signal(&cyclicBufferEmptyCond);

        // nextClientInfoIndex++;

        // nextClientIp = curClientInfo->ipStruct.s_addr;
        // nextClientPortNum = curClientInfo->portNumber;
    }

    pthread_exit((void**)0);
}

void* workerThreadJob(void* id) {
    struct sigaction sigAction;
    // setup the sighub handler
    sigAction.sa_handler = &handleSigUsrSecondaryThread;

    // restart the system call, if at all possible
    sigAction.sa_flags = SA_RESTART;

    // add only SIGINT signal (SIGUSR1 and SIGUSR2 signals are handled by the client's forked subprocesses)
    sigemptyset(&sigAction.sa_mask);
    sigaddset(&sigAction.sa_mask, SIGUSR1);

    if (sigaction(SIGUSR1, &sigAction, NULL) == -1) {
        perror("Error: cannot handle SIGINT");  // Should not happen
    }

    while (1) {
        printf("in worker thread: while1\n");
        pthread_mutex_lock(&cyclicBufferMutex);
        printf("buffer size: %d\n", cyclicBuffer.curSize);

        while (cyclicBufferEmpty(&cyclicBuffer)) {
            pthread_cond_wait(&cyclicBufferEmptyCond, &cyclicBufferMutex);
        }
        printf("in worker thread: while2\n");

        // char cyclicBufferWasFull = cyclicBufferFull(&cyclicBuffer);
        // if (cyclicBufferFull(&cyclicBuffer)) {
        //     cyclicBufferWasFull = 1;
        // }

        BufferNode* curBufferNode = getNodeFromCyclicBuffer(&cyclicBuffer);
        printf("in worker thread with id %ld: while3 ========================================================\n", *(long int*)id);

        pthread_mutex_unlock(&cyclicBufferMutex);

        // if (cyclicBufferWasFull) {
        pthread_cond_signal(&cyclicBufferFullCond);
        // }
        printf("in worker thread: while4\n");

        pthread_mutex_lock(&clientListMutex);

        if (findNodeInList(clientsList, &curBufferNode->portNumber, &curBufferNode->ip) == NULL) {
            printErrorLn("Client info invalid\n");
            freeBufferNode(&curBufferNode);
            continue;
        }
        printf("in worker thread: while5\n");

        pthread_mutex_unlock(&clientListMutex);

        int curPeerSocketFd;
        struct sockaddr_in curPeerSocketAddr;
        curPeerSocketAddr.sin_family = AF_INET;
        curPeerSocketAddr.sin_port = curBufferNode->portNumber;
        curPeerSocketAddr.sin_addr.s_addr = curBufferNode->ip;

        short int filePathSize;
        time_t version;
        if (strcmp(curBufferNode->filePath, ",") == 0) {  // client buffer node
            printf("Worker thread hanlding client node\n");
            // pthread_mutex_lock(&cyclicBufferMutex);
            printf("ip: %s, port: %d\n", inet_ntoa(curPeerSocketAddr.sin_addr), curPeerSocketAddr.sin_port);
            if (connectToPeer(&curPeerSocketFd, &curPeerSocketAddr) == 1)
                pthread_exit((void**)1);  //                                    TODO: add thread exit function maybe??????????????????????
            printf("Worker thread hanlding client node1\n");
            // pthread_mutex_unlock(&cyclicBufferMutex);

            trySend(curPeerSocketFd, GET_FILE_LIST, MAX_MESSAGE_SIZE, SECONDARY_THREAD);

            char message[MAX_MESSAGE_SIZE];
            tryRead(curPeerSocketFd, message, MAX_MESSAGE_SIZE, SECONDARY_THREAD);
            printf("worker thread --> got message: %s\n", message);

            unsigned int totalFilesNum;
            tryRead(curPeerSocketFd, &totalFilesNum, 4, SECONDARY_THREAD);
            printf("worker thread --> total files num: %u\n", totalFilesNum);

            char* pathNoInputDirName;

            for (int i = 0; i < totalFilesNum; i++) {
                printf("worker thread i: %d\n", i);
                tryRead(curPeerSocketFd, &filePathSize, 2, SECONDARY_THREAD);
                printf("worker thread i: %d ------------------------------> file path size: %d\n", i, filePathSize);

                pathNoInputDirName = (char*)malloc(filePathSize + 1);
                memset(pathNoInputDirName, 0, filePathSize + 1);  // clear file path

                tryRead(curPeerSocketFd, pathNoInputDirName, filePathSize, SECONDARY_THREAD);
                printf("worker thread i: %d ------------------------------> path: %s\n", i, pathNoInputDirName);

                tryRead(curPeerSocketFd, &version, 8, SECONDARY_THREAD);
                printf("worker thread i: %d ------------------------------> version: %ld\n", i, version);

                pthread_mutex_lock(&cyclicBufferMutex);

                while (cyclicBufferFull(&cyclicBuffer)) {
                    pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
                }
                printf("worker thread i: %d, 4\n", i);

                // char cyclicBufferWasEmpty = cyclicBufferEmpty(&cyclicBuffer);

                addNodeToCyclicBuffer(&cyclicBuffer, pathNoInputDirName, version, curBufferNode->ip, curBufferNode->portNumber);
                printf("worker thread i: %d, 5\n", i);
                printf("buffer size while adding: %d\n", cyclicBuffer.curSize);

                pthread_mutex_unlock(&cyclicBufferMutex);
                // if (cyclicBufferWasEmpty) {
                pthread_cond_signal(&cyclicBufferEmptyCond);
                // }

                // free pathNoInputDirName
                if (pathNoInputDirName != NULL) {
                    free(pathNoInputDirName);
                    pathNoInputDirName = NULL;
                }
            }
            printf("worker thread got all files\n");
            // freeBufferNode(&curBufferNode);
            close(curPeerSocketFd);
        } else {  // file buffer node
            printf("Worker thread hanlding file nodegggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg\n");
            char clientName[MAX_CLIENT_NAME_SIZE];
            buildClientName(&clientName, curBufferNode->ip, curBufferNode->portNumber);

            char filePath[strlen(clientName) + strlen(curBufferNode->filePath) + 1];
            sprintf(filePath, "%s/%s", clientName, curBufferNode->filePath);

            if (connectToPeer(&curPeerSocketFd, &curPeerSocketAddr) == 1)
                pthread_exit((void**)-1);
            printf("Worker thread hanlding file node///////////////////////////////////////////////////\n");

            trySend(curPeerSocketFd, GET_FILE, MAX_MESSAGE_SIZE, SECONDARY_THREAD);
            printf("Worker thread hanlding file node///////////////////////////////////////////////////11111111\n");

            filePathSize = strlen(curBufferNode->filePath);
            trySend(curPeerSocketFd, &filePathSize, 2, SECONDARY_THREAD);
            trySend(curPeerSocketFd, curBufferNode->filePath, filePathSize, SECONDARY_THREAD);

            if (!fileExists(filePath)) {  // file does not exist locally
                version = -1;
            } else {
                version = curBufferNode->version;
                // trySendFromThread(curPeerSocketFd, &version, 8);
            }

            trySend(curPeerSocketFd, &version, 8, SECONDARY_THREAD);

            char incomingMessage[MAX_MESSAGE_SIZE];
            tryRead(curPeerSocketFd, incomingMessage, MAX_MESSAGE_SIZE, SECONDARY_THREAD);
            printf("worker thread .................................... message received: %s\n", incomingMessage);
            if (strcmp(incomingMessage, FILE_SIZE) == 0) {
                int incomingVersion;
                tryRead(curPeerSocketFd, &incomingVersion, 8, SECONDARY_THREAD);

                printf("cur buffer file path*************************************************: %s\n", curBufferNode->filePath);
                // File* foundFile = findNodeInList(filesList, curBufferNode->filePath, NULL);
                // if (foundFile == NULL) {
                //     printErrorLn("Something's wrong");
                // } else {
                //     foundFile->version = incomingVersion;
                // }

                int contentsSize;
                tryRead(curPeerSocketFd, &contentsSize, 4, SECONDARY_THREAD);

                if (contentsSize == -1) {  // is a directory
                    createDir(filePath);
                    close(curPeerSocketFd);
                    continue;
                } else if (contentsSize == 0) {  // is an empty file
                    char tempFilePath[strlen(filePath) + 1];
                    strcpy(tempFilePath, filePath);
                    removeFileName(tempFilePath);
                    // printf("file path: %s\n", tempFilePath);
                    if (!dirExists(tempFilePath)) {
                        createDir(tempFilePath);
                    }
                    printf("file path: %s\n", filePath);
                    createAndWriteToFile(filePath, "");
                    close(curPeerSocketFd);
                    continue;
                }

                // is a file with contents
                // fileContents: buffer in which all of the read contents of the file will be stored in order to eventually write them to a file
                // bytesRead: the bytes of file contents that are read from fifo pipe (for logging purposes)
                char contents[contentsSize + 1];
                int bytesRead = 0;
                if (contentsSize > 0) {
                    memset(contents, 0, contentsSize + 1);  // clear fileContents buffer
                    char chunk[FILE_CHUNK_SIZE + 1];        // chunk: a part of the file contents

                    // remainingContentsSize: the number of bytes that are remaining to be read from the fifo pipe
                    // tempBufferSize: temporary buffer size to read from pipe in each while loop
                    int remainingContentsSize = contentsSize;
                    int tempBufferSize;
                    while (remainingContentsSize > 0) {
                        memset(chunk, 0, FILE_CHUNK_SIZE + 1);  // clear chunk buffer

                        // if the remaining contents of file have size less than the buffer size, read only the remaining contents from fifo pipe
                        tempBufferSize = (remainingContentsSize < FILE_CHUNK_SIZE ? remainingContentsSize : FILE_CHUNK_SIZE);

                        tryRead(curPeerSocketFd, chunk, tempBufferSize, SECONDARY_THREAD);  // read a chunk of size tempBufferSize from pipe

                        bytesRead += tempBufferSize;              // for logging purposes
                        strcat(contents, chunk);                  // concatenated the read chunk to the total file content's buffer
                        remainingContentsSize -= tempBufferSize;  // proceed tempBufferSize bytes
                    }
                }

                // // write to log
                // fprintf(logFileP, "Reader with pid %d received file with path \"%s\" and read %d bytes from fifo pipe\n", getpid(), filePath, bytesRead);
                // fflush(logFileP);

                // char mirrorFilePath[strlen(mirrorIdDirPath) + strlen(filePath) + 2];  // mirrorFilePath: the path of the mirrored file
                // sprintf(mirrorFilePath, "%s/%s", mirrorIdDirPath, filePath);           // format: [mirrorDir]/[id]/[filePath]

                char tempFilePath[strlen(filePath) + 1];
                strcpy(tempFilePath, filePath);
                removeFileName(tempFilePath);
                // printf("file path: %s\n", tempFilePath);
                if (!dirExists(tempFilePath)) {
                    createDir(tempFilePath);
                }
                printf("file path: %s\n", filePath);

                createAndWriteToFile(filePath, contents);
            }
            // trySend(selectedSocket, FILE_SIZE, MAX_MESSAGE_SIZE);
            // trySend(selectedSocket, &curFile->version, 8);
            // trySend(selectedSocket, &curFile->contentsSize, 4);

            close(curPeerSocketFd);
        }
        freeBufferNode(&curBufferNode);
    }
    pthread_exit((void**)0);
}

int main(int argc, char** argv) {
    struct sigaction sigAction;

    // setup the sighub handler
    sigAction.sa_handler = &handleSigIntMainThread;

    // restart the system call, if at all possible
    sigAction.sa_flags = SA_RESTART;

    // add only SIGINT signal (SIGUSR1 and SIGUSR2 signals are handled by the client's forked subprocesses)
    sigemptyset(&sigAction.sa_mask);
    sigaddset(&sigAction.sa_mask, SIGINT);

    if (sigaction(SIGINT, &sigAction, NULL) == -1) {
        perror("Error: cannot handle SIGINT");  // Should not happen
    }

    char* dirName;
    int portNum, workerThreadsNum, bufferSize, serverPort, mySocketFd;
    struct sockaddr_in myAddr;

    handleArgs(argc, argv, &dirName, &portNum, &workerThreadsNum, &bufferSize, &serverPort, &serverAddr);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = serverPort;

    if (connectToPeer(&serverSocketFd, &serverAddr) == 1) {
        handleExit(1);
    }
    printLn("Connected to server");

    struct sockaddr_in localAddr;
    socklen_t localAddrLength = sizeof(localAddr);
    getsockname(serverSocketFd, (struct sockaddr*)&localAddr,
                &localAddrLength);
    printf("local address: %s\n", inet_ntoa(localAddr.sin_addr));

    // if (send(serverSocketFd, LOG_ON, MAX_MESSAGE_SIZE, 0) == -1) {  // send LOG_ON to server
    //     perror("Send error");
    //     handleExit(1);
    // }
    uint32_t myIpToSend = htonl(localAddr.sin_addr.s_addr);
    int myPortToSend = htons(portNum);

    trySend(serverSocketFd, LOG_ON, MAX_MESSAGE_SIZE, MAIN_THREAD);
    trySend(serverSocketFd, &myIpToSend, 4, MAIN_THREAD);
    // printLn("after read 1");

    trySend(serverSocketFd, &myPortToSend, 4, MAIN_THREAD);
    // if (send(serverSocketFd, GET_CLIENTS, MAX_MESSAGE_SIZE, 0) == -1) {
    //     perror("Send error");
    //     handleExit(1);
    // }

    tryInitAndSend(&serverSocketFd, GET_CLIENTS, MAX_MESSAGE_SIZE, MAIN_THREAD, &serverAddr, serverPort);

    char message[MAX_MESSAGE_SIZE];
    tryRead(serverSocketFd, message, MAX_MESSAGE_SIZE, MAIN_THREAD);

    if (strcmp(message, CLIENT_LIST)) {
        printErrorLn("Initial communication with server failed");
        handleExit(1);
    }

    int receivedPortNum;
    struct in_addr receivedIpStruct;
    unsigned int clientsNum;
    // char endOfComm = 0;

    clientsList = initList(CLIENTS);

    tryRead(serverSocketFd, &clientsNum, 4, MAIN_THREAD);

    // tryRead(serverSocketFd, &receivedIpStruct.s_addr, 4, MAIN_THREAD);

    // while (receivedIpStruct.s_addr != -1) {  // -1 ------------> end of initial communication with server
    //     tryRead(serverSocketFd, &receivedPortNum, 4, MAIN_THREAD);

    //     addNodeToList(clientsList, initClientInfo(receivedIpStruct, receivedPortNum));

    //     tryRead(serverSocketFd, &receivedIpStruct.s_addr, 4, MAIN_THREAD);
    // }

    printf("clients num: %d\n", clientsNum);
    for (int i = 0; i < clientsNum; i++) {
        tryRead(serverSocketFd, &receivedIpStruct.s_addr, 4, MAIN_THREAD);
        tryRead(serverSocketFd, &receivedPortNum, 4, MAIN_THREAD);

        receivedIpStruct.s_addr = ntohl(receivedIpStruct.s_addr);
        receivedPortNum = ntohs(receivedPortNum);

        printf("ip1: %s, port1: %d\n", inet_ntoa(receivedIpStruct), receivedPortNum);
        if (receivedIpStruct.s_addr != localAddr.sin_addr.s_addr || receivedPortNum != portNum)

            addNodeToList(clientsList, initClientInfo(receivedIpStruct, receivedPortNum));
    }

    printLn("Got clients from server");

    initBuffer(&cyclicBuffer, bufferSize);

    ////// store client infos in buffer
    int clientsInserted = 0;
    ClientInfo* curClientInfo = clientsList->firstNode;
    while (curClientInfo != NULL) {
        nextClientIp = curClientInfo->ipStruct.s_addr;
        nextClientPortNum = curClientInfo->portNumber;

        if (addNodeToCyclicBuffer(&cyclicBuffer, NULL, -1, curClientInfo->ipStruct.s_addr, curClientInfo->portNumber) == 1)
            break;

        clientsInserted++;
        // nextClientInfoIndex++;
        // nextClientIp = curClientInfo->ipStruct.s_addr;
        // nextClientPortNum = curClientInfo->portNumber;

        curClientInfo = curClientInfo->nextClientInfo;
    }

    printLn("Filled cyclic buffer");

    filesList = initList(FILES);
    populateFileList(filesList, dirName, 0);
    printf("1\n");
    if (clientsInserted < clientsList->size) {
        pthread_create(&bufferFillerThreadId, NULL, bufferFillerThreadJob, NULL);  // 3rd arg is the function which I will add, 4th arg is the void* arg of the function
        printLn("Created buffer filler thread cause clients were too much");
    }
    printf("2\n");

    // pthread_t threadIds[workerThreadsNum];
    threadIds = (pthread_t*)malloc(workerThreadsNum * sizeof(pthread_t));
    printf("worker threads num: %d\n", workerThreadsNum);
    for (int i = 0; i < workerThreadsNum; i++) {
        printf("i: %d\n", i);
        pthread_create(&threadIds[i], NULL, workerThreadJob, &threadIds[i]);  // 3rd arg is the function which I will add, 4th arg is the void* arg of the function
    }

    printLn("Created worker threads");

    // // Creating socket file descriptor
    // if ((mySocketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    //     perror("Socket creation error");
    //     handleExit(1);
    // }

    // myAddr.sin_family = AF_INET;
    // myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // myAddr.sin_port = htons(portNum);

    // if (bind(mySocketFd, (struct sockaddr*)&myAddr,
    //          sizeof(myAddr)) < 0) {
    //     perror("Bind error");
    //     handleExit(1);
    // }
    // if (listen(mySocketFd, MAX_CONNECTIONS) < 0) {
    //     perror("Listen error");
    //     handleExit(1);
    // }

    if (createServer(&mySocketFd, &myAddr, portNum, MAX_CONNECTIONS)) {
        handleExit(1);
    }

    printLn("Created server to accept incoming connections");

    int newSocketFd, selectedSocket;
    struct sockaddr_in incomingAddr;
    int incomingAddrLen = sizeof(incomingAddr);
    char keepConnection = 0;

    while (1) {
        printLn("Listening for incoming connections");

        if ((newSocketFd = accept(mySocketFd, (struct sockaddr*)&incomingAddr, (socklen_t*)&incomingAddrLen)) < 0) {
            perror("Accept error");
            handleExit(1);
        }

        // if ((selectedSocket = selectSocket(serverSocketFd, newSocketFd)) == -1) {
        //     handleExit(1);
        // }

        // if (selectedSocket == newSocketFd) {
        //     ClientInfo* foundClientInfo = findNodeInList(clientsList, &incomingAddr.sin_port, &incomingAddr.sin_addr.s_addr);
        //     if (foundClientInfo == NULL)  // ignore incoming client
        //         continue;
        // }
        selectedSocket = newSocketFd;
        // char* message = malloc(MAX_MESSAGE_SIZE);
        // memset(message, 0, MAX_MESSAGE_SIZE);
        tryRead(selectedSocket, message, MAX_MESSAGE_SIZE, MAIN_THREAD);

        if (strcmp(message, GET_FILE_LIST) == 0) {
            printLn("Got GET_FILE_LIST message");
            printf("111\n");

            trySend(selectedSocket, FILE_LIST, MAX_MESSAGE_SIZE, MAIN_THREAD);
            printf("111.5\n");

            trySend(selectedSocket, &filesList->size, 4, MAIN_THREAD);
            printf("will send file list size: %u\n", filesList->size);

            printf("222\n");

            File* curFile = filesList->firstNode;
            while (curFile != NULL) {
                char curFilePathCopy[strlen(curFile->path) + 1];
                strcpy(curFilePathCopy, curFile->path);  // temporarily store current file's path in order to manipulate it and get cut the input directory's name from it
                // pathNoInputDirName: current file's path without the input directory's name
                char* pathNoInputDirName = strtok(curFilePathCopy, "/");  // cut the input directory's name
                pathNoInputDirName = strtok(NULL, "\n");                  // until end of path

                short int filePathSize = strlen(pathNoInputDirName);
                printf("will send file path size: %d\n", filePathSize);
                trySend(selectedSocket, &filePathSize, 2, MAIN_THREAD);  // short int

                printf("will send path: %s\n", pathNoInputDirName);

                trySend(selectedSocket, pathNoInputDirName, filePathSize, MAIN_THREAD);
                printf("will send version: %ld\n", curFile->version);

                trySend(selectedSocket, &curFile->version, 8, MAIN_THREAD);

                curFile = curFile->nextFile;
            }
            printf("handled all files\n");
            close(selectedSocket);
        } else if (strcmp(message, GET_FILE) == 0) {
            printLn("Got GET_FILE message");
            char* curFilePathNoInputDirName;
            time_t curFileVersion;
            short int curFilePathSize;

            tryRead(selectedSocket, &curFilePathSize, 2, MAIN_THREAD);

            curFilePathNoInputDirName = (char*)malloc(curFilePathSize + 1);
            memset(curFilePathNoInputDirName, 0, curFilePathSize + 1);

            tryRead(selectedSocket, curFilePathNoInputDirName, curFilePathSize, MAIN_THREAD);
            tryRead(selectedSocket, &curFileVersion, 8, MAIN_THREAD);

            char curFilePath[strlen(dirName) + curFilePathSize];
            // strcpy(curFilePath, dirName);
            sprintf(curFilePath, "%s/%s", dirName, curFilePathNoInputDirName);

            if (curFilePathNoInputDirName != NULL) {
                free(curFilePathNoInputDirName);
                curFilePathNoInputDirName = NULL;
            }

            // if (!fileExists(curFilePath)) {
            //     trySend(newSocketFd, FILE_NOT_FOUND, MAX_MESSAGE_SIZE);
            //     continue;
            // }

            File* curFile = findNodeInList(filesList, curFilePath, NULL);

            if (curFile == NULL) {
                trySend(selectedSocket, FILE_NOT_FOUND, MAX_MESSAGE_SIZE, MAIN_THREAD);
                continue;
            }

            if (curFile->version == curFileVersion) {
                trySend(selectedSocket, FILE_UP_TO_DATE, MAX_MESSAGE_SIZE, MAIN_THREAD);
                continue;
            }

            trySend(selectedSocket, FILE_SIZE, MAX_MESSAGE_SIZE, MAIN_THREAD);

            trySend(selectedSocket, &curFile->version, 8, MAIN_THREAD);
            trySend(selectedSocket, &curFile->contentsSize, 4, MAIN_THREAD);

            int fd;

            if (curFile->contentsSize != -1) {  // is a regular file (not a directory)
                // open the original file
                fd = open(curFile->path, O_RDONLY | O_NONBLOCK);
                if (fd < 0) {
                    perror("open failed");
                    handleExit(1);
                }

                char curBuffer[FILE_CHUNK_SIZE + 1];
                // bytesWritten: the bytes of file contents that are written to fifo pipe (for logging purposes)
                // remainingContentsSize: the number of bytes that are remaining to be read from the file
                // tempBufferSize: temporary buffer size to read from file and write to fifo pipe in each while loop
                int bytesWritten = 0, remainingContentsSize, tempBufferSize = FILE_CHUNK_SIZE;

                // use the encrypted file's size or the original file's size according to the gpg encryption mode
                remainingContentsSize = curFile->contentsSize;

                // if the remaining contents of file have size less than the buffer size, read only the remaining contents from file to then write them to fifo pipe
                tempBufferSize = (remainingContentsSize < FILE_CHUNK_SIZE ? remainingContentsSize : FILE_CHUNK_SIZE);

                memset(curBuffer, 0, FILE_CHUNK_SIZE + 1);               // clear buffer
                int readRetValue = read(fd, curBuffer, tempBufferSize);  // read a chunk of the current file
                while (readRetValue > 0) {
                    if (readRetValue == -1) {
                        perror("read failed");
                        handleExit(1);
                    }

                    // tryWrite(fifoFd, buffer, tempBufferSize);  // write a chunk of the current file to fifo pipe

                    trySend(selectedSocket, curBuffer, tempBufferSize, MAIN_THREAD);

                    bytesWritten += tempBufferSize;           // for logging purposes
                    remainingContentsSize -= tempBufferSize;  // proceed tempBufferSize bytes

                    // if the remaining contents of file have size less than the buffer size, read only the remaining contents from file to then write them to fifo pipe
                    tempBufferSize = (remainingContentsSize < FILE_CHUNK_SIZE ? remainingContentsSize : FILE_CHUNK_SIZE);

                    memset(curBuffer, 0, FILE_CHUNK_SIZE + 1);           // clear buffer
                    readRetValue = read(fd, curBuffer, tempBufferSize);  // read a chunk of the current file
                }

                // // write to log
                // fprintf(logFileP, "Writer with pid %d sent file with path \"%s\" and wrote %d bytes to fifo pipe\n", getpid(), pathNoInputDirName, bytesWritten);
                // fflush(logFileP);

                // try to close current file
                if (close(fd) == -1) {
                    perror("Close failed");
                    handleExit(1);
                }
            }
            //  else {  // is an empty directory
            // write directory's contents size which is always -1 to fifo pipe and write to log

            // trySend(newSocketFd, &curFile->contentsSize, 4);

            // fprintf(logFileP, "Writer with pid %d wrote 4 bytes of metadata to fifo pipe\n", getpid());
            // fflush(logFileP);

            // // write to log
            // fprintf(logFileP, "Writer with pid %d sent file with path \"%s\" and wrote %d bytes to fifo pipe\n", getpid(), pathNoInputDirName, 0);
            // fflush(logFileP);
            // }

            // trySend(newSocketFd, )  // send whole file at once //////////////////////////////////////////////////////////////////////////// maybe change this to chunks
            close(selectedSocket);
        } else if (strcmp(message, USER_OFF) == 0) {
            printLn("Got USER_OFF message");
            int curIp, curPortNum;

            tryRead(selectedSocket, &curIp, 4, MAIN_THREAD);
            tryRead(selectedSocket, &curPortNum, 4, MAIN_THREAD);
            curIp = ntohl(curIp);
            curPortNum = ntohs(curPortNum);

            if (curIp != serverAddr.sin_addr.s_addr || curPortNum != serverPort) {
                printErrorLn("Got USER_OFF message from a non-server application\n");
                continue;
            }

            pthread_mutex_lock(&clientListMutex);
            deleteNodeFromList(clientsList, &curPortNum, &curIp);
            pthread_mutex_unlock(&clientListMutex);
        } else if (strcmp(message, USER_ON) == 0) {
            printLn("Got USER_ON message");
            int curPortNum;
            struct in_addr curIpStruct;
            tryRead(selectedSocket, &curIpStruct.s_addr, 4, MAIN_THREAD);
            tryRead(selectedSocket, &curPortNum, 4, MAIN_THREAD);
            curIpStruct.s_addr = curIpStruct.s_addr;
            curPortNum = curPortNum;

            pthread_mutex_lock(&clientListMutex);

            addNodeToList(clientsList, initClientInfo(curIpStruct, curPortNum));

            pthread_mutex_unlock(&clientListMutex);

            pthread_mutex_lock(&cyclicBufferMutex);

            while (cyclicBufferFull(&cyclicBuffer)) {
                pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
            }

            addNodeToCyclicBuffer(&cyclicBuffer, NULL, -1, curIpStruct.s_addr, curPortNum);

            pthread_mutex_unlock(&cyclicBufferMutex);

            pthread_cond_signal(&cyclicBufferEmptyCond);
        } else {
            printf("Got unknown message: %s\n", message);
            keepConnection = 1;
        }
    }

    // while messages are valid do what we want

    // while ()

    // if (tryRead(mySocketFd, ))

    // if (receivedIpStruct.s_addr == -1) {
    //     perror("Read error")
    // }
}