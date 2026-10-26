#include "zm/player.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc,char **argv) {
    ZMPlayer player; ZMSnapshot state={0};
    Uint64 start,last=0,max_gap=0;
    unsigned frames=0,underruns=0;
    double sum=0,squares=0; unsigned intervals=0;
    int empty_before=0,failed=0,seconds=argc>2?atoi(argv[2]):12;
    if(argc<2 || seconds<2)return 1;
    SDL_SetMainReady(); SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    if(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_TIMER)<0 || !zm_player_init(&player))return 1;
    SDL_LockMutex(player.mutex); player.state.hardware_mode=2; player.state.subtitle_auto=0; SDL_UnlockMutex(player.mutex);
    zm_player_open(&player,argv[1]); start=SDL_GetTicks64();
    while(SDL_GetTicks64()-start<(Uint64)seconds*1000 && !state.error[0]) {
        ZMVideoFrame frame; Uint64 now=SDL_GetTicks64(); int shown=0;
        while(zm_player_take_frame(&player,&frame)) { av_frame_free(&frame.image); ++frames; shown=1; }
        zm_player_snapshot(&player,&state);
        if(shown) {
            if(last) { double gap=(double)(now-last); if(now-last>max_gap)max_gap=now-last; sum+=gap; squares+=gap*gap; ++intervals; }
            last=now;
        }
        if(state.audio_frames && state.loaded && !state.ended && state.position<state.duration-0.25) {
            int empty=SDL_GetAudioDeviceStatus(player.audio)==SDL_AUDIO_PLAYING && !SDL_GetQueuedAudioSize(player.audio);
            if(empty && !empty_before)++underruns;
            empty_before=empty;
        }
        SDL_Delay(2);
    }
    double jitter=intervals?sqrt(fmax(0,squares/intervals-(sum/intervals)*(sum/intervals))):0.0;
    printf("Cadence: seconds=%d frames=%u max_gap_ms=%llu jitter_ms=%.3f audio_underruns=%u position=%.3f error=%s\n",seconds,frames,(unsigned long long)max_gap,jitter,underruns,state.position,state.error);
    if(!state.loaded || frames<(unsigned)(seconds*10) || state.error[0] || max_gap>140 || jitter>8.0 || underruns)failed=1;
    zm_player_destroy(&player); SDL_Quit(); return failed;
}
