/*********************************************************************
*
* Software License Agreement (BSD License)
*
*  Copyright (c) 2009, Willow Garage, Inc.
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

#include <dwa_local_planner/dwa_planner_ros.h>
#include <Eigen/Core>
#include <cmath>

#include <ros/console.h>

// #include <pluginlib/class_list_macros.h>

#include <base_local_planner/goal_functions.h>
#include <nav_msgs/Path.h>
#include <tf2/utils.h>

#include <nav_core/parameter_magic.h>

#include <SDL_log.h>
// #include <SDL_filesystem.h>
// #include <SDL_image.h>
#include "rose_config_3rdparty.hpp"
#include "rose_exception.hpp"
#include <base_local_planner/line_iterator.h>
#include <rose_ros/pathfind.hpp>
#include <rose_ros/utils.hpp>
#include <rose_ros/node_wrapper.hpp>


namespace ros {

extern bool dwa_straight_ward;

extern bool transformed_plan_2_plan_points(const tpose2d& robot_pose, const std::vector<geometry_msgs::PoseStamped>& transformed_plan, const costmap_2d::Costmap2D& costmap, 
    std::vector<SDL_Point>& plan_points, int& diff_points, int& min_x, int& min_y, int& max_x, int& max_y);
}

void revise_twist_limit_max(const std::string& scene, double max_vel_x, double max_vel_theta, geometry_msgs::Twist& cmd_vel)
{
    VALIDATE(max_vel_x > 0 && max_vel_theta > 0, null_str);

    const double abs_traj_linear_x = fabs(cmd_vel.linear.x);
    const double abs_traj_theta = fabs(cmd_vel.angular.z);
    double tmp;
    if (abs_traj_linear_x > max_vel_x) {
        tmp = cmd_vel.linear.x > 0? max_vel_x: -1 * max_vel_x;
        ROS_INFO("[%s]linear.x from %.3f --> %.3f", 
            scene.c_str(), cmd_vel.linear.x, tmp);
        cmd_vel.linear.x = tmp;
    }

    if (abs_traj_theta > max_vel_theta) {
        tmp = cmd_vel.angular.z > 0? max_vel_theta: -1 * max_vel_theta;
        ROS_INFO("[%s]angular.z from deg(%.3f) --> deg(%.3f)", 
            scene.c_str(), RAD2DEG(cmd_vel.angular.z), RAD2DEG(tmp));
        cmd_vel.angular.z = tmp;
    }
}

namespace dwa_local_planner {

  void DWAPlannerROS::reconfigureCB(DWAPlannerConfig &config, uint32_t level) {
      if (setup_ && config.restore_defaults) {
        config = default_config_;
        config.restore_defaults = false;
      }
      if ( ! setup_) {
        default_config_ = config;
        setup_ = true;
      }

      // update generic local planner params
      base_local_planner::LocalPlannerLimits limits;
      limits.max_vel_trans = config.max_vel_trans;
      limits.min_vel_trans = config.min_vel_trans;
      limits.max_vel_x = config.max_vel_x;
      limits.min_vel_x = config.min_vel_x;
      limits.max_vel_y = config.max_vel_y;
      limits.min_vel_y = config.min_vel_y;
      limits.max_vel_theta = config.max_vel_theta;
      limits.min_vel_theta = config.min_vel_theta;
      limits.acc_lim_x = config.acc_lim_x;
      limits.acc_lim_y = config.acc_lim_y;
      limits.acc_lim_theta = config.acc_lim_theta;
      limits.acc_lim_trans = config.acc_lim_trans;
      // limits.xy_goal_tolerance = config.xy_goal_tolerance;
      // limits.yaw_goal_tolerance = config.yaw_goal_tolerance;
      limits.prune_plan = config.prune_plan;
      limits.trans_stopped_vel = config.trans_stopped_vel;
      limits.theta_stopped_vel = config.theta_stopped_vel;
      planner_util_.reconfigureCB(limits, config.restore_defaults);

      // update dwa specific configuration
      dp_->reconfigure(config);
  }

  DWAPlannerROS::DWAPlannerROS(ros::CallbackQueue& cbqueue) 
      : cbqueue_(cbqueue)
      , initialized_(false)
      , odom_helper_(cbqueue, "odom")
      , setup_(false)
      , computeVelocityCommands_id_(0)
      , use_negative_vel_(false)
      , straight_ward_(false)
      , maybe_uturn_(false)
      , next_narrow_(false)
      , fresh_move_(false)
      , top_is_narrow_(false)
      , plan1_theta_(0)
      , uturn_around_(utaround_no)
  {
      // double robot_width = 0.3;
      double robot_width = ros::base_cfg.robot_width;
      // setNarrowFootprint(robot_width);
      narrow_footprint_ = ros::getNarrowFootprint(robot_width);
  }

  void DWAPlannerROS::initialize(
      std::string name,
      tf2_ros::Buffer* tf,
      costmap_2d::Costmap2DROS* costmap_ros) {
    if (! isInitialized()) {

      ros::NodeHandle private_nh("~/" + name);
      private_nh.setCallbackQueue(&cbqueue_);
      {
        // <clbrobot>/param/navigation/tank/base_local_planner_params.yaml
        private_nh.setParam("max_vel_x", 0.20);
        // private_nh.setParam("max_vel_x", ros::buildmap? 0.20: 0.25);
        private_nh.setParam("min_vel_x", -0.025);
        private_nh.setParam("max_vel_y", 0.0); // Notice!
        private_nh.setParam("min_vel_y", 0.0);
        private_nh.setParam("max_vel_trans", 0.25);
        private_nh.setParam("min_vel_trans", 0.02);
        private_nh.setParam("max_vel_theta", DEG2RAD(30)); // Notice!  0.7
        private_nh.setParam("min_vel_theta", 0.01); // degree: 0.57

        private_nh.setParam("acc_lim_x", 1.25);
        private_nh.setParam("acc_lim_y", 0.0);
        private_nh.setParam("acc_lim_theta", 5);
        private_nh.setParam("acc_lim_trans", 1.25);

        private_nh.setParam("prune_plan", false);

        ros::xy_goal_tolerance = 0.10; // ROS_DEF_XY_GOAL_TOLERANCE;
        // ros::yaw_goal_tolerance = ros::buildmap? M_PI: ROS_DEF_YAW_GOAL_TOLERANCE;
        ros::yaw_goal_tolerance = ROS_DEF_YAW_GOAL_TOLERANCE;

        private_nh.setParam("trans_stopped_vel", 0.1);
        private_nh.setParam("rot_stopped_vel", 0.1);
        // private_nh.setParam("sim_time", 3.0);
        // private_nh.setParam("sim_granularity", 0.025); // relative with max_vel_x. keep num_steps about 18
        private_nh.setParam("sim_granularity", 0.02); // 0.02 <= max_vel_x: 0.2
        private_nh.setParam("angular_sim_granularity", 0.1);
        private_nh.setParam("path_distance_bias", 34.0);
        private_nh.setParam("goal_distance_bias", 24.0);
        private_nh.setParam("occdist_scale", 0.05);
        private_nh.setParam("twirling_scale", 0.0);
        private_nh.setParam("stop_time_buffer", 0.5);
        private_nh.setParam("oscillation_reset_dist", 0.05);
        private_nh.setParam("oscillation_reset_angle", 0.2);
        private_nh.setParam("forward_point_distance", 0.3);
        private_nh.setParam("scaling_speed", 0.25);
        private_nh.setParam("max_scaling_factor", 0.2);
        private_nh.setParam("vx_samples", 16); // 20
        private_nh.setParam("vy_samples", 0);
        private_nh.setParam("vth_samples", 16); // 20

        private_nh.setParam("use_dwa", true);
        private_nh.setParam("restore_defaults", false);


        int ii = 0;
      }
      g_plan_pub_ = private_nh.advertise<nav_msgs::Path>("global_plan", 1);
      g_full_plan_pub_ = private_nh.advertise<nav_msgs::Path>("global_full_plan", 1);
      l_plan_pub_ = private_nh.advertise<nav_msgs::Path>("local_plan", 1);
      tf_ = tf;
      costmap_ros_ = costmap_ros;
      costmap_ros_->getRobotPose(current_pose_);

      // make sure to update the costmap we'll use for this cycle
      costmap_2d::Costmap2D* costmap = costmap_ros_->getCostmap();

      planner_util_.initialize(tf, costmap, costmap_ros_->getGlobalFrameID());

      //create the actual planner that we'll use.. it'll configure itself from the parameter server
      dp_ = boost::shared_ptr<DWAPlanner>(new DWAPlanner(cbqueue_, name, &planner_util_));

      if( private_nh.getParam( "odom_topic", odom_topic_ ))
      {
        odom_helper_.setOdomTopic( odom_topic_ );
      }
      
      initialized_ = true;

      // Warn about deprecated parameters -- remove this block in N-turtle
      nav_core::warnRenamedParameter(private_nh, "acc_lim_trans", "acc_limit_trans");
      nav_core::warnRenamedParameter(private_nh, "theta_stopped_vel", "rot_stopped_vel");

      dsrv_ = new dynamic_reconfigure::Server<DWAPlannerConfig>(private_nh);
      dynamic_reconfigure::Server<DWAPlannerConfig>::CallbackType cb = std::bind(&DWAPlannerROS::reconfigureCB, this, _1, _2);
      dsrv_->setCallback(cb);
    }
    else{
      ROS_WARN("This planner has already been initialized, doing nothing.");
    }
  }
  
  bool DWAPlannerROS::setPlan(const std::vector<geometry_msgs::PoseStamped>& orig_global_plan) {
    if (! isInitialized()) {
      ROS_ERROR("This planner has not been initialized, please call initialize() before using this planner");
      return false;
    }
    //when we get a new plan, we also want to clear any latch we may have on goal tolerances
    latchedStopRotateController_.resetLatching();

    ROS_INFO("Got new plan");
    next_traj_backword_ = false;
    next_narrow_ = false;
    uturn_around_ = utaround_no;
    return dp_->setPlan(orig_global_plan);
  }

  bool DWAPlannerROS::isGoalReached() {
    if (! isInitialized()) {
      ROS_ERROR("This planner has not been initialized, please call initialize() before using this planner");
      return false;
    }
    if ( ! costmap_ros_->getRobotPose(current_pose_)) {
      ROS_ERROR("Could not get robot pose");
      return false;
    }

    if(latchedStopRotateController_.isGoalReached(&planner_util_, odom_helper_, current_pose_)) {
      ROS_INFO("[isGoalReached]Goal reached");
      return true;
    } else {
      return false;
    }
  }

  bool DWAPlannerROS::xy_tolerance_latch() const {
    if (! isInitialized()) {
      ROS_ERROR("This planner has not been initialized, please call initialize() before using this planner");
      return false;
    }
    
    return latchedStopRotateController_.xy_tolerance_latch();
  }

  void DWAPlannerROS::reset_xy_tolerance_latch() {
    VALIDATE(isInitialized(), null_str);
    
    latchedStopRotateController_.resetLatching();
  }

  void DWAPlannerROS::publishLocalPlan(std::vector<geometry_msgs::PoseStamped>& path) {
    base_local_planner::publishPlan(path, l_plan_pub_);
  }


  void DWAPlannerROS::publishGlobalPlan(std::vector<geometry_msgs::PoseStamped>& path) {
    base_local_planner::publishPlan(path, g_plan_pub_);
  }

  void DWAPlannerROS::publishGlobalFullPlan(std::vector<geometry_msgs::PoseStamped>& path) {
    base_local_planner::publishPlan(path, g_full_plan_pub_);
  }

  DWAPlannerROS::~DWAPlannerROS(){
    ros::dwa_straight_ward = false;
    //make sure to clean things up
    delete dsrv_;
  }

void revise_twist_moveable(geometry_msgs::Twist& cmd_vel)
{
    const double abs_traj_linear_x = fabs(cmd_vel.linear.x);
    const double abs_traj_theta = fabs(cmd_vel.angular.z);
    if (abs_traj_linear_x < ros::base_cfg.min_moveable_vel_x && abs_traj_theta < ros::base_cfg.min_moveable_vel_theta) {
        double linear_x_percent = abs_traj_linear_x / ros::base_cfg.min_moveable_vel_x;
        double theta_percent = abs_traj_theta / ros::base_cfg.min_moveable_vel_theta;
        if (linear_x_percent >= theta_percent) {
            ROS_INFO("[revise little velocity](%.5f/%.5f=%.5f) >= (%.5f/%.5f=%.5f) revise linear.x", 
                cmd_vel.linear.x, ros::base_cfg.min_moveable_vel_x, linear_x_percent, cmd_vel.angular.z, ros::base_cfg.min_moveable_vel_theta, theta_percent);
            cmd_vel.linear.x = cmd_vel.linear.x > 0? ros::base_cfg.min_moveable_vel_x: -1 * ros::base_cfg.min_moveable_vel_x;
        } else {
            ROS_INFO("[revise little velocity](%.5f/%.5f=%.5f) >= (%.5f/%.5f=%.5f) revise angular.z", 
                cmd_vel.linear.x, ros::base_cfg.min_moveable_vel_x, linear_x_percent, cmd_vel.angular.z, ros::base_cfg.min_moveable_vel_theta, theta_percent);
            cmd_vel.angular.z = cmd_vel.angular.z > 0? ros::base_cfg.min_moveable_vel_theta: -1 * ros::base_cfg.min_moveable_vel_theta;
        }
    }
}

double DWAPlannerROS::calc_robot_to_global_plan_yaw(const tpose2d& robot_pose2d, const std::vector<geometry_msgs::PoseStamped>& global_plan, double* angle_ptr)
{
    const geometry_msgs::Point& first = global_plan.front().pose.position;
    const geometry_msgs::Point& last = global_plan.back().pose.position;

    return ros::shortest_angular_distance2(robot_pose2d.yaw, first.x, first.y, last.x, last.y, angle_ptr);

    // double deltax = last.x - first.x;
    // double deltay = last.y - first.y;
    // double goal_th = atan2(deltay, deltax);

    // return angles::shortest_angular_distance(robot_pose2d.yaw, goal_th);
}

void DWAPlannerROS::adjust_traj_backward(const geometry_msgs::PoseStamped& global_pose, const std::vector<geometry_msgs::PoseStamped>& global_plan, bool front_pressure, base_local_planner::Trajectory& traj, geometry_msgs::Twist& cmd_vel)
{
    bool require_adjust = false;
    if (use_negative_vel_ || straight_ward_ || maybe_uturn_) {
        
    } else if (front_pressure) {
        // MUST backward, highest priority.
        require_adjust = true;

    } else if (next_traj_backword_) {
        require_adjust = true;

    } else if (traj.cost_ < 0) {
        // Since there is no backwards(negative velocity), when the robot is attached to an obstacle, 
        // it may result no candidate trajectory available.
        require_adjust = fresh_move_;
        if (require_adjust) {
            SDL_Log("adjust_traj_backward, traj.cost_ < 0, fresh_move_: %s", fresh_move_? "true": "false");
        }

    } else {
        double start_x;
        double start_y;
        double start_th;
        double end_x;
        double end_y;
        double end_th;

        traj.getPoint(0, start_x, start_y, start_th);
        traj.getPoint(traj.getPointsSize() - 1, end_x, end_y, end_th);
        double dist = sqrt((start_x - end_x) * (start_x - end_x) + ((start_y - end_y)) * (start_y - end_y));
        double dist_threshold = 0.09;

        if (traj.xv_ >= 0 && dist <= dist_threshold) {
            SDL_Log("adjust_traj_backward want back, dist_threshold: %.5f, vel(%.5f, %.5f, %.5f), (%.5f, %.5f)-->(%.5f, %.5f) => dist: %.5f, global_plan.size: %i", 
                dist_threshold, traj.xv_, traj.yv_, traj.thetav_, start_x, start_y, end_x, end_y, dist, (int)global_plan.size());
            require_adjust = true;
        }

        {
            // require_adjust = true;
        }
    }

    if (!require_adjust) {
        next_traj_backword_ = false;
        return;
    }

    // bool ret = adjust_traj_internal(true, global_pose, global_plan, front_pressure, cmd_vel);

    VALIDATE(!maybe_uturn_, null_str);
    VALIDATE(!straight_ward_, null_str);
    tpose2d robot_pose2d(global_pose.pose.position.x, global_pose.pose.position.y, tf2::getYaw(global_pose.pose.orientation));

    const geometry_msgs::Point& first = global_plan.front().pose.position;
    const geometry_msgs::Point& last = global_plan.back().pose.position;

    double deltax = last.x - first.x;
    double deltay = last.y - first.y;
    double goal_th = atan2(deltay, deltax);

    // ang_diff and see LatchedStopRotateController::rotateToGoal(...) 
    const double pos_angular_z = DEG2RAD(6); // 0.1
    const double more_pos_angular_z = DEG2RAD(20);
    double v_theta_samp = pos_angular_z;
    double ang_diff = angles::shortest_angular_distance(robot_pose2d.yaw, goal_th);
      
    double ang_diff2 = calc_robot_to_global_plan_yaw(robot_pose2d, global_plan, nullptr);
    VALIDATE(ang_diff == ang_diff2, null_str);

    ROS_INFO("(next:%s)adjust_traj_backward, front_pressure: %s, ang_diff: %.3f, execute adjust...", 
        next_traj_backword_? "true": "false", front_pressure? "true": "false", RAD2DEG(ang_diff));

    double threshold = DEG2RAD(10);
    double abs_ang_diff = fabs(ang_diff);
    bool ang_diff_almost_0 = abs_ang_diff < threshold || (M_PI - abs_ang_diff) < threshold;
    if (ang_diff_almost_0) {
        v_theta_samp = 0;

    } else if (ang_diff < 0) {
        v_theta_samp = - v_theta_samp;
    }

    enum {z_more, z_normal, z_0, z_count};
    const double abs_angular_z_s[] = {z_more, z_normal, z_0};
    int z_start = (abs_ang_diff >= DEG2RAD(40) && !ang_diff_almost_0)? 0: 1;

    // const double def_backward_abs_linear_x = 0.12;
    // const double abs_linear_x_s[] = {def_backward_abs_linear_x, 0.08};
    const double def_backward_abs_linear_x = 0.10;
    const double abs_linear_x_s[] = {def_backward_abs_linear_x, 0.06};
    const int min_abs_linear_x_count = 1;
    const int max_abs_linear_x_count = sizeof(abs_linear_x_s) / sizeof(abs_linear_x_s[0]);
    int abs_linear_x_count = min_abs_linear_x_count;
    if (front_pressure || next_traj_backword_) {
        abs_linear_x_count = max_abs_linear_x_count; // include 0.1, 0.08
    }

    double linear_x = 0;
    bool valid_cmd = false;
    for (int z_at = z_start; z_at < z_count && !valid_cmd; z_at ++) {
        if (z_at == z_more) {
            VALIDATE(!ang_diff_almost_0, null_str);
            v_theta_samp = ang_diff >= 0? more_pos_angular_z: -more_pos_angular_z;

        } else if (z_at == z_normal) {
            if (ang_diff_almost_0) {
                v_theta_samp = 0;

            } else if (ang_diff < 0) {
                v_theta_samp = ang_diff >= 0? pos_angular_z: -pos_angular_z;
            }

        } else {
            VALIDATE(z_at == z_0, null_str);
            if (!ang_diff_almost_0) {
                v_theta_samp = 0;
            } else {
                v_theta_samp = ang_diff >= 0? pos_angular_z: -pos_angular_z;
            }
        }

        for (int at = 0; at < abs_linear_x_count && !valid_cmd; at ++) {
            linear_x = -1 * abs_linear_x_s[at];
            valid_cmd = dp_->checkTrajectory(Eigen::Vector3f(robot_pose2d.x, robot_pose2d.y, robot_pose2d.yaw),
                Eigen::Vector3f(0, 0, 0), Eigen::Vector3f(linear_x, 0.0, v_theta_samp));
        }
    }

    if (!valid_cmd) {
        ros::rose_slot.did_adjust_traj_backward(front_pressure, ang_diff, next_traj_backword_? "fail, is next": "fail");
        if (next_traj_backword_ || front_pressure) {
            // I hope to be able to backword this time, but can not.
            // Then choose to rotate and avoid pulling back and forth.
            if (ang_diff_almost_0) {
                v_theta_samp = next_traj_backword_? pos_angular_z: 0;
            } else {
                if ((ang_diff > 0 && !front5cm_lr_pressure_.front_has_obs) || (ang_diff < 0 && !front5cm_lr_pressure_.back_has_obs)) {
                    v_theta_samp = SDL_min(abs_ang_diff, DEG2RAD(30));
                } else {
                    v_theta_samp = SDL_min(abs_ang_diff, DEG2RAD(10));
                }
            }

            linear_x = front_pressure? -1 * def_backward_abs_linear_x: 0.0;
            v_theta_samp = ang_diff >= 0? v_theta_samp: -v_theta_samp;
            SDL_Log("all check trajectory fail, but 'next_traj_backword_ || front_pressure', linear_x: %.5f, v_theta_samp: %.5f", linear_x, RAD2DEG(v_theta_samp));

        } else {
            SDL_Log("all check trajectory fail, use findBestPath's cmd_vel");
            return;
        }

    } else if (fabs(linear_x) < ros::base_cfg.min_moveable_vel_x) {
        ROS_INFO("Got linear_x(%.5f) is less than min_moveable_vel_x(%.5f), increast it", linear_x, ros::base_cfg.min_moveable_vel_x);
        linear_x = -1 * ros::base_cfg.min_moveable_vel_x;
    }

    double next_adjust_threshold = DEG2RAD(20);
    next_traj_backword_ = fabs(M_PI / 2 - abs_ang_diff) < next_adjust_threshold;

    ROS_INFO("(next:%s)robot_yaw: %.5f, goal_th: %.5f, ang_diff: %.5f(%.5f) ==> vel(%.5f, 0.0, %.5f)", 
        next_traj_backword_? "true": "false", RAD2DEG(robot_pose2d.yaw), RAD2DEG(goal_th), RAD2DEG(ang_diff), RAD2DEG(threshold), linear_x, RAD2DEG(v_theta_samp));

    // if we don't have a legal trajectory, we'll just command zero
    cmd_vel.linear.x = linear_x;
    cmd_vel.linear.y = 0.0;
    cmd_vel.angular.z = v_theta_samp;
    ros::rose_slot.did_adjust_traj_backward(front_pressure, ang_diff, "success");

    // update traj.cost_
    traj.cost_ = 0;
    if (front_pressure) {
        // If there is obstacle in front of robot, want to backword twice
        next_traj_backword_ = true;
    }
}

bool DWAPlannerROS::rotateToGoal(const geometry_msgs::PoseStamped& global_pose, const tpose2d& goal_pose, geometry_msgs::Twist& cmd_vel)
{
    tpose2d robot_pose(global_pose.pose.position.x, global_pose.pose.position.y, tf2::getYaw(global_pose.pose.orientation));

    ROS_INFO("[rotateToGoal]---from(%s) --> to(%s)", robot_pose.to_string().c_str(), goal_pose.to_string().c_str());

    double deltax = goal_pose.x - robot_pose.x;
    double deltay = goal_pose.y - robot_pose.y;
    const double dist = hypot(deltax, deltay);
    double line_ang = atan2(deltay, deltax); // (-pi, pi]
    // atan2's return value: >0(clockwise), <0(anticlockwise). 
    // It is the opposite of what mathematics teaches
    line_ang = -1 * line_ang;
    double line_ang_diff = angles::shortest_angular_distance(robot_pose.yaw, line_ang);
    const bool linear_x_pos = abs(line_ang_diff) < M_PI / 2;
    ROS_INFO("[rotateToGoal]dist: %.3f, line_ang: %.3f(deg:%.3f), line_ang_diff: %.3f(deg:%.3f) linear_x_pos: %s", 
        dist, line_ang, RAD2DEG(line_ang), line_ang_diff, RAD2DEG(line_ang_diff), linear_x_pos? "true": "false");

    // ang_diff and see LatchedStopRotateController::rotateToGoal(...)
    double ang_diff = angles::shortest_angular_distance(robot_pose.yaw, goal_pose.yaw);
    double abs_ang_diff = fabs(ang_diff);
    const double max_angular_z = SDL_min(SDL_max(DEG2RAD(40), ros::base_cfg.max_navigation_vel_theta), abs_ang_diff);
    const double min_angular_z = DEG2RAD(10);

    const double angular_z_granularity = (max_angular_z - min_angular_z) / 2;
    double abs_angular_z_s[] = {max_angular_z, min_angular_z + angular_z_granularity, min_angular_z, 0};
    const int abs_angular_z_count = sizeof(abs_angular_z_s) / sizeof(abs_angular_z_s[0]);
    bool angular_z_pos = ang_diff >= 0;

    ROS_INFO("[rotateToGoal]angular_z range[%.3f, %.3f], angular_z_granularity: %.3f, ang_diff: %.3f(deg:%.3f) angular_z_pos: %s",
        RAD2DEG(min_angular_z), RAD2DEG(max_angular_z), RAD2DEG(angular_z_granularity), ang_diff, RAD2DEG(ang_diff), angular_z_pos? "true": "false");

    //
    const double abs_linear_x_s[] = {0, 0.02, 0.04, 0.06};
    const int max_abs_linear_x_count = sizeof(abs_linear_x_s) / sizeof(abs_linear_x_s[0]);
    int abs_linear_x_count = max_abs_linear_x_count;
    if (dist < 0.05) {
        // abs_linear_x_count = 2;
    }
    double linear_x = 0;
    double angular_z = 0;
    bool valid_cmd = false;
    for (int at = 0; at < abs_linear_x_count && !valid_cmd; at ++) {
        linear_x = linear_x_pos? abs_linear_x_s[at]: -1 * abs_linear_x_s[at];
        for (int at2 = 0; at2 < abs_angular_z_count && !valid_cmd; at2 ++) {
            angular_z = angular_z_pos? abs_angular_z_s[at2]: -1 * abs_angular_z_s[at2];
            if (linear_x == 0 && angular_z == 0) {
                continue;
            }
            valid_cmd = dp_->checkTrajectory(Eigen::Vector3f(robot_pose.x, robot_pose.y, robot_pose.yaw),
                Eigen::Vector3f(0, 0, 0), Eigen::Vector3f(linear_x, 0.0, angular_z));
            SDL_Log("[rotateToGoal][%i/%i][%i/%i]check trajectory(%.5f, 0.0, %.5f(deg:%.5f)) %s", 
                at, abs_linear_x_count, at2, abs_angular_z_count,
                linear_x, angular_z, RAD2DEG(angular_z), valid_cmd? "success": "fail");
        }
    }

    if (!valid_cmd) {
        double xy_goal_tolerance = ros::xy_goal_tolerance;
        if (dist < xy_goal_tolerance) {
            // The opposite vel to the above
            cmd_vel.linear.x = linear_x_pos? -1 * ros::base_cfg.min_moveable_vel_x: ros::base_cfg.min_moveable_vel_x;;
            cmd_vel.linear.y = 0.0;
            cmd_vel.angular.z = 0;
            SDL_Log("[rotateToGoal]check all candidate vel fail, buf dist(%.3f) < xy_goal_tolerance(%.3f), vel: (%.5f, 0.0, %.5f(deg:%.5f))", 
                dist, xy_goal_tolerance, cmd_vel.linear.x, cmd_vel.angular.z, RAD2DEG(cmd_vel.angular.z));
            return true;
        }
        SDL_Log("[rotateToGoal]check all candidate vel fail");
        return false;
    }

    // if we don't have a legal trajectory, we'll just command zero
    cmd_vel.linear.x = linear_x;
    cmd_vel.linear.y = 0.0;
    cmd_vel.angular.z = angular_z;
    // ros::rose_slot.did_adjust_traj_backward(front_pressure, ang_diff, "success");
    ROS_INFO("[rotateToGoal]selected vel: (%.5f, 0.0, %.5f(deg:%.5f))", cmd_vel.linear.x, cmd_vel.angular.z, RAD2DEG(cmd_vel.angular.z));
    return true;
}

  bool DWAPlannerROS::dwaComputeVelocityCommands(const geometry_msgs::PoseStamped &global_pose, geometry_msgs::Twist& cmd_vel) {
    // dynamic window sampling approach to get useful velocity commands
    if(! isInitialized()){
      ROS_ERROR("This planner has not been initialized, please call initialize() before using this planner");
      return false;
    }

    geometry_msgs::PoseStamped robot_vel;
    odom_helper_.getRobotVel(robot_vel);

    // compute what trajectory to drive along
    base_local_planner::Trajectory path = dp_->findBestPath(global_pose, robot_vel, cmd_vel, use_negative_vel_, straight_ward_, maybe_uturn_);

    const tpose2d robot_pose2d(global_pose.pose.position.x, global_pose.pose.position.y, tf2::getYaw(global_pose.pose.orientation));
    const std::vector<geometry_msgs::PoseStamped>& global_plan = dp_->global_plan();

    double global_plan_angle = 0;
    const double angle_diff = calc_robot_to_global_plan_yaw(robot_pose2d, dp_->global_plan(), &global_plan_angle);
    const double abs_angle_diff = fabs(angle_diff);
    if (path.cost_ >= 0 && straight_ward_) {
        VALIDATE(!maybe_uturn_, null_str);

        bool pos_theta = true;
        double max_abs_theta = 0;

        if (use_negative_vel_) {
            pos_theta = angle_diff < 0;
            max_abs_theta = M_PI - abs_angle_diff;

        } else {
            pos_theta = angle_diff > 0;
            max_abs_theta = abs_angle_diff;
        }

        {
             // xr-robot(0.12)
            const double straight_ward_abs_linear_x = !use_negative_vel_? ros::base_cfg.min_moveable_vel_x + 0.06: ros::base_cfg.min_moveable_vel_x + 0.04;
            double linear_x = !use_negative_vel_? straight_ward_abs_linear_x: -1 * straight_ward_abs_linear_x;
            double angular_z = (pos_theta? 1: -1) * SDL_min(max_abs_theta, ros::base_cfg.min_moveable_vel_theta);
            bool only_rotate = false;
            double plan1_threshold = DEG2RAD(8);
            double only_ratate_linear_x = 0;
            double conside_opposite_obs_threshold = DEG2RAD(45);
            if (!use_negative_vel_) {
                if (padded5cm_pressure_.front_has_obs) {
                    only_rotate = true;
                    only_ratate_linear_x = -ROS_RID_PRESSURE_LINEAR_X;
                    SDL_Log("[narrow+straight_ward]only rotate, forward and front_has_obs");

                } /* else if (plan1_theta_ >= plan1_threshold) {
                    only_rotate = true;
                    SDL_Log("[narrow+straight_ward]only rotate, forward and abs_plan1_theta_(%.3f) >= plan1_threshold(%.3f)", plan1_theta_, plan1_threshold);

                } */ else if (padded5cm_pressure_.back_has_obs && max_abs_theta > conside_opposite_obs_threshold) {
                    only_rotate = true;
                    SDL_Log("[narrow+straight_ward]only rotate, forward and back_has_obs(conside opposite), max_abs_theta(%.3f) > conside_opposite_obs_threshold(%.3f)",
                        RAD2DEG(max_abs_theta), RAD2DEG(conside_opposite_obs_threshold));
                }

            } else {
                if (padded5cm_pressure_.back_has_obs) {
                    only_rotate = true;
                    only_ratate_linear_x = ROS_RID_PRESSURE_LINEAR_X;
                    SDL_Log("[narrow+straight_ward]only rotate, backward and back_has_obs");

                } else if (padded5cm_pressure_.front_has_obs && max_abs_theta > conside_opposite_obs_threshold) {
                    only_rotate = true;
                    SDL_Log("[narrow+straight_ward]only rotate, backward and front_has_obs(conside opposite), max_abs_theta(%.3f) > conside_opposite_obs_threshold(%.3f)",
                        RAD2DEG(max_abs_theta), RAD2DEG(conside_opposite_obs_threshold));
                }
            }
            if (only_rotate) {
                linear_x = only_ratate_linear_x;
                angular_z = (pos_theta? 1: -1) * (ros::base_cfg.min_moveable_vel_theta + DEG2RAD(10));
            }
            SDL_Log("[narrow+straight_ward]path from [(%.5f, %.5f, %.5f)-cost:%.5f] to [(%.5f, 0, %.5f)]. angle: %.5f, angle_diff: %.5f, %s_theta, max_abs_theta: %.5f",
                path.xv_, path.yv_, RAD2DEG(path.thetav_), path.cost_,
                linear_x, RAD2DEG(angular_z),
                RAD2DEG(global_plan_angle), RAD2DEG(angle_diff),
                pos_theta? "pos": "neg", RAD2DEG(max_abs_theta));
            path.xv_ = linear_x;
            path.thetav_ = angular_z;
        }

        cmd_vel.linear.x = path.xv_;
        cmd_vel.angular.z = path.thetav_;
    }

    if (maybe_uturn_) {
        VALIDATE(!use_negative_vel_, null_str);
        VALIDATE(!straight_ward_, null_str);

        bool valid_farward = true;
        if (path.cost_ >= 0) {
            double linear_x = 0.00; // 0.05
            double thetav = path.thetav_;
            VALIDATE(uturn_around_ == utaround_no || uturn_around_ == utaround_clockwise2 || uturn_around_ == utaround_anticlockwise2, null_str);
            if (uturn_around_ == utaround_clockwise2) {
                thetav = -1 * fabs(path.thetav_);
                SDL_Log("[path is all back]turn_around_ is taround_clockwise2, use thetav: %.5f(path:%.5f)", thetav, path.thetav_);

            } else if (uturn_around_ == utaround_anticlockwise2) {
                thetav = fabs(path.thetav_);
                SDL_Log("[path is all back]turn_around_ is taround_anticlockwise2, use thetav: %.5f(path:%.5f)", thetav, path.thetav_);
            } else {
                SDL_Log("[path is all back]turn_around_ is taround_no, use thetav: %.5f(path:%.5f)", thetav, path.thetav_);
            }


            const double abs_angular_z_s[] = {DEG2RAD(45), DEG2RAD(60), DEG2RAD(75), DEG2RAD(90), 
                DEG2RAD(105), DEG2RAD(130), DEG2RAD(145), DEG2RAD(160)};
            const int max_abs_angular_z_count = sizeof(abs_angular_z_s) / sizeof(abs_angular_z_s[0]);
            double angular_z = 0;
            for (int at = 0; at < max_abs_angular_z_count && valid_farward; at ++) {
                angular_z = thetav > 0? abs_angular_z_s[at]: -1 * abs_angular_z_s[at];
                valid_farward = dp_->checkTrajectory(Eigen::Vector3f(robot_pose2d.x, robot_pose2d.y, robot_pose2d.yaw),
                    Eigen::Vector3f(0.0f, 0.0f, 0.0f), Eigen::Vector3f(linear_x, 0.0f, angular_z));
                SDL_Log("[path is all back][%i/%i]check trajectory(%.5f, 0.0, %.5f(deg:%.5f)) %s", 
                    at, max_abs_angular_z_count, linear_x, angular_z, RAD2DEG(angular_z), valid_farward? "success": "fail");
            }

            if (valid_farward) {
                if (path.thetav_ != thetav) {
                    path.thetav_ = thetav;

                    cmd_vel.angular.z = thetav;
                }

                uturn_around_ = thetav > 0? utaround_anticlockwise: utaround_clockwise;
            }

        } else {
            valid_farward = false;
        }
        if (!valid_farward) {
            
            if (!padded5cm_pressure_.back_has_obs) {
                SDL_Log("[path is all back]forward check fail, use only_backward");
                uturn_around_ = utaround_no;
                use_negative_vel_ = true;
                maybe_uturn_ = false;
                path = dp_->findBestPath(global_pose, robot_vel, cmd_vel, use_negative_vel_, straight_ward_, maybe_uturn_);

            } else {
                cmd_vel.angular.z = 0;
                if (!front5cm_lr_pressure_.front_has_obs && front5cm_lr_pressure_.back_has_obs) {
                    cmd_vel.angular.z = DEG2RAD(30);
                    SDL_Log("[path is all back]forward check fail, but padded5cm's back has obstacle, (6.1)use front5cm_lr_pressure_");

                } else if (front5cm_lr_pressure_.front_has_obs && !front5cm_lr_pressure_.back_has_obs) {
                    cmd_vel.angular.z = -1 * DEG2RAD(30);
                    SDL_Log("[path is all back]forward check fail, but padded5cm's back has obstacle, (6.2)use front5cm_lr_pressure_");
                }

                if (cmd_vel.angular.z == 0 && uturn_around_ == utaround_anticlockwise2 || uturn_around_ == utaround_clockwise2) {
                    double theta = uturn_around_ == utaround_anticlockwise2? DEG2RAD(30): -1 * DEG2RAD(30);
                    bool valid = dp_->checkTrajectory(Eigen::Vector3f(robot_pose2d.x, robot_pose2d.y, robot_pose2d.yaw),
                        Eigen::Vector3f(0.0f, 0.0f, 0.0f), Eigen::Vector3f(0.0f, 0.0f, theta));
                    if (valid) {
                        cmd_vel.angular.z = theta;
                        SDL_Log("[path is all back]forward check fail, but padded5cm's back has obstacle, (6.3)uturn_around_(%i) --> angular.z: %.3f", uturn_around_, RAD2DEG(theta));
                    }
                }

                if (cmd_vel.angular.z == 0 && path.cost_ >= 0) {
                    cmd_vel.angular.z = path.thetav_;
                    SDL_Log("[path is all back]forward check fail, but padded5cm's back has obstacle, (6.4)use path.thetav_");
                }

                if (cmd_vel.angular.z == 0) {
                    // double ang_diff = calc_robot_to_global_plan_yaw(robot_pose2d, dp_->global_plan());
                    if (top_is_narrow_) {
                        // Not to make up for the ang_diff
                        cmd_vel.angular.z = angle_diff > 0? DEG2RAD(-10): DEG2RAD(10);
                        SDL_Log("[path is all back]forward check fail, but padded5cm's back has obstacle, (6.5)top_is_narrow_ is true, use ang_diff: %.3f", 
                            RAD2DEG(angle_diff));
                    } else {
                        cmd_vel.angular.z = angle_diff > 0? DEG2RAD(30): DEG2RAD(-30);
                        SDL_Log("[path is all back]forward check fail, but padded5cm's back has obstacle, (6.6)use ang_diff: %.3f", 
                            RAD2DEG(angle_diff));
                    }
                }

                // Since know that there is an obstacle behind robot, give a small positive speed, 
                // so that robot can move forward a little while rotating.
                // const double tiny_linear_x = 0.01; // 0.01
                path.cost_ = 0;
                path.xv_ = ROS_RID_PRESSURE_LINEAR_X;
                path.yv_ = 0;
                path.thetav_ = cmd_vel.angular.z;

                cmd_vel.linear.x = path.xv_;
                cmd_vel.linear.y = path.yv_;
                // cmd_vel.angular.z = path.thetav_;

                uturn_around_ = cmd_vel.angular.z > 0? utaround_anticlockwise: utaround_clockwise;
            }
        }
        if (!use_negative_vel_ && pressure_.front_has_obs) {
            if (cmd_vel.linear.x >= 0) {
                SDL_Log("[path is all back]front_has_obs is true, change linear.x from %.3f to %.3f", cmd_vel.linear.x, -1 * ros::base_cfg.min_moveable_vel_theta);
                cmd_vel.linear.x = -1 * ros::base_cfg.min_moveable_vel_theta;
            }
        }
    }

    // pass along drive commands
    adjust_traj_backward(global_pose, dp_->global_plan(), pressure_.front_has_obs, path, cmd_vel);
    if (fresh_move_) {
        if (path.cost_ < 0) {
            path.cost_ = 0;
            //  straight forward
            const double linear_x = 0.12;
            cmd_vel.linear.x = linear_x;
            cmd_vel.linear.y = 0.0;
            cmd_vel.angular.z = 0.0;
            ROS_INFO_NAMED("dwa_local_planner", "fresh_move_ == true, public vel(%.5f, %.5f, %.5f)", 
                cmd_vel.linear.x, cmd_vel.linear.y, cmd_vel.angular.z);
        }
        fresh_move_ = false;
    }

    //if we cannot move... tell someone
    std::vector<geometry_msgs::PoseStamped> local_plan;
    if(path.cost_ < 0) {
      ROS_INFO_NAMED("dwa_local_planner",
          "The dwa local planner failed to find a valid plan, cost functions discarded all candidates. This can mean there is an obstacle too close to the robot.");
      local_plan.clear();
      publishLocalPlan(local_plan);
      return false;
    }

    ROS_INFO_NAMED("dwa_local_planner", "A valid velocity command of (%.5f, %.5f, %.5f[deg:%.5f]) was found for this cycle.", 
                    cmd_vel.linear.x, cmd_vel.linear.y, cmd_vel.angular.z, RAD2DEG(cmd_vel.angular.z));

    // Fill out the local plan
    for(unsigned int i = 0; i < path.getPointsSize(); ++i) {
      double p_x, p_y, p_th;
      path.getPoint(i, p_x, p_y, p_th);

      geometry_msgs::PoseStamped p;
      p.header.frame_id = costmap_ros_->getGlobalFrameID();
      p.header.stamp = ros::Time::now();
      p.pose.position.x = p_x;
      p.pose.position.y = p_y;
      p.pose.position.z = 0.0;
      tf2::Quaternion q;
      q.setRPY(0, 0, p_th);
      tf2::convert(q, p.pose.orientation);
      local_plan.push_back(p);
    }

    //publish information to the visualizer

    publishLocalPlan(local_plan);
    return true;
  }

