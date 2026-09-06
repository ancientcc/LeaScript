/* $Id: dialog.cpp 50956 2011-08-30 19:41:22Z mordante $ */
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

#define GETTEXT_DOMAIN "launcher-lib"

#include "base_driver.hpp"
#include "ros_instance.hpp"

tbase_driver::tbase_driver(std::map<aplt::taplt_key, aplt::tapplet>& applets, tros_instance& ros_instance, 
		tros_base_node& base_node, trobot_imu& robot_imu, tcamera& camera, aplt::tbg_task& bg_task, 
		aplt::tcfg_cpp_api_core& cfg_cpp_api, aplt::thealth& health, tdrivers_core& drivers, tprivacy& privacy)
	: tbase_driver_core(applets, base_node, camera, bg_task, 
		cfg_cpp_api, health, drivers, privacy)
	, ros_instance_(ros_instance)
	, base_node_(base_node)
	, robot_imu_(robot_imu)
{}

void tbase_driver::set_slot(const std::string& _aplt_id, aplt::tbase_slot* _slot)
{
	if (slot != nullptr) {
		VALIDATE(mode_ == nposm, null_str);

		if (base_node_.started()) {
			VALIDATE(base_node_.registered, null_str);
			stop_node();
		}
		if (base_node_.registered) {
			ros_instance_.deregister_slot(base_node_);
		}

		delete slot;
		slot = nullptr;

		aplt::valuex.nposm_NMTHREAD_battery_level();
		aplt::valuex.battery_level = float_nposm;

	} else {
		// VALIDATE(slot->external_imu() == nullptr, null_str);
	}

	if (_slot != nullptr) {
		VALIDATE(!_aplt_id.empty(), null_str);
		slot = _slot;

	} else {
		VALIDATE(_aplt_id.empty(), null_str);
	}
	aplt_id_ = _aplt_id;

	if (slot != nullptr) {
		robot_imu_.has_magnetometer = slot->has_magnetometer();
		battery_info_ = slot->get_battery_info();

		last_install_ticks_ = SDL_GetTicks();
		long_no_battery_ticks_ = SDL_GetTicks() + long_no_battery_mask_ms_;

		// game_config::os == os_windows? "COM4": "/dev/ttyS7";
		std::string serial_path = preferences::im948serial();
		if (!serial_path.empty() && slot->use_external_imu()) {
			VALIDATE(slot->external_imu() == nullptr, null_str);
			slot->open_external_imu(serial_path, 115200, robot_imu_.has_magnetometer);
		}

	} else {
		last_install_ticks_ = 0;
	}
}

void tbase_driver::app_base_node_start_pre()
{
	if (!base_node_.registered) {
		// After one start, it stays at 'ros_instance.slots' all the time. Even if it was stopped.
		ros_instance_.register_slot(base_node_);
	}
}

void tbase_driver::app_base_node_start_post()
{
/*
	aplt::tbase_ext_lamp* lamp = slot->query_ext_lamp();
	if (lamp != nullptr) {
		lamp->lamp_ctrl_led(aplt::tbase_ext_lamp::ledtype_privacy, 
			privacy_.protect()? aplt::tbase_ext_lamp::ledact_on: aplt::tbase_ext_lamp::ledact_off);
	}
*/
}