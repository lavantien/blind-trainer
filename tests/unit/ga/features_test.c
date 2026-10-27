#include "ga/features.h"
#include "ga/inventory.h"

#include "common/bt_harness.h"

#include "ga_test_util.h"

static bt_ga_features f(const char *text) {
    return bt_ga_parse_or_abort(text);
}

static int onset_is(const bt_ga_features *feat, bt_ga_place place, bt_ga_manner manner) {
    return feat->has_onset && feat->onset.place == place && feat->onset.manner == manner;
}

BT_TEST(features, parse_segments_representative_syllables) {
    bt_ga_features khong = f("không");
    BT_CHECK_EQ((long long)khong.tone, (long long)BT_GA_TONE_NGANG);
    BT_CHECK(onset_is(&khong, BT_GA_PLACE_VELAR, BT_GA_MANNER_FRICATIVE));
    BT_CHECK_EQ((long long)khong.nucleus, (long long)BT_GA_NUC_OH);
    BT_CHECK_EQ((long long)khong.coda, (long long)BT_GA_CODA_NG);

    bt_ga_features cua = f("của");
    BT_CHECK_EQ((long long)cua.tone, (long long)BT_GA_TONE_HOI);
    BT_CHECK(onset_is(&cua, BT_GA_PLACE_VELAR, BT_GA_MANNER_STOP));
    BT_CHECK_EQ((long long)cua.nucleus, (long long)BT_GA_NUC_UA);
    BT_CHECK_EQ((long long)cua.coda, (long long)BT_GA_CODA_OPEN);

    bt_ga_features tia = f("tía");
    BT_CHECK_EQ((long long)tia.tone, (long long)BT_GA_TONE_SAC);
    BT_CHECK_EQ((long long)tia.nucleus, (long long)BT_GA_NUC_IA);

    bt_ga_features anh = f("anh");
    BT_CHECK(!anh.has_onset);
    BT_CHECK_EQ((long long)anh.nucleus, (long long)BT_GA_NUC_E);
    BT_CHECK_EQ((long long)anh.coda, (long long)BT_GA_CODA_NG);

    bt_ga_features ach = f("ach");
    BT_CHECK_EQ((long long)ach.nucleus, (long long)BT_GA_NUC_E);
    BT_CHECK_EQ((long long)ach.coda, (long long)BT_GA_CODA_K);

    bt_ga_features ac = f("ac");
    BT_CHECK_EQ((long long)ac.nucleus, (long long)BT_GA_NUC_A);
    BT_CHECK_EQ((long long)ac.coda, (long long)BT_GA_CODA_K);

    bt_ga_features ieng = f("iêng");
    BT_CHECK(!ieng.has_onset);
    BT_CHECK_EQ((long long)ieng.nucleus, (long long)BT_GA_NUC_IA);
    BT_CHECK_EQ((long long)ieng.coda, (long long)BT_GA_CODA_NG);

    bt_ga_features gi = f("gì");
    BT_CHECK(onset_is(&gi, BT_GA_PLACE_DENTAL, BT_GA_MANNER_FRICATIVE));
    BT_CHECK_EQ((long long)gi.nucleus, (long long)BT_GA_NUC_I);
    bt_ga_features gia = f("gịa");
    BT_CHECK_EQ((long long)gia.nucleus, (long long)BT_GA_NUC_IA);
    bt_ga_features gia2 = f("già");
    BT_CHECK_EQ((long long)gia2.nucleus, (long long)BT_GA_NUC_A);
    bt_ga_features gieng = f("giếng");
    BT_CHECK_EQ((long long)gieng.nucleus, (long long)BT_GA_NUC_IA);
    BT_CHECK_EQ((long long)gieng.coda, (long long)BT_GA_CODA_NG);
    BT_CHECK_EQ((long long)gieng.tone, (long long)BT_GA_TONE_SAC);

    bt_ga_features dat = f("đặt");
    BT_CHECK(onset_is(&dat, BT_GA_PLACE_DENTAL, BT_GA_MANNER_IMPLOSIVE));
    BT_CHECK_EQ((long long)dat.nucleus, (long long)BT_GA_NUC_AB);
    BT_CHECK_EQ((long long)dat.coda, (long long)BT_GA_CODA_T);

    bt_ga_features trang = f("trăng");
    BT_CHECK(onset_is(&trang, BT_GA_PLACE_RETROFLEX, BT_GA_MANNER_AFFRICATE));
    BT_CHECK_EQ((long long)trang.nucleus, (long long)BT_GA_NUC_AB);
    BT_CHECK_EQ((long long)trang.coda, (long long)BT_GA_CODA_NG);

    bt_ga_features y = f("y");
    BT_CHECK(!y.has_onset);
    BT_CHECK_EQ((long long)y.nucleus, (long long)BT_GA_NUC_I);
    BT_CHECK_EQ((long long)y.coda, (long long)BT_GA_CODA_OPEN);
}

