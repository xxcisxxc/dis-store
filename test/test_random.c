#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#define BUFSIZE 1024*1024*1024+1

static double num;

static void sig_alarm(int signo)
{
	printf("Total operations: %f\n", num);
	exit(1);
}

int main()
{
	int i;
	char *buffer = (char *)malloc(1024*1024*1024+1);
	char tmp;	

	if (signal(SIGALRM, sig_alarm)) {
		printf("signal error\n");
		exit(0);
	}

	printf("Begin Initialization\n");

	for (i = 0; i < BUFSIZE; i++)
		buffer[i] = 'a' + i % 26;
	
	srand(1);

	printf("Begin Random Access\n");

	alarm(20);
	for (;;) {
		tmp = buffer[rand() % BUFSIZE];
		num++;
	}
}
