#include "utils.h"

void printErrorLn(char* s) {
    printf(ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", s);

    return;
}

void printLn(char* s) {
    printf(ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\n", s);

    return;
}

char fileExists(char* fileName) {
    if (access(fileName, F_OK) != -1) {
        // file exists
        return 1;
    }

    // file does not exist
    return 0;
}

char dirExists(char* dirName) {
    char returnValue = 0;
    DIR* dir = opendir(dirName);
    if (dir) {
        // directory exists
        returnValue = 1;
        closedir(dir);
    }

    // directory does not exist
    return returnValue;
}

void _mkdir(const char* dir) {
    char tmp[256];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (!dirExists(tmp)) {
                mkdir(tmp, S_IRWXU);
            }
            *p = '/';
        }
    }
    if (!dirExists(tmp)) {
        mkdir(tmp, S_IRWXU);
    }
}

void createAndWriteToFile(char* fileName, char* contents) {
    FILE* file = fopen(fileName, "w");  // overwrite if file exists
    if (file == NULL) {
        perror("fopen failed");
        exit(1);
    }

    fprintf(file, "%s", contents);
    fflush(file);

    if (fclose(file) == EOF) {
        perror("fclose failed");
        exit(1);
    }
    return;
}

void removeFileName(char* path) {
    char* const last = strrchr(path, '/');
    if (last != NULL)
        *last = '\0';
}

struct in_addr getLocalIp() {
    FILE* f;
    char line[100], *p, *c;

    f = fopen("/proc/net/route", "r");

    while (fgets(line, 100, f)) {
        p = strtok(line, "\t");
        c = strtok(NULL, "\t");

        if (p != NULL && c != NULL) {
            if (strcmp(c, "00000000") == 0) {
                printf("Default interface is: %s\n", p);
                break;
            }
        }
    }

    int fm = AF_INET;  // IPv4
    struct ifaddrs *ifaddr, *ifa;
    int family;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs error");
        exit(EXIT_FAILURE);
    }

    // Walk through linked list, maintaining head pointer so we can free list later
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        family = ifa->ifa_addr->sa_family;

        if (strcmp(ifa->ifa_name, p) == 0) {
            if (family == fm) {
                printf("Private IP address: %s\n", inet_ntoa(((struct sockaddr_in*)ifa->ifa_addr)->sin_addr));
                break;
            }
        }
    }
    freeifaddrs(ifaddr);

    return ((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
}

int connectToPeer(int* socketFd, struct sockaddr_in* peerAddr) {
    if (((*socketFd) = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    int attempts = 1;
    char connected = 1;
    while (connect((*socketFd), (struct sockaddr*)peerAddr, sizeof(*peerAddr)) < 0) {
        perror("Socket connection failed");
        printf("Attempt: %d, retrying...\n", attempts);
        attempts++;
        if (attempts == (MAX_CONNECT_ATTEMPTS) + 1) {
            connected = 0;
            break;
        }
        sleep(1);
    }

    if (connected) {
        // printf("Connected to port %d and ip %s\n", peerAddr->sin_port, inet_ntoa(peerAddr->sin_addr));
        return 0;
    } else {
        close(*socketFd);
        return 1;
    }
}

int createServer(int* socketFd, struct sockaddr_in* socketAddr, int portNum, int maxConnectionsNum) {
    // Creating socket file descriptor
    if (((*socketFd) = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    socketAddr->sin_family = AF_INET;
    socketAddr->sin_addr = getLocalIp();

    socketAddr->sin_port = portNum;

    if (bind(*socketFd, (struct sockaddr*)socketAddr,
             sizeof(*socketAddr)) < 0) {
        perror("Bind error");
        return 1;
    }
    if (listen(*socketFd, maxConnectionsNum) < 0) {
        perror("Listen error");
        return 1;
    }

    return 0;
}