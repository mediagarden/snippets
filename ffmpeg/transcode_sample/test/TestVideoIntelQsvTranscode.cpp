#include <iostream>

using namespace std;

#include "StreamReader.h"
#include "StreamWriter.h"

#include "VideoTranscode.h"
#include "VideoSoftTranscode.h"
#include "VideoIntelQsvTranscode.h"

int main(int argc, char *argv[])
{
    cout << "transcode" << endl;

    av_log_set_level(AV_LOG_DEBUG);
    //av_log_set_level(AV_LOG_INFO);
    AVCodecParameters *videoCodecpar;
    AVCodecParameters *audioCodecpar;

    videoCodecpar = avcodec_parameters_alloc();
    audioCodecpar = avcodec_parameters_alloc();

    StreamReader reader;
    reader.setDataSource("rtmp://172.17.6.106:1935/live/Test", "flv");
    reader.open();

    //VideoIntelQsvTranscode transcode;
    VideoSoftTranscode transcode;
    VideoDecodeParam decodeParam;
    decodeParam.setCodecId(AV_CODEC_ID_H264);
    transcode.setDecodeParam(decodeParam);

    VideoEncodeParam encodeParam;
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
    StreamWriter writer;
    bool init = false;

    AVPacket *packet;
    packet = av_packet_alloc();
    while (true)
    {
        if (transcode.readyForReceive())
        {
            if (!init)
            {
                writer.setDataSource("rtmp://172.17.6.106:1935/live/Test_cdif", "flv");

                reader.getAudioCodecPar(audioCodecpar);
                transcode.getVideoCodecPar(videoCodecpar);

                writer.setAudioCodecPar(audioCodecpar);
                writer.setVideoCodecPar(videoCodecpar);
                writer.open();
                init = true;
            }

            if (init)
            {

                while (transcode.receivePacket(packet) == 0)
                {
                    packet->stream_index = writer.getVideoStream()->index;
                    printf("push video id:%d dts:%lld pts:%lld.\n", packet->stream_index, packet->dts, packet->pts);
                    writer.writePacket(packet);
                    av_packet_unref(packet);
                }
            }
        }

        if (reader.readPacket(packet) == 0)
        {
            if (packet->stream_index == reader.getVideoStream()->index)
            {
                //printf("transcode::sendPacket\n");
                transcode.sendPacket(packet);
            }
            else if (packet->stream_index == reader.getAudioStream()->index)
            {
                if (init)
                {
                    packet->stream_index = writer.getAudioStream()->index;
                    writer.writePacket(packet);
                }
            }
        }

        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    avcodec_parameters_free(&audioCodecpar);
    avcodec_parameters_free(&videoCodecpar);
    reader.close();
    transcode.close();
    writer.close();
    return 0;
}