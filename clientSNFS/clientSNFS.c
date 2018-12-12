#define FUSE_USE_VERSION 26
#define BUFFER_SIZE 1024
#define TOKEN ":6991:"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <utime.h>
#include <time.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <fuse.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

char* host = NULL;
char* port = NULL;

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

int connect_to_socket(){
    int check; // checks if function (getadd) worked
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);//IP4 - client side
    if(sock_fd == -1){
        return -1;
    }

    struct addrinfo hints, *address;//address gives u IP address to connect to
    //memset(&hints, 0, sizeof(struct addrinfo));//helps to find IP addr
    bzero(&hints, sizeof(struct addrinfo)); //this is the same thing as the line above
    hints.ai_family = AF_INET; //IP4
    hints.ai_socktype = SOCK_STREAM; //TCP

    check = getaddrinfo(host, port, &hints, &address); // convert to IP4, free
    if(check){//0 means success
            printf("getaddrinfo() error: %s\n", strerror(errno));
            freeaddrinfo(address);
            
            return -1;
    }

    if(connect(sock_fd, address->ai_addr, address->ai_addrlen) == -1){
            printf("connect error: %s\n", strerror(errno));
            freeaddrinfo(address);
            return -1;
    }
    if(address != NULL){
        freeaddrinfo(address);
    }
    //printf("Successfully connected to socket\n");
    return sock_fd;
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

int write_sock(int sock_fd, char* buffer){
    int res;
    char* newBuffer = (char*)malloc(strlen(buffer)+strlen(TOKEN)+1);
    strcpy(newBuffer, buffer);
    strcat(newBuffer, TOKEN);
    free(buffer);
    //printf("BUFFER: %s\n", newBuffer);
    res = write(sock_fd, newBuffer, strlen(newBuffer)+1);
    fsync(sock_fd);
    return res;
}

static int client_getattr(const char *path, struct stat *st){
    printf("In %s\nPath: %s\n", __func__, path);
    int sock_fd;
    int res;
    struct stat *server_st;
    void *buffer;
    void* data;
    void *ptr;
    char *msg;
    ssize_t bytes_read;
    ssize_t bytes_to_read;
    ssize_t total_bytes;
    
    if(strcmp(path, "/.Trash") == 0 || strcmp(path, "/.Trash-1000") == 0){
         //   owner of file in question
        st->st_uid = getuid();
        //group for owner
        st->st_gid = getgid();
        //last access time
        st->st_atime = time(NULL);
        //last modification time
        st->st_mtime = time(NULL);

        //root directory
        if(strcmp(path, "/") == 0){
            st->st_mode = S_IFDIR | 0755;
            //need . and .. to refer to itself
            st->st_nlink = 2;
        }

        else{
            st->st_mode = S_IFREG | 0644;
            st->st_nlink = 1;
            st->st_size = 1024;
        }
        return 0;
    }

    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    msg = (char*)malloc(strlen("getattr")+strlen(TOKEN)+strlen(path)+1);
    strcpy(msg, "getattr");
    strcat(msg, TOKEN);
    strcat(msg, path);
    
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }
    
    
    bytes_to_read = sizeof(struct stat);
    total_bytes = 0;
    data = (void*)malloc(sizeof(struct stat));
    buffer = malloc(sizeof(struct stat));
    ptr = data;
    while(total_bytes !=  bytes_to_read){
        
        if(ptr != data){
            ptr+=bytes_read;
        }
        bytes_read = read(sock_fd, buffer, sizeof(struct stat));
        if(bytes_read == -1){
            res = -errno;
            return res;
        }
        total_bytes += bytes_read;
        //printf("Bytes Read: %lu\nTotal Bytes: %lu\n", bytes_read, total_bytes);
        memcpy(ptr, buffer, bytes_read);
    }
    
    server_st = (struct stat*)data;
    close(sock_fd);
   
   
    memset(st, 0, sizeof(struct stat));
    
    //error using stat() on server
    if(server_st->st_gid == '!'){
        errno = server_st->st_dev;
        free(data);
        free(buffer);
        return -errno;
    }

    st->st_mode = server_st->st_mode;
    st->st_uid = server_st->st_uid;
    st->st_gid = server_st->st_gid;
    st->st_size = server_st->st_size;
    st->st_atime = server_st->st_atime;
    st->st_mtime = server_st->st_mtime;
    st->st_ctime = server_st->st_ctime;
    //printf("UID: %u\n",st->st_uid);
    // if(strcmp(path, "/") == 0){
    //     st->st_nlink = st->st_nlink + 1;
    // }
    
    free(data);
    free(buffer);
    
    //printf("LEAVING\n");
    return 0;
}

