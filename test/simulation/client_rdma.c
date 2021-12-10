#include "setup_ib.h"

int main()
{
    char *buf = (char *)malloc(4096);
    for (i = 0; i < 4096; i++) {
        buf[i] = 'a';
    }
    char *server_name = "", *port_name = "";
	check(argc == 3, "Usage: %s <addr> <port>", argv[0]);
	server_name = argv[3];
	port_name = argv[4];
    sleep(12);
    setup_ib(4096, 0, server_name, port_name);
    memset(ib_res.ib_buf, buf, 4096);
    post_write_signaled(0);
	wait_poll(IBV_WC_RDMA_WRITE);
    close_ib_connection();
    return 1;
}