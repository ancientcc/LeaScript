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

#include <ros/ros.h>
#include <ros/callback_queue.h>
#include <rose_exception.hpp>

static std::string xml_from_key(ros::NodeHandle& private_nh, const std::string& key) 
{
    XmlRpc::XmlRpcValue original_value;
    private_nh.getParam(key, original_value);
    return original_value.toXml();
}

static void set_param_from_xml(ros::NodeHandle& private_nh, const std::string& key, const std::string& xml)
{
    XmlRpc::XmlRpcValue xml_value;
    // const std::string xml_str = "<value><array><data><value><struct><member><name>joints</name><value><array><data><value>a</value><value>b</value><value>c</value></data></array></value></member><member><name>name</name><value>fake_robot_arm_controller</value></member></struct></value><value><struct><member><name>joints</name><value><array><data><value>d</value><value>f</value></data></array></value></member><member><name>name</name><value>fake_robot_claw_controller</value></member></struct></value><value><struct><member><name>joints</name><value><array><data><value>x</value><value>y</value></data></array></value></member><member><name>name</name><value>fake_robot_pan_controller</value></member></struct></value></data></array></value>";
    int offset = 0;
    xml_value.fromXml(xml, &offset);
    private_nh.setParam(key, xml_value);
}

void rose_set_move_group_init_params(ros::NodeHandle& private_nh)
{
    {
        // private_nh.setParam("default_planning_pipeline", "default");
    }

    private_nh.setParam("allow_trajectory_execution", true);
    // /move_group/capabilities:
    {
        const std::string key = "capabilities";
        const std::string xml = "<value></value>";
        set_param_from_xml(private_nh, key, xml);
    } 
    // /move_group/controller_list: [{'name': 'fake_r...
    {
        const std::string key = "controller_list";
        // const std::string xml = xml_from_key(private_nh, key);
        const std::string xml = "<value><array><data><value><struct><member><name>joints</name><value><array><data><value>a</value><value>b</value><value>c</value></data></array></value></member><member><name>name</name><value>fake_robot_arm_controller</value></member></struct></value><value><struct><member><name>joints</name><value><array><data><value>d</value><value>f</value></data></array></value></member><member><name>name</name><value>fake_robot_claw_controller</value></member></struct></value><value><struct><member><name>joints</name><value><array><data><value>x</value><value>y</value></data></array></value></member><member><name>name</name><value>fake_robot_pan_controller</value></member></struct></value></data></array></value>";
        set_param_from_xml(private_nh, key, xml);
    }

    // /move_group/disable_capabilities:
    {
        const std::string key = "disable_capabilities";
        // const std::string xml = xml_from_key(private_nh, key);
        const std::string xml = "<value></value>";
        set_param_from_xml(private_nh, key, xml);
    }

    private_nh.setParam("jiggle_fraction", 0.05);
    private_nh.setParam("max_range", 5.0);
    private_nh.setParam("max_safe_path_cost", 1);
    private_nh.setParam("moveit_controller_manager", "moveit_fake_controller_manager/MoveItFakeControllerManager");
    private_nh.setParam("moveit_manage_controllers", true);
    private_nh.setParam("octomap_resolution", 0.025);
    private_nh.setParam("planner_configs/BFMT/balanced", 0);
    private_nh.setParam("planner_configs/BFMT/cache_cc", 1);
    private_nh.setParam("planner_configs/BFMT/extended_fmt", 1);
    private_nh.setParam("planner_configs/BFMT/heuristics", 1);
    private_nh.setParam("planner_configs/BFMT/nearest_k", 1);
    private_nh.setParam("planner_configs/BFMT/num_samples", 1000);
    private_nh.setParam("planner_configs/BFMT/optimality", 1);
    private_nh.setParam("planner_configs/BFMT/radius_multiplier", 1.0);
    private_nh.setParam("planner_configs/BFMT/type", "geometric::BFMT");
    private_nh.setParam("planner_configs/BKPIECE/border_fraction", 0.9);
    private_nh.setParam("planner_configs/BKPIECE/failed_expansion_score_factor", 0.5);
    private_nh.setParam("planner_configs/BKPIECE/min_valid_path_fraction", 0.5);
    private_nh.setParam("planner_configs/BKPIECE/range", 0.0);
    private_nh.setParam("planner_configs/BKPIECE/type", "geometric::BKPIECE");
    private_nh.setParam("planner_configs/BiEST/range", 0.0);
    private_nh.setParam("planner_configs/BiEST/type", "geometric::BiEST");
    private_nh.setParam("planner_configs/BiTRRT/cost_threshold", 1e300);
    private_nh.setParam("planner_configs/BiTRRT/frountier_node_ratio", 0.1);
    private_nh.setParam("planner_configs/BiTRRT/frountier_threshold", 0.0);
    private_nh.setParam("planner_configs/BiTRRT/init_temperature", 100);
    private_nh.setParam("planner_configs/BiTRRT/range", 0.0);
    private_nh.setParam("planner_configs/BiTRRT/temp_change_factor", 0.1);
    private_nh.setParam("planner_configs/BiTRRT/type", "geometric::BiTRRT");
    private_nh.setParam("planner_configs/EST/goal_bias", 0.05);
    private_nh.setParam("planner_configs/EST/range", 0.0);
    private_nh.setParam("planner_configs/EST/type", "geometric::EST");
    private_nh.setParam("planner_configs/FMT/cache_cc", 1);
    private_nh.setParam("planner_configs/FMT/extended_fmt", 1);
    private_nh.setParam("planner_configs/FMT/heuristics", 0);
    private_nh.setParam("planner_configs/FMT/nearest_k", 1);
    private_nh.setParam("planner_configs/FMT/num_samples", 1000);
    private_nh.setParam("planner_configs/FMT/radius_multiplier", 1.1);
    private_nh.setParam("planner_configs/FMT/type", "geometric::FMT");
    private_nh.setParam("planner_configs/KPIECE/border_fraction", 0.9);
    private_nh.setParam("planner_configs/KPIECE/failed_expansion_score_factor", 0.5);
    private_nh.setParam("planner_configs/KPIECE/goal_bias", 0.05);
    private_nh.setParam("planner_configs/KPIECE/min_valid_path_fraction", 0.5);
    private_nh.setParam("planner_configs/KPIECE/range", 0.0);
    private_nh.setParam("planner_configs/KPIECE/type", "geometric::KPIECE");
    private_nh.setParam("planner_configs/LBKPIECE/border_fraction", 0.9);
    private_nh.setParam("planner_configs/LBKPIECE/min_valid_path_fraction", 0.5);
    private_nh.setParam("planner_configs/LBKPIECE/range", 0.0);
    private_nh.setParam("planner_configs/LBKPIECE/type", "geometric::LBKPIECE");
    private_nh.setParam("planner_configs/LBTRRT/epsilon", 0.4);
    private_nh.setParam("planner_configs/LBTRRT/goal_bias", 0.05);
    private_nh.setParam("planner_configs/LBTRRT/range", 0.0);
    private_nh.setParam("planner_configs/LBTRRT/type", "geometric::LBTRRT");
    private_nh.setParam("planner_configs/LazyPRM/range", 0.0);
    private_nh.setParam("planner_configs/LazyPRM/type", "geometric::LazyPRM");
    private_nh.setParam("planner_configs/LazyPRMstar/type", "geometric::LazyPRMstar");
    private_nh.setParam("planner_configs/PDST/type", "geometric::PDST");
    private_nh.setParam("planner_configs/PRM/max_nearest_neighbors", 10);
    private_nh.setParam("planner_configs/PRM/type", "geometric::PRM");
    private_nh.setParam("planner_configs/PRMstar/type", "geometric::PRMstar");
    private_nh.setParam("planner_configs/ProjEST/goal_bias", 0.05);
    private_nh.setParam("planner_configs/ProjEST/range", 0.0);
    private_nh.setParam("planner_configs/ProjEST/type", "geometric::ProjEST");
    private_nh.setParam("planner_configs/RRT/goal_bias", 0.05);
    private_nh.setParam("planner_configs/RRT/range", 0.0);
    private_nh.setParam("planner_configs/RRT/type", "geometric::RRT");
    private_nh.setParam("planner_configs/RRTConnect/range", 0.0);
    private_nh.setParam("planner_configs/RRTConnect/type", "geometric::RRTConnect");
    private_nh.setParam("planner_configs/RRTstar/delay_collision_checking", 1);
    private_nh.setParam("planner_configs/RRTstar/goal_bias", 0.05);
    private_nh.setParam("planner_configs/RRTstar/range", 0.0);
    private_nh.setParam("planner_configs/RRTstar/type", "geometric::RRTstar");
    private_nh.setParam("planner_configs/SBL/range", 0.0);
    private_nh.setParam("planner_configs/SBL/type", "geometric::SBL");
    private_nh.setParam("planner_configs/SPARS/dense_delta_fraction", 0.001);
    private_nh.setParam("planner_configs/SPARS/max_failures", 1000);
    private_nh.setParam("planner_configs/SPARS/sparse_delta_fraction", 0.25);
    private_nh.setParam("planner_configs/SPARS/stretch_factor", 3.0);
    private_nh.setParam("planner_configs/SPARS/type", "geometric::SPARS");
    private_nh.setParam("planner_configs/SPARStwo/dense_delta_fraction", 0.001);
    private_nh.setParam("planner_configs/SPARStwo/max_failures", 5000);
    private_nh.setParam("planner_configs/SPARStwo/sparse_delta_fraction", 0.25);
    private_nh.setParam("planner_configs/SPARStwo/stretch_factor", 3.0);
    private_nh.setParam("planner_configs/SPARStwo/type", "geometric::SPARStwo");
    private_nh.setParam("planner_configs/STRIDE/degree", 16);
    private_nh.setParam("planner_configs/STRIDE/estimated_dimension", 0.0);
    private_nh.setParam("planner_configs/STRIDE/goal_bias", 0.05);
    private_nh.setParam("planner_configs/STRIDE/max_degree", 18);
    private_nh.setParam("planner_configs/STRIDE/max_pts_per_leaf", 6);
    private_nh.setParam("planner_configs/STRIDE/min_degree", 12);
    private_nh.setParam("planner_configs/STRIDE/min_valid_path_fraction", 0.2);
    private_nh.setParam("planner_configs/STRIDE/range", 0.0);
    private_nh.setParam("planner_configs/STRIDE/type", "geometric::STRIDE");
    private_nh.setParam("planner_configs/STRIDE/use_projected_distance", 0);
    private_nh.setParam("planner_configs/TRRT/frountierNodeRatio", 0.1);
    private_nh.setParam("planner_configs/TRRT/frountier_threshold", 0.0);
    private_nh.setParam("planner_configs/TRRT/goal_bias", 0.05);
    private_nh.setParam("planner_configs/TRRT/init_temperature", 10e-6);
    private_nh.setParam("planner_configs/TRRT/k_constant", 0.0);
    private_nh.setParam("planner_configs/TRRT/max_states_failed", 10);
    private_nh.setParam("planner_configs/TRRT/min_temperature", 10e-10);
    private_nh.setParam("planner_configs/TRRT/range", 0.0);
    private_nh.setParam("planner_configs/TRRT/temp_change_factor", 2.0);
    private_nh.setParam("planner_configs/TRRT/type", "geometric::TRRT");
    private_nh.setParam("planning_plugin", "ompl_interface/OMPLPlanner");
    private_nh.setParam("planning_scene_monitor/publish_geometry_updates", true);
    private_nh.setParam("planning_scene_monitor/publish_planning_scene", true);
    private_nh.setParam("planning_scene_monitor/publish_state_updates", true);
    private_nh.setParam("planning_scene_monitor/publish_transforms_updates", true);
    // /move_group/request_adapters", default_planner_r...
    {
        const std::string key = "request_adapters";
        // const std::string xml = xml_from_key(private_nh, key);
        // const std::string xml = "<value>default_planner_request_adapters/AddTimeParameterization            default_planner_request_adapters/FixWorkspaceBounds            default_planner_request_adapters/FixStartStateBounds            default_planner_request_adapters/FixStartStateCollision            default_planner_request_adapters/FixStartStatePathConstraints</value>";
        // const std::string xml = "<value>default_planner_request_adapters/AddTimeParameterization</value>";
        const std::string xml = "<value>default_planner_request_adapters/AddIterativeSplineParameterization</value>";
        set_param_from_xml(private_nh, key, xml);
    }
    private_nh.setParam("robot_arm/default_planner_config", "None");
    private_nh.setParam("robot_arm/longest_valid_segment_fraction", 0.005);
    // /move_group/robot_arm/planner_configs, ['SBL', 'EST', 'L...
    {
        const std::string key = "robot_arm/planner_configs";
        // const std::string xml = xml_from_key(private_nh, key);
        const std::string xml = "<value><array><data><value>SBL</value><value>EST</value><value>LBKPIECE</value><value>BKPIECE</value><value>KPIECE</value><value>RRT</value><value>RRTConnect</value><value>RRTstar</value><value>TRRT</value><value>PRM</value><value>PRMstar</value><value>FMT</value><value>BFMT</value><value>PDST</value><value>STRIDE</value><value>BiTRRT</value><value>LBTRRT</value><value>BiEST</value><value>ProjEST</value><value>LazyPRM</value><value>LazyPRMstar</value><value>SPARS</value><value>SPARStwo</value></data></array></value>";
        set_param_from_xml(private_nh, key, xml);
    }
    private_nh.setParam("robot_arm/projection_evaluator", "joints(a,b)");
    // /move_group/robot_claw/default_planner_config",
    {
        const std::string key = "robot_claw/default_planner_config";
        // const std::string xml = xml_from_key(private_nh, key);
        const std::string xml = "<value></value>";
        set_param_from_xml(private_nh, key, xml);
    }
    // /move_group/robot_claw/planner_configs, ['SBL', 'EST', 'L...
    {
        const std::string key = "robot_claw/planner_configs";
        // const std::string xml = xml_from_key(private_nh, key);
        const std::string xml = "<value><array><data><value>SBL</value><value>EST</value><value>LBKPIECE</value><value>BKPIECE</value><value>KPIECE</value><value>RRT</value><value>RRTConnect</value><value>RRTstar</value><value>TRRT</value><value>PRM</value><value>PRMstar</value><value>FMT</value><value>BFMT</value><value>PDST</value><value>STRIDE</value><value>BiTRRT</value><value>LBTRRT</value><value>BiEST</value><value>ProjEST</value><value>LazyPRM</value><value>LazyPRMstar</value><value>SPARS</value><value>SPARStwo</value></data></array></value>";
        set_param_from_xml(private_nh, key, xml);
    }
    private_nh.setParam("robot_pan/default_planner_config", "None");
    private_nh.setParam("robot_pan/longest_valid_segment_fraction", 0.005);
    // /move_group/robot_pan/planner_configs", ['SBL', 'EST', 'L...
    {
        const std::string key = "robot_pan/planner_configs";
        // const std::string xml = xml_from_key(private_nh, key);
        const std::string xml = "<value><array><data><value>SBL</value><value>EST</value><value>LBKPIECE</value><value>BKPIECE</value><value>KPIECE</value><value>RRT</value><value>RRTConnect</value><value>RRTstar</value><value>TRRT</value><value>PRM</value><value>PRMstar</value><value>FMT</value><value>BFMT</value><value>PDST</value><value>STRIDE</value><value>BiTRRT</value><value>LBTRRT</value><value>BiEST</value><value>ProjEST</value><value>LazyPRM</value><value>LazyPRMstar</value><value>SPARS</value><value>SPARStwo</value></data></array></value>";
        set_param_from_xml(private_nh, key, xml);
    }
    private_nh.setParam("robot_pan/projection_evaluator", "joints(x,y)");
    // /move_group/sensors: [{'filtered_cloud...
    {
        const std::string key = "sensors";
        // const std::string xml = xml_from_key(private_nh, key);
        const std::string xml = "<value><array><data><value><struct><member><name>filtered_cloud_topic</name><value>filtered_cloud</value></member><member><name>max_range</name><value><double>5</double></value></member><member><name>max_update_rate</name><value><double>1</double></value></member><member><name>padding_offset</name><value><double>0.10000000000000001</double></value></member><member><name>padding_scale</name><value><double>1</double></value></member><member><name>point_cloud_topic</name><value>/head_mount_kinect/depth_registered/points</value></member><member><name>point_subsample</name><value><i4>1</i4></value></member><member><name>sensor_plugin</name><value>occupancy_map_monitor/PointCloudOctomapUpdater</value></member></struct></value></data></array></value>";
        set_param_from_xml(private_nh, key, xml);
    }
    private_nh.setParam("start_state_max_bounds_error", 0.1);
    private_nh.setParam("trajectory_execution/allowed_execution_duration_scaling", 1.2);
    private_nh.setParam("trajectory_execution/allowed_goal_duration_margin", 0.5);
    private_nh.setParam("trajectory_execution/allowed_start_tolerance", 0.01);

    ros::NodeHandle nh;

    // /robot_description: <...>
    nh.setParam("/robot_description_kinematics/robot_arm/kinematics_solver", "kdl_kinematics_plugin/KDLKinematicsPlugin");
    nh.setParam("/robot_description_kinematics/robot_arm/kinematics_solver_search_resolution", 0.005);
    nh.setParam("/robot_description_kinematics/robot_arm/kinematics_solver_timeout", 0.2);
    nh.setParam("/robot_description_kinematics/robot_pan/kinematics_solver", "kdl_kinematics_plugin/KDLKinematicsPlugin");
    nh.setParam("/robot_description_kinematics/robot_pan/kinematics_solver_search_resolution", 0.005);
    nh.setParam("/robot_description_kinematics/robot_pan/kinematics_solver_timeout", 0.2);
    nh.setParam("/robot_description_planning/joint_limits/a/has_acceleration_limits", false);
    nh.setParam("/robot_description_planning/joint_limits/a/has_velocity_limits", true);
    nh.setParam("/robot_description_planning/joint_limits/a/max_acceleration", 0);
    nh.setParam("/robot_description_planning/joint_limits/a/max_velocity", 2);
    nh.setParam("/robot_description_planning/joint_limits/b/has_acceleration_limits", false);
    nh.setParam("/robot_description_planning/joint_limits/b/has_velocity_limits", true);
    nh.setParam("/robot_description_planning/joint_limits/b/max_acceleration", 0);
    nh.setParam("/robot_description_planning/joint_limits/b/max_velocity", 2);
    nh.setParam("/robot_description_planning/joint_limits/c/has_acceleration_limits", false);
    nh.setParam("/robot_description_planning/joint_limits/c/has_velocity_limits", true);
    nh.setParam("/robot_description_planning/joint_limits/c/max_acceleration", 0);
    nh.setParam("/robot_description_planning/joint_limits/c/max_velocity", 2);
    nh.setParam("/robot_description_planning/joint_limits/d/has_acceleration_limits", false);
    nh.setParam("/robot_description_planning/joint_limits/d/has_velocity_limits", true);
    nh.setParam("/robot_description_planning/joint_limits/d/max_acceleration", 0);
    nh.setParam("/robot_description_planning/joint_limits/d/max_velocity", 2);
    nh.setParam("/robot_description_planning/joint_limits/f/has_acceleration_limits", false);
    nh.setParam("/robot_description_planning/joint_limits/f/has_velocity_limits", true);
    nh.setParam("/robot_description_planning/joint_limits/f/max_acceleration", 0);
    nh.setParam("/robot_description_planning/joint_limits/f/max_velocity", 2);
    nh.setParam("/robot_description_planning/joint_limits/x/has_acceleration_limits", false);
    nh.setParam("/robot_description_planning/joint_limits/x/has_velocity_limits", true);
    nh.setParam("/robot_description_planning/joint_limits/x/max_acceleration", 0);
    nh.setParam("/robot_description_planning/joint_limits/x/max_velocity", 2);
    nh.setParam("/robot_description_planning/joint_limits/y/has_acceleration_limits", false);
    nh.setParam("/robot_description_planning/joint_limits/y/has_velocity_limits", true);
    nh.setParam("/robot_description_planning/joint_limits/y/max_acceleration", 0);
    nh.setParam("/robot_description_planning/joint_limits/y/max_velocity", 2);
    // /robot_description_semantic: <?xml version="1....
}