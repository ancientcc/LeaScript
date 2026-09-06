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

#include "rose_ros/cartographer_utils.h"
// visual studio's <wingdi.h> has '#define ERROR 0', 
// it is in conflict with 'const int ERROR = -2' in <ceres>/include/glog/logging.h
// #undef ERROR

#include "cartographer/mapping/2d/submap_2d.h"
#include <cartographer/sensor/timed_point_cloud_data.h>

#include <SDL_log.h>
#include "rose_exception.hpp"
#include "sdl_utils.hpp"
#include "font.hpp"
#include <angles/angles.h>
#include <rose_ros/node_wrapper.hpp>
#include <rose_ros/utils.hpp>

namespace cartographer {

trose_slot::trose_slot()
    : range_data_gap_h(2)
    , tid(0)
    , hdpi_scale(nposm)
    , buildmap_(false)
    , current_page_(game_config::os == os_windows? NORMAL_PAGE: NORMAL_PAGE)
    , local_slam_minor_page_(LSLAM_MINOR_SUBMAPS_PAGE)
    , node_ready_(false)
    , dcamera_installed_(false)
    , show_camera_scan_(false)
    , navigation_start_ticks_(0)
    , submap_count_(0)
    , local_slam_dirty_(false)
    , data_constraints_size_(0)
	, work_queue_size_(0)
    , last_optimization_use_ms_(0)
    , global_slam_dirty_(false)
    , node_scan_msg_(nullptr)
    , max_add_poses_(200)
    , add_pose_times_(0)
    , cartographer_node_start_ticks_(0)
	, pose_msgs_(nullptr)
	, pose_msg_vsize_(0)
	, pose_msg_size_(0)
    , pose_msgs_buf_(nullptr)
	, pose_msg_buf_size_(0)
    , draw_points_(nullptr)
    , draw_points_size_(0)
    , draw_rects_(nullptr)
    , draw_rects_size_(0)
{
    for (int at = 0; at < SLOT_MAX_SUBMAPS; at ++) {
        submaps_[at] = new tsubmap;
    }
}

trose_slot::~trose_slot()
{
    for (int at = 0; at < SLOT_MAX_SUBMAPS; at ++) {
        VALIDATE(submaps_[at] != nullptr, null_str);
        delete submaps_[at];
    }

    if (pose_msgs_ != nullptr) {
		free(pose_msgs_);
		pose_msgs_ = nullptr;
	}

	reset_pose_msgs();

    if (draw_points_ != nullptr) {
		free(draw_points_);
	}
    if (draw_rects_ != nullptr) {
		free(draw_rects_);
	}
}

void trose_slot::switch_page(int page)
{
    if (page == nposm) {
        current_page_ ++;
        const int max_page_count = buildmap_? MAX_BUILDMAP_PAGE + 1: PAGE_COUNT;
        if (current_page_ == max_page_count) {
            current_page_ = 0;
        }

    } else {
        VALIDATE(page >= 0 && page < PAGE_COUNT, null_str);
        current_page_ = page;
    }
}

bool trose_slot::did_minor_longpress()
{
    if (current_page_ == NORMAL_PAGE) {
        if (dcamera_installed_) {
            show_camera_scan_ = !show_camera_scan_;
            return true;
        }

    } else if (current_page_ == LOCAL_SLAM_PAGE) {
        local_slam_minor_page_ ++;
        if (local_slam_minor_page_ == LSLAM_MINOR_COUNT) {
            local_slam_minor_page_ = 0;
        }
        return true;
    }
    return false;
}

bool trose_slot::can_draw_track() const
{
    if (current_page_ == LOCAL_SLAM_PAGE) {
        return local_slam_dirty_;

    } else if (current_page_ == GLOBAL_SLAM_PAGE) {
        return global_slam_dirty_;

    } else {
        VALIDATE(current_page_ == NORMAL_PAGE, null_str);
        return ros::rose_slot.normal_page_dirty() || show_camera_scan_;
    }
}
/*
void trose_slot::clear()
{
    threading::lock lock(cost_cell_mutex_);

    local_slam_dirty_ = true;
    global_slam_dirty_ = true;

    submap_count_ = 0;
    global_submaps_.clear();
}
*/
void trose_slot::trange_data::set(const sensor_msgs::LaserScan& scan_msg)
{
    VALIDATE(scan_msg.range_min >= 0.f, null_str);
    VALIDATE(scan_msg.range_max >= scan_msg.range_min, null_str);
    if (scan_msg.angle_increment > 0.f) {
        VALIDATE(scan_msg.angle_max > scan_msg.angle_min, null_str);
    } else {
        VALIDATE(scan_msg.angle_min > scan_msg.angle_max, null_str);
    }

    // cartographer::sensor::RangeData::returns
    int point_size = scan_msg.ranges.size();
    if (point_size > returns_size) {
        if (returns != nullptr) {
            free(returns);
        }
        returns = (SDL_FPoint*)malloc(point_size * sizeof(SDL_FPoint));
        returns_size = point_size;
    }
    returns_vsize = 0;

    const float* points = &scan_msg.ranges[0];
    float angle = scan_msg.angle_min;
    for (int idx = 0; idx < point_size; ++ idx) {
        // const float echo = scan_msg.ranges[idx];
        const float echo = points[idx];
        if (scan_msg.range_min <= echo && echo <= scan_msg.range_max) {
            float depth_data = scan_msg.ranges[idx];
            float dx = cos(angle) * depth_data;
            float dy = sin(angle) * depth_data;

            returns[returns_vsize].x = dx;
            returns[returns_vsize].y = dy;

            returns_vsize ++;

        }
        angle += scan_msg.angle_increment;
    }
}

void trose_slot::trange_data::set(const cartographer::sensor::TimedPointCloudOriginData& src)
{
    VALIDATE(src.origins.size() == 1, null_str);
    origin = src.origins[0];

    const std::vector<sensor::TimedPointCloudOriginData::RangeMeasurement>& ranges = src.ranges;
    // cartographer::sensor::RangeData::returns
    int point_size = ranges.size();
    if (point_size > returns_size) {
        if (returns != nullptr) {
            free(returns);
        }
        returns = (SDL_FPoint*)malloc(point_size * sizeof(SDL_FPoint));
        returns_size = point_size;
    }
    returns_vsize = point_size;
    // int index = 0;
    const sensor::TimedPointCloudOriginData::RangeMeasurement* points = &ranges[0];
    // for (const sensor::RangefinderPoint& hit : range_data_in_local.returns) {
    for (int index = 0; index < point_size; index ++) {
        const sensor::TimedPointCloudOriginData::RangeMeasurement& hit = points[index];
        returns[index].x = hit.point_time.position[0];
        returns[index].y = hit.point_time.position[1];
        index ++;
    }
    // VALIDATE(index == point_size, null_str);

    // cartographer::sensor::RangeData::misses is nullptr always
}

void trose_slot::trange_data::set(const cartographer::sensor::RangeData& range_data_in_local)
{
    origin = range_data_in_local.origin;
    // cartographer::sensor::RangeData::returns
    int point_size = range_data_in_local.returns.points().size();
    if (point_size > returns_size) {
        if (returns != nullptr) {
            free(returns);
        }
        returns = (SDL_FPoint*)malloc(point_size * sizeof(SDL_FPoint));
        returns_size = point_size;
    }
    returns_vsize = point_size;
    // int index = 0;
    const sensor::RangefinderPoint* points = &range_data_in_local.returns.points()[0];
    // for (const sensor::RangefinderPoint& hit : range_data_in_local.returns) {
    for (int index = 0; index < point_size; index ++) {
        const sensor::RangefinderPoint& hit = points[index];
        returns[index].x = hit.position[0];
        returns[index].y = hit.position[1];
        index ++;
    }
    // VALIDATE(index == point_size, null_str);

    // cartographer::sensor::RangeData::misses
    point_size = range_data_in_local.misses.points().size();
    if (point_size > misses_size) {
        if (misses != nullptr) {
            free(misses);
        }
        misses = (SDL_FPoint*)malloc(point_size * sizeof(SDL_FPoint));
        misses_size = point_size;
    }
    misses_vsize = point_size;
    // index = 0;
    points = &range_data_in_local.misses.points()[0];
    // for (const sensor::RangefinderPoint& hit : range_data_in_local.misses) {
    for (int index = 0; index < point_size; index ++) {
        const sensor::RangefinderPoint& hit = points[index];
        misses[index].x = hit.position[0];
        misses[index].y = hit.position[1];
        index ++;
    }
    // VALIDATE(index == point_size, null_str);
}

void trose_slot::set_tf_in_local(int64_t time, const transform::Rigid3d& gravity_alignment, const transform::Rigid3f& range_data_poses_back,
    const transform::Rigid3d& non_gravity_aligned_pose_prediction, const transform::Rigid3d& pose_estimate)
{
    VALIDATE_IN_THIS_THREAD(tid);

    VALIDATE(gravity_alignment.translation().z() == 0, null_str);
    {
        const Eigen::Quaternion<double>& rotation = gravity_alignment.rotation();
        tmp_misc_poses_.gravity_alignment.set(gravity_alignment.translation().x(), gravity_alignment.translation().y(),
            DEG2RAD(calculate_yaw_internal(rotation.x(), rotation.y(), rotation.z(), rotation.w())), true);
    }

    {
        const Eigen::Quaternion<float>& rotation = range_data_poses_back.rotation();
        tmp_misc_poses_.range_data_poses_back.set(range_data_poses_back.translation().x(), range_data_poses_back.translation().y(),
            DEG2RAD(calculate_yaw_internal(rotation.x(), rotation.y(), rotation.z(), rotation.w())), true);
    }

    transform::Rigid3f transform_to_gravity_aligned_frame = gravity_alignment.cast<float>() * range_data_poses_back.inverse();
    {
        const Eigen::Quaternion<float>& rotation = transform_to_gravity_aligned_frame.rotation();
        tmp_misc_poses_.transform_to_gravity_aligned_frame.set(transform_to_gravity_aligned_frame.translation().x(), transform_to_gravity_aligned_frame.translation().y(),
            DEG2RAD(calculate_yaw_internal(rotation.x(), rotation.y(), rotation.z(), rotation.w())), true);
    }

    {
        const Eigen::Quaternion<double>& rotation = non_gravity_aligned_pose_prediction.rotation();
        tmp_misc_poses_.non_gravity_aligned_pose_prediction.set(non_gravity_aligned_pose_prediction.translation().x(), non_gravity_aligned_pose_prediction.translation().y(),
            DEG2RAD(calculate_yaw_internal(rotation.x(), rotation.y(), rotation.z(), rotation.w())), true);
    }

    {
        VALIDATE(pose_estimate.translation().z() == 0, null_str);
        const Eigen::Quaternion<double>& rotation = pose_estimate.rotation();
        tmp_misc_poses_.local_pose.set(pose_estimate.translation().x(), pose_estimate.translation().y(),
            DEG2RAD(calculate_yaw_internal(rotation.x(), rotation.y(), rotation.z(), rotation.w())), true);
    }

    scan_match_ = tmp_scan_match_;
    timed_pose_queue_ = tmp_timed_pose_queue_;
    misc_poses_ = tmp_misc_poses_;

    if (add_pose_times_ < max_add_poses_) {
        add_pose_msg(time, scan_match_, timed_pose_queue_, misc_poses_);
    }
    add_pose_times_ ++;
}

void trose_slot::set_scan_match(const tscan_match& scan_match)
{
    VALIDATE_IN_THIS_THREAD(tid);

    // this scan's 'insertion_result' maybe nullptr, so use tmp_xxx.
    tmp_scan_match_ = scan_match;
}

void trose_slot::set_timed_pose_queue(const ttimed_pose_queue& queue)
{
    VALIDATE_IN_THIS_THREAD(tid);

    // this scan's 'insertion_result' maybe nullptr, so use tmp_xxx.
    tmp_timed_pose_queue_ = queue;
}

void trose_slot::fill_probability_grid(const cartographer::sensor::TimedPointCloudOriginData& synchronized_data,
    const cartographer::sensor::RangeData& accumulated_range_data,
    const cartographer::sensor::RangeData& gravity_aligned_range_data,
    const cartographer::sensor::RangeData& range_data_in_local,
    const std::vector<std::shared_ptr<const cartographer::mapping::Submap2D>>& submaps)
{
    VALIDATE_IN_THIS_THREAD(tid);
    if (current_page_ != LOCAL_SLAM_PAGE) {
        return;
    }

    threading::lock lock(cost_cell_mutex_);
    local_slam_dirty_ = true;

    if (last_local_pose_.valid) {
        double diff = fabs(misc_poses_.local_pose.x - last_local_pose_.x);
        if (diff > max_values_.local_pose_diff.x) {
            max_values_.local_pose_diff.x = diff;
        }
        diff = fabs(misc_poses_.local_pose.y - last_local_pose_.y);
        if (diff > max_values_.local_pose_diff.y) {
            max_values_.local_pose_diff.y = diff;
        }
        diff = fabs(angles::shortest_angular_distance(misc_poses_.local_pose.yaw, last_local_pose_.yaw));
        if (diff > max_values_.local_pose_diff.yaw) {
            max_values_.local_pose_diff.yaw = diff;
        }
    }
    last_local_pose_ = misc_poses_.local_pose;


    submap_count_ = 0;
    for (auto& submap : submaps) {
        const cartographer::mapping::Grid2D* grid = submap->grid();
        const std::vector<cartographer::uint16>& cost_cells = grid->correspondence_cost_cells();
        const cartographer::mapping::MapLimits& limits = grid->limits();

        const int at = submap_count_;
        tsubmap& to = *submaps_[at];
        int cell_size = cost_cells.size();
        if (cell_size > to.cost_cell_size) {
            if (to.cost_cells != nullptr) {
                free(to.cost_cells);
            }
            to.cost_cells = (uint8_t*)malloc(cell_size);
            to.cost_cell_size = cell_size;
        }
        to.cost_cell_vsize = cell_size;
        to.size = SDL_Point{limits.cell_limits().num_x_cells, limits.cell_limits().num_y_cells};
        transform::Rigid3d local_pose = submap->local_pose();
        to.origin_x = local_pose.translation().x();
        to.origin_y = local_pose.translation().y();

        const transform::Rigid3d::Quaternion& rotation = local_pose.rotation();
        VALIDATE(local_pose.translation().z() == 0, null_str);
        // ration: must be 0 degree
        VALIDATE(rotation.x() == 0 && rotation.y() == 0 && rotation.z() == 0 && rotation.w() == 1.0, null_str);

        to.num_range_data = submap->num_range_data();

        // Use of std::vector's data memory must be continuous to improve traversal efficiency
        const cartographer::uint16* cells = &cost_cells[0];
        // for (std::vector<cartographer::uint16>::const_iterator it = cost_cells.begin();  it != cost_cells.end(); ++ it, index ++) {
        for (int index = 0; index < cell_size; index ++) {
            cartographer::uint16 val16 = cells[index];
            // [0, kUpdateMarker-1]
            VALIDATE(val16 < cartographer::mapping::kUpdateMarker, null_str);
            to.cost_cells[index] = (uint8_t)(255.0 * val16 / cartographer::mapping::kUpdateMarker);
        }
        submap_count_ ++;
    }

    //
    // cartographer::sensor::RangeData
	//
    VALIDATE(node_scan_msg_ != nullptr, null_str);
    scan_msg_.set(*node_scan_msg_);

    synchronized_data_.set(synchronized_data);
    gravity_aligned_range_data_.set(gravity_aligned_range_data);
    accumulated_range_data_.set(accumulated_range_data);
    range_data_in_local_.set(range_data_in_local);
}

#define ROSE_PIXELS_PER_METER   20  // resolution always is 0.05(5cm)

void trose_slot::draw_major_track(const SDL_Rect& widget_rect)
{
    if (current_page_ == LOCAL_SLAM_PAGE) {
        draw_local_slam_page(widget_rect);

    } else if (current_page_ == GLOBAL_SLAM_PAGE) {
        draw_global_slam_page(widget_rect);

    } else {
        VALIDATE(current_page_ == NORMAL_PAGE, null_str);
        draw_normal_major(widget_rect);
    }
}

void trose_slot::draw_normal_major(const SDL_Rect& bg_rect)
{
    VALIDATE(current_page_ == NORMAL_PAGE, null_str);
    std::string msg = ros::rose_slot.to_string(navigation_start_ticks_);
    VALIDATE(!msg.empty(), null_str);

    SDL_Renderer* renderer = get_renderer();
    //

    surface text_surf = font::get_rendered_text(msg, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
    texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

    SDL_Rect dstrect = bg_rect;
    // dstrect.y = bg_rect.y + bg_rect.h - text_surf->h;
    dstrect.w = text_surf->w;
    dstrect.h = text_surf->h;
	SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
}

void trose_slot::draw_local_slam_page(const SDL_Rect& bg_rect)
{
    VALIDATE(current_page_ == LOCAL_SLAM_PAGE, null_str);

    threading::lock lock(cost_cell_mutex_);
    local_slam_dirty_ = false;
    if (submap_count_ == 0) {
        return;
    }

    const int gap_h = range_data_gap_h;
    int range_data_rect_size = bg_rect.w;

    SDL_Rect range_data_rect = bg_rect;
    range_data_rect.h = (bg_rect.h - 2 * gap_h) / 3;
    accumulated_range_data_.draw(type_accumulated_range_data, range_data_rect, *this);

    range_data_rect.y += range_data_rect.h + gap_h;
    gravity_aligned_range_data_.draw(type_gravity_aligned_range_data, range_data_rect, *this);

    range_data_rect.y += range_data_rect.h + gap_h;
    range_data_in_local_.draw(type_range_data_in_local, range_data_rect, *this);

    SDL_Renderer* renderer = get_renderer();
    //
    char buf[128];

    SDL_snprintf(buf, sizeof(buf), "(L)%s\nlast_node_index:%i\n(L2G)%s\nmaxdiff:%s", 
        misc_poses_.local_pose.to_string().c_str(),
        last_node_.node_index,
        last_node_.odom_2_map_pose.to_string().c_str(),
        max_values_.local_pose_diff.to_string().c_str());

    surface text_surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
    texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

    SDL_Rect dstrect = bg_rect;
    dstrect.y = bg_rect.y + bg_rect.h - text_surf->h;
    dstrect.w = text_surf->w;
    dstrect.h = text_surf->h;
	SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
}

void trose_slot::trange_data::draw(int type, const SDL_Rect& bg_rect, trose_slot& slot)
{
    SDL_Renderer* renderer = get_renderer();

    const int range_data_screen_size = SDL_min(bg_rect.w, bg_rect.h);
    SDL_Rect data_rect = bg_rect;
    data_rect.w = range_data_screen_size;
    data_rect.h = range_data_screen_size;
    data_rect.x += (bg_rect.w - data_rect.w) / 2;
    data_rect.y += (bg_rect.h - data_rect.h) / 2;

    // double scan_max_range = 12.0;
    double scan_max_range = 8.0;
    double pixels_per_meter = range_data_screen_size / (2 * scan_max_range);
    int half_thickness = 1;

    VALIDATE(returns_vsize > 0, null_str);
    int point_size = returns_vsize;
    bool use_point = false;
    if (use_point) {
        if (point_size > slot.draw_points_size_) {
            if (slot.draw_points_ != nullptr) {
                free(slot.draw_points_);
            }
            slot.draw_points_ = (SDL_Point*)malloc(point_size * sizeof(SDL_Point));
            slot.draw_points_size_ = point_size;
        }
        for (int at = 0; at < returns_vsize; at ++) {
            const SDL_FPoint& point = returns[at];
            slot.draw_points_[at].x = data_rect.x + (point.x + scan_max_range) * pixels_per_meter;
            slot.draw_points_[at].y = data_rect.y + (range_data_screen_size - 1) - (point.y + scan_max_range) * pixels_per_meter;
        }
        render_points(renderer, 0xffff0000, slot.draw_points_, returns_vsize);

    } else {
        if (point_size > slot.draw_rects_size_) {
            if (slot.draw_rects_ != nullptr) {
                free(slot.draw_rects_);
            }
            slot.draw_rects_ = (SDL_Rect*)malloc(point_size * sizeof(SDL_Rect));
            slot.draw_rects_size_ = point_size;
        }
        for (int at = 0; at < returns_vsize; at ++) {
            const SDL_FPoint& point = returns[at];
            int center_x = data_rect.x + (point.x + scan_max_range) * pixels_per_meter;
            int center_y = data_rect.y + (range_data_screen_size - 1) - (point.y + scan_max_range) * pixels_per_meter;
            slot.draw_rects_[at].x = center_x - half_thickness;
            slot.draw_rects_[at].w = 2 * half_thickness;
            slot.draw_rects_[at].y = center_y - half_thickness;
            slot.draw_rects_[at].h = 2 * half_thickness;
        }
        render_rects(renderer, 0xffff0000, slot.draw_rects_, returns_vsize);
    }

    SDL_Rect dstrect = data_rect;
    dstrect.w = range_data_screen_size;
    dstrect.h = range_data_screen_size;
    // x-axis
    render_line(renderer, 0xff000000, dstrect.x, dstrect.y + dstrect.h / 2, 
        dstrect.x + dstrect.w, dstrect.y + dstrect.h / 2);

    // y-axis
    render_line(renderer, 0xff000000, dstrect.x + dstrect.w / 2, dstrect.y, 
        dstrect.x + dstrect.w / 2, dstrect.y + dstrect.h);

    char buf[256];

    if (type == type_scan_msg) {
        SDL_snprintf(buf, sizeof(buf), "node scam_msg\npoints:%i", returns_vsize);

    } else if (type == type_synchronized_data) {
        SDL_snprintf(buf, sizeof(buf), "synchronized_data\n(%.3f, %.3f, %.3f) ps:%i", origin[0], origin[1], origin[2], returns_vsize);

    } else if (type == type_accumulated_range_data) {
        SDL_snprintf(buf, sizeof(buf), "(%.3f, %.3f, %.3f) ps:%i", origin[0], origin[1], origin[2], returns_vsize);

    } else if (type == type_gravity_aligned_range_data) {
        SDL_snprintf(buf, sizeof(buf), "%s\n(-)%s\n=>%s\n(%.3f, %.3f, %.3f) ps:%i",
            slot.misc_poses_.gravity_alignment.to_string().c_str(),
            slot.misc_poses_.range_data_poses_back.to_string().c_str(),
            slot.misc_poses_.transform_to_gravity_aligned_frame.to_string().c_str(),
            origin[0], origin[1], origin[2], returns_vsize);

    } else {
        VALIDATE(type == type_range_data_in_local, null_str);

        SDL_snprintf(buf, sizeof(buf), "%s\n(p)%s\n(Real)%s\n=>%s\n(%.3f, %.3f, %.3f)",
            slot.misc_poses_.non_gravity_aligned_pose_prediction.to_string().c_str(),
            slot.scan_match_.pose_prediction.to_string().c_str(),
            slot.scan_match_.initial_ceres_pose.to_string().c_str(),
            slot.scan_match_.pose_estimate_2d.to_string().c_str(),
            origin[0], origin[1], origin[2]);

    }

    surface text_surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
    texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

    int text_y_offset = 14 * slot.hdpi_scale;
    dstrect.x = bg_rect.x;
    dstrect.y = bg_rect.y + text_y_offset;
    dstrect.w = text_surf->w;
    dstrect.h = text_surf->h;
	SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
}

void trose_slot::draw_global_slam_page(const SDL_Rect& bg_rect)
{
    VALIDATE(current_page_ == GLOBAL_SLAM_PAGE, null_str);

    threading::lock lock(cost_cell_mutex_);
    SDL_Renderer* renderer = get_renderer();
    global_slam_dirty_ = false;

    if (!misc_poses_.local_pose.valid) {
        return;
    }

    char buf[256];
    // local pose
    SDL_snprintf(buf, sizeof(buf), "(L)%s", 
        misc_poses_.local_pose.to_string().c_str());
    surface text_surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
    texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

    const int block_h_gap = 1 * hdpi_scale;
    int text_y_offset = bg_rect.y + 0 * hdpi_scale;

    SDL_Rect dstrect;
    dstrect.x = bg_rect.x;
    dstrect.y = text_y_offset;
    dstrect.w = text_surf->w;
    dstrect.h = text_surf->h;
	SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
    text_y_offset += text_surf->h;

    // last node
    SDL_snprintf(buf, sizeof(buf), "nodes:%i submaps:%i\nLast Node: #%i-%i\n(L2G)%s\n%s\n=>%s\ng_a_yaw: %.2f\nconstraints: %i work_queue: %i\noptimization: use %ims",
            global_slam_summary_.nodes, global_slam_summary_.submaps,
            last_node_.trajectory_id,
            last_node_.node_index,
            last_node_.odom_2_map_pose.to_string().c_str(),
            misc_poses_.local_pose.to_string().c_str(),
            last_node_.optimized_pose.to_string().c_str(),
            RAD2DEG(last_node_.gravity_alignment_yaw),
            data_constraints_size_, work_queue_size_, last_optimization_use_ms_);
    text_surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
    text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

    dstrect.y = text_y_offset;
    dstrect.w = text_surf->w;
    dstrect.h = text_surf->h;
	SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
    text_y_offset += text_surf->h + block_h_gap;

    std::stringstream ss;
    // intra constraints
    ss.str("");
    ss << "...intra_constraints...";
    for (std::vector<tconstraint>::const_iterator it = last_intra_constraints_.begin(); it != last_intra_constraints_.end(); ++ it) {
        const tconstraint& constraint = *it;

        SDL_snprintf(buf, sizeof(buf), "\nnode#%i->map#%i\n{t[%.3f, %.3f]q:%.3f}\nij{t[%.3f, %.3f]q:%.3f}",
            constraint.node_index,
            constraint.submap_index,
            constraint.local_pose_2d.x,
            constraint.local_pose_2d.y,
            RAD2DEG(constraint.local_pose_2d.yaw),
            constraint.zbar_ij.x,
            constraint.zbar_ij.y,
            RAD2DEG(constraint.zbar_ij.yaw));
        ss << buf;
    }
    if (!ss.str().empty()) {
        text_surf = font::get_rendered_text(ss.str(), 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
        text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);
        dstrect.y = text_y_offset;
        dstrect.w = text_surf->w;
        dstrect.h = text_surf->h;
        SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
        text_y_offset += text_surf->h + block_h_gap;
    }

    // submap_poses_2d
    ss.str("");
    ss << "...submap_poses_2d...";
    const int max_disp_poses = 12;
    int start = (int)global_submap_poses_2d_.size() < max_disp_poses? 0: global_submap_poses_2d_.size() - max_disp_poses;
    int at = 0;
    for (std::vector<tSubmapSpec2D>::const_iterator it = global_submap_poses_2d_.begin(); it != global_submap_poses_2d_.end(); ++ it, at ++) {
        if (at < start) {
            continue;
        }
        const tSubmapSpec2D& submap_pose = *it;

        SDL_snprintf(buf, sizeof(buf), "\n#%i {t[%.3f, %.3f]q:%.2f}",
            submap_pose.submap_index,
            submap_pose.global_pose.x,
            submap_pose.global_pose.y,
            RAD2DEG(submap_pose.global_pose.yaw));
        ss << buf;
    }
    if (!ss.str().empty()) {
        text_surf = font::get_rendered_text(ss.str(), 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
        text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);
        dstrect.y = text_y_offset;
        dstrect.w = text_surf->w;
        dstrect.h = text_surf->h;
        SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
    }
}

void trose_slot::draw_minor_track(const SDL_Rect& widget_rect)
{
    if (current_page_ == LOCAL_SLAM_PAGE) {
        draw_local_minor(widget_rect);

    } else if (current_page_ == GLOBAL_SLAM_PAGE) {
        draw_global_minor(widget_rect);

    } else {
        VALIDATE(current_page_ == NORMAL_PAGE, null_str);
        draw_normal_minor(widget_rect);
    }
}
void trose_slot::draw_normal_minor(const SDL_Rect& bg_rect)
{
    VALIDATE(current_page_ == NORMAL_PAGE, null_str);
    SDL_Renderer* renderer = get_renderer();
    
    std::stringstream ss;

    if (dcamera_installed_) {
        ss << "If NORMAL, Longpress to display the real-time depth camera point cloud.";
        ss << "\n";
        ss << "show_camera_scan_: " << (show_camera_scan_? "true": "false");
        ss << "\n";
        ss << "\n";
    }
    ss << "If LOCAL_SLAM, Longpress to switch the display of content in this area.";

    surface text_surf = font::get_rendered_text(ss.str(), bg_rect.w, font::SIZE_SMALLEST, font::BLACK_COLOR);
    texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

    SDL_Rect dstrect = bg_rect;

    dstrect.x = bg_rect.x;
    dstrect.w = text_surf->w;
    dstrect.h = text_surf->h;
	SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
}

void trose_slot::draw_local_minor(const SDL_Rect& bg_rect)
{
    VALIDATE(current_page_ == LOCAL_SLAM_PAGE, null_str);

    threading::lock lock(cost_cell_mutex_);

    if (submap_count_ == 0) {
        return;
    }

    if (local_slam_minor_page_ == LSLAM_MINOR_LASERSCAN_PAGE) {
        draw_local_minor_laser_scan(bg_rect);
        return;

    } else if (local_slam_minor_page_ == LSLAM_MINOR_SCANMATCH_PAGE) {
        draw_local_minor_scan_match(bg_rect);
        return;
    }
    VALIDATE(local_slam_minor_page_ == LSLAM_MINOR_SUBMAPS_PAGE, null_str);

    const int gap_h = 4;

    SDL_Renderer* renderer = get_renderer();
    char buf[128];

    for (int at = 0; at < submap_count_; at ++) {
        const tsubmap& submap = *submaps_[at];
        const SDL_Point& grid_size = submap.size;
        const uint8_t* data = submap.cost_cells;

        int pos = 0;
        SDL_Surface* photo = SDL_CreateRGBSurface(0, grid_size.x, grid_size.y, 4 * 8,
			    0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
	    uint32_t* pixels = (uint32_t*)photo->pixels;
        memset(pixels, 0, grid_size.x * grid_size.y);
        for (int y = 0; y < grid_size.y; y ++) {
		    for(int x = 0; x < grid_size.x; x ++) {
			    // const int index = x + (size_y - y - 1) * size_x;
                const int index = x + y * grid_size.x;
                const uint8_t raw_u8 = data[index];
                uint32_t val = 0xff5d6e6c;
                if (raw_u8 != 0) {
                    val = posix_mku32(posix_mku16(raw_u8, raw_u8), posix_mku16(raw_u8, 0xff));
                }

                pos = x + (y) * grid_size.x;
                pixels[pos] = val;
		    }
        }

        tpoint ratio_size = calculate_adaption_ratio_size(bg_rect.w, (bg_rect.h - gap_h) / 2, grid_size.x, grid_size.y);
	    int image_width = ratio_size.x;
	    int image_height = ratio_size.y;
        SDL_Rect dstrect{bg_rect.x + (bg_rect.w - image_width) / 2, bg_rect.y + (bg_rect.h / 2 - image_height) / 2, 
            image_width, image_height};
        if (at != 0) {
            dstrect.y += image_height + gap_h;
        }

        // SDL_BlitSurface(photo, nullptr, screen.get(), &dstrect);
        texture tex = SDL_CreateTextureFromSurface2(renderer, photo);
		SDL_RenderCopy(renderer, tex.get(), NULL, &dstrect);

        // IMG_SavePNG(photo, "1.png");
        SDL_FreeSurface(photo);
        // imwrite(photo, "1.png");
        
        // x-axis
        render_line(renderer, 0xff0000ff, dstrect.x, dstrect.y + image_height / 2, 
            dstrect.x + image_width, dstrect.y + image_height / 2);
        // y-axis
        render_line(renderer, 0xff0000ff, dstrect.x + image_width / 2, dstrect.y, 
            dstrect.x + image_width / 2, dstrect.y + image_height);

        // SDL_snprintf(buf, sizeof(buf), "(%ix%i) %i", grid_size.x, grid_size.y, submap.num_range_data);
        SDL_snprintf(buf, sizeof(buf), "(%.2f, %.2f)\n(%ix%i) %i", 
            submap.origin_x, submap.origin_y, grid_size.x, grid_size.y, submap.num_range_data);

        surface text_surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLER, font::BAD_COLOR);
        texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

        dstrect.x = bg_rect.x;
        dstrect.w = text_surf->w;
        dstrect.h = text_surf->h;
	    SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
    }
}

void trose_slot::draw_local_minor_laser_scan(const SDL_Rect& bg_rect)
{
    const int gap_h = 4;

    SDL_Rect range_data_rect = bg_rect;
    range_data_rect.h = (bg_rect.h - gap_h) / 2;

    scan_msg_.draw(type_scan_msg, range_data_rect, *this);
    
    range_data_rect.y += range_data_rect.h + gap_h;
    synchronized_data_.draw(type_synchronized_data, range_data_rect, *this);
}

void trose_slot::draw_local_minor_scan_match(const SDL_Rect& bg_rect)
{
    SDL_Renderer* renderer = get_renderer();
    char buf[256];

    SDL_snprintf(buf, sizeof(buf), "---Scan Match---\nsearch_window: (%.2f,%.2f)\ncorrelative_scan: %.3f ms\nceres_scan: %.3f ms\nangular_velocity_z: %.3f", 
        scan_match_.linear_search_window,
        RAD2DEG(scan_match_.angular_search_window),
        scan_match_.correlative_scan_matching_second * 1000,
        scan_match_.ceres_scan_matcher_second * 1000,
        RAD2DEG(timed_pose_queue_.angular_velocity[2]));

    surface text_surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
    texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

    SDL_Rect dstrect = bg_rect;

    dstrect.x = bg_rect.x;
    dstrect.w = text_surf->w;
    dstrect.h = text_surf->h;
	SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
}

void trose_slot::draw_global_minor(const SDL_Rect& bg_rect)
{
    VALIDATE(current_page_ == GLOBAL_SLAM_PAGE, null_str);

    threading::lock lock(cost_cell_mutex_);
    SDL_Renderer* renderer = get_renderer();


    char buf[256];


    SDL_Rect dstrect = bg_rect;
    int text_y_offset = dstrect.y + 0 * hdpi_scale;

    std::stringstream ss;
    const int max_disp_submaps = 17;
    int start = (int)global_submaps_.size() < max_disp_submaps? 0: global_submaps_.size() - max_disp_submaps;
    int at = 0;
    for (std::vector<tsubmap>::const_iterator it = global_submaps_.begin(); it != global_submaps_.end(); ++ it, at ++) {
        if (at < start) {
            continue;
        }
        const tsubmap& submap = *it;

        SDL_snprintf(buf, sizeof(buf), "#%i-%i {t[%.3f, %.3f]}\n",
            submap.submap_index, submap.num_range_data, submap.origin_x, submap.origin_y);
        ss << buf;
    }
    if (!ss.str().empty()) {
        surface text_surf = font::get_rendered_text(ss.str(), 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
        texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);
        dstrect.y = text_y_offset;
        dstrect.w = text_surf->w;
        dstrect.h = text_surf->h;
        SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
    }
}

void trose_slot::set_last_node(int trajectory_id, int node_index, const transform::Rigid3d& odom_2_map_pose, const transform::Rigid3d& optimized_pose, double gravity_alignment)
{
    threading::lock lock(cost_cell_mutex_);

    last_node_.trajectory_id = trajectory_id;
    last_node_.node_index = node_index;

    {
        VALIDATE(odom_2_map_pose.translation().z() == 0, null_str);
        const Eigen::Quaternion<double>& rotation = odom_2_map_pose.rotation();
        last_node_.odom_2_map_pose.set(odom_2_map_pose.translation().x(), odom_2_map_pose.translation().y(),
            DEG2RAD(calculate_yaw_internal(rotation.x(), rotation.y(), rotation.z(), rotation.w())), true);
    }

    {
        VALIDATE(optimized_pose.translation().z() == 0, null_str);
        const Eigen::Quaternion<double>& rotation = optimized_pose.rotation();
        last_node_.optimized_pose.set(optimized_pose.translation().x(), optimized_pose.translation().y(),
            DEG2RAD(calculate_yaw_internal(rotation.x(), rotation.y(), rotation.z(), rotation.w())), true);
    }

    last_node_.gravity_alignment_yaw = gravity_alignment;
}

void trose_slot::set_global_slam_data(const std::vector<trose_slot::tsubmap>& submaps, const std::vector<trose_slot::tSubmapSpec2D>& poses_2d, int nodes) 
{ 
    VALIDATE_IN_THIS_THREAD(tid);
    threading::lock lock(cost_cell_mutex_);
    global_slam_dirty_ = true;

    global_submaps_ = submaps;
    global_submap_poses_2d_ = poses_2d;

    global_slam_summary_.nodes = nodes;
    global_slam_summary_.submaps = submaps.size();
}

void trose_slot::set_last_intra_constraints(const std::vector<trose_slot::tconstraint>& constraints)
{
    threading::lock lock(cost_cell_mutex_);
    global_slam_dirty_ = true;

    last_intra_constraints_ = constraints;
}

void trose_slot::set_data_constraints_size(int size)
{
    threading::lock lock(cost_cell_mutex_);
    global_slam_dirty_ = true;

    data_constraints_size_ = size;
}

void trose_slot::set_work_queue_size(int size)
{
    threading::lock lock(cost_cell_mutex_);
    global_slam_dirty_ = true;

    work_queue_size_ = size;
}

void trose_slot::set_last_optimization_data(int use_ms)
{
    threading::lock lock(cost_cell_mutex_);
    global_slam_dirty_ = true;

    last_optimization_use_ms_ = use_ms;
}

void log_set_SDL_DPoint(const std::string& scene, const std::set<tpoint>& points)
{
    int point_size = points.size();
    SDL_DPoint* points2 = (SDL_DPoint*)malloc(point_size * sizeof(SDL_DPoint));
    int at = 0;
    for (std::set<tpoint>::const_iterator it = points.begin(); it != points.end(); ++ it, at ++) {
        const tpoint& point = *it;
        points2[at].x = point.x / 1000.0;
        points2[at].y = point.y / 1000.0;
    }

    SDL_Log("---%s size:%i---", scene.c_str(), (int)points.size());
    int max_cols = 8;
    int lines = points.size() / max_cols;
    for (at = 0; at < lines; at ++) {
        int start = at * max_cols;
        SDL_Log("#[%i--%i](%.3f,%.3f) (%.3f,%.3f) (%.3f,%.3f) (%.3f,%.3f) (%.3f,%.3f) (%.3f,%.3f) (%.3f,%.3f) (%.3f,%.3f)", start, start + max_cols - 1,
            points2[start].x, points2[start].y, points2[start + 1].x, points2[start + 1].y, 
            points2[start + 2].x, points2[start + 2].y, points2[start + 3].x, points2[start + 3].y,
            points2[start + 4].x, points2[start + 4].y, points2[start + 5].x, points2[start + 5].y,
            points2[start + 6].x, points2[start + 6].y, points2[start + 7].x, points2[start + 7].y);
    }
    if (lines * max_cols != point_size) {
        char buf[256] = {0};
        int to_at = 0;
        for (at = lines * max_cols; at < point_size; at ++) {
            to_at += SDL_snprintf(buf + to_at, sizeof(buf) - to_at, "(%.3f,%.3f) ", points2[at].x, points2[at].y);
        }
        SDL_Log("#[%i--%i]%s", lines * max_cols, point_size, buf);
    }
    SDL_Log("------------------");
    free(points2);
}

void trose_slot::add_pose_msg(int64_t time, const tscan_match& scan_match, const ttimed_pose_queue& timed_pose_queue, const tmisc_poses& misc_poses)
{
    // threading::lock lock(cost_cell_mutex_);
    resize_pose_msgs(1);

	tpose_msg* to_msg = pose_msgs_ + pose_msg_vsize_;

    to_msg->time = time;
    
    // gravity_alignment
    to_msg->gravity_alignment.x = misc_poses.gravity_alignment.x;
    to_msg->gravity_alignment.y = misc_poses.gravity_alignment.y;
    to_msg->gravity_alignment.yaw = misc_poses.gravity_alignment.yaw;


    // range_data_poses_back
    to_msg->range_data_poses_back.x = misc_poses.range_data_poses_back.x;
    to_msg->range_data_poses_back.y = misc_poses.range_data_poses_back.y;
    to_msg->range_data_poses_back.yaw = misc_poses.range_data_poses_back.yaw;

    // transform_to_gravity_aligned_frame
    to_msg->transform_to_gravity_aligned_frame.x = misc_poses.transform_to_gravity_aligned_frame.x;
    to_msg->transform_to_gravity_aligned_frame.y = misc_poses.transform_to_gravity_aligned_frame.y;
    to_msg->transform_to_gravity_aligned_frame.yaw = misc_poses.transform_to_gravity_aligned_frame.yaw;
	
    // non_gravity_aligned_pose_prediction
    to_msg->non_gravity_aligned_pose_prediction.x = misc_poses.non_gravity_aligned_pose_prediction.x;
    to_msg->non_gravity_aligned_pose_prediction.y = misc_poses.non_gravity_aligned_pose_prediction.y;
    to_msg->non_gravity_aligned_pose_prediction.yaw = misc_poses.non_gravity_aligned_pose_prediction.yaw;

    // pose_prediction
    to_msg->pose_prediction.x = scan_match.pose_prediction.x;
    to_msg->pose_prediction.y = scan_match.pose_prediction.y;
    to_msg->pose_prediction.yaw = scan_match.pose_prediction.yaw;

    // initial_ceres_pose
    to_msg->linear_search_window = scan_match.linear_search_window;
	to_msg->angular_search_window = scan_match.angular_search_window;
    to_msg->correlative_scan_matching_second = scan_match.correlative_scan_matching_second;
    to_msg->initial_ceres_pose.x = scan_match.initial_ceres_pose.x;
    to_msg->initial_ceres_pose.y = scan_match.initial_ceres_pose.y;
    to_msg->initial_ceres_pose.yaw = scan_match.initial_ceres_pose.yaw;

    // pose_estimate_2d
    to_msg->ceres_scan_matcher_second = scan_match.ceres_scan_matcher_second;
    to_msg->pose_estimate_2d.x = scan_match.pose_estimate_2d.x;
    to_msg->pose_estimate_2d.y = scan_match.pose_estimate_2d.y;
    to_msg->pose_estimate_2d.yaw = scan_match.pose_estimate_2d.yaw;

    // pose_estimate
	to_msg->pose_estimate.x = misc_poses.local_pose.x;
    to_msg->pose_estimate.y = misc_poses.local_pose.y;
    to_msg->pose_estimate.yaw = misc_poses.local_pose.yaw;

    to_msg->timed_pose_queue = timed_pose_queue;

	to_msg->ticks = SDL_GetTicks() - cartographer_node_start_ticks_;
    to_msg->odom_2_map_pose.x = last_node_.odom_2_map_pose.x;
    to_msg->odom_2_map_pose.y = last_node_.odom_2_map_pose.y;
    to_msg->odom_2_map_pose.yaw = last_node_.odom_2_map_pose.yaw;
    to_msg->add_pose_times = add_pose_times_;

	pose_msg_vsize_ ++;
}

void trose_slot::reset_pose_msgs()
{
	pose_msg_vsize_ = 0;

    if (pose_msgs_buf_ != nullptr) {
		free(pose_msgs_buf_);
		pose_msgs_buf_ = nullptr;
        pose_msg_buf_size_ = 0;

	} else {
        VALIDATE(pose_msg_buf_size_ == 0, null_str);
    }
}

void trose_slot::resize_pose_msgs(int increment)
{
	int desire_size = pose_msg_vsize_ + increment;
	if (pose_msg_vsize_ + increment <= pose_msg_size_) {
		return;
	}

	desire_size = posix_align_ceil(desire_size, 32);

	tpose_msg* tmp = (tpose_msg*)malloc(desire_size * sizeof(tpose_msg));
    if (pose_msgs_ != nullptr) {
	    if (pose_msg_vsize_ != 0) {
		    memcpy(tmp, pose_msgs_, pose_msg_vsize_ * sizeof(tpose_msg));
	    }
        free(pose_msgs_);
    }
	pose_msgs_ = tmp;
	pose_msg_size_ = desire_size;
}

void trose_slot::resize_pose_msgs_buf(int size)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > pose_msg_buf_size_) {
		char* tmp = (char*)malloc(size);
		if (pose_msgs_buf_ != nullptr) {
			free(pose_msgs_buf_);
		}
		pose_msgs_buf_ = tmp;
		pose_msg_buf_size_ = size;
	}
}

