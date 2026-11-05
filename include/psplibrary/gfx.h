#ifndef PSPLIBRARY_GFX_H
#define PSPLIBRARY_GFX_H

#include <stddef.h>
#include <stdint.h>

#define PSPL_SCREEN_WIDTH 480
#define PSPL_SCREEN_HEIGHT 272
#define PSPL_GFX_STRIDE 512

typedef struct {
    int width;
    int height;
    int initialized;
    unsigned int frame;
} pspl_gfx_t;

int pspl_gfx_init(pspl_gfx_t *gfx);
void pspl_gfx_shutdown(pspl_gfx_t *gfx);
void pspl_gfx_begin(pspl_gfx_t *gfx);
void pspl_gfx_end(pspl_gfx_t *gfx);
void pspl_gfx_clear(uint32_t color);
void pspl_gfx_fill_rect(float x, float y, float width, float height, uint32_t color);
void pspl_gfx_rect(float x, float y, float width, float height, uint32_t color, float thickness);
void pspl_gfx_gradient_rect(float x, float y, float width, float height,
                            uint32_t top_color, uint32_t bottom_color);
void pspl_gfx_draw_texture(const void *pixels, int texture_width, int texture_height,
                           int image_width, int image_height,
                           float x, float y, float width, float height, uint32_t color);

#endif
