#ifndef LIBROS_ROS_NODE_WRAPPER_HPP
#define LIBROS_ROS_NODE_WRAPPER_HPP

#include <ros/common.h>
#include <nav_msgs/MapMetaData.h>
#include <move_base_msgs/MoveBaseGoal.h>
#include <geometry_msgs/Twist.h>

#include "rose_util.hpp"
#include "mtrand.h"
#include "rose_thread.hpp"


namespace tf2_ros {
class Buffer;
}

namespace costmap_2d {
class Costmap2D;
}

namespace rosbag {

struct trecord_input
{
	trecord_input(const std::string& filename, int max_duration_s, const std::vector<std::string>& topics)
		: filename(filename)
		, max_duration_s(max_duration_s)
		, topics(topics)
	{}

	void clear()
	{
		filename.clear();
		max_duration_s = nposm;
		topics.clear();
	}

	std::string filename;
	int max_duration_s;
	std::vector<std::string> topics;
};

struct trecord_result
{
    trecord_result()
    {
		clear();
	}

	void clear()
	{
		fsize = 0;
	}

    int64_t fsize;
};

struct tplay_input
{
	tplay_input(const std::string& filename, bool loop)
		: filename(filename)
		, loop(loop)
	{}

	void clear()
	{
		filename.clear();
		loop = false;
	}

	std::string filename;
	bool loop;
};

struct tplay_result
{
	tplay_result()
	{}

	void clear()
	{
		current_time_s = float_nposm;
		length_time_s = float_nposm;
	}

	double current_time_s;
	double length_time_s;
};

}

namespace ros {

class ROSCPP_DECL tnode_wrapper
{
public:
	// tnode_wrapper(ros::CallbackQueue& cbqueue, tf2_ros::Buffer& tf);
	tnode_wrapper();
	virtual ~tnode_wrapper();

	// void slice(bool global_planner, const nav_msgs::MapMetaData& info, const int8_t* data, const tpose2d& robot_pose);
	// void move_base_sendGoal(const move_base_msgs::MoveBaseGoal& goal);
	// void move_base_cancelGoal();

	// rrt_exploration
	bool exploration_detect(const tpose2d& robot_pose, geometry_msgs::Point& unk);

private:
	bool exploration_detect_rrt(const costmap_2d::Costmap2D& costmap, const tpose2d& robot_pose, geometry_msgs::Point& unk);
	bool exploration_detect_opencv(const costmap_2d::Costmap2D& costmap, const tpose2d& robot_pose, geometry_msgs::Point& unk);

private:
	// this is an example of initializing by an array
	// you may use MTRand(seed) with any 32bit integer
	// as a seed for a simpler initialization
	MTRand drand_; // double in [0, 1) generator, already init

	// ros::CallbackQueue& cbqueue_;
	// tf2_ros::Buffer& tf_;

	tpose2d old_robot_pose_;
	uint32_t check_movement_ticks_;

	std::vector<SDL_FPoint> V_;
};

class ROSCPP_DECL trose_slot
{
public:
	trose_slot();

	void did_post_adjust_global_plan(uint32_t id, bool narrow, bool top_is_narrow, bool straight_ward);
	void did_adjust_traj_backward(bool front_pressure, double ang_diff, const std::string& desc);
	void set_cmd_vel(const geometry_msgs::Twist& cmd_vel, bool use_negative_vel, bool xy_tolerance_latch, double yaw_goal_tolerance);

	bool normal_page_dirty() const { return normal_page_dirty_; }
	std::string to_string(uint32_t navigation_start_ticks);

public:
	SDL_threadID tid;

private:
	threading::mutex mutex_;
	bool normal_page_dirty_;

	uint32_t computeVelocityCommands_id_;
	bool narrow_;
	bool top_is_narrow_;
	bool straight_ward_;
	bool use_negative_vel_;
	bool xy_tolerance_latch_;
	double yaw_goal_tolerance_;
	bool front_pressure_;
	double backward_ang_diff_;
	std::string adjust_traj_backward_desc_;
	geometry_msgs::Twist cmd_vel_;
};

extern ROSVARIABLE_DECL trose_slot rose_slot;

} // namespace ros

#endif // LIBROS_ROS_NODE_WRAPPER_HPP
