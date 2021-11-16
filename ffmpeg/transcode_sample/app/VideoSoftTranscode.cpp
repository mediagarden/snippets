#include "VideoSoftTranscode.h"

#define LOG_TAGS "VideoSoftTranscode::"

#define LOGD(...) av_log(NULL, AV_LOG_DEBUG, __VA_ARGS__) //Log().d
#define LOGI(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)  //Log().i
#define LOGE(...) av_log(NULL, AV_LOG_ERROR, __VA_ARGS__) //Log().e

static int get_interval_by_framerate(int framerate)
{
    if (framerate == 0)
    {
        return 0;
    }

    int interval = 1000000 / framerate;
    if (framerate == 60)
    {
        interval = 16667;
    }
    else if (framerate == 30)
    {
        interval = 33333;
    }
    return interval;
}

static int need_drop_by_pts(int64_t output_interval, int64_t last_pts, int64_t pts)
{
    //上一帧的PTS
    int64_t lastPTS = last_pts;
    //当前帧的PTS
    int64_t curPTS = pts;

    //上一帧的显示序号
    int64_t lastIndex = lastPTS / output_interval;
    //当前帧的显示序号
    int64_t curIndex = curPTS / output_interval;

    return (lastIndex == curIndex) ? 1 : 0;
}

static void avcodec_decoder_flush(AVCodecContext *dec_ctx)
{
    int ret = 0;
    AVFrame *frame = av_frame_alloc();
    assert(frame != nullptr);
    ret = avcodec_send_packet(dec_ctx, nullptr);
    if (ret < 0)
    {
        LOGE(LOG_TAGS "Error sending a packet for decoding.\n");
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            LOGE(LOG_TAGS "Error during decoding.\n");
            break;
        }
        av_frame_unref(frame);
    }
    av_frame_free(&frame);
}

