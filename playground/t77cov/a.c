#include <stdio.h>

static int classify(int v) {
    if (v > 10) {
        return 2;
    }
    return v % 2;
}

int main(void) {
    printf("%d\n", classify(3) + classify(30));
    return 0;
}
