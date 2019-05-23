#include "utils.h"

void printErrorLn(char* s) {
    printf(ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", s);

    return;
}

void tryRead(int socketId, void* buffer, int bufferSize) {
    int returnValue, tempBufferSize = bufferSize, progress = 0;

    // trySelect(fd);
    returnValue = read(socketId, buffer, tempBufferSize);
    while (returnValue < tempBufferSize && returnValue != 0) {  // rare case
        // not all desired bytes were read, so read the remaining bytes and add them to buffer
        if (returnValue == -1) {
            perror("Read error");
            return;
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

// int trySend(int socketId, char* buffer, int bufferSize) {
//     if (send(socketId, buffer, bufferSize, 0) == -1) {
//         perror("Send error");
//         return -1;
//     }
//     return 0;
// }

// void tryWrite(int fd, const void* buffer, int bufferSize) {
//     int returnValue, tempBufferSize = bufferSize, progress = 0;

//     returnValue = write(fd, buffer, tempBufferSize);
//     while (returnValue < tempBufferSize && returnValue != 0) {  // rare case
//         // not all desired bytes were written, so write the remaining bytes of buffer
//         if (returnValue == -1) {
//             perror("write error");
//             handleExit(1, SIGUSR2);
//         }

//         tempBufferSize -= returnValue;
//         progress += returnValue;
//         returnValue = write(fd, buffer + progress, tempBufferSize); // write remaining bytes that aren't written yet
//     }

//     return;
// }