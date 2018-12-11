#define _GNU_SOURCE
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
#include <errno.h>
#include <dirent.h>

#define BUFFER_SIZE 1024
#define TOKEN ":6991:"


char *mount_path;

char* before_substring(char** buffer, char* substring){
    char *start = strstr(*buffer, substring);
    char* ret;
    if(start == NULL){
        return NULL;
    }else{
        int len = 0;
        char *ptr = *buffer;
        while(ptr != start){
            len++;
            ptr++;
        }
        ret = (char*)malloc(len+1);
        strncpy(ret, *buffer, len);
        ret[len] = '\0';
        *buffer = start;
        *buffer += strlen(substring);
        memset(start, 0, strlen(substring));
        if(*buffer[0] == '\0'){
            memset(*buffer, 0, 1);
        }
        return ret;
    }
}

int write_stat_sock(int sock_fd, void* buffer){
    int res;
    res = write(sock_fd, buffer, sizeof(struct stat));
    fsync(sock_fd);
    free(buffer);
    printf("Res: %d\n", res);
    return res;
}

int lenHelper(int x) {
    if (x >= 1000000000) return 10;
    if (x >= 100000000)  return 9;
    if (x >= 10000000)   return 8;
    if (x >= 1000000)    return 7;
    if (x >= 100000)     return 6;
    if (x >= 10000)      return 5;
    if (x >= 1000)       return 4;
    if (x >= 100)        return 3;
    if (x >= 10)         return 2;
    return 1;
}


int write_sock(int sock_fd, char* buffer){
    int res;
    char* newBuffer = (char*)malloc(strlen(buffer)+strlen(TOKEN)+1);
    strcpy(newBuffer, buffer);
    strcat(newBuffer, TOKEN);
    
    printf("BUFFER: %s\n", newBuffer);
    res = write(sock_fd, newBuffer, strlen(newBuffer)+1);
    fsync(sock_fd);
    free(buffer);
    free(newBuffer);
    return res;
}

char* read_sock(int sock_fd){
        
    ssize_t res;

    char *msg = NULL;
    char *buffer = malloc(2);
    while((res = read(sock_fd, buffer, 1))){
        printf("res: %lu\n", res);
        if (res == -1){
            free(buffer);
            if(msg != NULL){
                free(msg);
            }
            printf("RES is -1\n");
            return NULL;
        }
        buffer[res] = '\0';
        printf("Buffer: %s-hi-\n", buffer);
        if(buffer[0] == buffer[res]){
            break;
        }

        if(msg == NULL){
            msg = (char*)malloc(strlen(buffer)+1);
            strcpy(msg, buffer);
             
        }
        else{
            msg = (char*)realloc(msg, strlen(msg)+strlen(buffer)+1);
            strcat(msg, (char*)buffer);
        }
    }
    printf("msg: %s\n",msg);
    free(buffer);
    return msg; 
}

int server_getattr(int sock_fd, char* args){
    char* path = before_substring(&args, TOKEN);
    printf("PATH: %s\n", path);
    struct stat *st = (struct stat*)malloc(sizeof(struct stat));
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    printf("Full Path: %s\n", full_path);
    if(stat(full_path, st) != 0){
        printf("Stat Failed\n");
        st->st_dev = errno;
        st->st_gid = '!';
    }    

    if(write_stat_sock(sock_fd, (void*)st) == -1){
        free(full_path);
        return -1;
    }
    
    free(full_path);
    return 0;
}

int server_open(int sock_fd, char* args) {
    char* path = before_substring(&args, TOKEN);
    if (access(path, F_OK) != 0) {
        return -1;
    }
    return 0;
}

int server_read(int sock_fd, char* args, size_t nbytes) {
    char* path = before_substring(&args, TOKEN);
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
    printf("PATH: %s\n", path);
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    char* buffer;
    
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    printf("Full Path: %s\n", full_path);
    if (access(full_path, F_OK | R_OK) != 0) {
        buffer = (char*)malloc(3);
        strcpy(buffer, "no");
    }else{
        buffer = (char*)malloc(4);
        strcpy(buffer, "yes");
    }
    
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    return 0;
}

