#include "client.h"

void handleExit(int exitNum) {
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

            populateFileList(fileList, path, indent + 2);                  // continue traversing directory
        } else {                                                           // is a file
            addNodeToList(fileList, initFile(path, -1, curStat.st_size));  // add a REGULAR_FILE node to FileList
        }
    }

    if (isEmpty == 1) {
        // snprintf(path, PATH_MAX, "%s/%s", inputDirName, entry->d_name);  // append to inputDirName the current file's name
        stat(inputDirName, &curStat);

        addNodeToList(fileList, initFile(inputDirName, -1, curStat.st_mtime));  // add a DIRECTORY node to FileList only if it is empty
    }

    closedir(dir);  // close current directory
}

void initBuffer(int bufferSize) {
    cyclicBuffer.buffer = malloc(bufferSize * sizeof(BufferNode));
    cyclicBuffer.maxSize = bufferSize;
    cyclicBuffer.curSize = 0;
    cyclicBuffer.startIndex = 0;
    cyclicBuffer.endIndex = 0;

    return;
}

void trySend(int socketFd, void* buffer, int bufferSize) {
    if (send(socketFd, buffer, bufferSize, 0) == -1) {  // send LOG_ON to server
        perror("Send error");
        handleExit(1);
    }

    return;
}

BufferNode* initBufferNode(char* filePath, time_t version, uint32_t ip, int portNum) {
    // char filePath[MAX_PATH_SIZE];
    // time_t version;
    // struct in_addr ip;
    // int portNumber;
    BufferNode* bufferNode = (BufferNode*)malloc(sizeof(BufferNode));
    strcpy(bufferNode->filePath, filePath);
    bufferNode->version = version;
    bufferNode->ip = ip;
    bufferNode->portNumber = portNum;

    return bufferNode;
}

int cyclicBufferEmpty(CyclicBuffer* cyclicBuffer) {
    return cyclicBuffer->curSize == 0;
}

int cyclicBufferFull(CyclicBuffer* cyclicBuffer) {
    return cyclicBuffer->curSize == cyclicBuffer->maxSize;
}

int calculateBufferIndex(int index) {
    return index * sizeof(BufferNode);
}

int addNodeToCyclicBuffer(CyclicBuffer* cyclicBuffer, char* filePath, time_t version, uint32_t ip, int portNum) {
    if (cyclicBufferFull(cyclicBuffer))
        return 1;

    cyclicBuffer->endIndex += sizeof(BufferNode);
    // memcpy(cyclicBuffer->cyclicBuffer->buffer[cyclicBuffer->endIndex])
    cyclicBuffer->buffer[calculateBufferIndex(cyclicBuffer->endIndex)] = initBufferNode(filePath, version, ip, portNum);
    cyclicBuffer->curSize += sizeof(BufferNode);

    return 0;
}

BufferNode* getNodeFromCyclicBuffer(CyclicBuffer* cyclicBuffer) {
    if (cyclicBufferEmpty(cyclicBuffer))
        return NULL;

    BufferNode* bufferNodeToReturn = cyclicBuffer->buffer[calculateBufferIndex(cyclicBuffer->startIndex)];
    cyclicBuffer->startIndex += sizeof(BufferNode);
    cyclicBuffer->curSize -= sizeof(BufferNode);

    return bufferNodeToReturn;
}

void freeBufferNode(BufferNode** bufferNode) {
    free(*bufferNode);
    (*bufferNode) = NULL;

    return;
}

