#include "psplibrary/core.h"
#include "psplibrary/storage.h"
#include "psplibrary/tracker.h"
#include "psplibrary/navigation.h"
#include "psplibrary/demo.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_game(pspl_game_t *game, pspl_platform_t platform, const char *title, const char *key)
{
    memset(game, 0, sizeof(*game));
    assert(pspl_make_game_id(platform, key, game->id) == 0);
    snprintf(game->title, sizeof(game->title), "%s", title);
    snprintf(game->platform, sizeof(game->platform), "%s", pspl_platform_name(platform));
    snprintf(game->content_key, sizeof(game->content_key), "%s", key);
}

static void test_ids_and_sorting(void)
{
    char first[PSPL_ID_LEN];
    char second[PSPL_ID_LEN];
    pspl_library_t library;
    pspl_game_t game;
    size_t indices[4];
    int count;

    assert(pspl_make_game_id(PSPL_PLATFORM_GBA, "same-rom|US|1.0", first) == 0);
    assert(pspl_make_game_id(PSPL_PLATFORM_GBA, "same-rom|US|1.0", second) == 0);
    assert(strcmp(first, second) == 0);
    assert(pspl_make_game_id(PSPL_PLATFORM_GBA, "same-rom|US|1.1", second) == 0);
    assert(strcmp(first, second) != 0);

    pspl_library_init(&library);
    make_game(&game, PSPL_PLATFORM_PS1, "Charlie", "c"); game.total_seconds = 30; game.last_started_at = 100; assert(pspl_library_add_game(&library, &game) == 0);
    make_game(&game, PSPL_PLATFORM_PSP, "Bravo", "b"); game.total_seconds = 90; game.last_started_at = 200; assert(pspl_library_add_game(&library, &game) == 0);
    make_game(&game, PSPL_PLATFORM_GBA, "Alpha", "a"); game.total_seconds = 60; game.last_started_at = 300; assert(pspl_library_add_game(&library, &game) == 0);
    count = pspl_library_filter_sort(&library, PSPL_PLATFORM_GBA, PSPL_SORT_TITLE, indices, 4);
    assert(count == 1 && strcmp(library.games[indices[0]].title, "Alpha") == 0);
    count = pspl_library_filter_sort(&library, PSPL_PLATFORM_ALL, PSPL_SORT_TITLE, indices, 4);
    assert(count == 3 && strcmp(library.games[indices[0]].title, "Alpha") == 0 &&
           strcmp(library.games[indices[1]].title, "Bravo") == 0);
    count = pspl_library_filter_sort(&library, PSPL_PLATFORM_ALL, PSPL_SORT_TOTAL_TIME, indices, 4);
    assert(count == 3 && strcmp(library.games[indices[0]].title, "Bravo") == 0);
    count = pspl_library_filter_sort(&library, PSPL_PLATFORM_ALL, PSPL_SORT_LAST_PLAYED, indices, 4);
    assert(count == 3 && strcmp(library.games[indices[0]].title, "Alpha") == 0);
    pspl_library_free(&library);
}

