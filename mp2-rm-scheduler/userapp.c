#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#define PROC_FILE "/proc/mp2/status"
#define true 1
#define false 0

typedef unsigned int    uint;

void write_to_file(char *s) {
    FILE *fp = fopen(PROC_FILE, "w");
    if (fp == NULL) {
        printf("[WRITE] fail to open file: %s\n", PROC_FILE);
        exit(1);
    }
    fprintf(fp, "%s", s);
    fclose(fp);
}

int check_exist(uint pid) {
    char* line = NULL;
    size_t len = 0;
    uint pid_p;
    int exist = false;
    FILE *fp = fopen(PROC_FILE, "r");
    if (fp == NULL) {
        printf("[READ] fail to open file: %s\n", PROC_FILE);
        exit(1);
    }

    while (getline(&line, &len, fp) != -1) {
        sscanf(line, "%d,", &pid_p);
        if (pid_p == pid) {
            exist = true;
            break;
        }
    }
    fclose(fp);

    return exist;
}

void sched_register(uint pid, uint period, uint computation) {
    char str[128];
    sprintf(str, "R,%d,%d,%d", pid, period, computation);
    write_to_file(str);
    printf("[Registered] pid: %d, period: %d, computation: %d\n", pid, period, computation);
}

void sched_yield(uint pid) {
    char str[32];
    sprintf(str, "Y,%d", pid);
    write_to_file(str);
}

void sched_degister(uint pid) {
    char s[64];
    sprintf(s, "D,%d", pid);
    write_to_file(s);
    printf("[Deregistered] pid: %d\n", pid);
}

unsigned long factorial(uint n) {
    unsigned long ans = 1;
    for (int i = 1; i <= n; i++) {
        ans *= i;
    }
    return ans;
}

void print_time_gap(struct timeval *start, struct timeval *end) {
    unsigned long start_ms = start->tv_sec * 1000 + (start->tv_usec / 1000);
    unsigned long end_ms = end->tv_sec * 1000 + (end->tv_usec / 1000);
    printf("time elapse: %lu ms\n", end_ms - start_ms);
}

int main(int argc, char* argv[]) {
    uint pid, n, period, computation;
    struct timeval start, end;

    pid = getpid();

    n = atoi(argv[1]);
    period = atoi(argv[2]);
    computation = atoi(argv[3]);

    sched_register(pid, period, computation);
    if (!check_exist(pid)) {
        printf("pid doesn't exist, exit...\n");
        exit(1);
    }


    for (int i = 0; i < 10; i++) {
        gettimeofday(&start, NULL);
        factorial(n);
        gettimeofday(&end, NULL);
        print_time_gap(&start, &end);
    }

    sched_degister(pid);

    return 0;
}
