#include "psplibrary/core.h"
#include "psplibrary/storage.h"

#include <assert.h>
#include <stdio.h>
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
    event.type = PSPL_EVENT_GAME_STOPPED; event.timestamp = 160;
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

static void test_storage(void)
{
    const char *path = "test_storage.db";
    const char *partial_path = "test_storage.partial.db";
    pspl_library_t original;
    pspl_library_t loaded;
    pspl_library_t recovered;
    pspl_game_t game;
    remove(path);
    remove("test_storage.db.bak");
    remove(partial_path);
    pspl_library_init(&original);
    make_game(&game, PSPL_PLATFORM_PS1, "Metal Gear Solid", "mgs|US|1.0");
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
    remove(path);
    remove("test_storage.db.bak");
    remove(partial_path);
}

int main(void)
{
    test_ids_and_sorting();
    test_events();
    test_storage();
    puts("PSP Library core tests: OK");
    return 0;
}
