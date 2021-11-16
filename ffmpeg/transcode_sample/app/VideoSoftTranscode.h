//
// Created by Jinzhu on 2021/11/11.
//

#ifndef VIDEO_SOFT_TRANSCODE_H
#define VIDEO_SOFT_TRANSCODE_H

#include <string>
#include <mutex>
#include <thread>
#include <cassert>

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
 * @brief 软件转码类
 * 
 */
class VideoSoftTranscode : public VideoTranscode
{
public:
    VideoSoftTranscode();
    virtual ~VideoSoftTranscode();

    int open();

    int close();

    int reset();

    int getVideoCodecPar(AVCodecParameters *codecpar);

    bool readyForReceive();

    int sendPacket(AVPacket *packet);

    int receivePacket(AVPacket *packet);

private: //functions
    int openDecoder();
    int closeDecoder();

    int openScaleFilter();
    int closeScaleFilter();

    int openEncoder();
    int closeEncoder();

    int updateEncoder();

private: //members
    bool m_ScaleFilterReady;
    int64_t m_LastEncodePts;
    AVCodecContext *m_DecodeCodecCtx;
    AVFilterGraph *m_ScaleFilterGraph;
    AVFilterContext *m_ScaleBuffersinkCtx;
    AVFilterContext *m_ScaleBuffersrcCtx;
    AVCodecContext *m_EncodeCodecCtx;
};

#endif //VIDEO_SOFT_TRANSCODE_H
