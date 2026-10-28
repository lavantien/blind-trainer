#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char SIG[8] = {137, 80, 78, 71, 13, 10, 26, 10};

static void put32(FILE *file, unsigned long value) {
    fputc((int)(value >> 24), file);
    fputc((int)((value >> 16) & 0xFF), file);
    fputc((int)((value >> 8) & 0xFF), file);
    fputc((int)(value & 0xFF), file);
}

static void write_shot(const char *dir, const char *slug, unsigned long width,
                       unsigned long height) {
    char path[4096];
    snprintf(path, sizeof path, "%s/%s.png", dir, slug);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "stub cannot write %s\n", path);
        exit(3);
    }
    unsigned char ihdr[13];
    memset(ihdr, 0, sizeof ihdr);
    ihdr[0] = (unsigned char)(width >> 24);
    ihdr[1] = (unsigned char)(width >> 16);
    ihdr[2] = (unsigned char)(width >> 8);
    ihdr[3] = (unsigned char)width;
    ihdr[4] = (unsigned char)(height >> 24);
    ihdr[5] = (unsigned char)(height >> 16);
    ihdr[6] = (unsigned char)(height >> 8);
    ihdr[7] = (unsigned char)height;
    fwrite(SIG, 1, 8, file);
    put32(file, 13);
    fwrite("IHDR", 1, 4, file);
    fwrite(ihdr, 1, 13, file);
    put32(file, 0);
    put32(file, 0);
    fwrite("IEND", 1, 4, file);
    put32(file, 0);
    fclose(file);
}

int main(int argc, char **argv) {
    const char *dir = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--smoke") == 0 && i + 1 < argc) {
            dir = argv[i + 1];
        }
    }
    const char *appdata = getenv("APPDATA");
    const char *config = getenv("BT_CONFIG");
    printf("stub appdata=%s\n", appdata != NULL ? appdata : "<unset>");
    printf("stub config=%s\n", config != NULL ? config : "<unset>");
    if (config != NULL && config[0] != '\0') {
        FILE *file = fopen(config, "rb");
        printf("stub neg config exists=%d\n", file != NULL);
        if (file != NULL) {
            fclose(file);
        }
        const char *forced = getenv("ADV21A_NEG_EXIT");
        return forced != NULL ? atoi(forced) : 1;
    }
    if (dir == NULL) {
        fprintf(stderr, "stub missing --smoke dir\n");
        return 3;
    }
    const char *shots[] = {"menu",  "drill", "memo",  "solve",   "replay",
                           "algs",  "case-drill", "stats", "settings", "fatal"};
    for (size_t i = 0; i < sizeof shots / sizeof shots[0]; i++) {
        write_shot(dir, shots[i], 1280, 800);
    }
    return 0;
}
