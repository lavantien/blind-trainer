#include "ga/features.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const unsigned char bytes[4] = {'m', 'a', 0xC3, '\0'};
    char *text = malloc(sizeof bytes);
    memcpy(text, bytes, sizeof bytes);
    bt_ga_features out;
    int ok = bt_ga_parse(text, &out);
    free(text);
    printf("parse ok=%d\n", ok);
    return 0;
}
