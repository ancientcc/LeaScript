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
 *         David V. Lu!!
 *********************************************************************/
#include <costmap_2d/layered_costmap.h>
#include <costmap_2d/costmap_2d_ros.h>
#include <cstdio>
#include <string>
#include <algorithm>
#include <vector>
#include <tf2/convert.h>
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

#include <costmap_2d/inflation_layer.h>
#include <costmap_2d/static_layer.h>
#include <costmap_2d/obstacle_layer.h>

#include "rose_thread.hpp"
#include <rose_ros/utils.hpp>
#include <SDL_log.h>
#include "rose_exception.hpp"
#include <rose_ros/kidnap.hpp>

using namespace std;

namespace costmap_2d
{
Costmap2DROS* global_costmap_ros = nullptr;

void move_parameter(ros::NodeHandle& old_h, ros::NodeHandle& new_h, std::string name, bool should_delete = true)
{
  if (!old_h.hasParam(name))
    return;

  XmlRpc::XmlRpcValue value;
  old_h.getParam(name, value);
  new_h.setParam(name, value);
  if (should_delete) old_h.deleteParam(name);
}

Costmap2DROS::Costmap2DROS(ros::CallbackQueue& cbqueue, const std::string& name, tf2_ros::Buffer& tf, bool& exit) :
    layered_costmap_(NULL),
    name_(name),
    tf_(tf),
    transform_tolerance_(0.3),
    map_update_thread_shutdown_(false),
    stop_updates_(false),
    initialized_(true),
    stopped_(false),
    map_update_thread_(NULL),
    last_publish_(0),
    // plugin_loader_("costmap_2d", "costmap_2d::Layer"),
    publisher_(NULL),
    dsrv_(NULL),
    footprint_padding_(ROS_FOOTPRINT_PADDING)
{
  ros::NodeHandle private_nh("~/" + name);
  ros::NodeHandle g_nh;
  private_nh.setCallbackQueue(&cbqueue);
  g_nh.setCallbackQueue(&cbqueue);

  const std::string default_footprint = "[[-0.13, -0.14], [-0.13, 0.14], [0.13, 0.14], [0.13, -0.14]]";
  const bool global_costmap = name == "global_costmap";
  if (name == "global_costmap") {
      // <clbrobot>/param/navigation/global_costmap_params.yaml
      private_nh.setParam("global_frame", "map");
      private_nh.setParam("robot_base_frame", "base_footprint");
      private_nh.setParam("update_frequency", 1.0);
      private_nh.setParam("publish_frequency", 0.5);
      private_nh.setParam("static_map", true);
      private_nh.setParam("transform_tolerance", 0.5);
      private_nh.setParam("cost_scaling_factor", 10.0);
      private_nh.setParam("inflation_radius", 0.55);

      // private_nh.setParam("footprint", default_footprint);
      private_nh.setParam("map_type", "costmap");
      private_nh.setParam("observation_sources", "scan");
      private_nh.setParam("obstacle_range", 2.5);
      private_nh.setParam("raytrace_range", 3.0);
      private_nh.setParam("scan/clearing", true);
      private_nh.setParam("scan/data_type", "LaserScan");
      private_nh.setParam("scan/marking", "true");
      private_nh.setParam("scan/topic", "scan");
      private_nh.setParam("scan/sensor_frame", "laser"); // must set

  } else if (name == "local_costmap") {
      // <clbrobot>/param/navigation/local_costmap_params.yaml
      private_nh.setParam("global_frame", "map"); // odom
      private_nh.setParam("robot_base_frame", "base_footprint");
      private_nh.setParam("update_frequency", 1.0); // #before 5.0
      private_nh.setParam("publish_frequency", 2.0); // #before 2.0
      // private_nh.setParam("static_map", false); // local_costmap don't use static layer
      private_nh.setParam("rolling_window", true);
      private_nh.setParam("width", 2.5); // => 3
      private_nh.setParam("height", 2.5); // => 3
      private_nh.setParam("resolution", 0.05); // #increase to for higher res 0.025
      private_nh.setParam("transform_tolerance", 0.5);
      private_nh.setParam("cost_scaling_factor", 5);
      private_nh.setParam("inflation_radius", 0.55);

      // private_nh.setParam("footprint", default_footprint);
      private_nh.setParam("map_type", "costmap");
      private_nh.setParam("observation_sources", "scan");
      private_nh.setParam("obstacle_range", 2.5);
      private_nh.setParam("raytrace_range", 3.0);
      private_nh.setParam("scan/clearing", true);
      private_nh.setParam("scan/data_type", "LaserScan");
      private_nh.setParam("scan/marking", true);
      private_nh.setParam("scan/topic", "scan");
      private_nh.setParam("scan/sensor_frame", "laser"); // must set
  }
  {
      // [2/2]must not enter free_space.
      private_nh.setParam("track_unknown_space", true);
  }

  // get global and robot base frame names
  private_nh.param("global_frame", global_frame_, std::string("map"));
  private_nh.param("robot_base_frame", robot_base_frame_, std::string("base_link"));

  ros::Time last_error = ros::Time::now();
  std::string tf_error;
  // we need to make sure that the transform between the robot base frame and the global frame is available
  while (ros::ok() && !exit
      && !tf_.canTransform(global_frame_, robot_base_frame_, ros::Time(), ros::Duration(0.1), &tf_error))
  {
    // ros::spinOnce();
    cbqueue.callAvailable(ros::WallDuration());
    if (last_error + ros::Duration(5.0) < ros::Time::now())
    {
      ROS_WARN("Timed out waiting for transform from %s to %s to become available before running costmap, tf error: %s",
               robot_base_frame_.c_str(), global_frame_.c_str(), tf_error.c_str());
      last_error = ros::Time::now();
    }
    // The error string will accumulate and errors will typically be the same, so the last
    // will do for the warning above. Reset the string here to avoid accumulation.
    tf_error.clear();
  }
  if (exit) {
      return;
  }

  // check if we want a rolling window version of the costmap
  bool rolling_window, track_unknown_space, always_send_full_costmap;
  private_nh.param("rolling_window", rolling_window, false);
  private_nh.param("track_unknown_space", track_unknown_space, false);
  private_nh.param("always_send_full_costmap", always_send_full_costmap, false);
  if (name_ == "local_costmap") {
      int ii = 0;
      always_send_full_costmap = true;
  } else {
      rolling_window = false;
  }

  layered_costmap_ = new LayeredCostmap(global_frame_, rolling_window, track_unknown_space);

  // if (!private_nh.hasParam("plugins"))
  if (true)
  {
    loadOldParameters(private_nh);
  } else {
    warnForOldParameters(private_nh);
  }

  if (private_nh.hasParam("plugins"))
  {
    XmlRpc::XmlRpcValue my_list;
    private_nh.getParam("plugins", my_list);
    for (int32_t i = 0; i < my_list.size(); ++i)
    {
      std::string pname = static_cast<std::string>(my_list[i]["name"]);
      std::string type = static_cast<std::string>(my_list[i]["type"]);
      ROS_INFO("%s: Using plugin \"%s\", type: %s", name_.c_str(), pname.c_str(), type.c_str());

      copyParentParameters(pname, type, private_nh);

      // global_costmap: Using plugin "static_layer", type: costmap_2d::StaticLayer
      // global_costmap: Using plugin "obstacle_layer", type: costmap_2d::ObstacleLayer
      // global_costmap: Using plugin "inflation_layer", type: costmap_2d::InflationLayer
      // local_costmap: Using plugin "obstacle_layer", type: costmap_2d::ObstacleLayer
      // local_costmap: Using plugin "inflation_layer", type: costmap_2d::InflationLayer
      // boost::shared_ptr<Layer> plugin = plugin_loader_.createInstance(type);
      boost::shared_ptr<Layer> plugin;
      if (type == "costmap_2d::StaticLayer") {
          plugin.reset(new costmap_2d::StaticLayer(cbqueue, exit));

      } else if (type == "costmap_2d::ObstacleLayer") {
          plugin.reset(new costmap_2d::ObstacleLayer(cbqueue));

      } else if (type == "costmap_2d::InflationLayer") {
          plugin.reset(new costmap_2d::InflationLayer(cbqueue));
      } else {
          // VALIDATE(false, null_str);
      }
      layered_costmap_->addPlugin(plugin);
      plugin->initialize(layered_costmap_, name + "/" + pname, &tf_);
    }
  }

  // subscribe to the footprint topic
  std::string topic_param, topic;
  if (!private_nh.searchParam("footprint_topic", topic_param))
  {
    topic_param = "footprint_topic";
  }

  // rose_ros don't supprot runtime changes to footprint.
  // private_nh.param(topic_param, topic, std::string("footprint"));
  // footprint_sub_ = private_nh.subscribe(topic, 1, &Costmap2DROS::setUnpaddedRobotFootprintPolygon, this);

  if (!private_nh.searchParam("published_footprint_topic", topic_param))
  {
    topic_param = "published_footprint";
  }

  private_nh.param(topic_param, topic, std::string("footprint"));  // TODO: revert to oriented_footprint in N-turtle
  footprint_pub_ = private_nh.advertise<geometry_msgs::PolygonStamped>(topic, 1);

  publisher_ = new Costmap2DPublisher(&private_nh, layered_costmap_->getCostmap(), global_frame_, "costmap",
                                      always_send_full_costmap);

  // create a thread to handle updating the map
  stop_updates_ = false;
  initialized_ = true;
  stopped_ = false;

  ros::NodeHandle tmp_nh("~/" + name);
  tmp_nh.setCallbackQueue(&cbqueue);
  dsrv_ = new dynamic_reconfigure::Server<Costmap2DConfig>(tmp_nh);
  dynamic_reconfigure::Server<Costmap2DConfig>::CallbackType cb = std::bind(&Costmap2DROS::reconfigureCB, this, _1,
                                                                              _2);
  dsrv_->setCallback(cb);

  if (global_costmap) {
    VALIDATE(global_costmap_ros == nullptr, null_str);
    global_costmap_ros = this;
  }
}

void Costmap2DROS::setUnpaddedRobotFootprintPolygon(const geometry_msgs::Polygon& footprint)
{
  setUnpaddedRobotFootprint(toPointVector(footprint));
}

Costmap2DROS::~Costmap2DROS()
{
  const bool global_costmap = name_ == "global_costmap";
  if (global_costmap && global_costmap_ros != nullptr) {
      // VALIDATE(global_costmap_ros != nullptr, null_str);
      global_costmap_ros = nullptr;
  }

  map_update_thread_shutdown_ = true;
  if (map_update_thread_ != NULL)
  {
    // map_update_thread_->join();
    delete map_update_thread_;
  }
  if (publisher_ != NULL)
    delete publisher_;

  delete layered_costmap_;
  delete dsrv_;
}

void Costmap2DROS::loadOldParameters(ros::NodeHandle& nh)
{
  ROS_WARN("%s: Parameter \"plugins\" not provided, loading pre-Hydro parameters", name_.c_str());
  bool flag;
  std::string s;
  std::vector < XmlRpc::XmlRpcValue > plugins;

  XmlRpc::XmlRpcValue::ValueStruct map;
  SuperValue super_map;
  SuperValue super_array;

  if (nh.getParam("static_map", flag) && flag)
  {
    map["name"] = XmlRpc::XmlRpcValue("static_layer");
    map["type"] = XmlRpc::XmlRpcValue("costmap_2d::StaticLayer");
    super_map.setStruct(&map);
    plugins.push_back(super_map);

    ros::NodeHandle map_layer(nh, "static_layer");
    move_parameter(nh, map_layer, "map_topic");
    move_parameter(nh, map_layer, "unknown_cost_value");
    move_parameter(nh, map_layer, "lethal_cost_threshold");
    move_parameter(nh, map_layer, "track_unknown_space", false);
  }

  ros::NodeHandle obstacles(nh, "obstacle_layer");
  if (nh.getParam("map_type", s) && s == "voxel")
  {
    map["name"] = XmlRpc::XmlRpcValue("obstacle_layer");
    map["type"] = XmlRpc::XmlRpcValue("costmap_2d::VoxelLayer");
    super_map.setStruct(&map);
    plugins.push_back(super_map);

    move_parameter(nh, obstacles, "origin_z");
    move_parameter(nh, obstacles, "z_resolution");
    move_parameter(nh, obstacles, "z_voxels");
    move_parameter(nh, obstacles, "mark_threshold");
    move_parameter(nh, obstacles, "unknown_threshold");
    move_parameter(nh, obstacles, "publish_voxel_map");
  }
  else
  {
    map["name"] = XmlRpc::XmlRpcValue("obstacle_layer");
    map["type"] = XmlRpc::XmlRpcValue("costmap_2d::ObstacleLayer");
    super_map.setStruct(&map);
    plugins.push_back(super_map);
  }

  move_parameter(nh, obstacles, "max_obstacle_height");
  move_parameter(nh, obstacles, "raytrace_range");
  move_parameter(nh, obstacles, "obstacle_range");
  move_parameter(nh, obstacles, "track_unknown_space", true);
  nh.param("observation_sources", s, std::string(""));
  std::stringstream ss(s);
  std::string source;
  while (ss >> source)
  {
    move_parameter(nh, obstacles, source);
  }
  move_parameter(nh, obstacles, "observation_sources");

  ros::NodeHandle inflation(nh, "inflation_layer");
  move_parameter(nh, inflation, "cost_scaling_factor");
  move_parameter(nh, inflation, "inflation_radius");
  map["name"] = XmlRpc::XmlRpcValue("inflation_layer");
  map["type"] = XmlRpc::XmlRpcValue("costmap_2d::InflationLayer");
  super_map.setStruct(&map);
  plugins.push_back(super_map);

  super_array.setArray(&plugins);
  nh.setParam("plugins", super_array);
}

void Costmap2DROS::copyParentParameters(const std::string& plugin_name, const std::string& plugin_type, ros::NodeHandle& nh)
{
  ros::NodeHandle target_layer(nh, plugin_name);

  if(plugin_type == "costmap_2d::StaticLayer")
  {
    move_parameter(nh, target_layer, "map_topic", false);
    move_parameter(nh, target_layer, "unknown_cost_value", false);
    move_parameter(nh, target_layer, "lethal_cost_threshold", false);
    move_parameter(nh, target_layer, "track_unknown_space", false);
  }
  else if(plugin_type == "costmap_2d::VoxelLayer")
  {
    move_parameter(nh, target_layer, "origin_z", false);
    move_parameter(nh, target_layer, "z_resolution", false);
    move_parameter(nh, target_layer, "z_voxels", false);
    move_parameter(nh, target_layer, "mark_threshold", false);
    move_parameter(nh, target_layer, "unknown_threshold", false);
    move_parameter(nh, target_layer, "publish_voxel_map", false);
  }
  else if(plugin_type == "costmap_2d::ObstacleLayer")
  {
    move_parameter(nh, target_layer, "max_obstacle_height", false);
    move_parameter(nh, target_layer, "raytrace_range", false);
    move_parameter(nh, target_layer, "obstacle_range", false);
    move_parameter(nh, target_layer, "track_unknown_space", false);
  }
  else if(plugin_type == "costmap_2d::InflationLayer")
  {
    move_parameter(nh, target_layer, "cost_scaling_factor", false);
    move_parameter(nh, target_layer, "inflation_radius", false);
  }
}

void Costmap2DROS::warnForOldParameters(ros::NodeHandle& nh)
{
  checkOldParam(nh, "static_map");
  checkOldParam(nh, "map_type");
}

void Costmap2DROS::checkOldParam(ros::NodeHandle& nh, const std::string &param_name){
  if(nh.hasParam(param_name)){
    ROS_WARN("%s: Pre-Hydro parameter \"%s\" unused since \"plugins\" is provided", name_.c_str(), param_name.c_str());
  }
}

void Costmap2DROS::reconfigureCB(costmap_2d::Costmap2DConfig &config, uint32_t level)
{
  SDL_Log("[%s]Costmap2DROS::reconfigureCB--- level: %u", name_.c_str(), level);
  VALIDATE(map_update_thread_ == nullptr, null_str);

  transform_tolerance_ = config.transform_tolerance;
  if (map_update_thread_ != NULL)
  {
    map_update_thread_shutdown_ = true;
    // map_update_thread_->join();
    delete map_update_thread_;
    map_update_thread_ = NULL;
  }
  map_update_thread_shutdown_ = false;
  double map_update_frequency = config.update_frequency;

  double map_publish_frequency = config.publish_frequency;
  if (map_publish_frequency > 0)
    publish_cycle = ros::Duration(1 / map_publish_frequency);
  else
    publish_cycle = ros::Duration(-1);

  // find size parameters
  double map_width_meters = config.width, map_height_meters = config.height, resolution = config.resolution, origin_x =
             config.origin_x,
         origin_y = config.origin_y;

  if (!layered_costmap_->isSizeLocked())
  {
    layered_costmap_->resizeMap((unsigned int)(map_width_meters / resolution),
                                (unsigned int)(map_height_meters / resolution), resolution, origin_x, origin_y);
  }

  if (!ros::footprint_string.empty() && ros::footprint_string != "[]")
  {
    std::vector<geometry_msgs::Point> new_footprint;
    if (makeFootprintFromString(ros::footprint_string, new_footprint))
    {
        setUnpaddedRobotFootprint(new_footprint);
    }
    else
    {
        ROS_ERROR("Invalid footprint string from dynamic reconfigure");
    }
  }
  else
  {
    // robot_radius may be 0, but that must be intended at this point.
    setUnpaddedRobotFootprint(makeFootprintFromRadius(config.robot_radius));
  }


  old_config_ = config;

  // only construct the thread if the frequency is positive
  if(map_update_frequency > 0.0) {
    // map_update_thread_ = new boost::thread(std::bind(&Costmap2DROS::mapUpdateLoop, this, map_update_frequency));
    map_update_thread_ = create_rose_thread(std::bind(&Costmap2DROS::mapUpdateLoop, this, map_update_frequency), NULL, NULL, NULL, "updateMap");
  }
  SDL_Log("[%s]---Costmap2DROS::reconfigureCB, X level: %u", name_.c_str(), level);
}

void Costmap2DROS::setUnpaddedRobotFootprint(const std::vector<geometry_msgs::Point>& points)
{
  unpadded_footprint_ = points;
  padded_footprint_ = points;
  // padFootprint(padded_footprint_, footprint_padding_, footprint_padding_);
  padFootprint(padded_footprint_, footprint_padding_, 0.02);

  layered_costmap_->setFootprint(padded_footprint_);
}

void Costmap2DROS::mapUpdateLoop(double frequency)
{
  ros::NodeHandle nh;
  ros::Rate r(frequency);
  while (nh.ok() && !map_update_thread_shutdown_)
  {
/*
    #ifdef HAVE_SYS_TIME_H
    struct timeval start, end;
    double start_t, end_t, t_diff;
    gettimeofday(&start, NULL);
    #endif
*/    
    updateMap();
/*
    #ifdef HAVE_SYS_TIME_H
    gettimeofday(&end, NULL);
    start_t = start.tv_sec + double(start.tv_usec) / 1e6;
    end_t = end.tv_sec + double(end.tv_usec) / 1e6;
    t_diff = end_t - start_t;
    ROS_DEBUG("Map update time: %.9f", t_diff);
    #endif
*/

    r.sleep();
    // make sure to sleep for the remainder of our cycle time
    if (r.cycleTime() > ros::Duration(1 / frequency))
      ROS_WARN("Map update loop missed its desired rate of %.4fHz... the loop actually took %.4f seconds", frequency,
               r.cycleTime().toSec());
  }
}

void Costmap2DROS::updateMap()
{
  if (!stop_updates_)
  {
    // get global pose
    geometry_msgs::PoseStamped pose;
    if (getRobotPose (pose))
    {
      double x = pose.pose.position.x,
             y = pose.pose.position.y,
             yaw = tf2::getYaw(pose.pose.orientation);

      if (kidnap.is_allowed() && name_ == "global_costmap") {
          threading::lock lock(kidnap.last_pose2d_mutex);
          kidnap.last_pose2d.set(x, y, yaw, true);
      }
      layered_costmap_->updateMap(x, y, yaw);

      geometry_msgs::PolygonStamped footprint;
      footprint.header.frame_id = global_frame_;
      footprint.header.stamp = ros::Time::now();
      transformFootprint(x, y, yaw, padded_footprint_, footprint);
      footprint_pub_.publish(footprint);

      initialized_ = true;
    }

    if (publish_cycle.toSec() > 0 && layered_costmap_->isInitialized())
    {
      unsigned int x0, y0, xn, yn;
      layered_costmap_->getBounds(&x0, &xn, &y0, &yn);
      publisher_->updateBounds(x0, xn, y0, yn);

      ros::Time now = ros::Time::now();
      if (last_publish_ + publish_cycle < now)
      {
        publisher_->publishCostmap();
        last_publish_ = now;
      }
    }
  }
}

void Costmap2DROS::start(bool& exit)
{
  std::vector < boost::shared_ptr<Layer> > *plugins = layered_costmap_->getPlugins();
  // check if we're stopped or just paused
  if (stopped_)
  {
    // if we're stopped we need to re-subscribe to topics
    for (vector<boost::shared_ptr<Layer> >::iterator plugin = plugins->begin(); plugin != plugins->end();
        ++plugin)
    {
      (*plugin)->activate();
    }
    stopped_ = false;
  }
  stop_updates_ = false;

  // block until the costmap is re-initialized.. meaning one update cycle has run
  // note: this does not hold, if the user has disabled map-updates allgother
  ros::Rate r(100.0);
  while (ros::ok() && !exit && !initialized_ && map_update_thread_)
    r.sleep();
}

void Costmap2DROS::stop()
{
  stop_updates_ = true;
  std::vector < boost::shared_ptr<Layer> > *plugins = layered_costmap_->getPlugins();
  // unsubscribe from topics
  for (vector<boost::shared_ptr<Layer> >::iterator plugin = plugins->begin(); plugin != plugins->end();
      ++plugin)
  {
    (*plugin)->deactivate();
  }
  initialized_ = false;
  stopped_ = true;
}

void Costmap2DROS::pause()
{
  stop_updates_ = true;
  initialized_ = false;
}

void Costmap2DROS::resume()
{
  stop_updates_ = false;

  // block until the costmap is re-initialized.. meaning one update cycle has run
  ros::Rate r(100.0);
  while (!initialized_)
    r.sleep();
}


void Costmap2DROS::clearCostmaps()
{
  Costmap2D* top = layered_costmap_->getCostmap();
  top->resetMap(0, 0, top->getSizeInCellsX(), top->getSizeInCellsY());
  std::vector < boost::shared_ptr<Layer> > *plugins = layered_costmap_->getPlugins();
  for (vector<boost::shared_ptr<Layer> >::iterator plugin = plugins->begin(); plugin != plugins->end();
      ++plugin)
  {
    (*plugin)->clearCostmap();
  }
}

bool Costmap2DROS::getRobotPose(geometry_msgs::PoseStamped& global_pose) const
{
  tf2::toMsg(tf2::Transform::getIdentity(), global_pose.pose);
  geometry_msgs::PoseStamped robot_pose;
  tf2::toMsg(tf2::Transform::getIdentity(), robot_pose.pose);
  robot_pose.header.frame_id = robot_base_frame_;
  robot_pose.header.stamp = ros::Time();
  ros::Time current_time = ros::Time::now();  // save time for checking tf delay later

  // get the global pose of the robot
  try
  {
    // use current time if possible (makes sure it's not in the future)
    if (tf_.canTransform(global_frame_, robot_base_frame_, current_time))
    {
      geometry_msgs::TransformStamped transform = tf_.lookupTransform(global_frame_, robot_base_frame_, current_time);
      tf2::doTransform(robot_pose, global_pose, transform);
    }
    // use the latest otherwise
    else
    {
      tf_.transform(robot_pose, global_pose, global_frame_);
      if (name_ == "local_costmap") {
          int ii = 0;
      }
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
  // check global_pose timeout
  if (!global_pose.header.stamp.isZero() && current_time.toSec() - global_pose.header.stamp.toSec() > transform_tolerance_)
  {
    ROS_WARN_THROTTLE(1.0,
                      "Costmap2DROS transform timeout. Current time: %.4f, global_pose stamp: %.4f, tolerance: %.4f",
                      current_time.toSec(), global_pose.header.stamp.toSec(), transform_tolerance_);
    return false;
  }

  return true;
}

void Costmap2DROS::getOrientedFootprint(std::vector<geometry_msgs::Point>& oriented_footprint) const
{
  geometry_msgs::PoseStamped global_pose;
  if (!getRobotPose(global_pose))
    return;

  double yaw = tf2::getYaw(global_pose.pose.orientation);
  transformFootprint(global_pose.pose.position.x, global_pose.pose.position.y, yaw,
                     padded_footprint_, oriented_footprint);
}

}  // namespace costmap_2d
