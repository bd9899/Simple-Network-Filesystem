#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <pthread.h>

int numClients;

int server_open(int sock_fd, char* args, int oflags) {
    char* path = before_substring(&args, TOKEN);
    if (access(path, F_OK) != 0) {
        return -1;
    }
    return 0;
}

int server_read(int sock_fd, char* args, size_t nbytes) {
    char* path = before_substring(*args, TOKEN);
    char* buffer = (char*)malloc(nbytes +1);
    if (access(path, R_OK) != 0) {
        return -1;
    }
    int i; 
    i = open(path, O_RDONLY);
    read(i, buffer, nbytes);

    if (write_sock(sock_fd,buffer) == -1) {
        return -1;
    }

    return 0;
}

int server_truncate(int sock_fd, char* args, off_t length) {
    char* path = before_substring(&args, TOKEN);
    if (truncate(path, length) != 0) {
        return -1;
    }
    return 0;
}

int server_opendir(int sock_fd, char* args) {
    char* path = before_substring(&args, TOKEN);
    if (access(path, F_OK | R_OK) != 0) {
        return -1;
    }
    return 0;
}

int server_readdir(int sock_fd, char* args) {
    char* path = before_substring(&args, TOKEN);

    DIR *dp;
    struct dirent *de;

    dp = opendir(path);
    if (dp == NULL) {
        return -1;
    }

    while ((de = readdir(dp)) != NULL) {
        //
    }

    return 0;
}

int server_releasedir(DIR *dp) {
    closedir(dp);
    return 0;
}

int main(int argc, char **argv){

    if (argc == 5) {
            if (strcmp("-port", argv[1]) != 0) {
                printf("Error: '-port' argument missing or incorrect.\n");
                exit(1);
            }

            if (atoi(argv[2]) <= 1024 || atoi(argv[2]) > 65535) {
                printf("Error: Invalid port. Choose a port between 1024 & 65535.\n");
                exit(1);
            }
            
            if(strcmp("-mount", argv[3]) != 0) {
                printf("Error: '-mount' argument missing or incorrect.\n");
                exit(1);
            }
           
    } else {
        printf("Error: Invalid number of arguments.\n");
        exit(1);
    }

    int port = atoi(argv[2]);
    
    //mount path is a local variable for now, can change to global later
    char* mount_path = (char*)malloc(strlen(argv[4])+1);
    strcpy(mount_path, argv[4]);

    struct addrinfo hints;
    struct addrinfo *server;
    memset(&hints,0 ,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    //make socket
    int socketFD;
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if(socketFileDes < 0){
        //ERRNO HERE
        printf("ERROR CODE: %s\n",strerror(errno));
        return -1;
    }

    //bind socket
    if(bind(socketFileDes, server->ai_addr, server->ai_addrlen) == -1){
        //ERRNO HERE
        printf("ERROR CODE: %s\n",strerror(errno));
        return -1;
    }

    //Listen to socket
    if(listen(socketFileDes, 10) < 0){
        //ERRNO HERE
        printf("ERROR CODE: %s\n",strerror(errno));
        return -1;
    }

    //handle requests
    numClients = 0;
    while(1){
        int accSockFD = accept(socketFileDes, server->ai_addr, &server->ai_addrlen);
        int * clientSock = (int*)calloc(1,sizeof(int));
        *clientSock = accSockFD;

        //spawn thread to take care of client request
        if(numClients < 10){
            pthread_t newThread;
            pthread_create(&newThread, NULL, HANDLINGMETHODHERE, (void *)clientSock);
            numClients++;
        }
    }
    
}






