#ifndef PSPLIBRARY_NAVIGATION_H
#define PSPLIBRARY_NAVIGATION_H

#include <stddef.h>

#include "core.h"

#define PSPL_NAV_FILTER_COUNT ((size_t)PSPL_PLATFORM_OTHER + 1U)

typedef struct {
    char selected_ids[PSPL_NAV_FILTER_COUNT][PSPL_ID_LEN];
    size_t positions[PSPL_NAV_FILTER_COUNT];
} pspl_navigation_t;

void pspl_navigation_init(pspl_navigation_t *navigation);
void pspl_navigation_remember(pspl_navigation_t *navigation,
                              pspl_platform_t filter,
                              const pspl_library_t *library,
                              const size_t *indices,
                              size_t count,
                              size_t selected);
void pspl_navigation_restore(pspl_navigation_t *navigation,
                             pspl_platform_t filter,
                             const pspl_library_t *library,
                             const size_t *indices,
                             size_t count,
                             size_t *selected);

#endif
