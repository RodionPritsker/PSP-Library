#include "psplibrary/storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define PSPL_STORAGE_LINE_SIZE 1024

static uint32_t crc32_update(uint32_t crc, const unsigned char *data, size_t length)
{
    size_t i;
    int bit;
    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int)(crc & 1U));
    }
    return crc;
}

static int write_payload_line(FILE *file, uint32_t *crc, const char *format, ...)
{
    char line[PSPL_STORAGE_LINE_SIZE];
    va_list args;
    int written;

    va_start(args, format);
    written = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(line)) return -1;
    if (fwrite(line, 1, (size_t)written, file) != (size_t)written) return -1;
    *crc = crc32_update(*crc, (const unsigned char *)line, (size_t)written);
    return 0;
}

static int read_payload_line(FILE *file, char *line, size_t capacity, uint32_t *crc)
{
    size_t length;

    if (fgets(line, (int)capacity, file) == NULL) return 0;
    length = strlen(line);
    if (length == 0 || line[length - 1] != '\n') return -1;
    *crc = crc32_update(*crc, (const unsigned char *)line, length);
    line[length - 1] = '\0';
    if (length >= 2 && line[length - 2] == '\r') line[length - 2] = '\0';
    return 1;
}

static int read_unchecked_line(FILE *file, char *line, size_t capacity)
{
    size_t length;

    if (fgets(line, (int)capacity, file) == NULL) return 0;
    length = strlen(line);
    if (length == 0 || line[length - 1] != '\n') return -1;
    line[length - 1] = '\0';
    if (length >= 2 && line[length - 2] == '\r') line[length - 2] = '\0';
    return 1;
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

static void copy_field(char *destination, size_t capacity, const char *source)
{
    if (capacity == 0) return;
    snprintf(destination, capacity, "%s", source == NULL ? "" : source);
}

int pspl_storage_exists(const char *path)
{
    FILE *file;
    if (path == NULL) return 0;
    file = fopen(path, "rb");
    if (file == NULL) return 0;
    fclose(file);
    return 1;
}

int pspl_storage_save(const char *path, const pspl_library_t *library)
{
    char temporary[256];
    char backup[256];
    size_t i;
    int path_length;
    uint32_t crc = 0xFFFFFFFFU;
    FILE *file;

    if (path == NULL || library == NULL ||
        library->game_count > PSPL_STORAGE_MAX_GAMES ||
        library->session_count > PSPL_STORAGE_MAX_SESSIONS) return -1;
    path_length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (path_length < 0 || (size_t)path_length >= sizeof(temporary)) return -2;
    path_length = snprintf(backup, sizeof(backup), "%s.bak", path);
    if (path_length < 0 || (size_t)path_length >= sizeof(backup)) return -2;

    file = fopen(temporary, "wb");
    if (file == NULL) return -3;
    if (write_payload_line(file, &crc, "PSPLIB %d\n", PSPL_DB_VERSION) != 0 ||
        write_payload_line(file, &crc, "GAMES %lu\n", (unsigned long)library->game_count) != 0) {
        fclose(file);
        remove(temporary);
        return -4;
    }
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
            encode_field(game->cover_path, cover_path, sizeof(cover_path)) != 0 ||
            write_payload_line(file, &crc,
                               "G|%s|%s|%s|%s|%s|%lu|%lu|%lu|%lu|%u|%s\n",
                               id, title, platform, content_key, version,
                               (unsigned long)game->total_seconds,
                               (unsigned long)game->session_count,
                               (unsigned long)game->first_started_at,
                               (unsigned long)game->last_started_at,
                               (unsigned int)game->file_present, cover_path) != 0) {
            fclose(file);
            remove(temporary);
            return -4;
        }
    }
    if (write_payload_line(file, &crc, "SESSIONS %lu\n", (unsigned long)library->session_count) != 0) {
        fclose(file);
        remove(temporary);
        return -4;
    }
    for (i = 0; i < library->session_count; ++i) {
        const pspl_session_t *session = &library->sessions[i];
        char game_id[PSPL_ID_LEN * 2];
        if (encode_field(session->game_id, game_id, sizeof(game_id)) != 0 ||
            write_payload_line(file, &crc, "S|%s|%lu|%lu|%lu\n", game_id,
                               (unsigned long)session->started_at,
                               (unsigned long)session->ended_at,
                               (unsigned long)session->duration_seconds) != 0) {
            fclose(file);
            remove(temporary);
            return -4;
        }
    }
    {
        int write_result = fprintf(file, "CHECKSUM %08lX\n", (unsigned long)~crc);
        int flush_result = fflush(file);
        int close_result = fclose(file);
        if (write_result < 0 || flush_result != 0 || close_result != 0) {
            remove(temporary);
            return -4;
        }
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

static int parse_checksum(const char *text, uint32_t *value)
{
    char *end;
    unsigned long parsed;
    if (text == NULL || value == NULL || strncmp(text, "CHECKSUM ", 9) != 0) return -1;
    parsed = strtoul(text + 9, &end, 16);
    if (end == text + 9 || *end != '\0' || parsed > 0xFFFFFFFFUL) return -1;
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
            copy_field(destination->version, sizeof(destination->version), source->version);
    }
    if (source->file_present) destination->file_present = 1;
    if (destination->cover_path[0] == '\0' && source->cover_path[0] != '\0')
        copy_field(destination->cover_path, sizeof(destination->cover_path), source->cover_path);
    if (destination->version[0] == '\0' && source->version[0] != '\0')
        copy_field(destination->version, sizeof(destination->version), source->version);
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
    if (separator == NULL || strchr(body, '-') == NULL || separator[1] == '\0') return;
    if ((size_t)(separator - game->content_key) >= sizeof(stable_key)) return;
    memcpy(stable_key, game->content_key, (size_t)(separator - game->content_key));
    stable_key[separator - game->content_key] = '\0';
    copy_field(game->version, sizeof(game->version), separator + 1);
    copy_field(game->content_key, sizeof(game->content_key), stable_key);
}