SDL_Rect calculate_exclusion_rect(const SDL_Rect& plan_rect, int size_x, int size_y, const SDL_Point& origin)
{
    enum {exclusion_bottom, exclusion_top, exclusion_left, exclusion_right};
    const SDL_Rect candidates[4] = {
        {0, 0,              size_x,     size_y / 3}, // bottom
        {0, size_y * 2 / 3, size_x,     size_y / 3}, // top

        {0, 0,              size_x / 3, size_y}, // left
        {size_x * 2 / 3, 0, size_x / 3, size_y}, // right
    };

    const int s = 1;
    const SDL_Rect quadrants[4] = {
        {origin.x + 1 + s, origin.y + 1 + s, size_x / 2,   size_y / 2}, // #1
        {0,                origin.y + 1 + s, origin.x - s, size_y / 2}, // #2

        {0,                0,                origin.x - s, origin.y - s}, // #3
        {origin.x + 1 + s, 0,                size_x / 2,   origin.y - s}, // #4
    };
    uint32_t flags = 0;
    int intersections = 0;
    for (int at = 0; at < sizeof(quadrants) / sizeof(quadrants[0]); at ++) {
        if (SDL_HasIntersection(&plan_rect, quadrants + at)) {
            flags |= 1 << at;
            intersections ++;
        }
    }

    SDL_Rect exclusion_rect{0, 0, 0, 0};
    if (intersections == 2) {
        if ((flags & 0x1) && (flags & 0x2)) {
            exclusion_rect = candidates[exclusion_bottom];

        } else if ((flags & 0x2) && (flags & 0x4)) {
            exclusion_rect = candidates[exclusion_right];

        } else if ((flags & 0x4) && (flags & 0x8)) {
            exclusion_rect = candidates[exclusion_top];

        } else {
            // (flags & 0x8) && (flags & 0x1)
            exclusion_rect = candidates[exclusion_left];
        }
    } else if (intersections == 1) {
        if (flags & 0x1) {
            SDL_IntersectRect(&candidates[exclusion_left], &candidates[exclusion_bottom], &exclusion_rect); 
        } else if (flags & 0x2) {
            SDL_IntersectRect(&candidates[exclusion_bottom], &candidates[exclusion_right], &exclusion_rect); 
        } else if (flags & 0x4) {
            SDL_IntersectRect(&candidates[exclusion_right], &candidates[exclusion_top], &exclusion_rect); 
        } else {
            // flags & 0x8
            SDL_IntersectRect(&candidates[exclusion_top], &candidates[exclusion_left], &exclusion_rect); 
        }
    }
    return exclusion_rect;
}

