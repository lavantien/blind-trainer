#include "ga/inventory.h"

#include "common/bt_harness.h"

#include "ga_test_util.h"

static bt_ga_freqs freqs_from(const char *const *keys, const uint64_t *values, size_t count) {
    bt_ga_freqs freqs;
    freqs.keys = calloc(count, sizeof *freqs.keys);
    freqs.values = calloc(count, sizeof *freqs.values);
    freqs.count = count;
    for (size_t i = 0; i < count; i++) {
        freqs.keys[i] = _strdup(keys[i]);
        freqs.values[i] = values[i];
    }
    return freqs;
}

BT_TEST(inventory, stop_coda_syllables_take_only_sac_or_nang) {
    bt_ga_strset wordlist;
    if (!bt_ga_full_wordlist(&wordlist)) {
        BT_ABORT("full wordlist unavailable");
    }
    bt_ga_inventory_entry *entries = NULL;
    size_t count = 0;
    bt_ga_inventory_report report;
    char inv_err[BT_GA_ERR_MAX];
    if (!bt_ga_build_inventory(&wordlist, &entries, &count, &report, inv_err)) {
        BT_ABORT("build_inventory: %s", inv_err);
    }
    BT_CHECK(count > 0);
    for (size_t i = 0; i < count; i++) {
        if (entries[i].coda == BT_GA_CODA_P || entries[i].coda == BT_GA_CODA_T ||
            entries[i].coda == BT_GA_CODA_K) {
            BT_CHECK(entries[i].tone == BT_GA_TONE_SAC || entries[i].tone == BT_GA_TONE_NANG);
        }
    }
    bt_ga_inventory_free(entries);
    bt_ga_strset_free(&wordlist);
}

BT_TEST(inventory, renderings_are_pinned) {
    char out[BT_GA_TEXT_MAX];
    BT_CHECK(bt_ga_render("kh", BT_GA_NUC_OH, BT_GA_CODA_NG, BT_GA_TONE_NGANG, out));
    BT_CHECK_EQ(out, "không");
    BT_CHECK(bt_ga_render("t", BT_GA_NUC_IA, BT_GA_CODA_OPEN, BT_GA_TONE_SAC, out));
    BT_CHECK_EQ(out, "tía");
    BT_CHECK(bt_ga_render("c", BT_GA_NUC_UA, BT_GA_CODA_OPEN, BT_GA_TONE_HOI, out));
    BT_CHECK_EQ(out, "của");
    BT_CHECK(bt_ga_render("gi", BT_GA_NUC_IA, BT_GA_CODA_OPEN, BT_GA_TONE_NANG, out));
    BT_CHECK_EQ(out, "gịa");
    BT_CHECK(bt_ga_render("m", BT_GA_NUC_U, BT_GA_CODA_T, BT_GA_TONE_SAC, out));
    BT_CHECK_EQ(out, "mút");
    BT_CHECK(bt_ga_render("ngh", BT_GA_NUC_IA, BT_GA_CODA_M, BT_GA_TONE_NANG, out));
    BT_CHECK_EQ(out, "nghiệm");
    BT_CHECK(bt_ga_render("", BT_GA_NUC_E, BT_GA_CODA_NG, BT_GA_TONE_SAC, out));
    BT_CHECK_EQ(out, "ánh");
    BT_CHECK(bt_ga_render("b", BT_GA_NUC_AB, BT_GA_CODA_K, BT_GA_TONE_NANG, out));
    BT_CHECK_EQ(out, "bặc");
    BT_CHECK(bt_ga_render("b", BT_GA_NUC_IA, BT_GA_CODA_NG, BT_GA_TONE_NGANG, out));
    BT_CHECK_EQ(out, "biêng");
    const char *rime = NULL;
    size_t mark = 0;
    BT_CHECK(!bt_ga_nucleus_rime(BT_GA_NUC_OR, BT_GA_CODA_NG, &rime, &mark));
    BT_CHECK(!bt_ga_nucleus_rime(BT_GA_NUC_UW, BT_GA_CODA_M, &rime, &mark));
    BT_CHECK(!bt_ga_nucleus_rime(BT_GA_NUC_AH, BT_GA_CODA_OPEN, &rime, &mark));
    BT_CHECK(!bt_ga_nucleus_rime(BT_GA_NUC_AB, BT_GA_CODA_OPEN, &rime, &mark));
    BT_CHECK(!bt_ga_nucleus_rime(BT_GA_NUC_UA, BT_GA_CODA_P, &rime, &mark));
}

