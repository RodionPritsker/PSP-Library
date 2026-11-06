#include "psplibrary/text.h"

#include "psplibrary/gfx.h"
#include "psplibrary/text_layout.h"

#include <malloc.h>
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PSPL_FONT_ATLAS_PATH "ms0:/PSP/GAME/PSPLIBRARY/assets/fonts/pspl_noto_sans_sdf.t8"
#define PSPL_FONT_META_PATH  "ms0:/PSP/GAME/PSPLIBRARY/assets/fonts/pspl_noto_sans_sdf.meta"
#define PSPL_FONT_MAGIC "T8S1"
#define PSPL_FONT_MAX_CODEPOINT 65536U

typedef struct {
    uint32_t codepoint;
    float u;
    float v;
    float width;
    float height;
    float x_offset;
    float y_offset;
    float advance;
} pspl_font_glyph_t;

typedef struct {
    unsigned char *atlas;
    uint32_t atlas_width;
    uint32_t atlas_height;
    uint16_t *lookup;
    pspl_font_glyph_t *glyphs;
    uint32_t glyph_count;
    float line_height;
    float ascender;
    float base_size;
    int loaded;
} pspl_font_t;

static pspl_font_t g_font;
static uint32_t g_clut[256] __attribute__((aligned(16)));
static int g_clut_ready;

static void free_font(void)
{
    free(g_font.atlas);
    free(g_font.lookup);
    free(g_font.glyphs);
    memset(&g_font, 0, sizeof(g_font));
}

static int read_file(const char *path, void *buffer, size_t size)
{
    FILE *file = fopen(path, "rb");
    int ok = -1;
    if (file != NULL) {
        ok = fread(buffer, 1, size, file) == size ? 0 : -1;
        fclose(file);
    }
    return ok;
}

int pspl_text_init(void)
{
    FILE *file;
    char magic[4];
    size_t atlas_size;
    if (g_font.loaded) return 0;
    file = fopen(PSPL_FONT_META_PATH, "rb");
    if (file == NULL) return -1;
    memset(&g_font, 0, sizeof(g_font));
    if (fread(magic, 1, sizeof(magic), file) != sizeof(magic) ||
        memcmp(magic, PSPL_FONT_MAGIC, sizeof(magic)) != 0 ||
        fread(&g_font.atlas_width, sizeof(g_font.atlas_width), 1, file) != 1 ||
        fread(&g_font.atlas_height, sizeof(g_font.atlas_height), 1, file) != 1 ||
        fread(&g_font.line_height, sizeof(g_font.line_height), 1, file) != 1 ||
        fread(&g_font.ascender, sizeof(g_font.ascender), 1, file) != 1 ||
        fread(&g_font.base_size, sizeof(g_font.base_size), 1, file) != 1 ||
        fread(&g_font.glyph_count, sizeof(g_font.glyph_count), 1, file) != 1 ||
        g_font.atlas_width == 0 || g_font.atlas_height == 0 ||
        g_font.glyph_count == 0 || g_font.glyph_count > 4096U) {
        fclose(file);
        free_font();
        return -1;
    }
    g_font.lookup = (uint16_t *)memalign(16, sizeof(*g_font.lookup) * PSPL_FONT_MAX_CODEPOINT);
    g_font.glyphs = (pspl_font_glyph_t *)memalign(16, sizeof(*g_font.glyphs) * g_font.glyph_count);
    if (g_font.lookup == NULL || g_font.glyphs == NULL ||
        fread(g_font.lookup, sizeof(*g_font.lookup), PSPL_FONT_MAX_CODEPOINT, file) !=
            PSPL_FONT_MAX_CODEPOINT ||
        fread(g_font.glyphs, sizeof(*g_font.glyphs), g_font.glyph_count, file) !=
            g_font.glyph_count) {
        fclose(file);
        free_font();
        return -1;
    }
    fclose(file);
    atlas_size = (size_t)g_font.atlas_width * (size_t)g_font.atlas_height;
    g_font.atlas = (unsigned char *)memalign(16, atlas_size);
    if (g_font.atlas == NULL || read_file(PSPL_FONT_ATLAS_PATH, g_font.atlas, atlas_size) != 0) {
        free_font();
        return -1;
    }
    g_font.loaded = 1;
    sceKernelDcacheWritebackAll();
    return 0;
}

