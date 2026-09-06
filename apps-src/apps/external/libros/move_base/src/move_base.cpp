/*********************************************************************
*
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
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
*   * Neither the name of the Willow Garage nor the names of its
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
*         Mike Phillips (put the planner in its own thread)
*********************************************************************/
#include <move_base/move_base.h>
#include <move_base_msgs/RecoveryStatus.h>
#include <cmath>

#include <boost/algorithm/string.hpp>
// #include <boost/thread.hpp>

#include <geometry_msgs/Twist.h>

#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

#include <global_planner/planner_core.h>
#include <dwa_local_planner/dwa_planner_ros.h>
#include <clear_costmap_recovery/clear_costmap_recovery.h>
#include <rotate_recovery/rotate_recovery.h>
#include <rose_ros/rose_recovery.h>
#include "rose_exception.hpp"
#include <SDL_timer.h>
#include <rose_ros/utils.hpp>
#include <rose_ros/kidnap.hpp>

#include <rose_ros/aplt.hpp>

bool sendGoaling = false;

#define DEFAULT_OSCILLATION_TIMOUT      10.0
#define XYREACHED_OSCILLATION_TIMOUT    15.0

bool makePlanFail = false;
bool makPlan_getRobotPose0 = false;

bool get_makPlan_getRobotPose0()
{
    return makPlan_getRobotPose0;
}

namespace move_base {
  char MoveBaseStates[][20] = { "PLANNING", "CONTROLLING", "CLEARING"};

  MoveBase::MoveBase(ros::CallbackQueue& cbqueue, tf2_ros::Buffer& tf, bool& exit)
    : cbqueue_(cbqueue)
    , tf_(tf)
    , as_(NULL)
    , planner_costmap_ros_(NULL)
    , controller_costmap_ros_(NULL)
    , just_clearing_(false)
    , planner_plan_(NULL)
    , latest_plan_(NULL)
    , controller_plan_(NULL)
    , runPlanner_(false)
    , planner_thread_(nullptr)
    , dsrv_(nullptr)
    , setup_(false)
    , p_freq_change_(false)
    , c_freq_change_(false)
    , new_global_plan_(false)
    , exit_(exit)
  {
    makPlan_getRobotPose0 = !ros::buildmap;

    ros::NodeHandle server_nh;
    server_nh.setCallbackQueue(&cbqueue);
    as_ = new MoveBaseActionServer(server_nh/*ros::NodeHandle()*/, "move_base", std::bind(&MoveBase::executeCb, this, _1), false);

    ros::NodeHandle private_nh("~");
    ros::NodeHandle nh;
    private_nh.setCallbackQueue(&cbqueue);
    nh.setCallbackQueue(&cbqueue);

    recovery_trigger_ = PLANNING_R;

    {
      // <clbrobot>/param/navigation/move_base_params.yaml
      private_nh.setParam("base_global_planner", "global_planner/GlobalPlanner");
      private_nh.setParam("base_local_planner", "dwa_local_planner/DWAPlannerROS");

      private_nh.setParam("shutdown_costmaps", false);

      private_nh.setParam("controller_frequency", 5.0); // before 5.0
      private_nh.setParam("controller_patience", 3.0);

      private_nh.setParam("planner_frequency", 0.5);
      private_nh.setParam("planner_patience", 3.0); // 5.0

      private_nh.setParam("oscillation_timeout", DEFAULT_OSCILLATION_TIMOUT);
      private_nh.setParam("oscillation_distance", 0.2);

      private_nh.setParam("conservative_reset_dist", 0.1); // distance from an obstacle at which it will unstuck itself

      private_nh.setParam("cost_factor", 1.0);
      private_nh.setParam("neutral_cost", 55);
      private_nh.setParam("lethal_cost", 253);
      int ii = 0;
    }

    //get some parameters that will be global to the move base node
    std::string global_planner, local_planner;
    private_nh.param("base_global_planner", global_planner, std::string("navfn/NavfnROS"));
    private_nh.param("base_local_planner", local_planner, std::string("base_local_planner/TrajectoryPlannerROS"));
    private_nh.param("global_costmap/robot_base_frame", robot_base_frame_, std::string("base_link"));
    // "global_costmap/robot_base_frame" set by Costmap2DROS::Costmap2DROS. it not execute by far.
    robot_base_frame_ = "base_footprint";
    private_nh.param("global_costmap/global_frame", global_frame_, std::string("map"));
    planner_frequency_ = 0.0;

    private_nh.param("controller_frequency", controller_frequency_, 20.0);
    planner_patience_ = 2.0; // ==> param("planner_patience"), (3.0) 3600 is one hour, think disabled by default
    private_nh.param("controller_patience", controller_patience_, 15.0); 
    max_planning_retries_ = -1; // ==> param("max_planning_retries"), if want disabled, use -1

    private_nh.param("oscillation_timeout", oscillation_timeout_, 0.0);
    private_nh.param("oscillation_distance", oscillation_distance_, 0.5);

    // parameters of make_plan service
    private_nh.param("make_plan_clear_costmap", make_plan_clear_costmap_, true);
    private_nh.param("make_plan_add_unreachable_goal", make_plan_add_unreachable_goal_, true);

    //set up plan triple buffer
    planner_plan_ = new std::vector<geometry_msgs::PoseStamped>();
    latest_plan_ = new std::vector<geometry_msgs::PoseStamped>();
    controller_plan_ = new std::vector<geometry_msgs::PoseStamped>();

    //set up the planner's thread
    // planner_thread_ = new boost::thread(std::bind(&MoveBase::planThread, this));
    planner_thread_ = create_rose_thread(std::bind(&MoveBase::planThread, this, _1), NULL, NULL, std::bind(&MoveBase::OnTriggerExit, this), "MoveBase_planThread");

    //for commanding the base
    vel_pub_ = nh.advertise<geometry_msgs::Twist>("cmd_vel", 1);
    current_goal_pub_ = private_nh.advertise<geometry_msgs::PoseStamped>("current_goal", 0 );

    ros::NodeHandle action_nh("move_base");
    action_nh.setCallbackQueue(&cbqueue);
    action_goal_pub_ = action_nh.advertise<move_base_msgs::MoveBaseActionGoal>("goal", 1);
    recovery_status_pub_= action_nh.advertise<move_base_msgs::RecoveryStatus>("recovery_status", 1);

    //we'll provide a mechanism for some people to send goals as PoseStamped messages over a topic
    //they won't get any useful information back about its status, but this is useful for tools
    //like nav_view and rviz
    ros::NodeHandle simple_nh("move_base_simple");
    simple_nh.setCallbackQueue(&cbqueue);
    goal_sub_ = simple_nh.subscribe<geometry_msgs::PoseStamped>("goal", 1, std::bind(&MoveBase::goalCB, this, _1));

    //we'll assume the radius of the robot to be consistent with what's specified for the costmaps
    private_nh.param("local_costmap/inscribed_radius", inscribed_radius_, 0.325);
    private_nh.param("local_costmap/circumscribed_radius", circumscribed_radius_, 0.46);
    private_nh.param("clearing_radius", clearing_radius_, circumscribed_radius_);
    private_nh.param("conservative_reset_dist", conservative_reset_dist_, 3.0);

    private_nh.param("shutdown_costmaps", shutdown_costmaps_, false);
    private_nh.param("clearing_rotation_allowed", clearing_rotation_allowed_, true);
    private_nh.param("recovery_behavior_enabled", recovery_behavior_enabled_, true);

    VALIDATE(planner_frequency_ == 0, null_str);
    VALIDATE(robot_base_frame_ == "base_footprint", null_str);

    //create the ros wrapper for the planner's costmap... and initializer a pointer we'll use with the underlying map
    planner_costmap_ros_ = new costmap_2d::Costmap2DROS(cbqueue, "global_costmap", tf_, exit_);
    if (exit_) {
      return;
    }
    planner_costmap_ros_->pause();

    //initialize the global planner
    // try {
      // planner_ = bgp_loader_.createInstance(global_planner);
      // ROS_INFO("Created global_planner %s, getName: %s", global_planner.c_str(), planner_.getName(global_planner));
      // ==>Created global_planner global_planner/GlobalPlanner, getName GlobalPlanner
      // planner_->initialize(bgp_loader_.getName(global_planner), planner_costmap_ros_);

      // why use GlobalPlanner, not NavfnROS, reference to https://www.cnblogs.com/flyinggod/p/9079500.html
      // global_planner/GlobalPlanner
      planner_.reset(new global_planner::GlobalPlanner(cbqueue));
      planner_->initialize("GlobalPlanner", planner_costmap_ros_);
    // } catch (const pluginlib::PluginlibException& ex) {
    //  ROS_FATAL("Failed to create the %s planner, are you sure it is properly registered and that the containing library is built? Exception: %s", global_planner.c_str(), ex.what());
    //  exit(1);
    // }

    //create the ros wrapper for the controller's costmap... and initializer a pointer we'll use with the underlying map
    controller_costmap_ros_ = new costmap_2d::Costmap2DROS(cbqueue, "local_costmap", tf_, exit_);
    if (exit_) {
      return;
    }
    controller_costmap_ros_->pause();

    //create a local planner
    // try {
      // tc_ = blp_loader_.createInstance(local_planner);
      // ROS_INFO("Created local_planner %s, getName: %s", local_planner.c_str(), blp_loader_.getName(local_planner));
      // ==>Created local_planner dwa_local_planner/DWAPlannerROS, getName DWAPlannerROS
      // tc_->initialize(blp_loader_.getName(local_planner), &tf_, controller_costmap_ros_);

      tc_.reset(new dwa_local_planner::DWAPlannerROS(cbqueue));
      tc_->initialize("DWAPlannerROS", &tf_, controller_costmap_ros_);

    // } catch (const pluginlib::PluginlibException& ex) {
    //  ROS_FATAL("Failed to create the %s planner, are you sure it is properly registered and that the containing library is built? Exception: %s", local_planner.c_str(), ex.what());
    //  exit(1);
    // }

    // Start actively updating costmaps based on sensor data
    planner_costmap_ros_->start(exit_);
    controller_costmap_ros_->start(exit_);

    //advertise a service for getting a plan
    make_plan_srv_ = private_nh.advertiseService("make_plan", &MoveBase::planService, this);

    //advertise a service for clearing the costmaps
    clear_costmaps_srv_ = private_nh.advertiseService("clear_costmaps", &MoveBase::clearCostmapsService, this);

    //if we shutdown our costmaps when we're deactivated... we'll do that now
    if(shutdown_costmaps_){
      ROS_DEBUG_NAMED("move_base","Stopping costmaps initially");
      planner_costmap_ros_->stop();
      controller_costmap_ros_->stop();
    }

    //load any user specified recovery behaviors, and if that fails load the defaults
    if(!loadRecoveryBehaviors(private_nh)){
      loadDefaultRecoveryBehaviors();
    }

    //initially, we'll need to make a plan
    state_ = PLANNING;

    //we'll start executing recovery behaviors at the beginning of our list
    recovery_index_ = 0;

    //we're all set up now so we can start the action server
    as_->start();

    ros::NodeHandle tmp_nh("~");
    tmp_nh.setCallbackQueue(&cbqueue);
    dsrv_ = new dynamic_reconfigure::Server<move_base::MoveBaseConfig>(tmp_nh);
    dynamic_reconfigure::Server<move_base::MoveBaseConfig>::CallbackType cb = std::bind(&MoveBase::reconfigureCB, this, _1, _2);
    dsrv_->setCallback(cb);
  }