static const char *mapped_id(const legacy_id_map_t *maps, size_t count, const char *old_id)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        if (strcmp(maps[i].old_id, old_id) == 0) return maps[i].new_id;
    }
    return old_id;
}

static int parse_game_line(char *line, int db_version, pspl_game_t *game,
                           char old_id[PSPL_ID_LEN])
{
    char canonical_id[PSPL_ID_LEN];
    char *cursor;
    char *field;
    uint32_t number;

    if (line[0] != 'G' || line[1] != '|') return -1;
    memset(game, 0, sizeof(*game));
    cursor = line + 2;
    field = next_field(&cursor); if (field == NULL) return -1; decode_field(field); copy_field(game->id, sizeof(game->id), field);
    field = next_field(&cursor); if (field == NULL) return -1; decode_field(field); copy_field(game->title, sizeof(game->title), field);
    field = next_field(&cursor); if (field == NULL) return -1; decode_field(field); copy_field(game->platform, sizeof(game->platform), field);
    field = next_field(&cursor); if (field == NULL) return -1; decode_field(field); copy_field(game->content_key, sizeof(game->content_key), field);
    if (db_version >= 2) {
        field = next_field(&cursor); if (field == NULL) return -1; decode_field(field); copy_field(game->version, sizeof(game->version), field);
    }
    field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -1; game->total_seconds = number;
    field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -1; game->session_count = number;
    field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -1; game->first_started_at = number;
    field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -1; game->last_started_at = number;
    field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) return -1; game->file_present = (uint8_t)number;
    field = next_field(&cursor); if (field == NULL || cursor != NULL) return -1; decode_field(field); copy_field(game->cover_path, sizeof(game->cover_path), field);
    copy_field(old_id, PSPL_ID_LEN, game->id);
    if (db_version == 1) migrate_v1_identity(game);
    if (pspl_make_game_id(pspl_platform_from_name(game->platform), game->content_key, canonical_id) != 0) return -1;
    copy_field(game->id, sizeof(game->id), canonical_id);
    return 0;
}

