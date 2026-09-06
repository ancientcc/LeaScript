#ifndef SIMPLE_UNIT_MAP_HPP
#define SIMPLE_UNIT_MAP_HPP

#include "simple_unit.hpp"
#include "base_map.hpp"

class simple_controller;

class simple_unit_map: public base_map
{
public:
	simple_unit_map(simple_controller& controller, const tmap& gmap, bool consistent);

	void add(const map_location& loc, const base_unit* base_u);

	simple_unit* find_unit(const map_location& loc) const;
	simple_unit* find_unit(const map_location& loc, bool verlay) const;
	simple_unit* find_unit(int i) const { return dynamic_cast<simple_unit*>(map_[i]); }
};

#endif
