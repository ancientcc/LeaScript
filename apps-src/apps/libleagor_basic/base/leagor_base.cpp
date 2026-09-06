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

#include "leagor_base.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>
#include "im948.hpp"

using namespace std::placeholders;

int doll__node(bool& exit, const std::string& serial_path, int baudrate)
{
	VALIDATE(!serial_path.empty(), null_str);
	VALIDATE(baudrate > 0, null_str);

	double voltage = 12.0;
	while (!exit) {
		// Even if there is an external power supply, 
		// it is considered that there is a voltage detection circuit.
		aplt::valuex.set_NMTHREAD_battery_level(voltage);

		SDL_Delay(1000);
	}
	
    return 0;
}

int base_product_from_str(const std::string& product)
{
	if (product == "yahboom") {
		return product_yahboom;

    } else if (product == "rosserial") {
		return product_rosserial;

    } else if (product == "wheeltec_box") {
		return product_wheeltec_box;

	} else if (product == "wheeltec_open") {
		return product_wheeltec_open;

    } else if (product == "wheeltec_open_m") {
		return product_wheeltec_open_m;
    }

	// product == "doll" or other, now think as doll
	return product_doll;
}

void start_base__node(bool& exit, const std::string& serial_path, int baudrate, int product)
{
	VALIDATE_NOT_MAIN_THREAD();
	VALIDATE(product >= 0 && product < base_product_count, null_str);

	if (product == product_yahboom) {
		yahboom_base__node(exit, serial_path, baudrate);

    } else if (product == product_wheeltec_box || product == product_wheeltec_open || product == product_wheeltec_open_m) {
		wheeltec_base__node(exit, serial_path, baudrate);

    } else if (product == product_rosserial) {
		rosserial_cpp__node(exit, serial_path, baudrate);

    } else {
		VALIDATE(product == product_doll, null_str);
		doll__node(exit, serial_path, baudrate);
    }
}

namespace aplt {

tleagor_base::tleagor_base(const std::string& sn, const std::string& cpuid)
	: tbase_slot(sn, cpuid)
	, product_(nposm)
{
}

tleagor_base::~tleagor_base()
{
}

bool tleagor_base::use_external_imu() const
{
	return true;
}

bool tleagor_base::has_magnetometer() const
{
	return false;
}

void tleagor_base::open_external_imu(const std::string& serial_path, int baudrate, bool use_magnetometer)
{
	VALIDATE(use_external_imu(), null_str);
	VALIDATE(external_imu_ == nullptr, null_str);
	external_imu_ = im948serial_open(serial_path, baudrate, use_magnetometer);
}

bool tleagor_base::imu_can_read() const
{
	if (external_imu_ == nullptr) {
		return false;
	}
	return external_imu_->can_read();
}

int tleagor_base::get_serial_path(std::string& path, std::string& model)
{
	const trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

	path = aplt_prefs.get_str("base_serial");
	model = aplt_prefs.get_str("base_product");
    return aplt_prefs.get_int("base_serial_baudrate", nposm);
}

void tleagor_base::aplt_pre_start_ros_node(const std::string& model, const std::string& serial_dev, int baudrate)
{
	VALIDATE_IN_MAIN_THREAD();

	const std::string product_str = aplt::curr_aplt->prefs.get_str("base_product");
	product_ = base_product_from_str(product_str);

	moveable_ = base_product_is_moveable(product_);
}

void tleagor_base::aplt_start_ros_node(bool& exit, const std::string& serial_dev, int baudrate)
{
	VALIDATE_NOT_MAIN_THREAD();
	VALIDATE(product_ != nposm, null_str);

	// const std::string product_str = aplt::curr_aplt->prefs.get_str("base_product");
	// product_ = base_product_from_str(product_str);

	start_base__node(exit, serial_dev, baudrate, product_);
}

void tleagor_base::aplt_enter_navigation(bool buildmap, ros::tbase_cfg& base_cfg, double& max_mode_vel_x, double& max_mode_vel_theta)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(base_product_is_moveable(product_), null_str);

