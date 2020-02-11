#include <stdio.h>
#include <stdlib.h>
#include "userapp.h"

int fib(int n) {
	if (n < 2) {
		return n;
	}
	return fib(n-1) + fib(n-2);
}

int main(int argc, char* argv[])
{
	int n, r;

	if (argc != 2) {
		printf("Usage: ./userapp <n>\n\n");
		printf("Example:\n./userapp 20");
	}

	n = atoi(argv[1]);

	r = fib(n);
	printf("fib(%d)=%d\n", n, r);
	return 0;
}