BT_TEST(inventory, zero_onset_forms_include_y_variants) {
    bt_ga_form_list forms;
    bt_ga_zero_onset_variants(BT_GA_NUC_IA, BT_GA_CODA_OPEN, BT_GA_TONE_NANG, &forms);
    int has_ia = 0;
    int has_ya = 0;
    for (size_t i = 0; i < forms.count; i++) {
        has_ia = has_ia || strcmp(forms.items[i], "\xe1\xbb\x8b"
                                                  "a") == 0;
        has_ya = has_ya || strcmp(forms.items[i], "y\xe1\xba\xa1") == 0;
    }
    BT_CHECK(has_ia && has_ya);
    bt_ga_zero_onset_variants(BT_GA_NUC_I, BT_GA_CODA_OPEN, BT_GA_TONE_NGANG, &forms);
    int has_i = 0;
    int has_y = 0;
    for (size_t i = 0; i < forms.count; i++) {
        has_i = has_i || strcmp(forms.items[i], "i") == 0;
        has_y = has_y || strcmp(forms.items[i], "y") == 0;
    }
    BT_CHECK(has_i && has_y);
    bt_ga_zero_onset_variants(BT_GA_NUC_I, BT_GA_CODA_K, BT_GA_TONE_SAC, &forms);
    int has_ich = 0;
    int has_ych = 0;
    for (size_t i = 0; i < forms.count; i++) {
        has_ich = has_ich || strcmp(forms.items[i], "\xc3\xad"
                                                    "ch") == 0;
        has_ych = has_ych || strcmp(forms.items[i], "\xc3\xbd"
                                                    "ch") == 0;
    }
    BT_CHECK(has_ich && has_ych);
}

BT_TEST(inventory, inventory_filters_against_wordlist_and_reports) {
    const char *const texts[] = {"không", "tía", "của", "zzzz"};
    bt_ga_strset wordlist;
    bt_ga_wordlist_from(texts, 4, &wordlist);
    bt_ga_inventory_entry *entries = NULL;
    size_t count = 0;
    bt_ga_inventory_report report;
    char inv_err[BT_GA_ERR_MAX];
    if (!bt_ga_build_inventory(&wordlist, &entries, &count, &report, inv_err)) {
        BT_ABORT("build_inventory: %s", inv_err);
    }
    int has_khong = 0;
    int has_tia = 0;
    int has_cua = 0;
    int has_zzzz = 0;
    for (size_t i = 0; i < count; i++) {
        has_khong = has_khong || strcmp(entries[i].text, "không") == 0;
        has_tia = has_tia || strcmp(entries[i].text, "tía") == 0;
        has_cua = has_cua || strcmp(entries[i].text, "của") == 0;
        has_zzzz = has_zzzz || strcmp(entries[i].text, "zzzz") == 0;
    }
    BT_CHECK(has_khong && has_tia && has_cua && !has_zzzz);
    BT_CHECK_EQ((long long)report.wordlist_size, 4LL);
    BT_CHECK_EQ((long long)report.wordlist_covered, 3LL);
    BT_CHECK_EQ((long long)report.kept, (long long)count);
    BT_CHECK_EQ((long long)report.dropped_not_in_wordlist,
                (long long)(report.generated - report.kept));
    bt_ga_inventory_free(entries);
    bt_ga_strset_free(&wordlist);
}

