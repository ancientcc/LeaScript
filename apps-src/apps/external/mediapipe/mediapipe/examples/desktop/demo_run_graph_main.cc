// Copyright 2019 The MediaPipe Authors.
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
//
// An example of sending OpenCV webcam frames into a MediaPipe graph.
#include <cstdlib>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/absl_log.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/image_frame_opencv.h"
#include "mediapipe/framework/port/file_helpers.h"
// #include "mediapipe/framework/port/opencv_highgui_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
// #include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/util/resource_util.h"
#include "rose_sdl_utils.hpp"
#include "rose_config_3rdparty.hpp"
#include "mediapipe/rose/macros.h"

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/statusor.h"

constexpr char kInputStream[] = "input_video";
constexpr char kOutputStream[] = "output_video";
constexpr char kWindowName[] = "MediaPipe";

ABSL_FLAG(std::string, calculator_graph_config_file, "",
          "Name of file containing text format CalculatorGraphConfig proto.");
ABSL_FLAG(std::string, input_video_path, "",
          "Full path of video to load. "
          "If not provided, attempt to use a webcam.");
ABSL_FLAG(std::string, output_video_path, "",
          "Full path of where to save result (.mp4 only). "
          "If not provided, show result in a window.");

extern ::absl::Flag<std::string> FLAGS_resource_root_dir;



namespace mediapipe {

static const std::string mediapipe_dir = "C:/ddksample/apps-src/apps/external/mediapipe";

class PoseDetectionCpu : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_detection/pose_detection_cpu.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseDetectionToRoi : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_landmark/pose_detection_to_roi.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseLandmarkModelLoader : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_landmark/pose_landmark_model_loader.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class TensorsToPoseLandmarksAndSegmentation : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_landmark/tensors_to_pose_landmarks_and_segmentation.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseLandmarksAndSegmentationInverseProjection : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_landmark/pose_landmarks_and_segmentation_inverse_projection.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseLandmarkByRoiCpu : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_landmark/pose_landmark_by_roi_cpu.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseLandmarkFiltering : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_landmark/pose_landmark_filtering.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseLandmarksToRoi : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_landmark/pose_landmarks_to_roi.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseSegmentationFiltering : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_landmark/pose_segmentation_filtering.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseLandmarkCpu : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/modules/pose_landmark/pose_landmark_cpu.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseLandmarksToRenderData : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/graphs/pose_tracking/subgraphs/pose_landmarks_to_render_data.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

class PoseRendererCpu : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = mediapipe_dir + "/mediapipe/graphs/pose_tracking/subgraphs/pose_renderer_cpu.pbtxt";
    std::string calculator_graph_config_contents;
    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(file, &calculator_graph_config_contents));

    CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<CalculatorGraphConfig>(calculator_graph_config_contents);
    return config;
  }
};

static void register_subgraph()
{
    REGISTER_MEDIAPIPE_GRAPH(PoseDetectionToRoi);
    REGISTER_MEDIAPIPE_GRAPH(PoseDetectionCpu);
    REGISTER_MEDIAPIPE_GRAPH(PoseLandmarkCpu);
    REGISTER_MEDIAPIPE_GRAPH(PoseLandmarkModelLoader);
    REGISTER_MEDIAPIPE_GRAPH(TensorsToPoseLandmarksAndSegmentation);
    REGISTER_MEDIAPIPE_GRAPH(PoseLandmarksAndSegmentationInverseProjection);
    REGISTER_MEDIAPIPE_GRAPH(PoseLandmarkByRoiCpu);
    REGISTER_MEDIAPIPE_GRAPH(PoseLandmarkFiltering);
    REGISTER_MEDIAPIPE_GRAPH(PoseLandmarksToRoi);
    REGISTER_MEDIAPIPE_GRAPH(PoseSegmentationFiltering);

    REGISTER_MEDIAPIPE_GRAPH(PoseLandmarksToRenderData);
    REGISTER_MEDIAPIPE_GRAPH(PoseRendererCpu);
}

}

