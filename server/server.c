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

int main(int argc, char** argv) {
    
}