  void MoveBase::reconfigureCB(move_base::MoveBaseConfig &config, uint32_t level){
    VALIDATE(!setup_, null_str);
    boost::recursive_mutex::scoped_lock l(configuration_mutex_);

    //The first time we're called, we just want to make sure we have the
    //original configuration
    if(!setup_)
    {
      last_config_ = config;
      default_config_ = config;
      setup_ = true;
      return;
    }

    VALIDATE(false, null_str);
    if(config.restore_defaults) {
      config = default_config_;
      //if someone sets restore defaults on the parameter server, prevent looping
      config.restore_defaults = false;
    }

    if(planner_frequency_ != config.planner_frequency)
    {
      planner_frequency_ = config.planner_frequency;
      p_freq_change_ = true;
    }

    if(controller_frequency_ != config.controller_frequency)
    {
      controller_frequency_ = config.controller_frequency;
      c_freq_change_ = true;
    }

    planner_patience_ = config.planner_patience;
    controller_patience_ = config.controller_patience;
    max_planning_retries_ = config.max_planning_retries;
    conservative_reset_dist_ = config.conservative_reset_dist;

    recovery_behavior_enabled_ = config.recovery_behavior_enabled;
    clearing_rotation_allowed_ = config.clearing_rotation_allowed;
    shutdown_costmaps_ = config.shutdown_costmaps;

    oscillation_timeout_ = config.oscillation_timeout;
    oscillation_distance_ = config.oscillation_distance;
    if(config.base_global_planner != last_config_.base_global_planner) {
      boost::shared_ptr<nav_core::BaseGlobalPlanner> old_planner = planner_;
      //initialize the global planner
      ROS_INFO("Loading global planner %s", config.base_global_planner.c_str());
      // try {
        // There is no occasion to execute here, and so on to implement if need. 
        VALIDATE(false, null_str);
        // planner_ = bgp_loader_.createInstance(config.base_global_planner);

        // wait for the current planner to finish planning
        boost::unique_lock<boost::recursive_mutex> lock(planner_mutex_);

        // Clean up before initializing the new planner
        planner_plan_->clear();
        latest_plan_->clear();
        controller_plan_->clear();
        resetState();
        // planner_->initialize(bgp_loader_.getName(config.base_global_planner), planner_costmap_ros_);

        lock.unlock();
      // } catch (const pluginlib::PluginlibException& ex) {
      //  ROS_FATAL("Failed to create the %s planner, are you sure it is properly registered and that the \
      //             containing library is built? Exception: %s", config.base_global_planner.c_str(), ex.what());
      //  planner_ = old_planner;
      //  config.base_global_planner = last_config_.base_global_planner;
      // }
    }

    if(config.base_local_planner != last_config_.base_local_planner){
      boost::shared_ptr<nav_core::BaseLocalPlanner> old_planner = tc_;
      //create a local planner
      // try {
        // There is no occasion to execute here, and so on to implement if need. 
        VALIDATE(false, null_str);
        // tc_ = blp_loader_.createInstance(config.base_local_planner);
        // Clean up before initializing the new planner
        planner_plan_->clear();
        latest_plan_->clear();
        controller_plan_->clear();
        resetState();
        // tc_->initialize(blp_loader_.getName(config.base_local_planner), &tf_, controller_costmap_ros_);
      // } catch (const pluginlib::PluginlibException& ex) {
      //  ROS_FATAL("Failed to create the %s planner, are you sure it is properly registered and that the \
      //             containing library is built? Exception: %s", config.base_local_planner.c_str(), ex.what());
      //  tc_ = old_planner;
      //  config.base_local_planner = last_config_.base_local_planner;
      // }
    }

    make_plan_clear_costmap_ = config.make_plan_clear_costmap;
    make_plan_add_unreachable_goal_ = config.make_plan_add_unreachable_goal;

    last_config_ = config;
  }

  void MoveBase::goalCB(const geometry_msgs::PoseStamped::ConstPtr& goal){
    ROS_DEBUG_NAMED("move_base","In ROS goal callback, wrapping the PoseStamped in the action message and re-sending to the server.");
    move_base_msgs::MoveBaseActionGoal action_goal;
    action_goal.header.stamp = ros::Time::now();
    action_goal.goal.target_pose = *goal;

    action_goal_pub_.publish(action_goal);
  }

  void MoveBase::clearCostmapWindows(double size_x, double size_y){
    geometry_msgs::PoseStamped global_pose;

    //clear the planner's costmap
    getRobotPose(global_pose, planner_costmap_ros_);

    std::vector<geometry_msgs::Point> clear_poly;
    double x = global_pose.pose.position.x;
    double y = global_pose.pose.position.y;
    geometry_msgs::Point pt;

    pt.x = x - size_x / 2;
    pt.y = y - size_y / 2;
    clear_poly.push_back(pt);

    pt.x = x + size_x / 2;
    pt.y = y - size_y / 2;
    clear_poly.push_back(pt);

    pt.x = x + size_x / 2;
    pt.y = y + size_y / 2;
    clear_poly.push_back(pt);

    pt.x = x - size_x / 2;
    pt.y = y + size_y / 2;
    clear_poly.push_back(pt);

    planner_costmap_ros_->getCostmap()->setConvexPolygonCost(clear_poly, costmap_2d::FREE_SPACE, false);

    //clear the controller's costmap
    getRobotPose(global_pose, controller_costmap_ros_);

    clear_poly.clear();
    x = global_pose.pose.position.x;
    y = global_pose.pose.position.y;

    pt.x = x - size_x / 2;
    pt.y = y - size_y / 2;
    clear_poly.push_back(pt);

    pt.x = x + size_x / 2;
    pt.y = y - size_y / 2;
    clear_poly.push_back(pt);

    pt.x = x + size_x / 2;
    pt.y = y + size_y / 2;
    clear_poly.push_back(pt);

    pt.x = x - size_x / 2;
    pt.y = y + size_y / 2;
    clear_poly.push_back(pt);

    controller_costmap_ros_->getCostmap()->setConvexPolygonCost(clear_poly, costmap_2d::FREE_SPACE, false);
  }

