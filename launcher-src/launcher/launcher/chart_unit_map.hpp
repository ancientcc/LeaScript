#ifndef CHART_UNIT_MAP_HPP_INCLUDED
#define CHART_UNIT_MAP_HPP_INCLUDED

#include "chart_unit.hpp"
#include "base_map.hpp"

class chart_controller;

class chart_unit_map: public base_map
{
public:
	chart_unit_map(chart_controller& controller, const tmap& gmap, bool consistent);

	void create_coor_map(int w, int h) override;
	void add(const map_location& loc, const base_unit* base_u);

	chart_unit* find_unit(const map_location& loc) const;
	chart_unit* find_unit(const map_location& loc, bool verlay) const;

	chart_unit* find_unit(int i) const { return static_cast<chart_unit*>(map_[i]); }

	void left_shift_units(int count);

private:
	chart_controller& controller_;
};

#endif