int server_readdir(int sock_fd, char* args) {
    printf("Args: %s\n", args);
    char* path = before_substring(&args, TOKEN);
    printf("PATH: %s\n", path);
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    char* buffer = NULL;
    
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    printf("Full Path: %s\n", full_path);
    DIR *dp;
    struct dirent *de;
    dp = opendir(full_path);
    while ((de = readdir(dp)) != NULL) {
        if(buffer == NULL){
           buffer = (char*)malloc(strlen(de->d_name)+1);
           strcpy(buffer, de->d_name);
        }else{
            buffer = realloc(buffer, strlen(buffer)+strlen(TOKEN)+strlen(de->d_name)+1);
            strcat(buffer, TOKEN);
            strcat(buffer, de->d_name);
        }
    }
    
    closedir(dp);
    printf("Buffer: %s\n", buffer);
    if(buffer == NULL){
        buffer = (char*)malloc(strlen("empty")+1);
        strcpy(buffer, "empty");
    }
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    return 0;
}

int server_releasedir(void* args) {
    printf("Args: %s\n", args);
    char* path = before_substring(&args, TOKEN);
    printf("PATH: %s\n", path);
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    char* buffer = NULL;
    
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    printf("Full Path: %s\n", full_path);
    DIR *dp;
    dp = closedir(full_path);
    
    return 0;
}

int server_write(int sock_fd, char* args,size_t nbytes) {
//    char* path = before_substring(&args, TOKEN);
//    char* buffer = (char*)malloc(nbytes +1);
//    if (access(path, W_OK) != 0) {
//        return -1;
//    }
//    int i; 
//    i = open(path, O_WRONLY);
//    pwrite(i, buffer, nbytes);
    return 0;
}

int server_create(int sock_fd, char* args) {
    char* path = before_substring(&args, TOKEN);
    char* mode_str = before_substring(&args, TOKEN);
    
    int len = strlen(mode_str);
    printf("MODE_STR lEN: %d\n", len);
    int file_fd;
    printf("PATH: %s\n", path);
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    char* buffer;
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    printf("Full Path: %s\n", full_path);
    
    mode_t mode = strtoul(mode_str, NULL, 10);
    
    file_fd = creat(full_path, mode);
    free(full_path);
    printf("MODE: %u, file_fd: %d\n", mode, file_fd);
    if(file_fd == -1){
        int a = lenHelper(errno);
        int b = errno;
        printf("LEN ERRNO: %d\n", a);
        fflush(stdout);
        buffer = malloc(a+1);
        asprintf(&buffer, "%d", b);
        printf("Line 274: %s\n", strerror(errno));
        fflush(stdout);
    }else{
        buffer = malloc(strlen("success")+1);
        strcpy(buffer, "success");
    }
    
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    return 0;
}

int server_mkdir(int sock_fd, char* args) {
    char* path = before_substring(&args, TOKEN);
    if (access(path, F_OK) == 0) {
        return 0;
    }
    else {
        DIR *dp;
        dp = opendir(path);
    }
    return 0;
}

