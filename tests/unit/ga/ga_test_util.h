#ifndef BT_GA_TEST_UTIL_H
#define BT_GA_TEST_UTIL_H

#include "common/bt_harness.h"
#include "common/config.h"
#include "common/paths.h"
#include "common/rng.h"
#include "ga/features.h"
#include "ga/inventory.h"

#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <windows.h>

#define BT_GA_WATCHDOG_SECS 120

static DWORD WINAPI bt_ga_watchdog_main(void *unused) {
    (void)unused;
    Sleep(BT_GA_WATCHDOG_SECS * 1000);
    exit(101);
    return 0;
}

static inline void bt_ga_arm_hang_watchdog(void) {
    HANDLE thread = CreateThread(NULL, 0, bt_ga_watchdog_main, NULL, 0, NULL);
    if (thread != NULL) {
        CloseHandle(thread);
    }
}

static inline void bt_ga_scratch(const char *name, char out[BT_PATH_MAX]) {
    char temp[BT_PATH_MAX];
    GetTempPathA(sizeof temp, temp);
    snprintf(out, BT_PATH_MAX, "%sbt-ga-%s-%d", temp, name, _getpid());
    _mkdir(out);
}

static inline void bt_ga_scratch_remove(const char *name) {
    char dir[BT_PATH_MAX];
    bt_ga_scratch(name, dir);
    char command[BT_PATH_MAX + 16];
    snprintf(command, sizeof command, "rmdir /s /q \"%s\"", dir);
    system(command);
}

static inline bt_ga_features bt_ga_feat(bt_ga_tone tone, int has_onset, bt_ga_place place,
                                        bt_ga_manner manner, bt_ga_nucleus nucleus,
                                        bt_ga_coda coda) {
    bt_ga_features out;
    out.tone = tone;
    out.has_onset = has_onset;
    out.onset.place = place;
    out.onset.manner = manner;
    out.nucleus = nucleus;
    out.coda = coda;
    return out;
}

static inline bt_ga_features bt_ga_parse_or_abort(const char *text) {
    bt_ga_features out;
    if (!bt_ga_parse(text, &out)) {
        BT_ABORT("parse failed for %s", text);
    }
    return out;
}

static inline void bt_ga_wordlist_from(const char *const *texts, size_t count, bt_ga_strset *set) {
    set->items = calloc(count, sizeof *set->items);
    set->count = count;
    for (size_t i = 0; i < count; i++) {
        set->items[i] = _strdup(texts[i]);
    }
}

static inline void bt_ga_vietnamese_dir(char out[BT_PATH_MAX]) {
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    const bt_json_value *config = bt_config_load();
    const char *ref_doc = NULL;
    if (config == NULL || !bt_config_str(config, "paths.ref_doc", &ref_doc)) {
        BT_ABORT("paths.ref_doc missing from the hub");
    }
    snprintf(out, BT_PATH_MAX, "%s/%s/vietnamese", root, ref_doc);
}

static inline int bt_ga_full_wordlist(bt_ga_strset *set) {
    char dir[BT_PATH_MAX];
    bt_ga_vietnamese_dir(dir);
    char path[BT_PATH_MAX];
    snprintf(path, BT_PATH_MAX, "%s/syllables-7184-abc.txt", dir);
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_load_wordlist(path, set, err)) {
        return 0;
    }
    return set->count > 7000;
}

static inline int bt_ga_injective(const uint32_t *chrom, size_t count) {
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (chrom[i] == chrom[j]) {
                return 0;
            }
        }
    }
    return 1;
}

static inline void bt_ga_shuffle_u32(bt_rng *rng, uint32_t *items, size_t count) {
    for (size_t i = count; i > 1; i--) {
        size_t j = (size_t)bt_rng_below(rng, (uint64_t)i);
        uint32_t tmp = items[i - 1];
        items[i - 1] = items[j];
        items[j] = tmp;
    }
}

#endif
