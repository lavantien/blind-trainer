#ifndef MUTATE_MUTATE_H
#define MUTATE_MUTATE_H

#include <stddef.h>
#include <stdint.h>

#include "common/json.h"

#define BT_MUT_PATH_MAX 1024
#define BT_MUT_ID_LEN 16
#define BT_MUT_LINE_MAX 4096

int bt_mut_read_file(const char *path, char **out, size_t *out_len);
int bt_mut_write_file(const char *path, const char *data, size_t len);
int bt_mut_mkdirs(const char *path);
int bt_mut_rmtree(const char *path);

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} bt_mut_paths;

int bt_mut_paths_push(bt_mut_paths *paths, const char *text);
void bt_mut_paths_free(bt_mut_paths *paths);

typedef enum {
    BT_MUT_TOK_PP = 0,
    BT_MUT_TOK_KEYWORD,
    BT_MUT_TOK_IDENT,
    BT_MUT_TOK_NUMBER,
    BT_MUT_TOK_STRING,
    BT_MUT_TOK_CHAR,
    BT_MUT_TOK_OP,
} bt_mut_tok_kind;

typedef struct {
    uint8_t kind;
    size_t offset;
    size_t len;
    size_t line;
    size_t col;
    size_t end_line;
    size_t end_col;
} bt_mut_token;

typedef struct {
    char *text;
    size_t text_len;
    bt_mut_token *items;
    size_t count;
    size_t cap;
} bt_mut_tokens;

int bt_mut_tokenize(const char *text, size_t len, bt_mut_tokens *out);
void bt_mut_tokens_free(bt_mut_tokens *tokens);
const char *bt_mut_token_text(const bt_mut_tokens *tokens, const bt_mut_token *token);
size_t bt_mut_offset_at(const char *text, size_t line, size_t col);
const char *bt_mut_function_at(const bt_mut_tokens *tokens, size_t offset);

typedef enum {
    BT_MUT_NUMERIC_TWEAK = 0,
    BT_MUT_COMPARISON_SWAP,
    BT_MUT_CONDITION_NEGATION,
    BT_MUT_UNARY_DROP,
    BT_MUT_STATEMENT_DELETE,
} bt_mut_op;

const char *bt_mut_op_name(bt_mut_op op);

typedef struct {
    size_t offset;
    size_t len;
    char *text;
} bt_mut_edit;

typedef struct {
    char id[BT_MUT_ID_LEN + 1];
    char *file;
    size_t line;
    size_t col;
    size_t end_line;
    size_t end_col;
    bt_mut_op op;
    char *from;
    char *to;
    bt_mut_edit edits[2];
    size_t edit_count;
} bt_mut_mutant;

typedef struct {
    bt_mut_mutant *items;
    size_t count;
    size_t cap;
} bt_mut_mutant_list;

int bt_mut_enumerate(const char *file_rel, const char *text, size_t len, bt_mut_mutant_list *out);
void bt_mut_mutants_free(bt_mut_mutant_list *list);
int bt_mut_apply(const char *text, size_t len, const bt_mut_mutant *mutant, char **out,
                 size_t *out_len);
const bt_mut_mutant *bt_mut_find(const bt_mut_mutant_list *list, bt_mut_op op, const char *from,
                                 const char *to, size_t line);

typedef enum {
    BT_MUT_PENDING = 0,
    BT_MUT_KILLED,
    BT_MUT_SURVIVED,
    BT_MUT_TIMEOUT,
    BT_MUT_BUILDERR,
} bt_mut_status;

const char *bt_mut_status_name(bt_mut_status status);
int bt_mut_status_parse(const char *name, bt_mut_status *out);

typedef struct {
    char id[BT_MUT_ID_LEN + 1];
    char *file;
    size_t line;
    size_t col;
    char *op;
    char *from;
    char *to;
    bt_mut_status status;
    unsigned long long attempts;
    char *last_run;
} bt_mut_row;

typedef struct {
    bt_mut_row *rows;
    size_t count;
    size_t cap;
} bt_mut_ledger;

int bt_mut_ledger_load(const char *path, bt_mut_ledger *ledger);
int bt_mut_ledger_save(const char *path, const bt_mut_ledger *ledger);
bt_mut_row *bt_mut_ledger_find(bt_mut_ledger *ledger, const char *id);
int bt_mut_ledger_upsert(bt_mut_ledger *ledger, const bt_mut_mutant *mutant);
void bt_mut_ledger_free(bt_mut_ledger *ledger);
void bt_mut_now_iso(char out[32]);

int bt_mut_scratch_root(const char *config_value, char out[BT_MUT_PATH_MAX]);
uint64_t bt_mut_process_start(void);
int bt_mut_scratch_is_orphan(const char *dir, uint64_t live_start);
size_t bt_mut_scratch_sweep(const char *root, uint64_t live_start);
int bt_mut_file_creation(const char *path, uint64_t *out);
int bt_mut_set_file_creation(const char *path, uint64_t value);

typedef struct {
    char clang[BT_MUT_PATH_MAX];
    char **compile_flags;
    size_t compile_flag_count;
    char **link_flags;
    size_t link_flag_count;
    char include_dirs[3][BT_MUT_PATH_MAX];
} bt_mut_build;

int bt_mut_build_load(const char *repo_root, const bt_json_value *config,
                      const char *clang_override, bt_mut_build *out);
void bt_mut_build_free(bt_mut_build *build);
size_t bt_mut_compile_argv(const bt_mut_build *build, const char *extra_include, const char *source,
                           const char *obj, char **argv, size_t cap);
size_t bt_mut_link_argv(const bt_mut_build *build, char *const *objects, size_t object_count,
                        const char *exe, char **argv, size_t cap);

typedef struct {
    int rc;
    int exit_code;
    int timed_out;
    char *output;
    size_t output_len;
} bt_mut_proc;

int bt_mut_spawn(const char *exe, char *const argv[], const char *cwd, unsigned timeout_ms,
                 bt_mut_proc *out);
void bt_mut_proc_free(bt_mut_proc *proc);
int bt_mut_compile_file(const bt_mut_build *build, const char *extra_include, const char *source,
                        const char *obj, char *diag, size_t diag_cap);

typedef struct {
    char repo_root[BT_MUT_PATH_MAX];
    bt_mut_build build;
    char scratch_root[BT_MUT_PATH_MAX];
    char ledger_path[BT_MUT_PATH_MAX];
    unsigned long long timeout_ms;
    unsigned long long workers;
    char test_exe[BT_MUT_PATH_MAX];
} bt_mut_engine;

typedef struct {
    char file[BT_MUT_PATH_MAX];
    char **link_objs;
    size_t link_obj_count;
    size_t target_index;
} bt_mut_target;

int bt_mut_target_from_repo(const bt_mut_engine *engine, const char *file_rel, const char *test_exe,
                            bt_mut_target *target);
void bt_mut_target_free(bt_mut_target *target);

typedef struct {
    unsigned long long total;
    unsigned long long killed;
    unsigned long long survived;
    unsigned long long timeout;
    unsigned long long builderr;
    unsigned long long pending;
} bt_mut_summary;

int bt_mut_engine_run(bt_mut_engine *engine, bt_mut_target *targets, size_t target_count,
                      const char *only_csv, size_t limit, bt_mut_summary *summary);
const char *bt_mut_engine_last_error(void);

int bt_mut_report(const char *ledger_path, const char *repo_root, const char *out_path);

#endif
