#include "zm/update.h"
#include "zm/app.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d\n",__LINE__); ++failures; } } while(0)
int main(int argc,char **argv) {
    const char *digest="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    char json[1024],path[256]; ZMUpdateManifest manifest; int valid,failures=0;
    SDL_SetMainReady(); SDL_Init(SDL_INIT_TIMER);
    SDL_snprintf(json,sizeof(json),"{\"version\":\"0.1.0\",\"url\":\"https://github.com/Zernia01/Z_media/releases/download/v0.1.0/ZM-0.1.0-setup-win64.exe\",\"sha256\":\"%s\"}",digest);
    CHECK(zm_update_parse(json,&manifest)); CHECK(!strcmp(manifest.version,"0.1.0"));
    CHECK(zm_update_version_compare("0.10.0","0.9.9",&valid)>0 && valid);
    CHECK(zm_update_version_compare("1.0.0","0.999.9",&valid)>0 && valid);
    CHECK(zm_update_version_compare("0.1.0","0.1.0",&valid)==0 && valid);
    zm_update_version_compare("v0.1.0","0.1.0",&valid); CHECK(!valid);
    zm_update_version_compare("01.1.0","0.1.0",&valid); CHECK(!valid);
    zm_update_version_compare("0.1.0-beta","0.1.0",&valid); CHECK(!valid);
    json[0]='['; CHECK(!zm_update_parse(json,&manifest)); json[0]='{';
    { char *url=strstr(json,"https://github.com"); *url='x'; CHECK(!zm_update_parse(json,&manifest)); *url='h'; }
    CHECK(!zm_update_parse("{\"version\":\"0.1.0\",\"version\":\"0.2.0\",\"sha256\":\"bad\"}",&manifest));
    CHECK(!zm_update_parse("{}",&manifest)); CHECK(!zm_update_parse("",&manifest));
    SDL_snprintf(path,sizeof(path),"update-hash-%llu.txt",(unsigned long long)SDL_GetPerformanceCounter());
    FILE *file=zm_fopen_utf8(path,"wb"); CHECK(file!=NULL);
    if(file) { fwrite("abc",1,3,file); fclose(file); CHECK(zm_update_verify_file(path,digest)); CHECK(!zm_update_verify_file(path,"0000000000000000000000000000000000000000000000000000000000000000")); remove(path); }
    if(argc>1 && (!strcmp(argv[1],"--live") || !strcmp(argv[1],"--download"))) {
        ZMUpdater updater; ZMUpdateState state; Uint64 start=SDL_GetTicks64();
        int download=!strcmp(argv[1],"--download");
        if(download) { CHECK(argc>2); SDL_setenv("ZM_TEST_CURRENT_VERSION","0.0.0",1); SDL_setenv("ZM_TEST_UPDATE_DIRECTORY",argc>2?argv[2]:"test-media/update-cache/",1); }
        CHECK(zm_update_init(&updater)); zm_update_check(&updater,download);
        do { SDL_Delay(10); zm_update_snapshot(&updater,&state); } while((state.status==ZM_UPDATE_CHECKING || state.status==ZM_UPDATE_DOWNLOADING) && SDL_GetTicks64()-start<120000);
        printf("Live GitHub update: status=%d version=%s\n",state.status,state.version);
        CHECK(state.status==(download?ZM_UPDATE_READY:ZM_UPDATE_CURRENT) && !strcmp(state.version,ZM_VERSION));
        if(download)CHECK(zm_update_verify_file(state.path,state.sha256));
        zm_update_destroy(&updater);
    }
    SDL_Quit(); printf("Updater tests: %d failure(s)\n",failures); return failures?1:0;
}
