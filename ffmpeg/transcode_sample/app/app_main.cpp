#include <iostream>

using namespace std;

#include "StreamReader.h"
#include "VideoTranscode.h"
#include "VideoSoftTranscode.h"

int main(int argc, char *argv[])
{
    cout << "transcode" << endl;

    av_log_set_level(AV_LOG_DEBUG);
    //av_log_set_level(AV_LOG_INFO);
    AVCodecParameters *videoCodecpar;
    AVCodecParameters *audioCodecpar;
    StreamReader reader;
    reader.setDataSource("rtmp://127.0.0.1:1935/live/Test", "flv");
    reader.open();

    VideoSoftTranscode transcode;

    VideoDecodeParam decodeParam;
    decodeParam.setCodecId(AV_CODEC_ID_H264);
    transcode.setDecodeParam(decodeParam);

    VideoEncodeParam encodeParam;
    encodeParam.setEncodeMode(VIDEO_ENCODE_STREAM);
    encodeParam.setEncodeIFrame(true);
    encodeParam.setCodecId(AV_CODEC_ID_HEVC);
    encodeParam.setBitrate(500);
    encodeParam.setVideoSize(VideoSize{352, 288});
    encodeParam.setFramerate(25);
    encodeParam.setIFrameInterval(12);

    encodeParam.setPicInterval(0);
    transcode.setEncodeParam(encodeParam);

    transcode.open();

    AVPacket *packet;
    packet = av_packet_alloc();

    FILE *pTotalFile = fopen("es_total.h265", "ab");

    while (true)
    {
        while (transcode.receivePacket(packet) == 0)
        {
            printf("push video id:%d dts:%lld pts:%lld.\n", packet->stream_index, packet->dts, packet->pts);
            fwrite(packet->data, 1, packet->size, pTotalFile);
            av_packet_unref(packet);
        }

        if (reader.readPacket(packet) == 0)
        {
            if (packet->stream_index == reader.getVideoStream()->index)
            {
                // packet->stream_index = writer.getVideoStream()->index;
                // printf("push video id:%d pts:%lld.\n", packet->stream_index, packet->pts);
                // writer.writePacket(packet);
                transcode.sendPacket(packet);
            }
        }

        av_packet_unref(packet);
    }

    fclose(pTotalFile);

    av_packet_free(&packet);
    transcode.close();
    reader.close();
    return 0;
}