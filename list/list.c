#include "list.h"

// start of File functions

File* initFile(char* path, off_t contentsSize, time_t timestamp) {
    File* file = (File*)malloc(sizeof(File));

    file->path = (char*)malloc(strlen(path) + 1);
    strcpy(file->path, path);

    file->contentsSize = contentsSize;  // -1 for directory
    file->version = timestamp;

    file->nextFile = NULL;
    file->prevFile = NULL;

    return file;
}

void freeFile(File** file) {
    if ((*file) == NULL)
        return;

    free((*file)->path);
    (*file)->path = NULL;

    (*file)->nextFile = NULL;
    (*file)->prevFile = NULL;

    free(*file);
    (*file) = NULL;
}

void freeFileRec(File** file) {
    if ((*file) == NULL)
        return;

    freeFileRec(&((*file)->nextFile));

    freeFile(&(*file));

    return;
}

// end of File functions

// start of ClientInfo functions

ClientInfo* initClientInfo(struct in_addr ipStruct, int portNumber) {
    ClientInfo* clientInfo = (ClientInfo*)malloc(sizeof(ClientInfo));

    clientInfo->ipStruct = ipStruct;
    clientInfo->portNumber = portNumber;

    clientInfo->nextClientInfo = NULL;
    clientInfo->prevClientInfo = NULL;

    return clientInfo;
}

void freeClientInfo(ClientInfo** clientInfo) {
    if ((*clientInfo) == NULL)
        return;

    (*clientInfo)->nextClientInfo = NULL;
    (*clientInfo)->prevClientInfo = NULL;

    free(*clientInfo);
    (*clientInfo) = NULL;
}

void freeClientInfoRec(ClientInfo** clientInfo) {
    if ((*clientInfo) == NULL)
        return;

    freeClientInfoRec(&((*clientInfo)->nextClientInfo));

    freeClientInfo(&(*clientInfo));

    return;
}

// end of ClientInfo functions

// start of generic List functions

List* initList(ListMode mode) {
    List* list = (List*)malloc(sizeof(List));
    list->size = 0;
    list->firstNode = NULL;
    list->mode = mode;

    return list;
}

void freeList(List** list) {
    if ((*list) == NULL)
        return;

    // use the appropriate function according to mode's value
    if ((*list)->mode == FILES) {
        freeFileRec((File**)&(*list)->firstNode);
    } else if ((*list)->mode == CLIENTS) {
        freeClientInfoRec((ClientInfo**)&(*list)->firstNode);
    }

    free(*list);
    (*list) = NULL;

    return;
}

void* findNodeInList(List* list, void* searchValue1, void* searchValue2) {
    if (list == NULL)
        return NULL;

    void* curNode = list->firstNode;

    while (curNode != NULL) {
        // do the appropriate comparison according to mode's value
        if (list->mode == FILES ? strcmp(((File*)curNode)->path, (char*)searchValue1) == 0 : ((ClientInfo*)curNode)->portNumber == *((int*)searchValue1) && (((ClientInfo*)curNode)->ipStruct).s_addr == *(uint32_t*)searchValue2) {
            return curNode;
        } else if (list->mode == FILES ? strcmp((char*)searchValue1, ((File*)curNode)->path) < 0 : *((int*)searchValue1) < ((ClientInfo*)curNode)->portNumber) {
            // no need for searching further since the list is sorted by path or port number
            return NULL;
        }
        curNode = list->mode == FILES ? (void*)((File*)curNode)->nextFile : (void*)((ClientInfo*)curNode)->nextClientInfo;
    }
    return NULL;
}

