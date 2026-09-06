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

#include "task_api.hpp"
#include "leagor_nonblock.hpp"
#include "leagor_aiagent.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>

#include "rose_qr_code.hpp"
#include "rose_sdl_utils.hpp"
#include "rose_font.hpp"

#include <kdl/utilities/utility.h>

#include <boost/foreach.hpp>

using namespace std::placeholders;

namespace aplt {

tleagor_cpp_api::tleagor_cpp_api()
{}

tleagor_cpp_api::~tleagor_cpp_api()
{
}

std::string tleagor_cpp_api::start_task_moveto2_task()
{
	bool exe_task_only = task_id_ == "exe_task_only";

	std::string position_uuid;
	ttask_pair hit_task_pair;
	std::string position2_uuid;

	const std::map<aplt::taplt_key, aplt::tapplet>& applets = b_api_.const_applets();
	const tros_map& curmap = r_api_.curmap();

	utils::string_map symbols;
	std::vector<std::string> var_names;
	var_names.push_back(utils::join_app_prefix_id(aplt::fake_aplt.bundleid, "position"));
	var_names.push_back(utils::join_app_prefix_id(aplt::fake_aplt.bundleid, "task"));
	var_names.push_back(utils::join_app_prefix_id(aplt::fake_aplt.bundleid, "position2"));

	int at = 0;
	for (std::vector<std::string>::const_iterator it = var_names.begin(); it != var_names.end(); ++ it, at ++) {
		if (exe_task_only && (at == 0 || at == 2)) {
			continue;
		}
		const std::string& var_name = *it;
		symbols["var"] = var_name;
		if (!task_vars_->existed(var_name)) {
			if (at == 0) {
				return vgettext2("No var: $var", symbols);

			} else {
				// 1 or later maybe optional
				break;
			}
		}

		const ttask_var& var = task_vars_->get_var(var_name);
		if (var.is_array) {
			return vgettext2("var($var) must be single var", symbols);
		}
		if (var.val.type() != var_type_string) {
			return vgettext2("var($var)'s type must be string", symbols);
		}

		const std::string var_val = var.val.str();
		symbols["val"] = var_val;
		if (at == 0 || at == 2) {
			if (curmap.positions.count(var_val) == 0) {
				return vgettext2("var($var)'s value is $val, but curmap isn't existed", symbols);
			}
			if (at == 0) {
				position_uuid = var_val;
			} else {
				position2_uuid = var_val;
			}
		} else {
			VALIDATE(at == 1, null_str);
			std::pair<std::string, std::string> pair = utils::split_app_prefix_id(var_val);
			const tapplet* aplt = aplt_from_id_ex(applets, pair.first);
			if (aplt != nullptr && aplt->tasks.count(pair.second) != 0) {
				const tapplet::ttask& task = aplt->tasks.find(pair.second)->second;
				hit_task_pair.aplt = aplt;
				hit_task_pair.task = &task;
			} else {
				return vgettext2("var($var)'s value is $val, but this task isn't existed", symbols);
			}
		}
	}

	if (exe_task_only && hit_task_pair.aplt == nullptr) {
		VALIDATE(task_vars_->size() < 2, null_str);
		symbols["var"] = utils::join_app_prefix_id(aplt::fake_aplt.bundleid, "task");
		return vgettext2("No var: $var", symbols);
	}

	std::pair<std::map<int, tstate2>::iterator, bool> ins = states_.insert(std::make_pair(state_first, 
		tstate2(state_first, 240))); // 120
	tstate2& state2 = ins.first->second;

	if (hit_task_pair.aplt == nullptr) {
		aplt::ttask_pair pair = aplt::task_pair_for_move(b_api_.const_applets());
		const std::string device_id;
		state2.async_task.set_aplt_task2(*pair.aplt, *pair.task, device_id, 
			tif_block(position_uuid), tif_block(null_str));

	} else {
		const std::string device_id;
		// state2.async_task.set_aplt_task(*hit_task_pair.aplt, *hit_task_pair.task, device_id, position_uuid, position2_uuid);
		state2.async_task.set_aplt_task2(*hit_task_pair.aplt, *hit_task_pair.task, device_id, 
			tif_block(position_uuid), tif_block(position2_uuid));
	}

	// state2.doing = _("Executing $task task, and please wait");
	state2.doing = _("Executing task, and please wait");
	state2.finished.set_do_str_only(_("Task finished"));

	VALIDATE(states_.size() == state_count, null_str);
	return null_str;
}

std::string tleagor_cpp_api::start_task_speak_var()
{
	const std::map<aplt::taplt_key, aplt::tapplet>& applets = b_api_.const_applets();
	const tros_map& curmap = r_api_.curmap();

	utils::string_map symbols;

	std::stringstream vars_msg;

	int at = 0;
	const std::map<std::string, ttask_var>& task_vars_data = task_vars_->data();
	for (std::map<std::string, ttask_var>::const_iterator it = task_vars_data.begin(); it != task_vars_data.end(); ++ it) {
		const ttask_var& var = it->second;

		if (var_type_is_BI_env(var.type)) {
			continue;
		}

		std::string type_msg;
		std::string val_msg;

		if (var.is_array) {
			// type_msg = _("type^array");
			type_msg = var_types.find(var_type_array)->second.name;

		} else {
			var_type_t type = var.val.type();
			VALIDATE(var_types.count(type) != 0, null_str);
			type_msg = var_types.find(type)->second.name;

			if (type == var_type_bool) {
				// type_msg = _("type^bool");
				val_msg = var.val.to_bool()? _("val^true"): _("val^false");

			} else if (type == var_type_integer) {
				// type_msg = _("type^integer");
				val_msg = str_cast(var.val.to_int64());

			} else if (type == var_type_double) {
				// type_msg = _("type^double");
				val_msg = str_cast(var.val.to_double());

			} else {
				VALIDATE(type == var_type_string, null_str);
				// type_msg = _("type^string");
				val_msg = str_cast(var.val.str());

			}
		}

		symbols["index"] = str_cast(at + 1);
		symbols["type"] = type_msg;
		symbols["val"] = val_msg;

		vars_msg << vgettext2("The $index variable, Type: $type, Value: $val.", symbols);
		at ++;
	}

	symbols["count"] = str_cast(at); // str_cast(task_vars_->size());
	symbols["vars"] = vars_msg.str();
	const std::string msg = vgettext2("$count variables. $vars", symbols);

	std::pair<std::map<int, tstate2>::iterator, bool> ins = states_.insert(std::make_pair(state_first, 
		tstate2(state_first, nposm))); // 120
	tstate2& state2 = ins.first->second;

	// state2.doing = _("Executing $task task, and please wait");
	state2.doing = null_str;
	state2.finished.set_do_str_only(msg);

	VALIDATE(states_.size() == state_count, null_str);
	return null_str;
}

std::string tleagor_cpp_api::app_start_task(const ttaskpoint* /*taskpoint*/)
{
	VALIDATE(reception_state_ == nposm, null_str);
	VALIDATE(states_.empty(), null_str);

	recoverable_ = false;
	nonpreemptive_ = false;

	tif_branch branch;
	branch.do_to_state = state_first;
	startup_state_.branches.push_back(branch);

	if (task_id_ == "move2_task" || task_id_ == "exe_task_only") {
		return start_task_moveto2_task();

	} else if (task_id_ == "speak_var") {
		return start_task_speak_var();
	}

	utils::string_map symbols;
	symbols["task"] = task_id_;
	return vgettext2("Unknonw task: $task", symbols);
}

void tleagor_cpp_api::app_task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task, bool result)
{
}


