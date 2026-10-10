#include "zm/app.h"
#include <stdio.h>
#include <string.h>
typedef struct Entry { char key[48], value[512]; } Entry;
static Entry entries[128];
static int count;
void zm_i18n_load(const char *language) {
    char *base = SDL_GetBasePath();
    char path[32768], line[600];
    FILE *f;
    count = 0;
    if (!base) return;
    SDL_snprintf(path, sizeof(path), "%si18n/%s.ini", base, language);
    SDL_free(base);
    f = zm_fopen_utf8(path, "rb");
    if (!f) return;
    while (count < 128 && fgets(line, sizeof(line), f)) {
        char *equal = strchr(line, '=');
        if (!equal) continue;
        *equal++ = '\0'; equal[strcspn(equal, "\r\n")] = '\0';
        SDL_strlcpy(entries[count].key, line, sizeof(entries[count].key));
        SDL_strlcpy(entries[count].value, equal, sizeof(entries[count].value)); ++count;
    }
    fclose(f);
}
const char *zm_text(const char *key) {
    int i;
    for (i = 0; i < count; ++i) if (!strcmp(entries[i].key, key)) return entries[i].value;
    return key;
}
