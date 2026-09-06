/* $Id: string_utils.hpp 56274 2013-02-10 18:59:33Z boucman $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2013 by Guillaume Melquiond <guillaume.melquiond@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROS_ROSE_ROS_UTILS_HPP
#define LIBROS_ROSE_ROS_UTILS_HPP


#include "rose_util.hpp"
#include <rose_ros/values.hpp>
#include <string>
#include <memory>
#include <SDL.h>
#include <nav_msgs/LoadMap.h>
#include <nav_msgs/OccupancyGrid.h>
#include "rose_filesystem.hpp"

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <urdf_model/pose.h>
#include <sensor_msgs/JointState.h>
#include <geometry_msgs/PoseStamped.h>
#include <sensor_msgs/LaserScan.h>

// #include <kdl/frames.hpp>
// #include <kdl/segment.hpp>
// #include <kdl/tree.hpp>
#include <urdf_model/model.h>

// #include <moveit/robot_model/robot_model.h>
// #include <moveit/robot_state/robot_state.h>

namespace costmap_2d {
struct tcostmap_header;
class Costmap2D;
}

class tros_map
{
public:
	bool valid() const 
	{ 
		if (rspfile.empty() || map.info.width <= 0 || map.info.height <= 0) {
			return false;
		}
		return positions.count(charge_pos_uuid) != 0;
	}

	void clear()
	{
		rspfile.clear();
		desc.clear();
		map.info.width = 0;
		map.info.height = 0;
		positions.clear();
		markers.clear();
	}

public:
	std::string rspfile; // full rspfile name
	std::string desc;
	nav_msgs::OccupancyGrid map;
	std::map<std::string, tmap_position> positions;
	std::map<std::string, tmap_marker> markers;
};

namespace ros {

DECLSPEC double get_yaw_goal_tolerance();
DECLSPEC bool get_use_follow();

DECLSPEC double shortest_angular_distance2(double from, double to_x1, double to_y1, double to_x2, double to_y2, double* angle_ptr);

DECLSPEC std::vector<geometry_msgs::Point> getNarrowFootprint(double robot_width);

DECLSPEC tmap_position get_special_position(const std::string& uuid);
DECLSPEC void insert_special_position(const std::string& uuid, tros_map& ros_map);
DECLSPEC bool load_map_from_rsp(const std::string& path_to_rsp, tros_map& ros_map);

DECLSPEC bool LaserScan_write(const sensor_msgs::LaserScan& scan_msg, const tvoidcdata_C* data, const std::string& path);
DECLSPEC bool LaserScan_read(const std::string& path, sensor_msgs::LaserScan& scan_msg, const tvoiddata_C* data);

DECLSPEC tpose2d calcuate_target_pose2d(const tpose2d& source_pose, const tpose2d& source_2_taget_pose, bool verbose);

#define COSTMAP2D_SURF_MARGIN_X     16
#define COSTMAP2D_SURF_MARGIN_Y     12
#define COSTMAP2D_SURF_CELL_SIZE    4

// if @file_name isn't empty, save costmap2d to @file_name and return nullptr.
DECLSPEC SDL_Surface* costmap2d_2_SDL_Surface(const costmap_2d::Costmap2D& costmap, const std::string& file_name, 
    double robot_x, double robot_y, double robot_yaw, const std::vector<geometry_msgs::PoseStamped>& transformed_plan, const std::vector<geometry_msgs::Point>& _footprint);
// @_file_name: 1.costmap
DECLSPEC SDL_Surface* costmap2d_file_2_SDL_Surface(const std::string& _file_name, costmap_2d::tcostmap_header& header);

class NodeHandle;
DECLSPEC bool load_urdf(ros::NodeHandle& nh, const std::string& urdf_file_path);
DECLSPEC bool load_srdf(ros::NodeHandle& nh, const std::string& srdf_file_path);
DECLSPEC const std::set<const ros::NodeHandle*>& dynamic_reconfigure_servers();

DECLSPEC void isometry3d_from_xyz_rpy(const Eigen::Vector3d& xyz, const Eigen::Vector3d& rpy, Eigen::Isometry3d& result);
DECLSPEC void rpy_from_isometry3d(const Eigen::Isometry3d& src, double* roll, double* pitch, double* yaw);
DECLSPEC Eigen::Isometry3d p_M_2_Isometry3d(const Eigen::Vector3d& p, const Eigen::Matrix3d& M);
// @duble xyz[3]
// @duble rpy[3]
// @duble axis_tag_xyz[3]
// @angle: if this joint is fixed, angle is 0.
DECLSPEC Eigen::Isometry3d chain_joint_pose(const double* xyz, const double* rpy, const double* axis_tag_xyz, double angle);
// fk: Forward Kinematic
// this joint must be RotAxis(dof = 1) or Fixed. if Fixed, angle set to 0.
struct tfk_joint
{
	tfk_joint(const std::string& name, bool fixed, const double* _xyz, const double* _rpy, const double* _axis_tag_xyz)
		: name(name)
		, fixed(fixed)
	{
		memcpy(xyz, _xyz, sizeof(double) * 3);
		memcpy(rpy, _rpy, sizeof(double) * 3);
		memcpy(axis_tag_xyz, _axis_tag_xyz, sizeof(double) * 3);
	}

	tfk_joint(const std::string& name, bool fixed, const urdf::Pose& origin, const urdf::Vector3& axis)
		: name(name)
		, fixed(fixed)
	{
		xyz[0] = origin.position.x;
		xyz[1] = origin.position.y;
		xyz[2] = origin.position.z;
		origin.rotation.getRPY(rpy[0], rpy[1], rpy[2]);
		
		axis_tag_xyz[0] = axis.x;
		axis_tag_xyz[1] = axis.y;
		axis_tag_xyz[2] = axis.z;
	}

	const std::string name;
	const bool fixed;
	double xyz[3];
	double rpy[3];
	double axis_tag_xyz[3];
};

// @angles. if joint is RotAxis, dof = 1. so only angle value. angles.size() is count of variablable joint.
DECLSPEC Eigen::Isometry3d forward_kinematic(const std::vector<tfk_joint>& joints, const std::vector<double>& angles);

class DECLSPEC ttf_calculator
{
public:
	virtual ~ttf_calculator() {}
    virtual geometry_msgs::Pose calculate_tf(const std::vector<tfk_joint>& joints, const std::vector<double>& angles) = 0;
};

DECLSPEC ttf_calculator* create_kdl_tf_calculator(const urdf::ModelInterface& model);

DECLSPEC ttf_calculator* create_fk_tf_calculator(const urdf::ModelInterface& model);

#define MAX_NROFJOINTS  15  
#define MAX_ONEIK_POSES	100
struct DECLSPEC turdf
{
    enum {q_zero, q_middle, q_custom, q_count};
    turdf(const std::string& urdf_file, const std::string& chain_start, const std::string& chain_end,
		int _nj, int _q_type, const double* _q_custom = nullptr);

	void push_js(const double* js);
	void push_pose(const double* pose);

    void push_custom_q(const double* q)
    {
        memset(custom_q, 0, sizeof(custom_q));
        memcpy(custom_q, q, sizeof(double) * nj);
    }

    const std::string urdf_file;
    const std::string chain_start;
    const std::string chain_end;
    const int nj;

    double jss[MAX_ONEIK_POSES][MAX_NROFJOINTS];
	int valid_jss;
	double poses[MAX_ONEIK_POSES][6];
	int valid_poses;
    int q_type;
    double custom_q[MAX_NROFJOINTS];
};
DECLSPEC void ikfast_test(ros::NodeHandle& nh, const turdf& urdf);

}

/*
* call example
* -----
* static int times = 0;
* static double total_cost = 0;
* trosverbose verbose("Node::HandleLaserScanMessage", times, total_cost);
* 
* map_builder_bridge_.sensor_bridge(trajectory_id)
*   ->HandleLaserScanMessage(sensor_id, msg);
*/

class DECLSPEC trosverbose
{
public:
    trosverbose(const std::string& scene, int& times, double& total_cost);
    ~trosverbose();

private:
    const std::string scene;
    int& times;
    double& total_cost;
    const bool enable_logout;
    const double start;
};

DECLSPEC double calculate_yaw_internal(double x, double y, double z, double w);

DECLSPEC std::string JointState_to_string(const sensor_msgs::JointState& msg);
DECLSPEC std::string PoseStamped_DebugString(const geometry_msgs::PoseStamped& pose);
DECLSPEC std::string Vector3f_DebugString(const Eigen::Vector3f& v3);
DECLSPEC std::string vector_double_DebugString(const std::vector<double>& values);

#endif