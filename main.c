#define _GNU_SOURCE

#include <sys/mman.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>

//A=264;B=0x64F542D6;C=mmap;D=103;E=118;F=nocache;G=135;H=seq;I=98;J=sum;K=flock

#define MEMORY_SIZE (264 * 1024 * 1024)
#define MEMORY_ADDRESS 0x64F542D6
#define MEMORY_THREAD_COUNT 103

#define FILE_SIZE (118 * 1024 * 1024)
#define FILE_BLOCK_SIZE 135
#define FILE_READ_THREAD_COUNT 98

struct filling_stat { FILE* random_device; uint64_t address; size_t count; };
void* part_one_thread_func(void* args) {
    struct filling_stat* stat = (struct filling_stat*)args;
    fread((void*)stat->address, 1, stat->count, stat->random_device);
}

void part_one() {
// 1 breakpoint
    void* mem = mmap((void*)MEMORY_ADDRESS, MEMORY_SIZE, PROT_WRITE | PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED) {
        printf("so sad, mapping error: %i", errno);
        exit(EXIT_FAILURE);
    }
// 2 breakpoint
    FILE* file = fopen("/dev/urandom", "r");
    pthread_t threads[MEMORY_THREAD_COUNT];
    struct filling_stat* stats = malloc(sizeof(struct filling_stat) * MEMORY_THREAD_COUNT);
    size_t bytes = MEMORY_SIZE / MEMORY_THREAD_COUNT;
    int64_t left = MEMORY_SIZE % MEMORY_THREAD_COUNT;
    for (int i = 0; i < MEMORY_THREAD_COUNT; ++i) {
        stats[i].random_device = file;
        stats[i].address = MEMORY_ADDRESS + i * bytes + (left-- > 0 ? 1 : 0);
        stats[i].count = stats[i].address - MEMORY_SIZE;
        pthread_create(&threads[i], NULL, part_one_thread_func, (void*)&stats[i]);
    }

    for (int i = 0; i < MEMORY_THREAD_COUNT; ++i) {
        pthread_join(threads[i], NULL);
    }
    free(stats);

    fclose(file);
// 3 breakpoint
    munmap((void*)MEMORY_ADDRESS, MEMORY_SIZE);
// 4 breakpoint
}

void* part_two_thread_write(void* args) {
    struct { int* fds; uint32_t fd_count; } *temp_struct = args;

    int* fds = temp_struct->fds;
    uint32_t fd_count = temp_struct->fd_count;

    FILE* random_device = fopen("/dev/urandom", "r");
    struct flock* lock = calloc(fd_count, sizeof(struct flock));

    for (;;) {
        for (int i = 0; i < fd_count; ++i) {
            lock[i].l_type = F_RDLCK;
            fcntl(fds[i], F_OFD_SETLKW, &lock[i]);

            lseek(fds[i], 0, SEEK_SET);

/*
* Under Linux 2.4, transfer sizes, and the alignment of the user buffer and the file offset must
* all be multiples of the logical block size of the file system.
* Under Linux 2.6, alignment to 512-byte boundaries suffices.
*                   ????
*/
            void* buffer = valloc(FILE_BLOCK_SIZE);
            for (int j = 0; j < FILE_SIZE / FILE_BLOCK_SIZE + 1; ++j) {
                fread(buffer, 1, FILE_BLOCK_SIZE, random_device);
                write(fds[i], buffer, ((j + 1) * FILE_BLOCK_SIZE > FILE_SIZE) ? FILE_SIZE - j * FILE_BLOCK_SIZE : FILE_BLOCK_SIZE);
            }
            lseek(fds[i], 0, SEEK_SET);

            lock[i].l_type = F_UNLCK;
            fcntl(fds[i], F_OFD_SETLKW, &lock[i]);
        }
    }

    free(lock);
    fclose(random_device);
}

void* part_two_thread_read(void* args) {
    int fd = *(int*)args;

    struct flock lock;
    memset(&lock, 0, sizeof(struct flock));
    lock.l_type = F_WRLCK;
    fcntl(fd, F_OFD_SETLKW, &lock);

    uint64_t sum = 0;
    lseek(fd, 0, SEEK_SET);
    for (;;) {
        unsigned char buffer[FILE_BLOCK_SIZE] = {0};
        int ret = read(fd, buffer, FILE_BLOCK_SIZE);

        for (int j = 0; j < FILE_BLOCK_SIZE; ++j) {
            sum += buffer[j];
        }

        //if (ret == 0) break;
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_OFD_SETLKW, &lock);
}

void part_two() {
    uint32_t files_count = (MEMORY_SIZE / FILE_SIZE) + 1;
    int* fds = malloc(sizeof(int) * files_count);

    char file_name[128] = {0};
    for (int i = 0; i < files_count; ++i) {
        sprintf(file_name, "%i", i);
        fds[i] = open(file_name, O_RDWR | O_CREAT | O_TRUNC | O_DIRECT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fds[i] == -1) {
            perror("bad things happen");
        }
    }

    struct { int* fds; uint32_t fd_count; } f_args = {.fds = fds, .fd_count = files_count};
    pthread_t write_thread;
    pthread_create(&write_thread, NULL, part_two_thread_write, &f_args);

    pthread_t read_threads[FILE_READ_THREAD_COUNT];
    for (int i = 0; i < FILE_READ_THREAD_COUNT; ++i) {
        uint32_t thread_fd = i % files_count;
        pthread_create(&read_threads[i], NULL, part_two_thread_read, &(fds[thread_fd]));
    }

    for (int i = 0; i < FILE_READ_THREAD_COUNT; ++i) {
        pthread_join(read_threads[i], NULL);
    }

    pthread_join(write_thread, NULL);
    free(fds);
}

int main() {
    part_one();

    part_two();

    return EXIT_SUCCESS;
}