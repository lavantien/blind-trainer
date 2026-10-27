#ifndef POLICY_FSUTIL_H
#define POLICY_FSUTIL_H

#include <stddef.h>

#include "common/paths.h"
#include "policy/lib.h"

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} policy_strvec;

void policy_strvec_push(policy_strvec *vec, char *item);
void policy_strvec_sort(policy_strvec *vec);
void policy_strvec_free(policy_strvec *vec);
int policy_path_is_dir(const char *path);
int policy_path_is_file(const char *path);
int policy_path_is_reparse_or_link(const char *path);
int policy_join_checked(char out[BT_PATH_MAX], const char *base, const char *leaf, char *err,
                        size_t err_len);
int policy_list_dir(const char *dir, policy_strvec *names, char *err, size_t err_len);
void policy_rm_rf(const char *path);

#endif
