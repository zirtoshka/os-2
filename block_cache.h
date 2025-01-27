#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct
{
    off_t offset;
    char *data;
    size_t size;
    int valid;
    int access_count;
    int dirty;
} CacheBlock;

typedef struct
{
    CacheBlock *blocks;
    size_t block_count;
    size_t block_size;
    pthread_mutex_t lock;
    int fd;
} BlockCache;

int lab2_open(const char *path);
int lab2_close(int fd);
ssize_t lab2_read(int fd, void *buf, size_t count);
ssize_t lab2_write(int fd, const void *buf, size_t count);
off_t lab2_lseek(int fd, off_t offset, int whence);
int lab2_fsync(int fd);

#endif // BLOCK_CACHE_H