void pspl_text_shutdown(void)
{
    free_font();
    g_clut_ready = 0;
}

int pspl_text_atlas_loaded(void)
{
    return g_font.loaded;
}

static void build_clut(void)
{
    unsigned int i;
    if (g_clut_ready) return;
    for (i = 0; i < 256U; ++i) {
        g_clut[i] = ((uint32_t)i << 24) | 0x00FFFFFFU;
    }
    g_clut_ready = 1;
}

static float font_size(int scale)
{
    if (scale < 1) scale = 1;
    return 12.0f + (float)(scale - 1) * 6.0f;
}

float pspl_text_line_height(int scale)
{
    if (g_font.loaded) return g_font.line_height * font_size(scale);
    return 8.0f * (float)(scale < 1 ? 1 : scale);
}

static const pspl_font_glyph_t *font_glyph(uint32_t codepoint)
{
    uint16_t index;
    if (!g_font.loaded || codepoint >= PSPL_FONT_MAX_CODEPOINT) return NULL;
    index = g_font.lookup[codepoint];
    if (index == 0U) index = g_font.lookup[(unsigned int)'?'];
    if (index == 0U || (uint32_t)(index - 1U) >= g_font.glyph_count) return NULL;
    return &g_font.glyphs[index - 1U];
}

static float glyph_advance(uint32_t codepoint, int scale)
{
    const pspl_font_glyph_t *glyph = font_glyph(codepoint);
    if (glyph != NULL) return glyph->advance * font_size(scale);
    return 7.0f * (float)(scale < 1 ? 1 : scale);
}

static float range_width(const char *text, size_t start, size_t end, int scale)
{
    size_t index = start;
    uint32_t codepoint;
    float width = 0.0f;
    while (text != NULL && index < end && text[index] != '\0') {
        index = pspl_utf8_next(text, index, &codepoint);
        width += glyph_advance(codepoint, scale);
    }
    return width;
}

size_t pspl_text_width(const char *text, int scale)
{
    if (text == NULL || scale < 1) return 0;
    return (size_t)(range_width(text, 0, strlen(text), scale) + 0.5f);
}

static void draw_fallback(float x, float y, uint32_t codepoint, int scale, uint32_t color)
{
    if (codepoint != ' ')
        pspl_gfx_rect(x, y + 1.0f, 5.0f * scale, 5.0f * scale, color, (float)scale);
}

static void draw_range(float x, float y, const char *text, size_t start, size_t end,
                       int scale, uint32_t color)
{
    size_t index = start;
    uint32_t codepoint;
    float cursor = x;
    float size = font_size(scale);
    while (text != NULL && index < end && text[index] != '\0') {
        size_t next = pspl_utf8_next(text, index, &codepoint);
        const pspl_font_glyph_t *glyph = font_glyph(codepoint);
        if (g_font.loaded && glyph != NULL && glyph->width > 0.0f && glyph->height > 0.0f) {
            float glyph_x = cursor + glyph->x_offset * size;
            float glyph_y = y + (glyph->y_offset - g_font.ascender) * size;
            pspl_gfx_draw_t8(g_font.atlas, (int)g_font.atlas_width, (int)g_font.atlas_height,
                             glyph->u * g_font.atlas_width, glyph->v * g_font.atlas_height,
                             glyph->width * g_font.atlas_width, glyph->height * g_font.atlas_height,
                             glyph_x, glyph_y, glyph->width * size, glyph->height * size,
                             color, g_clut);
        } else if (!g_font.loaded) {
            draw_fallback(cursor, y, codepoint, scale, color);
        }
        cursor += glyph_advance(codepoint, scale);
        index = next;
    }
}

