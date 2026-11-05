#ifndef PSPLIBRARY_IMAGE_H
#define PSPLIBRARY_IMAGE_H

#include "core.h"

typedef struct pspl_image_entry pspl_image_entry_t;

typedef struct {
    pspl_image_entry_t *entries;
    unsigned int frame;
    size_t capacity;
    size_t count;
} pspl_image_cache_t;

int pspl_image_cache_init(pspl_image_cache_t *cache, size_t capacity);
void pspl_image_cache_free(pspl_image_cache_t *cache);
void pspl_image_cache_begin_frame(pspl_image_cache_t *cache);
void pspl_image_draw(pspl_image_cache_t *cache, const pspl_game_t *game,
                     float x, float y, float width, float height);
void pspl_image_draw_path(pspl_image_cache_t *cache, const char *path,
                          float x, float y, float width, float height,
                          const char *fallback_label);

#endif
