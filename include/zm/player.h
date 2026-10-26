#ifndef ZM_PLAYER_H
#define ZM_PLAYER_H
#include <SDL.h>
#include <libavutil/frame.h>
#define ZM_FRAME_CAPACITY 12
#define ZM_PATH_CAPACITY 32768
typedef struct ZMVideoFrame { AVFrame *image; double pts; } ZMVideoFrame;
typedef struct ZMSnapshot {
    int loaded, loading, paused, ended, has_audio, has_video;
    double position, duration;
    int volume, muted, width, height;
    unsigned generation, media_generation;
    char path[ZM_PATH_CAPACITY], error[512], codec[80];
    unsigned video_frames, audio_frames;
    int subtitles_enabled, subtitle_delay_ms, subtitle_size, subtitle_style, subtitle_auto;
    int subtitle_index, subtitle_count;
    char subtitle_path[ZM_PATH_CAPACITY], preferred_language[16];
    int hardware_mode, hardware_active;
    int audio_index, audio_count;
} ZMSnapshot;
typedef struct ZMPlayer {
    SDL_mutex *mutex;
    SDL_Thread *thread;
    SDL_atomic_t quit, change, seek_pending, subtitle_pending, audio_switch;
    SDL_AudioDeviceID audio;
    ZMSnapshot state;
    char requested_path[ZM_PATH_CAPACITY];
    double seek_target, audio_end, base_pts;
    Uint64 base_ticks;
    ZMVideoFrame frames[ZM_FRAME_CAPACITY];
    int read_index, count, preview_pending, allow_nv12, audio_clock_valid;
} ZMPlayer;
int zm_player_init(ZMPlayer *p);
void zm_player_destroy(ZMPlayer *p);
void zm_player_open(ZMPlayer *p, const char *path);
void zm_player_toggle_pause(ZMPlayer *p);
void zm_player_seek(ZMPlayer *p, double seconds);
void zm_player_volume(ZMPlayer *p, int volume);
void zm_player_mute(ZMPlayer *p);
void zm_player_subtitle(ZMPlayer *p, const char *path, int stream);
void zm_player_subtitle_toggle(ZMPlayer *p);
void zm_player_subtitle_options(ZMPlayer *p, int delay_ms, int size, int override_style);
void zm_player_next_audio(ZMPlayer *p);
void zm_player_snapshot(ZMPlayer *p, ZMSnapshot *out);
int zm_player_take_frame(ZMPlayer *p, ZMVideoFrame *out);
/* Worker helpers: mutex must be held for clock/clear. */
double zm_player_clock_locked(ZMPlayer *p);
void zm_player_clear_locked(ZMPlayer *p);
int zm_decode_worker(void *data);
#endif
