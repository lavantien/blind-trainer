#include "unit/tools/spawn_util.h"
#include "verify_test_util.h"

#include <stdlib.h>
#include <string.h>

#include "core/cube.h"
#include "verify/ev_bld.h"

#define DOOMED_MARKER "BT_VERIFY_EV_BLD_DOOMED_CHILD"

BT_TEST(verify_ev_bld_doomed, corrupted_piece_multiset_exits_the_audit) {
    if (getenv(DOOMED_MARKER) != NULL) {
        verify_bld_layout layout;
        verify_bld_layout_probe(&layout);
        size_t corners[VERIFY_BLD_MAX_BAND];
        size_t corner_count = verify_bld_corner_pieces(&layout, corners, VERIFY_BLD_MAX_BAND);
        BT_CHECK(corner_count > 0);
        bt_cube cube = bt_cube_solved();
        uint8_t center = bt_center_id(BT_FACE_D);
        cube.occ[verify_bld_slot_of_index(&layout, corners[0])[0]] = center;
        verify_bld_state_audit audit;
        verify_bld_audit(&cube, &layout, &audit);
        return;
    }
    char exe[BT_PATH_MAX];
    bt_test_bin_path(exe, "tools_verify_ev_bld_doomed_test.exe");
    const char *argv[] = {NULL};
    int code = 0;
    char out[512];
    char err[512];
    _putenv(DOOMED_MARKER "=1");
    int rc = bt_spawn_capture(exe, argv, NULL, 30000, &code, out, sizeof out, err, sizeof err);
    _putenv(DOOMED_MARKER "=");
    BT_CHECK_EQ(rc, 0);
    BT_CHECK_EQ(code, 1);
}
