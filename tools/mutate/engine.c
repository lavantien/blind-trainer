#include "mutate/mutate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/bt_trace.h"
#include "common/config.h"
#include "common/strbuf.h"

#ifdef _WIN32
#include <windows.h>
#endif

static bt_strbuf g_engine_error;
static int g_engine_error_ready;

const char *bt_mut_engine_last_error(void) {
    if (!g_engine_error_ready) {
        if (bt_strbuf_init(&g_engine_error, 64) != 0) {
            return "";
        }
        g_engine_error_ready = 1;
    }
    return g_engine_error.data;
}

static void engine_error(const char *fmt, ...) {
    if (!g_engine_error_ready) {
        if (bt_strbuf_init(&g_engine_error, 64) != 0) {
            return;
        }
        g_engine_error_ready = 1;
    }
    g_engine_error.len = 0;
    g_engine_error.data[0] = '\0';
    va_list ap;
    va_start(ap, fmt);
    char formatted[1024];
    vsnprintf(formatted, sizeof formatted, fmt, ap);
    va_end(ap);
    bt_strbuf_append_str(&g_engine_error, formatted);
}

static int cmp_strings(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

static int scan_dir_recursive(const char *base, const char *relative, bt_mut_paths *out,
                              const char *extension) {
#ifdef _WIN32
    char pattern[BT_MUT_PATH_MAX];
    if (relative[0] == '\0') {
        snprintf(pattern, sizeof pattern, "%s/*", base);
    } else {
        snprintf(pattern, sizeof pattern, "%s/%s/*", base, relative);
    }
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) {
        return 0;
    }
    int rc = 0;
    do {
        if (strcmp(entry.cFileName, ".") == 0 || strcmp(entry.cFileName, "..") == 0) {
            continue;
        }
        char child[BT_MUT_PATH_MAX];
        if (relative[0] == '\0') {
            snprintf(child, sizeof child, "%s", entry.cFileName);
        } else {
            snprintf(child, sizeof child, "%s/%s", relative, entry.cFileName);
        }
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            rc = scan_dir_recursive(base, child, out, extension);
            continue;
        }
        size_t len = strlen(child);
        size_t ext_len = strlen(extension);
        if (len >= ext_len && strcmp(child + len - ext_len, extension) == 0) {
            rc = bt_mut_paths_push(out, child);
        }
    } while (rc == 0 && FindNextFileA(find, &entry));
    FindClose(find);
    return rc;
#else
    return -1;
#endif
}

static int obj_root_of(const char *repo_root, char out[BT_MUT_PATH_MAX]) {
    char base[BT_MUT_PATH_MAX];
    snprintf(base, sizeof base, "%s/build/obj", repo_root);
#ifdef _WIN32
    char pattern[BT_MUT_PATH_MAX];
    snprintf(pattern, sizeof pattern, "%s/*", base);
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA(pattern, &entry);
    if (find == INVALID_HANDLE_VALUE) {
        engine_error("no build objects under %s, run python bt.py build first", base);
        return -1;
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
        snprintf(found, BT_MUT_PATH_MAX, "%s", entry.cFileName);
    } while (FindNextFileA(find, &entry));
    FindClose(find);
    if (count != 1) {
        engine_error("expected 1 object root under %s, found %zu", base, count);
        return -1;
    }
    snprintf(out, BT_MUT_PATH_MAX, "%s/%s", base, found);
    return 0;
#else
    engine_error("unsupported platform");
    return -1;
#endif
}

int bt_mut_area_set_contains(const bt_mut_area_set *set, const char *area) {
    for (size_t i = 0; i < set->count; i++) {
        if (strcmp(set->items[i], area) == 0) {
            return 1;
        }
    }
    return 0;
}

static int area_set_push(bt_mut_area_set *set, const char *area) {
    if (bt_mut_area_set_contains(set, area)) {
        return 0;
    }
    if (set->count >= BT_MUT_AREAS_MAX || strlen(area) >= BT_MUT_AREA_MAX) {
        engine_error("area set overflow at %s", area);
        return -1;
    }
    snprintf(set->items[set->count], BT_MUT_AREA_MAX, "%s", area);
    set->count++;
    return 0;
}

