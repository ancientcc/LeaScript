// Copyright 2020 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "mediapipe/calculators/tensor/image_to_tensor_calculator.pb.h"
#include "mediapipe/calculators/tensor/image_to_tensor_converter.h"
#include "mediapipe/calculators/tensor/image_to_tensor_utils.h"
#include "mediapipe/framework/api2/node.h"
#include "mediapipe/framework/api2/packet.h"
#include "mediapipe/framework/api2/port.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/rect.pb.h"
#include "mediapipe/framework/formats/tensor.h"
#include "mediapipe/framework/memory_manager.h"
#include "mediapipe/framework/memory_manager_service.h"
#include "mediapipe/framework/port.h"
#include "mediapipe/framework/port/canonical_errors.h"
#include "mediapipe/framework/port/ret_check.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/port/statusor.h"
#include "mediapipe/gpu/gpu_origin.pb.h"
#include "mediapipe/gpu/gpu_origin_utils.h"
#include "mediapipe/gpu/webgpu/webgpu_check.h"

#if !MEDIAPIPE_DISABLE_OPENCV
#include "mediapipe/calculators/tensor/image_to_tensor_converter_opencv.h"
#elif MEDIAPIPE_ENABLE_HALIDE
#include "mediapipe/calculators/tensor/image_to_tensor_converter_frame_buffer.h"
#endif

#if !MEDIAPIPE_DISABLE_GPU
#include "mediapipe/gpu/gpu_buffer.h"

#if MEDIAPIPE_METAL_ENABLED
#include "mediapipe/calculators/tensor/image_to_tensor_converter_metal.h"
#include "mediapipe/gpu/MPPMetalHelper.h"
#elif MEDIAPIPE_OPENGL_ES_VERSION >= MEDIAPIPE_OPENGL_ES_31
#include "mediapipe/calculators/tensor/image_to_tensor_converter_gl_buffer.h"
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_service.h"
#else
#include "mediapipe/calculators/tensor/image_to_tensor_converter_gl_texture.h"
#include "mediapipe/gpu/gl_calculator_helper.h"
#include "mediapipe/gpu/gpu_service.h"
#if MEDIAPIPE_USE_WEBGPU
#include "mediapipe/gpu/webgpu/image_to_tensor_converter_webgpu_texture.h"
#include "mediapipe/gpu/webgpu/webgpu_service.h"
#include "mediapipe/gpu/webgpu/webgpu_texture_buffer.h"
#endif  // MEDIAPIPE_USE_WEBGPU
#endif  // MEDIAPIPE_METAL_ENABLED
#endif  // !MEDIAPIPE_DISABLE_GPU

#include "tensorflow/lite/util.h"
#include <SDL.h>
#include "rose_sdl_utils.hpp"
#include "rose_filesystem_dll.hpp"
#include "rose_config_3rdparty.hpp"

