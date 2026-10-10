#include "zm/ui.h"
#include <string.h>
#include <math.h>

typedef struct SettingsLayout { SDL_Rect panel, sidebar, view, close, track; } SettingsLayout;
static const int pages[]={1,4,5,2,3,6};
static const char *page_keys[]={"general","appearance","performance","subtitles","shortcuts","information"};
static const char *languages[]={"ko","en","ja","zh-CN","zh-TW","ko-Hanja"};
static const char *language_names[]={"한국어","English","日本語","简体中文","繁體中文","國漢文混用體"};
static SDL_Color foreground(UI *u) { return u->config.theme?(SDL_Color){34,40,52,255}:(SDL_Color){227,232,240,255}; }
static SDL_Color secondary(UI *u) { return u->config.theme?(SDL_Color){99,110,127,255}:(SDL_Color){145,156,175,255}; }
static int hit(int x,int y,SDL_Rect r) { return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h; }
static SettingsLayout layout(UI *u) {
    SettingsLayout l;
    int w=u->width-48,h=u->height-48;
    if(w>980)w=980; if(h>660)h=660;
    l.panel=(SDL_Rect){(u->width-w)/2,(u->height-h)/2,w,h};
    l.sidebar=(SDL_Rect){l.panel.x,l.panel.y,84,h};
    l.view=(SDL_Rect){l.panel.x+116,l.panel.y+132,w-154,h-156};
    l.close=(SDL_Rect){l.panel.x+w-48,l.panel.y+16,32,32};
    l.track=(SDL_Rect){l.panel.x+w-12,l.view.y,5,l.view.h};
    return l;
}
static void ink(UI *u,SDL_Color c) { SDL_SetRenderDrawColor(u->renderer,c.r,c.g,c.b,c.a); }
static void separator(UI *u,int x,int y,int w) {
    ink(u,u->config.theme?(SDL_Color){211,217,228,255}:(SDL_Color){42,49,61,255});
    SDL_RenderDrawLine(u->renderer,x,y,x+w,y);
}
static void icon(UI *u,int type,int x,int y,SDL_Color c) {
    const ZMIcon icons[]={ZM_ICON_GENERAL,ZM_ICON_APPEARANCE,ZM_ICON_PERFORMANCE,ZM_ICON_SUBTITLES,ZM_ICON_KEYBOARD,ZM_ICON_INFO};
    zm_draw_icon(u->renderer,(SDL_Rect){x,y,22,22},icons[type],c);
}
static void checkbox(UI *u,SDL_Rect r,int checked) {
    ink(u,checked?(SDL_Color){137,111,250,255}:u->config.theme?(SDL_Color){215,222,232,255}:(SDL_Color){58,66,80,255});
    zm_draw_round_rect(u->renderer,r,4);
    if(checked) {
        ink(u,(SDL_Color){255,255,255,255});
        SDL_RenderDrawLine(u->renderer,r.x+4,r.y+8,r.x+7,r.y+11);
        SDL_RenderDrawLine(u->renderer,r.x+7,r.y+11,r.x+12,r.y+4);
    }
}
static int check_row(UI *u,SettingsLayout l,int y,const char *label,const char *description,int checked,int x,int mouse_y,int apply) {
    SDL_Rect row={l.view.x,y,l.view.w,description?64:44};
    if(!apply) {
        zm_draw_text(u->renderer,u->font,zm_text(label),row.x,y+2,foreground(u));
        if(description)zm_draw_text(u->renderer,u->font,zm_text(description),row.x,y+28,secondary(u));
        checkbox(u,(SDL_Rect){row.x+row.w-18,y+3,16,16},checked);
    }
    return apply && hit(x,mouse_y,l.view) && hit(x,mouse_y,row);
}
static int option(UI *u,SettingsLayout l,int y,int width,const char *label,int x,int mouse_y,int apply) {
    SDL_Rect r={l.view.x,y,width,36};
    if(!apply) {
        int hovered=!u->dropdown && zm_draw_hovered(u->renderer,r);
        ink(u,hovered?(u->config.theme?(SDL_Color){207,215,232,255}:(SDL_Color){51,60,78,255}):u->config.theme?(SDL_Color){225,230,239,255}:(SDL_Color){28,34,44,255});
        zm_draw_round_rect(u->renderer,r,6);
        zm_draw_text(u->renderer,u->font,label,r.x+14,r.y+10,foreground(u));
        ink(u,secondary(u)); SDL_RenderDrawLine(u->renderer,r.x+r.w-22,r.y+15,r.x+r.w-18,r.y+19);
        SDL_RenderDrawLine(u->renderer,r.x+r.w-18,r.y+19,r.x+r.w-14,r.y+15);
    }
    return apply && hit(x,mouse_y,l.view) && hit(x,mouse_y,r);
}
static void section(UI *u,int x,int y,const char *title,int apply) {
    if(!apply)zm_draw_text(u->renderer,u->bold,zm_text(title),x,y,foreground(u));
}
static void choice(UI *u,SettingsLayout l,int y,int width,const char *label,int id,int x,int mouse_y,int apply) {
    if(option(u,l,y,width,label,x,mouse_y,apply))u->dropdown=u->dropdown==id?0:id;
    if(u->dropdown==id)u->dropdown_anchor=(SDL_Rect){l.view.x,y,width,36};
}
static void clamp_scroll(UI *u,SettingsLayout l) {
    int maximum=u->settings_content_height-l.view.h;
    if(maximum<0)maximum=0;
    if(u->settings_scroll>maximum)u->settings_scroll=maximum;
    if(u->settings_scroll<0)u->settings_scroll=0;
}
static int update_controls(UI *u,SettingsLayout l,int y,int x,int mouse_y,int apply) {
    ZMUpdateState state; char text[160];
    const char *keys[]={"update_idle","update_checking","update_current","update_available","update_downloading","update_ready","update_failed"};
    zm_update_snapshot(&u->updater,&state);
    section(u,l.view.x,y,"updates",apply); y+=34;
    SDL_snprintf(text,sizeof(text),"%s%s%s",zm_text(keys[state.status]),state.version[0]?" · ":"",state.version);
    if(state.status==ZM_UPDATE_DOWNLOADING)SDL_snprintf(text,sizeof(text),"%s · %.1f MB",zm_text("update_downloading"),(double)state.progress/1024.0);
    if(!apply)zm_draw_text(u->renderer,u->font,text,l.view.x,y,secondary(u));
    y+=34;
    if(option(u,l,y,240,zm_text(state.status==ZM_UPDATE_READY?"update_install":state.status==ZM_UPDATE_AVAILABLE?"update_download":"update_check"),x,mouse_y,apply)) {
        if(state.status==ZM_UPDATE_READY) {
            if(zm_update_install(&u->updater))u->running=0;
            else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Zernia media",zm_text("update_failed"),u->window);
        } else zm_update_check(&u->updater,state.status==ZM_UPDATE_AVAILABLE?1:u->config.update_auto_download);
    }
    return y+64;
}
static void content(UI *u,SettingsLayout l,int x,int mouse_y,int apply) {
    int top=l.view.y+8-u->settings_scroll,y=top,i;
    char text[160];
    if(!apply)SDL_RenderSetClipRect(u->renderer,&l.view);
    if(u->settings==1) {
        int current=0;
        for(i=0;i<6;++i)if(!strcmp(languages[i],u->config.language))current=i;
        section(u,l.view.x,y,"language_label",apply);
        choice(u,l,y+34,240,language_names[current],1,x,mouse_y,apply);
        y+=98; if(!apply)separator(u,l.view.x,y,l.view.w); y+=28;
        section(u,l.view.x,y,"playback_options",apply); y+=42;
        if(check_row(u,l,y,"auto_open_folder","auto_open_folder_hint",u->config.auto_open_folder,x,mouse_y,apply))u->config.auto_open_folder=!u->config.auto_open_folder;
        y+=76;
        if(check_row(u,l,y,"subtitles_enabled",NULL,u->state.subtitles_enabled,x,mouse_y,apply))zm_player_subtitle_toggle(&u->player);
        y+=44;
        if(check_row(u,l,y,"subtitle_auto","next_file_hint",u->state.subtitle_auto,x,mouse_y,apply)) {
            SDL_LockMutex(u->player.mutex); u->player.state.subtitle_auto=!u->player.state.subtitle_auto; SDL_UnlockMutex(u->player.mutex);
        }
        y+=80; if(!apply)separator(u,l.view.x,y,l.view.w); y+=28;
        section(u,l.view.x,y,"audio_track",apply); y+=34;
        SDL_snprintf(text,sizeof(text),"%s  %d / %d",zm_text("audio_track"),u->state.audio_count?u->state.audio_index+1:0,u->state.audio_count);
        if(option(u,l,y,260,text,x,mouse_y,apply))zm_player_next_audio(&u->player);
        y+=64;
        if(check_row(u,l,y,"mute",NULL,u->state.muted,x,mouse_y,apply))zm_player_mute(&u->player);
        y+=64;
        if(!apply)separator(u,l.view.x,y,l.view.w); y+=28;
        section(u,l.view.x,y,"default_app",apply); y+=34;
        if(option(u,l,y,300,zm_text("default_app_open"),x,mouse_y,apply) && !zm_open_default_apps(u->window))
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Zernia media",zm_text("default_app_error"),u->window);
        y+=52;
        if(!apply)zm_draw_text(u->renderer,u->font,zm_text("default_app_hint"),l.view.x,y,secondary(u));
        y+=54;
        if(!apply)separator(u,l.view.x,y,l.view.w); y+=28;
        if(check_row(u,l,y,"update_auto_check",NULL,u->config.update_auto_check,x,mouse_y,apply))u->config.update_auto_check=!u->config.update_auto_check;
        y+=44;
        if(check_row(u,l,y,"update_auto_download",NULL,u->config.update_auto_download,x,mouse_y,apply))u->config.update_auto_download=!u->config.update_auto_download;
        y+=60; y=update_controls(u,l,y,x,mouse_y,apply);
    } else if(u->settings==4) {
        section(u,l.view.x,y,"playback_display",apply); y+=40;
        if(check_row(u,l,y,"fullscreen_hide_ui","fullscreen_hide_hint",u->config.fullscreen_hide_ui,x,mouse_y,apply))u->config.fullscreen_hide_ui=!u->config.fullscreen_hide_ui;
        y+=76;
        if(check_row(u,l,y,"play_during_move","play_during_move_hint",u->config.play_during_move,x,mouse_y,apply))u->config.play_during_move=!u->config.play_during_move;
        y+=80; if(!apply)separator(u,l.view.x,y,l.view.w); y+=28;
        section(u,l.view.x,y,"theme_label",apply); y+=34;
        choice(u,l,y,240,zm_text(u->config.theme?"light_theme":"dark_theme"),2,x,mouse_y,apply);
        y+=64; if(!apply)separator(u,l.view.x,y,l.view.w); y+=28;
        section(u,l.view.x,y,"font_label",apply); y+=40;
        if(check_row(u,l,y,"rounded_font","rounded_font_hint",u->config.rounded_font,x,mouse_y,apply)) {
            u->config.rounded_font=!u->config.rounded_font; zm_ui_reload_fonts(u);
        }
        y+=88;
        if(!apply) {
            ink(u,u->config.theme?(SDL_Color){233,236,245,255}:(SDL_Color){23,29,39,255});
            zm_draw_round_rect(u->renderer,(SDL_Rect){l.view.x,y,l.view.w,100},9);
            zm_draw_text(u->renderer,u->heading,zm_text("font_preview"),l.view.x+20,y+20,foreground(u));
            zm_draw_text(u->renderer,u->font,"Zernia media  0123456789",l.view.x+20,y+60,secondary(u));
        }
        y+=124;
    } else if(u->settings==5) {
        section(u,l.view.x,y,"hardware_label",apply); y+=34;
        choice(u,l,y,300,zm_text(u->config.hardware_mode==2?"hardware_off":u->config.hardware_mode==1?"hardware_on":"hardware_auto"),3,x,mouse_y,apply);
        y+=68;
        if(!apply)zm_draw_text(u->renderer,u->font,zm_text("hardware_hint"),l.view.x,y,secondary(u));
        y+=56; if(!apply)separator(u,l.view.x,y,l.view.w); y+=28;
        section(u,l.view.x,y,"decoder_status",apply); y+=38;
        if(!apply) {
            zm_draw_text(u->renderer,u->font,zm_text(u->state.hardware_active?"gpu_active":"cpu_active"),l.view.x,y,foreground(u));
            y+=40; zm_draw_text(u->renderer,u->font,zm_text("bounded_queue_hint"),l.view.x,y,secondary(u));
        } else y+=40;
        y+=64;
    } else if(u->settings==2) {
        if(check_row(u,l,y,"subtitles_enabled",NULL,u->state.subtitles_enabled,x,mouse_y,apply))zm_player_subtitle_toggle(&u->player);
        y+=56; if(!apply)separator(u,l.view.x,y,l.view.w); y+=26;
        section(u,l.view.x,y,"subtitle_sources",apply); y+=34;
        if(option(u,l,y,280,zm_text("subtitle_open"),x,mouse_y,apply))zm_ui_open_subtitle(u);
        y+=48;
        if(option(u,l,y,280,zm_text("subtitle_next"),x,mouse_y,apply))zm_ui_next_subtitle(u);
        y+=64;
        if(check_row(u,l,y,"subtitle_auto","next_file_hint",u->state.subtitle_auto,x,mouse_y,apply)) {
            SDL_LockMutex(u->player.mutex); u->player.state.subtitle_auto=!u->player.state.subtitle_auto; SDL_UnlockMutex(u->player.mutex);
        }
        y+=80; if(!apply)separator(u,l.view.x,y,l.view.w); y+=26;
        section(u,l.view.x,y,"subtitle_style",apply); y+=38;
        if(check_row(u,l,y,"subtitle_style",NULL,u->state.subtitle_style,x,mouse_y,apply))zm_player_subtitle_options(&u->player,u->state.subtitle_delay_ms,u->state.subtitle_size,!u->state.subtitle_style);
        y+=48;
        SDL_snprintf(text,sizeof(text),"%s  %d",zm_text("subtitle_size"),u->state.subtitle_size);
        if(!apply)zm_draw_text(u->renderer,u->font,text,l.view.x,y+9,foreground(u));
        for(i=0;i<2;++i) {
            SDL_Rect r={l.view.x+l.view.w-100+i*54,y,44,36};
            if(!apply)zm_draw_button(u->renderer,u->font,r,i?"+":"-",0,u->config.theme);
            if(apply && hit(x,mouse_y,l.view) && hit(x,mouse_y,r))zm_player_subtitle_options(&u->player,u->state.subtitle_delay_ms,u->state.subtitle_size+(i?2:-2),1);
        }
        y+=62; if(!apply)separator(u,l.view.x,y,l.view.w); y+=26;
        section(u,l.view.x,y,"subtitle_sync",apply); y+=38;
        SDL_snprintf(text,sizeof(text),"%+d ms",u->state.subtitle_delay_ms);
        if(!apply)zm_draw_text(u->renderer,u->font,text,l.view.x,y+9,foreground(u));
        for(i=0;i<2;++i) {
            SDL_Rect r={l.view.x+l.view.w-184+i*96,y,88,36};
            if(!apply)zm_draw_button(u->renderer,u->font,r,i?"+100 ms":"-100 ms",0,u->config.theme);
            if(apply && hit(x,mouse_y,l.view) && hit(x,mouse_y,r))zm_player_subtitle_options(&u->player,u->state.subtitle_delay_ms+(i?100:-100),u->state.subtitle_size,u->state.subtitle_style);
        }
        y+=68;
    } else if(u->settings==3) {
        SDL_Rect search={l.view.x,y,l.view.w,38};
        if(!apply) {
            ink(u,u->config.theme?(SDL_Color){227,232,241,255}:(SDL_Color){25,31,42,255});
            zm_draw_round_rect(u->renderer,search,6);
            SDL_snprintf(text,sizeof(text),"%s: %s",zm_text("search"),u->search);
            zm_draw_text(u->renderer,u->font,text,search.x+12,search.y+10,foreground(u));
        }
        if(apply && hit(x,mouse_y,search))SDL_StartTextInput();
        y+=54;
        for(i=0;;++i) {
            int action=zm_ui_shortcut_row(u,i);
            SDL_Rect row={l.view.x,y,l.view.w,42};
            char binding[80];
            if(action<0)break;
            if(!apply) {
                if(action==u->shortcut_selected) { ink(u,u->config.theme?(SDL_Color){226,219,250,255}:(SDL_Color){42,38,70,255}); zm_draw_round_rect(u->renderer,row,6); }
                zm_draw_text(u->renderer,u->font,zm_text(zm_action_name(action)),row.x+10,row.y+13,foreground(u));
                zm_shortcuts_label(u->shortcuts.bindings[action],binding,sizeof(binding));
                zm_draw_text(u->renderer,u->font,binding,row.x+row.w-160,row.y+13,secondary(u));
            }
            if(apply && hit(x,mouse_y,l.view) && hit(x,mouse_y,row)) { u->capturing=u->shortcut_selected=action; SDL_StopTextInput(); }
            y+=48;
        }
        y+=12;
        for(i=0;i<2;++i) {
            SDL_Rect r={l.view.x+i*208,y,192,36};
            if(!apply)zm_draw_button(u->renderer,u->font,r,zm_text(i?"reset_one":"reset_all"),0,u->config.theme);
            if(apply && hit(x,mouse_y,l.view) && hit(x,mouse_y,r)) {
                if(i)zm_shortcuts_restore(&u->shortcuts,u->shortcut_selected); else zm_shortcuts_defaults(&u->shortcuts);
            }
        }
        y+=60;
    } else {
        section(u,l.view.x,y,"about",apply); y+=44;
        if(!apply) {
            zm_draw_text(u->renderer,u->heading,"Zernia media",l.view.x,y,foreground(u));
            zm_draw_text(u->renderer,u->font,"ZM · " ZM_VERSION,l.view.x,y+40,secondary(u));
            separator(u,l.view.x,y+90,l.view.w);
            zm_draw_text(u->renderer,u->font,zm_text("about_description"),l.view.x,y+120,foreground(u));
            zm_draw_text(u->renderer,u->font,zm_text("font_credit"),l.view.x,y+164,secondary(u));
        }
        y+=224;
        section(u,l.view.x,y,"support",apply); y+=36;
        {
            SDL_Rect button={l.view.x,y,280,36};
            if(!apply)zm_draw_button(u->renderer,u->font,button,zm_text("support_open"),1,u->config.theme);
            else if(hit(x,mouse_y,l.view) && hit(x,mouse_y,button))SDL_OpenURL("https://buymeacoffee.com/zernia");
        }
        y+=52;
        if(!apply)zm_draw_text(u->renderer,u->font,"buymeacoffee.com/zernia",l.view.x,y,secondary(u));
        y+=44;
        if(!apply)separator(u,l.view.x,y,l.view.w); y+=28;
        y=update_controls(u,l,y,x,mouse_y,apply);
    }
    u->settings_content_height=y-top;
    if(!apply)SDL_RenderSetClipRect(u->renderer,NULL);
}
static SDL_Rect popup(UI *u,SettingsLayout l) {
    int count=u->dropdown==1?6:u->dropdown==2?2:3;
    SDL_Rect anchor=u->dropdown_anchor;
    int height=count*38,y=anchor.y+anchor.h+6;
    int bottom=l.panel.y+l.panel.h-12,top=l.panel.y+12;
    /* Follow the actual selector, including scrolling/layout changes. Flip
       directly above it when there is no room below. */
    if(y+height>bottom)y=anchor.y-height-6;
    if(y<top)y=top;
    return (SDL_Rect){anchor.x,y,anchor.w,height};
}
void zm_settings_draw(UI *u) {
    SettingsLayout l=layout(u);
    int i,index=0;
    int mouse_x=-1,mouse_y=-1;
    if(SDL_GetMouseFocus()==u->window)SDL_GetMouseState(&mouse_x,&mouse_y);
    zm_draw_set_pointer(u->capturing>=0?-1:mouse_x,u->capturing>=0?-1:mouse_y);
    const char *descriptions[]={"general_description","appearance_description","performance_description","subtitles_description","shortcuts_description","information_description"};
    SDL_SetRenderDrawBlendMode(u->renderer,SDL_BLENDMODE_BLEND);
    ink(u,(SDL_Color){0,0,0,170}); SDL_RenderFillRect(u->renderer,NULL);
    ink(u,u->config.theme?(SDL_Color){244,246,250,255}:(SDL_Color){15,19,26,255}); zm_draw_round_rect(u->renderer,l.panel,12);
    SDL_RenderSetClipRect(u->renderer,&l.panel);
    ink(u,u->config.theme?(SDL_Color){233,237,244,255}:(SDL_Color){21,26,35,255}); zm_draw_round_rect(u->renderer,l.sidebar,12);
    ink(u,u->config.theme?(SDL_Color){209,216,228,255}:(SDL_Color){41,48,60,255});
    SDL_RenderDrawLine(u->renderer,l.sidebar.x+l.sidebar.w,l.sidebar.y,l.sidebar.x+l.sidebar.w,l.sidebar.y+l.sidebar.h);
    zm_draw_icon(u->renderer,(SDL_Rect){l.sidebar.x+24,l.sidebar.y+24,36,36},ZM_ICON_SETTINGS,foreground(u));
    for(i=0;i<6;++i) {
        SDL_Rect r={l.sidebar.x+12,l.sidebar.y+70+i*46,l.sidebar.w-24,38};
        SDL_Color c=secondary(u);
        if(u->settings==pages[i]) {
            index=i;
            ink(u,u->config.theme?(SDL_Color){225,218,247,255}:(SDL_Color){42,38,68,255});
            zm_draw_round_rect(u->renderer,r,8); c=foreground(u);
        }
        if(u->settings!=pages[i] && !u->dropdown && zm_draw_hovered(u->renderer,r)) {
            ink(u,u->config.theme?(SDL_Color){218,225,238,255}:(SDL_Color){35,42,56,255});
            zm_draw_round_rect(u->renderer,r,8); c=foreground(u);
        }
        icon(u,i,r.x+(r.w-22)/2,r.y+(r.h-22)/2,c);
        if(!u->dropdown)zm_draw_tooltip_target(u->renderer,r,zm_text(page_keys[i]));
    }
    SDL_RenderSetClipRect(u->renderer,NULL);
    {
        SDL_Rect header={l.view.x,l.panel.y+24,l.view.w,76};
        SDL_RenderSetClipRect(u->renderer,&header);
        zm_draw_text(u->renderer,u->heading,zm_text(page_keys[index]),l.view.x,l.panel.y+30,foreground(u));
    zm_draw_text(u->renderer,u->font,zm_text(descriptions[index]),l.view.x,l.panel.y+73,secondary(u));
        SDL_RenderSetClipRect(u->renderer,NULL);
    }
    separator(u,l.view.x,l.panel.y+116,l.view.w);
    if(zm_draw_hovered(u->renderer,l.close)) {
        ink(u,u->config.theme?(SDL_Color){217,223,235,255}:(SDL_Color){48,56,72,255});
        zm_draw_round_rect(u->renderer,l.close,6);
    }
    ink(u,secondary(u));
    SDL_RenderDrawLine(u->renderer,l.close.x+10,l.close.y+10,l.close.x+22,l.close.y+22);
    SDL_RenderDrawLine(u->renderer,l.close.x+22,l.close.y+10,l.close.x+10,l.close.y+22);
    if(u->dropdown)zm_draw_set_pointer(-1,-1);
    content(u,l,0,0,0); clamp_scroll(u,l);
    if(u->settings_content_height>l.view.h) {
        SDL_Rect thumb=l.track;
        int maximum=u->settings_content_height-l.view.h;
        thumb.h=l.view.h*l.view.h/u->settings_content_height;
        if(thumb.h<28)thumb.h=28;
        thumb.y+=u->settings_scroll*(l.track.h-thumb.h)/maximum;
        ink(u,u->config.theme?(SDL_Color){217,222,231,255}:(SDL_Color){37,43,55,255}); zm_draw_round_rect(u->renderer,l.track,2);
        ink(u,secondary(u)); zm_draw_round_rect(u->renderer,thumb,2);
    }
    if(u->dropdown) {
        SDL_Rect r=popup(u,l);
        zm_draw_set_pointer(mouse_x,mouse_y);
        ink(u,u->config.theme?(SDL_Color){225,231,242,255}:(SDL_Color){33,39,52,255}); zm_draw_round_rect(u->renderer,r,6);
        for(i=0;i<r.h/38;++i) {
            SDL_Rect row={r.x+4,r.y+i*38,r.w-8,38};
            const char *label=u->dropdown==1?language_names[i]:u->dropdown==2?zm_text(i?"light_theme":"dark_theme"):zm_text(i==0?"hardware_auto":i==1?"hardware_on":"hardware_off");
            if(zm_draw_hovered(u->renderer,row)) {
                ink(u,u->config.theme?(SDL_Color){203,213,232,255}:(SDL_Color){57,65,86,255});
                zm_draw_round_rect(u->renderer,row,5);
            }
            zm_draw_text(u->renderer,u->dropdown==1 && (i==3 || i==4) && u->cjk?u->cjk:u->font,label,r.x+14,r.y+i*38+11,foreground(u));
        }
    }
    if(u->capturing>=0) {
        SDL_Rect r={l.view.x,l.view.y+50,l.view.w,126};
        ink(u,u->config.theme?(SDL_Color){218,211,242,255}:(SDL_Color){46,38,77,255}); zm_draw_round_rect(u->renderer,r,10);
        zm_draw_text(u->renderer,u->font,zm_text(u->conflict>=0?"shortcut_conflict":"shortcut_capture"),r.x+20,r.y+24,foreground(u));
        if(u->conflict>=0)zm_draw_text(u->renderer,u->bold,zm_text(zm_action_name(u->conflict)),r.x+20,r.y+70,foreground(u));
    }
}
void zm_settings_click(UI *u,int x,int y) {
    SettingsLayout l=layout(u);
    int i;
    if(hit(x,y,l.close)) {
        u->settings=u->dropdown=u->settings_scroll=0; u->capturing=u->conflict=-1; SDL_StopTextInput(); return;
    }
    if(u->capturing>=0)return;
    if(u->dropdown) {
        SDL_Rect r=popup(u,l);
        if(hit(x,y,r)) {
            int selected=(y-r.y)/38;
            if(u->dropdown==1)zm_ui_set_language(u,languages[selected]);
            else if(u->dropdown==2)u->config.theme=selected;
            else {
                u->config.hardware_mode=selected;
                SDL_LockMutex(u->player.mutex); u->player.state.hardware_mode=selected; SDL_UnlockMutex(u->player.mutex);
                if(u->state.loaded)zm_player_open(&u->player,u->state.path);
            }
            u->dropdown=0; return;
        }
        /* Dismiss the popup and let the same click reach another control.
           Clicking its own selector only closes it instead of reopening it. */
        int anchor_hit=hit(x,y,u->dropdown_anchor);
        u->dropdown=0;
        if(anchor_hit)return;
    }
    for(i=0;i<6;++i) {
        SDL_Rect r={l.sidebar.x+12,l.sidebar.y+70+i*46,l.sidebar.w-24,38};
        if(hit(x,y,r)) {
            u->settings=pages[i]; u->settings_scroll=u->dropdown=0;
            if(u->settings==3)SDL_StartTextInput(); else SDL_StopTextInput();
            return;
        }
    }
    if(x>=l.track.x-5 && x<=l.track.x+10 && y>=l.track.y && y<l.track.y+l.track.h) {
        u->scrollbar_drag=1; zm_settings_drag(u,y); return;
    }
    content(u,l,x,y,1); clamp_scroll(u,l);
}
void zm_settings_scroll(UI *u,int amount) {
    u->dropdown=0; u->settings_scroll-=amount*48; clamp_scroll(u,layout(u));
}
void zm_settings_drag(UI *u,int y) {
    SettingsLayout l=layout(u);
    int max=u->settings_content_height-l.view.h;
    if(max<0)max=0;
    u->settings_scroll=(y-l.track.y)*max/l.track.h;
    clamp_scroll(u,l);
}
