#ifndef PSPLIBRARY_THEME_H
#define PSPLIBRARY_THEME_H

#include <stdint.h>

typedef enum {
    PSPL_THEME_DARK = 0,
    PSPL_THEME_LIGHT = 1
} pspl_theme_mode_t;

typedef enum {
    PSPL_THEME_AMBER = 0,
    PSPL_THEME_OCEAN,
    PSPL_THEME_MINT,
    PSPL_THEME_FOREST,
    PSPL_THEME_VIOLET,
    PSPL_THEME_ROSE,
    PSPL_THEME_CRIMSON,
    PSPL_THEME_ACCENT_COUNT
} pspl_theme_accent_t;

typedef struct {
    uint32_t background;
    uint32_t surface;
    uint32_t surface_secondary;
    uint32_t primary_text;
    uint32_t secondary_text;
    uint32_t accent;
    uint32_t accent_text;
    uint32_t selected_background;
    uint32_t selected_border;
    uint32_t border;
} pspl_theme_t;

const pspl_theme_t *pspl_theme_get(pspl_theme_mode_t mode, pspl_theme_accent_t accent);
const pspl_theme_t *pspl_theme_current(void);
void pspl_theme_set(pspl_theme_mode_t mode, pspl_theme_accent_t accent);
const char *pspl_theme_mode_name(pspl_theme_mode_t mode);
const char *pspl_theme_accent_name(pspl_theme_accent_t accent);
int pspl_theme_contrast_ok(uint32_t background, uint32_t foreground);

#endif
