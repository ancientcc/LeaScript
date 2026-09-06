/*********************************************************************
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2008, 2013, Willow Garage, Inc.
 *  Copyright (c) 2015, Fetch Robotics, Inc.
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
#include <costmap_2d/dataptr_layer.h>
#include <costmap_2d/costmap_math.h>
// #include <pluginlib/class_list_macros.h>
#include <tf2/convert.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

// PLUGINLIB_EXPORT_CLASS(costmap_2d::DataptrLayer, costmap_2d::Layer)
#include "rose_exception.hpp"
#include <SDL_log.h>

using costmap_2d::NO_INFORMATION;
using costmap_2d::LETHAL_OBSTACLE;
using costmap_2d::FREE_SPACE;

namespace costmap_2d
{

DataptrLayer::DataptrLayer(ros::CallbackQueue& cbqueue, bool& exit) 
    : cbqueue_(cbqueue)
    , exit_(exit)
{}

DataptrLayer::~DataptrLayer()
{
}

void DataptrLayer::onInitialize()
{
  current_ = true;

  global_frame_ = layered_costmap_->getGlobalFrameID();

  track_unknown_space_ = true;

  trinary_costmap_ = true;

  // must not use 48(0x30). it will too fake LETHAL_OBSTACLE at first.
  // lethal_threshold_ = 57; // 100(0x64), 75(0x4b), 57(0x39)
  lethal_threshold_ = 100;
  unknown_cost_value_ = -1;

  enabled_ = true;
}

void DataptrLayer::matchSize()
{
  // If we are using rolling costmap, the static map size is
  //   unrelated to the size of the layered costmap
  // if (!layered_costmap_->isRolling())
  {
    Costmap2D* master = layered_costmap_->getCostmap();
    resizeMap(master->getSizeInCellsX(), master->getSizeInCellsY(), master->getResolution(),
              master->getOriginX(), master->getOriginY());
  }
}

void DataptrLayer::set_dataptr(bool global_planner, const nav_msgs::MapMetaData& info, const int8_t* data)
{
    VALIDATE(data != nullptr, null_str);

    if (global_planner) {
        layered_costmap_->resizeMap(info.width, info.height, info.resolution, info.origin.position.x,
            info.origin.position.y, true); // set size_locked to true, prevents reconfigureCb from overriding map size

        VALIDATE(info.width == size_x_ && info.height == size_y_, null_str);
        unsigned int index = 0;
        // initialize the costmap with static data
        for (unsigned int i = 0; i < size_y_; ++i) {
            for (unsigned int j = 0; j < size_x_; ++j) {
                unsigned char value = data[index];
                costmap_[index] = interpretValue(value);
                // costmap_[index] = value;
                ++ index;
            }
        }
        return;
    }

    resetMaps();

    const Costmap2D* master = layered_costmap_->getCostmap();

    int start_x;
    int start_y;
    master->worldToMapNoBounds(info.origin.position.x, info.origin.position.y, start_x, start_y);

    int end_size_x;
    int end_size_y;
    int costmap_start_x = 0;
    int costmap_start_y = 0;
    if (start_x <= 0) {
        start_x *= -1;
        end_size_x = size_x_;

        if (start_x + end_size_x > (int)info.width) {
            // overrun border: east
            end_size_x = info.width - start_x;
        }
    } else {
        // overrun border: west
        costmap_start_x = start_x;
        end_size_x = size_x_ - costmap_start_x;
        start_x = 0;
    }

    if (start_y <= 0) {
        start_y *= -1;
        end_size_y = size_y_;
        if (start_y + end_size_y > (int)info.height) {
            // overrun border: north
            end_size_y = info.height - start_y;
        }
    } else {
        // overrun border: south
        costmap_start_y = start_y;
        end_size_y = size_y_ - costmap_start_y;
        start_y = 0;
    }

    int src_index = 0;
    int dst_index = 0;
    // initialize the costmap with static data
    for (int y = 0; y < end_size_y; ++ y) {
        // const int src_y_line = (info.height - (y + start_y) - 1) * info.width;
        const int src_y_line = (y + start_y) * info.width;
        const int dst_y_line = (y + costmap_start_y) * size_x_;
        for (int x = 0; x < end_size_x; ++ x) {
            src_index = (x + start_x) + src_y_line;
            dst_index = (x + costmap_start_x) + dst_y_line;

            unsigned char value = data[src_index];
            costmap_[dst_index] = interpretValue(value);
            // costmap_[dst_index] = value;
        }
    }
}

unsigned char DataptrLayer::interpretValue(unsigned char value)
{
  // check if the static value is above the unknown or lethal thresholds
  if (track_unknown_space_ && value == unknown_cost_value_)
    return NO_INFORMATION;
  else if (!track_unknown_space_ && value == unknown_cost_value_)
    return FREE_SPACE;
  else if (value >= lethal_threshold_)
    return LETHAL_OBSTACLE;
  else if (trinary_costmap_)
    return FREE_SPACE;

  double scale = (double) value / lethal_threshold_;
  return scale * LETHAL_OBSTACLE;
}

void DataptrLayer::activate()
{
  onInitialize();
}

void DataptrLayer::deactivate()
{
}

void DataptrLayer::clearCostmap()
{
    onInitialize();
}

void DataptrLayer::updateBounds(double robot_x, double robot_y, double robot_yaw, double* min_x, double* min_y,
                               double* max_x, double* max_y)
{
    const bool verbose = true;
    const Costmap2D* master = layered_costmap_->getCostmap();
    if (verbose) {
        ROS_INFO("[%s]updateBounds---this.origin_: (%.3f, %.3f), masgter.origin: (%.3f, %.3f), isRolling: %s", 
            name_.c_str(), origin_x_, origin_y_, master->getOriginX(), master->getOriginY(),
            layered_costmap_->isRolling()? "true": "false");
    }

    VALIDATE(size_x_ > 0 && size_y_ > 0, null_str);

    useExtraBounds(min_x, min_y, max_x, max_y);

    const double master_origin_x = master->getOriginX();
    const double master_origin_y = master->getOriginY();
    
    *min_x = master_origin_x;
    *max_x = master_origin_x + master->getSizeInMetersX();

    *min_y = master_origin_y;
    *max_y = master_origin_y + master->getSizeInMetersY();

    if (verbose) {
        ROS_INFO("[%s]---updateBounds, x(%.5f, %.5f), y(%.5f, %.5f), size(%ux%u)",
            name_.c_str(), *min_x, *max_x, *min_y, *max_y, size_x_, size_y_);
    }
}

void DataptrLayer::updateCosts(costmap_2d::Costmap2D& master_grid, int min_i, int min_j, int max_i, int max_j)
{
    const bool verbose = true;
    if (verbose) {
        ROS_INFO("[%s]updateCosts---", name_.c_str());
    }
    VALIDATE(size_x_ > 0 && size_y_ > 0, null_str);

    // if not rolling, the layered costmap (master_grid) has same coordinates as this layer
    updateWithTrueOverwrite(master_grid, min_i, min_j, max_i, max_j);

    if (verbose) {
        ROS_INFO("---[%s]updateCosts, X", name_.c_str());
    }
}

}  // namespace costmap_2d
