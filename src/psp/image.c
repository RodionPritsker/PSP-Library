#include "psplibrary/image.h"

#include <png.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psplibrary/gfx.h"
#include "psplibrary/text.h"

#define PSPL_IMAGE_MAX_WIDTH 128
#define PSPL_IMAGE_MAX_HEIGHT 176
#define PSPL_IMAGE_PATH_LEN 256

struct pspl_image_entry {
    char path[PSPL_IMAGE_PATH_LEN];
    unsigned char *pixels;
    int texture_width;
    int texture_height;
    int image_width;
    int image_height;
    unsigned int last_used;
    int failed;
};

static int next_power_of_two(int value)
{
    int result = 1;
    while (result < value && result < 4096) result <<= 1;
    return result;
}

static void placeholder(const char *label, float x, float y, float width, float height)
{
    uint32_t accent = 0xFF3B6A83U;
    int text_scale = width < 40.0f || height < 40.0f ? 1 : 2;
    if (label != NULL && strcmp(label, "PSP") == 0) accent = 0xFF4A7ED0U;
    else if (label != NULL && strcmp(label, "PS1") == 0) accent = 0xFF8D4E9AU;
    else if (label != NULL && strcmp(label, "GBA") == 0) accent = 0xFF3B9C79U;
    pspl_gfx_gradient_rect(x, y, width, height, accent, 0xFF16202BU);
    pspl_gfx_rect(x, y, width, height, 0xFF6C8CA2U, 1.0f);
    if (label != NULL)
        pspl_text_draw_centered(x, y + height * (text_scale == 1 ? 0.38f : 0.44f),
                                width, label, text_scale, 0xFFDCE8F0U);
}

static void resolve_path(const char *path, char output[PSPL_IMAGE_PATH_LEN])
{
    if (path == NULL || path[0] == '\0') {
        output[0] = '\0';
    } else if (strncmp(path, "ms0:/", 5) == 0 || strncmp(path, "ef0:/", 5) == 0) {
        snprintf(output, PSPL_IMAGE_PATH_LEN, "%s", path);
    } else {
        snprintf(output, PSPL_IMAGE_PATH_LEN, "ms0:/PSP/GAME/PSPLIBRARY/%s", path);
    }
}

static int load_png(const char *path, pspl_image_entry_t *entry)
{
    FILE *file = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    unsigned char *row = NULL;
    unsigned char *pixels = NULL;
    png_uint_32 source_width;
    png_uint_32 source_height;
    int color_type;
    int bit_depth;
    int rowbytes;
    int width;
    int height;
    int target_width;
    int target_height;
    int texture_width;
    int texture_height;
    int y;

    file = fopen(path, "rb");
    if (file == NULL) return -1;
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png == NULL ? NULL : png_create_info_struct(png);
    if (png == NULL || info == NULL) goto fail;
    if (setjmp(png_jmpbuf(png)) != 0) goto fail;
    png_init_io(png, file);
    png_read_info(png, info);
    png_get_IHDR(png, info, &source_width, &source_height, &bit_depth,
                 &color_type, NULL, NULL, NULL);
    if (source_width == 0 || source_height == 0 || source_width > 2048 || source_height > 2048)
        goto fail;
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    png_read_update_info(png, info);
    rowbytes = (int)png_get_rowbytes(png, info);
    width = (int)source_width;
    height = (int)source_height;
    target_width = width;
    target_height = height;
    if (target_width > PSPL_IMAGE_MAX_WIDTH || target_height > PSPL_IMAGE_MAX_HEIGHT) {
        float scale_x = (float)PSPL_IMAGE_MAX_WIDTH / (float)target_width;
        float scale_y = (float)PSPL_IMAGE_MAX_HEIGHT / (float)target_height;
        float scale = scale_x < scale_y ? scale_x : scale_y;
        target_width = (int)((float)target_width * scale);
        target_height = (int)((float)target_height * scale);
        if (target_width < 1) target_width = 1;
        if (target_height < 1) target_height = 1;
    }
    texture_width = next_power_of_two(target_width);
    texture_height = next_power_of_two(target_height);
    {
        size_t pixel_bytes = (size_t)texture_width * (size_t)texture_height * 4U;
        pixels = (unsigned char *)memalign(16, pixel_bytes);
        if (pixels != NULL) memset(pixels, 0, pixel_bytes);
    }
    row = (unsigned char *)malloc((size_t)rowbytes);
    if (pixels == NULL || row == NULL) goto fail;
    for (y = 0; y < height; ++y) {
        int target_y = y * target_height / height;
        int x;
        png_read_row(png, row, NULL);
        if (y * target_height / height != target_y) continue;
        for (x = 0; x < target_width; ++x) {
            int source_x = x * width / target_width;
            unsigned char *destination = pixels + ((size_t)target_y * texture_width + x) * 4U;
            const unsigned char *origin = row + source_x * 4;
            memcpy(destination, origin, 4);
        }
    }
    png_read_end(png, NULL);
    fclose(file);
    png_destroy_read_struct(&png, &info, NULL);
    free(row);
    entry->pixels = pixels;
    entry->texture_width = texture_width;
    entry->texture_height = texture_height;
    entry->image_width = target_width;
    entry->image_height = target_height;
    entry->failed = 0;
    return 0;

fail:
    if (file != NULL) fclose(file);
    if (png != NULL) png_destroy_read_struct(&png, &info, NULL);
    free(row);
    free(pixels);
    return -1;
}

