#ifndef MKSCRIPT_UNIT_MAP_HPP_INCLUDED
#define MKSCRIPT_UNIT_MAP_HPP_INCLUDED

#include "mkscript_unit.hpp"
#include "base_map.hpp"

class mkscript_controller;

class mkscript_unit_map: public base_map
{
public:
	mkscript_unit_map(mkscript_controller& controller, const tmap& gmap, bool consistent);

	void create_coor_map(int w, int h) override;
	void add(const map_location& loc, const base_unit* base_u);

	mkscript_unit* find_unit(const map_location& loc) const;
	mkscript_unit* find_unit(const map_location& loc, bool verlay) const;
	mkscript_unit* find_unit(int i) const { return dynamic_cast<mkscript_unit*>(map_[i]); }

private:
	mkscript_controller& controller_;
};

#endif
