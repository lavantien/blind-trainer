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
