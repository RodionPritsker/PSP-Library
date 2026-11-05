#include "psplibrary/tracker.h"

#include <stdio.h>
#include <string.h>

static void copy_string(char *destination, size_t size, const char *source)
{
    if (size == 0) return;
    if (source == NULL) source = "";
    snprintf(destination, size, "%s", source);
}

void pspl_tracker_init(pspl_tracker_t *tracker, pspl_library_t *library)
{
    if (tracker == NULL) return;
    tracker->library = library;
}

int pspl_psp_build_content_key(const pspl_psp_launch_info_t *info,
                               char out_key[PSPL_CONTENT_KEY_LEN])
{
    const char *identity;
    int written;
    if (info == NULL || out_key == NULL) return -1;
    identity = info->content_id;
    if (identity == NULL || identity[0] == '\0') return -2;
    written = snprintf(out_key, PSPL_CONTENT_KEY_LEN, "PSP:%s", identity);
    if (written < 0 || written >= PSPL_CONTENT_KEY_LEN) return -3;
    return 0;
}

static int ensure_game(pspl_tracker_t *tracker, const pspl_psp_launch_info_t *info,
                       const char *content_key, char out_game_id[PSPL_ID_LEN])
{
    pspl_game_t game;
    pspl_game_t *existing;
    if (pspl_make_game_id(PSPL_PLATFORM_PSP, content_key, out_game_id) != 0) return -1;
    existing = pspl_library_find_game(tracker->library, out_game_id);
    if (existing != NULL) {
        if (info->title != NULL && info->title[0] != '\0') copy_string(existing->title, sizeof(existing->title), info->title);
        if (info->version != NULL && info->version[0] != '\0') copy_string(existing->version, sizeof(existing->version), info->version);
        if (info->file_present >= 0) existing->file_present = (uint8_t)info->file_present;
        if (info->cover_path != NULL && info->cover_path[0] != '\0') copy_string(existing->cover_path, sizeof(existing->cover_path), info->cover_path);
        return 0;
    }
    memset(&game, 0, sizeof(game));
    copy_string(game.id, sizeof(game.id), out_game_id);
    copy_string(game.title, sizeof(game.title), info->title != NULL && info->title[0] != '\0' ? info->title : "Unknown PSP game");
    copy_string(game.platform, sizeof(game.platform), pspl_platform_name(PSPL_PLATFORM_PSP));
    copy_string(game.content_key, sizeof(game.content_key), content_key);
    copy_string(game.version, sizeof(game.version), info->version);
    if (info->file_present >= 0) game.file_present = (uint8_t)info->file_present;
    copy_string(game.cover_path, sizeof(game.cover_path), info->cover_path);
    return pspl_library_add_game(tracker->library, &game);
}

int pspl_psp_tracker_start(pspl_tracker_t *tracker,
                           const pspl_psp_launch_info_t *info,
                           char out_game_id[PSPL_ID_LEN])
{
    char content_key[PSPL_CONTENT_KEY_LEN];
    pspl_event_t event;
    int result;
    if (tracker == NULL || tracker->library == NULL || info == NULL) return -1;
    result = pspl_psp_build_content_key(info, content_key);
    if (result != 0) return result;
    result = ensure_game(tracker, info, content_key, out_game_id);
    if (result != 0) return result;
    event.type = tracker->library->active_game_id[0] == '\0' ? PSPL_EVENT_GAME_STARTED : PSPL_EVENT_GAME_SWITCHED;
    event.game_id = out_game_id;
    event.timestamp = info->timestamp;
    return pspl_library_handle_event(tracker->library, &event);
}

int pspl_psp_tracker_stop(pspl_tracker_t *tracker, uint32_t timestamp)
{
    pspl_event_t event;
    if (tracker == NULL || tracker->library == NULL) return -1;
    if (tracker->library->active_game_id[0] == '\0') return 0;
    event.type = PSPL_EVENT_GAME_STOPPED;
    event.game_id = tracker->library->active_game_id;
    event.timestamp = timestamp;
    return pspl_library_handle_event(tracker->library, &event);
}
