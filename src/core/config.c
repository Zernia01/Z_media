#include "zm/app.h"
#include <stdio.h>
#include <string.h>
static FILE *config_file(const char *mode) {
    FILE *f = NULL;
    char *base = SDL_GetPrefPath("Zernia", "ZM");
    char path[32768];
    if (!base) return NULL;
    SDL_snprintf(path, sizeof(path), "%ssettings.ini", base);
    SDL_free(base);
    f = zm_fopen_utf8(path, mode);
    return f;
}
void zm_config_load(ZMConfig *c) {
    FILE *f;
    char line[128];
    c->volume = 80; c->theme = 0; SDL_strlcpy(c->language, "ko", sizeof(c->language));
    c->subtitles_enabled = c->subtitle_auto = 1; c->subtitle_size = 24; c->subtitle_style = c->subtitle_delay_ms = 0;
    c->hardware_mode = 0;
    c->rounded_font = 1;
    c->fullscreen_hide_ui = c->play_during_move = 1;
    c->auto_open_folder = 1;
    c->update_auto_check=c->update_auto_download=1;
    c->update_notify=1;
    f = config_file("r");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        int value;
        if (sscanf(line, "volume=%d", &value) == 1 && value >= 0 && value <= 100) c->volume = value;
        else if (sscanf(line, "theme=%d", &value) == 1 && value >= 0 && value <= 1) c->theme = value;
        else if (sscanf(line, "subtitles_enabled=%d", &value) == 1 && value >= 0 && value <= 1) c->subtitles_enabled = value;
        else if (sscanf(line, "subtitle_auto=%d", &value) == 1 && value >= 0 && value <= 1) c->subtitle_auto = value;
        else if (sscanf(line, "subtitle_style=%d", &value) == 1 && value >= 0 && value <= 1) c->subtitle_style = value;
        else if (sscanf(line, "subtitle_size=%d", &value) == 1 && value >= 8 && value <= 120) c->subtitle_size = value;
        else if (sscanf(line, "subtitle_delay_ms=%d", &value) == 1 && value >= -600000 && value <= 600000) c->subtitle_delay_ms = value;
        else if (sscanf(line, "hardware_mode=%d", &value) == 1 && value >= 0 && value <= 2) c->hardware_mode = value;
        else if (sscanf(line, "rounded_font=%d", &value) == 1 && value >= 0 && value <= 1) c->rounded_font = value;
        else if (sscanf(line, "fullscreen_hide_ui=%d", &value) == 1 && value >= 0 && value <= 1) c->fullscreen_hide_ui = value;
        else if (sscanf(line, "play_during_move=%d", &value) == 1 && value >= 0 && value <= 1) c->play_during_move = value;
        else if (sscanf(line, "auto_open_folder=%d", &value) == 1 && value >= 0 && value <= 1) c->auto_open_folder = value;
        else if (sscanf(line,"update_auto_check=%d",&value)==1 && value>=0 && value<=1)c->update_auto_check=value;
        else if (sscanf(line,"update_auto_download=%d",&value)==1 && value>=0 && value<=1)c->update_auto_download=value;
        else if (sscanf(line,"update_notify=%d",&value)==1 && value>=0 && value<=1)c->update_notify=value;
        else if (strncmp(line, "language=", 9) == 0) {
            char *lang = line + 9;
            lang[strcspn(lang, "\r\n")] = '\0';
            if (!strcmp(lang, "ko") || !strcmp(lang, "en") || !strcmp(lang, "ja") || !strcmp(lang, "zh-CN") || !strcmp(lang, "zh-TW") || !strcmp(lang, "ko-Hanja")) SDL_strlcpy(c->language, lang, sizeof(c->language));
        }
    }
    fclose(f);
}
void zm_config_save(const ZMConfig *c) {
    FILE *f = config_file("w");
    if (!f) { SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not save settings"); return; }
    fprintf(f, "version=1\nvolume=%d\ntheme=%d\nlanguage=%s\n", c->volume, c->theme, c->language);
    fprintf(f, "subtitles_enabled=%d\nsubtitle_auto=%d\nsubtitle_style=%d\nsubtitle_size=%d\nsubtitle_delay_ms=%d\n",
        c->subtitles_enabled, c->subtitle_auto, c->subtitle_style, c->subtitle_size, c->subtitle_delay_ms);
    fprintf(f,"hardware_mode=%d\n",c->hardware_mode);
    fprintf(f,"rounded_font=%d\n",c->rounded_font);
    fprintf(f,"fullscreen_hide_ui=%d\nplay_during_move=%d\n",c->fullscreen_hide_ui,c->play_during_move);
    fprintf(f,"auto_open_folder=%d\n",c->auto_open_folder);
    fprintf(f,"update_auto_check=%d\nupdate_auto_download=%d\n",c->update_auto_check,c->update_auto_download);
    fprintf(f,"update_notify=%d\n",c->update_notify);
    fclose(f);
}