// tcharcdata_C.len: Although it exists, but 'tcharcdata.len' don't include '\0'.
tcharcdata_C trose_slot::pose_msgs_to_string(int max_disp_msgs, char nl) // max_disp_msgs: 22
{
	VALIDATE_IN_MAIN_THREAD();

	tcharcdata_C result{nullptr, 0};
    if (pose_msg_vsize_ == 0) {
        return result;
    }

	const int maxlen = 256;

	resize_pose_msgs_buf(maxlen * 6 * pose_msg_vsize_);
    int pos = 0;
    {
        threading::lock lock(cost_cell_mutex_);

        int msg_len = 0;
        int start = 0;
		if (max_disp_msgs != nposm) {
			start = pose_msg_vsize_ < max_disp_msgs? 0: pose_msg_vsize_ - max_disp_msgs;
		}

        for (int at = 0; at < pose_msg_vsize_; at ++) {
            if (at < start) {
                continue;
            }
            if (pos > 0) {
                pose_msgs_buf_[pos - 1] = '\n';
            }

            char* msg_buf = pose_msgs_buf_ + pos;
            const tpose_msg& msg = pose_msgs_[at];
            int ms = msg.ticks % 1000;

            int ticks = (msg.ticks / 1000) % 3600;
            int sec = ticks % 60;
	        int min = ticks / 60;

            msg_len = SDL_snprintf(msg_buf, maxlen, "%02i:%02i.%03i [#%i]time: %" PRIi64 " add_pose: pose_estimate{t[%.3f, %.3f]q:%.3f} search_window: (%.2f, %.2f) odom_2_map{t[%.3f, %.3f]q:%.3f}\n", 
                min, sec, ms, msg.add_pose_times, msg.time,
				msg.pose_estimate.x, msg.pose_estimate.y, RAD2DEG(angles::normalize_angle(msg.pose_estimate.yaw)),
                scan_match_.linear_search_window, RAD2DEG(scan_match_.angular_search_window),
                msg.odom_2_map_pose.x, msg.odom_2_map_pose.y, RAD2DEG(angles::normalize_angle(msg.odom_2_map_pose.yaw)));
            pos += msg_len;
            msg_buf = pose_msgs_buf_ + pos;

            // gravity_alignment.cast<float>() * range_data_poses.back().inverse() = transform_to_gravity_aligned_frame
            msg_len = SDL_snprintf(msg_buf, maxlen, "[transform_to_gravity_aligned_frame]gravity_alignment{t[%.3f, %.3f]q:%.3f} * range_data_poses.back(){t[%.3f, %.3f]q:%.3f}.inverse() = transform_to_gravity_aligned_frame{t[%.3f, %.3f]q:%.3f}\n",
                msg.gravity_alignment.x, msg.gravity_alignment.y, RAD2DEG(angles::normalize_angle(msg.gravity_alignment.yaw)),
                msg.range_data_poses_back.x, msg.range_data_poses_back.y, RAD2DEG(angles::normalize_angle(msg.range_data_poses_back.yaw)),
                msg.transform_to_gravity_aligned_frame.x, msg.transform_to_gravity_aligned_frame.y, RAD2DEG(angles::normalize_angle(msg.transform_to_gravity_aligned_frame.yaw)));
            pos += msg_len;
            msg_buf = pose_msgs_buf_ + pos;
            
            // non_gravity_aligned_pose_prediction * gravity_alignment.inverse() = pose_prediction
            msg_len = SDL_snprintf(msg_buf, maxlen, "[pose_prediction]non_gravity_aligned_pose_prediction{t[%.3f, %.3f]q:%.3f} * gravity_alignment{t[%.3f, %.3f]q:%.3f}.inverse() = pose_prediction{t[%.3f, %.3f]q:%.3f}\n",
                msg.non_gravity_aligned_pose_prediction.x, msg.non_gravity_aligned_pose_prediction.y, RAD2DEG(angles::normalize_angle(msg.non_gravity_aligned_pose_prediction.yaw)),
                msg.gravity_alignment.x, msg.gravity_alignment.y, RAD2DEG(angles::normalize_angle(msg.gravity_alignment.yaw)),
                msg.pose_prediction.x, msg.pose_prediction.y, RAD2DEG(angles::normalize_angle(msg.pose_prediction.yaw)));
            pos += msg_len;
            msg_buf = pose_msgs_buf_ + pos;

            // pose_prediction -> initial_ceres_pose => pose_estimate_2d
            msg_len = SDL_snprintf(msg_buf, maxlen, "[ScanMatch]pose_prediction{t[%.3f, %.3f]q:%.3f} -> (%.3f ms)initial_ceres_pose{t[%.3f, %.3f]q:%.3f} => (%.3f ms){t[%.3f, %.3f]q:%.3f}\n",
                msg.pose_prediction.x, msg.pose_prediction.y, RAD2DEG(angles::normalize_angle(msg.pose_prediction.yaw)),
                msg.correlative_scan_matching_second * 1000,
                msg.initial_ceres_pose.x, msg.initial_ceres_pose.y, RAD2DEG(angles::normalize_angle(msg.initial_ceres_pose.yaw)),
                msg.ceres_scan_matcher_second * 1000,
                msg.pose_estimate_2d.x, msg.pose_estimate_2d.y, RAD2DEG(angles::normalize_angle(msg.pose_estimate_2d.yaw)));
            pos += msg_len;
            msg_buf = pose_msgs_buf_ + pos;

            // pose_estimate_2d * gravity_alignment = pose_estimate
            msg_len = SDL_snprintf(msg_buf, maxlen, "pose_estimate_2d{t[%.3f, %.3f]q:%.3f} * gravity_alignment{t[%.3f, %.3f]q:%.3f} = pose_estimate{t[%.3f, %.3f]q:%.3f}\n",
                msg.pose_estimate_2d.x, msg.pose_estimate_2d.y, RAD2DEG(angles::normalize_angle(msg.pose_estimate_2d.yaw)),
                msg.gravity_alignment.x, msg.gravity_alignment.y, RAD2DEG(angles::normalize_angle(msg.gravity_alignment.yaw)),
				msg.pose_estimate.x, msg.pose_estimate.y, RAD2DEG(angles::normalize_angle(msg.pose_estimate.yaw)));
            pos += msg_len;
            msg_buf = pose_msgs_buf_ + pos;

            // timed_pose_queue
            msg_len = SDL_snprintf(msg_buf, maxlen, "[timed_pose_queue]size: %i delta(duration): %.5f(%.5f) linear_velocity:(%.5f, %.5f, %.5f) angular_velocity:(%.5f, %.5f, %.5f)\n",
                msg.timed_pose_queue.size, msg.timed_pose_queue.delta, msg.timed_pose_queue.duration,
                msg.timed_pose_queue.linear_velocity[0], msg.timed_pose_queue.linear_velocity[1], msg.timed_pose_queue.linear_velocity[2],
				RAD2DEG(msg.timed_pose_queue.angular_velocity[0]), RAD2DEG(msg.timed_pose_queue.angular_velocity[1]), RAD2DEG(msg.timed_pose_queue.angular_velocity[2]));


            // '+1' is '\0'
            pos += msg_len + 1;
        }
    }

	result.ptr = pose_msgs_buf_;
	// Although it exists, but 'tcharcdata_C.len' don't include '\0'.
	result.len = pos - 1;

    return result;
}

