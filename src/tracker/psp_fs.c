#include "psplibrary/tracker.h"

#ifdef __PSP__
#include <pspiofilemgr.h>
#else
#include <stdio.h>
#endif

int pspl_psp_path_present(const char *path)
{
    if (path == NULL || path[0] == '\0') return 0;
#ifdef __PSP__
    {
        SceIoStat stat;
        return sceIoGetstat(path, &stat) >= 0 ? 1 : 0;
    }
#else
    {
        FILE *file = fopen(path, "rb");
        if (file == NULL) return 0;
        fclose(file);
        return 1;
    }
#endif
}
