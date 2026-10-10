#include "zm/playlist.h"
#include <SDL.h>
#include <string.h>
void zm_playlist_init(ZMPlaylist *list) { memset(list, 0, sizeof(*list)); list->current = -1; }
void zm_playlist_destroy(ZMPlaylist *list) {
    int i;
    for (i = 0; i < list->count; ++i) SDL_free(list->items[i]);
    zm_playlist_init(list);
}
int zm_playlist_add(ZMPlaylist *list, const char *path) {
    char *copy;
    if (!path || !*path || list->count >= ZM_PLAYLIST_LIMIT) return -1;
    copy = SDL_strdup(path);
    if (!copy) return -1;
    list->items[list->count] = copy;
    return list->count++;
}
void zm_playlist_remove(ZMPlaylist *list, int index) {
    if (index < 0 || index >= list->count) return;
    SDL_free(list->items[index]);
    memmove(&list->items[index], &list->items[index + 1], (size_t)(list->count - index - 1) * sizeof(char *));
    --list->count;
    if (index < list->current) --list->current;
    else if (index == list->current) list->current = -1;
}
void zm_playlist_move(ZMPlaylist *list, int from, int to) {
    char *item;
    if (from < 0 || to < 0 || from >= list->count || to >= list->count || from == to) return;
    item = list->items[from];
    if (from < to) memmove(&list->items[from], &list->items[from + 1], (size_t)(to - from) * sizeof(char *));
    else memmove(&list->items[to + 1], &list->items[to], (size_t)(from - to) * sizeof(char *));
    list->items[to] = item;
    if (list->current == from) list->current = to;
    else if (from < list->current && to >= list->current) --list->current;
    else if (from > list->current && to <= list->current) ++list->current;
}
const char *zm_playlist_step(ZMPlaylist *list, int direction, int automatic) {
    int next;
    if (!list->count) return NULL;
    if (automatic && list->repeat == 1 && list->current >= 0) return list->items[list->current];
    if (list->shuffle && list->count > 1) {
        next = (int)(SDL_GetPerformanceCounter() % (Uint64)(list->count - 1));
        if (next >= list->current && list->current >= 0) ++next;
    } else next = list->current + direction;
    if (next < 0 || next >= list->count) {
        if (list->repeat == 2 || !automatic) next = direction > 0 ? 0 : list->count - 1;
        else return NULL;
    }
    list->current = next; return list->items[next];
}
const char *zm_basename(const char *path) {
    const char *result = path, *p;
    for (p = path; *p; ++p) if (*p == '/' || *p == '\\') result = p + 1;
    return result;
}
