#ifndef LIST_H
#define LIST_H

#include "../utils/utils.h"

typedef enum ListMode {
    FILES,   // indicates a files list
    CLIENTS  // indicates a clients list
} ListMode;

typedef struct File {
    char* path;                        // path: path of the file
    off_t contentsSize;                // contentsSize: is -1 if the File node represents a directory
    time_t version;                    // version: timestamp which is the modified timestamp of the file
    struct File *nextFile, *prevFile;  // for list iteration
} File;

typedef struct ClientInfo {
    struct in_addr ipStruct;                             // ipStruct: struct that contains the ip address of current client
    int portNumber;                                      // portNumber: port number in which the current client is listening to
    struct ClientInfo *nextClientInfo, *prevClientInfo;  // for list iteration
} ClientInfo;

// generic list struct: its purpose is determined by mode
// List is sorted in ascending order by either the path of file or the port number of the client depending on mode's value
typedef struct List {
    ListMode mode;      // mode: can be FILES or CLIENTS
    void* firstNode;    // firstNode: first node of the list
    unsigned int size;  // size: current size of the list
} List;

// creates, initializes and returns a File node
File* initFile(char* path, off_t contentsSize, time_t timestamp);

// frees a File node
void freeFile(File** file);

// frees a Files' List recursively
void freeFileRec(File** file);

// creates, initializes and returns a ClientInfo node
ClientInfo* initClientInfo(struct in_addr ipStruct, int portNumber);

// frees a ClientInfo node
void freeClientInfo(ClientInfo** clientInfo);

// frees a ClientInfos' List recursively
void freeClientInfoRec(ClientInfo** clientInfo);

// creates, initializes and returns a generic list with the desired mode (FILES or CLIENTS)
List* initList(ListMode mode);

// frees list
void freeList(List** list);

// generic function that searches for a node in list
// there are 2 cases for the values of searchValue1 and searchValue2: 1. searchValue1 = path of a file (char*), searchValue2 = NULL
//                                                                    2. searchValue1 = pointer to an int port number, searchValue2 = pointer to uint32_t ip address
// returns the node that was found or NULL if the node wasn't found
void* findNodeInList(List* list, void* searchValue1, void* searchValue2);

// adds the pre-created nodeToInsert node to list
void* addNodeToList(List* list, void* nodeToInsert);

// generic function that searches for a node in list and deletes it
// there are 2 cases for the values of searchValue1 and searchValue2: 1. searchValue1 = path of a file (char*), searchValue2 = NULL
//                                                                    2. searchValue1 = pointer to an int port number, searchValue2 = pointer to uint32_t ip address
// returns 0 if the node was found and deleted
// returns -1 if the node wasn't found
int deleteNodeFromList(List* list, void* searchValue1, void* searchValue2);

#endif