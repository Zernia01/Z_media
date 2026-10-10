#include "zm/player.h"
#include "zm/subtitle.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned frame_hash(const AVFrame *frame) {
    unsigned hash=2166136261u;
    int x,y;
    for(y=0;y<frame->height;++y)for(x=0;x<frame->width;++x)
        hash=(hash^frame->data[0][y*frame->linesize[0]+x])*16777619u;
    return hash;
}
static unsigned render_hash(ZMSubtitleFilter *filter, double pts) {
    AVFrame *frame=av_frame_alloc();
    unsigned hash;
    int y;
    frame->format=AV_PIX_FMT_YUV420P; frame->width=320; frame->height=180;
    if(av_frame_get_buffer(frame,32)<0) { av_frame_free(&frame); return 0; }
    for(y=0;y<180;++y)memset(frame->data[0]+y*frame->linesize[0],16,320);
    for(y=0;y<90;++y) {
        memset(frame->data[1]+y*frame->linesize[1],128,160);
        memset(frame->data[2]+y*frame->linesize[2],128,160);
    }
    if(zm_subtitle_apply(filter,frame,pts)<0)hash=0;
    else hash=frame_hash(frame);
    av_frame_free(&frame); return hash;
}
int main(int argc,char **argv) {
    const double targets[]={70.0,600.0,70.0,603.3,615.0,70.0};
    ZMPlayer player;
    ZMSnapshot state;
    ZMSubtitleFilter reused={0},fresh={0};
    Uint64 start;
    unsigned blank,first=0;
    int i,failed=0;
    if(argc!=2)return 1;
    SDL_SetMainReady(); SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    if(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)<0 || !zm_player_init(&player))return 1;
    /* Arbitrary forward/backward subtitle timestamps must match a fresh graph. */
    blank=render_hash(&fresh,70.0);
    if(zm_subtitle_open(&reused,argv[1],0,320,180,24,1)<0)failed=1;
    for(i=0;i<6 && !failed;++i) {
        unsigned actual,expected;
        if(zm_subtitle_open(&fresh,argv[1],0,320,180,24,1)<0) { failed=1; break; }
        actual=render_hash(&reused,targets[i]); expected=render_hash(&fresh,targets[i]);
        if(!actual || actual!=expected)failed=1;
        if(i==0)first=actual;
        if(i==1 && (actual==blank || actual==first))failed=1;
        if(i==4 && actual!=blank)failed=1;
    }
    zm_subtitle_close(&reused); zm_subtitle_close(&fresh);
    if(zm_subtitle_open(&reused,argv[1],0,320,180,24,1)<0)failed=1;
    SDL_LockMutex(player.mutex); player.state.hardware_mode=2; player.state.subtitle_style=1; SDL_UnlockMutex(player.mutex);
    zm_player_open(&player,argv[1]); start=SDL_GetTicks64();
    do {
        ZMVideoFrame frame;
        while(zm_player_take_frame(&player,&frame))av_frame_free(&frame.image);
        zm_player_snapshot(&player,&state);
        if(state.loaded && state.video_frames && state.audio_frames)break;
        SDL_Delay(1);
    } while(SDL_GetTicks64()-start<10000 && !state.error[0]);
    if(!state.loaded || !state.video_frames || !state.audio_frames || state.error[0])failed=1;
    zm_player_toggle_pause(&player);
    for(i=0;i<6 && !failed;++i) {
        unsigned generation;
        int received=0;
        zm_player_snapshot(&player,&state); generation=state.generation;
        start=SDL_GetTicks64(); zm_player_seek(&player,targets[i]);
        do {
            ZMVideoFrame frame;
            zm_player_snapshot(&player,&state);
            if(state.generation!=generation && zm_player_take_frame(&player,&frame)) {
                printf("Seek %.3f: %llu ms, first frame %.3f\n",targets[i],(unsigned long long)(SDL_GetTicks64()-start),frame.pts);
                if(fabs(frame.pts-targets[i])>0.1)failed=1;
                if(frame_hash(frame.image)!=render_hash(&reused,frame.pts))failed=1;
                av_frame_free(&frame.image); received=1; break;
            }
            SDL_Delay(1);
        } while(SDL_GetTicks64()-start<10000 && !state.error[0]);
        if(!received || state.error[0])failed=1;
    }
    if(!failed) {
        unsigned audio_before=state.audio_frames;
        zm_player_toggle_pause(&player); start=SDL_GetTicks64();
        do {
            ZMVideoFrame frame;
            while(zm_player_take_frame(&player,&frame))av_frame_free(&frame.image);
            SDL_Delay(5);
        } while(SDL_GetTicks64()-start<250);
        zm_player_snapshot(&player,&state);
        if(state.position<=70.1 || state.audio_frames<=audio_before || state.error[0])failed=1;
    }
    zm_subtitle_close(&reused);
    zm_player_destroy(&player); SDL_Quit();
    printf("Seek/subtitle regression: %s\n",failed?"FAIL":"PASS"); return failed;
}
