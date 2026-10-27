#include "ga/features.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *heap_copy(const unsigned char *bytes, size_t size) {
    char *text = malloc(size);
    memcpy(text, bytes, size);
    return text;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: utf8_overread <parse|valid2|valid3|nfc4>\n");
        return 2;
    }
    static const unsigned char truncated2[] = {'m', 'a', 0xC3, '\0'};
    static const unsigned char lead_only[] = {0xE1, '\0'};
    static const unsigned char lead_cont[] = {0xE1, 0xBA, '\0'};
    static const unsigned char lead4[] = {0xF0, 0x9F, '\0'};
    bt_ga_features out;
    int ok = 0;
    if (strcmp(argv[1], "parse") == 0) {
        char *text = heap_copy(truncated2, sizeof truncated2);
        ok = bt_ga_parse(text, &out);
        free(text);
    } else if (strcmp(argv[1], "valid2") == 0) {
        char *text = heap_copy(lead_only, sizeof lead_only);
        ok = bt_ga_utf8_valid(text);
        free(text);
    } else if (strcmp(argv[1], "valid3") == 0) {
        char *text = heap_copy(lead_cont, sizeof lead_cont);
        ok = bt_ga_utf8_valid(text);
        free(text);
    } else if (strcmp(argv[1], "nfc4") == 0) {
        char *text = heap_copy(lead4, sizeof lead4);
        ok = bt_ga_is_nfc(text);
        free(text);
    } else {
        printf("unknown case %s\n", argv[1]);
        return 2;
    }
    printf("%s ok=%d\n", argv[1], ok);
    return 0;
}
