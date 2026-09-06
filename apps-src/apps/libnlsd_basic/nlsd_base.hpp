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
#include "llampcp.hpp"

#define MFR_NLSD		1
enum {product_lamp = 1, product_doll = 2, base_product_count};

#define base_product_is_moveable(product)	((product) > product_doll)

namespace aplt {

struct twrite_cmd
{
	twrite_cmd()
		: type(nposm)
		, importment(false)
	{}

	void assign_global_settings(int longpress_mul10)
	{
		type = tllampcpserial::SET_GLOBAL_SETTINGS;
		importment = true;
		global_settings.longpress_mul10 = longpress_mul10;
	}

	void assign_brightness(int value)
	{
		type = tllampcpserial::SET_BRIGHTNESS;
		importment = true;
		brightness.value = value;
	}

	void assign_color_temperature(int index)
	{
		type = tllampcpserial::SET_COLOR_TEMPERATURE;
		importment = true;
		color_temperature.index = index;
	}

	void assign_led(int _type, int value)
	{
		type = tllampcpserial::CTRL_LED;
		importment = true;
		led.type = _type;
		led.value = value;
	}

	void assign_button_threshold(int power, int non_power)
	{
		type = tllampcpserial::SET_BUTTON_THRESHOLD;
		importment = true;
		button_threshold.power = power;
		button_threshold.non_power = non_power;
	}

public:
	int type;
	bool importment;

	struct {
		int longpress_mul10;
	} global_settings;

	struct {
		int value;
	} brightness;

	struct {
		int index;
	} color_temperature;

	struct {
		int type;
		uint8_t value;
	} led;

	struct {
		int power;
		int non_power;
	} button_threshold;
};

class tnlsd_base: public tbase_slot, public tbase_ext_lamp
{
public:
	tnlsd_base(const std::string& sn, const std::string& cpuid);
	~tnlsd_base();

private:
	// void pre_start() override;
	// void did_capture_audio(uint8_t* stream, int len) override;
	bool use_external_imu() const override;

	// bool imu_can_read() const override { return false; }

	int get_serial_path(std::string& path, std::string& model) override;
	void slice() override;

	void aplt_pre_start_ros_node(const std::string& model, const std::string& serial_dev, int baudrate) override;
	void aplt_start_ros_node(bool& exit, const std::string& serial_dev, int baudrate) override;
	void aplt_post_stop_ros_node() override;

	void aplt_enter_navigation(bool buildmap, ros::tbase_cfg& base_cfg, double& max_mode_vel_x, double& max_mode_vel_theta) override;

	void aplt_get_battery_info(tbattery_info_C& info) override;

	tbase_ext_lamp* query_ext_lamp() override { return this; }

	void lamp_ctrl_led2(int type, uint8_t value, bool importment);
	void write_cmd_slice();
	void push_lamp_write_cmd(const twrite_cmd& cmd);
	void speak_error(const std::string& msg, bool force);

	int turn_off_nlsd_2_llampcp_brightness(int value);
	void turn_off_set_brightness(int value);

	void reload_pref_fields(int cpp_id);
	void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) override;

	//
	// tbase_ext_lamp
	//
	void lamp_set_global_settings(int longpress_mul10) override;
	void lamp_set_brightness(int value) override;
	void lamp_set_color_temperature(int index) override;
	void lamp_ctrl_led(int type, uint8_t value) override;
	void lamp_set_button_threshold(int power, int non_power) override;
	void lamp_did_status_report(bool first, int CB_ver, int manufacturer, int product, int temperature, int reserve, int color_temperature, int brightness) override;
	void lamp_did_button_pressed(int type, uint8_t value) override;
	tcode2 lamp_product_name() const override;
	void lamp_turn_off(bool delay) override;
	bool lamp_in_turn_off() const override;
	void lamp_clear_turn_off() override;

private:
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;
	aplt::tpinyin& pinyin_;
	const bool movable_from_lua_;
	int product_;
	int CB_ver_;
	tllampcpserial* llampcp_;
	uint32_t next_read_status_ticks_;

	const int default_button_threshold_;
	const int var_type_for_code_;
	const int magic_brightness_turn_off_;
	const SDL_Range brightness_range_;
	const int min_4level_brightness_;

	const int min_wirte_interval_ms_;
	std::vector<twrite_cmd> write_cmds_;
	uint32_t next_write_cmd_ticks_;

	const int repeat_speak_led_interval_ms_;
	const int repeat_speak_which_led_;
	bool next_repeat_speak_led_off_;
	uint32_t next_repeat_speak_led_ticks_;

	uint32_t next_speak_error_ticks_;


	int curr_brightness_;
	const int turn_off_decrease_interval_ms_;
	const int turn_off_decrease_brightness_;

	bool turn_off_use_delay_;
	uint32_t turn_off_next_decrease_ticks_;
	int turn_off_curr_brightness_;
};

}

#endif

