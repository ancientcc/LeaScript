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

#ifndef LIBROSE_ROS_CARTOGRAPHER_UTILS_H
#define LIBROSE_ROS_CARTOGRAPHER_UTILS_H

#include <Eigen/Core>
#include <vector>

#include "geometry_msgs/Pose.h"
#include "geometry_msgs/Transform.h"
#include "geometry_msgs/TransformStamped.h"
#include "nav_msgs/OccupancyGrid.h"
#include "sensor_msgs/LaserScan.h"
// #include "cartographer/transform/rigid_transform.h"

#include "rose_util.hpp"

#include <rose_thread.hpp>
#include <SDL_rect.h>


namespace cartographer {

namespace mapping {
class Submap2D;
}

namespace sensor {
struct TimedPointCloudOriginData;
}

namespace sensor {
struct RangeData;
}

namespace transform {
template <typename FloatType> class Rigid2;
using Rigid2d = Rigid2<double>;

template <typename FloatType> class Rigid3;
using Rigid3d = Rigid3<double>;

using Rigid3f = Rigid3<float>;
}

#define SLOT_MAX_SUBMAPS		2
class trose_slot
{
public:
	enum {NORMAL_PAGE, LOCAL_SLAM_PAGE, GLOBAL_SLAM_PAGE, MAX_BUILDMAP_PAGE = GLOBAL_SLAM_PAGE, 
		KIDNAP_PAGE, PAGE_COUNT};

	struct tsubmap {
		tsubmap()
			: submap_index(nposm)
			, cost_cells(nullptr)
			, cost_cell_size(0)
			, cost_cell_vsize(0)
			, size(SDL_Point{0, 0})
			, origin_x(0.0)
			, origin_y(0.0)
			, num_range_data(0)
		{}

		~tsubmap()
		{
			if (cost_cells != nullptr) {
				free(cost_cells);
			}
		}

		int submap_index;

		//
		// submap
		//
		// correspondence_cost_cells
		uint8_t* cost_cells;
		int cost_cell_size;
		int cost_cell_vsize;
		SDL_Point size;

		double origin_x;
		double origin_y;
		int num_range_data;
	};

	struct tscan_match
	{
		tscan_match()
			: linear_search_window(0)
			, angular_search_window(0)
			, correlative_scan_matching_second(0)
			, ceres_scan_matcher_second(0)
		{}

		tpose2d pose_prediction;
		double linear_search_window;
		double angular_search_window;
		double correlative_scan_matching_second;
		tpose2d initial_ceres_pose;
		double ceres_scan_matcher_second;
		tpose2d pose_estimate_2d;
	};

	struct ttimed_pose_queue
	{
		int size;
		double delta;
		double duration;
		double linear_velocity[3];
		double angular_velocity[3];
	};

	struct tmisc_poses
	{
		tpose2d gravity_alignment;
		tpose2d range_data_poses_back;
		tpose2d transform_to_gravity_aligned_frame;
		tpose2d non_gravity_aligned_pose_prediction;
		tpose2d local_pose;
	};

	struct trose_node
	{
		int trajectory_id;
		int node_index;
		tpose2d odom_2_map_pose;
		tpose2d optimized_pose;

		double gravity_alignment_yaw;
	};

	struct tglobal_slam_summary
	{
		tglobal_slam_summary()
			: nodes(0)
			, submaps(0)
		{}

		int nodes;
		int submaps;
	};

	struct tSubmapSpec2D
	{
		tSubmapSpec2D(int trajectory_id, int submap_index, double x, double y, double theta)
			: trajectory_id(trajectory_id)
			, submap_index(submap_index)
			, global_pose(x, y, theta)
		{}

		int trajectory_id;
		int submap_index;
		tpose2d global_pose;
	};

	struct tconstraint
	{
		tconstraint(int submap_index, int node_index, double local_pose_2d_x, double local_pose_2d_y, double local_pose_2d_yaw,
			double zbar_ij_x, double zbar_ij_y, double zbar_ij_yaw)
			: submap_index(submap_index)
			, node_index(node_index)
			, local_pose_2d(tpose2d(local_pose_2d_x, local_pose_2d_y, local_pose_2d_yaw))
			, zbar_ij(tpose2d(zbar_ij_x, zbar_ij_y, zbar_ij_yaw))
		{}

