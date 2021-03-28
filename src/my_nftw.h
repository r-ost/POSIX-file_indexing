#include "structs.h"
#include <ftw.h>

int my_nftw(const char *path, int (*fn)(const char *, const struct stat *, file_info_t*, int* n, file_info_t*, struct FTW *), file_info_t* files, int* n, int fd_limit, int flags);