#ifndef LIST_H
#define LIST_H

#include "../utils/utils.h"

// typedef enum FileType {
//     DIRECTORY,
//     REGULAR_FILE
// } FileType;

typedef enum ListMode {
    FILES,
    CLIENTS
} ListMode;

typedef struct File {
    char* path;
    off_t contentsSize;
    time_t version;
    // FileType type;
    struct File *nextFile, *prevFile;
} File;

typedef struct ClientInfo {
    struct in_addr ipStruct;
    int portNumber;
    struct ClientInfo *nextClientInfo, *prevClientInfo;
} ClientInfo;

// List is sorted in ascending order by either the path of file or the port number of the client depending on mode's value
typedef struct List {
    ListMode mode;
    void* firstNode;
    unsigned int size;
} List;

File* initFile(char* path, off_t contentsSize, time_t timestamp);

void freeFile(File** file);

void freeFileRec(File** file);

ClientInfo* initClientInfo(struct in_addr ipStruct, int portNumber);

void freeClientInfo(ClientInfo** clientInfo);

void freeClientInfoRec(ClientInfo** clientInfo);

List* initList(ListMode mode);

void freeList(List** list);

void* findNodeInList(List* list, void* searchValue1, void* searchValue2);

void* addNodeToList(List* list, void* nodeToInsert);

int deleteNodeFromList(List* list, void* searchValue1, void* searchValue2);

#endif