int bt_mut_area_closure(const bt_mut_area_map *map, const char *area, bt_mut_area_set *out) {
    BT_TRACE_ENTER("mutate.area_closure");
    memset(out, 0, sizeof *out);
    int rc = area_set_push(out, area);
    for (size_t i = 0; rc == 0 && i < out->count; i++) {
        for (size_t n = 0; rc == 0 && n < map->count; n++) {
            if (strcmp(map->areas[n], out->items[i]) != 0) {
                continue;
            }
            for (size_t d = 0; rc == 0 && d < map->dep_counts[n]; d++) {
                rc = area_set_push(out, map->deps[n][d]);
            }
            break;
        }
    }
    if (rc != 0) {
        BT_TRACE_FAIL("mutate.area_closure", "%s", bt_mut_engine_last_error());
    }
    BT_TRACE_EXIT("mutate.area_closure");
    return rc;
}

int bt_mut_area_map_load(const bt_json_value *config, bt_mut_area_map *out) {
    BT_TRACE_ENTER("mutate.area_map_load");
    memset(out, 0, sizeof *out);
    const bt_json_value *deps = bt_config_value(config, "build.src_deps");
    int rc = 0;
    if (deps == NULL) {
        BT_TRACE_EXIT("mutate.area_map_load");
        return 0;
    }
    if (deps->type != BT_JSON_OBJECT) {
        engine_error("build.src_deps is not an object");
        rc = -1;
    }
    for (size_t i = 0; rc == 0 && i < deps->as.object.count; i++) {
        const char *name = deps->as.object.keys[i];
        const bt_json_value *list = deps->as.object.values[i];
        size_t slot = out->count;
        if (name == NULL || strlen(name) >= BT_MUT_AREA_MAX || slot >= BT_MUT_AREAS_MAX) {
            engine_error("area map overflow at %s", name == NULL ? "?" : name);
            rc = -1;
            break;
        }
        if (list == NULL || list->type != BT_JSON_ARRAY) {
            engine_error("build.src_deps[%s] is not an array", name);
            rc = -1;
            break;
        }
        snprintf(out->areas[slot], BT_MUT_AREA_MAX, "%s", name);
        for (size_t d = 0; rc == 0 && d < list->as.array.count; d++) {
            const bt_json_value *dep = list->as.array.items[d];
            if (dep == NULL || dep->type != BT_JSON_STRING ||
                strlen(dep->as.string) >= BT_MUT_AREA_MAX ||
                out->dep_counts[slot] >= BT_MUT_AREAS_MAX) {
                engine_error("build.src_deps[%s] holds an invalid dependency", name);
                rc = -1;
                break;
            }
            snprintf(out->deps[slot][out->dep_counts[slot]], BT_MUT_AREA_MAX, "%s", dep->as.string);
            out->dep_counts[slot]++;
        }
        if (rc == 0) {
            out->count++;
        }
    }
    if (rc != 0) {
        memset(out, 0, sizeof *out);
        BT_TRACE_FAIL("mutate.area_map_load", "%s", bt_mut_engine_last_error());
    }
    BT_TRACE_EXIT("mutate.area_map_load");
    return rc;
}

static int src_in_link_areas(const char *src_rel, const bt_mut_area_set *areas) {
    const char *slash = strchr(src_rel, '/');
    if (slash == NULL) {
        return 0;
    }
    size_t len = (size_t)(slash - src_rel);
    char area[BT_MUT_AREA_MAX];
    if (len >= BT_MUT_AREA_MAX) {
        return 0;
    }
    memcpy(area, src_rel, len);
    area[len] = '\0';
    if (strcmp(area, "common") == 0) {
        return 1;
    }
    return bt_mut_area_set_contains(areas, area);
}

static int test_source_for_exe(const char *repo_root, const char *test_exe,
                               char out[BT_MUT_PATH_MAX]) {
    bt_mut_paths sources = {0};
    char base[BT_MUT_PATH_MAX];
    snprintf(base, sizeof base, "%s/tests/unit", repo_root);
    int rc = scan_dir_recursive(base, "", &sources, "_test.c");
    if (rc == 0) {
        qsort(sources.items, sources.count, sizeof(char *), cmp_strings);
    }
    for (size_t i = 0; rc == 0 && i < sources.count; i++) {
        char candidate[BT_MUT_PATH_MAX];
        snprintf(candidate, sizeof candidate, "%s", sources.items[i]);
        size_t len = strlen(candidate);
        if (len > 2 && strcmp(candidate + len - 2, ".c") == 0) {
            candidate[len - 2] = '\0';
        }
        for (char *cursor = candidate; *cursor != '\0'; cursor++) {
            if (*cursor == '/') {
                *cursor = '_';
            }
        }
        if (strcmp(candidate, test_exe) == 0) {
            snprintf(out, BT_MUT_PATH_MAX, "tests/unit/%s", sources.items[i]);
            bt_mut_paths_free(&sources);
            return 0;
        }
    }
    bt_mut_paths_free(&sources);
    engine_error("no test suite named %s under tests/unit", test_exe);
    return -1;
}

