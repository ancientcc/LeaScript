/*********************************************************************
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2008, 2013, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Eitan Marder-Eppstein
 *********************************************************************/
#ifndef LIBROSE2_ROS_VALUES_HPP_
#define LIBROSE2_ROS_VALUES_HPP_

#include "rose_util.hpp"

#define ROS_DEF_XY_GOAL_TOLERANCE		0.15 // 0.15m
#define ROS_DEF_YAW_GOAL_TOLERANCE		DEG2RAD(20)
// I want to use M_PI. unfortunately, there are multiple M_PI values.
#define ROS_UNRESTRICTED_YAW_GOAL_TOLERANCE		3.14159265358979323846264338327950288

#define set_quaternion_raw_nposm(q)	\
	(q).x = float_nposm;	\
	(q).y = 0;	\
	(q).z = 0;	\
	(q).w = 1
#define is_quaternion_raw_nposm(q)	(is_float_nposm((q).x) && (q).y == 0 && (q).z == 0 && (q).w == 1)

#define ROS_DEF_MIN_MOVEABLE_VEL_X      0.08 // 0.08m/s
#define ROS_DEF_MIN_MOVEABLE_VEL_THETA  DEG2RAD(20)
#define ROS_DEF_MAX_BUILDMAP_VEL_X      0.12
#define ROS_DEF_MAX_BUILDMAP_VEL_THETA  DEG2RAD(25)
#define ROS_DEF_MAX_NAVIGATION_VEL_X    0.15 // 0.20/0.15
#define ROS_DEF_MAX_NAVIGATION_VEL_THETA    DEG2RAD(30)  // 40/30
#define ROS_DEF_MOVE_BASE_CONTROLLER_FREQ   2.5 // 400ms

#define ROS_DEF_ROBOT_LENGTH			0.26
#define ROS_DEF_ROBOT_WIDTH				0.28
#define ROS_DEF_FOOTPRINT_STRING		"[[-0.13, -0.14], [-0.13, 0.14], [0.13, 0.14], [0.13, -0.14]]"

#define ROS_FOOTPRINT_PADDING			0.01 // 0.01m/1cm
#define ROS_RID_PRESSURE_LINEAR_X		0.01 // 0.01m/sec

namespace ros {

struct DECLSPEC tbase_cfg
{
	tbase_cfg();

	double min_moveable_vel_x;
	double min_moveable_vel_theta;
	double max_buildmap_vel_x;
	double max_buildmap_vel_theta;
	double max_navigation_vel_x;
	double max_navigation_vel_theta;
	double move_base_controller_freq;

	double robot_length;
	double robot_width;

	tpose2d laser_to_base_footprint_tf;
	tpose2d dcamera_to_base_footprint_tf;
};
extern LIB3RDPARTY_DECL tbase_cfg base_cfg;

extern LIB3RDPARTY_DECL std::string footprint_string;

// state variable
extern LIB3RDPARTY_DECL bool buildmap;
/*
extern double xy_goal_tolerance;
extern double yaw_goal_tolerance;

// for debug variable
extern bool rviz_breakpoint;
extern bool use_follow;

extern bool dcamera_installed;
extern double dcamera_height_from_ground;
extern double safe_obstacle_height;
*/

}
#endif  // LIBROS_ROSE_VALUES_HPP_
