#include "gendata/build.h"
#include "gendata/model.h"

#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "common/bt_harness.h"
#include "common/paths.h"
#include "common/strbuf.h"
#include "data/corpus.h"
#include "data/digest.h"
#include "unit/tools/spawn_util.h"

static void rm_tree(const char *path) {
    char pattern[BT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    struct _finddatai64_t entry;
    intptr_t handle = _findfirsti64(pattern, &entry);
    if (handle != -1) {
        do {
            if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
                continue;
            }
            char child[BT_PATH_MAX];
            snprintf(child, sizeof child, "%s/%s", path, entry.name);
            if (entry.attrib & _A_SUBDIR) {
                rm_tree(child);
            } else {
                remove(child);
            }
        } while (_findnexti64(handle, &entry) == 0);
        _findclose(handle);
    }
    _rmdir(path);
}

static void ref_ground(char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_test_repo_root(root);
    snprintf(out, BT_PATH_MAX, "%s/ref_ground", root);
}

static void loaded(bt_corpus *corpus) {
    char root[BT_PATH_MAX];
    ref_ground(root);
    bt_corpus_error err;
    if (bt_corpus_load(root, corpus, &err) != 0) {
        char text[BT_PATH_MAX + 96];
        bt_corpus_error_string(&err, text, sizeof text);
        BT_ABORT("%s", text);
    }
}

static void digest(bt_corpus_digest *out) {
    char root[BT_PATH_MAX];
    ref_ground(root);
    char err[192];
    if (bt_digest_corpus(root, out, err, sizeof err) != 0) {
        BT_ABORT("%s", err);
    }
}

static void pretty_bytes(const bt_gd_built *built, bt_strbuf *cases, bt_strbuf *specials) {
    bt_json_value *cases_value = bt_gd_cases_file_to_json(&built->cases);
    bt_json_value *specials_value = bt_gd_specials_file_to_json(&built->specials);
    bt_strbuf_init(cases, 0);
    bt_strbuf_init(specials, 0);
    if (bt_gd_json_pretty(cases_value, cases) != 0 ||
        bt_gd_json_pretty(specials_value, specials) != 0) {
        BT_ABORT("%s", "pretty serialization failed");
    }
    bt_json_free(cases_value);
    bt_json_free(specials_value);
}

BT_TEST(gendata_determinism, repeated_loads_and_builds_are_byte_identical) {
    bt_corpus first_corpus;
    bt_corpus second_corpus;
    loaded(&first_corpus);
    loaded(&second_corpus);
    bt_gd_built first;
    bt_gd_built second;
    bt_gd_build(&first_corpus, &first);
    bt_gd_build(&second_corpus, &second);
    bt_strbuf cases_a;
    bt_strbuf cases_b;
    bt_strbuf specials_a;
    bt_strbuf specials_b;
    pretty_bytes(&first, &cases_a, &specials_a);
    pretty_bytes(&second, &cases_b, &specials_b);
    BT_CHECK_EQ(cases_a.len, cases_b.len);
    BT_CHECK_EQ(memcmp(cases_a.data, cases_b.data, cases_a.len), 0);
    BT_CHECK_EQ(specials_a.len, specials_b.len);
    BT_CHECK_EQ(memcmp(specials_a.data, specials_b.data, specials_a.len), 0);
    bt_corpus_digest dig;
    digest(&dig);
    char cases_sha[65];
    char specials_sha[65];
    bt_digest_sha256_hex(cases_a.data, cases_a.len, cases_sha);
    bt_digest_sha256_hex(specials_a.data, specials_a.len, specials_sha);
    bt_gd_manifest manifest_a;
    bt_gd_manifest manifest_b;
    bt_gd_manifest_make(&first, &dig, cases_sha, specials_sha,
                        "4444444444444444444444444444444444444444444444444444444444444444",
                        "2026-10-12T00:00:00Z", &manifest_a);
    bt_gd_manifest_make(&second, &dig, cases_sha, specials_sha,
                        "4444444444444444444444444444444444444444444444444444444444444444",
                        "2026-10-12T00:00:00Z", &manifest_b);
    BT_CHECK(bt_gd_manifest_eq(&manifest_a, &manifest_b));
    bt_gd_manifest_free(&manifest_a);
    bt_gd_manifest_free(&manifest_b);
    bt_strbuf_free(&cases_a);
    bt_strbuf_free(&cases_b);
    bt_strbuf_free(&specials_a);
    bt_strbuf_free(&specials_b);
    bt_gd_built_free(&first);
    bt_gd_built_free(&second);
    bt_corpus_free(&first_corpus);
    bt_corpus_free(&second_corpus);
}

