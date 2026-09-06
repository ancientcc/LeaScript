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
 * Author: TKruse
 *********************************************************************/

#include <base_local_planner/map_grid_cost_function.h>

#include <costmap_2d/cost_values.h>
#include <rose_exception.hpp>
#include <SDL_log.h>

#ifndef M_PI_2
#define M_PI_2     1.57079632679489661923   // pi/2
#endif

namespace base_local_planner {

MapGridCostFunction::MapGridCostFunction(costmap_2d::Costmap2D* costmap,
    double xshift,
    double yshift,
    bool is_local_goal_function,
    CostAggregationType aggregationType,
    const std::string& name) :
    TrajectoryCostFunction(1.0, name.empty()? "MapGridCost": name),
    costmap_(costmap),
    map_(costmap->getSizeInCellsX(), costmap->getSizeInCellsY()),
    aggregationType_(aggregationType),
    xshift_(xshift),
    yshift_(yshift),
    is_local_goal_function_(is_local_goal_function),
    stop_on_failure_(true) {}

void MapGridCostFunction::setTargetPoses(std::vector<geometry_msgs::PoseStamped> target_poses) {
  target_poses_ = target_poses;
}

bool MapGridCostFunction::prepare() {
  map_.resetPathDist();

  if (is_local_goal_function_) {
    map_.setLocalGoal(*costmap_, target_poses_);
  } else {
    map_.setTargetCells(*costmap_, target_poses_);
  }
  return true;
}

double MapGridCostFunction::getCellCosts(unsigned int px, unsigned int py) {
  double grid_dist = map_(px, py).target_dist;
  if (grid_dist == map_.obstacleCosts()) {
      grid_dist = adjust_obstacleCosts(px, py);
  }
  return grid_dist;
}

double MapGridCostFunction::adjust_obstacleCosts(unsigned int cell_x, unsigned int cell_y)
{
    double original_grid_dist = map_(cell_x, cell_y).target_dist;
    VALIDATE(original_grid_dist == map_.obstacleCosts(), null_str);

    const unsigned int last_col = costmap_->getSizeInCellsX() - 1;
    const unsigned int last_row = costmap_->getSizeInCellsY() - 1;

    const unsigned char* data = costmap_->getCharMap();
    unsigned char cost = data[costmap_->getIndex(cell_x, cell_y)];
    if (cost != costmap_2d::INSCRIBED_INFLATED_OBSTACLE) {
        return original_grid_dist;
    }
    double min_grid_dist = original_grid_dist;
    std::vector<costmap_2d::MapLocation> candidates; 
    if (cell_x > 0) {
        candidates.push_back(costmap_2d::MapLocation{cell_x - 1, cell_y});
    }
    if (cell_x < last_col) {
        candidates.push_back(costmap_2d::MapLocation{cell_x + 1, cell_y});
    }
    if (cell_y > 0) {
        candidates.push_back(costmap_2d::MapLocation{cell_x, cell_y - 1});
    }
    if (cell_y < last_row) {
        candidates.push_back(costmap_2d::MapLocation{cell_x, cell_y + 1});
    }
    for (std::vector<costmap_2d::MapLocation>::const_iterator it = candidates.begin(); it != candidates.end(); ++ it) {
        const costmap_2d::MapLocation& cell = *it;
        double dist = map_(cell.x, cell.y).target_dist;
        if (dist < min_grid_dist) {
            min_grid_dist = dist + 1;
        }
    }
    if (min_grid_dist != original_grid_dist) {
        ROS_INFO("adjust_obstacleCosts, cell(%i, %i)(cost: %i) adjust dist from %.5f to %.5f", 
            (int)cell_x, (int)cell_y, cost, original_grid_dist, min_grid_dist);
    }
    return min_grid_dist;
}

double MapGridCostFunction::scoreTrajectory(Trajectory &traj) {
  double cost = 0.0;
  if (aggregationType_ == Product) {
    cost = 1.0;
  }
  double px, py, pth;
  unsigned int cell_x, cell_y;
  double grid_dist;

  for (unsigned int i = 0; i < traj.getPointsSize(); ++i) {
    traj.getPoint(i, px, py, pth);

    // translate point forward if specified
    if (xshift_ != 0.0) {
      px = px + xshift_ * cos(pth);
      py = py + xshift_ * sin(pth);
    }
    // translate point sideways if specified
    if (yshift_ != 0.0) {
      px = px + yshift_ * cos(pth + M_PI_2);
      py = py + yshift_ * sin(pth + M_PI_2);
    }

    //we won't allow trajectories that go off the map... shouldn't happen that often anyways
    if ( ! costmap_->worldToMap(px, py, cell_x, cell_y)) {
      //we're off the map
      ROS_WARN("Off Map %f, %f", px, py);
      return -4.0;
    }

    if (i == traj.getPointsSize() - 1) {
        // ROS_INFO("scoreTrajectory(%s) vel(%.5f, %.5f, %.5f), last cell(%i, %i)", 
        //    getName().c_str(), traj.xv_, traj.yv_, traj.thetav_, (int)cell_x, (int)cell_y);
    }

    grid_dist = getCellCosts(cell_x, cell_y);
    //if a point on this trajectory has no clear path to the goal... it may be invalid
    if (stop_on_failure_) {
      if (grid_dist == map_.obstacleCosts()) {
        return -3.0;
      } else if (grid_dist == map_.unreachableCellCosts()) {
        return -2.0;
      }
    }

    switch( aggregationType_ ) {
    case Last:
      cost = grid_dist;
      break;
    case Sum:
      cost += grid_dist;
      break;
    case Product:
      if (cost > 0) {
        cost *= grid_dist;
      }
      break;
    }
  }

  return cost;
}

} /* namespace base_local_planner */
