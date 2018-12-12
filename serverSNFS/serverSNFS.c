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
            *buffer = NULL;
        }
        return ret;
    }
}

int write_stat_sock(int sock_fd, void* buffer){
    int res;
    res = write(sock_fd, buffer, sizeof(struct stat));
    fsync(sock_fd);
    free(buffer);
    //printf("Res: %d\n", res);
    return res;
}

int lenHelper(long x){
    int totalDigits = 0;
    if(x == 0){
        return 1;
    }
    while(x!=0){
        x = x/10;
        totalDigits ++;
    }
    return totalDigits;
}


int write_sock(int sock_fd, char* buffer){
    int res;
    char* newBuffer = (char*)malloc(strlen(buffer)+strlen(TOKEN)+1);
    strcpy(newBuffer, buffer);
    strcat(newBuffer, TOKEN);
    
    //printf("BUFFER: %s\n", newBuffer);
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
        //printf("res: %lu\n", res);
        if (res == -1){
            free(buffer);
            if(msg != NULL){
                free(msg);
            }
            //printf("RES is -1\n");
            return NULL;
        }
        buffer[res] = '\0';
        //printf("Buffer: %s-hi-\n", buffer);
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
    //printf("msg: %s\n",msg);
    free(buffer);
    return msg; 
}

int server_getattr(int sock_fd, char* args){
    char* path = before_substring(&args, TOKEN);
    //printf("PATH: %s\n", path);
    struct stat *st = (struct stat*)malloc(sizeof(struct stat));
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    //printf("Full Path: %s\n", full_path);
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
    int fd;
    int flags;
    char* path = before_substring(&args, TOKEN);
    char* flags_str = before_substring(&args, TOKEN);
    //printf("PATH: %s\n", path);
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    char* buffer;
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    sscanf(flags_str, "%d", &flags);
    free(flags_str);

    if((fd = open(full_path, flags)) == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+strlen(TOKEN)+lenHelper(fd)+1);
        // strcpy(buffer, "success");
        sprintf(buffer, "%s%s%d", "success", TOKEN, fd);
    }

    free(path);
    free(full_path);
    

    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    
    return 0;
}

int server_read(int sock_fd, char* args) {
    int fd;
    off_t offset;
    size_t size;
    ssize_t bytes_read = 0;
    char* buffer;
    char* data;
    char* fd_str = before_substring(&args, TOKEN);
    char* size_str = before_substring(&args, TOKEN);
    char* offset_str = before_substring(&args, TOKEN);

    sscanf(fd_str, "%d", &fd);
    sscanf(offset_str, "%lu", &offset);
    sscanf(size_str, "%lu", &size);

    data = malloc(size+1);
    if((bytes_read = pread(fd, (void*)data, size, offset)) == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        data[size] = '\0';
        buffer = malloc(strlen("success")+(2*strlen(TOKEN))+strlen(data)+lenHelper(bytes_read)+1);
        sprintf(buffer, "%s%s%s%s%ld", "success", TOKEN, data, TOKEN, bytes_read);
    }
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    free(data);
    free(fd_str);
    free(size_str);
    free(offset_str);
    return 0;
}

int server_truncate(int sock_fd, char* args) {
    off_t offset;
    char* buffer;
    char* path = before_substring(&args, TOKEN);
    char* offset_str = before_substring(&args, TOKEN);
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    strcpy(full_path, mount_path);
    strcat(full_path, path);

    sscanf(offset_str, "%lu", &offset);

    if(truncate(full_path, offset) != 0){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+1);
        strcpy(buffer, "success");
    }

    if (write_sock(sock_fd,buffer) == -1) {
        return -1;
    }
    return 0;
}

int server_opendir(int sock_fd, char* args) {
    int fd;
    char* path = before_substring(&args, TOKEN);
    //printf("PATH: %s\n", path);
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    char* buffer;
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    DIR *dp;

    //printf("Full Path: %s\n", full_path);

    if((dp = opendir(full_path)) == NULL){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        fd = dirfd(dp);
        buffer = malloc(strlen("success")+strlen(TOKEN)+lenHelper(fd)+1);
        sprintf(buffer, "%s%s%d", "success", TOKEN, fd);
    }

    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    return 0;
}

