#ifndef ZM_HWACCEL_H
#define ZM_HWACCEL_H
#include <libavcodec/avcodec.h>
typedef struct ZMHardware { enum AVPixelFormat format; } ZMHardware;
int zm_hardware_attach(AVCodecContext *codec, ZMHardware *hardware);
#endif