//
// moveit aplt session
//
tleagor_moveit_api::tleagor_moveit_api()
	// : ros_(aplt::get_r_api())
{}

tleagor_moveit_api::~tleagor_moveit_api()
{
}

void tleagor_moveit_api::app_start_task(tmoveit_target_info_C& result)
{
	if (task_id_ == "grasp") {
		result.op = moveit_op_grasp;

	} else if (task_id_ == "press_top") {
		result.op = moveit_op_press_top;

	} else {
		VALIDATE(task_id_ == "press_middle", null_str);
		result.op = moveit_op_press_middle;
	}
	result.size = SDL_DSize3{0.03, 0.03, 0.03};
}

void tleagor_moveit_api::camera_work_frame(const surface& surf, const cv::Mat& argb, std::vector<SDL_2Point>& corners)
{
	VALIDATE(corners.empty(), null_str);

	cv::Mat rgb;
	cv::cvtColor(argb, rgb, cv::COLOR_BGRA2BGR);

	const bool detect_qrcode = true;

	std::vector<cv::Point> cv_corners;
	std::string qrcode;
	if (detect_qrcode) {
		qrcode = find_qr(rgb, &cv_corners);
	} else {
		qrcode = find_barcode(rgb, &cv_corners);
	}

	if (!qrcode.empty()) {
		SDL_Log("%u, {camera_work_frame}qrcode: %s", SDL_GetTicks(), qrcode.c_str());
		VALIDATE(cv_corners.size() == 4, null_str);
/*
		for (std::vector<cv::Point>::const_iterator it = cv_corners.begin(); it != cv_corners.end(); ++ it) {
			const cv::Point& pt = *it;
			corners.push_back(SDL_Point{pt.x, pt.y});
		}
*/
		const cv::Point* cv_corners_ptr = &cv_corners[0];

		corners.push_back(SDL_2Point{cv_corners_ptr[0].x, cv_corners_ptr[0].y, cv_corners_ptr[2].x, cv_corners_ptr[2].y});
		corners.push_back(SDL_2Point{cv_corners_ptr[1].x, cv_corners_ptr[1].y, cv_corners_ptr[3].x, cv_corners_ptr[3].y});

		corners.push_back(SDL_2Point{cv_corners_ptr[2].x, cv_corners_ptr[2].y, cv_corners_ptr[0].x, cv_corners_ptr[0].y});
		corners.push_back(SDL_2Point{cv_corners_ptr[3].x, cv_corners_ptr[3].y, cv_corners_ptr[1].x, cv_corners_ptr[1].y});
	}
}