namespace mediapipe {
extern const char *EnumNameTfLiteType(TfLiteType e);

TfLiteType ElementType_2_TfLiteType(Tensor::ElementType type)
{
    if (type == Tensor::ElementType::kFloat16) {
        return kTfLiteFloat16;

    } else if (type == Tensor::ElementType::kFloat32) {
        return kTfLiteFloat32;

    } else if (type == Tensor::ElementType::kUInt8) {
        return kTfLiteUInt8;

    } else if (type == Tensor::ElementType::kInt8) {
        return kTfLiteInt8;

    } else if (type == Tensor::ElementType::kInt32) {
        return kTfLiteInt32;

    } else if (type == Tensor::ElementType::kInt64) {
        return kTfLiteInt64;

    } else if (type == Tensor::ElementType::kChar) {
        return kTfLiteInt8;

    } else if (type == Tensor::ElementType::kBool) {
        return kTfLiteBool;

    } 
    // Tensor::ElementType::kNone
    return kTfLiteNoType;
}

void verbose_tensor(const std::string& scene, const Tensor& tensor)
{
    std::stringstream ss;
	ss.str("");
	// shape
    const Tensor::Shape& shape = tensor.shape();
	ss << "[";
	for (int at2 = 0; at2 < shape.dims.size(); at2 ++) {
		if (at2 != 0) {
			ss << ", ";
		}
		ss << shape.dims[at2];
	}
	ss << "], ";
	// type
    Tensor::ElementType type = tensor.element_type();
    TfLiteType type2 = ElementType_2_TfLiteType(type);
	ss << EnumNameTfLiteType(type2) << ", ";

	// buffer
    auto input_tensor_view = tensor.GetCpuReadView();
    const float* input_tensor_buffer = input_tensor_view.buffer<float>();

	if (tensor.bytes() != 0 && input_tensor_buffer != nullptr) {
		ss << tensor.bytes() << " Bytes";
	} else {
		ss << " 0 Bytes";
	}
    if (type2 == kTfLiteFloat32 && tensor.bytes() >= 16 * 4) {
        const float* f = input_tensor_buffer;
        char buf[4096];
        SDL_snprintf(buf, sizeof(buf), "[0]%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n[8]%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",
            f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], 
            f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15]);
        ss << "\n" << buf;
    }

    SDL_Log("---mediapipe::Tensor(%s)---", scene.c_str());
    SDL_Log("%s", ss.str().c_str());
    SDL_Log("-----------------");

    // [1, 224, 224, 3], Float32, 602112 Bytes
    if (shape.dims.size() == 4) {
        const std::vector<int>& dims = shape.dims;
        if (dims[0] == 1 && dims[1] == 224 && dims[2] == 224 && dims[3] == 3) {
            void* data = malloc(tensor.bytes());
            memcpy(data, input_tensor_buffer, tensor.bytes());
            cv::Mat src(224, 224, CV_8UC3, data);
            cv::Mat dst;
            cv::cvtColor(src, dst, cv::COLOR_BGR2RGB);
            imwrite(dst, "1-ImageToTensorCalculator.png");

            tfile file(game_config::preferences_dir + "/1-ImageToTensorCalculator.dat", GENERIC_WRITE, CREATE_ALWAYS);
            posix_fwrite(file.fp, input_tensor_buffer, tensor.bytes());
            free(data);
        }
    }
}

void copy_file_dat_to_tensor(Tensor& tensor)
{
    std::string filename;

    const Tensor::Shape& shape = tensor.shape();
    const std::vector<int>& dims = shape.dims;

    if (dims.size() == 3 &&
        dims[0] == 1 && dims[1] == 2254 && dims[2] == 12) {
        // ===============pose_detection.tflite===============
        // ---[output]441 tensor: Identity---
        // [1, 2254, 12], Float32, 108192 Bytes
        filename = "1-pose_detection-output-Identity.dat.bak";

    } else if (dims.size() == 3 && 
        dims[0] == 1 && dims[1] == 2254 && dims[2] == 1) {
        // ===============pose_detection.tflite===============
        // ---[output]429 tensor: Identity_1---
        //  [1, 2254, 1], Float32, 9016 Bytes
        filename = "1-pose_detection-output-Identity_1.dat.bak";

    } else if (dims.size() == 2 && 
        dims[0] == 1 && dims[1] == 195) {
        // =============pose_landmark_full.tflite=============
        // ---[output]337 tensor: Identity---
        // [1, 195], Float32, 780 Bytes
        filename = "1-pose_landmark_full-output-Identity.dat.bak";

    } else if (dims.size() == 2 && 
        dims[0] == 1 && dims[1] == 1) {
        // =============pose_landmark_full.tflite=============
        // ---[output]342 tensor: Identity_1---
        // [1, 1], Float32, 4 Bytes
        filename = "1-pose_landmark_full-output-Identity_1.dat.bak";

    } else if (dims.size() == 4 && 
        dims[0] == 1 && dims[1] == 256 && dims[2] == 256 && dims[3] == 1) {
        // =============pose_landmark_full.tflite=============
        // ---[output]297 tensor: Identity_2---
        // [1, 256, 256, 1], Float32, 262144 Bytes
        filename = "1-pose_landmark_full-output-Identity_2.dat.bak";

    } else if (dims.size() == 4 && 
        dims[0] == 1 && dims[1] == 64 && dims[2] == 64 && dims[3] == 39) {
        // =============pose_landmark_full.tflite=============
        // ---[output]298 tensor: Identity_3---
        // [1, 64, 64, 39], Float32, 638976 Bytes
        filename = "1-pose_landmark_full-output-Identity_3.dat.bak";

    } else if (dims.size() == 2 && 
        dims[0] == 1 && dims[1] == 117) {
        // =============pose_landmark_full.tflite=============
        // ---[output]339 tensor: Identity_4---
        // [1, 117], Float32, 468 Bytes
        filename = "1-pose_landmark_full-output-Identity_4.dat.bak";

    }
    VALIDATE(!filename.empty(), null_str);

    // buffer
    auto output_tensor_view = tensor.GetCpuWriteView();
    float* output_tensor_buffer = output_tensor_view.buffer<float>();

    if (!filename.empty()) {
        tfile file(game_config::preferences_dir + "/" + filename, GENERIC_READ, OPEN_EXISTING);
        int fsize = file.read_2_data();
        VALIDATE(fsize == tensor.bytes(), null_str);
        memcpy(output_tensor_buffer, file.data, fsize);
    }
}

