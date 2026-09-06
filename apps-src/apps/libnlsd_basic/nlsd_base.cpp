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

#define GETTEXT_DOMAIN "aplt_nlsd_basic-lib"

#include "nlsd_base.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>

#include <ros/rate.h>
#include "common.hpp"

using namespace std::placeholders;

namespace aplt {

tnlsd_base::tnlsd_base(const std::string& sn, const std::string& cpuid)
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
	, default_button_threshold_(40)
	, var_type_for_code_(var_env_button_code)
	, magic_brightness_turn_off_(0xff)
	, brightness_range_({0, 100})
	, min_4level_brightness_(10)
	, llampcp_(nullptr)
	, next_read_status_ticks_(0)
	, min_wirte_interval_ms_(150)
	, next_write_cmd_ticks_(0)
	, repeat_speak_led_interval_ms_(500)
	, repeat_speak_which_led_(ledtype_privacy)
	, next_repeat_speak_led_off_(false)
	, next_repeat_speak_led_ticks_(0)
	, next_speak_error_ticks_(0)
	, curr_brightness_(min_4level_brightness_)
	, turn_off_decrease_interval_ms_(1500) // 2500
	, turn_off_decrease_brightness_(5)
	, turn_off_use_delay_(false)
	, turn_off_next_decrease_ticks_(0)
	, turn_off_curr_brightness_(nposm)
{
	VALIDATE(repeat_speak_led_interval_ms_ >= min_wirte_interval_ms_ + 100, null_str);

	VALIDATE(nlsd::lamp == nullptr, null_str);
	nlsd::lamp = this;

	reload_pref_fields(cpp_id_driver_layer);
}

tnlsd_base::~tnlsd_base()
{
	VALIDATE(llampcp_ == nullptr, null_str);

	VALIDATE(nlsd::lamp != nullptr, null_str);
	nlsd::lamp = nullptr;
}

static int get_int_field2(trose_prefs& aplt_prefs, const std::string& key, const SDL_Range& range, int def)
{
	int val = aplt_prefs.get_int(key, def);
	if (val < range.min || val > range.max) {
		val = def;
	}
	return val;
}

bool tnlsd_base::use_external_imu() const
{
	return false;
}

int tnlsd_base::get_serial_path(std::string& path, std::string& model)
{
	const trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

	path = aplt_prefs.get_str("base_serial");
	model = aplt_prefs.get_str("base_product");
    return aplt_prefs.get_int("base_serial_baudrate", nposm);
}

void tnlsd_base::slice()
{
	if (llampcp_ != nullptr) {
		llampcp_->pool_read();

		if (next_read_status_ticks_ != 0 && SDL_GetTicks() >= next_read_status_ticks_) {
			SDL_Log("%u {read_status}it it time to send read_status", SDL_GetTicks());
			const trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

			int pref_power_threshold = aplt_prefs.get_int("power_threshold", default_button_threshold_);
			int pref_non_power_threshold = aplt_prefs.get_int("non_power_threshold", default_button_threshold_);

			lamp_set_button_threshold(pref_power_threshold, pref_non_power_threshold);

			next_read_status_ticks_ = 0;
		}

		write_cmd_slice();

		if (turn_off_next_decrease_ticks_ != 0 && SDL_GetTicks() >= turn_off_next_decrease_ticks_) {
			int value = turn_off_nlsd_2_llampcp_brightness(turn_off_curr_brightness_);
			turn_off_set_brightness(value);

		}

		// if (pinyin_.is_repeat_speaking()) {
		uint32_t now = SDL_GetTicks();
		if (pinyin_.is_speaking() || b_api_.is_listening()) {
			if (next_repeat_speak_led_ticks_ == 0) {
				next_repeat_speak_led_ticks_ = now;
			}
			if (now >= next_repeat_speak_led_ticks_) {
				if (next_repeat_speak_led_off_) {
					lamp_ctrl_led2(repeat_speak_which_led_, ledact_off, false);
				} else {
					lamp_ctrl_led2(repeat_speak_which_led_, ledact_on, false);
				}
				next_repeat_speak_led_off_ = !next_repeat_speak_led_off_;
				next_repeat_speak_led_ticks_ = SDL_GetTicks() + repeat_speak_led_interval_ms_;
			}

		// } else if (next_repeat_speak_led_ticks_ != 0 && now >= next_repeat_speak_led_ticks_) {
		} else if (next_repeat_speak_led_ticks_ != 0) {
			// why 'now >= next_repeat_speak_led_ticks_'?
			// -- Ensure an interval of at least 'repeat_speak_led_interval_ms_' between this and last 'ctrl_led()'.
			next_repeat_speak_led_ticks_ = 0;
			// std::pair<const aplt::tbase_scene*, int> curr_scene = ros_.aplt_curr_base_scene();
			// bool on = curr_scene.first != nullptr && curr_scene.second == sts_ing;
			bool on = b_api_.is_privacy_protecting();
			lamp_ctrl_led(repeat_speak_which_led_, on? ledact_on: ledact_off);
		}
		
	} else {
		VALIDATE(next_read_status_ticks_ == 0, null_str);
		VALIDATE(turn_off_next_decrease_ticks_ == 0, null_str);
	}
}

void tnlsd_base::aplt_pre_start_ros_node(const std::string& model, const std::string& serial_dev, int baudrate)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(product_ == nposm, null_str);
	VALIDATE(next_read_status_ticks_ == 0, null_str);

