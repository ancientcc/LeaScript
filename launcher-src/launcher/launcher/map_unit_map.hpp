#ifndef MAP_UNIT_MAP_HPP_INCLUDED
#define MAP_UNIT_MAP_HPP_INCLUDED

#include "map_unit.hpp"
#include "base_map.hpp"

class map_controller;

class map_unit_map: public base_map
{
public:
	map_unit_map(map_controller& controller, const tmap& gmap, bool consistent);

	void add(const map_location& loc, const base_unit* base_u);

	map_unit* find_unit(const map_location& loc) const;
	map_unit* find_unit(const map_location& loc, bool verlay) const;
	map_unit* find_unit(int i) const { return dynamic_cast<map_unit*>(map_[i]); }
	void create_coor_map(int w, int h) override;

private:
	map_controller& controller_;
};

#endif
