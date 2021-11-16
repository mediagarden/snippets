//
// Created by Jinzhu on 2020/5/8.
//

#ifndef VIDEOTRANSCODE_H
#define VIDEOTRANSCODE_H

#include <string>
#include <mutex>
#include <chrono>
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

#define VIDEO_ENCODE_STREAM (0)
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
          m_EncodeMode(VIDEO_ENCODE_STREAM),
          m_PicInterval(0),
          m_EncodeIFrame(false)
    {
    }

    virtual ~VideoEncodeParam()
    {
    }

    void setEncodeMode(int encodeMode)
    {
        m_EncodeMode = encodeMode;
    }

    int getEncodeMode()
    {
        return m_EncodeMode;
    }

    void setEncodeIFrame(bool encodeIFrame)
    {
        m_EncodeIFrame = encodeIFrame;
    }

    bool getEncodeIFrame()
    {
        return m_EncodeIFrame;
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

    void setPicInterval(int picInterval)
    {
        m_PicInterval = picInterval;
    }

    int getPicInterval()
    {
        return m_PicInterval;
    }

private:
    int m_EncodeMode;    //转码类型:流,图片
    bool m_EncodeIFrame; //只转码I帧,仅对转图片有效
                         //转流的话帧率过低SRS不支持

    AVCodecID m_CodecId;
    int m_Width;
    int m_Height;
    int m_Framerate;
    int m_Bitrate;
    int m_IFrameInterval;

    int m_PicInterval; //图片转码输出间隔(ms)
};

class VideoFpsCalculater
{
public:
    VideoFpsCalculater();
    virtual ~VideoFpsCalculater();

    void onVideoFrame(uint64_t curFramePTS);

    int getFrameRate();

private:
    void calcFrameRate();

    void reset();

    int64_t m_s64LastRecvTime;     //上一次收流时间
    uint64_t m_u64LastFramePTS;    //上一帧的PTS
    uint64_t m_u64CurFramePTS;     //当前帧的PTS
    uint64_t m_u64FramePTSBase;    //帧率微调的基准PTS
    uint64_t m_u64LastPTSInterval; //上一帧的PTS间隔
    uint64_t m_u64CurPTSInterval;  //当前帧的PTS间隔
    uint32_t m_uFrameNum;          //帧率微调的帧计数
    uint32_t m_uFrameRate;         //当前帧率(根据PTS统计的帧率)
    uint32_t m_uFrameRateLastStat; //上一次统计的帧率

    uint32_t m_uFrameNumPerSec;                       //当前帧率(根据帧计数统计的帧率)
    uint32_t m_uFrameCount;                           //帧计数
    std::chrono::system_clock::time_point m_StatTime; //帧计数起始时间
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

    virtual int getVideoCodecPar(AVCodecParameters *codecpar) = 0;

    virtual bool readyForReceive() = 0;

    virtual int sendPacket(AVPacket *packet) = 0;

    virtual int receivePacket(AVPacket *packet) = 0;

protected:
    VideoDecodeParam m_DecodeParam;
    VideoEncodeParam m_EncodeParam;
};

#endif //VIDEOTRANSCODE_H
