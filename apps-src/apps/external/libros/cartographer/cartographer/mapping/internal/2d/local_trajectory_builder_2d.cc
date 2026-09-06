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

#include "cartographer/mapping/internal/2d/local_trajectory_builder_2d.h"

#include <limits>
#include <memory>

#include "absl/memory/memory.h"
#include "cartographer/metrics/family_factory.h"
#include "cartographer/sensor/range_data.h"

#include <rose_ros/utils.hpp>
#include <rose_ros/cartographer_utils.h>

extern double calculate_yaw(const ::cartographer::transform::Rigid3d::Quaternion& rotation);
bool gcartoverbose = false;
static const cartographer::sensor::TimedPointCloudOriginData* curr_synchronized_data = nullptr;
static cartographer::transform::Rigid3f curr_range_data_poses_back;

namespace cartographer {
namespace mapping {

static auto* kLocalSlamLatencyMetric = metrics::Gauge::Null();
static auto* kLocalSlamRealTimeRatio = metrics::Gauge::Null();
static auto* kLocalSlamCpuRealTimeRatio = metrics::Gauge::Null();
static auto* kRealTimeCorrelativeScanMatcherScoreMetric =
    metrics::Histogram::Null();
static auto* kCeresScanMatcherCostMetric = metrics::Histogram::Null();
static auto* kScanMatcherResidualDistanceMetric = metrics::Histogram::Null();
static auto* kScanMatcherResidualAngleMetric = metrics::Histogram::Null();

LocalTrajectoryBuilder2D::LocalTrajectoryBuilder2D(
    const proto::LocalTrajectoryBuilderOptions2D& options,
    const std::vector<std::string>& expected_range_sensor_ids)
    : options_(options),
      active_submaps_(options.submaps_options()),
      motion_filter_(options_.motion_filter_options()),
      real_time_correlative_scan_matcher_(
          options_.real_time_correlative_scan_matcher_options()),
      ceres_scan_matcher_(options_.ceres_scan_matcher_options()),
      range_data_collator_(expected_range_sensor_ids) {}

LocalTrajectoryBuilder2D::~LocalTrajectoryBuilder2D() {}

sensor::RangeData
LocalTrajectoryBuilder2D::TransformToGravityAlignedFrameAndFilter(
    const transform::Rigid3f& transform_to_gravity_aligned_frame,
    const sensor::RangeData& range_data) const {
  if (cartographer_sdl_log) {
    SDL_Log("TransformToGravityAlignedFrameAndFilter, transform_to_gravity_aligned_frame: %s",
            transform_to_gravity_aligned_frame.DebugString().c_str());
  }
  const sensor::RangeData cropped =
      sensor::CropRangeData(sensor::TransformRangeData(
                                range_data, transform_to_gravity_aligned_frame),
                            options_.min_z(), options_.max_z());
  return sensor::RangeData{
      cropped.origin,
      sensor::VoxelFilter(cropped.returns, options_.voxel_filter_size()),
      sensor::VoxelFilter(cropped.misses, options_.voxel_filter_size())};
}

std::unique_ptr<transform::Rigid2d> LocalTrajectoryBuilder2D::ScanMatch(
    const common::Time time, const transform::Rigid2d& pose_prediction,
    const sensor::PointCloud& filtered_gravity_aligned_point_cloud) {
/*
    cartographer::transform::Rigid2d pose_prediction(Eigen::Vector2d(1, 2), 
            Eigen::Rotation2Dd(30 * M_PI / 180));
            // Eigen::AngleAxisd(0, Eigen::Vector3d(0, 0, 1)));
*/
    if (cartographer_sdl_log) {
        SDL_Log("LocalTrajectoryBuilder2D::ScanMatch, pose_prediction: %s", pose_prediction.DebugString().c_str());
            // return absl::make_unique<transform::Rigid2d>(pose_prediction);
    }
  static int times = 0;
  static double total_cost = 0;
  trosverbose verbose("LocalTrajectoryBuilder2D::ScanMatch", times, total_cost);
  trose_slot::tscan_match scan_match;
  scan_match.linear_search_window = options_.real_time_correlative_scan_matcher_options().linear_search_window();
  scan_match.angular_search_window = options_.real_time_correlative_scan_matcher_options().angular_search_window();
  scan_match.pose_prediction.set(pose_prediction.translation().x(), pose_prediction.translation().y(), pose_prediction.rotation().angle(), true);

  if (active_submaps_.submaps().empty()) {
    if (cartographer_sdl_log) {
      SDL_Log("ScanMatch, active_submaps_.submaps() is empty, pose_prediction(%s)", 
        pose_prediction.DebugString().c_str());
    }
    return absl::make_unique<transform::Rigid2d>(pose_prediction);
  }
  std::shared_ptr<const Submap2D> matching_submap =
      active_submaps_.submaps().front();
  // The online correlative scan matcher will refine the initial estimate for
  // the Ceres scan matcher.
  transform::Rigid2d initial_ceres_pose = pose_prediction;

  if (options_.use_online_correlative_scan_matching()) {
    static int times1 = 0;
    static double total_cost1 = 0;
    trosverbose verbose("LocalTrajectoryBuilder2D::ScanMatch [1/3]real_time_correlative_scan_matcher_.Match", times1, total_cost1);

    double start_second = ros::Time::now().toSec();
    const double score = real_time_correlative_scan_matcher_.Match(
        pose_prediction, filtered_gravity_aligned_point_cloud,
        *matching_submap->grid(), &initial_ceres_pose);
    scan_match.initial_ceres_pose.set(initial_ceres_pose.translation().x(), initial_ceres_pose.translation().y(), initial_ceres_pose.rotation().angle(), true);
    scan_match.correlative_scan_matching_second = ros::Time::now().toSec() - start_second;
    kRealTimeCorrelativeScanMatcherScoreMetric->Observe(score);
  }

  auto pose_observation = absl::make_unique<transform::Rigid2d>();
  ceres::Solver::Summary summary;
  {
     // transform::Rigid2d* const pose_estimate = pose_observation.get();
     // *pose_estimate = initial_ceres_pose;
     // summary.final_cost = 1;
  }

  double start_ceres_scan_second = ros::Time::now().toSec();
  ceres_scan_matcher_.Match(pose_prediction.translation(), initial_ceres_pose,
                            filtered_gravity_aligned_point_cloud,
                            *matching_submap->grid(), pose_observation.get(),
                            &summary);
  scan_match.ceres_scan_matcher_second = ros::Time::now().toSec() - start_ceres_scan_second;

  if (pose_observation) {
    static int times3 = 0;
    static double total_cost3 = 0;
    trosverbose verbose("LocalTrajectoryBuilder2D::ScanMatch [3/3]pose_observation", times3, total_cost3);

    kCeresScanMatcherCostMetric->Observe(summary.final_cost);
    const double residual_distance =
        (pose_observation->translation() - pose_prediction.translation())
            .norm();
    kScanMatcherResidualDistanceMetric->Observe(residual_distance);
    const double residual_angle =
        std::abs(pose_observation->rotation().angle() -
                 pose_prediction.rotation().angle());
    kScanMatcherResidualAngleMetric->Observe(residual_angle);
    scan_match.pose_estimate_2d.set(pose_observation->translation().x(), pose_observation->translation().y(), pose_observation->rotation().angle(), true);
  }
  rose_slot.set_scan_match(scan_match);

  if (cartographer_sdl_log) {
    SDL_Log("ScanMatch, pose: (%s) --> (%s)", 
      pose_prediction.DebugString().c_str(), pose_observation->DebugString().c_str());
  }

  return pose_observation;
}

std::unique_ptr<LocalTrajectoryBuilder2D::MatchingResult>
LocalTrajectoryBuilder2D::AddRangeData(
    const std::string& sensor_id,
    const sensor::TimedPointCloudData& unsynchronized_data) {

  static int times = 0;
  static double total_cost = 0;
  trosverbose verbose("LocalTrajectoryBuilder2D::AddRangeData", times, total_cost);

  auto synchronized_data =
      range_data_collator_.AddRangeData(sensor_id, unsynchronized_data);
  if (synchronized_data.ranges.empty()) {
    LOG(INFO) << "Range data collator filling buffer.";
    return nullptr;
  }
  curr_synchronized_data = &synchronized_data;

  const common::Time& time = synchronized_data.time;
  // Initialize extrapolator now if we do not ever use an IMU.
  if (!options_.use_imu_data()) {
    InitializeExtrapolator(time);
  }

  if (extrapolator_ == nullptr) {
    // Until we've initialized the extrapolator with our first IMU message, we
    // cannot compute the orientation of the rangefinder.
    LOG(INFO) << "Extrapolator not yet initialized.";
    return nullptr;
  }

  CHECK(!synchronized_data.ranges.empty());
  // TODO(gaschler): Check if this can strictly be 0.
  CHECK_LE(synchronized_data.ranges.back().point_time.time, 0.f);
  const common::Time time_first_point =
      time +
      common::FromSeconds(synchronized_data.ranges.front().point_time.time);
  if (time_first_point < extrapolator_->GetLastPoseTime()) {
    LOG(INFO) << "Extrapolator is still initializing.";
    return nullptr;
  }

  const common::Time previous_time = time;
  {

    double first_time = common::ToUniversal(time_first_point);
    double now_time = common::ToUniversal(time);
    // double last_duraing = common::FromSeconds(synchronized_data.ranges.back().point_time.time);
    int ii = 0;
  }
  if (cartographer_sdl_log) {
    SDL_Log("==[AddRangeData]== N: %u, first time: %.5f", (uint32_t)synchronized_data.ranges.size(), synchronized_data.ranges.front().point_time.time);
    SDL_Log("cloud[1/4]N: %u, front: %s, back: %s", (uint32_t)synchronized_data.ranges.size(),
      Vector3f_DebugString(synchronized_data.ranges.front().point_time.position).c_str(), Vector3f_DebugString(synchronized_data.ranges.back().point_time.position).c_str());
  }

  std::vector<transform::Rigid3f> range_data_poses;
  range_data_poses.reserve(synchronized_data.ranges.size());
  bool warned = false;
  int data_at = 0;
  for (const auto& range : synchronized_data.ranges) {
    common::Time time_point = time + common::FromSeconds(range.point_time.time);
    if (time_point < extrapolator_->GetLastExtrapolatedTime()) {
      if (!warned) {
        LOG(ERROR)
            << "Timestamp of individual range data point jumps backwards from "
            << extrapolator_->GetLastExtrapolatedTime() << " to " << time_point;
        warned = true;
      }
      time_point = extrapolator_->GetLastExtrapolatedTime();
    }
    if (data_at < 2 || data_at >= (int)synchronized_data.ranges.size() - 2) {
        gcartoverbose = false;
    }
    range_data_poses.push_back(
        extrapolator_->ExtrapolatePose(time_point).cast<float>());
    if (cartographer_sdl_log && (data_at < 5 || data_at >= (int)synchronized_data.ranges.size() - 5)) {
        SDL_Log("range_data_poses[%i/%i]: %s, time_point:%lli(%.5f)->%lli", 
            data_at, (int)synchronized_data.ranges.size(), range_data_poses.back().DebugString().c_str(),
            ToUniversal(time), range.point_time.time, ToUniversal(time_point));
    }

    gcartoverbose = false;
    data_at ++;
  }

  if (num_accumulated_ == 0) {
    // 'accumulated_range_data_.origin' is uninitialized until the last
    // accumulation.
    accumulated_range_data_ = sensor::RangeData{{}, {}, {}};
  }

  // Drop any returns below the minimum range and convert returns beyond the
  // maximum range into misses.
  for (size_t i = 0; i < synchronized_data.ranges.size(); ++i) {
    const sensor::TimedRangefinderPoint& hit =
        synchronized_data.ranges[i].point_time;
    const Eigen::Vector3f origin_in_local =
        range_data_poses[i] *
        synchronized_data.origins.at(synchronized_data.ranges[i].origin_index);
    sensor::RangefinderPoint hit_in_local =
        range_data_poses[i] * sensor::ToRangefinderPoint(hit);
    // if (i == synchronized_data.ranges.size() - 1) {
    if (i == 0 || i == synchronized_data.ranges.size() - 1) {
        if (cartographer_sdl_log) {
          SDL_Log("AddRangeData, [%u/%u](%s) + {%.5f}(%.5f, %.5f, %.5f) = {%.5f}(%.5f, %.5f, %.5f)", i, synchronized_data.ranges.size() - 1,
            range_data_poses[i].DebugString().c_str(),
            common::RadToDeg(atan2(hit.position[1], hit.position[0])),
            hit.position[0], hit.position[1], hit.position[2],
            common::RadToDeg(atan2(hit_in_local.position[1] - range_data_poses[i].translation().y(), hit_in_local.position[0] - range_data_poses[i].translation().x())),
            hit_in_local.position[0], hit_in_local.position[1], hit_in_local.position[2]);
        }
    }
    const Eigen::Vector3f delta = hit_in_local.position - origin_in_local;
    const float range = delta.norm();
    if (range >= options_.min_range()) {
      if (range <= options_.max_range()) {
        
        accumulated_range_data_.returns.push_back(hit_in_local);
      } else {
        if (cartographer_sdl_log) {
            SDL_Log("AddRangeData[warning], [%u/%u]Emax, range(%.5f) isn't in [%.5f, %.5f]", i, synchronized_data.ranges.size() - 1,
                range, options_.min_range(), options_.max_range());
        }
        hit_in_local.position =
            origin_in_local +
            options_.missing_data_ray_length() / range * delta;
        accumulated_range_data_.misses.push_back(hit_in_local);
      }
    } else {
        if (cartographer_sdl_log) {
            SDL_Log("AddRangeData[warning], [%u/%u]Emin, range(%.5f) isn't in [%.5f, %.5f]", i, synchronized_data.ranges.size() - 1,
                range, options_.min_range(), options_.max_range());
        }
    }
  }
  ++num_accumulated_;

  if (num_accumulated_ >= options_.num_accumulated_range_data()) {
    const common::Time current_sensor_time = synchronized_data.time;
    absl::optional<common::Duration> sensor_duration;
    if (last_sensor_time_.has_value()) {
      sensor_duration = current_sensor_time - last_sensor_time_.value();
    }
    last_sensor_time_ = current_sensor_time;
    num_accumulated_ = 0;

    const double delta_t = common::ToSeconds(previous_time - time);
    const double delta_t2 = common::ToSeconds(extrapolator_->GetLastExtrapolatedTime() - time);

    const transform::Rigid3d gravity_alignment = transform::Rigid3d::Rotation(
        extrapolator_->EstimateGravityOrientation(time));
    // TODO(gaschler): This assumes that 'range_data_poses.back()' is at time
    // 'time'.
    accumulated_range_data_.origin = range_data_poses.back().translation();

    const transform::Rigid3f transform_to_gravity_aligned_frame = gravity_alignment.cast<float>() * range_data_poses.back().inverse();
    if (cartographer_sdl_log) {
      SDL_Log("AddRangeData, [1]accumulated_range_data_.origin:(%.5f, %.5f, %.5f), %s * %s = %s",
            accumulated_range_data_.origin[0], accumulated_range_data_.origin[1], accumulated_range_data_.origin[2],
            gravity_alignment.cast<float>().DebugString().c_str(),
            range_data_poses.back().inverse().DebugString().c_str(),
            transform_to_gravity_aligned_frame.DebugString().c_str());
      SDL_Log("cloud[2/4]N: %u, front: %s, back: %s", (uint32_t)accumulated_range_data_.returns.points().size(),
          Vector3f_DebugString(accumulated_range_data_.returns.points().front().position).c_str(), Vector3f_DebugString(accumulated_range_data_.returns.points().back().position).c_str());

      std::vector<Eigen::Vector3f> points;
      points.push_back(accumulated_range_data_.returns.points().front().position);
      points.push_back(accumulated_range_data_.returns.points().back().position);
      for (std::vector<Eigen::Vector3f>::const_iterator it = points.begin(); it != points.end(); ++ it) {
          const Eigen::Vector3f& point = *it;
          Eigen::Vector3f first = range_data_poses.back().inverse() * point;
          Eigen::Vector3f second = gravity_alignment.cast<float>() * first;
          SDL_Log("[GravityAligned]{%.5f}(%.5f, %.5f, %.5f) => {%.5f}(%.5f, %.5f, %.5f) => {%.5f}(%.5f, %.5f, %.5f)",
              common::RadToDeg(atan2(point[1], point[0])), point[0], point[1], point[2], 
              common::RadToDeg(atan2(first[1], first[0])), first[0], first[1], first[2], 
              common::RadToDeg(atan2(second[1], second[0])), second[0], second[1], second[2]);
      }
    }

    if (gravity_alignment.translation().x() != 0 || gravity_alignment.translation().y() != 0 || gravity_alignment.translation().z() != 0) {
        SDL_Log("AddRangeData[warning], gravity_alignment(%.5f, %.5f, %.5f) isn't 0",
            gravity_alignment.translation().x(), gravity_alignment.translation().y(), gravity_alignment.translation().z());
    }
/*
    {
        const transform::Rigid3f pose = range_data_poses.back();
        transform::Rigid3f r = pose;
        transform::Rigid3f r1 = pose * gravity_alignment.cast<float>().inverse();
        transform::Rigid3f r2 = r1 * gravity_alignment.cast<float>();
        transform::Rigid3f r3 = gravity_alignment.cast<float>() * r1;
        SDL_Log("test_inverse, r: %s, r1: %s, r2: %s, r3: %s",
            r.DebugString().c_str(), r1.DebugString().c_str(), r2.DebugString().c_str(),
            r3.DebugString().c_str());

        transform::Rigid3f l = pose;
        transform::Rigid3f l1 = gravity_alignment.cast<float>().inverse() * pose;
        transform::Rigid3f l2 = gravity_alignment.cast<float>() * l1;
        transform::Rigid3f l3 = l1 * gravity_alignment.cast<float>();
        SDL_Log("test_inverse, l: %s, l1: %s, l2: %s, l3: %s, pose: %s, gravity_alignment: %s",
            l.DebugString().c_str(), l1.DebugString().c_str(), l2.DebugString().c_str(),
            l3.DebugString().c_str(), pose.DebugString().c_str(), gravity_alignment.cast<float>().DebugString().c_str());
    }
*/
    curr_range_data_poses_back = range_data_poses.back();
    return AddAccumulatedRangeData(
        time,
        TransformToGravityAlignedFrameAndFilter(
            gravity_alignment.cast<float>() * range_data_poses.back().inverse(),
            accumulated_range_data_),
        gravity_alignment, sensor_duration);
  }
  return nullptr;
}

std::unique_ptr<LocalTrajectoryBuilder2D::MatchingResult>
LocalTrajectoryBuilder2D::AddAccumulatedRangeData(
    const common::Time time,
    const sensor::RangeData& gravity_aligned_range_data,
    const transform::Rigid3d& gravity_alignment,
    const absl::optional<common::Duration>& sensor_duration) {
  static int times = 0;
  static double total_cost = 0;
  trosverbose verbose("LocalTrajectoryBuilder2D::AddAccumulatedRangeData", times, total_cost);

  if (gravity_aligned_range_data.returns.empty()) {
    LOG(WARNING) << "Dropped empty horizontal range data.";
    return nullptr;
  }

  // Computes a gravity aligned pose prediction.
  const transform::Rigid3d non_gravity_aligned_pose_prediction =
      extrapolator_->ExtrapolatePose(time);
  const transform::Rigid2d pose_prediction = transform::Project2D(
      non_gravity_aligned_pose_prediction * gravity_alignment.inverse());

  const sensor::PointCloud& filtered_gravity_aligned_point_cloud =
      sensor::AdaptiveVoxelFilter(gravity_aligned_range_data.returns,
                                  options_.adaptive_voxel_filter_options());
  if (filtered_gravity_aligned_point_cloud.empty()) {
    return nullptr;
  }

  // local map frame <- gravity-aligned frame
  std::unique_ptr<transform::Rigid2d> pose_estimate_2d =
      ScanMatch(time, pose_prediction, filtered_gravity_aligned_point_cloud);
  if (pose_estimate_2d == nullptr) {
    LOG(WARNING) << "Scan matching failed.";
    return nullptr;
  }

  const transform::Rigid3d pose_estimate =
      transform::Embed3D(*pose_estimate_2d) * gravity_alignment;
  double pose_estimateto3d_yaw_degree = calculate_yaw(transform::Embed3D(*pose_estimate_2d).rotation());
  double gravity_alignment_yaw_degree = calculate_yaw(gravity_alignment.rotation());
  if (cartographer_sdl_log) {
    SDL_Log("cloud[3/4]N: %u, front: %s, back: %s", (uint32_t)gravity_aligned_range_data.returns.points().size(),
      Vector3f_DebugString(gravity_aligned_range_data.returns.points().front().position).c_str(), Vector3f_DebugString(gravity_aligned_range_data.returns.points().back().position).c_str());
  
    SDL_Log("AddAccumulatedRangeData, time: %lli, non_gravity: %s * %s = 2d(%s), pose_estimate(%s) = [ScanMatch] 2d(%.5f) * gravity(%.5f)", 
      ToUniversal(time), non_gravity_aligned_pose_prediction.DebugString().c_str(),
      gravity_alignment.inverse().DebugString().c_str(),
      pose_prediction.DebugString().c_str(),
      pose_estimate.DebugString().c_str(), pose_estimateto3d_yaw_degree, gravity_alignment_yaw_degree);
  }
  extrapolator_->AddPose(time, pose_estimate);

  sensor::RangeData range_data_in_local =
      TransformRangeData(gravity_aligned_range_data,
                         transform::Embed3D(pose_estimate_2d->cast<float>()));
  if (cartographer_sdl_log) {
    SDL_Log("cloud[4/4]N: %u, front: %s, back: %s", (uint32_t)range_data_in_local.returns.points().size(),
        Vector3f_DebugString(range_data_in_local.returns.points().front().position).c_str(), Vector3f_DebugString(range_data_in_local.returns.points().back().position).c_str());
  }
  std::unique_ptr<InsertionResult> insertion_result = InsertIntoSubmap(
      time, range_data_in_local, filtered_gravity_aligned_point_cloud,
      pose_estimate, gravity_alignment.rotation());
  rose_slot.did_AddPose();
  if (insertion_result.get() != nullptr) {
    rose_slot.set_tf_in_local(ToUniversal(time), gravity_alignment, curr_range_data_poses_back, non_gravity_aligned_pose_prediction, pose_estimate);
    rose_slot.fill_probability_grid(*curr_synchronized_data, accumulated_range_data_, gravity_aligned_range_data, range_data_in_local, active_submaps_.submaps());
  }

  const auto wall_time = std::chrono::steady_clock::now();
  if (last_wall_time_.has_value()) {
    const auto wall_time_duration = wall_time - last_wall_time_.value();
    kLocalSlamLatencyMetric->Set(common::ToSeconds(wall_time_duration));
    if (sensor_duration.has_value()) {
      kLocalSlamRealTimeRatio->Set(common::ToSeconds(sensor_duration.value()) /
                                   common::ToSeconds(wall_time_duration));
    }
  }
  const double thread_cpu_time_seconds = common::GetThreadCpuTimeSeconds();
  if (last_thread_cpu_time_seconds_.has_value()) {
    const double thread_cpu_duration_seconds =
        thread_cpu_time_seconds - last_thread_cpu_time_seconds_.value();
    if (sensor_duration.has_value()) {
      kLocalSlamCpuRealTimeRatio->Set(
          common::ToSeconds(sensor_duration.value()) /
          thread_cpu_duration_seconds);
    }
  }
  last_wall_time_ = wall_time;
  last_thread_cpu_time_seconds_ = thread_cpu_time_seconds;
  return absl::make_unique<MatchingResult>(
      MatchingResult{time, pose_estimate, std::move(range_data_in_local),
                     std::move(insertion_result)});
}

std::unique_ptr<LocalTrajectoryBuilder2D::InsertionResult>
LocalTrajectoryBuilder2D::InsertIntoSubmap(
    const common::Time time, const sensor::RangeData& range_data_in_local,
    const sensor::PointCloud& filtered_gravity_aligned_point_cloud,
    const transform::Rigid3d& pose_estimate,
    const Eigen::Quaterniond& gravity_alignment) {
  if (motion_filter_.IsSimilar(time, pose_estimate)) {
    return nullptr;
  }
  std::vector<std::shared_ptr<const Submap2D>> insertion_submaps =
      active_submaps_.InsertRangeData(range_data_in_local);
  return absl::make_unique<InsertionResult>(InsertionResult{
      std::make_shared<const TrajectoryNode::Data>(TrajectoryNode::Data{
          time,
          gravity_alignment,
          filtered_gravity_aligned_point_cloud,
          {},  // 'high_resolution_point_cloud' is only used in 3D.
          {},  // 'low_resolution_point_cloud' is only used in 3D.
          {},  // 'rotational_scan_matcher_histogram' is only used in 3D.
          pose_estimate}),
      std::move(insertion_submaps)});
}

void LocalTrajectoryBuilder2D::AddImuData(const sensor::ImuData& imu_data) {
  CHECK(options_.use_imu_data()) << "An unexpected IMU packet was added.";
  InitializeExtrapolator(imu_data.time);
  extrapolator_->AddImuData(imu_data);
}

void LocalTrajectoryBuilder2D::AddOdometryData(
    const sensor::OdometryData& odometry_data) {
  if (extrapolator_ == nullptr) {
    // Until we've initialized the extrapolator we cannot add odometry data.
    LOG(INFO) << "Extrapolator not yet initialized.";
    return;
  }
  extrapolator_->AddOdometryData(odometry_data);
}

void LocalTrajectoryBuilder2D::InitializeExtrapolator(const common::Time time) {
  if (extrapolator_ != nullptr) {
    return;
  }
  CHECK(!options_.pose_extrapolator_options().use_imu_based());
  // TODO(gaschler): Consider using InitializeWithImu as 3D does.
  extrapolator_ = absl::make_unique<PoseExtrapolator>(
      ::cartographer::common::FromSeconds(options_.pose_extrapolator_options()
                                              .constant_velocity()
                                              .pose_queue_duration()),
      options_.pose_extrapolator_options()
          .constant_velocity()
          .imu_gravity_time_constant());
  extrapolator_->AddPose(time, transform::Rigid3d::Identity());
}

void LocalTrajectoryBuilder2D::RegisterMetrics(
    metrics::FamilyFactory* family_factory) {
  auto* latency = family_factory->NewGaugeFamily(
      "mapping_2d_local_trajectory_builder_latency",
      "Duration from first incoming point cloud in accumulation to local slam "
      "result");
  kLocalSlamLatencyMetric = latency->Add({});
  auto* real_time_ratio = family_factory->NewGaugeFamily(
      "mapping_2d_local_trajectory_builder_real_time_ratio",
      "sensor duration / wall clock duration.");
  kLocalSlamRealTimeRatio = real_time_ratio->Add({});

  auto* cpu_real_time_ratio = family_factory->NewGaugeFamily(
      "mapping_2d_local_trajectory_builder_cpu_real_time_ratio",
      "sensor duration / cpu duration.");
  kLocalSlamCpuRealTimeRatio = cpu_real_time_ratio->Add({});
  auto score_boundaries = metrics::Histogram::FixedWidth(0.05, 20);
  auto* scores = family_factory->NewHistogramFamily(
      "mapping_2d_local_trajectory_builder_scores", "Local scan matcher scores",
      score_boundaries);
  kRealTimeCorrelativeScanMatcherScoreMetric =
      scores->Add({{"scan_matcher", "real_time_correlative"}});
  auto cost_boundaries = metrics::Histogram::ScaledPowersOf(2, 0.01, 100);
  auto* costs = family_factory->NewHistogramFamily(
      "mapping_2d_local_trajectory_builder_costs", "Local scan matcher costs",
      cost_boundaries);
  kCeresScanMatcherCostMetric = costs->Add({{"scan_matcher", "ceres"}});
  auto distance_boundaries = metrics::Histogram::ScaledPowersOf(2, 0.01, 10);
  auto* residuals = family_factory->NewHistogramFamily(
      "mapping_2d_local_trajectory_builder_residuals",
      "Local scan matcher residuals", distance_boundaries);
  kScanMatcherResidualDistanceMetric =
      residuals->Add({{"component", "distance"}});
  kScanMatcherResidualAngleMetric = residuals->Add({{"component", "angle"}});
}

}  // namespace mapping
}  // namespace cartographer