void DWAPlannerROS::resolve_pressure(bool front_back, const tpose2d& robot_pose2d, const std::vector<SDL_Point>& obs_cells, double min_threshold, double max_threshold, 
    tpressure_result& result, std::set<tpoint>* front_obs_cells, std::set<tpoint>* back_obs_cells, bool verbose)
{
    VALIDATE(min_threshold >= 0 && max_threshold > min_threshold, null_str);
    boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(costmap_ros_->getCostmap()->getMutex()));
    const costmap_2d::Costmap2D& costmap = *costmap_ros_->getCostmap();

    unsigned robot_cell_x;
    unsigned robot_cell_y;
    costmap.worldToMap(robot_pose2d.x, robot_pose2d.y, robot_cell_x, robot_cell_y);

    double world_x;
    double world_y;
    result.clear();
    int at = 0;
    enum {front, back};

    for (std::vector<SDL_Point>::const_iterator it = obs_cells.begin(); it != obs_cells.end(); ++ it, at ++) {
        const SDL_Point& goal = *it;
        costmap.mapToWorld(goal.x, goal.y, world_x, world_y);

        double deltax = world_x - robot_pose2d.x;
        double deltay = world_y - robot_pose2d.y;
        // (-pi, pi]
        double goal_th = atan2(deltay, deltax);

        double ang_diff = angles::shortest_angular_distance(robot_pose2d.yaw, goal_th);
        double abs_ang_diff = fabs(ang_diff);
        double abs_PI_ang_diff = M_PI - abs_ang_diff;

        int belong_to = nposm;
        if (front_back) {
            if (abs_ang_diff >= min_threshold && abs_ang_diff <= max_threshold) {
                result.front_has_obs = true;
                if (front_obs_cells != nullptr) {
                    front_obs_cells->insert(tpoint(goal.x, goal.y));
                }
                belong_to = front;

            } else if (abs_PI_ang_diff >= min_threshold && abs_PI_ang_diff <= max_threshold) {
                result.back_has_obs = true;

                if (back_obs_cells != nullptr) {
                    back_obs_cells->insert(tpoint(goal.x, goal.y));
                }
                belong_to = back;
            }

        } else {
            if (abs_ang_diff >= min_threshold && abs_ang_diff <= max_threshold) {
                if (ang_diff >= 0) {
                    // left
                    result.front_has_obs = true;
                    if (front_obs_cells != nullptr) {
                        front_obs_cells->insert(tpoint(goal.x, goal.y));
                    }
                    belong_to = front;

                } else {
                    // right
                    result.back_has_obs = true;
                    if (back_obs_cells != nullptr) {
                        back_obs_cells->insert(tpoint(goal.x, goal.y));
                    }
                    belong_to = back;
                }
            }
        }

        if (verbose) {
            char belong_to_str[][12] = {"front", "back"};
            SDL_Log("resolve_pressure, [%i/%i]threshold:(%.3f, %.3f) robot: %s, (%i, %i) -> (%i, %i), ang_diff: %.3f, (M_PI - abs_ang_diff): %.3f, belong to: %s", 
                at, (int)obs_cells.size(), RAD2DEG(min_threshold), RAD2DEG(max_threshold), robot_pose2d.to_string().c_str(), 
                robot_cell_x, robot_cell_y, goal.x, goal.y, RAD2DEG(ang_diff), RAD2DEG(abs_PI_ang_diff), 
                belong_to != nposm? belong_to_str[belong_to]: "NO");
        }
    }

    if (verbose) {
        if (front_back) {
            SDL_Log("resolve_front_back_pressure, obs_cells.size: %i, front_has_obs: %s, back_has_obs: %s", 
                (int)obs_cells.size(), result.front_has_obs? "true": "false", result.back_has_obs? "true": "false");
        } else {
            SDL_Log("resolve_pos_neg_pressure, obs_cells.size: %i, pos_has_obs: %s, neg_has_obs: %s", 
                (int)obs_cells.size(), result.front_has_obs? "true": "false", result.back_has_obs? "true": "false");
        }
    }
}

