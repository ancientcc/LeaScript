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
#include <dwa_local_planner/dwa_planner.h>
#include <base_local_planner/goal_functions.h>
#include <cmath>

//for computing path distance
#include <queue>

#include <angles/angles.h>

#include <ros/ros.h>
#include <tf2/utils.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud2_iterator.h>

#include <SDL_log.h>
#include "rose_global.hpp"
#include <rose_exception.hpp>
#include <rose_ros/utils.hpp>


namespace dwa_local_planner {
  void DWAPlanner::reconfigure(DWAPlannerConfig &config)
  {

    boost::mutex::scoped_lock l(configuration_mutex_);

    generator_.setParameters(
        config.sim_time,
        config.sim_granularity,
        config.angular_sim_granularity,
        config.use_dwa,
        sim_period_);

    double resolution = planner_util_->getCostmap()->getResolution();
    path_distance_bias_ = resolution * config.path_distance_bias;
    // pdistscale used for both path and alignment, set  forward_point_distance to zero to discard alignment
    {
        int ii = 0;
        // path_distance_bias_ = 3; // #1(1.7)
    }
    path_costs_.setScale(path_distance_bias_);
    alignment_costs_.setScale(path_distance_bias_);

    goal_distance_bias_ = resolution * config.goal_distance_bias;
    {
        int ii = 0;
        goal_distance_bias_ = 4; // #1(4)
    }
    goal_costs_.setScale(goal_distance_bias_);
    goal_front_costs_.setScale(goal_distance_bias_);

    occdist_scale_ = config.occdist_scale;
    obstacle_costs_.setScale(occdist_scale_);

    stop_time_buffer_ = config.stop_time_buffer;
    oscillation_costs_.setOscillationResetDist(config.oscillation_reset_dist, config.oscillation_reset_angle);
    forward_point_distance_ = config.forward_point_distance;
    goal_front_costs_.setXShift(forward_point_distance_);
    alignment_costs_.setXShift(forward_point_distance_);
 
    // obstacle costs can vary due to scaling footprint feature
    obstacle_costs_.setParams(config.max_vel_trans, config.max_scaling_factor, config.scaling_speed);

    twirling_costs_.setScale(config.twirling_scale);
    // rose_costs_.setScale(1)

    int vx_samp, vy_samp, vth_samp;
    vx_samp = config.vx_samples;
    vy_samp = config.vy_samples;
    vth_samp = config.vth_samples;
 
    if (vx_samp <= 0) {
      ROS_WARN("You've specified that you don't want any samples in the x dimension. We'll at least assume that you want to sample one value... so we're going to set vx_samples to 1 instead");
      vx_samp = 1;
      config.vx_samples = vx_samp;
    }
 
    if (vy_samp <= 0) {
      ROS_WARN("You've specified that you don't want any samples in the y dimension. We'll at least assume that you want to sample one value... so we're going to set vy_samples to 1 instead");
      vy_samp = 1;
      config.vy_samples = vy_samp;
    }
 
    if (vth_samp <= 0) {
      ROS_WARN("You've specified that you don't want any samples in the th dimension. We'll at least assume that you want to sample one value... so we're going to set vth_samples to 1 instead");
      vth_samp = 1;
      config.vth_samples = vth_samp;
    }
 
    vsamples_[0] = vx_samp;
    vsamples_[1] = vy_samp;
    vsamples_[2] = vth_samp;
 

  }