static void avcodec_encoder_flush(AVCodecContext *enc_ctx)
{
    int ret = 0;
    AVPacket *packet = av_packet_alloc();
    assert(packet != nullptr);
    ret = avcodec_send_frame(enc_ctx, nullptr);
    if (ret < 0)
    {
        LOGE(LOG_TAGS "Error sending a frame for encoding.\n");
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_packet(enc_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            LOGE(LOG_TAGS "Error during encoding.\n");
            break;
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
}

VideoSoftTranscode::VideoSoftTranscode()
    : m_DecodeCodecCtx(nullptr),
      m_LastEncodePts(0),
      m_ScaleFilterReady(false),
      m_ScaleFilterGraph(nullptr),
      m_ScaleBuffersinkCtx(nullptr),
      m_ScaleBuffersrcCtx(nullptr),
      m_EncodeCodecCtx(nullptr)
{
}

VideoSoftTranscode::~VideoSoftTranscode()
{
    close();
}

int VideoSoftTranscode::open()
{
    int ret = 0;
    if ((ret = openEncoder()) < 0)
    {
        goto ERROR_PROC;
    }

    if ((ret = openDecoder()) < 0)
    {
        goto ERROR_PROC;
    }
    m_ScaleFilterReady = false;
    m_LastEncodePts = 0;
    return ret;
ERROR_PROC:
    closeDecoder();
    closeScaleFilter();
    closeEncoder();
    m_ScaleFilterReady = false;
    m_LastEncodePts = 0;
    return ret;
}

int VideoSoftTranscode::close()
{
    closeDecoder();
    closeScaleFilter();
    closeEncoder();
    m_ScaleFilterReady = false;
    m_LastEncodePts = 0;
    return 0;
}

int VideoSoftTranscode::reset()
{
    close();
    return open();
}

int VideoSoftTranscode::getVideoCodecPar(AVCodecParameters *codecpar)
{
    return avcodec_parameters_from_context(codecpar, m_EncodeCodecCtx);
}

bool VideoSoftTranscode::readyForReceive()
{
    return m_ScaleFilterReady;
}

int VideoSoftTranscode::sendPacket(AVPacket *packet)
{
    int ret = 0;
    bool bEncodeIFrame = m_EncodeParam.getEncodeIFrame();
    if (bEncodeIFrame)
    {
        if (!(packet->flags & AV_PKT_FLAG_KEY))
        {
            ret = AVERROR(EAGAIN);
            return ret;
        }
    }
    int outputFramerate = m_EncodeParam.getFramerate();
    ret = avcodec_send_packet(m_DecodeCodecCtx, packet);
    if (ret < 0)
    {
        LOGE(LOG_TAGS "Error sending a packet for decoding.\n");
        return ret;
    }
    AVFrame *frame;
    AVFrame *filter_frame;
    frame = av_frame_alloc();
    assert(frame != nullptr);
    filter_frame = av_frame_alloc();
    assert(filter_frame != nullptr);

    while (ret >= 0)
    {
        ret = avcodec_receive_frame(m_DecodeCodecCtx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return ret;
        }
        else if (ret < 0)
        {
            LOGE(LOG_TAGS "Error during decoding.\n");
            return ret;
        }

        if (!m_ScaleFilterReady)
        {
            if ((ret = openScaleFilter()) < 0)
            {
                LOGE(LOG_TAGS "Open scale filter failed.\n");
                return ret;
            }
            LOGI(LOG_TAGS "Open scale filter success.\n");
            m_ScaleFilterReady = true;
        }

        //对视频进行抽帧
        int64_t outputInterval = 0;
        if (outputFramerate > 0)
        {
            //需要抽帧
            outputInterval = get_interval_by_framerate(outputFramerate);
        }

        if ((outputInterval == 0) || (!need_drop_by_pts(outputInterval, m_LastEncodePts, frame->pts)))
        {
            m_LastEncodePts = frame->pts;
            /* push the decoded frame into the filtergraph */
            if (av_buffersrc_add_frame_flags(m_ScaleBuffersrcCtx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
            {
                LOGE(LOG_TAGS "Error while feeding the filtergraph.\n");
                break;
            }
        }

        /* pull filtered frames from the filtergraph */
        while (1)
        {
            ret = av_buffersink_get_frame(m_ScaleBuffersinkCtx, filter_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            if (ret < 0)
            {
                LOGE(LOG_TAGS "Error while receive from the filtergraph.\n");
                break;
            }

            //送编码的帧,不需要指定帧类型
            filter_frame->pict_type = AV_PICTURE_TYPE_NONE;
            //如果只送I帧,则编为I帧
            if (bEncodeIFrame)
            {
                filter_frame->pict_type = AV_PICTURE_TYPE_I;
            }
            if ((ret = avcodec_send_frame(m_EncodeCodecCtx, filter_frame)) < 0)
            {
                LOGE(LOG_TAGS "Error sending a packet for decoding.\n");
                break;
            }
            av_frame_unref(filter_frame);
        }
        av_frame_unref(frame);
    }
    av_frame_free(&filter_frame);
    av_frame_free(&frame);
    return ret;
}

int VideoSoftTranscode::receivePacket(AVPacket *packet)
{
    int ret = 0;
    if (!readyForReceive())
    {
        return -1;
    }
    ret = avcodec_receive_packet(m_EncodeCodecCtx, packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        //try again;
    }
    else if (ret < 0)
    {
        LOGE(LOG_TAGS "Error during encoding.\n");
    }
    return ret;
}

int VideoSoftTranscode::openDecoder()
{
    int ret = 0;
    AVCodecID codecId;
    const AVCodec *codec = nullptr;
    codecId = m_DecodeParam.getCodecId();
    if (codecId == AV_CODEC_ID_H264)
    {
        codec = avcodec_find_decoder_by_name("libx264");
    }
    else if (codecId == AV_CODEC_ID_HEVC)
    {
        codec = avcodec_find_decoder_by_name("libx265");
    }

    if (codec == nullptr)
    {
        codec = avcodec_find_decoder(codecId);
    }

    if (codec == nullptr)
    {
        LOGE(LOG_TAGS "Could not find decoder for %d.\n", codecId);
        ret = AVERROR(EINVAL);
        goto ERR_PROC;
    }

    m_DecodeCodecCtx = avcodec_alloc_context3(codec);
    if (m_DecodeCodecCtx == nullptr)
    {
        LOGE(LOG_TAGS "Could not allocate video codec context.\n");
        ret = AVERROR(ENOMEM);
        goto ERR_PROC;
    }

    /* open it */
    if ((ret = avcodec_open2(m_DecodeCodecCtx, codec, NULL)) < 0)
    {
        LOGE(LOG_TAGS "Could not open decoder codec.\n");
        goto ERR_PROC;
    }
    LOGI(LOG_TAGS "Open decoder codec success.\n");
    return 0;
ERR_PROC:
    return ret;
}

int VideoSoftTranscode::closeDecoder()
{
    /* flush the decoder */
    if (m_DecodeCodecCtx != nullptr)
    {
        avcodec_decoder_flush(m_DecodeCodecCtx);
    }
    avcodec_free_context(&m_DecodeCodecCtx);
    return 0;
}

int VideoSoftTranscode::openScaleFilter()
{
    char args[512];
    int ret = 0;
    int encWidth = m_EncodeParam.getVideoSize().Width;
    int encHeight = m_EncodeParam.getVideoSize().Height;
    int framerate = m_EncodeParam.getFramerate();
    AVRational time_base = (AVRational){1, framerate};
    AVRational sample_aspect_ratio = m_DecodeCodecCtx->sample_aspect_ratio;

    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");

    AVFilterInOut *outputs = avfilter_inout_alloc();
    assert(outputs != nullptr);
    AVFilterInOut *inputs = avfilter_inout_alloc();
    assert(inputs != nullptr);

    m_ScaleFilterGraph = avfilter_graph_alloc();
    if (m_ScaleFilterGraph == nullptr)
    {
        ret = AVERROR(ENOMEM);
        LOGE(LOG_TAGS "Could not allocate video filter graph.\n");
        goto ERR_PROC;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             m_DecodeCodecCtx->width,
             m_DecodeCodecCtx->height,
             m_DecodeCodecCtx->pix_fmt,
             time_base.num,
             time_base.den,
             sample_aspect_ratio.num,
             sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&m_ScaleBuffersrcCtx, buffersrc, "in",
                                       args, NULL, m_ScaleFilterGraph);
    if (ret < 0)
    {
        LOGE(LOG_TAGS "Cannot create buffer source.\n");
        goto ERR_PROC;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&m_ScaleBuffersinkCtx, buffersink, "out",
                                       NULL, NULL, m_ScaleFilterGraph);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink.\n");
        goto ERR_PROC;
    }

    ret = av_opt_set_int_list(m_ScaleBuffersinkCtx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format.\n");
        goto ERR_PROC;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = m_ScaleBuffersrcCtx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name = av_strdup("out");
    inputs->filter_ctx = m_ScaleBuffersinkCtx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    snprintf(args, sizeof(args), "scale=%d:%d", encWidth, encHeight);
    if ((ret = avfilter_graph_parse_ptr(m_ScaleFilterGraph, args,
                                        &inputs, &outputs, NULL)) < 0)
    {
        LOGE(LOG_TAGS "Failed to set avfilter_graph_parse_ptr.\n");
        goto ERR_PROC;
    }

    if ((ret = avfilter_graph_config(m_ScaleFilterGraph, NULL)) < 0)
    {
        LOGE(LOG_TAGS "Failed to set avfilter_graph_config.\n");
        goto ERR_PROC;
    }
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return ret;
ERR_PROC:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    m_ScaleBuffersinkCtx = nullptr;
    m_ScaleBuffersrcCtx = nullptr;
    avfilter_graph_free(&m_ScaleFilterGraph);
    return ret;
}

int VideoSoftTranscode::closeScaleFilter()
{
    m_ScaleBuffersinkCtx = nullptr;
    m_ScaleBuffersrcCtx = nullptr;
    avfilter_graph_free(&m_ScaleFilterGraph);
    return 0;
}

int VideoSoftTranscode::openEncoder()
{
    int ret = 0;

    AVCodecID codecId;
    AVDictionary *opt = nullptr;
    const AVCodec *codec = nullptr;
    codecId = m_EncodeParam.getCodecId();
    if (codecId == AV_CODEC_ID_H264)
    {
        codec = avcodec_find_encoder_by_name("libx264");
    }
    else if (codecId == AV_CODEC_ID_HEVC)
    {
        codec = avcodec_find_encoder_by_name("libx265");
    }

    if (codec == nullptr)
    {
        codec = avcodec_find_encoder(codecId);
    }

    if (codec == nullptr)
    {
        LOGE(LOG_TAGS "Could not find encoder for %d.\n", codecId);
        ret = AVERROR(EINVAL);
        goto ERR_PROC;
    }

    m_EncodeCodecCtx = avcodec_alloc_context3(codec);
    if (m_EncodeCodecCtx == nullptr)
    {
        LOGE(LOG_TAGS "Could not allocate video codec context.\n");
        ret = AVERROR(ENOMEM);
        goto ERR_PROC;
    }

    updateEncoder();

    //编码速度
    av_dict_set(&opt, "preset", "veryfast", 0);
    //零延时编码
    av_dict_set(&opt, "tune", "zerolatency", 0);

    /* open it */
    if ((ret = avcodec_open2(m_EncodeCodecCtx, codec, &opt)) < 0)
    {
        LOGE(LOG_TAGS "Could not open encoder codec.\n");
        goto ERR_PROC;
    }

    LOGI(LOG_TAGS "Open encoder codec success.\n");
    return 0;
ERR_PROC:
    return ret;
}

int VideoSoftTranscode::closeEncoder()
{
    /* flush the encoder */
    if (m_EncodeCodecCtx != nullptr)
    {
        avcodec_encoder_flush(m_EncodeCodecCtx);
    }
    avcodec_free_context(&m_EncodeCodecCtx);
    return 0;
}

int VideoSoftTranscode::updateEncoder()
{
    int bitrate = m_EncodeParam.getBitrate();
    int width = m_EncodeParam.getVideoSize().Width;
    int height = m_EncodeParam.getVideoSize().Height;
    int framerate = m_EncodeParam.getFramerate();
    int iFrameInterval = m_EncodeParam.getIFrameInterval();
    bool bEncodeIFrame = m_EncodeParam.getEncodeIFrame();

    if (framerate <= 0)
    {
        framerate = 25;
    }

    if (iFrameInterval <= 0)
    {
        iFrameInterval = 60;
    }
    /* put sample parameters */
    m_EncodeCodecCtx->bit_rate = bitrate * 1000;
    /* resolution must be a multiple of two */
    m_EncodeCodecCtx->width = width;
    m_EncodeCodecCtx->height = height;
    /* frames per second */
    m_EncodeCodecCtx->time_base = (AVRational){1, framerate};
    m_EncodeCodecCtx->framerate = (AVRational){framerate, 1};

    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    m_EncodeCodecCtx->gop_size = bEncodeIFrame ? 1 : iFrameInterval;
    m_EncodeCodecCtx->max_b_frames = 0;
    m_EncodeCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    //编码器输出extradata
    m_EncodeCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    //TODO:add flags
    return 0;
}