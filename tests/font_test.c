#include "zm/app.h"
#include <string.h>
int main(void) {
    TTF_Font *body=NULL,*heading=NULL,*fallbacks[ZM_FALLBACK_COUNT]={0},*titles[ZM_FALLBACK_COUNT]={0};
    const char *examples[]={"【全程回放（下）】B站哔哩哔哩2026冰火歌回夏日篇.mp4","한국어 日本語 简体中文 繁體中文 國漢文混用體","ZM 한글-日本語-中文 🎬 𠮷.mp4"};
    int i,failures=0;
    SDL_SetMainReady(); SDL_Init(SDL_INIT_TIMER); if(TTF_Init()<0)return 1;
    char *path=zm_font_path("ko",1,1); if(path) { body=TTF_OpenFont(path,16); heading=TTF_OpenFont(path,28); SDL_free(path); }
    for(i=0;i<ZM_FALLBACK_COUNT;++i) { path=zm_font_fallback_path(i); if(path) { fallbacks[i]=TTF_OpenFont(path,16); titles[i]=TTF_OpenFont(path,28); SDL_free(path); } }
    if(!body || !heading || !fallbacks[1] || !titles[1]) { failures=1; goto done; }
    /* Reproduce absent glyphs in the rounded UI face, then verify actual
       mixed-script coverage and measurement against rendered pixels. */
    zm_draw_set_fallback(NULL);
    if(zm_draw_text_supported(body,examples[0]))++failures;
    zm_draw_set_fallbacks(heading,fallbacks,titles);
    for(i=0;i<3;++i) {
        SDL_Surface *small,*large; int w,h;
        if(!zm_draw_text_supported(body,examples[i]) || !zm_draw_text_supported(heading,examples[i]))++failures;
        small=zm_draw_text_surface(body,examples[i],(SDL_Color){255,255,255,255});
        large=zm_draw_text_surface(heading,examples[i],(SDL_Color){255,255,255,255});
        if(!small || !large)++failures;
        else {
            zm_draw_text_size(body,examples[i],&w,&h);
            if(w!=small->w || h!=small->h || large->h<=small->h)++failures;
        }
        SDL_FreeSurface(small); SDL_FreeSurface(large);
    }
    /* Korean/Chinese must also work with only the two bundled font families. */
    for(i=0;i<ZM_FALLBACK_COUNT;++i)if(i!=1) { if(fallbacks[i])TTF_CloseFont(fallbacks[i]); fallbacks[i]=NULL; if(titles[i])TTF_CloseFont(titles[i]); titles[i]=NULL; }
    zm_draw_set_fallbacks(heading,fallbacks,titles);
    if(!zm_draw_text_supported(body,examples[0]) || !zm_draw_text_supported(body,examples[1]))++failures;
done:
    zm_draw_set_fallback(NULL); if(body)TTF_CloseFont(body); if(heading)TTF_CloseFont(heading);
    for(i=0;i<ZM_FALLBACK_COUNT;++i) { if(fallbacks[i])TTF_CloseFont(fallbacks[i]); if(titles[i])TTF_CloseFont(titles[i]); }
    TTF_Quit(); SDL_Quit(); printf("Mixed-script font tests: %d failure(s)\n",failures); return failures?1:0;
}
