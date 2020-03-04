#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PROC_FILE "/proc/mp2/status"

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

void check_exist(uint pid) {
    char* line = NULL;
    size_t len = 0;
    FILE *fp = fopen(PROC_FILE, "r");
    if (fp == NULL) {
        printf("[READ] fail to open file: %s\n", PROC_FILE);
        exit(1);
    }

    while (getline(&line, &len, fp) != -1) {
        printf("%s", line);
    }
    fclose(fp);
}

void sched_register(uint pid, uint period, uint computation) {
    char str[128];
    sprintf(str, "R,%d,%d,%d", pid, period, computation);
    write_to_file(str);
}

void sched_degister(uint pid) {
    char s[64];
    sprintf(s, "D,%d", pid);
    write_to_file(s);
}

int main(int argc, char* argv[]) {
    uint pid, period, computation;

    pid = getpid();
    printf("pid: %d\n", pid);

    period = atoi(argv[1]);
    computation = atoi(argv[2]);

    sched_register(pid, period, computation);
    check_exist(pid);

    sched_degister(pid);

    return 0;
}