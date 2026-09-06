#ifndef UNIT_HPP
#define UNIT_HPP

#include "base_unit.hpp"

class simple_controller;
class simple_display;
class simple_unit_map;

class simple_unit: public base_unit
{
public:
	simple_unit(simple_controller& controller, simple_display& disp, simple_unit_map& units);

private:
	void app_draw_unit(const int xsrc, const int ysrc) override;

protected:
	simple_controller& controller_;
	simple_display& disp_;
	simple_unit_map& units_;
};

#endif
