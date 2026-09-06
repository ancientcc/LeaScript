/*
 * Copyright 2016 The Cartographer Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "absl/memory/memory.h"
#include "cartographer/mapping/map_builder.h"
#include "cartographer_ros/node.h"
#include "cartographer_ros/node_options.h"
#include "cartographer_ros/ros_log_sink.h"
// #include "gflags/gflags.h"
#include "tf2_ros/transform_listener.h"

#include "rose_config.hpp"
#include "rose_ros/cartographer_utils.h"
#include <SDL_timer.h>

/*
DEFINE_bool(collect_metrics, false,
            "Activates the collection of runtime metrics. If activated, the "
            "metrics can be accessed via a ROS service.");
DEFINE_string(configuration_directory, "",
              "First directory in which configuration files are searched, "
              "second is always the Cartographer installation to allow "
              "including files from there.");
DEFINE_string(configuration_basename, "",
              "Basename, i.e. not containing any directory prefix, of the "
              "configuration file.");
DEFINE_string(load_state_filename, "",
              "If non-empty, filename of a .pbstream file to load, containing "
              "a saved SLAM state.");
DEFINE_bool(load_frozen_state, true,
            "Load the saved state as frozen (non-optimized) trajectories.");
DEFINE_bool(
    start_trajectory_with_default_topics, true,
    "Enable to immediately start the first trajectory with default topics.");
DEFINE_string(
    save_state_filename, "",
    "If non-empty, serialize state and write it to disk before shutting down.");
*/
namespace fLB {
  bool FLAGS_collect_metrics = false;
  bool FLAGS_load_frozen_state = true;
  bool FLAGS_start_trajectory_with_default_topics = true;
}
using fLB::FLAGS_collect_metrics;
using fLB::FLAGS_load_frozen_state;
using fLB::FLAGS_start_trajectory_with_default_topics;

namespace fLS {
  std::string FLAGS_configuration_directory = "";
  std::string FLAGS_configuration_basename = "";
  std::string FLAGS_load_state_filename = "";
  std::string FLAGS_save_state_filename = "";
}
using fLS::FLAGS_configuration_directory;
using fLS::FLAGS_configuration_basename;
using fLS::FLAGS_load_state_filename;
using fLS::FLAGS_save_state_filename;

namespace cartographer_ros {
namespace {

void Run(bool& exit) {
  ros::CallbackQueue cbqueue;
  constexpr double kTfBufferCacheTimeInSeconds = 10.;
  tf2_ros::Buffer tf_buffer{::ros::Duration(kTfBufferCacheTimeInSeconds)};
  tf2_ros::TransformListener tf(tf_buffer);
  NodeOptions node_options;
  TrajectoryOptions trajectory_options;
  std::tie(node_options, trajectory_options) =
      LoadOptions(FLAGS_configuration_directory, FLAGS_configuration_basename);

  auto map_builder =
      cartographer::mapping::CreateMapBuilder(node_options.map_builder_options);
  Node node(cbqueue, node_options, std::move(map_builder), &tf_buffer,
            FLAGS_collect_metrics);
  if (!FLAGS_load_state_filename.empty()) {
    node.LoadState(FLAGS_load_state_filename, FLAGS_load_frozen_state);
  }

  if (FLAGS_start_trajectory_with_default_topics) {
    node.StartTrajectoryWithDefaultTopics(trajectory_options);
  }
  SDL_Log("%u {dbg_stop_slow}pre 'while (!exit & ros::ok())'", SDL_GetTicks());

  // ::ros::spin();

  ros::WallDuration timeout(0.1f);
  while (!exit & ros::ok()) {
	cbqueue.callAvailable(timeout);
  }

  SDL_Log("%u {dbg_stop_slow}pre node.FinishAllTrajectories()", SDL_GetTicks());

  node.FinishAllTrajectories();

  SDL_Log("%u {dbg_stop_slow}pre node.RunFinalOptimization()", SDL_GetTicks());
  node.RunFinalOptimization();

  if (!FLAGS_save_state_filename.empty()) {
    SDL_Log("%u {dbg_stop_slow}pre node.SerializeState(...)", SDL_GetTicks());
    node.SerializeState(FLAGS_save_state_filename,
                        true /* include_unfinished_submaps */);
  }

  SDL_Log("%u {dbg_stop_slow}pre return", SDL_GetTicks());
}

}  // namespace
}  // namespace cartographer_ros

// int main(int argc, char** argv) {
int cartographer_ros__cartographer_node(bool& exit) {

  SDL_Log("%u {dbg_stop_slow}cartographer_ros__cartographer_node enter", SDL_GetTicks());
  // char* name[] = {"hello", "world"};
  // char **cp = name;
  cartographer::rose_slot.tid = SDL_ThreadID();

  char argv0[] = "cartographer_node";
  char argv1[] = "-configuration_directory";
  char argv2[] = "C:/ddksample/apps-src/apps/external/libros/cartographer_ros/cartographer_ros/configuration_files";
  char argv3[] = "-configuration_basename";
  char argv4[] = "revo_lds.lua";
  char* argvp[] = {argv0, argv1, argv2, argv3, argv4};

  char** argv = argvp;
  int argc = sizeof(argvp) / sizeof(argvp[0]);

  // -configuration_directory $(find cartographer_ros)/configuration_files -configuration_basename revo_lds.lua

  // google::InitGoogleLogging(argv[0]);
  // google::InitGoogleLogging(argv0);
  // google::ParseCommandLineFlags(&argc, &argv, true);

  fLS::FLAGS_configuration_directory = game_config::path + "/data/core/cert/cartographer_ros/configuration_files";
  fLS::FLAGS_configuration_basename = "revo_lds.lua";

  CHECK(!FLAGS_configuration_directory.empty())
      << "-configuration_directory is missing.";
  CHECK(!FLAGS_configuration_basename.empty())
      << "-configuration_basename is missing.";

  ::ros::init(argc, argv, "cartographer_node");
  ::ros::start();

  cartographer_ros::ScopedRosLogSink ros_log_sink;
  cartographer_ros::Run(exit);
  // ::ros::shutdown();
  SDL_Log("%u {dbg_stop_slow}cartographer_ros__cartographer_node exit", SDL_GetTicks());

  return 0;
}
