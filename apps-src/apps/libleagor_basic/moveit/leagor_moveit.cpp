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

#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "leagor_moveit.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>
#include "../common.hpp"


using namespace std::placeholders;

namespace aplt {

tleagor_moveit::tleagor_moveit()
	: product_(nposm)
{
	// const double joint1_0degree_ = -1.43; // -1.57, (used: -1.43)
	const double joint1_0degree_ = -1.57; // -1.57, (used: -1.43)
	const double joint1_180degree_ = 1.42;

	//
	// state: navigation
	//
	std::pair<std::map<int, std::vector<tgroup_state> >::iterator, bool> ins = common_states_.insert(std::make_pair(state_navigation, std::vector<tgroup_state>()));

	std::vector<tgroup_state>& navigation_groups = ins.first->second;
	aplt::tgroup_state navigation_all_group_state("all", std::vector<double>());
	navigation_all_group_state.values.push_back(joint1_0degree_);
	navigation_all_group_state.values.push_back(0.65);
	navigation_all_group_state.values.push_back(1.50); // 1.57(has trailer), 1.50(no trailer)
	navigation_all_group_state.values.push_back(1.57);
	navigation_all_group_state.values.push_back(0.0);
	navigation_all_group_state.values.push_back(-0.79); // Close as much as possible
	navigation_groups.push_back(navigation_all_group_state);

	//
	// state: recognize
	//
	ins = common_states_.insert(std::make_pair(state_recognize, std::vector<tgroup_state>()));

	std::vector<tgroup_state>& recognize_groups = ins.first->second;
	recognize_groups.push_back(aplt::tgroup_state("all", std::vector<double>()));
	aplt::tgroup_state& recognize_arm_state = recognize_groups.back();
	recognize_arm_state.values.push_back(joint1_0degree_);
	recognize_arm_state.values.push_back(0.7);
	recognize_arm_state.values.push_back(1.57);
	recognize_arm_state.values.push_back(1.57);
	recognize_arm_state.values.push_back(0.0);
	recognize_arm_state.values.push_back(0.70); // hand_open

	//
	// state: recognize_near
	//
	ins = common_states_.insert(std::make_pair(state_recognize_near, std::vector<tgroup_state>()));

	std::vector<tgroup_state>& recognize_near_groups = ins.first->second;
	recognize_near_groups.push_back(aplt::tgroup_state("all", std::vector<double>()));
	aplt::tgroup_state& recognize_near_arm_state = recognize_near_groups.back();
	recognize_near_arm_state.values.push_back(joint1_0degree_);
	recognize_near_arm_state.values.push_back(-0.2);  // (using)-0.2
	recognize_near_arm_state.values.push_back(0.89);   // (using)0.89
	recognize_near_arm_state.values.push_back(1.29); // 1.46/(using)1.29
	recognize_near_arm_state.values.push_back(0.0);
	recognize_near_arm_state.values.push_back(0.70); // hand_open

	//
	// state_overlook
	//
	ins = common_states_.insert(std::make_pair(state_overlook, std::vector<tgroup_state>()));

	std::vector<tgroup_state>& overlook_groups = ins.first->second;
	overlook_groups.push_back(aplt::tgroup_state("arm", std::vector<double>()));
	aplt::tgroup_state& overlook_arm_state = overlook_groups.back();
	overlook_arm_state.values.push_back(joint1_0degree_);
	overlook_arm_state.values.push_back(0.0);
	overlook_arm_state.values.push_back(0.0);
	overlook_arm_state.values.push_back(1.57);
	overlook_arm_state.values.push_back(0.0);

	overlook_groups.push_back(aplt::tgroup_state("hand", std::vector<double>()));
	aplt::tgroup_state& overlook_handle_state = overlook_groups.back();
	overlook_handle_state.values.push_back(0); // hand normal
	
	//
	// state_place
	//
	ins = common_states_.insert(std::make_pair(state_place, std::vector<tgroup_state>()));

	std::vector<tgroup_state>& place_groups = ins.first->second;
	aplt::tgroup_state place_arm_group_state1("arm", std::vector<double>());
	
	// place_arm_state1 is same as recognize_arm_state
	place_arm_group_state1.values.push_back(joint1_0degree_);
	place_arm_group_state1.values.push_back(0.3);
	place_arm_group_state1.values.push_back(0.66);
	place_arm_group_state1.values.push_back(1.0);
	place_arm_group_state1.values.push_back(0.0);
	place_groups.push_back(place_arm_group_state1);
	//

	place_groups.push_back(aplt::tgroup_state("arm", std::vector<double>()));
	aplt::tgroup_state& place_arm_state2 = place_groups.back();
	place_arm_state2.values.push_back(joint1_180degree_);
	place_arm_state2.values.push_back(-0.27);
	place_arm_state2.values.push_back(0.91);
	place_arm_state2.values.push_back(1.3);
	place_arm_state2.values.push_back(0.0);

	aplt::tgroup_state place_handle_group_state("hand", std::vector<double>());
	place_handle_group_state.values.push_back(0.45); // hand_open
	place_groups.push_back(place_handle_group_state);

	place_groups.push_back(aplt::tgroup_state("arm", std::vector<double>()));
	aplt::tgroup_state& place_arm_state3 = place_groups.back();
	place_arm_state3.values.push_back(joint1_180degree_);
	place_arm_state3.values.push_back(0.3);
	place_arm_state3.values.push_back(0.66);
	place_arm_state3.values.push_back(1.0);
	place_arm_state3.values.push_back(0.0);

	place_groups.push_back(aplt::tgroup_state("all", navigation_all_group_state.values));

	//
	// state_place_right
	//
	ins = common_states_.insert(std::make_pair(state_place_right, std::vector<tgroup_state>()));

	std::vector<tgroup_state>& place_right_groups = ins.first->second;
	place_right_groups.push_back(place_arm_group_state1);
/*
	aplt::tgroup_state& place_arm_state1 = place_right_groups.back();
	// place_arm_state1 is same as recognize_arm_state
	place_arm_state1.values.push_back(joint1_0degree_);
	place_arm_state1.values.push_back(0.3);
	place_arm_state1.values.push_back(0.66);
	place_arm_state1.values.push_back(1.0);
	place_arm_state1.values.push_back(0.0);
*/
	//
	place_right_groups.push_back(aplt::tgroup_state("arm", std::vector<double>()));
	aplt::tgroup_state& place_right_arm_state2 = place_right_groups.back();
	place_right_arm_state2.values.push_back(0);
	place_right_arm_state2.values.push_back(-1.1);
	place_right_arm_state2.values.push_back(0.66);
	place_right_arm_state2.values.push_back(1.0);
	place_right_arm_state2.values.push_back(0.0);

	place_right_groups.push_back(place_handle_group_state);
}


tleagor_moveit::~tleagor_moveit()
{
}

int tleagor_moveit::get_serial_path(std::string& path)
{
	const trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

	path = aplt_prefs.get_str("moveit_serial");
    return aplt_prefs.get_int("moveit_serial_baudrate", nposm);
}

void tleagor_moveit::pre_start_moveit()
{
	VALIDATE_IN_MAIN_THREAD();
    const std::string& product_str = aplt::curr_aplt->prefs.get_str("base_product");
	product_ = base_product_from_str(product_str);
}

void tleagor_moveit::start_moveit(bool& exit, const std::string& serial_path, int baudrate)
{
	VALIDATE_NOT_MAIN_THREAD();

	start_base__node(exit, serial_path, baudrate, product_);
}


}

void* aplt_create_moveit_slot()
{
	aplt::tleagor_moveit* leagor = new aplt::tleagor_moveit();
	// if (!leagor->xfyun().libmsc_loaded()) {
	//	delete leagor;
	//	return nullptr;
	// }

	aplt::tmoveit_slot* result = static_cast<aplt::tmoveit_slot*>(leagor);
	return result;
}
