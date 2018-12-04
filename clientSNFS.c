#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fuse.h>
#include <errno.h>

struct fuse_operations client_oper = {
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
};

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
    
    int port = atoi(argv[2]);
    
    char* address = (char*)malloc(strlen(argv[4])+1);
    strcpy(address, argv[4]);
    //mount path is a local variable for now, can change to global later
    char* mount_path = (char*)malloc(strlen(argv[6])+1);
    strcpy(mount_path, argv[4]);
    
    return 0;
}