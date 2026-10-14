#ifndef ZM_UI_H
#define ZM_UI_H
#include "zm/app.h"
#include "zm/player.h"
#include "zm/playlist.h"
#include "zm/shortcuts.h"
#include "zm/update.h"
typedef struct UI {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *video;
    TTF_Font *font, *heading, *bold, *fallback, *cjk;
    TTF_Font *glyph_fonts[ZM_FALLBACK_COUNT],*heading_glyph_fonts[ZM_FALLBACK_COUNT];
    ZMPlayer player;
    ZMUpdater updater;
    char update_notified_version[32],update_install_pending[32];
    ZMConfig config;
    ZMPlaylist playlist;
    ZMShortcuts shortcuts;
    int capturing, conflict, shortcut_scroll, shortcut_selected;
    ZMBinding pending_binding;
    char search[96];
    ZMSnapshot state;
    int width, height, fullscreen, settings, dragging, running;
    int video_width, video_height;
    unsigned generation, media_generation;
    int panel, panel_scroll, selected, handled_end, drop_first;
    int settings_scroll, settings_content_height, dropdown, scrollbar_drag;
    SDL_Rect dropdown_anchor;
    int folder_mode;
    double drag_position;
    Uint64 drag_seek_ticks;
    int presenting;
    unsigned presented_frames;
    int fullscreen_show_ui;
    Uint64 activity;
} UI;
typedef struct ZMControlsLayout {
    SDL_Rect buttons[6], video, time, info, seek, seek_hit, volume, volume_hit, volume_label, fullscreen;
} ZMControlsLayout;
void zm_controls_layout(UI *u, ZMControlsLayout *layout);
void zm_ui_set_language(UI *u, const char *language);
void zm_ui_reload_fonts(UI *u);
void zm_ui_open_subtitle(UI *u);
void zm_ui_next_subtitle(UI *u);
int zm_ui_shortcut_row(UI *u, int row);
void zm_settings_draw(UI *u);
void zm_settings_click(UI *u, int x, int y);
void zm_settings_scroll(UI *u, int amount);
void zm_settings_drag(UI *u, int y);
#endif