		int submap_index;
		int node_index;
		tpose2d local_pose_2d;
		tpose2d zbar_ij;
	};

	trose_slot();
	~trose_slot();


	int current_page() const { return current_page_; }
	void switch_page(int page = nposm);
	bool did_minor_longpress();
	bool can_draw_track() const;

	void set_tf_in_local(int64_t time, const transform::Rigid3d& gravity_alignment, const transform::Rigid3f& range_data_poses_back,
		const transform::Rigid3d& non_gravity_aligned_pose_prediction,
		const transform::Rigid3d& pose_estimate);
	void set_scan_match(const tscan_match& scan_match);
	void set_timed_pose_queue(const ttimed_pose_queue& queue);

	void fill_probability_grid(const cartographer::sensor::TimedPointCloudOriginData& synchronized_data,
		const cartographer::sensor::RangeData& accumulated_range_data,
		const cartographer::sensor::RangeData& gravity_aligned_range_data,
		const cartographer::sensor::RangeData& range_data_in_local,
		const std::vector<std::shared_ptr<const cartographer::mapping::Submap2D>>& submaps);

	void draw_major_track(const SDL_Rect& bg_rect);
	void draw_local_slam_page(const SDL_Rect& bg_rect);
	void draw_global_slam_page(const SDL_Rect& bg_rect);

	void draw_minor_track(const SDL_Rect& bg_rect);

	int submap_count() const { return submap_count_; }
	const tsubmap& submap(int at) const { return *submaps_[at]; }

	// Global SLAM
	void set_last_node(int trajectory_id, int node_index, const transform::Rigid3d& odom_2_map_pose, const transform::Rigid3d& optimized_pose, double gravity_alignment);
	void set_global_slam_data(const std::vector<trose_slot::tsubmap>& submaps_result, 
		const std::vector<trose_slot::tSubmapSpec2D>& poses_2d, int nodes);
	void set_last_intra_constraints(const std::vector<trose_slot::tconstraint>& constraints);
	void set_data_constraints_size(int size);
	void set_work_queue_size(int size);
	void set_last_optimization_data(int use_ms);

	struct tpose_msg {
		int64_t time;
		tpose2d_C gravity_alignment;
		tpose2d_C range_data_poses_back;
		tpose2d_C transform_to_gravity_aligned_frame;
		tpose2d_C non_gravity_aligned_pose_prediction;
		tpose2d_C pose_prediction;
		double linear_search_window;
		double angular_search_window;
		double correlative_scan_matching_second;
		tpose2d_C initial_ceres_pose;
		double ceres_scan_matcher_second;
		tpose2d_C pose_estimate_2d;
		tpose2d_C pose_estimate;
		ttimed_pose_queue timed_pose_queue;
		uint32_t ticks;
		tpose2d_C odom_2_map_pose;
		int add_pose_times;
	};
	void add_pose_msg(int64_t time, const tscan_match& scan_match, const ttimed_pose_queue& timed_pose_queue, const tmisc_poses& misc_poses);
	tcharcdata_C pose_msgs_to_string(int max_disp_msgs, char nl);

	void did_navigation_start(bool buildmap, bool dcamera_installed);
	void did_cartographer_node_will_start();
	void did_cartographer_node_start();
	void did_AddPose();

	bool node_ready() const { return node_ready_; }
	bool show_camera_scan() const { return show_camera_scan_; }

private:
	void draw_normal_major(const SDL_Rect& bg_rect);

	void draw_normal_minor(const SDL_Rect& bg_rect);
	void draw_local_minor(const SDL_Rect& bg_rect);
	void draw_local_minor_laser_scan(const SDL_Rect& bg_rect);
	void draw_local_minor_scan_match(const SDL_Rect& bg_rect);
	void draw_global_minor(const SDL_Rect& bg_rect);

