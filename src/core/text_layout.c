#include "psplibrary/text_layout.h"

static int continuation(unsigned char byte)
{
    return (byte & 0xC0U) == 0x80U;
}

size_t pspl_utf8_next(const char *text, size_t index, uint32_t *codepoint)
{
    unsigned char a;
    unsigned char b;
    unsigned char c;
    unsigned char d;
    uint32_t value;
    if (codepoint != NULL) *codepoint = 0xFFFDU;
    if (text == NULL || text[index] == '\0') return index;
    a = (unsigned char)text[index];
    if (a < 0x80U) {
        if (codepoint != NULL) *codepoint = a;
        return index + 1U;
    }
    if (a >= 0xC2U && a <= 0xDFU) {
        b = (unsigned char)text[index + 1U];
        if (continuation(b)) {
            if (codepoint != NULL) *codepoint = ((uint32_t)(a & 0x1FU) << 6) | (b & 0x3FU);
            return index + 2U;
        }
    } else if (a >= 0xE0U && a <= 0xEFU) {
        b = (unsigned char)text[index + 1U];
        c = b == '\0' ? 0U : (unsigned char)text[index + 2U];
        if (continuation(b) && c != '\0' && continuation(c)) {
            value = ((uint32_t)(a & 0x0FU) << 12) | ((uint32_t)(b & 0x3FU) << 6) | (c & 0x3FU);
            if (value >= 0x800U && !(value >= 0xD800U && value <= 0xDFFFU)) {
                if (codepoint != NULL) *codepoint = value;
                return index + 3U;
            }
        }
    } else if (a >= 0xF0U && a <= 0xF4U) {
        b = (unsigned char)text[index + 1U];
        c = b == '\0' ? 0U : (unsigned char)text[index + 2U];
        d = c == '\0' ? 0U : (unsigned char)text[index + 3U];
        if (continuation(b) && c != '\0' && d != '\0' && continuation(c) && continuation(d)) {
            value = ((uint32_t)(a & 0x07U) << 18) | ((uint32_t)(b & 0x3FU) << 12) |
                    ((uint32_t)(c & 0x3FU) << 6) | (d & 0x3FU);
            if (value >= 0x10000U && value <= 0x10FFFFU) {
                if (codepoint != NULL) *codepoint = value;
                return index + 4U;
            }
        }
    }
    return index + 1U;
}

size_t pspl_utf8_length(const char *text)
{
    size_t index = 0;
    size_t count = 0;
    if (text == NULL) return 0;
    while (text[index] != '\0') {
        index = pspl_utf8_next(text, index, NULL);
        ++count;
    }
    return count;
}

size_t pspl_utf8_truncate(const char *text, size_t max_bytes)
{
    size_t index = 0;
    size_t next;
    if (text == NULL) return 0;
    while (text[index] != '\0') {
        next = pspl_utf8_next(text, index, NULL);
        if (next > max_bytes) break;
        index = next;
    }
    return index;
}
