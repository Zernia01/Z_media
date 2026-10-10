#include "zm/app.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <SDL_syswm.h>
#include <wchar.h>
static const wchar_t *extensions[]={L".mp4",L".mkv",L".webm",L".avi",L".mov",L".ts",L".m2ts",L".m4v",L".wmv",L".flv",L".mpg",L".mpeg",L".mts",L".3gp",L".ogv",L".vob",L".mp3",L".flac",L".wav",L".ogg",L".m4a",L".aac",L".opus"};
static int value(const wchar_t *path,const wchar_t *name,const wchar_t *data) {
    HKEY key; LSTATUS result;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL)!=ERROR_SUCCESS)return 0;
    result=RegSetValueExW(key,name,0,REG_SZ,(const BYTE *)data,(DWORD)((wcslen(data)+1)*sizeof(wchar_t)));
    RegCloseKey(key); return result==ERROR_SUCCESS;
}
static int executable(wchar_t *path,DWORD capacity) {
    DWORD n=GetModuleFileNameW(NULL,path,capacity);
    return n>0 && n<capacity;
}
int zm_register_associations(void) {
    wchar_t exe[32768],command[32800],icon[32800],key[256];
    size_t i; int ok=1;
    if(!executable(exe,32768))return 0;
    swprintf(command,32800,L"\"%ls\" \"%%1\"",exe);
    swprintf(icon,32800,L"\"%ls\",0",exe);
    ok&=value(L"Software\\Classes\\ZerniaMedia.Media",NULL,L"Zernia media video and audio");
    ok&=value(L"Software\\Classes\\ZerniaMedia.Media\\DefaultIcon",NULL,icon);
    ok&=value(L"Software\\Classes\\ZerniaMedia.Media\\shell\\open\\command",NULL,command);
    ok&=value(L"Software\\ZerniaMedia\\Capabilities",L"ApplicationName",L"Zernia media");
    ok&=value(L"Software\\ZerniaMedia\\Capabilities",L"ApplicationDescription",L"ZM video and audio player");
    ok&=value(L"Software\\ZerniaMedia\\Capabilities",L"ApplicationIcon",icon);
    for(i=0;i<sizeof(extensions)/sizeof(extensions[0]);++i) {
        HKEY handle;
        ok&=value(L"Software\\ZerniaMedia\\Capabilities\\FileAssociations",extensions[i],L"ZerniaMedia.Media");
        swprintf(key,256,L"Software\\Classes\\%ls\\OpenWithProgids",extensions[i]);
        if(RegCreateKeyExW(HKEY_CURRENT_USER,key,0,NULL,0,KEY_SET_VALUE,NULL,&handle,NULL)==ERROR_SUCCESS) {
            ok&=RegSetValueExW(handle,L"ZerniaMedia.Media",0,REG_NONE,NULL,0)==ERROR_SUCCESS;
            RegCloseKey(handle);
        } else ok=0;
    }
    ok&=value(L"Software\\RegisteredApplications",L"Zernia media",L"Software\\ZerniaMedia\\Capabilities");
    SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL);
    return ok;
}
int zm_unregister_associations(void) {
    wchar_t exe[32768],expected[32800],current[32800],key[256];
    DWORD bytes=sizeof(current); size_t i; HKEY handle;
    if(!executable(exe,32768))return 0;
    swprintf(expected,32800,L"\"%ls\" \"%%1\"",exe);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Classes\\ZerniaMedia.Media\\shell\\open\\command",NULL,RRF_RT_REG_SZ,NULL,current,&bytes)!=ERROR_SUCCESS)return 1;
    /* A different installed/portable copy may now own the registration. */
    if(_wcsicmp(current,expected))return 1;
    for(i=0;i<sizeof(extensions)/sizeof(extensions[0]);++i) {
        swprintf(key,256,L"Software\\Classes\\%ls\\OpenWithProgids",extensions[i]);
        if(RegOpenKeyExW(HKEY_CURRENT_USER,key,0,KEY_SET_VALUE,&handle)==ERROR_SUCCESS) {
            RegDeleteValueW(handle,L"ZerniaMedia.Media"); RegCloseKey(handle);
        }
    }
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\RegisteredApplications",0,KEY_SET_VALUE,&handle)==ERROR_SUCCESS) {
        RegDeleteValueW(handle,L"Zernia media"); RegCloseKey(handle);
    }
    RegDeleteTreeW(HKEY_CURRENT_USER,L"Software\\ZerniaMedia\\Capabilities");
    RegDeleteTreeW(HKEY_CURRENT_USER,L"Software\\Classes\\ZerniaMedia.Media");
    SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,NULL,NULL); return 1;
}
int zm_open_default_apps(SDL_Window *window) {
    SDL_SysWMinfo info; HWND owner=NULL;
    SDL_VERSION(&info.version);
    if(SDL_GetWindowWMInfo(window,&info))owner=info.info.win.window;
    if(!zm_register_associations())return 0;
    return (INT_PTR)ShellExecuteW(owner,L"open",L"ms-settings:defaultapps?registeredAppUser=Zernia%20media",NULL,NULL,SW_SHOWNORMAL)>32;
}
#else
int zm_register_associations(void) { return 0; }
int zm_unregister_associations(void) { return 0; }
int zm_open_default_apps(SDL_Window *window) { (void)window; return 0; }
#endif