BT_TEST(inventory, build_inventory_is_deterministic) {
    bt_ga_strset wordlist;
    if (!bt_ga_full_wordlist(&wordlist)) {
        BT_ABORT("full wordlist unavailable");
    }
    bt_ga_inventory_entry *a = NULL;
    bt_ga_inventory_entry *b = NULL;
    size_t count_a = 0;
    size_t count_b = 0;
    bt_ga_inventory_report ra;
    bt_ga_inventory_report rb;
    char inv_err[BT_GA_ERR_MAX];
    if (!bt_ga_build_inventory(&wordlist, &a, &count_a, &ra, inv_err)) {
        BT_ABORT("build_inventory: %s", inv_err);
    }
    if (!bt_ga_build_inventory(&wordlist, &b, &count_b, &rb, inv_err)) {
        BT_ABORT("build_inventory: %s", inv_err);
    }
    BT_CHECK(count_a == count_b);
    int equal = 1;
    for (size_t i = 0; i < count_a; i++) {
        equal = equal && strcmp(a[i].text, b[i].text) == 0 && a[i].onset == b[i].onset &&
                a[i].nucleus == b[i].nucleus && a[i].coda == b[i].coda && a[i].tone == b[i].tone;
    }
    BT_CHECK(equal);
    BT_CHECK(memcmp(&ra, &rb, sizeof ra) == 0);
    bt_ga_inventory_free(a);
    bt_ga_inventory_free(b);
    bt_ga_strset_free(&wordlist);
}

BT_TEST(inventory, generated_entries_carry_no_combining_marks) {
    bt_ga_strset wordlist;
    if (!bt_ga_full_wordlist(&wordlist)) {
        BT_ABORT("full wordlist unavailable");
    }
    bt_ga_inventory_entry *entries = NULL;
    size_t count = 0;
    bt_ga_inventory_report report;
    char inv_err[BT_GA_ERR_MAX];
    if (!bt_ga_build_inventory(&wordlist, &entries, &count, &report, inv_err)) {
        BT_ABORT("build_inventory: %s", inv_err);
    }
    for (size_t i = 0; i < count; i++) {
        BT_CHECK(bt_ga_is_nfc(entries[i].text));
    }
    bt_ga_inventory_free(entries);
    bt_ga_strset_free(&wordlist);
}

BT_TEST(inventory, pool_ranks_by_frequency_and_stays_in_wordlist) {
    const char *const texts[] = {"ma", "má", "mà"};
    bt_ga_strset wordlist;
    bt_ga_wordlist_from(texts, 3, &wordlist);
    bt_ga_inventory_entry *entries = NULL;
    size_t count = 0;
    bt_ga_inventory_report report;
    char inv_err[BT_GA_ERR_MAX];
    if (!bt_ga_build_inventory(&wordlist, &entries, &count, &report, inv_err)) {
        BT_ABORT("build_inventory: %s", inv_err);
    }
    const char *const keys[] = {"má", "ma", "mà", "zzz"};
    const uint64_t values[] = {500, 900, 900, 99999};
    bt_ga_freqs freqs = freqs_from(keys, values, 4);
    bt_ga_pool_entry *pool = NULL;
    size_t pool_count = 0;
    bt_ga_build_pool(entries, count, &freqs, 2, &pool, &pool_count);
    BT_CHECK_EQ((long long)pool_count, 2LL);
    BT_CHECK_EQ(pool[0].text, "ma");
    BT_CHECK_EQ(pool[1].text, "mà");
    for (size_t i = 0; i < pool_count; i++) {
        BT_CHECK(bt_ga_strset_contains(&wordlist, pool[i].text));
    }
    bt_ga_pool_free(pool);
    bt_ga_freqs_free(&freqs);
    bt_ga_inventory_free(entries);
    bt_ga_strset_free(&wordlist);
}

BT_TEST(inventory, real_pool_size_meets_case_count) {
    const bt_json_value *config = bt_config_load();
    size_t top_k = 0;
    char err[BT_GA_ERR_MAX];
    if (config == NULL || !bt_ga_config_pool_top_k(config, &top_k, err)) {
        BT_ABORT("pool_top_k missing");
    }
    bt_ga_strset wordlist;
    if (!bt_ga_full_wordlist(&wordlist)) {
        BT_ABORT("full wordlist unavailable");
    }
    bt_ga_inventory_entry *entries = NULL;
    size_t count = 0;
    bt_ga_inventory_report report;
    char inv_err[BT_GA_ERR_MAX];
    if (!bt_ga_build_inventory(&wordlist, &entries, &count, &report, inv_err)) {
        BT_ABORT("build_inventory: %s", inv_err);
    }
    char dir[BT_PATH_MAX];
    bt_ga_vietnamese_dir(dir);
    char path[BT_PATH_MAX];
    snprintf(path, BT_PATH_MAX, "%s/vn-word-frequencies.tsv", dir);
    bt_ga_freqs freqs;
    if (!bt_ga_load_syllable_frequencies(path, &freqs, err)) {
        BT_ABORT("freqs load: %s", err);
    }
    bt_ga_pool_entry *pool = NULL;
    size_t pool_count = 0;
    bt_ga_build_pool(entries, count, &freqs, top_k, &pool, &pool_count);
    BT_CHECK(pool_count >= bt_ga_case_count());
    for (size_t i = 0; i < pool_count; i++) {
        for (size_t j = i + 1; j < pool_count; j++) {
            BT_CHECK(strcmp(pool[i].text, pool[j].text) != 0);
        }
        BT_CHECK(bt_ga_strset_contains(&wordlist, pool[i].text));
        BT_CHECK(pool[i].frequency > 0);
    }
    BT_CHECK(report.wordlist_covered > 1000);
    bt_ga_pool_free(pool);
    bt_ga_freqs_free(&freqs);
    bt_ga_inventory_free(entries);
    bt_ga_strset_free(&wordlist);
}

