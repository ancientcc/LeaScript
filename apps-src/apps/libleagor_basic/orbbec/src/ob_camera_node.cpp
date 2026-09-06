/*******************************************************************************
 * Copyright (c) 2023 Orbbec 3D Technology, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include "orbbec_camera/ob_camera_node.h"


#include "rose_exception.hpp"
// #include "libyuv/convert_argb.h"
#include "rose_sdl_utils.hpp"
#include <sensor_msgs/LaserScan.h>
#include "rose_thread.hpp"
#include <kdl/utilities/utility.h>
#include "rose_filesystem.hpp"

extern aplt::tdcamera_device* orbbec_create_device();

void dump_depth_profiles(const std::shared_ptr<ob::StreamProfileList>& depthProfiles)
{    
    ob::StreamProfileList& list = *depthProfiles.get();
    int count = list.count();
    SDL_Log("------dump_depth_profiles.size: %i------", count);
    for (uint32_t at = 0; at < count; at ++) {
        const std::shared_ptr<ob::StreamProfile>& profile = list.getProfile(at);

        const ob::VideoStreamProfile& video_profile = *profile->as<ob::VideoStreamProfile>().get();

        uint32_t width = video_profile.width();
        uint32_t height = video_profile.height();
        OBFormat format = video_profile.format();
        uint32_t fps = video_profile.fps();

        SDL_Log("[%i/%i]format: %i (%ix%i) fps: %i", at, count, format, (int)width, (int)height, (int)fps);
    }
    SDL_Log("-------------------------------");
}

namespace orbbec_camera {

OBCameraNode::OBCameraNode(ros::NodeHandle& nh, ros::NodeHandle& nh_private,
                           std::shared_ptr<ob::Device> device)
    : nh_(nh)
    , nh_private_(nh_private)
    , device_(orbbec_create_device())
    , device1_(std::move(device))
    , noised_frames_(0)
    , total_frames_(0)
{
    VALIDATE(device.get() == nullptr, null_str);
    intrinsic_.fx = 0;

    stream_name_[COLOR] = "color";
    stream_name_[DEPTH] = "depth";
    stream_name_[INFRA0] = "ir";
    stream_name_[INFRA1] = "ir2";
    stream_name_[ACCEL] = "accel";
    stream_name_[GYRO] = "gyro";

    LaserScan_pub_ = nh_.advertise<sensor_msgs::LaserScan>("camera_scan", 10);

    is_initialized_ = device_->main_start(dctask_depth, *this);
    if (is_initialized_) {
        // tdcintrinsics_C result;
        // camera_param_ = device_->get_intrinsics(true, &result);
        // VALIDATE(camera_param_.depthIntrinsic.fx != 0, null_str);
    }

    total_frames_ = 0;
    noised_frames_ = 0;
}

OBCameraNode::~OBCameraNode()
{
    ROS_INFO_STREAM("OBCameraNode::~OBCameraNode() start");

    device_->main_stop();

    ROS_INFO_STREAM("OBCameraNode::~OBCameraNode() end");

    if (device_ != nullptr) {
		delete device_;
		device_ = nullptr;
	}
}

void OBCameraNode::ColorViewer_slice()
{
    device_->main_slice();
}


void OBCameraNode::dcamera_did_frames(int task, const tdcframe_C* frames, int count)
{
    if (frames == nullptr) {
        return;
    }

    VALIDATE(task == dctask_depth, null_str);

    const tdcframe_C& depth_frame = frames[0];
    VALIDATE(depth_frame.type == dcframetype_depth, null_str);
    VALIDATE(count == 1, null_str);

    const int frame_width = depth_frame.width;
    const int frame_height = depth_frame.height;

    LaserScan_pre_time_ = LaserScan_time_;
	LaserScan_time_ = ros::Time::now();

    const bool from_file = false;
    if (!from_file) {
        publish_scan(LaserScan_pub_, depth_frame);

    } else {
        const std::string short_dbg_depth_data_dat = "dbg_depth_data-1280x800.dat";
	    const std::string depth_data_filename = game_config::preferences_dir + "/" + short_dbg_depth_data_dat;

        tdcdepth_data dcdepth_data;
	    dcdepth_data.load_from_file(depth_data_filename);
	    VALIDATE(dcdepth_data.valid(), null_str);

        const tdepth_file_header& header = dcdepth_data.header;
        VALIDATE(header.width == frame_width && header.height == frame_height, null_str);
        tdcframe_C depth_frame2;
        aplt::set_dcamera_frame(depth_frame2, dcframetype_depth, dcformat_y16,
			header.width, header.height, (const uint8_t*)dcdepth_data.data, dcdepth_data.data_len, header.depth_scale);
        publish_scan(LaserScan_pub_, depth_frame);
    }
}

int OBCameraNode::getScan(ros::Time& start_time, float& scan_duration) // get data and relative time
{
	start_time = LaserScan_pre_time_;
	scan_duration = (LaserScan_time_ - LaserScan_pre_time_).toSec();
	// SDL_Log("%u(3) LslidarDriver::getScan scan_during: %.3f", SDL_GetTicks(), scan_duration);
	return 1;
}

void OBCameraNode::publish_scan(ros::Publisher& pub, const tdcframe_C& depth_frame)
{
    uint32_t start_ticks = SDL_GetTicks();
    // SDL_Log("%u {ColorViewer_slice}publish_scan enter", start_ticks);

    // const int min_angle_degree = -40;
    // const int max_angle_degree = 40;
    // VALIDATE(max_angle_degree > min_angle_degree, null_str);

    // float angle_min = (float)DEG2RAD(min_angle_degree);
    // float angle_max = (float)DEG2RAD(max_angle_degree);
    const std::string frame_id = "camera"; // camera

    sensor_msgs::LaserScan scan_msg;

    ros::Time start_time;
	float scan_time;
	getScan(start_time, scan_time);

    scan_msg.header.stamp = start_time;
    scan_msg.header.frame_id = frame_id;
    // scan_count++;

    scan_msg.scan_time = scan_time;

    const int frame_width = depth_frame.width;
    const int frame_height = depth_frame.height;
    const uint16_t* depth_data = (const uint16_t*)depth_frame.data;

    double depth_scale = depth_frame.scale;
    int width = frame_width;
    int height = frame_height;

    if (intrinsic_.fx == 0) {
        device_->get_intrinsics(true, intrinsic_);
    }

    const tdcintrinsics_C& intrinsic = intrinsic_;

    double dcpitch = aplt::valuex.euler[1];
    tdepth_sector_result result = depth_sector_area(intrinsic, scan_msg, frame_width, frame_height, depth_data, depth_scale, nullptr, &points_, dcpitch);

    if (!result.noised) {
        if (result.left_no_depth || result.right_no_depth) {
            // set all data to obstacle
            float angle = result.angle_range.min;
            const double r = 0.10; // 0.10
            bool adjust;
            for (int at = 0; at < points_.vsize; at ++, angle += result.angle_increment) {
                adjust = (angle <= 0 && result.right_no_depth) || (angle >= 0 && result.left_no_depth);

                SDL_FPoint3& tmp = points_.data[at];
                if (adjust) {
                    tmp.x = r * cos(angle);
                    tmp.y = r * sin(angle);
                    tmp.z = result.min_z;
                }

                // SDL_Log("[%i/%i]%s. angle: %.3f(%.3f) xyz(%.3f, %.3f, %.3f)", at, points_.vsize, 
                //    adjust? "adjust": "not", angle, RAD2DEG(angle), tmp.x, tmp.y, tmp.z);
            }
        }

        aplt::tr_api& ros = aplt::get_r_api();
        ros.set_dcamera_points(points_.data, points_.vsize, result);

        if (pub.getNumSubscribers() > 0) {
            int node_count = scan_msg.ranges.size();
            float* range_data = &scan_msg.ranges[0];
            for (int at = 0; at < node_count; at ++) {
                if (range_data[at] == 0) {
                    range_data[at] = std::numeric_limits<float>::infinity();
                }
            }
            pub.publish(scan_msg);
        }
    } else {
        noised_frames_ ++;
    }
    total_frames_ ++;

    uint32_t stop_ticks = SDL_GetTicks();
    // SDL_Log("%u {ColorViewer_slice}(%ix%i) publish_scan exit, noised/total: (%i/%i) dcpitch: %.5f(deg:%.3f) noised: %s, elapse %u ms", 
    //    stop_ticks, width, height, noised_frames_, total_frames_, dcpitch, RAD2DEG(dcpitch), result.noised? "true": "false", stop_ticks - start_ticks);
}

bool OBCameraNode::isInitialized() const
{ 
    return is_initialized_; 
}

void OBCameraNode::readDefaultGain()
{
  for (const auto& stream_index : IMAGE_STREAMS) {
    // if (!enable_stream_[stream_index]) {
    //  continue;
    // }
    try {
      auto sensor = sensors_[stream_index];
      // CHECK_NOTNULL(sensor.get());
      auto gain = sensor->getGain();
      ROS_INFO_STREAM("stream " << stream_name_[stream_index] << " gain " << gain);
      default_gain_[stream_index] = gain;
    } catch (ob::Error& e) {
      default_gain_[stream_index] = 0;
      ROS_ERROR_STREAM("get gain error " << e.getMessage());
    }
  }
}

void OBCameraNode::readDefaultExposure()
{
  for (const auto& stream_index : IMAGE_STREAMS) {
    // if (!enable_stream_[stream_index]) {
    //  continue;
    // }
    try {
      auto sensor = sensors_[stream_index];
      // CHECK_NOTNULL(sensor.get());
      auto exposure = sensor->getExposure();
      ROS_INFO_STREAM("stream " << stream_name_[stream_index] << " exposure " << exposure);
      default_exposure_[stream_index] = exposure;
    } catch (ob::Error& e) {
      default_exposure_[stream_index] = 0;
      ROS_ERROR_STREAM("get exposure error " << e.getMessage());
    }
  }
}

void OBCameraNode::readDefaultWhiteBalance()
{
  try {
    auto sensor = sensors_[COLOR];
    if (!sensor) {
      ROS_INFO_STREAM("does not have color sensor");
      return;
    }
    // CHECK_NOTNULL(sensor.get());
    auto wb = sensor->getWhiteBalance();
    ROS_INFO_STREAM("stream " << stream_name_[COLOR] << " wb " << wb);
    default_white_balance_ = wb;
  } catch (ob::Error& e) {
    default_white_balance_ = 0;
    ROS_WARN_STREAM("get white balance error " << e.getMessage());
  }
}

}  // namespace orbbec_camera
