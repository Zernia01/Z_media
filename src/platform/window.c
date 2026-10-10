#include "zm/app.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <SDL_syswm.h>
typedef struct MoveCallback { void (*callback)(void *); void *data; int busy; } MoveCallback;
static HWND native_window(SDL_Window *window) {
    SDL_SysWMinfo info; SDL_VERSION(&info.version);
    return window && SDL_GetWindowWMInfo(window,&info)?info.info.win.window:NULL;
}
static LRESULT CALLBACK move_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR id,DWORD_PTR data) {
    MoveCallback *hook=(MoveCallback *)data;
    if(message==WM_ENTERSIZEMOVE)SetTimer(window,id,16,NULL);
    else if(message==WM_EXITSIZEMOVE)KillTimer(window,id);
    else if(message==WM_TIMER && wparam==id) {
        if(!hook->busy) { hook->busy=1; hook->callback(hook->data); hook->busy=0; }
        return 0;
    } else if(message==WM_NCDESTROY) {
        KillTimer(window,id); RemoveWindowSubclass(window,move_proc,id); SDL_free(hook);
    }
    return DefSubclassProc(window,message,wparam,lparam);
}
int zm_window_set_move_callback(SDL_Window *window,void (*callback)(void *),void *data) {
    HWND native=native_window(window);
    MoveCallback *hook=SDL_calloc(1,sizeof(*hook));
    if(!native || !hook) { SDL_free(hook); return 0; }
    hook->callback=callback; hook->data=data;
    if(!SetWindowSubclass(native,move_proc,0x5A4D0101,(DWORD_PTR)hook)) { SDL_free(hook); return 0; }
    return 1;
}
void zm_window_clear_move_callback(SDL_Window *window) {
    HWND native=native_window(window); DWORD_PTR data;
    if(native && GetWindowSubclass(native,move_proc,0x5A4D0101,&data)) {
        KillTimer(native,0x5A4D0101); RemoveWindowSubclass(native,move_proc,0x5A4D0101); SDL_free((void *)data);
    }
}
/* Smoke harness: pump native messages without running the normal SDL frame loop. */
int zm_window_test_move(SDL_Window *window,int milliseconds) {
    HWND native=native_window(window); MSG message;
    Uint64 start=SDL_GetTicks64();
    if(!native)return 0;
    SendMessageW(native,WM_ENTERSIZEMOVE,0,0);
    while(SDL_GetTicks64()-start<(Uint64)milliseconds) {
        while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        SDL_Delay(2);
    }
    SendMessageW(native,WM_EXITSIZEMOVE,0,0); return 1;
}
#else
int zm_window_set_move_callback(SDL_Window *window,void (*callback)(void *),void *data) { (void)window; (void)callback; (void)data; return 1; }
void zm_window_clear_move_callback(SDL_Window *window) { (void)window; }
int zm_window_test_move(SDL_Window *window,int milliseconds) { (void)window; (void)milliseconds; return 0; }
#endif