int pspl_storage_load(const char *path, pspl_library_t *library)
{
    pspl_library_t parsed;
    legacy_id_map_t *maps = NULL;
    size_t map_count = 0;
    FILE *file = NULL;
    char line[PSPL_STORAGE_LINE_SIZE];
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t expected_checksum;
    uint32_t expected_games;
    uint32_t expected_sessions;
    uint32_t i;
    int db_version;
    int status;
    int result = -6;

    if (path == NULL || library == NULL) return -1;
    pspl_library_init(&parsed);
    file = fopen(path, "rb");
    if (file == NULL) return -2;

    status = read_payload_line(file, line, sizeof(line), &crc);
    if (status != 1 || (strcmp(line, "PSPLIB 1") != 0 && strcmp(line, "PSPLIB 2") != 0)) goto fail;
    db_version = strcmp(line, "PSPLIB 1") == 0 ? 1 : 2;
    status = read_payload_line(file, line, sizeof(line), &crc);
    if (status != 1 || strncmp(line, "GAMES ", 6) != 0 ||
        parse_unsigned(line + 6, &expected_games) != 0 ||
        expected_games > PSPL_STORAGE_MAX_GAMES) goto fail;
    maps = (legacy_id_map_t *)calloc(expected_games == 0 ? 1 : expected_games, sizeof(*maps));
    if (maps == NULL) {
        result = -7;
        goto fail;
    }
    for (i = 0; i < expected_games; ++i) {
        pspl_game_t game;
        char old_id[PSPL_ID_LEN];
        pspl_game_t *existing;
        if (read_payload_line(file, line, sizeof(line), &crc) != 1 ||
            parse_game_line(line, db_version, &game, old_id) != 0) goto fail;
        existing = find_identity(&parsed, game.platform, game.content_key);
        if (existing != NULL) {
            merge_game(existing, &game);
            copy_field(game.id, sizeof(game.id), existing->id);
        } else if (pspl_library_add_game(&parsed, &game) != 0) {
            result = -7;
            goto fail;
        }
        copy_field(maps[map_count].old_id, sizeof(maps[map_count].old_id), old_id);
        copy_field(maps[map_count].new_id, sizeof(maps[map_count].new_id), game.id);
        ++map_count;
    }

    if (read_payload_line(file, line, sizeof(line), &crc) != 1 ||
        strncmp(line, "SESSIONS ", 9) != 0 ||
        parse_unsigned(line + 9, &expected_sessions) != 0 ||
        expected_sessions > PSPL_STORAGE_MAX_SESSIONS) goto fail;
    for (i = 0; i < expected_sessions; ++i) {
        pspl_session_t session;
        char *cursor;
        char *field;
        uint32_t number;
        if (read_payload_line(file, line, sizeof(line), &crc) != 1 ||
            line[0] != 'S' || line[1] != '|') goto fail;
        memset(&session, 0, sizeof(session));
        cursor = line + 2;
        field = next_field(&cursor); if (field == NULL) goto fail; decode_field(field);
        copy_field(session.game_id, sizeof(session.game_id), mapped_id(maps, map_count, field));
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) goto fail; session.started_at = number;
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0) goto fail; session.ended_at = number;
        field = next_field(&cursor); if (parse_unsigned(field, &number) != 0 || cursor != NULL) goto fail; session.duration_seconds = number;
        if (pspl_library_append_session(&parsed, &session) != 0) {
            result = -8;
            goto fail;
        }
    }
    if (read_unchecked_line(file, line, sizeof(line)) != 1 ||
        parse_checksum(line, &expected_checksum) != 0 ||
        ~crc != expected_checksum || fgetc(file) != EOF) {
        result = -5;
        goto fail;
    }
    fclose(file);
    free(maps);
    pspl_library_free(library);
    *library = parsed;
    return 0;

fail:
    if (file != NULL) fclose(file);
    free(maps);
    pspl_library_free(&parsed);
    return result;
}
