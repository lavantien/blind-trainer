#include "e2e/lib.h"
#include "e2e/png.h"

#include "common/bt_trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char TD_PNG_3X2[] = {
    137, 80,  78,  71,  13,  10, 26, 10,  0,   0,   0,  13, 73,  72, 68, 82, 0,   0,
    0,   3,   0,   0,   0,   2,   8,   6,  0,   0,   0,   157, 116, 102, 26, 0,  0,  0,
    17,  73,  68,  65,  84, 120, 156, 99, 224, 228, 228, 252, 15, 195, 12,  200, 28, 0,
    77,  96,  6,   157, 65,  42,  41,  210, 0,   0,   0,   0,   73,  69,  78,  68, 174, 66,
    96,  130,
};

static unsigned char body_600[8192];
static unsigned char huge_line[100001];
static unsigned char binary_garbage[4096];

static void write_file(const char *path, const unsigned char *data, size_t len) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "probe: cannot write %s\n", path);
        exit(3);
    }
    fwrite(data, 1, len, file);
    fclose(file);
}

static void show_tail(const char *label, const unsigned char *bytes, size_t len) {
    char out[2048];
    bt_e2e_tail(bytes, len, out, sizeof out);
    printf("%s: [%s]\n", label, out);
}

static void show_shot(const char *label, const char *path) {
    char reason[BT_E2E_ERROR_MAX];
    bt_e2e_window_dims dims = {3, 2};
    bt_e2e_shot_verdict verdict = bt_e2e_check_shot(path, dims, reason, sizeof reason);
    unsigned int width = 0;
    unsigned int height = 0;
    int dims_ok = bt_png_dims(path, &width, &height);
    printf("%s: verdict=%d reason=[%s] png_dims=%d %ux%u\n", label, verdict, reason, dims_ok,
           width, height);
}

int main(void) {
    bt_trace_init(65536, 8, 256);

    size_t at = 0;
    for (int line = 1; line <= 600; line++) {
        at += (size_t)snprintf((char *)body_600 + at, sizeof body_600 - at, "line %d\n", line);
    }
    show_tail("tail-600-lines", body_600, at);

    show_tail("tail-empty", (const unsigned char *)"", 0);
    show_tail("tail-no-newline", (const unsigned char *)"a\nb\nc", 5);
    show_tail("tail-crlf", (const unsigned char *)"a\r\nb\r\nc\r\n", 9);
    show_tail("tail-only-newlines", (const unsigned char *)"\n\n\n\n\n", 5);

    memset(huge_line, 'x', sizeof huge_line - 1);
    huge_line[sizeof huge_line - 1] = '\0';
    show_tail("tail-huge-line", huge_line, sizeof huge_line - 1);

    for (size_t i = 0; i < sizeof binary_garbage; i++) {
        binary_garbage[i] = (unsigned char)(i * 31 + 7);
    }
    show_tail("tail-binary", binary_garbage, sizeof binary_garbage);

    const char *dir = getenv("TEMP");
    char path[1024];
    unsigned char header_only[24];
    memcpy(header_only, TD_PNG_3X2, 24);
    snprintf(path, sizeof path, "%s\\adv21b_header_only.png", dir);
    write_file(path, header_only, 24);
    show_shot("shot-header-only", path);

    unsigned char bogus_len[24];
    memcpy(bogus_len, TD_PNG_3X2, 24);
    bogus_len[8] = 0x7F;
    bogus_len[9] = 0xFF;
    bogus_len[10] = 0xFF;
    bogus_len[11] = 0xFF;
    snprintf(path, sizeof path, "%s\\adv21b_bogus_len.png", dir);
    write_file(path, bogus_len, 24);
    show_shot("shot-bogus-ihdr-len", path);

    snprintf(path, sizeof path, "%s\\adv21b_truncated.png", dir);
    write_file(path, TD_PNG_3X2, 40);
    show_shot("shot-truncated-at-40", path);

    snprintf(path, sizeof path, "%s\\adv21b_valid.png", dir);
    write_file(path, TD_PNG_3X2, sizeof TD_PNG_3X2);
    show_shot("shot-valid", path);

    snprintf(path, sizeof path, "%s\\adv21b_signature.png", dir);
    write_file(path, TD_PNG_3X2, 8);
    show_shot("shot-signature-only", path);

    unsigned char corrupt_tail[sizeof TD_PNG_3X2];
    memcpy(corrupt_tail, TD_PNG_3X2, sizeof corrupt_tail);
    corrupt_tail[45] ^= 0xFF;
    snprintf(path, sizeof path, "%s\\adv21b_corrupt_idat.png", dir);
    write_file(path, corrupt_tail, sizeof corrupt_tail);
    show_shot("shot-corrupt-idat", path);

    const char *dup_expected[] = {"menu.png", "menu.png"};
    const char *dup_found[] = {"menu.png"};
    char missing[4][BT_E2E_NAME_MAX];
    char unexpected[4][BT_E2E_NAME_MAX];
    size_t missing_count = 0;
    size_t unexpected_count = 0;
    bt_e2e_classify(dup_found, 1, dup_expected, 2, missing, &missing_count, unexpected,
                    &unexpected_count);
    printf("classify-dups: missing=%zu unexpected=%zu\n", missing_count, unexpected_count);

    char long_slug[300];
    memset(long_slug, 'a', sizeof long_slug - 5);
    memcpy(long_slug + sizeof long_slug - 5, "b.png", 5);
    const char *slug_shots[] = {long_slug};
    char names[1][BT_E2E_NAME_MAX];
    size_t names_count = 0;
    int rc = bt_e2e_expected_names(slug_shots, 1, names, &names_count);
    printf("expected-names-long-slug: rc=%d count=%zu len=%zu expected-len=304\n", rc,
           names_count, strlen(names[0]));

    bt_trace_shutdown();
    return 0;
}
