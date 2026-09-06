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

#include "orbbec_camera/utils.h"
#include <tf2/LinearMath/Quaternion.h>
#include "sensor_msgs/PointCloud2.h"
#include "sensor_msgs/PointCloud.h"

#include "sensor_msgs/point_cloud2_iterator.h"
#include "sensor_msgs/point_cloud_conversion.h"
#include "ros/ros.h"

namespace orbbec_camera {


std::string ObDeviceTypeToString(const OBDeviceType &type) {
  switch (type) {
    case OBDeviceType::OB_STRUCTURED_LIGHT_BINOCULAR_CAMERA:
      return "structured light binocular camera";
    case OBDeviceType::OB_STRUCTURED_LIGHT_MONOCULAR_CAMERA:
      return "structured light monocular camera";
    case OBDeviceType::OB_TOF_CAMERA:
      return "tof camera";
  }
  return "unknown technology camera";
}

sensor_msgs::CameraInfo convertToCameraInfo(OBCameraIntrinsic intrinsic,
                                            OBCameraDistortion distortion, int width) {
  (void)width;
  sensor_msgs::CameraInfo info;
  info.distortion_model = sensor_msgs::distortion_models::PLUMB_BOB;
  info.width = intrinsic.width;
  info.height = intrinsic.height;
  info.D.resize(5, 0.0);
  info.D[0] = distortion.k1;
  info.D[1] = distortion.k2;
  info.D[2] = distortion.p1;
  info.D[3] = distortion.p2;
  info.D[4] = distortion.k3;

  info.K.fill(0.0);
  info.K[0] = intrinsic.fx;
  info.K[2] = intrinsic.cx;
  info.K[4] = intrinsic.fy;
  info.K[5] = intrinsic.cy;
  info.K[8] = 1.0;

  info.R.fill(0.0);
  info.R[0] = 1;
  info.R[4] = 1;
  info.R[8] = 1;

  info.P.fill(0.0);
  info.P[0] = info.K[0];
  info.P[2] = info.K[2];
  info.P[5] = info.K[4];
  info.P[6] = info.K[5];
  info.P[10] = 1.0;
  return info;
}

tf2::Quaternion rotationMatrixToQuaternion(const float rotation[9]) {
  Eigen::Matrix3f m;
  // We need to be careful about the order, as RS2 rotation matrix is
  // column-major, while Eigen::Matrix3f expects row-major.
  m << rotation[0], rotation[3], rotation[6], rotation[1], rotation[4], rotation[7], rotation[2],
      rotation[5], rotation[8];
  Eigen::Quaternionf q(m);
  return {q.x(), q.y(), q.z(), q.w()};
}

std::ostream &operator<<(std::ostream &os, const OBCameraParam &rhs) {
  auto depth_intrinsic = rhs.depthIntrinsic;
  auto rgb_intrinsic = rhs.rgbIntrinsic;
  os << "=====depth intrinsic=====\n";
  os << "fx : " << depth_intrinsic.fx << "\n";
  os << "fy : " << depth_intrinsic.fy << "\n";
  os << "cx : " << depth_intrinsic.cx << "\n";
  os << "cy : " << depth_intrinsic.cy << "\n";
  os << "width : " << depth_intrinsic.width << "\n";
  os << "height : " << depth_intrinsic.height << "\n";
  os << "=====rgb intrinsic=====\n";
  os << "fx : " << rgb_intrinsic.fx << "\n";
  os << "fy : " << rgb_intrinsic.fy << "\n";
  os << "cx : " << rgb_intrinsic.cx << "\n";
  os << "cy : " << rgb_intrinsic.cy << "\n";
  os << "width : " << rgb_intrinsic.width << "\n";
  os << "height : " << rgb_intrinsic.height << "\n";
  return os;
}

ros::Time frameTimeStampToROSTime(uint64_t ms) {
  auto total = static_cast<uint64_t>(ms * 1e6);
  uint64_t sec = total / 1000000000;
  uint64_t nano_sec = total % 1000000000;
  ros::Time stamp(sec, nano_sec);
  return stamp;
}

bool isOpenNIDevice(int pid) {
  static const std::vector<int> OPENNI_DEVICE_PIDS = {
      0x0300, 0x0301, 0x0400, 0x0401, 0x0402, 0x0403, 0x0404, 0x0407, 0x0601, 0x060b,
      0x060e, 0x060f, 0x0610, 0x0613, 0x0614, 0x0616, 0x0617, 0x0618, 0x061b, 0x062b,
      0x062c, 0x062d, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637, 0x0638, 0x0639,
      0x063a, 0x0650, 0x0651, 0x0654, 0x0655, 0x0656, 0x0657, 0x0658, 0x0659, 0x065a,
      0x065b, 0x065c, 0x065d, 0x0698, 0x0699, 0x069a, 0x055c, 0x065e, 0x06a0};

  for (const auto &pid_openni : OPENNI_DEVICE_PIDS) {
    if (pid == pid_openni) {
      return true;
    }
  }
  return false;
}

OBMultiDeviceSyncMode OBSyncModeFromString(const std::string &mode) {
  if (mode == "FREE_RUN") {
    return OBMultiDeviceSyncMode::OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN;
  } else if (mode == "STANDALONE") {
    return OBMultiDeviceSyncMode::OB_MULTI_DEVICE_SYNC_MODE_STANDALONE;
  } else if (mode == "PRIMARY") {
    return OBMultiDeviceSyncMode::OB_MULTI_DEVICE_SYNC_MODE_PRIMARY;
  } else if (mode == "SECONDARY") {
    return OBMultiDeviceSyncMode::OB_MULTI_DEVICE_SYNC_MODE_SECONDARY;
  } else if (mode == "SECONDARY_SYNCED") {
    return OBMultiDeviceSyncMode::OB_MULTI_DEVICE_SYNC_MODE_SECONDARY_SYNCED;
  } else if (mode == "SOFTWARE_TRIGGERING") {
    return OBMultiDeviceSyncMode::OB_MULTI_DEVICE_SYNC_MODE_SOFTWARE_TRIGGERING;
  } else if (mode == "HARDWARE_TRIGGERING") {
    return OBMultiDeviceSyncMode::OB_MULTI_DEVICE_SYNC_MODE_HARDWARE_TRIGGERING;
  } else {
    return OBMultiDeviceSyncMode::OB_MULTI_DEVICE_SYNC_MODE_FREE_RUN;
  }
}

OB_SAMPLE_RATE sampleRateFromString(std::string &sample_rate) {
  // covert to lower case
  std::transform(sample_rate.begin(), sample_rate.end(), sample_rate.begin(), ::tolower);
  if (sample_rate == "1.5625hz") {
    return OB_SAMPLE_RATE_1_5625_HZ;
  } else if (sample_rate == "3.125hz") {
    return OB_SAMPLE_RATE_3_125_HZ;
  } else if (sample_rate == "6.25hz") {
    return OB_SAMPLE_RATE_6_25_HZ;
  } else if (sample_rate == "12.5hz") {
    return OB_SAMPLE_RATE_12_5_HZ;
  } else if (sample_rate == "25hz") {
    return OB_SAMPLE_RATE_25_HZ;
  } else if (sample_rate == "50hz") {
    return OB_SAMPLE_RATE_50_HZ;
  } else if (sample_rate == "100hz") {
    return OB_SAMPLE_RATE_100_HZ;
  } else if (sample_rate == "200hz") {
    return OB_SAMPLE_RATE_200_HZ;
  } else if (sample_rate == "500hz") {
    return OB_SAMPLE_RATE_500_HZ;
  } else if (sample_rate == "1khz") {
    return OB_SAMPLE_RATE_1_KHZ;
  } else if (sample_rate == "2khz") {
    return OB_SAMPLE_RATE_2_KHZ;
  } else if (sample_rate == "4khz") {
    return OB_SAMPLE_RATE_4_KHZ;
  } else if (sample_rate == "8khz") {
    return OB_SAMPLE_RATE_8_KHZ;
  } else if (sample_rate == "16khz") {
    return OB_SAMPLE_RATE_16_KHZ;
  } else if (sample_rate == "32khz") {
    return OB_SAMPLE_RATE_32_KHZ;
  } else {
    ROS_ERROR_STREAM("Unknown OB_SAMPLE_RATE: " << sample_rate);
    return OB_SAMPLE_RATE_100_HZ;
  }
}

std::string sampleRateToString(const OB_SAMPLE_RATE &sample_rate) {
  switch (sample_rate) {
    case OB_SAMPLE_RATE_1_5625_HZ:
      return "1.5625Hz";
    case OB_SAMPLE_RATE_3_125_HZ:
      return "3.125Hz";
    case OB_SAMPLE_RATE_6_25_HZ:
      return "6.25Hz";
    case OB_SAMPLE_RATE_12_5_HZ:
      return "12.5Hz";
    case OB_SAMPLE_RATE_25_HZ:
      return "25Hz";
    case OB_SAMPLE_RATE_50_HZ:
      return "50Hz";
    case OB_SAMPLE_RATE_100_HZ:
      return "100Hz";
    case OB_SAMPLE_RATE_200_HZ:
      return "200Hz";
    case OB_SAMPLE_RATE_500_HZ:
      return "500Hz";
    case OB_SAMPLE_RATE_1_KHZ:
      return "1kHz";
    case OB_SAMPLE_RATE_2_KHZ:
      return "2kHz";
    case OB_SAMPLE_RATE_4_KHZ:
      return "4kHz";
    case OB_SAMPLE_RATE_8_KHZ:
      return "8kHz";
    case OB_SAMPLE_RATE_16_KHZ:
      return "16kHz";
    case OB_SAMPLE_RATE_32_KHZ:
      return "32kHz";
    default:
      return "100Hz";
  }
}

OB_GYRO_FULL_SCALE_RANGE fullGyroScaleRangeFromString(std::string &full_scale_range) {
  std::transform(full_scale_range.begin(), full_scale_range.end(), full_scale_range.begin(),
                 ::tolower);
  if (full_scale_range == "16dps") {
    return OB_GYRO_FS_16dps;
  } else if (full_scale_range == "31dps") {
    return OB_GYRO_FS_31dps;
  } else if (full_scale_range == "62dps") {
    return OB_GYRO_FS_62dps;
  } else if (full_scale_range == "125dps") {
    return OB_GYRO_FS_125dps;
  } else if (full_scale_range == "250dps") {
    return OB_GYRO_FS_250dps;
  } else if (full_scale_range == "500dps") {
    return OB_GYRO_FS_500dps;
  } else if (full_scale_range == "1000dps") {
    return OB_GYRO_FS_1000dps;
  } else if (full_scale_range == "2000dps") {
    return OB_GYRO_FS_2000dps;
  } else {
    ROS_ERROR_STREAM("Unknown OB_GYRO_FULL_SCALE_RANGE: " << full_scale_range);
    return OB_GYRO_FS_2000dps;
  }
}

std::string fullGyroScaleRangeToString(const OB_GYRO_FULL_SCALE_RANGE &full_scale_range) {
  switch (full_scale_range) {
    case OB_GYRO_FS_16dps:
      return "16dps";
    case OB_GYRO_FS_31dps:
      return "31dps";
    case OB_GYRO_FS_62dps:
      return "62dps";
    case OB_GYRO_FS_125dps:
      return "125dps";
    case OB_GYRO_FS_250dps:
      return "250dps";
    case OB_GYRO_FS_500dps:
      return "500dps";
    case OB_GYRO_FS_1000dps:
      return "1000dps";
    case OB_GYRO_FS_2000dps:
      return "2000dps";
    default:
      return "16dps";
  }
}

OBAccelFullScaleRange fullAccelScaleRangeFromString(std::string &full_scale_range) {
  std::transform(full_scale_range.begin(), full_scale_range.end(), full_scale_range.begin(),
                 ::tolower);
  if (full_scale_range == "2g") {
    return OB_ACCEL_FS_2g;
  } else if (full_scale_range == "4g") {
    return OB_ACCEL_FS_4g;
  } else if (full_scale_range == "8g") {
    return OB_ACCEL_FS_8g;
  } else if (full_scale_range == "16g") {
    return OB_ACCEL_FS_16g;
  } else {
    ROS_ERROR_STREAM("Unknown OB_ACCEL_FULL_SCALE_RANGE: " << full_scale_range);
    return OB_ACCEL_FS_16g;
  }
}

std::string fullAccelScaleRangeToString(const OBAccelFullScaleRange &full_scale_range) {
  switch (full_scale_range) {
    case OB_ACCEL_FS_2g:
      return "2g";
    case OB_ACCEL_FS_4g:
      return "4g";
    case OB_ACCEL_FS_8g:
      return "8g";
    case OB_ACCEL_FS_16g:
      return "16g";
    default:
      return "2g";
  }
}

bool isValidJPEG(const std::shared_ptr<ob::ColorFrame> &frame) {
  if (frame->dataSize() < 2) {  // Checking both start and end markers, so minimal size is 4
    return false;
  }

  const auto *data = static_cast<const uint8_t *>(frame->data());

  // Check for JPEG start marker
  if (data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }

  return true;
}

std::string fourccToString(const uint32_t fourcc) {
  std::string str;
  str += (fourcc & 0xFF);
  str += ((fourcc >> 8) & 0xFF);
  str += ((fourcc >> 16) & 0xFF);
  str += ((fourcc >> 24) & 0xFF);
  return str;
}

}  // namespace orbbec_camera