static int client_create(const char *path, mode_t mode, struct fuse_file_info *fi){
    printf("In %s, %u\n", __func__, mode);
    
    int sock_fd;
    char* fd_str;
    char* buf;
    char* result;
    //char* actual_path = before_substring(&path, ".swp");
    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    int modeDigits = lenHelper(mode);
    //printf("mode digits: %d\n", modeDigits);

    char *str_mode = (char*)malloc(modeDigits+1);
    sprintf(str_mode, "%u", mode);

    char *msg = (char*)malloc(strlen("create")+strlen(TOKEN)+strlen(path)+strlen(TOKEN)+modeDigits+1);
    strcpy(msg, "create");
    strcat(msg, TOKEN);
    strcat(msg, path);
    strcat(msg, TOKEN);
    strcat(msg, str_mode);
    //free(str_mode);
    
    //printf("CREATE MSG: %s\n", msg);
    fflush(stdout);
    int res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }

    
    //read from socket;
    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
        printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }else{
        fd_str = before_substring(&buf, TOKEN);
        sscanf(fd_str, "%lu", &(fi->fh));
        res = 0;
    }
    //free(actual_path);
    free(result);
    return res;
}

static int client_open(const char *path, struct fuse_file_info *fi){
    printf("In %s\n", __func__);

    int sock_fd;
    int res;
    char* fd_str;
    char* result;
    char* buf;
    char* msg;

    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    msg = (char*)malloc(strlen("open")+(2*strlen(TOKEN))+ lenHelper(fi->flags) +strlen(path)+1);
    sprintf(msg, "%s%s%s%s%d", "open", TOKEN, path, TOKEN, fi->flags);
    // strcpy(msg, "open");
    // strcat(msg, TOKEN);
    // strcat(msg, path);
    
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        return -errno;
    }

    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
        printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }
    else{
        fd_str = before_substring(&buf, TOKEN);
        sscanf(fd_str, "%lu", &(fi->fh));
        res = 0;
    }
    //free(actual_path);
    free(result);
    return res;

}

static int client_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    printf("In %s\n", __func__);

    int sock_fd;
    int res;
    int sizeDigits;
    int offsetDigits;
    ssize_t bytes_read;
    char* buf;
    char* result;
    char* msg;
    char* str_size;
    char* str_off;
    char* data_read;
    char* bytes_read_str;
    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    sizeDigits = lenHelper(size);
    offsetDigits = lenHelper(offset);
    //str_size = (char*)malloc(sizeDigits+1);
    //str_off = (char*)malloc(offsetDigits+1);
    //snprintf(str_size, sizeDigits+1, "%lu", size);
    //snprintf(str_off, offsetDigits+1, "%lu", offset);
    msg = (char*)malloc(strlen("read")+(3*strlen(TOKEN))+lenHelper(fi->fh)+sizeDigits+offsetDigits+1);
    // strcpy(msg, "write");
    // strcat(msg, TOKEN);
    // strcat(msg, path);
    // strcat(msg, TOKEN);
    // strcat(msg, str_size);
    // strcat(msg, TOKEN);
    // strcat(msg, str_off);
    // free(str_size);
    // free(str_off);
    sprintf(msg, "%s%s%lu%s%lu%s%lu", "read", TOKEN, fi->fh, TOKEN, size, TOKEN, offset);
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }
    //read from socket
    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
        printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }else{
        data_read = before_substring(&buf, TOKEN);
        bytes_read_str = before_substring(&buf, TOKEN);
        sscanf(bytes_read_str, "%ld", &bytes_read);
        strcpy(buffer, data_read);
        free(data_read);
        free(bytes_read_str);
    }
    //free(actual_path);
    free(result);
    return bytes_read;
}

