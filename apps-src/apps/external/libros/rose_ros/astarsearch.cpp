/* $Id: astarsearch.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
                 2005 - 2010 by Guillaume Melquiond <guillaume.melquiond@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "rose_util.hpp"

#include <queue>
#include <map>
#include <set>

#include <rose_ros/pathfind.hpp>
#include "rose_exception.hpp"
#include <SDL_log.h>
#include "rose_config_3rdparty.hpp"

namespace pathfind {

double heuristic(const SDL_Point& src, const SDL_Point& dst, const costmap_2d::Costmap2D& costmap)
{
	// We will mainly use the distances in hexes
	// but we subtract a tiny bonus for shorter Euclidean distance
	// based on how the path looks on the screen.

	int x_diff = (src.x - dst.x);
	int y_diff = (src.y - dst.y);

	int sq_dist = x_diff * x_diff + y_diff * y_diff;
	double dist = sqrt(sq_dist);
	// dist *= 1; // while_times:283 while_times: 603
	dist *= 16; // while_times:283 while_times: 603
	// dist *= 128; // while_time:730  while_time2:311
	// dist *= 2000; // while_times:2621 while_times: 7853

	return dist;
}

// values 0 and 1 mean uninitialized
static const uint32_t bad_search_counter = 0;
// The number of nodes already processed.
static uint32_t search_counter = bad_search_counter;

struct node {
	double g, h, t;
	SDL_Point curr, prev;
	/**
	 * If equal to search_counter, the node is off the list.
	 * If equal to search_counter + 1, the node is on the list.
	 * Otherwise it is outdated.
	 */
	uint32_t in;

	// nodes.resize(width * height) require node()
	node()
		: g(1e25)
		, h(1e25)
		, t(1e25)
		, curr({0, 0})
		, prev({0, 0})
		, in(bad_search_counter)
	{
	}

	node(double s, const SDL_Point &c, const SDL_Point &p, const SDL_Point &dst, const costmap_2d::Costmap2D& costmap)
		: g(s)
		, h(heuristic(c, dst, costmap))
		, t(g + h)
		, curr(c)
		, prev(p)
		, in(search_counter + 1)
	{
		VALIDATE(curr.x >= 0 && curr.y >= 0, null_str);
	}

	bool operator<(const node& o) const {
		return t < o.t;
	}
};

class comp {
	const std::vector<node>& nodes_;

public:
	comp(const std::vector<node>& n) : nodes_(n) { }
	bool operator()(int a, int b) {
		return nodes_[b] < nodes_[a];
	}
};

class indexer {
	int h_, w_;

public:
	indexer(int h, int w) : h_(h), w_(w) { }
	int operator()(int x, int y) {
		return y * h_ + x;
	}

	int operator()(const SDL_Point& loc) {
		return loc.y * h_ + loc.x;
	}
};

int hash_size;
struct cost_hash {
	double cost;
	uint32_t in;
	int inscribeds;
};
cost_hash* hash = nullptr;

void reallocate_hash(int width, int height)
{
	if (width == 0 || height == 0) {
		return;
	}
	int size = width * height;
	if (hash != nullptr) {
		if (size > hash_size) {
			free(hash);
			hash = (cost_hash*)malloc(sizeof(cost_hash) * size);
			hash_size = size;
		}
	} else {
		hash = (cost_hash*)malloc(sizeof(cost_hash) * size);
		hash_size = size;
	}
	for (int i = 0; i < size; i ++) {
		hash[i].in = search_counter - 2;
	}
}

void release_hash()
{
	if (hash != nullptr) {
		free(hash);
		hash = nullptr;
	}
}

int NoPathValue_cells(const SDL_Point& curr, const cost_calculator& calc, const costmap_2d::Costmap2D& costmap)
{
	const int width = costmap.getSizeInCellsX();
	const int height = costmap.getSizeInCellsY();
	indexer index(width, height);
	SDL_Point locs[8];

	int inscribeds;
	int ret = 0;
	const int count = costmap.getMax8Cells(curr.x, curr.y, locs, nullptr);
	for (int i = 0; i < count; i++) {
		const SDL_Point i_loc{(int)locs[i].x, (int)locs[i].y};
		if (calc.cost(i_loc, inscribeds) == calc.NoPathValue) {
			ret ++;
		}
	}
	return ret;
}

