#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "userapp.h"

#define PATH "/proc/mp1/status"

int fib(int n) {
	if (n < 2) {
		return n;
	}
	return fib(n-1) + fib(n-2);
}

int main(int argc, char* argv[])
{
	int n, r, pid;
	FILE *fp;

	if (argc != 2) {
		printf("Usage: ./userapp <n>\n\n");
		printf("Example:\n./userapp 20");
	}

	pid = getpid();
	printf("pid: %d\n", pid);

	fp = fopen(PATH, "w");
	if (fp == NULL) {
		printf("fail to open file: %s\n", PATH);
		return 1;
	}
	fprintf(fp, "%d", pid);
	fclose(fp);

	n = atoi(argv[1]);

	r = fib(n);
	printf("fib(%d)=%d\n", n, r);
	return 0;
}
