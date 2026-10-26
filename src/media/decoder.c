#include "zm/player.h"
#include "zm/subtitle.h"
#include "zm/hwaccel.h"
#include <libavutil/hwcontext.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/error.h>
#include <libavutil/channel_layout.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#define ZM_AUDIO_LIMIT (48000u * 2u * sizeof(float) / 2u)
#define ZM_AUDIO_LOW (48000u * 2u * sizeof(float) * 150u / 1000u)
#define ZM_PACKET_BUDGET (16u * 1024u * 1024u)

typedef struct PacketNode { AVPacket packet; struct PacketNode *next; } PacketNode;
typedef struct Decoder {
    ZMPlayer *p;
    AVFormatContext *format;
    AVCodecContext *video, *audio;
    AVPacket *packet;
    AVPacket *ahead_packet;
    AVFrame *frame;
    AVFrame *audio_frame;
    AVFrame *transfer;
    ZMHardware hardware;
    int software_only;
    struct SwsContext *sws;
    SwrContext *swr;
    float *audio_buffer;
    unsigned audio_capacity;
    PacketNode *ahead_first,*ahead_last;
    size_t ahead_bytes;
    unsigned ahead_count;
    int read_eof,read_error;
    ZMSubtitleFilter subtitle;
    int vi, ai, eof, requested_audio;
    double origin, target, next_audio, next_video;
} Decoder;
static int read_audio_ahead(Decoder *d);
static void clear_read_ahead(Decoder *d) {
    while(d->ahead_first) {
        PacketNode *node=d->ahead_first; d->ahead_first=node->next;
        av_packet_unref(&node->packet); SDL_free(node);
    }
    d->ahead_last=NULL; d->ahead_bytes=d->ahead_count=0;
    d->read_eof=d->read_error=0;
}
static int cancelled(Decoder *d) {
    return SDL_AtomicGet(&d->p->quit) || SDL_AtomicGet(&d->p->change) || SDL_AtomicGet(&d->p->seek_pending);
}
static int interrupt_read(void *data) {
    ZMPlayer *p = data;
    return SDL_AtomicGet(&p->quit) || SDL_AtomicGet(&p->change);
}
static void error_message(ZMPlayer *p, const char *context, int code) {
    char reason[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(code, reason, sizeof(reason));
    SDL_LockMutex(p->mutex);
    SDL_snprintf(p->state.error, sizeof(p->state.error), "%s: %s", context, reason);
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", p->state.error);
    SDL_UnlockMutex(p->mutex);
}
static void close_decoder(Decoder *d) {
    clear_read_ahead(d);
    av_packet_free(&d->ahead_packet); av_frame_free(&d->audio_frame);
    av_packet_free(&d->packet); av_frame_free(&d->frame);
    av_frame_free(&d->transfer);
    avcodec_free_context(&d->video); avcodec_free_context(&d->audio);
    sws_freeContext(d->sws); swr_free(&d->swr);
    av_freep(&d->audio_buffer);
    zm_subtitle_close(&d->subtitle);
    avformat_close_input(&d->format);
    memset(d, 0, sizeof(*d));
}
static int open_codec(Decoder *d, enum AVMediaType type, AVCodecContext **ctx) {
    const AVCodec *codec = NULL;
    int result, index = av_find_best_stream(d->format, type, -1, -1, &codec, 0);
    if(type==AVMEDIA_TYPE_AUDIO && d->requested_audio>=0) {
        unsigned i; int ordinal=0;
        index=AVERROR_STREAM_NOT_FOUND;
        for(i=0;i<d->format->nb_streams;++i) if(d->format->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO) {
            if(ordinal++==d->requested_audio) { index=(int)i; codec=avcodec_find_decoder(d->format->streams[i]->codecpar->codec_id); break; }
        }
        if(!codec) return AVERROR_DECODER_NOT_FOUND;
    }
    if (index < 0) return index;
    *ctx = avcodec_alloc_context3(codec);
    if (!*ctx) return AVERROR(ENOMEM);
    result = avcodec_parameters_to_context(*ctx, d->format->streams[index]->codecpar);
    if (result >= 0 && type == AVMEDIA_TYPE_VIDEO && !d->software_only) {
        int mode;
        SDL_LockMutex(d->p->mutex); mode = d->p->state.hardware_mode; SDL_UnlockMutex(d->p->mutex);
        if (mode != 2) zm_hardware_attach(*ctx, &d->hardware);
    }
    if(result>=0 && type==AVMEDIA_TYPE_VIDEO && !(*ctx)->hw_device_ctx) {
        int cores=SDL_GetCPUCount();
        (*ctx)->thread_count=cores<1?1:cores>8?8:cores;
    }
    if (result >= 0) result = avcodec_open2(*ctx, codec, NULL);
    if (result < 0 && (*ctx)->hw_device_ctx) {
        avcodec_free_context(ctx); d->software_only = 1;
        return open_codec(d, type, ctx);
    }
    if (result < 0) { avcodec_free_context(ctx); return result; }
    return index;
}
static int open_decoder(Decoder *d, ZMPlayer *p, const char *path) {
    int result;
    int subtitle_auto, subtitle_count = 0, subtitle_index = -1;
    char subtitle_path[ZM_PATH_CAPACITY], language[16];
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    memset(d, 0, sizeof(*d)); d->p = p; d->vi = d->ai = d->requested_audio = -1;
    d->format = avformat_alloc_context();
    if (!d->format) return AVERROR(ENOMEM);
    d->format->interrupt_callback.callback = interrupt_read;
    d->format->interrupt_callback.opaque = p;
    result = avformat_open_input(&d->format, path, NULL, NULL);
    if (result < 0) return result;
    result = avformat_find_stream_info(d->format, NULL);
    if (result < 0) return result;
    d->origin = d->format->start_time == AV_NOPTS_VALUE ? 0.0 : (double)d->format->start_time / AV_TIME_BASE;
    d->vi = open_codec(d, AVMEDIA_TYPE_VIDEO, &d->video);
    if (p->audio) d->ai = open_codec(d, AVMEDIA_TYPE_AUDIO, &d->audio);
    if (!d->video && !d->audio) return AVERROR_DECODER_NOT_FOUND;
    if (d->audio) {
        result = swr_alloc_set_opts2(&d->swr, &stereo, AV_SAMPLE_FMT_FLT, 48000,
            &d->audio->ch_layout, d->audio->sample_fmt, d->audio->sample_rate, 0, NULL);
        if (result < 0) return result;
        result = swr_init(d->swr);
        if (result < 0) return result;
    }
    d->packet = av_packet_alloc(); d->frame = av_frame_alloc();
    d->ahead_packet=av_packet_alloc(); d->audio_frame=av_frame_alloc();
    d->transfer = av_frame_alloc();
    if (!d->packet || !d->frame || !d->transfer || !d->ahead_packet || !d->audio_frame) return AVERROR(ENOMEM);
    SDL_LockMutex(p->mutex);
    subtitle_auto = p->state.subtitle_auto; SDL_strlcpy(language,p->state.preferred_language,sizeof(language));
    SDL_UnlockMutex(p->mutex);
    subtitle_path[0]='\0';
    {
        unsigned i;
        for(i=0;i<d->format->nb_streams;++i) if(d->format->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_SUBTITLE) ++subtitle_count;
    }
    if(subtitle_auto && !zm_subtitle_find(path,language,subtitle_path,sizeof(subtitle_path)) && subtitle_count) {
        SDL_strlcpy(subtitle_path,path,sizeof(subtitle_path)); subtitle_index=0;
    }
    SDL_LockMutex(p->mutex);
    p->state.loaded = 1; p->state.loading = 0; p->state.paused = p->state.ended = 0;
    p->state.has_audio = d->audio != NULL; p->state.has_video = d->video != NULL;
    p->state.hardware_active = 0;
    p->state.audio_count=0; p->state.audio_index=0;
    {
        unsigned i;
        for(i=0;i<d->format->nb_streams;++i) if(d->format->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO) {
            if((int)i==d->ai)p->state.audio_index=p->state.audio_count;
            ++p->state.audio_count;
        }
    }
    p->state.width = d->video ? d->video->width : 0;
    p->state.height = d->video ? d->video->height : 0;
    p->state.duration = d->format->duration == AV_NOPTS_VALUE ? 0.0 : (double)d->format->duration / AV_TIME_BASE;
    SDL_strlcpy(p->state.path, path, sizeof(p->state.path));
    SDL_strlcpy(p->state.codec, d->video ? d->video->codec->name : d->audio->codec->name, sizeof(p->state.codec));
    p->base_pts = 0.0; p->base_ticks = SDL_GetTicks64(); p->audio_end = -1.0; p->preview_pending = 1;
    SDL_strlcpy(p->state.subtitle_path,subtitle_path,sizeof(p->state.subtitle_path));
    p->state.subtitle_count=subtitle_count; p->state.subtitle_index=subtitle_index;
    SDL_AtomicSet(&p->subtitle_pending,1);
    if (p->audio) SDL_PauseAudioDevice(p->audio, 1);
    SDL_UnlockMutex(p->mutex);
    return 0;
}
static void refresh_subtitle(Decoder *d, int width, int height) {
    ZMSnapshot *state = SDL_malloc(sizeof(*state));
    int result;
    if (!state) return;
    SDL_LockMutex(d->p->mutex); *state = d->p->state; SDL_AtomicSet(&d->p->subtitle_pending, 0); SDL_UnlockMutex(d->p->mutex);
    result = zm_subtitle_open(&d->subtitle, state->subtitles_enabled ? state->subtitle_path : NULL, state->subtitle_index,
        width, height, state->subtitle_size, state->subtitle_style);
    if (result < 0) error_message(d->p, "Subtitle", result);
    SDL_free(state);
}
static double frame_pts(Decoder *d, int stream, double fallback) {
    int64_t ts = d->frame->best_effort_timestamp;
    return ts == AV_NOPTS_VALUE ? fallback : (double)ts * av_q2d(d->format->streams[stream]->time_base) - d->origin;
}
static int output_video(Decoder *d) {
    ZMPlayer *p = d->p;
    AVFrame *image;
    AVFrame *source = d->frame;
    double pts = frame_pts(d, d->vi, d->next_video);
    AVRational rate = av_guess_frame_rate(d->format, d->format->streams[d->vi], d->frame);
    d->next_video = pts + (rate.num > 0 && rate.den > 0 ? av_q2d(av_inv_q(rate)) : 1.0 / 30.0);
    if (pts + 0.01 < d->target) return 0;
    SDL_LockMutex(p->mutex);
    /* Continue decoding reference frames, but do not convert/render obsolete
       output when playback is catching up. Preserve paused/seek previews. */
    int late=!p->state.paused && !p->preview_pending && pts<zm_player_clock_locked(p)-0.1;
    SDL_UnlockMutex(p->mutex);
    if(late)return 0;
    while (!cancelled(d)) {
        size_t bytes=(size_t)d->frame->width*(size_t)d->frame->height*3u/2u;
        int limit=bytes ? (int)((64u*1024u*1024u)/bytes) : 1;
        int room;
        if(limit<1)limit=1; if(limit>ZM_FRAME_CAPACITY)limit=ZM_FRAME_CAPACITY;
        SDL_LockMutex(p->mutex);
        /* With little audio buffered, a due frame must not hold up audio for
           another 80 ms. Keep future frames and paused/seek previews intact. */
        double clock=zm_player_clock_locked(p);
        int audio_low=p->state.has_audio && p->audio && SDL_GetQueuedAudioSize(p->audio)<ZM_AUDIO_LOW;
        int refill_audio=audio_low && !p->state.paused;
        if(p->count>=limit && !p->state.paused && !p->preview_pending &&
           p->frames[p->read_index].pts<clock-(audio_low?0.0:0.08)) {
            av_frame_free(&p->frames[p->read_index].image);
            p->read_index=(p->read_index+1)%ZM_FRAME_CAPACITY; --p->count;
        }
        room = p->count < limit;
        SDL_UnlockMutex(p->mutex);
        if (room) break;
        /* Some MP4s store half a second of video before the next audio chunk.
           Read through that chunk without decoding more future video frames. */
        if(refill_audio) {
            int result=read_audio_ahead(d);
            if(result<0)return result;
        }
        SDL_Delay(3);
    }
    if (cancelled(d)) return 0;
    if (d->frame->hw_frames_ctx) {
        int transfer_result;
        av_frame_unref(d->transfer);
        transfer_result = av_hwframe_transfer_data(d->transfer, d->frame, 0);
        if (transfer_result < 0) return transfer_result;
        source = d->transfer;
    }
    SDL_LockMutex(p->mutex); p->state.hardware_active = source != d->frame; SDL_UnlockMutex(p->mutex);
    if (SDL_AtomicGet(&p->subtitle_pending) || (d->subtitle.graph && (d->subtitle.width != source->width || d->subtitle.height != source->height)))
        refresh_subtitle(d, source->width, source->height);
    SDL_LockMutex(p->mutex); int nv12=p->allow_nv12 && !d->subtitle.graph && source->format==AV_PIX_FMT_NV12; SDL_UnlockMutex(p->mutex);
    if((source->format==AV_PIX_FMT_YUV420P || nv12) && source->linesize[0]>0 && source->linesize[1]>0 && (nv12 || source->linesize[2]>0)) {
        /* Keep a buffer reference instead of allocating and copying all planes.
           The subtitle filter makes its input writable when drawing is needed. */
        image=av_frame_clone(source);
        if(!image)return AVERROR(ENOMEM);
    } else {
        image = av_frame_alloc();
        if (!image) return AVERROR(ENOMEM);
        image->format = AV_PIX_FMT_YUV420P; image->width = source->width; image->height = source->height;
        if (av_frame_get_buffer(image, 32) < 0) { av_frame_free(&image); return AVERROR(ENOMEM); }
        d->sws = sws_getCachedContext(d->sws, image->width, image->height, (enum AVPixelFormat)source->format,
            image->width, image->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
        if (!d->sws) { av_frame_free(&image); return AVERROR(ENOMEM); }
        if (sws_scale(d->sws, (const uint8_t *const *)source->data, source->linesize, 0, image->height, image->data, image->linesize) <= 0) {
            av_frame_free(&image); return AVERROR_INVALIDDATA;
        }
    }
    if (d->subtitle.graph) {
        int delay, result;
        SDL_LockMutex(p->mutex); delay = p->state.subtitle_delay_ms; SDL_UnlockMutex(p->mutex);
        result = zm_subtitle_apply(&d->subtitle, image, pts - (double)delay / 1000.0);
        if (result < 0) { error_message(p, "Subtitle render", result); zm_subtitle_close(&d->subtitle); }
    }
    SDL_LockMutex(p->mutex);
    if (!cancelled(d)) {
        int tail = (p->read_index + p->count) % ZM_FRAME_CAPACITY;
        p->frames[tail].image = image; p->frames[tail].pts = pts; ++p->count;
        ++p->state.video_frames;
    } else av_frame_free(&image);
    SDL_UnlockMutex(p->mutex);
    return 0;
}
static int output_audio(Decoder *d) {
    ZMPlayer *p = d->p;
    int capacity, samples, skip = 0, i, result = 0;
    float *buffer;
    uint8_t *output;
    double pts = frame_pts(d, d->ai, d->next_audio);
    int64_t delay = swr_get_delay(d->swr, d->audio->sample_rate);
    pts -= (double)delay / d->audio->sample_rate;
    capacity = (int)av_rescale_rnd(delay + d->frame->nb_samples, 48000, d->audio->sample_rate, AV_ROUND_UP);
    if(capacity<=0 || (size_t)capacity>UINT_MAX/(2u*sizeof(float)))return AVERROR(ENOMEM);
    av_fast_malloc(&d->audio_buffer,&d->audio_capacity,(size_t)capacity*2u*sizeof(float));
    buffer=d->audio_buffer;
    if (!buffer) return AVERROR(ENOMEM);
    output = (uint8_t *)buffer;
    samples = swr_convert(d->swr, &output, capacity, (const uint8_t **)d->frame->extended_data, d->frame->nb_samples);
    if (samples < 0) return samples;
    d->next_audio = pts + (double)samples / 48000.0;
    if (pts < d->target) skip = (int)fmin((double)samples, ceil((d->target - pts) * 48000.0));
    while (!cancelled(d) && SDL_GetQueuedAudioSize(p->audio) > ZM_AUDIO_LIMIT) SDL_Delay(3);
    SDL_LockMutex(p->mutex);
    if (!cancelled(d) && samples > skip) {
        float gain = p->state.muted ? 0.0f : (float)p->state.volume / 100.0f;
        for (i = skip * 2; i < samples * 2; ++i) buffer[i] *= gain;
        if (SDL_QueueAudio(p->audio, buffer + skip * 2, (Uint32)((samples - skip) * 2) * (Uint32)sizeof(float)) < 0)
            result = AVERROR_EXTERNAL;
        p->audio_end = d->next_audio;
        ++p->state.audio_frames;
        if((SDL_GetAudioDeviceStatus(p->audio)==SDL_AUDIO_PAUSED)!=p->state.paused)
            SDL_PauseAudioDevice(p->audio, p->state.paused);
    }
    SDL_UnlockMutex(p->mutex);
    return result;
}
static int drain(Decoder *d, AVCodecContext *codec, int video) {
    int result;
    while ((result = avcodec_receive_frame(codec, d->frame)) >= 0) {
        result = video ? output_video(d) : output_audio(d);
        av_frame_unref(d->frame);
        if (result < 0 || cancelled(d)) return result;
    }
    return result == AVERROR(EAGAIN) || result == AVERROR_EOF ? 0 : result;
}
static int decode_packet(Decoder *d, AVCodecContext *codec, int video, AVPacket *packet) {
    int result = avcodec_send_packet(codec, packet);
    if (result == AVERROR(EAGAIN)) {
        result = drain(d, codec, video);
        if (result < 0) return result;
        result = avcodec_send_packet(codec, packet);
    }
    if (result < 0 && result != AVERROR_EOF) return result;
    return drain(d, codec, video);
}
static int read_audio_ahead(Decoder *d) {
    unsigned scanned=0;
    if(!d->audio || d->read_eof)return 0;
    while(!cancelled(d) && d->ahead_count<512 && d->ahead_bytes<ZM_PACKET_BUDGET && scanned++<64) {
        int result=av_read_frame(d->format,d->ahead_packet);
        if(result==AVERROR(EAGAIN))return 0;
        if(result<0) {
            if(!cancelled(d)) { d->read_eof=1; if(result!=AVERROR_EOF)d->read_error=result; }
            return 0;
        }
        if(d->ahead_packet->stream_index==d->vi) {
            PacketNode *node=SDL_calloc(1,sizeof(*node));
            if(!node) { av_packet_unref(d->ahead_packet); return AVERROR(ENOMEM); }
            av_packet_move_ref(&node->packet,d->ahead_packet);
            if(d->ahead_last)d->ahead_last->next=node; else d->ahead_first=node;
            d->ahead_last=node; d->ahead_bytes+=(size_t)node->packet.size; ++d->ahead_count;
        } else if(d->ahead_packet->stream_index==d->ai) {
            /* Audio decoding must not overwrite the video frame waiting for room. */
            AVFrame *video_frame=d->frame; d->frame=d->audio_frame;
            result=decode_packet(d,d->audio,0,d->ahead_packet);
            d->frame=video_frame; av_packet_unref(d->ahead_packet); return result;
        } else av_packet_unref(d->ahead_packet);
    }
    return 0;
}
static void seek_decoder(Decoder *d) {
    ZMPlayer *p = d->p;
    double target;
    int result;
    SDL_LockMutex(p->mutex); target = p->seek_target; SDL_AtomicSet(&p->seek_pending, 0); SDL_UnlockMutex(p->mutex);
    /* Decode forward from a keyframe at/before the requested position. */
    int64_t timestamp = (int64_t)((target + d->origin) * AV_TIME_BASE);
    result = avformat_seek_file(d->format, -1, INT64_MIN, timestamp, timestamp, 0);
    if (result < 0) { error_message(p, "Seek", result); return; }
    if (d->video) avcodec_flush_buffers(d->video);
    if (d->audio) { avcodec_flush_buffers(d->audio); swr_close(d->swr); swr_init(d->swr); }
    av_packet_unref(d->packet);
    av_packet_unref(d->ahead_packet); clear_read_ahead(d);
    d->target = target; d->next_audio = d->next_video = target; d->eof = 0;
    /* The subtitle graph renders by timestamp; seeking does not change its
       track/style. Keep loaded cues/fonts instead of rereading the whole media. */
    SDL_LockMutex(p->mutex);
    if (p->audio) SDL_PauseAudioDevice(p->audio, 1);
    zm_player_clear_locked(p); ++p->state.generation;
    p->state.ended = 0; p->base_pts = target; p->base_ticks = SDL_GetTicks64(); p->audio_end = -1.0; p->preview_pending = 1;
    SDL_UnlockMutex(p->mutex);
}
int zm_decode_worker(void *data) {
    ZMPlayer *p = data;
    Decoder d = {0};
    av_log_set_level(AV_LOG_ERROR);
    while (!SDL_AtomicGet(&p->quit)) {
        int result;
        if (SDL_AtomicGet(&p->change)) {
            char path[ZM_PATH_CAPACITY];
            close_decoder(&d);
            SDL_LockMutex(p->mutex);
            SDL_strlcpy(path, p->requested_path, sizeof(path));
            SDL_AtomicSet(&p->change, 0); SDL_AtomicSet(&p->seek_pending, 0);
            SDL_AtomicSet(&p->audio_switch,0);
            if (p->audio) SDL_PauseAudioDevice(p->audio, 1);
            zm_player_clear_locked(p); ++p->state.generation; ++p->state.media_generation;
            p->state.loaded = 0; p->state.video_frames = p->state.audio_frames = 0;
            SDL_UnlockMutex(p->mutex);
            result = open_decoder(&d, p, path);
            if (result < 0) {
                if (!SDL_AtomicGet(&p->change) && !SDL_AtomicGet(&p->quit)) error_message(p, "Open media", result);
                close_decoder(&d);
                SDL_LockMutex(p->mutex); p->state.loading = SDL_AtomicGet(&p->change); SDL_UnlockMutex(p->mutex);
            }
            continue;
        }
        if (!d.format) { SDL_Delay(10); continue; }
        if(SDL_AtomicGet(&p->audio_switch)) {
            AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO;
            SDL_LockMutex(p->mutex); d.requested_audio=p->state.audio_index; SDL_AtomicSet(&p->audio_switch,0); SDL_UnlockMutex(p->mutex);
            avcodec_free_context(&d.audio); swr_free(&d.swr);
            d.ai=open_codec(&d,AVMEDIA_TYPE_AUDIO,&d.audio);
            result=d.ai<0?d.ai:swr_alloc_set_opts2(&d.swr,&stereo,AV_SAMPLE_FMT_FLT,48000,&d.audio->ch_layout,d.audio->sample_fmt,d.audio->sample_rate,0,NULL);
            if(result>=0)result=swr_init(d.swr);
            if(result<0) { error_message(p,"Audio track",result); avcodec_free_context(&d.audio); swr_free(&d.swr); d.ai=-1; }
            SDL_LockMutex(p->mutex); p->state.has_audio=d.audio!=NULL; SDL_UnlockMutex(p->mutex);
        }
        if (SDL_AtomicGet(&p->seek_pending)) { seek_decoder(&d); continue; }
        if (d.eof) {
            SDL_LockMutex(p->mutex);
            if (!p->count && (!p->audio || !SDL_GetQueuedAudioSize(p->audio))) {
                p->base_pts = p->state.duration > 0.0 ? p->state.duration : zm_player_clock_locked(p);
                p->state.ended = p->state.paused = 1;
            }
            SDL_UnlockMutex(p->mutex);
            SDL_Delay(10); continue;
        }
        if(d.ahead_first) {
            PacketNode *node=d.ahead_first; d.ahead_first=node->next;
            if(!d.ahead_first)d.ahead_last=NULL;
            d.ahead_bytes-=(size_t)node->packet.size; --d.ahead_count;
            av_packet_move_ref(d.packet,&node->packet); SDL_free(node); result=0;
        } else result=d.read_error?d.read_error:d.read_eof?AVERROR_EOF:av_read_frame(d.format,d.packet);
        if (result == AVERROR_EOF) {
            if (d.video) { result = decode_packet(&d, d.video, 1, NULL); if (result < 0) error_message(p, "Video flush", result); }
            if (d.audio && !cancelled(&d)) { result = decode_packet(&d, d.audio, 0, NULL); if (result < 0) error_message(p, "Audio flush", result); }
            d.eof = !cancelled(&d); continue;
        }
        if (result < 0) {
            if (!cancelled(&d)) { error_message(p, "Read media", result); d.eof = 1; }
            SDL_Delay(10); continue;
        }
        if (d.packet->stream_index == d.vi) result = decode_packet(&d, d.video, 1, d.packet);
        else if (d.packet->stream_index == d.ai) result = decode_packet(&d, d.audio, 0, d.packet);
        else result = 0;
        av_packet_unref(d.packet);
        if (result < 0 && !cancelled(&d)) {
            if (d.video && d.video->hw_device_ctx && !d.software_only) {
                avcodec_free_context(&d.video); d.software_only = 1;
                d.vi = open_codec(&d, AVMEDIA_TYPE_VIDEO, &d.video);
                if (d.vi >= 0) {
                    SDL_LockMutex(p->mutex); p->seek_target = zm_player_clock_locked(p); p->state.hardware_active = 0;
                    SDL_AtomicSet(&p->seek_pending,1); SDL_UnlockMutex(p->mutex);
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"GPU decode failed; falling back to software");
                } else { error_message(p,"Software fallback",d.vi); d.eof = 1; }
            } else { error_message(p, "Decode", result); d.eof = 1; }
        }
    }
    close_decoder(&d); return 0;
}
