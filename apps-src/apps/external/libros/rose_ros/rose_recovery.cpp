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
#include <rose_ros/rose_recovery.h>
#include <vector>
#include <costmap_2d/obstacle_layer.h>
#include <SDL_log.h>

trose_recovery::trose_recovery(ros::CallbackQueue& cbqueue)
    : nav_core::RecoveryBehavior(cbqueue)
    , global_costmap_(nullptr)
    , local_costmap_(nullptr)
    , tf_(nullptr)
    , initialized_(false) 
    , clearing_distance_(0.6)
{}

void trose_recovery::initialize(std::string name, tf2_ros::Buffer* tf,
    costmap_2d::Costmap2DROS* global_costmap, costmap_2d::Costmap2DROS* local_costmap)
{
    if (!initialized_) {
        name_ = name;
        tf_ = tf;
        global_costmap_ = global_costmap;
        local_costmap_ = local_costmap;

        // get some parameters from the parameter server
        // ros::NodeHandle private_nh("~/" + name_);

        // private_nh.param("reset_distance", reset_distance_, 3.0);

        initialized_ = true;
    } else{
        ROS_ERROR("You should not call initialize twice on this object, doing nothing");
    }
}

void trose_recovery::runBehavior()
{
    if (!initialized_){
        ROS_ERROR("This recovery behavior has not been initialized, doing nothing.");
        return;
    }

    if (global_costmap_ == nullptr || local_costmap_ == nullptr){
        ROS_ERROR("The costmaps passed to the ClearCostmapRecovery object cannot be NULL. Doing nothing.");
        return;
    }

    ROS_WARN("Rose clear recovery behavior started.");
    // global_costmap_->clearCostmaps();
    local_costmap_->clearCostmaps();
}

