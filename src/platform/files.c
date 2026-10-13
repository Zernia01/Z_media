#include "zm/app.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <SDL_syswm.h>
#endif
void zm_window_set_icon(SDL_Window *window) {
#ifdef _WIN32
    SDL_SysWMinfo info;
    HINSTANCE instance=GetModuleHandleW(NULL);
    HICON large=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED);
    HICON small=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED);
    SDL_VERSION(&info.version);
    if(SDL_GetWindowWMInfo(window,&info)) {
        if(large)SendMessageW(info.info.win.window,WM_SETICON,ICON_BIG,(LPARAM)large);
        if(small)SendMessageW(info.info.win.window,WM_SETICON,ICON_SMALL,(LPARAM)small);
    }
#else
    (void)window;
#endif
}
char **zm_open_files(SDL_Window *window, int *count) {
    *count = 0;
#ifdef _WIN32
    OPENFILENAMEW dialog = {0};
    SDL_SysWMinfo info;
    wchar_t *buffer = SDL_calloc(131072, sizeof(wchar_t));
    wchar_t full[32768], *name;
    char **paths = NULL;
    int n = 0;
    if (!buffer) return NULL;
    SDL_VERSION(&info.version);
    dialog.lStructSize = sizeof(dialog);
    if (SDL_GetWindowWMInfo(window, &info)) dialog.hwndOwner = info.info.win.window;
    dialog.lpstrFilter = L"All media\0*.*\0";
    dialog.lpstrFile = buffer; dialog.nMaxFile = 131072;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
    if (!GetOpenFileNameW(&dialog)) { SDL_free(buffer); return NULL; }
    paths = SDL_calloc(4096, sizeof(char *));
    if (!paths) { SDL_free(buffer); return NULL; }
    name = buffer + wcslen(buffer) + 1;
    do {
        int bytes;
        const wchar_t *path;
        if (!*name) path = buffer;
        else {
            if (wcslen(buffer) + wcslen(name) + 2 > 32768) break;
            swprintf(full, 32768, L"%ls\\%ls", buffer, name); path = full;
        }
        bytes = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
        paths[n] = SDL_malloc((size_t)bytes);
        if (!paths[n]) break;
        WideCharToMultiByte(CP_UTF8, 0, path, -1, paths[n], bytes, NULL, NULL); ++n;
        if (!*name) break;
        name += wcslen(name) + 1;
    } while (*name && n < 4096);
    SDL_free(buffer); *count = n; return paths;
#else
    (void)window; return NULL;
#endif
}
char *zm_open_dialog(SDL_Window *window) {
#ifdef _WIN32
    OPENFILENAMEW dialog = {0};
    SDL_SysWMinfo info;
    wchar_t path[32768] = {0};
    char *utf8;
    int size;
    SDL_VERSION(&info.version);
    dialog.lStructSize = sizeof(dialog);
    if (SDL_GetWindowWMInfo(window, &info)) dialog.hwndOwner = info.info.win.window;
    dialog.lpstrFilter = L"Media files\0*.mp4;*.mkv;*.webm;*.avi;*.mov;*.ts;*.m2ts;*.m4v;*.wmv;*.flv;*.mp3;*.flac;*.wav;*.ogg;*.m4a\0All files\0*.*\0";
    dialog.nFilterIndex = 2;
    dialog.lpstrFile = path; dialog.nMaxFile = 32768;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameW(&dialog)) return NULL;
    size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
    utf8 = SDL_malloc((size_t)size);
    if (utf8) WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, size, NULL, NULL);
    return utf8;
#else
    (void)window; return NULL;
#endif
}
char *zm_font_path(const char *language, int rounded, int bold) {
    if(rounded && (!strcmp(language,"ko") || !strcmp(language,"en"))) {
        char *base=SDL_GetBasePath();
        if(base) {
            char bundled[32768]; FILE *file;
            SDL_snprintf(bundled,sizeof(bundled),"%sfonts/NanumSquareRound%s.ttf",base,bold?"B":"R");
            SDL_free(base); file=zm_fopen_utf8(bundled,"rb");
            if(file) { fclose(file); return SDL_strdup(bundled); }
        }
    }
#ifdef _WIN32
    char windows[32768];
    char path[32768];
    GetWindowsDirectoryA(windows, sizeof(windows));
    if (!strcmp(language, "ja") || !strcmp(language, "zh-CN") || !strcmp(language, "zh-TW")) {
        SDL_snprintf(path, sizeof(path), "%s/Fonts/%s", windows, !strcmp(language,"ja") ? (bold?"YuGothB.ttc":"YuGothM.ttc") : !strcmp(language,"zh-CN") ? (bold?"msyhbd.ttc":"msyh.ttc") : (bold?"msjhbd.ttc":"msjh.ttc"));
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return SDL_strdup(path);
    }
    SDL_snprintf(path, sizeof(path), "%s/Fonts/%s", windows,bold?"malgunbd.ttf":"malgun.ttf");
    return SDL_strdup(path);
#else
    (void)language;
    return SDL_strdup("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
#endif
}
