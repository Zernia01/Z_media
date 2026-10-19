#include "zm/ui.h"
#include "zm/player.h"
#include "zm/playlist.h"
#include "zm/shortcuts.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static void color(UI *u, Uint8 r, Uint8 g, Uint8 b) { SDL_SetRenderDrawColor(u->renderer, r, g, b, 255); }
static int smoke_option(const char *name,int fallback) {
    const char *value=SDL_getenv(name);
    return value?SDL_atoi(value):fallback;
}
static void smoke_pointer_event(SDL_Window *window,Uint32 type,int x,int y) {
    SDL_Event event; SDL_zero(event); event.type=type;
    if(type==SDL_MOUSEMOTION) {
        event.motion.windowID=SDL_GetWindowID(window); event.motion.x=x; event.motion.y=y; event.motion.state=SDL_BUTTON_LMASK;
    } else {
        event.button.windowID=SDL_GetWindowID(window); event.button.button=SDL_BUTTON_LEFT;
        event.button.x=x; event.button.y=y; event.button.clicks=1;
    }
    SDL_PushEvent(&event);
}
static int inside(int x, int y, SDL_Rect r) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }
static void open_media(UI *u,const char *path) {
    int index=zm_playlist_load_folder(&u->playlist,path);
    u->folder_mode=index>=0;
    if(index<0)index=zm_playlist_add(&u->playlist,path);
    if(index>=0) {
        int rows=((u->height?u->height:720)-354)/38;
        if(rows<1)rows=1;
        u->playlist.current=u->selected=index;
        u->panel_scroll=index>=rows?index-rows+1:0;
        if(u->folder_mode && u->config.auto_open_folder)u->panel=1;
        zm_player_open(&u->player,u->playlist.items[index]); u->handled_end=0;
    }
}
static void open_file(UI *u) {
    int count, i, first = -1;
    char **paths = zm_open_files(u->window, &count);
    if (!paths) return;
    if(count==1) {
        open_media(u,paths[0]); SDL_free(paths[0]); SDL_free(paths); return;
    }
    u->folder_mode=0;
    for (i = 0; i < count; ++i) {
        int index = zm_playlist_add(&u->playlist, paths[i]);
        if (first < 0) first = index;
        SDL_free(paths[i]);
    }
    SDL_free(paths);
    if (first >= 0) {
        u->playlist.current = u->selected = first;
        zm_player_open(&u->player, u->playlist.items[first]);
        u->handled_end = 0;
    }
}
static void next_file(UI *u, int direction, int automatic) {
    const char *path = zm_playlist_step(&u->playlist, direction, automatic);
    if (path) { zm_player_open(&u->player, path); u->selected = u->playlist.current; }
}
void zm_ui_open_subtitle(UI *u) {
    char *path = zm_open_dialog(u->window);
    if (path) { zm_player_subtitle(&u->player, path, -1); SDL_free(path); }
}
void zm_ui_reload_fonts(UI *u) {
    char *regular=zm_font_path(u->config.language,u->config.rounded_font,1);
    char *bold=zm_font_path(u->config.language,u->config.rounded_font,1);
    TTF_Font *body=regular?TTF_OpenFont(regular,16):NULL;
    TTF_Font *title=bold?TTF_OpenFont(bold,28):NULL;
    TTF_Font *strong=bold?TTF_OpenFont(bold,16):NULL;
    TTF_Font *glyphs[ZM_FALLBACK_COUNT]={0},*titles[ZM_FALLBACK_COUNT]={0};
    int i;
    for(i=0;i<ZM_FALLBACK_COUNT;++i) {
        char *path=zm_font_fallback_path(i);
        if(path) { glyphs[i]=TTF_OpenFont(path,16); titles[i]=TTF_OpenFont(path,28); SDL_free(path); }
    }
    SDL_free(regular); SDL_free(bold);
    if(body && title && strong && glyphs[1] && titles[1]) {
        zm_draw_clear_cache();
        if(u->font)TTF_CloseFont(u->font);
        if(u->heading)TTF_CloseFont(u->heading);
        if(u->bold)TTF_CloseFont(u->bold);
        for(i=0;i<ZM_FALLBACK_COUNT;++i) {
            if(u->glyph_fonts[i])TTF_CloseFont(u->glyph_fonts[i]);
            if(u->heading_glyph_fonts[i])TTF_CloseFont(u->heading_glyph_fonts[i]);
        }
        u->font=body; u->heading=title; u->bold=strong;
        memcpy(u->glyph_fonts,glyphs,sizeof(glyphs)); memcpy(u->heading_glyph_fonts,titles,sizeof(titles));
        u->fallback=glyphs[0]; u->cjk=glyphs[1];
        zm_draw_set_fallbacks(title,glyphs,titles);
    } else {
        if(body)TTF_CloseFont(body); if(title)TTF_CloseFont(title); if(strong)TTF_CloseFont(strong);
        for(i=0;i<ZM_FALLBACK_COUNT;++i) { if(glyphs[i])TTF_CloseFont(glyphs[i]); if(titles[i])TTF_CloseFont(titles[i]); }
    }
}
void zm_ui_set_language(UI *u,const char *language) {
    SDL_strlcpy(u->config.language,language,sizeof(u->config.language));
    zm_i18n_load(language); zm_ui_reload_fonts(u);
    SDL_LockMutex(u->player.mutex);
    SDL_strlcpy(u->player.state.preferred_language,language,sizeof(u->player.state.preferred_language));
    SDL_UnlockMutex(u->player.mutex);
}
void zm_ui_next_subtitle(UI *u) {
    if (u->state.subtitle_count) {
        int index = u->state.subtitles_enabled ? u->state.subtitle_index + 1 : 0;
        if (index >= u->state.subtitle_count) zm_player_subtitle_toggle(&u->player);
        else zm_player_subtitle(&u->player, u->state.path, index);
    }
}
static void play(UI *u) {
    if (u->state.ended) zm_player_open(&u->player, u->state.path);
    else zm_player_toggle_pause(&u->player);
}
static void fullscreen(UI *u) {
    u->fullscreen = !u->fullscreen;
    u->fullscreen_show_ui=0;
    if (SDL_SetWindowFullscreen(u->window, u->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) < 0) u->fullscreen = !u->fullscreen;
    u->activity = SDL_GetTicks64();
}
static int controls_hidden(UI *u) {
    return u->fullscreen && u->config.fullscreen_hide_ui && u->state.loaded && !u->settings && !u->dragging && !u->fullscreen_show_ui;
}
static void clock_text(double seconds, char *text, size_t capacity) {
    int value = (int)seconds;
    if (value < 0) value = 0;
    SDL_snprintf(text, capacity, "%02d:%02d:%02d", value / 3600, value / 60 % 60, value % 60);
}
static void key(UI *u, const SDL_KeyboardEvent *e) {
    SDL_Keycode k = e->keysym.sym;
    int action;
    if (u->conflict >= 0) {
        if (k == SDLK_RETURN) { zm_shortcuts_assign(&u->shortcuts, u->capturing, u->pending_binding, 1); u->conflict = u->capturing = -1; SDL_StartTextInput(); }
        else if (k == SDLK_ESCAPE) { u->conflict = u->capturing = -1; SDL_StartTextInput(); }
        return;
    }
    if (u->capturing >= 0) {
        if (k == SDLK_ESCAPE) { u->capturing = -1; SDL_StartTextInput(); return; }
        if (k == SDLK_LCTRL || k == SDLK_RCTRL || k == SDLK_LALT || k == SDLK_RALT || k == SDLK_LSHIFT || k == SDLK_RSHIFT || (e->keysym.mod & KMOD_GUI)) return;
        u->pending_binding = (ZMBinding){k,zm_shortcuts_mods((SDL_Keymod)e->keysym.mod)};
        u->conflict = zm_shortcuts_assign(&u->shortcuts, u->capturing, u->pending_binding, 0);
        if (u->conflict < 0) { u->capturing = -1; SDL_FlushEvent(SDL_TEXTINPUT); SDL_StartTextInput(); }
        return;
    }
    if (k == SDLK_ESCAPE) {
        if(u->dropdown)u->dropdown=0;
        else if (u->settings) { u->settings=u->settings_scroll=0; SDL_StopTextInput(); }
        else if (u->fullscreen) fullscreen(u);
        return;
    }
    if (u->settings) {
        if (u->settings == 3 && k == SDLK_BACKSPACE && *u->search) {
            size_t n = strlen(u->search) - 1;
            while (n && ((unsigned char)u->search[n] & 0xc0u) == 0x80u) --n;
            u->search[n] = '\0'; u->shortcut_scroll = u->settings_scroll = 0;
        }
        return;
    }
    if(u->fullscreen && u->config.fullscreen_hide_ui && k==SDLK_TAB && !e->repeat && !(e->keysym.mod & (KMOD_CTRL|KMOD_ALT|KMOD_SHIFT|KMOD_GUI))) {
        u->fullscreen_show_ui=!u->fullscreen_show_ui; return;
    }
    if (u->panel && k == SDLK_DELETE && u->selected >= 0) {
        zm_playlist_remove(&u->playlist, u->selected);
        if (u->selected >= u->playlist.count) u->selected = u->playlist.count - 1;
        return;
    }
    if (u->panel && (e->keysym.mod & KMOD_SHIFT) && (k == SDLK_UP || k == SDLK_DOWN)) {
        int to = u->selected + (k == SDLK_UP ? -1 : 1);
        if (to >= 0 && to < u->playlist.count) { zm_playlist_move(&u->playlist, u->selected, to); u->selected = to; }
        return;
    }
    action = zm_shortcuts_match(&u->shortcuts, k, (SDL_Keymod)e->keysym.mod);
    if (e->repeat && action != ZM_BACK && action != ZM_FORWARD && action != ZM_BACK_LONG && action != ZM_FORWARD_LONG && action != ZM_VOLUME_UP && action != ZM_VOLUME_DOWN) return;
    switch (action) {
    case ZM_OPEN: open_file(u); break;
    case ZM_PLAY: play(u); break;
    case ZM_FULLSCREEN: fullscreen(u); break;
    case ZM_MUTE: zm_player_mute(&u->player); break;
    case ZM_PLAYLIST: u->panel = !u->panel; break;
    case ZM_SUBTITLE_TOGGLE: zm_player_subtitle_toggle(&u->player); break;
    case ZM_SUBTITLE_OPEN: zm_ui_open_subtitle(u); break;
    case ZM_SUBTITLE_NEXT: zm_ui_next_subtitle(u); break;
    case ZM_NEXT_FILE: next_file(u, 1, 0); break;
    case ZM_PREVIOUS_FILE: next_file(u, -1, 0); break;
    case ZM_BACK: zm_player_seek(&u->player, u->state.position - 5); break;
    case ZM_FORWARD: zm_player_seek(&u->player, u->state.position + 5); break;
    case ZM_BACK_LONG: zm_player_seek(&u->player, u->state.position - 30); break;
    case ZM_FORWARD_LONG: zm_player_seek(&u->player, u->state.position + 30); break;
    case ZM_VOLUME_UP: zm_player_volume(&u->player, u->state.volume + 5); break;
    case ZM_VOLUME_DOWN: zm_player_volume(&u->player, u->state.volume - 5); break;
    case ZM_SETTINGS: u->settings = 1; break;
    case ZM_NEXT_AUDIO: zm_player_next_audio(&u->player); break;
    default: break;
    }
}
int zm_ui_shortcut_row(UI *u, int row) {
    int i, match = 0;
    for (i = 0; i < ZM_ACTION_COUNT; ++i) {
        const char *name = zm_text(zm_action_name(i));
        if (*u->search && !strstr(name, u->search) && !strstr(zm_action_name(i), u->search)) continue;
        if (match++ == u->shortcut_scroll + row) return i;
    }
    return -1;
}
static void seek_at(UI *u, int x, int final) {
    ZMControlsLayout layout;
    zm_controls_layout(u,&layout);
    double fraction = (double)(x-layout.seek.x)/layout.seek.w;
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    if (u->state.duration > 0.0) {
        u->drag_position=fraction*u->state.duration;
        u->state.ended=0;
        Uint64 now=SDL_GetTicks64();
        /* The thumb follows every motion; cap expensive decoder seeks during
           dragging, and always commit the exact release position immediately. */
        if(final || now-u->drag_seek_ticks>=100) {
            zm_player_seek(&u->player,u->drag_position);
            u->drag_seek_ticks=now;
        }
    }
}
static void volume_at(UI *u,int x) {
    ZMControlsLayout layout; int volume;
    zm_controls_layout(u,&layout);
    volume=(x-layout.volume.x)*100/layout.volume.w;
    if(volume<0)volume=0; if(volume>100)volume=100;
    u->state.volume=volume; zm_player_volume(&u->player,volume);
}
static void click(UI *u, int x, int y, int clicks) {
    SDL_Rect rect;
    int i;
    ZMControlsLayout layout; zm_controls_layout(u,&layout);
    if (u->settings) { zm_settings_click(u,x,y); return; }
    if(controls_hidden(u)) {
        if(clicks==2)fullscreen(u);
        return;
    }
    rect = (SDL_Rect){u->width - 124, 14, 44, 32};
    if (inside(x, y, rect)) { u->panel = !u->panel; return; }
    if (u->panel && x >= u->width - 320 && y >= 60 && y < layout.video.y+layout.video.h) {
        if (y < 106) {
            if (x < u->width - 214) next_file(u, -1, 0);
            else if (x < u->width - 108) next_file(u, 1, 0);
            else u->playlist.repeat = (u->playlist.repeat + 1) % 3;
        } else if (y > layout.video.y+layout.video.h-44) u->playlist.shuffle = !u->playlist.shuffle;
        else {
            int index = u->panel_scroll + (y - 112) / 38;
            if (index >= 0 && index < u->playlist.count) {
                u->selected = index;
                u->playlist.current = index; zm_player_open(&u->player, u->playlist.items[index]);
            }
        }
        return;
    }
    rect = (SDL_Rect){u->width - 68, 14, 44, 32};
    if (inside(x, y, rect)) { u->settings = 1; return; }
    rect = layout.seek_hit;
    if (inside(x, y, rect) && u->state.duration>0) { u->dragging = 1; SDL_CaptureMouse(SDL_TRUE); seek_at(u, x, 1); return; }
    for (i = 0; i < 6; ++i) if (inside(x, y, layout.buttons[i])) {
        switch (i) {
        case 0: open_file(u); break;
        case 1: zm_player_seek(&u->player, u->state.position - 5); break;
        case 2: play(u); break;
        case 3: zm_player_seek(&u->player, u->state.position + 5); break;
        case 4: zm_player_seek(&u->player, 0); if (!u->state.paused) zm_player_toggle_pause(&u->player); break;
        case 5: zm_player_mute(&u->player); break;
        }
        return;
    }
    rect = layout.fullscreen;
    if (inside(x, y, rect)) { fullscreen(u); return; }
    rect = layout.volume_hit;
    if (inside(x, y, rect)) { u->dragging=2; SDL_CaptureMouse(SDL_TRUE); volume_at(u,x); return; }
    if (!u->state.loaded) open_file(u);
    else if (clicks == 2) fullscreen(u);
}
static void render(UI *u,int present) {
    zm_draw_begin_frame();
    SDL_Color fg = u->config.theme ? (SDL_Color){30, 37, 52, 255} : (SDL_Color){227, 232, 244, 255};
    SDL_Color muted = u->config.theme ? (SDL_Color){85, 93, 110, 255} : (SDL_Color){148, 157, 177, 255};
    ZMControlsLayout layout; zm_controls_layout(u,&layout);
    SDL_Rect area=layout.video, rect;
    const char *labels[6];
    char current[32], total[32], time[80], info[128];
    int i, hidden = controls_hidden(u);
    {
        int mouse_x=-1,mouse_y=-1;
        if(SDL_GetMouseFocus()==u->window)SDL_GetMouseState(&mouse_x,&mouse_y);
        zm_draw_set_pointer(u->settings || hidden?-1:mouse_x,u->settings || hidden?-1:mouse_y);
    }
    SDL_ShowCursor(hidden ? SDL_DISABLE : SDL_ENABLE);
    if (u->config.theme) color(u, 245, 247, 252); else color(u, 15, 18, 27);
    SDL_RenderClear(u->renderer);
    if (hidden) area = (SDL_Rect){0, 0, u->width, u->height};
    color(u, 0, 0, 0); SDL_RenderFillRect(u->renderer, &area);
    if (u->video) {
        double scale_x = (double)area.w / u->video_width;
        double scale_y = (double)area.h / u->video_height;
        double scale = scale_x < scale_y ? scale_x : scale_y;
        rect.w = (int)(u->video_width * scale); rect.h = (int)(u->video_height * scale);
        rect.x = area.x + (area.w - rect.w) / 2; rect.y = area.y + (area.h - rect.h) / 2;
        SDL_RenderCopy(u->renderer, u->video, NULL, &rect);
    } else {
        SDL_Color white = {232, 234, 248, 255};
        SDL_Color gray = {145, 151, 175, 255};
        int text_width, text_height;
        const char *message = u->state.loading || (u->state.loaded && u->state.has_video) ? zm_text("loading") : u->state.loaded && u->state.has_audio ? zm_text("audio") : zm_text("welcome");
        color(u, 108, 87, 230); rect = (SDL_Rect){area.x + area.w / 2 - 32, area.y + area.h / 2 - 90, 64, 52};
        SDL_RenderFillRect(u->renderer, &rect);
        zm_draw_text(u->renderer, u->heading, "ZM", rect.x + 9, rect.y + 6, white);
        zm_draw_text_size(u->heading, message, &text_width, &text_height);
        zm_draw_text(u->renderer, u->heading, message, area.x + (area.w - text_width) / 2, area.y + area.h / 2 - 14, white);
        zm_draw_text_size(u->font, zm_text("drop"), &text_width, &text_height);
        zm_draw_text(u->renderer, u->font, zm_text("drop"), area.x + (area.w - text_width) / 2, area.y + area.h / 2 + 38, gray);
    }
    if (!hidden) {
        zm_draw_text(u->renderer, u->font, "Zernia media", 24, 18, fg);
        zm_draw_text(u->renderer, u->font, ZM_VERSION, 170, 18, muted);
        if (u->state.path[0] && u->state.loaded) {
            SDL_Rect clip = {245, 14, u->width - 381, 32};
            SDL_RenderSetClipRect(u->renderer, &clip);
            zm_draw_text(u->renderer, u->font, zm_basename(u->state.path), 245, 18, muted);
            SDL_RenderSetClipRect(u->renderer, NULL);
        }
        rect = (SDL_Rect){u->width - 124, 14, 44, 32};
        zm_draw_icon_button(u->renderer,rect,ZM_ICON_FOLDER,zm_text(u->folder_mode?"folder_list":"playlist"),u->panel,u->config.theme);
        rect = (SDL_Rect){u->width - 68, 14, 44, 32};
        zm_draw_icon_button(u->renderer,rect,ZM_ICON_SETTINGS,zm_text("settings"),0,u->config.theme);
        {
            ZMUpdateState update; zm_update_snapshot(&u->updater,&update);
            if(update.status==ZM_UPDATE_AVAILABLE || update.status==ZM_UPDATE_READY) {
                color(u,137,111,250); zm_draw_round_rect(u->renderer,(SDL_Rect){rect.x+rect.w-9,rect.y+3,6,6},3);
            }
        }
        clock_text(u->dragging==1?u->drag_position:u->state.position, current, sizeof(current)); clock_text(u->state.duration, total, sizeof(total));
        SDL_snprintf(time, sizeof(time), "%s / %s", current, total);
        zm_draw_text(u->renderer, u->font, time, layout.time.x, layout.time.y, fg);
        if (u->state.loaded) SDL_snprintf(info, sizeof(info), "%s · %d × %d · %s", u->state.codec, u->state.width, u->state.height, u->state.hardware_active ? "GPU" : "CPU");
        else SDL_strlcpy(info, u->state.loading ? zm_text("loading") : zm_text("ready"), sizeof(info));
        SDL_RenderSetClipRect(u->renderer,&layout.info);
        zm_draw_text(u->renderer,u->font,info,layout.info.x,layout.info.y,muted);
        SDL_RenderSetClipRect(u->renderer,NULL);
        rect = layout.seek;
        if (u->config.theme) color(u, 212, 218, 231); else color(u, 53, 59, 78);
        SDL_RenderFillRect(u->renderer, &rect);
        rect.w = u->state.duration > 0 ? (int)((u->width - 48) * (u->dragging==1?u->drag_position:u->state.position) / u->state.duration) : 0;
        if(rect.w<0)rect.w=0; if(rect.w>layout.seek.w)rect.w=layout.seek.w;
        color(u, 131, 108, 250); SDL_RenderFillRect(u->renderer, &rect);
        rect = (SDL_Rect){layout.seek.x + rect.w - 5, layout.seek.y - 4, 10, 12}; SDL_RenderFillRect(u->renderer, &rect);
        labels[0] = zm_text("open"); labels[1] = "-5s"; labels[2] = zm_text(u->state.paused || !u->state.loaded ? "play" : "pause");
        labels[3] = "+5s"; labels[4] = zm_text("stop"); labels[5] = zm_text("mute");
        {
            ZMIcon icons[]={ZM_ICON_FOLDER,ZM_ICON_BACK,u->state.paused || !u->state.loaded?ZM_ICON_PLAY:ZM_ICON_PAUSE,ZM_ICON_FORWARD,ZM_ICON_STOP,u->state.muted?ZM_ICON_MUTE:ZM_ICON_VOLUME};
            for(i=0;i<6;++i)zm_draw_icon_button(u->renderer,layout.buttons[i],icons[i],labels[i],i==2 || (i==5 && u->state.muted),u->config.theme);
        }
        rect = layout.volume;
        color(u, 60, 65, 84); SDL_RenderFillRect(u->renderer, &rect);
        rect.w = 112 * u->state.volume / 100; color(u, 131, 108, 250); SDL_RenderFillRect(u->renderer, &rect);
        zm_draw_round_rect(u->renderer,(SDL_Rect){layout.volume.x+rect.w-4,layout.volume.y-4,8,12},3);
        SDL_snprintf(info, sizeof(info), "%d%%", u->state.volume);
        zm_draw_text(u->renderer, u->font, info, layout.volume_label.x, layout.volume_label.y, muted);
        rect = layout.fullscreen;
        zm_draw_icon_button(u->renderer,rect,u->fullscreen?ZM_ICON_RESTORE:ZM_ICON_FULLSCREEN,zm_text("fullscreen"),u->fullscreen,u->config.theme);
    }
    if (u->panel && !hidden) {
        int max_rows = (layout.video.h-100) / 38;
        rect = (SDL_Rect){u->width - 320, 60, 320, layout.video.h};
        if (u->config.theme) color(u, 232, 236, 245); else color(u, 24, 28, 40);
        SDL_RenderFillRect(u->renderer, &rect);
        rect = (SDL_Rect){u->width - 310, 68, 92, 34};
        zm_draw_icon_button(u->renderer,rect,ZM_ICON_PREVIOUS,zm_text("previous"),0,u->config.theme);
        rect.x += 100; zm_draw_icon_button(u->renderer,rect,ZM_ICON_NEXT,zm_text("next"),0,u->config.theme);
        rect.x += 100; zm_draw_icon_button(u->renderer,rect,u->playlist.repeat==1?ZM_ICON_REPEAT_ONE:ZM_ICON_REPEAT,zm_text(u->playlist.repeat==1?"repeat_one":u->playlist.repeat==2?"repeat_all":"repeat_off"),u->playlist.repeat!=0,u->config.theme);
        for (i = 0; i < max_rows && u->panel_scroll + i < u->playlist.count; ++i) {
            int index = u->panel_scroll + i;
            SDL_Rect clip = {u->width - 304, 112 + i * 38, 288, 34};
            if (index == u->selected) { color(u, 71, 58, 129); SDL_RenderFillRect(u->renderer, &clip); }
            SDL_RenderSetClipRect(u->renderer, &clip);
            zm_draw_text(u->renderer, u->font, zm_basename(u->playlist.items[index]), clip.x + 6, clip.y + 5, index == u->selected ? (SDL_Color){255,255,255,255} : fg);
            SDL_RenderSetClipRect(u->renderer, NULL);
        }
        rect = (SDL_Rect){u->width - 310, layout.video.y+layout.video.h-42, 300, 34};
        zm_draw_icon_button(u->renderer,rect,ZM_ICON_SHUFFLE,zm_text("shuffle"),u->playlist.shuffle,u->config.theme);
    }
    if (u->state.error[0]) {
        rect = (SDL_Rect){24, 72, u->width - 48, 72}; color(u, 80, 29, 45); SDL_RenderFillRect(u->renderer, &rect);
        zm_draw_text(u->renderer, u->font, zm_text("error"), 38, 80, (SDL_Color){255, 224, 229, 255});
        zm_draw_text(u->renderer, u->font, u->state.error, 38, 111, (SDL_Color){255, 224, 229, 255});
    }
    if (u->settings) zm_settings_draw(u);
    if(!hidden)zm_draw_tooltip(u->renderer,u->font,u->width,u->height,u->config.theme);
    if(present)SDL_RenderPresent(u->renderer);
}
static void present_video(UI *u) {
    ZMVideoFrame frame,latest={0};
    int frames;
    if(u->presenting)return;
    u->presenting=1;
    SDL_GetWindowSize(u->window,&u->width,&u->height);
    SDL_RenderSetLogicalSize(u->renderer,u->width,u->height);
    zm_player_snapshot(&u->player,&u->state);
    /* A seek invalidates queued frames, not the last displayed image. Clear
       that image only when opening another media session (including reopen). */
    if(u->media_generation!=u->state.media_generation) {
        SDL_DestroyTexture(u->video); u->video=NULL; u->media_generation=u->state.media_generation;
    }
    if(u->generation!=u->state.generation) {
        u->generation=u->state.generation; u->handled_end=0;
    }
    if(u->state.ended && !u->handled_end && !u->state.loading && !u->dragging) { u->handled_end=1; next_file(u,1,1); }
    /* Only the newest due frame can appear in this refresh. Avoid uploading
       every obsolete frame after a UI delay, which would prolong the delay. */
    for(frames=0;frames<ZM_FRAME_CAPACITY && zm_player_take_frame(&u->player,&frame);++frames) {
        av_frame_free(&latest.image); latest=frame;
    }
    if(latest.image) {
        frame=latest;
        if(!u->video || u->video_width!=frame.image->width || u->video_height!=frame.image->height) {
            SDL_DestroyTexture(u->video); u->video_width=frame.image->width; u->video_height=frame.image->height;
            u->video=SDL_CreateTexture(u->renderer,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,u->video_width,u->video_height);
        }
        if(u->video) {
            SDL_UpdateYUVTexture(u->video,NULL,frame.image->data[0],frame.image->linesize[0],frame.image->data[1],frame.image->linesize[1],frame.image->data[2],frame.image->linesize[2]);
            ++u->presented_frames;
        }
        av_frame_free(&frame.image);
    }
    render(u,1); u->presenting=0;
}
static void move_frame(void *data) {
    UI *u=data;
    if(u->running && u->config.play_during_move)present_video(u);
}
/* Reproduce the interval after a seek flush and before its first decoded frame.
   Stop the worker so a fast decoder cannot accidentally hide the regression. */
