#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAX_BUF_LEN 4096

static inline void die(char *msg, int type)
{
	if (type)
		perror(msg);
	else
		fprintf(stderr, "%s\n", msg);
	exit(1);
}

int main(int argc, char *argv[])
{
	if (argc != 2)
		die("Usage: [prog_name] <nbytes>", 0);

	char *nbytes_c = argv[1];
	int unit = 1;
	int str_len = strlen(nbytes_c);
	if (nbytes_c[str_len-1] < '0' || nbytes_c[str_len-1] > '9') {
		switch (nbytes_c[str_len-1]) {
			case 'K':
				unit *= 1024;
				break;
			case 'M':
				unit *= 1024 * 1024;
				break;
			case 'G':
				unit *= 1024 * 1024 * 1024;
				break;
			default:
				die("Wrong unit", 0);
		}
		nbytes_c[str_len-1] = '\0';
		str_len -= 1;
	}
	size_t nbytes = atoi(nbytes_c) * unit;

	int fd = open("test.bench", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 0)
		die("Can't open", 1);
	
	size_t i, cur_buf = 0;
	char buf[MAX_BUF_LEN];
	for (i = 0; i < nbytes; i++) {
		int w_i = i % 10;
		buf[cur_buf++] = w_i + '0';
		if (cur_buf == MAX_BUF_LEN) {
			if (write(fd, buf, cur_buf) != cur_buf)
				die("Write not enough!", 1);
			cur_buf = 0;
		}
	}
	if (cur_buf > 0)
		if (write(fd, buf, cur_buf) != cur_buf)
			die("Write not enough!", 1);

	return 0;
}
