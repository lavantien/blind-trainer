#include "common/bt_harness.h"

extern void bt_test_views_case_drill_a_stores_save_never_wipes_the_case_cards(void);

int main(void) {
    bt_test_entry tests[] = {
        {"views_case_drill.a_stores_save_never_wipes_the_case_cards", bt_test_views_case_drill_a_stores_save_never_wipes_the_case_cards},
    };
    return bt_harness_run("app_one_case_drill", tests, 1);
}
