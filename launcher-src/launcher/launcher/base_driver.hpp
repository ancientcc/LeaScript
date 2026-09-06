/* $Id: dialog.hpp 50956 2011-08-30 19:41:22Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LAUNCHER_BASE_DRIVER_HPP
#define LAUNCHER_BASE_DRIVER_HPP

#include "base_driver_core.hpp"

class tros_instance;
class tros_base_node;

struct trobot_imu
{
	trobot_imu()
		: has_magnetometer(false)
		, reposition_threshold(60 * 1000) // 1 min
		, require_full2_position(true)
		, has_result_ok(false)
		, last_full2_ticks(0)
	{}
	// Prevent the presence of a second trobot_imu in the system.
	posix_noncopyable(trobot_imu);

	void set_base_pitch(double pitch);

	void set_base_yaw(tbase_driver_core& base_driver, double robot_yaw);

	bool has_magnetometer;
	trpy base;

	bool require_full2_position;
	bool has_result_ok;
	const int reposition_threshold;
	uint32_t last_full2_ticks;

	trpy tmp;
};

class tbase_driver: public tbase_driver_core
{
public:
	tbase_driver(std::map<aplt::taplt_key, aplt::tapplet>& applets, tros_instance& ros_instance, 
		tros_base_node& base_node, trobot_imu& robot_imu, tcamera& camera, aplt::tbg_task& bg_task, 
		aplt::tcfg_cpp_api_core& cfg_cpp_api, aplt::thealth& health, tdrivers_core& drivers, tprivacy& privacy);

	~tbase_driver() {}

	void set_slot(const std::string& _aplt_id, aplt::tbase_slot* _slot) override;

private:
	void app_base_node_start_pre() override;
	void app_base_node_start_post() override;

private:
	tros_instance& ros_instance_;
	tros_base_node& base_node_;
	trobot_imu& robot_imu_;
};

#endif

