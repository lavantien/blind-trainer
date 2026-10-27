#include "data/corpus.h"

#include "common/bt_trace.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SOLVER_DIRS[3] = {"cherry", "eggins", "user"};
static const char *SHEET_FILES[BT_SHEET_CLASS_COUNT] = {
    "edge", "corner", "flips", "twists", "parity", "ltct",
};

const char *bt_solver_dir(bt_solver solver) {
    return SOLVER_DIRS[solver];
}

bool bt_solver_parse(const char *text, bt_solver *out) {
    for (int i = 0; i < 3; i++) {
        if (strcmp(text, SOLVER_DIRS[i]) == 0) {
            *out = (bt_solver)i;
            return true;
        }
    }
    return false;
}

const char *bt_sheet_class_file(bt_sheet_class sheet) {
    return SHEET_FILES[sheet];
}

bool bt_sheet_class_parse(const char *text, bt_sheet_class *out) {
    for (int i = 0; i < BT_SHEET_CLASS_COUNT; i++) {
        if (strcmp(text, SHEET_FILES[i]) == 0) {
            *out = (bt_sheet_class)i;
            return true;
        }
    }
    return false;
}

static bool sheet_class_parse_n(const char *text, size_t len, bt_sheet_class *out) {
    for (int i = 0; i < BT_SHEET_CLASS_COUNT; i++) {
        if (strlen(SHEET_FILES[i]) == len && strncmp(text, SHEET_FILES[i], len) == 0) {
            *out = (bt_sheet_class)i;
            return true;
        }
    }
    return false;
}

void bt_corpus_error_string(const bt_corpus_error *err, char *out, size_t out_len) {
    BT_TRACE_ENTER("corpus.error_string");
    char tsv_text[192];
    switch (err->kind) {
    case BT_CORPUS_ERR_READ:
        snprintf(out, out_len, "read %s: %s", err->path, strerror(errno));
        break;
    case BT_CORPUS_ERR_PARSE:
        bt_tsv_error_string(&err->tsv, tsv_text, sizeof tsv_text);
        snprintf(out, out_len, "parse %s: %s", err->path, tsv_text);
        break;
    case BT_CORPUS_ERR_ALG:
        snprintf(out, out_len, "alg in %s rowid %zu: %s", err->path, err->rowid, err->alg_error);
        break;
    }
    BT_TRACE_EXIT("corpus.error_string");
}

void bt_overlay_error_string(const bt_overlay_error *err, char *out, size_t out_len) {
    BT_TRACE_ENTER("corpus.overlay_error_string");
    if (err->kind == BT_OVERLAY_ERR_UNKNOWN_SHEET) {
        snprintf(out, out_len, "unknown sheet in overlay key: %s", err->key);
    } else {
        snprintf(out, out_len, "bad alg for position %s: %s", err->position, err->alg_error);
    }
    BT_TRACE_EXIT("corpus.overlay_error_string");
}

static char *text_dup(const char *src) {
    size_t len = strlen(src);
    char *out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, src, len + 1);
    return out;
}

void bt_corpus_init(bt_corpus *corpus) {
    corpus->rows = NULL;
    corpus->count = 0;
}

void bt_corpus_free(bt_corpus *corpus) {
    BT_TRACE_ENTER("corpus.free");
    for (size_t i = 0; i < corpus->count; i++) {
        free(corpus->rows[i].position);
        bt_alg_free(&corpus->rows[i].alg);
        free(corpus->rows[i].alg_text);
        free(corpus->rows[i].commutator);
        free(corpus->rows[i].recon_link);
        free(corpus->rows[i].youtube_link);
    }
    free(corpus->rows);
    corpus->rows = NULL;
    corpus->count = 0;
    BT_TRACE_EXIT("corpus.free");
}

int bt_corpus_from_rows(bt_corpus_row *rows, size_t count, bt_corpus *out) {
    BT_TRACE_ENTER("corpus.from_rows");
    out->rows = rows;
    out->count = count;
    BT_TRACE_EXIT("corpus.from_rows");
    return 0;
}

static int rows_push(bt_corpus *out, bt_corpus_row row) {
    bt_corpus_row *grown = realloc(out->rows, (out->count + 1) * sizeof *grown);
    if (grown == NULL) {
        return -1;
    }
    out->rows = grown;
    out->rows[out->count] = row;
    out->count += 1;
    return 0;
}

