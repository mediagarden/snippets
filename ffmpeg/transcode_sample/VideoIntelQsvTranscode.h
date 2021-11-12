//
// Created by Jinzhu on 2021/11/11.
//

#ifndef VIDEO_INTEL_QSV_TRANSCODE_H
#define VIDEO_INTEL_QSV_TRANSCODE_H

#include <string>
#include <mutex>
#include <thread>

#define __STDC_CONSTANT_MACROS

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavutil/log.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
};
#endif

#include "VideoTranscode.h"

/**
 * @brief Intel硬件转码类
 * 
 */
class VideoQsvTranscode : public VideoTranscode
{
public:
    VideoQsvTranscode();
    virtual ~VideoQsvTranscode();

    int open();

    int close();

    int reset();

    int sendPacket(AVPacket *packet);

    int receivePacket(AVPacket *packet);
};

#endif //VIDEO_INTEL_QSV_TRANSCODE_H