void DWAPlannerROS::save_costmap(const std::string& scene, const SDL_Point& src, const SDL_Point& dst, const std::vector<geometry_msgs::PoseStamped>& transformed_plan,
    const tpose2d& robot_pose, double inscribed_radius, const SDL_Rect& exclusion_rect)
{
    const std::string target_frame = "map";
    const std::string source_frame = costmap_ros_->getGlobalFrameID(); // "odom"
    geometry_msgs::TransformStamped transform = tf_->lookupTransform(target_frame, source_frame, ros::Time());
    tpose2d odom_2_map_pose(transform.transform.translation.x, transform.transform.translation.y, tf2::getYaw(transform.transform.rotation));
    SDL_Log("[%s]save_costmap, odom_2_map_pose: %s", scene.c_str(), odom_2_map_pose.to_string().c_str());

    const std::string file_name = game_config::preferences_dir + "/1.costmap";

    const std::vector<geometry_msgs::Point>& footprint = costmap_ros_->getRobotFootprint();
    costmap_2d::Costmap2D& costmap = *costmap_ros_->getCostmap();

    costmap.saveMap(file_name, transformed_plan, footprint, tpose2d_C{odom_2_map_pose.x, odom_2_map_pose.y, odom_2_map_pose.yaw, true}, 
        robot_pose.x, robot_pose.y, robot_pose.yaw, inscribed_radius, src, dst, exclusion_rect);

    const std::string file = game_config::preferences_dir + "/1.png";
    ros::costmap2d_2_SDL_Surface(costmap, file, robot_pose.x, robot_pose.y, robot_pose.yaw, transformed_plan, footprint);
}

