#ifndef PSPLIBRARY_TRACKER_H
#define PSPLIBRARY_TRACKER_H

#include "core.h"

#define PSPL_TRACKER_TITLE_LEN 64
#define PSPL_TRACKER_PATH_LEN 256

/*
 * A launch descriptor is deliberately independent from the way it was
 * observed. A future PRX, launcher integration, or test harness can feed
 * the same descriptor into this adapter.
 */
typedef struct {
    const char *title;
    const char *content_id; /* Must include region/release identity, e.g. ULUS-10509. */
    const char *version; /* Metadata only; never part of the stable ID. */
    const char *path;
    const char *cover_path;
    int file_present; /* -1 unknown, 0 absent, 1 present */
    uint32_t timestamp;
} pspl_psp_launch_info_t;

typedef struct {
    pspl_library_t *library;
} pspl_tracker_t;

void pspl_tracker_init(pspl_tracker_t *tracker, pspl_library_t *library);
int pspl_psp_build_content_key(const pspl_psp_launch_info_t *info,
                               char out_key[PSPL_CONTENT_KEY_LEN]);
int pspl_psp_tracker_start(pspl_tracker_t *tracker,
                           const pspl_psp_launch_info_t *info,
                           char out_game_id[PSPL_ID_LEN]);
int pspl_psp_tracker_stop(pspl_tracker_t *tracker, uint32_t timestamp);
int pspl_psp_path_present(const char *path);

#endif
