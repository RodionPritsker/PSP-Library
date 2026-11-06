#include "psplibrary/ui.h"

#include <pspctrl.h>
#include <pspkernel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "psplibrary/gfx.h"
#include "psplibrary/image.h"
#include "psplibrary/navigation.h"
#include "psplibrary/storage.h"
#include "psplibrary/text.h"
#include "psplibrary/theme.h"

#define PSPL_GRID_COLUMNS 3
#define PSPL_GRID_ROWS 2
#define PSPL_GRID_VISIBLE_ITEMS (PSPL_GRID_COLUMNS * PSPL_GRID_ROWS)
#define PSPL_LIST_VISIBLE_ROWS 6
#define PSPL_IMAGE_CACHE_CAPACITY 8

#if PSPL_IMAGE_CACHE_CAPACITY < PSPL_GRID_VISIBLE_ITEMS
#error "The image cache must hold every texture referenced by one grid frame"
#endif

#define COLOR_BG (pspl_theme_current()->background)
#define COLOR_PANEL (pspl_theme_current()->surface)
#define COLOR_TEXT (pspl_theme_current()->primary_text)
#define COLOR_MUTED (pspl_theme_current()->secondary_text)
#define COLOR_ACCENT (pspl_theme_current()->accent)
#define COLOR_ACCENT_SOFT (pspl_theme_current()->surface_secondary)
#define COLOR_SELECTED (pspl_theme_current()->selected_background)
#define COLOR_SELECTED_BORDER (pspl_theme_current()->selected_border)
#define COLOR_BORDER (pspl_theme_current()->border)

typedef struct {
    int detail;
    pspl_navigation_t navigation;
} ui_state_t;

static void format_date(uint32_t timestamp, char output[16])
{
    time_t value;
    struct tm *local;
    if (timestamp == 0) {
        snprintf(output, 16, "--/--/----");
        return;
    }
    value = (time_t)timestamp;
    local = localtime(&value);
    if (local == NULL) {
        snprintf(output, 16, "--/--/----");
        return;
    }
    snprintf(output, 16, "%02d/%02d/%04d", local->tm_mday, local->tm_mon + 1,
             local->tm_year + 1900);
}

static void draw_pill(float x, float y, float width, const char *label, uint32_t color)
{
    pspl_gfx_fill_rect(x, y, width, 20.0f, color);
    pspl_text_draw_centered(x, y + 4.0f, width, label, 1, COLOR_TEXT);
}

static float ui_text_width(const char *text, int scale)
{
    return (float)pspl_text_width(text, scale);
}

static void draw_header(const ui_state_t *state, int demo_mode, size_t count,
                        size_t page, size_t pages)
{
    const char *platform = pspl_platform_name(state->navigation.filter);
    const char *mode = demo_mode == 2 ? "TEST 100" : demo_mode ? "DEMO" : "MY LIBRARY";
    const float right = 472.0f;
    const float gap = 8.0f;
    char page_text[32];
    float title_end;
    float version_x;
    float version_width;
    float page_width;
    float page_x;
    float mode_width;
    float mode_x;
    float pill_width;
    float pill_x;
    float left_limit;

    pspl_gfx_fill_rect(0.0f, 0.0f, 480.0f, 34.0f, COLOR_PANEL);
    pspl_gfx_fill_rect(0.0f, 33.0f, 480.0f, 1.0f, COLOR_BORDER);
    pspl_text_draw(12.0f, 4.0f, "PSP LIBRARY", 2, COLOR_TEXT);
    title_end = 12.0f + ui_text_width("PSP LIBRARY", 2);
    version_x = title_end + 10.0f;
    version_width = ui_text_width("V0.3.1", 1);
    snprintf(page_text, sizeof(page_text), "%lu/%lu %lu GAMES",
             (unsigned long)page, (unsigned long)pages, (unsigned long)count);
    page_width = ui_text_width(page_text, 1);
    page_x = right - page_width;
    mode_width = ui_text_width(mode, 1);
    mode_x = page_x - gap - mode_width;
    pill_width = ui_text_width(platform, 1) + 12.0f;
    if (pill_width < 46.0f) pill_width = 46.0f;
    if (pill_width > 76.0f) pill_width = 76.0f;
    pill_x = mode_x - gap - pill_width;
    left_limit = version_x + version_width + gap;
    if (pill_x < left_limit) {
        float available = mode_x - gap - left_limit;
        if (available >= 44.0f) {
            pill_x = left_limit;
            pill_width = available;
        }
    }
    pspl_text_draw_clipped(version_x, 9.0f,
                           pill_x - gap - version_x, "V0.3.1", 1, COLOR_MUTED);
    draw_pill(pill_x, 7.0f, pill_width, platform, COLOR_ACCENT_SOFT);
    pspl_text_draw(mode_x, 9.0f, mode, 1, COLOR_MUTED);
    pspl_text_draw(page_x, 9.0f, page_text, 1, COLOR_TEXT);
}

