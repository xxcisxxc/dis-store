#include <time.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <libpmem.h>

#define DIRDISK "."
#define DIRRAMDISK "/dev/shm"
#define DIRPM "/mnt/pmem0"

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

static void **buffers;

void *write_disk_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "%s/write%d.test", DIRDISK, (int)arg);
    int fd_write = creat(name_buf, 0666);
    if (write(fd_write, addr, size_read) != size_read)
        die_thread("Not enough write!", 1);
    close(fd_write);
    return NULL;
}

void *write_ramdisk_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "%s/write%d.test", DIRRAMDISK, (int)arg);
    int fd_write = creat(name_buf, 0666);
    if (write(fd_write, addr, size_read) != size_read)
        die_thread("Not enough write!", 1);
    close(fd_write);
    return NULL;
}

void *write_dram_thread(void *arg)
{
    void *buf = buffers[(int)arg] = malloc(size_read);
    memcpy(buf, addr, size_read);
    return NULL;
}

void *write_pmem_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "%s/write%d.test", DIRPM, (int)arg);
    size_t mapped_size;
    void *pmemaddr = pmem_map_file(name_buf, size_read, 
        PMEM_FILE_CREATE, 0666, &mapped_size, NULL);
    if (pmemaddr == NULL)
        die("Can't map pmem file", 1);
    if (mapped_size != size_read)
        die("Not fully mapped", 0);
    pmem_memcpy_persist(pmemaddr, addr, size_read);
    pmem_unmap(pmemaddr, mapped_size);
    return NULL;
}

void *read_disk_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "%s/write%d.test", DIRDISK, (int)arg);
    int fd_write = open(name_buf, O_RDONLY);
    if (read(fd_write, addr, size_read) != size_read)
        die_thread("Not enough Read!", 1);
    close(fd_write);
    return NULL;
}

void *read_ramdisk_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "%s/write%d.test", DIRRAMDISK, (int)arg);
    int fd_write = open(name_buf, O_RDONLY);
    if (read(fd_write, addr, size_read) != size_read)
        die_thread("Not enough Read!", 1);
    close(fd_write);
    return NULL;
}

void *read_dram_thread(void *arg)
{
    void *buf = buffers[(int)arg];
    memcpy(addr, buf, size_read);
    free(buf);
    return NULL;
}

void *read_pmem_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "%s/write%d.test", DIRPM, (int)arg);
    size_t mapped_size;
    void *pmemaddr = pmem_map_file(name_buf, size_read, 
        PMEM_FILE_CREATE, 0666, &mapped_size, NULL);
    if (pmemaddr == NULL)
        die("Can't map pmem file", 1);
    if (mapped_size != size_read)
        die("Can't fully map", 0);
    memcpy(addr, pmemaddr, size_read);
    pmem_unmap(pmemaddr, mapped_size);
    return NULL;
}

int main(int argc, char *argv[])
{
    /***************
     ***************
     **Start Write**
     ***************
     ***************/
    
    /* Open test bench into memory-mapped file */
    int fd_read = open("test.bench", O_RDONLY);
    if (fd_read < 0)
        die("Open failed", 1);
    size_read = lseek(fd_read, 0, SEEK_END);
    addr = mmap(NULL, size_read, PROT_READ, MAP_PRIVATE, fd_read, 0);
    if (addr == NULL)
        die("Memory Mapped", 1);
    close(fd_read);

    /* Open write file to write from memory-mapped file */
    int n_threads = 1;
    if (argc > 2)
        die("Usage: [prog_name] <nthreads[default: 1]>", 0);
    if (argc == 2)
        n_threads = atoi(argv[2]);
    
    buffers = malloc(n_threads);
    
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t)*n_threads);
    int i;
    double time_spent, begin, end;

    /*** DISK ***/
    printf("Start test DISK\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, write_disk_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("Disk Write time is %f seconds\n", time_spent);

    /*** RAMDISK ***/
    printf("Start test RAMDISK\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, write_ramdisk_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("Ramdisk Write time is %f seconds\n", time_spent);

    /*** DRAM ***/
    printf("Start test DRAM\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, write_dram_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("DRAM Write time is %f seconds\n", time_spent);

    /*** PMEM ***/
    printf("Start test PMEM\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, write_pmem_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("PMEM Write time is %f seconds\n", time_spent);

    munmap(addr, size_read);

    /***************
     ***************
     **Start Read***
     ***************
     ***************/
    
    addr = mmap(NULL, size_read, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == NULL)
        die("Memory Mapped", 1);

    /*** DISK ***/
    printf("Start Read DISK\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, read_disk_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("Disk Read time is %f seconds\n", time_spent);

    /*** RAMDISK ***/
    printf("Start test RAMDISK\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, read_ramdisk_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("Ramdisk Read time is %f seconds\n", time_spent);

    /*** DRAM ***/
    printf("Start test DRAM\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, read_dram_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("DRAM Read time is %f seconds\n", time_spent);

    /*** PMEM ***/
    printf("Start test PMEM\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, read_pmem_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("PMEM Read time is %f seconds\n", time_spent);

    munmap(addr, size_read);
    free(threads);
    free(buffers);

    return 0;
}
