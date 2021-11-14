//
// Created by Jinzhu on 2020/5/8.
//

#ifndef STREAMREADER_H
#define STREAMREADER_H

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

class StreamReader
{
public:
    StreamReader();

    virtual ~StreamReader();

    /**
     * 获取类型
     * @return StreamReader的类型
     */
    std::string getType();

    /**
     * 设置数据源
     * @param dataSource    数据源
     * @param format        输入格式
     */
    void setDataSource(std::string dataSource, std::string format);

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
     * 读取一个数据帧
     * @param packet 数据帧
     * @return 0:成功,<0:失败
     */
    int readPacket(AVPacket *packet);

    /**
     * 获取音频的codecpar
     * @param codecpar codecpar
     * @return 0:成功,<0:失败
     */
    int getAudioCodecPar(AVCodecParameters *codecpar);

    /**
     * 获取音频的stream
     * @return stream
     */
    AVStream *getAudioStream();

    /**
     * 获取视频的codecpar
     * @param codecpar codecpar
     * @return 0:成功,<0:失败
     */
    int getVideoCodecPar(AVCodecParameters *codecpar);

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

    AVBSFContext *m_VideoBSFCtx;
    AVFormatContext *m_InputFmtCtx;
    int64_t m_LastAliveTime;

    const int READ_TIME_OUT = 15000; //ms
};

#endif //STREAMREADER_H