static void draw_footer(const ui_state_t *state)
{
    const char *full_labels[6] = {
        "D-PAD MOVE", "X OPEN", "SELECT GRID/LIST", "START SORT", "L/R FILTER", "TRI EXIT"
    };
    const char *compact_labels[6] = {
        "D-PAD", "X OPEN", "SELECT VIEW", "START SORT", "L/R FILTER", "TRI EXIT"
    };
    const char *minimal_labels[6] = { "D-PAD", "X", "SELECT", "START", "L/R", "TRI" };
    const char **labels = full_labels;
    float widths[6];
    float total = 0.0f;
    float gap;
    float x = 8.0f;
    size_t i;

    (void)state;

    pspl_gfx_fill_rect(0.0f, 250.0f, 480.0f, 22.0f, COLOR_PANEL);
    pspl_gfx_fill_rect(0.0f, 250.0f, 480.0f, 1.0f, COLOR_BORDER);
    for (i = 0; i < 6U; ++i) {
        widths[i] = ui_text_width(labels[i], 1);
        total += widths[i];
    }
    if (total + 5.0f * 4.0f > 464.0f) {
        labels = compact_labels;
        total = 0.0f;
        for (i = 0; i < 6U; ++i) {
            widths[i] = ui_text_width(labels[i], 1);
            total += widths[i];
        }
    }
    if (total + 5.0f * 4.0f > 464.0f) {
        labels = minimal_labels;
        total = 0.0f;
        for (i = 0; i < 6U; ++i) {
            widths[i] = ui_text_width(labels[i], 1);
            total += widths[i];
        }
    }
    gap = (464.0f - total) / 5.0f;
    if (gap < 4.0f) gap = 4.0f;
    for (i = 0; i < 6U; ++i) {
        pspl_text_draw(x, 253.0f, labels[i], 1,
                       i == 1U ? COLOR_TEXT : (i == 2U ? COLOR_ACCENT : COLOR_MUTED));
        x += widths[i] + gap;
    }
}

static void draw_grid(const pspl_library_t *library, const ui_state_t *state,
                      const size_t *indices, size_t count, pspl_image_cache_t *images)
{
    const size_t visible = PSPL_GRID_VISIBLE_ITEMS;
    size_t page_start = count == 0 ? 0 : (state->navigation.selected / visible) * visible;
    size_t page_end = page_start + visible;
    size_t i;
    char duration[16];
    if (page_end > count) page_end = count;
    for (i = page_start; i < page_end; ++i) {
        const pspl_game_t *game = &library->games[indices[i]];
        size_t slot = i - page_start;
        int column = (int)(slot % PSPL_GRID_COLUMNS);
        int row = (int)(slot / PSPL_GRID_COLUMNS);
        float x = 8.0f + column * 158.0f;
        float y = 39.0f + row * 104.0f;
        int selected = i == state->navigation.selected;
        pspl_gfx_fill_rect(x, y, 150.0f, 98.0f, selected ? COLOR_SELECTED : COLOR_PANEL);
        pspl_gfx_rect(x, y, 150.0f, 98.0f, selected ? COLOR_SELECTED : COLOR_BORDER, 1.0f);
        if (selected) pspl_gfx_rect(x, y, 150.0f, 98.0f, COLOR_SELECTED_BORDER, 2.0f);
        pspl_image_draw(images, game, x + 6.0f, y + 7.0f, 58.0f, 84.0f);
        pspl_text_draw_wrapped(x + 69.0f, y + 7.0f, 74.0f, game->title, 1, 2, COLOR_TEXT);
        pspl_text_draw_clipped(x + 69.0f, y + 50.0f, 74.0f, game->platform, 1, COLOR_ACCENT);
        pspl_text_draw(x + 69.0f, y + 68.0f,
                       pspl_format_duration(game->total_seconds, duration, sizeof(duration)),
                       1, COLOR_TEXT);
    }
    if (count == 0) {
        pspl_gfx_fill_rect(30.0f, 78.0f, 420.0f, 106.0f, COLOR_PANEL);
        pspl_text_draw_centered(30.0f, 104.0f, 420.0f, "NO GAMES IN THIS FILTER", 2, COLOR_TEXT);
        pspl_text_draw_centered(30.0f, 137.0f, 420.0f, "L/R CHANGES PLATFORM", 1, COLOR_MUTED);
    }
}

