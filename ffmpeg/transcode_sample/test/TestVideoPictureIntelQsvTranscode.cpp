#include <iostream>

using namespace std;

#include "StreamReader.h"
#include "StreamWriter.h"

#include "VideoTranscode.h"
#include "VideoPictureIntelQsvTranscode.h"

#define LOG_TAGS "app_main::"

#define LOGD(...) av_log(NULL, AV_LOG_DEBUG, __VA_ARGS__) //Log().d
#define LOGI(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)  //Log().i
#define LOGE(...) av_log(NULL, AV_LOG_ERROR, __VA_ARGS__) //Log().e

int main(int argc, char *argv[])
{
    cout << "transcode" << endl;
    av_log_set_level(AV_LOG_DEBUG);
    //av_log_set_level(AV_LOG_INFO);

    VideoPictureIntelQsvTranscode transcode;
    StreamReader reader;

    AVCodecParameters *videoCodecpar = nullptr;
    AVCodecParameters *audioCodecpar = nullptr;

    VideoDecodeParam decodeParam;
    VideoEncodeParam encodeParam;

    AVPacket *packet = nullptr;
    int main_loop = 0;
    int while_loop = 0;
MAIN_BEGIN:
    printf("main begin.\n");
    transcode.close();
    reader.close();

    reader.setDataSource("rtmp://172.17.6.106:1935/live/Test", "flv");
    if (reader.open() < 0)
    {
        goto MAIN_BEGIN;
    }

    decodeParam.setCodecId(AV_CODEC_ID_H264);
    transcode.setDecodeParam(decodeParam);

    encodeParam.setEncodeMode(VIDEO_ENCODE_PICTURE);
    encodeParam.setEncodeIFrame(false);
    encodeParam.setCodecId(AV_CODEC_ID_MJPEG);
    encodeParam.setBitrate(500);
    encodeParam.setVideoSize(VideoSize{352, 288});
    encodeParam.setFramerate(25);
    encodeParam.setIFrameInterval(12);

    encodeParam.setPicInterval(0);
    transcode.setEncodeParam(encodeParam);

    transcode.open();

    if (main_loop > 10)
    {
        goto MAIN_END;
    }

    while (true)
    {
        if (transcode.readyForReceive())
        {

            packet = av_packet_alloc();
            while (transcode.receivePacket(packet) == 0)
            {
                printf("get pic id:%d pts:%lld.\n", packet->stream_index, packet->pts);
                FILE *pTotalFile = fopen("preview.jpg", "wb");
                fwrite(packet->data, 1, packet->size, pTotalFile);
                fclose(pTotalFile);
                av_packet_unref(packet);
            }
            av_packet_free(&packet);
        }

        packet = av_packet_alloc();
        if (reader.readPacket(packet) == 0)
        {
            if (packet->stream_index == reader.getVideoStream()->index)
            {
                //printf("transcode::sendPacket\n");
                transcode.sendPacket(packet);
            }
        }
        av_packet_free(&packet);

        while_loop++;
        if (while_loop > 20)
        {
            while_loop = 0;
            main_loop++;
            goto MAIN_BEGIN;
        }
    }

MAIN_END:

    reader.close();
    transcode.close();

    return 0;
}