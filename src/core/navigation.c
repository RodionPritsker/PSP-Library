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

static void remember_current(pspl_navigation_t *navigation,
                             const pspl_library_t *library,
                             const size_t *indices, size_t count)
{
    if (navigation == NULL || count == 0 || library == NULL || indices == NULL) return;
    if (navigation->selected >= count) navigation->selected = count - 1;
    if (valid_game_index(library, indices[navigation->selected])) {
        copy_id(navigation->selected_ids[(size_t)navigation->filter],
                library->games[indices[navigation->selected]].id);
    }
}

static int refresh_list(pspl_navigation_t *navigation,
                        const pspl_library_t *library,
                        size_t *indices, size_t capacity, size_t *count)
{
    int result;
    size_t selected;
    const char *preferred;

    if (navigation == NULL || library == NULL || count == NULL ||
        !valid_filter(navigation->filter)) return -1;
    result = pspl_library_filter_sort(library, navigation->filter,
                                      navigation->sort, indices, capacity);
    if (result < 0) return result;
    *count = (size_t)result;
    navigation->selected = 0;
    if (*count == 0) {
        navigation->transition_id[0] = '\0';
        return 0;
    }

    preferred = navigation->transition_id[0] != '\0'
        ? navigation->transition_id
        : navigation->selected_ids[(size_t)navigation->filter];
    selected = find_id(library, indices, *count, preferred);
    if (selected >= *count) selected = 0;
    navigation->selected = selected;
    copy_id(navigation->selected_ids[(size_t)navigation->filter],
            library->games[indices[selected]].id);
    navigation->transition_id[0] = '\0';
    return 0;
}

void pspl_navigation_init(pspl_navigation_t *navigation)
{
    if (navigation != NULL) {
        memset(navigation, 0, sizeof(*navigation));
        navigation->filter = PSPL_PLATFORM_ALL;
        navigation->sort = PSPL_SORT_TITLE;
        navigation->grid = 1;
    }
}

int pspl_navigation_refresh(pspl_navigation_t *navigation,
                            const pspl_library_t *library,
                            size_t *indices, size_t capacity,
                            size_t *count)
{
    return refresh_list(navigation, library, indices, capacity, count);
}

int pspl_navigation_change_filter(pspl_navigation_t *navigation,
                                  int direction,
                                  const pspl_library_t *library,
                                  size_t *indices, size_t capacity,
                                  size_t current_count, size_t *count)
{
    pspl_platform_t next;
    if (navigation == NULL || direction == 0) return -1;
    remember_current(navigation, library, indices, current_count);
    next = (pspl_platform_t)((int)navigation->filter + (direction > 0 ? 1 : -1));
    if (next < PSPL_PLATFORM_ALL) next = PSPL_PLATFORM_OTHER;
    if (next > PSPL_PLATFORM_OTHER) next = PSPL_PLATFORM_ALL;
    if (current_count != 0) {
        copy_id(navigation->transition_id,
                navigation->selected_ids[(size_t)navigation->filter]);
    } else {
        navigation->transition_id[0] = '\0';
    }
    navigation->filter = next;
    return refresh_list(navigation, library, indices, capacity, count);
}

int pspl_navigation_change_sort(pspl_navigation_t *navigation,
                               const pspl_library_t *library,
                               size_t *indices, size_t capacity,
                               size_t current_count, size_t *count)
{
    if (navigation == NULL) return -1;
    remember_current(navigation, library, indices, current_count);
    navigation->sort = (pspl_sort_t)((navigation->sort + 1) % 3);
    return refresh_list(navigation, library, indices, capacity, count);
}

void pspl_navigation_toggle_view(pspl_navigation_t *navigation)
{
    if (navigation != NULL) navigation->grid = !navigation->grid;
}

void pspl_navigation_move(pspl_navigation_t *navigation,
                          const pspl_library_t *library,
                          const size_t *indices, size_t count,
                          unsigned int direction)
{
    size_t current;

    if (navigation == NULL || count == 0) return;
    if (navigation->selected >= count) navigation->selected = count - 1;
    current = navigation->selected;

    if (!navigation->grid) {
        if ((direction & PSPL_NAV_UP) != 0U) {
            if (current > 0) navigation->selected = current - 1;
        } else if ((direction & PSPL_NAV_DOWN) != 0U) {
            if (current + 1 < count) navigation->selected = current + 1;
        }
    } else if ((direction & PSPL_NAV_LEFT) != 0U) {
        if (current % PSPL_NAV_GRID_COLUMNS != 0U ||
            (current > 0 && current % PSPL_NAV_GRID_PAGE_SIZE == 0U)) {
            navigation->selected = current - 1;
        }
    } else if ((direction & PSPL_NAV_RIGHT) != 0U) {
        if (current + 1 < count &&
            (current % PSPL_NAV_GRID_COLUMNS != PSPL_NAV_GRID_COLUMNS - 1U ||
             current % PSPL_NAV_GRID_PAGE_SIZE == PSPL_NAV_GRID_PAGE_SIZE - 1U))
            navigation->selected = current + 1;
    } else if ((direction & PSPL_NAV_UP) != 0U) {
        if (current >= PSPL_NAV_GRID_COLUMNS)
            navigation->selected = current - PSPL_NAV_GRID_COLUMNS;
    } else if ((direction & PSPL_NAV_DOWN) != 0U) {
        if (current + PSPL_NAV_GRID_COLUMNS < count)
            navigation->selected = current + PSPL_NAV_GRID_COLUMNS;
    }
    remember_current(navigation, library, indices, count);
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