static int client_flush(const char *path, struct fuse_file_info *fi){
    printf("In %s\n", __func__);
    
    int sock_fd;
    int res;
    char* result;
    char* msg;
    char* buf;

    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    msg = (char*)malloc(strlen("flush")+strlen(TOKEN)+lenHelper(fi->fh)+1);
    // strcpy(msg, "flush");
    // strcat(msg, TOKEN);
    // strcat(msg, path);
    sprintf(msg, "%s%s%lu", "flush", TOKEN, fi->fh);

    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));    
        return -errno;
    }
    
    //read from socket
    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
        printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }else{
        res = 0;
    }

    free(result);
    return 0;
}

static int client_release(const char *path, struct fuse_file_info *fi){
    printf("In %s\n", __func__);
    
    int sock_fd;
    int res;
    char* msg;
    char* buf;
    char* result;
    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    msg = (char*)malloc(strlen("release")+strlen(TOKEN)+strlen(path)+1);
    sprintf(msg, "%s%s%lu", "release", TOKEN, fi->fh);
    // strcpy(msg, "release");
    // strcat(msg, TOKEN);
    // strcat(msg, path);
    // strcat(msg, TOKEN);    
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }
    
    //read from socket
    
    
    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
        printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }else{
        res = 0;
    }
    //free(actual_path);
    free(result);
    return res;
}

static int client_opendir(const char *path, struct fuse_file_info *fi){
    printf("In %s\n", __func__);
    
    int sock_fd;
    int res;
    unsigned int access;
    char* result;
    char *buf;
    char* msg;
    char* dirfd_str;
    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    msg = (char*)malloc(strlen("opendir")+strlen(TOKEN)+strlen(path)+1);
    strcpy(msg, "opendir");
    strcat(msg, TOKEN);
    strcat(msg, path);
    
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }
    
    
    if((buf = read_sock(sock_fd)) == NULL){
        printf("%s\n", strerror(errno));
        res = -errno;
        close(sock_fd);
        return res;
    }
    
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }else{
        // per = before_substring(&buf, TOKEN);
        // access = strtoul(per, NULL, 10);
        // fi->flags = access;
        // //long unsigned int he;
        dirfd_str = before_substring(&buf, TOKEN);
        sscanf(dirfd_str, "%lu", &(fi->fh));
        // fi->fh  = he;
        // printf("FD: %lu", fi->fh);
        // fflush(stdout);
        // free(per);
        free(dirfd_str);
        res = 0;

    }
    //free(actual_path);
    free(result);
    return res;

}

static int client_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    printf("In %s\n", __func__);

    
    int sock_fd;
    int res;
    char *fname;
    char *buf;
    char* msg;
    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }

    msg = (char*)malloc(strlen("readdir")+strlen(TOKEN)+lenHelper(fi->fh)+1);
    sprintf(msg, "%s%s%lu", "readdir", TOKEN, fi->fh);
    // strcpy(msg, "readdir");
    // strcat(msg, TOKEN);
    // strcat(msg, path);
    
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }
    
    if((buf = read_sock(sock_fd)) == NULL){
        printf("%s\n", strerror(errno));
        return -errno;
    }
    
    while(buf){
        fname = before_substring(&buf, TOKEN);
        if(strcmp(fname, "empty") == 0){
            free(fname);
            filler(buffer, NULL, NULL, 0);
            break;
        }
        if (filler(buffer, fname, NULL, 0)){
            break;
        }
    }
    
    close(sock_fd);
    return 0;
}

