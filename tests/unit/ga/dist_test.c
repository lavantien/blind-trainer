#include "ga/dist.h"

#include "common/bt_harness.h"
#include "common/config.h"
#include "common/json.h"
#include "common/strbuf.h"

#include "ga_test_util.h"

BT_TEST(dist, freq_data_round_trips_through_json) {
    const char *text = "{\"cases\":[{\"orbit\":\"edge\",\"letters\":\"AB\",\"count\":120},"
                       "{\"orbit\":\"corner\",\"letters\":\"QT\",\"count\":7}],"
                       "\"adjacent\":[{\"from\":0,\"to\":17,\"count\":3}],"
                       "\"cooccurring\":[{\"from\":5,\"to\":6,\"count\":9}],"
                       "\"total_draws\":1000,\"seed\":20261012}";
    bt_json_error error;
    bt_json_value *root = NULL;
    if (bt_json_parse(text, strlen(text), &root, &error) != 0) {
        BT_ABORT("fixture does not parse: %s", error.message);
    }
    bt_strbuf serialized;
    bt_strbuf_init(&serialized, 256);
    bt_json_serialize(root, &serialized);
    bt_json_free(root);
    char dir[BT_PATH_MAX];
    bt_ga_scratch("dist-test", dir);
    char path[BT_PATH_MAX];
    snprintf(path, BT_PATH_MAX, "%s/freq.json", dir);
    FILE *file = fopen(path, "wb");
    fwrite(serialized.data, 1, serialized.len, file);
    fclose(file);
    bt_strbuf_free(&serialized);
    bt_ga_freq_data data;
    char err[128];
    if (!bt_ga_load_freq_data(path, &data, err)) {
        BT_ABORT("load failed: %s", err);
    }
    BT_CHECK_EQ((long long)data.cases_count, 2LL);
    BT_CHECK_EQ(data.cases[0].orbit, "edge");
    BT_CHECK_EQ(data.cases[0].letters, "AB");
    BT_CHECK_EQ((unsigned long long)data.cases[0].count, 120ULL);
    BT_CHECK_EQ(data.cases[1].orbit, "corner");
    BT_CHECK_EQ(data.cases[1].letters, "QT");
    BT_CHECK_EQ((unsigned long long)data.cases[1].count, 7ULL);
    BT_CHECK_EQ((long long)data.adjacent_count, 1LL);
    BT_CHECK_EQ((unsigned long long)data.adjacent[0].from, 0ULL);
    BT_CHECK_EQ((unsigned long long)data.adjacent[0].to, 17ULL);
    BT_CHECK_EQ((unsigned long long)data.adjacent[0].count, 3ULL);
    BT_CHECK_EQ((long long)data.cooccurring_count, 1LL);
    BT_CHECK_EQ((unsigned long long)data.cooccurring[0].from, 5ULL);
    BT_CHECK_EQ((unsigned long long)data.cooccurring[0].to, 6ULL);
    BT_CHECK_EQ((unsigned long long)data.cooccurring[0].count, 9ULL);
    BT_CHECK_EQ((unsigned long long)data.total_draws, 1000ULL);
    BT_CHECK_EQ((unsigned long long)data.seed, 20261012ULL);
    bt_ga_freq_data_free(&data);
    bt_ga_scratch_remove("dist-test");
}

BT_TEST(dist, config_memofreq_reads_the_hub) {
    const bt_json_value *config = bt_config_load();
    uint64_t samples = 0;
    uint64_t seed = 0;
    char err[128];
    if (config == NULL || !bt_ga_config_memofreq(config, &samples, &seed, err)) {
        BT_ABORT("memofreq section: %s", err);
    }
    BT_CHECK_EQ((unsigned long long)samples, 200000ULL);
    BT_CHECK_EQ((unsigned long long)seed, 20261012ULL);
}

