#include "app/views/images.h"
#include "app/views/state.h"

#include "common.h"

#include <string.h>

BT_TEST(view_images, table_is_sorted_and_unique) {
    for (size_t i = 0; i + 1 < BT_VIEW_IMAGES_COUNT; i++) {
        BT_CHECK(strcmp(BT_VIEW_IMAGES[i].key, BT_VIEW_IMAGES[i + 1].key) < 0);
    }
}

BT_TEST(view_images, known_pairs_resolve) {
    BT_CHECK(bt_view_images_for("AB") != NULL);
    BT_CHECK(strcmp(bt_view_images_for("AB"), "Anthony Brooks") == 0);
    BT_CHECK(strcmp(bt_view_images_for("QM"), "Quail Man") == 0);
    BT_CHECK(bt_view_images_for("JX") == NULL);
    BT_CHECK(bt_view_images_for("XX") == NULL);
}

BT_TEST(view_images, table_holds_the_documented_620_entries) {
    BT_CHECK(sizeof BT_VIEW_IMAGES / sizeof BT_VIEW_IMAGES[0] == 620);
    BT_CHECK(sizeof BT_VIEW_IMAGES / sizeof BT_VIEW_IMAGES[0] == BT_VIEW_IMAGES_COUNT);
}

BT_TEST(view_images, every_key_is_two_uppercase_letters_with_a_word) {
    for (size_t i = 0; i < BT_VIEW_IMAGES_COUNT; i++) {
        const char *key = BT_VIEW_IMAGES[i].key;
        BT_CHECK(strlen(key) == 2);
        BT_CHECK(key[0] >= 'A' && key[0] <= 'Z' && key[1] >= 'A' && key[1] <= 'Z');
        BT_CHECK(BT_VIEW_IMAGES[i].word != NULL && BT_VIEW_IMAGES[i].word[0] != '\0');
    }
}

BT_TEST(view_images, image_for_resolves_every_entry_and_rejects_other_input) {
    for (size_t i = 0; i < BT_VIEW_IMAGES_COUNT; i++) {
        BT_CHECK(strcmp(bt_view_images_for(BT_VIEW_IMAGES[i].key), BT_VIEW_IMAGES[i].word) == 0);
    }
    BT_CHECK(bt_view_images_for("ab") == NULL);
    BT_CHECK(bt_view_images_for("A-") == NULL);
}

BT_TEST(view_images, install_registers_the_lookup_through_the_seam) {
    bt_view_images_install();
    const char *word = bt_views_image_for("AB");
    BT_CHECK(word != NULL);
    BT_CHECK(strcmp(word, "Anthony Brooks") == 0);
    BT_CHECK(bt_views_image_for("JX") == NULL);
}
