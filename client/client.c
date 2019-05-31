#include "client.h"

void handleExit(int exitNum) {
    int retVal;
    close(mySocketFd);

    if (threadIds != NULL) {
    for (int i = 0; i < workerThreadsNum; i++) {
        pthread_mutex_lock(&cyclicBufferMutex);

        while (cyclicBufferFull(&cyclicBuffer)) {
             struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 1;
            pthread_cond_timedwait(&cyclicBufferFullCond, &cyclicBufferMutex, &timeout);
        }
        addNodeToCyclicBuffer(&cyclicBuffer, ",,", -1, -1, -1);
        pthread_mutex_unlock(&cyclicBufferMutex);
        pthread_cond_signal(&cyclicBufferEmptyCond);
    }
    }

    if (bufferFillerThreadCreated) {
        pthread_cancel(bufferFillerThreadId);
    }
    if (bufferFillerThreadCreated) {
        pthread_join(bufferFillerThreadId, (void**)&retVal);
        if (retVal == 1) {
            printErrorLn("Buffer filler thread exited with an error");
        }
    }

    if (threadIds != NULL) {
        for (int i = 0; i < workerThreadsNum; i++) {

            pthread_join(threadIds[i], (void**)&retVal);

            if (retVal == 1) {
                printf(ANSI_COLOR_RED "Worker thread with id %lu exited with an error" ANSI_COLOR_RESET "\n", threadIds[i]);
            }
        }
        free(threadIds);
        threadIds = NULL;
    }
    close(serverSocketFd);

    if (connectToPeer(&serverSocketFd, &serverAddr) == 0) {
        trySend(serverSocketFd, LOG_OFF, MAX_MESSAGE_SIZE, MAIN_THREAD);  // send LOG_OFF to server
        uint32_t myIpToSend = htonl(localIpAddr.s_addr);
        int myPortToSend = htons(portNum);
        trySend(serverSocketFd, &myIpToSend, 4, MAIN_THREAD);
        trySend(serverSocketFd, &myPortToSend, 4, MAIN_THREAD);
    }

    close(serverSocketFd);
    freeBufferNodes(&cyclicBuffer);
    freeList(&clientsList);
    freeList(&filesList);

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

void populateFileList(List* fileList, char* inputDirName, int indent, char* rootDirName) {
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
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        else
            isEmpty = 0;

        snprintf(path, PATH_MAX, "%s/%s", inputDirName, entry->d_name);  // append to inputDirName the current file's name
        stat(path, &curStat);

        if (!S_ISREG(curStat.st_mode)) {  // is a directory
            populateFileList(fileList, path, indent + 2, rootDirName);  // continue traversing directory
        } else {                                                        // is a file
            addNodeToList(fileList, initFile(path, curStat.st_size, curStat.st_mtime));  // add a REGULAR_FILE node to FileList
        }
    }

    if (isEmpty == 1 && strcmp(inputDirName, rootDirName) != 0) {
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

    if (returnValue == 0) {  // 0 = EOF which means that writer failed and closed the pipe early
        return 1;
    }

    return 0;
}

void buildClientName(char (*clientName)[], uint32_t ip, int portNum) {
    struct in_addr ipStruct;
    ipStruct.s_addr = ip;
    sprintf(*clientName, "%s_%d", inet_ntoa(ipStruct), portNum);

    return;
}

void handleSigIntMainThread(int signal) {
    if (signal != SIGINT) {
        printErrorLn("Client caught wrong signal instead of SIGINT");
    }
    printf("Client caught SIGINT\n");

    handleExit(1);
}

void* bufferFillerThreadJob(void* a) {
    ClientInfo* curClientInfo;

    while (1) {
        if (nextClientIp == -1) 
            break;
    
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
            // won't happen
            printErrorLn("Unexpected behavior"); 
            pthread_mutex_unlock(&cyclicBufferMutex);
            pthread_mutex_unlock(&clientListMutex);
            break;
        }

        pthread_mutex_unlock(&cyclicBufferMutex);
        pthread_mutex_unlock(&clientListMutex);
        pthread_cond_signal(&cyclicBufferEmptyCond);
    }

    pthread_exit((void**)0);
}

