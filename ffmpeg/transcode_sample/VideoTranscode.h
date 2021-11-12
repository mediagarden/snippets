//
// Created by Jinzhu on 2020/5/8.
//

#ifndef VIDEOTRANSCODE_H
#define VIDEOTRANSCODE_H

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

#define VIDEO_ENCODE_STEAM (0)
#define VIDEO_ENCODE_PICTURE (1)

struct VideoSize
{
    int Width;
    int Height;
};

/**
 * @brief 解码参数配置
 * 
 */
class VideoDecodeParam
{
public:
    VideoDecodeParam()
        : m_CodecId(AV_CODEC_ID_H264)
    {
    }

    virtual ~VideoDecodeParam()
    {
    }

    void setCodecId(AVCodecID codecId)
    {
        m_CodecId = codecId;
    }

    AVCodecID getCodecId()
    {
        return m_CodecId;
    }

private:
    AVCodecID m_CodecId;
};

/**
 * @brief 编码参数配置
 * 
 */
class VideoEncodeParam
{
public:
    VideoEncodeParam()
        : m_CodecId(AV_CODEC_ID_H264),
          m_Width(352),
          m_Height(288),
          m_Framerate(30),
          m_Bitrate(500),
          m_IFrameInterval(12),
          m_EncodeMode(VIDEO_ENCODE_STEAM),
          m_PicInterval(0)
    {
    }

    virtual ~VideoEncodeParam()
    {
    }

    void setCodecId(AVCodecID codecId)
    {
        m_CodecId = codecId;
    }

    AVCodecID getCodecId()
    {
        return m_CodecId;
    }

    void setVideoSize(struct VideoSize videoSize)
    {
        m_Width = videoSize.Width;
        m_Height = videoSize.Height;
    }

    struct VideoSize getVideoSize()
    {
        struct VideoSize videoSize;
        videoSize.Width = m_Width;
        videoSize.Height = m_Height;
        return videoSize;
    }

    void setFramerate(int framerate)
    {
        m_Framerate = framerate;
    }

    int getFramerate()
    {
        return m_Framerate;
    }

    void setBitrate(int bitrate)
    {
        m_Bitrate = bitrate;
    }

    int getBitrate()
    {
        return m_Bitrate;
    }

    void setIFrameInterval(int iFrameInterval)
    {
        m_IFrameInterval = iFrameInterval;
    }

    int getIFrameInterval()
    {
        return m_IFrameInterval;
    }

    void setEncodeMode(int encodeMode)
    {
        m_EncodeMode = encodeMode;
    }

    int getEncodeMode()
    {
        return m_EncodeMode;
    }

    void setPicInterval(int picInterval)
    {
        m_PicInterval = picInterval;
    }

    int getPicInterval()
    {
        return m_PicInterval;
    }

private:
    AVCodecID m_CodecId;
    int m_Width;
    int m_Height;
    int m_Framerate;
    int m_Bitrate;
    int m_IFrameInterval;
    int m_EncodeMode;
    int m_PicInterval;
};

class VideoTranscode
{

public:
    VideoTranscode() {}

    virtual ~VideoTranscode() {}

    void setDecodeParam(const VideoDecodeParam &param)
    {
        m_DecodeParam = param;
    }

    const VideoDecodeParam &getDecodeParam() const
    {
        return m_DecodeParam;
    }

    void setEncodeParam(const VideoEncodeParam &param)
    {
        m_EncodeParam = param;
    }

    const VideoEncodeParam &getEncodeParam() const
    {
        return m_EncodeParam;
    }

    virtual int open() = 0;

    virtual int close() = 0;

    virtual int reset() = 0;

    virtual int sendPacket(AVPacket *packet) = 0;

    virtual int receivePacket(AVPacket *packet) = 0;

protected:
    VideoDecodeParam m_DecodeParam;
    VideoEncodeParam m_EncodeParam;
};

#endif //VIDEOTRANSCODE_H
