#include "common/bt_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "data/corpus.h"
#include "data/digest.h"

enum { TDG_PATH_MAX = 512 };

static const bt_solver WALK_SOLVERS[BT_SOLVER_VENDOR_COUNT] = {
    BT_SOLVER_CHERRY,
    BT_SOLVER_EGGINS,
};
static const bt_sheet_class WALK_SHEETS[BT_SHEET_CLASS_COUNT] = {
    BT_SHEET_EDGE, BT_SHEET_CORNER, BT_SHEET_FLIPS, BT_SHEET_TWISTS, BT_SHEET_PARITY, BT_SHEET_LTCT,
};

static void tdg_mkdir_p(const char *path) {
    char partial[TDG_PATH_MAX];
    snprintf(partial, sizeof partial, "%s", path);
    size_t len = strlen(partial);
    for (size_t at = 1; at < len; at++) {
        if (partial[at] != '/' && partial[at] != '\\') {
            continue;
        }
        char keep = partial[at];
        partial[at] = '\0';
#ifdef _WIN32
        _mkdir(partial);
#else
        mkdir(partial, 0755);
#endif
        partial[at] = keep;
    }
#ifdef _WIN32
    _mkdir(partial);
#else
    mkdir(partial, 0755);
#endif
}

static void tdg_join(char out[TDG_PATH_MAX], const char *base, const char *leaf) {
    snprintf(out, TDG_PATH_MAX, "%s/%s", base, leaf);
}

static int tdg_is_dir(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && (info.st_mode & S_IFMT) == S_IFDIR;
}

static void tdg_rm_rf(const char *path) {
    if (!tdg_is_dir(path)) {
        remove(path);
        return;
    }
#ifdef _WIN32
    char pattern[TDG_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", path);
    WIN32_FIND_DATAA entry;
    HANDLE handle = FindFirstFileA(pattern, &entry);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
                continue;
            }
            char child[TDG_PATH_MAX];
            tdg_join(child, path, entry.cFileName);
            tdg_rm_rf(child);
        } while (FindNextFileA(handle, &entry) != 0);
        FindClose(handle);
    }
    _rmdir(path);
#else
    DIR *dir = opendir(path);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child[TDG_PATH_MAX];
            tdg_join(child, path, entry->d_name);
            tdg_rm_rf(child);
        }
        closedir(dir);
    }
    rmdir(path);
#endif
}

static void tdg_scratch(const char *tag, char out[TDG_PATH_MAX]) {
    snprintf(out, TDG_PATH_MAX, "build/test-tmp/digest-%s", tag);
    tdg_rm_rf(out);
    tdg_mkdir_p(out);
}

static void tdg_write_file(const char *path, const char *text) {
    char parent[TDG_PATH_MAX];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '/');
    char *back = strrchr(parent, '\\');
    char *cut = back > slash ? back : slash;
    if (cut != NULL) {
        *cut = '\0';
        tdg_mkdir_p(parent);
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        BT_ABORT("cannot open %s", path);
    }
    size_t len = strlen(text);
    if (fwrite(text, 1, len, file) != len) {
        BT_ABORT("short write on %s", path);
    }
    fclose(file);
}

