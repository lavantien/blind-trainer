#include <errno.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }
    errno = 0;
    FILE *file = fopen(argv[1], "rb");
    printf("fopen=%p errno=%d\n", (void *)file, errno);
    if (file == NULL) {
        return 0;
    }
    errno = 0;
    int c = fgetc(file);
    printf("fgetc=%d errno=%d\n", c, errno);
    errno = 0;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    printf("ftell=%ld errno=%d\n", size, errno);
    fclose(file);
    return 0;
}