  bool MoveBase::clearCostmapsService(std_srvs::Empty::Request &req, std_srvs::Empty::Response &resp){
    //clear the costmaps
    boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock_controller(*(controller_costmap_ros_->getCostmap()->getMutex()));
    controller_costmap_ros_->clearCostmaps();

    boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock_planner(*(planner_costmap_ros_->getCostmap()->getMutex()));
    planner_costmap_ros_->clearCostmaps();
    return true;
  }


  bool MoveBase::planService(nav_msgs::GetPlan::Request &req, nav_msgs::GetPlan::Response &resp){
    if(as_->isActive()){
      ROS_ERROR("move_base must be in an inactive state to make a plan for an external user");
      return false;
    }
    //make sure we have a costmap for our planner
    if(planner_costmap_ros_ == NULL){
      ROS_ERROR("move_base cannot make a plan for you because it doesn't have a costmap");
      return false;
    }

    geometry_msgs::PoseStamped start;
    //if the user does not specify a start pose, identified by an empty frame id, then use the robot's pose
    if(req.start.header.frame_id.empty())
    {
        geometry_msgs::PoseStamped global_pose;
        if(!getRobotPose(global_pose, planner_costmap_ros_)){
          ROS_ERROR("move_base cannot make a plan for you because it could not get the start pose of the robot");
          return false;
        }
        start = global_pose;
    }
    else
    {
        start = req.start;
    }

    if (make_plan_clear_costmap_) {
      //update the copy of the costmap the planner uses
      clearCostmapWindows(2 * clearing_radius_, 2 * clearing_radius_);
    }

    //first try to make a plan to the exact desired goal
    std::vector<geometry_msgs::PoseStamped> global_plan;
    if(!planner_->makePlan(start, req.goal, global_plan) || global_plan.empty()){
      ROS_DEBUG_NAMED("move_base","Failed to find a plan to exact goal of (%.2f, %.2f), searching for a feasible goal within tolerance",
          req.goal.pose.position.x, req.goal.pose.position.y);

      //search outwards for a feasible goal within the specified tolerance
      geometry_msgs::PoseStamped p;
      p = req.goal;
      bool found_legal = false;
      float resolution = planner_costmap_ros_->getCostmap()->getResolution();
      float search_increment = resolution*3.0;
      if(req.tolerance > 0.0 && req.tolerance < search_increment) search_increment = req.tolerance;
      for(float max_offset = search_increment; max_offset <= req.tolerance && !found_legal; max_offset += search_increment) {
        for(float y_offset = 0; y_offset <= max_offset && !found_legal; y_offset += search_increment) {
          for(float x_offset = 0; x_offset <= max_offset && !found_legal; x_offset += search_increment) {

            //don't search again inside the current outer layer
            if(x_offset < max_offset-1e-9 && y_offset < max_offset-1e-9) continue;

            //search to both sides of the desired goal
            for(float y_mult = -1.0; y_mult <= 1.0 + 1e-9 && !found_legal; y_mult += 2.0) {

              //if one of the offsets is 0, -1*0 is still 0 (so get rid of one of the two)
              if(y_offset < 1e-9 && y_mult < -1.0 + 1e-9) continue;

              for(float x_mult = -1.0; x_mult <= 1.0 + 1e-9 && !found_legal; x_mult += 2.0) {
                if(x_offset < 1e-9 && x_mult < -1.0 + 1e-9) continue;

                p.pose.position.y = req.goal.pose.position.y + y_offset * y_mult;
                p.pose.position.x = req.goal.pose.position.x + x_offset * x_mult;

                if(planner_->makePlan(start, p, global_plan)){
                  if(!global_plan.empty()){

                    if (make_plan_add_unreachable_goal_) {
                      //adding the (unreachable) original goal to the end of the global plan, in case the local planner can get you there
                      //(the reachable goal should have been added by the global planner)
                      global_plan.push_back(req.goal);
                    }

                    found_legal = true;
                    ROS_DEBUG_NAMED("move_base", "Found a plan to point (%.2f, %.2f)", p.pose.position.x, p.pose.position.y);
                    break;
                  }
                }
                else{
                  ROS_DEBUG_NAMED("move_base","Failed to find a plan to point (%.2f, %.2f)", p.pose.position.x, p.pose.position.y);
                }
              }
            }
          }
        }
      }
    }

    //copy the plan into a message to send out
    resp.plan.poses.resize(global_plan.size());
    for(unsigned int i = 0; i < global_plan.size(); ++i){
      resp.plan.poses[i] = global_plan[i];
    }

    return true;
  }

  MoveBase::~MoveBase(){
    // 1. make sure exit executeCb and don't execute it again.
    recovery_index_ = (int)recovery_behaviors_.size();
    planner_patience_ = 0.0;
    max_planning_retries_ = 0;
    if (as_ != NULL) {
      delete as_;
    }

    recovery_behaviors_.clear();

    // 2. dynamic_reconfigure::Server
    delete dsrv_;

    // 3. destroy planThread. 
    // planThread use planner_costmap_ros_ during makePlan.
    delete planner_thread_;

    // 4. destroy gloal-costmap and local-costmap
    if (planner_costmap_ros_ != NULL) {
      delete planner_costmap_ros_;
    }

    if (controller_costmap_ros_ != NULL) {
      delete controller_costmap_ros_;
    }

    // 5. misc object
    delete planner_plan_;
    delete latest_plan_;
    delete controller_plan_;

    planner_.reset();
    tc_.reset();

    // pathfind::release_pq();
  }

  bool MoveBase::makePlan(const geometry_msgs::PoseStamped& goal, std::vector<geometry_msgs::PoseStamped>& plan){
    boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(planner_costmap_ros_->getCostmap()->getMutex()));

    //make sure to set the plan to be empty initially
    plan.clear();

    if (makePlanFail) {
      return false;
    }
    //since this gets called on handle activate
    if(planner_costmap_ros_ == NULL) {
      ROS_ERROR("Planner costmap ROS is NULL, unable to create global plan");
      return false;
    }

    //get the starting pose of the robot
    geometry_msgs::PoseStamped global_pose;

    if(!getRobotPose(global_pose, planner_costmap_ros_)) {
      ROS_WARN("Unable to get starting pose of robot, unable to create global plan");
      return false;
    }

    SDL_Log("{makPlan_getRobotPose0}post getRobotPose, global_pose.xy: (%.5f, %.5f) makPlan_getRobotPose0: %s", 
        global_pose.pose.position.x, global_pose.pose.position.y, makPlan_getRobotPose0? "true": "false");
    if (makPlan_getRobotPose0) {
        makPlan_getRobotPose0 = false;
    }

    if (just_clearing_) {
        SDL_Log("[makePlan]just a cleaing, execute clear+update for global-costmap");
        planner_costmap_ros_->clearCostmaps();
        planner_costmap_ros_->updateMap();
        just_clearing_ = false;
    }

    const geometry_msgs::PoseStamped& start = global_pose;

    //if the planner fails or returns a zero length plan, planning failed
    if(!planner_->makePlan(start, goal, plan) || plan.empty()){
      ROS_DEBUG_NAMED("move_base","Failed to find a  plan to point (%.2f, %.2f)", goal.pose.position.x, goal.pose.position.y);
      return false;
    }

