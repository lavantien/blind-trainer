#include <windows.h>

int main(void) {
    volatile unsigned long sink = 0;
    for (;;) {
        sink++;
        if (sink == 0) {
            Sleep(1);
        }
    }
}