int bt_mut_target_from_repo(const bt_mut_engine *engine, const char *file_rel, const char *test_exe,
                            bt_mut_target *target) {
    BT_TRACE_ENTER("mutate.target_from_repo");
    memset(target, 0, sizeof *target);
    snprintf(target->file, BT_MUT_PATH_MAX, "%s", file_rel);
    char obj_root[BT_MUT_PATH_MAX];
    if (obj_root_of(engine->repo_root, obj_root) != 0) {
        BT_TRACE_FAIL("mutate.target_from_repo", "%s", bt_mut_engine_last_error());
        BT_TRACE_EXIT("mutate.target_from_repo");
        return -1;
    }
    bt_mut_paths src_files = {0};
    char src_base[BT_MUT_PATH_MAX];
    snprintf(src_base, sizeof src_base, "%s/src", engine->repo_root);
    int rc = scan_dir_recursive(src_base, "", &src_files, ".c");
    if (rc == 0) {
        qsort(src_files.items, src_files.count, sizeof(char *), cmp_strings);
    }
    char test_source[BT_MUT_PATH_MAX];
    if (rc == 0 && test_source_for_exe(engine->repo_root, test_exe, test_source) != 0) {
        rc = -1;
    }
    bt_mut_area_set link_areas = {0};
    char suite_area[BT_MUT_AREA_MAX] = "";
    if (rc == 0) {
        const char *area_start = test_source + strlen("tests/unit/");
        const char *slash = strchr(area_start, '/');
        size_t area_len = slash == NULL ? strlen(area_start) : (size_t)(slash - area_start);
        if (area_len == 0 || area_len >= BT_MUT_AREA_MAX) {
            engine_error("cannot derive a suite area from %s", test_source);
            rc = -1;
        } else {
            memcpy(suite_area, area_start, area_len);
            suite_area[area_len] = '\0';
            rc = bt_mut_area_closure(&engine->area_map, suite_area, &link_areas);
        }
    }
    bt_mut_paths objs = {0};
    int target_seen = 0;
    const char *rel_under_src = strncmp(file_rel, "src/", 4) == 0 ? file_rel + 4 : file_rel;
    for (size_t i = 0; rc == 0 && i < src_files.count; i++) {
        char obj[BT_MUT_PATH_MAX];
        snprintf(obj, sizeof obj, "%s/src/%s", obj_root, src_files.items[i]);
        size_t obj_len = strlen(obj);
        if (obj_len > 2 && strcmp(obj + obj_len - 2, ".c") == 0) {
            snprintf(obj + obj_len - 2, sizeof obj - (obj_len - 2), ".obj");
        }
        int is_target = strcmp(src_files.items[i], rel_under_src) == 0;
        if (!is_target && !src_in_link_areas(src_files.items[i], &link_areas)) {
            continue;
        }
        if (!is_target) {
            FILE *probe = fopen(obj, "rb");
            if (probe == NULL) {
                continue;
            }
            fclose(probe);
        }
        if (is_target) {
            target->target_index = objs.count;
            target_seen = 1;
        }
        rc = bt_mut_paths_push(&objs, obj);
    }
    if (rc == 0 && !target_seen) {
        char obj[BT_MUT_PATH_MAX];
        snprintf(obj, sizeof obj, "%s/%s", obj_root, file_rel);
        size_t obj_len = strlen(obj);
        if (obj_len > 2 && strcmp(obj + obj_len - 2, ".c") == 0) {
            snprintf(obj + obj_len - 2, sizeof obj - (obj_len - 2), ".obj");
        }
        target->target_index = objs.count;
        rc = bt_mut_paths_push(&objs, obj);
    }
    char test_obj[BT_MUT_PATH_MAX];
    char main_obj[BT_MUT_PATH_MAX];
    if (rc == 0) {
        snprintf(test_obj, sizeof test_obj, "%s/%s", obj_root, test_source);
        size_t len = strlen(test_obj);
        snprintf(test_obj + len - 2, sizeof test_obj - (len - 2), ".obj");
        snprintf(main_obj, sizeof main_obj, "%s/gen/%s_main.obj", obj_root, test_exe);
        rc = bt_mut_paths_push(&objs, test_obj);
        if (rc == 0) {
            rc = bt_mut_paths_push(&objs, main_obj);
        }
    }
    bt_mut_paths vendor_files = {0};
    if (rc == 0) {
        char vendor_base[BT_MUT_PATH_MAX];
        snprintf(vendor_base, sizeof vendor_base, "%s/vendor", obj_root);
        rc = scan_dir_recursive(vendor_base, "", &vendor_files, ".obj");
        for (size_t i = 0; i < vendor_files.count && rc == 0; i++) {
            char obj[BT_MUT_PATH_MAX];
            snprintf(obj, sizeof obj, "%s/vendor/%s", obj_root, vendor_files.items[i]);
            rc = bt_mut_paths_push(&objs, obj);
        }
        bt_mut_paths_free(&vendor_files);
    }
    char probe[BT_MUT_PATH_MAX];
    if (rc == 0) {
        snprintf(probe, sizeof probe, "%s/%s", engine->repo_root, test_source);
        FILE *file = fopen(probe, "rb");
        if (file == NULL) {
            engine_error("test object source missing: %s", probe);
            rc = -1;
        } else {
            fclose(file);
        }
    }
    bt_mut_paths_free(&src_files);
    if (rc != 0) {
        bt_mut_paths_free(&objs);
        bt_mut_target_free(target);
        BT_TRACE_FAIL("mutate.target_from_repo", "%s", bt_mut_engine_last_error());
        BT_TRACE_EXIT("mutate.target_from_repo");
        return -1;
    }
    target->link_objs = objs.items;
    target->link_obj_count = objs.count;
    BT_TRACE_EXIT("mutate.target_from_repo");
    return 0;
}

