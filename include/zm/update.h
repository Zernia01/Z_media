#ifndef ZM_UPDATE_H
#define ZM_UPDATE_H
#include <SDL.h>
typedef struct ZMUpdateManifest { char version[32],url[256],sha256[65]; } ZMUpdateManifest;
typedef enum ZMUpdateStatus { ZM_UPDATE_IDLE,ZM_UPDATE_CHECKING,ZM_UPDATE_CURRENT,ZM_UPDATE_AVAILABLE,ZM_UPDATE_DOWNLOADING,ZM_UPDATE_READY,ZM_UPDATE_FAILED } ZMUpdateStatus;
typedef struct ZMUpdateState { ZMUpdateStatus status; int progress; char version[32],sha256[65],path[32768]; } ZMUpdateState;
typedef struct ZMUpdater {
    SDL_mutex *mutex;
    SDL_Thread *thread;
    SDL_atomic_t quit;
    ZMUpdateState state;
    int automatic_download;
} ZMUpdater;
int zm_update_parse(const char *json,ZMUpdateManifest *manifest);
int zm_update_version_compare(const char *a,const char *b,int *valid);
int zm_update_verify_file(const char *path,const char *sha256);
int zm_update_init(ZMUpdater *updater);
void zm_update_check(ZMUpdater *updater,int download);
void zm_update_snapshot(ZMUpdater *updater,ZMUpdateState *state);
int zm_update_install(ZMUpdater *updater);
void zm_update_destroy(ZMUpdater *updater);
#endif