absl::Status RunMPPGraph() {
  std::string calculator_graph_config_contents;
  MP_RETURN_IF_ERROR(mediapipe::file::GetContents(
      absl::GetFlag(FLAGS_calculator_graph_config_file),
      &calculator_graph_config_contents));
  ABSL_LOG(INFO) << "Get calculator graph config contents: "
                 << calculator_graph_config_contents;

  mediapipe::register_subgraph();

  mediapipe::CalculatorGraphConfig config =
      mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(
          calculator_graph_config_contents);

  ABSL_LOG(INFO) << "Initialize the calculator graph.";
  mediapipe::CalculatorGraph graph;
  MP_RETURN_IF_ERROR(graph.Initialize(config));

  ABSL_LOG(INFO) << "Initialize the camera or load the video.";
/*
  cv::VideoCapture capture;
  const bool load_video = !absl::GetFlag(FLAGS_input_video_path).empty();
  if (load_video) {
    capture.open(absl::GetFlag(FLAGS_input_video_path));
  } else {
    capture.open(0);
  }
  RET_CHECK(capture.isOpened());

  cv::VideoWriter writer;
  const bool save_video = !absl::GetFlag(FLAGS_output_video_path).empty();
  if (!save_video) {
    cv::namedWindow(kWindowName, 1);
#if (CV_MAJOR_VERSION >= 3) && (CV_MINOR_VERSION >= 2)
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    capture.set(cv::CAP_PROP_FPS, 30);
#endif
  }
*/
  const bool load_video = true;
  bool save_video = false;

  std::vector<std::string> files;
  files.push_back("owner-ydc1-1280x720.jpg");
  files.push_back("owner-wlk-1-1280x720.jpg");
  files.push_back("owner-wlk-2-1280x720.jpg");

  // surface surf = image::rose_get_image(game_config::preferences_dir + "/owner-ydc1.jpg");
  // surface surf = image::rose_get_image(game_config::preferences_dir + "/owner-wlk-1.jpg");
  // surface surf = image::rose_get_image(game_config::preferences_dir + "/owner-wlk-2.jpg");
  // tsurface_2_mat_lock lock(surf);

  ABSL_LOG(INFO) << "Start running the calculator graph.";
  MP_ASSIGN_OR_RETURN(mediapipe::OutputStreamPoller poller,
                      graph.AddOutputStreamPoller(kOutputStream));
  MP_RETURN_IF_ERROR(graph.StartRun({}));

  ABSL_LOG(INFO) << "Start grabbing and processing frames.";
  bool grab_frames = true;
  // while (grab_frames) {
  for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
	const std::string& short_file = *it;
	const std::string file = game_config::preferences_dir + "/" + short_file;
    surface surf = image::rose_get_image(file);
    VALIDATE(surf.get() != nullptr, null_str);
    tsurface_2_mat_lock lock(surf);

    // Capture opencv camera or video frame.
    // cv::Mat camera_frame_raw;
    // capture >> camera_frame_raw;
    cv::Mat camera_frame_raw = lock.mat;
    if (camera_frame_raw.empty()) {
      if (!load_video) {
        ABSL_LOG(INFO) << "Ignore empty frames from camera.";
        continue;
      }
      ABSL_LOG(INFO) << "Empty frame, end of video reached.";
      break;
    }
    cv::Mat camera_frame;
    cv::cvtColor(camera_frame_raw, camera_frame, cv::COLOR_BGR2RGB);
    if (!load_video) {
      cv::flip(camera_frame, camera_frame, /*flipcode=HORIZONTAL*/ 1);
    }

    // Wrap Mat into an ImageFrame.
    auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB, camera_frame.cols, camera_frame.rows,
        mediapipe::ImageFrame::kDefaultAlignmentBoundary);
    cv::Mat input_frame_mat = mediapipe::formats::MatView(input_frame.get());
    camera_frame.copyTo(input_frame_mat);

    // Send image packet into the graph.
    size_t frame_timestamp_us =
        (double)cv::getTickCount() / (double)cv::getTickFrequency() * 1e6;
    MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
        kInputStream, mediapipe::Adopt(input_frame.release())
                          .At(mediapipe::Timestamp(frame_timestamp_us))));
   
    // Get the graph result packet, or stop if that fails.
    mediapipe::Packet packet;
    if (!poller.Next(&packet)) {
        break;
    }
    auto& output_frame = packet.Get<mediapipe::ImageFrame>();

    // Convert back to opencv for display or saving.
    cv::Mat output_frame_mat = mediapipe::formats::MatView(&output_frame);
    cv::cvtColor(output_frame_mat, output_frame_mat, cv::COLOR_RGB2BGR);

    cv::Mat output_frame_mat1= output_frame_mat.clone();
    imwrite(output_frame_mat1, "1-wlk-1.png");

    grab_frames = false;
/*
    if (save_video) {
      if (!writer.isOpened()) {
        ABSL_LOG(INFO) << "Prepare video writer.";
        writer.open(absl::GetFlag(FLAGS_output_video_path),
                    mediapipe::fourcc('a', 'v', 'c', '1'),  // .mp4
                    capture.get(cv::CAP_PROP_FPS), output_frame_mat.size());
        RET_CHECK(writer.isOpened());
      }
      writer.write(output_frame_mat);
    } else {
      cv::imshow(kWindowName, output_frame_mat);
      // Press any key to exit.
      const int pressed_key = cv::waitKey(5);
      if (pressed_key >= 0 && pressed_key != 255) grab_frames = false;
    }
*/
  }

  ABSL_LOG(INFO) << "Shutting down.";
  // if (writer.isOpened()) writer.release();
  MP_RETURN_IF_ERROR(graph.CloseInputStream(kInputStream));
  return graph.WaitUntilDone();
}

// int main(int argc, char** argv) {
// MEDIAPIPE_API int demo_run_pose_tracking() {
int demo_run_pose_tracking() {
/*
  char argv0[] = "pose_tracking";
  char argv1[] = "-calculator_graph_config_file";
  char argv2[] = "C:/ddksample/apps-src/apps/external/mediapipe/mediapipe/graphs/pose_tracking/pose_tracking_cpu.pbtxt";
  char argv3[] = "-resource_root_dir";
  char argv4[] = "C:/ddksample/apps-src/apps/external/mediapipe";
  char* argvp[] = {argv0, argv1, argv2, argv3, argv4};
  char** argv = argvp;
  int argc = sizeof(argvp) / sizeof(argvp[0]);

  google::InitGoogleLogging(argv[0]);
  absl::ParseCommandLine(argc, argv);
*/
  absl::SetFlag(&FLAGS_calculator_graph_config_file, "C:/ddksample/apps-src/apps/external/mediapipe/mediapipe/graphs/pose_tracking/pose_tracking_cpu.pbtxt");
  absl::SetFlag(&FLAGS_resource_root_dir, "C:/ddksample/apps-src/apps/external/mediapipe");

  absl::Status run_status = RunMPPGraph();
  if (!run_status.ok()) {
    ABSL_LOG(ERROR) << "Failed to run the graph: " << run_status.message();
    return EXIT_FAILURE;
  } else {
    ABSL_LOG(INFO) << "Success!";
  }
  return EXIT_SUCCESS;
}