void bt_mut_target_free(bt_mut_target *target) {
    for (size_t i = 0; i < target->link_obj_count; i++) {
        free(target->link_objs[i]);
    }
    free(target->link_objs);
    target->link_objs = NULL;
    target->link_obj_count = 0;
}

int bt_mut_prepare_scratch(const bt_mut_engine *engine, const char *slot, const char *file_rel,
                           const char *text, size_t len, const bt_mut_mutant *mutant,
                           char source_out[BT_MUT_PATH_MAX], char include_out[BT_MUT_PATH_MAX]) {
    BT_TRACE_ENTER("mutate.prepare_scratch");
    include_out[0] = '\0';
    char dir[BT_MUT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s/%s", engine->scratch_root, slot);
    if (bt_mut_mkdirs(dir) != 0) {
        engine_error("cannot create scratch %s", dir);
        BT_TRACE_FAIL("mutate.prepare_scratch", "%s", bt_mut_engine_last_error());
        BT_TRACE_EXIT("mutate.prepare_scratch");
        return -1;
    }
    char marker[BT_MUT_PATH_MAX];
    snprintf(marker, sizeof marker, "%s/marker.txt", dir);
    char owner[32];
#ifdef _WIN32
    snprintf(owner, sizeof owner, "%lu", (unsigned long)GetCurrentProcessId());
#else
    snprintf(owner, sizeof owner, "0");
#endif
    if (bt_mut_write_file(marker, owner, strlen(owner)) != 0) {
        engine_error("cannot write %s", marker);
        BT_TRACE_FAIL("mutate.prepare_scratch", "%s", bt_mut_engine_last_error());
        BT_TRACE_EXIT("mutate.prepare_scratch");
        return -1;
    }
    const char *stem_start = strrchr(file_rel, '/');
    stem_start = stem_start == NULL ? file_rel : stem_start + 1;
    snprintf(source_out, BT_MUT_PATH_MAX, "%s/%s", dir, stem_start);
    if (mutant != NULL) {
        char *mutated = NULL;
        size_t mutated_len = 0;
        if (bt_mut_apply(text, len, mutant, &mutated, &mutated_len) != 0) {
            engine_error("cannot apply mutant %s", mutant->id);
            BT_TRACE_FAIL("mutate.prepare_scratch", "%s", bt_mut_engine_last_error());
            BT_TRACE_EXIT("mutate.prepare_scratch");
            return -1;
        }
        int rc = bt_mut_write_file(source_out, mutated, mutated_len);
        free(mutated);
        if (rc != 0) {
            engine_error("cannot write %s", source_out);
            BT_TRACE_FAIL("mutate.prepare_scratch", "%s", bt_mut_engine_last_error());
            BT_TRACE_EXIT("mutate.prepare_scratch");
            return -1;
        }
    } else if (bt_mut_write_file(source_out, text, len) != 0) {
        engine_error("cannot write %s", source_out);
        BT_TRACE_FAIL("mutate.prepare_scratch", "%s", bt_mut_engine_last_error());
        BT_TRACE_EXIT("mutate.prepare_scratch");
        return -1;
    }
    char obj_root[BT_MUT_PATH_MAX];
    if (obj_root_of(engine->repo_root, obj_root) == 0) {
        char dep[BT_MUT_PATH_MAX];
        snprintf(dep, sizeof dep, "%s/%s", obj_root, file_rel);
        size_t dep_len = strlen(dep);
        if (dep_len > 2 && strcmp(dep + dep_len - 2, ".c") == 0) {
            snprintf(dep + dep_len - 2, sizeof dep - (dep_len - 2), ".d");
        }
        char *dep_text = NULL;
        size_t dep_len_text = 0;
        if (bt_mut_read_file(dep, &dep_text, &dep_len_text) == 0) {
            char inputs[BT_MUT_PATH_MAX];
            snprintf(inputs, BT_MUT_PATH_MAX, "%s/inputs", dir);
            if (bt_mut_mkdirs(inputs) != 0) {
                free(dep_text);
                engine_error("cannot create %s", inputs);
                BT_TRACE_FAIL("mutate.prepare_scratch", "%s", bt_mut_engine_last_error());
                BT_TRACE_EXIT("mutate.prepare_scratch");
                return -1;
            }
            snprintf(include_out, BT_MUT_PATH_MAX, "%s", inputs);
            char root_norm[BT_MUT_PATH_MAX];
            snprintf(root_norm, sizeof root_norm, "%s", engine->repo_root);
            for (char *c = root_norm; *c != '\0'; c++) {
                if (*c == '\\') {
                    *c = '/';
                }
            }
            char obj_root_norm[BT_MUT_PATH_MAX];
            snprintf(obj_root_norm, sizeof obj_root_norm, "%s", obj_root);
            for (char *c = obj_root_norm; *c != '\0'; c++) {
                if (*c == '\\') {
                    *c = '/';
                }
            }
            size_t root_len = strlen(root_norm);
            size_t obj_root_len = strlen(obj_root_norm);
            char *cursor = dep_text;
            int failed = 0;
            while (cursor != NULL && *cursor != '\0' && !failed) {
                while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\\' ||
                       *cursor == '\t') {
                    cursor++;
                }
                if (*cursor == '\0') {
                    break;
                }
                char *end = cursor;
                while (*end != '\0' && *end != ' ' && *end != '\n' && *end != '\r') {
                    end++;
                }
                char saved = *end;
                *end = '\0';
                char token[BT_MUT_PATH_MAX];
                snprintf(token, sizeof token, "%s", cursor);
                *end = saved;
                cursor = end;
                for (char *c = token; *c != '\0'; c++) {
                    if (*c == '\\') {
                        *c = '/';
                    }
                }
                size_t token_len = strlen(token);
                if (token_len > 0 && token[token_len - 1] == ':') {
                    token[token_len - 1] = '\0';
                }
                if (strncmp(token, root_norm, root_len) != 0 || token[root_len] != '/' ||
                    strncmp(token, obj_root_norm, obj_root_len) == 0) {
                    continue;
                }
                const char *rel = token + root_len + 1;
                if (rel[0] == '\0') {
                    continue;
                }
                char copy_dest[BT_MUT_PATH_MAX];
                snprintf(copy_dest, sizeof copy_dest, "%s/inputs/%s", dir, rel);
                char *parent = strrchr(copy_dest, '/');
                if (parent != NULL) {
                    *parent = '\0';
                    int mk = bt_mut_mkdirs(copy_dest);
                    *parent = '/';
                    if (mk != 0) {
                        engine_error("cannot create input dir for %s", token);
                        failed = 1;
                        break;
                    }
                }
                char *content = NULL;
                size_t content_len = 0;
                if (bt_mut_read_file(token, &content, &content_len) == 0) {
                    int w = bt_mut_write_file(copy_dest, content, content_len);
                    free(content);
                    if (w != 0) {
                        engine_error("cannot copy input %s", token);
                        failed = 1;
                        break;
                    }
                }
            }
            free(dep_text);
            if (failed) {
                BT_TRACE_FAIL("mutate.prepare_scratch", "%s", bt_mut_engine_last_error());
                BT_TRACE_EXIT("mutate.prepare_scratch");
                return -1;
            }
        }
    }
    BT_TRACE_EXIT("mutate.prepare_scratch");
    return 0;
}