BT_TEST(dist, pair_rows_reject_values_over_u32) {
    static const char *const over[] = {
        "{\"cases\":[],\"adjacent\":[{\"from\":4294967296,\"to\":1,\"count\":1}],"
        "\"cooccurring\":[],\"total_draws\":1,\"seed\":2}",
        "{\"cases\":[],\"adjacent\":[{\"from\":1,\"to\":4294967296,\"count\":1}],"
        "\"cooccurring\":[],\"total_draws\":1,\"seed\":2}",
        "{\"cases\":[],\"adjacent\":[],\"cooccurring\":[{\"from\":4294967296,"
        "\"to\":1,\"count\":1}],\"total_draws\":1,\"seed\":2}",
    };
    char dir[BT_PATH_MAX];
    bt_ga_scratch("dist-test", dir);
    char path[BT_PATH_MAX];
    for (size_t i = 0; i < sizeof over / sizeof over[0]; i++) {
        snprintf(path, BT_PATH_MAX, "%s/over-%zu.json", dir, i);
        FILE *file = fopen(path, "wb");
        fwrite(over[i], 1, strlen(over[i]), file);
        fclose(file);
        bt_ga_freq_data data;
        char err[128];
        BT_CHECK(!bt_ga_load_freq_data(path, &data, err));
    }
    const char *edge = "{\"cases\":[],\"adjacent\":[{\"from\":4294967295,"
                       "\"to\":4294967295,\"count\":1}],\"cooccurring\":[],"
                       "\"total_draws\":1,\"seed\":2}";
    snprintf(path, BT_PATH_MAX, "%s/edge.json", dir);
    FILE *file = fopen(path, "wb");
    fwrite(edge, 1, strlen(edge), file);
    fclose(file);
    bt_ga_freq_data data;
    char err[128];
    if (!bt_ga_load_freq_data(path, &data, err)) {
        BT_ABORT("edge load failed: %s", err);
    }
    BT_CHECK_EQ((unsigned long long)data.adjacent[0].from, 4294967295ULL);
    BT_CHECK_EQ((unsigned long long)data.adjacent[0].to, 4294967295ULL);
    bt_ga_freq_data_free(&data);
    bt_ga_scratch_remove("dist-test");
}

BT_TEST(dist, case_rows_reject_strings_past_the_field_caps) {
    static const char *const over[] = {
        "{\"cases\":[{\"orbit\":\"0123456789abcdef\",\"letters\":\"AB\",\"count\":1}],"
        "\"adjacent\":[],\"cooccurring\":[],\"total_draws\":1,\"seed\":2}",
        "{\"cases\":[{\"orbit\":\"edge\",\"letters\":\"ABCDEFGH\",\"count\":1}],"
        "\"adjacent\":[],\"cooccurring\":[],\"total_draws\":1,\"seed\":2}",
    };
    char dir[BT_PATH_MAX];
    bt_ga_scratch("dist-test", dir);
    char path[BT_PATH_MAX];
    for (size_t i = 0; i < sizeof over / sizeof over[0]; i++) {
        snprintf(path, BT_PATH_MAX, "%s/over-case-%zu.json", dir, i);
        FILE *file = fopen(path, "wb");
        fwrite(over[i], 1, strlen(over[i]), file);
        fclose(file);
        bt_ga_freq_data data;
        char err[128];
        BT_CHECK(!bt_ga_load_freq_data(path, &data, err));
    }
    const char *edge = "{\"cases\":[{\"orbit\":\"0123456789abcde\",\"letters\":\"ABCDEFG\","
                       "\"count\":1}],\"adjacent\":[],\"cooccurring\":[],"
                       "\"total_draws\":1,\"seed\":2}";
    snprintf(path, BT_PATH_MAX, "%s/edge-case.json", dir);
    FILE *file = fopen(path, "wb");
    fwrite(edge, 1, strlen(edge), file);
    fclose(file);
    bt_ga_freq_data data;
    char err[128];
    if (!bt_ga_load_freq_data(path, &data, err)) {
        BT_ABORT("edge load failed: %s", err);
    }
    BT_CHECK_EQ(data.cases[0].orbit, "0123456789abcde");
    BT_CHECK_EQ(data.cases[0].letters, "ABCDEFG");
    bt_ga_freq_data_free(&data);
    bt_ga_scratch_remove("dist-test");
}

