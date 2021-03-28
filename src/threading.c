// Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów uczenia się 
// z przedmiotu SOP została wykonana przez emnie samodzielnie. Jan Szablanowski, 305893

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "utils.h"

int walk(const char* name, const struct stat* s, file_info_t* files, int* n, file_info_t* file, struct FTW *f)
{
    if(*n >= MAX_FILES)
        return 0;

    if(file->file_type != OTHER){
       files[(*n)++] = *file;
    }
    return 0;
}


void* index_action(void* voidArgs)
{
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    indexThreadArgs_t* args = voidArgs;

    pthread_mutex_lock(args->mxRunning);
    *(args->running) = 1;
    pthread_mutex_unlock(args->mxRunning);

    pthread_mutex_lock(args->mxFiles);
    file_info_t files[MAX_FILES];
    for(int i = 0; i < *(args->n); i++)
        files[i] = args->files[i]; // copy array of files
    int n = 0;
    pthread_mutex_unlock(args->mxFiles);


    sleep(5);


    if(my_nftw(args->path, walk, files, &n , MAXFD,  FTW_PHYS))
        printf("No access to directory!\n");

    writeToIndexFile(args, files, n);
    printf("Indexing finished.\n");
    *(args->running) = 0;

    if(args->clockThread->t != -1){
        if(pthread_create(args->clockThread->tid, NULL, clock_thread, args->clockThread))
            ERR("pthread_create");
    }

    return NULL;
}


void writeToIndexFile(indexThreadArgs_t* args, file_info_t* files, int n)
{
    int index_file = -1;
    if((index_file = TEMP_FAILURE_RETRY(open(args->index_path, O_WRONLY|O_CREAT|O_TRUNC|O_APPEND, 0777))) < 0)
        ERR("open");
    
    pthread_mutex_lock(args->mxFiles);
    for(int i = 0; i < n; i++)
    {
        args->files[i] = files[i];  // copy array back

        if(bulk_write(index_file, files[i].filename, MAX_NAME) < 0)             ERR("write");
        if(bulk_write(index_file, files[i].path, MAX_PATH) < 0)                 ERR("write");
        if(bulk_write(index_file, &files[i].size, sizeof(size_t)) < 0)          ERR("write");
        if(bulk_write(index_file, &files[i].owner_uid , sizeof(uid_t)) < 0)     ERR("write");
        if(bulk_write(index_file, &files[i].file_type, sizeof(type)) < 0)       ERR("write");
    }
    *(args->n) = n;
    pthread_mutex_unlock(args->mxFiles);
    
    if(TEMP_FAILURE_RETRY(close(index_file)))
        ERR("close");
}


void* clock_thread(void* voidArgs)
{
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    clockThreadArgs_t* args = voidArgs;
    struct timespec ts = {args->t, 0};

    nanosleep(&ts, NULL);

    if(pthread_create(args->indexThread->tid, NULL, index_action, args->indexThread))
        ERR("pthread_create");
    
    return NULL;
}