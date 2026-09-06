bzip2
======
bzip2-1.0.6

boost
======
1_86_0(<boost>/boost/version.hpp)
----
不要用boost::function/boost::bind，改用std::function/std::bind，混用两种bind会给编程较大麻烦。C++11已支持std::function/std::bind。

用boost_1_88_0编译，会报错    
11>C:\ddksample\apps-src\apps\external\boost\boost\mpl\aux_\include_preprocessed.hpp(37,13): error C1083: Cannot open include file: 'boost/mpl/aux_/preprocessed/plain/||.hpp': No such file or directory
11>(compiling source file '../../external/libros/cartographer/cartographer/io/points_batch.cc')
没测过boost_1_87_0，不知道是否也有这问题。

gettext
======
gettext include gettext and libiconv directory. compile require Preprocessor Definitions 
-------------------------
XCode(iOS/Mac OS X)
_LIBICONV_H   avoid include system defined <iconv.h>
---------------------------

libyuv
======
<external>/3rdparty/third_party/libyuv/include/libyuv/basic_types.h
增加
#ifndef HAVE_JPEG
#define HAVE_JPEG
#endif
目的是让实现libyuv::MJPGToARGB等和mjpeg有关的转换。当然，你可以把HAVE_JPEG放在项目预定宏，但我感觉有点了，先放在basic_types.h。

jpeg-9b
======
为什么要加入jpeg？1）webrtc时，从摄像头出来的图像是mjpeg格式，windows端需要有这库来解码这格式。Android因为相机模块解码了，不必包括jpeg。2）libyuv中使能了HAVE_JPEG，那在调用libyuv::MJPGToARGB时会调到jpeg-9b中的库函数。和webrtc一样，windows端需要有这个解码这格式。对android，有SDL_image.so包括了jpeg-9b，可以不需要。

--Preprocessor Definitions
  HAVE_JPEG AVOID_TABLES(Android需要)
要编译的源文件特点：1）一定是以j开始。2）没有jpegtran.c。3）jmem开始的文件中，一定须要jmemmgr.c，windows加jmemnobs.c，android加jmem-android
为不向include增加一个目录，修改<src>\apps\external\third_party\libyuv\source\mjpeg_decoder.cc，
#include <jpeglib.h> ==> #include <jpeg-9b/jpeglib.h>

针对android，遇到过libyuv::MJPGToARGB时，调用MJpegDecoder::MJpegDecoder内的jpeg_create_decompress（这可说是个jpeg-9b内api）出现非法。当然，那时是因为libyuv::MJPGToARGB是libOrbbecSDK.so这sdk提供了，后已改为由app自提供。考虑到有SDL_image.so包括了jpeg-9b，andorid下app没有编译jpeg-9b，如果要编译，只需在Android.mk加上下面这条语句。
include $(LOCAL_PATH)/external/3rdparty/jpeg-9b/Android.mk

protobuf
======
说明：http://www.libsdl.cn/bbs/forum.php?mod=viewthread&tid=149&extra=page%3D1

third_party/ffmpeg
======
Only header. Place here instead with linker/include, because webrtc quire this location.

zlib
======
zlib1.2.8

zxing
======
zxing来自https://github.com/glassechidna/zxing-cpp), 为什么没有用https://github.com/nu-book/zxing-cpp? --实测下来，在2017年MacBook Pro上。

识别1280x720视频流（没二维码）。nu-book/zxing-cpp：每帧平均花费80毫秒（fast=false时则要300多毫秒）。glassechidna/zxing-cpp：平均70毫秒。
循环识别同一张960x1280图像（二维码：260x260）。nu-book/zxing-cpp：平均55毫秒。glassechidna/zxing-cpp：平均75毫秒。
循环识别同一张260x260图像（整个是二维码，须旋转）。nu-book/zxing-cpp：平均6毫秒。glassechidna/zxing-cpp：平均5毫秒。
循环识别同一张260x260图像（整个是二维码，已正放）。nu-book/zxing-cpp：平均6毫秒。glassechidna/zxing-cpp：平均5毫秒。

当图像中没有二维码时，glassechidna/zxing-cpp要比nu-book快10秒毫。当图像中有二维码，而且nu-book至少不会比glassechidna差。测试下来，在成功识别率上，nu-book极可能要好过glassechidna。为使用nu-book，最好先精准扣出二维码，这会极大加