static void draw_list(const pspl_library_t *library, const ui_state_t *state,
                      const size_t *indices, size_t count, pspl_image_cache_t *images)
{
    const size_t visible = PSPL_LIST_VISIBLE_ROWS;
    size_t page_start = count == 0 ? 0 : (state->navigation.selected / visible) * visible;
    size_t page_end = page_start + visible;
    size_t i;
    char duration[16];
    if (page_end > count) page_end = count;
    for (i = page_start; i < page_end; ++i) {
        const pspl_game_t *game = &library->games[indices[i]];
        float y = 45.0f + (float)(i - page_start) * 33.0f;
        float duration_width;
        float duration_x;
        float platform_width;
        float platform_x;
        float title_width;
        int selected = i == state->navigation.selected;
        pspl_gfx_fill_rect(8.0f, y, 464.0f, 29.0f, selected ? COLOR_SELECTED : COLOR_PANEL);
        pspl_gfx_rect(8.0f, y, 464.0f, 29.0f, selected ? COLOR_SELECTED_BORDER : COLOR_BORDER, 1.0f);
        pspl_image_draw(images, game, 14.0f, y + 3.0f, 23.0f, 23.0f);
        pspl_format_duration(game->total_seconds, duration, sizeof(duration));
        duration_width = ui_text_width(duration, 1);
        duration_x = 464.0f - duration_width;
        platform_width = ui_text_width(game->platform, 1);
        platform_x = duration_x - 10.0f - platform_width;
        if (platform_x < 320.0f) platform_x = 320.0f;
        title_width = platform_x - 10.0f - 47.0f;
        pspl_text_draw_clipped(47.0f, y + 5.0f, title_width, game->title, 1, COLOR_TEXT);
        pspl_text_draw_clipped(platform_x, y + 5.0f,
                               duration_x - 10.0f - platform_x, game->platform,
                               1, COLOR_ACCENT);
        pspl_text_draw(duration_x, y + 5.0f, duration, 1, COLOR_TEXT);
    }
    if (count == 0) pspl_text_draw_centered(20.0f, 112.0f, 440.0f, "NO GAMES IN THIS FILTER", 2, COLOR_TEXT);
}

static void draw_stat(float x, float y, float width, const char *label, const char *value)
{
    pspl_gfx_fill_rect(x, y, width, 43.0f, COLOR_PANEL);
    pspl_text_draw(x + 8.0f, y + 4.0f, label, 1, COLOR_MUTED);
    pspl_text_draw(x + 8.0f, y + 19.0f, value, 1, COLOR_TEXT);
}

static void draw_detail(const pspl_library_t *library, const ui_state_t *state,
                        const size_t *indices, int count, pspl_image_cache_t *images)
{
    const pspl_game_t *game;
    char duration[16];
    char first_date[16];
    char last_date[16];
    size_t i;
    int shown = 0;
    if (count <= 0 || state->navigation.selected >= (size_t)count) {
        pspl_text_draw_centered(20.0f, 110.0f, 440.0f, "NO GAME SELECTED", 2, COLOR_TEXT);
        return;
    }
    game = &library->games[indices[state->navigation.selected]];
    format_date(game->first_started_at, first_date);
    format_date(game->last_started_at, last_date);
    pspl_text_draw(12.0f, 4.0f, "GAME DETAILS", 2, COLOR_TEXT);
    pspl_text_draw(468.0f - ui_text_width("O BACK", 1), 9.0f, "O BACK", 1, COLOR_ACCENT);
    pspl_image_draw(images, game, 14.0f, 45.0f, 110.0f, 158.0f);
    pspl_text_draw_wrapped(140.0f, 41.0f, 325.0f, game->title, 2, 2, COLOR_TEXT);
    pspl_text_draw(140.0f, 78.0f, game->platform, 1, COLOR_ACCENT);
    pspl_text_draw(192.0f, 78.0f, "OFFLINE LIBRARY", 1, COLOR_MUTED);
    pspl_format_duration(game->total_seconds, duration, sizeof(duration));
    draw_stat(140.0f, 95.0f, 148.0f, "TOTAL TIME", duration);
    {
        char sessions[16];
        snprintf(sessions, sizeof(sessions), "%lu", (unsigned long)game->session_count);
        draw_stat(296.0f, 95.0f, 79.0f, "SESSIONS", sessions);
    }
    draw_stat(383.0f, 95.0f, 82.0f, "LAST", last_date);
    pspl_text_draw(140.0f, 151.0f, "FIRST PLAYED", 1, COLOR_MUTED);
    pspl_text_draw(237.0f, 151.0f, first_date, 1, COLOR_TEXT);
    pspl_text_draw(140.0f, 168.0f, "COVER", 1, COLOR_MUTED);
    pspl_text_draw(187.0f, 168.0f, game->cover_path[0] ? "LOCAL PNG" : "PLACEHOLDER", 1, COLOR_TEXT);
    pspl_text_draw(140.0f, 188.0f, "RECENT SESSIONS", 1, COLOR_ACCENT);
    for (i = library->session_count; i > 0 && shown < 3; --i) {
        const pspl_session_t *session = &library->sessions[i - 1];
        if (strcmp(session->game_id, game->id) == 0) {
            char date[16];
            char row[48];
            format_date(session->started_at, date);
            snprintf(row, sizeof(row), "%s   %s", date,
                     pspl_format_duration(session->duration_seconds, duration, sizeof(duration)));
            pspl_text_draw(140.0f, 202.0f + shown * 14.0f, row, 1, COLOR_TEXT);
            ++shown;
        }
    }
    if (shown == 0) pspl_text_draw(140.0f, 202.0f, "NO COMPLETED SESSIONS", 1, COLOR_MUTED);
    pspl_gfx_fill_rect(0.0f, 250.0f, 480.0f, 22.0f, COLOR_PANEL);
    pspl_text_draw(14.0f, 258.0f, "O BACK", 1, COLOR_ACCENT);
    pspl_text_draw(74.0f, 258.0f, "SQUARE TEST SESSION", 1, COLOR_MUTED);
}