  DWAPlanner::DWAPlanner(ros::CallbackQueue& cbqueue, std::string name, base_local_planner::LocalPlannerUtil *planner_util)
      : cbqueue_(cbqueue),
      planner_util_(planner_util),
      map_viz_(cbqueue),
      obstacle_costs_(planner_util->getCostmap()),
      path_costs_(planner_util->getCostmap(), 0.0, 0.0, false, base_local_planner::CostAggregationType::Last, "path_costs"), // Sum
      goal_costs_(planner_util->getCostmap(), 0.0, 0.0, true, base_local_planner::CostAggregationType::Last, "goal_costs"),
      goal_front_costs_(planner_util->getCostmap(), 0.0, 0.0, true),
      alignment_costs_(planner_util->getCostmap())
  {
    ros::NodeHandle private_nh("~/" + name);
    private_nh.setCallbackQueue(&cbqueue);

    goal_front_costs_.setStopOnFailure( false );
    alignment_costs_.setStopOnFailure( false );

    //Assuming this planner is being run within the navigation stack, we can
    //just do an upward search for the frequency at which its being run. This
    //also allows the frequency to be overwritten locally.
    std::string controller_frequency_param_name;
    if(!private_nh.searchParam("controller_frequency", controller_frequency_param_name)) {
      sim_period_ = 0.05;
    } else {
      double controller_frequency = 0;
      private_nh.param(controller_frequency_param_name, controller_frequency, 20.0);
      if(controller_frequency > 0) {
        sim_period_ = 1.0 / controller_frequency;
      } else {
        ROS_WARN("A controller_frequency less than 0 has been set. Ignoring the parameter, assuming a rate of 20Hz");
        sim_period_ = 0.05;
      }
    }
    ROS_INFO("Sim period is set to %.2f", sim_period_);

    oscillation_costs_.resetOscillationFlags();

    bool sum_scores;
    private_nh.param("sum_scores", sum_scores, false);
    obstacle_costs_.setSumScores(sum_scores);


    private_nh.param("publish_cost_grid_pc", publish_cost_grid_pc_, false);
    map_viz_.initialize(name, planner_util->getGlobalFrame(), std::bind(&DWAPlanner::getCellCosts, this, _1, _2, _3, _4, _5, _6));

    traj_cloud_pub_ = private_nh.advertise<sensor_msgs::PointCloud2>("trajectory_cloud", 1);
    shift_traj_cloud_pub_ = private_nh.advertise<sensor_msgs::PointCloud2>("shift_trajectory_cloud", 1);
    private_nh.param("publish_traj_pc", publish_traj_pc_, false);
#ifdef _WIN32
        publish_traj_pc_ = true;
#endif

    // set up all the cost functions that will be applied in order
    // (any function returning negative values will abort scoring, so the order can improve performance)
    std::vector<base_local_planner::TrajectoryCostFunction*> critics;
    // critics.push_back(&oscillation_costs_); // discards oscillating motions (assisgns cost -1)
    critics.push_back(&obstacle_costs_); // discards trajectories that move into obstacles
    // critics.push_back(&goal_front_costs_); // prefers trajectories that make the nose go towards (local) nose goal
    // critics.push_back(&alignment_costs_); // prefers trajectories that keep the robot nose on nose path
    critics.push_back(&path_costs_); // prefers trajectories on global path
    critics.push_back(&goal_costs_); // prefers trajectories that go towards (local) goal, based on wave propagation
    // critics.push_back(&twirling_costs_); // optionally prefer trajectories that don't spin
    // critics.push_back(&rose_costs_);

    // trajectory generators
    std::vector<base_local_planner::TrajectorySampleGenerator*> generator_list;
    generator_list.push_back(&generator_);

    scored_sampling_planner_ = base_local_planner::SimpleScoredSamplingPlanner(generator_list, critics);

    private_nh.param("cheat_factor", cheat_factor_, 1.0);
  }

  // used for visualization only, total_costs are not really total costs
  bool DWAPlanner::getCellCosts(int cx, int cy, float &path_cost, float &goal_cost, float &occ_cost, float &total_cost) {

    path_cost = path_costs_.getCellCosts(cx, cy);
    goal_cost = goal_costs_.getCellCosts(cx, cy);
    occ_cost = planner_util_->getCostmap()->getCost(cx, cy);
    if (path_cost == path_costs_.obstacleCosts() ||
        path_cost == path_costs_.unreachableCellCosts() ||
        occ_cost >= costmap_2d::INSCRIBED_INFLATED_OBSTACLE) {
      return false;
    }

    total_cost =
        path_distance_bias_ * path_cost +
        goal_distance_bias_ * goal_cost +
        occdist_scale_ * occ_cost;
    return true;
  }

  bool DWAPlanner::setPlan(const std::vector<geometry_msgs::PoseStamped>& orig_global_plan) {
    oscillation_costs_.resetOscillationFlags();
    return planner_util_->setPlan(orig_global_plan);
  }

  /**
   * This function is used when other strategies are to be applied,
   * but the cost functions for obstacles are to be reused.
   */
  bool DWAPlanner::checkTrajectory(
      Eigen::Vector3f pos,
      Eigen::Vector3f vel,
      Eigen::Vector3f vel_samples){
    oscillation_costs_.resetOscillationFlags();
    base_local_planner::Trajectory traj;
    // geometry_msgs::PoseStamped goal_pose = global_plan_.back();
    // Eigen::Vector3f goal(goal_pose.pose.position.x, goal_pose.pose.position.y, tf2::getYaw(goal_pose.pose.orientation));
    base_local_planner::LocalPlannerLimits limits = planner_util_->getCurrentLimits();
    Eigen::Vector3f vsamples(0, 0, 0);
    generator_.initialise(pos,
        vel,
        &limits,
        vsamples); // vsamples_
    generator_.generateTrajectory(pos, vel, vel_samples, traj);
    double cost = scored_sampling_planner_.scoreTrajectory(traj, -1);
    //if the trajectory is a legal one... the check passes
    if(cost >= 0) {
      return true;
    }
    ROS_WARN("Invalid Trajectory %.3f, %.3f, %.3f(deg:%.3f), cost: %.3f", vel_samples[0], vel_samples[1], vel_samples[2], RAD2DEG(vel_samples[2]), cost);

    //otherwise the check fails
    return false;
  }


