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

#define PSPL_GRID_COLUMNS 3
#define PSPL_GRID_ROWS 2
#define PSPL_GRID_VISIBLE_ITEMS (PSPL_GRID_COLUMNS * PSPL_GRID_ROWS)
#define PSPL_LIST_VISIBLE_ROWS 6

#define COLOR_BG 0xFF0B1118U
#define COLOR_PANEL 0xFF14202BU
#define COLOR_TEXT 0xFFE5EEF4U
#define COLOR_MUTED 0xFF91A6B5U
#define COLOR_ACCENT 0xFF5BD6FFU
#define COLOR_ACCENT_SOFT 0xFF234E64U
#define COLOR_SELECTED 0xFF2B5F72U

typedef struct {
    int detail;
    int grid;
    size_t selected;
    pspl_platform_t filter;
    pspl_sort_t sort;
    pspl_navigation_t navigation;
    pspl_platform_t last_filter;
} ui_state_t;

static const pspl_platform_t filter_order[] = {
    PSPL_PLATFORM_ALL, PSPL_PLATFORM_PSP, PSPL_PLATFORM_PS1,
    PSPL_PLATFORM_GBA, PSPL_PLATFORM_GB, PSPL_PLATFORM_GBC,
    PSPL_PLATFORM_NES, PSPL_PLATFORM_SNES, PSPL_PLATFORM_OTHER
};

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
    pspl_text_draw_centered(x, y + 7.0f, width, label, 1, COLOR_TEXT);
}

static void draw_header(const ui_state_t *state, int demo_mode, size_t count,
                        size_t page, size_t pages)
{
    pspl_gfx_fill_rect(0.0f, 0.0f, 480.0f, 38.0f, COLOR_PANEL);
    pspl_gfx_fill_rect(0.0f, 37.0f, 480.0f, 1.0f, COLOR_ACCENT_SOFT);
    pspl_text_draw(14.0f, 9.0f, "PSP LIBRARY", 2, COLOR_TEXT);
    pspl_text_draw(175.0f, 11.0f, "V0.3", 1, COLOR_MUTED);
    draw_pill(218.0f, 8.0f, 68.0f, pspl_platform_name(state->filter), COLOR_ACCENT_SOFT);
    pspl_text_draw(298.0f, 12.0f, demo_mode == 2 ? "TEST 100" : demo_mode ? "DEMO" : "MY LIBRARY",
                   1, COLOR_MUTED);
    {
        char page_text[32];
        snprintf(page_text, sizeof(page_text), "%lu/%lu %lu GAMES",
                 (unsigned long)page, (unsigned long)pages, (unsigned long)count);
        pspl_text_draw(384.0f, 12.0f, page_text, 1, COLOR_TEXT);
    }
}

static void draw_footer(const ui_state_t *state)
{
    const char *view = state->grid ? "GRID" : "LIST";
    pspl_gfx_fill_rect(0.0f, 250.0f, 480.0f, 22.0f, COLOR_PANEL);
    pspl_text_draw(10.0f, 258.0f, "D-PAD MOVE", 1, COLOR_MUTED);
    pspl_text_draw(92.0f, 258.0f, "X OPEN", 1, COLOR_TEXT);
    pspl_text_draw(151.0f, 258.0f, "SELECT", 1, COLOR_ACCENT);
    pspl_text_draw(205.0f, 258.0f, view, 1, COLOR_TEXT);
    pspl_text_draw(254.0f, 258.0f, "START SORT", 1, COLOR_MUTED);
    pspl_text_draw(334.0f, 258.0f, "L/R FILTER", 1, COLOR_MUTED);
    pspl_text_draw(422.0f, 258.0f, "TRI EXIT", 1, COLOR_MUTED);
}

