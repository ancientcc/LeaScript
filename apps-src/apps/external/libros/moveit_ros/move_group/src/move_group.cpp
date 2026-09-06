/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2012, Willow Garage, Inc.
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
 *   * Neither the name of Willow Garage nor the names of its
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
 *********************************************************************/

/* Author: Ioan Sucan */

#include <moveit/moveit_cpp/moveit_cpp.h>
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>
#include <tf2_ros/transform_listener.h>
#include <moveit/move_group/capability_names.h>
#include <moveit/move_group/move_group_capability.h>
#include <boost/tokenizer.hpp>
#include <moveit/macros/console_colors.h>
#include <moveit/move_group/node_name.h>
#include <memory>
#include <set>

#include "default_capabilities/apply_planning_scene_service_capability.h"
#include "default_capabilities/cartesian_path_service_capability.h"
#include "default_capabilities/clear_octomap_service_capability.h"
#include "default_capabilities/execute_trajectory_action_capability.h"
// #include "default_capabilities/execute_trajectory_service_capability.h"
#include "default_capabilities/get_planning_scene_service_capability.h"
#include "default_capabilities/kinematics_service_capability.h"
#include "default_capabilities/move_action_capability.h"
#include "default_capabilities/plan_service_capability.h"
#include "default_capabilities/query_planners_service_capability.h"
#include "default_capabilities/state_validation_service_capability.h"
#include <moveit/move_group_pick_place_capability/pick_place_action_capability.h>

static const std::string ROBOT_DESCRIPTION =
    "robot_description";  // name of the robot description (a param name, so it can be changed externally)

namespace move_group
{
// These capabilities are loaded unless listed in disable_capabilities
// clang-format off
static const char* DEFAULT_CAPABILITIES[] = {
   "move_group/MoveGroupCartesianPathService",
   "move_group/MoveGroupKinematicsService",
   "move_group/MoveGroupExecuteTrajectoryAction",
   "move_group/MoveGroupMoveAction",
   "move_group/MoveGroupPickPlaceAction",
   "move_group/MoveGroupPlanService",
   "move_group/MoveGroupQueryPlannersService",
   "move_group/MoveGroupStateValidationService",
   "move_group/MoveGroupGetPlanningSceneService",
   "move_group/ApplyPlanningSceneService",
   "move_group/ClearOctomapService",
};
// clang-format on

class MoveGroupExe
{
public:
  MoveGroupExe(ros::CallbackQueue& cbqueue, const moveit_cpp::MoveItCppPtr& moveit_cpp, const std::string& default_planning_pipeline, bool debug)
    : node_handle_("~")
    , cbqueue_(cbqueue)
  {
    node_handle_.setCallbackQueue(&cbqueue);
    // if the user wants to be able to disable execution of paths, they can just set this ROS param to false
    bool allow_trajectory_execution;
    node_handle_.param("allow_trajectory_execution", allow_trajectory_execution, true);

    context_ =
        std::make_shared<MoveGroupContext>(cbqueue, moveit_cpp, default_planning_pipeline, allow_trajectory_execution, debug);

    // start the capabilities
    configureCapabilities();
  }

  ~MoveGroupExe()
  {
    capabilities_.clear();
    context_.reset();
    // capability_plugin_loader_.reset();
  }

