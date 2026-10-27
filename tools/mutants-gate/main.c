#include "mutants-gate/lib.h"
#include "mutants-gate/tests.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/config.h"

static char *read_arg(const char *path) {
    BT_TRACE_ENTER("mutants-gate.read_arg");
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "cannot read %s: %s\n", path, strerror(errno));
        BT_TRACE_FAIL("mutants-gate.read_arg", "cannot open %s", path);
        BT_TRACE_EXIT("mutants-gate.read_arg");
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "cannot read %s: %s\n", path, strerror(errno));
        BT_TRACE_FAIL("mutants-gate.read_arg", "seek failed on %s", path);
        BT_TRACE_EXIT("mutants-gate.read_arg");
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        fprintf(stderr, "cannot read %s: %s\n", path, strerror(errno));
        BT_TRACE_FAIL("mutants-gate.read_arg", "tell failed on %s", path);
        BT_TRACE_EXIT("mutants-gate.read_arg");
        return NULL;
    }
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        fprintf(stderr, "cannot read %s: out of memory\n", path);
        BT_TRACE_FAIL("mutants-gate.read_arg", "oom for %s", path);
        BT_TRACE_EXIT("mutants-gate.read_arg");
        return NULL;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    if (got != (size_t)size) {
        free(text);
        fprintf(stderr, "cannot read %s: short read\n", path);
        BT_TRACE_FAIL("mutants-gate.read_arg", "short read on %s", path);
        BT_TRACE_EXIT("mutants-gate.read_arg");
        return NULL;
    }
    text[size] = '\0';
    BT_TRACE_EXIT("mutants-gate.read_arg");
    return text;
}

static int run_gate(int argc, char **argv) {
    BT_TRACE_ENTER("mutants-gate.run_gate");
    if (argc != 3) {
        fprintf(stderr, "usage: mutants-gate <outcomes.json> <allowlist.tsv>\n");
        fprintf(stderr, "       mutants-gate merge <out.json> <in1.json> <in2.json> ...\n");
        BT_TRACE_FAIL("mutants-gate.run_gate", "expected 2 positional args, got %d", argc - 1);
        BT_TRACE_EXIT("mutants-gate.run_gate");
        return 2;
    }
    char *outcomes = read_arg(argv[1]);
    if (outcomes == NULL) {
        BT_TRACE_EXIT("mutants-gate.run_gate");
        return 2;
    }
    char *allowlist = read_arg(argv[2]);
    if (allowlist == NULL) {
        free(outcomes);
        BT_TRACE_EXIT("mutants-gate.run_gate");
        return 2;
    }
    char summary[BT_GATE_SUMMARY_MAX];
    int rc = bt_gate_run(outcomes, allowlist, summary, sizeof summary);
    free(outcomes);
    free(allowlist);
    if (rc == BT_GATE_OK) {
        printf("%s\n", summary);
        BT_TRACE_EXIT("mutants-gate.run_gate");
        return 0;
    }
    fprintf(stderr, "mutants gate: %s\n", bt_gate_last_error());
    BT_TRACE_FAIL("mutants-gate.run_gate", "gate verdict %d", rc);
    BT_TRACE_EXIT("mutants-gate.run_gate");
    return rc;
}

static int run_merge(int argc, char **argv) {
    BT_TRACE_ENTER("mutants-gate.run_merge");
    if (argc < 4) {
        fprintf(stderr, "usage: mutants-gate merge <out.json> <in1.json> <in2.json> ...\n");
        BT_TRACE_FAIL("mutants-gate.run_merge", "expected at least 3 args, got %d", argc - 1);
        BT_TRACE_EXIT("mutants-gate.run_merge");
        return 2;
    }
    size_t count = (size_t)(argc - 3);
    char **texts = calloc(count, sizeof(char *));
    if (texts == NULL) {
        BT_TRACE_FAIL("mutants-gate.run_merge", "oom");
        BT_TRACE_EXIT("mutants-gate.run_merge");
        return 2;
    }
    for (size_t i = 0; i < count; i++) {
        texts[i] = read_arg(argv[3 + (int)i]);
        if (texts[i] == NULL) {
            for (size_t j = 0; j <= i; j++) {
                free(texts[j]);
            }
            free(texts);
            BT_TRACE_EXIT("mutants-gate.run_merge");
            return 2;
        }
    }
    char *merged = NULL;
    unsigned long long total = 0;
    int rc = bt_gate_merge((const char *const *)texts, count, &merged, &total);
    for (size_t i = 0; i < count; i++) {
        free(texts[i]);
    }
    free(texts);
    if (rc != BT_GATE_OK) {
        fprintf(stderr, "mutants merge: %s\n", bt_gate_last_error());
        BT_TRACE_FAIL("mutants-gate.run_merge", "merge verdict %d", rc);
        BT_TRACE_EXIT("mutants-gate.run_merge");
        return rc;
    }
    FILE *out = fopen(argv[2], "wb");
    if (out == NULL) {
        fprintf(stderr, "cannot write %s: %s\n", argv[2], strerror(errno));
        free(merged);
        BT_TRACE_FAIL("mutants-gate.run_merge", "cannot open %s", argv[2]);
        BT_TRACE_EXIT("mutants-gate.run_merge");
        return 2;
    }
    size_t len = strlen(merged);
    size_t wrote = fwrite(merged, 1, len, out);
    int close_rc = fclose(out);
    free(merged);
    if (wrote != len || close_rc != 0) {
        fprintf(stderr, "cannot write %s: short write\n", argv[2]);
        BT_TRACE_FAIL("mutants-gate.run_merge", "short write on %s", argv[2]);
        BT_TRACE_EXIT("mutants-gate.run_merge");
        return 2;
    }
    printf("merged %d inputs, %llu mutants, wrote %s\n", (int)count, total, argv[2]);
    BT_TRACE_EXIT("mutants-gate.run_merge");
    return 0;
}

static void trace_from_config(void) {
    const bt_json_value *config = bt_config_load();
    long long ring_bytes = 65536;
    long long max_threads = 8;
    long long line_max = 256;
    bt_config_int(config, "trace.ring_bytes", &ring_bytes);
    bt_config_int(config, "trace.max_threads", &max_threads);
    bt_config_int(config, "trace.line_max", &line_max);
    bt_trace_init((size_t)ring_bytes, (size_t)max_threads, (size_t)line_max);
    bt_trace_install_crash_handler();
}

int main(int argc, char **argv) {
    trace_from_config();
    if (argc >= 2 && strcmp(argv[1], "selftest") == 0) {
        return bt_mutants_gate_selftest();
    }
    int rc;
    if (argc >= 2 && strcmp(argv[1], "merge") == 0) {
        rc = run_merge(argc, argv);
    } else {
        rc = run_gate(argc, argv);
    }
    if (rc != 0) {
        bt_trace_dump();
    }
    bt_trace_shutdown();
    return rc;
}