static int run_one(const bt_mut_engine *engine, const bt_mut_target *target, const char *slot,
                   const char *text, size_t len, const bt_mut_mutant *mutant,
                   bt_mut_status *verdict) {
    BT_TRACE_ENTER("mutate.run_one");
    *verdict = BT_MUT_BUILDERR;
    char source[BT_MUT_PATH_MAX];
    char include_extra[BT_MUT_PATH_MAX] = "";
    if (bt_mut_prepare_scratch(engine, slot, target->file, text, len, mutant, source,
                               include_extra) != 0) {
        BT_TRACE_FAIL("mutate.run_one", "%s", bt_mut_engine_last_error());
        BT_TRACE_EXIT("mutate.run_one");
        return -1;
    }
    char dir[BT_MUT_PATH_MAX];
    snprintf(dir, sizeof dir, "%s/%s", engine->scratch_root, slot);
    const char *stem_start = strrchr(target->file, '/');
    stem_start = stem_start == NULL ? target->file : stem_start + 1;
    char stem[256];
    snprintf(stem, sizeof stem, "%s", stem_start);
    size_t stem_len = strlen(stem);
    if (stem_len > 2 && strcmp(stem + stem_len - 2, ".c") == 0) {
        stem[stem_len - 2] = '\0';
    }
    char obj[BT_MUT_PATH_MAX];
    snprintf(obj, sizeof obj, "%s/%s.obj", dir, stem);
    char dir_include[BT_MUT_PATH_MAX] = "";
    const char *file_slash = strrchr(target->file, '/');
    if (file_slash != NULL) {
        snprintf(dir_include, sizeof dir_include, "%s/%.*s", engine->repo_root,
                 (int)(file_slash - target->file), target->file);
    }
    char diag[2048];
    int compile_rc = bt_mut_compile_file(
        &engine->build, dir_include[0] != '\0' ? dir_include : NULL,
        include_extra[0] != '\0' ? include_extra : NULL, source, obj, diag, sizeof diag);
    if (compile_rc == -1) {
        *verdict = BT_MUT_PENDING;
        bt_mut_rmtree(dir);
        BT_TRACE_FAIL("mutate.run_one", "compile spawn failed");
        BT_TRACE_EXIT("mutate.run_one");
        return 0;
    }
    if (compile_rc != 0) {
        *verdict = BT_MUT_BUILDERR;
        bt_mut_rmtree(dir);
        BT_TRACE_FAIL("mutate.run_one", "compile failed rc=%d", compile_rc);
        BT_TRACE_EXIT("mutate.run_one");
        return 0;
    }
    char **objects = malloc(target->link_obj_count * sizeof(char *));
    if (objects == NULL) {
        bt_mut_rmtree(dir);
        BT_TRACE_FAIL("mutate.run_one", "oom");
        BT_TRACE_EXIT("mutate.run_one");
        return -1;
    }
    for (size_t i = 0; i < target->link_obj_count; i++) {
        objects[i] = target->link_objs[i];
    }
    objects[target->target_index] = obj;
    char exe[BT_MUT_PATH_MAX];
    snprintf(exe, sizeof exe, "%s/%s.exe", dir, engine->test_exe);
    char *link_argv[256];
    size_t link_count =
        bt_mut_link_argv(&engine->build, objects, target->link_obj_count, exe, link_argv, 256);
    if (link_count == 0 || link_count + 1 > 256) {
        free(objects);
        bt_mut_rmtree(dir);
        *verdict = BT_MUT_BUILDERR;
        BT_TRACE_FAIL("mutate.run_one", "link argv assembly failed");
        BT_TRACE_EXIT("mutate.run_one");
        return 0;
    }
    link_argv[link_count] = NULL;
    bt_mut_proc proc;
    int link_rc = bt_mut_spawn(engine->build.clang, link_argv, NULL, 300000, &proc);
    free(objects);
    if (link_rc == -1) {
        bt_mut_proc_free(&proc);
        *verdict = BT_MUT_PENDING;
        bt_mut_rmtree(dir);
        BT_TRACE_FAIL("mutate.run_one", "link spawn failed");
        BT_TRACE_EXIT("mutate.run_one");
        return 0;
    }
    if (link_rc != 0 || proc.exit_code != 0) {
        bt_mut_proc_free(&proc);
        *verdict = BT_MUT_BUILDERR;
        bt_mut_rmtree(dir);
        BT_TRACE_FAIL("mutate.run_one", "link failed");
        BT_TRACE_EXIT("mutate.run_one");
        return 0;
    }
    bt_mut_proc_free(&proc);
    char exe_native[BT_MUT_PATH_MAX];
    snprintf(exe_native, sizeof exe_native, "%s", exe);
    for (char *cursor = exe_native; *cursor != '\0'; cursor++) {
        if (*cursor == '/') {
            *cursor = '\\';
        }
    }
    char *run_argv[2] = {exe_native, NULL};
    int run_rc =
        bt_mut_spawn(exe_native, run_argv, engine->repo_root, (unsigned)engine->timeout_ms, &proc);
    if (run_rc == -2) {
        *verdict = BT_MUT_TIMEOUT;
    } else if (run_rc == -1) {
        *verdict = BT_MUT_PENDING;
    } else if (proc.exit_code == 0) {
        *verdict = BT_MUT_SURVIVED;
    } else {
        *verdict = BT_MUT_KILLED;
    }
    bt_mut_proc_free(&proc);
    bt_mut_rmtree(dir);
    BT_TRACE_EXIT("mutate.run_one");
    return 0;
}

