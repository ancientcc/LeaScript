#ifndef AIKIT_BIZ_BUILDER_H
#define AIKIT_BIZ_BUILDER_H

#include "aikit_biz_type.h"
#include "aikit_biz_obsolete_builder.h"

namespace AIKIT {

/**
 * 
 */
class AIKITAPI AIKIT_ParamBuilder {
public:
    static AIKIT_ParamBuilder* create();
    static void destroy(AIKIT_ParamBuilder* builder);
    virtual ~AIKIT_ParamBuilder();

    //
    //
    virtual AIKIT_ParamBuilder* header() = 0;
    virtual AIKIT_ParamBuilder* header(const char* key, const char* data, uint32_t dataLen) = 0;
    virtual AIKIT_ParamBuilder* header(const char* key, int value)                          = 0;
    virtual AIKIT_ParamBuilder* header(const char* key, double value)                       = 0;
    virtual AIKIT_ParamBuilder* header(const char* key, bool value)                         = 0;
    //
    //
    virtual AIKIT_ParamBuilder* service(const char* serviceID) = 0;
    virtual AIKIT_ParamBuilder* service(const char* serviceID, AIKIT_ParamBuilder* value) = 0;

    virtual AIKIT_ParamBuilder* param(const char* key, const char* data, uint32_t dataLen) = 0;
    virtual AIKIT_ParamBuilder* param(const char* key, int value)                          = 0;
    virtual AIKIT_ParamBuilder* param(const char* key, double value)                       = 0;
    virtual AIKIT_ParamBuilder* param(const char* key, bool value)                         = 0;

    //
    virtual AIKIT_ParamBuilder* param(const char* key, AIKIT_ParamBuilder* value)          = 0;
    virtual AIKIT_BizParam* build() = 0;

    virtual void clear() = 0;
};

/**
 * @brief 
 * 
 */
class AIKITAPI AiData {
public:
    template <class T, class O>
    class AIKITAPI AiDataHolder {
    public:
        virtual T* status(int status) = 0;
        virtual T* begin()            = 0;
        virtual T* cont()             = 0;
        virtual T* end()              = 0;
        virtual T* once()             = 0;

        virtual T* data(const char* value,int dataLen) = 0;
        virtual T* path(const char* path)              = 0;
        virtual T* file(const FILE* file)              = 0;

        virtual O* valid() = 0;
    };

    virtual ~AiData() = 0;
};

/**
 * 
 * 
 * 
 * 
 */
class AIKITAPI AiText : public AiData {
public:
    class AiTextHolder : public AiDataHolder<AiTextHolder,AiText> {
    public:
        virtual AiTextHolder* encoding(const char* encoding) = 0;
        virtual AiTextHolder* compress(const char* compress) = 0;
        virtual AiTextHolder* format(const char* format)     = 0;
    };

    static AiTextHolder* get(const char* key);
    virtual ~AiText() = 0;
public:
    static constexpr char* const ENCODING_UTF8   = (char*)"utf8";
    static constexpr char* const ENCODING_GBK    = (char*)"gbk";
    static constexpr char* const ENCODING_GB2312 = (char*)"gb2312";
    static constexpr char* const ENCODING_DEF    = ENCODING_UTF8;

    static constexpr char* const COMPRESS_RAW  = (char*)"raw";
    static constexpr char* const COMPRESS_GZIP = (char*)"gzip";
    static constexpr char* const COMPRESS_DEF  = COMPRESS_RAW;

    static constexpr char* const FORMAT_PLAIN = (char*)"plain";
    static constexpr char* const FORMAT_JSON  = (char*)"json";
    static constexpr char* const FORMAT_XML   = (char*)"xml";
    static constexpr char* const FORMAT_DEF   = FORMAT_PLAIN;
};

/**
 * 
 * 
 * 
 * 
 * 
 */
class AIKITAPI  AiAudio : public AiData {
public :
    class AiAudioHolder : public AiDataHolder<AiAudioHolder,AiAudio> {
    public:
        virtual AiAudioHolder* encoding(const char* encoding) = 0;
        virtual AiAudioHolder* sampleRate(int sampleRate)     = 0;
        virtual AiAudioHolder* channels(int channels)         = 0;
        virtual AiAudioHolder* bitDepth(int bitDepth)         = 0;
    };

