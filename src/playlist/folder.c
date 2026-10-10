#include "zm/playlist.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
static int video_file(const wchar_t *name) {
    const wchar_t *ext=wcsrchr(name,L'.');
    const wchar_t *types[]={L".mp4",L".mkv",L".webm",L".avi",L".mov",L".ts",L".m2ts",L".m4v",L".wmv",L".flv",L".mpg",L".mpeg",L".mts",L".3gp",L".ogv",L".vob"};
    size_t i;
    if(!ext)return 0;
    for(i=0;i<sizeof(types)/sizeof(types[0]);++i)if(!_wcsicmp(ext,types[i]))return 1;
    return 0;
}
static int compare_names(const void *a,const void *b) {
    return SDL_strcasecmp(zm_basename(*(const char * const *)a),zm_basename(*(const char * const *)b));
}
int zm_playlist_load_folder(ZMPlaylist *list,const char *path) {
    wchar_t input[32768],full[32768],pattern[32768],candidate[32768],*name;
    WIN32_FIND_DATAW data;
    HANDLE search;
    ZMPlaylist *next;
    int selected=-1,i,repeat=list->repeat,shuffle=list->shuffle;
    DWORD length;
    if(!path || !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,input,32768))return -1;
    length=GetFullPathNameW(input,32768,full,&name);
    if(!length || length>=32768 || !name || !video_file(name))return -1;
    if((size_t)(name-full)+2>=32768)return -1;
    wcsncpy(pattern,full,(size_t)(name-full)); pattern[name-full]=L'*'; pattern[name-full+1]=0;
    search=FindFirstFileW(pattern,&data);
    if(search==INVALID_HANDLE_VALUE)return -1;
    next=SDL_malloc(sizeof(*next));
    if(!next) { FindClose(search); return -1; }
    zm_playlist_init(next);
    do {
        int bytes; char *utf8;
        if((data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || !video_file(data.cFileName))continue;
        if((size_t)(name-full)+wcslen(data.cFileName)+1>=32768)continue;
        wcsncpy(candidate,full,(size_t)(name-full)); candidate[name-full]=0;
        wcscat(candidate,data.cFileName);
        bytes=WideCharToMultiByte(CP_UTF8,0,candidate,-1,NULL,0,NULL,NULL);
        utf8=SDL_malloc((size_t)bytes);
        if(!utf8)break;
        WideCharToMultiByte(CP_UTF8,0,candidate,-1,utf8,bytes,NULL,NULL);
        zm_playlist_add(next,utf8); SDL_free(utf8);
    } while(next->count<ZM_PLAYLIST_LIMIT && FindNextFileW(search,&data));
    FindClose(search);
    qsort(next->items,(size_t)next->count,sizeof(next->items[0]),compare_names);
    for(i=0;i<next->count;++i) {
        if(MultiByteToWideChar(CP_UTF8,0,next->items[i],-1,candidate,32768) && !_wcsicmp(full,candidate)) { selected=i; break; }
    }
    if(selected<0) {
        int bytes=WideCharToMultiByte(CP_UTF8,0,full,-1,NULL,0,NULL,NULL);
        char *utf8=SDL_malloc((size_t)bytes);
        if(utf8) {
            WideCharToMultiByte(CP_UTF8,0,full,-1,utf8,bytes,NULL,NULL);
            if(next->count==ZM_PLAYLIST_LIMIT)zm_playlist_remove(next,next->count-1);
            selected=zm_playlist_add(next,utf8); SDL_free(utf8);
        }
    }
    if(selected>=0) {
        zm_playlist_destroy(list); *list=*next;
        list->current=selected; list->repeat=repeat; list->shuffle=shuffle;
    } else zm_playlist_destroy(next);
    SDL_free(next); return selected;
}
#else
int zm_playlist_load_folder(ZMPlaylist *list,const char *path) { (void)list; (void)path; return -1; }
#endif