	moveable_ = movable_from_lua_;

	VALIDATE(llampcp_ == nullptr, null_str);
	llampcp_ = llampcpserial_open(*this, serial_dev, baudrate);
	if (llampcp_ != nullptr) {
		bool enable = true;
		if (enable) {
			const int threshold = 2 * 1000; // 2 second
			next_read_status_ticks_ = SDL_GetTicks() + threshold;
			SDL_Log("%u {read_status}set next_read_status_ticks", SDL_GetTicks());
		}
	}
}

void tnlsd_base::aplt_start_ros_node(bool& exit, const std::string& serial_dev, int baudrate)
{
	VALIDATE_NOT_MAIN_THREAD();
	// VALIDATE(product_ == product_lamp_v1, null_str);

	VALIDATE(!serial_dev.empty(), null_str);
	VALIDATE(baudrate > 0, null_str);

	double frequency = 10.0; // 100 ms
	ros::Rate r(frequency);

	double voltage = 24.0;
	while (!exit) {
		// Even if there is an external power supply, 
		// it is considered that there is a voltage detection circuit.
		aplt::valuex.set_NMTHREAD_battery_level(voltage);

		// SDL_Delay(1000);
		r.sleep();
	}
}

void tnlsd_base::aplt_post_stop_ros_node()
{
	VALIDATE_IN_MAIN_THREAD();

	if (llampcp_ != nullptr) {
		delete llampcp_;
		llampcp_ = nullptr;

		if (next_read_status_ticks_ != 0) {
			SDL_Log("%u {read_status}next_read_status_ticks = 0 during aplt_post_stop_ros_node", SDL_GetTicks());
			next_read_status_ticks_ = 0;
		}
	}
	VALIDATE(next_read_status_ticks_ == 0, null_str);
}

void tnlsd_base::aplt_enter_navigation(bool buildmap, ros::tbase_cfg& base_cfg, double& max_mode_vel_x, double& max_mode_vel_theta)
{
	VALIDATE(false, "Not support moveable.");
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(movable_from_lua_, null_str);

    // const std::string product_str = aplt::curr_aplt->prefs.get_str("base_product");
	// const int product = base_product_from_str(product_str);
	// VALIDATE(product_ == product, null_str);
}

void tnlsd_base::aplt_get_battery_info(tbattery_info_C& info)
{
	info.max = 24.0; // 24.0
	info.charge = 10.5;
	info.cutoff = 10.0;
}

