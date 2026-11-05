#include "psplibrary/storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define PSPL_DB_BUFFER_SIZE 65536

static uint32_t crc32_bytes(const unsigned char *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFU;
    size_t i;
    int bit;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int)(crc & 1U));
    }
    return ~crc;
}

static int append_line(char *buffer, size_t capacity, size_t *used, const char *format, ...)
{
    va_list args;
    int written;
    if (*used >= capacity) return -1;
    va_start(args, format);
    written = vsnprintf(buffer + *used, capacity - *used, format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= capacity - *used) return -1;
    *used += (size_t)written;
    return 0;
}

static int encode_field(const char *source, char *destination, size_t capacity)
{
    size_t used = 0;
    size_t i;
    if (source == NULL || destination == NULL || capacity == 0) return -1;
    for (i = 0; source[i] != '\0'; ++i) {
        const char *replacement = NULL;
        if (source[i] == '|') replacement = "%7C";
        else if (source[i] == '%') replacement = "%25";
        else if (source[i] == '\n') replacement = "%0A";
        if (replacement != NULL) {
            size_t length = strlen(replacement);
            if (used + length >= capacity) return -1;
            memcpy(destination + used, replacement, length);
            used += length;
        } else {
            if (used + 1 >= capacity) return -1;
            destination[used++] = source[i];
        }
    }
    destination[used] = '\0';
    return 0;
}

static void decode_field(char *text)
{
    char *read = text;
    char *write = text;
    while (*read != '\0') {
        if (read[0] == '%' && read[1] == '2' && read[2] == '5') {
            *write++ = '%'; read += 3;
        } else if (read[0] == '%' && read[1] == '7' && read[2] == 'C') {
            *write++ = '|'; read += 3;
        } else if (read[0] == '%' && read[1] == '0' && read[2] == 'A') {
            *write++ = '\n'; read += 3;
        } else {
            *write++ = *read++;
        }
    }
    *write = '\0';
}

int pspl_storage_exists(const char *path)
{
    if (path == NULL) return 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) return 0;
    fclose(file);
    return 1;
}

int pspl_storage_save(const char *path, const pspl_library_t *library)
{
    static char buffer[PSPL_DB_BUFFER_SIZE];
    char temporary[256];
    char backup[256];
    size_t used = 0;
    size_t i;
    uint32_t checksum;
    FILE *file;
    int path_length;

    if (path == NULL || library == NULL ||
        library->game_count > PSPL_MAX_GAMES || library->session_count > PSPL_MAX_SESSIONS) return -1;
    if (append_line(buffer, sizeof(buffer), &used, "PSPLIB %d\n", PSPL_DB_VERSION) != 0) return -2;
    if (append_line(buffer, sizeof(buffer), &used, "GAMES %lu\n", (unsigned long)library->game_count) != 0) return -2;
    for (i = 0; i < library->game_count; ++i) {
        const pspl_game_t *game = &library->games[i];
        char id[PSPL_ID_LEN * 2];
        char title[PSPL_TITLE_LEN * 2];
        char platform[PSPL_PLATFORM_LEN * 2];
        char content_key[PSPL_CONTENT_KEY_LEN * 2];
        char version[PSPL_VERSION_LEN * 2];
        char cover_path[PSPL_COVER_PATH_LEN * 2];
        if (encode_field(game->id, id, sizeof(id)) != 0 ||
            encode_field(game->title, title, sizeof(title)) != 0 ||
            encode_field(game->platform, platform, sizeof(platform)) != 0 ||
            encode_field(game->content_key, content_key, sizeof(content_key)) != 0 ||
            encode_field(game->version, version, sizeof(version)) != 0 ||
            encode_field(game->cover_path, cover_path, sizeof(cover_path)) != 0) return -2;
        if (append_line(buffer, sizeof(buffer), &used,
                        "G|%s|%s|%s|%s|%s|%lu|%lu|%lu|%lu|%u|%s\n",
                        id, title, platform, content_key, version,
                        (unsigned long)game->total_seconds, (unsigned long)game->session_count,
                        (unsigned long)game->first_started_at, (unsigned long)game->last_started_at,
                        (unsigned int)game->file_present, cover_path) != 0) return -2;
    }
    if (append_line(buffer, sizeof(buffer), &used, "SESSIONS %lu\n", (unsigned long)library->session_count) != 0) return -2;
    for (i = 0; i < library->session_count; ++i) {
        const pspl_session_t *session = &library->sessions[i];
        char game_id[PSPL_ID_LEN * 2];
        if (encode_field(session->game_id, game_id, sizeof(game_id)) != 0) return -2;
        if (append_line(buffer, sizeof(buffer), &used, "S|%s|%lu|%lu|%lu\n",
                        game_id, (unsigned long)session->started_at,
                        (unsigned long)session->ended_at, (unsigned long)session->duration_seconds) != 0) return -2;
    }
    checksum = crc32_bytes((const unsigned char *)buffer, used);
    if (append_line(buffer, sizeof(buffer), &used, "CHECKSUM %08lX\n", (unsigned long)checksum) != 0) return -2;

    path_length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (path_length < 0 || (size_t)path_length >= sizeof(temporary)) return -3;
    path_length = snprintf(backup, sizeof(backup), "%s.bak", path);
    if (path_length < 0 || (size_t)path_length >= sizeof(backup)) return -3;
    file = fopen(temporary, "wb");
    if (file == NULL) return -3;
    if (fwrite(buffer, 1, used, file) != used) {
        fclose(file);
        remove(temporary);
        return -4;
    }
    if (fflush(file) != 0) {
        fclose(file);
        remove(temporary);
        return -4;
    }
    if (fclose(file) != 0) {
        remove(temporary);
        return -4;
    }
    remove(backup);
    if (pspl_storage_exists(path) && rename(path, backup) != 0) {
        remove(temporary);
        return -5;
    }
    if (rename(temporary, path) != 0) {
        if (pspl_storage_exists(backup)) rename(backup, path);
        remove(temporary);
        return -6;
    }
    return 0;
}

