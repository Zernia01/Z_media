#include "zm/subtitle.h"
#include "zm/app.h"
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <string.h>
void zm_subtitle_close(ZMSubtitleFilter *s) { avfilter_graph_free(&s->graph); memset(s, 0, sizeof(*s)); }
int zm_subtitle_open(ZMSubtitleFilter *s, const char *path, int stream, int width, int height, int size, int override_style) {
    AVFilterContext *sub;
    char args[256], style[256];
    int result;
    zm_subtitle_close(s);
    if (!path || !*path) return 0;
    s->graph = avfilter_graph_alloc();
    if (!s->graph) return AVERROR(ENOMEM);
    s->graph->nb_threads = 1;
    SDL_snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=1/1000:pixel_aspect=1/1", width, height, AV_PIX_FMT_YUV420P);
    result = avfilter_graph_create_filter(&s->source, avfilter_get_by_name("buffer"), "source", args, NULL, s->graph);
    if (result < 0) goto fail;
    sub = avfilter_graph_alloc_filter(s->graph, avfilter_get_by_name("subtitles"), "subtitles");
    if (!sub) { result = AVERROR_FILTER_NOT_FOUND; goto fail; }
    result = av_opt_set(sub, "filename", path, AV_OPT_SEARCH_CHILDREN);
    if (result < 0) goto fail;
    if (stream >= 0) {
        result = av_opt_set_int(sub, "stream_index", stream, AV_OPT_SEARCH_CHILDREN);
        if (result < 0) goto fail;
    }
    if (override_style) {
        SDL_snprintf(style, sizeof(style), "FontName=Malgun Gothic,FontSize=%d,Outline=2,Shadow=1,MarginV=24", size);
        result = av_opt_set(sub, "force_style", style, AV_OPT_SEARCH_CHILDREN);
        if (result < 0) goto fail;
    }
    result = avfilter_init_str(sub, NULL);
    if (result < 0) goto fail;
    result = avfilter_graph_create_filter(&s->sink, avfilter_get_by_name("buffersink"), "sink", NULL, NULL, s->graph);
    if (result < 0) goto fail;
    if ((result = avfilter_link(s->source, 0, sub, 0)) < 0 || (result = avfilter_link(sub, 0, s->sink, 0)) < 0 ||
        (result = avfilter_graph_config(s->graph, NULL)) < 0) goto fail;
    s->width = width; s->height = height; return 0;
fail:
    zm_subtitle_close(s); return result;
}
int zm_subtitle_apply(ZMSubtitleFilter *s, AVFrame *frame, double pts) {
    AVFrame *output;
    int result;
    if (!s->graph) return 0;
    frame->pts = (int64_t)(pts * 1000.0);
    result = av_buffersrc_add_frame_flags(s->source, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (result < 0) return result;
    output = av_frame_alloc();
    if (!output) return AVERROR(ENOMEM);
    result = av_buffersink_get_frame(s->sink, output);
    if (result >= 0) { av_frame_unref(frame); av_frame_move_ref(frame, output); }
    av_frame_free(&output); return result;
}
int zm_subtitle_find(const char *video_path, const char *language, char *result, size_t capacity) {
    static const char *extensions[] = {"srt", "ass", "ssa", "vtt"};
    const char *dot = strrchr(video_path, '.'), *slash = strrchr(video_path, '/'), *back = strrchr(video_path, '\\');
    size_t base;
    int i, pass;
    if (!dot || (slash && dot < slash) || (back && dot < back)) base = strlen(video_path);
    else base = (size_t)(dot - video_path);
    if (base + 24 >= capacity) return 0;
    for (pass = 0; pass < 3; ++pass) for (i = 0; i < 4; ++i) {
        FILE *file;
        memcpy(result, video_path, base); result[base] = '\0';
        if (pass == 0) SDL_snprintf(result + base, capacity - base, ".%s.%s", language, extensions[i]);
        else if (pass == 1) SDL_snprintf(result + base, capacity - base, ".%s.%s", !strcmp(language,"ko") ? "kor" : !strcmp(language,"en") ? "eng" : language, extensions[i]);
        else SDL_snprintf(result + base, capacity - base, ".%s", extensions[i]);
        file = zm_fopen_utf8(result, "rb");
        if (file) { fclose(file); return 1; }
    }
    result[0] = '\0'; return 0;
}
