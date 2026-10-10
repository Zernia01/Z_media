#include "zm/player.h"
#include "zm/app.h"
#include "zm/shortcuts.h"
#include "zm/playlist.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); failures++; } } while (0)
static void consume(ZMPlayer *p, int milliseconds) {
    Uint64 start = SDL_GetTicks64();
    while (SDL_GetTicks64() - start < (Uint64)milliseconds) {
        ZMVideoFrame f;
        while (zm_player_take_frame(p, &f)) av_frame_free(&f.image);
        SDL_Delay(5);
    }
}
static void wait_playing(ZMPlayer *p) {
    Uint64 start=SDL_GetTicks64();
    ZMSnapshot s;
    do {
        consume(p,25); zm_player_snapshot(p,&s);
        if(s.loaded && !s.loading && !SDL_AtomicGet(&p->change) && s.video_frames && s.audio_frames) return;
    } while(SDL_GetTicks64()-start<10000u && !s.error[0]);
}
int main(int argc, char **argv) {
    ZMPlayer p;
    ZMSnapshot s;
    int failures = 0, i;
    ZMShortcuts shortcuts;
    ZMPlaylist list;
    SDL_SetMainReady(); SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0 || !zm_player_init(&p)) return 1;
    zm_shortcuts_defaults(&shortcuts);
    CHECK(zm_shortcuts_match(&shortcuts,SDLK_o,KMOD_RCTRL)==ZM_OPEN);
    CHECK(zm_shortcuts_assign(&shortcuts,ZM_MUTE,(ZMBinding){SDLK_SPACE,0},0)==ZM_PLAY);
    CHECK(zm_shortcuts_match(&shortcuts,SDLK_SPACE,KMOD_NONE)==ZM_PLAY);
    CHECK(zm_shortcuts_assign(&shortcuts,ZM_MUTE,(ZMBinding){SDLK_SPACE,0},1)==-1);
    CHECK(zm_shortcuts_match(&shortcuts,SDLK_SPACE,KMOD_NONE)==ZM_MUTE);
    zm_shortcuts_restore(&shortcuts,ZM_PLAY);
    CHECK(zm_shortcuts_match(&shortcuts,SDLK_SPACE,KMOD_NONE)==ZM_PLAY);
    zm_playlist_init(&list);
    CHECK(zm_playlist_add(&list,"one.mp4")==0); CHECK(zm_playlist_add(&list,"two.mkv")==1);
    list.current=0; zm_playlist_move(&list,0,1); CHECK(list.current==1);
    CHECK(!strcmp(list.items[1],"one.mp4")); CHECK(zm_playlist_step(&list,1,1)==NULL);
    list.repeat=2; CHECK(!strcmp(zm_playlist_step(&list,1,1),"two.mkv"));
    zm_playlist_remove(&list,0); CHECK(list.count==1 && list.current==-1);
    zm_playlist_destroy(&list);
#ifdef _WIN32
    {
        wchar_t temp[32768],folder[32768],file[32768];
        const wchar_t *names[]={L"한글_日本語_中文.MP4",L"another.mkv",L"music.mp3",L"notes.txt"};
        char selected_path[32768];
        DWORD length=GetTempPathW(32768,temp);
        CHECK(length>0 && length<32700);
        swprintf(folder,32768,L"%lsZM-folder-test-%lu-%llu",temp,GetCurrentProcessId(),(unsigned long long)SDL_GetPerformanceCounter());
        CHECK(CreateDirectoryW(folder,NULL));
        for(i=0;i<4;++i) {
            HANDLE handle;
            swprintf(file,32768,L"%ls\\%ls",folder,names[i]);
            handle=CreateFileW(file,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
            CHECK(handle!=INVALID_HANDLE_VALUE);
            if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);
            if(i==0)CHECK(WideCharToMultiByte(CP_UTF8,0,file,-1,selected_path,sizeof(selected_path),NULL,NULL)>0);
        }
        swprintf(file,32768,L"%ls\\subfolder.mp4",folder); CHECK(CreateDirectoryW(file,NULL));
        zm_playlist_init(&list); list.repeat=2; list.shuffle=1;
        CHECK(zm_playlist_load_folder(&list,selected_path)>=0);
        CHECK(list.count==2 && list.current>=0 && list.repeat==2 && list.shuffle==1);
        if(list.current>=0)CHECK(!strcmp(list.items[list.current],selected_path));
        CHECK(zm_playlist_load_folder(&list,"missing-folder\\missing.mp4")==-1 && list.count==2);
        swprintf(file,32768,L"%ls\\music.mp3",folder);
        CHECK(WideCharToMultiByte(CP_UTF8,0,file,-1,selected_path,sizeof(selected_path),NULL,NULL)>0);
        CHECK(zm_playlist_load_folder(&list,selected_path)==-1 && list.count==2);
        zm_playlist_destroy(&list);
        for(i=0;i<4;++i) { swprintf(file,32768,L"%ls\\%ls",folder,names[i]); CHECK(DeleteFileW(file)); }
        swprintf(file,32768,L"%ls\\subfolder.mp4",folder); CHECK(RemoveDirectoryW(file)); CHECK(RemoveDirectoryW(folder));
    }