static int client_releasedir(const char *path, struct fuse_file_info *fi){
    printf("In client_releasedir()\n");
   
    int sock_fd;
    char *buf;
    char* result;
    int res;
    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    char *msg = (char*)malloc(strlen("releasedir")+strlen(TOKEN)+lenHelper(fi->fh)+1);
    //    strcpy(msg, "releasedir");
    //    strcat(msg, TOKEN);
    //    strcat(msg, path);
    sprintf(msg, "%s%s%lu", "releasedir", TOKEN, fi->fh);
    
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }

    if((buf = read_sock(sock_fd)) == NULL){
        printf("%s\n", strerror(errno));
        return -errno;
    }

    result = before_substring(&buf, TOKEN);
    if(strcmp("success", result) != 0){
        sscanf(result, "%d", &res);
        errno = res;
    }else{
        res = 0;
    }
    close(sock_fd);
    fi->flags = 0;
    fi->fh = 0;
    return res;
}

static int client_mkdir(const char *path, mode_t mode){
 //   printf("In %s\n", __func__);
    
    int sock_fd;
    int res;
    int modeDigits;
    char* result;
    char* msg;
    char* buf;
    //char* str_mode;
    
    if((sock_fd = connect_to_socket()) == -1){
       // printf("%s\n",strerror(errno));
        return -errno;
    }
    
    modeDigits = lenHelper(mode);
    //str_mode = (char*)malloc(modeDigits+1);
    //sprintf(str_mode, modeDigits+1, "%u", mode);
    msg = (char*)malloc(strlen("mkdir")+(2*strlen(TOKEN))+strlen(path)+modeDigits+1);
    sprintf(msg, "%s%s%s%s%u", "mkdir", TOKEN, path, TOKEN, mode);
    // strcpy(msg, "mkdir");
    // strcat(msg, TOKEN);
    // strcat(msg, path);
    // strcat(msg, TOKEN);
    // strcat(msg, str_mode);
    //free(str_mode);
    
    
    res = write_sock(sock_fd, msg);
    if(res == -1){
       // printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }
    
    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
       // printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }else{
        res = 0;
    }
    //free(actual_path);
    free(result);
    return res;
}

static int client_truncate(const char *path, off_t offset){
    printf("In %s\n", __func__);

    int sock_fd;
    int res;
    char* msg;
    char* buf;
    char* result;

    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    msg = (char*)malloc(strlen("truncate")+(2*strlen(TOKEN))+strlen(path)+lenHelper(offset)+1);
    // strcpy(msg, "truncate");
    // strcat(msg, TOKEN);
    // strcat(msg, path);

    sprintf(msg, "%s%s%s%s%lu", "truncate", TOKEN, path, TOKEN, offset);

    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }

    //read response

    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
        printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }else{
        res = 0;
    }
    //free(actual_path);
    free(result);
    return res;
}

static int client_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi){
    printf("In %s\n", __func__);

    int sock_fd;
    int res;
    int sizeDigits;
    int offsetDigits;
    ssize_t bytes_read;
    char* buf;
    char* result;
    char* msg;
    char* str_size;
    char* str_off;
    char* bytes_read_str;
    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    sizeDigits = lenHelper(size);
    offsetDigits = lenHelper(offset);
    //str_size = (char*)malloc(sizeDigits+1);
    //str_off = (char*)malloc(offsetDigits+1);
    //snprintf(str_size, sizeDigits+1, "%lu", size);
    //snprintf(str_off, offsetDigits+1, "%lu", offset);
    msg = (char*)malloc(strlen("write")+strlen(buffer)+(4*strlen(TOKEN))+lenHelper(fi->fh)+sizeDigits+offsetDigits+1);
    // strcpy(msg, "write");
    // strcat(msg, TOKEN);
    // strcat(msg, path);
    // strcat(msg, TOKEN);
    // strcat(msg, str_size);
    // strcat(msg, TOKEN);
    // strcat(msg, str_off);
    // free(str_size);
    // free(str_off);
    sprintf(msg, "%s%s%s%s%lu%s%lu%s%lu", "write", TOKEN, buffer, TOKEN, fi->fh, TOKEN, size, TOKEN, offset);
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        res = -errno;
        return res;
    }
    //read from socket
    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
        printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }else{
        bytes_read_str = before_substring(&buf,TOKEN);
        sscanf(bytes_read_str, "%ld", &bytes_read);
        free(bytes_read_str);
    }
    //free(actual_path);
    free(result);
    return bytes_read;

}

