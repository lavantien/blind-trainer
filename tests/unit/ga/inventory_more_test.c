#include "ga/inventory.h"

#include "common/bt_harness.h"
#include "common/json.h"

#include "ga_test_util.h"

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    fwrite(text, 1, strlen(text), file);
    fclose(file);
}

BT_TEST(inventory_more, wordlist_loader_trims_blanks_and_dedupes) {
    char dir[BT_PATH_MAX];
    bt_ga_scratch("more-wordlist-ok", dir);
    char path[BT_PATH_MAX];
    snprintf(path, BT_PATH_MAX, "%s/words.txt", dir);
    write_file(path, "# note\n\n   \n  ma  \nma\nm\xc3\xa1\n");
    bt_ga_strset set;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_load_wordlist(path, &set, err)) {
        BT_ABORT("load failed: %s", err);
    }
    BT_CHECK_EQ((long long)set.count, 2LL);
    BT_CHECK(bt_ga_strset_contains(&set, "ma"));
    BT_CHECK(bt_ga_strset_contains(&set, "m\xc3\xa1"));
    bt_ga_strset_free(&set);
    bt_ga_scratch_remove("more-wordlist-ok");
}

BT_TEST(inventory_more, wordlist_loader_rejects_missing_broken_and_marked_files) {
    char dir[BT_PATH_MAX];
    bt_ga_scratch("more-wordlist-bad", dir);
    char path[BT_PATH_MAX];
    bt_ga_strset set;
    char err[BT_GA_ERR_MAX];
    snprintf(path, BT_PATH_MAX, "%s/missing.txt", dir);
    BT_CHECK(!bt_ga_load_wordlist(path, &set, err));
    BT_CHECK(strstr(err, "missing.txt") != NULL);
    snprintf(path, BT_PATH_MAX, "%s/bad.txt", dir);
    write_file(path, "la\nla\xcc\x81\n");
    BT_CHECK(!bt_ga_load_wordlist(path, &set, err));
    BT_CHECK(strstr(err, "combining mark") != NULL);
    FILE *file = fopen(path, "wb");
    unsigned char broken[3] = {0xff, 0xfe, 0x00};
    fwrite(broken, 1, 3, file);
    fclose(file);
    BT_CHECK(!bt_ga_load_wordlist(path, &set, err));
    BT_CHECK(strstr(err, "utf8") != NULL);
    bt_ga_scratch_remove("more-wordlist-bad");
}

BT_TEST(inventory_more, frequency_loader_sums_tokens_across_rows) {
    char dir[BT_PATH_MAX];
    bt_ga_scratch("more-freq-ok", dir);
    char path[BT_PATH_MAX];
    snprintf(path, BT_PATH_MAX, "%s/freqs.tsv", dir);
    write_file(
        path,
        "# rank\tcount\tword\n\n1\t500\tma\n2\t300\tm\xc3\xa1\n3\t100\tma nhi\xe1\xbb\x81u\n");
    bt_ga_freqs freqs;
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_load_syllable_frequencies(path, &freqs, err)) {
        BT_ABORT("load failed: %s", err);
    }
    BT_CHECK_EQ((long long)freqs.count, 3LL);
    BT_CHECK_EQ((unsigned long long)bt_ga_freqs_get(&freqs, "ma"), 600ULL);
    BT_CHECK_EQ((unsigned long long)bt_ga_freqs_get(&freqs, "m\xc3\xa1"), 300ULL);
    BT_CHECK_EQ((unsigned long long)bt_ga_freqs_get(&freqs, "nhi\xe1\xbb\x81u"), 100ULL);
    bt_ga_freqs_free(&freqs);
    bt_ga_scratch_remove("more-freq-ok");
}

BT_TEST(inventory_more, frequency_loader_rejects_broken_rows) {
    char dir[BT_PATH_MAX];
    bt_ga_scratch("more-freq-bad", dir);
    char path[BT_PATH_MAX];
    bt_ga_freqs freqs;
    char err[BT_GA_ERR_MAX];
    snprintf(path, BT_PATH_MAX, "%s/missing.tsv", dir);
    BT_CHECK(!bt_ga_load_syllable_frequencies(path, &freqs, err));
    BT_CHECK(strstr(err, "missing.tsv") != NULL);
    snprintf(path, BT_PATH_MAX, "%s/freqs.tsv", dir);
    write_file(path, "1\t500\tma\n2\tx\tb\n");
    BT_CHECK(!bt_ga_load_syllable_frequencies(path, &freqs, err));
    BT_CHECK(strstr(err, "row 2") != NULL);
    write_file(path, "1\t500\tma\xcc\x81\n");
    BT_CHECK(!bt_ga_load_syllable_frequencies(path, &freqs, err));
    BT_CHECK(strstr(err, "combining mark") != NULL);
    FILE *file = fopen(path, "wb");
    unsigned char broken[2] = {0xff, 0xfe};
    fwrite(broken, 1, 2, file);
    fclose(file);
    BT_CHECK(!bt_ga_load_syllable_frequencies(path, &freqs, err));
    BT_CHECK(strstr(err, "utf8") != NULL);
    bt_ga_scratch_remove("more-freq-bad");
}

