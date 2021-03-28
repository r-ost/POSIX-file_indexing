// Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów uczenia się 
// z przedmiotu SOP została wykonana przez emnie samodzielnie. Jan Szablanowski, 305893

#define _GNU_SOURCE
#include "utils.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>


void get_options(int argc, char** argv, char* dir_path, char* index_path, int* t)
{
    char c;
    while(-1 != (c = getopt(argc, argv, "d:f:t:")))
    {
        switch(c)
        {
            case 'd':
                strcpy(dir_path, optarg);
                break;
            case 'f':
                strcpy(index_path, optarg);
                break;
            case 't':
                *t = atoi(optarg);
                if(*t < 30 || *t > 7200)
                  usage();
                break;
            case '?':
            default:
                usage();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///// Commands handling /////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void indexCommand(indexThreadArgs_t* indexArgs)
{
    if(*(indexArgs->running) != 0)
    {
        printf("Indexing already in progress. Wait...\n");
        return;        
    }
    
    if(indexArgs->clockThread->t != -1){
        if(pthread_cancel(*(indexArgs->clockThread->tid)))
            ERR("pthread_cancel");
    }

    pthread_join(*(indexArgs->tid), NULL);
    if(pthread_create(indexArgs->tid, NULL, index_action, indexArgs))
        ERR("pthread_create");
}

void countCommand(file_info_t* files, int fileCount)
{
    int counts[FILE_TYPES] = {0};
    for(int i = 0; i < fileCount; i++){
        if((int)files[i].file_type < (int)D)
            counts[(int)files[i].file_type] += 1;
        else
            counts[FILE_TYPES - 1] += 1;
    }
    printf("JPEGs: %d\nPNGs: %d\nGZIPs: %d\nZIPs: %d\nDirectories: %d\n",counts[0],counts[1],
            counts[2],counts[3],counts[4]);
}

void largerthanCommand(int x, file_info_t* files, int fileCount)
{   
    char output[MAX_PATH*MAX_FILES];
    output[0] = '\0';
    int lines = 0;
    char* c;
    
    for(int i = 0; i < fileCount; i++)
    {
        file_info_t f = files[i]; 
        if(f.size > x ){
            lines++;
            c = get_file_info(&f);
            strcat(output,c);
            free(c);
        }
    }
    print_file_info(output, lines);
}

void namepartCommand(char* namepart, file_info_t* files, int fileCount)
{
    char output[MAX_PATH*MAX_FILES];
    output[0] = '\0';
    int lines = 0;
    char* c;

    for(int i = 0; i < fileCount; i++)
    {
        file_info_t f = files[i];
        if(strstr(f.path, namepart)){
            lines++;
            c = get_file_info(&f);
            strcat(output,c);
            free(c);
        }   
    }
    print_file_info(output, lines);
}

void ownerCommand(uid_t owner_id, file_info_t* files, int fileCount)
{
    char output[MAX_PATH*MAX_FILES];
    output[0] = '\0';
    int lines = 0;
    char* c;

    for(int i = 0; i < fileCount; i++)
    {
        file_info_t f = files[i]; 
        if(f.owner_uid == owner_id){
            lines++;
            c = get_file_info(&f);
            strcat(output,c);
            free(c);
        }
    }
    print_file_info(output, lines);
}

void helpCommand()
{
    printf("Dostepne komendy:\n- exit - wyjdz z programu\n- exit! - natychmiast wyjdz z programu");
    printf("\n- index - uruchom indeksowanie w tle\n- count - pokazuje, ile jest plikow poszczegolnych plikow");
    printf("\n- largerthan x - wypisuje informacje o plikach wiekszych niz x bajtow");
    printf("\n- namepart y - wypisuje informacje o plikach, ktore maja w sciezce fragment y");
    printf("\n- owner uid - wypisuje informacje o plikach, ktorych wlascicielem jest uzytkownik o podanym uid\n");
}

ssize_t bulk_read(int fd, void *buf, size_t count)
{
	ssize_t c;
	ssize_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(read(fd,buf,count));
		if(c<0) return c;
		if(c==0) return len; //EOF
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}

ssize_t bulk_write(int fd, void *buf, size_t count)
{
	ssize_t c;
	ssize_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(write(fd,buf,count));
		if(c<0) return c;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}

void usage() 
{
    fprintf(stderr,"USAGE: [-d katalog_do_zindeksowania -f sciezka_do_pliku_z_indeksami -t czas_cyklicznych_indeksowan]\n");
    exit(EXIT_FAILURE);
}

void print_file_info(char* output, int lines)
{
    char* printingProgram = getenv("PAGER");

    if(lines > MAX_PRINTING_LINES && printingProgram)
    {
        FILE* s = popen(printingProgram, "w");
        if(!s)
            ERR("popen");
        fprintf(s, "%s", output);
        pclose(s);
    }
    else
    {
        printf("%s", output);
    }
}

char* get_file_info(file_info_t* file)
{
    char* file_info_str = malloc(MAX_PATH + MAX_NAME);
    sprintf(file_info_str , "%-100s\t%d", file->path, (int)file->size);
    switch (file->file_type)
    {
        case JPEG: strcat(file_info_str, "\t.JPG\n")      ; break;
        case PNG:  strcat(file_info_str, "\t.PNG\n")      ; break;
        case GZIP: strcat(file_info_str, "\t.GZ\n")       ; break;
        case ZIP:  strcat(file_info_str, "\tZIP-file\n")  ; break;
        case D: case DNR: case DP:
                    strcat(file_info_str,"\tDIR\n")       ; break; 
        default: strcat(file_info_str,"\n")               ; break;
    }
    return file_info_str;
}