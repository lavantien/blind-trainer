#include "common/bt_harness.h"
#include "data/verify.h"

#include <stdint.h>

BT_TEST(verify, chirality_pins_the_face_normal_triple_product) {
    BT_CHECK(bt_verify_chirality((const uint8_t[3]){12, 8, 0}) == -1);
    BT_CHECK(bt_verify_chirality((const uint8_t[3]){0, 12, 8}) == -1);
    BT_CHECK(bt_verify_chirality((const uint8_t[3]){8, 0, 12}) == -1);
}

BT_TEST(verify, chirality_pins_canonical_corner_handedness) {
    BT_CHECK(bt_verify_chirality((const uint8_t[3]){2, 9, 12}) == 1);
    BT_CHECK(bt_verify_chirality((const uint8_t[3]){1, 16, 13}) == -1);
    BT_CHECK(bt_verify_chirality((const uint8_t[3]){22, 19, 14}) == 1);
    BT_CHECK(bt_verify_chirality((const uint8_t[3]){20, 11, 6}) == 1);
}