namespace api2 {

// Converts image into Tensor, possibly with cropping, resizing and
// normalization, according to specified inputs and options.
//
// Inputs:
//   IMAGE - Image[ImageFormat::SRGB / SRGBA, GpuBufferFormat::kBGRA32] or
//           ImageFrame [ImageFormat::SRGB/SRGBA] (for backward compatibility
//           with existing graphs that use IMAGE for ImageFrame input)
//   IMAGE_GPU - GpuBuffer [GpuBufferFormat::kBGRA32]
//     Image to extract from.
//
//   Note:
//   - One and only one of IMAGE and IMAGE_GPU should be specified.
//   - IMAGE input of type Image is processed on GPU if the data is already on
//     GPU (i.e., Image::UsesGpu() returns true), or otherwise processed on CPU.
//   - IMAGE input of type ImageFrame is always processed on CPU.
//   - IMAGE_GPU input (of type GpuBuffer) is always processed on GPU.
//
//   NORM_RECT - NormalizedRect @Optional
//     Describes region of image to extract.
//     @Optional: rect covering the whole image is used if not specified.
//
// Outputs:
//   TENSORS - std::vector<Tensor>
//     Vector containing a single Tensor populated with an extracted RGB image.
//   MATRIX - std::array<float, 16> @Optional
//     An std::array<float, 16> representing a 4x4 row-major-order matrix that
//     maps a point on the input image to a point on the output tensor, and
//     can be used to reverse the mapping by inverting the matrix.
//   LETTERBOX_PADDING - std::array<float, 4> @Optional
//     An std::array<float, 4> representing the letterbox padding from the 4
//     sides ([left, top, right, bottom]) of the output image, normalized to
//     [0.f, 1.f] by the output dimensions. The padding values are non-zero only
//     when the "keep_aspect_ratio" is true.
//
//     For instance, when the input image is 10x10 (width x height) and the
//     output dimensions specified in the calculator option are 20x40 and
//     "keep_aspect_ratio" is true, the calculator scales the input image to
//     20x20 and places it in the middle of the output image with an equal
//     padding of 10 pixels at the top and the bottom. The resulting array is
//     therefore [0.f, 0.25f, 0.f, 0.25f] (10/40 = 0.25f).
//
// Example:
// node {
//   calculator: "ImageToTensorCalculator"
//   input_stream: "IMAGE:image"  # or "IMAGE_GPU:image"
//   input_stream: "NORM_RECT:roi"
//   output_stream: "TENSORS:tensors"
//   output_stream: "MATRIX:matrix"
//   options {
//     [mediapipe.ImageToTensorCalculatorOptions.ext] {
//       output_tensor_width: 256
//       output_tensor_height: 256
//       keep_aspect_ratio: false
//       output_tensor_float_range {
//         min: 0.0
//         max: 1.0
//       }
//       # gpu_origin: CONVENTIONAL # or TOP_LEFT
//     }
//   }
// }

#ifdef _WIN32
#ifdef GetObject
#undef GetObject
#endif
#endif


absl::Status CopyTensorToTfLiteTensor(const Tensor& input_tensor,
                                      TfLiteTensor& tflite_tensor) {
  auto input_tensor_view = input_tensor.GetCpuReadView();
  const float* input_tensor_buffer = input_tensor_view.buffer<float>();
  RET_CHECK(input_tensor_buffer) << "Input tensor buffer is null.";
  // RET_CHECK_EQ(tflite_tensor.type, tflite::typeToTfLiteType<float>())
  //        .SetCode(absl::StatusCode::kInvalidArgument)
  //    << "Tensor and TfLiteTensor types do not match.";
  void* local_tensor_buffer = tflite_tensor.data.raw;
  RET_CHECK(local_tensor_buffer) << "TfLiteTensor data is null.";
  // RET_CHECK_EQ(tflite_tensor.bytes, input_tensor.bytes())
  //        .SetCode(absl::StatusCode::kInvalidArgument)
  //    << "TfLiteTensor and Tensor sizes do not match. "
  //    << GetTfLiteTensorDebugInfo(tflite_tensor) << " vs. "
  //    << GetMpTensorDebugInfo(input_tensor);
  std::memcpy(local_tensor_buffer, input_tensor_buffer, input_tensor.bytes());
  return absl::OkStatus();
}

class ImageToTensorCalculator : public Node {
 public:
  static constexpr Input<
      OneOf<mediapipe::Image, mediapipe::ImageFrame>>::Optional kIn{"IMAGE"};
  static constexpr Input<GpuBuffer>::Optional kInGpu{"IMAGE_GPU"};
  static constexpr Input<mediapipe::NormalizedRect>::Optional kInNormRect{
      "NORM_RECT"};
  static constexpr Output<std::vector<Tensor>>::Optional kOutTensors{"TENSORS"};
  static constexpr Output<Tensor>::Optional kOutTensor{"TENSOR"};
  static constexpr Output<std::array<float, 4>>::Optional kOutLetterboxPadding{
      "LETTERBOX_PADDING"};
  static constexpr Output<std::array<float, 16>>::Optional kOutMatrix{"MATRIX"};