  void status()
  {
    if (context_)
    {
      if (context_->status())
      {
        if (capabilities_.empty())
          printf(MOVEIT_CONSOLE_COLOR_BLUE "\nmove_group is running but no capabilities are "
                                           "loaded.\n\n" MOVEIT_CONSOLE_COLOR_RESET);
        else
          printf(MOVEIT_CONSOLE_COLOR_GREEN "\nYou can start planning now!\n\n" MOVEIT_CONSOLE_COLOR_RESET);
        fflush(stdout);
      }
    }
    else
      ROS_ERROR("No MoveGroup context created. Nothing will work.");
  }

private:
  void configureCapabilities()
  {
/*
    try
    {
      capability_plugin_loader_ = std::make_shared<pluginlib::ClassLoader<MoveGroupCapability>>(
          "moveit_ros_move_group", "move_group::MoveGroupCapability");
    }
    catch (pluginlib::PluginlibException& ex)
    {
      ROS_FATAL_STREAM("Exception while creating plugin loader for move_group capabilities: " << ex.what());
      return;
    }
*/
    std::set<std::string> capabilities;

    // add default capabilities
    for (const char* capability : DEFAULT_CAPABILITIES)
      capabilities.insert(capability);

    // add capabilities listed in ROS parameter
    std::string capability_plugins;
    if (node_handle_.getParam("capabilities", capability_plugins))
    {
      boost::char_separator<char> sep(" ");
      boost::tokenizer<boost::char_separator<char>> tok(capability_plugins, sep);
      capabilities.insert(tok.begin(), tok.end());
    }

    // add capabilities configured for planning pipelines
    for (const auto& pipeline_entry : context_->moveit_cpp_->getPlanningPipelines())
    {
      const auto& pipeline_name = pipeline_entry.first;
      std::string pipeline_capabilities;
      if (node_handle_.getParam("planning_pipelines/" + pipeline_name + "/capabilities", pipeline_capabilities))
      {
        boost::char_separator<char> sep(" ");
        boost::tokenizer<boost::char_separator<char>> tok(pipeline_capabilities, sep);
        capabilities.insert(tok.begin(), tok.end());
      }
    }

    // drop capabilities that have been explicitly disabled
    if (node_handle_.getParam("disable_capabilities", capability_plugins))
    {
      boost::char_separator<char> sep(" ");
      boost::tokenizer<boost::char_separator<char>> tok(capability_plugins, sep);
      for (boost::tokenizer<boost::char_separator<char>>::iterator cap_name = tok.begin(); cap_name != tok.end();
           ++cap_name)
        capabilities.erase(*cap_name);
    }

    for (const std::string& capability : capabilities)
    {
      // try
      {
        ROS_INFO(MOVEIT_CONSOLE_COLOR_CYAN "Loading '%s'..." MOVEIT_CONSOLE_COLOR_RESET, capability.c_str());
        MoveGroupCapabilityPtr cap;
        if (capability == "move_group/MoveGroupCartesianPathService") {
            cap = std::make_shared<move_group::MoveGroupCartesianPathService>(cbqueue_);

        } else if (capability == "move_group/MoveGroupKinematicsService") {
            cap = std::make_shared<move_group::MoveGroupKinematicsService>(cbqueue_);

        } else if (capability == "move_group/MoveGroupExecuteTrajectoryAction") {
            cap = std::make_shared<move_group::MoveGroupExecuteTrajectoryAction>(cbqueue_);

        } else if (capability == "move_group/MoveGroupMoveAction") {
            cap = std::make_shared<move_group::MoveGroupMoveAction>(cbqueue_);

        } else if (capability == "move_group/MoveGroupPickPlaceAction") {
            cap = std::make_shared<move_group::MoveGroupPickPlaceAction>(cbqueue_);

        } else if (capability == "move_group/MoveGroupPlanService") {
            cap = std::make_shared<move_group::MoveGroupPlanService>(cbqueue_);

        } else if (capability == "move_group/MoveGroupQueryPlannersService") {
            cap = std::make_shared<move_group::MoveGroupQueryPlannersService>(cbqueue_);
            
        } else if (capability == "move_group/MoveGroupStateValidationService") {
            cap = std::make_shared<move_group::MoveGroupStateValidationService>(cbqueue_);

        } else if (capability == "move_group/MoveGroupGetPlanningSceneService") {
            cap = std::make_shared<move_group::MoveGroupGetPlanningSceneService>(cbqueue_);

        } else if (capability == "move_group/ApplyPlanningSceneService") {
            cap = std::make_shared<move_group::ApplyPlanningSceneService>(cbqueue_);

        } else if (capability == "move_group/ClearOctomapService") {
            cap = std::make_shared<move_group::ClearOctomapService>(cbqueue_);
        } else {
            int ii = 0;
        }
        cap->setContext(context_);
        cap->initialize();
        capabilities_.push_back(cap);
      }
      // catch (pluginlib::PluginlibException& ex)
      // {
      //  ROS_ERROR_STREAM("Exception while loading move_group capability '" << capability << "': " << ex.what());
      // }
    }

    std::stringstream ss;
    ss << std::endl;
    ss << std::endl;
    ss << "********************************************************" << std::endl;
    ss << "* MoveGroup using: " << std::endl;
    for (const MoveGroupCapabilityPtr& cap : capabilities_)
      ss << "*     - " << cap->getName() << std::endl;
    ss << "********************************************************" << std::endl;
    ROS_INFO_STREAM(ss.str());
  }

  ros::NodeHandle node_handle_;
  ros::CallbackQueueInterface& cbqueue_;
  MoveGroupContextPtr context_;
  // std::shared_ptr<pluginlib::ClassLoader<MoveGroupCapability>> capability_plugin_loader_;
  std::vector<MoveGroupCapabilityPtr> capabilities_;
};
}  // namespace move_group

extern void rose_set_move_group_init_params(ros::NodeHandle& private_nh);

