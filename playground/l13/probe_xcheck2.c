#include <stdio.h>

void bt_test_ga_run_lib_cross_check_compares_the_full_letter_strings(void);
void bt_test_ga_run_lib_cross_check_names_each_missing_letter_field(void);

int main(void) {
    fprintf(stderr, "calling compares\n");
    bt_test_ga_run_lib_cross_check_compares_the_full_letter_strings();
    fprintf(stderr, "compares done, calling names\n");
    bt_test_ga_run_lib_cross_check_names_each_missing_letter_field();
    fprintf(stderr, "names done\n");
    return 0;
}
