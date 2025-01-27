#include "block_cache.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>

#define CACHE_BLOCK_COUNT 128
#define CACHE_BLOCK_SIZE 4096

static BlockCache cache;

CacheBlock *find_least_used_block()
{
    CacheBlock *least_used_block = &cache.blocks[0];
    for (size_t i = 1; i < cache.block_count; ++i)
    {
        if (cache.blocks[i].access_count < least_used_block->access_count)
        {
            least_used_block = &cache.blocks[i];
        }
    }
    return least_used_block;
}

void evict_cache_block()
{
    CacheBlock *least_used_block = find_least_used_block();

    // блок изменен -> записать обратно на диск
    if (least_used_block->valid && least_used_block->dirty)
    {
        pwrite(cache.fd, least_used_block->data, cache.block_size, least_used_block->offset);
    }

    // освободждение блока
    least_used_block->valid = 0;
    least_used_block->dirty = 0;
    least_used_block->access_count = 0;
    least_used_block->offset = -1;
}

CacheBlock *find_or_allocate_block(off_t offset)
{
    pthread_mutex_lock(&cache.lock);
    CacheBlock *lpu_block = NULL;

    for (size_t i = 0; i < cache.block_count; ++i)
    {
        if (cache.blocks[i].valid && cache.blocks[i].offset == offset)
        {
            cache.blocks[i].access_count++;
            pthread_mutex_unlock(&cache.lock);
            return &cache.blocks[i];
        }

        if (!lpu_block || cache.blocks[i].access_count < lpu_block->access_count)
        {
            lpu_block = &cache.blocks[i];
        }
    }

    if (lpu_block->valid && cache.block_count == CACHE_BLOCK_COUNT)
    {
        evict_cache_block();
    }
    if (lpu_block->valid && lpu_block->access_count > 0)
    {
        pwrite(cache.fd, lpu_block->data, cache.block_size, lpu_block->offset);
    }

    lpu_block->offset = offset;
    lpu_block->access_count = 1;
    lpu_block->valid = 1;
    lpu_block->dirty = 0;

    ssize_t bytes_read = pread(cache.fd, lpu_block->data, cache.block_size, offset);
    if (bytes_read < 0)
    {
        perror("pread failed");
        pthread_mutex_unlock(&cache.lock);
        return NULL;
    }
    else if (bytes_read < cache.block_size)
    {
        memset(lpu_block->data + bytes_read, 0, cache.block_size - bytes_read);
    }

    pthread_mutex_unlock(&cache.lock);
    return lpu_block;
}

int lab2_open(const char *path)
{
    int fd = open(path, O_RDWR | O_SYNC);
    if (fd < 0)
        return -1;

    fcntl(fd, F_NOCACHE, 1);

    cache.blocks = (CacheBlock *)calloc(CACHE_BLOCK_COUNT, sizeof(CacheBlock));
    for (size_t i = 0; i < CACHE_BLOCK_COUNT; ++i)
    {
        cache.blocks[i].data = (char *)malloc(CACHE_BLOCK_SIZE);
        cache.blocks[i].valid = 0;
    }
    cache.block_count = CACHE_BLOCK_COUNT;
    cache.block_size = CACHE_BLOCK_SIZE;
    pthread_mutex_init(&cache.lock, NULL);
    cache.fd = fd;

    return fd;
}

int lab2_close(int fd)
{
    if (fd != cache.fd)
        return -1;

    for (size_t i = 0; i < cache.block_count; ++i)
    {
        if (cache.blocks[i].valid)
        {
            pwrite(cache.fd, cache.blocks[i].data, cache.block_size, cache.blocks[i].offset);
            free(cache.blocks[i].data);
        }
    }

    free(cache.blocks);
    pthread_mutex_destroy(&cache.lock);
    close(fd);
    return 0;
}

ssize_t lab2_read(int fd, void *buf, size_t count)
{
    if (fd != cache.fd)
        return -1;

    size_t remaining = count;
    char *buf_ptr = (char *)buf;
    off_t offset = lseek(fd, 0, SEEK_CUR);

    while (remaining > 0)
    {
        off_t block_offset = (offset / cache.block_size) * cache.block_size;
        size_t block_offset_in_buf = offset % cache.block_size;
        size_t to_read = cache.block_size - block_offset_in_buf;
        if (to_read > remaining)
            to_read = remaining;

        CacheBlock *block = find_or_allocate_block(block_offset);
        if (!block)
            return -1;

        if (block_offset_in_buf + to_read > cache.block_size)
        {
            to_read = cache.block_size - block_offset_in_buf;
        }

        memcpy(buf_ptr, block->data + block_offset_in_buf, to_read);

        buf_ptr += to_read;
        offset += to_read;
        remaining -= to_read;
    }

    lseek(fd, offset, SEEK_SET);

    return count - remaining;
}

ssize_t lab2_write(int fd, const void *buf, size_t count)
{
    if (fd != cache.fd)
        return -1;

    size_t remaining = count;
    const char *buf_ptr = (const char *)buf;
    off_t offset = lseek(fd, 0, SEEK_CUR);

    while (remaining > 0)
    {
        off_t block_offset = (offset / cache.block_size) * cache.block_size;
        size_t block_offset_in_buf = offset % cache.block_size;
        size_t to_write = cache.block_size - block_offset_in_buf;
        if (to_write > remaining)
            to_write = remaining;

        CacheBlock *block = find_or_allocate_block(block_offset);
        if (!block)
            return -1;

        if (block_offset_in_buf + to_write > cache.block_size)
        {
            to_write = cache.block_size - block_offset_in_buf;
        }

        memcpy(block->data + block_offset_in_buf, buf_ptr, to_write);
        block->dirty = 1; 

        buf_ptr += to_write;
        offset += to_write;
        remaining -= to_write;
    }

    lseek(fd, offset, SEEK_SET);

    return count - remaining;
}

off_t lab2_lseek(int fd, off_t offset, int whence)
{
    if (fd != cache.fd)
        return -1;
    return lseek(fd, offset, whence);
}

int lab2_fsync(int fd)
{
    if (fd != cache.fd)
        return -1;

    pthread_mutex_lock(&cache.lock);
    for (size_t i = 0; i < cache.block_count; ++i)
    {
        if (cache.blocks[i].valid && cache.blocks[i].dirty)
        {
            pwrite(fd, cache.blocks[i].data, cache.block_size, cache.blocks[i].offset);
            cache.blocks[i].dirty = 0; 
        }
    }
    pthread_mutex_unlock(&cache.lock);

    return fsync(fd);
}
