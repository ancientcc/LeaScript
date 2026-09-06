#ifndef MAP_UNIT_HPP_INCLUDED
#define MAP_UNIT_HPP_INCLUDED

#include "base_unit.hpp"

class map_controller;
class map_display;
class map_unit_map;

#define UNIT_LOCS	2
#define MAX_GLOBAL_PLAN	40 // plans_per_cel(20) * UNIT_LOCS 
#define MAX_LOCAL_PLAN	10

class map_unit: public base_unit
{
public:
	map_unit(map_controller& controller, map_display& disp, map_unit_map& units);
	~map_unit();

private:
	// bool require_sort() const override { return true; }
	void app_draw_unit(const int xsrc, const int ysrc) override;

	void draw_unit_laser(const int xsrc, const int ysrc);
	void draw_second_points(bool global, SDL_Point* points, int point_vsize);
	void draw_unit_points(bool global, const int xsrc, const int ysrc, const SDL_Rect& raw_rose_map_rect, float scale_ratio);

protected:
	map_controller& controller_;
	map_display& disp_;
	map_unit_map& units_;

	SDL_Point* points_;
};

#endif
