/*
 * Copyright 2016 The Cartographer Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cartographer_ros/submap.h"

#include "absl/memory/memory.h"
#include "cartographer/common/port.h"
#include "cartographer/transform/transform.h"
#include "cartographer_ros/msg_conversion.h"
#include "cartographer_ros_msgs/StatusCode.h"
#include "cartographer_ros_msgs/SubmapQuery.h"

extern double calculate_yaw(const ::cartographer::transform::Rigid3d::Quaternion& rotation);

namespace cartographer_ros {

std::unique_ptr<::cartographer::io::SubmapTextures> FetchSubmapTextures(
    const ::cartographer::mapping::SubmapId& submap_id,
    ros::ServiceClient* client) {
  ::cartographer_ros_msgs::SubmapQuery srv;
  srv.request.trajectory_id = submap_id.trajectory_id;
  srv.request.submap_index = submap_id.submap_index;
  if (!client->call(srv) ||
      srv.response.status.code != ::cartographer_ros_msgs::StatusCode::OK) {
    return nullptr;
  }
  if (srv.response.textures.empty()) {
    return nullptr;
  }
  auto response = absl::make_unique<::cartographer::io::SubmapTextures>();
  response->version = srv.response.submap_version;
  const int s = srv.response.textures.size();
  int at = 0;
  for (const auto& texture : srv.response.textures) {
    const std::string compressed_cells(texture.cells.begin(),
                                       texture.cells.end());
    
    response->textures.emplace_back(::cartographer::io::SubmapTexture{
        ::cartographer::io::UnpackTextureData(compressed_cells, texture.width,
                                              texture.height),
        texture.width, texture.height, texture.resolution,
        ToRigid3d(texture.slice_pose)});
    const cartographer::io::SubmapTexture& tex = response->textures.back();
    if (cartographer_sdl_log) {
      ROS_INFO("FetchSubmapTextures[%i/%i]sid: %i, version: %i, (%i x %i) resolution: %.5f, slice_pose: %s, intensity.size: %i, alpha.size: %i", 
        at, s - 1, submap_id.submap_index, response->version, tex.width, tex.height, tex.resolution, tex.slice_pose.DebugString().c_str(),
        (int)tex.pixels.intensity.size(), (int)tex.pixels.alpha.size());
    }
    if (calculate_yaw(tex.slice_pose.rotation()) != 0) {
      ROS_INFO("FetchSubmapTextures[warning], rotation != 0, tex.slice_pose: %s", tex.slice_pose.DebugString().c_str()); 
      int ii = 0;
    }
    at ++;
  }
  return response;
}

}  // namespace cartographer_ros
