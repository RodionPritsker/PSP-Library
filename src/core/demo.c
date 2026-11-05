#include "psplibrary/demo.h"

#include <stdio.h>
#include <string.h>

static void add_demo_game(pspl_library_t *library, pspl_platform_t platform,
                          const char *title, const char *key, uint32_t seconds,
                          uint32_t last, const char *cover)
{
    pspl_game_t game;
    memset(&game, 0, sizeof(game));
    pspl_make_game_id(platform, key, game.id);
    snprintf(game.title, sizeof(game.title), "%s", title);
    snprintf(game.platform, sizeof(game.platform), "%s", pspl_platform_name(platform));
    snprintf(game.content_key, sizeof(game.content_key), "%s", key);
    snprintf(game.cover_path, sizeof(game.cover_path), "%s", cover);
    game.total_seconds = seconds;
    game.session_count = seconds == 0 ? 0 : 3;
    game.first_started_at = last > 86400U * 30U ? last - 86400U * 30U : last;
    game.last_started_at = last;
    game.file_present = 0;
    if (pspl_library_add_game(library, &game) == 0) {
        pspl_game_t *stored = &library->games[library->game_count - 1];
        uint32_t part = seconds / 3U;
        int i;
        stored->total_seconds = 0;
        stored->session_count = 0;
        stored->first_started_at = 0;
        stored->last_started_at = 0;
        for (i = 0; i < 3; ++i) {
            pspl_event_t event;
            uint32_t started_at = last - (uint32_t)(2 - i) * 86400U;
            uint32_t duration = i == 2 ? seconds - part * 2U : part;
            event.type = PSPL_EVENT_GAME_STARTED;
            event.game_id = stored->id;
            event.timestamp = started_at;
            pspl_library_handle_event(library, &event);
            event.type = PSPL_EVENT_GAME_STOPPED;
            event.timestamp = started_at + duration;
            pspl_library_handle_event(library, &event);
        }
    }
}

void pspl_demo_populate(pspl_library_t *library, uint32_t now)
{
    pspl_library_init(library);
    add_demo_game(library, PSPL_PLATFORM_PSP, "Metal Gear Solid: Peace Walker",
                  "MGS-PW|US|1.00", 75600, now - 86400U * 2U, "covers/aurora-quest.png");
    add_demo_game(library, PSPL_PLATFORM_PSP, "Persona 3 Portable",
                  "P3P|US|1.00", 43200, now - 86400U * 6U, "covers/neon-racers.png");
    add_demo_game(library, PSPL_PLATFORM_GBA, "Pokemon Emerald",
                  "POKEMON-EMERALD|US|1.0", 28800, now - 86400U * 3U, "covers/ember-tactics.png");
    add_demo_game(library, PSPL_PLATFORM_GBA, "Metroid Fusion",
                  "METROID-FUSION|US|1.1", 14400, now - 86400U * 10U, "covers/metroid-fusion.png");
    add_demo_game(library, PSPL_PLATFORM_PS1, "Metal Gear Solid",
                  "MGS|US|DISC1|1.0", 21600, now - 86400U * 14U, "covers/mgs-ps1.png");
}

void pspl_demo_populate_large(pspl_library_t *library, uint32_t now)
{
    static const pspl_platform_t platforms[] = {
        PSPL_PLATFORM_PSP, PSPL_PLATFORM_PS1, PSPL_PLATFORM_GBA, PSPL_PLATFORM_GB,
        PSPL_PLATFORM_GBC, PSPL_PLATFORM_NES, PSPL_PLATFORM_SNES, PSPL_PLATFORM_OTHER
    };
    static const char *covers[] = {
        "covers/aurora-quest.png", "covers/neon-racers.png", "covers/ember-tactics.png",
        "covers/mgs-pw.png", "covers/p3p.png"
    };
    size_t i;
    pspl_library_init(library);
    for (i = 0; i < 100; ++i) {
        char title[PSPL_TITLE_LEN];
        char key[PSPL_CONTENT_KEY_LEN];
        pspl_platform_t platform = platforms[i % (sizeof(platforms) / sizeof(platforms[0]))];
        uint32_t seconds = (uint32_t)(900U + (i * 733U) % 72000U);
        uint32_t last = now - (uint32_t)((i * 3U + 1U) * 86400U);
        const char *cover = i % 7U == 0U ? covers[i % (sizeof(covers) / sizeof(covers[0]))] : "";
        snprintf(title, sizeof(title), "%s Archive %02lu", pspl_platform_name(platform),
                 (unsigned long)(i + 1));
        snprintf(key, sizeof(key), "DEMO-%s-%02lu|TEST|1.0", pspl_platform_name(platform),
                 (unsigned long)(i + 1));
        add_demo_game(library, platform, title, key, seconds, last, cover);
    }
}
