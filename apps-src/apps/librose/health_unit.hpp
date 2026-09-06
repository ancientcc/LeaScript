#ifndef HEALTH_UNIT_HPP_INCLUDED
#define HEALTH_UNIT_HPP_INCLUDED

#include "base_unit.hpp"

class health_controller;
class health_display;
class health_unit_map;

#define HEAL_UNIT_LOCS	1  // 2
#define VERT_LOCS_PER_CHART	5	// 5
enum {chart_posture, chart_routine, chart_workout, chart_count};

class health_unit: public base_unit
{
public:
	health_unit(health_controller& controller, health_display& disp, health_unit_map& units);

private:
	void app_draw_unit(const int xsrc, const int ysrc) override;

protected:
	health_controller& controller_;
	health_display& disp_;
	health_unit_map& units_;
};

#endif
