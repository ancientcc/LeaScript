/*
 * Copyright 2017 The Cartographer Authors
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

#include "cartographer/mapping/pose_extrapolator.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "cartographer/transform/transform.h"
#include "glog/logging.h"

#include <SDL_log.h>
#include <rose_ros/cartographer_utils.h>
#include "aplt_clazz.hpp"

extern double calculate_yaw(const ::cartographer::transform::Rigid3d::Quaternion& rotation);
extern bool gcartoverbose;

namespace cartographer {
namespace mapping {

PoseExtrapolator::PoseExtrapolator(const common::Duration pose_queue_duration,
                                   double imu_gravity_time_constant, bool in_node)
    : pose_queue_duration_(pose_queue_duration),
      gravity_time_constant_(imu_gravity_time_constant),
      cached_extrapolated_pose_{common::Time::min(),
                                transform::Rigid3d::Identity()},
    in_node_(in_node)
{}

std::unique_ptr<PoseExtrapolator> PoseExtrapolator::InitializeWithImu(
    const common::Duration pose_queue_duration,
    const double imu_gravity_time_constant, const sensor::ImuData& imu_data) {
  auto extrapolator = absl::make_unique<PoseExtrapolator>(
      pose_queue_duration, imu_gravity_time_constant);
  extrapolator->AddImuData(imu_data);
  extrapolator->imu_tracker_ =
      absl::make_unique<ImuTracker>(imu_gravity_time_constant, imu_data.time);
  extrapolator->imu_tracker_->AddImuLinearAccelerationObservation(
      imu_data.linear_acceleration);
  extrapolator->imu_tracker_->AddImuAngularVelocityObservation(
      imu_data.angular_velocity);
  extrapolator->imu_tracker_->Advance(imu_data.time);
  extrapolator->AddPose(
      imu_data.time,
      transform::Rigid3d::Rotation(extrapolator->imu_tracker_->orientation()));
  return extrapolator;
}

common::Time PoseExtrapolator::GetLastPoseTime() const {
  if (timed_pose_queue_.empty()) {
    return common::Time::min();
  }
  return timed_pose_queue_.back().time;
}

common::Time PoseExtrapolator::GetLastExtrapolatedTime() const {
  if (!extrapolation_imu_tracker_) {
    return common::Time::min();
  }
  return extrapolation_imu_tracker_->time();
}

void PoseExtrapolator::AddPose(const common::Time time,
                               const transform::Rigid3d& pose) {
  if (!in_node_) {
    if (cartographer_sdl_log) {
      SDL_Log("[%s]PoseExtrapolator::AddPose, imu_tracker_: %p, timed_pose_queue_.size: %u, time: %lli, pose: %.5f, pose_queue_duration_: %.5f(size: %i), imu_data_.size: %i", 
        in_node_? "node": "", imu_tracker_.get(), (uint32_t)timed_pose_queue_.size(), ToUniversal(time), calculate_yaw(pose.rotation()), 
        cartographer::common::ToSeconds(pose_queue_duration_), (int)timed_pose_queue_.size(), (int)imu_data_.size());
    }
  }

  if (imu_tracker_ == nullptr) {
    common::Time tracker_start = time;
    if (!imu_data_.empty()) {
      tracker_start = std::min(tracker_start, imu_data_.front().time);
    }
    imu_tracker_ =
        absl::make_unique<ImuTracker>(gravity_time_constant_, tracker_start);
    if (!in_node_) {
        imu_tracker_->SetVerbose(true);
    }
  }
  timed_pose_queue_.push_back(TimedPose{time, pose});
  while (timed_pose_queue_.size() > 2 &&
         timed_pose_queue_[1].time <= time - pose_queue_duration_) {
    timed_pose_queue_.pop_front();
  }
  UpdateVelocitiesFromPoses();
  AdvanceImuTracker(time, imu_tracker_.get());
  TrimImuData();
  TrimOdometryData();
  odometry_imu_tracker_ = absl::make_unique<ImuTracker>(*imu_tracker_);
  extrapolation_imu_tracker_ = absl::make_unique<ImuTracker>(*imu_tracker_);
  extrapolation_imu_tracker_->SetVerbose(false);
}

void PoseExtrapolator::AddImuData(const sensor::ImuData& imu_data) {
  CHECK(timed_pose_queue_.empty() ||
        imu_data.time >= timed_pose_queue_.back().time);
  imu_data_.push_back(imu_data);
  TrimImuData();
}

void PoseExtrapolator::AddOdometryData(
    const sensor::OdometryData& odometry_data) {
  CHECK(timed_pose_queue_.empty() ||
        odometry_data.time >= timed_pose_queue_.back().time);
  odometry_data_.push_back(odometry_data);
  TrimOdometryData();
  if (odometry_data_.size() < 2) {
    return;
  }
  // TODO(whess): Improve by using more than just the last two odometry poses.
  // Compute extrapolation in the tracking frame.
  const sensor::OdometryData& odometry_data_oldest = odometry_data_.front();
  const sensor::OdometryData& odometry_data_newest = odometry_data_.back();
  const double odometry_time_delta =
      common::ToSeconds(odometry_data_oldest.time - odometry_data_newest.time);
  const transform::Rigid3d odometry_pose_delta =
      odometry_data_newest.pose.inverse() * odometry_data_oldest.pose;
  angular_velocity_from_odometry_ =
      transform::RotationQuaternionToAngleAxisVector(
          odometry_pose_delta.rotation()) /
      odometry_time_delta;
  if (timed_pose_queue_.empty()) {
    return;
  }
  const Eigen::Vector3d
      linear_velocity_in_tracking_frame_at_newest_odometry_time =
          odometry_pose_delta.translation() / odometry_time_delta;
  const Eigen::Quaterniond orientation_at_newest_odometry_time =
      timed_pose_queue_.back().pose.rotation() *
      ExtrapolateRotation(odometry_data_newest.time,
                          odometry_imu_tracker_.get());
  linear_velocity_from_odometry_ =
      orientation_at_newest_odometry_time *
      linear_velocity_in_tracking_frame_at_newest_odometry_time;
}

transform::Rigid3d PoseExtrapolator::ExtrapolatePose(const common::Time time) {
  const TimedPose& newest_timed_pose = timed_pose_queue_.back();
  CHECK_GE(time, newest_timed_pose.time);
  if (cached_extrapolated_pose_.time != time) {
    if (!in_node_ && gcartoverbose) {
        extrapolation_imu_tracker_->SetVerbose(true);
    }
    if (!in_node_ && gcartoverbose) {
        extrapolation_imu_tracker_->SetVerbose(true);
        Eigen::Vector3d trans = ExtrapolateTranslation(time);
        const Eigen::Vector3d translation = trans + newest_timed_pose.pose.translation();

        const Eigen::Quaterniond rot = ExtrapolateRotation(time, extrapolation_imu_tracker_.get());
        const Eigen::Quaterniond rotation =
            newest_timed_pose.pose.rotation() * rot;

        cached_extrapolated_pose_ =
            TimedPose{time, transform::Rigid3d{translation, rotation}};
        if (cartographer_sdl_log) {
          SDL_Log("[node]ExtrapolatePose(time:%lli), {rotation}newest(%.5f) * %.5f = %.5f, {translation}(%.5f, %.5f, %.5f) + (%.5f, %.5f, %.5f)=(%.5f, %.5f, %.5f)", 
            ToUniversal(time), 
            calculate_yaw(newest_timed_pose.pose.rotation()),
            calculate_yaw(rot),
            calculate_yaw(rotation),
            trans[0], trans[1], trans[2],
            newest_timed_pose.pose.translation()[0], newest_timed_pose.pose.translation()[1], newest_timed_pose.pose.translation()[2],
            translation[0], translation[1], translation[2]);
        }
        extrapolation_imu_tracker_->SetVerbose(false);
    } else {
    const Eigen::Vector3d translation =
        ExtrapolateTranslation(time) + newest_timed_pose.pose.translation();
    const Eigen::Quaterniond rotation =
        newest_timed_pose.pose.rotation() *
        ExtrapolateRotation(time, extrapolation_imu_tracker_.get());

    cached_extrapolated_pose_ =
        TimedPose{time, transform::Rigid3d{translation, rotation}};
    }
  }
  if (in_node_) {
      // gtmp_yaw_degree = calculate_yaw(cached_extrapolated_pose_.pose.rotation());
      int64_t int64_time = ToUniversal(time);
      // SDL_Log("[node]PoseExtrapolator::ExtrapolatePose(time:%lli), %s, timed_pose_queue_.size: %i", 
      //     ToUniversal(time), cached_extrapolated_pose_.pose.DebugString().c_str(), (int)timed_pose_queue_.size());
      int ii = 0;
  }
  return cached_extrapolated_pose_.pose;
}

Eigen::Quaterniond PoseExtrapolator::EstimateGravityOrientation(
    const common::Time time) {
  ImuTracker imu_tracker = *imu_tracker_;
  if (cartographer_sdl_log) {
    SDL_Log("EstimateGravityOrientation(%p, %p), pre, time(%lli) - imu_tracker_->time(%lli) = %lli, imu_tracker.orientation: %.5f",
      imu_tracker_.get(), &imu_tracker,
      ToUniversal(time), ToUniversal(imu_tracker.time()), ToUniversal(time) - ToUniversal(imu_tracker.time()),
      calculate_yaw(imu_tracker.orientation()));
  }
  AdvanceImuTracker(time, &imu_tracker);
  if (cartographer_sdl_log) {
    SDL_Log("EstimateGravityOrientation(%p), post time(%lli) - imu_tracker_->time(%lli) = %lli, imu_tracker.orientation: %.5f",
      imu_tracker_.get(),
      ToUniversal(time), ToUniversal(imu_tracker.time()), ToUniversal(time) - ToUniversal(imu_tracker.time()),
      calculate_yaw(imu_tracker.orientation()));
  }
  return imu_tracker.orientation();
}

void PoseExtrapolator::UpdateVelocitiesFromPoses() {
  if (timed_pose_queue_.size() < 2) {
    // We need two poses to estimate velocities.
    return;
  }
  CHECK(!timed_pose_queue_.empty());
  const TimedPose& newest_timed_pose = timed_pose_queue_.back();
  const auto newest_time = newest_timed_pose.time;
  const TimedPose& oldest_timed_pose = timed_pose_queue_.front();
  const auto oldest_time = oldest_timed_pose.time;
  const double queue_delta = common::ToSeconds(newest_time - oldest_time);
  if (queue_delta < common::ToSeconds(pose_queue_duration_)) {
    LOG(WARNING) << "Queue too short for velocity estimation. Queue duration: "
                 << queue_delta << " s";
    return;
  }
  const transform::Rigid3d& newest_pose = newest_timed_pose.pose;
  const transform::Rigid3d& oldest_pose = oldest_timed_pose.pose;
  linear_velocity_from_poses_ =
      (newest_pose.translation() - oldest_pose.translation()) / queue_delta;
  angular_velocity_from_poses_ =
      transform::RotationQuaternionToAngleAxisVector(
          oldest_pose.rotation().inverse() * newest_pose.rotation()) /
      queue_delta;

  const bool use_imu_angular_velocity_z = false;
  if (use_imu_angular_velocity_z) {
    angular_velocity_from_poses_[2] = aplt::valuex.angular_vel[2];
  }
  if (!in_node_) {
      if (cartographer_sdl_log) {
        SDL_Log("UpdateVelocitiesFromPoses, linear(%.5f, %.5f, %.5f) (oldest(%.5f) * newest_pose(%.5f) / %.5f = (%.5f, %.5f, %.5f)", 
          linear_velocity_from_poses_[0],
          linear_velocity_from_poses_[1],
          linear_velocity_from_poses_[2],
          calculate_yaw(oldest_pose.rotation().inverse()), 
          calculate_yaw(newest_pose.rotation()),
          queue_delta, 
          angular_velocity_from_poses_[0] * 180. / M_PI,
          angular_velocity_from_poses_[1] * 180. / M_PI,
          angular_velocity_from_poses_[2] * 180. / M_PI);
      }

      trose_slot::ttimed_pose_queue to_queue;
      to_queue.size = timed_pose_queue_.size();
      to_queue.delta = queue_delta;
      to_queue.duration = common::ToSeconds(pose_queue_duration_);
      to_queue.linear_velocity[0] = linear_velocity_from_poses_[0];
      to_queue.linear_velocity[1] = linear_velocity_from_poses_[1];
      to_queue.linear_velocity[2] = linear_velocity_from_poses_[2];
      to_queue.angular_velocity[0] = angular_velocity_from_poses_[0];
      to_queue.angular_velocity[1] = angular_velocity_from_poses_[1];
      to_queue.angular_velocity[2] = angular_velocity_from_poses_[2];
      rose_slot.set_timed_pose_queue(to_queue);
  }
}

void PoseExtrapolator::TrimImuData() {
  while (imu_data_.size() > 1 && !timed_pose_queue_.empty() &&
         imu_data_[1].time <= timed_pose_queue_.back().time) {
    imu_data_.pop_front();
  }
}

void PoseExtrapolator::TrimOdometryData() {
  while (odometry_data_.size() > 2 && !timed_pose_queue_.empty() &&
         odometry_data_[1].time <= timed_pose_queue_.back().time) {
    odometry_data_.pop_front();
  }
}

void PoseExtrapolator::AdvanceImuTracker(const common::Time time,
                                         ImuTracker* const imu_tracker) const {
  CHECK_GE(time, imu_tracker->time());
  if (imu_data_.empty() || time < imu_data_.front().time) {
    // There is no IMU data until 'time', so we advance the ImuTracker and use
    // the angular velocities from poses and fake gravity to help 2D stability.
    if (!in_node_) {
        int ii = 0;
    }
    imu_tracker->Advance(time);
    imu_tracker->AddImuLinearAccelerationObservation(Eigen::Vector3d::UnitZ());
    imu_tracker->AddImuAngularVelocityObservation(
        odometry_data_.size() < 2 ? angular_velocity_from_poses_
                                  : angular_velocity_from_odometry_);
    return;
  }
  if (imu_tracker->time() < imu_data_.front().time) {
    // Advance to the beginning of 'imu_data_'.
    imu_tracker->Advance(imu_data_.front().time);
  }
  auto it = std::lower_bound(
      imu_data_.begin(), imu_data_.end(), imu_tracker->time(),
      [](const sensor::ImuData& imu_data, const common::Time& time) {
        return imu_data.time < time;
      });
  while (it != imu_data_.end() && it->time < time) {
    imu_tracker->Advance(it->time);
    imu_tracker->AddImuLinearAccelerationObservation(it->linear_acceleration);
    imu_tracker->AddImuAngularVelocityObservation(it->angular_velocity);
    ++it;
  }
  imu_tracker->Advance(time);
}

Eigen::Quaterniond PoseExtrapolator::ExtrapolateRotation(
    const common::Time time, ImuTracker* const imu_tracker) const {
  CHECK_GE(time, imu_tracker->time());
  AdvanceImuTracker(time, imu_tracker);
  const Eigen::Quaterniond last_orientation = imu_tracker_->orientation();
  if (!in_node_ && gcartoverbose) {
        if (cartographer_sdl_log) {
          SDL_Log("ExtrapolateRotation(time:%lli), last_orientation(%.5f) * imu_tracker->orientation(%.5f) = %.5f", 
            ToUniversal(time), 
            calculate_yaw(last_orientation.inverse()),
            calculate_yaw(imu_tracker->orientation()),
            calculate_yaw(last_orientation.inverse() * imu_tracker->orientation()));
        }
  }
  return last_orientation.inverse() * imu_tracker->orientation();
}

Eigen::Vector3d PoseExtrapolator::ExtrapolateTranslation(common::Time time) {
  const TimedPose& newest_timed_pose = timed_pose_queue_.back();
  const double extrapolation_delta =
      common::ToSeconds(time - newest_timed_pose.time);
  if (odometry_data_.size() < 2) {
    if (!in_node_ && gcartoverbose) {
        if (cartographer_sdl_log) {
          SDL_Log("ExtrapolateTranslation(time:%lli), extrapolation_delta(%.5f, %.5f, %.5f) * linear_velocity_from_poses_(%.5f) = (%.5f, %.5f, %.5f)",
            ToUniversal(time), extrapolation_delta,
            linear_velocity_from_poses_[0], linear_velocity_from_poses_[1], linear_velocity_from_poses_[2],
            extrapolation_delta * linear_velocity_from_poses_[0], extrapolation_delta * linear_velocity_from_poses_[1], extrapolation_delta * linear_velocity_from_poses_[2]);
        }
    }
    return extrapolation_delta * linear_velocity_from_poses_;
  }
  return extrapolation_delta * linear_velocity_from_odometry_;
}

PoseExtrapolator::ExtrapolationResult
PoseExtrapolator::ExtrapolatePosesWithGravity(
    const std::vector<common::Time>& times) {
  std::vector<transform::Rigid3f> poses;
  for (auto it = times.begin(); it != std::prev(times.end()); ++it) {
    poses.push_back(ExtrapolatePose(*it).cast<float>());
  }

  const Eigen::Vector3d current_velocity = odometry_data_.size() < 2
                                               ? linear_velocity_from_poses_
                                               : linear_velocity_from_odometry_;
  return ExtrapolationResult{poses, ExtrapolatePose(times.back()),
                             current_velocity,
                             EstimateGravityOrientation(times.back())};
}

}  // namespace mapping
}  // namespace cartographer