static char *next_field(char **cursor)
{
    char *field = *cursor;
    char *separator;
    if (field == NULL) return NULL;
    separator = strchr(field, '|');
    if (separator != NULL) {
        *separator = '\0';
        *cursor = separator + 1;
    } else {
        *cursor = NULL;
    }
    return field;
}

static int parse_unsigned(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;
    if (text == NULL || value == NULL || *text == '\0') return -1;
    parsed = strtoul(text, &end, 10);
    if (*end != '\0' || parsed > 0xFFFFFFFFUL) return -1;
    *value = (uint32_t)parsed;
    return 0;
}

typedef struct {
    char old_id[PSPL_ID_LEN];
    char new_id[PSPL_ID_LEN];
} legacy_id_map_t;

static pspl_game_t *find_identity(pspl_library_t *library, const char *platform,
                                  const char *content_key)
{
    size_t i;
    for (i = 0; i < library->game_count; ++i) {
        if (strcmp(library->games[i].platform, platform) == 0 &&
            strcmp(library->games[i].content_key, content_key) == 0) {
            return &library->games[i];
        }
    }
    return NULL;
}

static void merge_game(pspl_game_t *destination, const pspl_game_t *source)
{
    if (source->total_seconds > UINT32_MAX - destination->total_seconds)
        destination->total_seconds = UINT32_MAX;
    else
        destination->total_seconds += source->total_seconds;
    if (source->session_count > UINT32_MAX - destination->session_count)
        destination->session_count = UINT32_MAX;
    else
        destination->session_count += source->session_count;
    if (destination->first_started_at == 0 ||
        (source->first_started_at != 0 && source->first_started_at < destination->first_started_at))
        destination->first_started_at = source->first_started_at;
    if (source->last_started_at > destination->last_started_at) {
        destination->last_started_at = source->last_started_at;
        if (source->version[0] != '\0')
            snprintf(destination->version, sizeof(destination->version), "%s", source->version);
    }
    if (source->file_present) destination->file_present = 1;
    if (destination->cover_path[0] == '\0' && source->cover_path[0] != '\0')
        snprintf(destination->cover_path, sizeof(destination->cover_path), "%s", source->cover_path);
    if (destination->version[0] == '\0' && source->version[0] != '\0')
        snprintf(destination->version, sizeof(destination->version), "%s", source->version);
}

static void migrate_v1_identity(pspl_game_t *game)
{
    const char *body;
    const char *separator;
    char stable_key[PSPL_CONTENT_KEY_LEN];
    if (strcmp(game->platform, "PSP") != 0 || strncmp(game->content_key, "PSP:", 4) != 0)
        return;
    body = game->content_key + 4;
    separator = strchr(body, ':');
    /* Old tracker keys were PSP:<content-id>:<version>. Only migrate keys
       whose first component looks like a region-aware PSP content id. */
    if (separator == NULL || strchr(body, '-') == NULL || separator[1] == '\0') return;
    if ((size_t)(separator - game->content_key) >= sizeof(stable_key)) return;
    memcpy(stable_key, game->content_key, (size_t)(separator - game->content_key));
    stable_key[separator - game->content_key] = '\0';
    snprintf(game->version, sizeof(game->version), "%s", separator + 1);
    snprintf(game->content_key, sizeof(game->content_key), "%s", stable_key);
}

static const char *mapped_id(const legacy_id_map_t *maps, size_t count, const char *old_id)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        if (strcmp(maps[i].old_id, old_id) == 0) return maps[i].new_id;
    }
    return old_id;
}

