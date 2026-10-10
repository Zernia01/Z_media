#ifndef ZM_PLAYLIST_H
#define ZM_PLAYLIST_H
#define ZM_PLAYLIST_LIMIT 4096
typedef struct ZMPlaylist {
    char *items[ZM_PLAYLIST_LIMIT];
    int count, current, repeat, shuffle;
} ZMPlaylist;
void zm_playlist_init(ZMPlaylist *list);
void zm_playlist_destroy(ZMPlaylist *list);
int zm_playlist_add(ZMPlaylist *list, const char *path);
void zm_playlist_remove(ZMPlaylist *list, int index);
void zm_playlist_move(ZMPlaylist *list, int from, int to);
const char *zm_playlist_step(ZMPlaylist *list, int direction, int automatic);
const char *zm_basename(const char *path);
int zm_playlist_load_folder(ZMPlaylist *list, const char *path);
#endif
