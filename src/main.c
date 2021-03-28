// Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów uczenia się 
// z przedmiotu SOP została wykonana przez emnie samodzielnie. Jan Szablanowski, 305893

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include "utils.h"

void get_options(int argc, char** argv, char* dir_path, char* index_path, int* t);
int checkLastModificationTime(clockThreadArgs_t* clockThread, char* index_path);
void readIndexedFiles(char* index_path, file_info_t* files, int* filesCount);
void action(file_info_t* files, int n, indexThreadArgs_t* indexArgs);

int main(int argc, char** argv)
{
    char dir_path[MAX_PATH];     dir_path[0] = 0;           // path to indexed directory
    char index_path[MAX_PATH];   index_path[0] = 0;         // path to file containing indexes
    
    file_info_t files[MAX_FILES];                           // array of indexed files
    int filesCount = 0;       
    pthread_mutex_t mxFiles = PTHREAD_MUTEX_INITIALIZER;
                                
    int t = -1;                                             // period of time, when next indexing action happens; -1 means no time was set
    volatile sig_atomic_t indexThreadRunning = 0;           // 0 - not running, 1 - running
    pthread_mutex_t mxRunning = PTHREAD_MUTEX_INITIALIZER;

    pthread_t indexTid;                                     // indexing thread
    pthread_t clockTid;                                     // periodic index action check

    int initial_indexing; 

    if(getenv("MOLE_DIR"))
        strcpy(dir_path, getenv("MOLE_DIR"));
    if(getenv("MOLE_INDEX_PATH"))
        strcpy(index_path, getenv("MOLE_INDEX_PATH"));
    
    get_options(argc, argv, dir_path, index_path, &t); 

    if(strlen(dir_path) == 0)
        ERR("no directory");
    if(strlen(index_path) == 0){
        strcpy(index_path, getenv("HOME"));
        strcat(index_path, "/.mole-index");
    }

    indexThreadArgs_t indexArgs = {&indexTid, &indexThreadRunning, dir_path, files, &filesCount, index_path, NULL, &mxFiles, &mxRunning};
    clockThreadArgs_t clockArgs = {&clockTid, t, &indexArgs};
    indexArgs.clockThread = &clockArgs;

    initial_indexing = checkLastModificationTime(&clockArgs, index_path);  // 0 - no initial index action, 1 - initial index action
       
    if(access(index_path, F_OK) == 0 && !initial_indexing){                    // index file exists and no need for indexing -- read indexes from file
        readIndexedFiles(index_path, files, &filesCount);
    }
    else{                                                                      // indexing necessary
        clockArgs.t = t;
        if(pthread_create(indexArgs.tid, NULL, index_action, &indexArgs))
            ERR("pthread_create");
    }
    
    // MAIN LOOP
    while(1){
        action(files, filesCount, &indexArgs);
    }

    pthread_mutex_destroy(&mxFiles);
    pthread_mutex_destroy(&mxRunning);
    return EXIT_SUCCESS;
}


int checkLastModificationTime(clockThreadArgs_t* clockThread, char* index_path)
{
    struct stat attrib;
    int initial_indexing = 0;

    if(access(index_path, F_OK) == 0 && clockThread->t != -1){
        stat(index_path, &attrib);
        if(clockThread->t < attrib.st_mtim.tv_sec){
            initial_indexing = 1;
        }
        else
        {
            initial_indexing = 0;
            clockThread->t = clockThread->t - attrib.st_mtim.tv_sec;
        }
    }
    return initial_indexing;
}


void readIndexedFiles(char* index_path, file_info_t* files, int* filesCount)
{
    file_info_t new_file;
    int index_fd;
    ssize_t out;
   

    if((index_fd = TEMP_FAILURE_RETRY(open(index_path, O_RDONLY))) < 0)
        ERR("open");

    do{
        if((out = bulk_read(index_fd, new_file.filename, MAX_NAME)) < 0)         ERR("read");
        if((out = bulk_read(index_fd, new_file.path, MAX_PATH)) < 0)             ERR("read");
        if((out = bulk_read(index_fd, &new_file.size, sizeof(size_t))) < 0)      ERR("read");
        if((out = bulk_read(index_fd, &new_file.owner_uid, sizeof(uid_t))) < 0)  ERR("read");
        if((out = bulk_read(index_fd, &new_file.file_type, sizeof(type))) < 0)   ERR("read");

        if(out){
            files[*(filesCount)] = new_file;
            *filesCount = *filesCount + 1;
            if(*filesCount >= MAX_FILES)
                break;
        }
    }while(out);

    if(TEMP_FAILURE_RETRY(close(index_fd)))
        ERR("close");
}


void action(file_info_t* files, int n, indexThreadArgs_t* indexArgs)
{
    char command[MAX_COMMAND];
    int x;
    char* namepart;
    uid_t owner_id; 

    if(fgets(command, MAX_COMMAND, stdin) < 0)
        ERR("fgets");
    if(command[strlen(command) - 1] == '\n')
        command[strlen(command) - 1] = '\0';

    if(!strcmp(command, "exit")){
        pthread_join(*(indexArgs->tid), NULL);
        exit(EXIT_SUCCESS);
    }
    else if(!strcmp(command, "exit!")){
        pthread_cancel(*(indexArgs->tid));
        exit(EXIT_SUCCESS);
    }
    else if(!strcmp(command, "index")){
        indexCommand(indexArgs);
    }
    else if(!strcmp(command, "count")){
        countCommand(files, *(indexArgs->n));
    }
    else if(strstr(command, "largerthan ")){
        if((x = atoi(command + strlen("longerthan "))) < 0){
            printf("Niepoprawna komenda! Uzyj 'help', aby sprwadzic dostepne komendy'\n"); return;
        }

        largerthanCommand(x, files, *(indexArgs->n));
    }
    else if(strstr(command, "namepart ")){
        namepart = command + strlen("namepart ");
        if(!namepart){
            printf("Niepoprawna komenda! Uzyj 'help', aby sprwadzic dostepne komendy'\n"); return;
        }

        namepartCommand(namepart, files, *(indexArgs->n));
    }
    else if(strstr(command, "owner ")){
        if((owner_id = (uid_t)atoi(command + strlen("owner "))) < 0){
            printf("Niepoprawna komenda! Uzyj 'help', aby sprwadzic dostepne komendy'\n"); return;
        }

        ownerCommand(owner_id, files, *(indexArgs->n));
    }
    else if(strstr(command, "help")){
        helpCommand();
    }
    else{
        printf("Niepoprawna komenda! Uzyj 'help', aby sprawdzic dostepne komendy.\n");
    }
}
