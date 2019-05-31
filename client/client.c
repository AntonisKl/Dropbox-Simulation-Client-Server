#include "client.h"

void handleExit(int exitNum) {
    int retVal;
    // if (bufferFillerThreadCreated) {
    //     // pthread_kill(bufferFillerThreadId, SIGUSR1);
    // }
    // for (int i = 0; i < workerThreadsNum; i++) {
    //     printf("handleExit: i=%d\n", i);
    //     // pthread_kill(threadIds[i], SIGUSR1);
    //     pthread_cancel(threadIds[i]);
    // }
    close(mySocketFd);

    if (threadIds != NULL) {
    for (int i = 0; i < workerThreadsNum; i++) {
        // if (threadsRunning == NULL || !threadsRunning[i])
        //     continue;

        printf("i: %d\n", i);
        pthread_mutex_lock(&cyclicBufferMutex);

        while (cyclicBufferFull(&cyclicBuffer)) {
             struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 1;
            pthread_cond_timedwait(&cyclicBufferFullCond, &cyclicBufferMutex, &timeout);
        }
        // printf("worker thread i: %d, 4\n", i);

        // char cyclicBufferWasEmpty = cyclicBufferEmpty(&cyclicBuffer);

        addNodeToCyclicBuffer(&cyclicBuffer, ",,", -1, -1, -1);
        // printf("worker thread i: %d, 5\n", i);
        // printf("buffer size while adding: %d\n", cyclicBuffer.curSize);

        pthread_mutex_unlock(&cyclicBufferMutex);
        // if (cyclicBufferWasEmpty) {
        pthread_cond_signal(&cyclicBufferEmptyCond);
    }
    }

    if (bufferFillerThreadCreated) {
        // pthread_kill(bufferFillerThreadId, SIGUSR1);
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
            // if (!threadsRunning[i])
            //     continue;
            printf("2 handleExit: i=%d\n", i);

            pthread_join(threadIds[i], (void**)&retVal);
            printf("2 handleExit: i=%d\n", i);

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
        printf("will send ip: %s, port: %d\n", inet_ntoa(localIpAddr), myPortToSend);
        trySend(serverSocketFd, &myIpToSend, 4, MAIN_THREAD);
        // printLn("after read 1");

        trySend(serverSocketFd, &myPortToSend, 4, MAIN_THREAD);
    }

    //     socklen_t localAddrLength = sizeof(localAddr);
    // getsockname(serverSocketFd, (struct sockaddr*)&localAddr,
    //                 &localAddrLength);

    // struct in_addr myIpStructToSend = getLocalIp();

    // free(threadIds);
    // threadIds = NULL;

    close(serverSocketFd);

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
            // printf("dir: %s\n", path);
            populateFileList(fileList, path, indent + 2, rootDirName);  // continue traversing directory
        } else {                                                        // is a file
            // printf("file: %s\n", path);
            addNodeToList(fileList, initFile(path, curStat.st_size, curStat.st_mtime));  // add a REGULAR_FILE node to FileList
        }
    }

    if (isEmpty == 1 && strcmp(inputDirName, rootDirName) != 0) {
        // printf("he\n");
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

// void tryInitAndSend(int* socketFd, void* buffer, int bufferSize, CallingMode callingMode, struct sockaddr_in* addr, int portNum) {
//     printf("haaaaaaaaaaaaaaaaaaaaaaaaaaaiiiii\n");

//     addr->sin_family = AF_INET;
//     addr->sin_port = portNum;
//     if (connectToPeer(socketFd, addr) == 1) {
//         if (callingMode == MAIN_THREAD)
//             handleExit(1);
//         else
//             pthread_exit((void**)1);
//     }

//     if (send(*socketFd, buffer, bufferSize, 0) == -1) {  // send LOG_ON to server
//         perror("Send error");
//         if (callingMode == MAIN_THREAD)
//             handleExit(1);
//         else
//             pthread_exit((void**)1);
//     }

//     return;
// }

int tryRead(int socketId, void* buffer, int bufferSize, CallingMode callingMode) {
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

    if (returnValue == 0) {  // 0 = EOF which means that writer failed and closed the pipe early
        return 1;
    }

    return 0;
}

void buildClientName(char (*clientName)[], uint32_t ip, int portNum) {
    // char ipS[MAX_STRING_INT_SIZE], portNumS[MAX_STRING_INT_SIZE];
    // sprintf(ipS, "%u", ip);
    // sprintf(portNumS, "%d", portNum);
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

// void handleSigUsrSecondaryThread(int signal) {
//     if (signal != SIGUSR1) {
//         printErrorLn("Secondary thread caught wrong signal instead of SIGUSR1");
//     }
//     printf("Secondary thread caught SIGUSR1\n");

//     pthread_exit(NULL);
// }

void* bufferFillerThreadJob(void* a) {
    printLn("buffer filler job started!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    // struct sigaction sigAction;
    // // setup the sighub handler
    // sigAction.sa_handler = &handleSigUsrSecondaryThread;

    // // restart the system call, if at all possible
    // sigAction.sa_flags = SA_RESTART;

    // // add only SIGINT signal (SIGUSR1 and SIGUSR2 signals are handled by the client's forked subprocesses)
    // sigemptyset(&sigAction.sa_mask);
    // sigaddset(&sigAction.sa_mask, SIGUSR1);

    // if (sigaction(SIGUSR1, &sigAction, NULL) == -1) {
    //     perror("Error: cannot handle SIGINT");  // Should not happen
    // }

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

        // clock_gettime(CLOCK_REALTIME, &timeoutTime);
        printf("buffer filler thread will lock client list's mutex\n");
        pthread_mutex_lock(&clientListMutex);

        curClientInfo = findNodeInList(clientsList, &nextClientPortNum, &nextClientIp);

        if (curClientInfo->nextClientInfo != NULL) {
            nextClientIp = curClientInfo->nextClientInfo->ipStruct.s_addr;
            nextClientPortNum = curClientInfo->nextClientInfo->portNumber;
        } else {
            nextClientIp = -1;
        }
        printf("buffer filler thread will lock cyclicBufferMutex\n");

        pthread_mutex_lock(&cyclicBufferMutex);
        while (cyclicBufferFull(&cyclicBuffer)) {
                    printf("buffer filler thread waiting on full condition\n");

            pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
        }
        if (addNodeToCyclicBuffer(&cyclicBuffer, NULL, -1, curClientInfo->ipStruct.s_addr, curClientInfo->portNumber) == 1) {
            printErrorLn("Something's wrong\n");  ///////////////////////////////////////////////////////////////////////////////// should delete this
            pthread_mutex_unlock(&cyclicBufferMutex);
            pthread_mutex_unlock(&clientListMutex);
            break;
        }
                    printf("buffer filler thread will unlock cyclicBufferMutex\n");

        pthread_mutex_unlock(&cyclicBufferMutex);
                            printf("buffer filler thread will unlock clientListMutex\n");

        pthread_mutex_unlock(&clientListMutex);

                            printf("buffer filler thread will signal cyclicBufferEmptyCond\n");

        pthread_cond_signal(&cyclicBufferEmptyCond);

        // nextClientInfoIndex++;

        // nextClientIp = curClientInfo->ipStruct.s_addr;
        // nextClientPortNum = curClientInfo->portNumber;
    }

    pthread_exit((void**)0);
}

void* workerThreadJob(void* index) {
    // int oldState;
    // pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&oldState);
    // struct sigaction sigAction;
    // // setup the sighub handler
    // sigAction.sa_handler = &handleSigUsrSecondaryThread;

    // // restart the system call, if at all possible
    // sigAction.sa_flags = SA_RESTART;

    // // add only SIGINT signal (SIGUSR1 and SIGUSR2 signals are handled by the client's forked subprocesses)
    // sigemptyset(&sigAction.sa_mask);
    // sigaddset(&sigAction.sa_mask, SIGUSR1);

    // if (sigaction(SIGUSR1, &sigAction, NULL) == -1) {
    //     perror("Error: cannot handle SIGINT");  // Should not happen
    // }

    // threadsRunning[*(int*)index] = 1;

    while (1) {
        printf("in worker thread: while1\n");
        // struct timespec timeout;
        // clock_gettime(CLOCK_REALTIME, &timeout);
        // timeout.tv_sec += 10;
        pthread_mutex_lock(&cyclicBufferMutex);
        // printf("timeouttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt->%ld\n", timeout.tv_sec);
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
        // printf("in worker thread with id %ld: while3 ========================================================\n", *(long int*)id);

        pthread_mutex_unlock(&cyclicBufferMutex);
            pthread_cond_signal(&cyclicBufferFullCond);

        if (curBufferNode->portNumber != -1) {
            // if (cyclicBufferWasFull) {
            // pthread_cond_signal(&cyclicBufferFullCond);
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
        }

        int curPeerSocketFd;
        struct sockaddr_in curPeerSocketAddr;
        curPeerSocketAddr.sin_family = AF_INET;
        curPeerSocketAddr.sin_port = curBufferNode->portNumber;
        curPeerSocketAddr.sin_addr.s_addr = curBufferNode->ip;
        printf("haaaaaaaaaaaaaaaaaaaaaaaaa file path: %s\n", curBufferNode->filePath);
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

            char* pathNoInputDirName = NULL;

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
                printf("hiiiiiiiiiii\n");
                // if (cyclicBufferWasEmpty) {
                pthread_cond_signal(&cyclicBufferEmptyCond);
                // }
                printf("after cond signal\n");

                if (pathNoInputDirName != NULL) {
                    free(pathNoInputDirName);
                    pathNoInputDirName = NULL;
                }
            }
            printf("worker thread got all files\n");
            // freeBufferNode(&curBufferNode);
            close(curPeerSocketFd);
        } else if (strcmp(curBufferNode->filePath, ",,") != 0) {  // file buffer node
            printf("Worker thread hanlding file nodegggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg\n");

            char curClientName[MAX_CLIENT_NAME_SIZE];
            buildClientName(&curClientName, curBufferNode->ip, curBufferNode->portNumber);
            int filePathLen = strlen(clientName) + strlen(curClientName) + strlen(curBufferNode->filePath) + 3;
            printf("file path length: %d\n", filePathLen);
            char filePath[filePathLen];
            sprintf(filePath, "%s/%s/%s", clientName, curClientName, curBufferNode->filePath);
            printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++= filePath: |%s|\n", filePath);

            if (connectToPeer(&curPeerSocketFd, &curPeerSocketAddr) == 1)
                pthread_exit((void**)1);
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
            printf("worker thread .................................... with id %ld message received: %s\n", pthread_self(), incomingMessage);
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
                    _mkdir(filePath);
                    close(curPeerSocketFd);
                    continue;
                } else if (contentsSize == 0) {  // is an empty file
                    char tempFilePath[filePathLen];
                    strcpy(tempFilePath, filePath);
                    removeFileName(tempFilePath);
                    // printf("file path: %s\n", tempFilePath);
                    if (!dirExists(tempFilePath)) {
                        _mkdir(tempFilePath);
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
                printf("file path before remove file name1: %s\n", filePath);

                char tempFilePath[filePathLen];
                printf("file path before remove file name: %s\n", filePath);

                strcpy(tempFilePath, filePath);

                removeFileName(tempFilePath);

                // printf("file path: %s\n", tempFilePath);
                if (!dirExists(tempFilePath)) {
                    _mkdir(tempFilePath);
                }
                printf("file path: %s\n", filePath);

                createAndWriteToFile(filePath, contents);
            }
            // trySend(selectedSocket, FILE_SIZE, MAX_MESSAGE_SIZE);
            // trySend(selectedSocket, &curFile->version, 8);
            // trySend(selectedSocket, &curFile->contentsSize, 4);

            close(curPeerSocketFd);
        } else {
            printf("----------------------------------> thread with id %ld\n", pthread_self());
            // pthread_mutex_lock(&exitedMutex);
            // threadsExited++;
            // printf("threads exited: %d\n", threadsExited);
            // pthread_mutex_unlock(&exitedMutex);
            pthread_exit((void**)0);
        }
        freeBufferNode(&curBufferNode);
    }
    pthread_exit((void**)0);
}