int pspl_storage_load(const char *path, pspl_library_t *library)
{
    FILE *file;
    static unsigned char buffer[PSPL_DB_BUFFER_SIZE];
    size_t length;
    char *line;
    char *cursor;
    char *checksum_line = NULL;
    char *expected_text;
    unsigned long expected_checksum;
    uint32_t expected_games;
    uint32_t expected_sessions;
    uint32_t loaded_games = 0;
    uint32_t loaded_sessions = 0;
    if (path == NULL || library == NULL) return -1;
    file = fopen(path, "rb");
    if (file == NULL) return -2;
    length = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    if (length == 0 || length >= sizeof(buffer)) return -3;
    buffer[length] = '\0';
    checksum_line = strstr((char *)buffer, "\nCHECKSUM ");
    if (checksum_line == NULL) return -4;
    ++checksum_line;
    expected_text = checksum_line + 9;
    if (sscanf(expected_text, "%lx", &expected_checksum) != 1) return -4;
    *checksum_line = '\0';
    if (crc32_bytes(buffer, (size_t)(checksum_line - (char *)buffer)) != (uint32_t)expected_checksum) return -5;

    pspl_library_init(library);
    line = strtok((char *)buffer, "\n");
    if (line == NULL) return -6;
    {
        int db_version;
        legacy_id_map_t maps[PSPL_MAX_GAMES];
        size_t map_count = 0;
        if (strcmp(line, "PSPLIB 1") == 0) db_version = 1;
        else if (strcmp(line, "PSPLIB 2") == 0) db_version = 2;
        else return -6;
    line = strtok(NULL, "\n");
    if (line == NULL || strncmp(line, "GAMES ", 6) != 0 ||
        parse_unsigned(line + 6, &expected_games) != 0 || expected_games > PSPL_MAX_GAMES) return -6;
    line = strtok(NULL, "\n");
    while (line != NULL && strncmp(line, "SESSIONS ", 9) != 0 && loaded_games < expected_games) {
        pspl_game_t game;
        char old_id[PSPL_ID_LEN];
        char canonical_id[PSPL_ID_LEN];
        char *field;
        uint32_t number;
        if (line[0] != 'G' || line[1] != '|') return -6;
        memset(&game, 0, sizeof(game));
        cursor = line + 2;
        field = next_field(&cursor); if (field == NULL) return -6; decode_field(field); strncpy(game.id, field, sizeof(game.id) - 1);
        field = next_field(&cursor); if (field == NULL) return -6; decode_field(field); strncpy(game.title, field, sizeof(game.title) - 1);
        field = next_field(&cursor); if (field == NULL) return -6; decode_field(field); strncpy(game.platform, field, sizeof(game.platform) - 1);
        field = next_field(&cursor); if (field == NULL) return -6; decode_field(field); strncpy(game.content_key, field, sizeof(game.content_key) - 1);
        if (db_version >= 2) {
            field = next_field(&cursor); if (field == NULL) return -6; decode_field(field); strncpy(game.version, field, sizeof(game.version) - 1);
        }
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -6; game.total_seconds = number;
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -6; game.session_count = number;
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -6; game.first_started_at = number;
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -6; game.last_started_at = number;
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -6; game.file_present = (uint8_t)number;
        field = next_field(&cursor); if (field == NULL || cursor != NULL) return -6; decode_field(field); strncpy(game.cover_path, field, sizeof(game.cover_path) - 1);
        snprintf(old_id, sizeof(old_id), "%s", game.id);
        if (db_version == 1) migrate_v1_identity(&game);
        if (pspl_make_game_id(pspl_platform_from_name(game.platform), game.content_key, canonical_id) != 0) return -7;
        snprintf(game.id, sizeof(game.id), "%s", canonical_id);
        {
            pspl_game_t *existing = find_identity(library, game.platform, game.content_key);
            if (existing != NULL) {
                merge_game(existing, &game);
                snprintf(canonical_id, sizeof(canonical_id), "%s", existing->id);
            } else if (pspl_library_add_game(library, &game) != 0) {
                return -7;
            }
        }
        if (map_count < PSPL_MAX_GAMES) {
            snprintf(maps[map_count].old_id, sizeof(maps[map_count].old_id), "%s", old_id);
            snprintf(maps[map_count].new_id, sizeof(maps[map_count].new_id), "%s", canonical_id);
            ++map_count;
        }
        ++loaded_games;
        line = strtok(NULL, "\n");
    }
    if (loaded_games != expected_games || line == NULL || strncmp(line, "SESSIONS ", 9) != 0 ||
        parse_unsigned(line + 9, &expected_sessions) != 0 || expected_sessions > PSPL_MAX_SESSIONS) return -6;
    line = strtok(NULL, "\n");
    while (line != NULL && loaded_sessions < expected_sessions) {
        pspl_session_t session;
        char *field;
        uint32_t number;
        if (line[0] != 'S' || line[1] != '|') return -6;
        memset(&session, 0, sizeof(session));
        cursor = line + 2;
        field = next_field(&cursor); if (field == NULL) return -6; decode_field(field); strncpy(session.game_id, field, sizeof(session.game_id) - 1);
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -6; session.started_at = number;
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -6; session.ended_at = number;
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0 || cursor != NULL) return -6; session.duration_seconds = number;
        snprintf(session.game_id, sizeof(session.game_id), "%s", mapped_id(maps, map_count, session.game_id));
        if (pspl_library_find_game_const(library, session.game_id) == NULL) return -8;
        library->sessions[library->session_count++] = session;
        ++loaded_sessions;
        line = strtok(NULL, "\n");
    }
    return loaded_sessions == expected_sessions && line == NULL ? 0 : -6;
    }
}
