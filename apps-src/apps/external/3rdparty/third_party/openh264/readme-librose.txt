存在<openh264>目录是因为webrtc在使用openh264时，使用了类似以下的代码：
<webrtc>/modules/video_coding/codecs/h264/h264_encoder_impl.cc
------
#include "third_party/openh264/src/codec/api/svc/codec_api.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_ver.h"
免得改webrtc代码，这建了这个目录。