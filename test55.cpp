#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "block_cache.h"

#define CACHE_BLOCK_COUNT 128
#define FILE_PATH "test_filik.dat"
#define TEST_SIZE 10000
#define BLOCK_SIZE 4096

double get_time()
{
    using namespace std::chrono;
    auto start = high_resolution_clock::now();

    // что нужно замерить

    auto end = high_resolution_clock::now();
    return duration_cast<milliseconds>(end - start).count();
}

void test_with_smth_cache(int fl)
{
    int fd = open(FILE_PATH, O_RDWR | O_CREAT | O_SYNC, 0666);

    if (fd < 0)
    {
        perror("open");
        exit(1);
    }

    fcntl(fd, F_NOCACHE, fl);

    char buffer[BLOCK_SIZE];
    memset(buffer, 'A', BLOCK_SIZE);

    using namespace std::chrono;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < TEST_SIZE; i++)
    {
        off_t offset = i * BLOCK_SIZE; 
        lseek(fd, offset, SEEK_SET);
        write(fd, buffer, BLOCK_SIZE);
    }

    fsync(fd);
    auto end = high_resolution_clock::now();
    if (fl)
    {
        std::cout << "Time without cache: ";
    }
    else
    {
        std::cout << "Time with standard cache: ";
    }
    std::cout << duration_cast<milliseconds>(end - start).count() << " ms\n";

    close(fd);
}

void test_with_standard_cache()
{
    test_with_smth_cache(0);
}
void test_without_cache()
{
    test_with_smth_cache(1);
}

void test_with_custom_cache()
{
    int fd = lab2_open(FILE_PATH);
    if (fd < 0)
    {
        perror("lab2_open");
        exit(1);
    }

    char buffer[BLOCK_SIZE];
    memset(buffer, 'B', BLOCK_SIZE);

    using namespace std::chrono;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < TEST_SIZE; i++)
    {
        off_t offset = i * BLOCK_SIZE; 
        lab2_lseek(fd, offset, SEEK_SET);
        lab2_write(fd, buffer, BLOCK_SIZE);
    }

    lab2_fsync(fd);
    auto end = high_resolution_clock::now();
    std::cout << "Time with custom cache: "
              << duration_cast<milliseconds>(end - start).count() << " ms\n";

    lab2_close(fd);
}

void test_with_random_access_and_smth_cache(int fl)
{
    int fd = open(FILE_PATH, O_RDWR | O_CREAT | O_SYNC, 0666);
    if (fd < 0)
    {
        perror("open");
        exit(1);
    }
    fcntl(fd, F_NOCACHE, fl);

    char buffer[BLOCK_SIZE];
    memset(buffer, 'A', BLOCK_SIZE);

    using namespace std::chrono;
    auto start = high_resolution_clock::now();

    const int access_count = TEST_SIZE;

    std::vector<int> access_frequency(CACHE_BLOCK_COUNT, 0);

    for (int i = 0; i < access_count; ++i)
    {
        off_t offset = (rand() % CACHE_BLOCK_COUNT) * BLOCK_SIZE;
        lseek(fd, offset, SEEK_SET);

        write(fd, buffer, BLOCK_SIZE);

        int block_index = offset / BLOCK_SIZE;
        access_frequency[block_index]++;
    }

    fsync(fd);
    auto end = high_resolution_clock::now();

    if (fl)
    {
        std::cout << "Time with random access and no cache:";
    }
    else
    {
        std::cout << "Time with random access and standard cache: ";
    }

    std::cout << duration_cast<milliseconds>(end - start).count() << " ms\n";

    // std::cout << "Access frequency to blocks:\n";
    // for (size_t i = 0; i < CACHE_BLOCK_COUNT; ++i)
    // {
    //     std::cout << "Block " << i << ": " << access_frequency[i] << " accesses\n";
    // }

    close(fd);
}

void test_with_random_access_and_standard_cache()
{
    test_with_random_access_and_smth_cache(0);
}
void test_with_random_access_and_no_cache()
{
    test_with_random_access_and_smth_cache(1);
}

void test_with_random_access_and_lfu()
{
    int fd = lab2_open(FILE_PATH);
    if (fd < 0)
    {
        perror("lab2_open");
        exit(1);
    }

    char buffer[BLOCK_SIZE];
    memset(buffer, 'A', BLOCK_SIZE);

    using namespace std::chrono;
    auto start = high_resolution_clock::now();

    const int access_count = TEST_SIZE;

    std::vector<int> access_frequency(CACHE_BLOCK_COUNT, 0);

    for (int i = 0; i < access_count; ++i)
    {
        off_t offset = (rand() % CACHE_BLOCK_COUNT) * BLOCK_SIZE;
        lab2_lseek(fd, offset, SEEK_SET);
        lab2_write(fd, buffer, BLOCK_SIZE);
        int block_index = offset / BLOCK_SIZE;
        access_frequency[block_index]++;
    }

    lab2_fsync(fd);
    auto end = high_resolution_clock::now();

    std::cout << "Time with random access and castome cache: "
              << duration_cast<milliseconds>(end - start).count() << " ms\n";

    // std::cout << "Access frequency to blocks:\n";
    // for (size_t i = 0; i < CACHE_BLOCK_COUNT; ++i)
    // {
    //     std::cout << "Block " << i << ": " << access_frequency[i] << " accesses\n";
    // }

    lab2_close(fd);
}

int main()
{
    std::cout << "Starting tests...\n";

    test_with_standard_cache();
    test_with_custom_cache();
    test_without_cache();
    std::cout << "\n";
    test_with_random_access_and_standard_cache();
    test_with_random_access_and_lfu();
    test_with_random_access_and_no_cache();

    std::cout << "Tests completed.\n";

    return 0;
}
