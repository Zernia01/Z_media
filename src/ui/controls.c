#include "zm/ui.h"
void zm_controls_layout(UI *u, ZMControlsLayout *l) {
    int widths[6],i,x=24,total=0,fullscreen_width=50;
    for(i=0;i<6;++i) { widths[i]=50; total+=widths[i]+8; }
    {
        int two_rows=total+140+fullscreen_width+48>u->width;
        int bottom=two_rows?194:140;
        int buttons_y=u->height-(two_rows?112:62);
        for(i=0;i<6;++i) { l->buttons[i]=(SDL_Rect){x,buttons_y,widths[i],36}; x+=widths[i]+8; }
        l->video=(SDL_Rect){0,60,u->width-(u->panel?320:0),u->height-bottom-60};
        l->time=(SDL_Rect){24,u->height-bottom+12,240,24};
        l->info=(SDL_Rect){u->width/2,u->height-bottom+12,u->width/2-24,24};
        l->seek=(SDL_Rect){24,u->height-bottom+48,u->width-48,4};
        l->seek_hit=(SDL_Rect){24,l->seek.y-10,l->seek.w,24};
        l->fullscreen=(SDL_Rect){u->width-24-fullscreen_width,u->height-62,fullscreen_width,36};
        l->volume=(SDL_Rect){two_rows?24:l->fullscreen.x-132,u->height-36,112,4};
        l->volume_label=(SDL_Rect){l->volume.x,u->height-62,112,20};
        l->volume_hit=(SDL_Rect){l->volume.x,l->volume.y-8,112,24};
    }
}