// int main(int argc, char** argv)
MOVEIT_DECL int moveit_ros_move_group__move_group(bool& exit)
{
  int argc = 0;
  ros::init(argc, nullptr, move_group::NODE_NAME);
  // ros::init(argc, argv, move_group::NODE_NAME);

  ros::CallbackQueue cbqueue;
  ros::AsyncSpinner spinner(1, &cbqueue);
  spinner.start();

  // Load MoveItCpp parameters and check for valid planning pipeline configuration
  ros::NodeHandle pnh("~");
  pnh.setCallbackQueue(&cbqueue);
  moveit_cpp::MoveItCpp::Options moveit_cpp_options(pnh);

  // if (ros::nocopy_intra) {
      rose_set_move_group_init_params(pnh);
  // }

  // Prepare PlanningPipelineOptions
  moveit_cpp_options.planning_pipeline_options.parent_namespace = pnh.getNamespace() + "/planning_pipelines";
  XmlRpc::XmlRpcValue planning_pipeline_configs;
  if (pnh.getParam("planning_pipelines", planning_pipeline_configs))
  {
    if (planning_pipeline_configs.getType() != XmlRpc::XmlRpcValue::TypeStruct)
    {
      ROS_ERROR_STREAM("Parameter '" << moveit_cpp_options.planning_pipeline_options.parent_namespace
                                     << "' is expected to be a dictionary of pipeline configurations.");
    }
    else
    {
      for (std::pair<const std::string, XmlRpc::XmlRpcValue>& config : planning_pipeline_configs)
      {
        if (config.second.getType() != XmlRpc::XmlRpcValue::TypeStruct ||
            std::find_if(config.second.begin(), config.second.end(),
                         [](std::pair<const std::string, XmlRpc::XmlRpcValue>& item) {
                           return item.first == "planning_plugin";
                         }) == config.second.end())
          ROS_ERROR_STREAM("Planning pipeline parameter '~planning_pipelines/" << config.first
                                                                               << "/planning_plugin' doesn't exist");
        else
          moveit_cpp_options.planning_pipeline_options.pipeline_names.push_back(config.first);
      }
      if (planning_pipeline_configs.size() && moveit_cpp_options.planning_pipeline_options.pipeline_names.empty())
        ROS_WARN_STREAM("No valid planning pipelines found in "
                        << moveit_cpp_options.planning_pipeline_options.parent_namespace
                        << ". Did you use a namespace, e.g. planning_pipelines/ompl/ ?");
    }
  }

  // Retrieve default planning pipeline
  auto& pipeline_names = moveit_cpp_options.planning_pipeline_options.pipeline_names;
  std::string default_planning_pipeline;
  if (pnh.getParam("default_planning_pipeline", default_planning_pipeline))
  {
    // Ignore default_planning_pipeline if there is no matching entry in pipeline_names
    if (std::find(pipeline_names.begin(), pipeline_names.end(), default_planning_pipeline) == pipeline_names.end())
    {
      ROS_WARN("MoveGroup launched with ~default_planning_pipeline '%s' not configured in ~planning_pipelines",
               default_planning_pipeline.c_str());
      default_planning_pipeline = "";  // reset invalid pipeline id
    }
  }
  else if (pipeline_names.size() > 1)  // only warn if there are multiple pipelines to choose from
  {
    // Handle deprecated move_group.launch
    ROS_WARN("MoveGroup launched without ~default_planning_pipeline specifying the namespace for the default "
             "planning pipeline configuration");
  }

  // If there is no valid default pipeline, either pick the first available one, or fall back to old behavior
  if (default_planning_pipeline.empty())
  {
    if (!pipeline_names.empty())
    {
      ROS_WARN("Using default pipeline '%s'", pipeline_names[0].c_str());
      default_planning_pipeline = pipeline_names[0];
    }
    else
    {
      ROS_WARN("Falling back to using the move_group node's namespace (deprecated Melodic behavior).");
      moveit_cpp_options.planning_pipeline_options.pipeline_names = { default_planning_pipeline };
      moveit_cpp_options.planning_pipeline_options.parent_namespace = pnh.getNamespace();
    }

    // Reset invalid pipeline parameter for MGI requests
    pnh.setParam("default_planning_pipeline", default_planning_pipeline);
  }

  // Initialize MoveItCpp
  const auto tf_buffer = std::make_shared<tf2_ros::Buffer>(ros::Duration(10.0));
  const auto moveit_cpp = std::make_shared<moveit_cpp::MoveItCpp>(moveit_cpp_options, pnh, tf_buffer);
  const auto planning_scene_monitor = moveit_cpp->getPlanningSceneMonitor();

  if (planning_scene_monitor->getPlanningScene())
  {
    bool debug = false;
/*
    for (int i = 1; i < argc; ++i)
      if (strncmp(argv[i], "--debug", 7) == 0)
      {
        debug = true;
        break;
      }
*/
    if (debug)
      ROS_INFO("MoveGroup debug mode is ON");
    else
      ROS_INFO("MoveGroup debug mode is OFF");

    move_group::MoveGroupExe mge(cbqueue, moveit_cpp, default_planning_pipeline, debug);

    planning_scene_monitor->publishDebugInformation(debug);

    mge.status();

    while (!exit) {
        ros::WallDuration(0.05).sleep();
    }

    // ros::waitForShutdown();
  }
  else
    ROS_ERROR("Planning scene not configured");

  return 0;
}