void* test(void *param){
    int sock_fd = *((int*)param);
    pthread_detach(pthread_self());
    printf("Sock FD: %d\n", sock_fd);
    char* msg = read_sock(sock_fd);
    printf("MSG: %s\n", msg);
    if(msg == NULL){
        printf("Error reading from socket\n");
        close(sock_fd);
        pthread_exit(NULL);
    }
    //printf("Message: %s\n", msg);
    char* token = before_substring(&msg, TOKEN);
    printf("TOKEN: %s\n, MSG: %s\n", token, msg);

    
    if(strcmp(token, "create") == 0){
        if(server_create(sock_fd, msg) != 0){
            printf("Error in server create\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
    }
    else if(strcmp(token, "getattr") == 0){
        if(server_getattr(sock_fd, msg) != 0){
            printf("Error in server getattr\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
    }
    else if(strcmp(token, "open") == 0){
        if(server_open(sock_fd, msg) != 0){
            printf("Error in server open\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
    }
    else if(strcmp(token, "flush") == 0){
        
    }
    else if(strcmp(token, "release") == 0){
        
    }
    else if(strcmp(token, "read") == 0){
        
    }
    else if(strcmp(token, "truncate") == 0){
        
    }
    else if(strcmp(token, "opendir") == 0){
        if(server_opendir(sock_fd, msg) != 0){
            printf("Error in server opendir\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
    }
    else if(strcmp(token, "mkdir") == 0){
        if(server_mkdir(sock_fd, msg) != 0){
            printf("Error in server mkdir\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
    }
    else if(strcmp(token, "readdir") == 0){
        if(server_readdir(sock_fd, msg) != 0){
            printf("Error in server readdir\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);     
    }
    else if(strcmp(token, "releasedir") == 0){
        if(server_releasedir(sock_fd, msg) != 0){
            printf("Error in server releasedir\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL); 
    }
    else if(strcmp(token, "write") == 0){
        
    }
    else{
        //write to socket that there was an error
        char *buffer = (char*)malloc(strlen("error")+1);
        strcpy(buffer, "error");
        if(write_sock(sock_fd, buffer) == -1){
            printf("Error writing to socket\n"); 
        }
        free(buffer);
        free(token);
              
    }
    close(sock_fd);
    pthread_exit(NULL);
}



int valid_mount(char* mount){
    DIR* dir = opendir(mount);
    if (dir){
        closedir(dir);
        return 0;
    }
    else if (ENOENT == errno){
        /* Directory does not exist. */
        return -1;
    }
    else
    {
        /* opendir() failed for some other reason*/
        return 1;
    }
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
    mount_path = (char*)malloc(strlen(argv[4])+1);
    strcpy(mount_path, argv[4]);
    printf("Mount: %s\n", mount_path);
    
    if(valid_mount(mount_path)){
        printf("Mounting failed with the following error: %s\n", strerror(errno));
        free(mount_path);
        exit(1);
    }
    
    
    int check; // checks if function (getadd) worked
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);//IP4 - main socket
    if(server_fd == -1){
    	printf("socket creation error: %d\n", errno);
    }       

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);//0


    

    //this code allows u to reuse port immediately after close
    int optval = 10;
    check = setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    //bind using sockaddr
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) != 0) {//bind to this
        printf("bind()");
        return 1;
    }



    if (listen(server_fd, 10) != 0) {//10 is queue size for waiting connections
    	printf("listen()");
    	exit(1);
    }
    

    

    pthread_t tid;
    int newfd;
    int *ptr;
    
    //printf("Press 'q' followed by enter at anytime to terminate server!\n");
    printf("Received connections from: ");
    fflush(stdout);
    
    char ipstr[INET6_ADDRSTRLEN];
    bzero(ipstr, 50);		
    struct sockaddr_in newAddr;
    socklen_t newAddrSize = sizeof(newAddr);
    
    while(1){
        if((newfd = accept(server_fd, NULL, NULL)) == -1){
            printf("Accept Error: %d\n", errno);
            continue;
        }
        
        int err = getpeername(newfd, (struct sockaddr *) &newAddr, &newAddrSize);

        if (err == 0) {
            inet_ntop(AF_INET, &newAddr.sin_addr, ipstr, sizeof(ipstr));
            printf("%s, ", ipstr);	
            fflush(stdout);
        }else{
            printf("getpeername() error: %d\n", errno);
        }

        ptr = malloc(sizeof(int));
        *ptr = newfd;
        check = pthread_create(&tid, NULL, test, (void*)ptr);
        
        if(check){
           printf("pthread_create error: %d\n", errno);
        }

    }

    close(server_fd);

    // char *args = malloc(strlen("/tmp/server/hi.txt/:6991:2:6991:"));
    // strcpy(args, "/tmp/server/hi.txt/:6991:2:6991:");
    // int x = server_create(0, args);
    return 0;
}
