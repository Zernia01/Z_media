#include "zm/app.h"
#include <string.h>
#include <math.h>
#include <stdint.h>
static SDL_Point pointer={-1,-1};
static const char *tooltip_label;
static SDL_Rect tooltip_rect;
static char previous_tip[128];
static SDL_Rect previous_rect;
static Uint64 tooltip_since;
void zm_draw_set_pointer(int x,int y) { pointer=(SDL_Point){x,y}; }
int zm_draw_hovered(SDL_Renderer *r,SDL_Rect rect) {
    SDL_Rect clip;
    if(SDL_RenderIsClipEnabled(r)) {
        SDL_RenderGetClipRect(r,&clip);
        if(!SDL_PointInRect(&pointer,&clip))return 0;
    }
    return SDL_PointInRect(&pointer,&rect);
}
void zm_draw_round_rect(SDL_Renderer *r, SDL_Rect rect, int radius) {
    int y;
    if(radius>rect.h/2)radius=rect.h/2;
    if(radius>rect.w/2)radius=rect.w/2;
    for(y=0;y<rect.h;++y) {
        int inset=0, distance;
        if(y<radius || y>=rect.h-radius) {
            distance=y<radius?radius-y-1:y-(rect.h-radius);
            inset=radius-(int)sqrt((double)(radius*radius-distance*distance));
        }
        SDL_RenderDrawLine(r,rect.x+inset,rect.y+y,rect.x+rect.w-inset-1,rect.y+y);
    }
}
typedef struct TextCache {
    SDL_Texture *texture;
    TTF_Font *font;
    SDL_Color color;
    char text[512];
    int width, height;
    Uint64 used;
} TextCache;
static TextCache cache[128];
static TTF_Font *fallback_fonts[ZM_FALLBACK_COUNT],*heading_fonts[ZM_FALLBACK_COUNT],*heading_base;
void zm_draw_set_fallback(TTF_Font *font) {
    memset(fallback_fonts,0,sizeof(fallback_fonts)); memset(heading_fonts,0,sizeof(heading_fonts));
    fallback_fonts[0]=font; heading_base=NULL;
}
void zm_draw_set_fallbacks(TTF_Font *heading,TTF_Font **body,TTF_Font **titles) {
    memcpy(fallback_fonts,body,sizeof(fallback_fonts)); memcpy(heading_fonts,titles,sizeof(heading_fonts)); heading_base=heading;
}
static TTF_Font *glyph_font(TTF_Font *base,Uint32 cp) {
    TTF_Font **pool=base==heading_base?heading_fonts:fallback_fonts; int i;
    if(TTF_GlyphIsProvided32(base,cp))return base;
    for(i=0;i<ZM_FALLBACK_COUNT;++i)if(pool[i] && TTF_GlyphIsProvided32(pool[i],cp))return pool[i];
    return base;
}
int zm_draw_text_supported(TTF_Font *font,const char *text) {
    if(!font || !text)return 0;
    Uint32 *points=(Uint32 *)SDL_iconv_string("UTF-32LE","UTF-8",text,strlen(text)+1); size_t i; int result=1;
    if(!font || !points) { SDL_free(points); return 0; }
    for(i=0;points[i];++i)if(!TTF_GlyphIsProvided32(glyph_font(font,points[i]),points[i])) { result=0; break; }
    SDL_free(points); return result;
}
static size_t utf8_put(char *out,Uint32 cp) {
    if(cp<0x80) { out[0]=(char)cp; return 1; }
    if(cp<0x800) { out[0]=(char)(0xc0|(cp>>6)); out[1]=(char)(0x80|(cp&63)); return 2; }
    if(cp<0x10000) { out[0]=(char)(0xe0|(cp>>12)); out[1]=(char)(0x80|((cp>>6)&63)); out[2]=(char)(0x80|(cp&63)); return 3; }
    out[0]=(char)(0xf0|(cp>>18)); out[1]=(char)(0x80|((cp>>12)&63)); out[2]=(char)(0x80|((cp>>6)&63)); out[3]=(char)(0x80|(cp&63)); return 4;
}
SDL_Surface *zm_draw_text_surface(TTF_Font *font,const char *text,SDL_Color color) {
    typedef struct Run { SDL_Surface *surface; int ascent; } Run;
    Uint32 *points; Run *runs=NULL; char *chunk=NULL; SDL_Surface *result=NULL;
    size_t n=0,index=0,count=0,i; int width=0,ascent=0,descent=0;
    if(!font || !text || !*text)return NULL;
    points=(Uint32 *)SDL_iconv_string("UTF-32LE","UTF-8",text,strlen(text)+1);
    if(!points)return NULL;
    while(points[n])++n;
    if(n>(SIZE_MAX-1)/4)goto done;
    runs=SDL_calloc(n,sizeof(*runs)); chunk=SDL_malloc(n*4+1);
    if(!runs || !chunk)goto done;
    while(index<n) {
        TTF_Font *selected=glyph_font(font,points[index]); size_t used=0;
        do { used+=utf8_put(chunk+used,points[index++]); } while(index<n && glyph_font(font,points[index])==selected);
        chunk[used]='\0';
        runs[count].surface=TTF_RenderUTF8_Blended(selected,chunk,color);
        if(!runs[count].surface)goto done;
        runs[count].ascent=TTF_FontAscent(selected);
        width+=runs[count].surface->w;
        if(runs[count].ascent>ascent)ascent=runs[count].ascent;
        if(runs[count].surface->h-runs[count].ascent>descent)descent=runs[count].surface->h-runs[count].ascent;
        ++count;
    }
    result=SDL_CreateRGBSurfaceWithFormat(0,width>0?width:1,ascent+descent>0?ascent+descent:1,32,SDL_PIXELFORMAT_ARGB8888);
    if(result) {
        SDL_FillRect(result,NULL,0); width=0;
        SDL_SetSurfaceBlendMode(result,SDL_BLENDMODE_BLEND);
        for(i=0;i<count;++i) {
            SDL_Rect rect={width,ascent-runs[i].ascent,0,0};
            SDL_SetSurfaceBlendMode(runs[i].surface,SDL_BLENDMODE_NONE);
            SDL_BlitSurface(runs[i].surface,NULL,result,&rect); width+=runs[i].surface->w;
        }
    }
done:
    for(i=0;i<count;++i)SDL_FreeSurface(runs[i].surface);
    SDL_free(runs); SDL_free(chunk); SDL_free(points); return result;
}
int zm_draw_text_size(TTF_Font *font,const char *text,int *width,int *height) {
    int i; SDL_Surface *surface;
    for(i=0;i<128;++i)if(cache[i].texture && cache[i].font==font && !strcmp(cache[i].text,text)) {
        *width=cache[i].width; *height=cache[i].height; return 0;
    }
    surface=zm_draw_text_surface(font,text,(SDL_Color){255,255,255,255});
    if(!surface) { *width=*height=0; return -1; }
    *width=surface->w; *height=surface->h; SDL_FreeSurface(surface); return 0;
}
void zm_draw_clear_cache(void) {
    int i;
    for (i = 0; i < 128; ++i) SDL_DestroyTexture(cache[i].texture);
    memset(cache, 0, sizeof(cache));
}
void zm_draw_text(SDL_Renderer *r, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_Rect rect;
    TextCache *entry = NULL;
    int i, oldest = 0;
    if (!font || !text || !*text) return;
    for (i = 0; i < 128; ++i) {
        if (cache[i].texture && cache[i].font == font && !memcmp(&cache[i].color, &color, sizeof(color)) && !strcmp(cache[i].text, text)) {
            entry = &cache[i]; break;
        }
        if (cache[i].used < cache[oldest].used) oldest = i;
    }
    if (entry) {
        rect = (SDL_Rect){x, y, entry->width, entry->height};
        entry->used = SDL_GetTicks64(); SDL_RenderCopy(r, entry->texture, NULL, &rect); return;
    }
    surface=zm_draw_text_surface(font,text,color);
    if (!surface) return;
    texture = SDL_CreateTextureFromSurface(r, surface);
    rect.x = x; rect.y = y; rect.w = surface->w; rect.h = surface->h;
    if (texture) {
        SDL_RenderCopy(r, texture, NULL, &rect);
        if (strlen(text) < sizeof(cache[0].text)) {
            entry = &cache[oldest]; SDL_DestroyTexture(entry->texture);
            entry->texture = texture; entry->font = font; entry->color = color;
            entry->width = rect.w; entry->height = rect.h; entry->used = SDL_GetTicks64();
            SDL_strlcpy(entry->text, text, sizeof(entry->text));
        } else SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}
void zm_draw_button(SDL_Renderer *r, TTF_Font *font, SDL_Rect rect, const char *label, int active, int light) {
    SDL_Color fg = light ? (SDL_Color){30, 37, 52, 255} : (SDL_Color){225, 230, 242, 255};
    int width = 0, height = 0;
    int hovered=zm_draw_hovered(r,rect);
    if (active) { SDL_SetRenderDrawColor(r, hovered?130:108, hovered?111:87, hovered?247:230, 255); fg = (SDL_Color){255, 255, 255, 255}; }
    else if(hovered && light) SDL_SetRenderDrawColor(r,200,207,224,255);
    else if(hovered) SDL_SetRenderDrawColor(r,61,69,90,255);
    else if (light) SDL_SetRenderDrawColor(r, 221, 226, 237, 255);
    else SDL_SetRenderDrawColor(r, 39, 45, 61, 255);
    zm_draw_round_rect(r, rect, 7);
    zm_draw_text_size(font, label, &width, &height);
    {
        SDL_Rect previous, clip=rect;
        SDL_bool clipped=SDL_RenderIsClipEnabled(r);
        SDL_RenderGetClipRect(r,&previous);
        if(clipped)SDL_IntersectRect(&clip,&previous,&clip);
        SDL_RenderSetClipRect(r,&clip);
        zm_draw_text(r, font, label, rect.x + (rect.w - width) / 2, rect.y + (rect.h - height) / 2, fg);
        SDL_RenderSetClipRect(r,clipped?&previous:NULL);
    }
}
void zm_draw_begin_frame(void) { tooltip_label=NULL; }
void zm_draw_tooltip_target(SDL_Renderer *r,SDL_Rect rect,const char *label) {
    if(zm_draw_hovered(r,rect)) { tooltip_rect=rect; tooltip_label=label; }
}
void zm_draw_tooltip(SDL_Renderer *r,TTF_Font *font,int width,int height,int light) {
    int w,h;
    SDL_Rect box;
    if(!tooltip_label) { previous_tip[0]='\0'; return; }
    if(strcmp(previous_tip,tooltip_label) || memcmp(&previous_rect,&tooltip_rect,sizeof(tooltip_rect))) {
        SDL_strlcpy(previous_tip,tooltip_label,sizeof(previous_tip)); previous_rect=tooltip_rect; tooltip_since=SDL_GetTicks64();
    }
    if(SDL_GetTicks64()-tooltip_since<400)return;
    zm_draw_text_size(font,tooltip_label,&w,&h);
    box=(SDL_Rect){tooltip_rect.x+(tooltip_rect.w-w-20)/2,tooltip_rect.y-h-22,w+20,h+14};
    if(box.y<8)box.y=tooltip_rect.y+tooltip_rect.h+8;
    if(box.x<8)box.x=8; if(box.x+box.w>width-8)box.x=width-box.w-8;
    if(box.y+box.h>height-8)box.y=height-box.h-8;
    SDL_SetRenderDrawColor(r,light?222:49,light?228:56,light?240:74,255);
    zm_draw_round_rect(r,box,6);
    zm_draw_text(r,font,tooltip_label,box.x+10,box.y+7,light?(SDL_Color){30,37,52,255}:(SDL_Color){240,243,250,255});
}
static void line(SDL_Renderer *r,int x,int y,int a,int b,int c,int d) {
    SDL_RenderDrawLine(r,x+a,y+b,x+c,y+d);
    SDL_RenderDrawLine(r,x+a,y+b+1,x+c,y+d+1);
}
static void triangle(SDL_Renderer *r,int x,int y,int reversed) {
    int i;
    for(i=0;i<9;++i)SDL_RenderDrawLine(r,x+(reversed?8-i:i),y+i,x+(reversed?8-i:i),y+16-i);
}
void zm_draw_icon(SDL_Renderer *r,SDL_Rect rect,ZMIcon icon,SDL_Color color) {
    int x=rect.x+(rect.w-22)/2,y=rect.y+(rect.h-22)/2,i;
    SDL_Rect box;
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    switch(icon) {
    case ZM_ICON_PLAY: triangle(r,x+7,y+3,0); break;
    case ZM_ICON_PAUSE:
        box=(SDL_Rect){x+5,y+3,4,17}; SDL_RenderFillRect(r,&box); box.x=x+13; SDL_RenderFillRect(r,&box); break;
    case ZM_ICON_STOP: box=(SDL_Rect){x+4,y+4,15,15}; SDL_RenderFillRect(r,&box); break;
    case ZM_ICON_BACK: case ZM_ICON_FORWARD:
        triangle(r,x+3,y+3,icon==ZM_ICON_BACK); triangle(r,x+12,y+3,icon==ZM_ICON_BACK); break;
    case ZM_ICON_PREVIOUS: case ZM_ICON_NEXT:
        triangle(r,x+7,y+3,icon==ZM_ICON_PREVIOUS); line(r,x,y,icon==ZM_ICON_PREVIOUS?3:19,3,icon==ZM_ICON_PREVIOUS?3:19,19); break;
    case ZM_ICON_FOLDER:
        line(r,x,y,2,6,9,6); line(r,x,y,9,6,12,9); line(r,x,y,12,9,20,9);
        line(r,x,y,2,6,2,19); line(r,x,y,2,19,19,19); line(r,x,y,19,19,21,11); line(r,x,y,3,11,21,11); break;
    case ZM_ICON_VOLUME: case ZM_ICON_MUTE:
        box=(SDL_Rect){x+2,y+8,5,7}; SDL_RenderFillRect(r,&box);
        for(i=0;i<6;++i)SDL_RenderDrawLine(r,x+6+i,y+8-i,x+6+i,y+14+i);
        if(icon==ZM_ICON_MUTE) { line(r,x,y,15,8,21,15); line(r,x,y,21,8,15,15); }
        else { line(r,x,y,15,7,18,10); line(r,x,y,18,10,18,13); line(r,x,y,18,13,15,16); } break;
    case ZM_ICON_FULLSCREEN: case ZM_ICON_RESTORE:
        for(i=0;i<4;++i) {
            int a=i&1?19:3,b=i&2?19:3,c=i&1?-5:5,d=i&2?-5:5;
            if(icon==ZM_ICON_RESTORE) { a+=c; b+=d; c=-c; d=-d; }
            line(r,x,y,a,b,a+c,b); line(r,x,y,a,b,a,b+d);
        } break;
    case ZM_ICON_SETTINGS: case ZM_ICON_APPEARANCE: case ZM_ICON_INFO:
        for(i=0;i<32;++i) {
            double a=i*6.283185307/32,b=(i+1)*6.283185307/32;
            line(r,x,y,11+(int)(8*cos(a)),11+(int)(8*sin(a)),11+(int)(8*cos(b)),11+(int)(8*sin(b)));
        }
        if(icon==ZM_ICON_SETTINGS) {
            box=(SDL_Rect){x+8,y+8,7,7}; SDL_RenderDrawRect(r,&box);
            for(i=0;i<8;++i) {
                double a=i*6.283185307/8;
                line(r,x,y,11+(int)(8*cos(a)),11+(int)(8*sin(a)),11+(int)(11*cos(a)),11+(int)(11*sin(a)));
            }
        } else if(icon==ZM_ICON_INFO) { line(r,x,y,11,10,11,16); box=(SDL_Rect){x+10,y+6,3,2}; SDL_RenderFillRect(r,&box); }
        else { box=(SDL_Rect){x+6,y+6,3,3}; SDL_RenderFillRect(r,&box); box.x=x+13; SDL_RenderFillRect(r,&box); box.x=x+6; box.y=y+13; SDL_RenderFillRect(r,&box); } break;
    case ZM_ICON_GENERAL:
        for(i=0;i<3;++i) { line(r,x,y,2,4+i*7,20,4+i*7); box=(SDL_Rect){x+(i==1?13:5),y+2+i*7,4,5}; SDL_RenderFillRect(r,&box); } break;
    case ZM_ICON_PERFORMANCE:
        for(i=0;i<3;++i) { box=(SDL_Rect){x+3+i*6,y+13-i*5,4,8+i*5}; SDL_RenderFillRect(r,&box); } break;
    case ZM_ICON_SUBTITLES: case ZM_ICON_KEYBOARD:
        box=(SDL_Rect){x+1,y+4,21,15}; SDL_RenderDrawRect(r,&box);
        if(icon==ZM_ICON_KEYBOARD) {
            for(i=0;i<8;++i) { box=(SDL_Rect){x+4+(i%4)*4,y+7+(i/4)*4,2,2}; SDL_RenderFillRect(r,&box); }
            line(r,x,y,7,16,16,16);
        } else for(i=0;i<2;++i) { line(r,x,y,5+i*9,8,9+i*9,8); line(r,x,y,5+i*9,8,5+i*9,14); line(r,x,y,5+i*9,14,9+i*9,14); } break;
    case ZM_ICON_REPEAT: case ZM_ICON_REPEAT_ONE:
        line(r,x,y,3,6,19,6); line(r,x,y,19,6,16,3); line(r,x,y,19,6,16,9);
        line(r,x,y,19,16,3,16); line(r,x,y,3,16,6,13); line(r,x,y,3,16,6,19);
        if(icon==ZM_ICON_REPEAT_ONE)line(r,x,y,11,9,11,13); break;
    case ZM_ICON_SHUFFLE:
        line(r,x,y,2,5,19,17); line(r,x,y,2,17,19,5);
        line(r,x,y,19,5,14,5); line(r,x,y,19,5,19,10); line(r,x,y,19,17,14,17); line(r,x,y,19,17,19,12); break;
    }
}
void zm_draw_icon_button(SDL_Renderer *r,SDL_Rect rect,ZMIcon icon,const char *label,int active,int light) {
    int hovered=zm_draw_hovered(r,rect);
    SDL_Color fg=light?(SDL_Color){30,37,52,255}:(SDL_Color){225,230,242,255};
    if(active) { SDL_SetRenderDrawColor(r,hovered?130:108,hovered?111:87,hovered?247:230,255); fg=(SDL_Color){255,255,255,255}; }
    else if(hovered && light)SDL_SetRenderDrawColor(r,200,207,224,255);
    else if(hovered)SDL_SetRenderDrawColor(r,61,69,90,255);
    else if(light)SDL_SetRenderDrawColor(r,221,226,237,255);
    else SDL_SetRenderDrawColor(r,39,45,61,255);
    zm_draw_round_rect(r,rect,7); zm_draw_icon(r,rect,icon,fg); zm_draw_tooltip_target(r,rect,label);
}
