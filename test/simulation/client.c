#include "sim_page.h"
#include "utils.h"
#include <signal.h>
#include <setjmp.h>

#define NUM_SEC 2

static jmp_buf env_sig;

static void sig_alarm(int signo)
{
	longjmp(env_sig, 1);
}

int main(int argc, char *argv[])
{	
	char *server_name = "", *port_name = "";
	check(argc >= 3, "Usage: %s <phy_size> <vir_size> [<addr> <port>]", argv[0]);
	if (argc == 5) {
		server_name = argv[3];
		port_name = argv[4];
	}

	unsigned int buf_size = trans_size(argv[2]);

	init_phys_mem(trans_size(argv[1]));
	init_page_table(buf_size, server_name, port_name);

	int i;
	char tmp;
	double num;

	for (i = 0; i < buf_size; i++)
		put(i, rand_char());
	/*for (i = 0; i < buf_size; i++)
		check(get(i) =='a'+i%26, "Not equal");*/

	check(signal(SIGALRM, sig_alarm) != SIG_ERR, "Signal init error");
	if (setjmp(env_sig) != 0) {
		printf("Throughput: %f\n", num/NUM_SEC);
		goto end;
	}

	alarm(NUM_SEC);
	for (;;) {
		i = rand_int(buf_size);
		if (rand_int(100) < 90)
			tmp = get(i);
		else
			put(i, rand_char());
		num++;
	}

end:
	destroy_page_table();
	destroy_phys_mem();

	return 1;
}
