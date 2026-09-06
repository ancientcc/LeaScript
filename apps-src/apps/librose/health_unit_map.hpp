#ifndef HEALTH_UNIT_MAP_HPP_INCLUDED
#define HEALTH_UNIT_MAP_HPP_INCLUDED

#include "health_unit.hpp"
#include "base_map.hpp"

class health_controller;

class health_unit_map: public base_map
{
public:
	health_unit_map(health_controller& controller, const tmap& gmap, bool consistent);

	void create_coor_map(int w, int h) override;
	void add(const map_location& loc, const base_unit* base_u);

	health_unit* find_unit(const map_location& loc) const;
	health_unit* find_unit(const map_location& loc, bool verlay) const;
	health_unit* find_unit(int i) const { return dynamic_cast<health_unit*>(map_[i]); }

private:
	health_controller& controller_;
};

#endif
