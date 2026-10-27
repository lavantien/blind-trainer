#include <stdio.h>
int main(void) {
    float vals[] = {1.25F, 0.75F, 2.5F, 0.5F, 1.125F};
    for (int i = 0; i < 5; i++) {
        float total = vals[i] * 2.0F;
        float avg = total / 2.0F;
        printf("%.1f|%.1f\n", vals[i], avg);
    }
    return 0;
}
