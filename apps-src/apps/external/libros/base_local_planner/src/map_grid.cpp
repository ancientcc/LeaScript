/*********************************************************************
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
 *********************************************************************/
#include <base_local_planner/map_grid.h>
#include <costmap_2d/cost_values.h>
using namespace std;

#include <SDL_log.h>
#include <SDL.h>

namespace base_local_planner{

  MapGrid::MapGrid()
    : size_x_(0), size_y_(0)
  {
  }

  MapGrid::MapGrid(unsigned int size_x, unsigned int size_y) 
    : size_x_(size_x), size_y_(size_y)
  {
    commonInit();
  }

  MapGrid::MapGrid(const MapGrid& mg){
    size_y_ = mg.size_y_;
    size_x_ = mg.size_x_;
    map_ = mg.map_;
  }

  void MapGrid::commonInit(){
    //don't allow construction of zero size grid
    ROS_ASSERT(size_y_ != 0 && size_x_ != 0);

    map_.resize(size_y_ * size_x_);

    //make each cell aware of its location in the grid
    for(unsigned int i = 0; i < size_y_; ++i){
      for(unsigned int j = 0; j < size_x_; ++j){
        unsigned int id = size_x_ * i + j;
        map_[id].cx = j;
        map_[id].cy = i;
      }
    }
  }

  size_t MapGrid::getIndex(int x, int y){
    return size_x_ * y + x;
  }

  MapGrid& MapGrid::operator= (const MapGrid& mg){
    size_y_ = mg.size_y_;
    size_x_ = mg.size_x_;
    map_ = mg.map_;
    return *this;
  }

  void MapGrid::sizeCheck(unsigned int size_x, unsigned int size_y){
    if(map_.size() != size_x * size_y)
      map_.resize(size_x * size_y);

    if(size_x_ != size_x || size_y_ != size_y){
      size_x_ = size_x;
      size_y_ = size_y;

      for(unsigned int i = 0; i < size_y_; ++i){
        for(unsigned int j = 0; j < size_x_; ++j){
          int index = size_x_ * i + j;
          map_[index].cx = j;
          map_[index].cy = i;
        }
      }
    }
  }


  inline bool MapGrid::updatePathCell(MapCell* current_cell, MapCell* check_cell,
      const costmap_2d::Costmap2D& costmap){

    //if the cell is an obstacle set the max path distance
    unsigned char cost = costmap.getCost(check_cell->cx, check_cell->cy);

    if(! getCell(check_cell->cx, check_cell->cy).within_robot &&
        (cost == costmap_2d::LETHAL_OBSTACLE ||
         cost == costmap_2d::INSCRIBED_INFLATED_OBSTACLE ||
         cost == costmap_2d::NO_INFORMATION)){
      // SDL_Log("updatePathCell, (%u, %u) --> obstacleCosts()", check_cell->cx, check_cell->cy);
      check_cell->target_dist = obstacleCosts();
      return false;
    }

    double new_target_dist = current_cell->target_dist + 1;
    if (new_target_dist < check_cell->target_dist) {
      check_cell->target_dist = new_target_dist;
    }
    return true;
  }


  //reset the path_dist and goal_dist fields for all cells
  void MapGrid::resetPathDist(){
    for(unsigned int i = 0; i < map_.size(); ++i) {
      map_[i].target_dist = unreachableCellCosts();
      map_[i].target_mark = false;
      map_[i].within_robot = false;
    }
  }

  void MapGrid::adjustPlanResolution(const std::vector<geometry_msgs::PoseStamped>& global_plan_in,
      std::vector<geometry_msgs::PoseStamped>& global_plan_out, double resolution) {
    if (global_plan_in.size() == 0) {
      return;
    }
    double last_x = global_plan_in[0].pose.position.x;
    double last_y = global_plan_in[0].pose.position.y;
    global_plan_out.push_back(global_plan_in[0]);

    double min_sq_resolution = resolution * resolution;

    for (unsigned int i = 1; i < global_plan_in.size(); ++i) {
      double loop_x = global_plan_in[i].pose.position.x;
      double loop_y = global_plan_in[i].pose.position.y;
      double sqdist = (loop_x - last_x) * (loop_x - last_x) + (loop_y - last_y) * (loop_y - last_y);
      if (sqdist > min_sq_resolution) {
        int steps = ceil((sqrt(sqdist)) / resolution);
        // add a points in-between
        double deltax = (loop_x - last_x) / steps;
        double deltay = (loop_y - last_y) / steps;
        // TODO: Interpolate orientation
        for (int j = 1; j < steps; ++j) {
          geometry_msgs::PoseStamped pose;
          pose.pose.position.x = last_x + j * deltax;
          pose.pose.position.y = last_y + j * deltay;
          pose.pose.position.z = global_plan_in[i].pose.position.z;
          pose.pose.orientation = global_plan_in[i].pose.orientation;
          pose.header = global_plan_in[i].header;
          global_plan_out.push_back(pose);
        }
      }
      global_plan_out.push_back(global_plan_in[i]);
      last_x = loop_x;
      last_y = loop_y;
    }
  }