#define DEF_LONGPRESS_THRESHOLD		20

void tnlsd_base::reload_pref_fields(int cpp_id)
{
	trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;
	if (cpp_id == cpp_id_driver_layer) {
		lamp_longpress_threshold_ = get_int_field2(aplt_prefs, "longpress_threshold", SDL_Range{15, 50}, DEF_LONGPRESS_THRESHOLD);
	}
}

void tnlsd_base::fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id)
{
	VALIDATE_IN_MAIN_THREAD();

	if (cpp_id == cpp_id_driver_layer) {

		const int last_longpress_threshold = lamp_longpress_threshold_;

		// threading::lock lock(xf3params_.mutex);
		reload_pref_fields(cpp_id);

		if (lamp_longpress_threshold_ != last_longpress_threshold) {
			lamp_set_global_settings(lamp_longpress_threshold_);
		}
	}
}

//
// tbase_ext_lamp
//
void tnlsd_base::lamp_set_global_settings(int longpress_mul10)
{
	// SDL_Log("%u {nlsd_base.cpp}lamp_set_global_settings, llampcp_: 0x%p, longpress_mul10: %i", SDL_GetTicks(), llampcp_, longpress_mul10);
	if (llampcp_ != nullptr) {
		twrite_cmd cmd;
		cmd.assign_global_settings(longpress_mul10);
		push_lamp_write_cmd(cmd);

		// llampcp_->set_global_settings(longpress_mul10);
	}
}

void tnlsd_base::lamp_set_brightness(int value)
{
	if (value != magic_brightness_turn_off_) {
		VALIDATE(value >= brightness_range_.min && value <= brightness_range_.max, null_str);
	}

	if (llampcp_ != nullptr) {
		twrite_cmd cmd;
		cmd.assign_brightness(value);
		push_lamp_write_cmd(cmd);
		// llampcp_->set_brightness(value);
		if (value != magic_brightness_turn_off_) {
			curr_brightness_ = value;
		}
	}
}

void tnlsd_base::lamp_set_color_temperature(int index)
{
	if (llampcp_ != nullptr) {
		twrite_cmd cmd;
		cmd.assign_color_temperature(index);
		push_lamp_write_cmd(cmd);
		// llampcp_->set_color_temperature(index);
	}
}

void tnlsd_base::lamp_ctrl_led(int type, uint8_t value)
{
	if (llampcp_ != nullptr) {
		twrite_cmd cmd;
		cmd.assign_led(type, value);
		push_lamp_write_cmd(cmd);

		// llampcp_->ctrl_led(type, value);
	}
}

void tnlsd_base::lamp_ctrl_led2(int type, uint8_t value, bool importment)
{
	if (llampcp_ != nullptr) {
		twrite_cmd cmd;
		cmd.assign_led(type, value);
		cmd.importment = importment;
		push_lamp_write_cmd(cmd);

		// llampcp_->ctrl_led(type, value);
	}
}

void tnlsd_base::lamp_set_button_threshold(int power, int non_power)
{
	if (llampcp_ != nullptr) {
		twrite_cmd cmd;
		cmd.assign_button_threshold(power, non_power);
		push_lamp_write_cmd(cmd);

		// llampcp_->ctrl_led(type, value);
	}
}