static int client_unlink(const char* path){
    int sock_fd;
    int res;
    char* result;
    char* buf;
    char* msg;

    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    msg = (char*)malloc(strlen("unlink")+strlen(TOKEN)+strlen(path)+1);
    sprintf(msg, "%s%s%s", "unlink", TOKEN, path);
    // strcpy(msg, "open");
    // strcat(msg, TOKEN);
    // strcat(msg, path);
    
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        return -errno;
    }

    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
        printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }
    else{
        res = 0;
    }
    //free(actual_path);
    free(result);
    return res;
}

static int client_rmdir(const char* path){
    int sock_fd;
    int res;
    char* result;
    char* buf;
    char* msg;

    
    if((sock_fd = connect_to_socket()) == -1){
        printf("%s\n",strerror(errno));
        return -errno;
    }
    
    msg = (char*)malloc(strlen("rmdir")+strlen(TOKEN)+strlen(path)+1);
    sprintf(msg, "%s%s%s", "rmdir", TOKEN, path);
    // strcpy(msg, "open");
    // strcat(msg, TOKEN);
    // strcat(msg, path);
    
    res = write_sock(sock_fd, msg);
    if(res == -1){
        printf("%s\n", strerror(errno));
        return -errno;
    }

    if((buf = read_sock(sock_fd)) == NULL){
        close(sock_fd);
        printf("%s\n", strerror(errno));
        //free(actual_path);
        return -errno;
    }

    close(sock_fd);
    result = before_substring(&buf, TOKEN);

    if(strcmp(result, "success") != 0){
        errno = atoi(result);
        printf("%s\n", strerror(errno));
        res = -errno;
    }
    else{
        res = 0;
    }
    //free(actual_path);
    free(result);
    return res;
}


//have to return -errno for all these implementations
static struct fuse_operations client_oper = {
  .create = client_create,
  .getattr = client_getattr,
  .open = client_open,
  .flush = client_flush,
  .release = client_release,
  .read = client_read,
  .truncate = client_truncate,
  .opendir = client_opendir,
  .mkdir = client_mkdir,
  .readdir = client_readdir,
  .releasedir = client_releasedir,
  .write = client_write,
  .unlink = client_unlink,
  .rmdir = client_rmdir
};


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
    
    if(argc == 7){
        
            if(strcmp("-port", argv[1]) != 0){
                printf("Error: '-port' argument missing or incorrect.\n");
                exit(1);
            }

            if(atoi(argv[2]) <= 1024 || atoi(argv[2]) > 65535){
                printf("Error: Invalid port. Choose a port between 1024 & 65535.\n");
                exit(1);
            }
            
            if(strcmp("-address", argv[3]) != 0){
                printf("Error: '-address' argument missing or incorrect.\n");
                exit(1);
            }
            
            if(strcmp("-mount", argv[5]) != 0){
                printf("Error: '-mount' argument missing or incorrect.\n");
                exit(1);
            }
    }
    
    else{
        printf("Error: Invalid number of arguments.\n");
        exit(1);
    }
    
    port = (char*)malloc(strlen(argv[2])+1);
    strcpy(port, argv[2]);
    
    host = (char*)malloc(strlen(argv[4])+1);
    strcpy(host, argv[4]);
    //mount path is a local variable for now, can change to global later
    char* mount_path = (char*)malloc(strlen(argv[6])+1);
    strcpy(mount_path, argv[6]);
    

    if(valid_mount(mount_path)){
        printf("Mounting failed with the following error: %s\n", strerror(errno));
        free(mount_path);
        free(port);
        free(host);
        exit(1);
    }
    free(mount_path);

    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
    fuse_opt_add_arg(&args, argv[0]);
    fuse_opt_add_arg(&args, "-f");
    fuse_opt_add_arg(&args, argv[6]);
    
    printf("Terminate by pressing CTRL+C and unmount using fusermount -u [mount_path].\nYou can only unmount if you are not in the mount directory\n");
    return fuse_main(args.argc, args.argv, &client_oper, NULL);
}
