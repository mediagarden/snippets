#include "VideoTranscode.h"

#define LOG_TAGS "VideoTranscode::"

#define LOGD(...) av_log(NULL, AV_LOG_DEBUG, __VA_ARGS__) //Log().d
#define LOGI(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)  //Log().i
#define LOGE(...) av_log(NULL, AV_LOG_ERROR, __VA_ARGS__) //Log().e

VideoFpsCalculater::VideoFpsCalculater()
{
    reset();
}

VideoFpsCalculater::~VideoFpsCalculater()
{
}

void VideoFpsCalculater::reset()
{
    m_s64LastRecvTime = 0;
    m_u64LastFramePTS = 0;
    m_u64CurFramePTS = 0;
    m_u64FramePTSBase = 0;
    m_u64LastPTSInterval = 0;
    m_u64CurPTSInterval = 0;
    m_uFrameNum = 0;

    m_uFrameRate = 0;
    m_uFrameRateLastStat = 0;
    m_uFrameNumPerSec = 0;
    m_uFrameCount = 0;
    m_StatTime = std::chrono::system_clock::from_time_t(0);
}

void VideoFpsCalculater::onVideoFrame(uint64_t curFramePTS)
{
    m_u64CurFramePTS = curFramePTS;

    bool bStatsReset = false;
    int64_t s64RecvTime = av_gettime();
    if ((m_s64LastRecvTime != 0) && std::abs(s64RecvTime - m_s64LastRecvTime) > 5 * 1000 * 1000)
    {
        bStatsReset = true;
    }

    if (bStatsReset)
    {
        reset();
    }

    m_s64LastRecvTime = s64RecvTime;
    if ((m_u64CurFramePTS != m_u64LastFramePTS) && (m_u64CurFramePTS != 0))
    {
        calcFrameRate();
        getFrameRate();
    }
}

void VideoFpsCalculater::calcFrameRate()
{
    //根据单位时间收到的帧,计算帧率
    if (m_StatTime != std::chrono::system_clock::from_time_t(0))
    {
        std::chrono::system_clock::time_point nowTime = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - m_StatTime);
        if (duration.count() > 5000)
        {
            m_uFrameNumPerSec = (m_uFrameCount * 1000) / duration.count();
            m_uFrameCount = 0;
            m_StatTime = nowTime;
        }
    }

    //根据PTS计算帧率
    {
        if ((m_u64LastFramePTS != 0) && (m_u64CurFramePTS > m_u64LastFramePTS))
        {
            m_u64CurPTSInterval = (m_u64CurFramePTS - m_u64LastFramePTS);
            if ((m_u64CurPTSInterval > (m_u64LastPTSInterval * 3 / 2)) || (m_u64CurPTSInterval < (m_u64LastPTSInterval * 3 / 4)) || (m_u64CurFramePTS <= m_u64FramePTSBase))
            {
                m_u64FramePTSBase = m_u64CurFramePTS;
                m_uFrameNum = 0;
            }

            if (m_uFrameNum == 60)
            {
                uint32_t uFrameRate;
                uFrameRate = (60000000 / (m_u64CurFramePTS - m_u64FramePTSBase));
                if ((60000000 % (m_u64CurFramePTS - m_u64FramePTSBase)) > ((m_u64CurFramePTS - m_u64FramePTSBase) / 2))
                {
                    uFrameRate++;
                }

                m_uFrameRate = uFrameRate;
                m_uFrameNum = 0;
                m_u64FramePTSBase = m_u64CurFramePTS;
            }

            if (m_uFrameRate == 0 && m_u64CurPTSInterval != 0)
            { //刚开始收流时,计算帧率
                m_uFrameRate = 1000000 / m_u64CurPTSInterval;
            }
        }

        m_uFrameNum++;
        m_u64LastPTSInterval = m_u64CurPTSInterval;
        m_u64LastFramePTS = m_u64CurFramePTS;
    }
}

int VideoFpsCalculater::getFrameRate()
{
    if ((m_uFrameNumPerSec != 0))
    {
        if ((m_uFrameRate > m_uFrameNumPerSec) && (m_uFrameRate - m_uFrameNumPerSec > m_uFrameRate / 5))
        {
            if (m_uFrameNumPerSec > 80)
            {
                m_uFrameNumPerSec = 0;
            }

            if (m_uFrameRateLastStat != m_uFrameNumPerSec)
            {
                LOGI(LOG_TAGS "calc by FrameNumPerSec :frame rate [%u]>>[%u].\n", m_uFrameRateLastStat, m_uFrameNumPerSec);
                m_uFrameRateLastStat = m_uFrameNumPerSec;
            }
            return m_uFrameNumPerSec;
        }
        else if ((m_uFrameRate < m_uFrameNumPerSec) && (m_uFrameNumPerSec - m_uFrameRate > m_uFrameNumPerSec / 5))
        {
            if (m_uFrameNumPerSec > 80)
            {
                m_uFrameNumPerSec = 0;
            }

            if (m_uFrameRateLastStat != m_uFrameNumPerSec)
            {
                LOGI(LOG_TAGS "calc by FrameNumPerSec :frame rate [%u]>>[%u].\n", m_uFrameRateLastStat, m_uFrameNumPerSec);
                m_uFrameRateLastStat = m_uFrameNumPerSec;
            }
            return m_uFrameNumPerSec;
        }
    }

    if (m_uFrameRate > 80)
    {
        m_uFrameRate = 0;
    }

    if (m_uFrameRateLastStat != m_uFrameRate)
    {
        LOGI(LOG_TAGS "calc by PTS :frame rate [%u]>>[%u].\n", m_uFrameRateLastStat, m_uFrameRate);
        m_uFrameRateLastStat = m_uFrameRate;
    }
    return m_uFrameRate;
}