void tnlsd_base::lamp_did_status_report(bool first, int CB_ver, int manufacturer, int product, int temperature, int reserve, int custom0, int custom1)
{
	utils::string_map symbols;
	if (manufacturer != MFR_NLSD) {
		symbols["field"] = _("manufacturer code");
		speak_error(vgettext2("According to the $field, the control board is not a Nulandston product.", symbols), false);
		return;
	}

	if (product_ == nposm) {
		if (product == product_lamp) {
			product_ = product_lamp;

		} else if (product == product_doll) {
			product_ = product_doll;

		} else {
			symbols["field"] = _("product code");
			speak_error(vgettext2("According to the $field, the control board is not a Nulandston product.", symbols), false);
			return;
		}

	} else if (product != product_) {
		symbols["field"] = _("product code");
		speak_error(vgettext2("A fatal error has occurred. During operation, the product code in the control board has changed.", symbols), false);
		VALIDATE(false, null_str);
	}

	CB_ver_ = CB_ver; 

	if (product_ == product_lamp) {
		int color_temperature = custom0;
		int brightness = custom1;

		if (brightness < brightness_range_.min || brightness > brightness_range_.max) {
			SDL_Log("nlsd base dirver receives error brightness: %i", brightness);
			VALIDATE(game_config::os != os_windows, null_str);
		}

		SDL_Log("first: %s, nlsd base dirver receives brightness: %i", first? "true": "false", brightness);
		// if (!first) {
			curr_brightness_ = brightness;

		// }
	} else if (product_ == product_doll) {
		int power_threshold = custom0;
		int non_power_threshold = custom1;

		trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

		if (first) {
			int pref_power_threshold = aplt_prefs.get_int("power_threshold", default_button_threshold_);
			int pref_non_power_threshold = aplt_prefs.get_int("non_power_threshold", default_button_threshold_);
			if (power_threshold != pref_power_threshold || non_power_threshold != pref_non_power_threshold) {
				lamp_set_button_threshold(pref_power_threshold, pref_non_power_threshold);
			}

		} else {
			aplt_prefs.set_int("power_threshold", power_threshold);
			aplt_prefs.set_int("non_power_threshold", non_power_threshold);
		}
	}

	if (next_read_status_ticks_ != 0) {
		SDL_Log("%u {read_status}receive status report, set next_read_status_ticks = 0", SDL_GetTicks());
		next_read_status_ticks_ = 0;
	}
}

void tnlsd_base::lamp_did_button_pressed(int type, uint8_t value)
{
	if (product_ == nposm) {
		return;
	}

	enum {report_next_scene = 1, report_idle_scene, report_resume_scene, report_enable_privacy, report_disable_privacy};

	VALIDATE(type == btntype_scene || type == btntype_privacy, null_str);
	if (value != btnact_short_press && value != btnact_long_press) {
		SDL_Log("unknown button(type: %i) value: %i", type, value);
		return;
	}

	// if (type == repeat_speak_which_led_ && pinyin_.is_repeat_speaking()) {
	//	pinyin_.repeat_speak(null_str);
	if (type == repeat_speak_which_led_ && pinyin_.is_speaking()) {
		if (!b_api_.is_listen_speaking()) {
			pinyin_.stop_speak2();
			return;
		}
	}

	if (lamp_in_turn_off()) {
		lamp_clear_turn_off();
		pinyin_.speak(_("Automatic light off has been canceled"));
		return;
	}


	if (type == btntype_scene) {
		std::pair<const aplt::tbase_scene*, int> curr_scene = b_api_.aplt_curr_base_scene();

		int action = nposm;
		std::string err_msg;

		if (curr_scene.first == nullptr) {
			action = bs_action_switch_to_next;

		} else if (value == btnact_short_press) {
			if (curr_scene.second == sts_ing) {
				action = bs_action_switch_to_next;
			} else {
				action = bs_action_resume;
			}

		} else if (value == btnact_long_press) {
			if (curr_scene.second == sts_ing) {
				action = bs_action_idle;
			} else {
				action = bs_action_resume;
			}
		}

		VALIDATE(action != nposm, str_cast(value));

		const tbase_scene* new_scene = handle_base_scene(b_api_, nposm, null_str, action, err_msg);
		std::string msg;
		utils::string_map symbols;
		if (new_scene != nullptr) {
			symbols["scene"] = new_scene->name();
			if (action == bs_action_idle) {
				msg = vgettext2("suspended the current scene '$scene'", symbols);

			} else if (action == bs_action_resume) {
				msg = vgettext2("Resumed the current scene '$scene'", symbols);

			} else {
				VALIDATE(action == bs_action_switch_to_next, null_str);
				msg = vgettext2("Switched to the scene '$scene'", symbols);
			}

		} else {
			VALIDATE(!err_msg.empty(), null_str);
			symbols["err_msg"] = err_msg;
			msg = vgettext2("Switch scene failed: $err_msg", symbols);
			// msg = err_msg;
		}
		pinyin_.speak(msg);

	} else if (type == btntype_privacy) {

		enum {action_privacy, action_start_listen, action_stop_listen, action_next_course};
		int action = nposm;

		if (value == btnact_short_press) {
			if (!b_api_.is_listening()) {
				action = action_privacy;

			} else {
				action = action_next_course;
			}

		} else if (value == btnact_long_press) {
			if (!b_api_.is_listening()) {
				action = action_start_listen;

			} else {
				action = action_stop_listen;
			}
		}
		VALIDATE(action != nposm, null_str);

		std::string msg;
		if (action == action_privacy) {
			bool protect = true;
			if (b_api_.is_privacy_protecting()) {
				protect = false;
				msg = _("Privacy protection is disabled.");
			} else {
				protect = true;
				msg = _("Privacy protection is enabled.");
			}

			b_api_.set_privacy_protect(protect);

		} else if (action == action_start_listen) {
			bool ret = b_api_.start_listen();
			if (!ret) {
				msg = _("Failed to enter the lecture listening state. There must be at least one valid courseware in the courselist.");
			} else {
				// msg = _("Entered the lecture listening state.");
			}

		} else if (action == action_stop_listen) {
			b_api_.stop_listen();
			msg = _("Exited the lecture listening state.");

		} else {
			VALIDATE(action == action_next_course, null_str);
			b_api_.listen_next_course();
		}

		if (!msg.empty()) {
			pinyin_.speak(msg);
		}
	}
}

