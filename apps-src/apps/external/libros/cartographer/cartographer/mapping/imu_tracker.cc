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

#include "cartographer/mapping/imu_tracker.h"

#include <cmath>
#include <limits>

#include "cartographer/common/math.h"
#include "cartographer/mapping/internal/eigen_quaterniond_from_two_vectors.h"
#include "cartographer/transform/transform.h"
#include "glog/logging.h"

#include <SDL_log.h>
extern double calculate_yaw(const ::cartographer::transform::Rigid3d::Quaternion& rotation);

namespace cartographer {
namespace mapping {

ImuTracker::ImuTracker(const double imu_gravity_time_constant,
                       const common::Time time)
    : imu_gravity_time_constant_(imu_gravity_time_constant),
      time_(time),
      last_linear_acceleration_time_(common::Time::min()),
      orientation_(Eigen::Quaterniond::Identity()),
      gravity_vector_(Eigen::Vector3d::UnitZ()),
      imu_angular_velocity_(Eigen::Vector3d::Zero()),
      verbose_(false) {}

void ImuTracker::Advance(const common::Time time) {
  CHECK_LE(time_, time);
  const double delta_t = common::ToSeconds(time - time_);
  const Eigen::Quaterniond rotation =
      transform::AngleAxisVectorToRotationQuaternion(
          Eigen::Vector3d(imu_angular_velocity_ * delta_t));
  Eigen::Quaterniond preview_orientation = orientation_;
  orientation_ = (orientation_ * rotation).normalized();
  gravity_vector_ = rotation.conjugate() * gravity_vector_;
  if (imu_angular_velocity_[0] != 0 || imu_angular_velocity_[1] != 0) {
      SDL_Log("AddImuLinearAccelerationObservation[warning], imu_angular_velocity_ != (0, 0, x)"); 
  }
  if (verbose_ && cartographer_sdl_log) {
      SDL_Log("Advance(%p), time(%lli) - time_(%lli) = %lli, delta_t: %.5f, orientation_(%.5f) * rotation(%.5f) = orientation_(%.5f), gravity_vector_:(%.5f, %.5f, %.5f), imu_angular_velocity_:(%.5f, %.5f, %.5f)", 
          this, ToUniversal(time), ToUniversal(time_), ToUniversal(time) - ToUniversal(time_), delta_t,
          calculate_yaw(preview_orientation),
          calculate_yaw(rotation),
          calculate_yaw(orientation_),
          gravity_vector_[0], gravity_vector_[1], gravity_vector_[2],
          imu_angular_velocity_[0] * 180. / M_PI,
          imu_angular_velocity_[1] * 180. / M_PI,
          imu_angular_velocity_[2] * 180. / M_PI);
  }
  time_ = time;
}

void ImuTracker::AddImuLinearAccelerationObservation(
    const Eigen::Vector3d& imu_linear_acceleration) {
  // Update the 'gravity_vector_' with an exponential moving average using the
  // 'imu_gravity_time_constant'.
  const double delta_t =
      last_linear_acceleration_time_ > common::Time::min()
          ? common::ToSeconds(time_ - last_linear_acceleration_time_)
          : std::numeric_limits<double>::infinity();
  last_linear_acceleration_time_ = time_;
  const double alpha = 1. - std::exp(-delta_t / imu_gravity_time_constant_);
  gravity_vector_ =
      (1. - alpha) * gravity_vector_ + alpha * imu_linear_acceleration;
  // Change the 'orientation_' so that it agrees with the current
  // 'gravity_vector_'.
  const Eigen::Quaterniond previous_orientation = orientation_;
  const Eigen::Quaterniond rotation = FromTwoVectors(
      gravity_vector_, orientation_.conjugate() * Eigen::Vector3d::UnitZ());
  orientation_ = (orientation_ * rotation).normalized();

  double previous_orientation_yaw = calculate_yaw(previous_orientation);
  double orientation_yaw = calculate_yaw(orientation_);
  if ((uint32_t)(previous_orientation_yaw * 100000) != (uint32_t)(orientation_yaw * 100000)) {
      SDL_Log("AddImuLinearAccelerationObservation[warning], previous_orientation != orientation_"); 
      int ii = 0;
  }
  if (calculate_yaw(rotation) != 0) {
      SDL_Log("AddImuLinearAccelerationObservation[warning], rotation != 0"); 
      int ii = 0;
  }
  if (gravity_vector_[0] != 0 || gravity_vector_[1] != 0 || gravity_vector_[2] != 1) {
      SDL_Log("AddImuLinearAccelerationObservation[warning], gravity_vector_ != (0, 0, 1)"); 
  }
  if (verbose_) {
/*
      SDL_Log("AddImuLinearAccelerationObservation, time: %lli, %.8f --> gravity_vector_:(%.5f, %.5f, %.5f), rotation: %.5f, orientation_: %.8f", 
          ToUniversal(time_), previous_orientation_yaw, 
          gravity_vector_[0], gravity_vector_[1], gravity_vector_[2],
          calculate_yaw(rotation),
          orientation_yaw);
*/
  }
  CHECK_GT((orientation_ * gravity_vector_).z(), 0.);
  CHECK_GT((orientation_ * gravity_vector_).normalized().z(), 0.99);
}

void ImuTracker::AddImuAngularVelocityObservation(
    const Eigen::Vector3d& imu_angular_velocity) {
  imu_angular_velocity_ = imu_angular_velocity;
}

}  // namespace mapping
}  // namespace cartographer