BT_TEST(inventory, nucleus_rime_marks_and_gaps_are_pinned) {
    const char *rime = NULL;
    size_t mark = 0;
    BT_CHECK(bt_ga_nucleus_rime(BT_GA_NUC_IA, BT_GA_CODA_T, &rime, &mark));
    BT_CHECK_EQ(rime, "i\xc3\xaa");
    BT_CHECK_EQ((long long)mark, 1LL);
    BT_CHECK(bt_ga_nucleus_rime(BT_GA_NUC_WA, BT_GA_CODA_M, &rime, &mark));
    BT_CHECK_EQ(rime, "\xc6\xb0\xc6\xa1");
    BT_CHECK_EQ((long long)mark, 1LL);
    BT_CHECK(bt_ga_nucleus_rime(BT_GA_NUC_UA, BT_GA_CODA_NG, &rime, &mark));
    BT_CHECK_EQ(rime, "u\xc3\xb4");
    BT_CHECK_EQ((long long)mark, 1LL);
    BT_CHECK(bt_ga_nucleus_rime(BT_GA_NUC_U, BT_GA_CODA_T, &rime, &mark));
    BT_CHECK_EQ(rime, "u");
    BT_CHECK_EQ((long long)mark, 0LL);
    BT_CHECK(bt_ga_nucleus_rime(BT_GA_NUC_E, BT_GA_CODA_NG, &rime, &mark));
    BT_CHECK_EQ(rime, "a");
    BT_CHECK_EQ((long long)mark, 0LL);
    BT_CHECK(bt_ga_nucleus_rime(BT_GA_NUC_AH, BT_GA_CODA_P, &rime, &mark));
    BT_CHECK_EQ(rime, "\xc3\xa2");
    BT_CHECK_EQ((long long)mark, 0LL);
    BT_CHECK(bt_ga_nucleus_rime(BT_GA_NUC_I, BT_GA_CODA_OPEN, &rime, &mark));
    BT_CHECK_EQ(rime, "i");
    BT_CHECK_EQ((long long)mark, 0LL);
    BT_CHECK(!bt_ga_nucleus_rime(BT_GA_NUC_UW, BT_GA_CODA_N, &rime, &mark));
    BT_CHECK(!bt_ga_nucleus_rime(BT_GA_NUC_OR, BT_GA_CODA_K, &rime, &mark));
}