static int only_allows(const char *only_csv, const char *id) {
    if (only_csv == NULL || only_csv[0] == '\0') {
        return 1;
    }
    size_t id_len = strlen(id);
    const char *cursor = only_csv;
    while (*cursor != '\0') {
        const char *comma = strchr(cursor, ',');
        size_t len = comma == NULL ? strlen(cursor) : (size_t)(comma - cursor);
        if (len == id_len && memcmp(cursor, id, len) == 0) {
            return 1;
        }
        if (comma == NULL) {
            break;
        }
        cursor = comma + 1;
    }
    return 0;
}

int bt_mut_engine_run(bt_mut_engine *engine, bt_mut_target *targets, size_t target_count,
                      const char *only_csv, size_t limit, bt_mut_summary *summary) {
    BT_TRACE_ENTER("mutate.engine_run");
    memset(summary, 0, sizeof *summary);
    if (engine->workers != 1) {
        engine_error("mutants workers must be 1, got %llu", engine->workers);
        BT_TRACE_FAIL("mutate.engine_run", "%s", bt_mut_engine_last_error());
        BT_TRACE_EXIT("mutate.engine_run");
        return 2;
    }
    bt_mut_scratch_sweep(engine->scratch_root, bt_mut_process_start());
    bt_mut_ledger ledger;
    if (bt_mut_ledger_load(engine->ledger_path, &ledger) != 0) {
        engine_error("cannot load ledger %s", engine->ledger_path);
        BT_TRACE_FAIL("mutate.engine_run", "%s", bt_mut_engine_last_error());
        BT_TRACE_EXIT("mutate.engine_run");
        return 2;
    }
    bt_mut_mutant_list *lists = calloc(target_count, sizeof(bt_mut_mutant_list));
    char **texts = calloc(target_count, sizeof(char *));
    size_t *text_lens = calloc(target_count, sizeof(size_t));
    int rc = 0;
    if (lists == NULL || texts == NULL || text_lens == NULL) {
        rc = 2;
    }
    for (size_t t = 0; t < target_count && rc == 0; t++) {
        char abs_path[BT_MUT_PATH_MAX];
        snprintf(abs_path, sizeof abs_path, "%s/%s", engine->repo_root, targets[t].file);
        if (bt_mut_read_file(abs_path, &texts[t], &text_lens[t]) != 0) {
            engine_error("cannot read target %s", abs_path);
            rc = 2;
            break;
        }
        if (bt_mut_enumerate(targets[t].file, texts[t], text_lens[t], &lists[t]) != 0) {
            engine_error("cannot enumerate %s", targets[t].file);
            rc = 2;
            break;
        }
        for (size_t i = 0; i < lists[t].count; i++) {
            if (bt_mut_ledger_upsert(&ledger, &lists[t].items[i], engine->test_exe) != 0) {
                rc = 2;
                break;
            }
        }
    }
    for (size_t t = 0; t < target_count && rc == 0; t++) {
        for (size_t r = 0; r < ledger.count; r++) {
            if (strcmp(ledger.rows[r].file, targets[t].file) != 0) {
                continue;
            }
            int enumerated = 0;
            for (size_t m = 0; m < lists[t].count; m++) {
                if (strcmp(lists[t].items[m].id, ledger.rows[r].id) == 0) {
                    enumerated = 1;
                    break;
                }
            }
            if (!enumerated) {
                bt_mut_ledger_drop(&ledger, ledger.rows[r].id);
                r--;
            }
        }
    }
    if (rc == 0 && bt_mut_ledger_save(engine->ledger_path, &ledger) != 0) {
        engine_error("cannot save ledger %s", engine->ledger_path);
        rc = 2;
    }
    for (size_t t = 0; t < target_count && rc == 0; t++) {
        bt_mut_status verdict = BT_MUT_PENDING;
        char slot[64];
        snprintf(slot, sizeof slot, "baseline-%zu", t);
        int run_rc = run_one(engine, &targets[t], slot, texts[t], text_lens[t], NULL, &verdict);
        if (run_rc != 0 || verdict != BT_MUT_SURVIVED) {
            engine_error("baseline run failed for %s (verdict %s)", targets[t].file,
                         bt_mut_status_name(verdict));
            BT_TRACE_FAIL("mutate.engine_run", "%s", bt_mut_engine_last_error());
            rc = 1;
        }
        char dir[BT_MUT_PATH_MAX];
        snprintf(dir, sizeof dir, "%s/%s", engine->scratch_root, slot);
        bt_mut_rmtree(dir);
    }
    size_t executed = 0;
    int limit_hit = 0;
    for (size_t t = 0; t < target_count && rc == 0 && !limit_hit; t++) {
        for (size_t i = 0; i < lists[t].count && rc == 0; i++) {
            const bt_mut_mutant *mutant = &lists[t].items[i];
            bt_mut_row *row = bt_mut_ledger_find(&ledger, mutant->id);
            if (row == NULL || row->status != BT_MUT_PENDING) {
                continue;
            }
            if (!only_allows(only_csv, mutant->id)) {
                continue;
            }
            if (limit != 0 && executed >= limit) {
                limit_hit = 1;
                break;
            }
            executed++;
            row->attempts++;
            bt_mut_now_iso(row->last_run);
            bt_mut_ledger_save(engine->ledger_path, &ledger);
            bt_mut_status verdict = BT_MUT_BUILDERR;
            int run_rc =
                run_one(engine, &targets[t], mutant->id, texts[t], text_lens[t], mutant, &verdict);
            if (run_rc != 0) {
                rc = 1;
                break;
            }
            if (verdict == BT_MUT_PENDING) {
                engine_error("spawn failed for mutant %s in %s", mutant->id, targets[t].file);
                BT_TRACE_FAIL("mutate.engine_run", "%s", bt_mut_engine_last_error());
                rc = 1;
                break;
            }
            row->status = verdict;
            bt_mut_now_iso(row->last_run);
            if (bt_mut_ledger_save(engine->ledger_path, &ledger) != 0) {
                engine_error("cannot save ledger %s", engine->ledger_path);
                rc = 2;
                break;
            }
        }
    }
    for (size_t i = 0; i < ledger.count; i++) {
        summary->total++;
        switch (ledger.rows[i].status) {
        case BT_MUT_KILLED:
            summary->killed++;
            break;
        case BT_MUT_SURVIVED:
            summary->survived++;
            break;
        case BT_MUT_TIMEOUT:
            summary->timeout++;
            break;
        case BT_MUT_BUILDERR:
            summary->builderr++;
            break;
        case BT_MUT_PENDING:
            summary->pending++;
            break;
        }
    }
    for (size_t t = 0; t < target_count; t++) {
        bt_mut_mutants_free(&lists[t]);
        free(texts[t]);
    }
    free(lists);
    free(texts);
    free(text_lens);
    bt_mut_ledger_free(&ledger);
    if (rc != 0) {
        BT_TRACE_FAIL("mutate.engine_run", "verdict %d: %s", rc, bt_mut_engine_last_error());
    }
    BT_TRACE_EXIT("mutate.engine_run");
    return rc;
}
