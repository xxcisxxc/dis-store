#define USE_PMEM

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#ifdef USE_PMEM
#include <libpmem.h>
#endif

static inline void die(char *msg, int type)
{
	if (type)
		perror(msg);
	else
		fprintf(stderr, "%s\n", msg);
	exit(1);
}

static inline void die_thread(char *msg, int type)
{
	if (type)
		perror(msg);
	else
		fprintf(stderr, "%s\n", msg);
	pthread_exit(NULL);
}

static size_t size_read;
static void *addr;

void *read_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "write%d.test", (int)arg);
    int fd_write = open(name_buf, O_RDONLY);
    if (read(fd_write, addr, size_read) != size_read)
        die_thread("Not enough Read!", 1);
    close(fd_write);
    return NULL;
}

#ifdef USE_PMEM
void *read_pmem_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "write%d.test", (int)arg);
    size_t mapped_size;
    void *pmemaddr = pmem_map_file(name_buf, size_read, 
        PMEM_FILE_CREATE, 0666, &mapped_size, NULL);
    memcpy(addr, pmemaddr, size_read);
    pmem_unmap(pmemaddr, mapped_size);
    return NULL;
}
#endif

int main(int argc, char *argv[])
{
    /* Open an anonymous memory-mapped file */
    int fd_read = open("test.bench", O_RDONLY);
    if (fd_read < 0)
        die("Open failed", 1);
    size_read = lseek(fd_read, 0, SEEK_END);
    close(fd_read);
    addr = mmap(NULL, size_read, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == NULL)
        die("Memory Mapped", 1);

    /* Open write file to read to memory-mapped file */
    int n_threads = 1;
    if (argc > 2)
        die("Usage: [prog_name] <nthreads[default: 1]>", 0);
    if (argc == 2)
        n_threads = atoi(argv[1]);

    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t)*n_threads);
    int i;
    for (i = 0; i < n_threads; i++) {
#ifndef USE_PMEM
        pthread_create(threads+i, NULL, read_thread, (void *)i);
#else
        pthread_create(threads+i, NULL, read_pmem_thread, (void *)i);
#endif
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    munmap(addr, size_read);
    free(threads);
    return 0;
}