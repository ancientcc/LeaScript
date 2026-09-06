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

#ifndef LIBROSE_LEAGOR_LASER_HPP_INCLUDED
#define LIBROSE_LEAGOR_LASER_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "aplt2.hpp"

extern int rplidar_ros__rplidarNode(bool& exit, const std::string& serial_path, int baudrate);
extern int sc_mini__sc_mini(bool& exit, const std::string& serial_path, int baudrate);
extern int lslidar_driver__lslidar_driver_node(bool& exit, const std::string& serial_path, int baudrate, const std::string& lidar_name);

namespace aplt {

class tleagor_laser: public tlaser_slot
{
public:
	tleagor_laser() {}
	~tleagor_laser() {}

private:
	int get_serial_path(std::string& path) override;
	void pre_start_laser() override;
	void start_laser(bool& exit, const std::string& serial_path, int baudrate) override;

private:
	std::string product_;
};

}

#endif

