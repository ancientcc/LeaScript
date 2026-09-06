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
#include <costmap_2d/footprint.h>
#include <cstdio>
#include <string>
#include <algorithm>
#include <vector>

#include <SDL_timer.h>
#include <SDL_log.h>
#include <rose_exception.hpp>
#include <angles/angles.h>

using std::vector;

namespace costmap_2d
{

LayeredCostmap::LayeredCostmap(std::string global_frame, bool rolling_window, bool track_unknown) :
    costmap_(),
    global_frame_(global_frame),
    rolling_window_(rolling_window),
    current_(false),
    minx_(0.0),
    miny_(0.0),
    maxx_(0.0),
    maxy_(0.0),
    bx0_(0),
    bxn_(0),
    by0_(0),
    byn_(0),
    initialized_(false),
    size_locked_(false),
    circumscribed_radius_(1.0),
    inscribed_radius_(0.1)
{
  if (track_unknown)
    costmap_.setDefaultValue(NO_INFORMATION);
  else
    costmap_.setDefaultValue(FREE_SPACE);
}

LayeredCostmap::~LayeredCostmap()
{
  while (plugins_.size() > 0)
  {
    plugins_.pop_back();
  }
}

void LayeredCostmap::resizeMap(unsigned int size_x, unsigned int size_y, double resolution, double origin_x,
                               double origin_y, bool size_locked)
{
  boost::unique_lock<Costmap2D::mutex_t> lock(*(costmap_.getMutex()));
  ROS_INFO("[%s]resizeMap size(%u x %u), resolution: %.5f, origin(%.5f, %.5f), size_locked: %s", 
      global_frame_.c_str(), size_x, size_y, resolution, origin_x, origin_y, size_locked? "true": "false");
  size_locked_ = size_locked;
  costmap_.resizeMap(size_x, size_y, resolution, origin_x, origin_y);
  for (vector<boost::shared_ptr<Layer> >::iterator plugin = plugins_.begin(); plugin != plugins_.end();
      ++plugin)
  {
    (*plugin)->matchSize();
  }
}

void resolve_pressure2(const Costmap2D& costmap, double robot_x, double robot_y, double robot_yaw, const std::vector<SDL_Point>& obs2ins_cells, double inscribed_radius)
{
    tpose2d robot_pose2d;
    robot_pose2d.x = robot_x;
    robot_pose2d.y = robot_y;
    robot_pose2d.yaw = robot_yaw;

    unsigned robot_cell_x;
    unsigned robot_cell_y;
    costmap.worldToMap(robot_x, robot_y, robot_cell_x, robot_cell_y);

    double world_x;
    double world_y;
    const double threshold = DEG2RAD(40);
    
    bool front_has_obs = false;
    bool back_has_obs = false;
    int at = 0;
    for (std::vector<SDL_Point>::const_iterator it = obs2ins_cells.begin(); it != obs2ins_cells.end(); ++ it, at ++) {
        const SDL_Point& goal = *it;
        costmap.mapToWorld(goal.x, goal.y, world_x, world_y);

        double deltax = world_x - robot_pose2d.x;
        double deltay = world_y - robot_pose2d.y;
        // (-pi, pi]
        double goal_th = atan2(deltay, deltax);

        double cell_deltay = 0.05;
        double cell_th = atan2(cell_deltay, deltax);

        double ang_diff = angles::shortest_angular_distance(robot_pose2d.yaw, goal_th);
        double abs_ang_diff = fabs(ang_diff);

        ROS_INFO("[%i/%i] yaw: %.5f, radius: %.5f(%i), cell_th: %.5f (%i, %i) -> (%i, %i), ang_diff: %.5f", 
            at, (int)obs2ins_cells.size(), RAD2DEG(robot_yaw), 
            inscribed_radius, costmap.cellDistance(inscribed_radius), RAD2DEG(cell_th), robot_cell_x, robot_cell_y, goal.x, goal.y, RAD2DEG(ang_diff));

        if (abs_ang_diff < threshold) {
            front_has_obs = true;

        } else if ((M_PI - abs_ang_diff) < threshold) {
            back_has_obs = true;
        }
    }
    ROS_INFO("resolve_pressure2, obs2ins_cells.size: %i, front_has_obs: %s, back_has_obs: %s", 
        (int)obs2ins_cells.size(), front_has_obs? "true": "false", back_has_obs? "true": "false");
}

void LayeredCostmap::updateMap(double robot_x, double robot_y, double robot_yaw)
{
  // const bool verbose = global_frame_ != "map";
  const bool verbose = false;
  if (verbose) {
      ROS_INFO("%u, [%p]updateMap---", SDL_GetTicks(), this);
  }
  // Lock for the remainder of this function, some plugins (e.g. VoxelLayer)
  // implement thread unsafe updateBounds() functions.
  boost::unique_lock<Costmap2D::mutex_t> lock(*(costmap_.getMutex()));

  std::vector<SDL_Point>& master_obs_cells = costmap_.getObsCells();
  master_obs_cells.clear();

  // if we're using a rolling buffer costmap... we need to update the origin using the robot's position
  if (rolling_window_)
  {
    double new_origin_x = robot_x - costmap_.getSizeInMetersX() / 2;
    double new_origin_y = robot_y - costmap_.getSizeInMetersY() / 2;
    costmap_.updateOrigin(new_origin_x, new_origin_y);
  }

  if (plugins_.size() == 0)
    return;

  minx_ = miny_ = 1e30;
  maxx_ = maxy_ = -1e30;

  if (verbose) {
      SDL_Log("%u, [%s]updateMap[1/4], xy0_(%i, %i), xyn_(%i, %i), min(%.5f, %.5f), max(%.5f, %.5f)", 
          SDL_GetTicks(), global_frame_.c_str(), bx0_, by0_, bxn_, byn_, minx_, miny_, maxx_, maxy_);
  }

  for (vector<boost::shared_ptr<Layer> >::iterator plugin = plugins_.begin(); plugin != plugins_.end();
       ++plugin)
  {
    if(!(*plugin)->isEnabled())
      continue;
    double prev_minx = minx_;
    double prev_miny = miny_;
    double prev_maxx = maxx_;
    double prev_maxy = maxy_;
    (*plugin)->updateBounds(robot_x, robot_y, robot_yaw, &minx_, &miny_, &maxx_, &maxy_);
    if (minx_ > prev_minx || miny_ > prev_miny || maxx_ < prev_maxx || maxy_ < prev_maxy)
    {
      ROS_WARN_THROTTLE(1.0, "Illegal bounds change, was [tl: (%f, %f), br: (%f, %f)], but "
                        "is now [tl: (%f, %f), br: (%f, %f)]. The offending layer is %s",
                        prev_minx, prev_miny, prev_maxx , prev_maxy,
                        minx_, miny_, maxx_ , maxy_,
                        (*plugin)->getName().c_str());
    }
  }

  if (verbose) {
      SDL_Log("%u, [%s]updateMap[2/4]min(%.5f, %.5f), max(%.5f, %.5f)", 
          SDL_GetTicks(), global_frame_.c_str(), minx_, miny_, maxx_, maxy_);
  }
  int x0, xn, y0, yn;
  costmap_.worldToMapEnforceBounds(minx_, miny_, x0, y0);
  costmap_.worldToMapEnforceBounds(maxx_, maxy_, xn, yn);

  if (verbose) {
      SDL_Log("%u, [%s]updateMap[3/4]SizeInCells(%u x %u), xy0(%i, %i), xyn(%i, %i)", 
          SDL_GetTicks(), global_frame_.c_str(), costmap_.getSizeInCellsX(), costmap_.getSizeInCellsY(),
          x0, y0, xn, yn);
  }

  x0 = std::max(0, x0);
  xn = std::min(int(costmap_.getSizeInCellsX()), xn + 1);
  y0 = std::max(0, y0);
  yn = std::min(int(costmap_.getSizeInCellsY()), yn + 1);
/*
  if (verbose) {
    ROS_INFO("Updating area x: [%d, %d] y: [%d, %d] costmap_.origin[%.5f, %.5f]", x0, xn, y0, yn, 
        costmap_.getOriginX(), costmap_.getOriginY());
  }
*/
  if (xn < x0 || yn < y0)
    return;

  if (verbose) {
      SDL_Log("%u, [%s]updateMap[4/4]SizeInCells(%u x %u), xy0(%i, %i), xyn(%i, %i)", 
          SDL_GetTicks(), global_frame_.c_str(), costmap_.getSizeInCellsX(), costmap_.getSizeInCellsY(),
          x0, y0, xn, yn);
  }

  costmap_.resetMap(x0, y0, xn, yn);
  for (vector<boost::shared_ptr<Layer> >::iterator plugin = plugins_.begin(); plugin != plugins_.end();
       ++plugin)
  {
    if((*plugin)->isEnabled())
      (*plugin)->updateCosts(costmap_, x0, y0, xn, yn);
  }

  bx0_ = x0;
  bxn_ = xn;
  by0_ = y0;
  byn_ = yn;

  initialized_ = true;

  if (costmap_2d::rose_mode) {
      double inscribed_radius = getInscribedRadius();
      unsigned int origin_map_x;
      unsigned int origin_map_y;
      bool valid = costmap_.worldToMap(robot_x, robot_y, origin_map_x, origin_map_y);
      if (valid) {
/*
        int cell_inscribed_radius = costmap_.cellDistance(inscribed_radius);
        std::vector<SDL_Point> obs2ins_cells = costmap_.updateObstacleToInscribed(origin_map_x, origin_map_y, cell_inscribed_radius, robot_yaw);
        if (!obs2ins_cells.empty()) {
            master_obs_cells.insert(master_obs_cells.end(), obs2ins_cells.begin(), obs2ins_cells.end());
        }
*/
      } else {
          int ii = 0;
          // VALIDATE(false, null_str);
      }
  }

  if (verbose) {
    ROS_INFO("---[%p]updateMap, X", this);
  }
}

bool LayeredCostmap::isCurrent()
{
  current_ = true;
  for (vector<boost::shared_ptr<Layer> >::iterator plugin = plugins_.begin(); plugin != plugins_.end();
      ++plugin)
  {
    if((*plugin)->isEnabled())
      current_ = current_ && (*plugin)->isCurrent();
  }
  return current_;
}

void LayeredCostmap::setFootprint(const std::vector<geometry_msgs::Point>& footprint_spec)
{
  footprint_ = footprint_spec;
  costmap_2d::calculateMinAndMaxDistances(footprint_spec, inscribed_radius_, circumscribed_radius_);

  for (vector<boost::shared_ptr<Layer> >::iterator plugin = plugins_.begin(); plugin != plugins_.end();
      ++plugin)
  {
    (*plugin)->onFootprintChanged();
  }
}

}  // namespace costmap_2d