static void draw_grid(const pspl_library_t *library, const ui_state_t *state,
                      const size_t *indices, size_t count, pspl_image_cache_t *images)
{
    const size_t visible = PSPL_GRID_VISIBLE_ITEMS;
    size_t page_start = count == 0 ? 0 : (state->selected / visible) * visible;
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
        float y = 44.0f + row * 101.0f;
        int selected = i == state->selected;
        pspl_gfx_fill_rect(x, y, 150.0f, 91.0f, selected ? COLOR_SELECTED : COLOR_PANEL);
        if (selected) pspl_gfx_rect(x, y, 150.0f, 91.0f, COLOR_ACCENT, 2.0f);
        pspl_image_draw(images, game, x + 6.0f, y + 7.0f, 51.0f, 75.0f);
        pspl_text_draw_clipped(x + 64.0f, y + 10.0f, 80.0f, game->title, 1, COLOR_TEXT);
        pspl_text_draw_clipped(x + 64.0f, y + 27.0f, 80.0f, game->platform, 1, COLOR_ACCENT);
        pspl_text_draw(x + 64.0f, y + 47.0f,
                       pspl_format_duration(game->total_seconds, duration, sizeof(duration)),
                       1, COLOR_TEXT);
        {
            char sessions[24];
            snprintf(sessions, sizeof(sessions), "%lu SESSIONS", (unsigned long)game->session_count);
            pspl_text_draw_clipped(x + 64.0f, y + 65.0f, 80.0f, sessions, 1, COLOR_MUTED);
        }
    }
    if (count == 0) {
        pspl_gfx_fill_rect(30.0f, 78.0f, 420.0f, 106.0f, COLOR_PANEL);
        pspl_text_draw_centered(30.0f, 106.0f, 420.0f, "NO GAMES IN THIS FILTER", 2, COLOR_TEXT);
        pspl_text_draw_centered(30.0f, 137.0f, 420.0f, "L/R CHANGES PLATFORM", 1, COLOR_MUTED);
    }
}

static void draw_list(const pspl_library_t *library, const ui_state_t *state,
                      const size_t *indices, size_t count, pspl_image_cache_t *images)
{
    const size_t visible = PSPL_LIST_VISIBLE_ROWS;
    size_t page_start = count == 0 ? 0 : (state->selected / visible) * visible;
    size_t page_end = page_start + visible;
    size_t i;
    char duration[16];
    if (page_end > count) page_end = count;
    for (i = page_start; i < page_end; ++i) {
        const pspl_game_t *game = &library->games[indices[i]];
        float y = 45.0f + (float)(i - page_start) * 33.0f;
        int selected = i == state->selected;
        char sessions[20];
        pspl_gfx_fill_rect(8.0f, y, 464.0f, 29.0f, selected ? COLOR_SELECTED : COLOR_PANEL);
        if (selected) pspl_gfx_rect(8.0f, y, 464.0f, 29.0f, COLOR_ACCENT, 1.0f);
        pspl_image_draw(images, game, 14.0f, y + 3.0f, 23.0f, 23.0f);
        pspl_text_draw_clipped(47.0f, y + 6.0f, 245.0f, game->title, 1, COLOR_TEXT);
        pspl_text_draw(302.0f, y + 6.0f, game->platform, 1, COLOR_ACCENT);
        pspl_text_draw(354.0f, y + 6.0f,
                       pspl_format_duration(game->total_seconds, duration, sizeof(duration)),
                       1, COLOR_TEXT);
        snprintf(sessions, sizeof(sessions), "%lu", (unsigned long)game->session_count);
        pspl_text_draw(424.0f, y + 6.0f, sessions, 1, COLOR_MUTED);
    }
    if (count == 0) pspl_text_draw_centered(20.0f, 112.0f, 440.0f, "NO GAMES IN THIS FILTER", 2, COLOR_TEXT);
}

