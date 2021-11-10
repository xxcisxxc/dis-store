#define USE_PMEM "/mnt/pmem0/test"

#include <time.h> 
#include <signal.h>
#include "setup_ib.h"

void *server_write_thread(void *args)
{
    printf("Start Write.\n");
    post_write_signaled();
    printf("Successful Write.\n");
    return NULL;
}

void *server_read_thread(void *args)
{
    printf("Start Read.\n");
    post_read_signaled();
    printf("Successful Read.\n");
    return NULL;
}

void sigint_handler(int signum)
{
    printf("disconnect\n");
}

int main(int argc, char *argv[])
{
    /* Open test bench into memory-mapped file */
    int fd_read = open("test.bench", O_RDWR);
    if (fd_read < 0)
        die("Open failed", 1);
    int size_read = lseek(fd_read, 0, SEEK_END);
    
    //close(fd_read);
    
    int n_threads = 1, is_server;
    char *sock_port, *server_name;
    if (argc < 3 || argc > 5)
        die("Usage: [prog_name] s/c sock_port (server_name) (n_threads)", 0);
    if (argv[1][0] == 's') {
        is_server = 1;
        if (argc != 3)
            die("Usage: [prog_name] s sock_port", 0);
        sock_port = argv[2];
        if (argc == 4)
            n_threads = atoi(argv[3]);
    }
    else if (argv[1][0] == 'c') {
        is_server = 0;
        if (argc < 4 || argc > 5)
            die("Usage: [prog_name] c sock_port (server_name) (n_threads)", 0);
        sock_port = argv[2];
        server_name = argv[3];
        if (argc == 5)
            n_threads = atoi(argv[4]);
    }
    else
        die("Usage: first argument need to specify s(server) or c(client)", 0);
    
    setup_ib(fd_read, size_read, is_server, server_name, sock_port);
    close(fd_read);

    if (!is_server) {
        pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t)*n_threads);
        int i;
        double write_time = 0.0;
        clock_t begin1 = clock(); 

        for (i = 0; i < n_threads; i++) {
            pthread_create(threads+i, NULL, server_write_thread, NULL);
        }
        for (i = 0; i < n_threads; i++) {
            pthread_join(threads[i], NULL);
        }
        clock_t end1 = clock(); 
        time_spent += (double)(end1 - begin1) / CLOCKS_PER_SEC;
        printf("Write time is %f seconds", write_time);

        double read_time = 0.0;
        clock_t begin2 = clock(); 
        for (i = 0; i < n_threads; i++) {
            pthread_create(threads+i, NULL, server_read_thread, NULL);
        }
        for (i = 0; i < n_threads; i++) {
            pthread_join(threads[i], NULL);
        }
        clock_t end2 = clock(); 
        time_spent += (double)(end2 - begin2) / CLOCKS_PER_SEC;
        printf("Write time is %f seconds", read_time);
    }

    if (signal(SIGINT, sigint_handler) == SIG_ERR)
        die("Unable to register signal", 1);

    pause();

    close_ib_connection();
}
