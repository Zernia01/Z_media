#include "zm/app.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
static char *to_utf8(const wchar_t *wide) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    char *result = SDL_malloc((size_t)size);
    if (result) WideCharToMultiByte(CP_UTF8, 0, wide, -1, result, size, NULL, NULL);
    return result;
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command, int show) {
    int argc, i, smoke = 0, result;
    wchar_t **args = CommandLineToArgvW(GetCommandLineW(), &argc);
    char *path = NULL, *capture = NULL;
    (void)instance; (void)previous; (void)command; (void)show;
    if (!args) return 1;
    if(argc==2 && (!wcscmp(args[1],L"--register") || !wcscmp(args[1],L"--unregister"))) {
        result=!wcscmp(args[1],L"--register")?zm_register_associations():zm_unregister_associations();
        LocalFree(args); return result?0:1;
    }
    for (i = 1; i < argc; ++i) {
        if (!wcscmp(args[i], L"--smoke") && i + 1 < argc) smoke = _wtoi(args[++i]);
        else if (!wcscmp(args[i], L"--capture") && i + 1 < argc) { SDL_free(capture); capture = to_utf8(args[++i]); }
        else { SDL_free(path); path = to_utf8(args[i]); }
    }
    LocalFree(args);
    SDL_SetMainReady();
    result = zm_app_run(path, smoke, capture);
    SDL_free(path); SDL_free(capture); return result;
}
#else
int main(int argc, char **argv) {
    SDL_SetMainReady();
    return zm_app_run(argc > 1 ? argv[1] : NULL, 0, NULL);
}
#endif
