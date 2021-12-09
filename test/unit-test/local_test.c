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

#define N_EXPR 10

#define USEC_SEC 1000

static inline void die(char *msg, int type)
{
	if (type)
		perror(msg);
	else
		fprintf(stderr, "%s\n", msg);
	exit(1);
}

static size_t size_file;
static void *addr_write;
static void *addr_read;

void *write_thread_d(void *arg)
{
    if (write(*(int *)arg, addr_write, size_file) != size_file)
        die("Not enough write!", 1);
    fsync(*(int *)arg);
    return NULL;
}

void *write_thread_r(void *arg)
{
    double total_t = 0, begin, end, spent;
    printf("size: %ld\n", size_file);
    begin = clock();
    memcpy(arg, addr_write, size_file);
    end = clock();
    spent = (end - begin) / CLOCKS_PER_SEC;
    total_t += spent * USEC_SEC;
    printf("memcpy time: %f\n", spent * USEC_SEC);
    printf("total time: %f\n", total_t);
    return NULL;
}

void *write_thread_p(void *arg)
{
    pmem_memcpy_persist(arg, addr_write, size_file);
    pmem_persist(arg, size_file);
    return NULL;
}

void *read_thread_d(void *arg)
{
    if (read(*(int *)arg, addr_read, size_file) != size_file)
        die("Not enough Read!", 1);
    return NULL;
}

void *read_thread_m(void *arg)
{
    memcpy(addr_read, arg, size_file);
    return NULL;
}

int main(int argc, char *argv[])
{
    /****************
     ****************
     **Prepare test**
     ****************
     ****************/
    
    /* Open test bench into memory-mapped file */
    int fd_read = open("test.bench", O_RDONLY);
    if (fd_read < 0)
        die("Open failed", 1);
    size_file = lseek(fd_read, 0, SEEK_END);
    addr_write = mmap(NULL, size_file, PROT_READ, MAP_PRIVATE, fd_read, 0);
    if (addr_write == NULL)
        die("Memory Mapped", 1);
    close(fd_read);
    /* Claim a new memory for copy to */
    addr_read = mmap(NULL, size_file, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (addr_read == NULL)
        die("Memory Mapped", 1);

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

    /****************
     ****************
     ******DISK******
     ****************
     ****************/
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
                close(fd_writes[i]);
            }
            free(fd_writes);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test DISK WRITE\n", total_t/N_EXPR);

    printf("Start test DISK READ\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        int k = i;
        if (!fork()) {
            int *fd_reads = (int *)malloc(sizeof(int)*n_threads);
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/disk%d_%d.test", DIRDISK, k, i);
                fd_reads[i] = open(name_buf, O_RDONLY);
            }
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, read_thread_d, fd_reads+i);
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
                close(fd_reads[i]);
                remove(name_buf);
            }
            free(fd_reads);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test DISK READ\n\n", total_t/N_EXPR);

    /****************
     ****************
     *****RAMDISK****
     ****************
     ****************/
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
                close(fd_writes[i]);
            }
            free(fd_writes);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test RAMDISK WRITE\n", total_t/N_EXPR);

    printf("Start test RAMDISK READ\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        int k = i;
        if (!fork()) {
            int *fd_reads = (int *)malloc(sizeof(int)*n_threads);
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/ramd%d_%d.test", DIRRAMDISK, k, i);
                fd_reads[i] = open(name_buf, O_RDONLY);
            }
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, read_thread_d, fd_reads+i);
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
                close(fd_reads[i]);
                remove(name_buf);
            }
            free(fd_reads);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test RAMDISK READ\n\n", total_t/N_EXPR);

    /****************
     ****************
     ******DRAM******
     ****************
     ****************/
    printf("Start test DRAM WRITE\n");
    total_t = 0;
    void *buffers = malloc(size_file);
    for (i = 0; i < N_EXPR; i++) {
        if (!fork()) {
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
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test DRAM WRITE\n", total_t/N_EXPR);

    printf("Start test DRAM READ\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        if (!fork()) {
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, read_thread_m, buffers);
            }
            for (i = 0; i < n_threads; i++) {
                pthread_join(threads[i], NULL);
            }
            end = clock();
            total_t = (end - begin) / CLOCKS_PER_SEC;
            *spent = total_t * USEC_SEC;
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    free(buffers);
    printf("%f\nEnd test DRAM READ\n\n", total_t/N_EXPR);

    /****************
     ****************
     ******PMEM******
     ****************
     ****************/
    printf("Start test PMEM WRITE\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        int k = i;
        if (!fork()) {
            void **pmemaddr_writes = malloc(n_threads*sizeof(void *));
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/pmem%d_%d.test", DIRPM, k, i);
                size_t mapped_size;
                int is_pmem;
                pmemaddr_writes[i] = pmem_map_file(name_buf, size_file, 
                    PMEM_FILE_CREATE, 0666, &mapped_size, &is_pmem);
                if (pmemaddr_writes[i] == NULL)
                    die("Can't map pmem file", 1);
                if (mapped_size != size_file)
                    die("Not fully mapped", 0);
                /*if (is_pmem != 1)
                    die("Not a pmem", 0);*/
            }
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, write_thread_p, pmemaddr_writes[i]);
            }
            for (i = 0; i < n_threads; i++) {
                pthread_join(threads[i], NULL);
            }
            end = clock();
            total_t = (end - begin) / CLOCKS_PER_SEC;
            *spent = total_t * USEC_SEC;
            for (i = 0; i < n_threads; i++) {
                pmem_unmap(pmemaddr_writes[i], size_file);
            }
            free(pmemaddr_writes);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test PMEM WRITE\n", total_t/N_EXPR);

    printf("Start test PMEM READ\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        int k = i;
        if (!fork()) {
            void **pmemaddr_reads = malloc(n_threads*sizeof(void *));
            for (i = 0; i < n_threads; i++) {
                char name_buf[100];
                sprintf(name_buf, "%s/pmem%d_%d.test", DIRPM, k, i);
                size_t mapped_size;
                int is_pmem;
                pmemaddr_reads[i] = pmem_map_file(name_buf, size_file, 
                    PMEM_FILE_CREATE, 0666, &mapped_size, &is_pmem);
                if (pmemaddr_reads[i] == NULL)
                    die("Can't map pmem file", 1);
                if (mapped_size != size_file)
                    die("Not fully mapped", 0);
                /*if (is_pmem != 1)
                    die("Not a pmem", 0);*/
            }
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, read_thread_m, pmemaddr_reads[i]);
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
                pmem_unmap(pmemaddr_reads[i], size_file);
                remove(name_buf);
            }
            free(pmemaddr_reads);
            exit(0);
        } else {
            wait(NULL);
            total_t += *spent;
        }
    }
    printf("%f\nEnd test PMEM READ\n", total_t/N_EXPR);

    munmap(addr_write, size_file);
    munmap(addr_read, size_file);

    free(threads);

    return 0;
}
