#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#define ERR(source) (perror(source),\
                    fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                    exit(EXIT_FAILURE))
    
#define MAX_PATH 1024
#define MAX_NAME 128
#define MAX_COMMAND 256
#define MAX_FILES 1024
#define FILE_TYPES 5
#define MAXFD 100
#define MAX_PRINTING_LINES 3

typedef enum type {JPEG, PNG, GZIP, ZIP, D, DNR, DP, OTHER} type;

typedef struct file_info{
    char filename[MAX_NAME];
    char path[MAX_PATH];
    size_t size;
    uid_t owner_uid;
    type file_type;
} file_info_t;

typedef struct indexThreadArgs{
    pthread_t* tid;
    volatile sig_atomic_t* running;
    char* path;
    file_info_t* files;
    int* n;
    char* index_path;
    struct clockThreadArgs* clockThread;
    pthread_mutex_t* mxFiles;
    pthread_mutex_t* mxRunning;
} indexThreadArgs_t;

typedef struct clockThreadArgs{
    pthread_t* tid;
    int t;
    indexThreadArgs_t* indexThread;
} clockThreadArgs_t;