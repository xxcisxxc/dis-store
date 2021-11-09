#include "setup_ib.h"

void *server_write_thread(void *args)
{
    post_write_signaled();
    return NULL;
}

void *server_read_thread(void *args)
{
    post_read_signaled();
    return NULL;
}

int main(int argc, char *argv[])
{
    /* Open test bench into memory-mapped file */
    int fd_read = open("test.bench", O_RDONLY);
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
        if (argc < 3 || argc > 4)
            die("Usage: [prog_name] s sock_port (n_threads)", 0);
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
        for (i = 0; i < n_threads; i++) {
            pthread_create(threads+i, NULL, server_write_thread, NULL);
        }
        for (i = 0; i < n_threads; i++)
            pthread_join(threads[i], NULL);
        
        for (i = 0; i < n_threads; i++) {
            pthread_create(threads+i, NULL, server_read_thread, NULL);
        }
        for (i = 0; i < n_threads; i++)
            pthread_join(threads[i], NULL);
    }

    pause();

    close_ib_connection();
}