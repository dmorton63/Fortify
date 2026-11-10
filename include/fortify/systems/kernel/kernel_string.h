#ifndef FORTIFY_KERNEL_STRING_H
#define FORTIFY_KERNEL_STRING_H

#include <stdint.h>

void kernel_format_hex64(char *out, uint64_t value);
bool kernel_string_equals(const char *lhs, const char *rhs);
bool kernel_string_starts_with(const char *text, const char *prefix);
bool kernel_parse_u64_decimal(const char *text, uint64_t *out_value);
bool kernel_parse_u8_decimal(const char *text, uint8_t *out_value);
bool kernel_parse_token(const char *text, uint64_t start, char *out, uint64_t out_size, uint64_t *out_next);

#endif
