#include "client.h"

void handleExit(int exitNum) {
    int retVal;
    close(mySocketFd);

    if (threadIds != NULL) {
        // add nodes to buffer that indicate threads' exit
        // when each thread will handle each of these nodes, it will exit
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
        // kill buffer filler thread
        pthread_cancel(bufferFillerThreadId);
    }
    if (bufferFillerThreadCreated) {
        // wait for buffer filler thread to exit
        pthread_join(bufferFillerThreadId, (void**)&retVal);
        if (retVal == 1) {
            printErrorLn("Buffer filler thread exited with an error");
        }
    }

    if (threadIds != NULL) {
        // wait for worker threads to exit
        for (int i = 0; i < workerThreadsNum; i++) {
            pthread_join(threadIds[i], (void**)&retVal);

            if (retVal == 1) {
                printf(ANSI_COLOR_RED "Worker thread with id %lu exited with an error" ANSI_COLOR_RESET "\n", threadIds[i]);
            }
        }
        // free thread ids array
        free(threadIds);
        threadIds = NULL;
    }
    // close server connection
    close(serverSocketFd);

    // connect to server to send LOG_OFF message
    if (connectToPeer(&serverSocketFd, &serverAddr) == 0) {
        tryWrite(serverSocketFd, LOG_OFF, MAX_MESSAGE_SIZE, MAIN_THREAD);  // send LOG_OFF to server
        uint32_t myIpToSend = htonl(localIpAddr.s_addr);
        int myPortToSend = htons(portNum);
        tryWrite(serverSocketFd, &myIpToSend, 4, MAIN_THREAD);
        tryWrite(serverSocketFd, &myPortToSend, 4, MAIN_THREAD);
    }

    // close server connection
    close(serverSocketFd);

    // free allocated memory
    freeBuffer(&cyclicBuffer);
    freeList(&clientsList);
    freeList(&filesList);

    printf("Exiting...\n");
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

void createAndWriteToFile(char* fileName, char* contents) {
    FILE* file = fopen(fileName, "w");  // overwrite if file exists
    if (file == NULL) {
        perror("fopen failed");
        pthread_exit((void**)1);
    }

    fprintf(file, "%s", contents);
    fflush(file);

    if (fclose(file) == EOF) {
        perror("fclose failed");
        pthread_exit((void**)1);
    }
    return;
}

void populateFileList(List* fileList, char* inputDirName, char* rootDirName) {
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

        if (!S_ISREG(curStat.st_mode)) {                                                 // is a directory
            populateFileList(fileList, path, rootDirName);                               // continue traversing directory
        } else {                                                                         // is a file
            addNodeToList(fileList, initFile(path, curStat.st_size, curStat.st_mtime));  // add a REGULAR_FILE node to FileList
        }
    }

    // add directory's path to list only if it is empty
    if (isEmpty == 1 && strcmp(inputDirName, rootDirName) != 0) {
        stat(inputDirName, &curStat);

        addNodeToList(fileList, initFile(inputDirName, -1, curStat.st_mtime));  // add a DIRECTORY node to FileList only if it is empty
    }

    closedir(dir);  // close current directory
}

void tryWrite(int socketFd, void* buffer, int bufferSize, CallingMode callingMode) {
    // write data to socket
    if (write(socketFd, buffer, bufferSize) == -1) {
        perror("Send error");
        // exit according to callingMode's value
        if (callingMode == MAIN_THREAD)
            handleExit(1);
        else
            pthread_exit((void**)1);
    }

    return;
}