  static bool verbose = false;

  //update what map cells are considered path based on the global_plan
  void MapGrid::setTargetCells(const costmap_2d::Costmap2D& costmap,
      const std::vector<geometry_msgs::PoseStamped>& global_plan) {
      verbose = false;
    sizeCheck(costmap.getSizeInCellsX(), costmap.getSizeInCellsY());

    bool started_path = true;
    if (verbose) {
        SDL_Log("MapGrid::setTargetCells---");
    }

    queue<MapCell*> path_dist_queue;

    std::vector<geometry_msgs::PoseStamped> adjusted_global_plan;
    adjustPlanResolution(global_plan, adjusted_global_plan, costmap.getResolution());
    if (adjusted_global_plan.size() != global_plan.size()) {
      ROS_DEBUG("Adjusted global plan resolution, added %zu points", adjusted_global_plan.size() - global_plan.size());
    }
    unsigned int i;
    // put global path points into local map until we reach the border of the local map
    for (i = 0; i < adjusted_global_plan.size(); ++i) {
      double g_x = adjusted_global_plan[i].pose.position.x;
      double g_y = adjusted_global_plan[i].pose.position.y;
      unsigned int map_x, map_y;
      if (costmap.worldToMap(g_x, g_y, map_x, map_y) && costmap.getCost(map_x, map_y) != costmap_2d::NO_INFORMATION) {
        MapCell& current = getCell(map_x, map_y);
        current.target_dist = 0.0;
        current.target_mark = true;
        path_dist_queue.push(&current);
        started_path = true;
        if (verbose) {
            SDL_Log("[%u/%u]setTargetCells, try g_xy(%.5f, %.5f), map_xy(%i, %i), path_dist_queue.size: %u", 
                i, adjusted_global_plan.size(), g_x, g_y, map_x, map_y, path_dist_queue.size());
        }
      } else if (started_path) {
          if (verbose) {
                SDL_Log("[%u/%u]setLocalGoal, break, try g_xy(%.5f, %.5f), path_dist_queue.size: %u", 
                    i, adjusted_global_plan.size(), g_x, g_y, path_dist_queue.size());
            }
          break;
      }
    }
    if (!started_path) {
      ROS_ERROR("None of the %d first of %zu (%zu) points of the global plan were in the local costmap and free",
          i, adjusted_global_plan.size(), global_plan.size());
      return;
    }

    computeTargetDistance(path_dist_queue, costmap);
  }

  //mark the point of the costmap as local goal where global_plan first leaves the area (or its last point)
  void MapGrid::setLocalGoal(const costmap_2d::Costmap2D& costmap,
      const std::vector<geometry_msgs::PoseStamped>& global_plan) {
      verbose = false;
    if (verbose) {
        SDL_Log("MapGrid::setLocalGoal---");
    }
    sizeCheck(costmap.getSizeInCellsX(), costmap.getSizeInCellsY());

    int local_goal_x = -1;
    int local_goal_y = -1;
    bool started_path = false;

    std::vector<geometry_msgs::PoseStamped> adjusted_global_plan;
    adjustPlanResolution(global_plan, adjusted_global_plan, costmap.getResolution());

    // skip global path points until we reach the border of the local map
    for (unsigned int i = 0; i < adjusted_global_plan.size(); ++i) {
      double g_x = adjusted_global_plan[i].pose.position.x;
      double g_y = adjusted_global_plan[i].pose.position.y;
      unsigned int map_x, map_y;
      if (costmap.worldToMap(g_x, g_y, map_x, map_y) && costmap.getCost(map_x, map_y) != costmap_2d::NO_INFORMATION) {
        local_goal_x = map_x;
        local_goal_y = map_y;
        started_path = true;
        if (verbose) {
            SDL_Log("[%u/%u]setLocalGoal, try g_xy(%.5f, %.5f), map_xy(%i, %i)", 
                i, adjusted_global_plan.size(), g_x, g_y, map_x, map_y);
        }
      } else {
        if (started_path) {
            if (verbose) {
                SDL_Log("[%u/%u]setLocalGoal, break, try g_xy(%.5f, %.5f)", 
                    i, adjusted_global_plan.size(), g_x, g_y);
            }
          break;
        }// else we might have a non pruned path, so we just continue
      }
    }
    if (!started_path) {
      ROS_ERROR("None of the points of the global plan were in the local costmap, global plan points too far from robot");
      verbose = false;
      return;
    }

    queue<MapCell*> path_dist_queue;
    if (local_goal_x >= 0 && local_goal_y >= 0) {
      MapCell& current = getCell(local_goal_x, local_goal_y);
      costmap.mapToWorld(local_goal_x, local_goal_y, goal_x_, goal_y_);
      current.target_dist = 0.0;
      current.target_mark = true;
      path_dist_queue.push(&current);
      if (verbose) {
        SDL_Log("setLocalGoal, hit, local_goal_xy(%i, %i), goal_xy(%.5f, %.5f)", local_goal_x, local_goal_y, goal_x_, goal_y_);
      }
    }

    computeTargetDistance(path_dist_queue, costmap);
    verbose = false;
  }


