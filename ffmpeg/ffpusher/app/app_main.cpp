#define __STDC_CONSTANT_MACROS

extern "C"
{
#include <libavutil/log.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <vector>
using namespace std;

#include "url_parser.h"

#ifndef SAFE_FREE
#define SAFE_FREE(p) \
    if (p)           \
    {                \
        av_free(p);  \
        p = NULL;    \
    }
#endif
#define MAX_ERR_CNT (20)

typedef struct CStreamPuller
{
    int m_ExitFlag; //退出标志

    int m_InputOpen;
    char *m_InputPath; //输入流地址
    char *m_InputAddr; //输入网口

    int m_InputVideoStream;
    int m_InputAudioStream;
    int64_t m_InputLastTime;
    AVFormatContext *m_InputFmtCtx;

    int m_OutputOpen;
    char *m_OutputPath; //输出流地址
    char *m_OutputAddr; //输出网口
    int m_OutputVideoStream;
    int m_OutputAudioStream;
    int m_OutputErrCnt; //写入错误次数
    int64_t m_OutputLastTime;
    AVFormatContext *m_OutputFmtCtx;
} CStreamPuller;

CStreamPuller *NewStreamPuller()
{
    CStreamPuller *puller = (CStreamPuller *)av_malloc(sizeof(CStreamPuller));
    memset(puller, 0, sizeof(CStreamPuller));

    puller->m_ExitFlag = 0;

    puller->m_InputOpen = 0;
    puller->m_InputPath = NULL;
    puller->m_InputAddr = 0;
    puller->m_InputVideoStream = -1;
    puller->m_InputAudioStream = -1;
    puller->m_InputLastTime = 0;
    puller->m_InputFmtCtx = NULL;

    puller->m_OutputOpen = 0;
    puller->m_OutputPath = NULL;
    puller->m_OutputAddr = NULL;
    puller->m_OutputVideoStream = -1;
    puller->m_OutputAudioStream = -1;
    puller->m_OutputErrCnt = 0;
    puller->m_OutputLastTime = 0;
    puller->m_OutputFmtCtx = NULL;

    return puller;
}

void DeleteStreamPuller(CStreamPuller **puller)
{
    SAFE_FREE((*puller)->m_InputPath);
    SAFE_FREE((*puller)->m_InputAddr);
    SAFE_FREE((*puller)->m_OutputPath);
    SAFE_FREE((*puller)->m_OutputAddr);
    SAFE_FREE(*puller);
    *puller = NULL;
}

void SetInputPath(CStreamPuller *puller, const char *inputPath, const char *inputAddr)
{
    if (inputPath != NULL)
    {
        puller->m_InputPath = strdup(inputPath);
    }

    if (inputAddr != NULL)
    {
        puller->m_InputAddr = strdup(inputAddr);
    }
}

void SetOutputPath(CStreamPuller *puller, const char *outputPath, const char *outputAddr)
{
    if (outputPath != NULL)
    {
        puller->m_OutputPath = strdup(outputPath);
    }

    if (outputAddr != NULL)
    {
        puller->m_OutputAddr = strdup(outputAddr);
    }
}

int inputInterrupt(void *opaque)
{
    CStreamPuller *puller = (CStreamPuller *)opaque;

    if (puller->m_ExitFlag)
    {
        printf("input stream exit.\n");
        return -1;
    }
    int64_t time = av_gettime();
    if (time < puller->m_InputLastTime)
    {
        puller->m_InputLastTime = time;
    }

    if (time - puller->m_InputLastTime > 10 * 1000000)
    {
        printf("StreamPuller input stream time out.\n");
        return -1;
    }

    return 0;
}

int outputInterrupt(void *opaque)
{
    CStreamPuller *puller = (CStreamPuller *)opaque;

    if (puller->m_ExitFlag)
    {
        printf("output stream exit.\n");
        return -1;
    }

    int64_t time = av_gettime();
    if (time < puller->m_OutputLastTime)
    {
        puller->m_OutputLastTime = time;
    }

    if (time - puller->m_OutputLastTime > 10 * 1000000)
    {
        printf("StreamPuller output stream time out.\n");
        return -1;
    }

    return 0;
}

int OpenInput(CStreamPuller *puller)
{
    printf("open input\n");

    if (puller->m_InputOpen)
    {
        printf("StreamPuller input is already opened.\n");
        return 0;
    }

    puller->m_InputFmtCtx = avformat_alloc_context();
    //设置超时回调函数
    puller->m_InputFmtCtx->interrupt_callback.callback = inputInterrupt;
    puller->m_InputFmtCtx->interrupt_callback.opaque = puller;

    AVDictionary *opt = NULL;
    // 设置超时，udp:10秒 rtsp:10秒
    if (puller->m_InputAddr != NULL)
    {
        av_dict_set(&opt, "localaddr", puller->m_InputAddr, 0);
    }

    if (strncmp(puller->m_InputPath, "udp://", strlen("udp://")) == 0)
    {
        av_dict_set(&opt, "timeout", "10000000", 0);
        av_dict_set(&opt, "buffer_size", "32000", 0);
    }
    else if (strncmp(puller->m_InputPath, "rtsp://", strlen("rtsp://")) == 0)
    {
        char option_key[] = "rtsp_transport";
        char option_value[] = "udp";
        av_dict_set(&opt, option_key, option_value, 0);
        av_dict_set(&opt, "stimeout", "10000000", 0);
        av_dict_set(&opt, "rtbufsize", "32000", 0);
    }

    puller->m_InputLastTime = av_gettime();
    int ret = avformat_open_input(&puller->m_InputFmtCtx, puller->m_InputPath, NULL, &opt);
    if (ret < 0)
    {
        printf("Could not open input file %d.\n", ret);
        goto ERROR_PROC;
    }

    ret = avformat_find_stream_info(puller->m_InputFmtCtx, 0);
    if (ret < 0)
    {
        printf("Failed to retrieve input stream information.\n");
        goto ERROR_PROC;
    }
    av_dump_format(puller->m_InputFmtCtx, 0, puller->m_InputPath, 0);

    puller->m_InputVideoStream = av_find_best_stream(puller->m_InputFmtCtx,
                                                     AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (puller->m_InputVideoStream < 0)
    {
        printf("Can not found Video Stream.\n");
    }

    puller->m_InputAudioStream = av_find_best_stream(puller->m_InputFmtCtx,
                                                     AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (puller->m_InputAudioStream < 0)
    {
        printf("Can not found Audio Stream.\n");
    }
    av_dict_free(&opt);
    printf("Open input file success.\n");
    puller->m_InputOpen = 1;
    return ret;
ERROR_PROC:
    avformat_close_input(&(puller->m_InputFmtCtx));
    av_dict_free(&opt);
    puller->m_InputOpen = 1;
    return ret;
}

int CloseInput(CStreamPuller *puller)
{
    printf("close input.\n");
    if (puller->m_InputOpen)
    {
        avformat_close_input(&(puller->m_InputFmtCtx));
    }
    puller->m_InputOpen = 0;
    return 0;
}

int OpenOutput(CStreamPuller *puller)
{
    AVDictionary *opt = NULL;

    const char *outformat = NULL;

    if (strncmp(puller->m_OutputPath, "udp://", strlen("udp://")) == 0)
    {
        outformat = "mpegts";
        av_dict_set(&opt, "localaddr", puller->m_OutputAddr, 0);
        av_dict_set(&opt, "pkt_size", "1316", 0); // UDP协议时，需要设置包大小
    }
    else if (strncmp(puller->m_OutputPath, "rtmp://", strlen("rtmp://")) == 0)
    {
        outformat = "flv";
    }

    int ret = avformat_alloc_output_context2(&puller->m_OutputFmtCtx, NULL, outformat, puller->m_OutputPath);
    if (ret < 0)
    {
        printf("Open output context failed.\n");
        goto ERROR_PROC;
    }
    //设置超时回调函数
    puller->m_OutputFmtCtx->interrupt_callback.callback = outputInterrupt;
    puller->m_OutputFmtCtx->interrupt_callback.opaque = puller;
    ret = avio_open2(&puller->m_OutputFmtCtx->pb, puller->m_OutputPath, AVIO_FLAG_WRITE, NULL, &opt);
    if (ret < 0)
    {
        printf("Open avio failed.\n");
        goto ERROR_PROC;
    }

    for (size_t index = 0; index < puller->m_InputFmtCtx->nb_streams; index++)
    {
        AVStream *stream = avformat_new_stream(puller->m_OutputFmtCtx, NULL);
        ret = avcodec_parameters_copy(stream->codecpar, puller->m_InputFmtCtx->streams[index]->codecpar);
        stream->codecpar->codec_tag = 0; // fix bug for "incompatible with output codec id" bh_lfs@2021.08.26

        if (ret < 0)
        {
            printf("Copy codec context failed.\n");
            goto ERROR_PROC;
        }

        if (index == puller->m_InputVideoStream)
        {
            puller->m_OutputVideoStream = stream->index;
        }

        if (index == puller->m_InputAudioStream)
        {
            puller->m_OutputAudioStream = stream->index;
        }
    }
    av_dump_format(puller->m_OutputFmtCtx, 0, puller->m_OutputPath, 1);
    ret = avformat_write_header(puller->m_OutputFmtCtx, NULL);
    if (ret < 0)
    {
        printf("Format write header failed.\n");
        goto ERROR_PROC;
    }
    puller->m_OutputErrCnt = 0;
    printf(" Open output file success.\n");
    puller->m_OutputOpen = 1;
    return ret;
ERROR_PROC:
    if (puller->m_OutputFmtCtx)
    {
        avformat_close_input(&puller->m_OutputFmtCtx);
    }
    puller->m_OutputOpen = 0;
    return ret;
}

int CloseOutput(CStreamPuller *puller)
{
    printf("close output.\n");
    if (puller->m_OutputOpen)
    {
        int ret = av_write_trailer(puller->m_OutputFmtCtx);
        if (ret < 0)
        {
            printf("Format write trailer failed.\n");
        }
        avformat_close_input(&puller->m_OutputFmtCtx);
    }
    puller->m_OutputOpen = 0;
    return 0;
}

static void NewPacket(CStreamPuller *puller, AVPacket **packet)
{
    *packet = av_packet_alloc();
}

static void DeletePacket(CStreamPuller *puller, AVPacket **packet)
{
    av_packet_free(packet);
}

static void UnrefPacket(CStreamPuller *puller, AVPacket *packet)
{
    av_packet_unref(packet);
}

int ReadPacketFromInput(CStreamPuller *puller, AVPacket *packet)
{
    av_init_packet(packet);
    puller->m_InputLastTime = av_gettime();
    return av_read_frame(puller->m_InputFmtCtx, packet);
}

void hexdump(uint8_t *buf, int len)
{
    int i = 0;

    printf("\n----------------------hexdump------------------------\n");
    for (i = 0; i < len; i++)
    {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0)
        {
            printf("\n");
        }
    }

    if (i % 16 != 0)
    {
        printf("\n");
    }

    printf("---------------------hexdump-------------------------\n\n");
}

// Rec. ITU-T H.264 (02/2014)
// Table 7-1 - NAL unit type codes, syntax element categories, and NAL unit type classes
#define H264_NAL_IDR 5            // Coded slice of an IDR picture
#define H264_NAL_SEI 6            // Supplemental enhancement information
#define H264_NAL_SPS 7            // Sequence parameter set
#define H264_NAL_PPS 8            // Picture parameter set
#define H264_NAL_AUD 9            // Access unit delimiter
#define H264_NAL_SPS_EXTENSION 13 // Access unit delimiter
#define H264_NAL_PREFIX 14        // Prefix NAL unit
#define H264_NAL_SPS_SUBSET 15    // Subset sequence parameter set
#define H264_NAL_XVC 20           // Coded slice extension(SVC/MVC)
#define H264_NAL_3D 21            // Coded slice extension for a depth view component or a 3D-AVC texture view component

static bool findStartCode3(uint8_t *buffer)
{
    return (buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 1);
}

static bool findStartCode4(uint8_t *buffer)
{
    return (buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 1);
}

static int findNalu(uint8_t *pInData, size_t nDataLen, uint8_t **pStartPos, uint8_t **pEndPos)
{
    int pos = 0;
    int nFoundCount = 0;
    int nRewind = 0;
    while (pos < nDataLen)
    {
        if (pos >= 3 && findStartCode3(&pInData[pos - 3]))
        {
            nFoundCount++;
            //找到第一个00 00 01:起始位置
            if (nFoundCount == 1)
            {
                *pStartPos = pInData + pos;
                if (pos >= 4 && findStartCode4(&pInData[pos - 4]))
                {
                    *pStartPos = pInData + pos; // pStartPos 指向 NALU Type
                }
            }

            //找到第二个00 00 01:结束位置
            if (nFoundCount == 2)
            {
                *pEndPos = pInData + pos - 3;
                if (findStartCode4(&pInData[pos - 4]))
                {
                    *pEndPos = pInData + pos - 4; // pEndPos 指向 00 00 00 01 的开头
                }
            }
        }

        if (nFoundCount == 2)
        {
            return *pEndPos - *pStartPos;
        }

        pos++;
    }

    //最后一个NALU单元
    if (nFoundCount == 1)
    {
        *pEndPos = pInData + nDataLen;
        return *pEndPos - *pStartPos;
    }
    else
    {
        return -1;
    }
}

static void FilterVideoPacket(AVPacket *packet_in, AVPacket *packet_out)
{
    if (0 != av_new_packet(packet_out, packet_in->size))
    {
        printf("Can not alloc packet.\n");
        return;
    }

    packet_out->pts = packet_in->pts;
    packet_out->dts = packet_in->dts;
    packet_out->size = 0;
    packet_out->stream_index = packet_in->stream_index;
    packet_out->flags = packet_in->flags;
    packet_out->duration = packet_in->duration;

    uint8_t *pInData = packet_in->data;
    size_t nDataLen = packet_in->size;

    uint8_t *pStartPos = packet_in->data;
    uint8_t *pEndPos = packet_in->data;

    const uint8_t startcode[] = {0, 0, 0, 1};
    const int startlen = 4;
    while (pInData + nDataLen - pEndPos > 0)
    {
        //找一个NALU单元
        uint8_t *pBegPos = pEndPos;
        size_t dataLeft = pInData + nDataLen - pBegPos;
        int length = findNalu(pBegPos, dataLeft, &pStartPos, &pEndPos);
        //printf("orginal\n");
        //hexdump(pBegPos, pEndPos - pBegPos + 4 + 4);
        //printf("found\n");
        //hexdump(pStartPos, length);

        //printf("length %d.\n", length);
        if (length < -1)
        {
            break;
        }
        //码流中存在这样的NALU
        //0000b886h: 00 00 00 01 00 00 00 00 01 09 F0                ; ..........?
        int nalutype = pStartPos[0] & 0x1f;
        if (H264_NAL_SEI != nalutype && 0 != nalutype )
        {
            //printf("copy  %d.\n", length);
            // av_hex_dump_log(NULL, AV_LOG_ERROR, startcode, startlen);
            memcpy(&packet_out->data[packet_out->size], startcode, startlen);
            packet_out->size += startlen;
            // av_hex_dump_log(NULL, AV_LOG_ERROR, pStartPos, length);
            memcpy(&packet_out->data[packet_out->size], pStartPos, length);
            packet_out->size += length;
        }
    };
}

int WritePacketToOutput(CStreamPuller *puller, AVPacket *packet)
{
    if (puller->m_InputVideoStream == packet->stream_index)
    {
        if (puller->m_OutputVideoStream >= 0)
        {
            AVStream *inputStream = puller->m_InputFmtCtx->streams[puller->m_InputVideoStream];
            AVStream *outputStream = puller->m_OutputFmtCtx->streams[puller->m_OutputVideoStream];
            av_packet_rescale_ts(packet, inputStream->time_base, outputStream->time_base);
            packet->stream_index = puller->m_OutputVideoStream;
            puller->m_OutputLastTime = av_gettime();
            AVPacket *pkt = NULL;
            pkt = av_packet_alloc();
            if (pkt == NULL)
            {
                printf("Can not alloc packet.\n");
                return AVERROR(ENOMEM);
            }
            FilterVideoPacket(packet, pkt); //过滤SEI,AUD
            int ret = av_interleaved_write_frame(puller->m_OutputFmtCtx, pkt);
            av_packet_free(&pkt);
            if (ret == 0)
            {
                return ret;
            }
            else if (ret == AVERROR(EINVAL) && puller->m_OutputErrCnt <= MAX_ERR_CNT)
            {
                puller->m_OutputErrCnt++;
                printf("override an EINVAL error for non monotonically increasing dts.\n");
                return 0;
            }
            else
            {
                printf("occurs an error %d.\n", ret);
                return ret;
            }
        }
    }

    if (puller->m_InputAudioStream == packet->stream_index)
    {
        if (puller->m_OutputAudioStream >= 0)
        {
            AVStream *inputStream = puller->m_InputFmtCtx->streams[puller->m_InputAudioStream];
            AVStream *outputStream = puller->m_OutputFmtCtx->streams[puller->m_OutputAudioStream];

            av_packet_rescale_ts(packet, inputStream->time_base, outputStream->time_base);
            packet->stream_index = puller->m_OutputAudioStream;
            puller->m_OutputLastTime = av_gettime();
            int ret = av_interleaved_write_frame(puller->m_OutputFmtCtx, packet);
            if (ret == 0)
            {
                return ret;
            }
            else if (ret == AVERROR(EINVAL) && puller->m_OutputErrCnt <= MAX_ERR_CNT)
            {
                puller->m_OutputErrCnt++;
                printf("override an EINVAL error for non monotonically increasing dts.\n");
                return 0;
            }
            else
            {
                printf("occurs an error %d.\n", ret);
                return ret;
            }
        }
    }

    return -1;
}

// #define TOOL_MIN(x, y) ((x) < (y) ? (x) : (y))

// static int doShell(const char *cmd, char *ret, int len)
// {
//     FILE *stream;
//     char buf[10240];
//     int rlen, wlen, s32Ret;

//     if (NULL == cmd)
//     {
//         return -1;
//     }

//     memset(buf, '\0', sizeof(buf));
//     stream = popen(cmd, "r");
//     if (NULL == stream)
//     {
//         printf("popen execute %s error.\n", cmd);
//         perror("popen");
//         return -1;
//     }

//     rlen = fread(buf, sizeof(char), sizeof(buf), stream);
//     wlen = TOOL_MIN(len, rlen);
//     if (ret != NULL)
//     {
//         memcpy(ret, buf, wlen);
//     }
//     s32Ret = pclose(stream);

//     return s32Ret;
// }

// static void DropStream(const std::string &rtmpUrl)
// {
//     URLParser::HTTP_URL url = URLParser::Parse(rtmpUrl);
//     cout << "scheme: " + url.scheme << endl;
//     cout << "host name: " + url.host << endl;
//     cout << "port: " + url.port << endl;
//     for (string path : url.path)
//     {
//         std::cout << "path:[" << path << "]" << std::endl;
//     }

//     cout << "param: " + url.query_string << endl;
//     for (std::pair<std::string, std::string> pair : url.query)
//     {
//         std::cout << "Query:[" << pair.first << "]:[" << pair.second << "]" << std::endl;
//     }

//     string appId = url.path[0];
//     string streamId = url.path[1];
//     char szDropCmd[2048];
//     memset(szDropCmd, 0, sizeof(szDropCmd));

//     sprintf(szDropCmd, "curl http://%s:10000/control/drop/publisher?app=%s&name=%s", url.host.c_str(),appId.c_str(), streamId.c_str());

//     cout << szDropCmd << endl;

//     system(szDropCmd);

//     return;
// }

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("usage: ffpusher input output.\n");
        return -1;
    }

    av_register_all();
    avformat_network_init();
    int ret = 0;

    const char *inputPath = argv[1];
    const char *outputPath = argv[2];

    CStreamPuller *puller = NewStreamPuller();

    AVPacket *packet = NULL;
    NewPacket(puller, &packet);

    SetInputPath(puller, inputPath, NULL);
    SetOutputPath(puller, outputPath, NULL);

    ret = OpenInput(puller);
    if (ret != 0)
    {
        printf("open input failed.\n");
        goto END_PROC;
    }

    ret = OpenOutput(puller);
    if (ret != 0)
    {
        printf("open output failed.\n");
        goto END_PROC;
    }

    while (true)
    {
        ret = ReadPacketFromInput(puller, packet);
        if (ret != 0)
        {
            printf("read packet failed.\n");
            goto END_PROC;
        }

        ret = WritePacketToOutput(puller, packet);
        if (ret != 0)
        {
            printf("write packet failed.\n");
            goto END_PROC;
        }
        UnrefPacket(puller, packet);
    }

END_PROC:
    ret = CloseOutput(puller);
    if (ret != 0)
    {
        printf("close output failed.\n");
    }

    ret = CloseInput(puller);
    if (ret != 0)
    {
        printf("close input failed.\n");
    }

    DeletePacket(puller, &packet);
    DeleteStreamPuller(&puller);

    return ret;
}
