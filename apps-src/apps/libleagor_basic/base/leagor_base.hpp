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

#ifndef LIBROSE_LEAGOR_BASE_HPP_INCLUDED
#define LIBROSE_LEAGOR_BASE_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "aplt2.hpp"
#include "../common.hpp"

namespace aplt {

class tleagor_base: public tbase_slot
{
public:
	tleagor_base(const std::string& sn, const std::string& cpuid);
	~tleagor_base();

private:
	// void pre_start() override;
	// void did_capture_audio(uint8_t* stream, int len) override;
	bool use_external_imu() const override;
	bool has_magnetometer() const override;
	void open_external_imu(const std::string& serial_path, int baudrate, bool use_magnetometer) override;

	bool imu_can_read() const override;

	int get_serial_path(std::string& path, std::string& model) override;
	void aplt_pre_start_ros_node(const std::string& model, const std::string& serial_dev, int baudrate) override;
	void aplt_start_ros_node(bool& exit, const std::string& serial_dev, int baudrate) override;

	void aplt_enter_navigation(bool buildmap, ros::tbase_cfg& base_cfg, double& max_mode_vel_x, double& max_mode_vel_theta) override;

	void aplt_get_battery_info(tbattery_info_C& info) override;
private:
	int product_;
};

}

#endif

