#include "utils.h"
#include "setup_ib.h"

static void sig_int(int signo)
{
	close_ib_connection();
	exit(1);
}

int main(int argc, char *argv[])
{
	check(argc == 3, "Usage: %s <phy_size> <port>", argv[0]);
	char *port = argv[2];

	check(signal(SIGINT, sig_int) != SIG_ERR, "Signal init error");

	setup_ib(trans_size(argv[1]), 1, NULL, port);
}