#include "list.h"

File* initFile(char* path, off_t contentsSize, time_t timestamp) {
    File* file = (File*)malloc(sizeof(File));

    file->path = (char*)malloc(strlen(path) + 1);
    strcpy(file->path, path);

    file->contentsSize = contentsSize;  // -1 for directory
    file->version = timestamp;
    // file->type = type;

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

    // free((*clientInfo)->path);
    // (*file)->path = NULL;

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
        if (list->mode == FILES ? strcmp(((File*)curNode)->path, (char*)searchValue1) == 0 : ((ClientInfo*)curNode)->portNumber == *((int*)searchValue1) && (((ClientInfo*)curNode)->ipStruct).s_addr == *(uint32_t*)searchValue2) {
            return curNode;
        } else if (list->mode == FILES ? strcmp((char*)searchValue1, ((File*)curNode)->path) < 0 : *((int*)searchValue1) < ((ClientInfo*)curNode)->portNumber) {  // no need for searching further since the list is sorted by fileId
            return NULL;
        }
        // if (list->mode == FILES) {
        //     curNode = ((File*)curNode)->nextFile;
        // } else {
        //     curNode = ((ClientInfo*)curNode)->nextClientInfo;
        // }
        curNode = list->mode == FILES ? (void*)((File*)curNode)->nextFile : (void*)((ClientInfo*)curNode)->nextClientInfo;
    }
    return NULL;
}

