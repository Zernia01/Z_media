#ifndef ZM_SHORTCUTS_H
#define ZM_SHORTCUTS_H
#include <SDL.h>
typedef enum ZMAction {
    ZM_OPEN, ZM_PLAY, ZM_FULLSCREEN, ZM_MUTE, ZM_BACK, ZM_FORWARD,
    ZM_BACK_LONG, ZM_FORWARD_LONG, ZM_VOLUME_UP, ZM_VOLUME_DOWN,
    ZM_SUBTITLE_TOGGLE, ZM_SUBTITLE_OPEN, ZM_SUBTITLE_NEXT, ZM_SETTINGS,
    ZM_PLAYLIST, ZM_NEXT_FILE, ZM_PREVIOUS_FILE, ZM_NEXT_AUDIO, ZM_ACTION_COUNT
} ZMAction;
typedef struct ZMBinding { SDL_Keycode key; unsigned mods; } ZMBinding;
typedef struct ZMShortcuts { ZMBinding bindings[ZM_ACTION_COUNT]; } ZMShortcuts;
void zm_shortcuts_defaults(ZMShortcuts *s);
void zm_shortcuts_restore(ZMShortcuts *s, int action);
int zm_shortcuts_assign(ZMShortcuts *s, int action, ZMBinding binding, int replace);
int zm_shortcuts_match(const ZMShortcuts *s, SDL_Keycode key, SDL_Keymod mods);
unsigned zm_shortcuts_mods(SDL_Keymod mods);
void zm_shortcuts_label(ZMBinding binding, char *out, size_t capacity);
const char *zm_action_name(int action);
void zm_shortcuts_load(ZMShortcuts *s);
void zm_shortcuts_save(const ZMShortcuts *s);
#endif
