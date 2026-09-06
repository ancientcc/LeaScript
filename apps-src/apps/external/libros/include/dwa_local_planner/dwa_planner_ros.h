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
#ifndef DWA_LOCAL_PLANNER_DWA_PLANNER_ROS_H_
#define DWA_LOCAL_PLANNER_DWA_PLANNER_ROS_H_

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <tf2_ros/buffer.h>

#include <dynamic_reconfigure/server.h>
#include <dwa_local_planner/DWAPlannerConfig.h>

#include <angles/angles.h>

#include <nav_msgs/Odometry.h>

#include <costmap_2d/costmap_2d_ros.h>
#include <nav_core/base_local_planner.h>
#include <base_local_planner/latched_stop_rotate_controller.h>

#include <base_local_planner/odometry_helper_ros.h>

#include <dwa_local_planner/dwa_planner.h>

namespace dwa_local_planner {
  /**
   * @class DWAPlannerROS
   * @brief ROS Wrapper for the DWAPlanner that adheres to the
   * BaseLocalPlanner interface and can be used as a plugin for move_base.
   */
  class DWAPlannerROS : public nav_core::BaseLocalPlanner {
    public:
      /**
       * @brief  Constructor for DWAPlannerROS wrapper
       */
      DWAPlannerROS(ros::CallbackQueue& cbqueue);

      /**
       * @brief  Constructs the ros wrapper
       * @param name The name to give this instance of the trajectory planner
       * @param tf A pointer to a transform listener
       * @param costmap The cost map to use for assigning costs to trajectories
       */
      void initialize(std::string name, tf2_ros::Buffer* tf,
          costmap_2d::Costmap2DROS* costmap_ros);

      /**
       * @brief  Destructor for the wrapper
       */
      ~DWAPlannerROS();

      /**
       * @brief  Given the current position, orientation, and velocity of the robot,
       * compute velocity commands to send to the base
       * @param cmd_vel Will be filled with the velocity command to be passed to the robot base
       * @return True if a valid trajectory was found, false otherwise
       */
      bool computeVelocityCommands(geometry_msgs::Twist& cmd_vel);


      /**
       * @brief  Given the current position, orientation, and velocity of the robot,
       * compute velocity commands to send to the base, using dynamic window approach
       * @param cmd_vel Will be filled with the velocity command to be passed to the robot base
       * @return True if a valid trajectory was found, false otherwise
       */
      bool dwaComputeVelocityCommands(const geometry_msgs::PoseStamped& global_pose, geometry_msgs::Twist& cmd_vel);

      /**
       * @brief  Set the plan that the controller is following
       * @param orig_global_plan The plan to pass to the controller
       * @return True if the plan was updated successfully, false otherwise
       */
      bool setPlan(const std::vector<geometry_msgs::PoseStamped>& orig_global_plan);

      /**
       * @brief  Check if the goal pose has been achieved
       * @return True if achieved, false otherwise
       */
      bool isGoalReached();



      bool isInitialized() const {
        return initialized_;
      }

      struct tpressure_result {
          tpressure_result()
          {
              clear();
          }
          void clear()
          {
              front_has_obs = false;
              back_has_obs = false;
          }

