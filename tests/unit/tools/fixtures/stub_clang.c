#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *record = getenv("BT_STUB_RECORD");
    const char *exit_text = getenv("BT_STUB_EXIT");
    if (record != NULL && record[0] != '\0') {
        FILE *file = fopen(record, "a");
        if (file != NULL) {
            for (int i = 0; i < argc; i++) {
                fprintf(file, "%s\n", argv[i]);
            }
            fclose(file);
        }
    }
    if (exit_text != NULL && exit_text[0] != '\0') {
        return (int)strtol(exit_text, NULL, 10);
    }
    return 0;
}
