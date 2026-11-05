#ifndef PSPLIBRARY_CORE_H
#define PSPLIBRARY_CORE_H

#include <stddef.h>
#include <stdint.h>

#define PSPL_INITIAL_GAME_CAPACITY 16
#define PSPL_INITIAL_SESSION_CAPACITY 64
#define PSPL_ID_LEN 17
#define PSPL_TITLE_LEN 64
#define PSPL_PLATFORM_LEN 16
#define PSPL_CONTENT_KEY_LEN 96
#define PSPL_VERSION_LEN 32
#define PSPL_COVER_PATH_LEN 128

typedef enum {
    PSPL_PLATFORM_ALL = 0,
    PSPL_PLATFORM_PSP,
    PSPL_PLATFORM_PS1,
    PSPL_PLATFORM_GBA,
    PSPL_PLATFORM_GB,
    PSPL_PLATFORM_GBC,
    PSPL_PLATFORM_NES,
    PSPL_PLATFORM_SNES,
    PSPL_PLATFORM_OTHER
} pspl_platform_t;

typedef enum {
    PSPL_EVENT_GAME_STARTED = 1,
    PSPL_EVENT_GAME_STOPPED,
    PSPL_EVENT_GAME_SWITCHED
} pspl_event_type_t;

typedef struct {
    char game_id[PSPL_ID_LEN];
    uint32_t started_at;
    uint32_t ended_at;
    uint32_t duration_seconds;
} pspl_session_t;

typedef struct {
    char id[PSPL_ID_LEN];
    char title[PSPL_TITLE_LEN];
    char platform[PSPL_PLATFORM_LEN];
    char content_key[PSPL_CONTENT_KEY_LEN];
    char version[PSPL_VERSION_LEN];
    uint32_t total_seconds;
    uint32_t session_count;
    uint32_t first_started_at;
    uint32_t last_started_at;
    uint8_t file_present;
    char cover_path[PSPL_COVER_PATH_LEN];
} pspl_game_t;

typedef struct {
    pspl_game_t *games;
    size_t game_count;
    size_t game_capacity;
    pspl_session_t *sessions;
    size_t session_count;
    size_t session_capacity;
    char active_game_id[PSPL_ID_LEN];
    uint32_t active_started_at;
} pspl_library_t;

typedef struct {
    pspl_event_type_t type;
    const char *game_id;
    uint32_t timestamp;
} pspl_event_t;

typedef enum {
    PSPL_SORT_TITLE = 0,
    PSPL_SORT_TOTAL_TIME,
    PSPL_SORT_LAST_PLAYED
} pspl_sort_t;

void pspl_library_init(pspl_library_t *library);
void pspl_library_free(pspl_library_t *library);
const char *pspl_platform_name(pspl_platform_t platform);
pspl_platform_t pspl_platform_from_name(const char *name);
int pspl_make_game_id(pspl_platform_t platform, const char *content_key,
                      char out_id[PSPL_ID_LEN]);
int pspl_library_add_game(pspl_library_t *library, const pspl_game_t *game);
int pspl_library_append_session(pspl_library_t *library, const pspl_session_t *session);
pspl_game_t *pspl_library_find_game(pspl_library_t *library, const char *id);
const pspl_game_t *pspl_library_find_game_const(const pspl_library_t *library,
                                                const char *id);
int pspl_library_handle_event(pspl_library_t *library, const pspl_event_t *event);
int pspl_library_filter_sort(const pspl_library_t *library,
                             pspl_platform_t platform,
                             pspl_sort_t sort,
                             size_t *indices,
                             size_t capacity);
uint32_t pspl_library_now(void);
const char *pspl_format_duration(uint32_t seconds, char *buffer, size_t size);

#endif