int bt_corpus_load(const char *root, bt_corpus *out, bt_corpus_error *err) {
    BT_TRACE_ENTER("corpus.load");
    bt_corpus_init(out);
    for (int solver = 0; solver < BT_SOLVER_VENDOR_COUNT; solver++) {
        for (int sheet = 0; sheet < BT_SHEET_CLASS_COUNT; sheet++) {
            char path[BT_PATH_MAX];
            snprintf(path, sizeof path, "%s/%s/%s.tsv", root, SOLVER_DIRS[solver],
                     SHEET_FILES[sheet]);
            FILE *file = fopen(path, "rb");
            if (file == NULL) {
                err->kind = BT_CORPUS_ERR_READ;
                snprintf(err->path, sizeof err->path, "%s", path);
                bt_corpus_free(out);
                BT_TRACE_FAIL("corpus.load", "read %s failed", path);
                return -1;
            }
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, 0, SEEK_SET);
            char *text = malloc((size_t)size + 1);
            if (text == NULL) {
                fclose(file);
                bt_corpus_free(out);
                BT_TRACE_FAIL("corpus.load", "alloc failed for %s", path);
                return -1;
            }
            size_t got = fread(text, 1, (size_t)size, file);
            fclose(file);
            text[got] = '\0';
            bt_tsv_rows parsed;
            if (bt_tsv_parse_sheet(text, &parsed, &err->tsv) != 0) {
                free(text);
                err->kind = BT_CORPUS_ERR_PARSE;
                snprintf(err->path, sizeof err->path, "%s", path);
                bt_corpus_free(out);
                BT_TRACE_FAIL("corpus.load", "parse %s failed", path);
                return -1;
            }
            for (size_t i = 0; i < parsed.count; i++) {
                bt_corpus_row row;
                memset(&row, 0, sizeof row);
                row.solver = (bt_solver)solver;
                row.sheet = (bt_sheet_class)sheet;
                row.rowid = parsed.items[i].rowid;
                bt_alg_init(&row.alg);
                if (!bt_alg_parse(parsed.items[i].row.alg, &row.alg, err->alg_error,
                                  sizeof err->alg_error)) {
                    free(text);
                    err->kind = BT_CORPUS_ERR_ALG;
                    snprintf(err->path, sizeof err->path, "%s", path);
                    err->rowid = parsed.items[i].rowid;
                    bt_tsv_rows_free(&parsed);
                    bt_corpus_free(out);
                    BT_TRACE_FAIL("corpus.load", "alg rowid %zu failed in %s", row.rowid, path);
                    return -1;
                }
                row.position = text_dup(parsed.items[i].row.position);
                row.alg_text = text_dup(parsed.items[i].row.alg);
                row.commutator = text_dup(parsed.items[i].row.commutator);
                row.recon_link = text_dup(parsed.items[i].row.recon_link);
                row.youtube_link = text_dup(parsed.items[i].row.youtube_link);
                if (row.position == NULL || row.alg_text == NULL || row.commutator == NULL ||
                    row.recon_link == NULL || row.youtube_link == NULL) {
                    free(row.position);
                    free(row.alg_text);
                    free(row.commutator);
                    free(row.recon_link);
                    free(row.youtube_link);
                    bt_alg_free(&row.alg);
                    free(text);
                    bt_tsv_rows_free(&parsed);
                    bt_corpus_free(out);
                    BT_TRACE_FAIL("corpus.load", "alloc failed in %s", path);
                    return -1;
                }
                if (rows_push(out, row) != 0) {
                    free(row.position);
                    free(row.alg_text);
                    free(row.commutator);
                    free(row.recon_link);
                    free(row.youtube_link);
                    bt_alg_free(&row.alg);
                    free(text);
                    bt_tsv_rows_free(&parsed);
                    bt_corpus_free(out);
                    BT_TRACE_FAIL("corpus.load", "row push failed in %s", path);
                    return -1;
                }
            }
            bt_tsv_rows_free(&parsed);
            free(text);
        }
    }
    BT_TRACE_EXIT("corpus.load");
    return 0;
}

size_t bt_corpus_total_rows(const bt_corpus *corpus) {
    return corpus->count;
}

bool bt_corpus_is_empty(const bt_corpus *corpus) {
    return corpus->count == 0;
}

const bt_corpus_row *bt_corpus_row_at(const bt_corpus *corpus, bt_solver solver,
                                      bt_sheet_class sheet, size_t rowid) {
    for (size_t i = 0; i < corpus->count; i++) {
        const bt_corpus_row *row = &corpus->rows[i];
        if (row->solver == solver && row->sheet == sheet && row->rowid == rowid) {
            return row;
        }
    }
    return NULL;
}

size_t bt_corpus_sheet_rows(const bt_corpus *corpus, bt_solver solver, bt_sheet_class sheet,
                            size_t *out, size_t cap) {
    size_t count = 0;
    for (size_t i = 0; i < corpus->count; i++) {
        const bt_corpus_row *row = &corpus->rows[i];
        if (row->solver == solver && row->sheet == sheet) {
            if (count < cap) {
                out[count] = i;
            }
            count += 1;
        }
    }
    return count;
}

size_t bt_corpus_rows_for_position(const bt_corpus *corpus, bt_sheet_class sheet,
                                   const char *position, size_t *out, size_t cap) {
    size_t count = 0;
    for (size_t i = 0; i < corpus->count; i++) {
        const bt_corpus_row *row = &corpus->rows[i];
        if (row->sheet == sheet && strcmp(row->position, position) == 0) {
            if (count < cap) {
                out[count] = i;
            }
            count += 1;
        }
    }
    return count;
}