int pspl_image_cache_init(pspl_image_cache_t *cache, size_t capacity)
{
    if (cache == NULL || capacity == 0) return -1;
    memset(cache, 0, sizeof(*cache));
    cache->entries = (pspl_image_entry_t *)calloc(capacity, sizeof(*cache->entries));
    if (cache->entries == NULL) return -1;
    cache->capacity = capacity;
    return 0;
}

void pspl_image_cache_free(pspl_image_cache_t *cache)
{
    size_t i;
    if (cache == NULL) return;
    for (i = 0; i < cache->capacity; ++i) free(cache->entries[i].pixels);
    free(cache->entries);
    memset(cache, 0, sizeof(*cache));
}

void pspl_image_cache_begin_frame(pspl_image_cache_t *cache)
{
    if (cache != NULL) ++cache->frame;
}

static pspl_image_entry_t *cache_entry(pspl_image_cache_t *cache, const char *path)
{
    size_t i;
    size_t free_index = cache->capacity;
    size_t oldest_index = cache->capacity;
    unsigned int oldest = (unsigned int)-1;
    char resolved[PSPL_IMAGE_PATH_LEN];
    resolve_path(path, resolved);
    if (resolved[0] == '\0') return NULL;
    for (i = 0; i < cache->capacity; ++i) {
        if (cache->entries[i].path[0] != '\0' && strcmp(cache->entries[i].path, resolved) == 0) {
            cache->entries[i].last_used = cache->frame;
            return &cache->entries[i];
        }
        if (cache->entries[i].path[0] == '\0' && free_index == cache->capacity) free_index = i;
        /* A texture used by this frame is still referenced by the GU list. */
        if (cache->entries[i].path[0] != '\0' &&
            cache->entries[i].last_used != cache->frame &&
            cache->entries[i].last_used < oldest) {
            oldest = cache->entries[i].last_used;
            oldest_index = i;
        }
    }
    if (free_index != cache->capacity) {
        i = free_index;
    } else if (oldest_index != cache->capacity) {
        i = oldest_index;
    } else {
        /* The caller can draw a placeholder without invalidating the frame. */
        return NULL;
    }
    if (cache->entries[i].path[0] == '\0') ++cache->count;
    free(cache->entries[i].pixels);
    memset(&cache->entries[i], 0, sizeof(cache->entries[i]));
    snprintf(cache->entries[i].path, sizeof(cache->entries[i].path), "%s", resolved);
    cache->entries[i].last_used = cache->frame;
    if (load_png(resolved, &cache->entries[i]) != 0) cache->entries[i].failed = 1;
    return &cache->entries[i];
}

static pspl_image_entry_t *cache_find(pspl_image_cache_t *cache, const char *path)
{
    size_t i;
    char resolved[PSPL_IMAGE_PATH_LEN];
    if (cache == NULL || path == NULL || path[0] == '\0') return NULL;
    resolve_path(path, resolved);
    if (resolved[0] == '\0') return NULL;
    for (i = 0; i < cache->capacity; ++i) {
        if (cache->entries[i].path[0] != '\0' &&
            strcmp(cache->entries[i].path, resolved) == 0) {
            cache->entries[i].last_used = cache->frame;
            return &cache->entries[i];
        }
    }
    return NULL;
}

int pspl_image_cache_load_path(pspl_image_cache_t *cache, const char *path)
{
    if (cache == NULL || path == NULL || path[0] == '\0') return 0;
    if (cache_find(cache, path) != NULL) return 0;
    return cache_entry(cache, path) != NULL ? 1 : 0;
}

void pspl_image_draw_path(pspl_image_cache_t *cache, const char *path,
                          float x, float y, float width, float height,
                          const char *fallback_label)
{
    pspl_image_entry_t *entry;
    if (cache == NULL || path == NULL || path[0] == '\0') {
        placeholder(fallback_label, x, y, width, height);
        return;
    }
    /* PNG decoding stays outside the GU command list. */
    entry = cache_find(cache, path);
    if (entry == NULL || entry->failed || entry->pixels == NULL) {
        placeholder(fallback_label, x, y, width, height);
        return;
    }
    pspl_gfx_draw_texture(entry->pixels, entry->texture_width, entry->texture_height,
                          entry->image_width, entry->image_height,
                          x, y, width, height, 0xFFFFFFFFU);
}

void pspl_image_draw(pspl_image_cache_t *cache, const pspl_game_t *game,
                     float x, float y, float width, float height)
{
    if (game == NULL) {
        placeholder("GAME", x, y, width, height);
    } else {
        pspl_image_draw_path(cache, game->cover_path, x, y, width, height, game->platform);
    }
}
