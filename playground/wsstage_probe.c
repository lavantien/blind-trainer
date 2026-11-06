#include "mutate/mutate.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "common/bt_trace.h"
#include "common/config.h"
#include "common/json.h"

void bt_test_mutate_apply_compiles_for_every_operator(void);
void bt_test_mutate_comparison_swap_variants(void);
void bt_test_mutate_config_path_honors_bt_config_env(void);
void bt_test_mutate_config_path_joins_relative_env_and_keeps_the_default(void);
void bt_test_mutate_engine_records_builderr_when_the_mutant_fails_to_compile(void);
void bt_test_mutate_engine_records_killed_for_a_guarded_test(void);
void bt_test_mutate_engine_records_survived_for_unread_code(void);
void bt_test_mutate_engine_records_timeout_for_a_frozen_loop(void);
void bt_test_mutate_engine_rejects_parallel_workers(void);
void bt_test_mutate_engine_run_stops_after_the_limit(void);
void bt_test_mutate_engine_run_refuses_a_malformed_ledger(void);
void bt_test_mutate_engine_run_refuses_an_unreadable_target(void);
void bt_test_mutate_engine_run_refuses_a_poisoned_ledger_path(void);
void bt_test_mutate_target_from_repo_builds_a_tools_path_target_over_a_fake_root(void);
void bt_test_mutate_target_from_repo_rejects_ambiguous_and_missing_obj_roots(void);
void bt_test_mutate_target_from_repo_names_the_missing_suite_and_unknown_area(void);
void bt_test_mutate_area_map_rejects_malformed_src_deps(void);
void bt_test_mutate_prepare_scratch_refuses_unwritable_layouts(void);
void bt_test_mutate_prepare_scratch_copies_and_skips_dep_tokens_by_rule(void);
void bt_test_mutate_tokenizer_skips_comments_and_reads_prefixed_literals(void);
void bt_test_mutate_tokenizer_and_span_helpers_edge_inputs(void);
void bt_test_mutate_engine_stages_the_suite_exe_inside_the_workspace(void);

static void run_prefix(int count) {
    void (*tests[])(void) = {
        bt_test_mutate_apply_compiles_for_every_operator,
        bt_test_mutate_comparison_swap_variants,
        bt_test_mutate_config_path_honors_bt_config_env,
        bt_test_mutate_config_path_joins_relative_env_and_keeps_the_default,
        bt_test_mutate_engine_records_builderr_when_the_mutant_fails_to_compile,
        bt_test_mutate_engine_records_killed_for_a_guarded_test,
        bt_test_mutate_engine_records_survived_for_unread_code,
        bt_test_mutate_engine_records_timeout_for_a_frozen_loop,
        bt_test_mutate_engine_rejects_parallel_workers,
        bt_test_mutate_engine_run_stops_after_the_limit,
        bt_test_mutate_engine_run_refuses_a_malformed_ledger,
        bt_test_mutate_engine_run_refuses_an_unreadable_target,
        bt_test_mutate_engine_run_refuses_a_poisoned_ledger_path,
        bt_test_mutate_target_from_repo_builds_a_tools_path_target_over_a_fake_root,
        bt_test_mutate_target_from_repo_rejects_ambiguous_and_missing_obj_roots,
        bt_test_mutate_target_from_repo_names_the_missing_suite_and_unknown_area,
        bt_test_mutate_area_map_rejects_malformed_src_deps,
        bt_test_mutate_prepare_scratch_refuses_unwritable_layouts,
        bt_test_mutate_prepare_scratch_copies_and_skips_dep_tokens_by_rule,
        bt_test_mutate_tokenizer_skips_comments_and_reads_prefixed_literals,
        bt_test_mutate_tokenizer_and_span_helpers_edge_inputs,
    };
    int total = (int)(sizeof tests / sizeof tests[0]);
    if (count > total) {
        count = total;
    }
    for (int i = 0; i < count; i++) {
        printf("prefix %d\n", i + 1);
        fflush(stdout);
        tests[i]();
    }
}