static bt_ga_freq_data freq_fixture(void) {
    bt_ga_freq_data base = {0};
    base.cases_count = 1;
    base.adjacent_count = 1;
    base.cooccurring_count = 1;
    base.total_draws = 1000;
    base.seed = 20261012;
    return base;
}

static void fill_freq_fixture(bt_ga_freq_data *data, bt_ga_case_row *cases,
                              bt_ga_pair_count *adjacent, bt_ga_pair_count *cooccurring) {
    snprintf(cases[0].orbit, sizeof cases[0].orbit, "edge");
    snprintf(cases[0].letters, sizeof cases[0].letters, "AB");
    cases[0].count = 120;
    adjacent[0].from = 0;
    adjacent[0].to = 17;
    adjacent[0].count = 3;
    cooccurring[0].from = 5;
    cooccurring[0].to = 6;
    cooccurring[0].count = 9;
    data->cases = cases;
    data->adjacent = adjacent;
    data->cooccurring = cooccurring;
}

BT_TEST(dist, freq_data_eq_distinguishes_every_field) {
    bt_ga_case_row cases_a[1];
    bt_ga_pair_count adjacent_a[1];
    bt_ga_pair_count cooccurring_a[1];
    bt_ga_freq_data a = freq_fixture();
    fill_freq_fixture(&a, cases_a, adjacent_a, cooccurring_a);
    bt_ga_case_row cases_b[1];
    bt_ga_pair_count adjacent_b[1];
    bt_ga_pair_count cooccurring_b[1];
    bt_ga_freq_data b = freq_fixture();
    fill_freq_fixture(&b, cases_b, adjacent_b, cooccurring_b);
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &b), 1);
    BT_CHECK_EQ(bt_ga_freq_data_eq(&b, &a), 1);
    bt_ga_freq_data other = b;
    other.cases_count = 2;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &other), 0);
    other = b;
    other.total_draws = 1001;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &other), 0);
    other = b;
    other.seed = 20261013;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &other), 0);
    other = b;
    other.adjacent_count = 0;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &other), 0);
    other = b;
    other.cooccurring_count = 0;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &other), 0);
    bt_ga_freq_data shifted = b;
    bt_ga_pair_count adjacent_shifted[1] = {{0, 18, 3}};
    shifted.adjacent = adjacent_shifted;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &shifted), 0);
    bt_ga_pair_count adjacent_from[1] = {{1, 17, 3}};
    shifted.adjacent = adjacent_from;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &shifted), 0);
    bt_ga_pair_count adjacent_count[1] = {{0, 17, 4}};
    shifted.adjacent = adjacent_count;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &shifted), 0);
    bt_ga_pair_count cooccurring_shifted[1] = {{5, 7, 9}};
    shifted.adjacent = b.adjacent;
    shifted.cooccurring = cooccurring_shifted;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &shifted), 0);
    bt_ga_case_row cases_orbit[1];
    snprintf(cases_orbit[0].orbit, sizeof cases_orbit[0].orbit, "corner");
    snprintf(cases_orbit[0].letters, sizeof cases_orbit[0].letters, "AB");
    cases_orbit[0].count = 120;
    shifted.cooccurring = b.cooccurring;
    shifted.cases = cases_orbit;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &shifted), 0);
    bt_ga_case_row cases_letters[1];
    snprintf(cases_letters[0].orbit, sizeof cases_letters[0].orbit, "edge");
    snprintf(cases_letters[0].letters, sizeof cases_letters[0].letters, "AC");
    cases_letters[0].count = 120;
    shifted.cases = cases_letters;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &shifted), 0);
    bt_ga_case_row cases_count[1];
    snprintf(cases_count[0].orbit, sizeof cases_count[0].orbit, "edge");
    snprintf(cases_count[0].letters, sizeof cases_count[0].letters, "AB");
    cases_count[0].count = 121;
    shifted.cases = cases_count;
    BT_CHECK_EQ(bt_ga_freq_data_eq(&a, &shifted), 0);
}