int distinguishable_obs_cells(const costmap_2d::Costmap2D& costmap, const SDL_Point& robot_cell, const std::set<tpoint>& _obs_cells, bool verbose, int& cell_dist, std::vector<SDL_Point>* ret_ptr)
{
    VALIDATE(!_obs_cells.empty(), null_str);

    const bool spread8 = false;
    std::set<tpoint> obs_cells = _obs_cells;
    std::stringstream ss;
    for (std::set<tpoint>::const_iterator it = _obs_cells.begin(); it != _obs_cells.end(); ++ it) {
        const tpoint& cell = *it;
        if (spread8) {
            costmap.thisCost8Cells(cell.x, cell.y, costmap_2d::LETHAL_OBSTACLE, &obs_cells);
        }
        if (verbose) {
            ss << " (" << cell.x << ", " << cell.y << ")";
        }
    }
    if (verbose) {
        SDL_Log("input obs_cells: %s", ss.str().c_str());
    }

    struct ttmp_point {
        SDL_Point cell;
        bool merged;
        int mass;
        double angle;
    };
    
    const int obs_cells_size = obs_cells.size();
    std::vector<ttmp_point> tmp_points;
    tmp_points.resize(obs_cells.size());

    ttmp_point* tmp_points_ptr = &tmp_points[0];
    int at = 0;
    for (std::set<tpoint>::const_iterator it = obs_cells.begin(); it != obs_cells.end(); ++ it, at ++) {
        const tpoint& cell = *it;

        double deltax = cell.x - robot_cell.x;
        double deltay = cell.y - robot_cell.y;
        // (-pi, pi]
        double goal_th = atan2(deltay, deltax);

        if (verbose) {
            SDL_Log("[%i/%i]obs_cells cell(%i, %i) --> goal_th: %.3f", at, (int)obs_cells.size(), cell.x, cell.y, RAD2DEG(goal_th));
        }

        ttmp_point& to = tmp_points_ptr[at];
        to.cell.x = cell.x;
        to.cell.y = cell.y;
        to.angle = goal_th;
        to.merged = false;
        to.mass = nposm;
    }

    std::vector<int> masses(obs_cells_size, 0); // value is cell count belong to this mass.
    int* masses_ptr = &masses[0];

    const bool enable_angle_merge = false;
    const double angle_threshold = DEG2RAD(6);
    const int xy_threshold = spread8? 5: 6;
    int next_mass_index = 0;
    for (at = 0; at < obs_cells_size; at ++) {
        ttmp_point& cell1 = tmp_points_ptr[at];
        if (cell1.merged) {
            continue;
        }
        if (cell1.mass == nposm) {
            cell1.mass = next_mass_index ++;
            masses_ptr[cell1.mass] ++;
        }
        bool mergeable = false;
        for (int at2 = at + 1; at2 < obs_cells_size; at2 ++) {
            ttmp_point& cell2 = tmp_points_ptr[at2];
            VALIDATE(cell1.cell.x != cell2.cell.x || cell1.cell.y != cell2.cell.y, null_str);

            if (cell2.merged) {
                continue;
            }
            
            double ang_diff = angles::shortest_angular_distance(cell1.angle, cell2.angle);
            double abs_angle_diff = fabs(ang_diff);
            if (enable_angle_merge && (abs_angle_diff < angle_threshold)) {
                // merge
                cell2.merged = true;
                if (cell2.mass != nposm) {
                    masses_ptr[cell2.mass] --;
                    cell2.mass = nposm;
                }

            } else if (cell2.mass == nposm) {
                int x_diff = cell1.cell.x - cell2.cell.x;
                x_diff = posix_abs(x_diff);
                int y_diff = cell1.cell.y - cell2.cell.y;
                y_diff = posix_abs(y_diff);
                if (x_diff <= xy_threshold && y_diff <= xy_threshold) {
                    cell2.mass = cell1.mass;
                    masses_ptr[cell1.mass] ++;
                }
            }

        }
    }

    int best_mass_count = 0;
    int best_mass_index = nposm;
    for (at = 0; at < obs_cells_size; at ++) {
        int count = masses_ptr[at];
        if (at < next_mass_index) {
            if (count > best_mass_count) {
                best_mass_count = count;
                best_mass_index = at;
            }
        } else {
            VALIDATE(count == 0, null_str);
        }
    }
    VALIDATE(best_mass_count > 0 && best_mass_index != nposm, null_str);
    
    // The above is only a rough classification, and then classify continues
    bool massable = next_mass_index > 1;
    while (massable) {
        massable = false;
        for (at = 0; at < obs_cells_size && !massable; at ++) {
            ttmp_point& cell1 = tmp_points_ptr[at];
            if (cell1.merged || cell1.mass == best_mass_index) {
                continue;
            }
            for (int at2 = 0; at2 < obs_cells_size && !massable; at2 ++) {
                const ttmp_point& cell2 = tmp_points_ptr[at2];
                if (cell2.merged || cell2.mass != best_mass_index) {
                    continue;
                }
                int x_diff = cell1.cell.x - cell2.cell.x;
                x_diff = posix_abs(x_diff);
                int y_diff = cell1.cell.y - cell2.cell.y;
                y_diff = posix_abs(y_diff);

                if (x_diff <= xy_threshold && y_diff <= xy_threshold) {
                    VALIDATE(masses_ptr[cell1.mass] >= 1, null_str);
                    massable = true;

                    const int from_mass = cell1.mass;
                    // set cells mass that 'mass == cell1.mass' to best_mass_index.
                    // if only one, valueate it, else use for.
                    if (masses_ptr[from_mass] != 1) {
                        for (int at3 = 0; at3 < obs_cells_size; at3 ++) {
                            ttmp_point& cell3 = tmp_points_ptr[at3];
                            if (cell3.mass == from_mass) {
                                VALIDATE(!cell3.merged, null_str);
                                cell3.mass = best_mass_index;
                            }
                        }
                    } else {
                        cell1.mass = best_mass_index;
                    }

                    // update masses vector.
                    masses_ptr[best_mass_index] += masses_ptr[from_mass];
                    masses_ptr[from_mass] = 0;
                }
            }
        }
    }

    if (verbose && next_mass_index > 1) {
        int valid_mass_count = 0;
        int cells = 0;
        for (at = 0; at < obs_cells_size; at ++) {
            if (masses_ptr[at] != 0) {
                valid_mass_count ++;
                cells += masses_ptr[at];
            }
        }
        if (valid_mass_count == 1) {
            VALIDATE(cells == masses_ptr[best_mass_index], null_str);   
        } else {
            VALIDATE(cells > masses_ptr[best_mass_index], null_str);
        }
    }

    int min_x = INT32_MAX;
    int min_y = INT32_MAX;
    int max_x = INT32_MIN;
    int max_y = INT32_MIN;

    std::vector<SDL_Point> ret;
    for (at = 0; at < obs_cells_size; at ++) {
        const ttmp_point& cell = tmp_points_ptr[at];
        if (cell.merged) {
            VALIDATE(cell.mass == nposm, null_str);
            continue;
        }

        VALIDATE(cell.mass != nposm, null_str);
        if (cell.mass == best_mass_index) {
            ret.push_back(cell.cell);
            posix_touch_i32(cell.cell.x, cell.cell.y, &min_x, &min_y, &max_x, &max_y);
        }
    }

    const int max_x_dist = max_x - min_x;
    const int max_y_dist = max_y - min_y;
    cell_dist = max_x_dist >= max_y_dist? max_x_dist: max_y_dist;

    if (ret_ptr != nullptr) {
        *ret_ptr = ret;
    }

    return ret.size();
}

bool calc_narrow_by_distinguishable_cells(const costmap_2d::Costmap2D& costmap, const SDL_Point& robot_cell, 
    const std::set<tpoint>& front_obs_cells, const std::set<tpoint>& back_obs_cells, bool narrow)
{
    if (narrow) {
        VALIDATE(!front_obs_cells.empty() && !front_obs_cells.empty(), null_str);
        // if 'next_narrow == true', of couse must pass narrow. 
        // Notice: front_obs_cells or back_obs_cells maybe empty when if 'next_narrow == true'.
        int front_cell_dist;
        int front = distinguishable_obs_cells(costmap, robot_cell, front_obs_cells, true, front_cell_dist, nullptr);

        int back_cell_dist;
        int back = distinguishable_obs_cells(costmap, robot_cell, back_obs_cells, true, back_cell_dist, nullptr);

        const int min_distinguishable_cells = 4;
        const int min_cell_dist = 3;
        if (front < min_distinguishable_cells || front_cell_dist < min_cell_dist || 
            back < min_distinguishable_cells || back_cell_dist < min_cell_dist ) {
            SDL_Log("at least one distinguishable_cells(%i, %i) < min_distinguishable_cells(%i) or cell_size(%i, %i) < min_cell_size(%i), set narrow false",
                front, back, min_distinguishable_cells, front_cell_dist, back_cell_dist, min_cell_dist);
            narrow = false;
        } else {
            SDL_Log("both distinguishable_cells(%i, %i) >= min_distinguishable_cells(%i) and both cell_size(%i, %i) >= min_cell_size(%i), don't modify narrow",
                front, back, min_distinguishable_cells, front_cell_dist, back_cell_dist, min_cell_dist);
        }
    }
    return narrow;
}

#define handle_first_gt_at_least_dist() \
    if (first_gt_at_least_dist) {   \
        const int min_is_narrow_judges = only_forward? min_is_narrow_judges_forward: min_is_narrow_judges_backward; \
        this_narrow = is_narrow_judges >= min_is_narrow_judges || b_f_judges == is_narrow_judges;    \
        if (only_backward) {    \
            this_narrow = calc_narrow_by_distinguishable_cells(costmap, src_cell, front_obs_cells, back_obs_cells, this_narrow); \
        }   \
        narrow = this_narrow || next_narrow;    \
        if (!narrow) {  \
            if (only_backward) {    \
                only_backward = false;  \
            }   \
            if (only_forward) { \
                only_forward = false;   \
            }   \
        }   \
        first_gt_at_least_dist = false; \
    }

