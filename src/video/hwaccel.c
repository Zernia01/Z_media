#include "zm/hwaccel.h"
#include <libavutil/hwcontext.h>
static enum AVPixelFormat choose_format(AVCodecContext *codec, const enum AVPixelFormat *formats) {
    ZMHardware *hardware = codec->opaque;
    const enum AVPixelFormat *p;
    for (p = formats; *p != AV_PIX_FMT_NONE; ++p) if (*p == hardware->format) return *p;
    return avcodec_default_get_format(codec, formats);
}
int zm_hardware_attach(AVCodecContext *codec, ZMHardware *hardware) {
    int i;
    for (i = 0; ; ++i) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(codec->codec, i);
        if (!config) break;
#ifdef _WIN32
        if (config->device_type != AV_HWDEVICE_TYPE_D3D11VA) continue;
#else
        if (config->device_type != AV_HWDEVICE_TYPE_VAAPI) continue;
#endif
        if (!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) continue;
        if (av_hwdevice_ctx_create(&codec->hw_device_ctx, config->device_type, NULL, NULL, 0) < 0) continue;
        hardware->format = config->pix_fmt; codec->opaque = hardware; codec->get_format = choose_format;
        return 1;
    }
    return 0;
}
