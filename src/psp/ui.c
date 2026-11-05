#include "psplibrary/ui.h"

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspuser.h>

#include <stdio.h>
#include <string.h>

#include "psplibrary/storage.h"

#define PSPL_FRAMEBUFFER_STRIDE 512
#define PSPL_FRAMEBUFFER_HEIGHT 272
#define PSPL_FRAMEBUFFER_BYTES (PSPL_FRAMEBUFFER_STRIDE * PSPL_FRAMEBUFFER_HEIGHT * 4)

typedef struct {
    int detail;
    int grid;
    size_t selected;
    pspl_platform_t filter;
    pspl_sort_t sort;
} ui_state_t;

static const pspl_platform_t filter_order[] = {
    PSPL_PLATFORM_ALL, PSPL_PLATFORM_PSP, PSPL_PLATFORM_PS1,
    PSPL_PLATFORM_GBA, PSPL_PLATFORM_GB, PSPL_PLATFORM_GBC,
    PSPL_PLATFORM_NES, PSPL_PLATFORM_SNES, PSPL_PLATFORM_OTHER
};

static void clear_screen(void)
{
    pspDebugScreenSetBackColor(0x00101820);
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenSetTextColor(0x00E8F0F8);
}

static void draw_header(const ui_state_t *state, int demo_mode, int count)
{
    const char *sort_name = state->sort == PSPL_SORT_TITLE ? "TITLE" :
                            state->sort == PSPL_SORT_TOTAL_TIME ? "TIME" : "LAST PLAYED";
    pspDebugScreenSetTextColor(0x0068D8FF);
    pspDebugScreenPrintf("PSP LIBRARY  v0.1   %s\n", demo_mode ? "[DEMO DATA]" : "[USER DATA]");
    pspDebugScreenSetTextColor(0x00E8F0F8);
    pspDebugScreenPrintf("Filter: %-5s  Sort: %-10s  Games: %02d\n", pspl_platform_name(state->filter), sort_name, count);
    pspDebugScreenPrintf("X details  SELECT grid/list  START sort  L/R platform  SQUARE test event  TRIANGLE exit\n\n");
}

static void draw_library(const pspl_library_t *library, ui_state_t *state, int demo_mode,
                         size_t *indices, int count)
{
    int i;
    char duration[16];
    clear_screen();
    draw_header(state, demo_mode, count);
    if (count == 0) {
        pspDebugScreenPrintf("No games match this platform filter.\n");
        return;
    }
    if (state->grid) {
        for (i = 0; i < count; ++i) {
            const pspl_game_t *game = &library->games[indices[i]];
            int column = i % 3;
            int row = i / 3;
            pspDebugScreenSetXY(column * 20, 5 + row * 4);
            pspDebugScreenSetTextColor(i == (int)state->selected ? 0x00FFD060 : 0x00E8F0F8);
            pspDebugScreenPrintf("[%s]", game->platform);
            pspDebugScreenSetXY(column * 20, 6 + row * 4);
            pspDebugScreenPrintf("%-19.19s", game->title);
            pspDebugScreenSetXY(column * 20, 7 + row * 4);
            pspDebugScreenPrintf("%s", pspl_format_duration(game->total_seconds, duration, sizeof(duration)));
            pspDebugScreenSetXY(column * 20, 8 + row * 4);
            pspDebugScreenPrintf("%s", game->cover_path[0] ? "[cover]" : "[no cover]");
        }
    } else {
        for (i = 0; i < count; ++i) {
            const pspl_game_t *game = &library->games[indices[i]];
            pspDebugScreenSetTextColor(i == (int)state->selected ? 0x00FFD060 : 0x00E8F0F8);
            pspDebugScreenPrintf("%c %-31.31s %-5s %s  %s\n", i == (int)state->selected ? '>' : ' ',
                                 game->title, game->platform,
                                 pspl_format_duration(game->total_seconds, duration, sizeof(duration)),
                                 game->file_present ? "present" : "missing");
        }
    }
}

static void draw_detail(const pspl_library_t *library, const ui_state_t *state, int demo_mode,
                        size_t *indices, int count)
{
    const pspl_game_t *game;
    char duration[16];
    clear_screen();
    if (count <= 0 || state->selected >= (size_t)count) {
        pspDebugScreenPrintf("No game selected for this filter.\n");
        return;
    }
    game = &library->games[indices[state->selected]];
    pspDebugScreenSetTextColor(0x0068D8FF);
    pspDebugScreenPrintf("GAME DETAILS %s\n\n", demo_mode ? "[DEMO DATA]" : "[USER DATA]");
    pspDebugScreenSetTextColor(0x00E8F0F8);
    pspDebugScreenPrintf("%s\n", game->title);
    pspDebugScreenPrintf("Platform:       %s\n", game->platform);
    pspDebugScreenPrintf("Game ID:        %s\n", game->id);
    pspDebugScreenPrintf("Content key:    %s\n", game->content_key);
    pspDebugScreenPrintf("Total time:     %s (%lu sec)\n", pspl_format_duration(game->total_seconds, duration, sizeof(duration)), (unsigned long)game->total_seconds);
    pspDebugScreenPrintf("Sessions:       %lu\n", (unsigned long)game->session_count);
    pspDebugScreenPrintf("First launch:   %lu\n", (unsigned long)game->first_started_at);
    pspDebugScreenPrintf("Last launch:    %lu\n", (unsigned long)game->last_started_at);
    pspDebugScreenPrintf("File status:    %s\n", game->file_present ? "present" : "not present / unknown");
    pspDebugScreenPrintf("Cover:          %s\n\n", game->cover_path[0] ? game->cover_path : "placeholder");
    pspDebugScreenPrintf("Session history:\n");
    {
        int shown = 0;
        int i;
        for (i = (int)library->session_count - 1; i >= 0 && shown < 4; --i) {
            const pspl_session_t *session = &library->sessions[i];
            if (strcmp(session->game_id, game->id) == 0) {
                pspDebugScreenPrintf("  %lu -> %lu  %s\n", (unsigned long)session->started_at,
                                     (unsigned long)session->ended_at,
                                     pspl_format_duration(session->duration_seconds, duration, sizeof(duration)));
                ++shown;
            }
        }
        if (shown == 0) pspDebugScreenPrintf("  no completed sessions\n");
    }
    pspDebugScreenPrintf("\n");
    pspDebugScreenPrintf("O back   SQUARE generate test session\n");
}

