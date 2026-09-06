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

#include "leagor_laser.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>

using namespace std::placeholders;

namespace aplt {

int tleagor_laser::get_serial_path(std::string& path)
{
	const trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

	path = aplt_prefs.get_str("laser_serial");
    return aplt_prefs.get_int("laser_serial_baudrate", nposm);
}

void tleagor_laser::pre_start_laser()
{
	VALIDATE_IN_MAIN_THREAD();
	const config& cfg = aplt::curr_aplt->prefs.cfg();
    product_ = aplt::curr_aplt->prefs.get_str("laser_product");
}

void tleagor_laser::start_laser(bool& exit, const std::string& serial_path, int baudrate)
{
	VALIDATE_NOT_MAIN_THREAD();
	bool rplidar = product_ == "rplidar_a1m8";

    if (rplidar) {
        rplidar_ros__rplidarNode(exit, serial_path, baudrate);
    } else {
		// lslidar_n10
		lslidar_driver__lslidar_driver_node(exit, serial_path, baudrate, "N10");
        // sc_mini__sc_mini(exit, serial_path, baudrate)
    }
}

}

void* aplt_create_laser_slot()
{
	aplt::tleagor_laser* leagor = new aplt::tleagor_laser();
	// if (!leagor->xfyun().libmsc_loaded()) {
	//	delete leagor;
	//	return nullptr;
	// }

	aplt::tlaser_slot* result = static_cast<aplt::tlaser_slot*>(leagor);
	return result;
}