void trose_slot::did_navigation_start(bool buildmap, bool dcamera_installed)
{
    buildmap_ = buildmap;
    dcamera_installed_ = dcamera_installed;

    show_camera_scan_ = false;
    navigation_start_ticks_ = SDL_GetTicks();
}

void trose_slot::did_cartographer_node_will_start()
{
    node_ready_ = false;
}

void trose_slot::did_cartographer_node_start()
{
    threading::lock lock(cost_cell_mutex_);

    add_pose_times_ = 0;
    cartographer_node_start_ticks_ = SDL_GetTicks();

    reset_pose_msgs();

    last_local_pose_.set(0, 0, 0, false);

    misc_poses_.local_pose.set(0, 0, 0, false);

    max_values_.local_pose_diff.set(0, 0, 0, false);

    last_node_.odom_2_map_pose.set(0, 0, 0, false);

    //
    local_slam_dirty_ = true;
    global_slam_dirty_ = true;

    submap_count_ = 0;
    global_submaps_.clear();

    data_constraints_size_ = 0;
	work_queue_size_ = 0;
    last_optimization_use_ms_ = 0;
}

void trose_slot::did_AddPose()
{
    node_ready_ = true;
}

trose_slot::tnode_scan_msg_lock::tnode_scan_msg_lock(trose_slot& slot, const sensor_msgs::LaserScan& scan_msg)
    : slot_(slot)
{
    VALIDATE(slot_.node_scan_msg_ == nullptr, null_str);

    slot_.node_scan_msg_ = &scan_msg;
}

trose_slot::tnode_scan_msg_lock::~tnode_scan_msg_lock()
{
    VALIDATE(slot_.node_scan_msg_ != nullptr, null_str);

    slot_.node_scan_msg_ = nullptr;
}

trose_slot rose_slot;

}  // namespace cartographer
