#include "common/bt_harness.h"

#include <string.h>

#include "data/verify.h"

static void check_problem(bt_problem problem, const char *want) {
    char out[256];
    bt_problem_string(&problem, out, sizeof out);
    BT_CHECK_EQ(out, want);
}

BT_TEST(verify_problem_string, every_problem_kind_renders_the_rust_display_text) {
    bt_problem problem = {0};
    problem.kind = BT_PROBLEM_BAD_POSITION;
    snprintf(problem.detail, sizeof problem.detail, "%s", "unknown sticker name UB");
    check_problem(problem, "bad position: unknown sticker name UB");

    problem = (bt_problem){0};
    problem.kind = BT_PROBLEM_CENTERS_MOVED;
    check_problem(problem, "centers moved");

    problem = (bt_problem){0};
    problem.kind = BT_PROBLEM_WRONG_CYCLE;
    snprintf(problem.sticker, sizeof problem.sticker, "%s", "UF");
    snprintf(problem.expected, sizeof problem.expected, "%s", "UB");
    snprintf(problem.actual, sizeof problem.actual, "%s", "UL");
    check_problem(problem, "sticker UF should sit on UB but sits on UL");

    problem = (bt_problem){0};
    problem.kind = BT_PROBLEM_DISPLACED;
    snprintf(problem.piece, sizeof problem.piece, "%s", "UF");
    snprintf(problem.expected_slot, sizeof problem.expected_slot, "%s", "UB");
    snprintf(problem.actual_slot, sizeof problem.actual_slot, "%s", "UL");
    check_problem(problem, "piece UF should sit in slot UB but sits in UL");

    problem = (bt_problem){0};
    problem.kind = BT_PROBLEM_UNEXPECTED_DEFECT;
    problem.defect = BT_DEFECT_FLIP;
    snprintf(problem.piece, sizeof problem.piece, "%s", "UF");
    check_problem(problem, "piece UF is flipped but should be intact");

    problem = (bt_problem){0};
    problem.kind = BT_PROBLEM_UNEXPECTED_DEFECT;
    problem.defect = BT_DEFECT_TWIST;
    snprintf(problem.piece, sizeof problem.piece, "%s", "UFR");
    check_problem(problem, "piece UFR is twisted but should be intact");

    problem = (bt_problem){0};
    problem.kind = BT_PROBLEM_MISSING_DEFECT;
    problem.defect = BT_DEFECT_FLIP;
    snprintf(problem.piece, sizeof problem.piece, "%s", "UF");
    check_problem(problem, "piece UF should be flipped but is intact");

    problem = (bt_problem){0};
    problem.kind = BT_PROBLEM_NON_FIXED_RESIDUE;
    snprintf(problem.sticker, sizeof problem.sticker, "%s", "UF");
    snprintf(problem.actual, sizeof problem.actual, "%s", "DB");
    check_problem(problem, "sticker UF moved to DB outside the named pieces");

    problem = (bt_problem){0};
    problem.kind = BT_PROBLEM_TWISTS_NOT_OPPOSITE;
    snprintf(problem.first, sizeof problem.first, "%s", "UFR");
    snprintf(problem.second, sizeof problem.second, "%s", "UBL");
    check_problem(problem, "twists of UFR and UBL are not opposite");

    problem = (bt_problem){0};
    problem.kind = BT_PROBLEM_TWIST_SUM_NOT_ZERO;
    problem.sum = 1;
    check_problem(problem, "corner twist sum is 1, not 0 mod 3");
}