BT_TEST(features, parse_rejects_garbage_and_double_tones) {
    bt_ga_features out;
    BT_CHECK(!bt_ga_parse("xyz", &out));
    BT_CHECK(!bt_ga_parse("", &out));
    BT_CHECK(!bt_ga_parse("khôngg", &out));
    BT_CHECK(!bt_ga_parse("qua", &out));
}

BT_TEST(features, identical_and_homophonous_pairs_score_maximally) {
    bt_ga_features a = f("không");
    BT_CHECK_EQ(bt_ga_confusion(&a, &a), 1.0f);
    bt_ga_features i = f("i");
    bt_ga_features y = f("y");
    BT_CHECK_EQ(bt_ga_confusion(&i, &y), 1.0f);
}

BT_TEST(features, documented_tone_confusions_rank_above_undocumented) {
    bt_ga_features hoi = f("ả");
    bt_ga_features nga = f("ã");
    bt_ga_features huyen = f("à");
    bt_ga_features nang = f("ạ");
    bt_ga_features sac = f("á");
    float hoi_nga = bt_ga_confusion(&hoi, &nga);
    float huyen_nang = bt_ga_confusion(&huyen, &nang);
    float hoi_sac = bt_ga_confusion(&hoi, &sac);
    BT_CHECK(hoi_nga > 0.6f);
    BT_CHECK(huyen_nang > 0.5f);
    BT_CHECK(hoi_nga > hoi_sac);
    BT_CHECK(huyen_nang > hoi_sac);
    BT_CHECK(hoi_sac < 0.1f);
}

BT_TEST(features, stop_coda_place_confusions_follow_kirby_ranking) {
    bt_ga_features p = f("áp");
    bt_ga_features t = f("át");
    bt_ga_features k = f("ác");
    bt_ga_features m = f("ám");
    float pt = bt_ga_confusion(&p, &t);
    float tk = bt_ga_confusion(&t, &k);
    float pk = bt_ga_confusion(&p, &k);
    BT_CHECK(pt > tk && tk > pk);
    float pm = bt_ga_confusion(&p, &m);
    BT_CHECK(pm < tk);
    BT_CHECK(bt_ga_confusion(&p, &k) < bt_ga_confusion(&k, &k));
}

BT_TEST(features, short_long_nucleus_pairs_partial_confusion) {
    bt_ga_features e = f("en");
    bt_ga_features eh = f("ên");
    bt_ga_features o = f("on");
    bt_ga_features oh = f("ôn");
    BT_CHECK(bt_ga_confusion(&e, &eh) > 0.3f);
    BT_CHECK(bt_ga_confusion(&o, &oh) > 0.3f);
    bt_ga_features a = f("an");
    bt_ga_features u = f("un");
    BT_CHECK(bt_ga_confusion(&e, &eh) > bt_ga_confusion(&a, &u));
}

BT_TEST(features, confusion_is_symmetric) {
    const char *words[] = {"không", "của", "tía", "anh", "mút", "đặt", "y", "ác", "áp"};
    bt_ga_features feats[9];
    for (int i = 0; i < 9; i++) {
        feats[i] = f(words[i]);
    }
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            float forward = bt_ga_confusion(&feats[i], &feats[j]);
            float backward = bt_ga_confusion(&feats[j], &feats[i]);
            BT_CHECK(forward == backward);
        }
    }
}

BT_TEST(features, flow_penalty_counts_tone_clash_coda_clash_and_length) {
    bt_ga_features a = f("không");
    bt_ga_features b = f("lông");
    BT_CHECK_EQ(bt_ga_flow_penalty(&a, 4, &b, 4), 1.0f);
    bt_ga_features c = f("không");
    bt_ga_features d = f("không");
    BT_CHECK_EQ(bt_ga_flow_penalty(&c, 4, &d, 4), 2.0f);
    bt_ga_features e = f("ma");
    bt_ga_features g = f("má");
    BT_CHECK_EQ(bt_ga_flow_penalty(&e, 2, &g, 2), 0.0f);
    bt_ga_features h = f("cam");
    bt_ga_features i = f("ma");
    BT_CHECK(bt_ga_flow_penalty(&h, 3, &i, 2) >= 0.25f);
    bt_ga_features j = f("ác");
    bt_ga_features k = f("can");
    BT_CHECK(bt_ga_flow_penalty(&j, 2, &k, 3) >= 1.25f);
}