int tryRead(int socketId, void* buffer, int bufferSize, CallingMode callingMode) {
    int returnValue, tempBufferSize = bufferSize, progress = 0;

    // read data from socket until all the bufferSize bytes are read
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

void buildClientName(char (*clientName)[], uint32_t ip, int portNum) {
    struct in_addr ipStruct;
    ipStruct.s_addr = ip;
    sprintf(*clientName, "%s_%d", inet_ntoa(ipStruct), portNum);

    return;
}

void handleSigIntMainThread(int signal) {
    if (signal != SIGINT) {
        printErrorLn("Caught wrong signal instead of SIGINT");
    }
    printf("Caught SIGINT\n");

    handleExit(0);
}

void* bufferFillerThreadJob(void* a) {
    ClientInfo* curClientInfo;

    while (1) {
        if (nextClientIp == -1)  // all clients were put into the cyclic buffer, so exit
            break;

        pthread_mutex_lock(&clientListMutex);

        curClientInfo = findNodeInList(clientsList, &nextClientPortNum, &nextClientIp);

        if (curClientInfo->nextClientInfo != NULL) {
            nextClientIp = curClientInfo->nextClientInfo->ipStruct.s_addr;
            nextClientPortNum = curClientInfo->nextClientInfo->portNumber;
        } else {
            nextClientIp = -1;  // last client list node
        }

        pthread_mutex_lock(&cyclicBufferMutex);
        while (cyclicBufferFull(&cyclicBuffer)) {
            pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
        }

        // add client node to cyclic buffer
        if (addNodeToCyclicBuffer(&cyclicBuffer, NULL, -1, curClientInfo->ipStruct.s_addr, curClientInfo->portNumber) == 1) {
            // won't happen
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

        // get node from cyclic buffer by copying its contents to curBufferNode
        BufferNode* curBufferNode = getNodeFromCyclicBuffer(&cyclicBuffer);

        pthread_mutex_unlock(&cyclicBufferMutex);
        pthread_cond_signal(&cyclicBufferFullCond);

        if (curBufferNode->portNumber != -1) {
            // check if client is in clients list
            pthread_mutex_lock(&clientListMutex);

            if (findNodeInList(clientsList, &curBufferNode->portNumber, &curBufferNode->ip) == NULL) {
                printErrorLn("Client info invalid\n");
                freeBufferNode(&curBufferNode);
                pthread_mutex_unlock(&clientListMutex);
                // client not in clients list, so stop handling this buffer node
                continue;
            }

            pthread_mutex_unlock(&clientListMutex);
        }

        int curPeerSocketFd;
        struct sockaddr_in curPeerSocketAddr;  // address struct of the client that corresponds to the current buffer node
        curPeerSocketAddr.sin_family = AF_INET;
        curPeerSocketAddr.sin_port = curBufferNode->portNumber;
        curPeerSocketAddr.sin_addr.s_addr = curBufferNode->ip;
        short int filePathSize;
        time_t version;
        if (strcmp(curBufferNode->filePath, ",") == 0) {  // client buffer node
            if (connectToPeer(&curPeerSocketFd, &curPeerSocketAddr) == 1)
                pthread_exit((void**)1);

            // send GET_FILE_LIST message
            tryWrite(curPeerSocketFd, GET_FILE_LIST, MAX_MESSAGE_SIZE, SECONDARY_THREAD);

            char message[MAX_MESSAGE_SIZE];
            tryRead(curPeerSocketFd, message, MAX_MESSAGE_SIZE, SECONDARY_THREAD);

            unsigned int totalFilesNum;
            tryRead(curPeerSocketFd, &totalFilesNum, 4, SECONDARY_THREAD);

            char* pathNoInputDirName = NULL;  // path that doesn't contain the input directory's name of the remote client

            // get info about each file
            for (int i = 0; i < totalFilesNum; i++) {
                // get file path's size
                tryRead(curPeerSocketFd, &filePathSize, 2, SECONDARY_THREAD);

                pathNoInputDirName = (char*)malloc(filePathSize + 1);
                memset(pathNoInputDirName, 0, filePathSize + 1);  // clear file path

                // get file path
                tryRead(curPeerSocketFd, pathNoInputDirName, filePathSize, SECONDARY_THREAD);

                // get version
                tryRead(curPeerSocketFd, &version, 8, SECONDARY_THREAD);

                pthread_mutex_lock(&cyclicBufferMutex);

                while (cyclicBufferFull(&cyclicBuffer)) {
                    pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
                }

                // add file buffer node
                addNodeToCyclicBuffer(&cyclicBuffer, pathNoInputDirName, version, curBufferNode->ip, curBufferNode->portNumber);

                pthread_mutex_unlock(&cyclicBufferMutex);
                pthread_cond_signal(&cyclicBufferEmptyCond);

                if (pathNoInputDirName != NULL) {
                    free(pathNoInputDirName);
                    pathNoInputDirName = NULL;
                }
            }
            // close connection with the current client
            close(curPeerSocketFd);
        } else if (strcmp(curBufferNode->filePath, ",,") != 0) {  // file buffer node
            char curClientName[MAX_CLIENT_NAME_SIZE];
            buildClientName(&curClientName, curBufferNode->ip, curBufferNode->portNumber);
            int filePathLen = strlen(clientName) + strlen(curClientName) + strlen(curBufferNode->filePath) + 3;  // length of full file path
            char filePath[filePathLen];
            sprintf(filePath, "%s/%s/%s", clientName, curClientName, curBufferNode->filePath);  // format: 100.100.100.100_15/100.100.100.101_13/rest_of_file_path

            if (connectToPeer(&curPeerSocketFd, &curPeerSocketAddr) == 1)
                pthread_exit((void**)1);

            // send GET_FILE message to current client
            tryWrite(curPeerSocketFd, GET_FILE, MAX_MESSAGE_SIZE, SECONDARY_THREAD);

            filePathSize = strlen(curBufferNode->filePath);
            // send file path size
            tryWrite(curPeerSocketFd, &filePathSize, 2, SECONDARY_THREAD);
            // send file path
            tryWrite(curPeerSocketFd, curBufferNode->filePath, filePathSize, SECONDARY_THREAD);

            if (!fileExists(filePath)) {  // file does not exist locally
                version = -1;
            } else {  // file exists locally, so send local version
                struct stat curStat;
                stat(filePath, &curStat);
                version = curStat.st_mtime;
            }

            // send version
            tryWrite(curPeerSocketFd, &version, 8, SECONDARY_THREAD);

            char incomingMessage[MAX_MESSAGE_SIZE];
            tryRead(curPeerSocketFd, incomingMessage, MAX_MESSAGE_SIZE, SECONDARY_THREAD);
            // printf("message received: %s\n", incomingMessage);
            if (strcmp(incomingMessage, FILE_SIZE) == 0) {  // if received message is FILE_SIZE, get the file and store it locally
                time_t incomingVersion;
                // get version
                tryRead(curPeerSocketFd, &incomingVersion, 8, SECONDARY_THREAD);

                int contentsSize;
                // get contents size
                tryRead(curPeerSocketFd, &contentsSize, 4, SECONDARY_THREAD);

                struct utimbuf newTimes;
                struct stat curStat;
                if (contentsSize == -1) {  // is a (empty) directory
                    _mkdir(filePath);
                    // update local version by changing the modification time of the file
                    stat(filePath, &curStat);
                    newTimes.actime = curStat.st_atime;
                    newTimes.modtime = incomingVersion;
                    utime(filePath, &newTimes);
                    close(curPeerSocketFd);
                    freeBufferNode(&curBufferNode);
                    continue;
                } else if (contentsSize == 0) {  // is an empty file
                    char tempFilePath[filePathLen];
                    strcpy(tempFilePath, filePath);
                    removeFileName(tempFilePath);
                    if (!dirExists(tempFilePath)) {
                        _mkdir(tempFilePath);
                    }
                    createAndWriteToFile(filePath, "");
                    // update local version by changing the modification time of the file
                    stat(filePath, &curStat);
                    newTimes.actime = curStat.st_atime;
                    newTimes.modtime = incomingVersion;
                    utime(filePath, &newTimes);
                    close(curPeerSocketFd);
                    freeBufferNode(&curBufferNode);
                    continue;
                }

                // is a file with contents
                // contents: buffer in which all of the read contents of the file will be stored in order to eventually write them to a file
                char contents[contentsSize + 1];
                if (contentsSize > 0) {
                    memset(contents, 0, contentsSize + 1);  // clear contents buffer
                    char chunk[FILE_CHUNK_SIZE + 1];        // chunk: a part of the file's contents

                    // remainingContentsSize: the number of bytes that are remaining to be read from socket
                    // tempBufferSize: temporary buffer size to read from socket in each while loop
                    int remainingContentsSize = contentsSize;
                    int tempBufferSize;
                    while (remainingContentsSize > 0) {
                        memset(chunk, 0, FILE_CHUNK_SIZE + 1);  // clear chunk buffer

                        // if the remaining contents have size less than the buffer size, read only the remaining contents from socket
                        tempBufferSize = (remainingContentsSize < FILE_CHUNK_SIZE ? remainingContentsSize : FILE_CHUNK_SIZE);

                        tryRead(curPeerSocketFd, chunk, tempBufferSize, SECONDARY_THREAD);  // read a chunk of size tempBufferSize from socket

                        strcat(contents, chunk);                  // concatenate the read chunk to the total file content's buffer
                        remainingContentsSize -= tempBufferSize;  // proceed tempBufferSize bytes
                    }
                }

                char tempFilePath[filePathLen];
                strcpy(tempFilePath, filePath);  // store file's path in a temporary variable for string manipulation
                removeFileName(tempFilePath);    // remove the file's name

                if (!dirExists(tempFilePath)) {
                    // create directory and parent directories if they don't exist
                    _mkdir(tempFilePath);
                }

                // create file and write contents
                createAndWriteToFile(filePath, contents);
                // update local version by changing the modification time of the file
                stat(filePath, &curStat);
                newTimes.actime = curStat.st_atime;
                newTimes.modtime = incomingVersion;
                utime(filePath, &newTimes);
            }
            // close connection with current client
            close(curPeerSocketFd);
        } else {  // buffer node indicates that the thread should exit, so exit
            freeBufferNode(&curBufferNode);
            pthread_exit((void**)0);
        }
        freeBufferNode(&curBufferNode);
    }
}

void handleIncomingMessage(int socketFd, char* message, char* dirName) {
    if (strcmp(message, GET_FILE_LIST) == 0) {
        printLn("Got GET_FILE_LIST message");

        tryWrite(socketFd, FILE_LIST, MAX_MESSAGE_SIZE, MAIN_THREAD);

        tryWrite(socketFd, &filesList->size, 4, MAIN_THREAD);

        // send all local files one by one
        File* curFile = filesList->firstNode;
        while (curFile != NULL) {
            char curFilePathCopy[strlen(curFile->path) + 1];
            strcpy(curFilePathCopy, curFile->path);                            // temporarily store current file's path in order to manipulate it and get cut the input directory's name from it
            char* pathNoInputDirName = curFilePathCopy + strlen(dirName) + 1;  // cut input directory's name

            printf("path no input dir: %s\n", pathNoInputDirName);
            short int filePathSize = strlen(pathNoInputDirName);
            // send file path's size
            tryWrite(socketFd, &filePathSize, 2, MAIN_THREAD);
            // send file path that doesn't contain the local input directory's name
            tryWrite(socketFd, pathNoInputDirName, filePathSize, MAIN_THREAD);
            // send local version
            tryWrite(socketFd, &curFile->version, 8, MAIN_THREAD);

            curFile = curFile->nextFile;
        }
    } else if (strcmp(message, GET_FILE) == 0) {
        printLn("Got GET_FILE message");
        char* curFilePathNoInputDirName = NULL;
        time_t curFileVersion;
        short int curFilePathSize;

        // get file path's size
        tryRead(socketFd, &curFilePathSize, 2, MAIN_THREAD);

        curFilePathNoInputDirName = (char*)malloc(curFilePathSize + 1);
        memset(curFilePathNoInputDirName, 0, curFilePathSize + 1);

        // get file path that doesn't contain the local input directory's name
        tryRead(socketFd, curFilePathNoInputDirName, curFilePathSize, MAIN_THREAD);
        // get remote file's version
        tryRead(socketFd, &curFileVersion, 8, MAIN_THREAD);

        // construct full local file path
        char curFilePath[strlen(dirName) + curFilePathSize];
        sprintf(curFilePath, "%s/%s", dirName, curFilePathNoInputDirName);

        if (curFilePathNoInputDirName != NULL) {
            free(curFilePathNoInputDirName);
            curFilePathNoInputDirName = NULL;
        }

        File* curFile = findNodeInList(filesList, curFilePath, NULL);

        if (curFile == NULL) {  // file does not exist locally
            tryWrite(socketFd, FILE_NOT_FOUND, MAX_MESSAGE_SIZE, MAIN_THREAD);
            return;
        }

        if (curFile->version == curFileVersion) {  // remote file is up to date with the corresponding local file
            tryWrite(socketFd, FILE_UP_TO_DATE, MAX_MESSAGE_SIZE, MAIN_THREAD);
            return;
        }

        // file is not up to date, so we should send file

        // send FILE_SIZE message
        tryWrite(socketFd, FILE_SIZE, MAX_MESSAGE_SIZE, MAIN_THREAD);

        // send local file's version
        tryWrite(socketFd, &curFile->version, 8, MAIN_THREAD);
        // send local file's contents' size
        tryWrite(socketFd, &curFile->contentsSize, 4, MAIN_THREAD);

        int fd;
        if (curFile->contentsSize != -1) {  // is a regular file (not a directory)
            // open local file
            fd = open(curFile->path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) {
                perror("open failed");
                handleExit(1);
            }

            char curBuffer[FILE_CHUNK_SIZE + 1];
            // remainingContentsSize: the number of bytes that are remaining to be read from socket
            // tempBufferSize: temporary buffer size to read from file and write to socket in each while loop
            int remainingContentsSize = curFile->contentsSize, tempBufferSize = FILE_CHUNK_SIZE;

            // if the remaining contents of file have size less than the buffer size, read only the remaining contents from file to then write them to socket
            tempBufferSize = (remainingContentsSize < FILE_CHUNK_SIZE ? remainingContentsSize : FILE_CHUNK_SIZE);

            memset(curBuffer, 0, FILE_CHUNK_SIZE + 1);               // clear buffer
            int readRetValue = read(fd, curBuffer, tempBufferSize);  // read a chunk of the current file
            while (readRetValue > 0) {
                if (readRetValue == -1) {
                    perror("read failed");
                    handleExit(1);
                }

                tryWrite(socketFd, curBuffer, tempBufferSize, MAIN_THREAD);

                remainingContentsSize -= tempBufferSize;  // proceed tempBufferSize bytes

                // if the remaining contents of file have size less than the buffer size, read only the remaining contents from file to then write them to socket
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

        // read client's ip and port
        tryRead(socketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);
        tryRead(socketFd, &curPortNum, 4, MAIN_THREAD);
        curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
        curPortNum = ntohs(curPortNum);
        printf("USER_OFF IP: %s, port: %d\n", inet_ntoa(curIpStruct), curPortNum);

        pthread_mutex_lock(&clientListMutex);
        deleteNodeFromList(clientsList, &curPortNum, &curIpStruct.s_addr);  // delete client from list
        pthread_mutex_unlock(&clientListMutex);
    } else if (strcmp(message, USER_ON) == 0) {
        printLn("Got USER_ON message");
        int curPortNum;
        struct in_addr curIpStruct;
        tryRead(socketFd, &curIpStruct.s_addr, 4, MAIN_THREAD);
        tryRead(socketFd, &curPortNum, 4, MAIN_THREAD);
        curIpStruct.s_addr = ntohl(curIpStruct.s_addr);
        curPortNum = ntohs(curPortNum);
        printf("USER_ON IP: %s, port: %d\n", inet_ntoa(curIpStruct), curPortNum);

        pthread_mutex_lock(&clientListMutex);
        addNodeToList(clientsList, initClientInfo(curIpStruct, curPortNum));  // add new client to list
        pthread_mutex_unlock(&clientListMutex);

        pthread_mutex_lock(&cyclicBufferMutex);
        while (cyclicBufferFull(&cyclicBuffer)) {
            pthread_cond_wait(&cyclicBufferFullCond, &cyclicBufferMutex);
        }

        addNodeToCyclicBuffer(&cyclicBuffer, NULL, -1, curIpStruct.s_addr, curPortNum);  // add new client buffer node to cyclic buffer

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

    // add only SIGINT signal
    sigemptyset(&sigAction.sa_mask);
    sigaddset(&sigAction.sa_mask, SIGINT);

    if (sigaction(SIGINT, &sigAction, NULL) == -1) {
        perror("Error: cannot handle SIGINT");  // should not happen
    }

    char* dirName;
    int bufferSize, serverPort;

    handleArgs(argc, argv, &dirName, &portNum, &workerThreadsNum, &bufferSize, &serverPort, &serverAddr);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = serverPort;

    // connect to server
    if (connectToPeer(&serverSocketFd, &serverAddr) == 1) {
        handleExit(1);
    }
    printLn("Connected to server");

    localIpAddr = getPrivateIp();
    printf("local address: %s\n", inet_ntoa(localIpAddr));

    // create this client's name string
    buildClientName(&clientName, localIpAddr.s_addr, portNum);

    uint32_t myIpToSend = htonl(localIpAddr.s_addr);
    int myPortToSend = htons(portNum);

    // send LOG_ON message
    tryWrite(serverSocketFd, LOG_ON, MAX_MESSAGE_SIZE, MAIN_THREAD);
    // send this client's private IP
    tryWrite(serverSocketFd, &myIpToSend, 4, MAIN_THREAD);
    // send this client's port number
    tryWrite(serverSocketFd, &myPortToSend, 4, MAIN_THREAD);

    // send GET_CLIENTS message
    tryWrite(serverSocketFd, GET_CLIENTS, MAX_MESSAGE_SIZE, MAIN_THREAD);

    char message[MAX_MESSAGE_SIZE];
    tryRead(serverSocketFd, message, MAX_MESSAGE_SIZE, MAIN_THREAD);

    // check received message
    if (strcmp(message, CLIENT_LIST)) {
        printErrorLn("Initial communication with server failed");
        handleExit(1);
    }

    int receivedPortNum;
    struct in_addr receivedIpStruct;
    unsigned int clientsNum;

    clientsList = initList(CLIENTS);

    // get clients number
    tryRead(serverSocketFd, &clientsNum, 4, MAIN_THREAD);

    // get all clients' information from server and store it locally
    for (int i = 0; i < clientsNum; i++) {
        // get client's ip
        tryRead(serverSocketFd, &receivedIpStruct.s_addr, 4, MAIN_THREAD);
        // get client's port number
        tryRead(serverSocketFd, &receivedPortNum, 4, MAIN_THREAD);

        receivedIpStruct.s_addr = ntohl(receivedIpStruct.s_addr);
        receivedPortNum = ntohs(receivedPortNum);

        // store only if it is not the same client as this client
        if (receivedIpStruct.s_addr != localIpAddr.s_addr || receivedPortNum != portNum) {
            addNodeToList(clientsList, initClientInfo(receivedIpStruct, receivedPortNum));
        }
    }
    // close connection with server
    close(serverSocketFd);

    printLn("Got clients from server");

    initBuffer(&cyclicBuffer, bufferSize);

    int clientsInserted = 0;
    // insert a client buffer node to cyclic buffer for each client in clients list
    ClientInfo* curClientInfo = clientsList->firstNode;
    while (curClientInfo != NULL) {
        nextClientIp = curClientInfo->ipStruct.s_addr;
        nextClientPortNum = curClientInfo->portNumber;

        // NULL value for file path will be converted to ","
        if (addNodeToCyclicBuffer(&cyclicBuffer, NULL, -1, curClientInfo->ipStruct.s_addr, curClientInfo->portNumber) == 1)
            // buffer full, so stop inserting nodes
            break;

        clientsInserted++;  // count the clients that are initially inserted to buffer

        curClientInfo = curClientInfo->nextClientInfo;
    }

    printLn("Filled cyclic buffer");

    filesList = initList(FILES);
    // add all local file's infomation to filesList
    populateFileList(filesList, dirName, dirName);

    // if not all clients were initially inserted into buffer, create a thread that continues filling the buffer with the rest of the clients
    if (clientsInserted < clientsList->size) {
        pthread_create(&bufferFillerThreadId, NULL, bufferFillerThreadJob, NULL);
        bufferFillerThreadCreated = 1;
        printLn("Created buffer filler thread because clients were too much");
    }

    // create all worker threads and store their ids to an array
    threadIds = (pthread_t*)malloc(workerThreadsNum * sizeof(pthread_t));
    for (int i = 0; i < workerThreadsNum; i++) {
        pthread_create(&threadIds[i], NULL, workerThreadJob, &i);  // 3rd arg is the function which I will add, 4th arg is the void* arg of the function
    }

    printLn("Created worker threads");

    struct sockaddr_in myAddr;  // myAddr: address struct that holds the information of the local server that will be created
    if (createServer(&mySocketFd, &myAddr, portNum, MAX_CONNECTIONS)) {
        handleExit(1);
    }

    printLn("Created server to accept incoming connections");

    int newSocketFd;                  // descriptor of the socket that is created to communicate with a new client
    struct sockaddr_in incomingAddr;  // incomingAddr: holds information for incoming connection
    int incomingAddrLen = sizeof(incomingAddr);
    fd_set activeSocketsSet, readSocketsSet;  // socket descriptor sets for selecting

    // initialize the set of active sockets
    FD_ZERO(&activeSocketsSet);
    FD_SET(mySocketFd, &activeSocketsSet);

    while (1) {
        printLn("Listening for incoming connections");

        // block until data arrives on one or more active sockets
        readSocketsSet = activeSocketsSet;
        if (select(FD_SETSIZE, &readSocketsSet, NULL, NULL, NULL) < 0) {
            perror("Select error");
            handleExit(1);
        }

        // service all the pending sockets
        for (int i = 0; i < FD_SETSIZE; ++i) {
            if (!FD_ISSET(i, &readSocketsSet))  // socket not ready
                continue;

            if (!(i == mySocketFd)) {  // already-connected socket
                // data arrived on an already connected socket
                if (tryRead(i, message, MAX_MESSAGE_SIZE, MAIN_THREAD) == 1) {
                    close(i);
                    FD_CLR(i, &activeSocketsSet);
                    continue;
                }
                printLn("Handling existing connection");

                handleIncomingMessage(i, message, dirName);
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