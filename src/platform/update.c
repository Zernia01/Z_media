#include "zm/update.h"
#include "zm/app.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int version(const char *text,unsigned parts[3]) {
    int i;
    for(i=0;i<3;++i) {
        unsigned value=0;
        const char *start=text;
        if(*text<'0' || *text>'9')return 0;
        while(*text>='0' && *text<='9') { value=value*10u+(unsigned)(*text++-'0'); if(value>99999)return 0; }
        if(text-start>1 && *start=='0')return 0;
        parts[i]=value;
        if(i<2 && *text++!='.')return 0;
    }
    return *text=='\0';
}
int zm_update_version_compare(const char *a,const char *b,int *valid) {
    unsigned left[3],right[3]; int i;
    *valid=version(a,left) && version(b,right);
    if(!*valid)return 0;
    for(i=0;i<3;++i)if(left[i]!=right[i])return left[i]>right[i]?1:-1;
    return 0;
}
static void whitespace(const char **p) { while(**p==' ' || **p=='\r' || **p=='\n' || **p=='\t')++*p; }
static int string(const char **p,char *out,size_t capacity) {
    size_t used=0; whitespace(p);
    if(*(*p)++!='"')return 0;
    while(**p && **p!='"') {
        unsigned char c=(unsigned char)*(*p)++;
        if(c=='\\') { c=(unsigned char)*(*p)++; if(c!='"' && c!='/' && c!='\\')return 0; }
        if(c<32 || c>126 || used+1>=capacity)return 0;
        out[used++]=(char)c;
    }
    if(**p!='"')return 0;
    ++*p; out[used]='\0'; return 1;
}
int zm_update_parse(const char *json,ZMUpdateManifest *m) {
    unsigned seen=0,parts[3]; char expected[256]; const char *p=json; int i;
    memset(m,0,sizeof(*m)); whitespace(&p); if(*p++!='{')return 0;
    for(i=0;i<3;++i) {
        char key[32],value[256]; unsigned bit;
        if(!string(&p,key,sizeof(key)))return 0;
        whitespace(&p); if(*p++!=':' || !string(&p,value,sizeof(value)))return 0;
        if(!strcmp(key,"version")) { bit=1; if(strlen(value)>=sizeof(m->version))return 0; SDL_strlcpy(m->version,value,sizeof(m->version)); }
        else if(!strcmp(key,"url")) { bit=2; SDL_strlcpy(m->url,value,sizeof(m->url)); }
        else if(!strcmp(key,"sha256")) { bit=4; if(strlen(value)!=64)return 0; SDL_strlcpy(m->sha256,value,sizeof(m->sha256)); }
        else return 0;
        if(seen&bit)return 0; seen|=bit;
        whitespace(&p); if(*p++!=(i==2?'}':','))return 0;
    }
    whitespace(&p); if(*p || seen!=7 || !version(m->version,parts))return 0;
    for(i=0;i<64;++i)if(!isxdigit((unsigned char)m->sha256[i]))return 0;
    SDL_snprintf(expected,sizeof(expected),"https://github.com/Zernia01/Z_media/releases/download/v%s/ZM-%s-setup-win64.exe",m->version,m->version);
    return !strcmp(expected,m->url);
}
static void status(ZMUpdater *u,ZMUpdateStatus value) {
    SDL_LockMutex(u->mutex); u->state.status=value; SDL_UnlockMutex(u->mutex);
}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
static wchar_t *wide(const char *text) {
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0);
    wchar_t *result=count?SDL_malloc((size_t)count*sizeof(wchar_t)):NULL;
    if(result)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,result,count);
    return result;
}
int zm_update_verify_file(const char *path,const char *expected) {
    BCRYPT_ALG_HANDLE algorithm=NULL; BCRYPT_HASH_HANDLE hash=NULL;
    unsigned char buffer[65536],digest[32]; char actual[65];
    FILE *file=zm_fopen_utf8(path,"rb"); int ok=0,i; size_t count;
    if(!file)return 0;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,0)<0 ||
       BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,0)<0)goto done;
    while((count=fread(buffer,1,sizeof(buffer),file))>0)if(BCryptHashData(hash,buffer,(ULONG)count,0)<0)goto done;
    if(ferror(file) || BCryptFinishHash(hash,digest,sizeof(digest),0)<0)goto done;
    for(i=0;i<32;++i)SDL_snprintf(actual+i*2,3,"%02x",digest[i]);
    ok=!SDL_strcasecmp(actual,expected);
