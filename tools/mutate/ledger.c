#include "mutate/mutate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/bt_trace.h"
#include "common/paths.h"

const char *bt_mut_status_name(bt_mut_status status) {
    switch (status) {
    case BT_MUT_PENDING:
        return "pending";
    case BT_MUT_KILLED:
        return "killed";
    case BT_MUT_SURVIVED:
        return "survived";
    case BT_MUT_TIMEOUT:
        return "timeout";
    case BT_MUT_BUILDERR:
        return "builderr";
    }
    return "pending";
}

int bt_mut_status_parse(const char *name, bt_mut_status *out) {
    for (int status = 0; status <= BT_MUT_BUILDERR; status++) {
        if (strcmp(bt_mut_status_name((bt_mut_status)status), name) == 0) {
            *out = (bt_mut_status)status;
            return 0;
        }
    }
    return -1;
}

void bt_mut_now_iso(char out[32]) {
    time_t now = time(NULL);
    struct tm local;
    localtime_s(&local, &now);
    snprintf(out, 32, "%04d-%02d-%02dT%02d:%02d:%02d", local.tm_year + 1900, local.tm_mon + 1,
             local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
}

static char *dup_str(const char *text) {
    size_t len = strlen(text);
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static void row_free(bt_mut_row *row) {
    free(row->file);
    free(row->op);
    free(row->from);
    free(row->to);
    free(row->last_run);
    free(row->test_exe);
}

void bt_mut_ledger_free(bt_mut_ledger *ledger) {
    for (size_t i = 0; i < ledger->count; i++) {
        row_free(&ledger->rows[i]);
    }
    free(ledger->rows);
    ledger->rows = NULL;
    ledger->count = 0;
    ledger->cap = 0;
}

static int row_from_fields(char *fields[11], size_t field_count, bt_mut_row *row) {
    memset(row, 0, sizeof *row);
    if (strlen(fields[0]) != BT_MUT_ID_LEN) {
        return -1;
    }
    memcpy(row->id, fields[0], BT_MUT_ID_LEN);
    row->id[BT_MUT_ID_LEN] = '\0';
    row->line = strtoull(fields[2], NULL, 10);
    row->col = strtoull(fields[3], NULL, 10);
    row->file = dup_str(fields[1]);
    row->op = dup_str(fields[4]);
    row->from = dup_str(fields[5]);
    row->to = dup_str(fields[6]);
    row->attempts = strtoull(fields[8], NULL, 10);
    row->last_run = malloc(32);
    if (row->last_run != NULL) {
        snprintf(row->last_run, 32, "%s", fields[9]);
    }
    row->test_exe = dup_str(field_count >= 11 ? fields[10] : "");
    if (bt_mut_status_parse(fields[7], &row->status) != 0) {
        row_free(row);
        return -1;
    }
    if (row->file == NULL || row->op == NULL || row->from == NULL || row->to == NULL ||
        row->last_run == NULL || row->test_exe == NULL) {
        row_free(row);
        return -1;
    }
    return 0;
}

int bt_mut_ledger_load(const char *path, bt_mut_ledger *ledger) {
    BT_TRACE_ENTER("mutate.ledger_load");
    ledger->rows = NULL;
    ledger->count = 0;
    ledger->cap = 0;
    char *text = NULL;
    size_t len = 0;
    if (bt_mut_read_file(path, &text, &len) != 0) {
        BT_TRACE_EXIT("mutate.ledger_load");
        return 0;
    }
    int rc = 0;
    char *cursor = text;
    while (cursor != NULL && rc == 0) {
        char *newline = strchr(cursor, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }
        size_t line_len = strlen(cursor);
        if (line_len > 0 && cursor[line_len - 1] == '\r') {
            cursor[line_len - 1] = '\0';
        }
        if (strncmp(cursor, "id\t", 3) == 0 || cursor[0] == '\0') {
            cursor = newline == NULL ? NULL : newline + 1;
            continue;
        }
        char *fields[11];
        size_t tabs = 0;
        for (char *scan = cursor; *scan != '\0'; scan++) {
            if (*scan == '\t') {
                tabs++;
            }
        }
        if (tabs != 9 && tabs != 10) {
            rc = -1;
            break;
        }
        size_t field_count = tabs + 1;
        size_t index = 0;
        fields[index++] = cursor;
        for (char *scan = cursor; *scan != '\0'; scan++) {
            if (*scan == '\t') {
                *scan = '\0';
                fields[index++] = scan + 1;
            }
        }
        if (ledger->count == ledger->cap) {
            size_t want = ledger->cap == 0 ? 16 : ledger->cap * 2;
            bt_mut_row *grown = realloc(ledger->rows, want * sizeof(bt_mut_row));
            if (grown == NULL) {
                rc = -1;
                break;
            }
            ledger->rows = grown;
            ledger->cap = want;
        }
        if (row_from_fields(fields, field_count, &ledger->rows[ledger->count]) != 0) {
            rc = -1;
            break;
        }
        ledger->count++;
        cursor = newline == NULL ? NULL : newline + 1;
    }
    free(text);
    if (rc != 0) {
        bt_mut_ledger_free(ledger);
        BT_TRACE_FAIL("mutate.ledger_load", "malformed ledger %s", path);
    }
    BT_TRACE_EXIT("mutate.ledger_load");
    return rc;
}

int bt_mut_ledger_save(const char *path, const bt_mut_ledger *ledger) {
    BT_TRACE_ENTER("mutate.ledger_save");
    char *buffer = malloc(BT_MUT_LINE_MAX);
    size_t used = 0;
    size_t cap = BT_MUT_LINE_MAX;
    if (buffer == NULL) {
        BT_TRACE_FAIL("mutate.ledger_save", "oom");
        BT_TRACE_EXIT("mutate.ledger_save");
        return -1;
    }
    int rc = 0;
    int written = snprintf(buffer + used, cap - used,
                           "id\tfile\tline\tcol\top\tfrom\tto\tstatus\tattempts\tlast_run\t"
                           "test_exe\n");
    if (written < 0 || (size_t)written >= cap - used) {
        rc = -1;
    } else {
        used += (size_t)written;
    }
    for (size_t i = 0; i < ledger->count && rc == 0; i++) {
        const bt_mut_row *row = &ledger->rows[i];
        int needed = snprintf(NULL, 0, "%s\t%s\t%zu\t%zu\t%s\t%s\t%s\t%s\t%llu\t%s\t%s\n", row->id,
                              row->file, row->line, row->col, row->op, row->from, row->to,
                              bt_mut_status_name(row->status), row->attempts,
                              row->last_run == NULL ? "" : row->last_run,
                              row->test_exe == NULL ? "" : row->test_exe);
        if (needed < 0) {
            rc = -1;
            break;
        }
        if (used + (size_t)needed + 1 > cap) {
            size_t want = cap * 2;
            while (used + (size_t)needed + 1 > want) {
                want *= 2;
            }
            char *grown = realloc(buffer, want);
            if (grown == NULL) {
                rc = -1;
                break;
            }
            buffer = grown;
            cap = want;
        }
        snprintf(buffer + used, cap - used, "%s\t%s\t%zu\t%zu\t%s\t%s\t%s\t%s\t%llu\t%s\t%s\n",
                 row->id, row->file, row->line, row->col, row->op, row->from, row->to,
                 bt_mut_status_name(row->status), row->attempts,
                 row->last_run == NULL ? "" : row->last_run,
                 row->test_exe == NULL ? "" : row->test_exe);
        used += (size_t)needed;
    }
    if (rc == 0 && bt_paths_write_atomic(path, buffer, used) != 0) {
        rc = -1;
    }
    free(buffer);
    if (rc != 0) {
        BT_TRACE_FAIL("mutate.ledger_save", "cannot save %s", path);
    }
    BT_TRACE_EXIT("mutate.ledger_save");
    return rc;
}

bt_mut_row *bt_mut_ledger_find(bt_mut_ledger *ledger, const char *id) {
    for (size_t i = 0; i < ledger->count; i++) {
        if (strcmp(ledger->rows[i].id, id) == 0) {
            return &ledger->rows[i];
        }
    }
    return NULL;
}

int bt_mut_ledger_upsert(bt_mut_ledger *ledger, const bt_mut_mutant *mutant, const char *test_exe) {
    bt_mut_row *found = bt_mut_ledger_find(ledger, mutant->id);
    if (found != NULL) {
        if (found->test_exe == NULL || strcmp(found->test_exe, test_exe) != 0) {
            char *copy = dup_str(test_exe);
            if (copy == NULL) {
                return -1;
            }
            free(found->test_exe);
            found->test_exe = copy;
            found->status = BT_MUT_PENDING;
        }
        return 0;
    }
    if (ledger->count == ledger->cap) {
        size_t want = ledger->cap == 0 ? 16 : ledger->cap * 2;
        bt_mut_row *grown = realloc(ledger->rows, want * sizeof(bt_mut_row));
        if (grown == NULL) {
            return -1;
        }
        ledger->rows = grown;
        ledger->cap = want;
    }
    bt_mut_row *row = &ledger->rows[ledger->count];
    memset(row, 0, sizeof *row);
    memcpy(row->id, mutant->id, BT_MUT_ID_LEN + 1);
    row->file = dup_str(mutant->file);
    row->op = dup_str(bt_mut_op_name(mutant->op));
    row->from = dup_str(mutant->from);
    row->to = dup_str(mutant->to);
    row->line = mutant->line;
    row->col = mutant->col;
    row->status = BT_MUT_PENDING;
    row->attempts = 0;
    row->last_run = malloc(32);
    if (row->last_run != NULL) {
        row->last_run[0] = '\0';
    }
    row->test_exe = dup_str(test_exe);
    if (row->file == NULL || row->op == NULL || row->from == NULL || row->to == NULL ||
        row->last_run == NULL || row->test_exe == NULL) {
        row_free(row);
        return -1;
    }
    ledger->count++;
    return 0;
}

void bt_mut_ledger_drop(bt_mut_ledger *ledger, const char *id) {
    BT_TRACE_ENTER("mutate.ledger_drop");
    for (size_t i = 0; i < ledger->count; i++) {
        if (strcmp(ledger->rows[i].id, id) == 0) {
            row_free(&ledger->rows[i]);
            memmove(&ledger->rows[i], &ledger->rows[i + 1],
                    (ledger->count - i - 1) * sizeof(bt_mut_row));
            ledger->count--;
            BT_TRACE_EXIT("mutate.ledger_drop");
            return;
        }
    }
    BT_TRACE_EXIT("mutate.ledger_drop");
}
