#include "common/bt_harness.h"
#include "common/rng.h"

BT_TEST(rng, splitmix64_reference_vectors) {
    bt_rng rng;
    bt_rng_seed(&rng, 0);
    BT_CHECK_EQ(bt_rng_next(&rng), 16294208416658607535ULL);
    BT_CHECK_EQ(bt_rng_next(&rng), 7960286522194355700ULL);
    BT_CHECK_EQ(bt_rng_next(&rng), 487617019471545679ULL);
    BT_CHECK_EQ(bt_rng_next(&rng), 17909611376780542444ULL);
    bt_rng other;
    bt_rng_seed(&other, 0x0123456789ABCDEFULL);
    BT_CHECK_EQ(bt_rng_next(&other), 1547611027431991965ULL);
    BT_CHECK_EQ(bt_rng_next(&other), 15380727978956804243ULL);
}

BT_TEST(rng, below_stays_in_range_and_spreads) {
    bt_rng rng;
    bt_rng_seed(&rng, 42);
    unsigned counts[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < 5000; i++) {
        uint64_t value = bt_rng_below(&rng, 5);
        BT_CHECK(value < 5);
        counts[value]++;
    }
    for (int i = 0; i < 5; i++) {
        BT_CHECK(counts[i] > 800);
        BT_CHECK(counts[i] < 1200);
    }
    bt_rng one;
    bt_rng_seed(&one, 7);
    BT_CHECK_EQ(bt_rng_below(&one, 1), 0);
}

BT_TEST(rng, seeding_reproduces_the_same_stream) {
    bt_rng first;
    bt_rng second;
    bt_rng_seed(&first, 20261027);
    bt_rng_seed(&second, 20261027);
    BT_CHECK_EQ(first.state, second.state);
    for (int i = 0; i < 64; i++) {
        BT_CHECK_EQ(bt_rng_next(&first), bt_rng_next(&second));
    }
    bt_rng third;
    bt_rng_seed(&third, 20261027);
    BT_CHECK(third.state != first.state);
}