bool DWAPlannerROS::adjust_global_plan(const geometry_msgs::PoseStamped& global_pose, const std::vector<SDL_Point>& FREE_SPACE_obs_cells, double dist_to_goal, bool next_narrow, std::vector<geometry_msgs::PoseStamped>& transformed_plan)
{
    VALIDATE(!next_narrow_, null_str);
    boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(costmap_ros_->getCostmap()->getMutex()));

    costmap_2d::Costmap2D& costmap = *costmap_ros_->getCostmap();
    const uint8_t* raw_costmap_data = costmap.getCharMap();
    const int size_x = costmap.getSizeInCellsX();
    const int size_y = costmap.getSizeInCellsY();

    unsigned int origin_map_x;
    unsigned int origin_map_y;
    bool valid = costmap.worldToMap(global_pose.pose.position.x, global_pose.pose.position.y, origin_map_x, origin_map_y);
    if (!valid) {
        // if trigger breakpoint during visual studio debugging, may enter here.
        SDL_Log("adjust_global_plan, fail, global_pose.pose.position out of costmap");
        return false;
    }
    const SDL_Point src_cell = SDL_Point{(int)origin_map_x, (int)origin_map_y};

    double robot_x = global_pose.pose.position.x;
    double robot_y = global_pose.pose.position.y;
    double robot_yaw = tf2::getYaw(global_pose.pose.orientation);
    const tpose2d robot_pose2d(robot_x, robot_y, robot_yaw);
    const double inscribed_radius = costmap_ros_->getLayeredCostmap()->getInscribedRadius();
    const bool save_map = game_config::os == os_windows;

    int min_x = INT32_MAX;
    int min_y = INT32_MAX;
    int max_x = INT32_MIN;
    int max_y = INT32_MIN;

    std::vector<SDL_Point> plan_points;
    int diff_points;
    bool maybe_unreachable = ros::transformed_plan_2_plan_points(robot_pose2d, transformed_plan, costmap, plan_points, diff_points, min_x, min_y, max_x, max_y);

    const int min_points = 3;
    const double min_consider_unreachable_dist = 0.7; // 70cm
    if (diff_points < min_points) {
        if (save_map) {
            SDL_Rect empty_rect{0, 0, 0, 0};
            SDL_Point dst{nposm, nposm};
            if (!plan_points.empty()) {
                dst.x = plan_points.back().x;
                dst.y = plan_points.back().y;
            }
            save_costmap("diff_points < min_points#1", src_cell, dst,
                transformed_plan, robot_pose2d, inscribed_radius, empty_rect);
            // ros::rviz_breakpoint = true;
        }
        // if trigger breakpoint during visual studio debugging, may enter here.
        const geometry_msgs::Point& first = transformed_plan.front().pose.position;
        const geometry_msgs::Point& last = transformed_plan.back().pose.position;
        SDL_Log("adjust_global_plan, fail, diff_points(%i) < min_points(%i)#1, transformed_plan.size(%i), first(%.5f, %.5f), last(%.5f, %.5f), plan_points.size(%i)", 
            diff_points, min_points, (int)transformed_plan.size(), first.x, first.y, last.x, last.y, (int)plan_points.size()); 
        return false;

    } else if (maybe_unreachable && dist_to_goal >= min_consider_unreachable_dist) {
        if (save_map) {
            // SDL_Rect empty_rect{0, 0, 0, 0};
            // save_costmap("maybe_unreachable#1", costmap_2d::MapLocation{origin_map_x, origin_map_y}, costmap_2d::MapLocation{(uint32_t)plan_points.back().x, (uint32_t)plan_points.back().y},
            //    transformed_plan, robot_pose2d, inscribed_radius, empty_rect);
        }
        // ros::rviz_breakpoint = true;

        SDL_Log("adjust_global_plan, fail, maybe_unreachable#1 is true");
        return false;
    }

    SDL_Log("===adjust_global_plan, global_pose[(%.5f, %.5f)-yaw:%.5f], global_path:[%i](%.5f, %.5f)(%i, %i)-->(%.5f, %.5f)(%i, %i), plan_points.size: %i, Origin(%.5f, %.5f)",
        robot_x, robot_y, RAD2DEG(robot_yaw), (int)transformed_plan.size(),
        transformed_plan.front().pose.position.x, transformed_plan.front().pose.position.y, plan_points.front().x, plan_points.front().y,
        transformed_plan.back().pose.position.x, transformed_plan.back().pose.position.y, plan_points.back().x, plan_points.back().y,
        (int)plan_points.size(), costmap.getOriginX(), costmap.getOriginY());

    SDL_Rect plan_rect{min_x, min_y, max_x - min_x + 1, max_y - min_y + 1};
    SDL_Rect exclusion_rect = calculate_exclusion_rect(plan_rect, size_x, size_y, 
        SDL_Point{(int)origin_map_x, (int)origin_map_y});
    
    int cell_inscribed_radius = costmap.cellDistance(inscribed_radius);
    
    const pathfind::shortest_path_calculator calc(costmap, cell_inscribed_radius);
    pathfind::plain_route route = pathfind::a_star_search(pathfind::search_global_plan, SDL_Point{(int)origin_map_x, (int)origin_map_y}, plan_points.back(), 0.0,
        calc, costmap, cell_inscribed_radius, exclusion_rect, false);

    const bool fail_again = true;
    if (fail_again && route.steps.size() < min_points) {
        SDL_Log("adjust_global_plan, a_star_search#1 fail, route: %s, try again", route.toString().c_str());
        route.steps.clear();
        {
            boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(costmap_ros_->getCostmap()->getMutex()));
            costmap_ros_->clearCostmaps();
            costmap_ros_->updateMap();
        }

        ros::transformed_plan_2_plan_points(robot_pose2d, transformed_plan, costmap, plan_points, diff_points, min_x, min_y, max_x, max_y);
        if (plan_points.size() < min_points) {
            if (save_map) {
                SDL_Point dst{nposm, nposm};
                if (!plan_points.empty()) {
                    dst.x = plan_points.back().x;
                    dst.y = plan_points.back().y;
                }
                save_costmap("diff_points < min_points#2", src_cell, dst,
                    transformed_plan, robot_pose2d, inscribed_radius, exclusion_rect);
                // ros::rviz_breakpoint = true;
            }
            // if trigger breakpoint during visual studio debugging, may enter here.
            const geometry_msgs::Point& first = transformed_plan.front().pose.position;
            const geometry_msgs::Point& last = transformed_plan.back().pose.position;
            SDL_Log("adjust_global_plan, fail, diff_points(%i) < min_points(%i)#2, transformed_plan.size(%i), first(%.5f, %.5f), last(%.5f, %.5f), plan_points.size(%i)", 
                diff_points, min_points, (int)transformed_plan.size(), first.x, first.y, last.x, last.y, (int)plan_points.size()); 
            return false;
        } if (maybe_unreachable && dist_to_goal >= min_consider_unreachable_dist) {
            if (save_map) {
                // save_costmap("maybe_unreachable#2", costmap_2d::MapLocation{origin_map_x, origin_map_y}, costmap_2d::MapLocation{(uint32_t)plan_points.back().x, (uint32_t)plan_points.back().y},
                //    transformed_plan, robot_pose2d, inscribed_radius, exclusion_rect);
            }
            // ros::rviz_breakpoint = true;

            SDL_Log("adjust_global_plan, fail, maybe_unreachable#2 is true");
            return false;
        }

        route = pathfind::a_star_search(pathfind::search_global_plan, SDL_Point{(int)origin_map_x, (int)origin_map_y}, plan_points.back(), 0.0,
            calc, costmap, cell_inscribed_radius, exclusion_rect, false);
    }

    bool a_star_search_fail = false;
    if (route.steps.empty()) {
        a_star_search_fail = true; // !!!REMENBER
        SDL_Log("adjust_global_plan, a_star_search fail, n_loops: %i, global_path.size: %i, save costmap", route.n_loops, (int)transformed_plan.size());
        if (save_map) {
            save_costmap("a_star_serach fail", src_cell, plan_points.back(),
                transformed_plan, robot_pose2d, inscribed_radius, exclusion_rect);
        }

    } else if (route.steps.size() < min_points) {
        a_star_search_fail = true;
        SDL_Log("adjust_global_plan, a_star_search ok, n_loops: %i, global_path.size from %i to %i, but too short path", 
            route.n_loops, (int)transformed_plan.size(), (int)route.steps.size());
    }

    if (a_star_search_fail) {
        SDL_Log("adjust_global_plan, a_star_search fail, clear transformed_plan");
        return false;
    }

    // generate new plan
    base_local_planner::LocalPlannerLimits limits = planner_util_.getCurrentLimits();
    const double sim_max_vel_x = 0.2; // limits.max_vel_x
    const double sim_max_vel_y = 0; // limits.max_vel_y
    double vmag = hypot(sim_max_vel_x, sim_max_vel_y);
    // sim_time must be 1.7s, so sim_time_dist = 0.2m/s * 1.7s = 0.34m
    double sim_time_dist = vmag * default_config_.sim_time;
    // double b_f_dist = sim_time_dist * 0.6; // 0.8
    double at_least_dist = sim_time_dist * 1.0;
    double max_dist = sim_time_dist * 1.3; // 1.3
    // double sq_b_f_dist = b_f_dist * b_f_dist;
    double sq_at_least_dist = at_least_dist * at_least_dist;
    double sq_max_dist = max_dist * max_dist;

    double start_x = global_pose.pose.position.x;
    double start_y = global_pose.pose.position.y;

    // generate new plan(2/2)
    const std::vector<geometry_msgs::PoseStamped> old_transformed_plan = transformed_plan;
    std::vector<geometry_msgs::PoseStamped>& new_plan = transformed_plan;
    // SDL_Log("adjust_global_plan, a_star_search ok, obs_cells: %i, global_path.size from %i to %i, sim_time_dist(%.5f), adjust...", 
    //    obs_cells, (int)new_plan.size(), (int)route.steps.size(), sim_time_dist);
    SDL_Log("adjust_global_plan, a_star_search ok, robot_yaw: %.5f, global_path.size from %i to %s, sim_time_dist(%.5f), adjust...", 
        RAD2DEG(robot_yaw), (int)new_plan.size(), route.toString().c_str(), sim_time_dist);
    new_plan.clear();
    geometry_msgs::PoseStamped pose;
    ros::Time plan_time = ros::Time::now();
    

    costmap_2d::tset_cost_restorer LETHAL_OBSTACLE_restorer(costmap, FREE_SPACE_obs_cells, costmap_2d::LETHAL_OBSTACLE);
    const bool verbose_path_case = game_config::os == os_windows;
    // distance    cost
    // 1.414    -> 253
    // 2        -> 217
    // 2.236    -> 205
    // 3        -> 169
    // 4        -> 132
    const int min_cut_cost = 102;

    const int min_check_path_at = route.steps.size() > 3? 3: 2;
    // There must be an extra cell for calculating 'only_forward' and 'only_backward'. 
    // at last, 'only_forward' and 'only_backward' MUST NOT be both true.
    VALIDATE(min_check_path_at < (int)route.steps.size(), null_str);

    bool narrow = true;
    bool only_backward = true;
    bool only_forward = true;
    maybe_uturn_ = true;
    const SDL_Rect* first_point_ptr = nullptr;
    double base_th = 0;
    bool new_plan_terminated = false;
    int at = 0;
    const int min_b_f_judges = 6;
    const int min_is_narrow_judges_forward = 2;
    const int min_is_narrow_judges_backward = 3; // 4
    VALIDATE(min_b_f_judges >= min_is_narrow_judges_backward, null_str);
    VALIDATE(min_is_narrow_judges_backward >= min_is_narrow_judges_forward, null_str);
    int b_f_judges = 0;
    int is_narrow_judges = 0;
    bool first_gt_at_least_dist = true;
    bool this_narrow = false;
    std::set<tpoint> front_obs_cells;
    std::set<tpoint> back_obs_cells;

    tpose2d local_2_map(0.0, 0.0, 0.0);
    if (costmap_ros_->getGlobalFrameID() != "map") {
        const std::string target_frame = "map";
        const std::string source_frame = costmap_ros_->getGlobalFrameID(); // "odom"
        geometry_msgs::TransformStamped transform = tf_->lookupTransform(target_frame, source_frame, ros::Time());
        local_2_map.set(transform.transform.translation.x, transform.transform.translation.y, tf2::getYaw(transform.transform.rotation), true);
    }

    top_is_narrow_ = calc_top_is_narrow(robot_pose2d, local_2_map);
    for (std::vector<SDL_Rect>::const_iterator it = route.steps.begin(); it != route.steps.end(); ++ it, at ++) {
        const SDL_Rect& p = *it;
        // convert the map-point to world coordinates
        double world_x, world_y;
        costmap.mapToWorld(p.x, p.y, world_x, world_y);

        pose.header.stamp = plan_time;
        pose.header.frame_id = costmap_ros_->getGlobalFrameID();
        pose.pose.position.x = world_x;
        pose.pose.position.y = world_y;
        pose.pose.position.z = 0.0;
        pose.pose.orientation.x = 0.0;
        pose.pose.orientation.y = 0.0;
        pose.pose.orientation.z = 0.0;
        pose.pose.orientation.w = 1.0;

        double dq_dist = (world_x - start_x) * (world_x - start_x) + (world_y - start_y) * (world_y - start_y);
        bool insert = false;
        if (dq_dist <= sq_at_least_dist) {
            if (b_f_judges < min_b_f_judges) {
                insert = true;

                b_f_judges ++;
                double deltax = world_x - global_pose.pose.position.x;
                double deltay = world_y - global_pose.pose.position.y;
                if (at == 0) {
                    const SDL_Rect& p1 = route.steps[1];
                    double world_x1, world_y1;
                    costmap.mapToWorld(p1.x, p1.y, world_x1, world_y1);
                    // route.steps[0] and global_pose.pose.position are in the same cell, 
                    // and the angle is uncertain. use steps[1]'s angle.
                    deltax = world_x1 - global_pose.pose.position.x;
                    deltay = world_y1 - global_pose.pose.position.y;
                }
                double goal_th = atan2(deltay, deltax);

                double ang_diff = angles::shortest_angular_distance(robot_yaw, goal_th);
                double only_backward_threshold = DEG2RAD(80);
                double all_back_threshold = DEG2RAD(60); // 80 (navigation)60 is too small

                double abs_ang_diff = fabs(ang_diff);
                // distance    cost
                // 1.414    -> 253
                // 2        -> 217
                // 2.236    -> 205
                // 3        -> 169
                // 3.162    -> 162
                // 4        -> 132
                // 4.123    -> 128
                // 5        -> 102
                // 5.385    -> 93
                // 6        -> 80
                if (at == 1) {
                    // now, plan[1] equals plan[0] always.
                    plan1_theta_ = abs_ang_diff;
                }

                // why '>='? --see below 'only_backward = false' and 'maybe_uturn_ = false'
                VALIDATE(only_backward_threshold >= all_back_threshold, null_str);
                VALIDATE(all_back_threshold * 2 < DEG2RAD(180), null_str);


                narrow = is_narrow_point(local_2_map, world_x, world_y, goal_th, &front_obs_cells, &back_obs_cells, false);
                if (narrow) {
                    is_narrow_judges ++;
                }
                if (at >= min_check_path_at) {
                    if (abs_ang_diff <= only_backward_threshold) {
                    // if (abs_ang_diff <= all_backward_threshold) {
                        only_backward = false;
                    }
                    if (abs_ang_diff > only_backward_threshold) {
                    // if (abs_ang_diff > all_backward_threshold) {
                        only_forward = false;
                    }
                    if (abs_ang_diff <= all_back_threshold) {
                        maybe_uturn_ = false;
                    }
                }
                if (verbose_path_case) {
                    SDL_Log("[%i/%i](at_least_dist.1/2), b_f_judges: %i/%i narrow: %s (%i,%i,%i,%i)goal_th:%.4f ang_diff:%.4f only_backward:%s only_forward[%s] maybe_uturn_:%s left_obs_cells: %i right_obs_cells: %i",
                        at, (int)route.steps.size(), b_f_judges, min_b_f_judges, narrow? "true": "false",
                        p.x, p.y, p.w, p.h, RAD2DEG(goal_th), RAD2DEG(ang_diff), 
                        only_backward? "yes": "no", only_forward? "yes": "no", maybe_uturn_? "yes": "no",
                        (int)front_obs_cells.size(), (int)back_obs_cells.size());
                }
            } else if (!new_plan_terminated) {
                handle_first_gt_at_least_dist();
                if (only_backward || only_forward) {
                    // straight, make road more straight. don't condise (1.0~1.3)
                    new_plan_terminated = true;
                }
                if (verbose_path_case) {
                    SDL_Log("[%i/%i](at_least_dist.2/2), b_f_judges: %i/%i narrow: %s (%i,%i,%i,%i) only_backward:%s only_forward[%s] maybe_uturn_:%s",
                        at, (int)route.steps.size(), b_f_judges, min_b_f_judges, narrow? "true": "false",
                        p.x, p.y, p.w, p.h,
                        only_backward? "yes": "no", only_forward? "yes": "no", maybe_uturn_? "yes": "no");
                }
            }

        } else if (!new_plan_terminated && dq_dist <= sq_max_dist) {
            handle_first_gt_at_least_dist();
            if (only_backward || only_forward) {
                // straight, make road more straight. don't condise (1.0~1.3)
                new_plan_terminated = true;
            } else {
                if (first_point_ptr == nullptr) {
                    const int s = new_plan.size();
                    VALIDATE(s >= min_points, null_str);
                    VALIDATE(min_points >= 3, null_str);
                    const SDL_Rect& first = route.steps[s - 3];
                    const SDL_Rect& last = route.steps[s - 1];

                    double deltax = last.x - first.x;
                    double deltay = last.y - first.y;
                    base_th = atan2(deltay, deltax);
                    first_point_ptr = &first;
                    SDL_Log("[%i/%i](cut.1/2) dist[%.5f/%.5f] s: %i, (%i, %i) --> (%i, %i) = %.5f", 
                        at, (int)route.steps.size(), sqrt(dq_dist), sqrt(sq_at_least_dist), s, first.x, first.y, last.x, last.y, RAD2DEG(base_th));
                }
                const SDL_Rect& first = *first_point_ptr;
                const SDL_Rect& last = p;
                double deltax = last.x - first.x;
                double deltay = last.y - first.y;
                double goal_th = atan2(deltay, deltax);

                double ang_diff2 = angles::shortest_angular_distance(base_th, goal_th);
                double threshold = DEG2RAD(20);
                double abs_ang_diff = fabs(ang_diff2);
                if (abs_ang_diff < threshold || p.h < min_cut_cost) {
                    insert = true;
                } else {    
                    new_plan_terminated = true;
                    // ros::rviz_breakpoint = true;
                    SDL_Log("[%i/%i](cut.2/2)terminated, dist[%.5f/%.5f], at: %i/%i, ang_diff: %.5f, (%i, %i) --> (%i, %i) = %.5f", 
                        at, (int)route.steps.size(), sqrt(dq_dist), sqrt(sq_at_least_dist), at, (int)route.steps.size(), RAD2DEG(ang_diff2), first.x, first.y, last.x, last.y, RAD2DEG(goal_th));
                }
                // SDL_Log("(%i, %i) --> (%i, %i) = %.5f, ang_diff: %.5f(%.5f) insert: %s", 
                //    first.x, first.y, last.x, last.y, RAD2DEG(goal_th), RAD2DEG(ang_diff), RAD2DEG(abs_ang_diff), insert? "true": "false");
            }
        }
        if (insert) {
            new_plan.push_back(pose);
        }
        global_full_plan_.push_back(pose);
    }

    if (new_plan_terminated) {
        VALIDATE(!first_gt_at_least_dist, null_str);
    }
    handle_first_gt_at_least_dist();

    if (verbose_path_case) {
        SDL_Log("#%u narrow[%s] this_narrow[%s] top_is_narrow[%s] only_backward[%s] only_forward[%s] maybe_uturn_[%s] use_negative_vel_[%s] plan1_theta_: %.3f, local_2_map: %s",
            computeVelocityCommands_id_, narrow? "true": "false", this_narrow? "true": "false", top_is_narrow_? "true": "false", 
            only_backward? "true": "false", only_forward? "true": "false", maybe_uturn_? "true": "false", use_negative_vel_? "true": "false",
            RAD2DEG(plan1_theta_), local_2_map.to_string().c_str());
    }
    
    if (only_backward) {
        VALIDATE(!use_negative_vel_, null_str);
        VALIDATE(!only_forward, null_str);
        VALIDATE(maybe_uturn_, null_str);
        maybe_uturn_ = false;

        straight_ward_ = true;
        use_negative_vel_ = true;
        next_narrow_ = this_narrow;

/*
        {
            LETHAL_OBSTACLE_restorer.manual_restore();

            save_costmap("dbg only_backward", src_cell, plan_points.back(),
                old_transformed_plan, robot_pose2d, inscribed_radius, exclusion_rect);
            ros::rviz_breakpoint = true;
        }
*/
    } else if (only_forward) {
        VALIDATE(!use_negative_vel_, null_str);
        VALIDATE(!only_backward, null_str);
        // if 'only_backward_threshold >= all_backward_threshold', now maybe_uturn_ maybe true.
        // for example: -79.2084 -76.9980 -64.7392
        // VALIDATE(!maybe_uturn_, null_str);
        maybe_uturn_ = false;

        straight_ward_ = true;
        next_narrow_ = this_narrow;

/*
        {
            LETHAL_OBSTACLE_restorer.manual_restore();

            save_costmap("dbg only_forward", src_cell, plan_points.back(),
                old_transformed_plan, robot_pose2d, inscribed_radius, exclusion_rect);
            ros::rviz_breakpoint = true;
        }
*/
    }

    if (straight_ward_) {
        ros::dwa_straight_ward = straight_ward_; 
    }

    ros::rose_slot.did_post_adjust_global_plan(computeVelocityCommands_id_, narrow, top_is_narrow_, straight_ward_);
    return true;
}

