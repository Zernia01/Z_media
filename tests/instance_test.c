#include "zm/app.h"
#include <string.h>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL_syswm.h>

static int run_sender(Uint32 event_type,int enabled,int expected,const char *path) {
    wchar_t exe[32768],command[33000];
    STARTUPINFOW startup={0}; PROCESS_INFORMATION process={0};
    Uint64 start=SDL_GetTicks64();
    DWORD exit_code=STILL_ACTIVE;
    int received=0,failed=0;
    GetModuleFileNameW(NULL,exe,32768);
    swprintf(command,33000,L"\"%ls\" --send %d",exe,enabled);
    startup.cb=sizeof(startup);
    if(!CreateProcessW(NULL,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process))return 0;
    while(SDL_GetTicks64()-start<6000) {
        SDL_Event event;
        while(SDL_PollEvent(&event))if(event.type==event_type) {
            ++received;
            if(strcmp(event.user.data1,path))failed=1;
            SDL_free(event.user.data1);
        }
        GetExitCodeProcess(process.hProcess,&exit_code);
        if(exit_code!=STILL_ACTIVE)break;
        SDL_Delay(1);
    }
    if(exit_code==STILL_ACTIVE) { TerminateProcess(process.hProcess,9); WaitForSingleObject(process.hProcess,1000); }
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return !failed && exit_code==(DWORD)(expected?0:2) && received==expected;
}
int main(int argc,char **argv) {
    const char *relative="test-media/한글_日本語_中文_🎬.mp4";
    char absolute[32768];
    wchar_t full[32768];
    int enabled=1,failed=0;
    SDL_Window *window;
    Uint32 event_type;
    SDL_SetMainReady();
    if(argc==3 && !strcmp(argv[1],"--send")) {
        int delivered=zm_instance_forward(relative,atoi(argv[2]));
        zm_instance_ready(); return delivered?0:2;
    }
    GetFullPathNameW(L"test-media/한글_日本語_中文_🎬.mp4",32768,full,NULL);
    WideCharToMultiByte(CP_UTF8,0,full,-1,absolute,sizeof(absolute),NULL,NULL);
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0)return 1;
    window=SDL_CreateWindow("ZM isolated IPC test",0,0,320,240,SDL_WINDOW_HIDDEN);
    event_type=SDL_RegisterEvents(1);
    if(!window || event_type==(Uint32)-1 || !zm_window_accept_media(window,event_type,&enabled))return 1;
    /* A separate process sends an absolute UTF-8 path, without creating a player. */
    if(!run_sender(event_type,1,1,absolute))failed=1;
    enabled=0;
    if(!run_sender(event_type,1,0,absolute))failed=1;
    enabled=1;
    if(!run_sender(event_type,0,0,absolute))failed=1;
    if(!run_sender(event_type,1,1,absolute))failed=1;
    {
        SDL_SysWMinfo info; COPYDATASTRUCT copy; SDL_Event event;
        unsigned char invalid[]={ 0xff,0 };
        SDL_VERSION(&info.version); SDL_GetWindowWMInfo(window,&info);
        copy.dwData=0x5A4D0160u; copy.cbData=2; copy.lpData=invalid;
        if(SendMessageW(info.info.win.window,WM_COPYDATA,0,(LPARAM)&copy))failed=1;
        copy.cbData=32769;
        if(SendMessageW(info.info.win.window,WM_COPYDATA,0,(LPARAM)&copy))failed=1;
        while(SDL_PollEvent(&event))if(event.type==event_type) { SDL_free(event.user.data1); failed=1; }
    }
    zm_window_stop_media(window);
    if(!run_sender(event_type,1,0,absolute))failed=1;
    SDL_DestroyWindow(window); SDL_Quit();
    printf("Media forwarding: %s\n",failed?"FAIL":"PASS");
    return failed;
}
