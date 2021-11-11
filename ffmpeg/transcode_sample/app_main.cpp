#include <iostream>

using namespace std;

#include "StreamReader.h"
#include "StreamWriter.h"

int main(int argc, char *argv[])
{
    cout << "transcode" << endl;

    av_log_set_level(AV_LOG_DEBUG);

    AVCodecParameters *videoCodecpar;
    AVCodecParameters *audioCodecpar;
    StreamReader reader;
    reader.setDataSource("rtmp://172.17.6.106:1935/live/Test", "flv");
    reader.open();

    StreamWriter writer;
    writer.setDataSource("rtmp://172.17.6.106:1935/live/Test_cif", "flv");
    videoCodecpar = avcodec_parameters_alloc();
    reader.getVideoCodecPar(videoCodecpar);
    writer.setVideoCodecPar(videoCodecpar);

    audioCodecpar = avcodec_parameters_alloc();
    reader.getAudioCodecPar(audioCodecpar);
    writer.setAudioCodecPar(audioCodecpar);

    writer.open();

    AVPacket *packet;
    packet = av_packet_alloc();

    while (true)
    {
        av_init_packet(packet);
        if (reader.readPacket(packet) == 0)
        {
            if (packet->stream_index == reader.getVideoStream()->index)
            {
                packet->stream_index = writer.getVideoStream()->index;
                printf("push video id:%d pts:%lld.\n", packet->stream_index, packet->pts);
                writer.writePacket(packet);
            }
            else if (packet->stream_index == reader.getAudioStream()->index)
            {
                packet->stream_index = writer.getAudioStream()->index;
                printf("push audio id:%d pts:%lld.\n", packet->stream_index, packet->pts);
                writer.writePacket(packet);
            }
        }

        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    avcodec_parameters_free(&videoCodecpar);
    avcodec_parameters_free(&audioCodecpar);
    writer.close();

    reader.close();
    return 0;
}