    static AiAudioHolder* get(const char* key);
    virtual ~AiAudio() = 0;
public:
    static constexpr char* const ENCODING_PCM      = (char*)"pcm";
    static constexpr char* const ENCODING_RAW      = (char*)"raw";
    static constexpr char* const ENCODING_ICO      = (char*)"ico";
    static constexpr char* const ENCODING_SPEEX    = (char*)"speex";
    static constexpr char* const ENCODING_SPEEX_WB = (char*)"speex-wb";
    static constexpr char* const ENCODING_LAME     = (char*)"lame";
    static constexpr char* const ENCODING_OPUS     = (char*)"opus";
    static constexpr char* const ENCODING_OPUS_WB  = (char*)"opus-wb";
    static constexpr char* const ENCODING_WAV      = (char*)"wav";
    static constexpr char* const ENCODING_AMR      = (char*)"amr";
    static constexpr char* const ENCODING_AMR_WB   = (char*)"amr-wb";
    static constexpr char* const ENCODING_MP3      = (char*)"mp3";
    static constexpr char* const ENCODING_CDA      = (char*)"cda";
    static constexpr char* const ENCODING_WAVE     = (char*)"wave";
    static constexpr char* const ENCODING_AIFF     = (char*)"aiff";
    static constexpr char* const ENCODING_MPEG     = (char*)"mpeg";
    static constexpr char* const ENCODING_MID      = (char*)"mid";
    static constexpr char* const ENCODING_WMA      = (char*)"wma";
    static constexpr char* const ENCODING_RA       = (char*)"ra";
    static constexpr char* const ENCODING_RM       = (char*)"rm";
    static constexpr char* const ENCODING_RMX      = (char*)"rmx";
    static constexpr char* const ENCODING_VQF      = (char*)"vqf";
    static constexpr char* const ENCODING_OGG      = (char*)"ogg";
    static constexpr char* const ENCODING_DEF      = ENCODING_SPEEX_WB;

    static const int SAMPLE_RATE_8K  = 8000;
    static const int SAMPLE_RATE_16K = 16000;
    static const int SAMPLE_RATE_DEF = SAMPLE_RATE_16K;

    static const int CHANNELS_1   = 1;
    static const int CHANNELS_2   = 2;
    static const int CHANNELS_DEF = CHANNELS_1;

    static const int BIT_DEPTH_8   = 8;
    static const int BIT_DEPTH_16  = 16;
    static const int BIT_DEPTH_DEF = BIT_DEPTH_16;
};

/**
 * 
 * 
 * 
 * 
 * 
 */
class AIKITAPI AiImage : public AiData {
public:
    class AiImageHolder : public AiDataHolder<AiImageHolder,AiImage> {
    public:
        virtual AiImageHolder* encoding(const char* encoding) = 0;
        virtual AiImageHolder* width(int width)               = 0;
        virtual AiImageHolder* height(int height)             = 0;
        virtual AiImageHolder* dims(int dims)                 = 0;
    };

    static AiImageHolder* get(const char* key);
    virtual ~AiImage() = 0;
public:
    static constexpr char* const ENCODING_RAW    = (char*)"raw";
    static constexpr char* const ENCODING_JPG    = (char*)"jpg";
    static constexpr char* const ENCODING_JPEG   = (char*)"jpeg";
    static constexpr char* const ENCODING_PNG    = (char*)"png";
    static constexpr char* const ENCODING_APNG   = (char*)"apng";
    static constexpr char* const ENCODING_BMP    = (char*)"bmp";
    static constexpr char* const ENCODING_WEBP   = (char*)"webp";
    static constexpr char* const ENCODING_TIFF   = (char*)"tiff";
    static constexpr char* const ENCODING_RGB565 = (char*)"rgb565";
    static constexpr char* const ENCODING_RGB888 = (char*)"rgb888";
    static constexpr char* const ENCODING_BGR565 = (char*)"bgr565";
    static constexpr char* const ENCODING_BGR888 = (char*)"bgr888";
    static constexpr char* const ENCODING_YUV12  = (char*)"yuv12";
    static constexpr char* const ENCODING_YUV21  = (char*)"yuv21";
    static constexpr char* const ENCODING_YUV420 = (char*)"yuv420";
    static constexpr char* const ENCODING_YUV422 = (char*)"yuv422";
    static constexpr char* const ENCODING_PSD    = (char*)"psd";
    static constexpr char* const ENCODING_PCD    = (char*)"pcd";
    static constexpr char* const ENCODING_DEF    = ENCODING_JPG;
};

/**
 * 
 * 
 * 
 * 
 * 
 */
class AIKITAPI AiVideo : public AiData {
public:
    class AiVideoHolder : public AiDataHolder<AiVideoHolder,AiVideo> {
    public:
        virtual AiVideoHolder* encoding(const char* key) = 0;
        virtual AiVideoHolder* width(int width)          = 0;
        virtual AiVideoHolder* height(int height)        = 0;
        virtual AiVideoHolder* frameRate(int frameRate)  = 0;
    };

