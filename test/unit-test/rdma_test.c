//#define USE_PMEM "/mnt/pmem/test"

#include <time.h> 
#include <signal.h>
#include "setup_ib.h"

#define N_EXPR 100
#define USEC_SEC 1000

#define MSG "DONE"

void *server_write_thread(void *args)
{
    post_write_signaled();
    wait_poll();
    return NULL;
}

void *server_read_thread(void *args)
{
    post_read_signaled();
    wait_poll();
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

    sock_port = "7778";
    if (is_server) {
        /* Build connection */
        int	sockfd = 0;
        int	peer_sockfd = 0;
        char sock_buf[64] = {'\0'};
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(struct sockaddr_in);

        sockfd = sock_create_bind(sock_port);
        if (sockfd <= 0) 
            die("Failed to create server socket", 1);
        listen(sockfd, 5);
        peer_sockfd = accept(sockfd, (struct sockaddr *)&peer_addr,
			        &peer_addr_len);
        if (peer_sockfd <= 0)
            die("Failed to accept peer_sockfd", 1);
        int n = sock_read(peer_sockfd, sock_buf, sizeof(MSG));
        if (n != sizeof(MSG))
            die("Failed to receive DONE from client", 1);
        close(peer_sockfd);
        close(sockfd);
        goto waitend;
    }

    /* Start test RDMA: client side */
    int peer_sockfd = sock_create_connect(server_name, sock_port);

    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t)*n_threads);
    int i;
    double total_t, begin, end;
    double *spent = (double *)
        mmap(NULL, sizeof(double), PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED, -1, 0);
    printf("Start test RDMA WRITE\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        if (!fork()) {
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, server_write_thread, NULL);
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
    printf("%f\nEnd test RDMA WRITE\n", total_t/N_EXPR);
    printf("Start test RDMA READ\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        if (!fork()) {
            begin = clock();
            for (i = 0; i < n_threads; i++) {
                pthread_create(threads+i, NULL, server_read_thread, NULL);
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
    printf("%f\nEnd test RDMA READ\n", total_t/N_EXPR);
    free(threads);

    char sock_buf[64] = MSG;
    int n = sock_write(peer_sockfd, sock_buf, sizeof(MSG));
    if (n != sizeof(MSG))
        die("Failed to send DONE from server", 1);
    close(peer_sockfd);

waitend:
    close_ib_connection();
    printf("disconnect\n");

    return 1;
}
