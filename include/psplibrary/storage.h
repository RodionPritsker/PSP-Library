#ifndef PSPLIBRARY_STORAGE_H
#define PSPLIBRARY_STORAGE_H

#include "core.h"

#define PSPL_DB_VERSION 2
#define PSPL_STORAGE_MAX_GAMES 4096
#define PSPL_STORAGE_MAX_SESSIONS 65536

int pspl_storage_exists(const char *path);
int pspl_storage_load(const char *path, pspl_library_t *library);
int pspl_storage_save(const char *path, const pspl_library_t *library);

#endif
