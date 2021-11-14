//
// Created by Jinzhu on 2020/5/4.
//
#include <string>
#include <iostream>
#include <assert.h>

#include "StreamReader.h"

#define LOG_TAGS "StreamReader"

#define LOGD(...) av_log(NULL, AV_LOG_DEBUG, __VA_ARGS__) //Log().d
#define LOGI(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)  //Log().i
#define LOGE(...) av_log(NULL, AV_LOG_ERROR, __VA_ARGS__) //Log().e

StreamReader::StreamReader()
{
    m_IsOpen = false;
    m_LastAliveTime = 0;
    m_InputFmtCtx = nullptr;
    m_VideoBSFCtx = nullptr;

    m_DataSource = "";
    m_Format = "";
}

StreamReader::~StreamReader()
{
    close();
}

std::string StreamReader::getType()
{
    return "StreamReader";
}

void StreamReader::setDataSource(std::string dataSource, std::string format)
{
    m_DataSource = dataSource;
    m_Format = format;
}

int StreamReader::interrupt(void *opaque)
{
    StreamReader *pThis = (StreamReader *)opaque;
    if (av_gettime() - pThis->m_LastAliveTime > pThis->READ_TIME_OUT * 1000)
    {
        LOGE("StreamReader IO timeout.\n");
        return -1;
    }
    return 0;
}