#endif
    zm_player_volume(&p, -10); zm_player_snapshot(&p, &s); CHECK(s.volume == 0);
    zm_player_volume(&p, 200); zm_player_snapshot(&p, &s); CHECK(s.volume == 100);
    zm_player_open(&p, "nonexistent-media-file.mkv"); consume(&p, 150);
    zm_player_snapshot(&p, &s); CHECK(!s.loaded && s.error[0]);
    if (argc > 1) {
        zm_player_open(&p, argv[1]); wait_playing(&p); consume(&p,200); zm_player_snapshot(&p, &s);
        CHECK(s.loaded && !s.error[0]); CHECK(s.video_frames > 0); CHECK(s.audio_frames > 0);
        printf("Playback: codec=%s hardware=%d audio_tracks=%d subtitles=%d\n",s.codec,s.hardware_active,s.audio_count,s.subtitle_count);
        if(s.audio_count>1) {
            int before=s.audio_index;
            zm_player_next_audio(&p); consume(&p,400); zm_player_snapshot(&p,&s);
            CHECK(s.audio_index!=(before) && !s.error[0]);
        }
        zm_player_toggle_pause(&p); zm_player_snapshot(&p, &s);
        { double paused = s.position; consume(&p, 150); zm_player_snapshot(&p, &s); CHECK(fabs(paused - s.position) < 0.02); }
        zm_player_seek(&p, 2.0); consume(&p, 400); zm_player_snapshot(&p, &s); CHECK(fabs(s.position - 2.0) < 0.05);
        zm_player_toggle_pause(&p); consume(&p, 300); zm_player_snapshot(&p, &s); CHECK(s.position > 2.1);
        zm_player_subtitle_toggle(&p); consume(&p,150); zm_player_snapshot(&p,&s); CHECK(!s.subtitles_enabled);
        zm_player_subtitle_toggle(&p); consume(&p,150); zm_player_snapshot(&p,&s); CHECK(s.subtitles_enabled && !s.error[0]);
        for (i = 0; i < 20; ++i) { zm_player_seek(&p, (double)(i % 3)); consume(&p, 10); }
        consume(&p, 200); zm_player_snapshot(&p, &s); CHECK(!s.error[0]);
        for (i = 0; i < 15; ++i) { zm_player_open(&p, argv[1]); consume(&p, 30); }
        consume(&p, 300); zm_player_snapshot(&p, &s); CHECK(s.loaded && !s.error[0]);
        zm_player_seek(&p,s.duration-0.2); consume(&p,600); zm_player_snapshot(&p,&s); CHECK(s.ended);
        SDL_LockMutex(p.mutex); p.state.hardware_mode=2; SDL_UnlockMutex(p.mutex);
        zm_player_open(&p,argv[1]); wait_playing(&p); zm_player_snapshot(&p,&s);
        CHECK(s.loaded && !s.hardware_active && !s.error[0]);
    }
    zm_player_destroy(&p); SDL_Quit();
    printf("ZM tests: %d failure(s)\n", failures); return failures ? 1 : 0;
}
