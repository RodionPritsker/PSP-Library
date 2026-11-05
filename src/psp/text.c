#include "psplibrary/text.h"

#include "psplibrary/gfx.h"

#include <string.h>

static const unsigned char *glyph(char character)
{
    static const unsigned char blank[5] = {0, 0, 0, 0, 0};
    static const unsigned char question[5] = {0x0E, 0x11, 0x02, 0x04, 0x04};
    static const unsigned char digits[10][5] = {
        {0x0E,0x11,0x13,0x15,0x0E}, {0x04,0x0C,0x04,0x04,0x0E},
        {0x0E,0x11,0x02,0x04,0x1F}, {0x1E,0x01,0x06,0x01,0x1E},
        {0x02,0x06,0x0A,0x1F,0x02}, {0x1F,0x10,0x1E,0x01,0x1E},
        {0x06,0x08,0x1E,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08},
        {0x0E,0x11,0x0E,0x11,0x0E}, {0x0E,0x11,0x0F,0x01,0x0E}
    };
    static const unsigned char letters[26][5] = {
        {0x0E,0x11,0x1F,0x11,0x11}, {0x1E,0x11,0x1E,0x11,0x1E},
        {0x0F,0x10,0x10,0x10,0x0F}, {0x1E,0x11,0x11,0x11,0x1E},
        {0x1F,0x10,0x1E,0x10,0x1F}, {0x1F,0x10,0x1E,0x10,0x10},
        {0x0F,0x10,0x17,0x11,0x0F}, {0x11,0x11,0x1F,0x11,0x11},
        {0x0E,0x04,0x04,0x04,0x0E}, {0x07,0x02,0x02,0x12,0x0C},
        {0x11,0x12,0x1C,0x12,0x11}, {0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x11,0x11}, {0x11,0x19,0x15,0x13,0x11},
        {0x0E,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x1E,0x10,0x10},
        {0x0E,0x11,0x11,0x15,0x0E}, {0x1E,0x11,0x1E,0x14,0x12},
        {0x0F,0x10,0x0E,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x0A,0x04},
        {0x11,0x11,0x15,0x1B,0x11}, {0x11,0x0A,0x04,0x0A,0x11},
        {0x11,0x0A,0x04,0x04,0x04}, {0x1F,0x02,0x04,0x08,0x1F}
    };
    static const unsigned char punctuation[16][5] = {
        {0,0,0,0,0}, {0x04,0x04,0x04,0,0x04}, {0x0A,0x0A,0,0,0},
        {0x0A,0x1F,0x0A,0x1F,0x0A}, {0x04,0x0F,0x14,0x0F,0x04},
        {0x19,0x02,0x04,0x08,0x13}, {0x0C,0x12,0x0C,0x12,0x0D},
        {0x04,0x04,0,0,0}, {0x02,0x04,0x04,0x04,0x02},
        {0x08,0x04,0x04,0x04,0x08}, {0,0x04,0x15,0x0E,0x04},
        {0,0x04,0x0E,0x04,0}, {0,0,0,0x04,0x08}, {0,0,0x1F,0,0},
        {0,0,0,0,0x04}, {0x02,0x04,0x08,0x10,0}
    };
    if (character >= 'a' && character <= 'z') character = (char)(character - 'a' + 'A');
    if (character >= 'A' && character <= 'Z') return letters[character - 'A'];
    if (character >= '0' && character <= '9') return digits[character - '0'];
    switch (character) {
    case ' ': return blank;
    case '!': return punctuation[1];
    case '"': return punctuation[2];
    case '#': return punctuation[3];
    case '$': return punctuation[4];
    case '%': return punctuation[5];
    case '&': return punctuation[6];
    case '\'': return punctuation[7];
    case '(': return punctuation[8];
    case ')': return punctuation[9];
    case '+': return punctuation[10];
    case ',': return punctuation[11];
    case '-': return punctuation[12];
    case '.': return punctuation[13];
    case '/': return punctuation[15];
    case ':': return punctuation[14];
    case '=': return punctuation[13];
    default: return question;
    }
}

static size_t next_character(const char *text, size_t index, char *out)
{
    unsigned char byte = (unsigned char)text[index];
    if (byte == '\0') return index;
    if (byte < 0x80U) {
        *out = (char)byte;
        return index + 1;
    }
    *out = '?';
    if ((byte & 0xE0U) == 0xC0U) return index + 2;
    if ((byte & 0xF0U) == 0xE0U) return index + 3;
    if ((byte & 0xF8U) == 0xF0U) return index + 4;
    return index + 1;
}

size_t pspl_text_width(const char *text, int scale)
{
    size_t index = 0;
    size_t count = 0;
    char character;
    if (text == NULL || scale < 1) return 0;
    while (text[index] != '\0') {
        index = next_character(text, index, &character);
        ++count;
    }
    return count == 0 ? 0 : count * (size_t)(6 * scale) - (size_t)scale;
}

static void draw_character(float x, float y, char character, int scale, uint32_t color)
{
    const unsigned char *shape = glyph(character);
    int column;
    int row;
    for (row = 0; row < 5; ++row) {
        for (column = 0; column < 5; ++column) {
            if ((shape[row] & (unsigned char)(1U << (4 - column))) != 0U)
                pspl_gfx_fill_rect(x + (float)(column * scale), y + (float)(row * scale),
                                   (float)scale, (float)scale, color);
        }
    }
}

void pspl_text_draw(float x, float y, const char *text, int scale, uint32_t color)
{
    size_t index = 0;
    char character;
    if (text == NULL || scale < 1) return;
    while (text[index] != '\0') {
        index = next_character(text, index, &character);
        draw_character(x, y, character, scale, color);
        x += (float)(6 * scale);
    }
}

void pspl_text_draw_clipped(float x, float y, float width, const char *text,
                            int scale, uint32_t color)
{
    size_t index = 0;
    size_t last_index = 0;
    size_t characters = 0;
    char character;
    float cursor = x;
    if (text == NULL || scale < 1 || width <= 0.0f) return;
    while (text[index] != '\0' && cursor - x + (float)(5 * scale) <= width) {
        last_index = index;
        index = next_character(text, index, &character);
        draw_character(cursor, y, character, scale, color);
        cursor += (float)(6 * scale);
        ++characters;
    }
    if (text[index] != '\0' && width >= (float)(17 * scale)) {
        size_t limit = (size_t)((width / (float)(6 * scale)) - 2.0f);
        (void)last_index;
        (void)characters;
        (void)limit;
        draw_character(x + width - (float)(17 * scale), y, '.', scale, color);
        draw_character(x + width - (float)(11 * scale), y, '.', scale, color);
        draw_character(x + width - (float)(5 * scale), y, '.', scale, color);
    }
}

void pspl_text_draw_centered(float x, float y, float width, const char *text,
                             int scale, uint32_t color)
{
    size_t text_width = pspl_text_width(text, scale);
    float offset = width > (float)text_width ? (width - (float)text_width) * 0.5f : 0.0f;
    pspl_text_draw_clipped(x + offset, y, width - offset, text, scale, color);
}
