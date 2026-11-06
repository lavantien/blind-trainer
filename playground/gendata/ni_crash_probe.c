#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/paths.h"
#include "data/corpus.h"
#include "data/paths.h"
#include "gendata/build.h"
#include "gendata/lib.h"

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    bt_json_value *config = bt_data_load_config();
    if (config == NULL) {
        puts("config load failed");
        return 1;
    }
    char root[BT_PATH_MAX];
    bt_paths_error perr;
    if (bt_data_vendor_root_from(getenv("BT_REF_GROUND"), config, root, &perr) != 0) {
        puts("root resolve failed");
        return 1;
    }
    bt_json_free(config);
    printf("root %s\n", root);
    bt_corpus corpus;
    bt_corpus_error cerr;
    if (bt_corpus_load(root, &corpus, &cerr) != 0) {
        puts("corpus load failed");
        return 1;
    }
    printf("corpus rows %zu\n", corpus.count);
    bt_gd_built built;
    bt_gd_build(&corpus, &built);
    printf("built cases %zu specials %zu\n", built.cases.count, built.specials.count);
    size_t derivable = 0;
    for (size_t i = 0; i < built.cases.count; i++) {
        bt_gd_case_entry *entry = &built.cases.cases[i];
        if (entry->vendor_count != 0) {
            continue;
        }
        for (size_t m = 0; m < built.cases.count; m++) {
            if (strcmp(built.cases.cases[m].orbit, entry->orbit) == 0 &&
                built.cases.cases[m].t1 == entry->t2 && built.cases.cases[m].t2 == entry->t1 &&
                built.cases.cases[m].vendor_count != 0) {
                derivable++;
                break;
            }
        }
    }
    printf("derivable %zu\n", derivable);
    puts("deriving");
    bt_gd_derive_mirrored_inverses(&built.cases);
    puts("derived");
    size_t covered = 0;
    for (size_t i = 0; i < built.cases.count; i++) {
        if (built.cases.cases[i].covered) {
            covered++;
        }
    }
    printf("covered after derive %zu of %zu\n", covered, built.cases.count);
    bt_gd_built_free(&built);
    bt_corpus_free(&corpus);
    puts("freed clean");
    return 0;
}