BT_TEST(inventory, tone_char_marks_every_base_and_passes_unknown_through) {
    BT_CHECK_EQ(bt_ga_tone_char("a", BT_GA_TONE_HUYEN), "\xc3\xa0");
    BT_CHECK_EQ(bt_ga_tone_char("a", BT_GA_TONE_SAC), "\xc3\xa1");
    BT_CHECK_EQ(bt_ga_tone_char("a", BT_GA_TONE_HOI), "\xe1\xba\xa3");
    BT_CHECK_EQ(bt_ga_tone_char("a", BT_GA_TONE_NGA), "\xc3\xa3");
    BT_CHECK_EQ(bt_ga_tone_char("a", BT_GA_TONE_NANG), "\xe1\xba\xa1");
    BT_CHECK_EQ(bt_ga_tone_char("e", BT_GA_TONE_SAC), "\xc3\xa9");
    BT_CHECK_EQ(bt_ga_tone_char("i", BT_GA_TONE_NANG), "\xe1\xbb\x8b");
    BT_CHECK_EQ(bt_ga_tone_char("o", BT_GA_TONE_HOI), "\xe1\xbb\x8f");
    BT_CHECK_EQ(bt_ga_tone_char("u", BT_GA_TONE_NGA), "\xc5\xa9");
    BT_CHECK_EQ(bt_ga_tone_char("y", BT_GA_TONE_HUYEN), "\xe1\xbb\xb3");
    BT_CHECK_EQ(bt_ga_tone_char("\xc4\x83", BT_GA_TONE_NGA), "\xe1\xba\xb5");
    BT_CHECK_EQ(bt_ga_tone_char("\xc3\xa2", BT_GA_TONE_HOI), "\xe1\xba\xa9");
    BT_CHECK_EQ(bt_ga_tone_char("\xc3\xaa", BT_GA_TONE_SAC), "\xe1\xba\xbf");
    BT_CHECK_EQ(bt_ga_tone_char("\xc3\xb4", BT_GA_TONE_SAC), "\xe1\xbb\x91");
    BT_CHECK_EQ(bt_ga_tone_char("\xc6\xa1", BT_GA_TONE_HUYEN), "\xe1\xbb\x9d");
    BT_CHECK_EQ(bt_ga_tone_char("\xc6\xb0", BT_GA_TONE_SAC), "\xe1\xbb\xa9");
    BT_CHECK_EQ(bt_ga_tone_char("q", BT_GA_TONE_SAC), "q");
    BT_CHECK_EQ(bt_ga_tone_char("w", BT_GA_TONE_NANG), "w");
    BT_CHECK_EQ(bt_ga_tone_char("a", BT_GA_TONE_NGANG), "a");
}

BT_TEST(inventory, zero_onset_variants_fill_the_remaining_coda_arms) {
    bt_ga_form_list forms;
    bt_ga_zero_onset_variants(BT_GA_NUC_I, BT_GA_CODA_M, BT_GA_TONE_SAC, &forms);
    BT_CHECK_EQ((long long)forms.count, 2LL);
    BT_CHECK_EQ(forms.items[0], "\xc3\xadm");
    BT_CHECK_EQ(forms.items[1], "\xc3\xbdm");
    bt_ga_zero_onset_variants(BT_GA_NUC_I, BT_GA_CODA_P, BT_GA_TONE_NANG, &forms);
    BT_CHECK_EQ((long long)forms.count, 2LL);
    BT_CHECK_EQ(forms.items[0], "\xe1\xbb\x8bp");
    BT_CHECK_EQ(forms.items[1], "\xe1\xbb\xb5p");
    bt_ga_zero_onset_variants(BT_GA_NUC_I, BT_GA_CODA_NG, BT_GA_TONE_SAC, &forms);
    BT_CHECK_EQ((long long)forms.count, 2LL);
    BT_CHECK_EQ(forms.items[0], "\xc3\xadnh");
    BT_CHECK_EQ(forms.items[1], "\xc3\xbdnh");
    bt_ga_zero_onset_variants(BT_GA_NUC_U, BT_GA_CODA_OPEN, BT_GA_TONE_NGANG, &forms);
    BT_CHECK_EQ((long long)forms.count, 1LL);
    BT_CHECK_EQ(forms.items[0], "u");
    bt_ga_zero_onset_variants(BT_GA_NUC_UW, BT_GA_CODA_N, BT_GA_TONE_SAC, &forms);
    BT_CHECK_EQ((long long)forms.count, 0LL);
    bt_ga_zero_onset_variants(BT_GA_NUC_AH, BT_GA_CODA_OPEN, BT_GA_TONE_NGANG, &forms);
    BT_CHECK_EQ((long long)forms.count, 0LL);
}

