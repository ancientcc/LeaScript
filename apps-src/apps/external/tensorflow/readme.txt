一、gl_delegate.cc
是否须要定义GL_GLES_PROTOTYPES？——不需要。在每个版本gles，增加的扩展函数是定死的。在include时，须要逐版增加。
<tensorflow>\tensorflow\lite\delegates\gpu\gl_delegate.cc，
------
#include "tensorflow/lite/delegates/gpu/gl/portable_gl31.h" <---- 增加这一行
#include "tensorflow/lite/delegates/gpu/gl_delegate.h"


在gl_delegate.h前，须让有portable_gl31.h，后者有出现这么个顺序：
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>
------------

二、flatc
--C:\ddksample\apps-src\apps\external\tensorflow\tensorflow\lite\delegates\gpu\gl
C:\ddksample\apps-src\scripts\flatc-25.2.10.exe --cpp common.fbs
C:\ddksample\apps-src\scripts\flatc-25.2.10.exe --scoped-enums --cpp compiled_model.fbs
C:\ddksample\apps-src\scripts\flatc-25.2.10.exe --cpp metadata.fbs
C:\ddksample\apps-src\scripts\flatc-25.2.10.exe --cpp workgroups.fbs

--C:\ddksample\apps-src\apps\external\tensorflow\tensorflow\compiler\mlir\lite\schema
C:\ddksample\apps-src\scripts\flatc-25.2.10.exe --gen-mutable --gen-object-api --cpp schema.fbs
C:\ddksample\apps-src\scripts\flatc-25.2.10.exe --gen-mutable --gen-object-api --cpp conversion_metadata.fbs

--C:\ddksample\apps-src\apps\external\tensorflow\tensorflow\lite\acceleration\configuration
C:\ddksample\apps-src\scripts\flatc-25.2.10.exe --gen-mutable --gen-object-api --cpp configuration.fbs

--C:\ddksample\apps-src\apps\external\tensorflow\tensorflow\lite\delegates\xnnpack
C:\ddksample\apps-src\scripts\flatc-25.2.10.exe --gen-mutable --gen-object-api --cpp weight_cache_schema.fbs

三、FlatBufferModel、model_builder_rose.h
官方FlatBufferModel是派生于FlatBufferModelBase。
class FlatBufferModel : public FlatBufferModelBase<FlatBufferModel> {
但按这种方法编译下来，链接阶段会报数个函数没实现错误，像FlatBufferModel::~FlatBufferModel。目前解决办法是FlatBufferModel改为不于从FlatBufferModelBase派生了。而是直接把FlatBufferModelBase改为FlatBufferModel，见model_builder_rose.h。

四、android下需定义宏：TFLITE_WITH_RUY
TFLITE_WITH_RUY用于控制是否启用‌RUY库作为矩阵乘法(GEMM)的优化后端，像kernels/conv.cc中的计算卷积。如果不定义，意味着在计算卷积时会用Eigen多线程加速(TFLITE_WITH_MULTITHREADED_EIGEN)。一旦用Eigen，pose_landmark_full.tflite进行本地推导过程，在android会出现阻塞，或崩溃。查下来，阻塞是发生在multithreaded_conv.h中的struct MatMulConvFunctor，更具体是在in0.contract(in1, dim_pair)。

windows也可以定义TFLITE_WITH_RUY，但这会使得推导非常耗时。像用Eigen多线程加速只要200毫秒的，会变成3秒。


五、tensorflowlite须要的xnnpack版本
写在<tensorflow-20250320>/tensorflow/workspace2.bzl，搜xnnpack。

-rvv-  #include <riscv_vector.h>
-hvx-


Calculator  计算器/处理单元，核心处理单元