const std::vector<geometry_msgs::Point>& DWAPlannerROS::getNarrowFootprint()
{
    VALIDATE(narrow_footprint_.size() == 4, null_str);
    return narrow_footprint_;
}

bool get_footprint_obs_cells(costmap_2d::Costmap2D& costmap, const std::vector<geometry_msgs::Point>& footprint, const tpose2d& robot_pose, double padding_x, double padding_y, double yaw_increment, std::vector<SDL_Point>& obs_cells)
{
    obs_cells.clear();
    // costmap_2d::Costmap2D& costmap = *costmap_ros_->getCostmap();
    const unsigned char* costs = costmap.getCharMap();

    std::vector<geometry_msgs::Point> padded10cm_footprint = footprint;
    if (padding_x != 0 || padding_y != 0) {
        costmap_2d::padFootprint(padded10cm_footprint, padding_x, padding_y);
    }

    std::vector<geometry_msgs::Point> transformed_footprint;
    costmap_2d::transformFootprint(robot_pose.x, robot_pose.y, robot_pose.yaw + yaw_increment, padded10cm_footprint, transformed_footprint);

    // we assume the polygon is given in the global_frame... we need to transform it to map coordinates
    const std::vector<geometry_msgs::Point>& polygon = transformed_footprint;
    std::vector<costmap_2d::MapLocation> map_polygon;
    for (unsigned int i = 0; i < polygon.size(); ++i) {
        costmap_2d::MapLocation loc;
        if (!costmap.worldToMap(polygon[i].x, polygon[i].y, loc.x, loc.y)) {
            // ("Polygon lies outside map bounds, so we can't fill it");
            return false;
        }
        map_polygon.push_back(loc);
    }

    std::vector<costmap_2d::MapLocation> polygon_cells;

    // get the cells that fill the polygon
    costmap.convexFillCells(map_polygon, polygon_cells);

    // std::vector<SDL_Point> obs_cells;
    for (unsigned int at = 0; at < polygon_cells.size(); ++ at) {
        const costmap_2d::MapLocation& goal = polygon_cells[at];

        unsigned int index = costmap.getIndex(goal.x, goal.y);
        if (costs[index] == costmap_2d::LETHAL_OBSTACLE) {
            obs_cells.push_back(SDL_Point{(int)goal.x, (int)goal.y});
            continue;
        }
    }

    // const bool save_transform_footprint = game_config::os == os_windows;
    const bool save_transform_footprint = false;
    if (save_transform_footprint) {
        std::set<tpoint> set_obs_cells;
        for (std::vector<costmap_2d::MapLocation>::const_iterator it = polygon_cells.begin(); it != polygon_cells.end(); ++ it) {
            const costmap_2d::MapLocation& cell = *it;
            set_obs_cells.insert(tpoint(cell.x, cell.y));
        }

        std::string main_file = "a_star_move.png";
        if (yaw_increment > 0) {
            main_file = "a_star_move-anticlockwise.png";
        } else if (yaw_increment < 0) {
            main_file = "a_star_move-clockwise.png";
        }

        std::vector<geometry_msgs::PoseStamped> transformed_plan;
        std::string file = game_config::preferences_dir + "/" + main_file;
        ros::costmap2d_2_SDL_Surface(costmap, file, robot_pose.x, robot_pose.y, 
            robot_pose.yaw + yaw_increment, transformed_plan, padded10cm_footprint);
    }
    return true;
}

bool DWAPlannerROS::is_narrow_point(const tpose2d& local_2_map, double x, double y, double yaw, std::set<tpoint>* front_obs_cells, std::set<tpoint>* back_obs_cells, bool verbose)
{
    double min_threshold = DEG2RAD(30);
    double max_threshold = DEG2RAD(150);

    std::vector<SDL_Point> obs_cells;
    tpose2d pose2d(x, y, yaw);
    get_footprint_obs_cells(*costmap_ros_->getCostmap(), getNarrowFootprint(), pose2d, 0.00, 0.00, -1 * local_2_map.yaw, obs_cells);

    tpressure_result narrow_pressure;
    // obs_cells.insert(obs_cells.end(), FREE_SPACE_obs_cells.begin(), FREE_SPACE_obs_cells.end()); 
    resolve_pressure(false, pose2d, obs_cells, min_threshold, max_threshold, narrow_pressure, front_obs_cells, back_obs_cells, verbose);

    return narrow_pressure.front_has_obs && narrow_pressure.back_has_obs;
}

bool DWAPlannerROS::calc_top_is_narrow(const tpose2d& local_2_map, const tpose2d& robot_pose)
{
    double robot_length = 0.15; // 0.15cm
    double half_legnth = robot_length / 2;

    double cos_th = cos(robot_pose.yaw);
    double sin_th = sin(robot_pose.yaw);
    double x = robot_pose.x + half_legnth * cos_th;
    double y = robot_pose.y + half_legnth * sin_th;

    SDL_Log("calc_top_is_narrow top(x: %.3f, y: %.3f)", x, y);
    return is_narrow_point(local_2_map, x, y, robot_pose.yaw, nullptr, nullptr, false);
}

void DWAPlannerROS::transform_footprint_pressure(const tpose2d& robot_pose, double padding_x, double padding_y, double yaw_increment, tpressure_result& result)
{
    std::vector<SDL_Point> obs_cells;
    get_footprint_obs_cells(*costmap_ros_->getCostmap(), costmap_ros_->getUnpaddedRobotFootprint(), robot_pose, padding_x, padding_y, yaw_increment, obs_cells);

    const double threshold = DEG2RAD(35);
    resolve_pressure(true, robot_pose, obs_cells, 0, threshold, result, nullptr, nullptr, true);
}

