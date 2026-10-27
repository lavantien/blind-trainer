#include "data/seedpack/parse.h"

#include "common/bt_trace.h"
#include "common/json.h"
#include "common/strbuf.h"
#include "data/seedpack/hashing.h"
#include "data/seedpack/normalizer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool valid_source_at(const char *text) {
    size_t len = strlen(text);
    static const size_t digit_positions[8] = {0, 1, 2, 3, 5, 6, 8, 9};
    if (len != 10 || text[4] != '-' || text[7] != '-') {
        return false;
    }
    for (int i = 0; i < 8; i++) {
        if (!isdigit((unsigned char)text[digit_positions[i]])) {
            return false;
        }
    }
    return true;
}

static char *dup_range(const char *start, size_t len) {
    char *out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static char *trim_range(const char *start, size_t len) {
    size_t begin = 0;
    size_t end = len;
    while (begin < end && isspace((unsigned char)start[begin])) {
        begin += 1;
    }
    while (end > begin && isspace((unsigned char)start[end - 1])) {
        end -= 1;
    }
    return dup_range(start + begin, end - begin);
}

static int enrich_front_json(const char *raw, const bt_normalized *normalized, const char *file,
                             const char *case_key, char **out, bt_seed_error *err) {
    BT_TRACE_ENTER("seedpack.enrich_front_json");
    bt_json_error jerr;
    bt_json_value *front = NULL;
    if (bt_json_parse(raw, strlen(raw), &front, &jerr) != 0) {
        err->kind = BT_SEED_ERR_BAD_FRONT_JSON;
        snprintf(err->file, sizeof err->file, "%s", file);
        snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
        snprintf(err->json_error, sizeof err->json_error, "line %zu column %zu: %s", jerr.line,
                 jerr.column, jerr.message);
        BT_TRACE_FAIL("seedpack.enrich_front_json", "json parse failed for %s", case_key);
        return -1;
    }
    if (front->type != BT_JSON_OBJECT) {
        bt_json_free(front);
        err->kind = BT_SEED_ERR_FRONT_JSON_SHAPE;
        snprintf(err->file, sizeof err->file, "%s", file);
        snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
        BT_TRACE_FAIL("seedpack.enrich_front_json", "front_json is not an object for %s", case_key);
        return -1;
    }
    if (bt_json_get(front, BT_SEEDPACK_NOTES_KEY) != NULL) {
        bt_json_free(front);
        err->kind = BT_SEED_ERR_NOTE_COLLISION;
        snprintf(err->file, sizeof err->file, "%s", file);
        snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
        BT_TRACE_FAIL("seedpack.enrich_front_json", "loader_notes collision for %s", case_key);
        return -1;
    }
    if (normalized->note_count > 0) {
        bt_json_value *notes = bt_json_new_array();
        if (notes == NULL) {
            bt_json_free(front);
            err->kind = BT_SEED_ERR_BAD_FRONT_JSON;
            snprintf(err->file, sizeof err->file, "%s", file);
            snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
            snprintf(err->json_error, sizeof err->json_error, "out of memory");
            BT_TRACE_FAIL("seedpack.enrich_front_json", "alloc failed for %s", case_key);
            return -1;
        }
        for (size_t i = 0; i < normalized->note_count; i++) {
            bt_json_value *note = bt_json_new_string(normalized->notes[i]);
            if (note == NULL || bt_json_array_push(notes, note) != 0) {
                bt_json_free(note);
                bt_json_free(notes);
                bt_json_free(front);
                err->kind = BT_SEED_ERR_BAD_FRONT_JSON;
                snprintf(err->file, sizeof err->file, "%s", file);
                snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
                snprintf(err->json_error, sizeof err->json_error, "out of memory");
                BT_TRACE_FAIL("seedpack.enrich_front_json", "note push failed for %s", case_key);
                return -1;
            }
        }
        if (bt_json_object_set(front, BT_SEEDPACK_NOTES_KEY, notes) != 0) {
            bt_json_free(notes);
            bt_json_free(front);
            err->kind = BT_SEED_ERR_BAD_FRONT_JSON;
            snprintf(err->file, sizeof err->file, "%s", file);
            snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
            snprintf(err->json_error, sizeof err->json_error, "out of memory");
            BT_TRACE_FAIL("seedpack.enrich_front_json", "note set failed for %s", case_key);
            return -1;
        }
    }
    bt_strbuf serialized;
    bt_strbuf_init(&serialized, 128);
    if (bt_json_serialize(front, &serialized) != 0) {
        bt_strbuf_free(&serialized);
        bt_json_free(front);
        err->kind = BT_SEED_ERR_BAD_FRONT_JSON;
        snprintf(err->file, sizeof err->file, "%s", file);
        snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
        snprintf(err->json_error, sizeof err->json_error, "serialization failed");
        BT_TRACE_FAIL("seedpack.enrich_front_json", "serialize failed for %s", case_key);
        return -1;
    }
    bt_json_free(front);
    *out = serialized.data;
    BT_TRACE_EXIT("seedpack.enrich_front_json");
    return 0;
}

static bool blank_line(const char *line, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (!isspace((unsigned char)line[i])) {
            return false;
        }
    }
    return true;
}

