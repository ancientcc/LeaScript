/* $Id: pathfind.hpp 47637 2010-11-21 13:58:12Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


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
 * This module contains various pathfinding functions and utilities.
 */

#ifndef LIBROS_PATHFIND_HPP
#define LIBROS_PATHFIND_HPP


#include "rose_util.hpp"

#include <map>
#include <list>
#include <functional>
#include <costmap_2d/costmap_2d.h>
#include <costmap_2d/cost_values.h>

namespace pathfind {

struct cost_calculator
{
	cost_calculator() {}

	virtual double cost(const SDL_Point& loc, int& inscribeds) const = 0;
	virtual ~cost_calculator() {}

	static const double NoPathValue;
	static const double NoInformation;
	static const double InscribedInflatedObstacle;
	static double default_stop_at(int costmap_size);
};

/** Structure which holds a single route between one location and another. */
struct plain_route
{
	plain_route()
	{
		clear();
	}

	void clear();
	std::string toString() const;

	std::vector<SDL_Rect> steps;
	/** Movement cost for reaching the end of the route. */
	int move_cost;
	int n_loops;
};

int NoPathValue_cells(const SDL_Point& curr, const cost_calculator& calc, const costmap_2d::Costmap2D& costmap);

enum {search_global_plan, search_move};
plain_route a_star_search(int type, const SDL_Point& src, const SDL_Point& dst, double stop_at,
	const cost_calculator& calc, const costmap_2d::Costmap2D& costmap, 
	int cell_inscribed_radius, const SDL_Rect& exclusion_rect, bool verbose);

/**
 * Add marks on a route @a rt assuming that the unit located at the first hex of
 * rt travels along it.
 */

struct shortest_path_calculator : cost_calculator
{
	shortest_path_calculator(const costmap_2d::Costmap2D& costmap, int cell_inscribed_radius);
	double cost(const SDL_Point& loc, int& inscribeds) const override;

private:
	const costmap_2d::Costmap2D& costmap_;
	const int cell_inscribed_radius_;
};

}

#endif // LIBROS_PATHFIND_HPP