void* addNodeToList(List* list, void* nodeToInsert) {
    // printf("hedddddd\n");
    if (list == NULL)
        return NULL;
    // printf("heoo\n");

    void* foundNode = list->mode == FILES ? findNodeInList(list, ((File*)nodeToInsert)->path, NULL) : findNodeInList(list, &((ClientInfo*)nodeToInsert)->portNumber, &((ClientInfo*)nodeToInsert)->ipStruct.s_addr);
    // printf("hsssse\n");

    if (foundNode != NULL) {  // is duplicate
        // printf("")
        return NULL;  // do not add duplicate
    }
    // printf("hee\n");

    // nodeToInsert = list->mode == FILES ? (File*) nodeToInsert : (ClientInfo*) nodeToInsert;
    if (list->size == 0) {
        // printf("right\n");
        list->firstNode = nodeToInsert;

        list->size++;
        // printf(ANSI_COLOR_MAGENTA "Inserted file with path %s to List\n" ANSI_COLOR_RESET, path);
        return list->firstNode;
    } else {
        void* curNode = list->firstNode;
        // curNode = (list->mode == FILES ? (File*) curNode : (ClientInfo*) curNode);
        printf("before if\n");

        if (list->mode == FILES ? strcmp(((File*)nodeToInsert)->path, ((File*)curNode)->path) < 0 : ((ClientInfo*)nodeToInsert)->portNumber < ((ClientInfo*)curNode)->portNumber) {
                    printf("will insert at start\n");

            // insert at start
            // void* fileToInsert = nodeToInsert;
            if (list->mode == FILES) {
                ((File*)nodeToInsert)->nextFile = (File*)curNode;
                ((File*)curNode)->prevFile = (File*)nodeToInsert;
            } else {
                ((ClientInfo*)nodeToInsert)->nextClientInfo = (ClientInfo*)curNode;
                ((ClientInfo*)curNode)->prevClientInfo = (ClientInfo*)nodeToInsert;
            }

            list->firstNode = nodeToInsert;
            list->size++;
            // printf(ANSI_COLOR_MAGENTA "Inserted file with path %s to List\n" ANSI_COLOR_RESET, path);
            return list->firstNode;
        }
        printf("before while\n");
        while (curNode != NULL) {
            if (list->mode == FILES ? ((File*)curNode)->nextFile != NULL : ((ClientInfo*)curNode)->nextClientInfo != NULL) {
                printf("in big if\n");
                if (list->mode == FILES ? strcmp(((File*)nodeToInsert)->path, ((File*)curNode)->nextFile->path) < 0 : ((ClientInfo*)nodeToInsert)->portNumber < ((ClientInfo*)curNode)->nextClientInfo->portNumber) {
                    // Node* fileToInsert = initNode(path, contentsSize, type);
                    if (list->mode == FILES) {
                        ((File*)nodeToInsert)->prevFile = (File*)curNode;
                        ((File*)nodeToInsert)->nextFile = ((File*)curNode)->nextFile;
                        ((File*)curNode)->nextFile->prevFile = (File*)nodeToInsert;
                        ((File*)curNode)->nextFile = (File*)nodeToInsert;
                    } else {
                        printf("hi\n");
                        ((ClientInfo*)nodeToInsert)->prevClientInfo = (ClientInfo*)curNode;
                                                printf("hi1\n");

                        ((ClientInfo*)nodeToInsert)->nextClientInfo = ((ClientInfo*)curNode)->nextClientInfo;
                                                printf("hi2\n");

                        ((ClientInfo*)curNode)->nextClientInfo->prevClientInfo = (ClientInfo*)nodeToInsert;
                                                printf("hi3\n");

                        ((ClientInfo*)curNode)->nextClientInfo = (ClientInfo*)nodeToInsert;
                                                printf("hi4\n");

                    }
                    // nodeToInsert->nextNode = curNode->nextNode;

                    // curNode->nextNode = fileToInsert;
                    list->size++;
                    // printf(ANSI_COLOR_MAGENTA "Inserted file with path %s to List\n" ANSI_COLOR_RESET, path);
                    return list->mode == FILES ? (void*)((File*)curNode)->nextFile : (void*)((ClientInfo*)curNode)->nextClientInfo;
                }
            } else {
                printf("in else\n");
                // insert at the end
                // curNode->nextNode = initNode(path, contentsSize, type);
                if (list->mode == FILES) {
                    ((File*)curNode)->nextFile = (File*)nodeToInsert;
                    ((File*)curNode)->nextFile->prevFile = ((File*)curNode);
                    // ((File*)curNode)->nextFile = (File*)nodeToInsert;
                } else {
                    ((ClientInfo*)curNode)->nextClientInfo = (ClientInfo*)nodeToInsert;
                    ((ClientInfo*)curNode)->nextClientInfo->prevClientInfo = ((ClientInfo*)curNode);
                    // ((ClientInfo*)nodeToInsert)->nextClientInfo = ((ClientInfo*)curNode)->nextClientInfo;
                    // ((ClientInfo*)curNode)->nextClientInfo = (ClientInfo*)nodeToInsert;
                }
                list->size++;
                // printf(ANSI_COLOR_MAGENTA "Inserted file with path %s to List\n" ANSI_COLOR_RESET, path);
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
    // printf("nodetodelete ip: %u, port: %d, first node ip: %u, port: %d\n",((struct in_addr)((ClientInfo*)nodeToDelete)->ipStruct).s_addr, (int)((ClientInfo*)nodeToDelete)->portNumber,  ( (struct in_addr)((ClientInfo*)list->firstNode)->ipStruct).s_addr,((ClientInfo*)list->firstNode)->portNumber );
    if ((list->mode == FILES && strcmp(((File*)nodeToDelete)->path, ((File*)list->firstNode)->path) == 0) || (list->mode == CLIENTS && ((ClientInfo*)list->firstNode)->portNumber == ((ClientInfo*)nodeToDelete)->portNumber &&
                                                                                                              ((struct in_addr)((ClientInfo*)list->firstNode)->ipStruct).s_addr == ((struct in_addr)((ClientInfo*)nodeToDelete)->ipStruct).s_addr)) {
        //  printf("hey\n");

        list->firstNode = (list->mode == FILES ? (void*)((File*)nodeToDelete)->nextFile : (void*)((ClientInfo*)nodeToDelete)->nextClientInfo);
    }
    if (list->mode == FILES ? ((File*)nodeToDelete)->prevFile != NULL : ((ClientInfo*)nodeToDelete)->prevClientInfo != NULL) {
        // nodeToDelete->prevNode->nextNode = nodeToDelete->nextNode;
        if (list->mode == FILES) {
            ((File*)nodeToDelete)->prevFile->nextFile = ((File*)nodeToDelete)->nextFile;
        } else {
            ((ClientInfo*)nodeToDelete)->prevClientInfo->nextClientInfo = ((ClientInfo*)nodeToDelete)->nextClientInfo;
        }
        // printf("ha\n");
    }
    if (list->mode == FILES ? ((File*)nodeToDelete)->nextFile != NULL : ((ClientInfo*)nodeToDelete)->nextClientInfo != NULL) {
        // nodeToDelete->nextNode->prevNode = nodeToDelete->prevNode;
        if (list->mode == FILES) {
            ((File*)nodeToDelete)->nextFile->prevFile = ((File*)nodeToDelete)->prevFile;
        } else {
            ((ClientInfo*)nodeToDelete)->nextClientInfo->prevClientInfo = ((ClientInfo*)nodeToDelete)->prevClientInfo;
        }
        // printf("hi\n");
    }

    if (list->mode == FILES) {
        freeFile((File**)&nodeToDelete);
    } else {
        freeClientInfo((ClientInfo**)&nodeToDelete);
        printf("deleted client\n");
    }

    list->size--;

    return 0;
}