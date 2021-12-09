#define USE_PMEM "/mnt/pmem/test"

#include <time.h> 
#include <signal.h>
#include "setup_ib.h"

#define N_EXPR 1
#define USEC_SEC 1000

#define MSG "DONE"

static void *addr_write, *addr_read;

void *server_write_thread(void *args)
{
    /*double total_t = 0, begin, end, spent;
    printf("size: %ld\n", ib_res.ib_buf_size);
    begin = clock();*/
    memcpy(ib_res.ib_buf, addr_write, ib_res.ib_buf_size);
    /*end = clock();
    spent = (end - begin) / CLOCKS_PER_SEC;
    total_t += spent * USEC_SEC;
    printf("memcpy time: %f\n", spent * USEC_SEC);
    begin = clock();*/
    if (post_write_signaled() != 0)
        die("Not success write", 1);
    wait_poll(IBV_WC_RDMA_WRITE);
    /*end = clock();
    spent = (end - begin) / CLOCKS_PER_SEC;
    total_t += spent * USEC_SEC;
    printf("rdma time: %f\n", spent * USEC_SEC);
    printf("total time: %f\n", total_t);*/
    return NULL;
}

void *server_read_thread(void *args)
{
    if (post_read_signaled() != 0)
        die("Not success read", 1);
    wait_poll(IBV_WC_RDMA_READ);
    memcpy(addr_read, ib_res.ib_buf, ib_res.ib_buf_size);
    return NULL;
}

int main(int argc, char *argv[])
{
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

    /* Open test bench into memory-mapped file */
    int fd_read = open("test.bench", O_RDWR);
    if (fd_read < 0)
        die("Open failed", 1);
    int size_file = lseek(fd_read, 0, SEEK_END);
    addr_write = mmap(NULL, size_file, PROT_READ, MAP_PRIVATE, fd_read, 0);
    if (addr_write == NULL)
        die("Memory Mapped", 1);
    close(fd_read);
    /* Claim a new memory for copy to */
    addr_read = mmap(NULL, size_file, PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (addr_read == NULL)
        die("Memory Mapped", 1);
    if (is_server) {
        munmap(addr_write, size_file);
        munmap(addr_read, size_file);
    }

    setup_ib(fd_read, size_file, is_server, server_name, sock_port);

    sock_port = "7778";
    if (is_server) {
        /* Build connection */
        int	sockfd = 0;
        int	peer_sockfd = 0;
        char sock_buf[5] = {'\0'};
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
    usleep(1000);
    int peer_sockfd = sock_create_connect(server_name, sock_port);

    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t)*n_threads);
    int i, j;
    double total_t, begin, end, spent;

    printf("Start test RDMA WRITE\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        begin = clock();
        for (j = 0; j < n_threads; j++) {
            pthread_create(threads+j, NULL, server_write_thread, NULL);
        }
        for (j = 0; j < n_threads; j++) {
            pthread_join(threads[j], NULL);
        }
        end = clock();
        spent = (end - begin) / CLOCKS_PER_SEC;
        total_t += spent * USEC_SEC;
    }
    printf("%f\nEnd test RDMA WRITE\n", total_t/N_EXPR);

    memset(ib_res.ib_buf, 0, ib_res.ib_buf_size);
    printf("Start test RDMA READ\n");
    total_t = 0;
    for (i = 0; i < N_EXPR; i++) {
        begin = clock();
        for (j = 0; j < n_threads; j++) {
            pthread_create(threads+j, NULL, server_read_thread, NULL);
        }
        for (j = 0; j < n_threads; j++) {
            pthread_join(threads[j], NULL);
        }
        end = clock();
        spent = (end - begin) / CLOCKS_PER_SEC;
        total_t += spent * USEC_SEC;
    }
    printf("%f\nEnd test RDMA READ\n", total_t/N_EXPR);
    free(threads);

    char sock_buf[5] = MSG;
    int n = sock_write(peer_sockfd, sock_buf, sizeof(MSG));
    if (n != sizeof(MSG))
        die("Failed to send DONE from server", 1);
    close(peer_sockfd);

waitend:
    close_ib_connection();
    printf("disconnect\n");

    return 1;
}
