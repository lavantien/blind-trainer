#ifndef BT_RV_ZIP_H
#define BT_RV_ZIP_H

#include <stddef.h>
#include <stdint.h>

#include "common/paths.h"

#define BT_RV_ERR_MAX 512

typedef struct {
    char **names;
    char **paths;
    size_t count;
} bt_rv_file_set;

void bt_rv_file_set_free(bt_rv_file_set *set);
uint32_t bt_rv_crc32(const unsigned char *data, size_t len);
int bt_rv_staging_file_set(const char *staging, const char *exe, const char *config_file,
                           const char *fonts_dir, bt_rv_file_set *set, char *err, size_t err_cap);
int bt_rv_normalize_entry(const char *name, const char *root, char *out, size_t out_cap);
int bt_rv_check_zip(const char *zip_path, const char *root, const bt_rv_file_set *set,
                    size_t *count, char *err, size_t err_cap);
int bt_rv_extract_zip(const char *zip_path, const char *dest, const char *root, size_t *count,
                      char *err, size_t err_cap);

#endif