static void draw_stat(float x, float y, float width, const char *label, const char *value)
{
    pspl_gfx_fill_rect(x, y, width, 37.0f, COLOR_PANEL);
    pspl_text_draw(x + 8.0f, y + 7.0f, label, 1, COLOR_MUTED);
    pspl_text_draw(x + 8.0f, y + 21.0f, value, 1, COLOR_TEXT);
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
    if (count <= 0 || state->selected >= (size_t)count) {
        pspl_text_draw_centered(20.0f, 110.0f, 440.0f, "NO GAME SELECTED", 2, COLOR_TEXT);
        return;
    }
    game = &library->games[indices[state->selected]];
    format_date(game->first_started_at, first_date);
    format_date(game->last_started_at, last_date);
    pspl_text_draw(14.0f, 9.0f, "GAME DETAILS", 2, COLOR_TEXT);
    pspl_text_draw(374.0f, 12.0f, "O BACK", 1, COLOR_ACCENT);
    pspl_image_draw(images, game, 14.0f, 45.0f, 110.0f, 158.0f);
    pspl_text_draw_clipped(140.0f, 47.0f, 325.0f, game->title, 2, COLOR_TEXT);
    pspl_text_draw(140.0f, 72.0f, game->platform, 1, COLOR_ACCENT);
    pspl_text_draw(192.0f, 72.0f, "OFFLINE LIBRARY", 1, COLOR_MUTED);
    pspl_format_duration(game->total_seconds, duration, sizeof(duration));
    draw_stat(140.0f, 88.0f, 103.0f, "TOTAL TIME", duration);
    {
        char sessions[16];
        snprintf(sessions, sizeof(sessions), "%lu", (unsigned long)game->session_count);
        draw_stat(250.0f, 88.0f, 73.0f, "SESSIONS", sessions);
    }
    draw_stat(330.0f, 88.0f, 135.0f, "LAST PLAYED", last_date);
    pspl_text_draw(140.0f, 136.0f, "FIRST PLAYED", 1, COLOR_MUTED);
    pspl_text_draw(237.0f, 136.0f, first_date, 1, COLOR_TEXT);
    pspl_text_draw(140.0f, 153.0f, "COVER", 1, COLOR_MUTED);
    pspl_text_draw(187.0f, 153.0f, game->cover_path[0] ? "LOCAL PNG" : "PLACEHOLDER", 1, COLOR_TEXT);
    pspl_text_draw(140.0f, 176.0f, "RECENT SESSIONS", 1, COLOR_ACCENT);
    for (i = library->session_count; i > 0 && shown < 3; --i) {
        const pspl_session_t *session = &library->sessions[i - 1];
        if (strcmp(session->game_id, game->id) == 0) {
            char date[16];
            char row[48];
            format_date(session->started_at, date);
            snprintf(row, sizeof(row), "%s   %s", date,
                     pspl_format_duration(session->duration_seconds, duration, sizeof(duration)));
            pspl_text_draw(140.0f, 190.0f + shown * 14.0f, row, 1, COLOR_TEXT);
            ++shown;
        }
    }
    if (shown == 0) pspl_text_draw(140.0f, 190.0f, "NO COMPLETED SESSIONS", 1, COLOR_MUTED);
    pspl_gfx_fill_rect(0.0f, 250.0f, 480.0f, 22.0f, COLOR_PANEL);
    pspl_text_draw(14.0f, 258.0f, "O BACK", 1, COLOR_ACCENT);
    pspl_text_draw(74.0f, 258.0f, "SQUARE TEST SESSION", 1, COLOR_MUTED);
}

static size_t clamp_selection(size_t selected, int count)
{
    if (count <= 0) return 0;
    return selected >= (size_t)count ? (size_t)count - 1 : selected;
}

