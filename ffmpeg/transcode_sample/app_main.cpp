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

    // av_log_set_level(AV_LOG_DEBUG);
    av_log_set_level(AV_LOG_INFO);
    AVCodecParameters *videoCodecpar;
    AVCodecParameters *audioCodecpar;
    StreamReader reader;
    reader.setDataSource("rtmp://172.17.6.106:1935/live/Test", "flv");
    reader.open();

    // StreamWriter writer;
    // writer.setDataSource("rtmp://172.17.6.106:1935/live/Test_cif", "flv");

    VideoSoftTranscode transcode;

    VideoDecodeParam decodeParam;
    decodeParam.setCodecId(AV_CODEC_ID_H264);
    transcode.setDecodeParam(decodeParam);

    VideoEncodeParam encodeParam;
    encodeParam.setCodecId(AV_CODEC_ID_HEVC);
    encodeParam.setBitrate(2000);
    encodeParam.setVideoSize(VideoSize{352, 288});
    encodeParam.setFramerate(25);
    encodeParam.setIFrameInterval(25);
    encodeParam.setEncodeMode(VIDEO_ENCODE_STEAM);
    transcode.setEncodeParam(encodeParam);

    transcode.open();

    // videoCodecpar = avcodec_parameters_alloc();
    // reader.getVideoCodecPar(videoCodecpar);
    // writer.setVideoCodecPar(videoCodecpar);

    audioCodecpar = avcodec_parameters_alloc();
    reader.getAudioCodecPar(audioCodecpar);
    //writer.setAudioCodecPar(audioCodecpar);

    //writer.open();

    AVPacket *packet;
    packet = av_packet_alloc();

    FILE *pTotalFile = fopen("es_total.h265", "ab");

    while (true)
    {
        av_init_packet(packet);
        if (reader.readPacket(packet) == 0)
        {
            if (packet->stream_index == reader.getVideoStream()->index)
            {
                // packet->stream_index = writer.getVideoStream()->index;
                // printf("push video id:%d pts:%lld.\n", packet->stream_index, packet->pts);
                // writer.writePacket(packet);
                transcode.sendPacket(packet);
                if (transcode.receivePacket(packet) == 0)
                {
                    // writer.writePacket(packet);
                    fwrite(packet->data, 1, packet->size, pTotalFile);
                }
            }
            else if (packet->stream_index == reader.getAudioStream()->index)
            {
                // packet->stream_index = writer.getAudioStream()->index;s
                // writer.writePacket(packet);
            }
        }

        av_packet_unref(packet);
    }

    fclose(pTotalFile);

    av_packet_free(&packet);
    avcodec_parameters_free(&videoCodecpar);
    avcodec_parameters_free(&audioCodecpar);

    transcode.close();
    // writer.close();
    reader.close();
    return 0;
}