tcode2 tnlsd_base::lamp_product_name() const
{
	if (product_ == nposm) {
		return tcode2(nposm, null_str);
	}

	std::stringstream ss;
	if (product_ == product_lamp) {
		ss << _("Lamp(V1)");
	}

	VALIDATE(product_ == product_doll, null_str);
	ss << _("Doll(V1)");

	ss << "  ";
	utils::string_map symbols;
	symbols["ver"] = str_cast(CB_ver_);
	ss << vgettext2("Control board(V$ver)", symbols);
	return tcode2(product_doll, ss.str());
}

void tnlsd_base::lamp_turn_off(bool delay)
{
	if (llampcp_ == nullptr) {
		return;
	}

	// The gradient duration is typically fixed at 30-60 seconds.
	SDL_Log("%u, {lamp_turn_off} start turn_off, curr_brightness_: %i, delay: %s", SDL_GetTicks(), curr_brightness_, delay? "true": "false");
	turn_off_use_delay_ = delay;

	int value = turn_off_nlsd_2_llampcp_brightness(curr_brightness_);
	turn_off_set_brightness(value);
}

bool tnlsd_base::lamp_in_turn_off() const
{
	return turn_off_next_decrease_ticks_ != 0;
}

void tnlsd_base::lamp_clear_turn_off()
{
	turn_off_next_decrease_ticks_ = 0;
	turn_off_curr_brightness_ = nposm;
}

