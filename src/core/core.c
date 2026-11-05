#include "psplibrary/core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void copy_string(char *destination, size_t size, const char *source)
{
    if (size == 0) return;
    if (source == NULL) source = "";
    strncpy(destination, source, size - 1);
    destination[size - 1] = '\0';
}

void pspl_library_init(pspl_library_t *library)
{
    if (library != NULL) memset(library, 0, sizeof(*library));
}

const char *pspl_platform_name(pspl_platform_t platform)
{
    switch (platform) {
    case PSPL_PLATFORM_PSP: return "PSP";
    case PSPL_PLATFORM_PS1: return "PS1";
    case PSPL_PLATFORM_GBA: return "GBA";
    case PSPL_PLATFORM_GB: return "GB";
    case PSPL_PLATFORM_GBC: return "GBC";
    case PSPL_PLATFORM_NES: return "NES";
    case PSPL_PLATFORM_SNES: return "SNES";
    case PSPL_PLATFORM_OTHER: return "OTHER";
    default: return "ALL";
    }
}

pspl_platform_t pspl_platform_from_name(const char *name)
{
    pspl_platform_t platform;
    if (name == NULL) return PSPL_PLATFORM_OTHER;
    for (platform = PSPL_PLATFORM_PSP; platform <= PSPL_PLATFORM_OTHER; ++platform) {
        if (strcmp(name, pspl_platform_name(platform)) == 0) return platform;
    }
    return PSPL_PLATFORM_OTHER;
}

int pspl_make_game_id(pspl_platform_t platform, const char *content_key,
                      char out_id[PSPL_ID_LEN])
{
    uint64_t hash = 1469598103934665603ULL;
    const char *platform_name = pspl_platform_name(platform);
    const unsigned char *part;

    if (content_key == NULL || out_id == NULL || platform == PSPL_PLATFORM_ALL) return -1;
    for (part = (const unsigned char *)platform_name; *part; ++part) {
        hash ^= *part;
        hash *= 1099511628211ULL;
    }
    hash ^= ':';
    hash *= 1099511628211ULL;
    for (part = (const unsigned char *)content_key; *part; ++part) {
        hash ^= *part;
        hash *= 1099511628211ULL;
    }
    snprintf(out_id, PSPL_ID_LEN, "%016llX", (unsigned long long)hash);
    return 0;
}

int pspl_library_add_game(pspl_library_t *library, const pspl_game_t *game)
{
    if (library == NULL || game == NULL || game->id[0] == '\0') return -1;
    if (pspl_library_find_game(library, game->id) != NULL) return -2;
    if (library->game_count >= PSPL_MAX_GAMES) return -3;
    library->games[library->game_count] = *game;
    library->games[library->game_count].title[PSPL_TITLE_LEN - 1] = '\0';
    library->games[library->game_count].platform[PSPL_PLATFORM_LEN - 1] = '\0';
    library->games[library->game_count].content_key[PSPL_CONTENT_KEY_LEN - 1] = '\0';
    library->games[library->game_count].cover_path[PSPL_COVER_PATH_LEN - 1] = '\0';
    library->game_count++;
    return 0;
}

pspl_game_t *pspl_library_find_game(pspl_library_t *library, const char *id)
{
    size_t i;
    if (library == NULL || id == NULL) return NULL;
    for (i = 0; i < library->game_count; ++i) {
        if (strcmp(library->games[i].id, id) == 0) return &library->games[i];
    }
    return NULL;
}

const pspl_game_t *pspl_library_find_game_const(const pspl_library_t *library,
                                                const char *id)
{
    size_t i;
    if (library == NULL || id == NULL) return NULL;
    for (i = 0; i < library->game_count; ++i) {
        if (strcmp(library->games[i].id, id) == 0) return &library->games[i];
    }
    return NULL;
}