done:
    if(hash)BCryptDestroyHash(hash); if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0); fclose(file); return ok;
}
/* HTTPS only, bounded response sizes/timeouts, no disabled TLS validation. */
static int get(ZMUpdater *u,const char *url,FILE *file,char *text,size_t capacity) {
    HINTERNET session=NULL,connection=NULL,request=NULL;
    wchar_t *address=wide(url),host[256],route[1024]; URL_COMPONENTS parts={0};
    DWORD code=0,length=sizeof(code),read=0,policy=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    unsigned char buffer[65536]; size_t total=0; int ok=0;
    parts.dwStructSize=sizeof(parts); parts.lpszHostName=host; parts.dwHostNameLength=256;
    parts.lpszUrlPath=route; parts.dwUrlPathLength=1024;
    if(!address || !WinHttpCrackUrl(address,0,0,&parts) || parts.nScheme!=INTERNET_SCHEME_HTTPS)goto done;
    session=WinHttpOpen(L"Zernia media/" L"0.1.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session)goto done;
    WinHttpSetTimeouts(session,3000,3000,5000,5000);
    connection=WinHttpConnect(session,host,parts.nPort,0); if(!connection)goto done;
    request=WinHttpOpenRequest(connection,L"GET",route,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!request)goto done;
    WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
    if(!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) ||
       !WinHttpReceiveResponse(request,NULL) || !WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,NULL,&code,&length,NULL) || code!=200)goto done;
    for(;;) {
        if(SDL_AtomicGet(&u->quit) || !WinHttpReadData(request,buffer,sizeof(buffer),&read))goto done;
        if(!read)break;
        if(total+read>256u*1024u*1024u || (!file && total+read>=capacity))goto done;
        if(file) { if(fwrite(buffer,1,read,file)!=read)goto done; }
        else memcpy(text+total,buffer,read);
        total+=read;
        if(file) { SDL_LockMutex(u->mutex); u->state.progress=(int)(total/1024u); SDL_UnlockMutex(u->mutex); }
    }
    if(!file)text[total]='\0'; ok=total>0;
