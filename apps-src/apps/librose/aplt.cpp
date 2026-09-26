/*
   Copyright (C) 2009 - 2018 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Provides a Lua interpreter, to be embedded in WML.
 *
 * @note Naming conventions:
 *   - intf_ functions are exported in the rose domain,
 *   - impl_ functions are hidden inside metatables,
 *   - cfun_ functions are closures,
 *   - luaW_ functions are helpers in Lua style.
 */
#define GETTEXT_DOMAIN "rose-lib"

#include "aplt.hpp"
#include "aplt_common.hpp"

#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vconfig.hpp"
#include "scripts/gui/dialogs/rldialog.hpp"

#include "gui/widgets/settings.hpp"
#include "gui/dialogs/message.hpp"
#include "serialization/string_utils.hpp"
#include "rose_config.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "config_cache.hpp"
#include "xwml.hpp"
#include "wml_exception.hpp"
#include "base_instance.hpp"
#include "protobuf.hpp"
#include "chinese.hpp"

#include <lua/lauxlib.h>

using namespace std::placeholders;


namespace aplt {

const version_info min_aplt_rose_ver("1.0.1-20260926");

const std::string file_launcher_android = "launcher.android";
const std::string file_kdesktop_android = "kdesktop.android";
const std::string file_chinese_pinyin = "chinese.py";
const std::string file_latex_data = "latex.data";

std::map<int, tbuildin> all_fake_applets;
std::map<int, tcode3> aplt_drivers;
char base_subtask_states[sts_count][48];

static task_type_t task_type_from_str(const std::string& str)
{
	VALIDATE(!task_types.empty(), null_str);

	for (std::map<task_type_t, std::string>::const_iterator it = task_types.begin(); it != task_types.end(); ++ it) {
		const std::string& type = it->second;
		if (type == str) {
			return it->first;
		}
	}
	return task_type_nposm;
}

const std::map<int, std::string> task_nonblock_subtypes {
	{tnonblock_api::subtype_charge, "charge"},
	{tnonblock_api::subtype_nlp, "nlp"},
};

static int task_subtype_from_str(int type, const std::string& str, bool validate)
{
	if (type != task_nonblock) {
		if (validate) {
			VALIDATE(str.empty(), "if not task_nonblock, subtype must be empty");
		}
		return nposm;
	}
	VALIDATE(!task_nonblock_subtypes.empty(), null_str);
	for (std::map<int, std::string>::const_iterator it = task_nonblock_subtypes.begin(); it != task_nonblock_subtypes.end(); ++ it) {
		const std::string& type = it->second;
		if (type == str) {
			return it->first;
		}
	}

	if (validate) {
		std::stringstream err;
		err << "unknown subtype: " << str;
		VALIDATE(false, err.str());
	}
	return nposm;
}

static per_t permission_per_from_id(const std::string& id)
{
	VALIDATE(!sys_permissions.empty(), null_str);

	for (std::map<int, tpermission>::const_iterator it = sys_permissions.begin(); it != sys_permissions.end(); ++ it) {
		const tpermission& per = it->second;
		if (per.id == id) {
			return per.per;
		}
	}
	return per_nposm;
}

static var_type_t var_type_from_str(const std::string& str)
{
	VALIDATE(!var_types.empty(), null_str);

	for (std::map<var_type_t, tcode3>::const_iterator it = var_types.begin(); it != var_types.end(); ++ it) {
		const tcode3& type = it->second;
		if (type.id == str) {
			return it->first;
		}
	}
	return var_type_nposm;
}

static std::map<std::string, int> reserved_task_id_2_codes;

void initial()
{
	VALIDATE(sources.size() == src_count, null_str);
	// sources.insert(std::make_pair(src_distribution, "distribution"));
	// sources.insert(std::make_pair(src_development, "development"));
	// sources.insert(std::make_pair(src_studio, "studio"));
	// VALIDATE(sources.size() == src_count, null_str);
	for (std::map<int, std::string>::const_iterator it = sources.begin(); it != sources.end(); ++ it) {
		const std::string& name = it->second;
		VALIDATE(name.size() < RSP_MAXAPPLETIDBYTES - RSP_MAXBUNDLEIDBYTES - 2, null_str);
	}

	VALIDATE(apps.size() == app_count, null_str);

	all_fake_applets.insert(std::make_pair(aplt::builtinid_store, 
		aplt::tbuildin(aplt::builtinid_store, "misc/builtinid_store.png", _("fakeid^Store"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_settings, 
		aplt::tbuildin(aplt::builtinid_settings, "misc/builtinid_settings.png", _("fakeid^Settings"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_klink, 
		aplt::tbuildin(aplt::builtinid_klink, "misc/builtinid_klink.png", _("fakeid^kLink"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_speech, 
		aplt::tbuildin(aplt::builtinid_speech, "misc/builtinid_speech.png", _("fakeid^Speech"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_task, 
		aplt::tbuildin(aplt::builtinid_task, "misc/builtinid_task.png", _("fakeid^Task"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_artifact, 
		aplt::tbuildin(aplt::builtinid_artifact, "misc/builtinid_artifact.png", _("fakeid^Artifact"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_mkscript, 
		aplt::tbuildin(aplt::builtinid_mkscript, "misc/builtinid_mkscript.png", _("fakeid^Make script"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_mkcourse,
		aplt::tbuildin(aplt::builtinid_mkcourse, "misc/builtinid_mkcourse.png", _("fakeid^Make course"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_health,
		aplt::tbuildin(aplt::builtinid_health, "misc/builtinid_health.png", _("fakeid^Health"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_map, 
		aplt::tbuildin(aplt::builtinid_map, "misc/builtinid_map.png", _("fakeid^Map"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_moveit, 
		aplt::tbuildin(aplt::builtinid_moveit, "misc/builtinid_moveit.png", _("fakeid^Moveit"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_dnn, 
		aplt::tbuildin(aplt::builtinid_dnn, "misc/builtinid_dnn.png", _("fakeid^DNN"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_center, 
		aplt::tbuildin(aplt::builtinid_center, "misc/builtinid_center.png", _("fakeid^Center"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_mic, 
		aplt::tbuildin(aplt::builtinid_mic, "misc/builtinid_mic.png", _("fakeid^Mic"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_explorer,
		aplt::tbuildin(aplt::builtinid_explorer, "misc/builtinid_explorer.png", _("fakeid^Explorer"))));
	all_fake_applets.insert(std::make_pair(aplt::builtinid_dcamera,
		aplt::tbuildin(aplt::builtinid_dcamera, "misc/builtinid_camera.png", 
		game_config::app_code == aplt::app_launcher? _("fakeid^Depth Camera"): _("Camera"))));
	VALIDATE(all_fake_applets.size() == builtinid_count - MAX_APPLETS, null_str);

	// enum {taskid_basic_alert, taskid_basic_base_scene, taskid_basic_timed_task, taskid_workout};
	// extern LIB3RDPARTY_DECL const std::map<int, tcode3> reserved_tasks;

	reserved_tasks.insert(std::make_pair(taskid_basic_alert, tcode3(taskid_basic_alert, "alert", null_str)));
	reserved_tasks.insert(std::make_pair(taskid_basic_base_scene, tcode3(taskid_basic_base_scene, "base_scene", null_str)));
	reserved_tasks.insert(std::make_pair(taskid_basic_timed_task, tcode3(taskid_basic_timed_task, "timed_task", null_str)));
	reserved_tasks.insert(std::make_pair(taskid_workout, tcode3(taskid_workout, "workout", _("task^workout"))));
	VALIDATE(reserved_tasks.size() == taskid_reserved_count, null_str);

	for (std::map<int, tcode3>::const_iterator it = reserved_tasks.begin(); it != reserved_tasks.end(); ++ it) {
		const tcode3& code3 = it->second;
		reserved_task_id_2_codes.insert(std::make_pair(code3.id, code3.code));
	}
	VALIDATE((int)reserved_task_id_2_codes.size() == taskid_reserved_count, null_str);

	sys_permissions.insert(std::make_pair(per_ble, aplt::tpermission(per_ble, "ble", _("per^ble"))));
	sys_permissions.insert(std::make_pair(per_navigation, aplt::tpermission(per_navigation, "navigation", _("per^navigation"))));
	sys_permissions.insert(std::make_pair(per_camera, aplt::tpermission(per_camera, "camera", _("per^camera"))));
	sys_permissions.insert(std::make_pair(per_moveit, aplt::tpermission(per_moveit, "moveit", _("per^moveit"))));


	minor_key_strategies.insert(std::make_pair(mkeys_any_one, _("keys^any_one")));
	minor_key_strategies.insert(std::make_pair(mkeys_all_match_and_order, _("keys^all_match_and_order")));
	minor_key_strategies.insert(std::make_pair(mkeys_all_match_no_order, _("keys^all_match_no_order")));
	VALIDATE(minor_key_strategies.size() == aplt::mkeys_count, null_str);


	// --iot's sensor
	std::vector<tiot_src2> iot_code_id3s = {
		{iot_src_doorbell, "doorbell", _("iotsrc^doorbell"), {iot_evt_pressdown}},
		{iot_src_doorcontact, "doorcontact", _("iotsrc^doorcontact"), {iot_evt_open, iot_evt_close}},
		{iot_src_ir_motion_sensor, "iot_src_ir_motion_sensor", _("iotsrc^ir_motion_sensor"), {iot_evt_trigger}},
		{iot_src_smoke_sensor, "smoke_sensor", _("iotsrc^smoke_sensor"), {iot_evt_trigger}},
		{iot_src_gas_sensor, "gas_sensor", _("iotsrc^gas_sensor"), {iot_evt_trigger}},
		{iot_src_co_sensor, "co_sensor", _("iotsrc^co_sensor"), {iot_evt_trigger}},
		{iot_src_water_leak_sensor, "water_leak_sensor", _("iotsrc^water_leak_sensor"), {iot_evt_trigger}},
		{iot_src_vibration_sensor, "vibration_sensor", _("iotsrc^vibration_sensor"), {iot_evt_trigger}},
		{iot_src_infrated_emission_detector, "infrated_emission_detector", _("iotsrc^infrated_emission_detector"), {iot_evt_trigger}},
		{iot_src_glass_break_detector, "glass_break_detector", _("iotsrc^glass_break_detector"), {iot_evt_trigger}},
		{iot_src_sos_button, "sos_button", _("iotsrc^sos_button"), {iot_evt_trigger}},
		{iot_src_remote_controller, "remote_controller", _("iotsrc^remote_controller"), {iot_evt_trigger}},
		{iot_src_keypad, "keypad", _("iotsrc^keypad"), {iot_evt_trigger}},

		{iot_src_lamp, "lamp", _("iotsrc^lamp"), {}},
		{iot_src_curtain, "curtain", _("iotsrc^curtain"), {}}
	};

	std::pair<std::map<int, tiot_src2>::iterator, bool> ins;
	for (std::vector<tiot_src2>::const_iterator it = iot_code_id3s.begin(); it != iot_code_id3s.end(); ++ it) {
		const tiot_src2& src = *it;
		if (src.code >= iot_src_sensor_min && src.code <= iot_src_sensor_max) {
			VALIDATE(!src.events.empty(), null_str);
		} else {
			VALIDATE(src.events.empty(), null_str);
		}
		ins = iot_sources.insert(std::make_pair(src.code, src));
		VALIDATE(ins.second, null_str);
	}
	VALIDATE((int)iot_sources.size() == iot_src_sensor_max - iot_src_sensor_min + 1 + iot_src_device_max - iot_src_device_min + 1, null_str);


	iot_events.insert(std::make_pair(iot_evt_pressdown, 
		tcode3(iot_evt_pressdown, "pressdown", _("iotevt^pressdown"))));
	iot_events.insert(std::make_pair(iot_evt_pressup, 
		tcode3(iot_evt_pressup, "pressup", _("iotevt^pressup"))));
	iot_events.insert(std::make_pair(iot_evt_trigger, 
		tcode3(iot_evt_trigger, "trigger", _("iotevt^trigger"))));
	iot_events.insert(std::make_pair(iot_evt_open, 
		tcode3(iot_evt_open, "open", _("iotevt^open"))));
	iot_events.insert(std::make_pair(iot_evt_close, 
		tcode3(iot_evt_close, "close", _("iotevt^close"))));
	VALIDATE((int)iot_events.size() == iot_evt_count, null_str);

	utils::string_map symbols;
	// aplt task's priority
	for (int at = priority_nontimed_min; at <= priority_nontimed_max; at ++) {
		if (at == priority_timed) {
			continue;
		}
		symbols["priority"] = str_cast(at);
		nontimed_priorities.insert(std::make_pair(at, vgettext2("PRI-$priority", symbols)));
	}
	// '-1' is priority_timing.
	VALIDATE((int)nontimed_priorities.size() == priority_nontimed_max - priority_nontimed_min + 1 - 1, null_str);

	//
	// var_type
	//
	var_types.insert(std::make_pair(var_type_bool, tcode3(var_type_bool, "bool", _("vartype^bool"))));
	var_types.insert(std::make_pair(var_type_integer, tcode3(var_type_integer, "integer", _("vartype^integer"))));
	var_types.insert(std::make_pair(var_type_double, tcode3(var_type_double, "double", _("vartype^double"))));
	var_types.insert(std::make_pair(var_type_string, tcode3(var_type_string, "string", _("vartype^string"))));
	var_types.insert(std::make_pair(var_type_tstring, tcode3(var_type_tstring, "tstring", _("vartype^tstring"))));
	var_types.insert(std::make_pair(var_type_array, tcode3(var_type_array, "array", _("vartype^array"))));
	VALIDATE(var_types.size() == var_type_count, null_str);

	//
	// function
	//
	VALIDATE(functions.empty(), null_str);
	functions.insert(std::make_pair(tfunction::func_add, tfunction_code(tfunction::func_add, 
		"add", {"var", "operand"}, _("function^add desc"))));
	functions.insert(std::make_pair(tfunction::func_sub, tfunction_code(tfunction::func_sub, 
		"sub", {"var", "operand"}, _("function^sub desc"))));
	functions.insert(std::make_pair(tfunction::func_mul, tfunction_code(tfunction::func_mul, 
		"mul", {"var", "operand"}, _("function^mul desc"))));
	functions.insert(std::make_pair(tfunction::func_div, tfunction_code(tfunction::func_div, 
		"div", {"var", "operand"}, _("function^div desc"))));

	functions.insert(std::make_pair(tfunction::func_inv_bool, tfunction_code(tfunction::func_inv_bool, 
		"inv_bool", {"var"}, _("function^inv_bool desc"))));

	functions.insert(std::make_pair(tfunction::func_size, tfunction_code(tfunction::func_size, 
		"size", {"var"}, _("function^size desc"))));
	functions.insert(std::make_pair(tfunction::func_sum, tfunction_code(tfunction::func_sum, 
		"sum", {"var"}, _("function^sum desc"))));
	functions.insert(std::make_pair(tfunction::func_element, tfunction_code(tfunction::func_element, 
		"element", {"var", "index"}, _("function^element desc"))));
	functions.insert(std::make_pair(tfunction::func_join_element, tfunction_code(tfunction::func_join_element, 
		"join_element", {"var", "base", "prefix", "postfix", "connector", "add_comma"}, _("function^join_element desc"))));
	functions.insert(std::make_pair(tfunction::func_join_index, tfunction_code(tfunction::func_join_index, 
		"join_index", {"size", "base", "prefix", "postfix", "connector", "add_comma"}, _("function^join_index desc"))));
	functions.insert(std::make_pair(tfunction::func_join2_2element, tfunction_code(tfunction::func_join2_2element, 
		"join2_2element", {"var0", "var1", "max_size", "prefix", "middle", "postfix", "connector", "add_comma"}, _("function^join2_2element desc"))));

	functions.insert(std::make_pair(tfunction::func_res_path, tfunction_code(tfunction::func_res_path, 
		"res_path", {"aplt_id"}, _("function^res_path desc"))));
	functions.insert(std::make_pair(tfunction::func_preferences_dir, tfunction_code(tfunction::func_preferences_dir, 
		"preferences_dir", {"aplt_id"}, _("function^preferences_dir desc"))));

	functions.insert(std::make_pair(tfunction::func_format_time_date, tfunction_code(tfunction::func_format_time_date, 
		"format_time_date", {"t"}, _("function^format_time_date desc"))));
	functions.insert(std::make_pair(tfunction::func_format_elapse_hms, tfunction_code(tfunction::func_format_elapse_hms, 
		"format_elapse_hms", {"elapse"}, _("function^format_elapse_hms desc"))));
	functions.insert(std::make_pair(tfunction::func_is_speaking, tfunction_code(tfunction::func_is_speaking, 
		"is_speaking", {}, _("function^is_speaking desc"))));
	functions.insert(std::make_pair(tfunction::func_is_wko_task_finished, tfunction_code(tfunction::func_is_wko_task_finished, 
		"is_wko_task_finished", {}, _("function^is_wko_task_finished desc"))));
	VALIDATE((int)functions.size() == tfunction::func_builtin_count, null_str);

	for (std::map<int, tfunction_code>::const_iterator it = functions.begin(); it != functions.end(); ++ it) {
		const tfunction_code& func = it->second;
		function_keys.insert(std::make_pair(func.id, func.code));
	}

	//
	// var, tif_block
	//

	bool_set_types.insert(std::make_pair(bool_set_none, 
		tcode3(bool_set_none, "set_none", _("bool^set_none"))));
	bool_set_types.insert(std::make_pair(bool_set_false, 
		tcode3(bool_set_false, "set_false", _("bool^set_false"))));
	bool_set_types.insert(std::make_pair(bool_set_true, 
		tcode3(bool_set_true, "set_true", _("bool^set_true"))));
	VALIDATE(bool_set_types.size() == bool_set_count, null_str);

	if_judge_ops.insert(std::make_pair(tif_judge::op_var_exist, 
		tcode3(tif_judge::op_var_exist, "var_exist", _("op^var_exist"))));
	if_judge_ops.insert(std::make_pair(tif_judge::op_var_not_exist, 
		tcode3(tif_judge::op_var_not_exist, "var_not_exist", _("op^var_not_exist"))));

	if_judge_ops.insert(std::make_pair(tif_judge::op_string_equal, 
		tcode3(tif_judge::op_string_equal, "string_equal", _("op^string_equal"))));

	if_judge_ops.insert(std::make_pair(tif_judge::op_bool_equal, 
		tcode3(tif_judge::op_bool_equal, "bool_equal", _("op^bool_equal"))));
	if_judge_ops.insert(std::make_pair(tif_judge::op_bool_not_equal, 
		tcode3(tif_judge::op_bool_not_equal, "bool_not_equal", _("op^bool_not_equal"))));

	if_judge_ops.insert(std::make_pair(tif_judge::op_numerical_equal, 
		tcode3(tif_judge::op_numerical_equal, "numerical_equal", _("op^numerical_equal"))));
	if_judge_ops.insert(std::make_pair(tif_judge::op_numerical_not_equal, 
		tcode3(tif_judge::op_numerical_not_equal, "numerical_not_equal", _("op^numerical_not_equal"))));
	if_judge_ops.insert(std::make_pair(tif_judge::op_greater_than, 
		tcode3(tif_judge::op_greater_than, "greater_than", _("op^greater_than"))));
	if_judge_ops.insert(std::make_pair(tif_judge::op_less_than, 
		tcode3(tif_judge::op_less_than, "less_than", _("op^less_than"))));
	if_judge_ops.insert(std::make_pair(tif_judge::op_greater_than_equal_to, 
		tcode3(tif_judge::op_greater_than_equal_to, "greater_than_equal_to", _("op^greater_than_equal_to"))));
	if_judge_ops.insert(std::make_pair(tif_judge::op_less_than_equal_to, 
		tcode3(tif_judge::op_less_than_equal_to, "less_than_equal_to", _("op^less_than_equal_to"))));

	VALIDATE(if_judge_ops.size() == tif_judge::op_count, null_str);

	if_logic_ops.insert(std::make_pair(tif_judge::logic_and, 
		tcode3(tif_judge::logic_and, "and", _("logic^and"))));
	if_logic_ops.insert(std::make_pair(tif_judge::logic_or, 
		tcode3(tif_judge::logic_or, "or", _("logic^or"))));
	VALIDATE(if_logic_ops.size() == tif_judge::logic_count, null_str);

	charge_pos_name = _("Charge");
	privacy_protect_msgstr = _("Currently in privacy protection state");

	aplt_drivers.insert(std::make_pair(apltsotype_base, tcode3(apltsotype_base,
		"base_driver", _("Base driver"))));
	aplt_drivers.insert(std::make_pair(apltsotype_laser, tcode3(apltsotype_laser,
		"laser_driver", _("Laser driver"))));
	aplt_drivers.insert(std::make_pair(apltsotype_moveit, tcode3(apltsotype_moveit,
		"moveit_driver", _("Moveit driver"))));
	aplt_drivers.insert(std::make_pair(apltsotype_dcamera, tcode3(apltsotype_dcamera,
		"dcamera_driver", _("DCamera driver"))));
	aplt_drivers.insert(std::make_pair(apltsotype_iot, tcode3(apltsotype_iot,
		"iot_driver", _("IoT driver"))));
	aplt_drivers.insert(std::make_pair(apltsotype_speech, tcode3(apltsotype_speech,
		"speech_driver", _("Speech driver"))));
	aplt_drivers.insert(std::make_pair(apltsotype_ai, tcode3(apltsotype_ai,
		"ai_driver", _("AI driver"))));
	aplt_drivers.insert(std::make_pair(apltsotype_fgaplt, tcode3(apltsotype_fgaplt,
		"fg_applet", _("Foreground applet"))));
	aplt_drivers.insert(std::make_pair(apltsotype_base2th, tcode3(apltsotype_base2th,
		"base2th_applet", _("Base2th applet"))));
	aplt_drivers.insert(std::make_pair(apltsotype_aiagent_task, tcode3(apltsotype_aiagent_task,
		"aiagent_task_applet", _("Aiagent task applet"))));
	VALIDATE((int)aplt_drivers.size() == apltsotype_count, null_str);

	SDL_snprintf(base_subtask_states[aplt::sts_nposm], sizeof(base_subtask_states[aplt::sts_nposm]), "%s", "nposm");
	SDL_snprintf(base_subtask_states[aplt::sts_ing], sizeof(base_subtask_states[aplt::sts_ing]), "%s", _("Ing"));
	SDL_snprintf(base_subtask_states[aplt::sts_idle], sizeof(base_subtask_states[aplt::sts_idle]), "%s", _("sts^Idle"));
	SDL_snprintf(base_subtask_states[aplt::sts_preempted], sizeof(base_subtask_states[aplt::sts_preempted]), "%s", _("Preempted"));
	VALIDATE(sizeof(base_subtask_states) / sizeof(base_subtask_states[0]) == aplt::sts_count, null_str);

}


// class tapplet {
//   std::map<const std::string, const aplt::tapplet::ttask> timings;
// };
// I want define below state in tapplet, but compile faie on android.
// static std::map<std::string, tapplet::ttask> timings;

// #define get_libroseaplt_so_path(res_path)	aplt_so_path(res_path, LIBROSEAPLT_SO)

void get_settings_cfg(const std::string& aplt_path, bool alert_valid, tapplet& aplt)
{
	enum {invalid_bundleid, invalid_version};

	const std::string file_path = aplt_path + "/settings.cfg";

	config cfg;
	config_cache& cache = config_cache::instance();
	cache.get_config(file_path, cfg);
	cache.clear_defines();

	std::string bundleid;
	int errcode = nposm;
	if (cfg && !cfg.empty()) {
		 bundleid = cfg["bundle_id"].str();
		if (is_bundleid(bundleid)) {
			version_info version(cfg["version"].str());
			if (version.is_rose_recommended()) {
				aplt.version = version;
				aplt.base_driver = cfg["base_driver"].to_bool();
				aplt.laser_driver = cfg["laser_driver"].to_bool();
				aplt.moveit_driver = cfg["moveit_driver"].to_bool();
				aplt.dcamera_driver = cfg["dcamera_driver"].to_bool();
				aplt.iot_driver = cfg["iot_driver"].to_bool();
				aplt.speech_driver = cfg["speech_driver"].to_bool();
				aplt.ai_driver = cfg["ai_driver"].to_bool();

				aplt.lua_base_slot = cfg["lua_base_slot"].to_bool();
				aplt.lua_task_api = cfg["lua_task_api"].to_bool();

				if (aplt.lua_base_slot || aplt.lua_task_api) {
					const std::string libroseaplt_so_path = get_libroseaplt_so_path(aplt_path);
					if (SDL_IsFile(libroseaplt_so_path.c_str())) {
						// this applet doesn' exist libroseaplt.so
						VALIDATE(false, "if 'lua_base_slot' or 'lua_task_api' is true, cannot exist libroseaplt.so");
					}
				}

				// key: persmissions
				std::vector<std::string> v_permissions = utils::split(cfg["permissions"].str());
				for (std::vector<std::string>::const_iterator it = v_permissions.begin(); it != v_permissions.end(); ++ it) {
					const std::string& id = *it;
					per_t per = permission_per_from_id(id);
					VALIDATE(per != nposm, null_str);
					aplt.permissions.insert(per);
				}
				fill_per_if_necessary(aplt.permissions);
				
				// key: objects(discarded)
				
				const std::set<std::string> reserved_var_names = {ble_builtin_var_name(null_str, ble_var_device_id)};
				// child: [task]
				BOOST_FOREACH (const config& task, cfg.child_range("task")) {
					const std::string& id = task["id"].str();
					VALIDATE(isvalid_normal_id_or_var_name224(id), null_str);
					VALIDATE(aplt.tasks.count(id) == 0, null_str);
					// const std::string object = timing["object"].str();

					task_type_t type = task_type_from_str(task["type"].str());
					VALIDATE(type >= 0 && type < task_type_count, null_str);
					int subtype = task_subtype_from_str(type, task["subtype"].str(), true);

					bool recoverable = task["recoverable"].to_bool();
					bool nonpreemptive = task["nonpreemptive"].to_bool();
					if (recoverable || nonpreemptive) {
						VALIDATE(type == task_cpp, "if recoverable or nonpreemptive is true, task must be task_cpp");
					}
					bool no_swap_wh_for_screen = task["no_swap_wh_for_screen"].to_bool();
					if (no_swap_wh_for_screen) {
						VALIDATE(type == task_camera, null_str);
					}
					int src = nposm;
					if (type == task_ble) {
						src = iot_src_from_str(task["src"].str());
						VALIDATE(IS_VALID_APLT_TASK_IOT_SRC(src), null_str);

					} else if (task.has_attribute("src")) {
						SDL_Log("aplt task's type: %i, don't has 'src' key", type);
					}

					std::vector<std::string> v_permissions = utils::split(task["permissions"].str());
					std::set<per_t> permissions;
					for (std::vector<std::string>::const_iterator it = v_permissions.begin(); it != v_permissions.end(); ++ it) {
						const std::string& id = *it;
						per_t per = permission_per_from_id(id);
						VALIDATE(per != nposm, null_str);
						permissions.insert(per);
					}
					fill_per_if_necessary(permissions);

					// int max_fails = task["max_fails"].to_int(1);
					const int ble_max_files = 2;
					int max_fails = type == task_ble? ble_max_files: 1;

					// [var][/var]
					std::vector<tapplet::tvar> vars;
					if (type == task_ble) {
						vars.push_back(tapplet::tvar(ble_builtin_var_name(null_str, ble_var_device_id), 
							true, var_type_string, false));
					}

					std::set<std::string> existed_var_names;
					bool has_optional_input_var = false;
					bool has_output_var = false;
					BOOST_FOREACH (const config& var_cfg, task.child_range("var")) {
						const std::string name = var_cfg["name"].str();
						VALIDATE(isvalid_normal_id_or_var_name224(name), null_str);
						VALIDATE(reserved_var_names.count(name) == 0, null_str);
						VALIDATE(existed_var_names.count(name) == 0, null_str);
						existed_var_names.insert(name);

						bool input = var_cfg["input"].to_bool();
						var_type_t type = var_type_from_str(var_cfg["type"].str());
						VALIDATE(is_aplt_task_var_type(type), null_str);
						bool optional = var_cfg["optional"].to_bool();
						if (input) {
							// optional: true, true, false, false, false, ....
							if (!has_optional_input_var) {
								if (optional) {
									has_optional_input_var = true;
								}
							} else {
								// if has optional=false, subsequent must be false.
								VALIDATE(optional, "ouput var's must not be optional");
							}

							// input, input, input, output, output, ...
							VALIDATE(!has_output_var, null_str);

						} else {
							VALIDATE(!optional, null_str);

							if (!has_output_var) {
								has_output_var = true;
							}
						}
						vars.push_back(tapplet::tvar(name, input, type, optional));
					}
					aplt.tasks.insert(std::make_pair(id, tapplet::ttask(id, type, subtype, nonpreemptive,
						recoverable, src, permissions, max_fails, vars, false, no_swap_wh_for_screen)));
				}

			} else {
				bundleid.clear();
				errcode = invalid_version;
			}
		} else {
			bundleid.clear();
			errcode = invalid_bundleid;
		}
	} else {
		errcode = invalid_bundleid;
	}
	if (!bundleid.empty()) {
		VALIDATE(errcode == nposm, null_str);
	} else {
		VALIDATE(errcode != nposm, null_str);
	}
	if (errcode != nposm) {
		if (alert_valid) {
			utils::string_map symbols;
			symbols["file"] = file_path;
			symbols["field"] = errcode == invalid_bundleid? "bundle_id": "version";
			gui2::show_message(null_str, vgettext2("No valid $field in $file", symbols));
		}
	}
	aplt.bundleid = bundleid;
}

config get_distribution_cfg(const std::string& aplt_path)
{
	const std::string file_path = aplt_path + "/" + APLT_DISTRIBUTION_CFG;

	config cfg;
	config_cache& cache = config_cache::instance();
	cache.get_config(file_path, cfg);
	cache.clear_defines();

	return cfg;
}

std::string aplt_id_2_bundleid(const std::string& bundleid)
{
	size_t npos = bundleid.find("(");
	if (npos != std::string::npos) {
		return bundleid.substr(0, npos);
	}
	return bundleid;
}

per_t conflicted_permission(const std::set<per_t>& pers1, const std::set<per_t>& pers2)
{
	for (std::set<per_t>::const_iterator it = pers1.begin(); it != pers1.end(); ++ it) {
		per_t per = *it;
		if (pers2.count(per) != 0) {
			return per;
		}
	}
	return per_nposm;
}

std::string aplt_can_run(const tapplet& aplt)
{
/*
	if (aplt.source == src_studio) {
		return null_str;
	}
*/
	utils::string_map symbols;
	symbols["app"] = game_config::get_app_msgstr(null_str);
	symbols["aplt"] = aplt.name2();
	if (game_config::rose_version < aplt.rose_version) {
		return vgettext2("The $app version is too low, can not run $aplt.", symbols);

	} else if (game_config::rose_version > aplt.rose_version) {
		if (aplt.rose_version < min_aplt_rose_ver) {
			return vgettext2("The $aplt version is too low, can not run.", symbols);
		}
	}
	return null_str;
}

int app_code_from_str(const std::string& app)
{
	for (std::map<int, std::string>::const_iterator it = apps.begin(); it != apps.end(); ++ it) {
		const std::string& that = it->second;
		if (that == app) {
			return it->first;
		}
	}
	return nposm;
}

int tdisable_new_klink_task_lock::reason = nposm;
std::map<int, std::string> tdisable_new_klink_task_lock::reasons;
bool tdisable_new_klink_task_lock::only_disable_fake_aplt_task = true;

tdisable_new_klink_task_lock::tdisable_new_klink_task_lock(int _reason, bool _only_disable_fake_aplt_task)
	: original_(reason)
	, original_only_disable_fake_aplt_task_(only_disable_fake_aplt_task)
{
	VALIDATE(_reason >= 0 && _reason < reason_count, null_str);
	reason = _reason;

	if (!only_disable_fake_aplt_task) {
		// before disable all aplt_task, now disable fake_aplt_task only,
		// It is forbidden to narrow the disable scope.
		VALIDATE(!_only_disable_fake_aplt_task, null_str);
	}
	only_disable_fake_aplt_task = _only_disable_fake_aplt_task;
}

tdisable_new_klink_task_lock::~tdisable_new_klink_task_lock()
{
	reason = original_;
	only_disable_fake_aplt_task = original_only_disable_fake_aplt_task_;
}

std::string tdisable_new_klink_task_lock::desc()
{
	VALIDATE(reason >= 0 && reason < reason_count, null_str);
	if (reasons.empty()) {
		reasons.insert(std::make_pair(reason_map, _("'Map' window is displayed")));
		reasons.insert(std::make_pair(reason_settings, _("'Settings' window is displayed")));
		reasons.insert(std::make_pair(reason_trigger, _("'Trigger' window is displayed")));
		reasons.insert(std::make_pair(reason_task, _("'Task' window is displayed")));
		reasons.insert(std::make_pair(reason_dnn, _("'DNN' window is displayed")));
		reasons.insert(std::make_pair(reason_moveit, _("'Moveit' window is displayed")));
		reasons.insert(std::make_pair(reason_explorer, _("'Explorer' window is displayed")));
		reasons.insert(std::make_pair(reason_camera, _("'Camera' window is displayed")));
		// reasons.insert(std::make_pair(reason_fg_aplt, _("A applet is running in the foreground")));
		reasons.insert(std::make_pair(reason_netxmit, _("Network task is being performed")));
		VALIDATE(reasons.size() == reason_count, null_str);
	}

	utils::string_map symbols;
	symbols["reason"] = reasons.find(reason)->second;
	return vgettext2("$reason, don't execute", symbols);
}

bool new_klink_task_disabled2(std::string* desc)
{
	if (tdisable_new_klink_task_lock::disabled()) {
		if (desc != nullptr) {
			*desc = tdisable_new_klink_task_lock::desc();
		}
		return true;

	} else if (gui2::tprogress_::top_instance != nullptr) {
		if (desc != nullptr) {
			*desc = "task with progress is running";
		}
		return true;
	}

	return false;
}

struct tbind_textdomain_lock
{
	tbind_textdomain_lock(const tapplet& _aplt)
		: aplt(_aplt)
	{
		instance->increment_textdomain_usage(aplt);
	}

	~tbind_textdomain_lock()
	{
		instance->decrement_textdomain_usage(aplt);
	}

	const tapplet& aplt;
};

void aplt_set_msgstr(tapplet& aplt)
{
	const std::string textdomain = bundleid_2_lua_bundleid(aplt.bundleid) + "-lib";
	tbind_textdomain_lock textdomain_lock(aplt);
	char buf[128];
/*
	for (std::map<std::string, std::string>::iterator it = aplt.objects.begin(); it != aplt.objects.end(); ++ it) {
		SDL_snprintf(buf, sizeof(buf), "position^%s", it->first.c_str());
		it->second = dsgettext(textdomain.c_str(), buf);
	}
*/
	std::string reserved_task_name;
	for (std::map<std::string, tapplet::ttask>::iterator it = aplt.tasks.begin(); it != aplt.tasks.end(); ++ it) {
		tapplet::ttask& cfg_task = it->second;

		if (reserved_task_id_2_codes.count(cfg_task.id) == 0) {
			reserved_task_name.clear();

		} else {
			int code = reserved_task_id_2_codes.find(cfg_task.id)->second;
			reserved_task_name = reserved_tasks.find(code)->second.name;
		}

		if (reserved_task_name.empty()) {
			SDL_snprintf(buf, sizeof(buf), "task^%s", cfg_task.id.c_str());
			cfg_task.name = dsgettext(textdomain.c_str(), buf);

		} else {
			cfg_task.name = reserved_task_name;
		}
	}
}

void aplt_set_pinyin(tapplet& aplt)
{
	int tone_ = 0;
	bool eng_lowercase_ = true;
	// aplt::get_curr_pinyin();

	for (std::map<std::string, tapplet::ttask>::iterator it = aplt.tasks.begin(); it != aplt.tasks.end(); ++ it) {
		tapplet::ttask& task = it->second;
		VALIDATE(!task.name.empty(), null_str);

		task.py_name = chinese::curr_pinyin.from_utf8str2(task.name, tone_, eng_lowercase_);
	}
}


void initial_fake_aplt()
{
	tapplet& aplt = fake_aplt;
	aplt.fake = true;

	const std::string bundleid = "aplt.launcher.fake";
	aplt.set_id(src_distribution, bundleid);
	const std::string subtitle;
	const std::string username;
	int64_t ts = 0;
	aplt.set(_("Built-in"), subtitle, username, ts, null_str, null_str, game_config::rose_version.str(true));
	// aplt_set_msgstr(aplt);

	fill_per_if_necessary(aplt.permissions);

	//
	// insert fixed task. these tasks cannot be erase all time.
	// 
	fake_fixed_tasks.insert(std::make_pair(fake_task_id_empty, _("Empty/Move")));
	int max_fails = 1;
	std::vector<tapplet::tvar> vars;

	std::set<per_t> permissions;
	fill_per_if_necessary(permissions);

	for (std::map<std::string, std::string>::const_iterator it = fake_fixed_tasks.begin(); it != fake_fixed_tasks.end(); ++ it) {
		const std::string& id = it->first;
		const std::string& name = it->second;
		std::pair<std::map<std::string, tapplet::ttask>::iterator, bool> ins = aplt.tasks.insert(std::make_pair(id, 
			tapplet::ttask(id, task_block, nposm, false, false, nposm, permissions, max_fails, vars, false, false)));
		tapplet::ttask& task = ins.first->second;
		task.name = name;
	}

	//
	// builtin vars
	//
	BI_env_vars.insert(std::make_pair(var_env_time,
		tcode3(var_env_time, utils::join_app_prefix_id(fake_aplt.bundleid, "env_time_"), _("desc^var_env_time"))));
	BI_env_vars.insert(std::make_pair(var_env_hour24_time, 
		tcode3(var_env_hour24_time, utils::join_app_prefix_id(fake_aplt.bundleid, "env_hour24_time_"), _("desc^var_env_hour24_time"))));
	BI_env_vars.insert(std::make_pair(var_env_temperature, 
		tcode3(var_env_temperature, utils::join_app_prefix_id(fake_aplt.bundleid, "env_temperature_"), _("desc^var_env_temperature"))));
	BI_env_vars.insert(std::make_pair(var_env_light, 
		tcode3(var_env_light, utils::join_app_prefix_id(fake_aplt.bundleid, "env_light_"), _("desc^var_env_light"))));
	BI_env_vars.insert(std::make_pair(var_env_humidity, 
		tcode3(var_env_humidity, utils::join_app_prefix_id(fake_aplt.bundleid, "env_humidity_"), _("desc^var_env_humidity"))));
	BI_env_vars.insert(std::make_pair(var_env_integer,
		tcode3(var_env_integer, utils::join_app_prefix_id(fake_aplt.bundleid, "env_integer_"), _("desc^var_env_integer"))));
	BI_env_vars.insert(std::make_pair(var_env_bool,
		tcode3(var_env_bool, utils::join_app_prefix_id(fake_aplt.bundleid, "env_bool_"), _("desc^var_env_bool"))));
	BI_env_vars.insert(std::make_pair(var_env_basesubtask_code,
		tcode3(var_env_basesubtask_code, utils::join_app_prefix_id(fake_aplt.bundleid, "env_basesubtask_code_"), _("desc^var_env_basesubtask_code"))));
	BI_env_vars.insert(std::make_pair(var_env_basesubtask_str,
		tcode3(var_env_basesubtask_str, utils::join_app_prefix_id(fake_aplt.bundleid, "env_basesubtask_str_"), _("desc^var_env_basesubtask_str"))));
	BI_env_vars.insert(std::make_pair(var_env_button_code,
		tcode3(var_env_button_code, utils::join_app_prefix_id(fake_aplt.bundleid, "env_button_code_"), _("desc^var_env_button_code"))));
	BI_env_vars.insert(std::make_pair(var_env_ocr_pen_code,
		tcode3(var_env_ocr_pen_code, utils::join_app_prefix_id(fake_aplt.bundleid, "env_ocr_pen_code_"), _("desc^var_env_ocr_pen_code"))));
	BI_env_vars.insert(std::make_pair(var_env_ocr_pen_text,
		tcode3(var_env_ocr_pen_text, utils::join_app_prefix_id(fake_aplt.bundleid, "env_ocr_pen_text_"), _("desc^var_env_ocr_pen_text"))));

	BI_env_vars.insert(std::make_pair(var_env_time_last,
		tcode3(var_env_time_last, utils::join_app_prefix_id(fake_aplt.bundleid, "env_time_last_"), _("desc^var_env_time_last"))));
	BI_env_vars.insert(std::make_pair(var_env_hour24_time1_last, 
		tcode3(var_env_hour24_time1_last, utils::join_app_prefix_id(fake_aplt.bundleid, "env_hour24_time1_last_"), _("desc^var_env_hour24_time1_last"))));
	BI_env_vars.insert(std::make_pair(var_env_temperature_last, 
		tcode3(var_env_temperature_last, utils::join_app_prefix_id(fake_aplt.bundleid, "env_temperature_last_"), _("desc^var_env_temperature_last"))));
	BI_env_vars.insert(std::make_pair(var_env_light_last, 
		tcode3(var_env_light_last, utils::join_app_prefix_id(fake_aplt.bundleid, "env_light_last_"), _("desc^var_env_light_last"))));
	BI_env_vars.insert(std::make_pair(var_env_humidity_last, 
		tcode3(var_env_humidity_last, utils::join_app_prefix_id(fake_aplt.bundleid, "env_humidity_last_"), _("desc^var_env_humidity_last"))));
	BI_env_vars.insert(std::make_pair(var_env_integer_last, 
		tcode3(var_env_integer_last, utils::join_app_prefix_id(fake_aplt.bundleid, "env_integer_last_"), _("desc^var_env_integer_last"))));
	BI_env_vars.insert(std::make_pair(var_env_bool_last, 
		tcode3(var_env_bool_last, utils::join_app_prefix_id(fake_aplt.bundleid, "env_bool_last_"), _("desc^var_env_bool_last"))));
	BI_env_vars.insert(std::make_pair(var_env_hour24_time2_last, 
		tcode3(var_env_hour24_time2_last, utils::join_app_prefix_id(fake_aplt.bundleid, "env_hour24_time2_last_"), _("desc^var_env_hour24_time2_last"))));
	BI_env_vars.insert(std::make_pair(var_env_hour24_time3_last, 
		tcode3(var_env_hour24_time3_last, utils::join_app_prefix_id(fake_aplt.bundleid, "env_hour24_time3_last_"), _("desc^var_env_hour24_time3_last"))));

	BI_task_vars.insert(std::make_pair(var_last_matched_index, 
		tcode3(var_last_matched_index, utils::join_app_prefix_id(fake_aplt.bundleid, "last_matched_index_"), _("desc^var_last_matched_index"))));
	BI_task_vars.insert(std::make_pair(var_iot_device_id, 
		tcode3(var_iot_device_id, utils::join_app_prefix_id(fake_aplt.bundleid, "iot_device_id_"), _("desc^var_iot_device_id"))));
	BI_task_vars.insert(std::make_pair(var_iot_alias,
		tcode3(var_iot_alias, utils::join_app_prefix_id(fake_aplt.bundleid, "iot_alias_"), _("desc^var_iot_alias"))));
	VALIDATE(BI_env_vars.size() + BI_task_vars.size() == BI_var_count, null_str);

	BI_ble_vars.insert(std::make_pair(ble_var_device_id, 
		tcode3(ble_var_device_id, "device_id", _("desc^ble_device_id"))));
	VALIDATE(BI_ble_vars.size() == BI_ble_var_count, null_str);

	std::vector<const std::map<int, tcode3>* > code3_maps = {&BI_env_vars, &BI_task_vars, &BI_ble_vars};
	int at = 0;
	for (std::vector<const std::map<int, tcode3>* >::const_iterator it = code3_maps.begin(); it != code3_maps.end(); ++ it, at ++) {
		const std::map<int, tcode3>& m = **it;
		for (std::map<int, tcode3>::const_iterator it2= m.begin(); it2 != m.end(); ++ it2) {
			const tcode3& code3 = it2->second;
			VALIDATE(it2->first == code3.code, null_str);
			if (at < 1) {
				BI_env_var_id_2_types.insert(std::make_pair(code3.id, code3.code));
			}
			if (at < 2) {
				BI_var_id_2_types.insert(std::make_pair(code3.id, code3.code));
			}
		}
	}
	VALIDATE(BI_env_var_id_2_types.size() == BI_env_vars.size(), null_str);
	VALIDATE(BI_var_id_2_types.size() == BI_var_count, null_str);

	// don't want export @env_vars.
	init_env_vars();

	amp_modes.insert(std::make_pair(ampmode_1x, 
		tcode3(ampmode_1x, "1x", _("amp^1x"))));
	amp_modes.insert(std::make_pair(ampmode_1_25x,
		tcode3(ampmode_1_25x, "1_25x", _("amp^1_25x"))));
	amp_modes.insert(std::make_pair(ampmode_1_6x,
		tcode3(ampmode_1_6x, "1_6x", _("amp^1_6x"))));
	amp_modes.insert(std::make_pair(ampmode_2x,
		tcode3(ampmode_2x, "2x", _("amp^2x"))));
	VALIDATE(amp_modes.size() == ampmode_count, null_str);
}

void load_applets_from_disk(std::map<taplt_key, tapplet>& applets)
{
	VALIDATE(!sources.empty(), null_str);
	VALIDATE(applets.empty(), null_str);
	
	initial_fake_aplt();

	std::set<std::string> aplts;
	std::set<std::string> partial = aplts_in_preferences_dir();
	for (std::set<std::string>::const_iterator it = partial.begin(); it != partial.end(); ++ it) {
		const std::string& lua_bundleid = *it;
		const std::string aplt_path = game_config::preferences_dir + "/" + lua_bundleid;
		aplts.insert(aplt_path);
	}
	partial = aplts_in_res();
	for (std::set<std::string>::const_iterator it = partial.begin(); it != partial.end(); ++ it) {
		const std::string& lua_bundleid = *it;
		const std::string aplt_path = game_config::path + "/" + lua_bundleid;
		aplts.insert(aplt_path);
	}

	tapplet tmp_aplt;
	for (std::set<std::string>::const_iterator it = aplts.begin(); it != aplts.end(); ++ it) {
		const std::string& aplt_path = *it;
		const std::string lua_bundleid = utils::extract_file(aplt_path);
		bool in_preferences = aplt_path.find(game_config::path) != 0;

		tapplet& aplt = tmp_aplt;
		aplt.clear();

		get_settings_cfg(aplt_path, false, aplt);
		if (utils::replace_all(aplt.bundleid, ".", "_") == lua_bundleid) {
			std::string name, subtitle, username, rose_version;
			int64_t ts = 0;
			int type = src_studio;
			if (in_preferences) {
				config dist = get_distribution_cfg(aplt_path);
				name = dist["name"].str();
				subtitle = dist["subtitle"].str();
				username = dist["username"].str();
				type = dist["distribution"].to_bool()? src_distribution: src_development;
				ts = dist["ts"].to_int64();
				rose_version = dist["rose_version"].str();
			} else {
				name = aplt.bundleid;
				rose_version = game_config::rose_version.str(true);
			}
			aplt.set_id(type, aplt.bundleid);
			aplt.set(name, subtitle, username, ts, aplt_path, aplt_path + "/" + APPLET_ICON, rose_version);
			aplt_set_msgstr(aplt);

			std::pair<std::map<taplt_key, tapplet>::iterator, bool> ins = applets.insert(std::make_pair(taplt_key(aplt.source, aplt.bundleid), aplt));
			ins.first->second.fill_input_output_vars();
		}
	}
}

texecutor::texecutor(rose_lua_kernel& lua, const tapplet& aplt)
	: lua_(lua)
	, aplt_(aplt)
	, lua_bundleid_(utils::replace_all(aplt.bundleid, ".", "_"))
	, L(lua.get_state())
{
}

struct tcurrent_aplt_lock
{
	tcurrent_aplt_lock(const tapplet& aplt)
	{
		VALIDATE(instance->fg_aplt() == nullptr, null_str);
		instance->set_fg_aplt(&aplt);
	}

	~tcurrent_aplt_lock()
	{
		VALIDATE(instance->fg_aplt() != nullptr, null_str);
		instance->set_fg_aplt(nullptr);
	}
};

void setup_aplt_user_data_dir(const std::string& aplt_preferences_dir)
{
	const std::string& dir_path = aplt_preferences_dir;

	const bool res = create_directory_if_missing(dir_path);
	// probe read permissions (if we could make the directory)
	if (!res || !is_directory(dir_path)) {
		// could not open or create preferences directory at $dir_path;
		return;
	}

	// Create user data and add-on directories
	create_directory_if_missing(dir_path + "/cert");
	// create_directory_if_missing(dir_path + "/data");
	// create_directory_if_missing(dir_path + "/images");
	// create_directory_if_missing(dir_path + "/images/misc");
	create_directory_if_missing(dir_path + "/saves");
	create_directory_if_missing(dir_path + "/tflites");
	create_directory_if_missing(dir_path + "/wkocourse");
	create_directory_if_missing(dir_path + "/wkoscript");
}

void texecutor::run()
{
	config aplt_cfg;
	const std::string aplt_path = aplt_.res_path;
	const std::string textdomain = lua_bundleid_ + "-lib";
	
	const std::string ver_err = aplt_can_run(aplt_);
	if (!ver_err.empty()) {
		gui2::show_message(null_str, ver_err);
		return;
	}

	if (instance->bg_task().is_ing()) {
		tbg_task& bg_task = instance->bg_task();
		bool require_stop = false;
		std::string log;
		/*if (bg_aplt == &aplt_) {
			log = _("To run the applet in which the task resides, stop the task");
			require_stop = true;

		} else */ {
			// bg_aplt are always base_driver. This permission judgment require be improved later. 
			// But how to put it, it is not right to judge permission conflicts based on sys_task->aplt_task->id. 
			// Because in the process of performing a task_cpp, it is not known what permissions are required.
			const std::set<per_t>* bg_task_perssions = nullptr;

			// if (bg_task.running().sys_task->aplt_task != nullptr) {
			{
				const aplt::tbg_task::tbase_bg_task2& sys_task = bg_task.bg_task2();
				bg_task_perssions = &sys_task.permissions;
			}

			const per_t per = conflicted_permission(*bg_task_perssions, aplt_.permissions);
			if (per != per_nposm) {
				utils::string_map symbols;
				symbols["per"] = sys_permissions.find(per)->second.name;
				log = vgettext2("The applet to run requires $per permission, stop the task", symbols);
				require_stop = true;
			}

			if (!require_stop) {
				// now stop bg task always. modify in future.
				require_stop = true;
			}
		}
		if (require_stop) {
			// VALIDATE(!log.empty(), null_str);
			std::string warnning = _("A bg task is running and it needs to be stopped first. Do you want to continue?");
			if (!instance->stop_bg_task_if_runing(warnning, log)) {
				return;
			}
		}
	}

	// tdisable_new_klink_task_lock disable_new_aplt_lock(tdisable_new_klink_task_lock::reason_fg_aplt);
	tcurrent_aplt_lock aplt_lock(aplt_);
	tbind_textdomain_lock textdomain_lock(aplt_);

	if (!instance->app_handle_aplt_msg(&aplt_, APLT_MSG_WILLRUN)) {
/*
		utils::string_map symbols;
		symbols["libroseaplt"] = LIBROSEAPLT_SO;
		std::string msg_err = vgettext2("The applet exists $libroseaplt, but it failed to open", symbols);
		gui2::show_message(null_str, msg_err);
*/
		return;
	}

	wml_config_from_file(aplt_path + "/xwml/" + BASENAME_APLT, aplt_cfg);
	
	gui2::insert_window_builder_from_cfg(aplt_cfg);

	config& sub_cfg = aplt_cfg.add_child("binary_path");
	sub_cfg["path"] = binary_paths_manager::full_path_indicator + aplt_path;
	const binary_paths_manager bin_paths_manager(aplt_cfg);

	//
	setup_aplt_user_data_dir(get_aplt_user_data_dir(lua_bundleid_));
	// create_directory_if_missing(get_aplt_user_data_dir(lua_bundleid_));

	lua_.register_applet(aplt_, lua_bundleid_, aplt_.version, aplt_cfg);

	// instance->app_handle_aplt_msg(true, &aplt_, APLT_MSG_WILLRUN);

	std::string next_dlg;
	std::map<int, std::string> windows;
	{
		// tstack_size_lock lock(L, 0);

		luaW_getglobal(L, lua_bundleid_.c_str(), "start");
		lua_.protected_call(0, 2);

		const int MIN_APP_RETVAL = 1;
		vconfig vcfg = luaW_checkvconfig(L, -2);
		for (const config::attribute& val: vcfg.get_config().attribute_range()) {
			const std::string id = val.first;
			int n = val.second.to_int();
			VALIDATE(n >= MIN_APP_RETVAL && windows.count(n) == 0, null_str);
			windows.insert(std::make_pair(n, id));
		}
		int launcher = lua_tointeger(L, -1);
		lua_pop(L, 2);

		VALIDATE(windows.count(launcher) != 0, null_str);
		next_dlg = windows.find(launcher)->second;

	}

	bool cvideo_quit = false;
	try {
		while (!next_dlg.empty()) {
			gui2::trldialog dlg(lua_, lua_bundleid_, next_dlg);
			dlg.show();
			lua_.post_show(lua_bundleid_, next_dlg);
			instance->app_handle_aplt_msg(&aplt_, APLT_MSG_DIDDLGCLOSE);
			instance->app_fg_aplt_send_cpp_id(aplt_, dlg.joined_id(), cpp_id_sys_dlg_closed);

			int retval = dlg.get_retval();
			if (windows.count(retval) != 0) {
				next_dlg = windows.find(retval)->second;
			} else {
				next_dlg.clear();
			}
		}
	} catch (CVideo::quit&) {
		SDL_Log("when aplt::texecutor::run, catched CVideo::quit");
		cvideo_quit = true;
	}

	lua_.unregister_applet(lua_bundleid_);
	gui2::remove_window_builder_baseon_app(lua_bundleid_);

	// Enforce a complete garbage collection
	lua_gc(L, LUA_GCCOLLECT);

	// lua_.call_lua_breakpoint();

	// must execute APLT_MSG_DIDTERMINATE after lua_gc()
	// --during APLT_MSG_DIDTERMINATE, aplt_unload() of libapltmain.so will be called. it will destroy C++ class.
	instance->app_handle_aplt_msg(&aplt_, APLT_MSG_DIDTERMINATE);

	if (cvideo_quit) {
		throw CVideo::quit();
	}
}

// #define ONE_DAY_SECONDS		86400 // 24 * 3600

int tbg_task::tbase_bg_task2::in_which_single_task(const std::map<taplt_key, tapplet>& applets) const
{
	if (aplt_task != nullptr) {
		const aplt::tapplet* aplt = aplt::aplt_from_id_ex(applets, aplt_task->aplt_id);
		VALIDATE(aplt != nullptr && aplt->tasks.count(aplt_task->task_id) != 0, null_str);
		const aplt::tapplet::ttask& task = aplt->tasks.find(aplt_task->task_id)->second;
		return task.type;
	}
	return nposm;
}

void tbg_task::tbase_bg_task2::set_task_var(const std::string& name, bool is_array, const config::attribute_value& val)
{
	VALIDATE_IN_MAIN_THREAD();

	if (aplt::BI_env_var_id_2_types.count(name)) {
		VALIDATE(!is_array, null_str);
		int type = aplt::BI_env_var_id_2_types.find(name)->second;
		aplt::set_env_var(type, val);

		if (bg_task_.is_ing()) {
			// set task_vars_
			if (task_vars_.existed(name)) {
				task_vars_.insert_attribute(name, false, val, type);
			}
		}
		return;
	}

	VALIDATE(bg_task_.is_ing(), null_str);
	task_vars_.insert_attribute(name, is_array, val);
}

void tbg_task::tbase_bg_task2::erase_task_var(const std::string& name)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(aplt::BI_var_id_2_types.count(name) == 0, null_str);

	VALIDATE(bg_task_.is_ing(), null_str);
	task_vars_.erase(name);
}

tbg_task::tbg_task(base_instance& _instance, std::map<taplt_key, tapplet>& applets)
	: instance_(_instance)
	, applets_(applets)
	, bg_task2_(nullptr)
	, lua_(nullptr)
	, L(nullptr)
	, is_ing_(false)
	, stopping_(false)
	, last_shedule_zerotz_t_(nposm)
	, disable_stop_timing_(false)
	, overdue_threshold_(5 * 60) // 5 min
	, pb_klink_type_(nposm)
	, pb_klink_backup_type_(backup_on_idle)
{
}

tbg_task::~tbg_task()
{
	VALIDATE(!is_ing_, null_str);
}

void tbg_task::set_bg_task2(tbase_bg_task2& bg_task2)
{
	VALIDATE(bg_task2_ == nullptr, null_str);
	bg_task2_ = &bg_task2;
}

void tbg_task::set_lua(rose_lua_kernel& lua)
{
	// when ttiming::ttiming, textdomain maybe not ready.
	state_descs_.insert(std::make_pair(aplt::taplt_task::state_fresh, _("timing^state_fresh")));
	state_descs_.insert(std::make_pair(aplt::taplt_task::state_running, _("timing^state_running")));
	state_descs_.insert(std::make_pair(aplt::taplt_task::state_finished_ok, _("timing^state_finished_ok")));
	state_descs_.insert(std::make_pair(aplt::taplt_task::state_finished_fail, _("timing^state_finished_fail")));
	state_descs_.insert(std::make_pair(aplt::taplt_task::state_finished_expired, _("timing^state_finished_expired")));
	state_descs_.insert(std::make_pair(aplt::taplt_task::state_apltnotfound, _("timing^state_apltnotfound")));
	state_descs_.insert(std::make_pair(aplt::taplt_task::state_verdismatch, _("timing^state_verdismatch")));
	VALIDATE(state_descs_.size() == aplt::taplt_task::state_count, null_str);

	VALIDATE(lua_ == nullptr, null_str);
	lua_ = &lua;
	L = lua.get_state();
}

const std::string& tbg_task::task_state_desc(int state) const
{
	VALIDATE(state >= 0 && state < aplt::taplt_task::state_count, null_str);
	return state_descs_.find(state)->second;
}

void tbg_task::set_scene_name(const std::string& _name, bool write_pb)
{
	VALIDATE(!_name.empty(), null_str);
	const std::string name = utils::truncate_to_max_bytes(_name.c_str(), _name.size(), MAX_KLINK_SCENE_NAME_BYTES);
	if (name == scene_name_) {
		return;
	}
	scene_name_ = name;
	pb_klink_.set_scene_name(name);

	if (write_pb) {
		protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);
	}
}

bool tbg_task::in_4tasks(const taplt_task& aplt_task) const
{
	const std::map<taplt_task_key, taplt_task>& tasks = get_tasks(aplt_task.type);
	return aplt_task.pb_at >= 0 && aplt_task.pb_at < (int)tasks.size();
}

bool tbg_task::in_4tasks_use_ptr(const taplt_task& aplt_task) const
{
	const std::map<taplt_task_key, taplt_task>& tasks = get_tasks(aplt_task.type);
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const taplt_task& that = it->second;
		if (&that == &aplt_task) {
			return true;
		}
	}
	return false;
}

void tbg_task::add_pb_task(const aplt::taplt_task& task)
{
	pb2::ttask* new_task = nullptr;
	if (task.type == aplt::taplt_task::type_iot) {
		VALIDATE(IS_VALID_APLT_TASK_NONTIMED_PRIORITY(task.priority), null_str);
		new_task = pb_klink_.add_iot_tasks();

	} else if (task.type == aplt::taplt_task::type_speech) {
		VALIDATE(IS_VALID_APLT_TASK_NONTIMED_PRIORITY(task.priority), null_str);
		new_task = pb_klink_.add_speech_tasks();

	} else if (task.type == aplt::taplt_task::type_var) {
		VALIDATE(IS_VALID_APLT_TASK_TIMED_PRIORITY(task.priority), null_str);
		new_task = pb_klink_.add_var_tasks();

	} else {
		VALIDATE(task.type == aplt::taplt_task::type_timed, null_str);
		VALIDATE(IS_VALID_APLT_TASK_TIMED_PRIORITY(task.priority), null_str);
		new_task = pb_klink_.add_timed_tasks();

	} 
	new_task->set_type(task.type);
	new_task->set_priority(task.priority);
	new_task->set_aplt_id(task.aplt_id);
	new_task->set_task_id(task.task_id);
	new_task->set_ble_device_id(task.ble_device_id);
	new_task->set_position1(task.position1);
	new_task->set_position2(task.position2);
	new_task->set_state(task.state);

	if (task.type == aplt::taplt_task::type_iot) {
		VALIDATE(IS_VALID_APLT_TASK_IOT_SRC(task.iot_src), null_str);
		VALIDATE(IS_VALID_APLT_TASK_IOT_SRC_EVT(task.src_evt), null_str);
		VALIDATE(!task.src_device_id.empty(), null_str);
		VALIDATE(task.speech_id.empty(), null_str);
		VALIDATE(task.var_at == nposm, null_str);
		VALIDATE(task.zerotz_t == nposm, null_str);
		VALIDATE(task.timed_at == nposm, null_str);

	} else {		
		VALIDATE(task.iot_src == nposm, null_str);
		VALIDATE(task.src_evt == nposm, null_str);
		VALIDATE(task.src_device_id.empty(), null_str);
		if (task.type == aplt::taplt_task::type_speech) {
			VALIDATE(!task.speech_id.empty(), null_str);
			VALIDATE(task.var_at == nposm, null_str);
			VALIDATE(task.zerotz_t == nposm, null_str);
			VALIDATE(task.timed_at == nposm, null_str);

		} else if (task.type == taplt_task::type_var) {
			VALIDATE(task.speech_id.empty(), null_str);
			VALIDATE(IS_VALID_APLT_TASK_VAR_AT(task.var_at), null_str);
			VALIDATE(task.zerotz_t == nposm, null_str);
			VALIDATE(task.timed_at == nposm, null_str);

		} else {
			VALIDATE(task.type == aplt::taplt_task::type_timed, null_str);

			VALIDATE(task.speech_id.empty(), null_str);
			VALIDATE(task.var_at == nposm, null_str);
			VALIDATE(IS_VALID_APLT_TASK_TIMED_ZEROTZ_T(task.zerotz_t), null_str);
			VALIDATE(IS_VALID_APLT_TASK_TIMED_AT(task.timed_at), null_str);

		} 
	}

	new_task->set_iot_src(task.iot_src);
	new_task->set_src_evt(task.src_evt);
	new_task->set_src_device_id(task.src_device_id);
	new_task->set_speech_id(task.speech_id);
	new_task->set_var_at(task.var_at);
	new_task->set_zerotz_t(task.zerotz_t);
	new_task->set_timed_at(task.timed_at);
}

const taplt_task* tbg_task::insert_task(int type, int priority, const std::string& aplt_id, const std::string& task_id, 
	const std::string& ble_device_id, int state, int src, int src_evt, const std::string& src_device_id, const std::string& speech_id, int zerotz_t)
{
	VALIDATE(state >= 0 && state < aplt::taplt_task::state_count, null_str);

	std::pair<std::map<taplt_task_key, taplt_task>::iterator, bool> ins;
	if (type == taplt_task::type_iot) {
		VALIDATE(IS_VALID_APLT_TASK_NONTIMED_PRIORITY(priority), null_str);
		VALIDATE(IS_VALID_APLT_TASK_IOT_SRC(src), null_str);
		VALIDATE(IS_VALID_APLT_TASK_IOT_SRC_EVT(src_evt), null_str);
		VALIDATE(!src_device_id.empty(), null_str);
		VALIDATE(speech_id.empty(), null_str);
		VALIDATE(zerotz_t == nposm, null_str);
		ins = iot_tasks_.insert(std::make_pair(taplt_task_key(src, src_evt, src_device_id),
			taplt_task(priority, aplt_id, task_id, ble_device_id, null_str, null_str, state, pb_klink_.iot_tasks_size(), src, src_evt, src_device_id)));

	} else if (type == taplt_task::type_speech) {
		VALIDATE(IS_VALID_APLT_TASK_NONTIMED_PRIORITY(priority), null_str);
		VALIDATE(src == nposm, null_str);
		VALIDATE(src_evt == nposm, null_str);
		VALIDATE(src_device_id.empty(), null_str);
		VALIDATE(!speech_id.empty(), null_str);
		VALIDATE(zerotz_t == nposm, null_str);
		ins = speech_tasks_.insert(std::make_pair(taplt_task_key(speech_id),
			taplt_task(priority, aplt_id, task_id, ble_device_id, null_str, null_str, state, pb_klink_.speech_tasks_size(), speech_id)));

	} else if (type == taplt_task::type_var) {
		VALIDATE(IS_VALID_APLT_TASK_TIMED_PRIORITY(priority), null_str);
		VALIDATE(ble_device_id.empty(), null_str);

		VALIDATE(src == nposm, null_str);
		VALIDATE(src_evt == nposm, null_str);
		VALIDATE(src_device_id.empty(), null_str);
		VALIDATE(speech_id.empty(), null_str);
		VALIDATE(zerotz_t == nposm, null_str);
		// VALIDATE(IS_VALID_APLT_TASK_VAR_AT(var_at), null_str);
		const int var_at = var_tasks_.size();
		const int aux_key_id = next_aux_key_id(var_tasks_);
		ins = var_tasks_.insert(std::make_pair(taplt_task_key(aux_key_id),
			taplt_task(aplt_id, task_id, null_str, null_str, state, pb_klink_.var_tasks_size(), var_at, aux_key_id)));

	} else {
		VALIDATE(type == taplt_task::type_timed, null_str);

		VALIDATE(IS_VALID_APLT_TASK_TIMED_PRIORITY(priority), null_str);
		VALIDATE(!aplt_id.empty(), null_str);
		VALIDATE(!task_id.empty(), null_str);

		VALIDATE(src == nposm, null_str);
		VALIDATE(src_evt == nposm, null_str);
		VALIDATE(speech_id.empty(), null_str);
		VALIDATE(IS_VALID_APLT_TASK_TIMED_ZEROTZ_T(zerotz_t), null_str);
		// VALIDATE(IS_VALID_APLT_TASK_TIMED_AT(timed_at), null_str);

		for (std::map<taplt_task_key, taplt_task>::const_iterator it = timed_tasks_.begin(); it != timed_tasks_.end(); ++ it) {
			const taplt_task& task = it->second;
			if (task.zerotz_t == zerotz_t) {
				SDL_Log("%u insert_task(%s__%s zerotz_t: %i) fail, duplicated zerotz_t", SDL_GetTicks(), aplt_id.c_str(), task_id.c_str(), zerotz_t);
				return nullptr;
			}
		}

		const int timed_at = timed_tasks_.size();
		const int aux_key_id = next_aux_key_id(timed_tasks_);
		ins = timed_tasks_.insert(std::make_pair(taplt_task_key(aplt_id, task_id, ble_device_id, aux_key_id),
			taplt_task(aplt_id, task_id, ble_device_id, null_str, null_str, state, pb_klink_.timed_tasks_size(), zerotz_t, timed_at, aux_key_id)));

	}

	if (!ins.second) {
		SDL_Log("%u insert_task(%s__%s) fail, duplicated taplt_task_key", SDL_GetTicks(), aplt_id.c_str(), task_id.c_str());
		return nullptr;
	}

	add_pb_task(ins.first->second);
	protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);

	return &(ins.first->second);
}

void tbg_task::insert_iot_task(int priority, int state, int src, int src_evt, const std::string& src_device_id)
{
	insert_task(taplt_task::type_iot, priority, null_str, null_str, null_str, state, src, src_evt, src_device_id, null_str, nposm);
}

void tbg_task::insert_speech_task(int priority, int state, const std::string& speech_id)
{
	insert_task(taplt_task::type_speech, priority, null_str, null_str, null_str, state, nposm, nposm, null_str, speech_id, nposm);
}

const taplt_task* tbg_task::insert_var_task(int state)
{
	return insert_task(taplt_task::type_var, priority_timed, null_str, null_str, null_str, state, nposm, nposm, null_str, null_str, nposm);
}

const taplt_task* tbg_task::insert_timed_task(const std::string& aplt_id, const std::string& task_id, const std::string& pin, int state, int zerotz_t)
{
	return insert_task(taplt_task::type_timed, priority_timed, aplt_id, task_id, pin, state, nposm, nposm, null_str, null_str, zerotz_t);
}


void tbg_task::erase_task2(int tasks_type, const aplt::taplt_task& task)
{
	std::map<taplt_task_key, taplt_task>& tasks = get_mutable_tasks(tasks_type);

	const taplt_task_key key = aplt_task_key_from_aplt_task(task);
	std::map<taplt_task_key, taplt_task>::iterator it = tasks.find(key);
	VALIDATE(it != tasks.end(), null_str);
	int pb_at = it->second.pb_at;
	tasks.erase(it);

	if (tasks_type == taplt_task::type_iot) {
		pb_klink_.mutable_iot_tasks()->DeleteSubrange(pb_at, 1);

	} else if (tasks_type == taplt_task::type_speech) {
		pb_klink_.mutable_speech_tasks()->DeleteSubrange(pb_at, 1);

	} else if (tasks_type == taplt_task::type_var) {
		pb_klink_.mutable_var_tasks()->DeleteSubrange(pb_at, 1);

	} else {
		VALIDATE(tasks_type == taplt_task::type_timed, null_str);
		pb_klink_.mutable_timed_tasks()->DeleteSubrange(pb_at, 1);
	} 
	protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);

	// if task.pb_at > pb_at, decrement pb_at.
	std::vector<taplt_task> post_ins_var_task;
	for (std::map<taplt_task_key, taplt_task>::iterator it = tasks.begin(); it != tasks.end(); ) {
		aplt::taplt_task& task = it->second;
		// pb_at does not participate in sort, modify it here, it should be safe
		if (task.pb_at > pb_at) {
			task.pb_at --;
		}
		bool field_changed = false;
		if (tasks_type == taplt_task::type_var) {
			if (task.var_at > pb_at) {
				task.var_at --;

				// field_changed = true;
			}

		} else if (tasks_type == taplt_task::type_timed) {
			if (task.timed_at > pb_at) {
				task.timed_at --;

				// field_changed = true;
			}
		}
		if (field_changed) {
			post_ins_var_task.push_back(task);
			tasks.erase(it ++);
			continue;
		}
		++ it;
	}

	std::pair<std::map<taplt_task_key, taplt_task>::iterator, bool> ins;
	for (std::vector<taplt_task>::const_iterator it = post_ins_var_task.begin(); it != post_ins_var_task.end(); ++ it) {
		const taplt_task& task = *it;
		VALIDATE(false, null_str);
		if (task.type == taplt_task::type_var) {
			ins = tasks.insert(std::make_pair(taplt_task_key(task.aux_key_id), task));

		} else if (task.type == taplt_task::type_timed) {
			ins = tasks.insert(std::make_pair(taplt_task_key(task.aplt_id, task.task_id, task.ble_device_id, task.timed_at), task));

		} else {
			VALIDATE(false, null_str);
		}
		VALIDATE(ins.second, null_str);
	}

	// if (game_config::os == os_windows) {
	if (true) {
		verify_4tasks();
	}
}

void tbg_task::modify_task(const taplt_task& task, const taplt_task& val_task, uint32_t flags, bool write_pb)
{
	std::map<taplt_task_key, taplt_task>& tasks = get_mutable_tasks(task.type);

	VALIDATE(in_4tasks_use_ptr(task), null_str);

	aplt::taplt_task& mutable_task = *const_cast<taplt_task*>(&task);

	bool dirty = false;
	if ((flags & taplt_task::FLAG_PRIORITY) && val_task.priority != task.priority) {
		if (task.type == taplt_task::type_iot || task.type == taplt_task::type_speech) {
			VALIDATE(IS_VALID_APLT_TASK_NONTIMED_PRIORITY(val_task.priority), null_str);
		} else {
			VALIDATE(false, null_str);
			VALIDATE(val_task.priority == nposm, null_str);
		}
		dirty = true;
	}
	if ((flags & taplt_task::FLAG_ZEROTZ_T) && val_task.zerotz_t != task.zerotz_t) {
		if (task.type == taplt_task::type_iot || task.type == taplt_task::type_speech || task.type == taplt_task::type_var) {
			VALIDATE(false, null_str);
			VALIDATE(val_task.zerotz_t == nposm, null_str);
		} else {
			VALIDATE(IS_VALID_APLT_TASK_TIMED_ZEROTZ_T(val_task.zerotz_t), null_str);
		}
		dirty = true;
	}
	if ((flags & taplt_task::FLAG_STATE) && val_task.state != task.state) {
		VALIDATE(IS_VALID_APLT_TASK_STATE(val_task.state), null_str);
		dirty = true;
	}
	if ((flags & taplt_task::FLAG_POSITION1) && val_task.position1 != task.position1) {
		VALIDATE(val_task.position1.empty() || utils::is_uuid(val_task.position1, true), null_str);
		dirty = true;
	}
	if ((flags & taplt_task::FLAG_POSITION2) && val_task.position2 != task.position2) {
		VALIDATE(val_task.position2.empty() || utils::is_uuid(val_task.position2, true), null_str);
		dirty = true;
	}

	if ((flags & taplt_task::FLAG_APLT_ID) && val_task.aplt_id != task.aplt_id) {
		if (task.type == taplt_task::type_iot || task.type == taplt_task::type_speech || task.type == taplt_task::type_var) {
			VALIDATE(val_task.aplt_id.empty() || aplt::aplt_from_id_ex(applets_, val_task.aplt_id) != nullptr, null_str);
		} else {
			// for timing_task, must not modify aplt_id.
			VALIDATE(false, null_str);
		}
		dirty = true;
	}
	if ((flags & taplt_task::FLAG_TASK_ID) && val_task.task_id != task.task_id) {
		dirty = true;
	}
	if ((flags & taplt_task::FLAG_BLE_DEVICE_ID) && val_task.ble_device_id != task.ble_device_id) {
		dirty = true;
	}

	if (dirty) {
		VALIDATE(mutable_task.pb_at == task.pb_at, null_str);

		if ((flags & taplt_task::FLAG_PRIORITY) && val_task.priority != task.priority) {
			mutable_task.priority = val_task.priority;
		}
		if ((flags & taplt_task::FLAG_ZEROTZ_T) && val_task.zerotz_t != task.zerotz_t) {
			mutable_task.zerotz_t = val_task.zerotz_t;
		}
		if ((flags & taplt_task::FLAG_STATE) && val_task.state != task.state) {
			mutable_task.state = val_task.state;
		}
		if ((flags & taplt_task::FLAG_POSITION1) && val_task.position1 != task.position1) {
			mutable_task.position1 = val_task.position1;
		}
		if ((flags & taplt_task::FLAG_POSITION2) && val_task.position2 != task.position2) {
			mutable_task.position2 = val_task.position2;
		}

		if ((flags & taplt_task::FLAG_APLT_ID)  && val_task.aplt_id != task.aplt_id) {
			mutable_task.aplt_id = val_task.aplt_id;
		}
		if ((flags & taplt_task::FLAG_TASK_ID) && val_task.task_id != task.task_id) {
			mutable_task.task_id = val_task.task_id;
		}
		if ((flags & taplt_task::FLAG_BLE_DEVICE_ID) && val_task.ble_device_id != task.ble_device_id) {
			mutable_task.ble_device_id = val_task.ble_device_id;
		}

		// 1/2: modify tasks_
		VALIDATE(task.pb_at == mutable_task.pb_at, null_str);
		pb2::ttask* pb_task = nullptr;
		if (mutable_task.type == taplt_task::type_iot) {
			pb_task = pb_klink_.mutable_iot_tasks(mutable_task.pb_at);

		} else if (mutable_task.type == taplt_task::type_speech) {
			pb_task = pb_klink_.mutable_speech_tasks(mutable_task.pb_at);

		} else if (mutable_task.type == taplt_task::type_var) {
			pb_task = pb_klink_.mutable_var_tasks(mutable_task.pb_at);

		} else {
			VALIDATE(mutable_task.type == taplt_task::type_timed, null_str);
			pb_task = pb_klink_.mutable_timed_tasks(mutable_task.pb_at);

		} 

		// 2/2: modify pb
		if ((flags & taplt_task::FLAG_PRIORITY)) {
			pb_task->set_priority(val_task.priority);
		}
		if ((flags & taplt_task::FLAG_ZEROTZ_T)) {
			pb_task->set_zerotz_t(val_task.zerotz_t);
		}
		if ((flags & taplt_task::FLAG_STATE)) {
			pb_task->set_state(val_task.state);
		}
		if ((flags & taplt_task::FLAG_POSITION1)) {
			pb_task->set_position1(val_task.position1);
		}
		if ((flags & taplt_task::FLAG_POSITION2)) {
			pb_task->set_position2(val_task.position2);
		}
		
		if ((flags & taplt_task::FLAG_APLT_ID)) {
			pb_task->set_aplt_id(val_task.aplt_id);
		}
		if ((flags & taplt_task::FLAG_TASK_ID)) {
			pb_task->set_task_id(val_task.task_id);
		}
		if ((flags & taplt_task::FLAG_BLE_DEVICE_ID)) {
			pb_task->set_ble_device_id(val_task.ble_device_id);
		}

		if (write_pb) {
			protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);
		}
	}
}

void tbg_task::modify_task_state(const taplt_task& aplt_task, int state, bool write_pb)
{
	VALIDATE(in_4tasks_use_ptr(aplt_task), null_str);
	if (aplt_task.state == state) {
		return;
	}
	aplt::taplt_task* mutable_aplt_task = const_cast<aplt::taplt_task*>(&aplt_task);
	mutable_aplt_task->state = state;
	if (write_pb) {
		protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);
	}
}

void tbg_task::modify_task_last_shedule_ticks(const taplt_task& aplt_task, uint32_t ticks)
{
	VALIDATE(in_4tasks_use_ptr(aplt_task), null_str);
	aplt::taplt_task* mutable_aplt_task = const_cast<aplt::taplt_task*>(&aplt_task);
	mutable_aplt_task->last_shedule_ticks = ticks;
}

static void assign_tasks(const std::map<taplt_task_key, taplt_task>& from, std::map<taplt_task_key, taplt_task>& to)
{
	to.clear();
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = from.begin(); it != from.end(); ++ it) {
		to.insert(std::make_pair(it->first, it->second));
	}
}

void tbg_task::set_4tasks(const std::string& scene_name, const std::map<taplt_task_key, taplt_task>& iot_tasks, const std::map<taplt_task_key, taplt_task>& speech_tasks,
		const std::map<taplt_task_key, taplt_task>& var_tasks, const std::map<taplt_task_key, taplt_task>& timed_tasks, const std::vector<aplt::tiot_device>& vec_iot_devices, bool write_pb)
{
	VALIDATE(!scene_name.empty(), null_str);
	int size1 = pb_klink_.iot_tasks_size();

	pb_klink_.mutable_iot_tasks()->Clear();
	int size = pb_klink_.iot_tasks_size();

	pb_klink_.mutable_speech_tasks()->Clear();
	size = pb_klink_.speech_tasks_size();

	pb_klink_.mutable_var_tasks()->Clear();
	size = pb_klink_.var_tasks_size();

	pb_klink_.mutable_timed_tasks()->Clear();
	size = pb_klink_.timed_tasks_size();

	set_scene_name(scene_name, false);

	// in android, 'iot_tasks_ = iot_tasks' will generate compile error.
	assign_tasks(iot_tasks, iot_tasks_);
	assign_tasks(speech_tasks, speech_tasks_);
	assign_tasks(var_tasks, var_tasks_);
	assign_tasks(timed_tasks, timed_tasks_);

	const std::map<taplt_task_key, taplt_task>* all_tasks[] = {&iot_tasks_, &speech_tasks_, &var_tasks_, &timed_tasks_};
	int count = sizeof(all_tasks) / sizeof(all_tasks[0]);
	for (int at = 0; at < count; at ++) {
		const std::map<taplt_task_key, taplt_task>& tasks = *all_tasks[at];
		for (std::map<taplt_task_key, taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
			const aplt::taplt_task& task = it->second;
			add_pb_task(task);
		}
	}

	pb_klink_.mutable_iot_devices()->Clear();
	iot_devices_.clear();
	for (std::vector<aplt::tiot_device>::const_iterator it = vec_iot_devices.begin(); it != vec_iot_devices.end(); ++ it) {
		const aplt::tiot_device& device = *it;
		int64_t ts = nposm;
		std::pair<std::map<tiot_device_key, tiot_device>::const_iterator, bool> ins = iot_devices_.insert(std::make_pair(tiot_device_key(device.src, device.device_id), 
			tiot_device(device.src, device.device_id, device.alias, device.icon, ts, iot_devices_.size())));
		add_pb_iot_device(ins.first->second);
	}

	if (write_pb) {
		protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);
	}

	verify_4tasks();
}

const aplt::taplt_task& tbg_task::task_from_at(int tasks_type, int at) const
{
	const std::map<taplt_task_key, taplt_task>& tasks = get_tasks(tasks_type);
	VALIDATE(at >= 0 && at < (int)tasks.size(), null_str);

	std::map<taplt_task_key, taplt_task>::const_iterator it = tasks.begin();
	if (at != 0) {
		std::advance(it, at);
	}
	return it->second;
}

const aplt::taplt_task& tbg_task::task_from_pb_at(int tasks_type, int pb_at) const
{
	const std::map<taplt_task_key, taplt_task>& tasks = get_tasks(tasks_type);
	VALIDATE(pb_at >= 0 && pb_at < (int)tasks.size(), null_str);

	for (std::map<taplt_task_key, taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const aplt::taplt_task& aplt_task = it->second;
		if (aplt_task.pb_at == pb_at) {
			return aplt_task;
		}
	}

	VALIDATE(false, null_str);
	return tasks.begin()->second;
}

const aplt::taplt_task* tbg_task::iot_task_by_iot_evt(int src, int evt, const std::string& device_id)
{
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = iot_tasks_.begin(); it != iot_tasks_.end(); ++ it) {
		const aplt::taplt_task& aplt_task = it->second;
		if (aplt_task.iot_src == src && aplt_task.src_evt == evt && aplt_task.src_device_id == device_id) {
			return &aplt_task;
		}
	}
	return nullptr;
}

const aplt::taplt_task* tbg_task::speech_task_by_speech_id(const std::string& speech_id)
{
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = speech_tasks_.begin(); it != speech_tasks_.end(); ++ it) {
		const aplt::taplt_task& aplt_task = it->second;
		if (aplt_task.speech_id == speech_id) {
			return &aplt_task;
		}
	}
	return nullptr;
}

void tbg_task::insert_iot_device(int src, const std::string& device_id)
{
	VALIDATE(IS_VALID_APLT_TASK_IOT_SRC(src), null_str);
	VALIDATE(!device_id.empty(), null_str);

	tiot_device_key key(src, device_id);
	int new_pb_at = iot_devices_.size();
	std::pair<std::map<tiot_device_key, tiot_device>::iterator, bool> ins = iot_devices_.insert(std::make_pair(key, 
		tiot_device(src, device_id, null_str, null_str, nposm, new_pb_at)));

	if (!ins.second) {
		SDL_Log("%u insert_alias(src: %i devcie_id: %s) fail", SDL_GetTicks(), src, device_id.c_str());
		return;
	}

	add_pb_iot_device(ins.first->second);
	protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);

	// if (game_config::os == os_windows) {
	if (true) {
		verify_iot_devices();
	}
}

void tbg_task::erase_iot_device(int src, const std::string device_id)
{
	std::map<tiot_device_key, tiot_device>::iterator it = iot_devices_.find(tiot_device_key(src, device_id));
	VALIDATE(it != iot_devices_.end(), null_str);
	int pb_at = it->second.pb_at;
	iot_devices_.erase(it);

	pb_klink_.mutable_iot_devices()->DeleteSubrange(pb_at, 1);
	protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);

	// if alias.pb_at > pb_at, decrement pb_at. 
	for (std::map<tiot_device_key, tiot_device>::iterator it = iot_devices_.begin(); it != iot_devices_.end(); ++ it) {
		// talias* alias = const_cast<talias*>(&it->second);
		tiot_device& iot_device = it->second;
		// pb_at does not participate in sort, modify it here, it should be safe
		if (iot_device.pb_at > pb_at) {
			iot_device.pb_at --;
		}
	}

	// if (game_config::os == os_windows) {
	if (true) {
		verify_iot_devices();
	}
}

void tbg_task::modify_iot_device(int src, const std::string& device_id, const std::string& alias, const std::string& icon, int64_t ts, uint32_t flags)
{
	std::map<tiot_device_key, tiot_device>::iterator it = iot_devices_.find(tiot_device_key(src, device_id));
	VALIDATE(it != iot_devices_.end(), null_str);
	tiot_device& iot_device = it->second;
	int pb_at = it->second.pb_at;

	bool dirty = false;
	bool dirty2 = false;
	if ((flags & tiot_device::FLAG_ALIAS) && alias != iot_device.alias) {
		dirty = true;
		dirty2 = true;
	}
	if ((flags & tiot_device::FLAG_ICON) && icon != iot_device.icon) {
		dirty = true;
		dirty2 = true;
	}
	if ((flags & tiot_device::FLAG_TS) && ts != iot_device.ts) {
		dirty = true;
	}

	if (dirty) {
		pb2::tiot_device* pb_iot_device = pb_klink_.mutable_iot_devices(pb_at);

		if ((flags & tiot_device::FLAG_ALIAS) && alias != iot_device.alias) {
			iot_device.alias = alias;
			pb_iot_device->set_alias(iot_device.alias);
		}
		if ((flags & tiot_device::FLAG_ICON) && icon != iot_device.icon) {
			iot_device.icon = icon;
			pb_iot_device->set_icon(iot_device.icon);
		}
		if ((flags & tiot_device::FLAG_TS) && ts != iot_device.ts) {
			iot_device.ts = ts;
			// pb_iot_device->set_ts(iot_device.ts);
		}

		if (dirty2) {
			protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);
		}
	}

	if (true) {
		verify_iot_devices();
	}
}

std::vector<const tiot_device*> tbg_task::iot_devices_from_src(int src) const
{
	std::vector<const tiot_device*> result;
	for (std::map<tiot_device_key, tiot_device>::const_iterator it = iot_devices_.begin(); it != iot_devices_.end(); ++ it) {
		const tiot_device& alias = it->second;
		if (alias.src == src) {
			result.push_back(&alias);
		}
	}
	return result;
}

void tbg_task::modify_misc_cfg(int type, const std::string& str)
{
	VALIDATE(utils::is_utf8str(str.c_str(), str.size()), null_str);

	if (type == misc_cfg_speech_sensor) {
		pb_klink_.set_speech_sensor_cfg(str);

	} else if (type == misc_cfg_var_sensor) {
		pb_klink_.set_var_sensor_cfg(str);

	} else if (type == misc_cfg_timed_sensor) {
		pb_klink_.set_timed_sensor_cfg(str);

	} else if (type == misc_cfg_task_cpp) {
		pb_klink_.set_task_cpp_cfg(str);
	
	} else if (type == misc_cfg_base_scene) {
		pb_klink_.set_base_scene_cfg(str);

	} else if (type == misc_cfg_add_timed_task) {
		pb_klink_.set_add_timed_task_cfg(str);

	} else {
		VALIDATE(type == misc_cfg_courselist, null_str);
		pb_klink_.set_courselist_cfg(str);
	}

	protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);
}

void tbg_task::apltnotfound_or_verdismatch_to_fresh(bool write_pb)
{
	// At least for now, I don't think it's necessary to modify the 'state' and save the file.
	VALIDATE(!write_pb, null_str);

	bool dirty = false;

	std::map<taplt_task_key, taplt_task>* v_tasks[] = {&iot_tasks_, &speech_tasks_, &var_tasks_, &timed_tasks_};
	int count = sizeof(v_tasks) / sizeof(v_tasks[0]);
	for (int at = 0; at < count; at ++) {
		std::map<taplt_task_key, taplt_task>& tasks = *v_tasks[at];
		for (std::map<taplt_task_key, taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
			const aplt::taplt_task& aplt_task = it->second;
			if (aplt_task.state == taplt_task::state_apltnotfound || aplt_task.state == taplt_task::state_verdismatch) {
				aplt::taplt_task& mutable_task = *const_cast<aplt::taplt_task*>(&aplt_task);
				mutable_task.state = taplt_task::state_fresh;
				dirty = true;
			}
		}
	}
	if (dirty && write_pb) {
		protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);
	}
}

bool tbg_task::did_nav2th_finished(bool result)
{
	set_nav2th_finished();

	// I think it should return false. That is, the ros_instace::erase_task is called immediately 
	// but I don't know if there will be a problem when the task is a task_moveit.
	return false; // true
}

void tbg_task::pump()
{
	uint32_t now = SDL_GetTicks();
	// if (!is_ing_ || (instance->app_in_pure_task_cpp() && finished2() == nposm)) {
	if (!is_ing_) {
		shedule();

	} else if (finished2() != nposm || now >= running_.should_stop_ticks) {
		VALIDATE(bg_task2_ != nullptr, null_str);
		bool fail_retry = true;
		if (now >= running_.should_stop_ticks) {
			fail_retry = false;
			if (!bg_task2_->in_pure_task_cpp()) {
				add_log2(time(nullptr), _("The execution takes too long and force to stop"), 0, false);
			}

		} else if (bg_task2_->aplt_task != nullptr && !bg_task2_->aplt_task->position2.empty()) {
			VALIDATE(finished2() != nposm, null_str);
			if (finished2() == finished_luafunc) {
				const aplt::taplt_task* task = bg_task2_->aplt_task;
				const tapplet* aplt = aplt_from_id(applets_, task->aplt_id);
				const tapplet::ttask& cfg_task = aplt->tasks.find(task->task_id)->second;

				VALIDATE(!running_.exe_result, null_str);
				running_.exe_result = call_stop_single_task(*task, *aplt, cfg_task);
				if (running_.exe_result || task->fails + 1 >= cfg_task.max_fails) {
					set_nav2th_ing();

					fn_navigation_bh luafunc = std::bind(&tbg_task::did_nav2th_finished, this, _1);
					bool result = instance->lua_did_navigation_start(*aplt, bg_task2_->aplt_task->position2, luafunc);
					if (result) {
						now = SDL_GetTicks();
						const int min_navigation_threshold = 5 * 60 * 1000; // 5min
						if (now > running_.should_stop_ticks || running_.should_stop_ticks - now < min_navigation_threshold) {
							SDL_Log("{dbg_nav2th}3.1 now(%u) add min_navigation_threshold, from %u to %u", now, running_.should_stop_ticks, now + min_navigation_threshold);
							running_.should_stop_ticks = now + min_navigation_threshold;
						} else {
							SDL_Log("{dbg_nav2th}3.2 now(%u), running_.should_stop_ticks(%u)", now, running_.should_stop_ticks);
						}
					} else {
						SDL_Log("{dbg_nav2th}3.3 lua_did_navigation_start fail");
						luafunc(false);
					}
				}
			}
			if (finished2() == ing_nav2th) {
				return;
			}
		}
		instance->stop_bg_task(fail_retry);

	} else if (running_.timer_interval != 0) {
		timer_handler();
	}
}

void tbg_task::verify_tasks(const std::map<taplt_task_key, taplt_task>& tasks, int pb_tasks_size)
{
	VALIDATE((int)tasks.size() == pb_tasks_size, null_str);
	int type = nposm;
	std::set<int> pb_ats;
	int max_pb_at = 0;
	std::set<int> var_ats;
	int max_var_at = 0;
	std::set<int> timed_ats;
	int max_timed_at = 0;
	std::set<int> aux_key_ids;
	int max_aux_key_id = 0;
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const taplt_task_key& key = it->first;
		const aplt::taplt_task& task = it->second;

		VALIDATE(key.type == task.type, null_str);
		VALIDATE(key.iot_src == task.iot_src, null_str);
		VALIDATE(key.src_evt == task.src_evt, null_str);
		// VALIDATE(key.var_at == task.var_at, null_str);
		VALIDATE(key.aux_key_id == task.aux_key_id, null_str);

		if (type != nposm) {
			VALIDATE(task.type == type, null_str);
		} else {
			type = task.type;
		}
		VALIDATE(pb_ats.count(task.pb_at) == 0, null_str);
		pb_ats.insert(task.pb_at);
		if (task.pb_at > max_pb_at) {
			max_pb_at = task.pb_at;
		}

		if (task.type == aplt::taplt_task::type_var) {
			// VALIDATE(task.var_at == var_at, null_str);
			// VALIDATE(task.var_at == task.pb_at, null_str);
			// var_at ++;
			VALIDATE(var_ats.count(task.var_at) == 0, null_str);
			var_ats.insert(task.var_at);
			if (task.var_at > max_var_at) {
				max_var_at = task.var_at;
			}
		} else {
			VALIDATE(task.last_shedule_ticks == 0, null_str);
		}

		if (task.type == aplt::taplt_task::type_timed) {
			VALIDATE(timed_ats.count(task.timed_at) == 0, null_str);
			timed_ats.insert(task.timed_at);
			if (task.timed_at > max_timed_at) {
				max_timed_at = task.timed_at;
			}

		} else {
			VALIDATE(task.timed_at == nposm, null_str);
		}

		if (task.type == aplt::taplt_task::type_var || task.type == aplt::taplt_task::type_timed) {
			VALIDATE(aux_key_ids.count(task.aux_key_id) == 0, null_str);
			aux_key_ids.insert(task.aux_key_id);
			if (task.aux_key_id > max_aux_key_id) {
				max_aux_key_id = task.aux_key_id;
			}

		} else {
			VALIDATE(task.aux_key_id == nposm, null_str);
		}
	}
	if (!tasks.empty()) {
		std::stringstream err;
		if (max_pb_at + 1 != tasks.size()) {
			err << "pb_ats: " << utils::join(pb_ats) << " tasks_.size:" << tasks.size();
			VALIDATE(false, err.str());
		}
		if (type == aplt::taplt_task::type_var) {
			if (max_var_at + 1 != tasks.size()) {
				err << "var_ats: " << utils::join(var_ats) << " tasks_.size:" << tasks.size();
				VALIDATE(false, err.str());
			}

		} else if (type == aplt::taplt_task::type_timed) {
			if (max_timed_at + 1 != tasks.size()) {
				std::stringstream err;
				err << "timed_ats: " << utils::join(timed_ats) << " tasks_.size:" << tasks.size();
				VALIDATE(false, err.str());
			}
		}

		// For aux_key_id, it only needs to guarantee uniqueness and does not require 'max_aux_key_id + 1 == tasks.size()'.
		// When a task in the middle is erased, 'max_aux_key_id + 1 == tasks.size()' is not satisfied.
		// Multiple tasks in the middle may have been erased, which makes it impossible to use max_aux_key_id for verification.
/*
		if (type == aplt::taplt_task::type_timed) {
			// Below code supports erasing at most one task from the middle.
			if (max_aux_key_id + 1 != (int)tasks.size() && max_aux_key_id != (int)tasks.size()) {
				std::stringstream err;
				err << "type: " << type << ", aux_key_ids: " << utils::join(aux_key_ids) << " tasks_.size:" << tasks.size();
				VALIDATE(false, err.str());
			}
		}
*/
	}
}

void tbg_task::verify_4tasks()
{
	verify_tasks(iot_tasks_, pb_klink_.iot_tasks_size());
	verify_tasks(speech_tasks_, pb_klink_.speech_tasks_size());
	verify_tasks(var_tasks_, pb_klink_.var_tasks_size());
	verify_tasks(timed_tasks_, pb_klink_.timed_tasks_size());
}

void tbg_task::verify_iot_devices()
{
	int pb_aliases_size = pb_klink_.iot_devices_size();

	VALIDATE((int)iot_devices_.size() == pb_aliases_size, null_str);
	std::set<int> pb_ats;
	int max_pb_at = 0;
	for (std::map<tiot_device_key, tiot_device>::const_iterator it = iot_devices_.begin(); it != iot_devices_.end(); ++ it) {
		const tiot_device& alias = it->second;
		VALIDATE(pb_ats.count(alias.pb_at) == 0, null_str);
		pb_ats.insert(alias.pb_at);
		if (alias.pb_at > max_pb_at) {
			max_pb_at = alias.pb_at;
		}
	}
	if (!iot_devices_.empty()) {
		if (max_pb_at + 1 != iot_devices_.size()) {
			std::stringstream err;
			err << "pb_ats: " << utils::join(pb_ats) << " iot_devices_.size:" << iot_devices_.size();
			VALIDATE(false, err.str());
		}
	}
}

void tbg_task::shedule()
{
	// VALIDATE(!is_ing_ || instance->app_in_pure_task_cpp(), null_str);
	VALIDATE(!is_ing_, null_str);
	if (stopping_) {
		SDL_Log("ttiming::shedule, stopping_ == true, do nothing");
		return;
	}

	const int elapse_today = utils::calculate_hour24_time(time(nullptr));

	// int dbg_zerotz_t = (20 * 3600 + 47 * 60 + 0 + game_config::equation_of_time) % (24 * 3600);
	if (last_shedule_zerotz_t_ != nposm && elapse_today < last_shedule_zerotz_t_) {
	// if (last_shedule_zerotz_t_ != nposm && last_shedule_zerotz_t_ >= dbg_zerotz_t) {
		// When there is a circle, it is often 0 o'clock
		handle_0clock(elapse_today);
	}

	last_shedule_zerotz_t_ = elapse_today;

	if (is_ing_) {
		// in task cpp, single task is sheduled during above high priority sys_task.
		// in task cpp, must not run timing-task, or low priority sys-task.
		return;
	}

	const aplt::taplt_task* hit_task = nullptr;
	// modify only one fresh-task to expired-task every shedule.
	const aplt::taplt_task* expired_task = nullptr;

	// if (game_config::os == os_windows) {
	if (true) {
		verify_4tasks();
	}

	int min_zerotz_t = INT32_MAX;
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = timed_tasks_.begin(); it != timed_tasks_.end(); ++ it) {
		const aplt::taplt_task& task = it->second;
		int elapse_today2 = elapse_today;
		// if (elapse_today < task.zerotz_t && task.zerotz_t - elapse_today >= 3600 ) {
		//	elapse_today2 = elapse_today2
		// }
		if (task.state == aplt::taplt_task::state_fresh) {
			if (elapse_today2 >= task.zerotz_t) {
				// [task.t_zerotz, task.zerotz_t + overdue_threshold_)
				if (elapse_today2 < task.zerotz_t + overdue_threshold_) {
					if (hit_task == nullptr || task.zerotz_t < hit_task->zerotz_t) {
						// timed_tasks_ are not sorted by 'zerotz_t'. 
						// When multiple items meet the range, take the one with the smallest 'zerotz_t'.
						hit_task = &task;
					}
				} else {
					expired_task = &task;
				}
			}
		}
	}

	// if (game_config::os != os_windows && expired_task != nullptr) {
	if (expired_task != nullptr) {
		VALIDATE(expired_task->state == aplt::taplt_task::state_fresh && expired_task->zerotz_t < elapse_today, null_str);
		// guss reason
		std::string disable_desc;
		if (new_klink_task_disabled2(&disable_desc)) {
		} else if (instance->fg_aplt() != nullptr) {
			disable_desc = _("A apllet is running in the foreground");
		} else {
			disable_desc = _("Unknown");
		}

		utils::string_map symbols;
		symbols["reason"] = disable_desc;
		const std::string msg = vgettext2("Time has expired and cannot be executed. Possible reason: $reason", symbols);
		add_log7(expired_task->aplt_id, expired_task->task_id, expired_task->ble_device_id, time(nullptr), msg, 0, false);

		// Strictly, it should used write_pb == true,
		// It is estimated that there will be a write_pb operation in the near future.
		SDL_Log("change task(%s-%s) state from state_fresh to state_finished_expired", expired_task->aplt_id.c_str(), expired_task->task_id.c_str());
		// maybe first fail, and it is second or more.
		const_cast<aplt::taplt_task*>(expired_task)->fails = 0;
		modify_task_state(*expired_task, aplt::taplt_task::state_finished_expired, false);
		return;
	}

	if (instance->fg_aplt() != nullptr || new_klink_task_disabled2(nullptr)) {
		return;
	}

	if (!instance->app_can_run_var_or_timing_task()) {
		return;
	}

	const tapplet* hit_aplt = nullptr;
	const tapplet::ttask* cfg_task = nullptr;

	aplt::ttask_vars task_vars;
	if (hit_task != nullptr) {
		hit_aplt = aplt::aplt_from_id_ex(applets_, hit_task->aplt_id);
		if (hit_aplt != nullptr && hit_aplt->tasks.count(hit_task->task_id) != 0) {
			cfg_task = &hit_aplt->tasks.find(hit_task->task_id)->second;
		}

		if (cfg_task == nullptr) {
			// Strictly, it should used write_pb == true,
			// It is estimated that there will be a write_pb operation in the near future.
			modify_task_state(*hit_task, aplt::taplt_task::state_apltnotfound, false);
			hit_task = nullptr;

		} else {
			task_vars = clone_env_vars(true);
		}
	}

	if (hit_task == nullptr) {
		hit_task = instance->app_shedule_var_task(task_vars);
		if (hit_task == nullptr) {
			return;
		}
	}


	std::string str = utils::format_elapse_hms(elapse_today);
	SDL_Log("%s It is time to execute: %s-%s", str.c_str(), hit_task->aplt_id.c_str(), hit_task->task_id.c_str());

	request_klink_aplt_task(*hit_task, task_vars);
}

void tbg_task::request_klink_aplt_task(const taplt_task& _aplt_task, const aplt::ttask_vars& task_vars)
{
	VALIDATE(in_4tasks_use_ptr(_aplt_task), null_str);

	// const taplt_task* _aplt_task2 = &_aplt_task;
	if (_aplt_task.type == taplt_task::type_iot || _aplt_task.type == taplt_task::type_speech) {
		int pb_at = _aplt_task.pb_at;
		VALIDATE(pb_at != nposm, null_str);
		instance->stop_bg_task_if_runing(null_str, _("To run a IoT or speech aplt task, stop the current one"));
		// _aplt_task2 = &task_from_pb_at(_aplt_task.type, pb_at);

	} else {
		// type_var, type_timed
		VALIDATE(!is_ing_, null_str);
	}

	// modify to state_running after stop_bg_task_if_runing(...), becuase it maybe change state to state_finished_ok/fail.
	modify_task_state(_aplt_task, taplt_task::state_running, false);
	const taplt_task& aplt_task = _aplt_task;

	VALIDATE(instance->fg_aplt() == nullptr && !is_ing_, null_str);

	if (aplt_task.type == taplt_task::type_var) {
		modify_task_last_shedule_ticks(aplt_task, SDL_GetTicks());
		// SDL_Log("%u, aplt_task.last_shedule_ticks: %u", SDL_GetTicks(), aplt_task.last_shedule_ticks);
	}

	const bool original_in_pure_task_cpp = instance->app_in_pure_task_cpp();

	// Although it is not called 'modify_task' in 'instance->app_request_klink_aplt_task' now, 
	// avoid not letting the problem even if it is called.
	// const aplt::taplt_task hit_task2 = aplt_task;

	tbase_bg_task2* sys_task = instance->app_request_klink_aplt_task(aplt_task, task_vars);
	if (sys_task != nullptr) {
		start_sys2(*sys_task, original_in_pure_task_cpp);

	} else {
		// _aplt_task2 = &task_from_pb_at(hit_task2.type, hit_task2.pb_at);
		if (aplt_task.state == taplt_task::state_running) {
			modify_task_state(aplt_task, taplt_task::state_finished_fail, true);

		} else {
			VALIDATE(aplt_task.state == taplt_task::state_apltnotfound || aplt_task.state == taplt_task::state_verdismatch, null_str);
		}
	}
}

void tbg_task::start_task_cpp_fake(const tbase_bg_task2& sys_task)
{
	VALIDATE(!is_ing_, null_str);
	running_.verify_pre_start();

	is_ing_ = true;
	VALIDATE(bg_task2_ != nullptr, null_str);

	running_.finished2 = nposm;
	running_.start_ticks = SDL_GetTicks();
	// const int terminate_threshold = 6 * 60 * 1000; // 6min
	// running_.should_stop_ticks = running_.start_ticks + terminate_threshold;
	running_.should_stop_ticks = UINT32_MAX;

	running_.task_cpp_fake_started = true;
}

void tbg_task::nullptr_task_cpp_fake()
{
	VALIDATE(is_ing_, null_str);
	VALIDATE(bg_task2_ != nullptr, null_str);
	VALIDATE(running_.task_cpp_fake_started, null_str);

	is_ing_ = false;

	running_.clear();	
}

void tbg_task::pre_start(tbg_task::tbase_bg_task2& sys_task)
{
	VALIDATE(lua_ != nullptr && L != nullptr, null_str);
	VALIDATE(!is_ing_, null_str);
	running_.verify_pre_start();
	running_.start_times ++;

	// I hope bg_aplt awlays is the applet that base_driver belongs.
	// but for example log, them use bg_aplt_'s filds.
	is_ing_ = true;

	if (sys_task.curr_aplt != nullptr) {
		const tapplet& curr_aplt = *sys_task.curr_aplt;
		lua_bundleid_ = utils::replace_all(curr_aplt.bundleid, ".", "_");
		lua_aplt_timing_clazz_ = utils::join_app_prefix_id(lua_bundleid_, "bg_task");

		if (!curr_aplt.fake) {
			instance->increment_textdomain_usage(curr_aplt);
		}

	} else {
		lua_bundleid_.clear();
		lua_aplt_timing_clazz_.clear();
	}

	running_.finished2 = nposm;
	running_.start_ticks = SDL_GetTicks();
	// const int terminate_threshold = 6 * 60 * 1000; // 6min
	// const int terminate_threshold = 2 * 60 * 60 * 1000; // 2hour
	const int terminate_threshold = 2 * 24 * 60 * 60 * 1000; // 2day
	running_.should_stop_ticks = running_.start_ticks + terminate_threshold;
}

bool tbg_task::did_navigation_bh_single_task(bool result, const aplt::tapplet& aplt, const aplt::taplt_task& aplt_task, const aplt::tapplet::ttask& cfg_task)
{
	VALIDATE(is_ing_, null_str);
	if (cfg_task.type == task_ble) {
		bool ok = false;
		{
			tstack_size_lock lock(L, 0);
			const ttask_vars& task_vars = bg_task2_->task_vars();

			const int max_retvalues = 2;

			luaW_getglobal(L, lua_aplt_timing_clazz_.c_str(), "start");
			lua_pushstring(L, bg_task2_->aplt_task->task_id.c_str());
			// sys_task.luaW_pushtask_vars(L, task_vars);
			luaW_pushtask_vars(L, task_vars);

			lua_->protected_call(2, max_retvalues);

			VALIDATE(lua_isboolean(L, 1), null_str);
			ok = lua_toboolean(L, 1)? true: false;
			int interval = 0;
			if (ok) {
				interval = lua_tointegerx(L, 2, nullptr);

			} else {
				VALIDATE(running_.finished2 == finished_luafunc, "if fail, ble bg_task require call rose.luafunc_finished()");
			}
			lua_pop(L, max_retvalues);
			if (interval != 0) {
				VALIDATE(interval >= 400, null_str);
				running_.timer_next_ticks = SDL_GetTicks();
				running_.timer_interval = interval;

			}
		}

		// for task_ble, don't keep ros_task always.
		return false;
	}

	VALIDATE(cfg_task.type != task_ble && aplt::is_single_task(cfg_task.type), null_str);
	return instance->app_did_navigation_bh_use_cpp(result, aplt, aplt_task, cfg_task);
}

#define APLT_TASK_MAX_FAILS		2
void tbg_task::start_aplt_task(tbg_task::tbase_bg_task2& sys_task)
{
	VALIDATE(sys_task.curr_aplt != nullptr && sys_task.aplt_task != nullptr, null_str);
	const aplt::taplt_task& tmp_aplt_task = *sys_task.aplt_task;

	const tapplet* verify_aplt = aplt_from_id_ex(applets_, tmp_aplt_task.aplt_id);
	VALIDATE(verify_aplt != nullptr && verify_aplt == sys_task.curr_aplt, null_str);

	if (sys_task.aplt_task->is_klink()) {
		VALIDATE(sys_task.klink_single_aplt_task2 != nullptr && sys_task.klink_single_aplt_task2->equal8(*sys_task.aplt_task), null_str);
	} else {
		VALIDATE(sys_task.klink_single_aplt_task2 == nullptr, null_str);
	}

	pre_start(sys_task);

	const tapplet& curr_aplt = *sys_task.curr_aplt;

	const tapplet::ttask& cfg_task = curr_aplt.tasks.find(tmp_aplt_task.task_id)->second;
	VALIDATE(is_single_task(cfg_task.type), null_str);

	// '2position + task' don't support fail retry.
	// running_.aplt_task_max_fails = tmp_aplt_task.position2.empty()? cfg_task.max_fails: 1;
	running_.aplt_task_max_fails = cfg_task.max_fails;
	VALIDATE(running_.aplt_task_max_fails != nposm && running_.aplt_task_max_fails <= APLT_TASK_MAX_FAILS, null_str);

	instance->bg_task_will_start(sys_task);

	config aplt_cfg;
	const std::string aplt_path = curr_aplt.res_path;
	const std::string textdomain = lua_bundleid_ + "-lib";
	
	// tcurrent_aplt_lock aplt_lock(aplt_);
	// tbind_textdomain_lock textdomain_lock(textdomain, aplt_path);

	// wml_config_from_file(aplt_path + "/xwml/" + BASENAME_APLT, aplt_cfg);
	
	gui2::insert_window_builder_from_cfg(aplt_cfg);

	config& sub_cfg = aplt_cfg.add_child("binary_path");
	sub_cfg["path"] = binary_paths_manager::full_path_indicator + aplt_path;
	const binary_paths_manager bin_paths_manager(aplt_cfg);

	//
	if (!curr_aplt.fake) {
		setup_aplt_user_data_dir(get_aplt_user_data_dir(lua_bundleid_));

		lua_->register_timing(curr_aplt, lua_bundleid_, curr_aplt.version, aplt_cfg);
	} else {
		VALIDATE(cfg_task.type == task_block, null_str);
	}
	// load main.lua
	// lua_->load_lua("lua/main.lua");
	// lua_->load_lua("lua/bg_task.lua");

	const bool navigation = !bg_task2_->aplt_task->position1.empty();

	bool require_navigation = navigation && bg_task2_->aplt_task->fails == 0;
	fn_navigation_bh luafunc = std::bind(&tbg_task::did_navigation_bh_single_task, this, _1,
		std::ref(curr_aplt), std::ref(tmp_aplt_task), std::ref(cfg_task));

	if (require_navigation) {
		sys_task.aplt_task_pre_navigation();
			
		bool result = instance->lua_did_navigation_start(curr_aplt, bg_task2_->aplt_task->position1, luafunc);
		if (!result) {
			// here task is type_aplt_task
			sys_task.aplt_task_navigation_stopped(false, false);

			luafunc(false);
		}

	} else {
		if (navigation && bg_task2_->aplt_task->fails != 0) {
			add_log2(time(nullptr), _("Second or later, don't navigation"), 0, false);
		}
		// for second or later, result of navigation is true always.
		luafunc(true);
	}
}

void tbg_task::start_sys2(tbg_task::tbase_bg_task2& sys_task, bool original_in_pure_task_cpp)
{
	if (sys_task.in_pure_task_cpp()) {
		VALIDATE(!original_in_pure_task_cpp, null_str);
		SDL_Log("{task_cpp}[tbg_task::shedule]shedule one 'cpp' task run");
		start_task_cpp_fake(sys_task);
		instance->bg_task_will_start(sys_task);

		// const tapplet* aplt = aplt_from_id(applets_, sys_task.aplt_task->aplt_id);
		// instance->increment_textdomain_usage(*aplt);
				
	} else {
		if (is_ing_ && bg_task2_->in_task_cpp()) {
			VALIDATE(running_.task_cpp_fake_started, null_str);
			VALIDATE(original_in_pure_task_cpp, null_str);
			nullptr_task_cpp_fake();

		} else {
			VALIDATE(!original_in_pure_task_cpp, null_str);
		}
		start_aplt_task(sys_task);
	}
}

bool tbg_task::priority_can_run(const aplt::taplt_task& desire, std::string* msg_result) const
{
	VALIDATE(is_ing_, null_str);
	VALIDATE(desire.is_nontimed_priority(), null_str);
	VALIDATE(in_4tasks(desire), null_str);

	const aplt::tbg_task::tbase_bg_task2& bg_task2 = *bg_task2_;
	const aplt::taplt_task* bg_aplt_task = bg_task2.klink_cpp_aplt_task2 != nullptr? bg_task2.klink_cpp_aplt_task2: bg_task2.klink_single_aplt_task2;
	if (bg_aplt_task == nullptr) {
		bg_aplt_task = bg_task2.aplt_task;
	}

	std::string err_msg;
	bool can_run = true;
	if (bg_aplt_task != nullptr) {
		if (desire.priority < bg_aplt_task->priority) {
			can_run = false;

		} else if (desire.priority == bg_aplt_task->priority) {
			if (desire.aplt_id == bg_aplt_task->aplt_id && desire.task_id == bg_aplt_task->task_id) {
				ttask_pair pair = task_pair_from_2_id(applets_, bg_aplt_task->aplt_id, bg_aplt_task->task_id, true, false);
				VALIDATE(pair.aplt != nullptr && pair.task != nullptr, null_str);
				if (pair.task->nonpreemptive) {
					can_run = false;
					err_msg = _("Is running the same task, and it is the same priority, non-preemptive, and can not run this task.");
				}
			} else {
				// in default, same priority can be preemptible
			}
		}
	} else {
		// bg_aplt_task == nullptr
		// bg-aplt_task is trigger by center-gui. In this situation, nontimed-aplt_task always can run.
		// if is trigger by center-gui, sys_task.aplt_task maybe is nullptr still. for example, this aplt_task is move only.
	}

	if (!can_run) {
		if (err_msg.empty()) {
			err_msg = _("There is a task with a higher priority running, and can not run this task.");
		}
		instance->add_msg_only_log(logtype_warn, err_msg, 0, false);
	}
	if (msg_result != nullptr) {
		*msg_result = err_msg;
	}

	return can_run;
}

void tbg_task::set_luafunc_finished()
{
	VALIDATE(running_.finished2 == nposm || running_.finished2 == finished_luafunc, null_str);
	// if (running_.finished2 != nposm) {
		// 
	//	return;
	// }

	running_.finished2 = finished_luafunc;
}

void tbg_task::set_nav2th_ing()
{
	VALIDATE(running_.finished2 == finished_luafunc, null_str);
	running_.finished2 = ing_nav2th;
}

void tbg_task::set_nav2th_finished()
{
	VALIDATE(running_.finished2 == ing_nav2th, null_str);
	running_.finished2 = finished_nav2th;
}

void tbg_task::set_task_finished()
{
	// VALIDATE(running_.finished2 == nposm || running_.finished2 == finished_luafunc, null_str);

	// must set to finished_luafunc. becuase task_cpp, see ::pump(
	// running_.finished2 = finished_luafunc;

	SDL_Log("{dbg_nav2th}(2)in set_task_finished(), change should_stop_ticks from %u to %u", running_.should_stop_ticks, SDL_GetTicks());
	running_.should_stop_ticks = SDL_GetTicks();
}

void tbg_task::handle_0clock(int zerotz_t)
{
	SDL_Log("handle_0clock, zerotz_t: %i, last_shedule_zerotz_t_: %i", zerotz_t, last_shedule_zerotz_t_);
	bool dirty = false;
	while (true) {
		const aplt::taplt_task* fresh_task = nullptr;
		for (std::map<taplt_task_key, taplt_task>::const_iterator it = timed_tasks_.begin(); it != timed_tasks_.end(); ++ it) {
			const aplt::taplt_task& task = it->second;
			if (state_can_fresh(task.state)) {
				fresh_task = &task;
				break;
			}
		}
		if (fresh_task != nullptr) {
			SDL_Log("[handle_0clock]to fresh: %s %s-%s", utils::format_second_24hoursys(fresh_task->zerotz_t).c_str(), fresh_task->aplt_id.c_str(), fresh_task->task_id.c_str());
			modify_task_state(*fresh_task, aplt::taplt_task::state_fresh, false);
			dirty = true;
		} else {
			break;
		}
	}
	if (dirty) {
		protobuf::write_sha1pb(pb_klink_type_, pb_klink_backup_type_);
	}
}

void tbg_task::timer_handler()
{
	VALIDATE(is_ing_, null_str);
	VALIDATE(running_.timer_interval != 0, null_str);
	VALIDATE(running_.timer_next_ticks != 0, null_str);

	uint32_t now = SDL_GetTicks();
	if (now < running_.timer_next_ticks) {
		return;
	}

	// tstack_size_lock lock(L, 0);
	// gui2::twidget::tdisable_lua_unlimited_mem_lock lock;
	luaW_getglobal(L, lua_aplt_timing_clazz_.c_str(), "timer_handler");
	lua_pushinteger(L, now);
	lua_->protected_call(1, 0);

	running_.timer_next_ticks = SDL_GetTicks() + running_.timer_interval;
}

const aplt::tapplet* tbg_task::task_cpp_aplt(const tapplet::ttask** task) const
{
	if (is_ing_ && bg_task2_->in_task_cpp()) {
		aplt::ttask_pair task_cpp_pair = bg_task2_->task_cpp_pair();
		if (task != nullptr) {
			*task = task_cpp_pair.task;
		}
		return task_cpp_pair.aplt;
	}
	if (task != nullptr) {
		*task = nullptr;
	}
	return nullptr;
}

const aplt::taplt_task* tbg_task::klink_task_if_ing() const
{
	if (!is_ing()) {
		return nullptr;
	}

	const aplt::tbg_task::tbase_bg_task2& sys_task = *bg_task2_;
	return sys_task.klink_cpp_aplt_task2 != nullptr? sys_task.klink_cpp_aplt_task2: sys_task.klink_single_aplt_task2;
}

void tbg_task::stop2(bool fail_retry)
{
	VALIDATE(is_ing_, null_str);
	is_ing_ = false;

	const bool in_task_cpp = bg_task2_->in_task_cpp();
	const tbase_bg_task2* sys_task = bg_task2_;

	bool retry = false;
	bool in_pure_task_cpp = false;
	if (bg_task2_->aplt_task != nullptr) {
		const tapplet* curr_aplt = bg_task2_->curr_aplt;
		retry = stop_aplt_task(fail_retry);
		if (!curr_aplt->fake) {
			instance->decrement_textdomain_usage(*curr_aplt);
		}

	} else {
		in_pure_task_cpp = stop_non_aplt_task();
		VALIDATE(in_pure_task_cpp, null_str);
	}

	if (in_pure_task_cpp) {
		return;
	}

	// launcher's APLT_MSG_DIDTERMINATE require task_ must be nullptr
	// so must call ros_instance_.erase_task() before it.

	if (in_task_cpp) {
		SDL_Log("{task_cpp}[tbg_task::stop2]in task_cpp still, restore back");
		start_task_cpp_fake(*sys_task);
	}

	if (retry) {
		instance->app_request_retry_aplt_task();
		// const bool original_in_pure_task_cpp = instance->app_in_pure_task_cpp();
/*
		tbg_task::tbase_bg_task2* result = instance->app_request_retry_aplt_task();
		if (result != nullptr) {
			// if fail retry, must be aplt_task.
			VALIDATE(result == bg_task2_ && bg_task2_->aplt_task != nullptr, null_str);
			start_sys2(*bg_task2_, original_in_pure_task_cpp);
		}
*/
	}
}

void tbg_task::call_start_single_task(const aplt::taplt_task& task, const tapplet::ttask& cfg_task)
{
}

bool tbg_task::call_stop_single_task(const aplt::taplt_task& task, const tapplet& aplt, const tapplet::ttask& cfg_task)
{
	VALIDATE(!running_.stop_single_task_called, null_str);

	bool ret = true;
	if (cfg_task.type == task_ble) {
		// program 'stop' use lua.
		 
		// tstack_size_lock lock(L, 0);
		tstop_lock lock2(*this);

		luaW_getglobal(L, lua_aplt_timing_clazz_.c_str(), "stop");
		lua_->protected_call(0, 1);

		ret = luaW_toboolean(L, -1);
		lua_pop(L, 1);

	} else {
		// program 'stop' use cpp.
		instance->app_stop_single_task_use_cpp(task, aplt, cfg_task);
	}
	running_.stop_single_task_called = true;

	return ret;
}

bool tbg_task::stop_aplt_task(bool fail_retry)
{
	VALIDATE(!is_ing_, null_str);
	const tbase_bg_task2& sys_task = *bg_task2_;
	VALIDATE(sys_task.curr_aplt != nullptr && sys_task.aplt_task != nullptr, null_str);
	if (sys_task.aplt_task->is_klink()) {
		VALIDATE(sys_task.klink_single_aplt_task2 != nullptr && sys_task.klink_single_aplt_task2->equal8(*sys_task.aplt_task), null_str);
	} else {
		VALIDATE(sys_task.klink_single_aplt_task2 == nullptr, null_str);
	}
	VALIDATE(sys_task.aplt_task->state == aplt::taplt_task::state_running, null_str);
	VALIDATE(running_.aplt_task_max_fails != nposm, null_str);

	const aplt::taplt_task* task = bg_task2_->aplt_task;

	const tapplet& curr_aplt = *bg_task2_->curr_aplt;
	const tapplet::ttask& cfg_task = curr_aplt.tasks.find(task->task_id)->second;
	VALIDATE(is_single_task(cfg_task.type), null_str);

	int max_fails = running_.aplt_task_max_fails;
	
	if (task->is_klink()) {
		const aplt::taplt_task& klink_aplt_task = task_from_pb_at(task->type, task->pb_at);
		VALIDATE(klink_aplt_task.equal8(*task), null_str);
	}

	// below running_.clear will set 'running_.finished2 = nposm'

	SDL_Log("tbg_task::stop_aplt_task(fail_retry: %s), should_stop_ticks: %u", fail_retry? "true": "false", running_.should_stop_ticks);

	bool outer_log = true;
	bool& exe_result = running_.exe_result;
	if (!running_.stop_single_task_called) {
		VALIDATE(!exe_result, null_str);
		exe_result = call_stop_single_task(*task, curr_aplt, cfg_task);

	} else {
		VALIDATE(!exe_result || single_task_nav2th_is_started(), str_cast(running_.finished2));
	}
		
	bool retry = false;
	if (!exe_result && fail_retry) {
		if (task->fails + 1 < max_fails) {
			aplt::taplt_task* task2 = const_cast<aplt::taplt_task*>(task);
			task2->fails ++;
			retry = true;
		}
	}
		
	if (retry) {
		utils::string_map symbols;
		symbols["times"] = str_cast(task->fails);
		const std::string log = vgettext2("The $times|th execution failed, wait for the next time", symbols);

		add_log7(task->aplt_id, task->task_id, task->ble_device_id, time(nullptr), log, 0, false);
		outer_log = false;

	} else {
		aplt::taplt_task* task2 = const_cast<aplt::taplt_task*>(task);
		task2->state = exe_result? aplt::taplt_task::state_finished_ok: aplt::taplt_task::state_finished_fail;

		if (task->fails != 0) {
			aplt::taplt_task* task2 = const_cast<aplt::taplt_task*>(task);
			task2->fails = 0;
		}
		SDL_Log("finished %s-%s, result: %s", task->aplt_id.c_str(), task->task_id.c_str(), exe_result? "ok": "fail");
	}
	

	VALIDATE(sys_task.aplt_task == task, null_str);

	if (!curr_aplt.fake) {
		// lua_->call_lua_breakpoint();
		lua_->unregister_timing(lua_bundleid_);

		// if (instance->fg_aplt() == nullptr) {
			// Enforce a complete garbage collection
			lua_gc(L, LUA_GCCOLLECT);
		// }
	}

	running_.clear();
	instance->bg_task_stopped(sys_task, outer_log);

	return retry;
}

bool tbg_task::stop_non_aplt_task()
{
	VALIDATE(!is_ing_, null_str);
	VALIDATE(bg_task2_->curr_aplt == nullptr && bg_task2_->aplt_task == nullptr, null_str);

	const bool in_pure_task_cpp = bg_task2_->in_pure_task_cpp();

	instance->bg_task_stopped(*bg_task2_, true);
	running_.clear();

	return in_pure_task_cpp;
}

void tbg_task::add_pb_iot_device(const tiot_device& iot_device)
{
	pb2::tiot_device& new_iot_device = *pb_klink_.add_iot_devices();
	new_iot_device.set_src(iot_device.src);
	new_iot_device.set_device_id(iot_device.device_id);
	new_iot_device.set_alias(iot_device.alias);
	new_iot_device.set_icon(iot_device.icon);
	// new_iot_device.set_ts(iot_device.ts);
}

void tbg_task::load_pb_tasks2(int tasks_type)
{
	const int elapse_today = (time(nullptr) + game_config::equation_of_time) % ONE_DAY_SECONDS;
	int size = nposm;
	if (tasks_type == taplt_task::type_iot) {
		size = pb_klink_.iot_tasks_size();

	} else if (tasks_type == taplt_task::type_speech) {
		size = pb_klink_.speech_tasks_size();

	} else if (tasks_type == taplt_task::type_var) {
		size = pb_klink_.var_tasks_size();

	} else {
		VALIDATE(tasks_type == taplt_task::type_timed, null_str);
		size = pb_klink_.timed_tasks_size();
	}

	std::set<std::string> existed_speech_ids;
	for (int at = 0; at < size; ) {
		pb2::ttask* pb_task = nullptr;
		if (tasks_type == taplt_task::type_iot) {
			pb_task = pb_klink_.mutable_iot_tasks(at);

		} else if (tasks_type == taplt_task::type_speech) {
			pb_task = pb_klink_.mutable_speech_tasks(at);

		} else if (tasks_type == taplt_task::type_var) {
			pb_task = pb_klink_.mutable_var_tasks(at);

		} else {
			VALIDATE(tasks_type == taplt_task::type_timed, null_str);
			pb_task = pb_klink_.mutable_timed_tasks(at);
		}

		const int type = pb_task->type();
		const int priority = pb_task->priority();
		const std::string aplt_id = pb_task->aplt_id();
		const std::string task_id = pb_task->task_id();
		const std::string ble_device_id = pb_task->ble_device_id();
		const std::string position1 = pb_task->position1();
		const std::string position2 = pb_task->position2();

		const int iot_src = pb_task->iot_src();
		const int src_evt = pb_task->src_evt();
		const std::string src_device_id = pb_task->src_device_id();

		const std::string speech_id = pb_task->speech_id();

		const int var_at = pb_task->var_at();

		const int zerotz_t = pb_task->zerotz_t();
		const int timed_at = pb_task->timed_at();

		int state = pb_task->state();

		bool valid = true;
		// const bool fresh_when_start = true;
		// bool state_dirty = false;

		// field: state
		if (type != tasks_type) {
			valid = false;

		} else if (!IS_VALID_APLT_TASK_STATE(state)) {
			// if state is invalid, think this pb_task is invalid. for example timing.proto' s format changed.
			valid = false;

		}

		int desire_state = state;
		if (state == aplt::taplt_task::state_running) {
			// last maybe unexpected termination
			desire_state = aplt::taplt_task::state_fresh;

		} else if (tasks_type != taplt_task::type_timed) {
			desire_state = aplt::taplt_task::state_fresh;

		} else {
			VALIDATE(tasks_type == taplt_task::type_timed, null_str);
			if (zerotz_t >= elapse_today) {
				desire_state = aplt::taplt_task::state_fresh;

			} else {
				desire_state = aplt::taplt_task::state_finished_expired;
			}
		}


		if (desire_state != state) {
			state = desire_state;
			pb_task->set_state(state);
		}

		// field: priority
		if (valid) {
			if (type == taplt_task::type_iot || type == taplt_task::type_speech) {
				valid = IS_VALID_APLT_TASK_NONTIMED_PRIORITY(priority);

			} else {
				VALIDATE(type == taplt_task::type_var || type == taplt_task::type_timed, null_str);
				valid = IS_VALID_APLT_TASK_TIMED_PRIORITY(priority);
			}
		}

		// field: position1
		if (valid) {
			valid = position1.empty() || utils::is_uuid(position1, true);
		}

		// field: position2
		if (valid) {
			valid = position2.empty() || utils::is_uuid(position2, true);
		}

		// field: var_at
		if (valid) {
			if (type == taplt_task::type_var) {
				valid = IS_VALID_APLT_TASK_VAR_AT(var_at);
			} else {
				valid = var_at == nposm;
			}
		}

		// field: zerotz_t
		if (valid) {
			if (type == taplt_task::type_timed) {
				valid = IS_VALID_APLT_TASK_TIMED_ZEROTZ_T(zerotz_t);
			} else {
				valid = zerotz_t == nposm;
			}
		}

		// field: timed_at
		if (valid) {
			if (type == taplt_task::type_timed) {
				valid = IS_VALID_APLT_TASK_TIMED_AT(timed_at);
			} else {
				valid = timed_at == nposm;
			}
		}

		// field: iot_src
		if (valid) {
			if (type == taplt_task::type_iot) {
				valid = IS_VALID_APLT_TASK_IOT_SRC(iot_src);
			} else {
				valid = iot_src == nposm;
			}
		}

		// field: evt
		if (valid) {
			if (type == taplt_task::type_iot) {
				valid = IS_VALID_APLT_TASK_IOT_SRC_EVT(src_evt);
			} else {
				valid = src_evt == nposm;
			}
		}

		// field: src_device_id
		if (valid) {
			if (type == taplt_task::type_iot) {
				valid = !src_device_id.empty();
			} else {
				valid = src_device_id.empty();
			}
		}

		// field: speech_id
		if (valid) {
			if (type == taplt_task::type_speech) {
				valid = !speech_id.empty();
				if (valid) {
					if (existed_speech_ids.count(speech_id) == 0) {
						existed_speech_ids.insert(speech_id);
					} else {
						valid = false;
					}
				}
			} else {
				valid = speech_id.empty();
			}
		}

		if (valid) {
			if (type == taplt_task::type_var) {
				if (var_tasks_.size() == MAX_KLINK_VARS) {
					valid = false;
				}
			}
		}

		if (valid) {
			std::pair<std::map<taplt_task_key, taplt_task>::iterator, bool> ins;
			if (type == taplt_task::type_iot) {
				ins = iot_tasks_.insert(std::make_pair(taplt_task_key(iot_src, src_evt, src_device_id),
					aplt::taplt_task(priority, aplt_id, task_id, ble_device_id, position1, position2, state, at, iot_src, src_evt, src_device_id)));

			} else if (type == taplt_task::type_speech) {
				ins = speech_tasks_.insert(std::make_pair(taplt_task_key(speech_id),
					aplt::taplt_task(priority, aplt_id, task_id, ble_device_id, position1, position2, state, at, speech_id)));

			} else if (type == taplt_task::type_var) {
				const int var_at2 = var_tasks_.size();
				int aux_key_id = next_aux_key_id(var_tasks_);
				ins = var_tasks_.insert(std::make_pair(taplt_task_key(aux_key_id),
					aplt::taplt_task(aplt_id, task_id, position1, position2, state, at, var_at2, aux_key_id)));

			} else {
				VALIDATE(type == taplt_task::type_timed, null_str);
				int aux_key_id = next_aux_key_id(timed_tasks_);
				const int timed_at2 = timed_tasks_.size();
				ins = timed_tasks_.insert(std::make_pair(taplt_task_key(aplt_id, task_id, ble_device_id, aux_key_id),
					aplt::taplt_task(aplt_id, task_id, ble_device_id, position1, position2, state, at, zerotz_t, timed_at2, aux_key_id)));

			}
			valid = ins.second;
		}

		if (valid) {
			at ++;

		} else {
			if (type == taplt_task::type_iot) {
				pb_klink_.mutable_iot_tasks()->DeleteSubrange(at, 1);
				size = pb_klink_.iot_tasks_size();

			} else if (type == taplt_task::type_speech) {
				pb_klink_.mutable_speech_tasks()->DeleteSubrange(at, 1);
				size = pb_klink_.speech_tasks_size();

			} else if (type == taplt_task::type_var) {
				pb_klink_.mutable_var_tasks()->DeleteSubrange(at, 1);
				size = pb_klink_.var_tasks_size();

			} else {
				VALIDATE(type == taplt_task::type_timed, null_str);
				pb_klink_.mutable_timed_tasks()->DeleteSubrange(at, 1);
				size = pb_klink_.timed_tasks_size();

			}
			continue;
		}
	}
}

void tbg_task::app_load_pb(int type)
{
	VALIDATE(pb_klink_type_ == nposm, null_str);
	pb_klink_type_ = type;
	protobuf::load_sha1pb(type, true);
	const int timing_pb_version = 1;
	if (pb_klink_.version() != timing_pb_version) {
		pb_klink_.set_version(timing_pb_version);
		// pb_klink_.set_timing_ts(0);
		// if (pb_klink_.logs_size() > 0) {
		//	pb_klink_.mutable_logs()->DeleteSubrange(0, pb_klink_.logs_size());
		// }
	}

	std::string scene_name = pb_klink_.scene_name();
	if (scene_name.empty()) {
		scene_name = _("Household");
	}
	scene_name_ = utils::truncate_to_max_bytes(scene_name.c_str(), scene_name.size(), MAX_KLINK_SCENE_NAME_BYTES);

	load_pb_tasks2(taplt_task::type_iot);
	load_pb_tasks2(taplt_task::type_speech);
	load_pb_tasks2(taplt_task::type_var);
	load_pb_tasks2(taplt_task::type_timed);

	verify_4tasks();

	std::set<std::string> existed_bundleids;
	int size = pb_klink_.iot_devices_size();
	for (int at = 0; at < size; ) {
		pb2::tiot_device* pb_pin = pb_klink_.mutable_iot_devices(at);

		const int src = pb_pin->src();
		const std::string device_id = pb_pin->device_id();
		const std::string alias = pb_pin->alias();
		const std::string icon = pb_pin->icon();
		int64_t ts = nposm; // pb_pin->ts();

		bool valid = IS_VALID_APLT_TASK_IOT_SRC(src);
		if (valid) {
			valid = !device_id.empty();
		}
		if (valid) {
			valid = alias.empty() || isvalid_normal_utf8_name224(alias);
		}

		if (valid) {
			iot_devices_.insert(std::make_pair(tiot_device_key(src, device_id), tiot_device(src, device_id, alias, icon, ts, at)));
			at ++;

		} else {
			pb_klink_.mutable_iot_devices()->DeleteSubrange(at, 1);
			size = pb_klink_.iot_devices_size();
			continue;
		}
	}
	verify_iot_devices();
}

void tbg_task::add_log2(int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent)
{
	std::string aplt_id;
	std::string task_id;
	std::string device_id;

	if (bg_task2_->aplt_task != nullptr) {
		aplt_id = bg_task2_->curr_aplt->id;
		task_id = bg_task2_->aplt_task->task_id;
		device_id = bg_task2_->aplt_task->ble_device_id;

	} else {
		VALIDATE(bg_task2_->in_task_cpp(), null_str);
		aplt::ttask_pair task_cpp_pair = bg_task2_->task_cpp_pair();
		aplt_id = task_cpp_pair.aplt->id;
		task_id = task_cpp_pair.task->id;
	}

	add_log7(aplt_id, task_id, device_id, ts, msg, tokens, aiagent);
}

void tbg_task::add_log7(const std::string& aplt_id, const std::string& task_id, const std::string& device_id, int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent)
{
	instance->add_aplt_task_log(aplt_id, task_id, device_id, ts, msg, tokens, aiagent);
}


}