SDL_DPoint3 tleagor_moveit_api::calculate_TP(const cv::Mat& argb, double dcpitch, const std::vector<SDL_2Point>& corners, const SDL_DPoint3* map_xyz)
{
	VALIDATE(corners.size() == 4, null_str);

	SDL_DPoint3 center_map_xyz;

	center_map_xyz.x = (map_xyz[0].x + map_xyz[2].x) / 2;
	center_map_xyz.y = (map_xyz[0].y + map_xyz[2].y) / 2;
	center_map_xyz.z = (map_xyz[0].z + map_xyz[2].z) / 2;

	if (target_info_.op == moveit_op_press_middle) {
		// SDL_DPoint3 std_offset{-0.065, 0, -0.100};
		// SDL_DPoint3 std_offset{-0.060, 0, -0.100};
		// SDL_DPoint3 std_offset{-0.065, 0, -0.095};
		SDL_DPoint3 std_offset{-0.063, 0, -0.098};

		SDL_DPoint3 offset2 = std_offset;

		double x_theta = atan2(corners[2].x1 - corners[3].x1, corners[2].y1 - corners[3].y1);
		double x_theta2 = atan2(corners[1].x1 - corners[0].x1, corners[1].y1 - corners[0].y1);
		double avg_theta = (x_theta + x_theta2) / 2;

		SDL_DPoint3 result = utils::transform_xyz_2D(0, 0, avg_theta, std_offset);
		SDL_Log("((%i, %i), (%i, %i), (%i, %i), (%i, %i)), theta: %.2f, theta2: %.2f, avg_theta: %.2f, result: (%.6f, %.6f, %.6f)", 
			corners[0].x1, corners[0].y1, corners[1].x1, corners[1].y1, 
			corners[2].x1, corners[2].y1, corners[3].x1, corners[3].y1, 
			RAD2DEG(x_theta), RAD2DEG(x_theta2), RAD2DEG(avg_theta),
			result.x, result.y, result.z);

		offset2 = result;

		center_map_xyz.x += offset2.x;
		center_map_xyz.y += offset2.y;
		center_map_xyz.z += offset2.z;
	}

	return center_map_xyz;
}

