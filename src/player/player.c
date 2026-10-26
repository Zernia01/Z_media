#include "zm/player.h"
#include <string.h>
#include <math.h>

double zm_player_clock_locked(ZMPlayer *p) {
    double clock;
    Uint64 now=SDL_GetTicks64();
    if (p->state.paused || !p->state.loaded) return p->base_pts;
    Uint32 queued=p->audio?SDL_GetQueuedAudioSize(p->audio):0;
    if (p->state.has_audio && queued && p->audio_end >= 0.0) {
        double reference=p->audio_end-(double)queued/(48000.0*2.0*sizeof(float));
        double elapsed=(double)(now-p->base_ticks)/1000.0;
        clock=p->base_pts+elapsed;
        /* SDL's queued-byte count advances in device-sized blocks. Interpolate
           between them and gently follow the audio clock instead of scheduling
           30 fps video against a staircase of roughly 21 ms audio callbacks. */
        if(!p->audio_clock_valid || fabs(reference-clock)>0.1)clock=reference;
        else {
            double correction=(reference-clock)*fmin(1.0,elapsed/0.5);
            double bound=elapsed*0.1;
            clock+=fmax(-bound,fmin(bound,correction));
        }
        p->audio_clock_valid=1;
        p->base_pts=clock; p->base_ticks=now;
    } else {
        clock = p->base_pts + (double)(SDL_GetTicks64() - p->base_ticks) / 1000.0;
    }
    if (clock < 0.0) clock = 0.0;
    if (p->state.duration > 0.0 && clock > p->state.duration) clock = p->state.duration;
    return clock;
}
void zm_player_clear_locked(ZMPlayer *p) {
    int i;
    for (i = 0; i < ZM_FRAME_CAPACITY; ++i) av_frame_free(&p->frames[i].image);
    p->count = p->read_index = 0;
    p->audio_clock_valid=0;
    if (p->audio) SDL_ClearQueuedAudio(p->audio);
}
int zm_player_init(ZMPlayer *p) {
    SDL_AudioSpec desired = {0};
    memset(p, 0, sizeof(*p));
    p->mutex = SDL_CreateMutex();
    if (!p->mutex) return 0;
    p->state.volume = 80;
    p->state.subtitles_enabled = 1; p->state.subtitle_size = 24; p->state.subtitle_auto = 1;
    p->state.subtitle_index = -1; SDL_strlcpy(p->state.preferred_language, "ko", sizeof(p->state.preferred_language));
    p->audio_end = -1.0;
    desired.freq = 48000;
    desired.format = AUDIO_F32SYS;
    desired.channels = 2;
    desired.samples = 1024;
    p->audio = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
    p->thread = SDL_CreateThread(zm_decode_worker, "ZM decode", p);
    if (!p->thread) { zm_player_destroy(p); return 0; }
    return 1;
}
void zm_player_destroy(ZMPlayer *p) {
    SDL_AtomicSet(&p->quit, 1);
    if (p->thread) SDL_WaitThread(p->thread, NULL);
    zm_player_clear_locked(p);
    if (p->audio) SDL_CloseAudioDevice(p->audio);
    if (p->mutex) SDL_DestroyMutex(p->mutex);
    memset(p, 0, sizeof(*p));
}
void zm_player_open(ZMPlayer *p, const char *path) {
    if (!path || !*path || strlen(path) >= ZM_PATH_CAPACITY) return;
    SDL_LockMutex(p->mutex);
    SDL_strlcpy(p->requested_path, path, sizeof(p->requested_path));
    p->state.loading = 1;
    p->state.error[0] = '\0';
    SDL_AtomicSet(&p->change, 1);
    SDL_UnlockMutex(p->mutex);
}
void zm_player_toggle_pause(ZMPlayer *p) {
    SDL_LockMutex(p->mutex);
    if (p->state.loaded && !p->state.ended) {
        p->base_pts = zm_player_clock_locked(p);
        p->base_ticks = SDL_GetTicks64();
        p->state.paused = !p->state.paused;
        if (p->audio) SDL_PauseAudioDevice(p->audio, p->state.paused);
    }
    SDL_UnlockMutex(p->mutex);
}
void zm_player_seek(ZMPlayer *p, double seconds) {
    SDL_LockMutex(p->mutex);
    if (p->state.loaded) {
        if (seconds < 0.0) seconds = 0.0;
        if (p->state.duration > 0.0 && seconds > p->state.duration) seconds = p->state.duration;
        p->seek_target = seconds;
        SDL_AtomicSet(&p->seek_pending, 1);
    }
    SDL_UnlockMutex(p->mutex);
}
void zm_player_volume(ZMPlayer *p, int volume) {
    SDL_LockMutex(p->mutex);
    p->state.volume = volume < 0 ? 0 : volume > 100 ? 100 : volume;
    SDL_UnlockMutex(p->mutex);
}
void zm_player_mute(ZMPlayer *p) {
    SDL_LockMutex(p->mutex); p->state.muted = !p->state.muted; SDL_UnlockMutex(p->mutex);
}
void zm_player_next_audio(ZMPlayer *p) {
    SDL_LockMutex(p->mutex);
    if(p->state.loaded && p->state.audio_count>1) {
        p->state.audio_index=(p->state.audio_index+1)%p->state.audio_count;
        p->seek_target=zm_player_clock_locked(p);
        SDL_AtomicSet(&p->audio_switch,1); SDL_AtomicSet(&p->seek_pending,1);
    }
    SDL_UnlockMutex(p->mutex);
}
static void refresh_subtitle_locked(ZMPlayer *p) {
    p->state.error[0] = '\0';
    SDL_AtomicSet(&p->subtitle_pending, 1);
    if (p->state.loaded) {
        p->seek_target = zm_player_clock_locked(p);
        SDL_AtomicSet(&p->seek_pending, 1);
    }
}
void zm_player_subtitle(ZMPlayer *p, const char *path, int stream) {
    SDL_LockMutex(p->mutex);
    if (path) SDL_strlcpy(p->state.subtitle_path, path, sizeof(p->state.subtitle_path));
    p->state.subtitle_index = stream; p->state.subtitles_enabled = 1;
    refresh_subtitle_locked(p); SDL_UnlockMutex(p->mutex);
}
void zm_player_subtitle_toggle(ZMPlayer *p) {
    SDL_LockMutex(p->mutex); p->state.subtitles_enabled = !p->state.subtitles_enabled;
    refresh_subtitle_locked(p); SDL_UnlockMutex(p->mutex);
}
void zm_player_subtitle_options(ZMPlayer *p, int delay_ms, int size, int override_style) {
    SDL_LockMutex(p->mutex);
    p->state.subtitle_delay_ms = delay_ms < -600000 ? -600000 : delay_ms > 600000 ? 600000 : delay_ms;
    p->state.subtitle_size = size < 8 ? 8 : size > 120 ? 120 : size;
    p->state.subtitle_style = override_style;
    refresh_subtitle_locked(p); SDL_UnlockMutex(p->mutex);
}
void zm_player_snapshot(ZMPlayer *p, ZMSnapshot *out) {
    SDL_LockMutex(p->mutex); *out = p->state; out->position = zm_player_clock_locked(p); SDL_UnlockMutex(p->mutex);
}
int zm_player_take_frame(ZMPlayer *p, ZMVideoFrame *out) {
    int result = 0;
    SDL_LockMutex(p->mutex);
    if (p->count && (p->preview_pending || (!p->state.paused && p->frames[p->read_index].pts <= zm_player_clock_locked(p) + 0.015))) {
        *out = p->frames[p->read_index];
        p->frames[p->read_index].image = NULL;
        p->read_index = (p->read_index + 1) % ZM_FRAME_CAPACITY;
        --p->count; p->preview_pending = 0; result = 1;
    }
    SDL_UnlockMutex(p->mutex);
    return result;
}