void* workerThreadJob(void* a) {
    while (1) {
        pthread_mutex_lock(&cyclicBufferMutex);

        while (cyclicBufferEmpty(&cyclicBuffer)) {
            pthread_cond_wait(&cyclicBufferEmptyCond, &cyclicBufferMutex);
        }

        // char cyclicBufferWasFull = cyclicBufferFull(&cyclicBuffer);
        // if (cyclicBufferFull(&cyclicBuffer)) {
        //     cyclicBufferWasFull = 1;
        // }

        BufferNode* curBufferNode = getNodeFromCyclicBuffer(&cyclicBuffer);

        pthread_mutex_unlock(&cyclicBufferMutex);

        // if (cyclicBufferWasFull) {
        pthread_cond_signal(&cyclicBufferFullCond);
        // }

        if (curBufferNode->filePath == NULL) {  // client buffer node
            pthread_mutex_lock(&clientListMutex);

            if (findNodeInList(clientsList, curBufferNode->portNumber, curBufferNode->ip) == NULL) {
                pthread_mutex_unlock(&clientListMutex);
                printErrorLn("Client info invalid\n");
                freeBufferNode(&curBufferNode);
                continue;
            }

            pthread_mutex_unlock(&clientListMutex);

            int curPeerSocketFd;
            struct sockaddr_in curPeerSocketAddr;

            curPeerSocketAddr.sin_family = AF_INET;
            curPeerSocketAddr.sin_port = ntohs(curBufferNode->portNumber);
            curPeerSocketAddr.sin_addr.s_addr = ntohs(curBufferNode->ip);

            if (connectToPeer(curPeerSocketFd, &curPeerSocketAddr) == 1)
                handleExit(1);  //                                    TODO: add thread exit function maybe??????????????????????

            trySend(curPeerSocketFd, GET_FILE_LIST, MAX_MESSAGE_SIZE);

            unsigned int totalFilesNum;
            tryRead(curPeerSocketFd, &totalFilesNum, 4);

            short int filePathSize;
            char pathNoInputDirName;
            time_t version;

            for (int i = 0; i < totalFilesNum; i++) {
                tryRead(curPeerSocketFd, &filePathSize, 2);

                pathNoInputDirName = (char*)malloc(filePathSize + 1);
                tryRead(curPeerSocketFd, &pathNoInputDirName, filePathSize);

                tryRead(curPeerSocketFd, &version, 8);

                pthread_mutex_lock(&cyclicBufferMutex);

                if (cyclicBufferFull(&cyclicBuffer)) {
                    pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
                }
                // char cyclicBufferWasEmpty = cyclicBufferEmpty(&cyclicBuffer);

                addNodeToCyclicBuffer(&cyclicBuffer, pathNoInputDirName, version, curBufferNode->ip, curBufferNode->portNumber);

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

            freeBufferNode(&curBufferNode);
            close(curPeerSocketFd);
        } else {  // file buffer node
        }
    }
}

int main(int argc, char** argv) {
    char* dirName;
    int portNum, workerThreadsNum, bufferSize, serverPort, serverSocketFd, mySocketFd;
    struct sockaddr_in serverAddr, myAddr;

    handleArgs(argc, argv, &dirName, &portNum, &workerThreadsNum, &bufferSize, &serverPort, &serverAddr);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = serverPort;

    if (connectToPeer(&serverSocketFd, &serverAddr) == 1) {
        handleExit(1);
    }

    // if (send(serverSocketFd, LOG_ON, MAX_MESSAGE_SIZE, 0) == -1) {  // send LOG_ON to server
    //     perror("Send error");
    //     handleExit(1);
    // }

    trySend(serverSocketFd, LOG_ON, MAX_MESSAGE_SIZE);

    // if (send(serverSocketFd, GET_CLIENTS, MAX_MESSAGE_SIZE, 0) == -1) {
    //     perror("Send error");
    //     handleExit(1);
    // }

    trySend(serverSocketFd, GET_CLIENTS, MAX_MESSAGE_SIZE);

    int receivedPortNum;
    struct in_addr receivedIpStruct;
    // char endOfComm = 0;

    filesList = initList(FILES);
    populateFileList(filesList, dirName, 0);

    clientsList = initList(CLIENTS);

    tryRead(serverSocketFd, &receivedIpStruct.s_addr, sizeof(uint32_t));

    while (receivedIpStruct.s_addr != -1) {  // -1 ------------> end of initial communication with server
        tryRead(serverSocketFd, &receivedPortNum, 4);

        addNodeToList(clientsList, initClientInfo(receivedIpStruct, receivedPortNum));

        tryRead(serverSocketFd, &receivedIpStruct.s_addr, 4);
    }

    initBuffer(bufferSize);

    int nextClientInfoIndex = 0;
    ////// store client infos in buffer
    ClientInfo* curClientInfo = clientsList->firstNode;
    while (curClientInfo != NULL) {
        if (addNodeToCyclicBuffer(&cyclicBuffer, NULL, -1, curClientInfo->ipStruct.s_addr, curClientInfo->portNumber) == 1) {
            break;
        }
        nextClientInfoIndex++;

        curClientInfo = curClientInfo->nextClientInfo;
    }

    pthread_t threadIds[workerThreadsNum];

    for (int i = 0; i < workerThreadsNum; i++) {
        pthread_create(&threadIds[i], NULL, workerThreadJob, NULL);  // 3rd arg is the function which I will add, 4th arg is the void* arg of the function
    }

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

    int newSocketFd, selectedSocket;
    struct sockaddr_in incomingAddr;
    int incomingAddrLen = sizeof(incomingAddr);

    char message[MAX_MESSAGE_SIZE];
    while (1) {
        if ((newSocketFd = accept(mySocketFd, (struct sockaddr*)&incomingAddr, (socklen_t*)&incomingAddrLen)) < 0) {
            perror("Accept error");
            handleExit(1);
        }

        if ((selectedSocket = selectSocket(serverSocketFd, newSocketFd)) == -1) {
            handleExit(1);
        }

        if (selectedSocket == newSocketFd) {
            ClientInfo* foundClientInfo = findNodeInList(clientsList, &incomingAddr.sin_port, &incomingAddr.sin_addr.s_addr);
            if (foundClientInfo == NULL)  // ignore incoming client
                continue;
        }

        tryRead(selectedSocket, &message, MAX_MESSAGE_SIZE);

        if (strcmp(message, GET_FILE_LIST)) {
            trySend(selectedSocket, FILE_LIST, MAX_MESSAGE_SIZE);
            trySend(selectedSocket, filesList->size, 4);

            File* curFile = filesList->firstNode;
            while (curFile != NULL) {
                char curFilePathCopy[strlen(curFile->path) + 1];
                strcpy(curFilePathCopy, curFile->path);  // temporarily store current file's path in order to manipulate it and get cut the input directory's name from it
                // pathNoInputDirName: current file's path without the input directory's name
                char* pathNoInputDirName = strtok(curFilePathCopy, "/");  // cut the input directory's name
                pathNoInputDirName = strtok(NULL, "\n");                  // until end of path

                short int filePathSize = strlen(pathNoInputDirName);

                trySend(selectedSocket, &filePathSize, 2);  // short int

                trySend(selectedSocket, pathNoInputDirName, filePathSize);

                trySend(selectedSocket, curFile->version, sizeof(long int));

                curFile = curFile->nextFile;
            }

            close(selectedSocket);
        } else if (strcmp(message, GET_FILE)) {
            char* curFilePathNoInputDirName;
            time_t curFileVersion;
            short int curFilePathSize;

            tryRead(selectedSocket, &curFilePathSize, 2);
            tryRead(selectedSocket, &curFilePathNoInputDirName, curFilePathSize);
            tryRead(selectedSocket, &curFileVersion, 8);

            char curFilePath[strlen(dirName) + curFilePathSize];
            // strcpy(curFilePath, dirName);
            sprintf(curFilePath, "%s/%s", dirName, curFilePathNoInputDirName);

            // if (!fileExists(curFilePath)) {
            //     trySend(newSocketFd, FILE_NOT_FOUND, MAX_MESSAGE_SIZE);
            //     continue;
            // }

            File* curFile = findNodeInList(filesList, curFilePath, NULL);

            if (curFile == NULL) {
                trySend(selectedSocket, FILE_NOT_FOUND, MAX_MESSAGE_SIZE);
                continue;
            }

            if (curFile->version == curFileVersion) {
                trySend(selectedSocket, FILE_UP_TO_DATE, MAX_MESSAGE_SIZE);
                continue;
            }

            trySend(selectedSocket, FILE_SIZE, MAX_MESSAGE_SIZE);

            trySend(selectedSocket, &curFile->version, 8);
            trySend(selectedSocket, &curFile->contentsSize, 4);

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

                    trySend(selectedSocket, curBuffer, tempBufferSize);

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
        } else if (strcmp(message, LOG_OFF)) {
            int curIp, curPortNum;
            if (incomingAddr.sin_addr.s_addr != serverAddr.sin_addr.s_addr || incomingAddr.sin_port != serverPort) {
                printf("Got LOG_ON message from a non-server application\n");
                continue;
            }

            tryRead(selectedSocket, &curIp, 4);
            tryRead(selectedSocket, &curPortNum, 4);

            deleteNodeFromList(clientsList, &curPortNum, &curIp);
        }
    }

    // while messages are valid do what we want

    // while ()

    // if (tryRead(mySocketFd, ))

    // if (receivedIpStruct.s_addr == -1) {
    //     perror("Read error")
    // }
}