//
// tleagor_block_api
//
tleagor_block_api::tleagor_block_api()
	: b_api_(aplt::get_b_api())
	// , r_api_(aplt::get_r_api())
	, pinyin_(aplt::get_curr_pinyin())
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
{}

void tleagor_block_api::clear_output_vars(ttask_vars& task_vars, const std::vector<std::string>& output_vars)
{
	for (std::vector<std::string>::const_iterator it = output_vars.begin(); it != output_vars.end(); ++ it) {
		const std::string& name = *it;
		if (task_vars.existed(name) != 0) {
			task_vars.erase(name);
		}
	}
}

std::string tleagor_block_api::start_task_modify_env_var(const tapplet& aplt, ttask_vars& task_vars)
{
	const std::string var_name_var_name = utils::join_app_prefix_id(aplt.bundleid, "var_name");
	const std::string var_name_var_value = utils::join_app_prefix_id(aplt.bundleid, "var_value");

	std::vector<std::string> var_names;
	var_names.push_back(var_name_var_name);
	var_names.push_back(var_name_var_value);

	// const ttask_var* name_BI_var = nullptr;
	int name_var_type = nposm;
	const ttask_var* value_var = nullptr;
	utils::string_map symbols;
	for (std::vector<std::string>::const_iterator it = var_names.begin(); it != var_names.end(); ++ it) {
		const std::string& var_name = *it;
		symbols["var"] = var_name;

		if (task_vars.existed(var_name) == 0) {
			return vgettext2("Missing input var: $var", symbols);
		}

		const ttask_var& var = task_vars.get_var(var_name);
		if (var.is_array) {
			return vgettext2("Input var($var) must be single var", symbols);
		}
		const int val_type = var.val.type();

		if (var_name == var_name_var_name) {
			if (val_type != var_type_string) {
				return vgettext2("Input var($var)'s type must be string", symbols);
			}

			const std::string var_name2 = var.val.str();
			// var_name2 is env_var, may not in @task_vars
			symbols["var"] = var_name2;
			if (aplt::BI_env_var_id_2_types.count(var_name2) == 0) {
				return vgettext2("var($var) isn't environment variable.", symbols);
			}
			name_var_type = aplt::BI_env_var_id_2_types.find(var_name2)->second;

		} else {
			value_var = &var;
		}
	}

	if (BI_var_is_auto_update(name_var_type)) {		
		return vgettext2("var($var) is an environment variable, but it is automatically updated by the system and cannot be modified by the user.", symbols);
	}

	config::attribute_value val;
	if (BI_var_val_type_is_bool(name_var_type)) {
		bool b = value_var->val.to_bool2();
		val.from_bool(b);

	} else if (BI_var_val_type_is_string(name_var_type)) {
		std::string b = value_var->val.str();
		val.from_string(b, true);

	} else {
		VALIDATE(BI_var_val_type_is_integer(name_var_type), null_str);
		int64_t n64 = value_var->val.to_int64_2();
		val.from_int64(n64);
	}
	b_api_.aplt_set_env_var(name_var_type, val);
	
	return null_str;
}

std::string tleagor_block_api::start_task_alert(const tapplet& aplt, ttask_vars& vars)
{
	const std::string var_name_var_msg = utils::join_app_prefix_id(aplt.bundleid, "msg");
	const std::string var_name_var_persist = utils::join_app_prefix_id(aplt.bundleid, "persist");
	const std::string var_name_var_voice = utils::join_app_prefix_id(aplt.bundleid, "voice");
	const std::string var_name_var_floating_window = utils::join_app_prefix_id(aplt.bundleid, "floating_window");

	utils::string_map symbols;
	std::string msg;
	symbols["var"] = utils::split_app_prefix_id(var_name_var_msg).second;
	if (vars.existed(var_name_var_msg)) {
		msg = vars.get_string(var_name_var_msg);
	}
	// 'msg' allows empty. it mean stop speak or repeat speak.
	
	// if (msg.empty()) {
	//	return vgettext2("Input var($var)'s cannot be empty", symbols);
	// }

	bool persist = false;
	if (vars.existed(var_name_var_persist)) {
		persist = vars.get_bool(var_name_var_persist);
	}

	if (vars.existed(var_name_var_voice)) {
		if (vars.get_bool(var_name_var_voice)) {
			if (!persist) {
				pinyin_.speak(msg);
			} else {
				pinyin_.repeat_speak(msg);
			}
		}
	}

	if (vars.existed(var_name_var_floating_window) && !msg.empty()) {
		if (vars.get_bool(var_name_var_floating_window)) {
			const int non_persist_duration = 10000; // 10 second
			b_api_.push_floating_window_task(msg, persist? nposm: non_persist_duration);
		}
	}

	return null_str;
}