static size_t clamp_selection(size_t selected, int count)
{
    if (count <= 0) return 0;
    return selected >= (size_t)count ? (size_t)count - 1 : selected;
}

static void change_filter(ui_state_t *state, int direction)
{
    size_t i;
    for (i = 0; i < sizeof(filter_order) / sizeof(filter_order[0]); ++i) {
        if (filter_order[i] == state->filter) {
            int next = (int)i + direction;
            int count = (int)(sizeof(filter_order) / sizeof(filter_order[0]));
            if (next < 0) next = count - 1;
            if (next >= count) next = 0;
            state->filter = filter_order[next];
            state->selected = 0;
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

int pspl_ui_run(pspl_library_t *library, const char *database_path, int demo_mode)
{
    ui_state_t state;
    size_t indices[PSPL_MAX_GAMES];
    size_t previous_selected;
    int count;
    int dirty = 1;
    int displayed_offset = 0;
    unsigned int vram_base_address;
    SceCtrlData pad;
    unsigned int previous = 0;

    memset(&state, 0, sizeof(state));
    state.grid = 1;
    state.filter = PSPL_PLATFORM_ALL;
    vram_base_address = 0x40000000U | (unsigned int)sceGeEdramGetAddr();
    pspDebugScreenSetBase((u32 *)vram_base_address);
    for (;;) {
        count = pspl_library_filter_sort(library, state.filter, state.sort, indices, PSPL_MAX_GAMES);
        previous_selected = state.selected;
        state.selected = clamp_selection(state.selected, count);
        if (state.selected != previous_selected) dirty = 1;
        if (dirty) {
            int draw_offset = displayed_offset == 0 ? PSPL_FRAMEBUFFER_BYTES : 0;
            pspDebugScreenSetOffset(draw_offset);
            if (state.detail) draw_detail(library, &state, demo_mode, indices, count);
            else draw_library(library, &state, demo_mode, indices, count);
            if (sceDisplaySetFrameBuf((void *)(vram_base_address + (unsigned int)draw_offset),
                                      PSPL_FRAMEBUFFER_STRIDE,
                                      PSP_DISPLAY_PIXEL_FORMAT_8888,
                                      PSP_DISPLAY_SETBUF_NEXTVSYNC) >= 0) {
                sceDisplayWaitVblankStart();
                displayed_offset = draw_offset;
                dirty = 0;
            }
        }
        sceCtrlReadBufferPositive(&pad, 1);
        if ((pad.Buttons & PSP_CTRL_TRIANGLE) && !(previous & PSP_CTRL_TRIANGLE)) break;
        if ((pad.Buttons & PSP_CTRL_SELECT) && !(previous & PSP_CTRL_SELECT)) {
            state.grid = !state.grid;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_START) && !(previous & PSP_CTRL_START)) {
            state.sort = (state.sort + 1) % 3;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_LTRIGGER) && !(previous & PSP_CTRL_LTRIGGER)) {
            change_filter(&state, -1);
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_RTRIGGER) && !(previous & PSP_CTRL_RTRIGGER)) {
            change_filter(&state, 1);
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_CROSS) && !(previous & PSP_CTRL_CROSS)) {
            state.detail = 1;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_CIRCLE) && !(previous & PSP_CTRL_CIRCLE)) {
            state.detail = 0;
            dirty = 1;
        }
        if ((pad.Buttons & PSP_CTRL_SQUARE) && !(previous & PSP_CTRL_SQUARE)) {
            generate_test_session(library, indices, count, state.selected);
            if (!demo_mode) pspl_storage_save(database_path, library);
            dirty = 1;
        }
        if (!state.detail && (pad.Buttons & PSP_CTRL_UP) && !(previous & PSP_CTRL_UP) && state.selected > 0) {
            --state.selected;
            dirty = 1;
        }
        if (!state.detail && (pad.Buttons & PSP_CTRL_DOWN) && !(previous & PSP_CTRL_DOWN) && state.selected + 1 < (size_t)count) {
            ++state.selected;
            dirty = 1;
        }
        if (!state.detail && (pad.Buttons & PSP_CTRL_LEFT) && !(previous & PSP_CTRL_LEFT)) {
            if (state.grid && state.selected >= 3) {
                state.selected -= 3;
                dirty = 1;
            }
        }
        if (!state.detail && (pad.Buttons & PSP_CTRL_RIGHT) && !(previous & PSP_CTRL_RIGHT)) {
            if (state.grid && state.selected + 3 < (size_t)count) {
                state.selected += 3;
                dirty = 1;
            }
        }
        previous = pad.Buttons;
        sceKernelDelayThread(50000);
    }
    return 0;
}