	void reset_pose_msgs();
	void resize_pose_msgs(int increment);
	void resize_pose_msgs_buf(int size);

public:
	const int range_data_gap_h;
	SDL_threadID tid;
	double hdpi_scale;

	class tnode_scan_msg_lock
	{
	public:
		tnode_scan_msg_lock(trose_slot& slot, const sensor_msgs::LaserScan& scan_msg);
		~tnode_scan_msg_lock();

	private:
		trose_slot& slot_;
	};

private:
	threading::mutex cost_cell_mutex_;
	bool buildmap_;
	int current_page_;
	enum {LSLAM_MINOR_SUBMAPS_PAGE, LSLAM_MINOR_SCANMATCH_PAGE, LSLAM_MINOR_LASERSCAN_PAGE, LSLAM_MINOR_COUNT};
	int local_slam_minor_page_;
	bool node_ready_;

	bool dcamera_installed_;
	// Whether to display 'camera_scan' topic.
	bool show_camera_scan_;

	uint32_t navigation_start_ticks_;

	// If use tsubmap submaps_[SLOT_MAX_SUBMAPS], will there be a problem? For safety, do not take this risk.
	tsubmap* submaps_[SLOT_MAX_SUBMAPS];
	int submap_count_;
	bool local_slam_dirty_;

	//
	// cartographer::sensor::RangeData
	//
	enum {type_scan_msg, type_synchronized_data, type_accumulated_range_data, type_gravity_aligned_range_data, type_range_data_in_local};
	struct trange_data
	{
		trange_data()
			: returns(nullptr)
			, returns_size(0)
			, returns_vsize(0)
			, misses(nullptr)
			, misses_size(0)
			, misses_vsize(0)
		{}

		~trange_data()
		{
			if (returns != nullptr) {
				free(returns);
			}
			if (misses != nullptr) {
				free(misses);
			}
		}

		void set(const sensor_msgs::LaserScan& scan_msg);
		void set(const cartographer::sensor::TimedPointCloudOriginData& src);
		void set(const cartographer::sensor::RangeData& src);
		void draw(int type, const SDL_Rect& bg_rect, trose_slot& slot);

		Eigen::Vector3f origin;

		SDL_FPoint* returns;
		int returns_size;
		int returns_vsize;

		SDL_FPoint* misses;
		int misses_size;
		int misses_vsize;
	};
	trange_data scan_msg_;
	trange_data synchronized_data_;
	trange_data accumulated_range_data_;
	trange_data gravity_aligned_range_data_;
	trange_data range_data_in_local_;

	tscan_match tmp_scan_match_;
	tscan_match scan_match_;

	ttimed_pose_queue tmp_timed_pose_queue_;
	ttimed_pose_queue timed_pose_queue_;

	tmisc_poses tmp_misc_poses_;
	tmisc_poses misc_poses_;
	tpose2d last_local_pose_;

	// Global SLAM
	trose_node last_node_;

	tglobal_slam_summary global_slam_summary_;
	std::vector<trose_slot::tsubmap> global_submaps_;
	std::vector<trose_slot::tSubmapSpec2D> global_submap_poses_2d_;
	std::vector<trose_slot::tconstraint> last_intra_constraints_;
	int data_constraints_size_;
	int work_queue_size_;
	int last_optimization_use_ms_;
	bool global_slam_dirty_;
	const sensor_msgs::LaserScan* node_scan_msg_;

	struct tmax_values
	{
		tpose2d ceres_correlative_scan_diff;
		tpose2d local_pose_diff;
	};
	tmax_values max_values_;

	const int max_add_poses_;
	int add_pose_times_;
	uint32_t cartographer_node_start_ticks_;
	tpose_msg* pose_msgs_;
	int pose_msg_vsize_;
	int pose_msg_size_;
	char* pose_msgs_buf_;
	int pose_msg_buf_size_;

	// help for draw
	SDL_Point* draw_points_;
    int draw_points_size_;

	SDL_Rect* draw_rects_;
    int draw_rects_size_;
};

extern trose_slot rose_slot;

}  // namespace cartographer

#endif  // CARTOGRAPHER_ROS_CARTOGRAPHER_ROS_MSG_CONVERSION_H
