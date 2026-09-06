// Copyright 2023 The MediaPipe Authors.
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

#include "mediapipe/calculators/util/landmarks_smoothing_calculator.h"

#include <memory>

#include "mediapipe/calculators/util/landmarks_smoothing_calculator.pb.h"
#include "mediapipe/calculators/util/landmarks_smoothing_calculator_utils.h"
#include "mediapipe/framework/api2/node.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/formats/rect.pb.h"
#include "mediapipe/framework/timestamp.h"

#include "mediapipe/rose/mediapipe_api.hpp"
#include "rose_exception.hpp"

namespace mediapipe {

extern SDL_FPoint g_xy_landmarks[mediapipe::kNumPoseLandmarks];

void NormalizedLandmarksToxy_landmarks(const NormalizedLandmarkList& norm_landmarks)
{
    VALIDATE(norm_landmarks.landmark_size() == mediapipe::kNumPoseLandmarks, str_cast(norm_landmarks.landmark_size()));
    int width = 1280;
    int height = 720;
    for (int i = 0; i < norm_landmarks.landmark_size(); ++i) {
      const auto& norm_landmark = norm_landmarks.landmark(i);
      g_xy_landmarks[i].x = norm_landmark.x();
      g_xy_landmarks[i].y = norm_landmark.y();
    }
}

void LandmarkListToxy_landmarks(const LandmarkList& norm_landmarks)
{
    VALIDATE(norm_landmarks.landmark_size() == mediapipe::kNumPoseLandmarks, str_cast(norm_landmarks.landmark_size()));
    int width = 1280;
    int height = 720;
    for (int i = 0; i < norm_landmarks.landmark_size(); ++i) {
      const auto& norm_landmark = norm_landmarks.landmark(i);
      g_xy_landmarks[i].x = norm_landmark.x();
      g_xy_landmarks[i].y = norm_landmark.y();
    }
}


namespace api2 {

namespace {

using ::mediapipe::NormalizedRect;
using ::mediapipe::Rect;
using ::mediapipe::landmarks_smoothing::GetObjectScale;
using ::mediapipe::landmarks_smoothing::InitializeLandmarksFilter;
using ::mediapipe::landmarks_smoothing::LandmarksFilter;
using ::mediapipe::landmarks_smoothing::LandmarksToNormalizedLandmarks;
using ::mediapipe::landmarks_smoothing::NormalizedLandmarksToLandmarks;

}  // namespace

class LandmarksSmoothingCalculatorImpl
    : public NodeImpl<LandmarksSmoothingCalculator> {
 public:
  absl::Status Open(CalculatorContext* cc) override {
    MP_ASSIGN_OR_RETURN(
        landmarks_filter_,
        InitializeLandmarksFilter(
            cc->Options<LandmarksSmoothingCalculatorOptions>()));
    return absl::OkStatus();
  }

  absl::Status Process(CalculatorContext* cc) override {
    // Check that landmarks are not empty and reset the filter if so.
    // Don't emit an empty packet for this timestamp.
    if ((kInNormLandmarks(cc).IsConnected() &&
         kInNormLandmarks(cc).IsEmpty()) ||
        (kInLandmarks(cc).IsConnected() && kInLandmarks(cc).IsEmpty())) {
      MP_RETURN_IF_ERROR(landmarks_filter_->Reset());
      return absl::OkStatus();
    }

    const auto& timestamp =
        absl::Microseconds(cc->InputTimestamp().Microseconds());

    if (kInNormLandmarks(cc).IsConnected()) {
      const auto& in_norm_landmarks = kInNormLandmarks(cc).Get();

      int image_width;
      int image_height;
      std::tie(image_width, image_height) = kImageSize(cc).Get();

      absl::optional<float> object_scale;
      if (kObjectScaleRoi(cc).IsConnected() && !kObjectScaleRoi(cc).IsEmpty()) {
        auto& roi = kObjectScaleRoi(cc).Get<NormalizedRect>();
        object_scale = GetObjectScale(roi, image_width, image_height);
      }

      auto in_landmarks = absl::make_unique<LandmarkList>();
      NormalizedLandmarksToLandmarks(in_norm_landmarks, image_width,
                                     image_height, *in_landmarks.get());

      auto out_landmarks = absl::make_unique<LandmarkList>();
      MP_RETURN_IF_ERROR(landmarks_filter_->Apply(
          *in_landmarks, timestamp, object_scale, *out_landmarks));

      auto out_norm_landmarks = absl::make_unique<NormalizedLandmarkList>();
      LandmarksToNormalizedLandmarks(*out_landmarks, image_width, image_height,
                                     *out_norm_landmarks.get());
      if (out_norm_landmarks->landmark_size() == mediapipe::kNumPoseLandmarks) {
        NormalizedLandmarksToxy_landmarks(*out_norm_landmarks.get());
      }
      // SDL_Log("%u LandmarksSmoothingCalculatorImpl::process, IsConnected, size: %i", SDL_GetTicks(), (int)out_norm_landmarks->landmark_size());
      kOutNormLandmarks(cc).Send(std::move(out_norm_landmarks));

    } else {
      const auto& in_landmarks = kInLandmarks(cc).Get();

      absl::optional<float> object_scale;
      if (kObjectScaleRoi(cc).IsConnected() && !kObjectScaleRoi(cc).IsEmpty()) {
        auto& roi = kObjectScaleRoi(cc).Get<Rect>();
        object_scale = GetObjectScale(roi);
      }

      auto out_landmarks = absl::make_unique<LandmarkList>();
      MP_RETURN_IF_ERROR(landmarks_filter_->Apply(
          in_landmarks, timestamp, object_scale, *out_landmarks));
      // LandmarkListToxy_landmarks(*out_landmarks.get());
      // SDL_Log("%u LandmarksSmoothingCalculatorImpl::process, Isn'tConnected, size: %i", SDL_GetTicks(), (int)out_landmarks->landmark_size());

      kOutLandmarks(cc).Send(std::move(out_landmarks));
    }

    return absl::OkStatus();
  }

 private:
  std::unique_ptr<LandmarksFilter> landmarks_filter_;
};
MEDIAPIPE_NODE_IMPLEMENTATION(LandmarksSmoothingCalculatorImpl);

}  // namespace api2
}  // namespace mediapipe