  void DWAPlanner::updatePlanAndLocalCosts(
      const geometry_msgs::PoseStamped& global_pose,
      const std::vector<geometry_msgs::PoseStamped>& new_plan,
      const std::vector<geometry_msgs::Point>& footprint_spec) {
    global_plan_.resize(new_plan.size());
    for (unsigned int i = 0; i < new_plan.size(); ++i) {
      global_plan_[i] = new_plan[i];
    }

    obstacle_costs_.setFootprint(footprint_spec);

    // costs for going away from path
    path_costs_.setTargetPoses(global_plan_);

    // costs for not going towards the local goal as much as possible
    goal_costs_.setTargetPoses(global_plan_);

    // alignment costs
    geometry_msgs::PoseStamped goal_pose = global_plan_.back();

    Eigen::Vector3f pos(global_pose.pose.position.x, global_pose.pose.position.y, tf2::getYaw(global_pose.pose.orientation));
    double sq_dist =
        (pos[0] - goal_pose.pose.position.x) * (pos[0] - goal_pose.pose.position.x) +
        (pos[1] - goal_pose.pose.position.y) * (pos[1] - goal_pose.pose.position.y);

    // we want the robot nose to be drawn to its final position
    // (before robot turns towards goal orientation), not the end of the
    // path for the robot center. Choosing the final position after
    // turning towards goal orientation causes instability when the
    // robot needs to make a 180 degree turn at the end
    std::vector<geometry_msgs::PoseStamped> front_global_plan = global_plan_;
    double angle_to_goal = atan2(goal_pose.pose.position.y - pos[1], goal_pose.pose.position.x - pos[0]);
    front_global_plan.back().pose.position.x = front_global_plan.back().pose.position.x +
      forward_point_distance_ * cos(angle_to_goal);
    front_global_plan.back().pose.position.y = front_global_plan.back().pose.position.y + forward_point_distance_ *
      sin(angle_to_goal);

    goal_front_costs_.setTargetPoses(front_global_plan);
    
    // keeping the nose on the path
    if (sq_dist > forward_point_distance_ * forward_point_distance_ * cheat_factor_) {
      alignment_costs_.setScale(path_distance_bias_);
      // costs for robot being aligned with path (nose on path, not ju
      alignment_costs_.setTargetPoses(global_plan_);
    } else {
      // once we are close to goal, trying to keep the nose close to anything destabilizes behavior.
      alignment_costs_.setScale(0.0);
    }

    // rose_costs_.setOneMisc(global_pose, new_plan, planner_util_->getCurrentLimits(), near_goal);
  }

static std::string trajectory_cost_2_str(const base_local_planner::Trajectory& t)
{
    char buf[128];
    SDL_snprintf(buf, sizeof(buf), "[(%.5f)%.5f + %.5f + %.5f]",
        t.cost_, t.verbose_costs[0], t.verbose_costs[1], t.verbose_costs[2]);

    return buf;
}


