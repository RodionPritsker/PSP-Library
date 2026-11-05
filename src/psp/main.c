#include <pspuser.h>
#include <pspctrl.h>
#include <pspiofilemgr.h>

#include "psplibrary/core.h"
#include "psplibrary/demo.h"
#include "psplibrary/storage.h"
#include "psplibrary/ui.h"

PSP_MODULE_INFO("PSP Library", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1;
    (void)arg2;
    (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    sceKernelRegisterExitCallback(sceKernelCreateCallback("Exit Callback", exit_callback, NULL));
    sceKernelSleepThreadCB();
    return 0;
}

static int callback_setup(void)
{
    int thread = sceKernelCreateThread("Update Callback Thread", callback_thread,
                                       0x11, 0xFA0, 0, NULL);
    if (thread >= 0) sceKernelStartThread(thread, 0, NULL);
    return thread;
}

static pspl_library_t g_library;

int main(void)
{
    const char *user_path = "ms0:/PSP/SAVEDATA/PSPLIBRARY/library.db";
    const char *user_backup_path = "ms0:/PSP/SAVEDATA/PSPLIBRARY/library.db.bak";
    const char *demo_path = "ms0:/PSP/SAVEDATA/PSPLIBRARY/demo.db";
    const char *test100_path = "ms0:/PSP/SAVEDATA/PSPLIBRARY/test100.db";
    const char *test100_flag = "ms0:/PSP/SAVEDATA/PSPLIBRARY/test100.flag";
    int demo_mode = 0;

    callback_setup();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    sceIoMkdir("ms0:/PSP/SAVEDATA", 0777);
    sceIoMkdir("ms0:/PSP/SAVEDATA/PSPLIBRARY", 0777);

    pspl_library_init(&g_library);
    if (pspl_storage_exists(test100_flag)) {
        if (pspl_storage_exists(test100_path) && pspl_storage_load(test100_path, &g_library) == 0) {
            demo_mode = 2;
        } else {
            pspl_demo_populate_large(&g_library, pspl_library_now());
            pspl_storage_save(test100_path, &g_library);
            demo_mode = 2;
        }
    } else if (pspl_storage_exists(user_path) && pspl_storage_load(user_path, &g_library) == 0) {
        demo_mode = 0;
    } else if (pspl_storage_exists(user_backup_path) && pspl_storage_load(user_backup_path, &g_library) == 0) {
        demo_mode = 0;
    } else if (pspl_storage_exists(demo_path) && pspl_storage_load(demo_path, &g_library) == 0) {
        demo_mode = 1;
    } else {
        pspl_demo_populate(&g_library, pspl_library_now());
        pspl_storage_save(demo_path, &g_library);
        demo_mode = 1;
    }

    pspl_ui_run(&g_library, user_path, demo_mode);
    pspl_library_free(&g_library);
    sceKernelExitGame();
    return 0;
}