bool DWAPlannerROS::a_star_search_move(const geometry_msgs::PoseStamped& planner_goal, geometry_msgs::Twist& cmd_vel)
{
    geometry_msgs::PoseStamped goal_pose;
    std::vector<geometry_msgs::PoseStamped> global_plan;
    global_plan.push_back(planner_goal);
    if (!base_local_planner::getGoalPose(*tf_, global_plan, costmap_ros_->getGlobalFrameID(), goal_pose)) {
    // if (!planner_util_.getGoal(goal_pose)) {
        ROS_ERROR("[a_star_search_move]Could not get goal pose");
        return false;
    }

    boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(costmap_ros_->getCostmap()->getMutex()));
    costmap_ros_->updateMap();
    if (!costmap_ros_->getRobotPose(current_pose_)) {
        ROS_ERROR("[a_star_search_move]Could not get robot pose");
        return false;
    }
    costmap_2d::Costmap2D& costmap = *costmap_ros_->getCostmap();
    const unsigned char* costs = costmap.getCharMap();
    tpose2d robot_pose(current_pose_.pose.position.x, current_pose_.pose.position.y, tf2::getYaw(current_pose_.pose.orientation));
    unsigned int robot_map_x, robot_map_y;
    if (!costmap.worldToMap(robot_pose.x, robot_pose.y, robot_map_x, robot_map_y)) {
        return false;
    }

    // 
    const double max_angular_z = SDL_min(DEG2RAD(40), ros::base_cfg.max_navigation_vel_theta);
    const double min_angular_z = DEG2RAD(10);

    const double angular_z_granularity = (max_angular_z - min_angular_z) / 2;
    double abs_angular_z_s[] = {max_angular_z, -1 * max_angular_z, 
        min_angular_z + angular_z_granularity, -1 * (min_angular_z + angular_z_granularity),
        min_angular_z, -1 * min_angular_z, 
        0};
    const int abs_angular_z_count = sizeof(abs_angular_z_s) / sizeof(abs_angular_z_s[0]);

    ROS_INFO("[a_star_search_move]angular_z range[%.3f, %.3f], angular_z_granularity: %.3f",
        RAD2DEG(min_angular_z), RAD2DEG(max_angular_z), RAD2DEG(angular_z_granularity));

    //
    const double abs_linear_x_s[] = {0.10, 0.05, 0, -0.05};
    const int max_abs_linear_x_count = sizeof(abs_linear_x_s) / sizeof(abs_linear_x_s[0]);
    int abs_linear_x_count = max_abs_linear_x_count;

    double linear_x = 0;
    double angular_z = 0;
    bool valid_cmd = false;
    for (int at = 0; at < abs_linear_x_count && !valid_cmd; at ++) {
        linear_x = abs_linear_x_s[at];
        for (int at2 = 0; at2 < abs_angular_z_count && !valid_cmd; at2 ++) {
            angular_z = abs_angular_z_s[at2];
            if (linear_x == 0 && angular_z == 0) {
                continue;
            }
            valid_cmd = dp_->checkTrajectory(Eigen::Vector3f(robot_pose.x, robot_pose.y, robot_pose.yaw),
                Eigen::Vector3f(0, 0, 0), Eigen::Vector3f(linear_x, 0.0, angular_z));
            SDL_Log("[a_star_search_move][%i/%i][%i/%i]check trajectory(%.5f, 0.0, %.5f(deg:%.5f)) %s", 
                at, abs_linear_x_count, at2, abs_angular_z_count,
                linear_x, angular_z, RAD2DEG(angular_z), valid_cmd? "success": "fail");
        }
    }

    if (!valid_cmd) {
        tpressure_result h_result;
        transform_footprint_pressure(robot_pose, 0.15, 0.0, 0.0, h_result);

        const tpose2d& first = robot_pose;
        const geometry_msgs::Point& last = goal_pose.pose.position;

        double deltax = last.x - first.x;
        double deltay = last.y - first.y;
        double goal_th = atan2(deltay, deltax);

        double ang_diff = angles::shortest_angular_distance(robot_pose.yaw, goal_th);

        bool v_theta_pos = ang_diff > 0;
        SDL_Log("ang_diff: %.3f v_theta: %s", RAD2DEG(ang_diff), v_theta_pos? "positive": "nagetive");

        linear_x = 0;
        angular_z = 0;
        if (!h_result.front_has_obs) {
            linear_x = ros::base_cfg.min_moveable_vel_x;
            valid_cmd = true;
            SDL_Log("[a_star_search_move]use footprint(1/5), forward"); 
        }

        tpressure_result anticlockwise_result;
        tpressure_result clockwise_result;
        if (!valid_cmd) {
            double raw_increament = DEG2RAD(40);
            transform_footprint_pressure(robot_pose, 0.0, 0.0, raw_increament, anticlockwise_result);
            transform_footprint_pressure(robot_pose, 0.0, 0.0, -1 * raw_increament, clockwise_result);
            // 

            const tpressure_result& rotate_result = v_theta_pos? anticlockwise_result: clockwise_result;
            if (!rotate_result.front_has_obs && !rotate_result.back_has_obs) {
                angular_z = v_theta_pos? ros::base_cfg.min_moveable_vel_theta: -1 * ros::base_cfg.min_moveable_vel_theta;
                valid_cmd = true;
                SDL_Log("[a_star_search_move]use footprint(2/5), reducing angle"); 
            }
        }

        if (!valid_cmd) {
            if (!h_result.back_has_obs) {
                linear_x = -1 * ros::base_cfg.min_moveable_vel_x;
                valid_cmd = true;
                SDL_Log("[a_star_search_move]use footprint(3/5), backward"); 
            }
        }

        if (!valid_cmd) {
            const tpressure_result& anti_rotate_result = v_theta_pos? clockwise_result: anticlockwise_result;
            if (!anti_rotate_result.front_has_obs && !anti_rotate_result.back_has_obs) {
                angular_z = v_theta_pos? -1 * ros::base_cfg.min_moveable_vel_theta: ros::base_cfg.min_moveable_vel_theta;
                valid_cmd = true;
                SDL_Log("[a_star_search_move]use footprint(4/5), anti-reducing angle"); 
            }
        }

        if (!valid_cmd) {
            // same as meet with 'rotate_result' 
            angular_z = v_theta_pos? ros::base_cfg.min_moveable_vel_theta: -1 * ros::base_cfg.min_moveable_vel_theta;
            valid_cmd = true;
            SDL_Log("[a_star_search_move]use footprint(5/5), check all candidate vel fail, but must use one, same as reducing angle"); 
        } 
    }

    VALIDATE(valid_cmd, null_str);

    // if we don't have a legal trajectory, we'll just command zero
    cmd_vel.linear.x = linear_x;
    cmd_vel.linear.y = 0.0;
    cmd_vel.angular.z = angular_z;
    // ros::rose_slot.did_adjust_traj_backward(front_pressure, ang_diff, "success");
    ROS_INFO("[a_star_search_move]selected vel: (%.5f, 0.0, %.5f(deg:%.5f))", cmd_vel.linear.x, cmd_vel.angular.z, RAD2DEG(cmd_vel.angular.z));
    revise_twist_moveable(cmd_vel);

    return true;
}

  bool DWAPlannerROS::computeVelocityCommands(geometry_msgs::Twist& cmd_vel) {
    // dispatches to either dwa sampling control or stop and rotate control, depending on whether we have been close enough to goal
    computeVelocityCommands_id_ ++;
    const bool next_narrow = next_narrow_;
    next_narrow_ = false;

    if ( ! costmap_ros_->getRobotPose(current_pose_)) {
      ROS_ERROR("Could not get robot pose");
      return false;
    }
    std::vector<geometry_msgs::PoseStamped> transformed_plan;
    if ( ! planner_util_.getLocalPlan(current_pose_, transformed_plan)) {
      ROS_ERROR("Could not get local plan");
      return false;
    }

    //if the global plan passed in is empty... we won't do anything
    if(transformed_plan.empty()) {
      ROS_WARN_NAMED("dwa_local_planner", "Received an empty transformed plan.");
      return false;
    }
    // ROS_INFO_NAMED("dwa_local_planner", "Received a transformed plan with %zu points.", transformed_plan.size());
    geometry_msgs::PoseStamped goal_pose;
    if (!planner_util_.getGoal(goal_pose)) {
        ROS_WARN_NAMED("dwa_local_planner", "Could not get goal_pose.");
        return false;
    }
    tpose2d goal_pose2d(goal_pose.pose.position.x, goal_pose.pose.position.y, tf2::getYaw(goal_pose.pose.orientation));

    if (costmap_2d::rose_mode) {
        double goal_x = goal_pose.pose.position.x;
        double goal_y = goal_pose.pose.position.y;
        double near_goal_tolerance = 1.5; // 1.5m
        double dist_to_goal = base_local_planner::getGoalPositionDistance(current_pose_, goal_pose2d.x, goal_pose2d.y);
        near_goal_ = dist_to_goal <= near_goal_tolerance;
        use_negative_vel_ = false;
        straight_ward_ = false;
        maybe_uturn_ = false;
        top_is_narrow_ = false;
        plan1_theta_ = 0;
        if (uturn_around_ == utaround_clockwise) {
            uturn_around_ = utaround_clockwise2;

        } else if (uturn_around_ == utaround_anticlockwise) {
            uturn_around_ = utaround_anticlockwise2;

        } else {
            uturn_around_ = utaround_no;
        }
        ROS_INFO_NAMED("dwa_local_planner", "dist(%.5f) <> near_goal_tolerance(%.5f) => near_goal(%s), uturn_around_: %i next_narrow: %s", 
            dist_to_goal, near_goal_tolerance, near_goal_? "true": "false", uturn_around_, next_narrow? "true": "false");
        ros::rviz_breakpoint = false;
        tpose2d robot_pose2d(current_pose_.pose.position.x, current_pose_.pose.position.y, tf2::getYaw(current_pose_.pose.orientation));

        boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(costmap_ros_->getCostmap()->getMutex()));

        costmap_ros_->updateMap();

        costmap_2d::Costmap2D& costmap = *costmap_ros_->getCostmap();

        const double pressure_threshold = DEG2RAD(40); // ok: 30
        const std::vector<SDL_Point> FREE_SPACE_obs_cells = costmap_ros_->getCostmap()->getObsCells();
        resolve_pressure(true, robot_pose2d, FREE_SPACE_obs_cells, 0, pressure_threshold, pressure_, nullptr, nullptr, false);

        {
            double threshold = DEG2RAD(45);
            std::vector<SDL_Point> obs_cells;
            get_footprint_obs_cells(costmap, costmap_ros_->getUnpaddedRobotFootprint(), robot_pose2d, 0.05, 0.01, 0, obs_cells);
            obs_cells.insert(obs_cells.end(), FREE_SPACE_obs_cells.begin(), FREE_SPACE_obs_cells.end()); 
            resolve_pressure(true, robot_pose2d, obs_cells, 0, threshold, padded5cm_pressure_, nullptr, nullptr, false);

            threshold = DEG2RAD(90);
            get_footprint_obs_cells(costmap, costmap_ros_->getUnpaddedRobotFootprint(), robot_pose2d, 0.05, 0.10, 0.0, obs_cells);
            obs_cells.insert(obs_cells.end(), FREE_SPACE_obs_cells.begin(), FREE_SPACE_obs_cells.end()); 
            resolve_pressure(false, robot_pose2d, obs_cells, 0, threshold, front5cm_lr_pressure_, nullptr, nullptr, false);
        }

        SDL_Log("pressure_(front: %s, back: %s), padded5cm_pressure_(front: %s, back: %s), front5cm_lr_pressure_(left: %s, right: %s)",
            pressure_.front_has_obs? "true": "flase", pressure_.back_has_obs? "true": "false",
            padded5cm_pressure_.front_has_obs? "true": "flase", padded5cm_pressure_.back_has_obs? "true": "false",
            front5cm_lr_pressure_.front_has_obs? "true": "flase", front5cm_lr_pressure_.back_has_obs? "true": "false");

        global_full_plan_.clear();
        bool adjusted = adjust_global_plan(current_pose_, FREE_SPACE_obs_cells, dist_to_goal, next_narrow, transformed_plan);
        if (!adjusted) {
            double adjust_global_plan_tolerance = 0.4; // 0.4m
            if (dist_to_goal > adjust_global_plan_tolerance) {
                ROS_WARN_NAMED("dwa_local_planner", "adjust_global_plan fail and 'dist_to_goal(%.3f) > adjust_global_plan_tolerance(%.3f), do nothing", 
                    dist_to_goal, adjust_global_plan_tolerance);
                return false;
            } else {
                VALIDATE(!transformed_plan.empty(), null_str);
                ROS_WARN_NAMED("dwa_local_planner", "adjust_global_plan fail, but 'dist_to_goal(%.3f) <= adjust_global_plan_tolerance(%.3f), do nothing", 
                    dist_to_goal, adjust_global_plan_tolerance);
            }
        }
    }

    // update plan in dwa_planner even if we just stop and rotate, to allow checkTrajectory
    dp_->updatePlanAndLocalCosts(current_pose_, transformed_plan, costmap_ros_->getRobotFootprint());

    bool isOk = false;
    if (latchedStopRotateController_.isPositionReached(&planner_util_, current_pose_)) {
      //publish an empty plan because we've reached our goal position
      ROS_INFO_NAMED("dwa_local_planner", "Positiion is reached, try to rotate to goal's yaw.");
      std::vector<geometry_msgs::PoseStamped> local_plan;
      std::vector<geometry_msgs::PoseStamped> transformed_plan;
      publishGlobalPlan(transformed_plan);
      publishLocalPlan(local_plan);
      base_local_planner::LocalPlannerLimits limits = planner_util_.getCurrentLimits();
      
      isOk = false;
/*
      isOk = latchedStopRotateController_.computeVelocityCommandsStopRotate(
          cmd_vel,
          limits.getAccLimits(),
          dp_->getSimPeriod(),
          &planner_util_,
          odom_helper_,
          current_pose_,
          std::bind(&DWAPlanner::checkTrajectory, dp_, _1, _2, _3));
*/
      if (!isOk) {
          isOk = rotateToGoal(current_pose_, goal_pose2d, cmd_vel);
      }
    } else {
      isOk = dwaComputeVelocityCommands(current_pose_, cmd_vel);
      publishGlobalFullPlan(global_full_plan_);
      if (isOk) {
        publishGlobalPlan(transformed_plan);
      } else {
        ROS_WARN_NAMED("dwa_local_planner", "DWA planner failed to produce path.");
        std::vector<geometry_msgs::PoseStamped> empty_plan;
        // publishGlobalPlan(empty_plan);
        publishGlobalPlan(transformed_plan);
      }
    }
    if (isOk) {
        revise_twist_moveable(cmd_vel);
        ros::rose_slot.set_cmd_vel(cmd_vel, use_negative_vel_, latchedStopRotateController_.xy_tolerance_latch(), ros::yaw_goal_tolerance);

        if (ros::buildmap || near_goal_) {
            revise_twist_limit_max("revise buildmap velocity", ros::base_cfg.max_buildmap_vel_x, ros::base_cfg.max_buildmap_vel_theta, cmd_vel);
        } else {
            revise_twist_limit_max("revise navigation velocity", ros::base_cfg.max_navigation_vel_x, ros::base_cfg.max_navigation_vel_theta, cmd_vel);
        }
    }
    return isOk;
  }


};

namespace nav_core {
void warnRenamedParameter(const ros::NodeHandle& nh, const std::string current_name, const std::string old_name)
{
  if (nh.hasParam(old_name))
  {
    ROS_WARN("Parameter %s is deprecated (and will not load properly). Use %s instead.", old_name.c_str(), current_name.c_str());
  }
}
}