    const std::string product_str = aplt::curr_aplt->prefs.get_str("base_product");
	const int product = base_product_from_str(product_str);
	VALIDATE(product_ == product, null_str);

    if (product_ == product_yahboom) {
		base_cfg.min_moveable_vel_x = 0.12;
		base_cfg.min_moveable_vel_theta = DEG2RAD(50);
		base_cfg.move_base_controller_freq = 1.5; // 2.5

    } else if (product_ == product_rosserial) {

    } else {
		// product == "joybot" or other, now think as joybot
		VALIDATE(product_ == product_wheeltec_box || product_ == product_wheeltec_open || product_ == product_wheeltec_open_m, null_str);

		base_cfg.min_moveable_vel_x = 0.06;
		base_cfg.min_moveable_vel_theta = DEG2RAD(10); // DEG2RAD(15)

		if (product_ == product_wheeltec_box) {
			base_cfg.laser_to_base_footprint_tf.set(-0.03, 0.00, DEG2RAD(0.0), true);  // N10, (0.06 0.00 0.20 3.14 0 0)
			// result.set(0.045, -0.08, DEG2RAD(0.0), true);  // N10, (0.06 0.00 0.20 3.14 0 0)

		} else if (product_ == product_wheeltec_open) {
			// result.set(0.03, 0.00, DEG2RAD(0.0), true);  // A1M8
			base_cfg.laser_to_base_footprint_tf.set(0.045, 0.00, DEG2RAD(0.0), true);  // N10, (0.06 0.00 0.20 3.14 0 0)
			// result.set(0.045, -0.08, DEG2RAD(0.0), true);  // N10, (0.06 0.00 0.20 3.14 0 0)

		} else if (product_ == product_wheeltec_open_m) {
			const bool trailer_tmp_new = false;
			double dcamera_trans_y = 0.011 + 0.007;
			if (trailer_tmp_new) {
				base_cfg.laser_to_base_footprint_tf.set(-0.02, 0.00, DEG2RAD(0), true);
				// robot_length = 0.28;
				double dcamera_trans_x = base_cfg.laser_to_base_footprint_tf.x + 0.22; // 0.24, becuase claw -0.2
				base_cfg.dcamera_to_base_footprint_tf.set(dcamera_trans_x, dcamera_trans_y, DEG2RAD(0), true);

			} else {
				base_cfg.laser_to_base_footprint_tf.set(-0.065, 0.00, DEG2RAD(0), true);
				double dcamera_trans_x = base_cfg.laser_to_base_footprint_tf.x + 0.175;
				base_cfg.dcamera_to_base_footprint_tf.set(dcamera_trans_x, dcamera_trans_y, DEG2RAD(0), true);
			}
		}
    } 
}

void tleagor_base::aplt_get_battery_info(tbattery_info_C& info)
{
	// https://item.jd.com/10098013425671.html
	// cutoff voltage: 9V
	// nominal voltage: 11.1V
	// full charge voltage: 12.6V

	info.max = 12.6;
	info.charge = 10.5;
	info.cutoff = 10.0;
}

}

void* aplt_create_base_slot(const char* sn, const char* cpuid)
{
	VALIDATE(sn != nullptr && cpuid != nullptr, null_str);

	utils::string_map symbols;
	aplt::tb_api& ros = aplt::get_b_api();

	std::pair<std::string, std::string> pairs = utils::split_app_prefix_id(sn);
	const bool no_check_sn = game_config::os == os_windows;
	if (!no_check_sn && pairs.first != "leagor") { // leagor__xxxx
		symbols["sn"] = sn;
		ros.aplt_add_msg_log(time(nullptr), vgettext2("[Leagor]Unsupport SN: $sn", symbols), 0, false);
		return nullptr;
	}

	aplt::tleagor_base* leagor = new aplt::tleagor_base(sn, cpuid);

	aplt::tbase_slot* result = static_cast<aplt::tbase_slot*>(leagor);
	aplt::set_base_slot(leagor);
	return result;
}
