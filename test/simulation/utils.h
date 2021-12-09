#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#define check(cond, ...) \
do {\
	if (cond)\
		break;\
	fprintf(stderr, __VA_ARGS__);\
	if (errno)\
		perror(" ");\
	else\
		fprintf(stderr, "\n");\
	exit(0);\
} while(0)

unsigned int trans_size(char *csize)
{
	unsigned int unit = 1, nbytes;
	unsigned int str_len = strlen(csize);

	if (csize[str_len-1] < '0' || csize[str_len-1] > '9') {
		switch (csize[str_len-1]) {
			case 'G':
				unit *= 1024;
			case 'M':
				unit *= 1024;
			case 'K':
				unit *= 1024;
			default:
				break;
		}
		csize[str_len-1] = '\0';
		str_len--;
	}
	nbytes = atoi(csize) * unit;

	return nbytes;
}

unsigned int rand_int(unsigned int size)
{
	return rand() % size;
}

unsigned int rand_range(unsigned int base, unsigned int offset)
{
	return rand() % offset + base;
}

char rand_char()
{
	int r = rand_int(2);
	char base = r ? '0' : 'a';
	int offset = rand_int(r ? 10 : 26);

	return base + offset;
}

#endif /* _UTILS_H */