  /*
   * given the current state of the robot, find a good trajectory
   */
  base_local_planner::Trajectory DWAPlanner::findBestPath(
      const geometry_msgs::PoseStamped& global_pose,
      const geometry_msgs::PoseStamped& global_vel,
      geometry_msgs::Twist& cmd_vel, 
      bool use_negative_vel, bool straight_ward, bool maybe_uturn) {

    //make sure that our configuration doesn't change mid-run
    boost::mutex::scoped_lock l(configuration_mutex_);

    Eigen::Vector3f pos(global_pose.pose.position.x, global_pose.pose.position.y, tf2::getYaw(global_pose.pose.orientation));
    Eigen::Vector3f vel(global_vel.pose.position.x, global_vel.pose.position.y, tf2::getYaw(global_vel.pose.orientation));
    geometry_msgs::PoseStamped goal_pose = global_plan_.back();
    Eigen::Vector3f goal(goal_pose.pose.position.x, goal_pose.pose.position.y, tf2::getYaw(goal_pose.pose.orientation));
    base_local_planner::LocalPlannerLimits limits = planner_util_->getCurrentLimits();
    Eigen::Vector3f vsamples = vsamples_;
    if (use_negative_vel) {
        limits.min_vel_x = -ros::base_cfg.min_moveable_vel_x;  // -0.08
        limits.max_vel_x = -0.02;
        if (straight_ward) {
            limits.min_vel_x = -0.12;
            limits.max_vel_theta = ros::base_cfg.min_moveable_vel_theta;
        } else {
            limits.max_vel_theta = ros::base_cfg.min_moveable_vel_theta;
        }

    } else {
        if (maybe_uturn) {
            vsamples[0] = 1;
            limits.min_vel_x = 0.00; // 0.02
            limits.max_vel_x = 0.00; // 0.02
            limits.min_vel_trans = 0.03; // 0.52333

            limits.min_vel_theta = DEG2RAD(30); // DEG2RAD(30)[0.52333]  DEG2RAD(35)[0.6109]
            // limits.max_vel_theta = DEG2RAD(35); // 0.6109

        } else {
            // must use Positive velocity
            limits.min_vel_x = 0.02; // 
            if (straight_ward) {
                limits.max_vel_theta = ros::base_cfg.min_moveable_vel_theta;
            }
        }
    }

    // prepare cost functions and generators for this run
    generator_.initialise(pos,
        vel,
        &limits,
        vsamples);

    result_traj_.cost_ = -7;
    // find best trajectory by sampling and scoring the samples
    std::vector<base_local_planner::Trajectory> all_explored;
    scored_sampling_planner_.findBestTrajectory(result_traj_, &all_explored);

    if (game_config::os == os_windows && publish_traj_pc_) {
        sensor_msgs::PointCloud2 traj_cloud;
        traj_cloud.header.frame_id = global_pose.header.frame_id; // frame_id_
        traj_cloud.header.stamp = ros::Time::now();

        sensor_msgs::PointCloud2Modifier cloud_mod(traj_cloud);

        cloud_mod.setPointCloud2Fields(5, "x", 1, sensor_msgs::PointField::FLOAT32,
                                          "y", 1, sensor_msgs::PointField::FLOAT32,
                                          "z", 1, sensor_msgs::PointField::FLOAT32,
                                          "theta", 1, sensor_msgs::PointField::FLOAT32,
                                          "cost", 1, sensor_msgs::PointField::FLOAT32);

        unsigned int num_points = 0;
        struct tcost {
            double cost;
            double costs[10];
        };
        std::vector<tcost> verbose_costs;
        bool exclude_fail_cost = false;
        const base_local_planner::Trajectory* verbose_traj = nullptr;
        for(std::vector<base_local_planner::Trajectory>::iterator t=all_explored.begin(); t != all_explored.end(); ++t)
        {
            verbose_costs.push_back(tcost());
            tcost& cost2 = verbose_costs.back();
            cost2.cost = t->cost_;
            for (int n = 0; n < 10; n ++) {
                cost2.costs[n] = t->verbose_costs[n];
            }
            if (exclude_fail_cost && t->cost_<0) {
              continue;
            }

            if (t->xv_ == result_traj_.xv_ && t->thetav_ == result_traj_.thetav_) {
            } else if (verbose_traj == nullptr && (t->cost_ >= 0 && t->xv_ > 0.22)) {
                verbose_traj = &*t;
            } else {
                // continue;
                int ii = 0;
            }

            num_points += t->getPointsSize();
        }

        cloud_mod.resize(num_points);
        sensor_msgs::PointCloud2Iterator<float> iter_x(traj_cloud, "x");
        int at = 0;

        sensor_msgs::PointCloud2 shift_traj_cloud = traj_cloud;
        sensor_msgs::PointCloud2Iterator<float> shift_iter_x(shift_traj_cloud, "x");

        for(std::vector<base_local_planner::Trajectory>::iterator t=all_explored.begin(); t != all_explored.end(); ++t, at ++)
        {
            // bool verbose = at == 0 || t->cost_ == result_traj_.cost_;
            // bool verbose = at == 0;
            // bool verbose = t->xv_ > 0 && t->thetav_ < 0;
            bool verbose = true;
            if(exclude_fail_cost && t->cost_<0) {
                continue;
            }

            // if ((t->xv_ == 0.09078 && t->thetav_ == -0.05263) || (t->xv_ == 0.25 && t->thetav_ == 1.0)) {
            if (t->xv_ == result_traj_.xv_ && t->thetav_ == result_traj_.thetav_) {
            } else if (&*t == verbose_traj) {
            } else {
                // continue;
                int ii = 0;
            }
            // Fill out the plan
            if (verbose && t->cost_ >= 0) {
            // if (verbose) {
                const costmap_2d::Costmap2D& costmap = *planner_util_->getCostmap();
                unsigned int start_cell_x = 0, start_cell_y = 0;
                costmap.worldToMap(global_plan_.back().pose.position.x, global_plan_.back().pose.position.y,
                    start_cell_x, start_cell_y);

                unsigned int last_cell_x = 0, last_cell_y = 0;
                const int points = t->getPointsSize();
                double end_x = 0;
                double end_y = 0;
                if (points > 0) {
                    double end_th;
                    t->getPoint(points - 1, end_x, end_y, end_th);
                    costmap.worldToMap(end_x, end_y, last_cell_x, last_cell_y);
                }

                // SDL_Log("[%i/%i]Trajectory, vel(%.5f, %.5f, %.5f), points:%i, cost: %s, best_cost: %.5f, start_cell(%i, %i) -> last_cell(%i, %i)(%.5f, %.5f), orign_(%.5f, %.5f)", 
                //     at, (int)all_explored.size(), t->xv_, t->yv_, t->thetav_, (int)t->getPointsSize(), 
                //    trajectory_cost_2_str(*t).c_str(), result_traj_.cost_, (int)start_cell_x, (int)start_cell_y, (int)last_cell_x, (int)last_cell_y, end_x, end_y, costmap.getOriginX(), costmap.getOriginY());

            }

            for(unsigned int i = 0; i < t->getPointsSize() && verbose; ++i) {
                double p_x, p_y, p_th;
                t->getPoint(i, p_x, p_y, p_th);
                iter_x[0] = p_x;
                iter_x[1] = p_y;
                iter_x[2] = 0.0;
                iter_x[3] = p_th;
                iter_x[4] = t->cost_;

                ++iter_x;

                shift_iter_x[0] = t->verbose_shiftx[i];
                shift_iter_x[1] = t->verbose_shifty[i];
                shift_iter_x[2] = 0.0;
                shift_iter_x[3] = p_th;
                shift_iter_x[4] = t->cost_;

                ++ shift_iter_x;

                if (verbose) {
                    // SDL_Log("[%u/%u](%0.5f, %.5f), p_th: %.5f, vel(%.5f, %.5f, %.5f), cost: %.5f, best_cost: %.5f", 
                    //    i, t->getPointsSize(), p_x, p_y, p_th, t->xv_, t->yv_, t->thetav_, t->cost_, result_traj_.cost_);
                }
            }
        }

        traj_cloud_pub_.publish(traj_cloud);
        shift_traj_cloud_pub_.publish(shift_traj_cloud);
    }

    if (result_traj_.cost_ >= 0) {
        double start_x;
        double start_y;
        double start_th;
        double end_x;
        double end_y;
        double end_th;

        result_traj_.getPoint(0, start_x, start_y, start_th);
        result_traj_.getPoint(result_traj_.getPointsSize() - 1, end_x, end_y, end_th);
        double dist = sqrt((start_x - end_x) * (start_x - end_x) + ((start_y - end_y)) * (start_y - end_y));
        ROS_INFO("findBestPath, vel(%.5f, %.5f, %.5f), (%.5f, %.5f)-->(%.5f, %.5f) => dist: %.5f", 
            result_traj_.xv_, result_traj_.yv_, result_traj_.thetav_, start_x, start_y, end_x, end_y, dist);
        if (dist <= 0.09) {
            int ii = 0;
        }

    } else {
        ROS_INFO("findBestPath fail, result_traj_.cost_: %.3f", result_traj_.cost_); 
    }

    // verbose publishing of point clouds
    if (publish_cost_grid_pc_) {
      //we'll publish the visualization of the costs to rviz before returning our best trajectory
      map_viz_.publishCostCloud(planner_util_->getCostmap());
    }

    // debrief stateful scoring functions
    oscillation_costs_.updateOscillationFlags(pos, &result_traj_, planner_util_->getCurrentLimits().min_vel_trans);

    //if we don't have a legal trajectory, we'll just command zero
    if (result_traj_.cost_ < 0) {
      cmd_vel.linear.x = 0;
      cmd_vel.linear.y = 0;
      cmd_vel.linear.z = 0;
      cmd_vel.angular.z = 0;
    } else {
      cmd_vel.linear.x = result_traj_.xv_;
      cmd_vel.linear.y = result_traj_.yv_;
      cmd_vel.angular.z = result_traj_.thetav_;

      // tf2::Quaternion q;
      // q.setRPY(0, 0, result_traj_.thetav_);
      // tf2::convert(q, drive_velocities.pose.orientation);
    }

    return result_traj_;
  }
};