          bool front_has_obs;
          bool back_has_obs;
      };
      void resolve_pressure(bool front_back, const tpose2d& robot_pose2d, const std::vector<SDL_Point>& obs_cells, double min_threshold, double max_threshold, tpressure_result& result, 
          std::set<tpoint>* front_obs_cells, std::set<tpoint>* back_obs_cells, bool verbose);
      bool adjust_global_plan(const geometry_msgs::PoseStamped& global_pose, const std::vector<SDL_Point>& FREE_SPACE_obs_cells, double dist_to_goal, bool next_narrow, std::vector<geometry_msgs::PoseStamped>& transformed_plan);
      void adjust_traj_backward(const geometry_msgs::PoseStamped& global_pose, const std::vector<geometry_msgs::PoseStamped>& global_plan, bool front_pressure, base_local_planner::Trajectory& traj, geometry_msgs::Twist& cmd_vel);
      bool xy_tolerance_latch() const override;
      void reset_xy_tolerance_latch() override;
      void set_fresh_move() override { /*fresh_move_ = true;*/ }
      // bool get_footprint_obs_cells(const std::vector<geometry_msgs::Point>& footprint, const tpose2d& robot_pose, double padding_x, double padding_y, double yaw_increment, std::vector<SDL_Point>& obs_cells);
      void transform_footprint_pressure(const tpose2d& robot_pose, double padding_x, double padding_y, double yaw_increment, tpressure_result& result);
      bool a_star_search_move(const geometry_msgs::PoseStamped& planner_goal, geometry_msgs::Twist& cmd_vel) override;
      bool rotateToGoal(const geometry_msgs::PoseStamped& global_pose, const tpose2d& goal_pose, geometry_msgs::Twist& cmd_vel);
      // void setNarrowFootprint(double robot_width);
      const std::vector<geometry_msgs::Point>& getNarrowFootprint();
      bool is_narrow_point(const tpose2d& local_2_map, double x, double y, double yaw, std::set<tpoint>* front_obs_cells, std::set<tpoint>* back_obs_cells, bool verbose);
      bool calc_top_is_narrow(const tpose2d& local_2_map, const tpose2d& robot_pose);

    private:
      /**
       * @brief Callback to update the local planner's parameters based on dynamic reconfigure
       */
      void reconfigureCB(DWAPlannerConfig &config, uint32_t level);

      void publishLocalPlan(std::vector<geometry_msgs::PoseStamped>& path);

      void publishGlobalPlan(std::vector<geometry_msgs::PoseStamped>& path);

      void publishGlobalFullPlan(std::vector<geometry_msgs::PoseStamped>& path);

      double calc_robot_to_global_plan_yaw(const tpose2d& robot_pose2d, const std::vector<geometry_msgs::PoseStamped>& global_plan, double* angle_ptr);

      void save_costmap(const std::string& scene, const SDL_Point& src, const SDL_Point& dst, const std::vector<geometry_msgs::PoseStamped>& transformed_plan,
        const tpose2d& robot_pose, double inscribed_radius, const SDL_Rect& exclusion_rect);

  private:
      ros::CallbackQueue& cbqueue_;
      tf2_ros::Buffer* tf_; ///< @brief Used for transforming point clouds

      // for visualisation, publishers of global and local plan
      ros::Publisher g_plan_pub_, g_full_plan_pub_, l_plan_pub_;

      base_local_planner::LocalPlannerUtil planner_util_;

      boost::shared_ptr<DWAPlanner> dp_; ///< @brief The trajectory controller

      costmap_2d::Costmap2DROS* costmap_ros_;

      dynamic_reconfigure::Server<DWAPlannerConfig> *dsrv_;
      dwa_local_planner::DWAPlannerConfig default_config_;
      bool setup_;
      geometry_msgs::PoseStamped current_pose_;

      base_local_planner::LatchedStopRotateController latchedStopRotateController_;


      bool initialized_;


      base_local_planner::OdometryHelperRos odom_helper_;
      std::string odom_topic_;

      uint32_t computeVelocityCommands_id_;
      bool next_traj_backword_;
      bool near_goal_;
      bool use_negative_vel_;
      bool straight_ward_;
      bool maybe_uturn_;
      bool next_narrow_;
      bool fresh_move_;
      std::vector<geometry_msgs::PoseStamped> global_full_plan_;
      std::vector<geometry_msgs::Point> narrow_footprint_;

      tpressure_result pressure_;
      tpressure_result padded5cm_pressure_;
      tpressure_result front5cm_lr_pressure_;
      bool top_is_narrow_;
      double plan1_theta_;
      enum turn_around_t {utaround_clockwise2 = -2, utaround_clockwise = -1, utaround_no = 0, 
          utaround_anticlockwise = 1, utaround_anticlockwise2 = 2};
      turn_around_t uturn_around_;
  };
};
#endif
