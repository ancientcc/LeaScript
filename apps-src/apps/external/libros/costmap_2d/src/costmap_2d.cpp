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
#include <costmap_2d/costmap_2d.h>
#include <cstdio>

#include <angles/angles.h>
#include "rose_exception.hpp"
#include <SDL_log.h>
#include <costmap_2d/cost_values.h>
#include <rose_filesystem.hpp>



namespace costmap_2d
{
Costmap2D::Costmap2D(unsigned int cells_size_x, unsigned int cells_size_y, double resolution,
                     double origin_x, double origin_y, unsigned char default_value) :
    size_x_(cells_size_x), size_y_(cells_size_y), resolution_(resolution), origin_x_(origin_x),
    origin_y_(origin_y), costmap_(NULL), default_value_(default_value)
{
  access_ = new mutex_t();

  // create the costmap
  initMaps(size_x_, size_y_);
  resetMaps();
}

void Costmap2D::deleteMaps()
{
  // clean up data
  boost::unique_lock<mutex_t> lock(*access_);
  delete[] costmap_;
  costmap_ = NULL;
}

void Costmap2D::initMaps(unsigned int size_x, unsigned int size_y)
{
  boost::unique_lock<mutex_t> lock(*access_);
  delete[] costmap_;
  costmap_ = new unsigned char[size_x * size_y];
}

void Costmap2D::resizeMap(unsigned int size_x, unsigned int size_y, double resolution,
                          double origin_x, double origin_y)
{
  size_x_ = size_x;
  size_y_ = size_y;
  resolution_ = resolution;
  origin_x_ = origin_x;
  origin_y_ = origin_y;

  initMaps(size_x, size_y);

  // reset our maps to have no information
  resetMaps();
}

void Costmap2D::resetMaps()
{
  boost::unique_lock<mutex_t> lock(*access_);
  memset(costmap_, default_value_, size_x_ * size_y_ * sizeof(unsigned char));
}

void Costmap2D::resetMap(unsigned int x0, unsigned int y0, unsigned int xn, unsigned int yn)
{
  boost::unique_lock<mutex_t> lock(*(access_));
  unsigned int len = xn - x0;
  for (unsigned int y = y0 * size_x_ + x0; y < yn * size_x_ + x0; y += size_x_)
    memset(costmap_ + y, default_value_, len * sizeof(unsigned char));
}

bool Costmap2D::copyCostmapWindow(const Costmap2D& map, double win_origin_x, double win_origin_y, double win_size_x,
                                  double win_size_y)
{
  // check for self windowing
  if (this == &map)
  {
    // ROS_ERROR("Cannot convert this costmap into a window of itself");
    return false;
  }

  // clean up old data
  deleteMaps();

  // compute the bounds of our new map
  unsigned int lower_left_x, lower_left_y, upper_right_x, upper_right_y;
  if (!map.worldToMap(win_origin_x, win_origin_y, lower_left_x, lower_left_y)
      || !map.worldToMap(win_origin_x + win_size_x, win_origin_y + win_size_y, upper_right_x, upper_right_y))
  {
    // ROS_ERROR("Cannot window a map that the window bounds don't fit inside of");
    return false;
  }

  size_x_ = upper_right_x - lower_left_x;
  size_y_ = upper_right_y - lower_left_y;
  resolution_ = map.resolution_;
  origin_x_ = win_origin_x;
  origin_y_ = win_origin_y;

  // initialize our various maps and reset markers for inflation
  initMaps(size_x_, size_y_);

  // copy the window of the static map and the costmap that we're taking
  copyMapRegion(map.costmap_, lower_left_x, lower_left_y, map.size_x_, costmap_, 0, 0, size_x_, size_x_, size_y_);
  return true;
}

Costmap2D& Costmap2D::operator=(const Costmap2D& map)
{
  // check for self assignement
  if (this == &map)
    return *this;

  // clean up old data
  deleteMaps();

  size_x_ = map.size_x_;
  size_y_ = map.size_y_;
  resolution_ = map.resolution_;
  origin_x_ = map.origin_x_;
  origin_y_ = map.origin_y_;

  // initialize our various maps
  initMaps(size_x_, size_y_);

  // copy the cost map
  memcpy(costmap_, map.costmap_, size_x_ * size_y_ * sizeof(unsigned char));

  return *this;
}

Costmap2D::Costmap2D(const Costmap2D& map) :
    costmap_(NULL)
{
  access_ = new mutex_t();
  *this = map;
}

// just initialize everything to NULL by default
Costmap2D::Costmap2D() :
    size_x_(0), size_y_(0), resolution_(0.0), origin_x_(0.0), origin_y_(0.0), costmap_(NULL), default_value_(costmap_2d::NO_INFORMATION)
{
  access_ = new mutex_t();
}

Costmap2D::~Costmap2D()
{
  deleteMaps();
  delete access_;
}

unsigned int Costmap2D::cellDistance(double world_dist) const
{
  if (resolution_ == 0) {
      // It's incredible that there will be 0.
      return UINT32_MAX;
  }
  double cells_dist = std::max(0.0, ceil(world_dist / resolution_));
  return (unsigned int)cells_dist;
}

unsigned char* Costmap2D::getCharMap() const
{
  return costmap_;
}

unsigned char Costmap2D::getCost(unsigned int mx, unsigned int my) const
{
  return costmap_[getIndex(mx, my)];
}

void Costmap2D::setCost(unsigned int mx, unsigned int my, unsigned char cost)
{
  costmap_[getIndex(mx, my)] = cost;
}

void Costmap2D::mapToWorld(unsigned int mx, unsigned int my, double& wx, double& wy) const
{
  wx = origin_x_ + (mx + 0.5) * resolution_;
  wy = origin_y_ + (my + 0.5) * resolution_;
}

bool Costmap2D::worldToMap(double wx, double wy, unsigned int& mx, unsigned int& my) const
{
  if (wx < origin_x_ || wy < origin_y_)
    return false;

  mx = (int)((wx - origin_x_) / resolution_);
  my = (int)((wy - origin_y_) / resolution_);

  if (mx < size_x_ && my < size_y_)
    return true;

  return false;
}

void Costmap2D::worldToMapNoBounds(double wx, double wy, int& mx, int& my) const
{
  mx = (int)((wx - origin_x_) / resolution_);
  my = (int)((wy - origin_y_) / resolution_);
}

void Costmap2D::worldToMapEnforceBounds(double wx, double wy, int& mx, int& my) const
{
  // Here we avoid doing any math to wx,wy before comparing them to
  // the bounds, so their values can go out to the max and min values
  // of double floating point.
  if (wx < origin_x_)
  {
    mx = 0;
  }
  else if (wx >= resolution_ * size_x_ + origin_x_)
  {
    mx = size_x_ - 1;
  }
  else
  {
    mx = (int)((wx - origin_x_) / resolution_);
  }

  if (wy < origin_y_)
  {
    my = 0;
  }
  else if (wy >= resolution_ * size_y_ + origin_y_)
  {
    my = size_y_ - 1;
  }
  else
  {
    my = (int)((wy - origin_y_) / resolution_);
  }
}

void Costmap2D::updateOrigin(double new_origin_x, double new_origin_y)
{
  // project the new origin into the grid
  int cell_ox, cell_oy;
  cell_ox = int((new_origin_x - origin_x_) / resolution_);
  cell_oy = int((new_origin_y - origin_y_) / resolution_);

  // Nothing to update
  if (cell_ox == 0 && cell_oy == 0)
    return;

  // compute the associated world coordinates for the origin cell
  // because we want to keep things grid-aligned
  double new_grid_ox, new_grid_oy;
  new_grid_ox = origin_x_ + cell_ox * resolution_;
  new_grid_oy = origin_y_ + cell_oy * resolution_;

  // To save casting from unsigned int to int a bunch of times
  int size_x = size_x_;
  int size_y = size_y_;

  // we need to compute the overlap of the new and existing windows
  int lower_left_x, lower_left_y, upper_right_x, upper_right_y;
  lower_left_x = std::min(std::max(cell_ox, 0), size_x);
  lower_left_y = std::min(std::max(cell_oy, 0), size_y);
  upper_right_x = std::min(std::max(cell_ox + size_x, 0), size_x);
  upper_right_y = std::min(std::max(cell_oy + size_y, 0), size_y);

  unsigned int cell_size_x = upper_right_x - lower_left_x;
  unsigned int cell_size_y = upper_right_y - lower_left_y;

  // we need a map to store the obstacles in the window temporarily
  unsigned char* local_map = new unsigned char[cell_size_x * cell_size_y];

  // copy the local window in the costmap to the local map
  copyMapRegion(costmap_, lower_left_x, lower_left_y, size_x_, local_map, 0, 0, cell_size_x, cell_size_x, cell_size_y);

  // now we'll set the costmap to be completely unknown if we track unknown space
  resetMaps();

  // update the origin with the appropriate world coordinates
  origin_x_ = new_grid_ox;
  origin_y_ = new_grid_oy;

  // compute the starting cell location for copying data back in
  int start_x = lower_left_x - cell_ox;
  int start_y = lower_left_y - cell_oy;

  // now we want to copy the overlapping information back into the map, but in its new location
  copyMapRegion(local_map, 0, 0, cell_size_x, costmap_, start_x, start_y, size_x_, cell_size_x, cell_size_y);

  // make sure to clean up
  delete[] local_map;
}

std::vector<MapLocation> polygon_increase1(const std::vector<MapLocation>& src, int size_x, int size_y)
{
    int min_x = INT32_MAX;
    int min_y = INT32_MAX;
    int max_x = INT32_MIN;
    int max_y = INT32_MIN;

    for (std::vector<MapLocation>::const_iterator it = src.begin(); it != src.end(); ++ it) {
        const MapLocation& loc = *it;
        posix_touch_i32(loc.x, loc.y, &min_x, &min_y, &max_x, &max_y);
    }
    SDL_DPoint center{(min_x + max_x) / 2.0, (min_y + max_y) / 2.0};

    std::vector<MapLocation> result;
    for (std::vector<MapLocation>::const_iterator it = src.begin(); it != src.end(); ++ it) {
        const MapLocation& loc = *it;

        double deltax = loc.x - center.x;
        double deltay = loc.y - center.y;
        // (-pi, pi]
        double angle = atan2(deltay, deltax);
        // [0, 2*pi]
        angle = angles::normalize_angle_positive(angle);

        int new_x = loc.x;
        int new_y = loc.y;
        // 360 = 40 * 4 + 50 * 4
        if (angle < DEG2RAD(20)) {
            // 40(first 20)
            new_x ++;
            
        } else if (angle <= DEG2RAD(70)) {
            // 50
            new_x ++;
            new_y ++;

        } else if (angle <= DEG2RAD(110)) {
            // 40
            new_y ++;

        } else if (angle <= DEG2RAD(160)) {
            // 50
            new_x --;
            new_y ++;

        } else if (angle <= DEG2RAD(200)) {
            // 40
            new_x --;

        } else if (angle <= DEG2RAD(250)) {
            // 50
            new_x --;
            new_y --;

        } else if (angle <= DEG2RAD(290)) {
            // 40
            new_y --;

        } else if (angle <= DEG2RAD(340)) {
            // 50
            new_x ++;
            new_y --;

        } else {
            // 40(last 20)
            new_x ++;
        }

        if (new_x < 0) {
            new_x = 0;
        }
        if (new_x >= size_x) {
            new_x = loc.x;
        }
        if (new_y < 0) {
            new_y = 0;
        }
        if (new_y >= size_y) {
            new_y = loc.y;
        }

        result.push_back(MapLocation{(unsigned int)new_x, (unsigned int)new_y});
    }
    return result;
}

bool Costmap2D::setConvexPolygonCost(const std::vector<geometry_msgs::Point>& polygon, unsigned char cost_value, bool increase1, std::vector<SDL_Point>* obs_cells)
{
  if (obs_cells != nullptr) {
      obs_cells->clear();
  }

  // we assume the polygon is given in the global_frame... we need to transform it to map coordinates
  std::vector<MapLocation> map_polygon;
  for (unsigned int i = 0; i < polygon.size(); ++i)
  {
    MapLocation loc;
    if (!worldToMap(polygon[i].x, polygon[i].y, loc.x, loc.y))
    {
      // ("Polygon lies outside map bounds, so we can't fill it");
      return false;
    }
    map_polygon.push_back(loc);
  }

  if (increase1) {
    map_polygon = polygon_increase1(map_polygon, size_x_, size_y_);
  }

  std::vector<MapLocation> polygon_cells;

  // get the cells that fill the polygon
  convexFillCells(map_polygon, polygon_cells);

  // set the cost of those cells
  for (unsigned int i = 0; i < polygon_cells.size(); ++i)
  {
    unsigned int index = getIndex(polygon_cells[i].x, polygon_cells[i].y);
    if (obs_cells != nullptr && costmap_[index] == costmap_2d::LETHAL_OBSTACLE) {
        obs_cells->push_back(SDL_Point{(int)polygon_cells[i].x, (int)polygon_cells[i].y});
    }
    costmap_[index] = cost_value;
  }
  return true;
}

void Costmap2D::polygonOutlineCells(const std::vector<MapLocation>& polygon, std::vector<MapLocation>& polygon_cells)
{
  PolygonOutlineCells cell_gatherer(*this, costmap_, polygon_cells);
  for (unsigned int i = 0; i < polygon.size() - 1; ++i)
  {
    raytraceLine(cell_gatherer, polygon[i].x, polygon[i].y, polygon[i + 1].x, polygon[i + 1].y);
  }
  if (!polygon.empty())
  {
    unsigned int last_index = polygon.size() - 1;
    // we also need to close the polygon by going from the last point to the first
    raytraceLine(cell_gatherer, polygon[last_index].x, polygon[last_index].y, polygon[0].x, polygon[0].y);
  }
}

void Costmap2D::convexFillCells(const std::vector<MapLocation>& polygon, std::vector<MapLocation>& polygon_cells)
{
  // we need a minimum polygon of a triangle
  if (polygon.size() < 3)
    return;

  std::vector<MapLocation> polygon_line_cells;
  // first get the cells that make up the outline of the polygon
  polygonOutlineCells(polygon, polygon_line_cells);

  std::set<uint64_t> sorted_keys;
  for (unsigned int at = 0; at < polygon_line_cells.size(); ++ at) {
    const costmap_2d::MapLocation& cell = polygon_line_cells[at];

    uint64_t key = posix_mku64(cell.x, cell.y);
    if (sorted_keys.count(key) == 0) {
        sorted_keys.insert(key);
        polygon_cells.push_back(cell);
    }
  }

  // quick bubble sort to sort points by x
  MapLocation swap;
  unsigned int i = 0;
  while (i < polygon_cells.size() - 1)
  {
    if (polygon_cells[i].x > polygon_cells[i + 1].x)
    {
      swap = polygon_cells[i];
      polygon_cells[i] = polygon_cells[i + 1];
      polygon_cells[i + 1] = swap;

      if (i > 0)
        --i;
    }
    else
      ++i;
  }

  i = 0;
  MapLocation min_pt;
  MapLocation max_pt;
  unsigned int min_x = polygon_cells[0].x;
  unsigned int max_x = polygon_cells[polygon_cells.size() - 1].x;

  // walk through each column and mark cells inside the polygon
  for (unsigned int x = min_x; x <= max_x; ++x)
  {
    if (i >= polygon_cells.size() - 1)
      break;

    if (polygon_cells[i].y < polygon_cells[i + 1].y)
    {
      min_pt = polygon_cells[i];
      max_pt = polygon_cells[i + 1];
    }
    else
    {
      min_pt = polygon_cells[i + 1];
      max_pt = polygon_cells[i];
    }

    i += 2;
    while (i < polygon_cells.size() && polygon_cells[i].x == x)
    {
      if (polygon_cells[i].y < min_pt.y)
        min_pt = polygon_cells[i];
      else if (polygon_cells[i].y > max_pt.y)
        max_pt = polygon_cells[i];
      ++i;
    }

    MapLocation pt;
    // loop though cells in the column
    for (unsigned int y = min_pt.y; y <= max_pt.y; ++y)
    {
      pt.x = x;
      pt.y = y;

      uint64_t key = posix_mku64(pt.x, pt.y);
      if (sorted_keys.count(key) == 0) {
        sorted_keys.insert(key);
        polygon_cells.push_back(pt);
      }
    }
  }
}

unsigned int Costmap2D::getSizeInCellsX() const
{
  return size_x_;
}

unsigned int Costmap2D::getSizeInCellsY() const
{
  return size_y_;
}

double Costmap2D::getSizeInMetersX() const
{
  return (size_x_ - 1 + 0.5) * resolution_;
}

double Costmap2D::getSizeInMetersY() const
{
  return (size_y_ - 1 + 0.5) * resolution_;
}

double Costmap2D::getOriginX() const
{
  return origin_x_;
}

double Costmap2D::getOriginY() const
{
  return origin_y_;
}

double Costmap2D::getResolution() const
{
  return resolution_;
}
/*
bool Costmap2D::saveMap(std::string file_name)
{
  FILE *fp = fopen(file_name.c_str(), "w");

  if (!fp)
  {
    return false;
  }

  fprintf(fp, "P2\n%u\n%u\n%u\n", size_x_, size_y_, 0xff);
  for (unsigned int iy = 0; iy < size_y_; iy++)
  {
    for (unsigned int ix = 0; ix < size_x_; ix++)
    {
      unsigned char cost = getCost(ix, iy);
      fprintf(fp, "%d ", cost);
    }
    fprintf(fp, "\n");
  }
  fclose(fp);
  return true;
}
*/

const bool rose_mode = true;

bool Costmap2D::saveMap(const std::string& file_name, const std::vector<geometry_msgs::PoseStamped>& transformed_plan, const std::vector<geometry_msgs::Point>& footprint, const tpose2d_C& odom_2_map_pose, double robot_x, double robot_y, double robot_yaw, 
    double inscribed_radius, const SDL_Point& src, const SDL_Point& dst, const SDL_Rect& exclusion_rect) const
{
    tfile file(file_name, GENERIC_WRITE, CREATE_ALWAYS);
    if (!file.valid()) {
        return false;
    }

    tcostmap_header header;
    header.fourcc = SDL_FOURCC('C', 'O', 'S', 1);
    header.width = size_x_;
    header.height = size_y_;
    header.resolution = resolution_;
    header.origin_x = origin_x_;
    header.origin_y = origin_y_;

    header.transformed_plan_points = transformed_plan.size();
    header.footprint_points = footprint.size();
    header.odom_2_map_pose = odom_2_map_pose;
    header.robot_x = robot_x;
    header.robot_y = robot_y;
    header.robot_yaw = robot_yaw;
    header.inscribed_radius = inscribed_radius;
    header.src = src;
    header.dst = dst;
    header.exclusion_x = exclusion_rect.x;
    header.exclusion_y = exclusion_rect.y;
    header.exclusion_w = exclusion_rect.w;
    header.exclusion_h = exclusion_rect.h;

    // 1/4: header
    posix_fwrite(file.fp, &header, sizeof(header));

    // 2/4: transformed_plan
    if (!transformed_plan.empty()) {
        file.resize_data(transformed_plan.size() * sizeof(SDL_DPoint));
        SDL_DPoint* mem = (SDL_DPoint*)file.data;
        int at = 0;
        for (std::vector<geometry_msgs::PoseStamped>::const_iterator it = transformed_plan.begin(); it != transformed_plan.end(); ++ it, at ++) {
            const geometry_msgs::PoseStamped& src = *it;
            SDL_DPoint& to = mem[at];
            to.x = src.pose.position.x;
            to.y = src.pose.position.y;
        }
        posix_fwrite(file.fp, file.data, transformed_plan.size() * sizeof(SDL_DPoint));
    }

    // 3/4: footprint
    if (!footprint.empty()) {
        file.resize_data(footprint.size() * sizeof(SDL_DPoint));
        SDL_DPoint* mem = (SDL_DPoint*)file.data;
        int at = 0;
        for (std::vector<geometry_msgs::Point>::const_iterator it = footprint.begin(); it != footprint.end(); ++ it, at ++) {
            const geometry_msgs::Point& src = *it;
            SDL_DPoint& to = mem[at];
            to.x = src.x;
            to.y = src.y;
        }
        posix_fwrite(file.fp, file.data, footprint.size() * sizeof(SDL_DPoint));
    }

    // 4/4: map's cost
    const int cells = size_x_ * size_y_;
    file.resize_data(cells);
    int pos = 0;
    for (unsigned int iy = 0; iy < size_y_; iy++) {
        for (unsigned int ix = 0; ix < size_x_; ix++) {
            unsigned char cost = getCost(ix, iy);
            file.data[pos ++] = cost;
        }
    }
    VALIDATE(pos == cells, null_str);
    posix_fwrite(file.fp, file.data, cells);
    return true;
}

void Costmap2D::saveMap(const std::string& file_name, const tpose2d& robot_pose, const SDL_Point& src) const
{
    const std::vector<geometry_msgs::PoseStamped> transformed_plan;
    const std::vector<geometry_msgs::Point> footprint;

    saveMap(file_name, transformed_plan, footprint, tpose2d_C{0, 0, 0.0, true}, 
        robot_pose.x, robot_pose.y, robot_pose.yaw, 0.0, src, SDL_Point{nposm, nposm}, SDL_Rect{0, 0, 0, 0});
}

bool Costmap2D::loadMap(const std::string& file_name, tcostmap_header& header, std::vector<geometry_msgs::PoseStamped>& transformed_plan, std::vector<geometry_msgs::Point>& footprint)
{
    tfile file(file_name, GENERIC_READ, OPEN_EXISTING);
    if (!file.valid()) {
        return false;
    }

    int fsize = posix_fsize(file.fp);
	if (fsize < sizeof(tcostmap_header)) {
		return false;
	}
	posix_fseek(file.fp, 0);

    posix_fread(file.fp, &header, sizeof(tcostmap_header));
    if (header.fourcc != SDL_FOURCC('C', 'O', 'S', 1)) {
        return false;
    }
    const int cells = header.height * header.width;
    int footprint_bytes = header.footprint_points * sizeof(SDL_DPoint);
    int transformed_plan_bytes = header.transformed_plan_points * sizeof(SDL_DPoint);
    if (fsize != sizeof(tcostmap_header) + footprint_bytes + transformed_plan_bytes + cells) {
        return false;
    }

    // 2/4: transformed_plan
    file.resize_data(transformed_plan_bytes);
    posix_fread(file.fp, file.data, transformed_plan_bytes);
    geometry_msgs::PoseStamped to_pose;
    to_pose.pose.position.z = 0;
    SDL_DPoint* mem = (SDL_DPoint*)file.data;
    for (int at = 0; at < header.transformed_plan_points; at ++) {
        const SDL_DPoint& src = mem[at];
        to_pose.pose.position.x = src.x;
        to_pose.pose.position.y = src.y;
        transformed_plan.push_back(to_pose);
    }

    // 3/4: footprint
    file.resize_data(footprint_bytes);
    posix_fread(file.fp, file.data, footprint_bytes);
    geometry_msgs::Point to_point;
    to_point.z = 0;
    mem = (SDL_DPoint*)file.data;
    for (int at = 0; at < header.footprint_points; at ++) {
        const SDL_DPoint& src = mem[at];
        to_point.x = src.x;
        to_point.y = src.y;
        footprint.push_back(to_point);
    }

    // 4/4: map's cost
    resizeMap(header.width, header.height, header.resolution, header.origin_x, header.origin_y);
    posix_fread(file.fp, costmap_, cells);

    return true;
}

int Costmap2D::thisCost8Cells(int x, int y, unsigned char cost, std::set<tpoint>* cost_cells) const
{
    const int start_cell = getIndex(x, y);
    const int s = 1;

    int ret = 0;
    for (int y_diff = -s; y_diff <= s; y_diff ++){
        for (int x_diff = -s; x_diff <= s; x_diff ++){
            if (x_diff == 0 && y_diff == 0) {
                continue;
            }
            const int new_x = x + x_diff;
            const int new_y = y + y_diff;
            if (new_x < 0 || new_x >= (int)size_x_) {
                continue;
            }
            if (new_y < 0 || new_y >= (int)size_y_) {
                continue;
            }
            int n = start_cell + x_diff + size_x_ * y_diff;
            if (costmap_[n] == cost) {
                if (cost_cells != nullptr) {
                    cost_cells->insert(tpoint(new_x, new_y));
                }
                ret ++;
            }
        }
    }
    return ret;
}

int Costmap2D::thisCostCells(int x, int y, int radius, unsigned char cost, std::vector<SDL_Point>& cells) const
{
    VALIDATE(radius > 0, null_str);
    cells.clear();
    const int start_cell = getIndex(x, y);
    int s = radius;

    int ret = 0;
    for (int y_diff = -s; y_diff <= s; y_diff ++) {
        for (int x_diff = -s; x_diff <= s; x_diff ++) {
            const int new_x = x + x_diff;
            const int new_y = y + y_diff;
            if (new_x < 0 || new_x >= (int)size_x_) {
                continue;
            }
            if (new_y < 0 || new_y >= (int)size_y_) {
                continue;
            }
            int n = start_cell + x_diff + size_x_ * y_diff;
            if (costmap_[n] == cost) {
                ret ++;
                cells.push_back(SDL_Point{new_x, new_y});
            }
        }
    }
    return ret;
}

int Costmap2D::getMax8Cells(int x, int y, SDL_Point* locs, std::vector<SDL_Point>* v_locs) const
{
    int index = 0;
    const int s = 1;
    for (int y_diff = -s; y_diff <= s; y_diff ++){
        for (int x_diff = -s; x_diff <= s; x_diff ++){
            if (x_diff == 0 && y_diff == 0) {
                continue;
            }
            const int new_x = x + x_diff;
            const int new_y = y + y_diff;
            if (new_x < 0 || new_x >= (int)size_x_) {
                continue;
            }
            if (new_y < 0 || new_y >= (int)size_y_) {
                continue;
            }
            if (locs != nullptr) {
                locs[index].x = new_x;
                locs[index].y = new_y;
            }
            if (v_locs != nullptr) {
                v_locs->push_back(SDL_Point{new_x, new_y});
            }
            index ++;
        }
    }
    return index;
}

std::vector<SDL_Point> Costmap2D::updateObstacleToInscribed(int x, int y, int cell_inscribed_radius, double robot_yaw)
{
    const int costmap_cells = size_x_ * size_y_;
    const int start_cell = getIndex(x, y);

    // Approximate diagonal
    int x_s = cell_inscribed_radius + 1;
    int y_s = cell_inscribed_radius + 1;
    if (cell_inscribed_radius < 3) {
        x_s = cell_inscribed_radius;
        y_s = cell_inscribed_radius;
    }
/*
    double ang_diff0 = angles::shortest_angular_distance(robot_yaw, 0);
    double ang_diff1 = angles::shortest_angular_distance(robot_yaw, M_PI);
    double threshold = DEG2RAD(10); // 35
    if (fabs(ang_diff0) < threshold || fabs(ang_diff1) < threshold) {
        // Approximate horizontal
        x_s = cell_inscribed_radius + 1;
        y_s = cell_inscribed_radius;
    } else {
        ang_diff0 = angles::shortest_angular_distance(robot_yaw, M_PI / 2);
        ang_diff1 = angles::shortest_angular_distance(robot_yaw, M_PI * 1.5);
        if (fabs(ang_diff0) < threshold || fabs(ang_diff1) < threshold) {
            // Approximate vertical
            x_s = cell_inscribed_radius;
            y_s = cell_inscribed_radius + 1;
        } else {
            int ii = 0;
        }
    }
*/
    std::vector<SDL_Point> result;
    for (int y_diff = -y_s; y_diff <= y_s; y_diff ++){
        for (int x_diff = -x_s; x_diff <= x_s; x_diff ++){
            if (x_diff == 0 && y_diff == 0) {
                continue;
            }
            const int new_x = x + x_diff;
            const int new_y = y + y_diff;
            if (new_x < 0 || new_x >= (int)size_x_) {
                continue;
            }
            if (new_y < 0 || new_y >= (int)size_y_) {
                continue;
            }
            int n = start_cell + x_diff + size_x_ * y_diff;
            if (costmap_[n] == costmap_2d::LETHAL_OBSTACLE) {
                costmap_[n] = costmap_2d::INSCRIBED_INFLATED_OBSTACLE;
                result.push_back(SDL_Point{new_x, new_y});
            }
        }
    }
    return result;
}

}  // namespace costmap_2d