int server_readdir(int sock_fd, char* args) {
    //printf("Args: %s\n", args);
    //char* path = before_substring(&args, TOKEN);
    int fd;
    char* dirfd_str = before_substring(&args, TOKEN);
    sscanf(dirfd_str, "%d", &fd);
    // printf("PATH: %s\n", path);
    // char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    char* buffer = NULL;
    
    // strcpy(full_path, mount_path);
    // strcat(full_path, path);
    // printf("Full Path: %s\n", full_path);

    DIR *dp;
    struct dirent *de;
    dp = fdopendir(fd);
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
    
    //closedir(dp);
    //printf("Buffer: %s\n", buffer);
    if(buffer == NULL){
        buffer = (char*)malloc(strlen("empty")+1);
        strcpy(buffer, "empty");
    }
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    return 0;
}

int server_release(int sock_fd, char* args){
    int fd;
    char* buffer;
    char* fd_str = before_substring(&args, TOKEN);
    sscanf(fd_str, "%d", &fd);

    if(close(fd) == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+1);
        strcpy(buffer, "success");
    }

    if(write_sock(sock_fd, buffer)<0){
        return -1;
    }
    return 0;
}

int server_releasedir(int sock_fd, char* args) {
    //printf("Args: %s\n", args);
    //char* path = before_substring(&args, TOKEN);
    int fd;
    char* dirfd_str = before_substring(&args, TOKEN);
    sscanf(dirfd_str, "%d", &fd);
    // printf("PATH: %s\n", path);
    // char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    char* buffer;
    if(close(fd) == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+1);
        strcpy(buffer,"success");
    }
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    free(dirfd_str);
    return 0;
}

int server_flush(int sock_fd, char* args){
    int fd;
    char* buffer;
    char* fd_str = before_substring(&args, TOKEN);
    sscanf(fd_str, "%d", &fd);

    if(fsync(fd) == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+1);
        strcpy(buffer,"success");
    }
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    free(fd_str);
    return 0;

}

int server_write(int sock_fd, char* args) {
    int fd;
    off_t offset;
    size_t size;
    ssize_t bytes_read = 0;
    char* buffer;
    char* data = before_substring(&args, TOKEN);
    char* fd_str = before_substring(&args, TOKEN);
    char* size_str = before_substring(&args, TOKEN);
    char* offset_str = before_substring(&args, TOKEN);

    sscanf(fd_str, "%d", &fd);
    sscanf(offset_str, "%lu", &offset);
    sscanf(size_str, "%lu", &size);

    if((bytes_read = pwrite(fd, (void*)data, size, offset)) == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+strlen(TOKEN)+lenHelper(bytes_read)+1);
        //strcpy(buffer,"success");
        sprintf(buffer, "%s%s%ld", "success", TOKEN, bytes_read);
    }
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    free(data);
    free(fd_str);
    free(size_str);
    free(offset_str);
    return 0;
}

int server_create(int sock_fd, char* args) {
    char* path = before_substring(&args, TOKEN);
    char* mode_str = before_substring(&args, TOKEN);
    
    int len = strlen(mode_str);
    //printf("MODE_STR lEN: %d\n", len);
    int file_fd;
    //printf("PATH: %s\n", path);
    char* full_path = (char*)malloc(strlen(mount_path)+strlen(path)+1);
    char* buffer;
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    //printf("Full Path: %s\n", full_path);
    
    mode_t mode = strtoul(mode_str, NULL, 10);
    
    file_fd = creat(full_path, mode);
    free(full_path);
    //printf("MODE: %u, file_fd: %d\n", mode, file_fd);
    if(file_fd == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+strlen(TOKEN)+lenHelper(file_fd)+1);
        sprintf(buffer, "%s%s%d", "success", TOKEN, file_fd);
    }
    
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    return 0;
}

int server_mkdir(int sock_fd, char* args) {
    
    char* buffer;
    char* path = before_substring(&args, TOKEN);
    char* full_path = (char*)malloc(strlen(mount_path) + strlen(path)+1);
    char* str_mode = before_substring(&args, TOKEN);
    strcpy(full_path, mount_path);
    strcat(full_path, path);
    mode_t mode = strtoul(str_mode, NULL, 10);
    int ret = mkdir(full_path, mode);

    free(full_path);
    free(path);
    free(str_mode);

    if(ret == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+1);
        strcpy(buffer, "success");
    }
    
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    return 0;
}

