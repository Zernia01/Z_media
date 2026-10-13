#ifndef ZM_APP_H
#define ZM_APP_H
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
FILE *zm_fopen_utf8(const char *path, const char *mode);
typedef struct ZMConfig {
    int volume, theme, subtitles_enabled, subtitle_auto, subtitle_size, subtitle_style, subtitle_delay_ms, hardware_mode, rounded_font;
    char language[16];
    int fullscreen_hide_ui, play_during_move;
    int auto_open_folder;
    int update_auto_check,update_auto_download;
    int update_notify;
} ZMConfig;
void zm_config_load(ZMConfig *c);
void zm_config_save(const ZMConfig *c);
const char *zm_text(const char *key);
void zm_i18n_load(const char *language);
char *zm_open_dialog(SDL_Window *window);
char **zm_open_files(SDL_Window *window, int *count);
char *zm_font_path(const char *language, int rounded, int bold);
void zm_window_set_icon(SDL_Window *window);
int zm_window_set_move_callback(SDL_Window *window, void (*callback)(void *), void *data);
void zm_window_clear_move_callback(SDL_Window *window);
int zm_window_test_move(SDL_Window *window, int milliseconds);
int zm_register_associations(void);
int zm_unregister_associations(void);
int zm_open_default_apps(SDL_Window *window);
int zm_app_run(const char *path, int smoke_seconds, const char *capture_path);
void zm_draw_text(SDL_Renderer *r, TTF_Font *font, const char *text, int x, int y, SDL_Color color);
void zm_draw_button(SDL_Renderer *r, TTF_Font *font, SDL_Rect rect, const char *label, int active, int light);
void zm_draw_clear_cache(void);
void zm_draw_round_rect(SDL_Renderer *r, SDL_Rect rect, int radius);
void zm_draw_set_fallback(TTF_Font *font);
void zm_draw_set_pointer(int x, int y);
int zm_draw_hovered(SDL_Renderer *r, SDL_Rect rect);
typedef enum ZMIcon {
    ZM_ICON_FOLDER, ZM_ICON_PLAY, ZM_ICON_PAUSE, ZM_ICON_STOP,
    ZM_ICON_BACK, ZM_ICON_FORWARD, ZM_ICON_VOLUME, ZM_ICON_MUTE,
    ZM_ICON_FULLSCREEN, ZM_ICON_RESTORE, ZM_ICON_SETTINGS,
    ZM_ICON_PREVIOUS, ZM_ICON_NEXT, ZM_ICON_REPEAT, ZM_ICON_REPEAT_ONE,
    ZM_ICON_SHUFFLE, ZM_ICON_GENERAL, ZM_ICON_APPEARANCE,
    ZM_ICON_PERFORMANCE, ZM_ICON_SUBTITLES, ZM_ICON_KEYBOARD, ZM_ICON_INFO
} ZMIcon;
void zm_draw_icon(SDL_Renderer *r, SDL_Rect rect, ZMIcon icon, SDL_Color color);
void zm_draw_icon_button(SDL_Renderer *r, SDL_Rect rect, ZMIcon icon, const char *label, int active, int light);
void zm_draw_tooltip_target(SDL_Renderer *r, SDL_Rect rect, const char *label);
void zm_draw_begin_frame(void);
void zm_draw_tooltip(SDL_Renderer *r, TTF_Font *font, int width, int height, int light);
#endif