    return true;
  }

  void MoveBase::publishZeroVelocity(){
    geometry_msgs::Twist cmd_vel;
    cmd_vel.linear.x = 0.0;
    cmd_vel.linear.y = 0.0;
    cmd_vel.angular.z = 0.0;
    vel_pub_.publish(cmd_vel);
  }

  bool MoveBase::isQuaternionValid(const geometry_msgs::Quaternion& q){
    //first we need to check if the quaternion has nan's or infs
    if(!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) || !std::isfinite(q.w)){
      ROS_ERROR("Quaternion has nans or infs... discarding as a navigation goal");
      return false;
    }

    tf2::Quaternion tf_q(q.x, q.y, q.z, q.w);

    //next, we need to check if the length of the quaternion is close to zero
    if(tf_q.length2() < 1e-6){
      ROS_ERROR("Quaternion has length close to zero... discarding as navigation goal");
      return false;
    }

    //next, we'll normalize the quaternion and check that it transforms the vertical vector correctly
    tf_q.normalize();

    tf2::Vector3 up(0, 0, 1);

    double dot = up.dot(up.rotate(tf_q.getAxis(), tf_q.getAngle()));

    if(fabs(dot - 1) > 1e-3){
      ROS_ERROR("Quaternion is invalid... for navigation the z-axis of the quaternion must be close to vertical.");
      return false;
    }

    return true;
  }

  geometry_msgs::PoseStamped MoveBase::goalToGlobalFrame(const geometry_msgs::PoseStamped& goal_pose_msg){
    std::string global_frame = planner_costmap_ros_->getGlobalFrameID();
    geometry_msgs::PoseStamped goal_pose, global_pose;
    goal_pose = goal_pose_msg;

    //just get the latest available transform... for accuracy they should send
    //goals in the frame of the planner
    goal_pose.header.stamp = ros::Time();

    try{
      tf_.transform(goal_pose_msg, global_pose, global_frame);
    }
    catch(tf2::TransformException& ex){
      ROS_WARN("Failed to transform the goal pose from %s into the %s frame: %s",
          goal_pose.header.frame_id.c_str(), global_frame.c_str(), ex.what());
      return goal_pose_msg;
    }

    return global_pose;
  }

  void MoveBase::wakePlanner(const ros::TimerEvent& event)
  {
    // we have slept long enough for rate
    planner_cond_.notify_one();
  }

  void MoveBase::OnTriggerExit()
  {
    planner_cond_.notify_one();
  }

  void MoveBase::resetTimeouts()
  {
    last_valid_control_ = ros::Time::now();
    last_valid_plan_ = ros::Time::now();
    last_oscillation_reset_ = ros::Time::now();
    planning_retries_ = 0;
  }

  void MoveBase::planThread(bool& exit){
  // void MoveBase::planThread(){
    ROS_DEBUG_NAMED("move_base_plan_thread","Starting planner thread...");
    ros::NodeHandle n;
    n.setCallbackQueue(&cbqueue_);
    ros::Timer timer;
    bool wait_for_wake = false;
    boost::unique_lock<boost::recursive_mutex> lock(planner_mutex_);

    double planner_frequency2 = 5.0; // makePlan every 200ms
    ros::Rate r(planner_frequency2);

    while(!exit && n.ok()){
    // while(n.ok()){
      //check if we should run the planner (the mutex is locked)
      // while(wait_for_wake || !runPlanner_){
      while(!exit && (wait_for_wake || !runPlanner_)){
        //if we should not be running the planner then suspend this thread
        ROS_DEBUG_NAMED("move_base_plan_thread","Planner thread is suspending");
        planner_cond_.wait(lock);
        wait_for_wake = false;
      }
      if (exit) { break; }
      ros::Time start_time = ros::Time::now();

      //time to plan! get a copy of the goal and unlock the mutex
      geometry_msgs::PoseStamped temp_goal = planner_goal_;
      lock.unlock();
      ROS_DEBUG_NAMED("move_base_plan_thread","Planning...");

      //run planner
      planner_plan_->clear();
      bool gotPlan = n.ok() && makePlan(temp_goal, *planner_plan_);

      if(gotPlan){
        ROS_DEBUG_NAMED("move_base_plan_thread","Got Plan with %zu points!", planner_plan_->size());
        //pointer swap the plans under mutex (the controller will pull from latest_plan_)
        std::vector<geometry_msgs::PoseStamped>* temp_plan = planner_plan_;

        lock.lock();
        planner_plan_ = latest_plan_;
        latest_plan_ = temp_plan;
        last_valid_plan_ = ros::Time::now();
        planning_retries_ = 0;
        new_global_plan_ = true;

        ROS_DEBUG_NAMED("move_base_plan_thread","Generated a plan from the base_global_planner");

        //make sure we only start the controller if we still haven't reached the goal
        if (runPlanner_)
          state_ = CONTROLLING;
        if (planner_frequency_ <= 0) {
          runPlanner_ = false;
        }
        lock.unlock();
      }
      //if we didn't get a plan and we are in the planning state (the robot isn't moving)
      else if(state_==PLANNING){
        // ROS_INFO_NAMED("move_base_plan_thread", "planning_retries_(%i), No Plan...", planning_retries_);
        ros::Time attempt_end = last_valid_plan_ + ros::Duration(planner_patience_);

        //check if we've tried to make a plan for over our time limit or our maximum number of retries
        //issue #496: we stop planning when one of the conditions is true, but if max_planning_retries_
        //is negative (the default), it is just ignored and we have the same behavior as ever
        lock.lock();
        planning_retries_++;

        {
            threading::lock lock(kidnap.last_pose2d_mutex);
            if (kidnap.estimating()) {
              ROS_DEBUG_NAMED("move_base", "is estimating, set set_apply_immediately");
              // kidnapp is estimating. Most likely, the next makePlan will also fail, delay,
              kidnap.set_apply_immediately();
              SDL_Delay(100);
              // resetTimeouts() will extend last_valid_plan_, don't call it.
            }
        }

        if(runPlanner_ &&
           (ros::Time::now() > attempt_end || planning_retries_ > uint32_t(max_planning_retries_))){
          //we'll move into our obstacle clearing mode
          state_ = CLEARING;
          runPlanner_ = false;  // proper solution for issue #523
          publishZeroVelocity();
          recovery_trigger_ = PLANNING_R;
        }

        lock.unlock();
      }

      r.sleep();

      // take the mutex for the next iteration
      lock.lock();

/*
      // setup sleep interface if needed
      if(planner_frequency_ > 0){
        ros::Duration sleep_time = (start_time + ros::Duration(1.0/planner_frequency_)) - ros::Time::now();
        if (sleep_time > ros::Duration(0.0)){
          wait_for_wake = true;
          timer = n.createTimer(sleep_time, &MoveBase::wakePlanner, this);
        }
      }
*/
    }
  }

  void adjust_quaternion_yaw_nposm(geometry_msgs::Quaternion& q)
  {
      bool raw_unrestricted = is_quaternion_raw_nposm(q);
      if (raw_unrestricted) {
          q.x = 0;
      }
      ros::yaw_goal_tolerance = ros::buildmap || raw_unrestricted? ROS_UNRESTRICTED_YAW_GOAL_TOLERANCE: ROS_DEF_YAW_GOAL_TOLERANCE;
  }

  void MoveBase::executeCb(const move_base_msgs::MoveBaseGoalConstPtr& _move_base_goal)
  {
    move_base_msgs::MoveBaseGoal move_base_goal = *_move_base_goal.get();
    adjust_quaternion_yaw_nposm(move_base_goal.target_pose.pose.orientation);

    const geometry_msgs::PoseStamped& target_pose2 = move_base_goal.target_pose;

    sendGoaling = true;
    const geometry_msgs::Quaternion& q = target_pose2.pose.orientation;
    tf2::Matrix3x3 mat(tf2::Quaternion(q.x, q.y, q.z, q.w));
    double yaw, pitch, roll;
    mat.getEulerYPR(yaw, pitch, roll);
/*
    SDL_Log("%.5f MoveBase::executeCb, seq: %u, stamp: %.5f, frame_id: %s, position: (%.5f, %.5f, %.5f), orientation(%.5f, %.5f, %.5f)",
        ros::Time::now().toSec(), target_pose2.header.seq, target_pose2.header.stamp.toSec(), target_pose2.header.frame_id.c_str(),
        target_pose2.pose.position.x, target_pose2.pose.position.y, target_pose2.pose.position.z,
        roll, pitch, yaw);
*/
    if(!isQuaternionValid(move_base_goal.target_pose.pose.orientation)){
      as_->setAborted(move_base_msgs::MoveBaseResult(), "Aborting on goal because it was sent with an invalid quaternion");
      return;
    }

    geometry_msgs::PoseStamped goal = goalToGlobalFrame(move_base_goal.target_pose);

    publishZeroVelocity();
    //we have a goal so start the planner
    boost::unique_lock<boost::recursive_mutex> lock(planner_mutex_);
    planner_goal_ = goal;
    runPlanner_ = true;
    planner_cond_.notify_one();
    lock.unlock();
    tc_->set_fresh_move();

    current_goal_pub_.publish(goal);

    // double controller_frequency2 = 2.5;  // default is 2.5
    double controller_frequency2 = ros::base_cfg.move_base_controller_freq;
    // ros::Rate r(controller_frequency_);
    ros::Rate r(controller_frequency2);
    if(shutdown_costmaps_){
      ROS_DEBUG_NAMED("move_base","Starting up costmaps that were shut down previously");
      planner_costmap_ros_->start(exit_);
      controller_costmap_ros_->start(exit_);
    }

    VALIDATE(!kidnap.estimating(), null_str);
    if (kidnap.can_estimate()) {
        threading::lock lock(kidnap.last_pose2d_mutex);
        kidnap.set_estimate(tkidnap::reason_goal, kidnap.last_pose2d);
    }

    //we want to make sure that we reset the last time we had a valid plan and control
    resetTimeouts();
    oscillation_timeout_ = DEFAULT_OSCILLATION_TIMOUT;

    ros::NodeHandle n;
    while(n.ok())
    {
      ROS_INFO("%u [move_base]executeCb, slice", SDL_GetTicks());
      if(c_freq_change_)
      {
        ROS_INFO("Setting controller frequency to %.2f", controller_frequency_);
        r = ros::Rate(controller_frequency_);
        c_freq_change_ = false;
      }

      if(as_->isPreemptRequested()){
        if(as_->isNewGoalAvailable()){
          //if we're active and a new goal is available, we'll accept it, but we won't shut anything down
          move_base_msgs::MoveBaseGoal new_goal = *as_->acceptNewGoal();
          adjust_quaternion_yaw_nposm(new_goal.target_pose.pose.orientation);

          if(!isQuaternionValid(new_goal.target_pose.pose.orientation)){
            as_->setAborted(move_base_msgs::MoveBaseResult(), "Aborting on goal because it was sent with an invalid quaternion");
            return;
          }

          goal = goalToGlobalFrame(new_goal.target_pose);

          //we'll make sure that we reset our state for the next execution cycle
          recovery_index_ = 0;
          state_ = PLANNING;

          //we have a new goal so make sure the planner is awake
          lock.lock();
          planner_goal_ = goal;
          runPlanner_ = true;
          planner_cond_.notify_one();
          lock.unlock();
          tc_->set_fresh_move();

          //publish the goal point to the visualizer
          ROS_DEBUG_NAMED("move_base","move_base has received a goal of x: %.2f, y: %.2f", goal.pose.position.x, goal.pose.position.y);
          current_goal_pub_.publish(goal);

          //make sure to reset our timeouts and counters
          resetTimeouts();
          oscillation_timeout_ = DEFAULT_OSCILLATION_TIMOUT;
        }
        else {
          //if we've been preempted explicitly we need to shut things down
          resetState();

          //notify the ActionServer that we've successfully preempted
          ROS_DEBUG_NAMED("move_base","Move base preempting the current goal");
          as_->setPreempted();

          //we'll actually return from execute after preempting
          return;
        }
      }

      //we also want to check if we've changed global frames because we need to transform our goal pose
      if(goal.header.frame_id != planner_costmap_ros_->getGlobalFrameID()){
        goal = goalToGlobalFrame(goal);

        //we want to go back to the planning state for the next execution cycle
        recovery_index_ = 0;
        state_ = PLANNING;

        //we have a new goal so make sure the planner is awake
        lock.lock();
        planner_goal_ = goal;
        runPlanner_ = true;
        planner_cond_.notify_one();
        lock.unlock();

        //publish the goal point to the visualizer
        ROS_DEBUG_NAMED("move_base","The global frame for move_base has changed, new frame: %s, new goal position x: %.2f, y: %.2f", goal.header.frame_id.c_str(), goal.pose.position.x, goal.pose.position.y);
        current_goal_pub_.publish(goal);

        //make sure to reset our timeouts and counters
        resetTimeouts();
      }

      //for timing that gives real time even in simulation
      ros::WallTime start = ros::WallTime::now();

      //the real work on pursuing a goal is done here
      bool done = executeCycle(goal);

      //if we're done, then we'll return from execute
      if(done)
        return;

      //check if execution of the goal has completed in some way

      ros::WallDuration t_diff = ros::WallTime::now() - start;
      // ROS_DEBUG_NAMED("move_base","Full control cycle time: %.9f\n", t_diff.toSec());
      // SDL_Log("%u [move_base]Full control cycle time: %.9f", SDL_GetTicks(), t_diff.toSec());

      r.sleep();
      //make sure to sleep for the remainder of our cycle time
      // if(r.cycleTime() > ros::Duration(1 / controller_frequency_) && state_ == CONTROLLING) {
      if(r.cycleTime() > ros::Duration(1 / controller_frequency2) && state_ == CONTROLLING) {
        ROS_WARN("Control loop missed its desired rate of %.4fHz... the loop actually took %4f seconds", controller_frequency_, r.cycleTime().toSec());
      }
    }

    //wake up the planner thread so that it can exit cleanly
    lock.lock();
    runPlanner_ = true;
    planner_cond_.notify_one();
    lock.unlock();

    //if the node is killed then we'll abort and return
    as_->setAborted(move_base_msgs::MoveBaseResult(), "Aborting on the goal because the node has been killed");
    return;
  }

  double MoveBase::distance(const geometry_msgs::PoseStamped& p1, const geometry_msgs::PoseStamped& p2)
  {
    return hypot(p1.pose.position.x - p2.pose.position.x, p1.pose.position.y - p2.pose.position.y);
  }

  // PLANNING/CLEARING: Whether can publish cmd_vel that this generate.
  // CONTROLLING: Whether can publish cmd_vel that received before computeVelocityCommands(...).
  bool MoveBase::set_kidnap_vel_state(geometry_msgs::Twist& cmd_vel)
  {
      bool valid = false;
      threading::lock lock(kidnap.last_pose2d_mutex);
      if (kidnap.estimating()) {
          if (kidnap.vel_state() == tkidnap::vel_req) {
              const tpose2d& from = kidnap.vel_req_start_pose2d;
              const tpose2d& to = kidnap.last_pose2d;
              double dist = hypot(from.x - to.x, from.y - to.y);
              double yaw_diff = fabs(angles::normalize_angle(to.yaw - from.yaw));
              uint32_t elapsed = SDL_GetTicks() - kidnap.vel_req_start_ticks;

              if (kidnap.vel_pub_times >= 1 && (dist >= 0.1 || yaw_diff > DEG2RAD(45) || (int)elapsed >= kidnap.MOVE_TIMEOUT_MS)) {
                  // for get into 'vel_finished', cmd_vel needs to be published at least once.
                  {
                    // Ensure that the move has ended when ros_instance::kidnap_slice executes. 
                    // As for how much time to delay, it is still necessary to consider.
                    SDL_Delay(1000);
                  }
                  kidnap.set_vel_state(tkidnap::vel_finished, state_);
              } else {
                  if (state_ != CONTROLLING) {
                      valid = tc_->a_star_search_move(planner_goal_, cmd_vel);
                  } else {
                      valid = true;
                  }
                  if (valid) {
                      kidnap.vel_pub_times ++;
                      kidnap.add_dbg_msg(tkidnap::msg_set_vel_state, tkidnap::vel_pub, state_, tpose2d_C{cmd_vel.linear.x, cmd_vel.linear.y, cmd_vel.angular.z, true});
                  }
              }
              SDL_Log("%u {kestimate}move_base.%s, #%i vel_pub_times: %i, set_vel_state(%i) dist: %.3f yaw_diff: %.3f elapsed %u ms", 
                  SDL_GetTicks(), MoveBaseStates[state_], kidnap.moved_times, kidnap.vel_pub_times, kidnap.vel_state(), dist, RAD2DEG(yaw_diff), elapsed);

              // resetTimeouts();

          } else {
              SDL_Log("%u {kestimate}move_base.%s, #%i vel_state: %i, don't public vel", SDL_GetTicks(), MoveBaseStates[state_], kidnap.moved_times, kidnap.vel_state());
          }
          resetTimeouts();

      } else if (state_ == CONTROLLING) {
          valid = true;
      }
      return valid;
  }

  bool MoveBase::executeCycle(geometry_msgs::PoseStamped& goal){
    boost::recursive_mutex::scoped_lock ecl(configuration_mutex_);
    bool goalReached = false;
    const bool goalReached_true = !ros::buildmap && (game_config::is_dbg_charge() || game_config::is_dbg_not_move());
    //we need to be able to publish velocity commands
    geometry_msgs::Twist cmd_vel;

    //update feedback to correspond to our curent position
    geometry_msgs::PoseStamped global_pose;
    getRobotPose(global_pose, planner_costmap_ros_);
    const geometry_msgs::PoseStamped& current_position = global_pose;

    //push the feedback out
    move_base_msgs::MoveBaseFeedback feedback;
    feedback.base_position = current_position;
    as_->publishFeedback(feedback);

    //check to see if we've moved far enough to reset our oscillation timeout
    if(distance(current_position, oscillation_pose_) >= oscillation_distance_)
    {
      last_oscillation_reset_ = ros::Time::now();
      oscillation_pose_ = current_position;

      //if our last recovery was caused by oscillation, we want to reset the recovery index
      if(recovery_trigger_ == OSCILLATION_R)
        recovery_index_ = 0;
    }

    //check that the observation buffers for the costmap are current, we don't want to drive blind
    if(!controller_costmap_ros_->isCurrent()){
      ROS_WARN("[%s]:Sensor data is out of date, we're not going to allow commanding of the base for safety",ros::this_node::getName().c_str());
      publishZeroVelocity();
      return false;
    }

    //if we have a new plan then grab it and give it to the controller
    if(new_global_plan_){
      //make sure to set the new plan flag to false
      new_global_plan_ = false;

      ROS_DEBUG_NAMED("move_base","Got a new plan...swap pointers");

      //do a pointer swap under mutex
      std::vector<geometry_msgs::PoseStamped>* temp_plan = controller_plan_;

      boost::unique_lock<boost::recursive_mutex> lock(planner_mutex_);
      controller_plan_ = latest_plan_;
      latest_plan_ = temp_plan;
      lock.unlock();
      ROS_DEBUG_NAMED("move_base","pointers swapped!");

      if(!tc_->setPlan(*controller_plan_)){
        //ABORT and SHUTDOWN COSTMAPS
        ROS_ERROR("Failed to pass global plan to the controller, aborting.");
        resetState();

        //disable the planner thread
        lock.lock();
        runPlanner_ = false;
        lock.unlock();

        as_->setAborted(move_base_msgs::MoveBaseResult(), "Failed to pass global plan to the controller.");
        return true;
      }

      //make sure to reset recovery_index_ since we were able to find a valid plan
      if(recovery_trigger_ == PLANNING_R)
        recovery_index_ = 0;
    }

    
    if (kidnap.position_changed()) {
        tc_->reset_xy_tolerance_latch();
        std::vector<costmap_2d::Costmap2DROS*> costmaps;
        costmaps.push_back(planner_costmap_ros_);
        costmaps.push_back(controller_costmap_ros_);
        for (std::vector<costmap_2d::Costmap2DROS*>::const_iterator it = costmaps.begin(); it != costmaps.end(); ++ it) {
            costmap_2d::Costmap2DROS* costmap2d = *it;
            boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(costmap2d->getCostmap()->getMutex()));
            costmap2d->clearCostmaps();
            SDL_Log("%u {kestimate}position changed, clear %s costmap", SDL_GetTicks(), costmap2d->getName().c_str());
        }
        kidnap.clear_position_changed();
    }

    //the move_base state machine, handles the control logic for navigation
    switch(state_){
      //if we are in a planning state, then we'll attempt to make a plan
      case PLANNING:
        {
          boost::recursive_mutex::scoped_lock lock(planner_mutex_);
          runPlanner_ = true;
          planner_cond_.notify_one();
        }
        if (set_kidnap_vel_state(cmd_vel)) {
            vel_pub_.publish(cmd_vel);
        }
        {
          ros::Time attempt_end = last_valid_plan_ + ros::Duration(planner_patience_);
          ROS_INFO_NAMED("move_base", "Waiting for plan, in the planning state. planner_patience_: %.3f, makePlan should terminate: %.4f", planner_patience_, attempt_end.toSec());
        }
        break;

      //if we're controlling, we'll attempt to find valid velocity commands
      case CONTROLLING:
        ROS_DEBUG_NAMED("move_base", "In controlling state.");


        if (!goalReached_true) {
            //check to see if we've reached our goal
            goalReached = tc_->isGoalReached();
            if (goalReached && kidnap.is_allowed() && kidnap.estimated_times == 0) {
                SDL_Log("%u Although goal reached, but estimated_times is 0, holp one estimate", SDL_GetTicks());
                goalReached = false;
                resetTimeouts();
            }
        } else {
            goalReached = true;
        }
        if (goalReached) {
          ROS_INFO_NAMED("move_base", "[executeCycle]Goal reached!");
          resetState();

          //disable the planner thread
          boost::unique_lock<boost::recursive_mutex> lock(planner_mutex_);
          runPlanner_ = false;
          lock.unlock();

          as_->setSucceeded(move_base_msgs::MoveBaseResult(), "Goal reached.");
          return true;
        }

        if (oscillation_timeout_ != XYREACHED_OSCILLATION_TIMOUT && tc_->xy_tolerance_latch()) {
            ROS_INFO_NAMED("move_base", "Positiion is reached, change timeout from %.2f to %.2f", oscillation_timeout_, XYREACHED_OSCILLATION_TIMOUT);
            oscillation_timeout_ = XYREACHED_OSCILLATION_TIMOUT;
            last_oscillation_reset_ = ros::Time::now();
        }

        //check for an oscillation condition
        if(oscillation_timeout_ > 0.0 &&
            last_oscillation_reset_ + ros::Duration(oscillation_timeout_) < ros::Time::now())
        {
          publishZeroVelocity();
          state_ = CLEARING;
          recovery_trigger_ = OSCILLATION_R;
        }

        {
        boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(controller_costmap_ros_->getCostmap()->getMutex()));
            tpose2d current_pose2d;
            {
                threading::lock lock(kidnap.last_pose2d_mutex);
                current_pose2d = kidnap.last_pose2d;
            }
            int estimate_reason = nposm;
            if (kidnap.require_relay_estimate(current_pose2d.x, current_pose2d.y)) {
                SDL_Log("{kestimate}move_base, because relay(%.3f), current_pose2d:%s, trigger estimate...", 
                    hypot(current_pose2d.x - kidnap.last_ok_estimated_pose2d().x, current_pose2d.y - kidnap.last_ok_estimated_pose2d().y), current_pose2d.to_string().c_str());
                estimate_reason = tkidnap::reason_relay;
            }

            double dist_to_goal = hypot(current_pose2d.x - planner_goal_.pose.position.x, current_pose2d.y - planner_goal_.pose.position.y);
            if (estimate_reason == nposm && dist_to_goal <= kidnap.NEAR_GOAL) {
                if (kidnap.can_estimate_near_goal(current_pose2d.x, current_pose2d.y)) {
                    SDL_Log("{kestimate}move_base, because near goal(%.3f), current_pose2d: %s distance: %.3f trigger estimate...", 
                        dist_to_goal, current_pose2d.to_string().c_str(), hypot(current_pose2d.x - kidnap.last_ok_estimated_pose2d().x, current_pose2d.y - kidnap.last_ok_estimated_pose2d().y));
                    estimate_reason = tkidnap::reason_neargoal;
                } // else {
                //    SDL_Log("{kestimate}computeVelocityCommands, PLANNING_R, current_pose2d: %s not estimate", current_pose2d.to_string().c_str());
                // }
            }
            if (estimate_reason != nposm) {
                kidnap.set_estimate(estimate_reason, kidnap.last_pose2d);
            }

        if(tc_->computeVelocityCommands(cmd_vel)){
          ROS_DEBUG_NAMED( "move_base", "Got a valid command from the local planner: %.3lf, %.3lf, %.3lf",
                           cmd_vel.linear.x, cmd_vel.linear.y, cmd_vel.angular.z );
          last_valid_control_ = ros::Time::now();
          //make sure that we send the velocity command to the base

          if (ros::rviz_breakpoint) {
              SDL_Delay(2000);
              int ii = 0; // here set breakpoint
          }
          bool public_vel = set_kidnap_vel_state(cmd_vel);
          if (public_vel) {
            vel_pub_.publish(cmd_vel);
          }
          if (recovery_trigger_ == CONTROLLING_R) {
            recovery_index_ = 0;
          }

        } else {
          ROS_INFO_NAMED("move_base", "The local planner could not find a valid plan.");
          ros::Time attempt_end = last_valid_control_ + ros::Duration(controller_patience_);

          if (ros::rviz_breakpoint) {
              SDL_Delay(2000);
              int ii = 0; // here set breakpoint
          }

          //check if we've tried to find a valid control for longer than our time limit
          if(ros::Time::now() > attempt_end){
            //we'll move into our obstacle clearing mode
            publishZeroVelocity();
            state_ = CLEARING;
            recovery_trigger_ = CONTROLLING_R;
          }
          else{
            //otherwise, if we can't find a valid control, we'll go back to planning
            last_valid_plan_ = ros::Time::now();
            {
                ros::Time plan_attempt_end = last_valid_plan_ + ros::Duration(planner_patience_);
                ROS_INFO_NAMED("move_base", "planner_patience_: %.3f, makePlan should terminate: %.4f", planner_patience_, plan_attempt_end.toSec());
            }
            planning_retries_ = 0;
            state_ = PLANNING;
            publishZeroVelocity();

            //enable the planner thread in case it isn't running on a clock
            boost::unique_lock<boost::recursive_mutex> lock(planner_mutex_);
            runPlanner_ = true;
            planner_cond_.notify_one();
            just_clearing_ = true;
            lock.unlock();
          }
        }
        }

        break;

      //we'll try to clear out space with any user-provided recovery behaviors
      case CLEARING:
        ROS_DEBUG_NAMED("move_base", "In clearing/recovery state");
        //we'll invoke whatever recovery behavior we're currently on if they're enabled
        if (recovery_behavior_enabled_ && recovery_index_ < recovery_behaviors_.size()){
          move_base_msgs::RecoveryStatus msg;
          msg.pose_stamped = current_position;
          msg.current_recovery_number = recovery_index_;
          msg.total_number_of_recoveries = (uint16_t)recovery_behaviors_.size();
          msg.recovery_behavior_name =  recovery_behavior_names_[recovery_index_];
          ROS_INFO_NAMED("move_base_recovery", "Executing behavior %u of %zu %s, reason: %i", 
              recovery_index_+1, recovery_behaviors_.size(), msg.recovery_behavior_name.c_str(), recovery_trigger_);

          recovery_status_pub_.publish(msg);

          recovery_behaviors_[recovery_index_]->runBehavior();

          // startup kidnap estimate or a_start_search_move.
          tpose2d current_pose2d;
          {
              threading::lock lock(kidnap.last_pose2d_mutex);
              current_pose2d = kidnap.last_pose2d;
          }
          if (kidnap.can_estimate_CLEARING(current_pose2d.x, current_pose2d.y)) {
              SDL_Log("{kestimate}move_base, because PLANNING_R, current_pose2d: %s distance: %.3f trigger estimate...", 
                  current_pose2d.to_string().c_str(), hypot(current_pose2d.x - kidnap.last_ok_estimated_pose2d().x, current_pose2d.y - kidnap.last_ok_estimated_pose2d().y));
              threading::lock lock(kidnap.last_pose2d_mutex);
              kidnap.set_estimate(tkidnap::reason_clearing, kidnap.last_pose2d);
              if (recovery_trigger_ == PLANNING_R) {
                  kidnap.set_apply_immediately();
              }
          } else {
              SDL_Log("{kestimate}move_base, PLANNING_R, current_pose2d: %s not estimate", current_pose2d.to_string().c_str());
          }

          bool pub_vel = set_kidnap_vel_state(cmd_vel);
          if (!kidnap.estimating()) {
              // if not in estimating, don't move robot, let call makePlan as soon as.
              // pub_vel = tc_->a_star_search_move(planner_goal_, cmd_vel);
          }
          if (pub_vel) {
              vel_pub_.publish(cmd_vel);
          }
          tc_->set_fresh_move();

          {
              // trose_recovery clear both global_costmap and local_costmap,
              // make sure has laser_scan before makePlan.
              // for controller_costmap_ros_, updateMap in a_star_search_move().

              // Since the robot_pose may be wrong, it will make makePlan fail
              // boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(planner_costmap_ros_->getCostmap()->getMutex()));
              // planner_costmap_ros_->updateMap();
          }

          //we at least want to give the robot some time to stop oscillating after executing the behavior
          last_oscillation_reset_ = ros::Time::now();

          //we'll check if the recovery behavior actually worked
          ROS_INFO_NAMED("move_base_recovery", "Going back to planning state");
          last_valid_plan_ = ros::Time::now();
          planning_retries_ = 0;
          state_ = PLANNING;
          just_clearing_ = true;

          //update the index of the next recovery behavior that we'll try
          recovery_index_++;
        }
        else{
          ROS_INFO_NAMED("move_base_recovery", "All recovery behaviors have failed, locking the planner and disabling it.");
          //disable the planner thread
          boost::unique_lock<boost::recursive_mutex> lock(planner_mutex_);
          runPlanner_ = false;
          lock.unlock();

          ROS_INFO_NAMED("move_base_recovery", "Something should abort after this.");

          if(recovery_trigger_ == CONTROLLING_R){
            ROS_ERROR("Aborting because a valid control could not be found. Even after executing all recovery behaviors");
            as_->setAborted(move_base_msgs::MoveBaseResult(), "Failed to find a valid control. Even after executing recovery behaviors.");
          }
          else if(recovery_trigger_ == PLANNING_R){
            ROS_ERROR("Aborting because a valid plan could not be found. Even after executing all recovery behaviors");
            as_->setAborted(move_base_msgs::MoveBaseResult(), "Failed to find a valid plan. Even after executing recovery behaviors.");
          }
          else if(recovery_trigger_ == OSCILLATION_R){
            ROS_ERROR("Aborting because the robot appears to be oscillating over and over. Even after executing all recovery behaviors");
            as_->setAborted(move_base_msgs::MoveBaseResult(), "Robot is oscillating. Even after executing recovery behaviors.");
          }
          resetState();

          if (ros::rviz_breakpoint) {
            SDL_Delay(2000);
            int ii = 0; // here set breakpoint
          }
          return true;
        }
        break;
      default:
        ROS_ERROR("This case should never be reached, something is wrong, aborting");
        resetState();
        //disable the planner thread
        boost::unique_lock<boost::recursive_mutex> lock(planner_mutex_);
        runPlanner_ = false;
        lock.unlock();
        as_->setAborted(move_base_msgs::MoveBaseResult(), "Reached a case that should not be hit in move_base. This is a bug, please report it.");
        return true;
    }

    //we aren't done yet
    return false;
  }

  bool MoveBase::loadRecoveryBehaviors(ros::NodeHandle node){
    XmlRpc::XmlRpcValue behavior_list;
    if(node.getParam("recovery_behaviors", behavior_list)){
      if(behavior_list.getType() == XmlRpc::XmlRpcValue::TypeArray){
        for(int i = 0; i < behavior_list.size(); ++i){
          if(behavior_list[i].getType() == XmlRpc::XmlRpcValue::TypeStruct){
            if(behavior_list[i].hasMember("name") && behavior_list[i].hasMember("type")){
              //check for recovery behaviors with the same name
              for(int j = i + 1; j < behavior_list.size(); j++){
                if(behavior_list[j].getType() == XmlRpc::XmlRpcValue::TypeStruct){
                  if(behavior_list[j].hasMember("name") && behavior_list[j].hasMember("type")){
                    std::string name_i = behavior_list[i]["name"];
                    std::string name_j = behavior_list[j]["name"];
                    if(name_i == name_j){
                      ROS_ERROR("A recovery behavior with the name %s already exists, this is not allowed. Using the default recovery behaviors instead.",
                          name_i.c_str());
                      return false;
                    }
                  }
                }
              }
            }
            else{
              ROS_ERROR("Recovery behaviors must have a name and a type and this does not. Using the default recovery behaviors instead.");
              return false;
            }
          }
          else{
            ROS_ERROR("Recovery behaviors must be specified as maps, but they are XmlRpcType %d. We'll use the default recovery behaviors instead.",
                behavior_list[i].getType());
            return false;
          }
        }

        //if we've made it to this point, we know that the list is legal so we'll create all the recovery behaviors
        for(int i = 0; i < behavior_list.size(); ++i){
          // try{
            // There is no occasion to execute here, and so on to implement if need. 
            VALIDATE(false, null_str);
            //check if a non fully qualified name has potentially been passed in
/*
            if(!recovery_loader_.isClassAvailable(behavior_list[i]["type"])){
              std::vector<std::string> classes = recovery_loader_.getDeclaredClasses();
              for(unsigned int i = 0; i < classes.size(); ++i){
                if(behavior_list[i]["type"] == recovery_loader_.getName(classes[i])){
                  //if we've found a match... we'll get the fully qualified name and break out of the loop
                  ROS_WARN("Recovery behavior specifications should now include the package name. You are using a deprecated API. Please switch from %s to %s in your yaml file.",
                      std::string(behavior_list[i]["type"]).c_str(), classes[i].c_str());
                  behavior_list[i]["type"] = classes[i];
                  break;
                }
              }
            }

            const std::string lookup_name = behavior_list[i]["type"];
            ROS_INFO("behavior_list[%i/%i][type]: %s", i, (int)behavior_list.size(), lookup_name.c_str());
            boost::shared_ptr<nav_core::RecoveryBehavior> behavior(recovery_loader_.createInstance(behavior_list[i]["type"]));

            //shouldn't be possible, but it won't hurt to check
            if(behavior.get() == NULL){
              ROS_ERROR("The ClassLoader returned a null pointer without throwing an exception. This should not happen");
              return false;
            }

            //initialize the recovery behavior with its name
            behavior->initialize(behavior_list[i]["name"], &tf_, planner_costmap_ros_, controller_costmap_ros_);
            recovery_behavior_names_.push_back(behavior_list[i]["name"]);
            recovery_behaviors_.push_back(behavior);
*/
          // }
          // catch(pluginlib::PluginlibException& ex){
          //  ROS_ERROR("Failed to load a plugin. Using default recovery behaviors. Error: %s", ex.what());
          //  return false;
          // }
        }
      }
      else{
        ROS_ERROR("The recovery behavior specification must be a list, but is of XmlRpcType %d. We'll use the default recovery behaviors instead.",
            behavior_list.getType());
        return false;
      }
    }
    else{
      //if no recovery_behaviors are specified, we'll just load the defaults
      return false;
    }

    //if we've made it here... we've constructed a recovery behavior list successfully
    return true;
  }

  //we'll load our default recovery behaviors here
  void MoveBase::loadDefaultRecoveryBehaviors(){
    recovery_behaviors_.clear();
    {
        boost::shared_ptr<nav_core::RecoveryBehavior> rose_clear(new trose_recovery(cbqueue_));
        rose_clear->initialize("rose_clear", &tf_, planner_costmap_ros_, controller_costmap_ros_);
        // #1
        recovery_behavior_names_.push_back("rose_clear#1");
        recovery_behaviors_.push_back(rose_clear);
        // #2 twice
        recovery_behavior_names_.push_back("rose_clear#2");
        recovery_behaviors_.push_back(rose_clear);
        return;
    }

    // try{
      //we need to set some parameters based on what's been passed in to us to maintain backwards compatibility
      ros::NodeHandle n("~");
      n.setParam("conservative_reset/reset_distance", conservative_reset_dist_);
      n.setParam("aggressive_reset/reset_distance", circumscribed_radius_ * 4);

      //first, we'll load a recovery behavior to clear the costmap
      // boost::shared_ptr<nav_core::RecoveryBehavior> cons_clear(recovery_loader_.createInstance("clear_costmap_recovery/ClearCostmapRecovery"));
      boost::shared_ptr<nav_core::RecoveryBehavior> cons_clear(new clear_costmap_recovery::ClearCostmapRecovery(cbqueue_));
      cons_clear->initialize("conservative_reset", &tf_, planner_costmap_ros_, controller_costmap_ros_);
      recovery_behavior_names_.push_back("conservative_reset");
      recovery_behaviors_.push_back(cons_clear);

      //next, we'll load a recovery behavior to rotate in place
      // boost::shared_ptr<nav_core::RecoveryBehavior> rotate(recovery_loader_.createInstance("rotate_recovery/RotateRecovery"));
      boost::shared_ptr<nav_core::RecoveryBehavior> rotate(new rotate_recovery::RotateRecovery(cbqueue_));
      if(clearing_rotation_allowed_){
        rotate->initialize("rotate_recovery", &tf_, planner_costmap_ros_, controller_costmap_ros_);
        recovery_behavior_names_.push_back("rotate_recovery");
        recovery_behaviors_.push_back(rotate);
      }

      //next, we'll load a recovery behavior that will do an aggressive reset of the costmap
      // boost::shared_ptr<nav_core::RecoveryBehavior> ags_clear(recovery_loader_.createInstance("clear_costmap_recovery/ClearCostmapRecovery"));
      boost::shared_ptr<nav_core::RecoveryBehavior> ags_clear(new clear_costmap_recovery::ClearCostmapRecovery(cbqueue_));
      ags_clear->initialize("aggressive_reset", &tf_, planner_costmap_ros_, controller_costmap_ros_);
      recovery_behavior_names_.push_back("aggressive_reset");
      recovery_behaviors_.push_back(ags_clear);

      //we'll rotate in-place one more time
      if(clearing_rotation_allowed_){
        recovery_behaviors_.push_back(rotate);
        recovery_behavior_names_.push_back("rotate_recovery");
      }
    // }
    // catch(pluginlib::PluginlibException& ex){
    //  ROS_FATAL("Failed to load a plugin. This should not happen on default recovery behaviors. Error: %s", ex.what());
    // }

    return;
  }

  void MoveBase::resetState(){
    // Disable the planner thread
    boost::unique_lock<boost::recursive_mutex> lock(planner_mutex_);
    runPlanner_ = false;
    lock.unlock();

    // Reset statemachine
    state_ = PLANNING;
    recovery_index_ = 0;
    recovery_trigger_ = PLANNING_R;
    publishZeroVelocity();

    //if we shutdown our costmaps when we're deactivated... we'll do that now
    if(shutdown_costmaps_){
      ROS_DEBUG_NAMED("move_base","Stopping costmaps");
      planner_costmap_ros_->stop();
      controller_costmap_ros_->stop();
    }
    sendGoaling = false;
  }

  bool MoveBase::getRobotPose(geometry_msgs::PoseStamped& global_pose, costmap_2d::Costmap2DROS* costmap)
  {
    tf2::toMsg(tf2::Transform::getIdentity(), global_pose.pose);
    geometry_msgs::PoseStamped robot_pose;
    tf2::toMsg(tf2::Transform::getIdentity(), robot_pose.pose);
    robot_pose.header.frame_id = robot_base_frame_;
    robot_pose.header.stamp = ros::Time(); // latest available
    ros::Time current_time = ros::Time::now();  // save time for checking tf delay later

    // get robot pose on the given costmap frame
    try
    {
      tf_.transform(robot_pose, global_pose, costmap->getGlobalFrameID());
      if (makPlan_getRobotPose0) {
          SDL_Log("{makPlan_getRobotPose0}getRobotPose() [%s]robot_pose_xy: (%.5f, %.5f) -> [%s]global_pose: (%.5f, %.5f)",
              robot_pose.header.frame_id.c_str(), robot_pose.pose.position.x, robot_pose.pose.position.y,
              global_pose.header.frame_id.c_str(), global_pose.pose.position.x, global_pose.pose.position.y);
      }
    }
    catch (tf2::LookupException& ex)
    {
      ROS_ERROR_THROTTLE(1.0, "No Transform available Error looking up robot pose: %s\n", ex.what());
      return false;
    }
    catch (tf2::ConnectivityException& ex)
    {
      ROS_ERROR_THROTTLE(1.0, "Connectivity Error looking up robot pose: %s\n", ex.what());
      return false;
    }
    catch (tf2::ExtrapolationException& ex)
    {
      ROS_ERROR_THROTTLE(1.0, "Extrapolation Error looking up robot pose: %s\n", ex.what());
      return false;
    }

    // check if global_pose time stamp is within costmap transform tolerance
    if (!global_pose.header.stamp.isZero() &&
        current_time.toSec() - global_pose.header.stamp.toSec() > costmap->getTransformTolerance())
    {
      ROS_WARN_THROTTLE(1.0, "Transform timeout for %s. " \
                        "Current time: %.4f, pose stamp: %.4f, tolerance: %.4f", costmap->getName().c_str(),
                        current_time.toSec(), global_pose.header.stamp.toSec(), costmap->getTransformTolerance());
      return false;
    }

    return true;
  }
};
