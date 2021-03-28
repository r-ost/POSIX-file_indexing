#include "my_nftw.h"

void* index_action(void* voidArgs);
void* clock_thread(void* voidArgs);
int walk(const char* name, const struct stat* s, file_info_t* files, int* n, file_info_t* file, struct FTW *f);
void writeToIndexFile(indexThreadArgs_t* args, file_info_t* files, int n);