static int smoke_seek_display(UI *u) {
    SDL_Texture *before=u->video;
    unsigned uploads=u->presented_frames;
    int paused;
    if(!before)return 0;
    SDL_AtomicSet(&u->player.quit,1);
    SDL_WaitThread(u->player.thread,NULL); u->player.thread=NULL;
    for(paused=0;paused<=1;++paused) {
        SDL_LockMutex(u->player.mutex);
        zm_player_clear_locked(&u->player); ++u->player.state.generation;
        u->player.state.paused=paused; u->player.state.ended=0;
        SDL_UnlockMutex(u->player.mutex);
        u->handled_end=1;
        present_video(u);
        if(u->video!=before || u->presented_frames!=uploads || u->handled_end)return 0;
    }
    SDL_LockMutex(u->player.mutex);
    ++u->player.state.media_generation; ++u->player.state.generation;
    u->player.state.has_video=0; u->player.state.has_audio=1;
    SDL_UnlockMutex(u->player.mutex);
    present_video(u);
    SDL_Log("Seek display: retained during playing/paused seek; cleared for new audio media");
    return u->video==NULL;
}
static void capture(UI *u, const char *path) {
    SDL_Surface *surface;
    int w, h;
    /* The backbuffer is invalidated by Present; redraw before reading it. */
    render(u,0);
    SDL_GetRendererOutputSize(u->renderer, &w, &h);
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface) {
        if (SDL_RenderReadPixels(u->renderer, NULL, surface->format->format, surface->pixels, surface->pitch) == 0)
            SDL_SaveBMP(surface, path);
        SDL_FreeSurface(surface);
    }
    SDL_RenderPresent(u->renderer);
}
static void update_notifications(UI *u) {
    ZMUpdateState update;
    zm_update_snapshot(&u->updater,&update);
    if(u->update_install_pending[0] && update.status==ZM_UPDATE_FAILED)u->update_install_pending[0]='\0';
    if(u->dragging || u->scrollbar_drag || u->capturing>=0 || !(SDL_GetWindowFlags(u->window)&SDL_WINDOW_INPUT_FOCUS))return;
    if(u->update_install_pending[0] && update.status==ZM_UPDATE_READY) {
        int approved=!strcmp(u->update_install_pending,update.version);
        u->update_install_pending[0]='\0';
        if(approved) {
            if(zm_update_install(&u->updater))u->running=0;
            else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Zernia media",zm_text("update_failed"),u->window);
            return;
        }
    }
    if(!u->config.update_notify || (update.status!=ZM_UPDATE_READY && update.status!=ZM_UPDATE_AVAILABLE) ||
       !update.version[0] || !strcmp(u->update_notified_version,update.version))return;
    SDL_strlcpy(u->update_notified_version,update.version,sizeof(u->update_notified_version));
    ZMSnapshot playback; zm_player_snapshot(&u->player,&playback);
    int resume=playback.loaded && !playback.paused && !playback.ended;
    if(resume)zm_player_toggle_pause(&u->player);
    char message[512]; int selected=0;
    SDL_snprintf(message,sizeof(message),"%s\n\nZM %s",zm_text("update_prompt"),update.version);
    const SDL_MessageBoxButtonData buttons[]={
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,1,zm_text("update_yes")},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,0,zm_text("update_later")}
    };
    const SDL_MessageBoxData dialog={SDL_MESSAGEBOX_INFORMATION,u->window,zm_text("updates"),message,2,buttons,NULL};
    if(SDL_ShowMessageBox(&dialog,&selected)<0)selected=0;
    if(selected==1) {
        if(update.status==ZM_UPDATE_READY) {
            if(zm_update_install(&u->updater))u->running=0;
            else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Zernia media",zm_text("update_failed"),u->window);
        } else {
            SDL_strlcpy(u->update_install_pending,update.version,sizeof(u->update_install_pending));
            zm_update_check(&u->updater,1);
        }
    }
    if(resume && u->running)zm_player_toggle_pause(&u->player);
}
int zm_app_run(const char *path, int smoke_seconds, const char *capture_path) {
    UI u = {0};
    Uint64 start;
    Uint32 media_event=(Uint32)-1;
    char *smoke_media=NULL;
    int smoke_media_sent=0;

    int result = 0,smoke_controls=0,smoke_folder=0,smoke_events_sent=0,smoke_selected=-1,smoke_move=0,move_tested=0,smoke_fullscreen=0;
    double smoke_target=0;
    int smoke_auto_folder=smoke_seconds?smoke_option("ZM_SMOKE_AUTO_FOLDER",-1):-1;
    int smoke_edge=smoke_seconds?smoke_option("ZM_SMOKE_FULLSCREEN_EDGE",0):0,edge_tested=0;
    int smoke_seek=smoke_seconds?smoke_option("ZM_SMOKE_SEEK_DISPLAY",0):0;
    if(smoke_seconds) {
        smoke_controls=smoke_option("ZM_SMOKE_CONTROLS",0);
        smoke_folder=smoke_option("ZM_SMOKE_FOLDER_CLICK",0);
        smoke_move=smoke_option("ZM_SMOKE_MOVE",0);
        smoke_fullscreen=smoke_option("ZM_SMOKE_FULLSCREEN",0);
    }
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    /* Deliver the activation click too, so an unfocused window's buttons
       respond to the first click instead of requiring a second one. */
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) return 1;
    media_event=SDL_RegisterEvents(1);
    if (TTF_Init() < 0) { SDL_Quit(); return 1; }
    zm_config_load(&u.config);
    if(!zm_update_init(&u.updater)) { result=1; goto cleanup; }
    if(smoke_seconds) {
        const char *lang=SDL_getenv("ZM_SMOKE_LANGUAGE");
        if(lang)SDL_strlcpy(u.config.language,lang,sizeof(u.config.language));
        const char *media=SDL_getenv("ZM_SMOKE_MEDIA_PATH");
        if(media) {
            ZMPlaylist expected; int selected;
            zm_playlist_init(&expected); selected=zm_playlist_load_folder(&expected,media);
            smoke_media=SDL_strdup(selected>=0?expected.items[selected]:media);
            zm_playlist_destroy(&expected);
        }
        u.config.theme=smoke_option("ZM_SMOKE_LIGHT",u.config.theme)!=0;
        u.settings=smoke_option("ZM_SMOKE_PAGE",0);
        u.panel=smoke_option("ZM_SMOKE_PLAYLIST",0);
        u.config.fullscreen_hide_ui=smoke_option("ZM_SMOKE_HIDE_UI",u.config.fullscreen_hide_ui)!=0;
        u.config.play_during_move=smoke_option("ZM_SMOKE_PLAY_DURING_MOVE",u.config.play_during_move)!=0;
        if(smoke_auto_folder>=0)u.config.auto_open_folder=smoke_auto_folder!=0;
    }
    zm_i18n_load(u.config.language);
    zm_playlist_init(&u.playlist); u.selected = -1;
    zm_shortcuts_load(&u.shortcuts); u.capturing = u.conflict = u.shortcut_selected = -1;
    u.window = SDL_CreateWindow("Zernia media · ZM · " ZM_VERSION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1120, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!u.window) { result = 1; goto cleanup; }
    zm_window_set_icon(u.window);
    SDL_SetWindowMinimumSize(u.window, 760, 480);
    if(smoke_seconds) {
        int width=smoke_option("ZM_SMOKE_WIDTH",1120),height=smoke_option("ZM_SMOKE_HEIGHT",720);
        if(width<760)width=760; if(height<480)height=480;
        SDL_SetWindowSize(u.window,width,height);
        u.settings_scroll=smoke_option("ZM_SMOKE_SCROLL",0);
        u.dropdown=smoke_option("ZM_SMOKE_DROPDOWN",0);
    }
    u.renderer = SDL_CreateRenderer(u.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!u.renderer) u.renderer = SDL_CreateRenderer(u.window, -1, SDL_RENDERER_SOFTWARE);
    if (!u.renderer) { result = 1; goto cleanup; }
    zm_ui_reload_fonts(&u);
    if (!u.font || !u.heading) { result = 1; goto cleanup; }
    if (!zm_player_init(&u.player)) { result = 1; goto cleanup; }
    zm_player_volume(&u.player, u.config.volume);
    SDL_LockMutex(u.player.mutex);
    u.player.state.subtitles_enabled = u.config.subtitles_enabled;
    u.player.state.subtitle_auto = u.config.subtitle_auto;
    u.player.state.subtitle_size = u.config.subtitle_size;
    u.player.state.subtitle_style = u.config.subtitle_style;
    u.player.state.subtitle_delay_ms = u.config.subtitle_delay_ms;
    u.player.state.hardware_mode = u.config.hardware_mode;
    SDL_strlcpy(u.player.state.preferred_language, u.config.language, sizeof(u.player.state.preferred_language));
    SDL_UnlockMutex(u.player.mutex);
    if (path)open_media(&u,path);
    u.running = 1; start = u.activity = SDL_GetTicks64();
    if(!smoke_seconds && media_event!=(Uint32)-1 && !zm_window_accept_media(u.window,media_event,&u.config.reuse_window))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Could not register media forwarding");
    zm_instance_ready();
    if(!smoke_seconds && u.config.update_auto_check)zm_update_check(&u.updater,u.config.update_auto_download);
    if(!zm_window_set_move_callback(u.window,move_frame,&u))SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Could not install live move callback");
    if(smoke_fullscreen)fullscreen(&u);
    while (u.running) {
        SDL_Event e;
        Uint64 cycle_start=SDL_GetTicks64();
        SDL_GetWindowSize(u.window, &u.width, &u.height);
        SDL_RenderSetLogicalSize(u.renderer, u.width, u.height);
        zm_player_snapshot(&u.player, &u.state);
        if(smoke_media && !smoke_media_sent && u.video && u.state.loaded && SDL_GetTicks64()-start>500) {
            SDL_Event media; SDL_zero(media); media.type=media_event; media.user.data1=SDL_strdup(smoke_media);
            if(!media.user.data1 || SDL_PushEvent(&media)<=0) { SDL_free(media.user.data1); result=9; }
            else smoke_media_sent=1;
        }
        if(smoke_fullscreen==2 && u.state.loaded && SDL_GetTicks64()-start>500) {
            if(!u.state.paused)zm_player_toggle_pause(&u.player);
            smoke_fullscreen=3;
        }
        if(smoke_edge && !edge_tested && u.state.loaded && SDL_GetTicks64()-start>600) {
            SDL_KeyboardEvent tab; SDL_zero(tab); tab.keysym.sym=SDLK_TAB;
            key(&u,&tab);
            if(controls_hidden(&u))result=7;
            key(&u,&tab);
            if(!controls_hidden(&u))result=7;
            smoke_pointer_event(u.window,SDL_MOUSEMOTION,u.width/2,1);
            smoke_pointer_event(u.window,SDL_MOUSEMOTION,u.width/2,u.height-1);
            smoke_pointer_event(u.window,SDL_MOUSEBUTTONDOWN,u.width/2,u.height-1);
            smoke_pointer_event(u.window,SDL_MOUSEBUTTONUP,u.width/2,u.height-1);
            edge_tested=1;
        }
        if(smoke_move && !move_tested && u.state.loaded && SDL_GetTicks64()-start>500) {
            unsigned before=u.presented_frames;
            if(!zm_window_test_move(u.window,650) || (u.config.play_during_move?u.presented_frames<=before:u.presented_frames!=before))result=5;
            SDL_Log("Move test: enabled=%d presented=%u",u.config.play_during_move,u.presented_frames-before);
            move_tested=1;
        }
        if((smoke_controls || smoke_folder) && !smoke_events_sent && u.state.loaded && SDL_GetTicks64()-start>500) {
            ZMControlsLayout controls; zm_controls_layout(&u,&controls);
            if(smoke_controls) {
                int seek_x=controls.seek.x+controls.seek.w/4,seek_y=controls.seek.y;
                int volume_x=controls.volume.x+controls.volume.w*3/4,volume_y=controls.volume.y;
                if(!u.state.paused)zm_player_toggle_pause(&u.player);
                smoke_target=(double)(controls.seek.w/4)/controls.seek.w*u.state.duration;
                smoke_pointer_event(u.window,SDL_MOUSEBUTTONDOWN,controls.seek.x+controls.seek.w/2,seek_y);
                smoke_pointer_event(u.window,SDL_MOUSEMOTION,controls.seek.x-50,seek_y);
                smoke_pointer_event(u.window,SDL_MOUSEMOTION,controls.seek.x+controls.seek.w+50,seek_y);
                smoke_pointer_event(u.window,SDL_MOUSEMOTION,seek_x,seek_y);
                smoke_pointer_event(u.window,SDL_MOUSEBUTTONUP,seek_x,seek_y);
                smoke_pointer_event(u.window,SDL_MOUSEBUTTONDOWN,controls.volume.x+controls.volume.w/2,volume_y);
                smoke_pointer_event(u.window,SDL_MOUSEMOTION,controls.volume.x-50,volume_y);
                smoke_pointer_event(u.window,SDL_MOUSEMOTION,controls.volume.x+controls.volume.w+50,volume_y);
                smoke_pointer_event(u.window,SDL_MOUSEMOTION,volume_x,volume_y);
                smoke_pointer_event(u.window,SDL_MOUSEBUTTONUP,volume_x,volume_y);
            } else if(u.folder_mode && u.playlist.count>1) {
                smoke_selected=u.playlist.current==0?1:0; u.panel_scroll=0;
                smoke_pointer_event(u.window,SDL_MOUSEBUTTONDOWN,u.width-160,128+smoke_selected*38);
                smoke_pointer_event(u.window,SDL_MOUSEBUTTONUP,u.width-160,128+smoke_selected*38);
            }
            smoke_events_sent=1;
        }
        while (SDL_PollEvent(&e)) {
            if(e.type==media_event) {
                char *media=e.user.data1;
                u.settings=u.dropdown=0;
                u.dragging=u.scrollbar_drag=0; SDL_CaptureMouse(SDL_FALSE);
                open_media(&u,media); SDL_free(media);
                continue;
            }
            switch (e.type) {
            case SDL_QUIT: u.running = 0; break;
            case SDL_DROPBEGIN: u.drop_first=1; break;
            case SDL_DROPCOMPLETE: u.drop_first=0; break;
            case SDL_DROPFILE: {
                const char *extension = strrchr(e.drop.file, '.');
                if (extension && u.state.loaded && (!SDL_strcasecmp(extension, ".srt") || !SDL_strcasecmp(extension, ".ass") || !SDL_strcasecmp(extension, ".ssa") || !SDL_strcasecmp(extension, ".vtt"))) {
                    zm_player_subtitle(&u.player, e.drop.file, -1); SDL_free(e.drop.file); break;
                }
                if (u.drop_first || (!u.state.loaded && !u.state.loading)) {
                    open_media(&u,e.drop.file); u.state.loading=1; u.drop_first=0;
                } else if(!u.folder_mode)zm_playlist_add(&u.playlist,e.drop.file);
                SDL_free(e.drop.file); break;
            }
            case SDL_KEYDOWN: u.activity = SDL_GetTicks64(); key(&u, &e.key); break;
            case SDL_TEXTINPUT:
                if(u.settings==3 && u.capturing<0 && strlen(u.search)+strlen(e.text.text)<sizeof(u.search)) { SDL_strlcat(u.search,e.text.text,sizeof(u.search)); u.shortcut_scroll=u.settings_scroll=0; }
                break;
            case SDL_MOUSEMOTION:
                u.activity = SDL_GetTicks64();
                if(u.scrollbar_drag)zm_settings_drag(&u,e.motion.y); else if (u.dragging==1) seek_at(&u,e.motion.x,0); else if(u.dragging==2)volume_at(&u,e.motion.x);
                break;
            case SDL_MOUSEBUTTONDOWN: u.activity = SDL_GetTicks64(); if (e.button.button == SDL_BUTTON_LEFT) click(&u, e.button.x, e.button.y, e.button.clicks); break;
            case SDL_MOUSEBUTTONUP:
                if(e.button.button==SDL_BUTTON_LEFT) {
                    if(u.dragging==1)seek_at(&u,e.button.x,1); else if(u.dragging==2)volume_at(&u,e.button.x);
                    u.dragging=u.scrollbar_drag=0; SDL_CaptureMouse(SDL_FALSE);
                }
                break;
            case SDL_WINDOWEVENT:
                if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) { u.dragging=u.scrollbar_drag=0; SDL_CaptureMouse(SDL_FALSE); }
                break;
            case SDL_MOUSEWHEEL: {
                int x, y; SDL_GetMouseState(&x, &y); (void)y;
                u.activity = SDL_GetTicks64();
                if(u.settings) {
                    zm_settings_scroll(&u,e.wheel.y);
                } else if (u.panel && x >= u.width - 320) {
                    ZMControlsLayout layout; zm_controls_layout(&u,&layout);
                    int max_rows = (layout.video.h-100) / 38;
                    int maximum = u.playlist.count > max_rows ? u.playlist.count - max_rows : 0;
                    u.panel_scroll -= e.wheel.y;
                    if (u.panel_scroll < 0) u.panel_scroll = 0;
                    if (u.panel_scroll > maximum) u.panel_scroll = maximum;
                } else zm_player_volume(&u.player, u.state.volume + e.wheel.y * 5);
                break;
            }
            default: break;
            }
        }
        if(u.running && !smoke_seconds)update_notifications(&u);
        if(!u.running)break;
        present_video(&u);
        if (smoke_seconds > 0 && SDL_GetTicks64() - start >= (Uint64)smoke_seconds * 1000u) {
            if (capture_path) capture(&u, capture_path);
            SDL_Log("Smoke: loaded=%d video=%u audio=%u position=%.3f error=%s", u.state.loaded, u.state.video_frames, u.state.audio_frames, u.state.position, u.state.error);
            if (path && (!u.state.loaded || u.state.error[0] || (u.state.has_video && !u.state.video_frames) || (u.state.has_audio && !u.state.audio_frames))) result = 2;
            if(smoke_controls && (!smoke_events_sent || u.dragging || u.state.volume!=75 || !u.state.paused || fabs(u.state.position-smoke_target)>0.1))result=3;
            if(smoke_folder && (smoke_selected<0 || u.playlist.current!=smoke_selected || strcmp(u.state.path,u.playlist.items[smoke_selected])))result=4;
            if(smoke_move && !move_tested)result=5;
            if(smoke_auto_folder>=0 && (!u.folder_mode || u.panel!=(smoke_auto_folder!=0)))result=6;
            if(smoke_edge && (!edge_tested || !controls_hidden(&u)))result=7;
            if(smoke_seek && !smoke_seek_display(&u))result=8;
            if(smoke_media && (!smoke_media_sent || strcmp(u.state.path,smoke_media) || u.state.media_generation<2))result=9;
            u.running = 0;
        }
        /* Vsync already paces normal rendering. Sleep only for a very fast
           renderer rather than adding 5ms after every display refresh. */
        Uint64 cycle_elapsed=SDL_GetTicks64()-cycle_start;
        if(cycle_elapsed<4)SDL_Delay((Uint32)(4-cycle_elapsed));
    }
    u.config.volume = u.state.volume;
    u.config.subtitles_enabled = u.state.subtitles_enabled; u.config.subtitle_auto = u.state.subtitle_auto;
    u.config.subtitle_size = u.state.subtitle_size; u.config.subtitle_style = u.state.subtitle_style; u.config.subtitle_delay_ms = u.state.subtitle_delay_ms;
    if (!smoke_seconds) { zm_config_save(&u.config); zm_shortcuts_save(&u.shortcuts); }
    zm_window_clear_move_callback(u.window);
    zm_player_destroy(&u.player);
cleanup:
    SDL_free(smoke_media);
    zm_window_stop_media(u.window);
    if(media_event!=(Uint32)-1) {
        SDL_Event pending;
        while(SDL_PeepEvents(&pending,1,SDL_GETEVENT,media_event,media_event)>0)SDL_free(pending.user.data1);
    }
    zm_update_destroy(&u.updater);
    zm_playlist_destroy(&u.playlist);
    if (result == 1) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Zernia media", SDL_GetError(), u.window);
    SDL_DestroyTexture(u.video);
    zm_draw_clear_cache();
    if (u.font) TTF_CloseFont(u.font);
    if (u.heading) TTF_CloseFont(u.heading);
    if (u.bold) TTF_CloseFont(u.bold);
    zm_draw_set_fallback(NULL);
    for(int i=0;i<ZM_FALLBACK_COUNT;++i) {
        if(u.glyph_fonts[i])TTF_CloseFont(u.glyph_fonts[i]);
        if(u.heading_glyph_fonts[i])TTF_CloseFont(u.heading_glyph_fonts[i]);
    }
    SDL_DestroyRenderer(u.renderer); SDL_DestroyWindow(u.window);
    TTF_Quit(); SDL_Quit(); return result;
}
