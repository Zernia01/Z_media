#include "zm/app.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
FILE *zm_fopen_utf8(const char *path, const char *mode) {
#ifdef _WIN32
    wchar_t wide[32768], wide_mode[16];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, 32768) ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, mode, -1, wide_mode, 16)) return NULL;
    return _wfopen(wide, wide_mode);
#else
    return fopen(path, mode);
#endif
}
