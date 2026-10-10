#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "zm/app.h"
int main(void) {
    HKEY isolated;
    wchar_t root[128],text[32800]={0};
    DWORD bytes;
    int ok=1;
    swprintf(root,128,L"Software\\ZM-association-test-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());
    if(RegCreateKeyExW(HKEY_CURRENT_USER,root,0,NULL,0,KEY_ALL_ACCESS,NULL,&isolated,NULL)!=ERROR_SUCCESS)return 1;
    if(RegOverridePredefKey(HKEY_CURRENT_USER,isolated)!=ERROR_SUCCESS) { RegCloseKey(isolated); RegDeleteTreeW(HKEY_CURRENT_USER,root); return 1; }
    ok&=zm_register_associations();
    bytes=sizeof(text);
    ok&=RegGetValueW(HKEY_CURRENT_USER,L"Software\\RegisteredApplications",L"Zernia media",RRF_RT_REG_SZ,NULL,text,&bytes)==ERROR_SUCCESS;
    ok&=!wcscmp(text,L"Software\\ZerniaMedia\\Capabilities");
    bytes=sizeof(text); text[0]=0;
    ok&=RegGetValueW(HKEY_CURRENT_USER,L"Software\\ZerniaMedia\\Capabilities\\FileAssociations",L".mp4",RRF_RT_REG_SZ,NULL,text,&bytes)==ERROR_SUCCESS;
    ok&=!wcscmp(text,L"ZerniaMedia.Media");
    bytes=sizeof(text); text[0]=0;
    ok&=RegGetValueW(HKEY_CURRENT_USER,L"Software\\Classes\\ZerniaMedia.Media\\shell\\open\\command",NULL,RRF_RT_REG_SZ,NULL,text,&bytes)==ERROR_SUCCESS;
    ok&=text[0]==L'"' && wcsstr(text,L"\" \"%1\"")!=NULL;
    /* Registration must never silently choose the user's default app. */
    bytes=sizeof(text);
    ok&=RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.mp4\\UserChoice",L"ProgId",RRF_RT_REG_SZ,NULL,text,&bytes)==ERROR_FILE_NOT_FOUND;
    ok&=zm_unregister_associations();
    bytes=sizeof(text);
    ok&=RegGetValueW(HKEY_CURRENT_USER,L"Software\\RegisteredApplications",L"Zernia media",RRF_RT_REG_SZ,NULL,text,&bytes)==ERROR_FILE_NOT_FOUND;
    RegOverridePredefKey(HKEY_CURRENT_USER,NULL); RegCloseKey(isolated);
    ok&=RegDeleteTreeW(HKEY_CURRENT_USER,root)==ERROR_SUCCESS;
    printf("Isolated association registration/unregistration: %s\n",ok?"passed":"failed");
    return ok?0:1;
}
