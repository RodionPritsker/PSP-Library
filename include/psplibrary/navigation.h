#ifndef PSPLIBRARY_NAVIGATION_H
#define PSPLIBRARY_NAVIGATION_H

#include <stddef.h>
#include <stdint.h>

#include "core.h"

#define PSPL_NAV_FILTER_COUNT ((size_t)PSPL_PLATFORM_OTHER + 1U)

#define PSPL_NAV_UP 0x01U
#define PSPL_NAV_DOWN 0x02U
#define PSPL_NAV_LEFT 0x04U
#define PSPL_NAV_RIGHT 0x08U

typedef struct {
    unsigned int direction;
    uint64_t next_repeat_us;
} pspl_navigation_repeat_t;

typedef struct {
    char selected_ids[PSPL_NAV_FILTER_COUNT][PSPL_ID_LEN];
    pspl_platform_t filter;
    pspl_sort_t sort;
    int grid;
    size_t selected;
    char transition_id[PSPL_ID_LEN];
} pspl_navigation_t;

void pspl_navigation_init(pspl_navigation_t *navigation);
int pspl_navigation_refresh(pspl_navigation_t *navigation,
                            const pspl_library_t *library,
                            size_t *indices, size_t capacity,
                            size_t *count);
int pspl_navigation_change_filter(pspl_navigation_t *navigation,
                                  int direction,
                                  const pspl_library_t *library,
                                  size_t *indices, size_t capacity,
                                  size_t current_count, size_t *count);
int pspl_navigation_change_sort(pspl_navigation_t *navigation,
                               const pspl_library_t *library,
                               size_t *indices, size_t capacity,
                               size_t current_count, size_t *count);
void pspl_navigation_toggle_view(pspl_navigation_t *navigation);
void pspl_navigation_move(pspl_navigation_t *navigation,
                          const pspl_library_t *library,
                          const size_t *indices, size_t count,
                          unsigned int direction);
void pspl_navigation_repeat_init(pspl_navigation_repeat_t *repeat);
unsigned int pspl_navigation_repeat(pspl_navigation_repeat_t *repeat,
                                    unsigned int direction,
                                    uint64_t now_us);

#endif