BT_TEST(digest, empty_input_matches_nist_vector) {
    char out[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_sha256_hex("", 0, out);
    BT_CHECK_EQ(out, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

BT_TEST(digest, short_input_matches_nist_vector) {
    char out[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_sha256_hex("abc", 3, out);
    BT_CHECK_EQ(out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

BT_TEST(digest, two_block_input_matches_nist_vector) {
    char out[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, out);
    BT_CHECK_EQ(out, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

BT_TEST(digest, corpus_hash_is_the_ordered_concatenation) {
    char out[BT_DIGEST_SHA256_HEX_LEN];
    char want[BT_DIGEST_SHA256_HEX_LEN];
    const unsigned char *parts[2] = {(const unsigned char *)"a", (const unsigned char *)"bc"};
    const size_t lens[2] = {1, 2};
    bt_digest_corpus_sha256_hex(parts, lens, 2, out);
    bt_digest_sha256_hex("abc", 3, want);
    BT_CHECK_EQ(out, want);
    bt_digest_corpus_sha256_hex(NULL, NULL, 0, out);
    bt_digest_sha256_hex("", 0, want);
    BT_CHECK_EQ(out, want);
}

BT_TEST(digest, digest_corpus_walks_vendor_sheets_in_canonical_order) {
    char root[TDG_PATH_MAX];
    tdg_scratch("walk", root);
    unsigned char *texts[BT_DIGEST_FILES];
    size_t text_lens[BT_DIGEST_FILES];
    size_t slot = 0;
    for (size_t index = 0; index < BT_SOLVER_VENDOR_COUNT; index++) {
        for (size_t sheet_at = 0; sheet_at < BT_SHEET_CLASS_COUNT; sheet_at++) {
            size_t data_rows = index * (size_t)BT_SHEET_CLASS_COUNT + sheet_at + 1;
            char text[8192];
            size_t used =
                (size_t)snprintf(text, sizeof text,
                                 "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\n");
            for (size_t row = 0; row < data_rows; row++) {
                used += (size_t)snprintf(text + used, sizeof text - used,
                                         "p%zu-%zu-%zu\tR U R' U'\t\t\t\n", index, sheet_at, row);
            }
            char path[TDG_PATH_MAX];
            snprintf(path, sizeof path, "%s/%s/%s.tsv", root, bt_solver_dir(WALK_SOLVERS[index]),
                     bt_sheet_class_file(WALK_SHEETS[sheet_at]));
            tdg_write_file(path, text);
            texts[slot] = malloc(used + 1);
            BT_CHECK(texts[slot] != NULL);
            memcpy(texts[slot], text, used + 1);
            text_lens[slot] = used;
            slot += 1;
        }
    }
    bt_corpus_digest digest;
    char err[BT_DIGEST_ERR_MAX];
    int rc = bt_digest_corpus(root, &digest, err, sizeof err);
    BT_CHECK_EQ(rc, 0);
    char want[BT_DIGEST_SHA256_HEX_LEN];
    bt_digest_corpus_sha256_hex((const unsigned char *const *)texts, text_lens, BT_DIGEST_FILES,
                                want);
    BT_CHECK_EQ(digest.sha256, want);
    BT_CHECK_EQ(digest.rows, 78);
    BT_CHECK_EQ(digest.file_count, 12);
    for (size_t at = 0; at < digest.file_count; at++) {
        BT_CHECK_EQ(digest.files[at].solver,
                    bt_solver_dir(WALK_SOLVERS[at / BT_SHEET_CLASS_COUNT]));
        BT_CHECK_EQ(digest.files[at].sheet,
                    bt_sheet_class_file(WALK_SHEETS[at % BT_SHEET_CLASS_COUNT]));
        BT_CHECK_EQ(digest.files[at].rows, at + 1);
        bt_digest_sha256_hex(texts[at], text_lens[at], want);
        BT_CHECK_EQ(digest.files[at].sha256, want);
        free(texts[at]);
    }
    tdg_rm_rf(root);
}

BT_TEST(digest, digest_corpus_reports_a_missing_sheet) {
    char root[TDG_PATH_MAX];
    tdg_scratch("miss", root);
    bt_corpus_digest digest;
    char err[BT_DIGEST_ERR_MAX];
    int rc = bt_digest_corpus(root, &digest, err, sizeof err);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK(strncmp(err, "read ", 5) == 0);
    tdg_rm_rf(root);
}

BT_TEST(digest, digest_corpus_rejects_a_malformed_sheet) {
    char root[TDG_PATH_MAX];
    char path[TDG_PATH_MAX];
    tdg_scratch("bad", root);
    snprintf(path, sizeof path, "%s/%s/%s.tsv", root, bt_solver_dir(BT_SOLVER_CHERRY),
             bt_sheet_class_file(BT_SHEET_EDGE));
    tdg_write_file(path,
                   "Position\tAlg\tCommutator\tReconstruction Link\tYoutube Link\nonly\ttwo\n");
    bt_corpus_digest digest;
    char err[BT_DIGEST_ERR_MAX];
    int rc = bt_digest_corpus(root, &digest, err, sizeof err);
    BT_CHECK_EQ(rc, 1);
    BT_CHECK(strncmp(err, "parse ", 6) == 0);
    tdg_rm_rf(root);
}