static void test_navigation(void)
{
    pspl_library_t library;
    pspl_navigation_t navigation;
    pspl_game_t game;
    size_t indices[16];
    size_t count;
    char persona_id[PSPL_ID_LEN];
    char fusion_id[PSPL_ID_LEN];

    pspl_library_init(&library);
    make_game(&game, PSPL_PLATFORM_PSP, "Peace Walker", "peace-walker");
    assert(pspl_library_add_game(&library, &game) == 0);
    make_game(&game, PSPL_PLATFORM_PSP, "Persona 3 Portable", "persona-3");
    assert(pspl_library_add_game(&library, &game) == 0);
    make_game(&game, PSPL_PLATFORM_GBA, "Metroid Fusion", "metroid-fusion");
    assert(pspl_library_add_game(&library, &game) == 0);

    pspl_navigation_init(&navigation);

    assert(pspl_navigation_refresh(&navigation, &library, indices,
                                   sizeof(indices) / sizeof(indices[0]), &count) == 0);
    assert(count == 3);
    navigation.selected = 2;
    pspl_navigation_move(&navigation, &library, indices, count, 0);
    strcpy(persona_id, library.games[indices[navigation.selected]].id);

    /* A: ALL -> PSP carries the current game_id, even if PSP has a different first item. */
    assert(pspl_navigation_change_filter(&navigation, 1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(navigation.filter == PSPL_PLATFORM_PSP);
    assert(strcmp(library.games[indices[navigation.selected]].id, persona_id) == 0);

    /* B: PSP -> ALL carries the same game_id back. */
    assert(pspl_navigation_change_filter(&navigation, -1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(navigation.filter == PSPL_PLATFORM_ALL);
    assert(strcmp(library.games[indices[navigation.selected]].id, persona_id) == 0);

    /* C: each platform remembers its own selection when the current game is absent. */
    assert(pspl_navigation_change_filter(&navigation, 1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    navigation.selected = 0;
    pspl_navigation_move(&navigation, &library, indices, count, PSPL_NAV_DOWN);
    strcpy(persona_id, library.games[indices[navigation.selected]].id);
    assert(pspl_navigation_change_filter(&navigation, 1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(navigation.filter == PSPL_PLATFORM_PS1);
    assert(count == 0 && navigation.selected == 0);
    assert(pspl_navigation_change_filter(&navigation, 1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(navigation.filter == PSPL_PLATFORM_GBA);
    assert(count == 1);
    strcpy(fusion_id, library.games[indices[navigation.selected]].id);
    assert(pspl_navigation_change_filter(&navigation, -1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(navigation.filter == PSPL_PLATFORM_PS1 && count == 0);
    assert(pspl_navigation_change_filter(&navigation, -1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(navigation.filter == PSPL_PLATFORM_PSP);
    assert(strcmp(library.games[indices[navigation.selected]].id, persona_id) == 0);
    assert(pspl_navigation_change_filter(&navigation, 1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(navigation.filter == PSPL_PLATFORM_PS1 && count == 0);
    assert(pspl_navigation_change_filter(&navigation, 1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(navigation.filter == PSPL_PLATFORM_GBA);
    assert(strcmp(library.games[indices[navigation.selected]].id, fusion_id) == 0);

    /* D/E: sort and view changes keep the identity and use the new linear position. */
    assert(pspl_navigation_change_filter(&navigation, -1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(pspl_navigation_change_filter(&navigation, -1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(pspl_navigation_change_filter(&navigation, -1, &library, indices,
                                         sizeof(indices) / sizeof(indices[0]),
                                         count, &count) == 0);
    assert(navigation.filter == PSPL_PLATFORM_ALL);
    navigation.selected = 1;
    pspl_navigation_move(&navigation, &library, indices, count, 0);
    strcpy(persona_id, library.games[indices[navigation.selected]].id);
    assert(pspl_navigation_change_sort(&navigation, &library, indices,
                                       sizeof(indices) / sizeof(indices[0]),
                                       count, &count) == 0);
    assert(strcmp(library.games[indices[navigation.selected]].id, persona_id) == 0);
    pspl_navigation_toggle_view(&navigation);
    assert(strcmp(library.games[indices[navigation.selected]].id, persona_id) == 0);

    /* F: an unavailable saved id falls back to a valid first item. */
    strcpy(navigation.selected_ids[PSPL_PLATFORM_ALL], "missing-id");
    assert(pspl_navigation_refresh(&navigation, &library, indices,
                                   sizeof(indices) / sizeof(indices[0]), &count) == 0);
    assert(count == 3 && navigation.selected < count);

    pspl_library_free(&library);
}

static size_t move_index(int grid, size_t current, size_t count, unsigned int direction)
{
    pspl_navigation_t navigation;
    pspl_navigation_init(&navigation);
    navigation.grid = grid;
    navigation.selected = current;
    pspl_navigation_move(&navigation, NULL, NULL, count, direction);
    return navigation.selected;
}

static void test_page_navigation(void)
{
    static const size_t counts[] = { 5, 6, 7, 12, 13, 100 };
    static const unsigned int directions[] = {
        PSPL_NAV_UP, PSPL_NAV_DOWN, PSPL_NAV_LEFT, PSPL_NAV_RIGHT
    };
    size_t scenario;

    for (scenario = 0; scenario < sizeof(counts) / sizeof(counts[0]); ++scenario) {
        int visited[100] = { 0 };
        size_t queue[100];
        size_t head = 0;
        size_t tail = 0;
        size_t current;
        size_t direction_index;
        size_t visited_count = 0;
        size_t count = counts[scenario];

        visited[0] = 1;
        queue[tail++] = 0;
        ++visited_count;
        while (head < tail) {
            current = queue[head++];
            for (direction_index = 0; direction_index < 4; ++direction_index) {
                size_t next = current;
                next = move_index(1, next, count, directions[direction_index]);
                assert(next < count);
                if (!visited[next]) {
                    visited[next] = 1;
                    queue[tail++] = next;
                    ++visited_count;
                }
            }
        }
        assert(visited_count == count);

        if (count == 7) {
            current = 5;
            current = move_index(1, current, count, PSPL_NAV_RIGHT);
            assert(current == 6);
            current = move_index(1, current, count, PSPL_NAV_LEFT);
            assert(current == 5);
        }
        if (count == 13) {
            current = 9;
            current = move_index(1, current, count, PSPL_NAV_DOWN);
            assert(current == 12);
            current = move_index(1, current, count, PSPL_NAV_UP);
            assert(current == 9);
        }
    }

    {
        size_t selected = 0;
        size_t i;
        for (i = 0; i < 99; ++i)
            selected = move_index(0, selected, 100, PSPL_NAV_DOWN);
        assert(selected == 99);
        for (i = 0; i < 99; ++i)
            selected = move_index(0, selected, 100, PSPL_NAV_UP);
        assert(selected == 0);
    }
}

static void test_navigation_repeat(void)
{
    pspl_navigation_repeat_t repeat;

    pspl_navigation_repeat_init(&repeat);
    assert(pspl_navigation_repeat(&repeat, PSPL_NAV_RIGHT, 0) == PSPL_NAV_RIGHT);
    assert(pspl_navigation_repeat(&repeat, PSPL_NAV_RIGHT, 279999) == 0);
    assert(pspl_navigation_repeat(&repeat, PSPL_NAV_RIGHT, 280000) == PSPL_NAV_RIGHT);
    assert(pspl_navigation_repeat(&repeat, PSPL_NAV_RIGHT, 384999) == 0);
    assert(pspl_navigation_repeat(&repeat, PSPL_NAV_RIGHT, 385000) == PSPL_NAV_RIGHT);
    assert(pspl_navigation_repeat(&repeat, 0, 390000) == 0);
    assert(pspl_navigation_repeat(&repeat, 0, 500000) == 0);
    assert(pspl_navigation_repeat(&repeat, PSPL_NAV_RIGHT, 500001) == PSPL_NAV_RIGHT);
}

static void test_events(void)
{
    pspl_library_t library;
    pspl_game_t game;
    pspl_event_t event;

    pspl_library_init(&library);
    make_game(&game, PSPL_PLATFORM_PSP, "A", "a"); assert(pspl_library_add_game(&library, &game) == 0);
    make_game(&game, PSPL_PLATFORM_PSP, "B", "b"); assert(pspl_library_add_game(&library, &game) == 0);
    event.type = PSPL_EVENT_GAME_STARTED; event.game_id = library.games[0].id; event.timestamp = 100;
    assert(pspl_library_handle_event(&library, &event) == 0);
    event.timestamp = 150;
    assert(pspl_library_handle_event(&library, &event) == 0);
    assert(library.games[0].first_started_at == 100 && library.games[0].last_started_at == 100);
    event.type = PSPL_EVENT_GAME_STOPPED; event.game_id = library.games[1].id; event.timestamp = 155;
    assert(pspl_library_handle_event(&library, &event) == -4);
    event.type = PSPL_EVENT_GAME_STOPPED; event.game_id = library.games[0].id; event.timestamp = 160;
    assert(pspl_library_handle_event(&library, &event) == 0);
    assert(library.games[0].total_seconds == 60 && library.games[0].session_count == 1);
    event.timestamp = 200;
    assert(pspl_library_handle_event(&library, &event) == 0);
    assert(library.games[0].total_seconds == 60 && library.games[0].session_count == 1);
    event.type = PSPL_EVENT_GAME_STARTED; event.game_id = library.games[0].id; event.timestamp = 300;
    assert(pspl_library_handle_event(&library, &event) == 0);
    event.type = PSPL_EVENT_GAME_STOPPED; event.timestamp = 350;
    assert(pspl_library_handle_event(&library, &event) == 0);
    assert(library.games[0].total_seconds == 110 && library.games[0].session_count == 2);
    event.type = PSPL_EVENT_GAME_SWITCHED; event.game_id = library.games[0].id; event.timestamp = 200;
    assert(pspl_library_handle_event(&library, &event) == 0);
    event.timestamp = 220;
    assert(pspl_library_handle_event(&library, &event) == 0);
    event.type = PSPL_EVENT_GAME_SWITCHED; event.game_id = library.games[1].id; event.timestamp = 260;
    assert(pspl_library_handle_event(&library, &event) == 0);
    event.type = PSPL_EVENT_GAME_STOPPED; event.timestamp = 300;
    assert(pspl_library_handle_event(&library, &event) == 0);
    assert(library.games[0].total_seconds == 170 && library.games[1].total_seconds == 40);
    assert(library.games[0].session_count == 3 && library.games[1].session_count == 1);
    assert(library.session_count == 4);
    pspl_library_free(&library);
}

static void corrupt_first_byte(const char *path)
{
    FILE *file = fopen(path, "r+b");
    int value;
    assert(file != NULL);
    value = fgetc(file);
    assert(value != EOF);
    assert(fseek(file, 0, SEEK_SET) == 0);
    assert(fputc(value == 'P' ? 'X' : 'P', file) != EOF);
    assert(fclose(file) == 0);
}

static void copy_prefix(const char *source_path, const char *destination_path, size_t length)
{
    unsigned char buffer[64];
    FILE *source = fopen(source_path, "rb");
    FILE *destination;
    size_t read_count;
    assert(source != NULL && length <= sizeof(buffer));
    read_count = fread(buffer, 1, length, source);
    assert(read_count == length);
    assert(fclose(source) == 0);
    destination = fopen(destination_path, "wb");
    assert(destination != NULL);
    assert(fwrite(buffer, 1, length, destination) == length);
    assert(fclose(destination) == 0);
}

static uint32_t test_crc32(const unsigned char *data, size_t length)
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

static void write_legacy_v1_database(const char *path)
{
    char old_id_1[PSPL_ID_LEN];
    char old_id_2[PSPL_ID_LEN];
    char payload[4096];
    size_t used;
    int written;
    FILE *file;
    assert(pspl_make_game_id(PSPL_PLATFORM_PSP, "PSP:ULUS-10509:1.00", old_id_1) == 0);
    assert(pspl_make_game_id(PSPL_PLATFORM_PSP, "PSP:ULUS-10509:1.01", old_id_2) == 0);
    written = snprintf(payload, sizeof(payload),
                       "PSPLIB 1\nGAMES 2\n"
                       "G|%s|Peace Walker|PSP|PSP:ULUS-10509:1.00|60|1|100|100|1|\n"
                       "G|%s|Peace Walker|PSP|PSP:ULUS-10509:1.01|90|1|200|200|0|\n"
                       "SESSIONS 2\n"
                       "S|%s|100|160|60\n"
                       "S|%s|200|290|90\n",
                       old_id_1, old_id_2, old_id_1, old_id_2);
    assert(written > 0 && (size_t)written < sizeof(payload));
    used = (size_t)written;
    written = snprintf(payload + used, sizeof(payload) - used, "CHECKSUM %08lX\n",
                       (unsigned long)test_crc32((const unsigned char *)payload, used));
    assert(written > 0 && (size_t)written < sizeof(payload) - used);
    file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(payload, 1, used + (size_t)written, file) == used + (size_t)written);
    assert(fclose(file) == 0);
}

static void test_storage(void)
{
    const char *path = "test_storage.db";
    const char *partial_path = "test_storage.partial.db";
    const char *legacy_path = "test_storage_v1.db";
    pspl_library_t original;
    pspl_library_t loaded;
    pspl_library_t recovered;
    pspl_game_t game;
    pspl_library_init(&loaded);
    pspl_library_init(&recovered);
    remove(path);
    remove("test_storage.db.bak");
    remove(partial_path);
    remove(legacy_path);
    pspl_library_init(&original);
    make_game(&game, PSPL_PLATFORM_PS1, "Metal Gear Solid", "mgs|US|1.0");
    strcpy(game.version, "1.00");
    game.file_present = 1;
    strcpy(game.cover_path, "covers/mgs.png");
    assert(pspl_library_add_game(&original, &game) == 0);
    {
        pspl_event_t event = { PSPL_EVENT_GAME_STARTED, original.games[0].id, 100 };
        assert(pspl_library_handle_event(&original, &event) == 0);
        event.type = PSPL_EVENT_GAME_STOPPED;
        event.timestamp = 160;
        assert(pspl_library_handle_event(&original, &event) == 0);
    }
    original.games[0].total_seconds += 1234;
    assert(pspl_storage_save(path, &original) == 0);
    assert(pspl_storage_load(path, &loaded) == 0);
    assert(loaded.game_count == 1);
    assert(strcmp(loaded.games[0].id, original.games[0].id) == 0);
    assert(loaded.games[0].total_seconds == 1294);
    assert(loaded.games[0].session_count == 1 && loaded.session_count == 1);
    assert(strcmp(loaded.games[0].content_key, "mgs|US|1.0") == 0);
    assert(strcmp(loaded.games[0].version, "1.00") == 0);
    assert(strcmp(loaded.games[0].cover_path, "covers/mgs.png") == 0);

    assert(pspl_storage_save(path, &loaded) == 0);
    corrupt_first_byte(path);
    assert(pspl_storage_load(path, &recovered) != 0);
    assert(pspl_storage_load("test_storage.db.bak", &recovered) == 0);
    assert(recovered.game_count == 1 && recovered.games[0].total_seconds == 1294);

    copy_prefix("test_storage.db.bak", partial_path, 20);
    assert(pspl_storage_load(partial_path, &loaded) != 0);

    recovered.games[0].file_present = 0;
    assert(pspl_storage_save(path, &recovered) == 0);
    assert(pspl_storage_load(path, &loaded) == 0);
    assert(loaded.game_count == 1 && loaded.games[0].file_present == 0);
    assert(loaded.session_count == 1 && loaded.games[0].session_count == 1);

    write_legacy_v1_database(legacy_path);
    assert(pspl_storage_load(legacy_path, &loaded) == 0);
    assert(loaded.game_count == 1 && loaded.session_count == 2);
    assert(strcmp(loaded.games[0].content_key, "PSP:ULUS-10509") == 0);
    assert(strcmp(loaded.games[0].version, "1.01") == 0);
    assert(loaded.games[0].total_seconds == 150 && loaded.games[0].session_count == 2);
    assert(strcmp(loaded.sessions[0].game_id, loaded.games[0].id) == 0);
    assert(strcmp(loaded.sessions[1].game_id, loaded.games[0].id) == 0);
    assert(pspl_storage_save(path, &loaded) == 0);
    assert(pspl_storage_load(path, &recovered) == 0);
    assert(recovered.game_count == 1 && recovered.session_count == 2);
    assert(strcmp(recovered.games[0].version, "1.01") == 0);
    remove(path);
    remove("test_storage.db.bak");
    remove(partial_path);
    remove(legacy_path);
    pspl_library_free(&original);
    pspl_library_free(&loaded);
    pspl_library_free(&recovered);
}

static void test_psp_tracker_adapter(void)
{
    const char *source_path = "tracker-source.bin";
    pspl_library_t library;
    pspl_tracker_t tracker;
    pspl_psp_launch_info_t info;
    char first_id[PSPL_ID_LEN];
    char second_id[PSPL_ID_LEN];
    FILE *source;

    remove(source_path);
    source = fopen(source_path, "wb");
    assert(source != NULL);
    assert(fputc('P', source) != EOF);
    assert(fclose(source) == 0);
    assert(pspl_psp_path_present(source_path) == 1);

    pspl_library_init(&library);
    pspl_tracker_init(&tracker, &library);
    info.title = "Metal Gear Solid: Peace Walker";
    info.content_id = "ULUS-10509";
    info.version = "1.00";
    info.path = "ms0:/PSP/GAME/ULUS10509/EBOOT.PBP";
    info.cover_path = "covers/mgs-pw.png";
    info.file_present = pspl_psp_path_present(source_path);
    info.timestamp = 100;
    assert(pspl_psp_tracker_start(&tracker, &info, first_id) == 0);
    assert(library.game_count == 1 && library.games[0].file_present == 1);
    info.timestamp = 150;
    assert(pspl_psp_tracker_start(&tracker, &info, second_id) == 0);
    assert(strcmp(first_id, second_id) == 0 && library.game_count == 1);
    assert(pspl_psp_tracker_stop(&tracker, 200) == 0);
    assert(library.games[0].total_seconds == 100 && library.games[0].session_count == 1);

    remove(source_path);
    info.file_present = pspl_psp_path_present(source_path);
    info.timestamp = 300;
    assert(pspl_psp_tracker_start(&tracker, &info, second_id) == 0);
    assert(library.games[0].file_present == 0);
    assert(pspl_psp_tracker_stop(&tracker, 350) == 0);
    assert(library.games[0].session_count == 2);

    info.version = "1.01";
    info.title = "Metal Gear Solid: Peace Walker (revision)";
    info.path = "ms0:/PSP/GAME/OTHER10509/EBOOT.PBP";
    info.timestamp = 400;
    assert(pspl_psp_tracker_start(&tracker, &info, second_id) == 0);
    assert(library.game_count == 1 && strcmp(first_id, second_id) == 0);
    assert(strcmp(library.games[0].version, "1.01") == 0);
    info.timestamp = 450;
    assert(pspl_psp_tracker_start(&tracker, &info, second_id) == 0);
    assert(library.game_count == 1);
    assert(pspl_psp_tracker_stop(&tracker, 500) == 0);
    assert(library.games[0].total_seconds == 250 && library.games[0].session_count == 3);

    info.content_id = "ULES-10509";
    info.version = "1.00";
    info.timestamp = 600;
    assert(pspl_psp_tracker_start(&tracker, &info, second_id) == 0);
    assert(library.game_count == 2 && strcmp(first_id, second_id) != 0);
    info.timestamp = 650;
    assert(pspl_psp_tracker_start(&tracker, &info, second_id) == 0);
    assert(pspl_psp_tracker_stop(&tracker, 700) == 0);
    assert(library.games[1].total_seconds == 100 && library.games[1].session_count == 1);

    info.content_id = "ULUS-10509";
    info.version = "1.00";
    info.timestamp = 800;
    assert(pspl_psp_tracker_start(&tracker, &info, second_id) == 0);
    info.content_id = "ULES-10509";
    info.timestamp = 850;
    assert(pspl_psp_tracker_start(&tracker, &info, second_id) == 0);
    assert(pspl_psp_tracker_stop(&tracker, 900) == 0);
    assert(library.games[0].total_seconds == 300 && library.games[0].session_count == 4);
    assert(library.games[1].total_seconds == 150 && library.games[1].session_count == 2);

    remove(source_path);
    pspl_library_free(&library);
}

static void populate_scaled_library(pspl_library_t *library, size_t game_count,
                                    size_t session_count)
{
    size_t i;
    for (i = 0; i < game_count; ++i) {
        pspl_game_t game;
        char title[PSPL_TITLE_LEN];
        char key[PSPL_CONTENT_KEY_LEN];
        pspl_platform_t platform = (pspl_platform_t)(PSPL_PLATFORM_PSP + (i % 8));
        snprintf(title, sizeof(title), "Stress Game %04lu", (unsigned long)i);
        snprintf(key, sizeof(key), "STRESS-%04lu|US|1.0", (unsigned long)i);
        make_game(&game, platform, title, key);
        assert(pspl_library_add_game(library, &game) == 0);
    }
    for (i = 0; i < session_count; ++i) {
        pspl_event_t event;
        size_t game_index = i % game_count;
        event.type = PSPL_EVENT_GAME_STARTED;
        event.game_id = library->games[game_index].id;
        event.timestamp = 1000U + (uint32_t)(i * 2U);
        assert(pspl_library_handle_event(library, &event) == 0);
        event.type = PSPL_EVENT_GAME_STOPPED;
        event.timestamp += 1U;
        assert(pspl_library_handle_event(library, &event) == 0);
    }
}

static void test_stress_scaling(void)
{
    const size_t game_counts[] = { 100, 500, 1000 };
    size_t scenario;
    for (scenario = 0; scenario < sizeof(game_counts) / sizeof(game_counts[0]); ++scenario) {
        char path[64];
        pspl_library_t library;
        pspl_library_t loaded;
        pspl_library_t recovered;
        size_t *indices;
        size_t game_count = game_counts[scenario];
        size_t session_count = game_count == 1000 ? 10000 : game_count * 3;
        size_t i;
        int count;

        snprintf(path, sizeof(path), "test_stress_%lu.db", (unsigned long)game_count);
        remove(path);
        {
            char backup[80];
            char temporary[80];
            snprintf(backup, sizeof(backup), "%s.bak", path);
            snprintf(temporary, sizeof(temporary), "%s.tmp", path);
            remove(backup);
            remove(temporary);
        }
        pspl_library_init(&library);
        pspl_library_init(&loaded);
        pspl_library_init(&recovered);
        populate_scaled_library(&library, game_count, session_count);
        assert(library.game_count == game_count && library.session_count == session_count);
        assert(library.game_capacity >= game_count && library.session_capacity >= session_count);
        for (i = 0; i < game_count; ++i)
            assert(library.games[i].session_count == session_count / game_count);
        printf("stress %lu games / %lu sessions: %lu bytes capacity\n",
               (unsigned long)game_count, (unsigned long)session_count,
               (unsigned long)(library.game_capacity * sizeof(pspl_game_t) +
                               library.session_capacity * sizeof(pspl_session_t)));

        indices = (size_t *)malloc(game_count * sizeof(*indices));
        assert(indices != NULL);
        count = pspl_library_filter_sort(&library, PSPL_PLATFORM_ALL,
                                         PSPL_SORT_TITLE, indices, game_count);
        assert(count == (int)game_count);
        assert(strcmp(library.games[indices[0]].title, "Stress Game 0000") == 0);
        free(indices);

        assert(pspl_storage_save(path, &library) == 0);
        assert(pspl_storage_save(path, &library) == 0);
        assert(pspl_storage_load(path, &loaded) == 0);
        assert(loaded.game_count == game_count && loaded.session_count == session_count);
        assert(loaded.games[0].total_seconds == session_count / game_count);
        corrupt_first_byte(path);
        assert(pspl_storage_load(path, &recovered) != 0);
        {
            char backup[80];
            snprintf(backup, sizeof(backup), "%s.bak", path);
            assert(pspl_storage_load(backup, &recovered) == 0);
        }
        assert(recovered.game_count == game_count && recovered.session_count == session_count);
        remove(path);
        {
            char backup[80];
            char temporary[80];
            snprintf(backup, sizeof(backup), "%s.bak", path);
            snprintf(temporary, sizeof(temporary), "%s.tmp", path);
            remove(backup);
            remove(temporary);
        }
        pspl_library_free(&library);
        pspl_library_free(&loaded);
        pspl_library_free(&recovered);
    }
}

static void test_large_demo_fixture(void)
{
    pspl_library_t library;
    size_t i;
    int cover_count = 0;
    pspl_library_init(&library);
    pspl_demo_populate_large(&library, 1700000000U);
    assert(library.game_count == 100);
    assert(library.session_count == 300);
    for (i = 0; i < library.game_count; ++i) {
        if (library.games[i].cover_path[0] != '\0') ++cover_count;
        assert(library.games[i].session_count == 3);
        assert(library.games[i].first_started_at > 0);
        assert(library.games[i].last_started_at > 0);
    }
    assert(cover_count > 0 && cover_count < 100);
    pspl_library_free(&library);
}

static unsigned int read_png_u32(const unsigned char value[4])
{
    return ((unsigned int)value[0] << 24) | ((unsigned int)value[1] << 16) |
           ((unsigned int)value[2] << 8) | (unsigned int)value[3];
}

static void test_cover_assets_are_runtime_sized(void)
{
    static const char *paths[] = {
        "../covers/aurora-quest.png",
        "../covers/ember-tactics.png",
        "../covers/neon-racers.png"
    };
    size_t i;
    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        unsigned char signature[8];
        unsigned char chunk_length[4];
        unsigned char chunk_type[4];
        unsigned char ihdr[13];
        FILE *file = fopen(paths[i], "rb");
        long size;
        assert(file != NULL);
        assert(fread(signature, 1, sizeof(signature), file) == sizeof(signature));
        assert(memcmp(signature, "\x89PNG\r\n\x1a\n", sizeof(signature)) == 0);
        assert(fread(chunk_length, 1, sizeof(chunk_length), file) == sizeof(chunk_length));
        assert(read_png_u32(chunk_length) == 13U);
        assert(fread(chunk_type, 1, sizeof(chunk_type), file) == sizeof(chunk_type));
        assert(memcmp(chunk_type, "IHDR", sizeof(chunk_type)) == 0);
        assert(fread(ihdr, 1, sizeof(ihdr), file) == sizeof(ihdr));
        assert(read_png_u32(ihdr) <= 128U);
        assert(read_png_u32(ihdr + 4) <= 176U);
        assert(fseek(file, 0, SEEK_END) == 0);
        size = ftell(file);
        assert(size > 0 && size < 200000L);
        fclose(file);
    }
}

int main(void)
{
    test_ids_and_sorting();
    test_navigation();
    test_page_navigation();
    test_navigation_repeat();
    test_events();
    test_storage();
    test_psp_tracker_adapter();
    test_stress_scaling();
    test_large_demo_fixture();
    test_cover_assets_are_runtime_sized();
    puts("PSP Library core tests: OK");
    return 0;
}
