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
    
    
}






