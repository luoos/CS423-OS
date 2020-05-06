#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    FILE *f;
    char *filename;

    if (argc == 1) {
        filename = "/home/junluo2/file.txt";
    } else {
        filename = argv[1];
    }

    f = fopen(filename, "w");
    if (f == NULL) {
        printf("fail to open file\n");
        exit(1);
    }

    fprintf(f, "Yahaha\n");
    fclose(f);

    return 0;
}