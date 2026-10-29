#include <stdio.h>

#include "common/paths.h"
#include "unit/tools/fixtures/fx_ws_target.h"

int main(void) {
    if (fx_ws_probe(1) != 1) {
        return 1;
    }
    char root[BT_PATH_MAX];
    bt_paths_workspace_root(root);
    char config_path[BT_PATH_MAX + 32];
    snprintf(config_path, sizeof config_path, "%s/config/config.json", root);
    FILE *file = fopen(config_path, "rb");
    if (file == NULL) {
        return 2;
    }
    fclose(file);
    return 0;
}