static int preload_next_cover(const pspl_library_t *library, const ui_state_t *state,
                              const size_t *indices, size_t count,
                              pspl_image_cache_t *images)
{
    size_t visible = state->detail ? 1U :
        (state->navigation.grid ? PSPL_GRID_VISIBLE_ITEMS : PSPL_LIST_VISIBLE_ROWS);
    size_t page_start = count == 0 ? 0 : (state->navigation.selected / visible) * visible;
    size_t page_end = page_start + visible;
    size_t i;
    if (page_end > count) page_end = count;
    for (i = page_start; i < page_end; ++i) {
        const pspl_game_t *game = &library->games[indices[i]];
        if (pspl_image_cache_load_path(images, game->cover_path) != 0) return 1;
    }
    return 0;
}

static void generate_test_session(pspl_library_t *library, size_t *indices, int count,
                                  size_t selected)
{
    pspl_event_t event;
    uint32_t now;
    if (count <= 0 || selected >= (size_t)count) return;
    now = pspl_library_now();
    event.game_id = library->games[indices[selected]].id;
    event.type = PSPL_EVENT_GAME_STARTED;
    event.timestamp = now - 45U;
    pspl_library_handle_event(library, &event);
    event.type = PSPL_EVENT_GAME_STOPPED;
    event.timestamp = now;
    pspl_library_handle_event(library, &event);
}

static uint64_t input_time_us(void)
{
    return (uint64_t)sceKernelGetSystemTimeWide();
}

static unsigned int navigation_direction(unsigned int buttons)
{
    unsigned int direction = 0;
    if ((buttons & PSP_CTRL_UP) != 0) direction |= PSPL_NAV_UP;
    if ((buttons & PSP_CTRL_DOWN) != 0) direction |= PSPL_NAV_DOWN;
    if ((buttons & PSP_CTRL_LEFT) != 0) direction |= PSPL_NAV_LEFT;
    if ((buttons & PSP_CTRL_RIGHT) != 0) direction |= PSPL_NAV_RIGHT;
    return direction;
}