size_t bt_corpus_distinct_positions(const bt_corpus *corpus, bt_solver solver,
                                    bt_sheet_class sheet) {
    size_t distinct = 0;
    for (size_t i = 0; i < corpus->count; i++) {
        const bt_corpus_row *row = &corpus->rows[i];
        if (row->solver != solver || row->sheet != sheet) {
            continue;
        }
        bool seen = false;
        for (size_t j = 0; j < i; j++) {
            const bt_corpus_row *other = &corpus->rows[j];
            if (other->solver == solver && other->sheet == sheet &&
                strcmp(other->position, row->position) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            distinct += 1;
        }
    }
    return distinct;
}

static bt_corpus_row row_clone(const bt_corpus_row *src) {
    bt_corpus_row out;
    out.solver = src->solver;
    out.sheet = src->sheet;
    out.rowid = src->rowid;
    out.position = text_dup(src->position);
    out.alg_text = text_dup(src->alg_text);
    out.commutator = text_dup(src->commutator);
    out.recon_link = text_dup(src->recon_link);
    out.youtube_link = text_dup(src->youtube_link);
    bt_alg_init(&out.alg);
    for (size_t i = 0; i < bt_alg_len(&src->alg); i++) {
        bt_alg_push(&out.alg, src->alg.items[i]);
    }
    return out;
}

int bt_corpus_with_overlay(const bt_corpus *corpus, const bt_user_algs *overlay, bt_corpus *out,
                           bt_overlay_error *err) {
    BT_TRACE_ENTER("corpus.with_overlay");
    bt_corpus_init(out);
    for (size_t i = 0; i < corpus->count; i++) {
        bt_corpus_row copy = row_clone(&corpus->rows[i]);
        if (copy.position == NULL || copy.alg_text == NULL || copy.commutator == NULL ||
            copy.recon_link == NULL || copy.youtube_link == NULL) {
            bt_corpus_free(out);
            BT_TRACE_FAIL("corpus.with_overlay", "clone failed");
            return -1;
        }
        if (rows_push(out, copy) != 0) {
            bt_corpus_free(out);
            BT_TRACE_FAIL("corpus.with_overlay", "push failed");
            return -1;
        }
    }
    size_t user_rowids[BT_SHEET_CLASS_COUNT] = {0};
    for (size_t i = 0; i < overlay->count; i++) {
        const char *key = overlay->items[i].key;
        const char *slash = strchr(key, '/');
        bt_sheet_class sheet;
        if (slash == NULL || !sheet_class_parse_n(key, (size_t)(slash - key), &sheet)) {
            err->kind = BT_OVERLAY_ERR_UNKNOWN_SHEET;
            snprintf(err->key, sizeof err->key, "%s", key);
            bt_corpus_free(out);
            BT_TRACE_FAIL("corpus.with_overlay", "unknown sheet in %s", key);
            return -1;
        }
        const char *position = slash + 1;
        bt_corpus_row row;
        memset(&row, 0, sizeof row);
        row.solver = BT_SOLVER_USER;
        row.sheet = sheet;
        bt_alg_init(&row.alg);
        if (!bt_alg_parse(overlay->items[i].alg, &row.alg, err->alg_error, sizeof err->alg_error)) {
            err->kind = BT_OVERLAY_ERR_BAD_ALG;
            snprintf(err->position, sizeof err->position, "%s", position);
            bt_corpus_free(out);
            BT_TRACE_FAIL("corpus.with_overlay", "bad alg for %s", position);
            return -1;
        }
        user_rowids[sheet] += 1;
        row.rowid = user_rowids[sheet];
        row.position = text_dup(position);
        row.alg_text = text_dup(overlay->items[i].alg);
        row.commutator = text_dup("");
        row.recon_link = text_dup("");
        row.youtube_link = text_dup("");
        if (row.position == NULL || row.alg_text == NULL || row.commutator == NULL ||
            row.recon_link == NULL || row.youtube_link == NULL) {
            free(row.position);
            free(row.alg_text);
            free(row.commutator);
            free(row.recon_link);
            free(row.youtube_link);
            bt_alg_free(&row.alg);
            bt_corpus_free(out);
            BT_TRACE_FAIL("corpus.with_overlay", "alloc failed for %s", position);
            return -1;
        }
        if (rows_push(out, row) != 0) {
            free(row.position);
            free(row.alg_text);
            free(row.commutator);
            free(row.recon_link);
            free(row.youtube_link);
            bt_alg_free(&row.alg);
            bt_corpus_free(out);
            BT_TRACE_FAIL("corpus.with_overlay", "push failed for %s", position);
            return -1;
        }
    }
    BT_TRACE_EXIT("corpus.with_overlay");
    return 0;
}