A：为什么第一次crop时要向内缩10像素。
Q：旋转后会没有部分会有“黑色”填充，在破坏这些“黑色”自连成一个轮廓。

让我很纳闷，在android，直接把摄像头图像输入nu-book/zxing-cpp时，花的时间大概45毫秒，作为3所检测阶段，应该还能忍受。为此，在QrParse::FindQrPoint效果没做到比nu-book/zxing-cpp更优时，把直接输入nu-book/zxing-cpp作为优先算法。

SDL/libvpx
======
在用的这版本，ios编译很烦索，但好在目前不会用vp8、vp9。一旦要用vp8、vp9，一定要升级webrtc，连接着升libvpx。

ceres
======
为减少须要设置的头文件目录数目，移动了几个头文件位置。
#include "ceres/internal/config.h"
<ceres>/config/ceres/internal/config.h ==> <ceres>/include/ceres/internal/config.h

#include "glog/logging.h"
<ceres>/internal/ceres/miniglog/glog/logging.h ==> <ceres>/include/glog/logging.h

tinyxml
======
必须定义宏：TIXML_USE_STL
原因：ros在使用tynyxml，肯定会使用std::string，它只有在TIXML_USE_STL才会有效。
处理方法：修改<tinyxml>/tinyxml.h，增加定义该宏，避免在全局定义。


ompl
======
放在3rdparty目录，没放在libros。是因为ompl有太多头文件，不想花时间挑出这些文件，然后放在一个专门libros/include/ompl目录。


pthreadpool
======
对src/memory.c，ios编译时失败，内容不变，换个文件名，就成功了。我不知道原因，但考虑这个，memory.c改名为memory1.c。只是文件名换下，没改内容。


lz4
======
该库有个xxhash.c，roslz4也有xxhash.c，它们都实现了三个同名函数。要是两个xxhash.c都编译，在android会报同一函数重复定错。
ld: error: duplicate symbol: XXH32
>>> defined at xxhash.c:393 (jni/../../../../external/3rdparty/lz4/lib\xxhash.c:393)
>>>            ./obj/local/arm64-v8a/objs/3rdparty/external/3rdparty/lz4/lib/xxhash.o:(XXH32)
>>> defined at xxhash.c:266 (jni/../../../../external/libros/ros_comm/roslz4/src\xxhash.c:266)
>>>            ./obj/local/arm64-v8a/objs/3rdparty/external/libros/ros_comm/roslz4/src/xxhash.o:(.text.XXH32+0x0)
同样的还有XXH32_update、XXH32_digest。这三个函数都在xxhash.c。不能不编译roslz4的xxhash.c，那只能不编译lz4下的xxhash.c。可一旦不编译它，lz4frame.c就出错了，它要用到xxhash.c实现的函数。为此，lz4只编译lz4.c。

glog
======
存在glog，纯粹就是一些开源项目会用到它，要尽快编译出一个可运行版本。发布时，不要包含glog。
使用glog，要以静态库方式。当glog是以dll编译时，链接阶段会报很多error LNK2005错误。
13>3rdparty.lib(3rdparty.dll) : error LNK2005: "public: class std::basic_streambuf<char,struct std::char_traits<char> > * __cdecl std::basic_ios<char,struct std::char_traits<char> >::rdbuf(void)const " (?rdbuf@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBAPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@XZ) already defined in cliprdr_common2.obj
目前没找到这错误原因。rdbuf似乎是std::ostream的成员函数，而glog中是有个class LogMessage::LogStream，它派生于public std::ostream。但我看到这和错误有啥联系。

lib3rdparty2包含的absl已支持输出日志和CHECK诊断，后面用到的glog的，要用absl去代替。如何代替参考：https://www.cswamp.com/post/270。

pthread_win32
======
pthread_win32放在mediapipe，而不是3rdparty，是为了避免DllMain重定义。
3rdparty包含opencv，opencv中<opencv>/modules/core/src/system.c实现了DllMain。pthread_win32也有DllMain。放在mediapipe，那只pthread_win32中的这一个DllMain了。

避免冲突，1）宏名HAVE_CONFIG_H改为PTHREAD_WIN32_HAVE_CONFIG_H。2）文件名config.h改为pthread_win32_config.h。

