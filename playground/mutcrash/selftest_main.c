#include <stdio.h>

#include "mutate/mutate.h"

void bt_test_mutate_engine_stages_the_suite_exe_inside_the_workspace(void);
void bt_test_mutate_engine_records_killed_for_a_guarded_test(void);
void bt_test_mutate_engine_records_survived_for_unread_code(void);

int main(void) {
    bt_test_mutate_engine_stages_the_suite_exe_inside_the_workspace();
    bt_test_mutate_engine_records_killed_for_a_guarded_test();
    bt_test_mutate_engine_records_survived_for_unread_code();
    printf("selftest-main done\n");
    return 0;
}
