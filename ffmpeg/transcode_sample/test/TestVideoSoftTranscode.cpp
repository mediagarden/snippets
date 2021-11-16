#include <iostream>

using namespace std;

#include "StreamReader.h"
#include "StreamWriter.h"

#include "VideoTranscode.h"
#include "VideoSoftTranscode.h"
#include "VideoIntelQsvTranscode.h"

#define LOG_TAGS "app_main::"

#define LOGD(...) av_log(NULL, AV_LOG_DEBUG, __VA_ARGS__) //Log().d
#define LOGI(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)  //Log().i
#define LOGE(...) av_log(NULL, AV_LOG_ERROR, __VA_ARGS__) //Log().e

int main(int argc, char *argv[])
{
    cout << "transcode" << endl;
    if (argc != 3)
    {
        cout << "usage rtmp:// rtmp://" << endl;
        return 0;
    }
    std::string inputDataSource = argv[1];
    std::string outputDataSource = argv[2];
    //av_log_set_level(AV_LOG_DEBUG);
    av_log_set_level(AV_LOG_INFO);

    bool outputOpen;
    VideoSoftTranscode transcode;
    StreamWriter writer;
    StreamReader reader;

    AVCodecParameters *videoCodecpar = nullptr;
    AVCodecParameters *audioCodecpar = nullptr;

    VideoDecodeParam decodeParam;
    VideoEncodeParam encodeParam;

    AVPacket *packet = nullptr;

    reader.setDataSource(inputDataSource.c_str(), "flv");
    reader.open();

    decodeParam.setCodecId(AV_CODEC_ID_H264);
    transcode.setDecodeParam(decodeParam);

    encodeParam.setEncodeMode(VIDEO_ENCODE_STREAM);
    encodeParam.setEncodeIFrame(false);
    encodeParam.setCodecId(AV_CODEC_ID_H264);
    encodeParam.setBitrate(500);
    encodeParam.setVideoSize(VideoSize{352, 288});
    encodeParam.setFramerate(25);
    encodeParam.setIFrameInterval(12);

    encodeParam.setPicInterval(0);
    transcode.setEncodeParam(encodeParam);

    transcode.open();
    outputOpen = false;

    packet = av_packet_alloc();
    while (true)
    {
        if (transcode.readyForReceive())
        {
            if (!outputOpen)
            {
                writer.setDataSource(outputDataSource.c_str(), "flv");

                videoCodecpar = avcodec_parameters_alloc();
                audioCodecpar = avcodec_parameters_alloc();
                reader.getAudioCodecPar(audioCodecpar);
                transcode.getVideoCodecPar(videoCodecpar);

                writer.setAudioCodecPar(audioCodecpar);
                writer.setVideoCodecPar(videoCodecpar);

                writer.open();

                avcodec_parameters_free(&audioCodecpar);
                avcodec_parameters_free(&videoCodecpar);
                outputOpen = true;
            }

            if (outputOpen)
            {
                packet = av_packet_alloc();
                while (transcode.receivePacket(packet) == 0)
                {
                    packet->stream_index = writer.getVideoStream()->index;
                    //printf("push video id:%d dts:%lld pts:%lld.\n", packet->stream_index, packet->dts, packet->pts);
                    writer.writePacket(packet);
                    av_packet_unref(packet);
                }
                av_packet_free(&packet);
            }
        }
        packet = av_packet_alloc();
        if (reader.readPacket(packet) == 0)
        {
            if (packet->stream_index == reader.getVideoStream()->index)
            {
                //printf("transcode::sendPacket\n");
                transcode.sendPacket(packet);
            }
            else if (packet->stream_index == reader.getAudioStream()->index)
            {
                if (outputOpen)
                {
                    packet->stream_index = writer.getAudioStream()->index;
                    writer.writePacket(packet);
                }
            }
        }
        av_packet_free(&packet);
    }
    reader.close();
    transcode.close();
    writer.close();
    return 0;
}