int server_rmdir(int sock_fd, char* args){
    char* buffer;
    char* path = before_substring(&args, TOKEN);
    char* full_path = (char*)malloc(strlen(mount_path) + strlen(path)+1);
    strcpy(full_path, mount_path);
    strcat(full_path, path);

    int ret = rmdir(full_path);

    free(full_path);
    free(path);

    if(ret == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+1);
        strcpy(buffer, "success");
    } 
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    return 0;

}

int server_unlink(int sock_fd, char* args){
    char* buffer;
    char* path = before_substring(&args, TOKEN);
    char* full_path = (char*)malloc(strlen(mount_path) + strlen(path)+1);
    strcpy(full_path, mount_path);
    strcat(full_path, path);

    int ret = unlink(full_path);

    free(full_path);
    free(path);

    if(ret == -1){
        buffer = malloc(lenHelper(errno)+1);
        sprintf(buffer, "%d", errno);
    }else{
        buffer = malloc(strlen("success")+1);
        strcpy(buffer, "success");
    } 
    if(write_sock(sock_fd, buffer) < 0){
        return -1;
    }
    return 0;

}



void* test(void *param){
    int sock_fd = *((int*)param);
    pthread_detach(pthread_self());
    //printf("Sock FD: %d\n", sock_fd);
    char* msg = read_sock(sock_fd);
    //printf("MSG: %s\n", msg);
    if(msg == NULL){
        printf("Error reading from socket\n");
        close(sock_fd);
        pthread_exit(NULL);
    }
    //printf("Message: %s\n", msg);
    char* token = before_substring(&msg, TOKEN);
    //printf("TOKEN: %s\n, MSG: %s\n", token, msg);

    
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
        if(server_flush(sock_fd, msg) != 0){
            printf("Error in server flush\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
    }
    else if(strcmp(token, "release") == 0){
        if(server_release(sock_fd, msg) != 0){
            printf("Error in server release\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
        
    }
    else if(strcmp(token, "read") == 0){
        if(server_read(sock_fd, msg) != 0){
            printf("Error in server read\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
        
    }
    else if(strcmp(token, "truncate") == 0){
        if(server_truncate(sock_fd, msg) != 0){
            printf("Error in server truncate\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
        
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
        if(server_write(sock_fd, msg) != 0){
            printf("Error in server write\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
    }
    else if(strcmp(token, "unlink") == 0){
        if(server_unlink(sock_fd, msg) != 0){
            printf("Error in server unlink\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
    }
    else if(strcmp(token, "rmdir") == 0){
        if(server_rmdir(sock_fd, msg) != 0){
            printf("Error in server rmdir\n");
        }
        close(sock_fd);
        free(token);
        pthread_exit(NULL);
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
    //printf("Mount: %s\n", mount_path);
    
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
        printf("%s\n", strerror(errno));
        exit(1);
    }



    if (listen(server_fd, 10) != 0) {//10 is queue size for waiting connections
    	printf("%s\n", strerror(errno));
    	exit(1);
    }
    

    

    pthread_t tid;
    int newfd;
    int *ptr;
    
    printf("Terminate server with CTRL+C\n");
    //printf("Received connections from: ");
    fflush(stdout);
    
    char ipstr[INET6_ADDRSTRLEN];
    bzero(ipstr, 50);		
    struct sockaddr_in newAddr;
    socklen_t newAddrSize = sizeof(newAddr);
    
    while(1){
        if((newfd = accept(server_fd, NULL, NULL)) == -1){
            printf("Accept Error: %s\n", strerror(errno));
            continue;
        }
        
        int err = getpeername(newfd, (struct sockaddr *) &newAddr, &newAddrSize);

        if (err == 0) {
            inet_ntop(AF_INET, &newAddr.sin_addr, ipstr, sizeof(ipstr));
            //printf("%s, ", ipstr);	
            //fflush(stdout);
        }else{
            printf("getpeername() error: %d\n", errno);
        }

        ptr = malloc(sizeof(int));
        *ptr = newfd;
        check = pthread_create(&tid, NULL, test, (void*)ptr);
        
        if(check){
           printf("pthread_create error: %s\n", strerror(errno));
        }

    }

    close(server_fd);

    // char *args = malloc(strlen("/tmp/server/hi.txt/:6991:2:6991:"));
    // strcpy(args, "/tmp/server/hi.txt/:6991:2:6991:");
    // int x = server_create(0, args);
    return 0;
}