static void change_filter(ui_state_t *state, int direction)
{
    size_t i;
    size_t filter_count = sizeof(filter_order) / sizeof(filter_order[0]);
    for (i = 0; i < filter_count; ++i) {
        if (filter_order[i] == state->filter) {
            int next = (int)i + direction;
            if (next < 0) next = (int)filter_count - 1;
            if (next >= (int)filter_count) next = 0;
            state->filter = filter_order[next];
            return;
        }
    }
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

static unsigned int repeated_direction(unsigned int buttons, unsigned int *previous,
                                       unsigned int *ticks)
{
    unsigned int direction = buttons & (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT);
    unsigned int previous_direction = *previous & (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT);
    if (direction == 0) {
        *ticks = 0;
        *previous = buttons;
        return 0;
    }
    if (direction != previous_direction) {
        *ticks = 0;
        *previous = buttons;
        return direction;
    }
    ++(*ticks);
    *previous = buttons;
    if (*ticks >= 7U && ((*ticks - 7U) % 3U) == 0U) return direction;
    return 0;
}

static void move_selection(ui_state_t *state, int count, unsigned int direction)
{
    if (count <= 0) return;
    if (state->grid) {
        if (direction & PSP_CTRL_LEFT) {
            if (state->selected % PSPL_GRID_COLUMNS != 0) --state->selected;
        } else if (direction & PSP_CTRL_RIGHT) {
            if (state->selected + 1 < (size_t)count &&
                state->selected % PSPL_GRID_COLUMNS != PSPL_GRID_COLUMNS - 1) ++state->selected;
        } else if (direction & PSP_CTRL_UP) {
            if (state->selected >= PSPL_GRID_COLUMNS) state->selected -= PSPL_GRID_COLUMNS;
        } else if (direction & PSP_CTRL_DOWN) {
            if (state->selected + PSPL_GRID_COLUMNS < (size_t)count)
                state->selected += PSPL_GRID_COLUMNS;
        }
    } else if (direction & PSP_CTRL_UP) {
        if (state->selected > 0) --state->selected;
    } else if (direction & PSP_CTRL_DOWN) {
        if (state->selected + 1 < (size_t)count) ++state->selected;
    }
}

int pspl_ui_run(pspl_library_t *library, const char *database_path, int demo_mode)
{
    pspl_gfx_t gfx;
    pspl_image_cache_t images;
    ui_state_t state;
    size_t *indices = NULL;
    size_t indices_capacity = 0;
    int count;
    int dirty = 1;
    unsigned int previous_buttons = 0;
    unsigned int direction_ticks = 0;
    SceCtrlData pad;

    memset(&gfx, 0, sizeof(gfx));
    if (library == NULL || pspl_gfx_init(&gfx) != 0 || pspl_image_cache_init(&images, 4) != 0) {
        pspl_gfx_shutdown(&gfx);
        return -1;
    }
    memset(&state, 0, sizeof(state));
    pspl_navigation_init(&state.navigation);
    state.grid = 1;
    state.filter = PSPL_PLATFORM_ALL;
    state.last_filter = state.filter;
    for (;;) {
        size_t visible;
        size_t pages;
        size_t page;
        size_t previous_selected = state.selected;
        if (library->game_count > indices_capacity) {
            size_t *new_indices = (size_t *)realloc(indices, library->game_count * sizeof(*indices));
            if (new_indices == NULL) break;
            indices = new_indices;
            indices_capacity = library->game_count;
        }
        count = pspl_library_filter_sort(library, state.filter, state.sort,
                                         indices, indices_capacity);
        if (count < 0) break;
        if (state.filter == state.last_filter) {
            pspl_navigation_remember(&state.navigation, state.filter, library, indices,
                                     (size_t)count, state.selected);
        }
        pspl_navigation_restore(&state.navigation, state.filter, library, indices,
                                (size_t)count, &state.selected);
        state.selected = clamp_selection(state.selected, count);
        pspl_navigation_remember(&state.navigation, state.filter, library, indices,
                                 (size_t)count, state.selected);
        state.last_filter = state.filter;
        if (state.selected != previous_selected) dirty = 1;
        visible = state.grid ? PSPL_GRID_VISIBLE_ITEMS : PSPL_LIST_VISIBLE_ROWS;
        pages = count == 0 ? 1U : ((size_t)count + visible - 1U) / visible;
        page = count == 0 ? 1U : state.selected / visible + 1U;
        if (dirty) {
            pspl_image_cache_begin_frame(&images);
            pspl_gfx_begin(&gfx);
            pspl_gfx_clear(COLOR_BG);
            if (state.detail) draw_detail(library, &state, indices, count, &images);
            else {
                draw_header(&state, demo_mode, (size_t)count, page, pages);
                if (state.grid) draw_grid(library, &state, indices, (size_t)count, &images);
                else draw_list(library, &state, indices, (size_t)count, &images);
                draw_footer(&state);
            }
            pspl_gfx_end(&gfx);
            dirty = 0;
        }
        sceCtrlReadBufferPositive(&pad, 1);
        if ((pad.Buttons & PSP_CTRL_TRIANGLE) && !(previous_buttons & PSP_CTRL_TRIANGLE)) break;
        if ((pad.Buttons & PSP_CTRL_SELECT) && !(previous_buttons & PSP_CTRL_SELECT) && !state.detail) {
            state.grid = !state.grid;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_START) && !(previous_buttons & PSP_CTRL_START) && !state.detail) {
            state.sort = (pspl_sort_t)((state.sort + 1) % 3);
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_LTRIGGER) && !(previous_buttons & PSP_CTRL_LTRIGGER) && !state.detail) {
            change_filter(&state, -1);
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_RTRIGGER) && !(previous_buttons & PSP_CTRL_RTRIGGER) && !state.detail) {
            change_filter(&state, 1);
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
            generate_test_session(library, indices, count, state.selected);
            if (!demo_mode) pspl_storage_save(database_path, library);
            dirty = 1;
        }
        if (!state.detail) {
            unsigned int direction = repeated_direction(pad.Buttons, &previous_buttons, &direction_ticks);
            if (direction != 0) {
                move_selection(&state, count, direction);
                dirty = 1;
            }
        } else {
            previous_buttons = pad.Buttons;
            direction_ticks = 0;
        }
        sceKernelDelayThread(16000);
    }
    free(indices);
    pspl_image_cache_free(&images);
    pspl_gfx_shutdown(&gfx);
    return 0;
}