BT_TEST(features, parse_keeps_the_longest_onset_prefix) {
    bt_ga_features nghi = f("nghỉ");
    BT_CHECK(nghi.has_onset);
    BT_CHECK_EQ((long long)nghi.onset.place, (long long)BT_GA_PLACE_VELAR);
    BT_CHECK_EQ((long long)nghi.onset.manner, (long long)BT_GA_MANNER_NASAL);
    BT_CHECK_EQ((long long)nghi.nucleus, (long long)BT_GA_NUC_I);
    BT_CHECK_EQ((long long)nghi.coda, (long long)BT_GA_CODA_OPEN);
    BT_CHECK_EQ((long long)nghi.tone, (long long)BT_GA_TONE_HOI);
    bt_ga_features nga = f("ngà");
    BT_CHECK_EQ((long long)nga.onset.place, (long long)BT_GA_PLACE_VELAR);
    BT_CHECK_EQ((long long)nga.nucleus, (long long)BT_GA_NUC_A);
    bt_ga_features na = f("na");
    BT_CHECK_EQ((long long)na.onset.place, (long long)BT_GA_PLACE_DENTAL);
    bt_ga_features nha = f("nhà");
    BT_CHECK_EQ((long long)nha.onset.place, (long long)BT_GA_PLACE_PALATAL);
    BT_CHECK_EQ((long long)nha.nucleus, (long long)BT_GA_NUC_A);
}

BT_TEST(features, parse_rejects_stacked_tones_combining_marks_and_case) {
    bt_ga_features out;
    BT_CHECK(!bt_ga_parse("màá", &out));
    BT_CHECK(!bt_ga_parse("a\xcc\x81", &out));
    BT_CHECK(!bt_ga_parse("mả\xcc\x83", &out));
    BT_CHECK(!bt_ga_parse("KHÔNG", &out));
}

BT_TEST(features, merged_gi_expands_the_long_rimes) {
    bt_ga_features giem = f("giêm");
    BT_CHECK_EQ((long long)giem.onset.place, (long long)BT_GA_PLACE_DENTAL);
    BT_CHECK_EQ((long long)giem.nucleus, (long long)BT_GA_NUC_IA);
    BT_CHECK_EQ((long long)giem.coda, (long long)BT_GA_CODA_M);
    bt_ga_features giec = f("giêc");
    BT_CHECK_EQ((long long)giec.nucleus, (long long)BT_GA_NUC_IA);
    BT_CHECK_EQ((long long)giec.coda, (long long)BT_GA_CODA_K);
    BT_CHECK(!bt_ga_parse("giêx", &giec));
    BT_CHECK(!bt_ga_parse("giao", &giec));
}

BT_TEST(features, onset_feature_collapse_makes_cross_spelling_pairs_homophonous) {
    bt_ga_features di = f("dì");
    bt_ga_features xi = f("xì");
    BT_CHECK(di.has_onset && xi.has_onset);
    BT_CHECK_EQ((long long)di.onset.place, (long long)xi.onset.place);
    BT_CHECK_EQ((long long)di.onset.manner, (long long)xi.onset.manner);
    BT_CHECK_EQ(bt_ga_confusion(&di, &xi), 1.0f);
    bt_ga_features ga = f("gà");
    bt_ga_features kha = f("khà");
    BT_CHECK(ga.has_onset && kha.has_onset);
    BT_CHECK_EQ((long long)ga.onset.place, (long long)kha.onset.place);
    BT_CHECK_EQ((long long)ga.onset.manner, (long long)kha.onset.manner);
    BT_CHECK_EQ(bt_ga_confusion(&ga, &kha), 1.0f);
    bt_ga_features pha = f("pha");
    bt_ga_features va = f("va");
    BT_CHECK(pha.has_onset && va.has_onset);
    BT_CHECK_EQ((long long)pha.onset.place, (long long)va.onset.place);
    BT_CHECK_EQ((long long)pha.onset.manner, (long long)va.onset.manner);
    BT_CHECK_EQ(bt_ga_confusion(&pha, &va), 1.0f);
}

BT_TEST(features, flow_penalty_pins_exact_values_on_parsed_syllables) {
    bt_ga_features ma = f("ma");
    for (size_t len = 2; len <= 6; len++) {
        float expected = 1.0f + 0.25f * ((float)len - 2.0f);
        BT_CHECK_EQ(bt_ga_flow_penalty(&ma, 2, &ma, len), expected);
    }
    bt_ga_features ap = f("áp");
    bt_ga_features ban = f("ban");
    BT_CHECK_EQ(bt_ga_flow_penalty(&ap, 2, &ban, 3), 1.25f);
    bt_ga_features am = f("ám");
    BT_CHECK_EQ(bt_ga_flow_penalty(&am, 2, &ma, 2), 1.0f);
    bt_ga_features bap = f("báp");
    BT_CHECK_EQ(bt_ga_flow_penalty(&bap, 3, &bap, 2), 2.25f);
}