  std::string to_string(const MapCell& cell)
  {
      char buf[256];
      SDL_snprintf(buf, sizeof(buf), "xy: (%u, %u), target_dist: %.5f, target_mark: %s, within_robot: %s", 
          cell.cx, cell.cy, cell.target_dist, cell.target_mark? "true": "false", cell.within_robot? "true": "false");
      return buf;   
  }

  void MapGrid::computeTargetDistance(queue<MapCell*>& dist_queue, const costmap_2d::Costmap2D& costmap){
    MapCell* current_cell;
    MapCell* check_cell;
    const unsigned int last_col = size_x_ - 1;
    const unsigned int last_row = size_y_ - 1;
    if (verbose) {
        SDL_Log("computeTargetDistance---dist_queue.size: %u, size(%ux%u), last_col: %u, last_row: %u", 
            dist_queue.size(), costmap.getSizeInCellsX(), costmap.getSizeInCellsY(), last_col, last_row);
    }
    while(!dist_queue.empty()){
      current_cell = dist_queue.front();


      dist_queue.pop();
      if (verbose) {
        SDL_Log("computeTargetDistance[1/5]dist_queue.size: %u, current_cell: %s", 
            dist_queue.size(), to_string(*current_cell).c_str());
      }

      if(current_cell->cx > 0){
        check_cell = current_cell - 1;
        if(!check_cell->target_mark){
          //mark the cell as visisted
          check_cell->target_mark = true;
          if (verbose) {
            SDL_Log("computeTargetDistance[2/5](current_cell->cx > 0) check_cell: %s", 
                   to_string(*check_cell).c_str());
          }
          if(updatePathCell(current_cell, check_cell, costmap)) {
            dist_queue.push(check_cell);
          }
        }
      }

      if(current_cell->cx < last_col){
        check_cell = current_cell + 1;
        if(!check_cell->target_mark){
          check_cell->target_mark = true;
          if (verbose) {
            SDL_Log("computeTargetDistance[3/5](current_cell->cx < last_col) check_cell: %s", 
                   to_string(*check_cell).c_str());
          }
          if(updatePathCell(current_cell, check_cell, costmap)) {
            dist_queue.push(check_cell);
          }
        }
      }

      if(current_cell->cy > 0){
        check_cell = current_cell - size_x_;
        if(!check_cell->target_mark){
          check_cell->target_mark = true;
          if (verbose) {
            SDL_Log("computeTargetDistance[3/5](current_cell->cy > 0) check_cell: %s", 
                   to_string(*check_cell).c_str());
          }
          if(updatePathCell(current_cell, check_cell, costmap)) {
            dist_queue.push(check_cell);
          }
        }
      }

      if(current_cell->cy < last_row){
        check_cell = current_cell + size_x_;
        if(!check_cell->target_mark){
          check_cell->target_mark = true;
          if (verbose) {
            SDL_Log("computeTargetDistance[4/5](current_cell->cy < last_row) check_cell: %s", 
                   to_string(*check_cell).c_str());
          }
          if(updatePathCell(current_cell, check_cell, costmap)) {
            dist_queue.push(check_cell);
          }
        }
      }
    }
    if (verbose) {
        SDL_Log("---computeTargetDistance, X, dist_queue: %u, size(%ux%u)", 
            dist_queue.size(), costmap.getSizeInCellsX(), costmap.getSizeInCellsY());
    }

    if (verbose) {
        for (std::vector<MapCell>::const_iterator it = map_.begin(); it != map_.end(); ++ it) {
            const MapCell& cell = *it;
            if (cell.target_dist <= 0 || cell.target_dist > 96) {
                // if (cell.target_dist != obstacleCosts()) {
                    SDL_Log("(%u, %u), target_dist: %.5f", cell.cx, cell.cy, cell.target_dist);
                // }
            }
        }
        int ii = 0;
    }
  }

};
