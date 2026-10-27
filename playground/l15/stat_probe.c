#include <direct.h>
#include <stdio.h>
#include <sys/stat.h>

int main(void) {
    _mkdir("playground/l15/statdir");
    _mkdir("playground\\l15\\statdir2");
    struct stat info;
    int a = stat("playground/l15/statdir", &info);
    int b = stat("playground\\l15\\statdir", &info);
    int c = stat("playground\\l15\\statdir2", &info);
    int d = stat("playground/l15/statdir2", &info);
    int e = stat("playground/l15/missing", &info);
    printf("a=%d b=%d c=%d d=%d e=%d\n", a, b, c, d, e);
    return 0;
}
