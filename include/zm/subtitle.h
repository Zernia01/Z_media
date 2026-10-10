#ifndef ZM_SUBTITLE_H
#define ZM_SUBTITLE_H
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>
typedef struct ZMSubtitleFilter {
    AVFilterGraph *graph;
    AVFilterContext *source, *sink;
    int width, height;
} ZMSubtitleFilter;
void zm_subtitle_close(ZMSubtitleFilter *s);
int zm_subtitle_open(ZMSubtitleFilter *s, const char *path, int stream, int width, int height, int size, int override_style);
int zm_subtitle_apply(ZMSubtitleFilter *s, AVFrame *frame, double pts);
int zm_subtitle_find(const char *video_path, const char *language, char *result, size_t capacity);
#endif