BT_TEST(gendata_determinism, timestamp_is_the_only_varying_manifest_field) {
    bt_corpus corpus;
    loaded(&corpus);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    bt_strbuf cases;
    bt_strbuf specials;
    pretty_bytes(&built, &cases, &specials);
    bt_corpus_digest dig;
    digest(&dig);
    char cases_sha[65];
    char specials_sha[65];
    bt_digest_sha256_hex(cases.data, cases.len, cases_sha);
    bt_digest_sha256_hex(specials.data, specials.len, specials_sha);
    bt_gd_manifest early;
    bt_gd_manifest late;
    bt_gd_manifest_make(&built, &dig, cases_sha, specials_sha,
                        "4444444444444444444444444444444444444444444444444444444444444444",
                        "2026-01-01T00:00:00Z", &early);
    bt_gd_manifest_make(&built, &dig, cases_sha, specials_sha,
                        "4444444444444444444444444444444444444444444444444444444444444444",
                        "2026-12-31T23:59:59Z", &late);
    BT_CHECK(strcmp(early.generated_at, late.generated_at) != 0);
    free(late.generated_at);
    late.generated_at = _strdup(early.generated_at);
    BT_CHECK(bt_gd_manifest_eq(&early, &late));
    bt_gd_manifest_free(&early);
    bt_gd_manifest_free(&late);
    bt_strbuf_free(&cases);
    bt_strbuf_free(&specials);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
}

BT_TEST(gendata_determinism, digest_corpus_is_stable_and_complete) {
    bt_corpus_digest dig;
    digest(&dig);
    BT_CHECK_EQ(dig.file_count, 12U);
    BT_CHECK_EQ(dig.rows, 1077U);
    bt_corpus_digest again;
    digest(&again);
    BT_CHECK_EQ(strcmp(dig.sha256, again.sha256), 0);
    for (size_t i = 0; i < dig.file_count; i++) {
        BT_CHECK_EQ(strlen(dig.files[i].sha256), 64U);
        bool hex = true;
        for (const char *c = dig.files[i].sha256; *c != '\0'; c++) {
            if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f'))) {
                hex = false;
            }
        }
        BT_CHECK(hex);
    }
}

BT_TEST(gendata_determinism, digest_corpus_tracks_file_contents) {
    bt_corpus_digest dig;
    digest(&dig);
    char root[BT_PATH_MAX];
    ref_ground(root);
    const char *base = getenv("TEMP");
    if (base == NULL || base[0] == '\0') {
        base = ".";
    }
    char tmp[BT_PATH_MAX];
    snprintf(tmp, sizeof tmp, "%s/bt-gendata-digest-copy-%lu", base, GetCurrentProcessId());
    rm_tree(tmp);
    for (const char *const *solver =
             (const char *const[]){(const char *)"cherry", (const char *)"eggins", NULL};
         *solver != NULL; solver++) {
        char dir[BT_PATH_MAX];
        snprintf(dir, sizeof dir, "%s/%s", tmp, *solver);
        bt_test_mkdirs(dir);
        for (const char *const *sheet =
                 (const char *const[]){
                     (const char *)"edge",
                     (const char *)"corner",
                     (const char *)"flips",
                     (const char *)"twists",
                     (const char *)"parity",
                     (const char *)"ltct",
                     NULL,
                 };
             *sheet != NULL; sheet++) {
            char src[BT_PATH_MAX];
            char dst[BT_PATH_MAX];
            snprintf(src, sizeof src, "%s/%s/%s.tsv", root, *solver, *sheet);
            snprintf(dst, sizeof dst, "%s/%s/%s.tsv", tmp, *solver, *sheet);
            FILE *in = fopen(src, "rb");
            FILE *out = fopen(dst, "wb");
            if (in == NULL || out == NULL) {
                fprintf(stderr, "copy fail %s -> %s\n", src, dst);
            }
            BT_CHECK(in != NULL && out != NULL);
            if (in != NULL && out != NULL) {
                char chunk[4096];
                size_t got = 0;
                while ((got = fread(chunk, 1, sizeof chunk, in)) > 0) {
                    fwrite(chunk, 1, got, out);
                }
            }
            if (in != NULL) {
                fclose(in);
            }
            if (out != NULL) {
                fclose(out);
            }
        }
    }
    bt_corpus_digest copied;
    char err[192];
    BT_CHECK_EQ(bt_digest_corpus(tmp, &copied, err, sizeof err), 0);
    BT_CHECK_EQ(strcmp(copied.sha256, dig.sha256), 0);
    BT_CHECK_EQ(copied.rows, dig.rows);
    char sheet_path[BT_PATH_MAX];
    snprintf(sheet_path, sizeof sheet_path, "%s/cherry/edge.tsv", tmp);
    FILE *file = fopen(sheet_path, "rb");
    BT_CHECK(file != NULL);
    if (file != NULL) {
        char *text = malloc(1U << 20U);
        size_t len = fread(text, 1, 1U << 20U, file);
        text[len] = '\0';
        fclose(file);
        const char *marker = strstr(text, "U M2");
        BT_CHECK(marker != NULL);
        if (marker != NULL) {
            memcpy(text + (marker - text), "U  M2", 5);
            file = fopen(sheet_path, "wb");
            fwrite(text, 1, len, file);
            fclose(file);
        }
        free(text);
    }
    bt_corpus_digest mutated;
    BT_CHECK_EQ(bt_digest_corpus(tmp, &mutated, err, sizeof err), 0);
    BT_CHECK(strcmp(mutated.sha256, dig.sha256) != 0);
    BT_CHECK(strcmp(mutated.files[0].sha256, dig.files[0].sha256) != 0);
    BT_CHECK_EQ(mutated.rows, dig.rows);
    for (size_t i = 1; i < dig.file_count; i++) {
        BT_CHECK_EQ(strcmp(mutated.files[i].sha256, dig.files[i].sha256), 0);
    }
}