void* addNodeToList(List* list, void* nodeToInsert) {
    if (list == NULL)
        return NULL;

    void* foundNode = list->mode == FILES ? findNodeInList(list, ((File*)nodeToInsert)->path, NULL) : findNodeInList(list, &((ClientInfo*)nodeToInsert)->portNumber, &((ClientInfo*)nodeToInsert)->ipStruct.s_addr);

    if (foundNode != NULL)  // is duplicate
        return NULL;        // do not add duplicate

    if (list->size == 0) {
        list->firstNode = nodeToInsert;

        list->size++;
        return list->firstNode;
    } else {
        void* curNode = list->firstNode;

        if (list->mode == FILES ? strcmp(((File*)nodeToInsert)->path, ((File*)curNode)->path) < 0 : ((ClientInfo*)nodeToInsert)->portNumber < ((ClientInfo*)curNode)->portNumber) {
            // insert at start
            if (list->mode == FILES) {
                ((File*)nodeToInsert)->nextFile = (File*)curNode;
                ((File*)curNode)->prevFile = (File*)nodeToInsert;
            } else {
                ((ClientInfo*)nodeToInsert)->nextClientInfo = (ClientInfo*)curNode;
                ((ClientInfo*)curNode)->prevClientInfo = (ClientInfo*)nodeToInsert;
            }

            list->firstNode = nodeToInsert;
            list->size++;
            return list->firstNode;
        }
        while (curNode != NULL) {
            if (list->mode == FILES ? ((File*)curNode)->nextFile != NULL : ((ClientInfo*)curNode)->nextClientInfo != NULL) {
                if (list->mode == FILES ? strcmp(((File*)nodeToInsert)->path, ((File*)curNode)->nextFile->path) < 0 : ((ClientInfo*)nodeToInsert)->portNumber < ((ClientInfo*)curNode)->nextClientInfo->portNumber) {
                    // insert somewhere in the middle
                    if (list->mode == FILES) {
                        ((File*)nodeToInsert)->prevFile = (File*)curNode;
                        ((File*)nodeToInsert)->nextFile = ((File*)curNode)->nextFile;
                        ((File*)curNode)->nextFile->prevFile = (File*)nodeToInsert;
                        ((File*)curNode)->nextFile = (File*)nodeToInsert;
                    } else {
                        ((ClientInfo*)nodeToInsert)->prevClientInfo = (ClientInfo*)curNode;
                        ((ClientInfo*)nodeToInsert)->nextClientInfo = ((ClientInfo*)curNode)->nextClientInfo;
                        ((ClientInfo*)curNode)->nextClientInfo->prevClientInfo = (ClientInfo*)nodeToInsert;
                        ((ClientInfo*)curNode)->nextClientInfo = (ClientInfo*)nodeToInsert;
                    }
                    list->size++;
                    return list->mode == FILES ? (void*)((File*)curNode)->nextFile : (void*)((ClientInfo*)curNode)->nextClientInfo;
                }
            } else {
                // insert at the end
                if (list->mode == FILES) {
                    ((File*)curNode)->nextFile = (File*)nodeToInsert;
                    ((File*)curNode)->nextFile->prevFile = ((File*)curNode);
                } else {
                    ((ClientInfo*)curNode)->nextClientInfo = (ClientInfo*)nodeToInsert;
                    ((ClientInfo*)curNode)->nextClientInfo->prevClientInfo = ((ClientInfo*)curNode);
                }
                list->size++;
                return list->mode == FILES ? (void*)((File*)curNode)->nextFile : (void*)((ClientInfo*)curNode)->nextClientInfo;
            }

            curNode = list->mode == FILES ? (void*)((File*)curNode)->nextFile : (void*)((ClientInfo*)curNode)->nextClientInfo;
        }
    }

    return NULL;  // not normal behavior
}

int deleteNodeFromList(List* list, void* searchValue1, void* searchValue2) {
    if (list == NULL)
        return -1;

    void* nodeToDelete = findNodeInList(list, searchValue1, searchValue2);

    if (nodeToDelete == NULL) {
        if (list->mode == FILES) {
            printf("File with name %s not found in names' list\n", (char*)searchValue1);
        } else {
            printf("Client info of client with port %d and ip %u not found in names' list\n", *(int*)searchValue1, *(uint32_t*)searchValue2);
        }
        return -1;
    }
    if (/* files list */ (list->mode == FILES && strcmp(((File*)nodeToDelete)->path, ((File*)list->firstNode)->path) == 0) ||
        /* clients list */ (list->mode == CLIENTS && ((ClientInfo*)list->firstNode)->portNumber == ((ClientInfo*)nodeToDelete)->portNumber &&
                            ((struct in_addr)((ClientInfo*)list->firstNode)->ipStruct).s_addr == ((struct in_addr)((ClientInfo*)nodeToDelete)->ipStruct).s_addr)) {
        // nodeToDelete is the first node of list
        list->firstNode = (list->mode == FILES ? (void*)((File*)nodeToDelete)->nextFile : (void*)((ClientInfo*)nodeToDelete)->nextClientInfo);
    }
    if (list->mode == FILES ? ((File*)nodeToDelete)->prevFile != NULL : ((ClientInfo*)nodeToDelete)->prevClientInfo != NULL) {
        // nodeToDelete has a previous node
        if (list->mode == FILES) {
            ((File*)nodeToDelete)->prevFile->nextFile = ((File*)nodeToDelete)->nextFile;
        } else {
            ((ClientInfo*)nodeToDelete)->prevClientInfo->nextClientInfo = ((ClientInfo*)nodeToDelete)->nextClientInfo;
        }
    }
    if (list->mode == FILES ? ((File*)nodeToDelete)->nextFile != NULL : ((ClientInfo*)nodeToDelete)->nextClientInfo != NULL) {
        // nodeToDelete has a next node
        if (list->mode == FILES) {
            ((File*)nodeToDelete)->nextFile->prevFile = ((File*)nodeToDelete)->prevFile;
        } else {
            ((ClientInfo*)nodeToDelete)->nextClientInfo->prevClientInfo = ((ClientInfo*)nodeToDelete)->prevClientInfo;
        }
    }

    // free nodeToDelete according to mode's value
    if (list->mode == FILES) {
        freeFile((File**)&nodeToDelete);
    } else {
        freeClientInfo((ClientInfo**)&nodeToDelete);
    }

    list->size--;  // update list's size

    return 0;
}

// end of generic List functions