void* workerThreadJob(void* index) {
    while (1) {
        pthread_mutex_lock(&cyclicBufferMutex);

        while (cyclicBufferEmpty(&cyclicBuffer)) {
            pthread_cond_wait(&cyclicBufferEmptyCond, &cyclicBufferMutex);
        }

        BufferNode* curBufferNode = getNodeFromCyclicBuffer(&cyclicBuffer);

        pthread_mutex_unlock(&cyclicBufferMutex);
            pthread_cond_signal(&cyclicBufferFullCond);

        if (curBufferNode->portNumber != -1) {
            pthread_mutex_lock(&clientListMutex);

            if (findNodeInList(clientsList, &curBufferNode->portNumber, &curBufferNode->ip) == NULL) {
                printErrorLn("Client info invalid\n");
                freeBufferNode(&curBufferNode);
                continue;
            }

            pthread_mutex_unlock(&clientListMutex);
        }

        int curPeerSocketFd;
        struct sockaddr_in curPeerSocketAddr;
        curPeerSocketAddr.sin_family = AF_INET;
        curPeerSocketAddr.sin_port = curBufferNode->portNumber;
        curPeerSocketAddr.sin_addr.s_addr = curBufferNode->ip;
        short int filePathSize;
        time_t version;
        if (strcmp(curBufferNode->filePath, ",") == 0) {  // client buffer node
            if (connectToPeer(&curPeerSocketFd, &curPeerSocketAddr) == 1)
                pthread_exit((void**)1);  //                                    TODO: add thread exit function maybe??????????????????????

            trySend(curPeerSocketFd, GET_FILE_LIST, MAX_MESSAGE_SIZE, SECONDARY_THREAD);

            char message[MAX_MESSAGE_SIZE];
            tryRead(curPeerSocketFd, message, MAX_MESSAGE_SIZE, SECONDARY_THREAD);

            unsigned int totalFilesNum;
            tryRead(curPeerSocketFd, &totalFilesNum, 4, SECONDARY_THREAD);

            char* pathNoInputDirName = NULL;

            for (int i = 0; i < totalFilesNum; i++) {
                tryRead(curPeerSocketFd, &filePathSize, 2, SECONDARY_THREAD);

                pathNoInputDirName = (char*)malloc(filePathSize + 1);
                memset(pathNoInputDirName, 0, filePathSize + 1);  // clear file path

                tryRead(curPeerSocketFd, pathNoInputDirName, filePathSize, SECONDARY_THREAD);

                tryRead(curPeerSocketFd, &version, 8, SECONDARY_THREAD);

                pthread_mutex_lock(&cyclicBufferMutex);

                while (cyclicBufferFull(&cyclicBuffer)) {
                    pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
                }

                addNodeToCyclicBuffer(&cyclicBuffer, pathNoInputDirName, version, curBufferNode->ip, curBufferNode->portNumber);

                pthread_mutex_unlock(&cyclicBufferMutex);
                pthread_cond_signal(&cyclicBufferEmptyCond);

                if (pathNoInputDirName != NULL) {
                    free(pathNoInputDirName);
                    pathNoInputDirName = NULL;
                }
            }
            close(curPeerSocketFd);
        } else if (strcmp(curBufferNode->filePath, ",,") != 0) {  // file buffer node

            char curClientName[MAX_CLIENT_NAME_SIZE];
            buildClientName(&curClientName, curBufferNode->ip, curBufferNode->portNumber);
            int filePathLen = strlen(clientName) + strlen(curClientName) + strlen(curBufferNode->filePath) + 3;
            char filePath[filePathLen];
            sprintf(filePath, "%s/%s/%s", clientName, curClientName, curBufferNode->filePath);

            if (connectToPeer(&curPeerSocketFd, &curPeerSocketAddr) == 1)
                pthread_exit((void**)1);

            trySend(curPeerSocketFd, GET_FILE, MAX_MESSAGE_SIZE, SECONDARY_THREAD);

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
            if (strcmp(incomingMessage, FILE_SIZE) == 0) {
                int incomingVersion;
                tryRead(curPeerSocketFd, &incomingVersion, 8, SECONDARY_THREAD);

                int contentsSize;
                tryRead(curPeerSocketFd, &contentsSize, 4, SECONDARY_THREAD);

                if (contentsSize == -1) {  // is a directory
                    _mkdir(filePath);
                    close(curPeerSocketFd);
                    continue;
                } else if (contentsSize == 0) {  // is an empty file
                    char tempFilePath[filePathLen];
                    strcpy(tempFilePath, filePath);
                    removeFileName(tempFilePath);
                    if (!dirExists(tempFilePath)) {
                        _mkdir(tempFilePath);
                    }
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

                char tempFilePath[filePathLen];
                strcpy(tempFilePath, filePath);
                removeFileName(tempFilePath);

                if (!dirExists(tempFilePath)) {
                    _mkdir(tempFilePath);
                }

                createAndWriteToFile(filePath, contents);
            }
            close(curPeerSocketFd);
        } else {
            pthread_exit((void**)0);
        }
        freeBufferNode(&curBufferNode);
    }
    pthread_exit((void**)0);
}

void handleIncomingMessage(int socketFd, char* message, char* dirName) {
    if (strcmp(message, GET_FILE_LIST) == 0) {
        printLn("Got GET_FILE_LIST message");

        trySend(socketFd, FILE_LIST, MAX_MESSAGE_SIZE, MAIN_THREAD);

        trySend(socketFd, &filesList->size, 4, MAIN_THREAD);

        File* curFile = filesList->firstNode;
        while (curFile != NULL) {
            char curFilePathCopy[strlen(curFile->path) + 1];
            strcpy(curFilePathCopy, curFile->path);  // temporarily store current file's path in order to manipulate it and get cut the input directory's name from it
            char* pathNoInputDirName = strtok(curFilePathCopy, "/");  // cut input directory's name
            pathNoInputDirName = strtok(NULL, "\n");                  // until end of path

            short int filePathSize = strlen(pathNoInputDirName);
            trySend(socketFd, &filePathSize, 2, MAIN_THREAD);  // short int
            trySend(socketFd, pathNoInputDirName, filePathSize, MAIN_THREAD);
            trySend(socketFd, &curFile->version, 8, MAIN_THREAD);

            curFile = curFile->nextFile;
        }
    } else if (strcmp(message, GET_FILE) == 0) {
        printLn("Got GET_FILE message");
        char* curFilePathNoInputDirName = NULL;
        time_t curFileVersion;
        short int curFilePathSize;

        tryRead(socketFd, &curFilePathSize, 2, MAIN_THREAD);

        curFilePathNoInputDirName = (char*)malloc(curFilePathSize + 1);
        memset(curFilePathNoInputDirName, 0, curFilePathSize + 1);

        tryRead(socketFd, curFilePathNoInputDirName, curFilePathSize, MAIN_THREAD);
        tryRead(socketFd, &curFileVersion, 8, MAIN_THREAD);

        char curFilePath[strlen(dirName) + curFilePathSize];
        sprintf(curFilePath, "%s/%s", dirName, curFilePathNoInputDirName);

        if (curFilePathNoInputDirName != NULL) {
            free(curFilePathNoInputDirName);
            curFilePathNoInputDirName = NULL;
        }

        File* curFile = findNodeInList(filesList, curFilePath, NULL);

        if (curFile == NULL) {
            trySend(socketFd, FILE_NOT_FOUND, MAX_MESSAGE_SIZE, MAIN_THREAD);
            return;
        }

        if (curFile->version == curFileVersion) {
            trySend(socketFd, FILE_UP_TO_DATE, MAX_MESSAGE_SIZE, MAIN_THREAD);
            return;
        }

        trySend(socketFd, FILE_SIZE, MAX_MESSAGE_SIZE, MAIN_THREAD);

        trySend(socketFd, &curFile->version, 8, MAIN_THREAD);
        trySend(socketFd, &curFile->contentsSize, 4, MAIN_THREAD);

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

                trySend(socketFd, curBuffer, tempBufferSize, MAIN_THREAD);

                bytesWritten += tempBufferSize;           // for logging purposes
                remainingContentsSize -= tempBufferSize;  // proceed tempBufferSize bytes

                // if the remaining contents of file have size less than the buffer size, read only the remaining contents from file to then write them to fifo pipe
                tempBufferSize = (remainingContentsSize < FILE_CHUNK_SIZE ? remainingContentsSize : FILE_CHUNK_SIZE);

                memset(curBuffer, 0, FILE_CHUNK_SIZE + 1);           // clear buffer
                readRetValue = read(fd, curBuffer, tempBufferSize);  // read a chunk of the current file
            }

            // try to close current file
            if (close(fd) == -1) {
                perror("Close failed");
                handleExit(1);
            }
        }
    } else if (strcmp(message, USER_OFF) == 0) {
        printLn("Got USER_OFF message");
        int curPortNum;
        struct in_addr curIpStruct;

        tryRead(socketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);
        tryRead(socketFd, &curPortNum, 4, MAIN_THREAD);
        curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
        curPortNum = ntohs(curPortNum);

        pthread_mutex_lock(&clientListMutex);
        deleteNodeFromList(clientsList, &curPortNum, &curIpStruct.s_addr);
        pthread_mutex_unlock(&clientListMutex);
    } else if (strcmp(message, USER_ON) == 0) {
        printLn("Got USER_ON message");
        int curPortNum;
        struct in_addr curIpStruct;
        tryRead(socketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);
        tryRead(socketFd, &curPortNum, 4, MAIN_THREAD);
        curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
        curPortNum = ntohs(curPortNum);

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
    }
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
    int bufferSize, serverPort;
    struct sockaddr_in myAddr;

    handleArgs(argc, argv, &dirName, &portNum, &workerThreadsNum, &bufferSize, &serverPort, &serverAddr);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = serverPort;

    if (connectToPeer(&serverSocketFd, &serverAddr) == 1) {
        handleExit(1);
    }
    printLn("Connected to server");

    localIpAddr = getLocalIp();
    printf("local address: %s\n", inet_ntoa(localIpAddr));

    buildClientName(&clientName, localIpAddr.s_addr, portNum);

    uint32_t myIpToSend = htonl(localIpAddr.s_addr);
    int myPortToSend = htons(portNum);

    trySend(serverSocketFd, LOG_ON, MAX_MESSAGE_SIZE, MAIN_THREAD);
    trySend(serverSocketFd, &myIpToSend, 4, MAIN_THREAD);

    trySend(serverSocketFd, &myPortToSend, 4, MAIN_THREAD);

    if (connectToPeer(&serverSocketFd, &serverAddr) == 1)
        handleExit(1);

    trySend(serverSocketFd, GET_CLIENTS, MAX_MESSAGE_SIZE, MAIN_THREAD);

    char message[MAX_MESSAGE_SIZE];
    tryRead(serverSocketFd, message, MAX_MESSAGE_SIZE, MAIN_THREAD);

    if (strcmp(message, CLIENT_LIST)) {
        printErrorLn("Initial communication with server failed");
        handleExit(1);
    }

    int receivedPortNum;
    struct in_addr receivedIpStruct;
    unsigned int clientsNum;

    clientsList = initList(CLIENTS);

    tryRead(serverSocketFd, &clientsNum, 4, MAIN_THREAD);

    for (int i = 0; i < clientsNum; i++) {
        tryRead(serverSocketFd, &receivedIpStruct.s_addr, 4, MAIN_THREAD);
        tryRead(serverSocketFd, &receivedPortNum, 4, MAIN_THREAD);

        receivedIpStruct.s_addr = ntohl(receivedIpStruct.s_addr);
        receivedPortNum = ntohs(receivedPortNum);

        if (receivedIpStruct.s_addr != localIpAddr.s_addr || receivedPortNum != portNum)
            addNodeToList(clientsList, initClientInfo(receivedIpStruct, receivedPortNum));
    }
    close(serverSocketFd);

    printLn("Got clients from server");

    initBuffer(&cyclicBuffer, bufferSize);

    int clientsInserted = 0;
    ClientInfo* curClientInfo = clientsList->firstNode;
    while (curClientInfo != NULL) {
        nextClientIp = curClientInfo->ipStruct.s_addr;
        nextClientPortNum = curClientInfo->portNumber;

        if (addNodeToCyclicBuffer(&cyclicBuffer, NULL, -1, curClientInfo->ipStruct.s_addr, curClientInfo->portNumber) == 1)
            break;

        clientsInserted++;

        curClientInfo = curClientInfo->nextClientInfo;
    }

    printLn("Filled cyclic buffer");

    filesList = initList(FILES);
    populateFileList(filesList, dirName, 0, dirName);
    if (clientsInserted < clientsList->size) {
        pthread_create(&bufferFillerThreadId, NULL, bufferFillerThreadJob, NULL);
        bufferFillerThreadCreated = 1;
        printLn("Created buffer filler thread because clients were too much");
    }

    threadIds = (pthread_t*)malloc(workerThreadsNum * sizeof(pthread_t));
    for (int i = 0; i < workerThreadsNum; i++) {
        pthread_create(&threadIds[i], NULL, workerThreadJob, &i);  // 3rd arg is the function which I will add, 4th arg is the void* arg of the function
    }

    printLn("Created worker threads");

    if (createServer(&mySocketFd, &myAddr, portNum, MAX_CONNECTIONS)) {
        handleExit(1);
    }

    printLn("Created server to accept incoming connections");

    int newSocketFd;
    struct sockaddr_in incomingAddr;
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
                /* Data arriving on an already-connected socket. */
                if (tryRead(i, message, MAX_MESSAGE_SIZE, MAIN_THREAD) == 1) {
                    close(i);
                    FD_CLR(i, &activeSocketsSet);
                    continue;
                }
                printLn("Handling existing connection");

                printf("Read message: %s\n", message);
                handleIncomingMessage(i, message, dirName);
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