done:
    if(request)WinHttpCloseHandle(request); if(connection)WinHttpCloseHandle(connection); if(session)WinHttpCloseHandle(session);
    SDL_free(address); return ok;
}
static int worker(void *data) {
    ZMUpdater *u=data; ZMUpdateManifest manifest; char json[4096]; int valid;
    char *base=NULL,*path=NULL,*partial=NULL; wchar_t *wpath=NULL,*wpartial=NULL; FILE *file=NULL;
    if(!get(u,"https://github.com/Zernia01/Z_media/releases/latest/download/update.json",NULL,json,sizeof(json)) || !zm_update_parse(json,&manifest))goto fail;
    SDL_LockMutex(u->mutex); SDL_strlcpy(u->state.version,manifest.version,sizeof(u->state.version)); SDL_strlcpy(u->state.sha256,manifest.sha256,sizeof(u->state.sha256)); SDL_UnlockMutex(u->mutex);
    const char *current=ZM_VERSION;
#ifdef ZM_UPDATE_TESTING
    if(SDL_getenv("ZM_TEST_CURRENT_VERSION"))current=SDL_getenv("ZM_TEST_CURRENT_VERSION");
#endif
    if(zm_update_version_compare(manifest.version,current,&valid)<=0 && valid) { status(u,ZM_UPDATE_CURRENT); return 0; }
    if(!valid)goto fail;
    if(!u->automatic_download) { status(u,ZM_UPDATE_AVAILABLE); return 0; }
    base=SDL_GetPrefPath("Zernia","ZM"); path=SDL_malloc(32768); partial=SDL_malloc(32768);
#ifdef ZM_UPDATE_TESTING
    if(SDL_getenv("ZM_TEST_UPDATE_DIRECTORY")) { SDL_free(base); base=SDL_strdup(SDL_getenv("ZM_TEST_UPDATE_DIRECTORY")); }
#endif
    if(partial)partial[0]='\0';
    if(!base || !path || !partial || strlen(base)+100>=32768)goto fail;
    SDL_snprintf(path,32768,"%sZM-%s-setup-win64.exe",base,manifest.version);
    SDL_snprintf(partial,32768,"%s.part.%lu",path,GetCurrentProcessId());
    if(!zm_update_verify_file(path,manifest.sha256)) {
        status(u,ZM_UPDATE_DOWNLOADING);
        file=zm_fopen_utf8(partial,"wb"); if(!file)goto fail;
        int fetched=get(u,manifest.url,file,NULL,0);
        if(fclose(file)!=0)fetched=0; file=NULL;
        if(!fetched || !zm_update_verify_file(partial,manifest.sha256))goto fail;
        wpath=wide(path); wpartial=wide(partial);
        if(!wpath || !wpartial || !MoveFileExW(wpartial,wpath,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))goto fail;
    }
    SDL_LockMutex(u->mutex); SDL_strlcpy(u->state.path,path,sizeof(u->state.path)); u->state.status=ZM_UPDATE_READY; SDL_UnlockMutex(u->mutex);
    goto done;
fail:
    status(u,ZM_UPDATE_FAILED);
done:
    if(file)fclose(file);
    if(partial && *partial) { wchar_t *temp=wide(partial); if(temp)DeleteFileW(temp); SDL_free(temp); }
    SDL_free(base); SDL_free(path); SDL_free(partial); SDL_free(wpath); SDL_free(wpartial); return 0;
}
int zm_update_install(ZMUpdater *u) {
    ZMUpdateState state; wchar_t *path;
    /* Verify the cached installer again before starting it. */
    zm_update_snapshot(u,&state);
    if(state.status!=ZM_UPDATE_READY)return 0;
    if(!zm_update_verify_file(state.path,state.sha256)) { status(u,ZM_UPDATE_FAILED); return 0; }
    path=wide(state.path); if(!path)return 0;
    INT_PTR result=(INT_PTR)ShellExecuteW(NULL,L"open",path,L"/NORESTART",NULL,SW_SHOWNORMAL);
    SDL_free(path); return result>32;
}
#else
int zm_update_verify_file(const char *path,const char *sha) { (void)path; (void)sha; return 0; }
static int worker(void *data) { status(data,ZM_UPDATE_FAILED); return 0; }
int zm_update_install(ZMUpdater *u) { (void)u; return 0; }
#endif
int zm_update_init(ZMUpdater *u) { memset(u,0,sizeof(*u)); u->mutex=SDL_CreateMutex(); return u->mutex!=NULL; }
void zm_update_check(ZMUpdater *u,int download) {
    ZMUpdateState state;
    if(!u->mutex)return;
    zm_update_snapshot(u,&state);
    if(state.status==ZM_UPDATE_CHECKING || state.status==ZM_UPDATE_DOWNLOADING)return;
    if(u->thread) { SDL_WaitThread(u->thread,NULL); u->thread=NULL; }
    u->automatic_download=download; status(u,ZM_UPDATE_CHECKING);
    u->thread=SDL_CreateThread(worker,"ZM updater",u); if(!u->thread)status(u,ZM_UPDATE_FAILED);
}
void zm_update_snapshot(ZMUpdater *u,ZMUpdateState *state) { SDL_LockMutex(u->mutex); *state=u->state; SDL_UnlockMutex(u->mutex); }
void zm_update_destroy(ZMUpdater *u) {
    SDL_AtomicSet(&u->quit,1); if(u->thread)SDL_WaitThread(u->thread,NULL);
    if(u->mutex)SDL_DestroyMutex(u->mutex); memset(u,0,sizeof(*u));
}