void handleIncomingMessage(int socketFd, char* message, char* dirName) {
    if (strcmp(message, GET_FILE_LIST) == 0) {
        printLn("Got GET_FILE_LIST message");
       // printf("111\n");

        trySend(socketFd, FILE_LIST, MAX_MESSAGE_SIZE, MAIN_THREAD);
       // printf("111.5\n");

        trySend(socketFd, &filesList->size, 4, MAIN_THREAD);
       // printf("will send file list size: %u\n", filesList->size);

       // printf("222\n");
        File* curFile = filesList->firstNode;
        while (curFile != NULL) {
            char curFilePathCopy[strlen(curFile->path) + 1];
            strcpy(curFilePathCopy, curFile->path);  // temporarily store current file's path in order to manipulate it and get cut the input directory's name from it
            // pathNoInputDirName: current file's path without the input directory's name
            char* pathNoInputDirName = strtok(curFilePathCopy, "/");  // cut the input directory's name
            pathNoInputDirName = strtok(NULL, "\n");                  // until end of path

            short int filePathSize = strlen(pathNoInputDirName);
            printf("will send file path size: %d\n", filePathSize);
            trySend(socketFd, &filePathSize, 2, MAIN_THREAD);  // short int

            printf("will send path: %s\n", pathNoInputDirName);

            trySend(socketFd, pathNoInputDirName, filePathSize, MAIN_THREAD);
            printf("will send version: %ld\n", curFile->version);

            trySend(socketFd, &curFile->version, 8, MAIN_THREAD);

            curFile = curFile->nextFile;
        }
        printf("handled all files\n");
        // close(selectedSocket);
    } else if (strcmp(message, GET_FILE) == 0) {
        printLn("Got GET_FILE message");
        char* curFilePathNoInputDirName;
        time_t curFileVersion;
        short int curFilePathSize;

        tryRead(socketFd, &curFilePathSize, 2, MAIN_THREAD);

        curFilePathNoInputDirName = (char*)malloc(curFilePathSize + 1);
        memset(curFilePathNoInputDirName, 0, curFilePathSize + 1);

        tryRead(socketFd, curFilePathNoInputDirName, curFilePathSize, MAIN_THREAD);
        tryRead(socketFd, &curFileVersion, 8, MAIN_THREAD);

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

                // tryWrite(fifoFd, buffer, tempBufferSize);  // write a chunk of the current file to fifo pipe

                trySend(socketFd, curBuffer, tempBufferSize, MAIN_THREAD);

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
        // close(selectedSocket);
    } else if (strcmp(message, USER_OFF) == 0) {
        printLn("Got USER_OFF message");
        int curPortNum;
        struct in_addr curIpStruct;

        tryRead(socketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);
        tryRead(socketFd, &curPortNum, 4, MAIN_THREAD);
        // curIp = ntohl(curIp);
        // curPortNum = ntohs(curPortNum);
        curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
        curPortNum = ntohs(curPortNum);
        printf("ip: %s, port: %d\n", inet_ntoa(curIpStruct), curPortNum);

        // if (curIp != serverAddr.sin_addr.s_addr || curPortNum != serverPort) {
        //     printErrorLn("Got USER_OFF message from a non-server application\n");
        //     continue;
        // }

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
        printf("ip: %s, port: %d\n", inet_ntoa(curIpStruct), curPortNum);

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
        // keepConnection = 1;
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

    // socklen_t localAddrLength = sizeof(localAddr);
    localIpAddr = getLocalIp();
    printf("local address: %s\n", inet_ntoa(localIpAddr));

    buildClientName(&clientName, localIpAddr.s_addr, portNum);

    // if (send(serverSocketFd, LOG_ON, MAX_MESSAGE_SIZE, 0) == -1) {  // send LOG_ON to server
    //     perror("Send error");
    //     handleExit(1);
    // }
    uint32_t myIpToSend = htonl(localIpAddr.s_addr);
    int myPortToSend = htons(portNum);

    trySend(serverSocketFd, LOG_ON, MAX_MESSAGE_SIZE, MAIN_THREAD);
    trySend(serverSocketFd, &myIpToSend, 4, MAIN_THREAD);
    // printLn("after read 1");

    trySend(serverSocketFd, &myPortToSend, 4, MAIN_THREAD);
    // if (send(serverSocketFd, GET_CLIENTS, MAX_MESSAGE_SIZE, 0) == -1) {
    //     perror("Send error");
    //     handleExit(1);
    // }

    // serverAddr.sin_family = AF_INET;
    // serverAddr.sin_port = portNum;
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
        if (receivedIpStruct.s_addr != localIpAddr.s_addr || receivedPortNum != portNum)
            addNodeToList(clientsList, initClientInfo(receivedIpStruct, receivedPortNum));
    }
    close(serverSocketFd);
    // close(serverSocketFd);

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
    populateFileList(filesList, dirName, 0, dirName);
   // printf("1\n");
    if (clientsInserted < clientsList->size) {
        pthread_create(&bufferFillerThreadId, NULL, bufferFillerThreadJob, NULL);  // 3rd arg is the function which I will add, 4th arg is the void* arg of the function
        bufferFillerThreadCreated = 1;
        printLn("Created buffer filler thread cause clients were too much");
    }
   // printf("2\n");

    // pthread_t threadIds[workerThreadsNum];
    threadIds = (pthread_t*)malloc(workerThreadsNum * sizeof(pthread_t));
    // threadsRunning = (char*)malloc(workerThreadsNum * sizeof(char));
    printf("worker threads num: %d\n", workerThreadsNum);
    for (int i = 0; i < workerThreadsNum; i++) {
       // printf("i: %d\n", i);
        pthread_create(&threadIds[i], NULL, workerThreadJob, &i);  // 3rd arg is the function which I will add, 4th arg is the void* arg of the function
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

    int newSocketFd;
    struct sockaddr_in incomingAddr;
    int incomingAddrLen = sizeof(incomingAddr);
    // char keepConnection = 0;
    fd_set activeSocketsSet, readSocketsSet;
    /* Initialize the set of active sockets. */
    FD_ZERO(&activeSocketsSet);
    FD_SET(mySocketFd, &activeSocketsSet);

    while (1) {
        printLn("Listening for incoming connections");

        // if ((newSocketFd = accept(mySocketFd, (struct sockaddr*)&incomingAddr, (socklen_t*)&incomingAddrLen)) < 0) {
        //     perror("Accept error");
        //     handleExit(1);
        // }

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

            // char message[MAX_MESSAGE_SIZE];
            // if (tryRead(i, message, MAX_MESSAGE_SIZE, MAIN_THREAD) == 1) {
            //     close(i);
            //     FD_CLR(i, &activeSocketsSet);
            // }
            // printf("Read message: %s\n", message);
            // handleIncomingMessage(clientsList, message);

            FD_SET(newSocketFd, &activeSocketsSet);
        }

        // if ((selectedSocket = selectSocket(serverSocketFd, newSocketFd)) == -1) {
        //     handleExit(1);
        // }

        // if (selectedSocket == newSocketFd) {
        //     ClientInfo* foundClientInfo = findNodeInList(clientsList, &incomingAddr.sin_port, &incomingAddr.sin_addr.s_addr);
        //     if (foundClientInfo == NULL)  // ignore incoming client
        //         continue;
        // }
        // selectedSocket = newSocketFd;
        // char* message = malloc(MAX_MESSAGE_SIZE);
        // memset(message, 0, MAX_MESSAGE_SIZE);
        // tryRead(selectedSocket, message, MAX_MESSAGE_SIZE, MAIN_THREAD);
    }

    // while messages are valid do what we want

    // while ()

    // if (tryRead(mySocketFd, ))

    // if (receivedIpStruct.s_addr == -1) {
    //     perror("Read error")
    // }
}