static void draw_ellipsis(float x, float y, int scale, uint32_t color)
{
    static const char dots[] = "...";
    draw_range(x, y, dots, 0, sizeof(dots) - 1U, scale, color);
}

static void draw_clipped_range(float x, float y, float width, const char *text,
                               size_t start, size_t end, int scale, uint32_t color)
{
    static const char ellipsis[] = "...";
    float ellipsis_width;
    float available;
    size_t index;
    if (text == NULL || width <= 0.0f || start >= end) return;
    if (range_width(text, start, end, scale) <= width) {
        draw_range(x, y, text, start, end, scale, color);
        return;
    }
    ellipsis_width = range_width(ellipsis, 0, sizeof(ellipsis) - 1U, scale);
    if (ellipsis_width >= width) {
        draw_ellipsis(x, y, scale, color);
        return;
    }
    available = width - ellipsis_width;
    index = start;
    while (index < end) {
        size_t next = pspl_utf8_next(text, index, NULL);
        if (range_width(text, start, next, scale) > available) break;
        index = next;
    }
    draw_range(x, y, text, start, index, scale, color);
    draw_ellipsis(x + available, y, scale, color);
}

void pspl_text_draw(float x, float y, const char *text, int scale, uint32_t color)
{
    if (text == NULL || scale < 1) return;
    build_clut();
    draw_range(x, y, text, 0, strlen(text), scale, color);
}

void pspl_text_draw_clipped(float x, float y, float width, const char *text,
                            int scale, uint32_t color)
{
    if (text == NULL || scale < 1 || width <= 0.0f) return;
    build_clut();
    draw_clipped_range(x, y, width, text, 0, strlen(text), scale, color);
}

void pspl_text_draw_centered(float x, float y, float width, const char *text,
                             int scale, uint32_t color)
{
    size_t text_width;
    float offset;
    if (text == NULL || width <= 0.0f) return;
    text_width = pspl_text_width(text, scale);
    offset = width > (float)text_width ? (width - (float)text_width) * 0.5f : 0.0f;
    pspl_text_draw_clipped(x + offset, y, width - offset, text, scale, color);
}

void pspl_text_draw_wrapped(float x, float y, float width, const char *text,
                            int scale, int max_lines, uint32_t color)
{
    size_t start = 0;
    int line;
    if (text == NULL || width <= 0.0f || scale < 1 || max_lines < 1) return;
    build_clut();
    for (line = 0; line < max_lines && text[start] != '\0'; ++line) {
        size_t line_start = start;
        size_t index = start;
        size_t end = start;
        size_t next_start = start;
        size_t last_space_start = start;
        size_t last_space_end = start;
        float measured = 0.0f;
        int more = 0;
        while (text[index] != '\0' && text[index] != '\n') {
            size_t next = pspl_utf8_next(text, index, NULL);
            float advance = range_width(text, index, next, scale);
            if (end != line_start && measured + advance > width) {
                if (last_space_end > line_start) {
                    end = last_space_start;
                    next_start = last_space_end;
                } else {
                    end = index;
                    next_start = index;
                }
                more = 1;
                break;
            }
            if (text[index] == ' ') {
                last_space_start = index;
                last_space_end = next;
            }
            measured += advance;
            end = next;
            index = next;
        }
        if (!more) {
            end = index;
            next_start = text[index] == '\n' ? index + 1U : index;
            more = text[next_start] != '\0';
        }
        if (end == line_start && text[line_start] != '\0') {
            end = pspl_utf8_next(text, line_start, NULL);
            next_start = end;
            more = text[next_start] != '\0';
        }
        if (line == max_lines - 1 && more)
            draw_clipped_range(x, y + line * pspl_text_line_height(scale), width,
                               text, line_start, end, scale, color);
        else
            draw_range(x, y + line * pspl_text_line_height(scale), text,
                       line_start, end, scale, color);
        start = next_start;
    }
}
