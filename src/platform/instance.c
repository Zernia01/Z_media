#include "zm/app.h"
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <SDL_syswm.h>

#ifdef ZM_INSTANCE_TESTING
#define INSTANCE_NAME L"ZerniaMedia.OpenMedia.Test.016"
#else
#define INSTANCE_NAME L"ZerniaMedia.OpenMedia.016"
#endif
#define MEDIA_MESSAGE 0x5A4D0160u
#define MEDIA_LIMIT 32768u
typedef struct Receiver { Uint32 event_type; const int *enabled; } Receiver;
typedef struct Request { const char *path; int delivered; } Request;
static HANDLE startup_gate;

void zm_instance_ready(void) {
    if(startup_gate) { ReleaseMutex(startup_gate); CloseHandle(startup_gate); startup_gate=NULL; }
}
static BOOL CALLBACK forward_to_window(HWND window,LPARAM data) {
    Request *request=(Request *)data;
    COPYDATASTRUCT copy;
    DWORD_PTR accepted=0;
    DWORD process;
    if(!GetPropW(window,INSTANCE_NAME))return TRUE;
    GetWindowThreadProcessId(window,&process); AllowSetForegroundWindow(process);
    copy.dwData=MEDIA_MESSAGE; copy.cbData=(DWORD)strlen(request->path)+1; copy.lpData=(void *)request->path;
    if(SendMessageTimeoutW(window,WM_COPYDATA,0,(LPARAM)&copy,SMTO_ABORTIFHUNG|SMTO_BLOCK,1000,&accepted) && accepted==1) {
        request->delivered=1; return FALSE;
    }
    return TRUE;
}
int zm_instance_forward(const char *path,int enabled) {
    wchar_t input[32768],absolute[32768];
    char utf8[MEDIA_LIMIT];
    DWORD length,wait;
    Request request;
    if(!enabled || !path || !*path)return 0;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,input,32768))return 0;
    length=GetFullPathNameW(input,32768,absolute,NULL);
    if(!length || length>=32768 || !WideCharToMultiByte(CP_UTF8,0,absolute,-1,utf8,sizeof(utf8),NULL,NULL))return 0;
    startup_gate=CreateMutexW(NULL,FALSE,INSTANCE_NAME L".Startup");
    if(startup_gate) {
        wait=WaitForSingleObject(startup_gate,3000);
        if(wait!=WAIT_OBJECT_0 && wait!=WAIT_ABANDONED) { CloseHandle(startup_gate); startup_gate=NULL; }
    }
    request.path=utf8; request.delivered=0;
    EnumWindows(forward_to_window,(LPARAM)&request);
    if(request.delivered)zm_instance_ready();
    /* With no ready receiver, hold the gate until our own window is ready.
       This also serializes two launches made during application startup. */
    return request.delivered;
}
static LRESULT CALLBACK receiver_proc(HWND window,UINT message,WPARAM wparam,LPARAM lparam,UINT_PTR id,DWORD_PTR data) {
    Receiver *receiver=(Receiver *)data;
    if(message==WM_COPYDATA) {
        COPYDATASTRUCT *copy=(COPYDATASTRUCT *)lparam;
        SDL_Event event;
        char *path;
        if(!*receiver->enabled || !copy || copy->dwData!=MEDIA_MESSAGE || !copy->lpData || copy->cbData<2 || copy->cbData>MEDIA_LIMIT)return 0;
        path=(char *)copy->lpData;
        if(path[copy->cbData-1] || memchr(path,0,copy->cbData-1) || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0))return 0;
        SDL_zero(event); event.type=receiver->event_type; event.user.data1=SDL_strdup(path);
        if(!event.user.data1)return 0;
        if(SDL_PushEvent(&event)<=0) { SDL_free(event.user.data1); return 0; }
        if(IsIconic(window))ShowWindow(window,SW_RESTORE);
        SetForegroundWindow(window);
        return 1;
    }
    if(message==WM_NCDESTROY) {
        RemovePropW(window,INSTANCE_NAME); RemoveWindowSubclass(window,receiver_proc,id); SDL_free(receiver);
    }
    return DefSubclassProc(window,message,wparam,lparam);
}
static HWND native_window(SDL_Window *window) {
    SDL_SysWMinfo info; SDL_VERSION(&info.version);
    return window && SDL_GetWindowWMInfo(window,&info)?info.info.win.window:NULL;
}
int zm_window_accept_media(SDL_Window *window,Uint32 event_type,const int *enabled) {
    HWND native=native_window(window);
    Receiver *receiver=SDL_malloc(sizeof(*receiver));
    if(!native || !receiver) { SDL_free(receiver); return 0; }
    receiver->event_type=event_type; receiver->enabled=enabled;
    if(!SetWindowSubclass(native,receiver_proc,MEDIA_MESSAGE,(DWORD_PTR)receiver)) { SDL_free(receiver); return 0; }
    if(!SetPropW(native,INSTANCE_NAME,(HANDLE)1)) {
        RemoveWindowSubclass(native,receiver_proc,MEDIA_MESSAGE); SDL_free(receiver); return 0;
    }
    return 1;
}
void zm_window_stop_media(SDL_Window *window) {
    HWND native=native_window(window); DWORD_PTR data;
    if(native && GetWindowSubclass(native,receiver_proc,MEDIA_MESSAGE,&data)) {
        RemovePropW(native,INSTANCE_NAME); RemoveWindowSubclass(native,receiver_proc,MEDIA_MESSAGE); SDL_free((void *)data);
    }
}
#else
int zm_instance_forward(const char *path,int enabled) { (void)path; (void)enabled; return 0; }
void zm_instance_ready(void) {}
int zm_window_accept_media(SDL_Window *window,Uint32 event_type,const int *enabled) { (void)window; (void)event_type; (void)enabled; return 1; }
void zm_window_stop_media(SDL_Window *window) { (void)window; }
#endif
