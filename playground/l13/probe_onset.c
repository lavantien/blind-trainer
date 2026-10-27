#include "ga/inventory.h"
#include "common/config.h"
#include "common/paths.h"
#include "data/paths.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    char root[BT_PATH_MAX];
    bt_data_workspace_root(root);
    const bt_json_value *config = bt_config_load();
    const char *ref_doc = NULL;
    bt_config_str(config, "paths.ref_doc", &ref_doc);
    char dir[BT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s/%s/vietnamese", root, ref_doc);
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/syllables-7184-abc.txt", dir);
    bt_ga_strset wordlist = {0};
    char err[BT_GA_ERR_MAX];
    if (!bt_ga_load_wordlist(path, &wordlist, err)) { fprintf(stderr, "wl: %s\n", err); return 1; }
    bt_ga_inventory_entry *entries = NULL;
    size_t count = 0;
    bt_ga_inventory_report report = {0};
    if (!bt_ga_build_inventory(&wordlist, &entries, &count, &report, err)) { fprintf(stderr, "inv: %s\n", err); return 1; }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].text, "anh") == 0) {
            fprintf(stderr, "anh onset=%d nucleus=%d coda=%d tone=%d\n", (int)entries[i].onset,
                    (int)entries[i].nucleus, (int)entries[i].coda, (int)entries[i].tone);
        }
    }
    fprintf(stderr, "entries=%zu\n", count);
    return 0;
}