  MEDIAPIPE_NODE_CONTRACT(kIn, kInGpu, kInNormRect, kOutTensors, kOutTensor,
                          kOutLetterboxPadding, kOutMatrix);

  static absl::Status UpdateContract(CalculatorContract* cc) {
    const auto& options =
        cc->Options<mediapipe::ImageToTensorCalculatorOptions>();

    RET_CHECK_OK(ValidateOptionOutputDims(options));
    RET_CHECK(kIn(cc).IsConnected() ^ kInGpu(cc).IsConnected())
        << "One and only one of IMAGE and IMAGE_GPU input is expected.";
    RET_CHECK(kOutTensors(cc).IsConnected() ^ kOutTensor(cc).IsConnected())
        << "One and only one of TENSORS and TENSOR output is supported.";

#if MEDIAPIPE_DISABLE_GPU
    if (kInGpu(cc).IsConnected()) {
      return absl::UnimplementedError(
          "GPU processing is disabled in build flags");
    }
#else  // !MEDIAPIPE_DISABLE_GPU
#if MEDIAPIPE_METAL_ENABLED
    MP_RETURN_IF_ERROR([MPPMetalHelper updateContract:cc]);
#else
    cc->UseService(kGpuService).Optional();
#if MEDIAPIPE_USE_WEBGPU
    cc->UseService(kWebGpuService).Optional();
#endif  // MEDIAPIPE_USE_WEBGPU
#endif  // MEDIAPIPE_METAL_ENABLED
#endif  // MEDIAPIPE_DISABLE_GPU

    cc->UseService(kMemoryManagerService).Optional();
    return absl::OkStatus();
  }

