//
// Created by Jinzhu on 2020/5/8.
//

#ifndef STREAMWRITER_H
#define STREAMWRITER_H

#include <string>
#include <mutex>
#include <thread>

#define __STDC_CONSTANT_MACROS

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavutil/log.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
};
#endif

class StreamWriter
{
public:
    StreamWriter();

    virtual ~StreamWriter();

    /**
     * 获取类型
     * @return StreamWriter的类型
     */
    std::string getType();

    /**
     * 设置数据源
     * @param dataSource    数据源
     * @param format        输入格式
     */
    void setDataSource(std::string dataSource, std::string format);

    /**
     * 设置视频的codecpar
     * @param codecpar codecpar
     * @return 0:成功,<0:失败
     */
    int setVideoCodecPar(AVCodecParameters *codecpar);

    /**
     * 设置音频的codecpar
     * @param codecpar codecpar
     * @return 0:成功,<0:失败
     */
    int setAudioCodecPar(AVCodecParameters *codecpar);

    /**
     * 打开输入
     * @return 0:成功,<0:失败
     */
    int open();

    /**
     * 关闭输入
     */
    void close();

    /**
     * 写入一个数据帧
     * @param packet 数据帧
     * @return 0:成功,<0:失败
     */
    int writePacket(AVPacket *packet);

    /**
     * 获取音频的stream
     * @return stream
     */
    AVStream *getAudioStream();

    /**
     * 获取视频的stream
     * @return stream
     */
    AVStream *getVideoStream();

protected:
    /**
     * 超时回调函数
     * @param opaque this指针
     * @return
     */
    static int interrupt(void *opaque);

    bool m_IsOpen;
    std::string m_DataSource;
    std::string m_Format;

    AVCodecParameters *m_VideoCodecpar;
    AVCodecParameters *m_AudioCodecpar;

    AVFormatContext *m_OutputFmtCtx;
    int64_t m_LastAliveTime;

    const int BLOCK_TIME_OUT = 15000; //ms
};

#endif //STREAMWRITER_H