int bt_seedpack_parse_pack(const bt_pack_entry *entry, const char *text, bt_seed_rows *out,
                           bt_seed_error *err) {
    BT_TRACE_ENTER("seedpack.parse_pack");
    bt_seed_rows_init(out);
    const char *file = entry->file;
    bool saw_header = false;
    size_t index = 0;
    const char *cursor = text;
    while (1) {
        const char *line = cursor;
        const char *newline = strchr(cursor, '\n');
        size_t raw_len = newline == NULL ? strlen(cursor) : (size_t)(newline - cursor);
        cursor = newline == NULL ? cursor + raw_len : newline + 1;
        size_t len = raw_len;
        if (len > 0 && line[len - 1] == '\r') {
            len -= 1;
        }
        if (!blank_line(line, len)) {
            size_t line_number = index + 1;
            if (!saw_header) {
                if (len != strlen(BT_SEEDPACK_HEADER) ||
                    strncmp(line, BT_SEEDPACK_HEADER, len) != 0) {
                    err->kind = BT_SEED_ERR_BAD_HEADER;
                    snprintf(err->file, sizeof err->file, "%s", file);
                    bt_seed_rows_free(out);
                    BT_TRACE_FAIL("seedpack.parse_pack", "bad header in %s", file);
                    return -1;
                }
                saw_header = true;
                index += 1;
                if (newline == NULL) {
                    break;
                }
                continue;
            }
            char *cells[5] = {NULL, NULL, NULL, NULL, NULL};
            const char *cell_starts[5];
            size_t cell_lens[5];
            size_t cell_count = 1;
            cell_starts[0] = line;
            cell_lens[0] = 0;
            for (size_t i = 0; i < len; i++) {
                if (line[i] == '\t') {
                    if (cell_count >= 5) {
                        cell_count += 1;
                        break;
                    }
                    cell_starts[cell_count] = line + i + 1;
                    cell_lens[cell_count] = 0;
                    cell_count += 1;
                } else if (cell_count <= 5) {
                    cell_lens[cell_count - 1] += 1;
                }
            }
            if (cell_count != 5) {
                err->kind = BT_SEED_ERR_RAGGED_ROW;
                snprintf(err->file, sizeof err->file, "%s", file);
                err->line = line_number;
                err->found = cell_count;
                bt_seed_rows_free(out);
                BT_TRACE_FAIL("seedpack.parse_pack", "line %zu holds %zu columns in %s",
                              line_number, cell_count, file);
                return -1;
            }
            static const char *columns[5] = {
                "case_key", "alg", "front_json", "source_url", "source_at",
            };
            for (int c = 0; c < 5; c++) {
                char *trimmed = trim_range(cell_starts[c], cell_lens[c]);
                if (trimmed == NULL) {
                    bt_seed_rows_free(out);
                    BT_TRACE_FAIL("seedpack.parse_pack", "alloc failed in %s", file);
                    return -1;
                }
                bool empty = trimmed[0] == '\0';
                free(trimmed);
                if (empty) {
                    err->kind = BT_SEED_ERR_EMPTY_FIELD;
                    snprintf(err->file, sizeof err->file, "%s", file);
                    err->line = line_number;
                    snprintf(err->column, sizeof err->column, "%s", columns[c]);
                    bt_seed_rows_free(out);
                    BT_TRACE_FAIL("seedpack.parse_pack", "%s empty at line %zu in %s", columns[c],
                                  line_number, file);
                    return -1;
                }
                cells[c] = dup_range(cell_starts[c], cell_lens[c]);
            }
            char *case_key = cells[0];
            char *case_key_trimmed = trim_range(case_key, strlen(case_key));
            bool padded =
                (case_key_trimmed == NULL || strcmp(case_key, case_key_trimmed) != 0) != 0;
            if (padded) {
                err->kind = BT_SEED_ERR_BAD_FIELD;
                snprintf(err->file, sizeof err->file, "%s", file);
                snprintf(err->case_key, sizeof err->case_key, "%s",
                         case_key_trimmed == NULL ? case_key : case_key_trimmed);
                snprintf(err->field, sizeof err->field, "%s", "case_key padding");
                free(case_key_trimmed);
                for (int c = 0; c < 5; c++) {
                    free(cells[c]);
                }
                bt_seed_rows_free(out);
                BT_TRACE_FAIL("seedpack.parse_pack", "padded case_key at line %zu in %s",
                              line_number, file);
                return -1;
            }
            free(case_key_trimmed);
            char *source_at = trim_range(cells[4], strlen(cells[4]));
            if (source_at == NULL) {
                for (int c = 0; c < 5; c++) {
                    free(cells[c]);
                }
                bt_seed_rows_free(out);
                BT_TRACE_FAIL("seedpack.parse_pack", "alloc failed in %s", file);
                return -1;
            }
            if (!valid_source_at(source_at)) {
                for (int c = 0; c < 5; c++) {
                    free(cells[c]);
                }
                err->kind = BT_SEED_ERR_BAD_FIELD;
                snprintf(err->file, sizeof err->file, "%s", file);
                snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
                snprintf(err->field, sizeof err->field, "%s", "source_at date");
                free(source_at);
                bt_seed_rows_free(out);
                BT_TRACE_FAIL("seedpack.parse_pack", "bad source_at at line %zu in %s", line_number,
                              file);
                return -1;
            }
            bool duplicate = false;
            for (size_t r = 0; r < out->count; r++) {
                if (strcmp(out->items[r].case_key, case_key) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                for (int c = 0; c < 5; c++) {
                    free(cells[c]);
                }
                free(source_at);
                err->kind = BT_SEED_ERR_DUPLICATE_KEY;
                snprintf(err->file, sizeof err->file, "%s", file);
                snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
                bt_seed_rows_free(out);
                BT_TRACE_FAIL("seedpack.parse_pack", "duplicate %s in %s", case_key, file);
                return -1;
            }
            bt_normalized normalized;
            bt_normalize_error nerr;
            if (!bt_seedpack_normalize_alg(entry->event, cells[1], &normalized, &nerr)) {
                for (int c = 0; c < 5; c++) {
                    free(cells[c]);
                }
                free(source_at);
                err->kind = BT_SEED_ERR_NORMALIZE;
                snprintf(err->file, sizeof err->file, "%s", file);
                snprintf(err->case_key, sizeof err->case_key, "%s", case_key);
                bt_normalize_error_string(&nerr, err->normalize, sizeof err->normalize);
                bt_seed_rows_free(out);
                BT_TRACE_FAIL("seedpack.parse_pack", "normalize failed for %s in %s", case_key,
                              file);
                return -1;
            }
            char *front_json = NULL;
            char *raw_front = trim_range(cells[2], strlen(cells[2]));
            if (raw_front == NULL) {
                bt_normalized_free(&normalized);
                for (int c = 0; c < 5; c++) {
                    free(cells[c]);
                }
                free(source_at);
                bt_seed_rows_free(out);
                BT_TRACE_FAIL("seedpack.parse_pack", "alloc failed in %s", file);
                return -1;
            }
            if (enrich_front_json(raw_front, &normalized, file, case_key, &front_json, err) != 0) {
                free(raw_front);
                bt_normalized_free(&normalized);
                for (int c = 0; c < 5; c++) {
                    free(cells[c]);
                }
                free(source_at);
                bt_seed_rows_free(out);
                return -1;
            }
            free(raw_front);
            char *source_url = trim_range(cells[3], strlen(cells[3]));
            if (source_url == NULL) {
                free(source_url);
                free(front_json);
                bt_normalized_free(&normalized);
                for (int c = 0; c < 5; c++) {
                    free(cells[c]);
                }
                free(source_at);
                bt_seed_rows_free(out);
                BT_TRACE_FAIL("seedpack.parse_pack", "alloc failed in %s", file);
                return -1;
            }
            bt_seed_row *grown = realloc(out->items, (out->count + 1) * sizeof *grown);
            if (grown == NULL) {
                free(source_url);
                free(front_json);
                bt_normalized_free(&normalized);
                for (int c = 0; c < 5; c++) {
                    free(cells[c]);
                }
                free(source_at);
                bt_seed_rows_free(out);
                BT_TRACE_FAIL("seedpack.parse_pack", "row push failed in %s", file);
                return -1;
            }
            out->items = grown;
            bt_seed_row *row = &out->items[out->count];
            snprintf(row->event, sizeof row->event, "%s", entry->event);
            snprintf(row->method, sizeof row->method, "%s", entry->method);
            snprintf(row->stage, sizeof row->stage, "%s", entry->stage);
            snprintf(row->case_key, sizeof row->case_key, "%s", case_key);
            row->alg = normalized.alg;
            row->front_json = front_json;
            row->source_url = source_url;
            row->source_at = source_at;
            row->content_hash[0] = '\0';
            out->count += 1;
            normalized.alg = NULL;
            bt_normalized_free(&normalized);
            for (int c = 0; c < 5; c++) {
                free(cells[c]);
            }
        }
        index += 1;
        if (newline == NULL) {
            break;
        }
    }
    if (!saw_header) {
        err->kind = BT_SEED_ERR_BAD_HEADER;
        snprintf(err->file, sizeof err->file, "%s", file);
        bt_seed_rows_free(out);
        BT_TRACE_FAIL("seedpack.parse_pack", "no header in %s", file);
        return -1;
    }
    if (out->count != entry->expected_rows) {
        err->kind = BT_SEED_ERR_COUNT_DRIFT;
        snprintf(err->file, sizeof err->file, "%s", file);
        err->expected = entry->expected_rows;
        err->found = out->count;
        bt_seed_rows_free(out);
        BT_TRACE_FAIL("seedpack.parse_pack", "count drift in %s: want %zu got %zu", file,
                      entry->expected_rows, out->count);
        return -1;
    }
    for (size_t i = 0; i < out->count; i++) {
        bt_seedpack_row_hash(&out->items[i], out->items[i].content_hash);
    }
    BT_TRACE_EXIT("seedpack.parse_pack");
    return 0;
}

int bt_seedpack_load_pack(const char *packs_root, const bt_pack_entry *entry, bt_seed_rows *out,
                          bt_seed_error *err) {
    BT_TRACE_ENTER("seedpack.load_pack");
    char path[BT_PATH_MAX];
    snprintf(path, sizeof path, "%s/%s", packs_root, entry->file);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        err->kind = BT_SEED_ERR_IO;
        snprintf(err->path, sizeof err->path, "%s", path);
        BT_TRACE_FAIL("seedpack.load_pack", "read %s failed", path);
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        err->kind = BT_SEED_ERR_IO;
        snprintf(err->path, sizeof err->path, "%s", path);
        BT_TRACE_FAIL("seedpack.load_pack", "alloc failed for %s", path);
        return -1;
    }
    size_t got = fread(text, 1, (size_t)size, file);
    fclose(file);
    text[got] = '\0';
    int rc = bt_seedpack_parse_pack(entry, text, out, err);
    free(text);
    if (rc != 0) {
        BT_TRACE_FAIL("seedpack.load_pack", "parse failed for %s", path);
        return -1;
    }
    BT_TRACE_EXIT("seedpack.load_pack");
    return 0;
}
