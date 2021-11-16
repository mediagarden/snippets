//
// Created by Jinzhu on 2020/5/4.
//
#include <string>
#include <iostream>
#include <assert.h>

#include "StreamWriter.h"

#define LOG_TAGS "StreamWriter"

#define LOGD(...) av_log(NULL, AV_LOG_DEBUG, __VA_ARGS__) //Log().d
#define LOGI(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)  //Log().i
#define LOGE(...) av_log(NULL, AV_LOG_ERROR, __VA_ARGS__) //Log().e


StreamWriter::StreamWriter()
{
    m_IsOpen = false;
    m_LastAliveTime = 0;
    m_OutputFmtCtx = nullptr;

    m_VideoCodecpar = nullptr;
    m_AudioCodecpar = nullptr;

    m_DataSource = "";
    m_Format = "";
}

StreamWriter::~StreamWriter()
{
    close();

    avcodec_parameters_free(&m_VideoCodecpar);
    avcodec_parameters_free(&m_AudioCodecpar);
}

std::string StreamWriter::getType()
{
    return "StreamWriter";
}

void StreamWriter::setDataSource(std::string dataSource, std::string format)
{
    m_DataSource = dataSource;
    m_Format = format;
}

int StreamWriter::setAudioCodecPar(AVCodecParameters *codecpar)
{
    if (m_AudioCodecpar == nullptr)
    {
        m_AudioCodecpar = avcodec_parameters_alloc();
        assert(m_AudioCodecpar != nullptr);
    }
    return avcodec_parameters_copy(m_AudioCodecpar, codecpar);
}

int StreamWriter::setVideoCodecPar(AVCodecParameters *codecpar)
{
    if (m_VideoCodecpar == nullptr)
    {
        m_VideoCodecpar = avcodec_parameters_alloc();
        assert(m_VideoCodecpar != nullptr);
    }
    return avcodec_parameters_copy(m_VideoCodecpar, codecpar);
}

int StreamWriter::interrupt(void *opaque)
{
    StreamWriter *pThis = (StreamWriter *)opaque;
    if (av_gettime() - pThis->m_LastAliveTime > pThis->BLOCK_TIME_OUT * 1000)
    {
        LOGE("StreamWriter IO timeout.\n");
        return -1;
    }
    return 0;
}

int StreamWriter::open()
{
    int ret = 0;
    int stream_index = 0;
    AVDictionary *opt = nullptr;
    ret = avformat_alloc_output_context2(&m_OutputFmtCtx,
                                         nullptr,
                                         m_Format.empty() ? nullptr : m_Format.c_str(),
                                         m_DataSource.c_str());
    if (ret < 0)
    {
        LOGE("Open output context failed.\n");
        goto ERROR_PROC;
    }

    // 设置超时，udp:5秒 rtmp:15秒  rtsp:15秒
    if (m_DataSource.find("udp://") != std::string::npos)
    {
        av_dict_set(&opt, "timeout", std::to_string(5 * 1000000).c_str(), 0);
        av_dict_set(&opt, "buffer_size", "32000", 0);
        av_dict_set(&opt, "pkt_size", "1316", 0);
    }
    else if (m_DataSource.find("rtsp://") != std::string::npos)
    {
        char option_key[] = "rtsp_transport";
        char option_value[] = "tcp";
        av_dict_set(&opt, option_key, option_value, 0);
        av_dict_set(&opt, "stimeout", std::to_string(15 * 1000000).c_str(), 0);
    }
    else if (m_DataSource.find("rtmp://") != std::string::npos)
    {
        av_dict_set(&opt, "stimeout", std::to_string(15 * 1000000).c_str(), 0);
    }

    m_LastAliveTime = av_gettime();
    //设置超时回调函数
    m_OutputFmtCtx->interrupt_callback.callback = StreamWriter::interrupt;
    m_OutputFmtCtx->interrupt_callback.opaque = this;
    ret = avio_open2(&m_OutputFmtCtx->pb, m_DataSource.c_str(), AVIO_FLAG_WRITE, nullptr, &opt);
    if (ret < 0)
    {
        LOGE("Open avio failed.\n");
        goto ERROR_PROC;
    }

    if (m_VideoCodecpar != nullptr)
    {
        AVStream *stream = avformat_new_stream(m_OutputFmtCtx, nullptr);
        ret = avcodec_parameters_copy(stream->codecpar, m_VideoCodecpar);
        if (ret < 0)
        {
            LOGE("Copy codec context failed.\n");
            goto ERROR_PROC;
        }
        stream_index++;
    }

    if (m_AudioCodecpar != nullptr)
    {
        AVStream *stream = avformat_new_stream(m_OutputFmtCtx, nullptr);
        ret = avcodec_parameters_copy(stream->codecpar, m_AudioCodecpar);
        if (ret < 0)
        {
            LOGE("Copy codec context failed.\n");
            goto ERROR_PROC;
        }
        stream_index++;
    }

    ret = avformat_write_header(m_OutputFmtCtx, nullptr);
    if (ret < 0)
    {
        LOGE("Format write header failed.\n");
        goto ERROR_PROC;
    }

    av_dump_format(m_OutputFmtCtx, 0, m_DataSource.c_str(), 1);
    av_dict_free(&opt);
    LOGI("Open output file success.\n");
    m_IsOpen = true;
    return ret;
ERROR_PROC:
    avformat_close_input(&m_OutputFmtCtx);
    av_dict_free(&opt);
    m_IsOpen = false;
    return ret;
}

void StreamWriter::close()
{
    if (m_IsOpen)
    {
        if (m_OutputFmtCtx != nullptr)
        {
            av_write_trailer(m_OutputFmtCtx);
            avformat_close_input(&m_OutputFmtCtx);
            LOGI("Close output file.\n");
            m_IsOpen = false;
        }
    }
}

int StreamWriter::writePacket(AVPacket *packet)
{
    if (m_IsOpen)
    {
        m_LastAliveTime = av_gettime();
        size_t stream_index = packet->stream_index;
        AVRational time_base = {1, 1000000};
        av_packet_rescale_ts(packet, time_base, m_OutputFmtCtx->streams[stream_index]->time_base);
        int ret = av_write_frame(m_OutputFmtCtx, packet);
        if (ret < 0)
        {
            LOGE("Failed to write packet error with %d.\n", ret);
            return ret;
        }
    }
    return -1;
}

AVStream *StreamWriter::getAudioStream()
{
    if (m_IsOpen)
    {
        for (size_t i = 0; i < m_OutputFmtCtx->nb_streams; i++)
        {
            if (m_OutputFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                return m_OutputFmtCtx->streams[i];
            }
        }
    }
    return nullptr;
}

AVStream *StreamWriter::getVideoStream()
{
    if (m_IsOpen)
    {
        for (size_t i = 0; i < m_OutputFmtCtx->nb_streams; i++)
        {
            if (m_OutputFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                return m_OutputFmtCtx->streams[i];
            }
        }
    }
    return nullptr;
}