#include "psplibrary/navigation.h"

#include <string.h>

#define PSPL_NAV_GRID_COLUMNS 3U
#define PSPL_NAV_GRID_PAGE_SIZE 6U
#define PSPL_NAV_REPEAT_INITIAL_US 280000ULL
#define PSPL_NAV_REPEAT_INTERVAL_US 105000ULL

static int valid_filter(pspl_platform_t filter)
{
    return filter >= PSPL_PLATFORM_ALL && filter <= PSPL_PLATFORM_OTHER;
}

static int valid_game_index(const pspl_library_t *library, size_t index)
{
    return library != NULL && library->games != NULL && index < library->game_count;
}

static void copy_id(char destination[PSPL_ID_LEN], const char *source)
{
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }
    memcpy(destination, source, PSPL_ID_LEN);
    destination[PSPL_ID_LEN - 1] = '\0';
}

static size_t find_id(const pspl_library_t *library, const size_t *indices,
                      size_t count, const char *id)
{
    size_t i;
    if (library == NULL || indices == NULL || id == NULL || id[0] == '\0') return count;
    for (i = 0; i < count; ++i) {
        if (!valid_game_index(library, indices[i])) continue;
        if (strcmp(library->games[indices[i]].id, id) == 0) return i;
    }
    return count;
}

void pspl_navigation_init(pspl_navigation_t *navigation)
{
    if (navigation != NULL) memset(navigation, 0, sizeof(*navigation));
}

void pspl_navigation_remember(pspl_navigation_t *navigation,
                              pspl_platform_t filter,
                              const pspl_library_t *library,
                              const size_t *indices,
                              size_t count,
                              size_t selected)
{
    if (navigation == NULL || !valid_filter(filter)) return;
    if (count == 0 || library == NULL || indices == NULL) return;
    if (selected >= count) selected = count - 1;
    if (!valid_game_index(library, indices[selected])) return;
    navigation->positions[(size_t)filter] = selected;
    copy_id(navigation->selected_ids[(size_t)filter],
            library->games[indices[selected]].id);
}

void pspl_navigation_restore(pspl_navigation_t *navigation,
                             pspl_platform_t filter,
                             const pspl_library_t *library,
                             const size_t *indices,
                             size_t count,
                             size_t *selected)
{
    size_t filter_index;
    size_t restored;
    if (navigation == NULL || !valid_filter(filter) || selected == NULL) return;
    filter_index = (size_t)filter;
    if (count == 0 || library == NULL || indices == NULL) {
        *selected = 0;
        return;
    }

    restored = find_id(library, indices, count, navigation->selected_ids[filter_index]);
    if (restored >= count) {
        restored = navigation->positions[filter_index];
        if (restored >= count) restored = count - 1;
        while (restored > 0 && !valid_game_index(library, indices[restored])) --restored;
        if (!valid_game_index(library, indices[restored])) {
            navigation->positions[filter_index] = 0;
            navigation->selected_ids[filter_index][0] = '\0';
            *selected = 0;
            return;
        }
        copy_id(navigation->selected_ids[filter_index],
                library->games[indices[restored]].id);
    }
    navigation->positions[filter_index] = restored;
    *selected = restored;
}

void pspl_navigation_move(int grid, size_t *selected, size_t count,
                          unsigned int direction)
{
    size_t current;

    if (selected == NULL || count == 0) return;
    if (*selected >= count) *selected = count - 1;
    current = *selected;

    if (!grid) {
        if ((direction & PSPL_NAV_UP) != 0U) {
            if (current > 0) *selected = current - 1;
        } else if ((direction & PSPL_NAV_DOWN) != 0U) {
            if (current + 1 < count) *selected = current + 1;
        }
        return;
    }

    if ((direction & PSPL_NAV_LEFT) != 0U) {
        if (current % PSPL_NAV_GRID_COLUMNS != 0U) {
            *selected = current - 1;
        } else if (current > 0 && current % PSPL_NAV_GRID_PAGE_SIZE == 0U) {
            /* The first card on a page links back to the previous page. */
            *selected = current - 1;
        }
    } else if ((direction & PSPL_NAV_RIGHT) != 0U) {
        if (current + 1 >= count) return;
        if (current % PSPL_NAV_GRID_COLUMNS != PSPL_NAV_GRID_COLUMNS - 1U ||
            current % PSPL_NAV_GRID_PAGE_SIZE == PSPL_NAV_GRID_PAGE_SIZE - 1U) {
            /* The last card on a page links to the next page. */
            *selected = current + 1;
        }
    } else if ((direction & PSPL_NAV_UP) != 0U) {
        if (current >= PSPL_NAV_GRID_COLUMNS) *selected = current - PSPL_NAV_GRID_COLUMNS;
    } else if ((direction & PSPL_NAV_DOWN) != 0U) {
        if (current + PSPL_NAV_GRID_COLUMNS < count)
            *selected = current + PSPL_NAV_GRID_COLUMNS;
    }
}

void pspl_navigation_repeat_init(pspl_navigation_repeat_t *repeat)
{
    if (repeat != NULL) {
        repeat->direction = 0;
        repeat->next_repeat_us = 0;
    }
}

unsigned int pspl_navigation_repeat(pspl_navigation_repeat_t *repeat,
                                    unsigned int direction,
                                    uint64_t now_us)
{
    if (repeat == NULL) return 0;
    if (direction == 0U) {
        repeat->direction = 0;
        repeat->next_repeat_us = 0;
        return 0;
    }
    if (direction != repeat->direction) {
        repeat->direction = direction;
        repeat->next_repeat_us = now_us + PSPL_NAV_REPEAT_INITIAL_US;
        return direction;
    }
    if (repeat->next_repeat_us != 0 && now_us >= repeat->next_repeat_us) {
        repeat->next_repeat_us = now_us + PSPL_NAV_REPEAT_INTERVAL_US;
        return direction;
    }
    return 0;
}
