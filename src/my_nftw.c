// Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów uczenia się 
// z przedmiotu SOP została wykonana przez emnie samodzielnie. Jan Szablanowski, 305893

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <ftw.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "utils.h"

////////////////////////////////////////////////////////////////////////////////////////////
///// It is an original nftw implementation changed to return specific info about file /////.
////////////////////////////////////////////////////////////////////////////////////////////

struct history
{
	struct history *chain;
	dev_t dev;
	ino_t ino;
	int level;
	int base;
};

#undef dirfd
#define dirfd(d) (*(int *)d)


static int do_nftw(char *path, int (*fn)(const char *, const struct stat *, file_info_t*,int *, file_info_t*, struct FTW *), file_info_t* files, int* n, int fd_limit, int flags, struct history *h)
{
	size_t l = strlen(path), j = l && path[l-1]=='/' ? l-1 : l;
	struct stat st;
	struct history new;
	type file_type;
	file_info_t file; 
	int r;
	int dfd;
	int err;
	struct FTW lev;

    int f;
    int count;
    unsigned char magic[12] = {0};

    const unsigned char jpeg_magic_num[2] = {0xFF, 0xD8};
    const unsigned char png_magic_num[8] =  {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    const unsigned char gzip_magic_num[2] = {0x1F, 0x8B};
    const unsigned char zip_magic_num[12] = {0x50, 0x4B, 0x03, 0x04,
                                             0x50, 0x4B, 0x05, 0x06,
                                             0x50, 0x4B, 0x07, 0x08};

	if ((flags & FTW_PHYS) ? lstat(path, &st) : stat(path, &st) < 0) {
		if (!(flags & FTW_PHYS) && errno==ENOENT && !lstat(path, &st))
			file_type = OTHER;
		else if (errno != EACCES){return -1;}
		else file_type = OTHER;
	} else if (S_ISDIR(st.st_mode)) {
		if (flags & FTW_DEPTH) file_type = DP;
		else file_type = D;
	} else if (S_ISLNK(st.st_mode)) {
		if (flags & FTW_PHYS) file_type = OTHER;
		else file_type = OTHER;
	} else {
        if((f = TEMP_FAILURE_RETRY(open(path, O_RDONLY))) < 0)
            return 0;
        if((count = bulk_read(f, (char*)magic, sizeof(magic))) < 0)
            ERR("read");
        if(TEMP_FAILURE_RETRY(close(f)))
            ERR("close");
        // JPEG
        if(memcmp(magic, jpeg_magic_num, sizeof(jpeg_magic_num)) == 0){
            file_type = JPEG;
        }
        // PNG
        else if(memcmp(magic, png_magic_num, sizeof(png_magic_num)) == 0){
            file_type = PNG;
        }
        // GZIP
        else if(memcmp(magic, gzip_magic_num, sizeof(gzip_magic_num)) == 0){
            file_type = GZIP;
        }
        // ZIP 
        else if(memcmp(magic, zip_magic_num, sizeof(zip_magic_num) / 3) == 0 ||
                memcmp(magic, zip_magic_num + sizeof(zip_magic_num) / 3, sizeof(zip_magic_num) / 3) == 0 ||
                memcmp(magic, zip_magic_num + 2*sizeof(zip_magic_num) / 3, sizeof(zip_magic_num) / 3) == 0){
            file_type = ZIP;
        }
        else{
            file_type = OTHER;
        }
    }

	if ((flags & FTW_MOUNT) && h && st.st_dev != h->dev)
		return 0;
	
	new.chain = h;
	new.dev = st.st_dev;
	new.ino = st.st_ino;
	new.level = h ? h->level+1 : 0;
	new.base = j+1;
	
	lev.level = new.level;
	if (h) {
		lev.base = h->base;
	} else {
		size_t k;
		for (k=j; k && path[k]=='/'; k--);
		for (; k && path[k-1]!='/'; k--);
		lev.base = k;
	}

	if (file_type == D || file_type == DP) {
		dfd = open(path, O_RDONLY);
		err = errno;
		if (dfd < 0 && err == EACCES) file_type = DNR;
		//if (!fd_limit) 
			close(dfd);
	}

	file.file_type = file_type;
	strcpy(file.filename, basename(path));
	if(strlen(file.filename) >= MAX_NAME - 1)
		printf("Nazwa pliku za dluga!\n");
	file.owner_uid = st.st_uid;
	strcpy(file.path,realpath(path,NULL));
	if(strlen(file.path) >= MAX_PATH - 1)
		printf("Nazwa sciezki za dluga!\n");
	file.size = st.st_size;

	if (!(flags & FTW_DEPTH) && (r=fn(path, &st, files, n, &file, &lev)))
		return r;

	for (; h; h = h->chain)
		if (h->dev == st.st_dev && h->ino == st.st_ino)
			return 0;

	if ((file_type == D || file_type == DP) && fd_limit) {
		if (dfd < 0) {
			errno = err;
	
			return -1;
		}
		DIR *d = opendir(path);
		if (d) {
			struct dirent *de;
			while ((de = readdir(d))) {
				if (de->d_name[0] == '.'
				 && (!de->d_name[1]
				  || (de->d_name[1]=='.'
				   && !de->d_name[2]))) continue;
				if (strlen(de->d_name) >= PATH_MAX-l) {
					errno = ENAMETOOLONG;
					closedir(d);
					return -1;
				}
				path[j]='/';
				strcpy(path+j+1, de->d_name);
				if ((r=do_nftw(path, fn, files, n, fd_limit-1, flags, &new))) {
					closedir(d);
					return r;
				}
			}
			closedir(d);
		} else {
			close(dfd);
			// printf("debug\n");
			return -1;
		}
	}

	path[l] = 0;
	if ((flags & FTW_DEPTH) && (r=fn(path, &st, files, n, &file, &lev)))
		return r;

	return 0;
}

int my_nftw(const char *path, int (*fn)(const char *, const struct stat *, file_info_t*, int* n, file_info_t*, struct FTW *), file_info_t* files, int* n, int fd_limit, int flags)
{
	int r, cs;
	size_t l;
	char pathbuf[PATH_MAX+1];

	if (fd_limit <= 0) return 0;

	l = strlen(path);
	if (l > PATH_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}
	memcpy(pathbuf, path, l+1);
	
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &cs);
	r = do_nftw(pathbuf, fn, files, n, fd_limit, flags, NULL);
	pthread_setcancelstate(cs, 0);

	return r;
}