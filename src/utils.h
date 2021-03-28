#include "threading.h"

void get_options(int argc, char** argv, char* dir_path, char* index_path, int* t);

void indexCommand(indexThreadArgs_t* indexArgs);
void countCommand(file_info_t* files, int fileCount);
void largerthanCommand(int x, file_info_t* files, int fileCount);
void namepartCommand(char* namepart, file_info_t* files, int fileCount);
void ownerCommand(uid_t owner_id, file_info_t* files, int fileCount);
void helpCommand();

ssize_t bulk_read(int fd, void *buf, size_t count);
ssize_t bulk_write(int fd, void *buf, size_t count);
void usage();  
void print_file_info(char* output, int lines);
char* get_file_info(file_info_t* file);