void tnlsd_base::write_cmd_slice()
{
	if (SDL_GetTicks() < next_write_cmd_ticks_ || write_cmds_.empty()) {
		return;
	}

	if (llampcp_ == nullptr) {
		// SDL_Log("%u {nlsd_base.cpp}'llampcp_ == nullptr', clear write_cmds_", SDL_GetTicks());
		write_cmds_.clear();
		return;
	}

	const twrite_cmd& cmd = write_cmds_[0];
	// SDL_Log("%u {nlsd_base.cpp}write lamp cmd: 0x%x", SDL_GetTicks(), cmd.type);
	if (cmd.type == tllampcpserial::SET_GLOBAL_SETTINGS) {
		llampcp_->set_global_settings(cmd.global_settings.longpress_mul10);

	} else if (cmd.type == tllampcpserial::SET_BRIGHTNESS) {
		llampcp_->set_brightness(cmd.brightness.value);

	} else if (cmd.type == tllampcpserial::SET_COLOR_TEMPERATURE) {
		llampcp_->set_color_temperature(cmd.color_temperature.index);

	} else if (cmd.type == tllampcpserial::CTRL_LED) {
		llampcp_->ctrl_led(cmd.led.type, cmd.led.value);

	} else {
		VALIDATE(cmd.type == tllampcpserial::SET_BUTTON_THRESHOLD, null_str);
		llampcp_->set_button_threshold(cmd.button_threshold.power, cmd.button_threshold.non_power);
	}
	write_cmds_.erase(write_cmds_.begin());

	next_write_cmd_ticks_ = SDL_GetTicks() + min_wirte_interval_ms_;
}

void tnlsd_base::push_lamp_write_cmd(const twrite_cmd& cmd)
{
	VALIDATE_IN_MAIN_THREAD();

	if (!cmd.importment && !write_cmds_.empty()) {
		// Discard this command.
		VALIDATE(cmd.type == tllampcpserial::CTRL_LED, null_str);
		// SDL_Log("%u {nlsd_base.cpp}discard cmd: 0x%x", SDL_GetTicks(), cmd.type);
		return;
	}

	// SDL_Log("%u {nlsd_base.cpp}push_lamp_write_cmd, cmd.type: %02x", SDL_GetTicks(), cmd.type);

	write_cmds_.push_back(cmd);
	write_cmd_slice();
}

void tnlsd_base::speak_error(const std::string& msg, bool force)
{
	VALIDATE(!msg.empty(), null_str);

	uint32_t now = SDL_GetTicks();
	if (!force && next_speak_error_ticks_ != 0 && now < next_speak_error_ticks_) {
		return;
	}
	const int mask_time = 10 * 1000;
	next_speak_error_ticks_ = now + mask_time;
	pinyin_.speak(msg);
}

int tnlsd_base::turn_off_nlsd_2_llampcp_brightness(int value)
{
	VALIDATE(value >= brightness_range_.min && value <= brightness_range_.max, null_str);

	if (!turn_off_use_delay_) {
		return magic_brightness_turn_off_;
	}

	int value2 = magic_brightness_turn_off_;
	const int min_decrease = 5;
	if (value > min_decrease) {
		int decrease = min_decrease; 
		if (value >= 60) {
			decrease = 15;
		} else if (value >= 30) {
			decrease = 10;
		}
		value2 = value - decrease;
	}
	return value2;
}

void tnlsd_base::turn_off_set_brightness(int value)
{
	turn_off_next_decrease_ticks_ = SDL_GetTicks() + turn_off_decrease_interval_ms_;
	if (value != magic_brightness_turn_off_) {
		turn_off_curr_brightness_ = value;
	} else {
		turn_off_curr_brightness_ = 0;
	}

	lamp_set_brightness(value);
	SDL_Log("%u, {lamp_turn_off} current brightness: %i, llampcp: %i", SDL_GetTicks(), turn_off_curr_brightness_, value);
}

}

void* aplt_create_base_slot(const char* sn, const char* cpuid)
{
	VALIDATE(sn != nullptr && cpuid != nullptr, null_str);

	std::pair<std::string, std::string> pairs = utils::split_app_prefix_id(sn);
	if (pairs.first != aplt::curr_aplt->bundleid) {
		utils::string_map symbols;
		symbols["sn"] = sn;
		aplt::get_b_api().aplt_add_msg_log(time(nullptr), 
			vgettext2("[NLSD]Unsupport SN: $sn", symbols), 0, false);
		return nullptr;
	}
		
	aplt::tnlsd_base* nlsd = new aplt::tnlsd_base(sn, cpuid);

	aplt::tbase_slot* result = static_cast<aplt::tbase_slot*>(nlsd);
	aplt::set_base_slot(nlsd);
	return result;
}