  absl::Status Open(CalculatorContext* cc) {
    if (cc->Service(kMemoryManagerService).IsAvailable()) {
      memory_manager_ = &cc->Service(kMemoryManagerService).GetObject();
    }
    options_ = cc->Options<mediapipe::ImageToTensorCalculatorOptions>();
    params_ = GetOutputTensorParams(options_);
    return absl::OkStatus();
  }

  absl::Status Process(CalculatorContext* cc) {
    if ((kIn(cc).IsConnected() && kIn(cc).IsEmpty()) ||
        (kInGpu(cc).IsConnected() && kInGpu(cc).IsEmpty())) {
      // Timestamp bound update happens automatically.
      return absl::OkStatus();
    }

    absl::optional<mediapipe::NormalizedRect> norm_rect;
    if (kInNormRect(cc).IsConnected()) {
      if (kInNormRect(cc).IsEmpty()) {
        // Timestamp bound update happens automatically. (See Open().)
        return absl::OkStatus();
      }
      norm_rect = *kInNormRect(cc);
      if (norm_rect->width() == 0 && norm_rect->height() == 0) {
        // WORKAROUND: some existing graphs may use sentinel rects {width=0,
        // height=0, ...} quite often and calculator has to handle them
        // gracefully by updating timestamp bound instead of returning failure.
        // Timestamp bound update happens automatically. (See Open().)
        // NOTE: usage of sentinel rects should be avoided.
        ABSL_DLOG(WARNING)
            << "Updating timestamp bound in response to a sentinel rect";
        return absl::OkStatus();
      }
    }

#if MEDIAPIPE_DISABLE_GPU
    MP_ASSIGN_OR_RETURN(auto image, GetInputImage(kIn(cc)));
#else
    const bool is_input_gpu = kInGpu(cc).IsConnected();
    MP_ASSIGN_OR_RETURN(auto image, is_input_gpu ? GetInputImage(kInGpu(cc))
                                                 : GetInputImage(kIn(cc)));
#endif  // MEDIAPIPE_DISABLE_GPU

    RotatedRect roi = GetRoi(image->width(), image->height(), norm_rect);
    const int tensor_width = params_.output_width.value_or(image->width());
    const int tensor_height = params_.output_height.value_or(image->height());
    MP_ASSIGN_OR_RETURN(auto padding,
                        PadRoi(tensor_width, tensor_height,
                               options_.keep_aspect_ratio(), &roi));
    if (kOutLetterboxPadding(cc).IsConnected()) {
      kOutLetterboxPadding(cc).Send(padding);
    }
    if (kOutMatrix(cc).IsConnected()) {
      std::array<float, 16> matrix;
      GetRotatedSubRectToRectTransformMatrix(
          roi, image->width(), image->height(),
          /*flip_horizontally=*/false, &matrix);
      kOutMatrix(cc).Send(std::move(matrix));
    }

    // Lazy initialization of the GPU or CPU converter.
    MP_RETURN_IF_ERROR(InitConverterIfNecessary(cc, *image.get()));

    Tensor::ElementType output_tensor_type =
        GetOutputTensorType(image->UsesGpu(), params_);
    Tensor tensor(
        output_tensor_type,
        {1, tensor_height, tensor_width, GetNumOutputChannels(*image)},
        memory_manager_);
    MP_RETURN_IF_ERROR((image->UsesGpu() ? gpu_converter_ : cpu_converter_)
                           ->Convert(*image, roi, params_.range_min,
                                     params_.range_max,
                                     /*tensor_buffer_offset=*/0, tensor));

    if (kOutTensors(cc).IsConnected()) {
      // verbose_tensor("ImageToTensorCalculator::Process", tensor);
      auto result = std::make_unique<std::vector<Tensor>>();
      result->push_back(std::move(tensor));
      kOutTensors(cc).Send(std::move(result));
    } else {
      kOutTensor(cc).Send(std::move(tensor));
    }
    return absl::OkStatus();
  }