    static AiVideoHolder* get(const char* key);
    virtual ~AiVideo() = 0;
public:
    static constexpr char* const ENCODING_H264 = (char*)"h264";
    static constexpr char* const ENCODING_H265 = (char*)"h265";
    static constexpr char* const ENCODING_AVI  = (char*)"avi";
    static constexpr char* const ENCODING_NAVI = (char*)"navi";
    static constexpr char* const ENCODING_MP4  = (char*)"mp4";
    static constexpr char* const ENCODING_RM   = (char*)"rm";
    static constexpr char* const ENCODING_RMVB = (char*)"rmvb";
    static constexpr char* const ENCODING_MKV  = (char*)"mkv";
    static constexpr char* const ENCODING_FLV  = (char*)"flv";
    static constexpr char* const ENCODING_F4V  = (char*)"f4v";
    static constexpr char* const ENCODING_MPG  = (char*)"mpg";
    static constexpr char* const ENCODING_MLV  = (char*)"mlv";
    static constexpr char* const ENCODING_MPE  = (char*)"mpe";
    static constexpr char* const ENCODING_MPEG = (char*)"mpeg";
    static constexpr char* const ENCODING_DAT  = (char*)"dat";
    static constexpr char* const ENCODING_m2v  = (char*)"m2v";
    static constexpr char* const ENCODING_VOB  = (char*)"vob";
    static constexpr char* const ENCODING_ASF  = (char*)"asf";
    static constexpr char* const ENCODING_MOV  = (char*)"mov";
    static constexpr char* const ENCODING_WMV  = (char*)"wmv";
    static constexpr char* const ENCODING_3GP  = (char*)"3gp";
    static constexpr char* const ENCODING_DEF  = ENCODING_H264;
};

/**
 * 输入构造类
 */
class AIKITAPI AIKIT_DataBuilder : public AIKIT_DataBuilderObsolete {
public:
    static AIKIT_DataBuilder* create();
    static void destroy(AIKIT_DataBuilder* builder);
    virtual ~AIKIT_DataBuilder();

    virtual AIKIT_DataBuilder* payload(AiData* data) = 0;
    virtual AIKIT_InputData* build() = 0;

    virtual void clear() = 0;
};

/**
 * 
 */
class AIKITAPI AIKIT_CustomBuilder {
public:
    static AIKIT_CustomBuilder* create();
    static void destroy(AIKIT_CustomBuilder* builder);
    virtual ~AIKIT_CustomBuilder();

    virtual AIKIT_CustomBuilder* text(const char* key, const char* data, uint32_t dataLen, int32_t index) = 0;
    virtual AIKIT_CustomBuilder* textPath(const char* key, const char* path, int32_t index)               = 0;
    virtual AIKIT_CustomBuilder* textFile(const char* key, const FILE* file, int32_t index)               = 0;

    virtual AIKIT_CustomBuilder* audio(const char* key, const char* data, uint32_t dataLen, int32_t index) = 0;
    virtual AIKIT_CustomBuilder* audioPath(const char* key, const char* path, int32_t index)               = 0;
    virtual AIKIT_CustomBuilder* audioFile(const char* key, const FILE* file, int32_t index)               = 0;

    virtual AIKIT_CustomBuilder* image(const char* key, const char* data, uint32_t dataLen, int32_t index) = 0;
    virtual AIKIT_CustomBuilder* imagePath(const char* key, const char* path, int32_t index)               = 0;
    virtual AIKIT_CustomBuilder* imageFile(const char* key, const FILE* file, int32_t index)               = 0;

    virtual AIKIT_CustomBuilder* video(const char* key, const char* data, uint32_t dataLen, int32_t index) = 0;
    virtual AIKIT_CustomBuilder* videoPath(const char* key, const char* path, int32_t index)               = 0;
    virtual AIKIT_CustomBuilder* videoFile(const char* key, const FILE* file, int32_t index)               = 0;

    virtual AIKIT_CustomData* build() = 0;
    virtual void clear() = 0;
};

/**
 * 
 */
class AIKITAPI AIKIT_Builder {
public:
    static AIKIT_BizParam*  build(AIKIT_ParamBuilder* param);
    static AIKIT_InputData* build(AIKIT_DataBuilder* data);
    static AIKIT_CustomData* build(AIKIT_CustomBuilder* custom);
};

} // namespace AIKIT

#endif