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
#include "mediapipe/framework/formats/landmark.pb.h"

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/statusor.h"

#include "mediapipe_api.hpp"
#include <SDL.h>
#include "rose_exception.hpp"
// #include "rose_string_utils.hpp"

#include "absl/base/log_severity.h"
// #include "absl/log/log.h"
#include <absl/log/globals.h>

#include <Eigen/Core>

using namespace std::placeholders;

extern ::absl::Flag<std::string> FLAGS_resource_root_dir;

namespace mediapipe {

// static const std::string mediapipe_dir = "C:/ddksample/apps-src/apps/external/mediapipe";

class PoseDetectionCpu : public Subgraph {
 public:
  absl::StatusOr<CalculatorGraphConfig> GetConfig(
      const SubgraphOptions& options) override {
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_detection/pose_detection_cpu.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_landmark/pose_detection_to_roi.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_landmark/pose_landmark_model_loader.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_landmark/tensors_to_pose_landmarks_and_segmentation.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_landmark/pose_landmarks_and_segmentation_inverse_projection.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_landmark/pose_landmark_by_roi_cpu.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_landmark/pose_landmark_filtering.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_landmark/pose_landmarks_to_roi.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_landmark/pose_segmentation_filtering.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/modules/pose_landmark/pose_landmark_cpu.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/graphs/pose_tracking/subgraphs/pose_landmarks_to_render_data.pbtxt";
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
    const std::string file = absl::GetFlag(FLAGS_resource_root_dir) + "/mediapipe/graphs/pose_tracking/subgraphs/pose_renderer_cpu.pbtxt";
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

class timpl_pose_tracking_api: public tpose_tracking_api
{
public:
    timpl_pose_tracking_api();
    ~timpl_pose_tracking_api();

    cv::Mat next_image(const cv::Mat& mat, bool flip_h, SDL_FPoint* norm_xy_landmarks, bool& is_less_than_min_interval);

private:
    absl::Status initial_graph();
    absl::Status next_image_internal(const cv::Mat& mat, bool flip_h, cv::Mat& ouput_mat, SDL_FPoint* norm_xy_landmarks);

private:
    char kInputStream[32];
    char kOutputStream[32];
    mediapipe::CalculatorGraph graph_;
    std::unique_ptr<mediapipe::OutputStreamPoller> poller_;
};

timpl_pose_tracking_api::timpl_pose_tracking_api()
    : kInputStream("input_video")
    , kOutputStream("output_video")
    // , kOutputStream("pose_landmarks")
{
    // absl::SetMinLogLevel(absl::LogSeverity::kInfo - 3);
    // absl::SetMinLogLevel(-3);
    // absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);
    // absl::SetMinLogLevel(absl::LogSeverityAtLeast::kWarning);
    // The added logs {dbg_mediapipe} are set to kWarning level. 
    // After removing those, set MinLogLevel to the kWarning level.
    absl::SetMinLogLevel(absl::LogSeverityAtLeast::kError);
    
    // absl::SetVLogLevel("my_module", 3);

    // SDL_Log("Eigen::nbThreads: %i", Eigen::nbThreads());
	// Eigen::setNbThreads(0);
    // SDL_Log("post Eigen::nbThreads: %i", Eigen::nbThreads());

    absl::Status status = initial_graph();
    if (!status.ok()) {
        return;
    }
    graph_initialized_ = true;
}

timpl_pose_tracking_api::~timpl_pose_tracking_api()
{
    absl::Status status = graph_.CloseInputStream(kInputStream);
    VALIDATE(status.ok(), null_str);
    status = graph_.WaitUntilDone();
    if (!status.ok()) {
        ABSL_LOG(ERROR) << "Failed to run the graph: " << status.message();
    }
}

absl::Status timpl_pose_tracking_api::initial_graph()
{
    // C:\ddksample\apps-res\data\core\tflites\mediapipe;
    const std::string resource_root_dir = game_config::path + "/data/core/tflites";
    absl::SetFlag(&FLAGS_resource_root_dir, resource_root_dir);

    // std::string file = "C:/ddksample/apps-src/apps/external/mediapipe/mediapipe/graphs/pose_tracking/pose_tracking_cpu.pbtxt";
    std::string file = resource_root_dir + "/mediapipe/graphs/pose_tracking/pose_tracking_cpu_rose.pbtxt";
    std::string calculator_graph_config_contents;

    MP_RETURN_IF_ERROR(mediapipe::file::GetContents(
      file,
      &calculator_graph_config_contents));

    ABSL_LOG(INFO) << "Get calculator graph config contents: "
                 << calculator_graph_config_contents;

    mediapipe::register_subgraph();

    mediapipe::CalculatorGraphConfig config =
        mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(
          calculator_graph_config_contents);

    ABSL_LOG(WARNING) << "Initialize the calculator graph.";
    MP_RETURN_IF_ERROR(graph_.Initialize(config));

    StatusOrPoller status_or_poller = graph_.AddOutputStreamPoller(kOutputStream);
    if (!status_or_poller.ok()) {
        return status_or_poller.status();
    }
    poller_.reset(new OutputStreamPoller(std::move(status_or_poller.value())));

    absl::Status status = graph_.StartRun({});
    if (!status.ok()) {
        return status;
    }

    return absl::OkStatus();
}

const SDL_Point POSE_CONNECTIONS[] = {
    {0, 1}, {0, 5}, {3, 7}, {5, 8}, {9, 10},
    {11, 12}, {23, 24},
    {11, 13}, {13, 15}, {15, 17}, {15, 21}, {15, 19}, {17, 19},
    {12, 14}, {14, 16}, {16, 18}, {16, 22}, {16, 20}, {18, 20},
    {11, 23}, {23, 25}, {25, 27}, {27, 29}, {27, 31}, {29, 31},
    {12, 24}, {24, 26}, {26, 28}, {28, 30}, {28, 32}, {30, 32},
};

SDL_FPoint g_xy_landmarks[mediapipe::kNumPoseLandmarks];

absl::Status timpl_pose_tracking_api::next_image_internal(const cv::Mat& camera_frame_raw, bool flip_h, cv::Mat& output_mat, SDL_FPoint* norm_xy_landmarks)
{
    VALIDATE(graph_initialized_, null_str);

    VALIDATE(!camera_frame_raw.empty(), null_str);

    cv::Mat camera_frame;
    cv::cvtColor(camera_frame_raw, camera_frame, cv::COLOR_BGR2RGB);
    if (flip_h) {
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
    MP_RETURN_IF_ERROR(graph_.AddPacketToInputStream(
        kInputStream, mediapipe::Adopt(input_frame.release())
                          .At(mediapipe::Timestamp(frame_timestamp_us))));

    OutputStreamPoller& poller = *poller_.get();

    // Get the graph result packet, or stop if that fails.
    mediapipe::Packet packet;

    memset(g_xy_landmarks, 0xff, sizeof(g_xy_landmarks));
    if (!poller.Next(&packet)) {
        return absl::Status(absl::StatusCode::kCancelled, "");
    }

/*
    while (poller.QueueSize() > 0) {
        if (!poller.Next(&packet)) {
            return absl::Status(absl::StatusCode::kCancelled, "");
        }
    }
*/

    if (SDL_strcasecmp(kOutputStream, "output_video") == 0) {
        auto& output_frame = packet.Get<mediapipe::ImageFrame>();

        // Convert back to opencv for display or saving.
        output_mat = mediapipe::formats::MatView(&output_frame);
        cv::cvtColor(output_mat, output_mat, cv::COLOR_RGB2BGR);

        if (output_mat.u == nullptr) {
            // it isn't in main thread. output_mat will use in main thread.
            output_mat = output_mat.clone();
        }

        memcpy(norm_xy_landmarks, g_xy_landmarks, sizeof(g_xy_landmarks));

    } else {
        VALIDATE(false, null_str);
        output_mat = camera_frame_raw.clone();
        std::stringstream ss;
        int image_width = output_mat.cols;
        int image_height = output_mat.rows;
        const mediapipe::NormalizedLandmarkList& landmarks = 
            packet.Get<mediapipe::NormalizedLandmarkList>();

        VALIDATE(norm_xy_landmarks != nullptr, null_str);

        VALIDATE(landmarks.landmark_size() == kNumPoseLandmarks, null_str);

        for (int i = 0; i < landmarks.landmark_size(); ++i) {
            const auto& landmark = landmarks.landmark(i);
            // int x = static_cast<int>(landmark.x() * image_width); // to pixel coor
            // int y = static_cast<int>(landmark.y() * image_height);
            // float z = landmark.z(); // relative depth

            // norm_xy_landmarks[i].x = x;
            // norm_xy_landmarks[i].y = y;

            norm_xy_landmarks[i].x = landmark.x();
            norm_xy_landmarks[i].y = landmark.y();
        }
    }

    return absl::OkStatus();
}

cv::Mat timpl_pose_tracking_api::next_image(const cv::Mat& mat, bool flip_h, SDL_FPoint* xy_landmarks, 
    bool& is_less_than_min_interval)
{
    cv::Mat result;

    if (!is_cpu_saver() && min_next_interval_no_cpu_saver_ != 0) {
		if (SDL_GetTicks() < next_next_new_ticks_) {
            is_less_than_min_interval = true;
			return result;
		}
		next_next_new_ticks_ = SDL_GetTicks() + min_next_interval_no_cpu_saver_;
		// SDL_Log("%u {dbg-cpu_saver}is_cpu_saver, new", SDL_GetTicks());
	}
    is_less_than_min_interval = false;

    absl::Status status = next_image_internal(mat, flip_h, result, xy_landmarks);
    if (status.ok()) {
        VALIDATE(!result.empty(), null_str);
    } else {
        VALIDATE(result.empty(), null_str);
    }
    return result;
}

tpose_tracking_api* create_pose_tracking_api()
{
	return new timpl_pose_tracking_api;
}


}

