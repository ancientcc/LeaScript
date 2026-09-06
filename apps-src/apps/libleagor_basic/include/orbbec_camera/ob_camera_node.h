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

#pragma once
#include "types.h"
#include "utils.h"
#include "ros_sensor.h"
#include "ros/ros.h"
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/distortion_models.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/point_cloud2_iterator.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Transform.h>
#include <tf2/LinearMath/Vector3.h>
#include <tf2_ros/static_transform_broadcaster.h>
#include <tf2_ros/transform_broadcaster.h>
#include <condition_variable>
#include <thread>
#include <camera_info_manager/camera_info_manager.h>
#include <std_srvs/SetBool.h>
#include <std_srvs/Empty.h>
#include <boost/optional.hpp>

#include "rose_ros/aplt.hpp"
#include "base_slot.hpp"

#include "../dcamera/dcamera_device.hpp"


namespace orbbec_camera {
class OBCameraNode: public aplt::tdcamera_slot::treceiver
{
 public:
  OBCameraNode(ros::NodeHandle& nh, ros::NodeHandle& nh_private,
               std::shared_ptr<ob::Device> device);
  OBCameraNode(const OBCameraNode&) = delete;
  OBCameraNode& operator=(const OBCameraNode&) = delete;
  OBCameraNode(OBCameraNode&&) = delete;
  OBCameraNode& operator=(OBCameraNode&&) = delete;
  ~OBCameraNode();
  bool isInitialized() const;

  void ColorViewer_slice();

 private:
  // void resize_points(int size, int vsize);
  int getScan(ros::Time& start_time, float& scan_duration);
  void publish_scan(ros::Publisher& pub, const tdcframe_C& depth_frame1);

  //
  // aplt::tdcamera_slot::treceiver
  //
  void dcamera_did_frames(int task, const tdcframe_C* frames, int count) override;


  void readDefaultGain();

  void readDefaultExposure();

  void readDefaultWhiteBalance();

 private:
  ros::NodeHandle nh_;
  ros::NodeHandle nh_private_;
  aplt::tdcamera_device* device_;

  std::shared_ptr<ob::Device> device1_ = nullptr;
  std::map<stream_index_pair, std::shared_ptr<ROSOBSensor>> sensors_;
  std::map<stream_index_pair, std::string> stream_name_;

  std::map<stream_index_pair, int> default_gain_;
  std::map<stream_index_pair, int> default_exposure_;
  int default_white_balance_ = 0;

  bool is_initialized_ = false;

    // OBCameraParam camera_param_;
    tdcintrinsics_C intrinsic_;

    ros::Publisher LaserScan_pub_;
    ros::Time LaserScan_pre_time_;
    ros::Time LaserScan_time_;

    tfpoint3_buffer points_;
    int noised_frames_;
    int total_frames_;
};

}  // namespace orbbec_camera
