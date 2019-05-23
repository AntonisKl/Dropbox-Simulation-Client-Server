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

    if ((dir = opendir(inputDirName)) == NULL) {
        perror("Could not open input directory");
        handleExit(1);
    }

    // traverse the whole directory recursively
    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(path, PATH_MAX, "%s/%s", inputDirName, entry->d_name);  // append to inputDirName the current file's name
        struct stat curStat;
        stat(path, &curStat);

        if (!S_ISREG(curStat.st_mode)) {                       // is a directory
            addNodeToList(fileList,initFile(path, -1, curStat.st_mtime));  // add a DIRECTORY node to FileList

            populateFileList(fileList, path, indent + 2);                      // continue traversing directory
        } else {                                                               // is a file
            addNodeToList(fileList, initFile(path, -1, curStat.st_size));  // add a REGULAR_FILE node to FileList
        }
    }

    closedir(dir);  // close current directory
}

void* workerThreadJob(void* a) {
}

int main(int argc, char** argv) {
    char* dirName;
    int portNum, workerThreadsNum, bufferSize, serverPort, serverSocketFd, mySocketFd;
    struct sockaddr_in serverAddr, myAddr;

    handleArgs(argc, argv, &dirName, &portNum, &workerThreadsNum, &bufferSize, &serverPort, &serverAddr);

    if ((serverSocketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        handleExit(1);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);

    if (connect(serverSocketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("\nSocket connection failed\n");
        handleExit(1);
    }

    if (send(serverSocketFd, LOG_ON, MAX_MESSAGE_SIZE, 0) == -1) {  // send LOG_ON to server
        perror("Send error");
        handleExit(1);
    }

    if (send(serverSocketFd, GET_CLIENTS, MAX_MESSAGE_SIZE, 0) == -1) {
        perror("Send error");
        handleExit(1);
    }

    int receivedPortNum;
    struct in_addr receivedIpStruct;
    // char endOfComm = 0;

    clientsList = initList(CLIENTS);
    populateFileList(clientsList, dirName, 0);

    tryRead(serverSocketFd, &receivedIpStruct.s_addr, sizeof(uint32_t));

    while (receivedIpStruct.s_addr != -1) {
        tryRead(serverSocketFd, &receivedPortNum, sizeof(int));

        addNodeToList(clientsList, initClientInfo(receivedIpStruct, receivedPortNum));

        tryRead(serverSocketFd, &receivedIpStruct.s_addr, sizeof(uint32_t));
    }

    buffer = malloc(bufferSize * sizeof(BufferNode));

    pthread_t threadIds[workerThreadsNum];

    for (int i = 0; i < workerThreadsNum; i++) {
        pthread_create(&threadIds[i], NULL, workerThreadJob, NULL);  // 3rd arg is the function which I will add, 4th arg is the void* arg of the function
    }

    // Creating socket file descriptor
    if ((mySocketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        handleExit(1);
    }

    // // Forcefully attaching socket to the port 8080
    // if (setsockopt(serverSocketFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
    //                                               &opt, sizeof(opt)))
    // {
    //     perror("setsockopt");
    //     exit(EXIT_FAILURE);
    // }

    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myAddr.sin_port = htons(portNum);

    if (bind(mySocketFd, (struct sockaddr*)&myAddr,
             sizeof(myAddr)) < 0) {
        perror("Bind error");
        handleExit(1);
    }
    if (listen(mySocketFd, MAX_CONNECTIONS) < 0) {
        perror("Listen error");
        handleExit(1);
    }

    int newSocket;
    int myAddrLen = sizeof(myAddr);
    if ((newSocket = accept(mySocketFd, (struct sockaddr*)&myAddr,
                            (socklen_t*)&myAddrLen)) < 0) {
        perror("Accept error");
        handleExit(1);
    }

    char message[MAX_MESSAGE_SIZE];
    while (1) {
        tryRead(mySocketFd, &message, MAX_MESSAGE_SIZE);
        
        if (strcmp(message, GET_FILE_LIST)) {
        } else if (strcmp(message, GET_FILE)) {
        } else if (strcmp(message, LOG_OFF)) {
        }
    }

    // while messages are valid do what we want

    // while ()

    // if (tryRead(mySocketFd, ))

    // if (receivedIpStruct.s_addr == -1) {
    //     perror("Read error")
    // }
}