int pspl_ui_run(pspl_library_t *library, const char *database_path, int demo_mode)
{
    pspl_gfx_t gfx;
    pspl_image_cache_t images;
    ui_state_t state;
    size_t *indices = NULL;
    size_t indices_capacity = 0;
    int count;
    size_t count_size;
    int dirty = 1;
    unsigned int previous_buttons = 0;
    pspl_navigation_repeat_t repeat;
    SceCtrlData pad;

    memset(&gfx, 0, sizeof(gfx));
    if (library == NULL || pspl_gfx_init(&gfx) != 0) {
        pspl_text_shutdown();
        pspl_gfx_shutdown(&gfx);
        return -1;
    }
    /* A missing package asset is non-fatal: the text module has a safe outline fallback. */
    (void)pspl_text_init();
    if (pspl_image_cache_init(&images, PSPL_IMAGE_CACHE_CAPACITY) != 0) {
        pspl_text_shutdown();
        pspl_gfx_shutdown(&gfx);
        return -1;
    }
    memset(&state, 0, sizeof(state));
    pspl_navigation_init(&state.navigation);
    pspl_navigation_repeat_init(&repeat);
    if (library->game_count > indices_capacity) {
        indices = (size_t *)realloc(indices, library->game_count * sizeof(*indices));
        if (indices == NULL) {
            pspl_image_cache_free(&images);
            pspl_text_shutdown();
            pspl_gfx_shutdown(&gfx);
            return -1;
        }
        indices_capacity = library->game_count;
    }
    if (pspl_navigation_refresh(&state.navigation, library, indices,
                                indices_capacity, &count_size) != 0) {
        free(indices);
        pspl_image_cache_free(&images);
        pspl_text_shutdown();
        pspl_gfx_shutdown(&gfx);
        return -1;
    }
    for (;;) {
        size_t visible;
        size_t pages;
        size_t page;
        count = (int)count_size;
        visible = state.navigation.grid ? PSPL_GRID_VISIBLE_ITEMS : PSPL_LIST_VISIBLE_ROWS;
        pages = count == 0 ? 1U : ((size_t)count + visible - 1U) / visible;
        page = count == 0 ? 1U : state.navigation.selected / visible + 1U;
        if (dirty) {
            pspl_image_cache_begin_frame(&images);
            if (pspl_gfx_begin(&gfx) != 0) break;
            pspl_gfx_clear(COLOR_BG);
            if (state.detail) draw_detail(library, &state, indices, count, &images);
            else {
                draw_header(&state, demo_mode, (size_t)count, page, pages);
                if (state.navigation.grid) draw_grid(library, &state, indices, (size_t)count, &images);
                else draw_list(library, &state, indices, (size_t)count, &images);
                draw_footer(&state);
            }
            pspl_gfx_end(&gfx);
            dirty = 0;

            /*
             * The GU frame is synchronized now. Decode one uncached cover
             * while the stable frame remains visible, then redraw it on the
             * next loop. This prevents large PNGs from blocking a half-built
             * frame during startup or navigation.
             */
            if (preload_next_cover(library, &state, indices, (size_t)count, &images))
                dirty = 1;
        }
        sceCtrlReadBufferPositive(&pad, 1);
        if ((pad.Buttons & PSP_CTRL_TRIANGLE) && !(previous_buttons & PSP_CTRL_TRIANGLE)) break;
        if ((pad.Buttons & PSP_CTRL_SELECT) && !(previous_buttons & PSP_CTRL_SELECT) && !state.detail) {
            pspl_navigation_toggle_view(&state.navigation);
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_START) && !(previous_buttons & PSP_CTRL_START) && !state.detail) {
            if (pspl_navigation_change_sort(&state.navigation, library, indices,
                                            indices_capacity, count_size,
                                            &count_size) != 0) break;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_LTRIGGER) && !(previous_buttons & PSP_CTRL_LTRIGGER) && !state.detail) {
            if (pspl_navigation_change_filter(&state.navigation, -1, library, indices,
                                              indices_capacity, count_size,
                                              &count_size) != 0) break;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_RTRIGGER) && !(previous_buttons & PSP_CTRL_RTRIGGER) && !state.detail) {
            if (pspl_navigation_change_filter(&state.navigation, 1, library, indices,
                                              indices_capacity, count_size,
                                              &count_size) != 0) break;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_CROSS) && !(previous_buttons & PSP_CTRL_CROSS) && !state.detail) {
            state.detail = 1;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_CIRCLE) && !(previous_buttons & PSP_CTRL_CIRCLE) && state.detail) {
            state.detail = 0;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_SQUARE) && !(previous_buttons & PSP_CTRL_SQUARE)) {
            generate_test_session(library, indices, count, state.navigation.selected);
            if (!demo_mode) pspl_storage_save(database_path, library);
            if (pspl_navigation_refresh(&state.navigation, library, indices,
                                        indices_capacity, &count_size) != 0) break;
            dirty = 1;
        }
        if (!state.detail) {
            unsigned int direction = pspl_navigation_repeat(
                &repeat, navigation_direction(pad.Buttons), input_time_us());
            if (direction != 0) {
                pspl_navigation_move(&state.navigation, library, indices,
                                     count_size, direction);
                dirty = 1;
            }
        } else {
            pspl_navigation_repeat_init(&repeat);
        }
        previous_buttons = pad.Buttons;
        sceKernelDelayThread(16000);
    }
    free(indices);
    pspl_image_cache_free(&images);
    pspl_text_shutdown();
    pspl_gfx_shutdown(&gfx);
    return 0;
}