BT_TEST(inventory_more, pool_truncates_orders_by_frequency_then_text) {
    bt_ga_inventory_entry entries[4];
    const char *const texts[4] = {"z", "b", "a", "m"};
    for (int i = 0; i < 4; i++) {
        strcpy(entries[i].text, texts[i]);
        entries[i].onset = BT_GA_ONSET_M;
        entries[i].nucleus = BT_GA_NUC_A;
        entries[i].coda = BT_GA_CODA_OPEN;
        entries[i].tone = BT_GA_TONE_NGANG;
    }
    const char *const keys[3] = {"z", "b", "a"};
    const uint64_t values[3] = {1, 3, 3};
    bt_ga_freqs freqs;
    freqs.keys = calloc(3, sizeof *freqs.keys);
    freqs.values = calloc(3, sizeof *freqs.values);
    freqs.count = 3;
    for (int i = 0; i < 3; i++) {
        freqs.keys[i] = _strdup(keys[i]);
        freqs.values[i] = values[i];
    }
    bt_ga_pool_entry *pool = NULL;
    size_t pool_count = 0;
    bt_ga_build_pool(entries, 4, &freqs, 0, &pool, &pool_count);
    BT_CHECK_EQ((long long)pool_count, 0LL);
    bt_ga_pool_free(pool);
    bt_ga_freqs empty;
    empty.keys = NULL;
    empty.values = NULL;
    empty.count = 0;
    bt_ga_build_pool(entries, 4, &empty, 10, &pool, &pool_count);
    BT_CHECK_EQ((long long)pool_count, 0LL);
    bt_ga_pool_free(pool);
    bt_ga_build_pool(entries, 4, &freqs, 2, &pool, &pool_count);
    BT_CHECK_EQ((long long)pool_count, 2LL);
    BT_CHECK_EQ(pool[0].text, "a");
    BT_CHECK_EQ(pool[1].text, "b");
    BT_CHECK_EQ((unsigned long long)pool[0].frequency, 3ULL);
    bt_ga_pool_free(pool);
    bt_ga_build_pool(entries, 4, &freqs, 10, &pool, &pool_count);
    BT_CHECK_EQ((long long)pool_count, 3LL);
    BT_CHECK_EQ(pool[0].text, "a");
    BT_CHECK_EQ(pool[1].text, "b");
    BT_CHECK_EQ(pool[2].text, "z");
    bt_ga_pool_free(pool);
    bt_ga_freqs_free(&freqs);
}

static bt_json_value *pool_k_config(const char *literal) {
    bt_json_value *config = NULL;
    bt_json_error error;
    if (bt_json_parse(literal, strlen(literal), &config, &error) != 0) {
        BT_ABORT("literal does not parse: %s", error.message);
    }
    return config;
}

BT_TEST(inventory_more, pool_top_k_reads_only_whole_numbers_from_the_hub) {
    bt_json_value *ok = pool_k_config("{\"ga\": {\"pool_top_k\": 822}}");
    size_t value = 0;
    char err[BT_GA_ERR_MAX];
    BT_CHECK(bt_ga_config_pool_top_k(ok, &value, err));
    BT_CHECK_EQ((long long)value, 822LL);
    bt_json_free(ok);
    bt_json_value *missing = pool_k_config("{\"ga\": {}}");
    BT_CHECK(!bt_ga_config_pool_top_k(missing, &value, err));
    BT_CHECK(strstr(err, "ga.pool_top_k") != NULL);
    bt_json_free(missing);
    static const char *const wrong[4] = {
        "{\"ga\": {\"pool_top_k\": \"822\"}}",
        "{\"ga\": {\"pool_top_k\": 1.5}}",
        "{\"ga\": {\"pool_top_k\": -1}}",
        "{}",
    };
    for (int i = 0; i < 4; i++) {
        bt_json_value *config = pool_k_config(wrong[i]);
        BT_CHECK(!bt_ga_config_pool_top_k(config, &value, err));
        bt_json_free(config);
    }
}
