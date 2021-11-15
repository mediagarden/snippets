static int insert_start_code(uint8_t *data, int offset, int len)
{
    if (offset + 4 <= len)
    {
        data[offset++] = 0x00;
        data[offset++] = 0x00;
        data[offset++] = 0x00;
        data[offset++] = 0x01;
        return offset;
    }
    return -1;
}

//使用FFmpeg推annexb格式的流时,需要将extradata设置为annexb格式;
static void avcodec_parameters_annexbtomp4(AVCodecParameters *codec_par)
{
    if (codec_par->codec_id == AV_CODEC_ID_H264)
    {
        struct mpeg4_avc_t avc;
        mpeg4_avc_decoder_configuration_record_load(codec_par->extradata, codec_par->extradata_size, &avc);
        av_freep(&codec_par->extradata);
        codec_par->extradata_size = 0;

        const size_t max_len = 4 * 1024;
        uint8_t data[max_len];
        size_t offset = 0;
        for (int i = 0; i < avc.nb_sps; i++)
        {
            offset = insert_start_code(data, offset, max_len);
            memcpy(&data[offset], avc.sps[i].data, avc.sps[i].bytes);
            offset += avc.sps[i].bytes;
        }
        for (int i = 0; i < avc.nb_pps; i++)
        {
            offset = insert_start_code(data, offset, max_len);
            memcpy(&data[offset], avc.pps[i].data, avc.pps[i].bytes);
            offset += avc.pps[i].bytes;
        }

        size_t malloc_len = offset + AV_INPUT_BUFFER_PADDING_SIZE;
        codec_par->extradata = (uint8_t *)av_malloc(malloc_len);
        assert(codec_par->extradata != nullptr);
        memset(codec_par->extradata, 0, malloc_len);
        memcpy(codec_par->extradata, data, offset);
        codec_par->extradata_size = offset;
    }
    else if (codec_par->codec_id == AV_CODEC_ID_HEVC)
    {
        //TODO:此段代码未测试,需要debug
        struct mpeg4_hevc_t hevc;
        mpeg4_hevc_decoder_configuration_record_load(codec_par->extradata, codec_par->extradata_size, &hevc);
        av_freep(&codec_par->extradata);
        codec_par->extradata_size = 0;

        const size_t max_len = 4 * 1024;
        uint8_t data[max_len];
        size_t offset = 0;
        for (int i = 0; i < hevc.numOfArrays; i++)
        {
            offset = insert_start_code(data, offset, max_len);
            memcpy(&data[offset], hevc.nalu[i].data, hevc.nalu[i].bytes);
            offset += hevc.nalu[i].bytes;
        }

        size_t malloc_len = offset + AV_INPUT_BUFFER_PADDING_SIZE;
        codec_par->extradata = (uint8_t *)av_malloc(malloc_len);
        assert(codec_par->extradata != nullptr);
        memset(codec_par->extradata, 0, malloc_len);
        memcpy(codec_par->extradata, data, offset);
        codec_par->extradata_size = offset;
    }
}