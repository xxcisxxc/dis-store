#include <time.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
#define DIRPM "/mnt/pmem"

#define N_EXPR 1

#define USEC_SEC 1000

static inline void die(char *msg, int type)
{
	if (type)
		perror(msg);
	else
		fprintf(stderr, "%s\n", msg);
	exit(1);
}

static size_t size_read;
static void *addr;

static void *buffers;

void *write_thread_d(void *arg)
{
    if (write(*(int *)arg, addr, size_read) != size_read)
        die("Not enough write!", 1);
    fsync(*(int *)arg);
    return NULL;
}

void *write_thread_r(void *arg)
{
    memcpy(arg, addr, size_read);
    return NULL;
}

void *write_thread_p(void *arg)
{
    pmem_memcpy_persist(arg, addr, size_read);
    pmem_persist(arg, size_read);
    return NULL;
}

void *read_disk_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "%s/disk%d.test", DIRDISK, (int)arg);
    int fd_write = open(name_buf, O_RDONLY);
    if (read(fd_write, addr, size_read) != size_read)
        die("Not enough Read!", 1);
    close(fd_write);
    return NULL;
}

void *read_ramdisk_thread(void *arg)
{
    char name_buf[100];
    sprintf(name_buf, "%s/write%d.test", DIRRAMDISK, (int)arg);
    int fd_write = open(name_buf, O_RDONLY);
    if (read(fd_write, addr, size_read) != size_read)
        die("Not enough Read!", 1);
    close(fd_write);
    return NULL;
}

void *read_dram_thread(void *arg)
{
    void *buf = buffers + (int)arg * size_read;
    memcpy(addr, buf, size_read);
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
        n_threads = atoi(argv[1]);
    /* Open correspoding number of threads */    
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t)*n_threads);

    /* Time spent between processes */
    double total_t, begin, end;
    double *spent = (double *)
        mmap(NULL, sizeof(double), PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);

    int i;

    /*** DISK ***/
    printf("Start test DISK WRITE\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        int k = i;
        if (!fork()) {
            int *fd_writes = (int *)malloc(sizeof(int)*n_threads);
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/disk%d_%d.test", DIRDISK, k, i);
                fd_writes[i] = creat(name_buf, 0666);
            }
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, write_thread_d, fd_writes+i);
            }
            for (i = 0; i < n_threads; i++) {
                pthread_join(threads[i], NULL);
            }
            end = clock();
            total_t = (end - begin) / CLOCKS_PER_SEC;
            *spent = total_t * USEC_SEC;
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/disk%d_%d.test", DIRDISK, k, i);
                close(fd_writes[i]);
                remove(name_buf);
            }
            free(fd_writes);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test DISK WRITE\n", total_t/N_EXPR);

    /*** RAMDISK ***/
    printf("Start test RAMDISK WRITE\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        int k = i;
        if (!fork()) {
            int *fd_writes = (int *)malloc(sizeof(int)*n_threads);
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/ramd%d_%d.test", DIRRAMDISK, k, i);
                fd_writes[i] = creat(name_buf, 0666);
            }
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, write_thread_d, fd_writes+i);
            }
            for (i = 0; i < n_threads; i++) {
                pthread_join(threads[i], NULL);
            }
            end = clock();
            total_t = (end - begin) / CLOCKS_PER_SEC;
            *spent = total_t * USEC_SEC;
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/ramd%d_%d.test", DIRRAMDISK, k, i);
                close(fd_writes[i]);
                remove(name_buf);
            }
            free(fd_writes);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test RAMDISK WRITE\n", total_t/N_EXPR);

    /*** DRAM (may be deleted) ***/
    printf("Start test DRAM WRITE\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        int k = i;
        if (!fork()) {
            buffers = malloc(size_read);
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, write_thread_r, buffers);
            }
            for (i = 0; i < n_threads; i++) {
                pthread_join(threads[i], NULL);
            }
            end = clock();
            total_t = (end - begin) / CLOCKS_PER_SEC;
            *spent = total_t * USEC_SEC;
            free(buffers);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test DRAM WRITE\n", total_t/N_EXPR);

    /*** PMEM ***/
    printf("Start test PMEM WRITE\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        int k = i;
        if (!fork()) {
            void **pmemaddrs = malloc(n_threads);
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/pmem%d_%d.test", DIRPM, k, i);
                size_t mapped_size;
                int is_pmem;
                pmemaddrs[i] = pmem_map_file(name_buf, size_read, 
                    PMEM_FILE_CREATE, 0666, &mapped_size, &is_pmem);
                if (pmemaddrs[i] == NULL)
                    die("Can't map pmem file", 1);
                if (mapped_size != size_read)
                    die("Not fully mapped", 0);
                if (is_pmem != 1)
                    die("Not a pmem", 0);
            }
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, write_thread_p, pmemaddrs[i]);
            }
            for (i = 0; i < n_threads; i++) {
                pthread_join(threads[i], NULL);
            }
            end = clock();
            total_t = (end - begin) / CLOCKS_PER_SEC;
            *spent = total_t * USEC_SEC;
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/pmem%d_%d.test", DIRPM, k, i);
                pmem_unmap(pmemaddrs[i], size_read);
                remove(name_buf);
            }
            free(pmemaddrs);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test PMEM WRITE\n", total_t/N_EXPR);

    munmap(addr, size_read);

    /***************
     ***************
     **Start Read***
     ***************
     ***************/
    
    /*addr = mmap(NULL, size_read, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (addr == NULL)
        die("Memory Mapped", 1);*/

    /*** DISK ***/
    /*printf("Start Read DISK\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, read_disk_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("Disk Read time is %f seconds\n", time_spent);*/

    /*** RAMDISK ***/
    /*printf("Start test RAMDISK\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, read_ramdisk_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("Ramdisk Read time is %f seconds\n", time_spent);*/

    /*** DRAM ***/
    /*printf("Start test DRAM\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, read_dram_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("DRAM Read time is %f seconds\n", time_spent);*/

    /*** PMEM ***/
    /*printf("Start test PMEM\n");
    begin = clock();
    for (i = 0; i < n_threads; i++) {
        pthread_create(threads+i, NULL, read_pmem_thread, (void *)i);
    }
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    end = clock();
    time_spent = (end - begin) / CLOCKS_PER_SEC;
    printf("PMEM Read time is %f seconds\n", time_spent);*/

    //munmap(addr, size_read);
    free(threads);
    //free(buffers);

    return 0;
}