static void find_single_dir(const char *base, char out[BT_MUT_PATH_MAX]) {
    char pattern[BT_MUT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", base);
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) {
        snprintf(out, BT_MUT_PATH_MAX, "%s", base);
        return;
    }
    char found[BT_MUT_PATH_MAX] = "";
    size_t count = 0;
    do {
        if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }
        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
            continue;
        }
        count++;
        snprintf(found, sizeof found, "%s", entry.cFileName);
    } while (FindNextFileA(find, &entry));
    FindClose(find);
    if (count != 1) {
        snprintf(out, BT_MUT_PATH_MAX, "%s", base);
        return;
    }
    snprintf(out, BT_MUT_PATH_MAX, "%s/%s", base, found);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: wsstage_probe <repo_root> <prefix_count>\n");
        return 2;
    }
    const char *repo = argv[1];
    int prefix = atoi(argv[2]);
    run_prefix(prefix);
    if (prefix >= 0) {
        printf("wsstage start\n");
        fflush(stdout);
        bt_test_mutate_engine_stages_the_suite_exe_inside_the_workspace();
        printf("wsstage done\n");
        bt_trace_dump_to(stdout);
        return 0;
    }
    bt_json_value *config = NULL;
    char config_path[BT_MUT_PATH_MAX];
    char root[BT_MUT_PATH_MAX];
    snprintf(root, sizeof root, "%s", repo);
    bt_mut_config_path(NULL, root, config_path);
    if (bt_config_try_load(config_path, &config) != 0) {
        fprintf(stderr, "cannot load config %s\n", config_path);
        return 2;
    }
    bt_mut_engine engine;
    memset(&engine, 0, sizeof engine);
    snprintf(engine.repo_root, BT_MUT_PATH_MAX, "%s", repo);
    if (bt_mut_build_load(repo, config, NULL, &engine.build) != 0) {
        fprintf(stderr, "cannot load build\n");
        return 2;
    }
    if (bt_mut_area_map_load(config, &engine.area_map) != 0) {
        fprintf(stderr, "cannot load area map\n");
        return 2;
    }
    char temp[BT_MUT_PATH_MAX];
    GetTempPathA(BT_MUT_PATH_MAX, temp);
    size_t len = strlen(temp);
    while (len > 0 && (temp[len - 1] == '\\' || temp[len - 1] == '/')) {
        len--;
    }
    temp[len] = '\0';
    snprintf(engine.scratch_root, BT_MUT_PATH_MAX, "%s/bt-mut-wsstage-probe", temp);
    bt_mut_rmtree(engine.scratch_root);
    char work_dir[BT_MUT_PATH_MAX];
    snprintf(work_dir, BT_MUT_PATH_MAX, "%s/build/test-tmp/wsstage-probe", repo);
    bt_mut_mkdirs(work_dir);
    snprintf(engine.ledger_path, BT_MUT_PATH_MAX, "%s/ledger.tsv", work_dir);
    remove(engine.ledger_path);
    engine.timeout_ms = 60000;
    engine.workers = 1;
    snprintf(engine.test_exe, BT_MUT_PATH_MAX, "fx_test");
    char fx_ws_main[BT_MUT_PATH_MAX];
    snprintf(fx_ws_main, BT_MUT_PATH_MAX, "%s/tests/unit/tools/fixtures/fx_ws_main.c", repo);
    char fx_ws_main_obj[BT_MUT_PATH_MAX];
    snprintf(fx_ws_main_obj, BT_MUT_PATH_MAX, "%s/fx_ws_main.obj", work_dir);
    char diag[2048];
    if (bt_mut_compile_file(&engine.build, NULL, NULL, fx_ws_main, fx_ws_main_obj, diag,
                            sizeof diag) != 0) {
        fprintf(stderr, "cannot compile fx_ws_main: %s\n", diag);
        return 2;
    }
    char obj_base[BT_MUT_PATH_MAX];
    char obj_root[BT_MUT_PATH_MAX];
    snprintf(obj_base, sizeof obj_base, "%s/build/obj", repo);
    find_single_dir(obj_base, obj_root);
    char paths_obj[BT_MUT_PATH_MAX];
    char trace_obj[BT_MUT_PATH_MAX];
    char alloc_obj[BT_MUT_PATH_MAX];
    snprintf(paths_obj, sizeof paths_obj, "%s/src/common/paths.obj", obj_root);
    snprintf(trace_obj, sizeof trace_obj, "%s/src/common/bt_trace.obj", obj_root);
    snprintf(alloc_obj, sizeof alloc_obj, "%s/src/common/alloc.obj", obj_root);
    char slot_obj[BT_MUT_PATH_MAX];
    snprintf(slot_obj, sizeof slot_obj, "%s/fx_ws_target.obj", work_dir);
    char *objs[5] = {slot_obj, fx_ws_main_obj, paths_obj, trace_obj, alloc_obj};
    bt_mut_target target;
    memset(&target, 0, sizeof target);
    snprintf(target.file, BT_MUT_PATH_MAX, "tests/unit/tools/fixtures/fx_ws_target.c");
    target.link_objs = objs;
    target.link_obj_count = 5;
    target.target_index = 0;
    bt_mut_summary summary;
    printf("wsstage start\n");
    fflush(stdout);
    int rc = bt_mut_engine_run(&engine, &target, 1, "zzz-none", 0, &summary);
    printf("engine_run rc=%d error=%s\n", rc, rc == 0 ? "(none)" : bt_mut_engine_last_error());
    printf("summary total=%llu killed=%llu survived=%llu timeout=%llu builderr=%llu pending=%llu\n",
           summary.total, summary.killed, summary.survived, summary.timeout, summary.builderr,
           summary.pending);
    bt_trace_dump_to(stdout);
    bt_mut_build_free(&engine.build);
    bt_json_free(config);
    return rc == 0 ? 0 : 1;
}
