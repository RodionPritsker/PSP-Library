#include "psplibrary/gfx.h"

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <string.h>

#define PSPL_GFX_LIST_WORDS 262144U

/*
 * GU keeps command pointers and the temporary vertex allocations in the
 * active list.  The UI deliberately emits many small sprites for text, so a
 * 0x4000-word list is not enough once a frame is redrawn several times.
 * Match the size used by the PSPDEV GU samples and keep both lists aligned.
 */
static unsigned int g_list[2][PSPL_GFX_LIST_WORDS] __attribute__((aligned(16)));
static pspl_gfx_t *g_active;

typedef struct {
    uint32_t color;
    float x;
    float y;
    float z;
} pspl_vertex_t;

typedef struct {
    float u;
    float v;
    uint32_t color;
    float x;
    float y;
    float z;
} pspl_texture_vertex_t;

static int setup_2d(void)
{
    if (sceGuStart(GU_DIRECT, g_list[0]) < 0) return -1;
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, PSPL_GFX_STRIDE);
    sceGuDispBuffer(PSPL_SCREEN_WIDTH, PSPL_SCREEN_HEIGHT,
                    (void *)(PSPL_GFX_STRIDE * PSPL_SCREEN_HEIGHT), PSPL_GFX_STRIDE);
    sceGuDepthBuffer((void *)(PSPL_GFX_STRIDE * PSPL_SCREEN_HEIGHT * 2), PSPL_GFX_STRIDE);
    sceGuOffset(2048 - PSPL_SCREEN_WIDTH / 2, 2048 - PSPL_SCREEN_HEIGHT / 2);
    sceGuViewport(2048, 2048, PSPL_SCREEN_WIDTH, PSPL_SCREEN_HEIGHT);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, PSPL_SCREEN_WIDTH, PSPL_SCREEN_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);
    sceKernelDcacheWritebackAll();
    return 0;
}

int pspl_gfx_init(pspl_gfx_t *gfx)
{
    if (gfx == NULL) return -1;
    memset(gfx, 0, sizeof(*gfx));
    gfx->width = PSPL_SCREEN_WIDTH;
    gfx->height = PSPL_SCREEN_HEIGHT;
    if (sceGuInit() < 0) return -1;
    if (setup_2d() != 0) {
        sceGuTerm();
        return -1;
    }
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    gfx->initialized = 1;
    g_active = gfx;
    return 0;
}

void pspl_gfx_shutdown(pspl_gfx_t *gfx)
{
    if (gfx == NULL || !gfx->initialized) return;
    sceGuSync(0, 0);
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    gfx->initialized = 0;
    if (g_active == gfx) g_active = NULL;
}

int pspl_gfx_begin(pspl_gfx_t *gfx)
{
    if (gfx == NULL || !gfx->initialized) return -1;
    g_active = gfx;
    if (sceGuStart(GU_DIRECT, g_list[gfx->frame & 1U]) < 0) return -1;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    return 0;
}

void pspl_gfx_end(pspl_gfx_t *gfx)
{
    if (gfx == NULL || !gfx->initialized) return;
    sceGuFinish();
    /* Flush command data, temporary vertices, and RAM-backed textures. */
    sceKernelDcacheWritebackAll();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    gfx->frame++;
}

void pspl_gfx_clear(uint32_t color)
{
    sceGuClearColor(color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void pspl_gfx_fill_rect(float x, float y, float width, float height, uint32_t color)
{
    pspl_vertex_t *vertices = (pspl_vertex_t *)sceGuGetMemory(sizeof(*vertices) * 2U);
    if (vertices == NULL) return;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(color);
    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[1].color = color;
    vertices[1].x = x + width;
    vertices[1].y = y + height;
    vertices[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   2, NULL, vertices);
}

void pspl_gfx_rect(float x, float y, float width, float height, uint32_t color, float thickness)
{
    pspl_gfx_fill_rect(x, y, width, thickness, color);
    pspl_gfx_fill_rect(x, y + height - thickness, width, thickness, color);
    pspl_gfx_fill_rect(x, y + thickness, thickness, height - thickness * 2.0f, color);
    pspl_gfx_fill_rect(x + width - thickness, y + thickness, thickness,
                       height - thickness * 2.0f, color);
}

void pspl_gfx_gradient_rect(float x, float y, float width, float height,
                            uint32_t top_color, uint32_t bottom_color)
{
    pspl_vertex_t *vertices = (pspl_vertex_t *)sceGuGetMemory(sizeof(*vertices) * 2U);
    if (vertices == NULL) return;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_SMOOTH);
    vertices[0].color = top_color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[1].color = bottom_color;
    vertices[1].x = x + width;
    vertices[1].y = y + height;
    vertices[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   2, NULL, vertices);
}

void pspl_gfx_draw_texture(const void *pixels, int texture_width, int texture_height,
                           int image_width, int image_height,
                           float x, float y, float width, float height, uint32_t color)
{
    pspl_texture_vertex_t *vertices;
    if (pixels == NULL || texture_width <= 0 || texture_height <= 0) return;
    vertices = (pspl_texture_vertex_t *)sceGuGetMemory(sizeof(*vertices) * 2U);
    if (vertices == NULL) return;
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(0, texture_width, texture_height, texture_width, pixels);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    vertices[0].u = 0.0f;
    vertices[0].v = 0.0f;
    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0.0f;
    vertices[1].u = (float)image_width;
    vertices[1].v = (float)image_height;
    vertices[1].color = color;
    vertices[1].x = x + width;
    vertices[1].y = y + height;
    vertices[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 |
                   GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, NULL, vertices);
    sceGuDisable(GU_TEXTURE_2D);
}
