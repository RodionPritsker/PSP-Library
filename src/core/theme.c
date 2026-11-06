#include "psplibrary/theme.h"

#define RGB(r, g, b) (0xFF000000U | ((uint32_t)(b) << 16) | \
                      ((uint32_t)(g) << 8) | (uint32_t)(r))

/* PSP GU colors are written as 0xAABBGGRR.  The table is deliberately
 * explicit so every one of the 14 combinations is reviewable and stable. */
static const pspl_theme_t g_themes[2][PSPL_THEME_ACCENT_COUNT] = {
    {
        { RGB(14, 10, 5), RGB(28, 20, 10), RGB(43, 28, 12), RGB(255, 241, 208), RGB(201, 181, 143), RGB(240, 168, 58), RGB(31, 19, 4), RGB(80, 50, 14), RGB(240, 168, 58), RGB(107, 75, 31) },
        { RGB(10, 14, 20), RGB(20, 29, 41), RGB(29, 42, 58), RGB(233, 243, 255), RGB(164, 190, 214), RGB(76, 153, 224), RGB(7, 19, 32), RGB(18, 55, 88), RGB(76, 153, 224), RGB(46, 82, 117) },
        { RGB(7, 16, 15), RGB(15, 35, 31), RGB(21, 52, 44), RGB(225, 250, 241), RGB(157, 201, 185), RGB(57, 168, 138), RGB(5, 27, 21), RGB(14, 66, 52), RGB(57, 168, 138), RGB(39, 93, 78) },
        { RGB(10, 17, 9), RGB(21, 37, 19), RGB(29, 55, 26), RGB(230, 247, 226), RGB(164, 196, 157), RGB(77, 138, 87), RGB(8, 25, 8), RGB(27, 67, 32), RGB(77, 138, 87), RGB(53, 96, 57) },
        { RGB(14, 9, 22), RGB(29, 19, 43), RGB(42, 27, 61), RGB(243, 235, 255), RGB(195, 174, 219), RGB(138, 101, 196), RGB(25, 11, 42), RGB(66, 39, 100), RGB(138, 101, 196), RGB(85, 59, 117) },
        { RGB(22, 9, 14), RGB(43, 18, 28), RGB(59, 25, 38), RGB(255, 235, 240), RGB(215, 171, 185), RGB(201, 105, 125), RGB(44, 10, 19), RGB(92, 40, 55), RGB(201, 105, 125), RGB(119, 59, 75) },
        { RGB(22, 8, 8), RGB(43, 16, 16), RGB(60, 23, 23), RGB(255, 235, 235), RGB(216, 173, 173), RGB(185, 77, 77), RGB(44, 8, 8), RGB(94, 34, 34), RGB(185, 77, 77), RGB(119, 52, 52) }
    },
    {
        { RGB(246, 240, 228), RGB(255, 249, 238), RGB(237, 224, 201), RGB(47, 35, 21), RGB(106, 86, 58), RGB(194, 122, 24), RGB(255, 249, 238), RGB(250, 225, 175), RGB(194, 122, 24), RGB(198, 177, 141) },
        { RGB(237, 244, 250), RGB(248, 252, 255), RGB(215, 231, 245), RGB(27, 43, 59), RGB(75, 103, 128), RGB(43, 111, 177), RGB(248, 252, 255), RGB(190, 218, 241), RGB(43, 111, 177), RGB(155, 184, 208) },
        { RGB(234, 247, 243), RGB(248, 255, 252), RGB(211, 237, 228), RGB(24, 58, 48), RGB(70, 111, 98), RGB(32, 139, 109), RGB(248, 255, 252), RGB(182, 226, 211), RGB(32, 139, 109), RGB(147, 194, 178) },
        { RGB(238, 247, 235), RGB(250, 255, 248), RGB(216, 236, 211), RGB(34, 65, 30), RGB(83, 119, 78), RGB(57, 117, 64), RGB(250, 255, 248), RGB(190, 222, 184), RGB(57, 117, 64), RGB(152, 185, 145) },
        { RGB(244, 239, 250), RGB(253, 249, 255), RGB(229, 216, 245), RGB(57, 35, 83), RGB(103, 78, 130), RGB(111, 70, 171), RGB(253, 249, 255), RGB(211, 190, 237), RGB(111, 70, 171), RGB(175, 151, 200) },
        { RGB(252, 239, 243), RGB(255, 249, 251), RGB(244, 214, 222), RGB(83, 39, 51), RGB(132, 81, 94), RGB(177, 76, 101), RGB(255, 249, 251), RGB(239, 191, 204), RGB(177, 76, 101), RGB(202, 151, 165) },
        { RGB(252, 237, 237), RGB(255, 248, 248), RGB(244, 211, 211), RGB(83, 31, 31), RGB(132, 76, 76), RGB(165, 54, 54), RGB(255, 248, 248), RGB(237, 181, 181), RGB(165, 54, 54), RGB(201, 147, 147) }
    }
};

static pspl_theme_mode_t g_mode = PSPL_THEME_DARK;
static pspl_theme_accent_t g_accent = PSPL_THEME_AMBER;

const pspl_theme_t *pspl_theme_get(pspl_theme_mode_t mode, pspl_theme_accent_t accent)
{
    if (mode < PSPL_THEME_DARK || mode > PSPL_THEME_LIGHT) mode = PSPL_THEME_DARK;
    if (accent < PSPL_THEME_AMBER || accent >= PSPL_THEME_ACCENT_COUNT)
        accent = PSPL_THEME_AMBER;
    return &g_themes[mode][accent];
}

const pspl_theme_t *pspl_theme_current(void)
{
    return pspl_theme_get(g_mode, g_accent);
}

void pspl_theme_set(pspl_theme_mode_t mode, pspl_theme_accent_t accent)
{
    if (mode < PSPL_THEME_DARK || mode > PSPL_THEME_LIGHT) mode = PSPL_THEME_DARK;
    if (accent < PSPL_THEME_AMBER || accent >= PSPL_THEME_ACCENT_COUNT)
        accent = PSPL_THEME_AMBER;
    g_mode = mode;
    g_accent = accent;
}

const char *pspl_theme_mode_name(pspl_theme_mode_t mode)
{
    return mode == PSPL_THEME_LIGHT ? "LIGHT" : "DARK";
}

const char *pspl_theme_accent_name(pspl_theme_accent_t accent)
{
    static const char *const names[] = {
        "AMBER", "OCEAN", "MINT", "FOREST", "VIOLET", "ROSE", "CRIMSON"
    };
    if (accent < PSPL_THEME_AMBER || accent >= PSPL_THEME_ACCENT_COUNT)
        return names[PSPL_THEME_AMBER];
    return names[accent];
}

static unsigned int channel(uint32_t color, unsigned int shift)
{
    return (color >> shift) & 0xFFU;
}

int pspl_theme_contrast_ok(uint32_t background, uint32_t foreground)
{
    unsigned int br = channel(background, 0);
    unsigned int bg = channel(background, 8);
    unsigned int bb = channel(background, 16);
    unsigned int fr = channel(foreground, 0);
    unsigned int fg = channel(foreground, 8);
    unsigned int fb = channel(foreground, 16);
    unsigned int b = (br * 299U + bg * 587U + bb * 114U) / 1000U;
    unsigned int f = (fr * 299U + fg * 587U + fb * 114U) / 1000U;
    unsigned int difference = b > f ? b - f : f - b;
    return difference >= 90U;
}