std::string tleagor_block_api::start_task_privacy(const tapplet& aplt, ttask_vars& vars)
{
	const std::string var_name_protect = utils::join_app_prefix_id(aplt.bundleid, "protect");

	utils::string_map symbols;
	std::string msg;

	bool protect = false;
	if (vars.existed(var_name_protect)) {
		protect = vars.get_bool(var_name_protect);
	}

	b_api_.set_privacy_protect(protect);

	return null_str;
}

void tleagor_block_api::did_start_task_base_scene_quited(ttask_vars& vars, std::string& err_msg, const std::string& var_name_retbool, const std::string& var_name_result_msg)
{
	vars.insert_bool(var_name_retbool, false, err_msg.empty());
	if (!err_msg.empty()) {
		vars.insert_string(var_name_result_msg, false, err_msg);
	}
}

std::string tleagor_block_api::start_task_base_scene(const tapplet& aplt, ttask_vars& vars)
{
	const std::string var_name_name = utils::join_app_prefix_id(aplt.bundleid, "name");
	const std::string var_name_action = utils::join_app_prefix_id(aplt.bundleid, "action");
	const std::string var_name_retbool = utils::join_app_prefix_id(aplt.bundleid, "bs_retbool");
	const std::string var_name_result_msg = utils::join_app_prefix_id(aplt.bundleid, "bs_result_msg");

	std::vector<std::string> output_vars = {var_name_retbool, var_name_result_msg};
	clear_output_vars(vars, output_vars);

	std::string err_msg;
	tauto_destruct_executor destruct_executor(std::bind(&tleagor_block_api::did_start_task_base_scene_quited, this, std::ref(vars), std::ref(err_msg), var_name_retbool, var_name_result_msg));

	utils::string_map symbols;

	std::string desire_name;
	// std::string py_desire_name;

	if (vars.existed(var_name_name)) {
		desire_name = vars.get_string(var_name_name);
		if (!desire_name.empty()) {
			// py_desire_name = pinyin_.from_utf8str2(desire_name, tone_, eng_lowercase_);
		}
	}
/*
	const std::vector<tbase_scene>& scenes = ros_.aplt_base_scenes();
	if (scenes.empty()) {
		err_msg = _("There is no scene.");
		symbols["err_msg"] = err_msg;
		return vgettext2("Switch scene failed. $err_msg", symbols);
	}
*/
	// switch to next, or idle current, or resume current.
	// enum {action_switch_to_next, action_idle, action_resume};
	std::map<std::string, int> actions = {
		{"switch_to_next", bs_action_switch_to_next},
		{"idle", bs_action_idle}, 
		{"resume", bs_action_resume},
	};

	int action = bs_action_switch_to_next;
	if (vars.existed(var_name_action)) {
		const std::string str = vars.get_string(var_name_action);
		if (!str.empty()) {
			if (actions.count(str) == 0) {
				symbols["action"] = str;
				err_msg = vgettext2("Unsupported action: $action.", symbols);

				symbols["err_msg"] = err_msg;
				return vgettext2("Switch scene failed. $err_msg", symbols);
			}
			action = actions.find(str)->second;
		}
	}
	
	const tbase_scene* new_scene = handle_base_scene(b_api_, nposm, desire_name, action, err_msg);

	if (new_scene != nullptr) {
		vars.insert_string(var_name_result_msg, false, new_scene->name());

	} else {
		VALIDATE(!err_msg.empty(), null_str);
		symbols["err_msg"] = err_msg;
		return vgettext2("Switch scene failed. $err_msg", symbols);
	}

	return null_str;
}