BT_TEST(inventory, render_pins_mark_positions_and_prefix_rules) {
    char out[BT_GA_TEXT_MAX];
    BT_CHECK(bt_ga_render("l", BT_GA_NUC_IA, BT_GA_CODA_T, BT_GA_TONE_NANG, out));
    BT_CHECK_EQ(out, "li\xe1\xbb\x87t");
    BT_CHECK(bt_ga_render("x", BT_GA_NUC_WA, BT_GA_CODA_OPEN, BT_GA_TONE_NGANG, out));
    BT_CHECK_EQ(out, "x\xc6\xb0"
                     "a");
    BT_CHECK(bt_ga_render("", BT_GA_NUC_UW, BT_GA_CODA_OPEN, BT_GA_TONE_SAC, out));
    BT_CHECK_EQ(out, "\xe1\xbb\xa9");
    BT_CHECK(bt_ga_render("b", BT_GA_NUC_AH, BT_GA_CODA_N, BT_GA_TONE_HUYEN, out));
    BT_CHECK_EQ(out, "b\xe1\xba\xa7n");
    BT_CHECK(bt_ga_render("b", BT_GA_NUC_OH, BT_GA_CODA_K, BT_GA_TONE_SAC, out));
    BT_CHECK_EQ(out, "b\xe1\xbb\x91"
                     "c");
    BT_CHECK(bt_ga_render("b", BT_GA_NUC_E, BT_GA_CODA_K, BT_GA_TONE_SAC, out));
    BT_CHECK_EQ(out, "b\xc3\xa1"
                     "ch");
    BT_CHECK(bt_ga_render("b", BT_GA_NUC_A, BT_GA_CODA_NG, BT_GA_TONE_NGANG, out));
    BT_CHECK_EQ(out, "bang");
    BT_CHECK(bt_ga_render("l", BT_GA_NUC_UA, BT_GA_CODA_N, BT_GA_TONE_HUYEN, out));
    BT_CHECK_EQ(out, "lu\xe1\xbb\x93n");
    BT_CHECK(!bt_ga_render("b", BT_GA_NUC_AH, BT_GA_CODA_OPEN, BT_GA_TONE_SAC, out));
}

BT_TEST(inventory, onset_spellings_track_frontness_and_z_alternates) {
    bt_ga_onset_spelling_list list;
    bt_ga_onset_spellings(BT_GA_ONSET_K, BT_GA_NUC_I, &list);
    BT_CHECK_EQ((long long)list.count, 1LL);
    BT_CHECK_EQ(list.items[0], "k");
    bt_ga_onset_spellings(BT_GA_ONSET_K, BT_GA_NUC_A, &list);
    BT_CHECK_EQ((long long)list.count, 2LL);
    BT_CHECK_EQ(list.items[0], "c");
    BT_CHECK_EQ(list.items[1], "k");
    bt_ga_onset_spellings(BT_GA_ONSET_NG, BT_GA_NUC_EH, &list);
    BT_CHECK_EQ((long long)list.count, 1LL);
    BT_CHECK_EQ(list.items[0], "ngh");
    bt_ga_onset_spellings(BT_GA_ONSET_NG, BT_GA_NUC_OH, &list);
    BT_CHECK_EQ((long long)list.count, 1LL);
    BT_CHECK_EQ(list.items[0], "ng");
    bt_ga_onset_spellings(BT_GA_ONSET_G, BT_GA_NUC_IA, &list);
    BT_CHECK_EQ((long long)list.count, 1LL);
    BT_CHECK_EQ(list.items[0], "gh");
    bt_ga_onset_spellings(BT_GA_ONSET_G, BT_GA_NUC_A, &list);
    BT_CHECK_EQ((long long)list.count, 1LL);
    BT_CHECK_EQ(list.items[0], "g");
    bt_ga_onset_spellings(BT_GA_ONSET_Z, BT_GA_NUC_OH, &list);
    BT_CHECK_EQ((long long)list.count, 2LL);
    BT_CHECK_EQ(list.items[0], "d");
    BT_CHECK_EQ(list.items[1], "r");
    bt_ga_onset_spellings(BT_GA_ONSET_Z, BT_GA_NUC_A, &list);
    BT_CHECK_EQ((long long)list.count, 3LL);
    BT_CHECK_EQ(list.items[0], "d");
    BT_CHECK_EQ(list.items[1], "r");
    BT_CHECK_EQ(list.items[2], "gi");
    bt_ga_onset_spellings(BT_GA_ONSET_Z, BT_GA_NUC_WA, &list);
    BT_CHECK_EQ((long long)list.count, 3LL);
    bt_ga_onset_spellings(BT_GA_ONSET_Z, BT_GA_NUC_UA, &list);
    BT_CHECK_EQ((long long)list.count, 2LL);
    bt_ga_onset_spellings(BT_GA_ONSET_B, BT_GA_NUC_A, &list);
    BT_CHECK_EQ((long long)list.count, 1LL);
    BT_CHECK_EQ(list.items[0], "b");
    bt_ga_onset_spellings(BT_GA_ONSET_ZERO, BT_GA_NUC_A, &list);
    BT_CHECK_EQ((long long)list.count, 1LL);
    BT_CHECK_EQ(list.items[0], "");
}