plain_route a_star_search(int type, const SDL_Point& src, const SDL_Point& origin_dst, double stop_at, 
	const cost_calculator& calc, const costmap_2d::Costmap2D& costmap, int cell_inscribed_radius, const SDL_Rect& exclusion_rect, bool verbose)
{
	VALIDATE(type == search_global_plan || type == search_move, null_str);

	const int width = costmap.getSizeInCellsX();
	const int height = costmap.getSizeInCellsY();
	//----------------- PRE_CONDITIONS ------------------
	VALIDATE(width > 0, null_str);
	VALIDATE(width == height, null_str);
	VALIDATE(cell_inscribed_radius > 0, null_str);
	//---------------------------------------------------

	if (stop_at == 0) {
		stop_at = calc.default_stop_at(width);
	}

	// const unsigned char dst_cell_cost = costmap.getCharMap()[costmap.getIndex(origin_dst.x, origin_dst.y)];
	// VALIDATE(dst_cell_cost != costmap_2d::NO_INFORMATION, null_str);

	// A* search: $src -> $dst
	SDL_Point dst = origin_dst; // dst maybe change
	int inscribeds;
	
	double sq_max_stop_dist = -1; // -1
	double sq_min_move_dist = width * height; // default don't conside min_move
	double dst_cost = calc.cost(dst, inscribeds);
	// calculate dst's cost, must use block.
	if (type == search_global_plan) {
		double max_stop_dist = -1;
		if (dst_cost == calc.NoPathValue || dst_cost == calc.NoInformation) {
			max_stop_dist = cell_inscribed_radius + 3;

		} else if (dst_cost == calc.InscribedInflatedObstacle) {
			max_stop_dist = cell_inscribed_radius - 1;
		}

		// if dist > 1.0m, bonus 1 cell every 0.1m.
		const double min_bonus_dist = max_stop_dist == -1? 1.1: 1.2;
		double dist = hypot(src.x - dst.x, src.y - dst.y) * costmap.getResolution();
		if (dist >= min_bonus_dist) {
			double bonus_dist = dist - min_bonus_dist;
			double bonus = bonus_dist / 2;
			// '1' cell is (1.1m - 1.0m)'s bonus.
			double bonus_cells = 1 + bonus / costmap.getResolution();
			// dist = 1.4 ==> bonus = 0.3 ==> bonus_cells = 4
			if (max_stop_dist == -1) {
				max_stop_dist = 0;
			}
			max_stop_dist += bonus_cells;
		}
		if (max_stop_dist != -1) {
			sq_max_stop_dist = max_stop_dist * max_stop_dist;
		}


	} else if (type == search_move) {
		sq_min_move_dist = 4 * 4; // 0.2cm * 0.2cm
	}

	SDL_Point locs[8];

	// increment search_counter but skip the range equivalent to uninitialized
	search_counter += 2;
	if (search_counter - bad_search_counter <= 1u) {
		search_counter += 2;
	}
	
	reallocate_hash(width, height);
	static std::vector<node> nodes;
	nodes.resize(width * height);  // this create uninitalized nodes

	indexer index(width, height);
	comp node_comp(nodes);

	nodes[index(dst)].g = stop_at + 1;
	nodes[index(src)] = node(0, src, SDL_Point{nposm, nposm}, dst, costmap);

	// rember destination cost.
	// is destination is all, don't remember! make below code find out right path.
	// [see remark#40]
	hash[index(dst)].in = search_counter;
	hash[index(dst)].cost = dst_cost;
	hash[index(dst)].inscribeds = inscribeds;

	std::vector<int> pq;
	pq.push_back(index(src));

	int while_times = 0;
	int better_times2 = 0;
	// if (verbose) {
		SDL_Log("===a_star_search=== (%i, %i) -> (%i, %i), stop_at: %.5f, sq_max_stop_dist: %.5f", src.x, src.y, dst.x, dst.y, stop_at, sq_max_stop_dist);
	// }

	int n_loops = 0;
	const int costmap_cells = width * height;
	while (!pq.empty()) {
		node& n = nodes[pq.front()];
		if (verbose) {
			SDL_Log("while#%i n(%i, %i).g(%.5f).t(%.5f), pq.size: %i", while_times ++, n.curr.x, n.curr.y, n.g, n.t, (int)pq.size());
		}

		n.in = search_counter;

		std::pop_heap(pq.begin(), pq.end(), node_comp);
		pq.pop_back();

		if (n.t >= nodes[index(dst)].g) {
			if (verbose) {
				SDL_Log("terminate a_star_search, n.t(%.5f).g(%.5f)(%i, %i) >= nodes[index(dst)].g(%.5f)[%i, %i]", 
					n.t, n.g, n.curr.x, n.curr.y, nodes[index(dst)].g, dst.x, dst.y);
			}
			break;
		}

		n_loops ++;
		const int count = costmap.getMax8Cells(n.curr.x, n.curr.y, locs, nullptr);
		for (int i = 0; i < count; i++) {
			const int i_index = index(locs[i].x, locs[i].y);
			VALIDATE(i_index >= 0 && i_index < costmap_cells, null_str);

			const SDL_Point i_loc{(int)locs[i].x, (int)locs[i].y};
			if (SDL_PointInRect(&i_loc, &exclusion_rect)) {
				continue;
			}
			node& next = nodes[i_index];

			double threshold = (next.in - search_counter <= 1u) ? next.g : stop_at + 1;
			if (n.g + 1 >= threshold) {
				// cost() is always >= 1  (assumed and needed by the heuristic)
				// but now, cost() maybe 0.
				continue;
			}

			if (verbose && next.in - search_counter <= 1u) {
				SDL_Log("better#%i next(%i, %i) has g(%.5f), but this may has better g(%.5f + ?)  next.in: %u search_counter: %u", 
					better_times2 ++, next.curr.x, next.curr.y, next.g, n.g, next.in, search_counter);
			}

			double cost;
			if (hash[i_index].in == search_counter) {
				cost = hash[i_index].cost;
			} else {
				cost = calc.cost(i_loc, inscribeds);
				if (verbose) {
					SDL_Log("next(%i, %i) from n(%i, %i), calc->cost() => %.5f", i_loc.x, i_loc.y, n.curr.x, n.curr.y, cost);
				}
				{
					hash[i_index].in = search_counter;
					hash[i_index].cost = cost;
					hash[i_index].inscribeds = inscribeds;
				}
			}
			cost += n.g;
			// double cost = n.g + calc->cost(locs[i], n.g);
			if (cost >= threshold) {
				continue;
			}

			// may change dst?
			const SDL_Point& target_loc = type == search_global_plan? origin_dst: src;
			int x_diff = locs[i].x - target_loc.x;
			int y_diff = locs[i].y - target_loc.y;
			const int sq_dist = x_diff * x_diff + y_diff * y_diff;
			if (type == search_global_plan) {
				if (sq_dist <= sq_max_stop_dist) {
					dst.x = locs[i].x;
					dst.y = locs[i].y;
					if (verbose) {
						SDL_Log("[search_global_plan]sq_dist(%i) >= sq_max_stop_dist(%.3f), set locs[i]:(%i, %i) to dst",
							sq_dist, sq_max_stop_dist, locs[i].x, locs[i].y);
					}
					// why -0.1? --make late sq_dist not enter here. 
					sq_max_stop_dist = sq_dist - 0.1;
				}
			} else if (type == search_move) {
				if (sq_dist >= sq_min_move_dist) {
					dst.x = locs[i].x;
					dst.y = locs[i].y;
					if (verbose) {
						SDL_Log("[search_move]sq_dist(%i) >= sq_min_move_dist(%.3f), set locs[i]:(%i, %i) to dst",
							sq_dist, sq_min_move_dist, locs[i].x, locs[i].y);
					}
					sq_min_move_dist = sq_dist;
				}
			}

			bool in_list = next.in == search_counter + 1;

			next = node(cost, i_loc, n.curr, dst, costmap);

			if (in_list) {
				std::push_heap(pq.begin(), std::find(pq.begin(), pq.end(), i_index) + 1, node_comp);
			} else {
				pq.push_back(i_index);
				std::push_heap(pq.begin(), pq.end(), node_comp);
			}
		}
	}

	if (verbose) {
		SDL_Log("while_times:%i better_times2: %i, pose while, n_loops: %i, pq.size: %i", while_times, better_times2, n_loops, (int)pq.size());
	}

	plain_route route;
	route.n_loops = n_loops;
	if (nodes[index(dst)].g <= stop_at) {
		int i_index;
		route.move_cost = static_cast<int>(nodes[index(dst)].g);
		for (node curr = nodes[index(dst)]; curr.prev.x != nposm; curr = nodes[index(curr.prev)]) {
			i_index = index(curr.curr.x, curr.curr.y);
			if (game_config::os == os_windows) {
				VALIDATE(hash[i_index].in == search_counter, null_str);
			}
			route.steps.push_back(SDL_Rect{ curr.curr.x, curr.curr.y, hash[i_index].inscribeds, (int)hash[i_index].cost});
		}
		i_index = index(src.x, src.y);
		route.steps.push_back(SDL_Rect{src.x, src.y, hash[i_index].inscribeds, (int)hash[i_index].cost});
		std::reverse(route.steps.begin(), route.steps.end());

	} else {
		// aborted a* search
		route.move_cost = static_cast<int>(calc.NoPathValue);
	}

	release_hash();
	return route;
}

}