int StreamReader::open()
{
    int ret = 0;
    size_t stream_index = 0;
    AVDictionary *opt = nullptr;
    AVInputFormat *format = nullptr;
    m_InputFmtCtx = avformat_alloc_context();
    if (m_InputFmtCtx == nullptr)
    {
        LOGE("Could not allocate m_InputFmtCtx.\n");
        goto ERROR_PROC;
    }

    m_LastAliveTime = av_gettime();
    //设置超时回调函数
    m_InputFmtCtx->interrupt_callback.callback = StreamReader::interrupt;
    m_InputFmtCtx->interrupt_callback.opaque = this;

    // 设置超时，udp:5秒 rtmp:15秒  rtsp:15秒
    if (m_DataSource.find("udp://") != std::string::npos)
    {
        av_dict_set(&opt, "timeout", std::to_string(5 * 1000000).c_str(), 0);
        av_dict_set(&opt, "buffer_size", "32000", 0);
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

    format = av_find_input_format(m_Format.c_str());
    if (format == nullptr)
    {
        LOGE("Could not find format:%s.\n", m_Format.c_str());
        goto ERROR_PROC;
    }

    ret = avformat_open_input(&m_InputFmtCtx, m_DataSource.c_str(), format, &opt);
    if (ret < 0)
    {
        LOGE("Could not open input file.\n");
        goto ERROR_PROC;
    }

    if ((ret = avformat_find_stream_info(m_InputFmtCtx, 0)) < 0)
    {
        LOGE("Failed to retrieve input stream information.\n");
        goto ERROR_PROC;
    }

    if (m_DataSource.find("rtmp") != std::string::npos)
    {
        /*h264有两种封装:
        一种是annexb模式:传统模式,有startcode,SPS和PPS是在ES中
        一种是mp4模式:一般mp4 mkv会有,没有startcode,SPS和PPS以及其它信息被封装在container中,每一个frame前面是这个frame的长度
        很多解码器只支持annexb这种模式，因此需要将mp4做转换.*/

        for (stream_index = 0; stream_index < m_InputFmtCtx->nb_streams; stream_index++)
        {
            if (m_InputFmtCtx->streams[stream_index]->codecpar->codec_id == AV_CODEC_ID_H264)
            {
                if ((ret = av_bsf_alloc(av_bsf_get_by_name("h264_mp4toannexb"), &m_VideoBSFCtx)) < 0)
                {
                    LOGE("Failed to alloc h264_mp4toannexb bsf.\n");
                    goto ERROR_PROC;
                }
                avcodec_parameters_copy(m_VideoBSFCtx->par_in, m_InputFmtCtx->streams[stream_index]->codecpar);
                LOGI("init h264_mp4toannexb bsf.\n");
                if ((ret = av_bsf_init(m_VideoBSFCtx)) < 0)
                {
                    LOGE("Failed to init h264_mp4toannexb bsf.\n");
                    goto ERROR_PROC;
                }
            }
            else if (m_InputFmtCtx->streams[stream_index]->codecpar->codec_id == AV_CODEC_ID_HEVC)
            {
                if ((ret = av_bsf_alloc(av_bsf_get_by_name("hevc_mp4toannexb"), &m_VideoBSFCtx)) < 0)
                {
                    LOGE("Failed to alloc hevc_mp4toannexb bsf.\n");
                    goto ERROR_PROC;
                }
                avcodec_parameters_copy(m_VideoBSFCtx->par_in, m_InputFmtCtx->streams[stream_index]->codecpar);
                LOGI("init hevc_mp4toannexb bsf.\n");
                if ((ret = av_bsf_init(m_VideoBSFCtx)) < 0)
                {
                    LOGE("Failed to init hevc_mp4toannexb bsf.\n");
                    goto ERROR_PROC;
                }
            }
        }
    }

    av_dump_format(m_InputFmtCtx, 0, m_DataSource.c_str(), 0);
    av_dict_free(&opt);
    LOGI("Open input file success.\n");
    m_IsOpen = true;
    return ret;
ERROR_PROC:
    av_bsf_free(&m_VideoBSFCtx);
    avformat_close_input(&m_InputFmtCtx);
    av_dict_free(&opt);
    m_IsOpen = false;
    return ret;
}

void StreamReader::close()
{
    if (m_IsOpen)
    {
        if (m_InputFmtCtx != nullptr)
        {
            av_bsf_free(&m_VideoBSFCtx);
            avformat_close_input(&m_InputFmtCtx);
            LOGI("Close input file.\n");
            m_IsOpen = false;
        }
    }
}

int StreamReader::readPacket(AVPacket *packet)
{
    if (m_IsOpen)
    {
        if (m_VideoBSFCtx != nullptr)
        {
            av_init_packet(packet);
            int ret = 0;
            if ((ret = av_bsf_receive_packet(m_VideoBSFCtx, packet)) == 0)
            {
                return ret;
            }
        }

        av_init_packet(packet);
        m_LastAliveTime = av_gettime();
        int ret = av_read_frame(m_InputFmtCtx, packet);
        size_t streamIndex = packet->stream_index;
        AVRational time_base = {1, 1000000};
        av_packet_rescale_ts(packet, m_InputFmtCtx->streams[streamIndex]->time_base, time_base);
        if (ret < 0)
        {
            LOGE("Failed to read packet error with:%d.\n", ret);
            return ret;
        }
        else
        {
            if (m_VideoBSFCtx != nullptr)
            {
                if (m_InputFmtCtx->streams[streamIndex]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
                {
                    if ((ret = av_bsf_send_packet(m_VideoBSFCtx, packet)) < 0)
                    {
                        LOGE("Failed to send packet to bsf.\n");
                        return ret;
                    }
                    return av_bsf_receive_packet(m_VideoBSFCtx, packet);
                }
            }
            return ret;
        }
    }
    return -1;
}

int StreamReader::getAudioCodecPar(AVCodecParameters *codecpar)
{
    if (m_IsOpen)
    {
        assert(codecpar != nullptr);
        for (size_t i = 0; i < m_InputFmtCtx->nb_streams; i++)
        {
            if (m_InputFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                return avcodec_parameters_copy(codecpar, m_InputFmtCtx->streams[i]->codecpar);
            }
        }
    }
    return AVERROR_INVALIDDATA;
}

AVStream *StreamReader::getAudioStream()
{
    if (m_IsOpen)
    {
        for (size_t i = 0; i < m_InputFmtCtx->nb_streams; i++)
        {
            if (m_InputFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                return m_InputFmtCtx->streams[i];
            }
        }
    }
    return nullptr;
}

int StreamReader::getVideoCodecPar(AVCodecParameters *codecpar)
{
    if (m_IsOpen)
    {
        for (size_t i = 0; i < m_InputFmtCtx->nb_streams; i++)
        {
            if (m_InputFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                return avcodec_parameters_copy(codecpar, m_InputFmtCtx->streams[i]->codecpar);
            }
        }
    }
    return AVERROR_INVALIDDATA;
}

AVStream *StreamReader::getVideoStream()
{
    if (m_IsOpen)
    {
        for (size_t i = 0; i < m_InputFmtCtx->nb_streams; i++)
        {
            if (m_InputFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                return m_InputFmtCtx->streams[i];
            }
        }
    }
    return nullptr;
}