BT_TEST(inventory, build_inventory_pins_cells_across_feature_slots) {
    const char *const texts[] = {"nghiệm", "xưa", "mút", "gịa"};
    bt_ga_strset wordlist;
    bt_ga_wordlist_from(texts, 4, &wordlist);
    bt_ga_inventory_entry *entries = NULL;
    size_t count = 0;
    bt_ga_inventory_report report;
    char inv_err[BT_GA_ERR_MAX];
    if (!bt_ga_build_inventory(&wordlist, &entries, &count, &report, inv_err)) {
        BT_ABORT("build_inventory: %s", inv_err);
    }
    const bt_ga_inventory_entry *nghiem = NULL;
    const bt_ga_inventory_entry *xua = NULL;
    const bt_ga_inventory_entry *mut = NULL;
    const bt_ga_inventory_entry *gia = NULL;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].text, "nghiệm") == 0) {
            nghiem = &entries[i];
        }
        if (strcmp(entries[i].text, "xưa") == 0) {
            xua = &entries[i];
        }
        if (strcmp(entries[i].text, "mút") == 0) {
            mut = &entries[i];
        }
        if (strcmp(entries[i].text, "gịa") == 0) {
            gia = &entries[i];
        }
    }
    BT_CHECK(nghiem != NULL && xua != NULL && mut != NULL && gia != NULL);
    BT_CHECK_EQ((long long)nghiem->onset, (long long)BT_GA_ONSET_NG);
    BT_CHECK_EQ((long long)nghiem->nucleus, (long long)BT_GA_NUC_IA);
    BT_CHECK_EQ((long long)nghiem->coda, (long long)BT_GA_CODA_M);
    BT_CHECK_EQ((long long)nghiem->tone, (long long)BT_GA_TONE_NANG);
    BT_CHECK_EQ((long long)xua->onset, (long long)BT_GA_ONSET_X);
    BT_CHECK_EQ((long long)xua->nucleus, (long long)BT_GA_NUC_WA);
    BT_CHECK_EQ((long long)xua->coda, (long long)BT_GA_CODA_OPEN);
    BT_CHECK_EQ((long long)xua->tone, (long long)BT_GA_TONE_NGANG);
    BT_CHECK_EQ((long long)mut->onset, (long long)BT_GA_ONSET_M);
    BT_CHECK_EQ((long long)mut->nucleus, (long long)BT_GA_NUC_U);
    BT_CHECK_EQ((long long)mut->coda, (long long)BT_GA_CODA_T);
    BT_CHECK_EQ((long long)mut->tone, (long long)BT_GA_TONE_SAC);
    BT_CHECK_EQ((long long)gia->onset, (long long)BT_GA_ONSET_Z);
    BT_CHECK_EQ((long long)gia->nucleus, (long long)BT_GA_NUC_IA);
    BT_CHECK_EQ((long long)gia->coda, (long long)BT_GA_CODA_OPEN);
    BT_CHECK_EQ((long long)gia->tone, (long long)BT_GA_TONE_NANG);
    BT_CHECK_EQ((long long)report.wordlist_size, 4LL);
    BT_CHECK_EQ((long long)report.wordlist_covered, 4LL);
    for (size_t i = 0; i < count; i++) {
        BT_CHECK(bt_ga_strset_contains(&wordlist, entries[i].text));
    }
    bt_ga_inventory_free(entries);
    bt_ga_strset_free(&wordlist);
}

BT_TEST(inventory, utf8_checks_reject_malformed_sequences) {
    static const char *const rows[] = {
        "ma\xc3",       "\xe1\xba",         "\xf0\x9f\x98",     "a\x80", "\xc0\xaf", "\xe0\x80\xaf",
        "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf0\x81\xba\xa3",
    };
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        BT_CHECK(!bt_ga_utf8_valid(rows[i]));
        BT_CHECK(!bt_ga_is_nfc(rows[i]));
    }
}
