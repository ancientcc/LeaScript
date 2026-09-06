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

#include "lua_task_api.hpp"
#include "wkocamera_task.hpp"

#include "base_slot.hpp"

using namespace std::placeholders;

namespace aplt {

enum {cpp_id_driver_layer = cpp_id_aplt_min};

class tlua_base: public tbase_slot
{
public:
	tlua_base(const std::string& sn, const std::string& cpuid);
	~tlua_base() {}

private:
	// void pre_start() override;
	// void did_capture_audio(uint8_t* stream, int len) override;
	bool use_external_imu() const override { return false; }

	// bool imu_can_read() const override { return false; }

	int get_serial_path(std::string& path, std::string& model) override;
	void slice() override;

	void aplt_pre_start_ros_node(const std::string& model, const std::string& serial_dev, int baudrate) override;
	void aplt_start_ros_node(bool& exit, const std::string& serial_dev, int baudrate) override;
	void aplt_post_stop_ros_node() override;

	void aplt_enter_navigation(bool buildmap, ros::tbase_cfg& base_cfg, double& max_mode_vel_x, double& max_mode_vel_theta) override;

	void aplt_get_battery_info(tbattery_info_C& info) override;

	void reload_pref_fields(int cpp_id);
	void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) override;

private:
	aplt::tb_api& b_api_;
	aplt::tpinyin& pinyin_;
	const bool movable_from_lua_;
	int product_;
	int CB_ver_;
};

tlua_base::tlua_base(const std::string& sn, const std::string& cpuid)
	: tbase_slot(sn, cpuid)
	, b_api_(get_b_api())
	// , ros_(get_r_api())
	, pinyin_(aplt::get_curr_pinyin())
	// movable_from_lua_ is from 'lua preference'. 
	// Currently, all devices cannot be moved, so it is simplified to directly setting this here.
	, movable_from_lua_(false)
	// product_ is from 0x10 command.
	, product_(nposm)
	, CB_ver_(nposm)
{
	reload_pref_fields(cpp_id_driver_layer);
}

int tlua_base::get_serial_path(std::string& path, std::string& model)
{
	// const trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

	SDL_Log("{dbg-kdesktop}tlua_base::get_serial_path");

	path = "/dev/ttyS0"; // android, ios
	if (game_config::os == os_windows) {
		path = "COM1";
	}
	model = "doll";
    return 115200;
}

void tlua_base::slice()
{
}

void tlua_base::aplt_pre_start_ros_node(const std::string& model, const std::string& serial_dev, int baudrate)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(product_ == nposm, null_str);

	moveable_ = movable_from_lua_;
}

void tlua_base::aplt_start_ros_node(bool& exit, const std::string& serial_dev, int baudrate)
{
	VALIDATE_NOT_MAIN_THREAD();
	// VALIDATE(product_ == product_lamp_v1, null_str);

	VALIDATE(!serial_dev.empty(), null_str);
	VALIDATE(baudrate > 0, null_str);

	double frequency = 10.0; // 100 ms
	// ros::Rate r(frequency);

	double voltage = 23.0;
	while (!exit) {
		// Even if there is an external power supply, 
		// it is considered that there is a voltage detection circuit.
		aplt::valuex.set_NMTHREAD_battery_level(voltage);

		SDL_Delay(100);
		// r.sleep();
	}
}

void tlua_base::aplt_post_stop_ros_node()
{
	VALIDATE_IN_MAIN_THREAD();
}

void tlua_base::aplt_enter_navigation(bool buildmap, ros::tbase_cfg& base_cfg, double& max_mode_vel_x, double& max_mode_vel_theta)
{
	VALIDATE(false, "Not support moveable.");
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(movable_from_lua_, null_str);

    // const std::string product_str = aplt::curr_aplt->prefs.get_str("base_product");
	// const int product = base_product_from_str(product_str);
	// VALIDATE(product_ == product, null_str);
}

void tlua_base::aplt_get_battery_info(tbattery_info_C& info)
{
	info.max = 24.0; // 24.0
	info.charge = 10.5;
	info.cutoff = 10.0;
}

#define DEF_LONGPRESS_THRESHOLD		20

void tlua_base::reload_pref_fields(int cpp_id)
{
/*
	trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;
	if (cpp_id == cpp_id_driver_layer) {
		// lamp_longpress_threshold_ = get_int_field2(aplt_prefs, "longpress_threshold", SDL_Range{15, 50}, DEF_LONGPRESS_THRESHOLD);
	}
*/
}

void tlua_base::fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id)
{
	VALIDATE_IN_MAIN_THREAD();

	if (cpp_id == cpp_id_driver_layer) {

		// const int last_longpress_threshold = lamp_longpress_threshold_;

		// threading::lock lock(xf3params_.mutex);
		reload_pref_fields(cpp_id);

		// if (lamp_longpress_threshold_ != last_longpress_threshold) {
		//	lamp_set_global_settings(lamp_longpress_threshold_);
		// }
	}
}

class tlua_camera_api: public thelper_camera_api
{
public:
	enum {camera_workout, camera_count};
	tlua_camera_api(tapplet& aplt);

private:
	thelper_camera_task_slot* app_create_camera_task_slot(int code, const tapplet::ttask& cfg_task, ttask_vars& task_vars) override;

private:
	thelper_lua_camera lua_camera_;
};

tlua_camera_api::tlua_camera_api(tapplet& aplt)
	: thelper_camera_api(aplt)
{
	task_ids_.insert(std::make_pair("workout", camera_workout));
	VALIDATE(task_ids_.size() == (int)camera_count, null_str);
}

thelper_camera_task_slot* tlua_camera_api::app_create_camera_task_slot(int code, const tapplet::ttask& cfg_task, ttask_vars& task_vars)
{
	VALIDATE(code == camera_workout, null_str);

	return new twkocamera_task(lua_camera_, aplt_, cfg_task, task_vars);
}

}

void* lua_aplt_create_base_slot(const char* sn, const char* cpuid)
{
	VALIDATE(sn != nullptr && cpuid != nullptr, null_str);
/*
	utils::string_map symbols;
	aplt::tb_api& ros = aplt::get_b_api();

	std::pair<std::string, std::string> pairs = utils::split_app_prefix_id(sn);
	const bool no_check_sn = game_config::os == os_windows;
	if (!no_check_sn && pairs.first != "leagor") { // leagor__xxxx
		symbols["sn"] = sn;
		ros.aplt_add_msg_log(time(nullptr), vgettext2("[Leagor]Unsupport SN: $sn", symbols), 0, false);
		return nullptr;
	}
*/
	aplt::tlua_base* leagor = new aplt::tlua_base(sn, cpuid);

	aplt::tbase_slot* result = static_cast<aplt::tbase_slot*>(leagor);
	aplt::set_base_slot(leagor);
	return result;
}

void* lua_aplt_create_task_api(void* aplt1)
{
	aplt::tapplet* aplt = reinterpret_cast<aplt::tapplet*>(aplt1);
	aplt::tlua_camera_api* camera_api = new aplt::tlua_camera_api(*aplt);

	aplt::ttask_api* leagor = new aplt::ttask_api(*aplt, nullptr, nullptr, camera_api, nullptr, nullptr, nullptr);
	return leagor;
}
