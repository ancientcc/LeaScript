/* $Id: pathfind.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2010 by Guillaume Melquiond <guillaume.melquiond@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Various pathfinding functions and utilities.
 */


#include <rose_ros/pathfind.hpp>
#include "rose_exception.hpp"
#include "rose_config_3rdparty.hpp"

#include <SDL_log.h>
#include <vector>
#include <algorithm>

namespace pathfind {

const double cost_calculator::NoPathValue = 42424242.0;
const double cost_calculator::NoInformation = costmap_2d::INSCRIBED_INFLATED_OBSTACLE * 2.5; // 1.5
const double cost_calculator::InscribedInflatedObstacle = costmap_2d::INSCRIBED_INFLATED_OBSTACLE * 2.5;
// const double cost_calculator::InscribedInflatedObstacle = costmap_2d::INSCRIBED_INFLATED_OBSTACLE * 3.0;

double cost_calculator::default_stop_at(int costmap_size)
{
    VALIDATE(costmap_size > 0, null_str);
    // 60 ==> 19734.0
    return costmap_size * 0.75 * InscribedInflatedObstacle;
}

void plain_route::clear()
{
    steps.clear();
	move_cost = 0;
	n_loops = 0;
}

std::string plain_route::toString() const
{
    char buf[32];
    const int avg_move_cost = !steps.empty()? move_cost / steps.size(): nposm;
    SDL_snprintf(buf, sizeof(buf), "{%i(avg:%i) - %i - [%i]", move_cost, avg_move_cost, n_loops, (int)steps.size());
    std::stringstream ss;
    ss << buf;
    if (game_config::os == os_windows) {
        for (std::vector<SDL_Rect>::const_iterator it = steps.begin(); it != steps.end(); ++ it) {
            const SDL_Rect& step = *it;
            if (!ss.str().empty()) {
                ss << " ";
            }
            ss << "(" << step.x << "," << step.y << ":" << step.w << ")";
        }
    }
    ss << "}";
    return ss.str();
}

shortest_path_calculator::shortest_path_calculator(const costmap_2d::Costmap2D& costmap, int cell_inscribed_radius)
	: costmap_(costmap)
	, cell_inscribed_radius_(cell_inscribed_radius)
{
}

double shortest_path_calculator::cost(const SDL_Point& loc, int& inscribeds) const
{
    const int x = loc.x;
    const int y = loc.y;
    const int size_x_ = costmap_.getSizeInCellsX();
    const int size_y_ = costmap_.getSizeInCellsY();
	const unsigned char* costs = costmap_.getCharMap();

	inscribeds = 0;

	const int costmap_cells = size_x_ * size_y_;
    const int start_cell = costmap_.getIndex(x, y);
    const int s = cell_inscribed_radius_;
    double result_cost = 0;

    const bool sum = true;
    const double min_cost_ = 1;
    int neg_s = s < 3? s: s - 1;
    int pos_s = s < 3? s: s - 1;
    int count = 0;
    for (int y_diff = -neg_s; y_diff <= pos_s; y_diff ++) {
        for (int x_diff = -neg_s; x_diff <= pos_s; x_diff ++) {
            const int new_x = x + x_diff;
            const int new_y = y + y_diff;
            if (new_x < 0 || new_x >= size_x_) {
                // No enter off-map area
                return NoPathValue;
                // continue;
            }
            if (new_y < 0 || new_y >= size_y_) {
                // No enter off-map area
                return NoPathValue;
                // continue;
            }
            
            int n = start_cell + x_diff + size_x_ * y_diff;
            double cell_cost = costs[n];

            if (cell_cost == costmap_2d::LETHAL_OBSTACLE) {
                return NoPathValue;

            } else if (cell_cost == costmap_2d::NO_INFORMATION) { 
                cell_cost = NoInformation;

            } else if (cell_cost == costmap_2d::INSCRIBED_INFLATED_OBSTACLE) {
                cell_cost = InscribedInflatedObstacle;
                inscribeds ++;

            } else if (cell_cost < min_cost_) {
                cell_cost = min_cost_;
            }

            if (sum) {
                result_cost += cell_cost;
                count ++;

            } else if (cell_cost > result_cost) {
                result_cost = cell_cost;
            }
            // SDL_Log("cell(%i, %i), cell_cost: %.5f, [%i]result_cost: %.5f",
            //    new_x, new_y, cell_cost, count, result_cost);
        }
    }

    if (sum) {
       result_cost = result_cost / count;
    }
	return result_cost;
}

}

