#include "fortify/systems/kernel/kernel_string.h"

void kernel_format_hex64(char *out, uint64_t value) {
    static const char kHex[] = "0123456789abcdef";
    out[0] = '0';
    out[1] = 'x';
    for (uint64_t i = 0; i < 16; i++) {
        out[17 - i] = kHex[value & 0x0f];
        value >>= 4;
    }
    out[18] = '\0';
}

bool kernel_string_equals(const char *lhs, const char *rhs) {
    if (lhs == 0 || rhs == 0) {
        return false;
    }
    uint64_t i = 0;
    while (lhs[i] != '\0' && rhs[i] != '\0') {
        if (lhs[i] != rhs[i]) {
            return false;
        }
        i++;
    }
    return lhs[i] == '\0' && rhs[i] == '\0';
}

bool kernel_string_starts_with(const char *text, const char *prefix) {
    if (text == 0 || prefix == 0) {
        return false;
    }
    uint64_t i = 0;
    while (prefix[i] != '\0') {
        if (text[i] == '\0' || text[i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return true;
}

bool kernel_parse_u64_decimal(const char *text, uint64_t *out_value) {
    if (text == 0 || out_value == 0 || text[0] == '\0') {
        return false;
    }
    uint64_t value = 0;
    for (uint64_t i = 0; text[i] != '\0'; i++) {
        const char ch = text[i];
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = (value * 10) + static_cast<uint64_t>(ch - '0');
    }
    *out_value = value;
    return true;
}

bool kernel_parse_u8_decimal(const char *text, uint8_t *out_value) {
    uint64_t value = 0;
    if (!kernel_parse_u64_decimal(text, &value) || value > 0xffULL) {
        return false;
    }
    *out_value = static_cast<uint8_t>(value);
    return true;
}

bool kernel_parse_token(const char *text, uint64_t start, char *out, uint64_t out_size, uint64_t *out_next) {
    if (text == 0 || out == 0 || out_size < 2) {
        return false;
    }
    uint64_t i = start;
    while (text[i] == ' ') {
        i++;
    }
    if (text[i] == '\0') {
        return false;
    }
    uint64_t out_index = 0;
    while (text[i] != '\0' && text[i] != ' ') {
        if (out_index + 1 >= out_size) {
            return false;
        }
        out[out_index++] = text[i++];
    }
    out[out_index] = '\0';
    if (out_next != 0) {
        *out_next = i;
    }
    return true;
}
