#include "zm/shortcuts.h"
#include "zm/app.h"
static const ZMBinding defaults[ZM_ACTION_COUNT] = {
    {SDLK_o,1},{SDLK_SPACE,0},{SDLK_f,0},{SDLK_m,0},
    {SDLK_LEFT,0},{SDLK_RIGHT,0},{SDLK_LEFT,1},{SDLK_RIGHT,1},
    {SDLK_UP,0},{SDLK_DOWN,0},{SDLK_s,0},{SDLK_s,1},{SDLK_j,0},
    {SDLK_COMMA,1},{SDLK_p,0},{SDLK_PAGEDOWN,0},{SDLK_PAGEUP,0},{SDLK_a,0}
};
static const char *names[ZM_ACTION_COUNT] = {
    "open","play","fullscreen","mute","seek_back","seek_forward","seek_back_long","seek_forward_long",
    "volume_up","volume_down","subtitles_enabled","subtitle_open","subtitle_next","settings","playlist","next","previous","audio_track"
};
const char *zm_action_name(int action) { return action >= 0 && action < ZM_ACTION_COUNT ? names[action] : ""; }
void zm_shortcuts_defaults(ZMShortcuts *s) { SDL_memcpy(s->bindings, defaults, sizeof(defaults)); }
unsigned zm_shortcuts_mods(SDL_Keymod mods) {
    return (mods & KMOD_CTRL ? 1u : 0u) | (mods & KMOD_ALT ? 2u : 0u) | (mods & KMOD_SHIFT ? 4u : 0u);
}
int zm_shortcuts_match(const ZMShortcuts *s, SDL_Keycode key, SDL_Keymod mods) {
    int i;
    if (mods & KMOD_GUI) return -1;
    for (i = 0; i < ZM_ACTION_COUNT; ++i) if (s->bindings[i].key == key && s->bindings[i].mods == zm_shortcuts_mods(mods)) return i;
    return -1;
}
int zm_shortcuts_assign(ZMShortcuts *s, int action, ZMBinding binding, int replace) {
    int i;
    if (action < 0 || action >= ZM_ACTION_COUNT) return -2;
    for (i = 0; i < ZM_ACTION_COUNT; ++i) if (i != action && binding.key && s->bindings[i].key == binding.key && s->bindings[i].mods == binding.mods) {
        if (!replace) return i;
        s->bindings[i].key = 0; s->bindings[i].mods = 0;
    }
    s->bindings[action] = binding; return -1;
}
void zm_shortcuts_restore(ZMShortcuts *s, int action) {
    if (action >= 0 && action < ZM_ACTION_COUNT) zm_shortcuts_assign(s, action, defaults[action], 1);
}
void zm_shortcuts_label(ZMBinding binding, char *out, size_t capacity) {
    SDL_snprintf(out, capacity, "%s%s%s%s", binding.mods & 1 ? "Ctrl+" : "", binding.mods & 2 ? "Alt+" : "", binding.mods & 4 ? "Shift+" : "", binding.key ? SDL_GetKeyName(binding.key) : "—");
}
static FILE *file(const char *mode) {
    char *base = SDL_GetPrefPath("Zernia", "ZM"), path[32768];
    FILE *f;
    if (!base) return NULL;
    SDL_snprintf(path, sizeof(path), "%sshortcuts.ini", base); SDL_free(base);
    f = zm_fopen_utf8(path, mode); return f;
}
void zm_shortcuts_load(ZMShortcuts *s) {
    FILE *f;
    char line[128];
    zm_shortcuts_defaults(s); f = file("r"); if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        int action, key; unsigned mods;
        if (sscanf(line, "%d=%d,%u", &action, &key, &mods) == 3 && action >= 0 && action < ZM_ACTION_COUNT && mods < 8u && key >= 0)
            zm_shortcuts_assign(s, action, (ZMBinding){(SDL_Keycode)key,mods}, 1);
    }
    fclose(f);
}
void zm_shortcuts_save(const ZMShortcuts *s) {
    int i; FILE *f = file("w"); if (!f) return;
    for (i = 0; i < ZM_ACTION_COUNT; ++i) fprintf(f, "%d=%d,%u\n", i, s->bindings[i].key, s->bindings[i].mods);
    fclose(f);
}
