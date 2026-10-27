#ifndef VERIFY_WHEN_H
#define VERIFY_WHEN_H

#include <stddef.h>
#include <stdint.h>

#define VERIFY_ISO8601_LEN 21

void verify_when_iso8601(uint64_t unix_secs, char *out, size_t out_len);
void verify_when_now(char *out, size_t out_len);

#endif