 private:
  absl::Status InitConverterIfNecessary(CalculatorContext* cc,
                                        const Image& image) {
    // Lazy initialization of the GPU or CPU converter.
    if (image.UsesGpu()) {
      if (!params_.is_float_output) {
        return absl::UnimplementedError(
            "ImageToTensorConverter for the input GPU image currently doesn't "
            "support quantization.");
      }
      if (!gpu_converter_) {
#if !MEDIAPIPE_DISABLE_GPU
#if MEDIAPIPE_METAL_ENABLED
        MP_ASSIGN_OR_RETURN(
            gpu_converter_,
            CreateMetalConverter(cc, GetBorderMode(options_.border_mode())));
#elif MEDIAPIPE_OPENGL_ES_VERSION >= MEDIAPIPE_OPENGL_ES_31
        MP_ASSIGN_OR_RETURN(bool input_starts_at_bottom,
                            IsGpuOriginAtBottom(options_.gpu_origin()));
        MP_ASSIGN_OR_RETURN(gpu_converter_,
                            CreateImageToGlBufferTensorConverter(
                                cc, input_starts_at_bottom,
                                GetBorderMode(options_.border_mode())));
#else
        if (IsWebGpuAvailable()) {
#if MEDIAPIPE_USE_WEBGPU
          MP_ASSIGN_OR_RETURN(gpu_converter_,
                              CreateImageToWebGpuTextureTensorConverter(cc));
#endif  // MEDIAPIPE_USE_WEBGPU
        }
#if MEDIAPIPE_OPENGL_ES_VERSION >= MEDIAPIPE_OPENGL_ES_30
        if (!gpu_converter_) {
          MP_ASSIGN_OR_RETURN(bool input_starts_at_bottom,
                              IsGpuOriginAtBottom(options_.gpu_origin()));
          MP_ASSIGN_OR_RETURN(gpu_converter_,
                              CreateImageToGlTextureTensorConverter(
                                  cc, input_starts_at_bottom,
                                  GetBorderMode(options_.border_mode())));
        }
#endif  // MEDIAPIPE_OPENGL_ES_VERSION >= MEDIAPIPE_OPENGL_ES_30
        if (!gpu_converter_) {
          return absl::UnimplementedError(
              "ImageToTensorConverter for the input GPU image is unavailable.");
        }
#endif  // MEDIAPIPE_METAL_ENABLED
#endif  // !MEDIAPIPE_DISABLE_GPU
      }
    } else {
      if (!cpu_converter_) {
#if !MEDIAPIPE_DISABLE_OPENCV
        MP_ASSIGN_OR_RETURN(
            cpu_converter_,
            CreateOpenCvConverter(
                cc, GetBorderMode(options_.border_mode()),
                GetOutputTensorType(/*uses_gpu=*/false, params_)));
// TODO: FrameBuffer-based converter needs to call GetGpuBuffer()
// to get access to a FrameBuffer view. Investigate if GetGpuBuffer() can be
// made available even with MEDIAPIPE_DISABLE_GPU set.
#elif MEDIAPIPE_ENABLE_HALIDE
        MP_ASSIGN_OR_RETURN(
            cpu_converter_,
            CreateFrameBufferConverter(
                cc, GetBorderMode(options_.border_mode()),
                GetOutputTensorType(/*uses_gpu=*/false, params_)));
#else
        ABSL_LOG(FATAL) << "Cannot create image to tensor CPU converter since "
                           "MEDIAPIPE_DISABLE_OPENCV is defined and "
                           "MEDIAPIPE_ENABLE_HALIDE is not defined.";
#endif  // !MEDIAPIPE_DISABLE_HALIDE
      }
    }
    return absl::OkStatus();
  }

  std::unique_ptr<ImageToTensorConverter> gpu_converter_;
  std::unique_ptr<ImageToTensorConverter> cpu_converter_;
  mediapipe::ImageToTensorCalculatorOptions options_;
  OutputTensorParams params_;
  MemoryManager* memory_manager_ = nullptr;
};

MEDIAPIPE_REGISTER_NODE(ImageToTensorCalculator);

}  // namespace api2
}  // namespace mediapipe