static int close_active(pspl_library_t *library, uint32_t ended_at)
{
    pspl_game_t *game;
    pspl_session_t *session;
    uint32_t duration;

    if (library->active_game_id[0] == '\0') return 0;
    game = pspl_library_find_game(library, library->active_game_id);
    if (game == NULL) return -1;
    if (library->session_count >= PSPL_MAX_SESSIONS) return -5;
    if (ended_at < library->active_started_at) ended_at = library->active_started_at;
    duration = ended_at - library->active_started_at;
    game->total_seconds += duration;
    game->session_count++;
    if (library->session_count < PSPL_MAX_SESSIONS) {
        session = &library->sessions[library->session_count++];
        copy_string(session->game_id, sizeof(session->game_id), game->id);
        session->started_at = library->active_started_at;
        session->ended_at = ended_at;
        session->duration_seconds = duration;
    }
    library->active_game_id[0] = '\0';
    library->active_started_at = 0;
    return 0;
}

int pspl_library_handle_event(pspl_library_t *library, const pspl_event_t *event)
{
    pspl_game_t *game;
    if (library == NULL || event == NULL || event->game_id == NULL) return -1;
    game = pspl_library_find_game(library, event->game_id);
    if (game == NULL) return -2;

    switch (event->type) {
    case PSPL_EVENT_GAME_STARTED:
        if (library->active_game_id[0] != '\0' &&
            strcmp(library->active_game_id, event->game_id) == 0) {
            return 0;
        }
        if (library->active_game_id[0] != '\0' &&
            strcmp(library->active_game_id, event->game_id) != 0) {
            if (close_active(library, event->timestamp) != 0) return -3;
        }
        copy_string(library->active_game_id, sizeof(library->active_game_id), event->game_id);
        library->active_started_at = event->timestamp;
        if (game->first_started_at == 0) game->first_started_at = event->timestamp;
        game->last_started_at = event->timestamp;
        return 0;
    case PSPL_EVENT_GAME_STOPPED:
        if (library->active_game_id[0] == '\0') return 0;
        if (strcmp(library->active_game_id, event->game_id) != 0) return -4;
        return close_active(library, event->timestamp);
    case PSPL_EVENT_GAME_SWITCHED:
        if (library->active_game_id[0] != '\0' &&
            strcmp(library->active_game_id, event->game_id) == 0) {
            return 0;
        }
        if (library->active_game_id[0] != '\0' && close_active(library, event->timestamp) != 0) {
            return -3;
        }
        copy_string(library->active_game_id, sizeof(library->active_game_id), event->game_id);
        library->active_started_at = event->timestamp;
        if (game->first_started_at == 0) game->first_started_at = event->timestamp;
        game->last_started_at = event->timestamp;
        return 0;
    default:
        return -5;
    }
}

static int compare_games(const pspl_library_t *library, size_t left, size_t right,
                         pspl_sort_t sort)
{
    const pspl_game_t *a = &library->games[left];
    const pspl_game_t *b = &library->games[right];
    if (sort == PSPL_SORT_TOTAL_TIME && a->total_seconds != b->total_seconds)
        return a->total_seconds > b->total_seconds ? -1 : 1;
    if (sort == PSPL_SORT_LAST_PLAYED && a->last_started_at != b->last_started_at)
        return a->last_started_at > b->last_started_at ? -1 : 1;
    return strcmp(a->title, b->title);
}

int pspl_library_filter_sort(const pspl_library_t *library,
                             pspl_platform_t platform,
                             pspl_sort_t sort,
                             size_t *indices,
                             size_t capacity)
{
    size_t i;
    size_t count = 0;
    if (library == NULL || indices == NULL) return -1;
    for (i = 0; i < library->game_count && count < capacity; ++i) {
        if (platform == PSPL_PLATFORM_ALL ||
            pspl_platform_from_name(library->games[i].platform) == platform) {
            indices[count++] = i;
        }
    }
    for (i = 1; i < count; ++i) {
        size_t value = indices[i];
        size_t j = i;
        while (j > 0 && compare_games(library, value, indices[j - 1], sort) < 0) {
            indices[j] = indices[j - 1];
            --j;
        }
        indices[j] = value;
    }
    return (int)count;
}

uint32_t pspl_library_now(void)
{
    time_t current = time(NULL);
    return current < 0 ? 0U : (uint32_t)current;
}

const char *pspl_format_duration(uint32_t seconds, char *buffer, size_t size)
{
    uint32_t hours = seconds / 3600U;
    uint32_t minutes = (seconds % 3600U) / 60U;
    if (size == 0) return buffer;
    snprintf(buffer, size, "%02lu:%02lu", (unsigned long)hours, (unsigned long)minutes);
    return buffer;
}
