#include "client.h"

void handleExit(int exitNum) {
    exit(exitNum);
}

void handleArgs(int argc, char** argv, char** dirName, int* portNum, int* workerThreadsNum, int* bufferSize, int* serverPort, struct in_addr* serverIpStructP) {
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
        inet_aton(argv[12], serverIpStructP);
    } else {
        printErrorLn("Invalid arguments\nExiting...");
        handleExit(1);
    }

    return;
}

int main(int argc, char** argv) {
    char* dirName;
    int portNum, workerThreadsNum, bufferSize, serverPort;
    struct in_addr serverIpStruct;

    handleArgs(argc, argv, &dirName, &portNum, &workerThreadsNum, &bufferSize, &serverPort, &serverIpStruct);

    
}