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

#include "moveit_driver.hpp"
#include "game_config.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "aplt.hpp"
#include "ros_instance.hpp"

using namespace std::placeholders;

namespace aplt {

void tvariable_positions::set_position(int at, double val)
{
	VALIDATE(at >= 0 && at < count, null_str);
	positions[at] = val;
}

std::string tvariable_positions::to_string() const
{
	std::stringstream ss;
	ss << "(";
	for (int at = 0; at < count; at ++) {
		if (at != 0) {
			ss << " ";
		}
		ss << positions[at];
	}
	ss << ")";

	return ss.str();
}

}

tmoveit_driver::tmoveit_driver(tros_instance& ros_instance, const std::map<aplt::taplt_key, aplt::tapplet>& applets)
	: slot(nullptr)
	, ros_instance_(ros_instance)
	, applets_(applets)
	, shared_thread_(false)
{}

void tmoveit_driver::set_slot(const std::string& _aplt_id, aplt::tmoveit_slot* _slot)
{
	const aplt::tmoveit_slot* original_slot = slot;
	if (slot != nullptr) {
		VALIDATE(!shared_thread_, null_str);
		delete slot;
		slot = nullptr;
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
		VALIDATE(!ros_instance_.is_moveit_model_valid(), null_str);

		const aplt::tapplet* aplt = aplt::aplt_from_id(applets_, _aplt_id);
		// const std::string name = "tank_arm";
		const std::string name = "wheeltec_mec_six";
		ros_instance_.load_moveit_model(aplt->res_path, name, false);
		VALIDATE(ros_instance_.is_moveit_model_valid(), null_str);

	} else if (original_slot != nullptr) {
		VALIDATE(ros_instance_.is_moveit_model_valid(), null_str);
		ros_instance_.clear_moveit_model();
	}
}

const std::vector<aplt::tgroup_state>& tmoveit_driver::get_common_state(int state)
{
	VALIDATE(slot != nullptr, null_str);
	return slot->get_common_state(state);
}

void tmoveit_driver::slice()
{
	if (slot != nullptr) {
		// slot->slice();
	}
}

void tmoveit_driver::start_moveit(const std::string& serial_path, int baudrate)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	VALIDATE(thread_.get() == nullptr, null_str);

	slot->pre_start_moveit();
	// VALIDATE(driver_main != nullptr, null_str);
	thread_.reset(new net::tworker(std::bind(&aplt::tmoveit_slot::start_moveit, slot, _1, serial_path, baudrate), NULL, NULL, NULL, "moveit_driver_node"));
}

// void tmoveit_driver::set_thread(net::tshared_worker& thread)
void tmoveit_driver::set_thread()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	VALIDATE(thread_.get() == nullptr, null_str);
	// VALIDATE(thread.get() != nullptr, null_str);

	VALIDATE(!shared_thread_, null_str);

	slot->pre_start_moveit();
	// thread_ = thread;
	shared_thread_ = true;
}

void tmoveit_driver::stop_moveit()
{
	VALIDATE_IN_MAIN_THREAD();
	if (shared_thread_) {
		VALIDATE(thread_.get() == nullptr, null_str);
		shared_thread_ = false;

	} else {
		VALIDATE(thread_.get() != nullptr, null_str);
		thread_.reset(nullptr);
	}
}