#include "zm/player.h"
#include <stdio.h>

int main(int argc,char **argv) {
    ZMPlayer player;
    ZMSnapshot state;
    Uint64 start;
    unsigned frames=0,underruns=0;
    int failed=0,was_empty=0;
    if(argc!=2)return 1;
    SDL_SetMainReady(); SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    if(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)<0 || !zm_player_init(&player))return 1;
    SDL_LockMutex(player.mutex); player.state.hardware_mode=2; SDL_UnlockMutex(player.mutex);
    zm_player_open(&player,argv[1]); start=SDL_GetTicks64();
    do {
        ZMVideoFrame frame;
        while(zm_player_take_frame(&player,&frame)) { av_frame_free(&frame.image); ++frames; }
        zm_player_snapshot(&player,&state);
        SDL_Delay(2);
    } while(SDL_GetTicks64()-start<1500 && !state.error[0]);
    if(!state.loaded || !frames || !state.audio_frames || state.error[0])failed=1;
    /* A slow renderer must not stop the decoder from supplying audio. */
    start=SDL_GetTicks64();
    do {
        int empty=SDL_GetQueuedAudioSize(player.audio)==0;
        if(empty && !was_empty)++underruns;
        was_empty=empty;
        SDL_Delay(2);
    } while(SDL_GetTicks64()-start<450);
    start=SDL_GetTicks64();
    do {
        ZMVideoFrame frame;
        while(zm_player_take_frame(&player,&frame)) { av_frame_free(&frame.image); ++frames; }
        zm_player_snapshot(&player,&state); SDL_Delay(2);
    } while(SDL_GetTicks64()-start<350 && !state.error[0]);
    printf("Slow renderer: audio underruns=%u frames=%u clock=%.3f error=%s\n",underruns,frames,state.position,state.error);
    if(state.error[0] || state.position<2.0)failed=1;
    zm_player_destroy(&player); SDL_Quit();
    return failed?1:underruns?2:0;
}