static std::string join_err_msg(const std::string& tag, const std::string& reason)
{
	utils::string_map symbols;
	symbols["tag"] = tag;
	symbols["reason"] = reason;

	return vgettext2("$tag. $reason", symbols);
}
/*
// month: [1, 12]
int64_t posix_mktime(int year, int month, int day, int hour, int minute, int second)
{
	VALIDATE(month >= 1 && month <= 12, null_str);
	VALIDATE(day >= 1 && day <= 31, null_str);

	struct tm tm1;
	memset(&tm1, 0, sizeof(tm1));

	tm1.tm_year = year;
	tm1.tm_mon = month;
	tm1.tm_mday = day;

	tm1.tm_hour = hour;
	tm1.tm_min = minute;
	tm1.tm_sec = second;

	// Don't have to judge the validity of each field of tm1, 
	// it will be guaranteed to be valid by 'carrying'
	
	tm1.tm_year -= 1900;
	tm1.tm_mon --;
	tm1.tm_isdst = -1;

	return mktime(&tm1);
}
*/
std::string tleagor_block_api::start_task_timed_task(const tapplet& aplt, ttask_vars& vars)
{
	const std::string var_name_task_cfg = utils::join_app_prefix_id(aplt.bundleid, "task_cfg");
	const std::string var_name_retbool = utils::join_app_prefix_id(aplt.bundleid, "tt_retbool");

	std::vector<std::string> output_vars = {var_name_retbool};
	clear_output_vars(vars, output_vars);

	config task_cfg;
	if (vars.existed(var_name_task_cfg)) {
		std::string task_cfg_str = vars.get_string(var_name_task_cfg);
		// aplt::read_config_ex(task_cfg_str, true, task_cfg);
		if (!task_cfg_str.empty()) {
			aplt::read_config(task_cfg_str, task_cfg);
		}
	}

	const std::string err_tag = _("Add timed task failed");
	if (task_cfg.empty()) {
		return join_err_msg(err_tag, _("'task_cfg' cannot be empty"));
	}

	std::map<int64_t, tb_api::tadd_timed_task> tasks;
	cfg_to_add_timed_tasks(task_cfg, tasks);

	if (tasks.empty()) {
		return join_err_msg(err_tag, _("'task_cfg' is invalid"));
	}

	b_api_.add_timed_task(tasks);

	return null_str;
}

std::string tleagor_block_api::app_start_task(ttask_vars& vars)
{
	VALIDATE_IN_MAIN_THREAD();
	const tapplet& aplt = *curr_aplt;

	if (task_id_ == "modify_env_var") {
		return start_task_modify_env_var(aplt, vars);

	} else if (task_id_ == "alert") {
		return start_task_alert(aplt, vars);

	} else if (task_id_ == "privacy") {
		return start_task_privacy(aplt, vars);

	} else if (task_id_ == "base_scene") {
		return start_task_base_scene(aplt, vars);

	} else if (task_id_ == "timed_task") {
		return start_task_timed_task(aplt, vars);
	}

	utils::string_map symbols;
	symbols["task"] = task_id_;
	return vgettext2("Unknonw task: $task", symbols);
}

}

void* aplt_create_task_api(void* aplt1)
{
	aplt::tleagor_cpp_api* cpp_api = new aplt::tleagor_cpp_api();
	aplt::tleagor_moveit_api* moveit_api = new aplt::tleagor_moveit_api();
	aplt::tleagor_block_api* block_api = new aplt::tleagor_block_api();
	aplt::tleagor_nonblock_api* nonblock_api = new aplt::tleagor_nonblock_api();
	aplt::tleagor_aiagent_api* aiagent_api = new aplt::tleagor_aiagent_api();

	aplt::ttask_api* leagor = new aplt::ttask_api(*aplt::curr_aplt, cpp_api, moveit_api, nullptr, block_api, nonblock_api, aiagent_api);
	return leagor;
}
