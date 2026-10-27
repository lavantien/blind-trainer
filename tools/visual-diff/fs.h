#ifndef VISUAL_DIFF_FS_H
#define VISUAL_DIFF_FS_H

#include <stddef.h>
#include <stdio.h>

#include "lib.h"

int btvd_fs_is_dir(const char *path);
int btvd_fs_is_file(const char *path);
void btvd_fs_mkdir_p(const char *path);
void btvd_fs_rm_rf(const char *path);
int btvd_fs_list_names(const char *dir, btvd_strvec *names, char *err, size_t err_len);
unsigned char *btvd_fs_read_bytes(const char *path, size_t *len_out, char *err, size_t err_len);
int btvd_fs_write_bytes(const char *path, const void *data, size_t len, char *err, size_t err_len);
int btvd_fs_copy(const char *from, const char *to, char *err, size_t err_len);
int btvd_fs_remove(const char *path, char *err, size_t err_len);
FILE *btvd_fs_open(const char *path, const char *mode);
int btvd_fs_spawn_wait(const char *exe, char *const *argv, size_t argc);

#endif
