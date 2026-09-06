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

#define GETTEXT_DOMAIN "rose-lib"

#include "cfg_cpp_api_core.hpp"
// #include "game_config.hpp"

#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
// #include "gui/dialogs/messagefs.hpp"
#include "serialization/parser.hpp"
#include "chinese.hpp"
#include "base_driver_core.hpp"
// #include "speech_driver.hpp"
#include "base_instance.hpp"

using namespace std::placeholders;

std::map<int, tcfg_4field> klink_cfgs {
	{cfgtype_klink, tcfg_4field(cfgtype_klink, "klink_", "klink")},
	{cfgtype_task_cpp, tcfg_4field(cfgtype_task_cpp, "task_cpp_", "task_cpp")},
	{cfgtype_speech, tcfg_4field(cfgtype_speech, "speech_", "speech")}
};

bool is_valid_klink_cfg_name(const tcfg_4field& cfg_3field, const char* name)
{
	const std::string& prefix = cfg_3field.prefix;
	// klink_xxx.cfg
	if (name == nullptr) {
		return false;
	}
	int s = SDL_strlen(name);
	if (s <= (int)prefix.size() + 4) {
		return false;
	}
	if (SDL_strncmp(name, prefix.c_str(), prefix.size()) != 0) {
		return false;
	}

	if (SDL_strncmp(name + s - 4, ".cfg", 4) != 0) {
		return false;
	}
	return true;
}

std::string get_klink_cfg_dir(int type, bool preferences)
{
	VALIDATE(type >= 0 && type < klink_cfgtype_count, null_str);
	if (preferences) {
		return get_saves_dir();
	}
	return game_config::app_dir_root + "/cert/klink";
}

static bool did_walk_klink_cfg(const std::string& dir, const SDL_dirent2* dirent, const tcfg_4field& cfg_3field, std::set<std::string>& files, const std::string& root)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		bool insert = false;
		// const std::string name = utils::lowercase(dirent->name);
		if (cfg_3field.type == cfgtype_klink) {
			if (is_valid_klink_cfg_name(cfg_3field, dirent->name)) {
				insert = true;		
			}
		} else {
			insert = true;
		}
		if (insert) {
			files.insert(root + "/" + dirent->name);
		}
	}
	return true;
}

void collect_klink_cfg_files(int type, std::set<std::string>& files)
{
	VALIDATE(type == cfgtype_klink, null_str);
	const tcfg_4field& cfg_3field = klink_cfgs.find(type)->second;

	files.clear();

	for (int at = 0; at < 2; at ++) {
		const std::string saves_dir = get_klink_cfg_dir(type, at == 0? true: false);
		walk_dir(saves_dir, false, std::bind(&did_walk_klink_cfg, _1, _2, std::ref(cfg_3field), std::ref(files), std::ref(saves_dir)));
	}
}

namespace aplt {

bool tspeech_sensor::tvar::from_cfg(aplt::tpinyin& pinyin, int tone, bool eng_lowercase, const config& cfg, int at)
{
	const std::string cfg_name = cfg["name"].str();

	std::pair<std::string, std::string> pairs = utils::split_app_prefix_id(cfg_name);
	if (pairs.first.empty() || pairs.second.empty()) {
		return false;
	}

	if (!is_bundleid(pairs.first)) {
		return false;
	}

	aplt_id = pairs.first;
	name = pairs.second;
	optional = cfg["optional"].to_bool();
	first_words_is_prefix = cfg["first_words_is_prefix"].to_bool();
	if (at != 0 && first_words_is_prefix) {
		first_words_is_prefix = false;
	}

	const std::string prefix_words_str = cfg["prefix_words"].str();
	if (first_words_is_prefix) {
		if (!prefix_words_str.empty()) {
			// if fist_words_is_prefix is true, 'prefix_words' must be empty.
			return false;
		}
	} else {
		aplt::parse_py_words_from_cfg_str(pinyin, tone, eng_lowercase, prefix_words_str, prefix_words, py_prefix_words);
	}

	const std::string postfix_words_str = cfg["postfix_words"].str();
	aplt::parse_py_words_from_cfg_str(pinyin, tone, eng_lowercase, postfix_words_str, postfix_words, py_postfix_words);

	word_match_fields_from_cfg(pinyin, tone, eng_lowercase, cfg, major_word, py_major_word, minor_words, py_minor_words, strategy);

	return true;

}

void tspeech_sensor::tvar::to_cfg(config& cfg) const
{
	cfg.clear();

	VALIDATE(is_bundleid(aplt_id), null_str);
	VALIDATE(!name.empty(), null_str);
	cfg["name"] = name2();

	cfg["optional"].from_bool(optional);
	cfg["first_words_is_prefix"].from_bool(first_words_is_prefix);

	if (first_words_is_prefix) {
		cfg["prefix_words"].from_string(null_str, true);
	} else {
		cfg["prefix_words"].from_string(utils::join(prefix_words), true);
	}
	cfg["postfix_words"].from_string(utils::join(postfix_words), true);

	word_match_fields_to_cfg(major_word, minor_words, strategy, cfg);
}

tspeech_sensor::tspeech_sensor()
	: pinyin_(aplt::get_curr_pinyin())
, tone_(PINYIN_DEF_TONE)
, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
{
	clear();
}

bool tspeech_sensor::from_cfg(const config& cfg)
{
	clear();

	bool fail = false;
	id = cfg["id"].str();
	name = cfg["name"].str();

	countdown_s = cfg["countdown"].to_int();

	const std::string first_words_str = cfg["first_words"].str();
	aplt::parse_py_words_from_cfg_str(pinyin_, tone_, eng_lowercase_, first_words_str, first_words, py_first_words);

	word_match_fields_from_cfg(pinyin_, tone_, eng_lowercase_, cfg, major_word, py_major_word, minor_words, py_minor_words, strategy);

	int vars = 0;
	BOOST_FOREACH(const config::any_child& c, cfg.all_children_range()) {
		if (c.key == "var" && vars < MAX_SPEECH_SENSOR_VARS) {
			const config& var_cfg = c.cfg;
			tvar& var = mutable_var(vars);
			if (!var.from_cfg(pinyin_, tone_, eng_lowercase_, var_cfg, vars)) {
				fail = true;
				break;

			} else if (var.first_words_is_prefix) {
				// VALIDATE(var.prefix_words.empty() && var.py_prefix_words.empty(), null_str);
				VALIDATE(vars == 0, null_str);
				var.prefix_words = first_words;
				var.py_prefix_words = py_first_words;
			}
			vars ++;
		}
	}

	return !fail;
}

void tspeech_sensor::to_cfg(config& cfg) const
{
	cfg.clear();

	cfg["id"] = id;
	cfg["name"] = name;
	cfg["countdown"].from_int(countdown_s);

	cfg["first_words"].from_string(utils::join(first_words), true);

	word_match_fields_to_cfg(major_word, minor_words, strategy, cfg);

	const int valid_vars2 = valid_vars();
	for (int at = 0; at < valid_vars2; at ++) {
		const tvar& tmp_var = var(at);
		config& var_cfg = cfg.add_child("var");
		tmp_var.to_cfg(var_cfg);
	}
}

void tspeech_sensor::green()
{
	const int valid_vars2 = valid_vars();
	for (int at = 0; at < valid_vars2; at ++) {
		tvar& tmp_var = mutable_var(at);
		if (tmp_var.first_words_is_prefix) {
			tmp_var.prefix_words = first_words;
			tmp_var.py_prefix_words = py_first_words;
		}
	}
}

std::string tspeech_sensor::py_from_utf8str(const std::string& str, tint32data_C* pos_data) const
{ 
	return pinyin_.from_utf8str(str, tone_, eng_lowercase_, pos_data, true);
}

void evaluate_task_pairs(const std::map<std::string, aplt::ttask_cpp_pair>& from, std::map<std::string, aplt::ttask_cpp_pair>& to)
{
	to.clear();
	std::copy(from.begin(), from.end(), std::inserter(to, to.end()));
}

void evaluate_speech_sensors(const std::string& from_nick, const std::map<std::string, aplt::tspeech_sensor>& from, std::string& to_nick, std::map<std::string, aplt::tspeech_sensor>& to)
{
	to_nick = from_nick;
	to.clear();
	std::copy(from.begin(), from.end(), std::inserter(to, to.end()));
}

tcfg_cpp_api_core::tcfg_cpp_api_core(const std::map<aplt::taplt_key, aplt::tapplet>& applets,
	aplt::tbg_task& bg_task, tbase_driver_core& base_driver)
	: applets_(applets)
/*
	, curmap_(curmap)
*/
	, bg_task_(bg_task)
	, base_driver_(base_driver)
/*
	, speech_driver_(speech_driver)
*/
	, add_timed_task_20sec_handled_(false)
	, add_timed_tasks_last_shedule_zerotz_t_(nposm)
	, pinyin_(aplt::get_curr_pinyin())
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
{}

tcfg_cpp_api_core::~tcfg_cpp_api_core()
{
	VALIDATE(taskpoint_.get() == nullptr, null_str);
}

void tcfg_cpp_api_core::load()
{
	//
	// task_cpp cfg
	//
	std::string stream = bg_task_.pb_klink().task_cpp_cfg();
	string_to_task_pairs(stream, task_cpp_pairs_);

	VALIDATE(fake_aplt.tasks.size() == fake_fixed_tasks.size(), null_str);
	sync_fake_aplt_tasks(task_cpp_pairs_);

	// buildin
	const int max_cfg_size = 75 * 1024; // 50K bytes
	std::string filename = game_config::app_dir_root + "/cert/klink/task_cpp_doll(zh_CN).cfg";
	SDL_Log("load task cpp pairs from: %s", filename.c_str());
	{
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize > 0 && fsize <= max_cfg_size && utils::is_utf8str(file.data, fsize)) {
			stream.assign(file.data, fsize);
			string_to_task_pairs(stream, buildin_task_cpp_pairs_);
		}
	}

	//
	// trigger cfg
	//
	stream = bg_task_.pb_klink().speech_sensor_cfg();
	string_to_speech_sensors(stream, nick_, speech_sensors_);

	// buildin
	filename = game_config::app_dir_root + "/cert/klink/speech_doll(zh_CN).cfg";
	{
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize > 0 && fsize <= max_cfg_size && utils::is_utf8str(file.data, fsize)) {
			stream.assign(file.data, fsize);
			std::string nick;
			string_to_speech_sensors(stream, nick, buildin_speech_sensors_);
		}
	}

	//
	// var_sensor cfg
	//
	stream = bg_task_.pb_klink().var_sensor_cfg();
	string_to_var_sensors(stream, true, var_sensors_);

	// buildin
	filename = game_config::app_dir_root + "/cert/klink/klink_lamp(default)(zh_CN).cfg";
	{
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize > 0 && fsize <= max_cfg_size && utils::is_utf8str(file.data, fsize)) {
			stream.assign(file.data, fsize);
			std::string nick;
			string_to_var_sensors(stream, false, buildin_var_sensors_);
		}
	}

	//
	// timed_sensor cfg
	//
	stream = bg_task_.pb_klink().timed_sensor_cfg();
	string_to_timed_sensors(stream, true, timed_sensors_);

	//
	// base_scene cfg
	//
	stream = bg_task_.pb_klink().base_scene_cfg();
	string_to_base_scenes(stream, base_scenes_);

	//
	// add_timed_task cfg
	//
	stream = bg_task_.pb_klink().add_timed_task_cfg();
	string_to_add_timed_tasks(stream, add_timed_tasks_);

	//
	// courselist cfg
	//
	stream = bg_task_.pb_klink().courselist_cfg();
	string_to_courselist(stream, courselist_);

	app_load();
}

bool tcfg_cpp_api_core::fake_aplt_tasks_dirty(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs)
{
	if (fake_aplt.tasks.size() != aplt::fake_fixed_tasks.size() + task_pairs.size()) {
		return true;
	}

	const tapplet& verbose_aplt = fake_aplt;
	for (std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = task_pairs.begin(); it != task_pairs.end(); ++ it) {
		const aplt::ttask_cpp_pair& pair = it->second;

		if (fake_aplt.tasks.count(pair.id) == 0) {
			return true;
		}

		const tapplet::ttask& that_task = fake_aplt.tasks.find(pair.id)->second;
		if (that_task.name != pair.name) {
			return true;
		}
	}
	return false;
}

void tcfg_cpp_api_core::sync_fake_aplt_tasks(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs)
{
	VALIDATE_IN_MAIN_THREAD();

	aplt::tapplet& fake_aplt2 = fake_aplt;

	if (bg_task_.is_ing()) {
		VALIDATE(bg_task_.task_cpp_aplt(nullptr) != &fake_aplt2, null_str);
	}

	if (!fake_aplt_tasks_dirty(task_pairs)) {
		// sync @recoverable, @nonpreemptive
		for (std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = task_pairs.begin(); it != task_pairs.end(); ++ it) {
			const aplt::ttask_cpp_pair& pair = it->second;

			VALIDATE(fake_aplt2.tasks.count(pair.id) != 0, null_str);

			tapplet::ttask& cfg_task = fake_aplt2.tasks.find(pair.id)->second;
			if (cfg_task.recoverable != pair.recoverable) {
				cfg_task.recoverable = pair.recoverable;
			}
			if (cfg_task.nonpreemptive != pair.nonpreemptive) {
				cfg_task.nonpreemptive = pair.nonpreemptive;
			}
		}
		return;
	}

	fake_aplt_erase_nonfixed_tasks();

	std::set<per_t> permissions;
	fill_per_if_necessary(permissions);

	for (std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = task_pairs.begin(); it != task_pairs.end(); ++ it) {
		const aplt::ttask_cpp_pair& pair = it->second;
		VALIDATE(fake_fixed_tasks.count(pair.id) == 0, null_str);

		int max_fails = 1;
		std::vector<tapplet::tvar> vars;
		std::pair<std::map<std::string, tapplet::ttask>::iterator, bool> ins = fake_aplt2.tasks.insert(std::make_pair(pair.id, 
			tapplet::ttask(pair.id, task_cpp, nposm, pair.nonpreemptive, pair.recoverable, nposm, permissions, max_fails, vars, false, false)));

		tapplet::ttask& task = ins.first->second;
		task.name = pair.name;
		task.py_name = chinese::curr_pinyin.from_utf8str2(task.name, tone_, eng_lowercase_);
	}
}

void tcfg_cpp_api_core::save_task_pairs(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs)
{
	evaluate_task_pairs(task_pairs, task_cpp_pairs_);
	save_klink_pb(aplt::tbg_task::misc_cfg_task_cpp);
}

void tcfg_cpp_api_core::did_gui2task2_back()
{
	std::map<std::string, aplt::ttask_cpp_pair>& task_pairs = task_cpp_pairs_;
	for (std::map<std::string, aplt::ttask_cpp_pair>::iterator it = task_pairs.begin(); it != task_pairs.end(); ++ it) {
		aplt::ttask_cpp_pair& pair = it->second;
		pair.green();
	}

	sync_fake_aplt_tasks(task_pairs);
}

// false: parse config fail.
// true: success. but result maybe is empty when stream is empty.
bool tcfg_cpp_api_core::string_to_task_pairs(const std::string& stream, std::map<std::string, aplt::ttask_cpp_pair>& result) const
{
	if (game_config::os == os_windows) {
		// ::write_file(game_config::preferences_dir + "/1.cfg", stream.c_str(), stream.size());
	}

	VALIDATE(result.empty(), null_str);

	config root_cfg;
	if (!read_config_ex(stream, true, root_cfg)) {
		return false;
	}

	return cfg_to_task_pairs(root_cfg, result);
}

bool tcfg_cpp_api_core::cfg_to_task_pairs(const config& root_cfg, std::map<std::string, aplt::ttask_cpp_pair>& result) const
{
	std::string err_msg;
	BOOST_FOREACH(const config::any_child &task_cpp, root_cfg.all_children_range()) {
		if (task_cpp.key == "task_cpp") {
			const std::string id = task_cpp.cfg["id"].str();
			if (id.empty()) {
				continue;
			}
			std::pair<std::map<std::string, aplt::ttask_cpp_pair>::iterator, bool> ins = 
				result.insert(std::make_pair(id, aplt::ttask_cpp_pair()));

			aplt::ttask_cpp_pair& pair = ins.first->second;
			bool valid = pair.from_cfg(task_cpp.cfg);
			if (valid) {
				valid = app_task_cpp_is_valid(pair, err_msg) == TCOOKIE3F_CHECK_OK;
			}
			if (!valid) {
				result.erase(ins.first);
			}
		}
	}

	return true;
}

bool tcfg_cpp_api_core::string_to_speech_sensors(const std::string& stream, std::string& nick, std::map<std::string, aplt::tspeech_sensor>& result) const
{
	VALIDATE(result.empty(), null_str);

	config root_cfg;
	if (!read_config_ex(stream, true, root_cfg)) {
		return false;
	}

	return cfg_to_speech_sensors(root_cfg, nick, result);
}

bool tcfg_cpp_api_core::cfg_to_speech_sensors(const config& root_cfg, std::string& nick, std::map<std::string, aplt::tspeech_sensor>& result) const
{
	std::set<std::string> existed_ids;
	std::set<std::string> existed_names;
	std::string err_msg;
	nick = root_cfg["nick"].str();
	BOOST_FOREACH(const config::any_child &speech_sensor, root_cfg.all_children_range()) {
		if (speech_sensor.key == "speech_sensor") {
			const std::string id = speech_sensor.cfg["id"].str();
			if (id.empty() || existed_ids.count(id) != 0) {
				continue;
			}

			std::pair<std::map<std::string, aplt::tspeech_sensor>::iterator, bool> ins = 
				result.insert(std::make_pair(id, aplt::tspeech_sensor()));

			aplt::tspeech_sensor& sensor = ins.first->second;
			bool valid = sensor.from_cfg(speech_sensor.cfg);
			if (valid) {
				if (existed_names.count(sensor.name) != 0) {
					valid = false;

				} else if (app_speech_sensor_is_valid(sensor, err_msg) != TCOOKIE3F_CHECK_OK) {
					valid = false;
				}
			}
			if (!valid) {
				result.erase(ins.first);

			} else {
				existed_ids.insert(id);
				existed_names.insert(sensor.name);
			}
		}
	}

	return true;
}

bool tcfg_cpp_api_core::string_to_var_sensors(const std::string& stream, bool sync_with_var_tasks, std::vector<tvar_sensor>& result) const
{
	VALIDATE(result.empty(), null_str);

	config root_cfg;
	if (!read_config_ex(stream, true, root_cfg)) {
		return false;
	}

	return cfg_to_var_sensors(root_cfg, sync_with_var_tasks, result);
}

bool tcfg_cpp_api_core::cfg_to_var_sensors(const config& root_cfg, bool sync_with_var_tasks, std::vector<tvar_sensor>& result) const
{	
	// sync bg_task_.var_tasks_ with var_sensors_
	// must make sure 'var_tasks_[at].var_at' == 'var_sensors_[at].at'
	const std::map<taplt_task_key, taplt_task>& var_tasks = bg_task_.var_tasks();
	std::map<taplt_task_key, taplt_task>::const_iterator task_it = var_tasks.begin();

	int at = 0;
	// if var_sensor.'at' is right, this var_sensor will not exist.
	BOOST_FOREACH(const config::any_child &var_sensor, root_cfg.all_children_range()) {
		if (var_sensor.key == "var_sensor") {
			if (sync_with_var_tasks) {
				if (at == (int)var_tasks.size()) {
					break;
				}
				const taplt_task& task = task_it->second;
				VALIDATE(task.var_at == at, null_str);
			}

			result.push_back(tvar_sensor());
			tvar_sensor& sensor = result.back();

			sensor.at = var_sensor.cfg["at"].to_int(nposm);
			sensor.name = var_sensor.cfg["name"].str();
			const std::string sync_vars_str = var_sensor.cfg["sync_vars"].str();
			if (!sync_vars_str.empty()) {
				std::vector<std::string> v_str = utils::split(sync_vars_str);
				for (std::vector<std::string>::const_iterator it = v_str.begin(); it != v_str.end(); ++ it) {
					const std::string& var_name = *it;
					std::pair<std::string, std::string> pair = utils::split_app_prefix_id(var_name);
					if (pair.first != fake_aplt.bundleid) {
						continue;
					}
					if (pair.second.empty()) {
						continue;
					}
					sensor.sync_vars.insert(var_name);
				}
			}
			if (sync_with_var_tasks) {
				if (sensor.at == at) {
					if (!sensor.if_block.from_cfg("if_block", 0, var_sensor.cfg)) {
						sensor.if_block.clear();
					}
				} else {
					sensor.at = at;
				}
				at ++;
				++ task_it;

			} else {
				sensor.at = nposm;
				bool fail = sensor.name.empty();
				if (!fail) {
					if (!sensor.if_block.from_cfg("if_block", 0, var_sensor.cfg)) {
						fail = true;
					}
				}
				if (fail) {
					std::vector<tvar_sensor>::iterator erase_it = result.begin();
					if (result.size() != 1) {
						std::advance(erase_it, result.size() - 1);
					}
					result.erase(erase_it);
				}
			}
		}
	}

	if (sync_with_var_tasks && var_tasks.size() != result.size()) {
		VALIDATE(var_tasks.size() >= result.size(), null_str);
		while (var_tasks.size() != result.size()) {
			result.push_back(tvar_sensor());
			tvar_sensor& sensor = result.back();
			sensor.at = result.size() - 1;
		}
		verity_var_sensors();
	}

	return true;
}

bool tcfg_cpp_api_core::string_to_base_scenes(const std::string& stream, std::vector<tbase_scene>& result) const
{
	VALIDATE(result.empty(), null_str);

	config root_cfg;
	if (!read_config_ex(stream, true, root_cfg)) {
		return false;
	}

	return cfg_to_base_scenes(root_cfg, result);
}

bool tcfg_cpp_api_core::cfg_to_base_scenes(const config& root_cfg, std::vector<tbase_scene>& result) const
{
	int at = 0;
	std::set<std::string> exist_ids;
	std::set<std::string> exist_names;
	BOOST_FOREACH(const config::any_child &base_scene, root_cfg.all_children_range()) {
		if (base_scene.key == "base_scene") {
			std::string id = base_scene.cfg["id"].str();
			if (!isvalid_normal_id_or_var_name224(id)) {
				continue;
			}
			if (exist_ids.count(id) != 0) {
				continue;
			}
			const std::string name = base_scene.cfg["name"].str();
			if (!isvalid_normal_utf8_name224(name)) {
				continue;
			}
			if (exist_names.count(name) != 0) {
				continue;
			}
			const std::string aplt = base_scene.cfg["aplt"].str();
			if (!utils::is_rose_bundleid(aplt, '.')) {
				continue;
			}
			const std::string task = base_scene.cfg["task"].str();
			if (task.empty()) {
				continue;
			}
			result.push_back(tbase_scene());
			tbase_scene& scene = result.back();
			scene.id = id;
			scene.set_name(name);
			scene.aplt = aplt;
			scene.task = task;
			scene.amp = amp_mode_from_str(base_scene.cfg["amp"].str(), true);
			VALIDATE(scene.amp >= 0 && scene.amp < ampmode_count, null_str);

			const config& ipupt_vars_cfg = base_scene.cfg.child("input_vars");
			if (ipupt_vars_cfg) {
				for (const config::attribute &v: ipupt_vars_cfg.attribute_range()) {
					const std::string& key = v.first;
					const std::string& val = v.second;
					scene.input_vars.insert(std::make_pair(key, val));
				}
			}

			exist_ids.insert(id);
			exist_names.insert(name);
		}
	}

	return true;
}

void tcfg_cpp_api_core::task_pairs_to_stringstream(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs, std::stringstream& result) const
{
	result.str("");
	config top_cfg;

	const tcfg_4field& cfg_3field = klink_cfgs.find(cfgtype_task_cpp)->second;
	top_cfg["type"] = cfg_3field.id;

	for (std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = task_pairs.begin(); it != task_pairs.end(); ++ it) {
		const aplt::ttask_cpp_pair& pair = it->second;
		config& pair_cfg = top_cfg.add_child("task_cpp");
		pair.to_cfg(pair_cfg);
	}

	if (!top_cfg.empty()) {
		write(result, top_cfg);
	}
}

void tcfg_cpp_api_core::speech_sensors_to_stringstream(const std::string& nick, const std::map<std::string, aplt::tspeech_sensor>& sensors, std::stringstream& result) const
{
	result.str("");
	config top_cfg;

	const tcfg_4field& cfg_3field = klink_cfgs.find(cfgtype_speech)->second;
	top_cfg["type"] = cfg_3field.id;

	top_cfg["nick"].from_string(nick, true);

	for (std::map<std::string, aplt::tspeech_sensor>::const_iterator it = sensors.begin(); it != sensors.end(); ++ it) {
		const aplt::tspeech_sensor& sensor = it->second;
		config& sensor_cfg = top_cfg.add_child("speech_sensor");
		sensor.to_cfg(sensor_cfg);
	}

	if (!top_cfg.empty()) {
		write(result, top_cfg);
	}
}

void tcfg_cpp_api_core::save_speech_sensors(const std::string& nick, std::map<std::string, aplt::tspeech_sensor>& speech_sensors)
{
	aplt::evaluate_speech_sensors(nick, speech_sensors, nick_, speech_sensors_);
	save_klink_pb(aplt::tbg_task::misc_cfg_speech_sensor);
}

void tcfg_cpp_api_core::var_sensors_to_cfg(const std::vector<tvar_sensor>& sensors, config& cfg) const
{
	// cfg["nick"].from_string(nick, true);

	int at = 0;
	for (std::vector<tvar_sensor>::const_iterator it = sensors.begin(); it != sensors.end(); ++ it, at ++) {
		const tvar_sensor& sensor = *it;
		VALIDATE(sensor.at == at, null_str);
		config& sensor_cfg = cfg.add_child("var_sensor");
		sensor_cfg["at"].from_int(sensor.at);
		sensor_cfg["name"].from_string(sensor.name, true);
		sensor_cfg["sync_vars"].from_string(utils::join(sensor.sync_vars), true);
		sensor.if_block.to_cfg("if_block", 0, sensor_cfg);
	}
}

void tcfg_cpp_api_core::var_sensors_to_stringstream(const std::vector<tvar_sensor>& sensors, std::stringstream& result) const
{
	result.str("");
	config cfg;

	var_sensors_to_cfg(sensors, cfg);

	if (!cfg.empty()) {
		write(result, cfg);
	}
}

void tcfg_cpp_api_core::base_scenes_to_cfg(const std::vector<tbase_scene>& scenes, config& cfg) const
{
	for (std::vector<tbase_scene>::const_iterator it = scenes.begin(); it != scenes.end(); ++ it) {
		const tbase_scene& scene = *it;

		scene.validate();

		config& sensor_cfg = cfg.add_child("base_scene");
		sensor_cfg["id"].from_string(scene.id, true);
		sensor_cfg["name"].from_string(scene.name(), true);
		sensor_cfg["aplt"].from_string(scene.aplt, true);
		sensor_cfg["task"].from_string(scene.task, true);
		VALIDATE(scene.amp >= 0 && scene.amp < ampmode_count, null_str);
		if (scene.amp != ampmode_1x) {
			sensor_cfg["amp"].from_string(aplt::amp_modes.find(scene.amp)->second.id, true);
		}

		if (scene.input_vars.empty()) {
			continue;
		}
		config& input_vars_cfg = sensor_cfg.add_child("input_vars");
		for (std::map<std::string, std::string>::const_iterator it = scene.input_vars.begin(); it != scene.input_vars.end(); ++ it) {
			const std::string& key = it->first;
			const std::string& val = it->second;
			VALIDATE(!key.empty(), null_str);
			input_vars_cfg[key].from_string(val, true);
		}
	}
}

void tcfg_cpp_api_core::base_scenes_to_stringstream(const std::vector<tbase_scene>& scenes, std::stringstream& result) const
{
	result.str("");
	config cfg;

	base_scenes_to_cfg(scenes, cfg);

	if (!cfg.empty()) {
		write(result, cfg);
	}
}

void tcfg_cpp_api_core::save_klink_pb(int type)
{
	std::stringstream out;

	if (type == tbg_task::misc_cfg_speech_sensor) {
		speech_sensors_to_stringstream(nick_, speech_sensors_, out);

	} else if (type == tbg_task::misc_cfg_var_sensor) {
		var_sensors_to_stringstream(var_sensors_, out);

	} else if (type == tbg_task::misc_cfg_timed_sensor) {
		timed_sensors_to_stringstream(timed_sensors_, out);

	} else if (type == tbg_task::misc_cfg_task_cpp) {
		task_pairs_to_stringstream(task_cpp_pairs_, out);
		bg_task_.apltnotfound_or_verdismatch_to_fresh(false);
	
	} else if (type == tbg_task::misc_cfg_base_scene) {
		base_scenes_to_stringstream(base_scenes_, out);

	} else if (type == tbg_task::misc_cfg_add_timed_task) {
		add_timed_tasks_to_stringstream(add_timed_tasks_, out);

	} else {
		VALIDATE(type == tbg_task::misc_cfg_courselist, null_str);
		courselist_to_stringstream(courselist_, out);
	}

	bg_task_.modify_misc_cfg(type, out.str());
}
/*
std::string tcfg_cpp_api::app_start_task(const ttaskpoint* taskpoint)
{
	if (speech_driver_.installed()) {
		VALIDATE(!speech_driver_.get_visual_info().allow_short_voice, null_str);
		speech_driver_.set_allow_short_voice(true);
	}

	const std::map<aplt::taplt_key, aplt::tapplet>& applets = b_api_.const_applets();

	VALIDATE(task_cpp_pairs_.count(task_id_) != 0, null_str);

	const aplt::ttask_cpp_pair& cpp_pair = task_cpp_pairs_.find(task_id_)->second;
	reception_state_ = cpp_pair.reception_state;
	recoverable_ = cpp_pair.recoverable;
	nonpreemptive_ = cpp_pair.nonpreemptive;
	VALIDATE(startup_state_.branches.empty(), null_str);
	if (taskpoint == nullptr) {
		startup_state_ = cpp_pair.startup_state;

	} else {
		tif_branch branch;
		branch.do_to_state = taskpoint->state;
		startup_state_.branches.push_back(branch);
	}
	states_ = cpp_pair.states;
	key_2_states_ = cpp_pair.key_2_states;

	std::string err_msg;
	utils::string_map symbols;
	symbols["task"] = cpp_pair.name;
	for (std::map<int, tcpp_api::tstate2>::iterator it = states_.begin(); it != states_.end(); ++ it) {
		tcpp_api::tstate2& state2 = it->second;
		if (state2.async_task.valid()) {
			VALIDATE(!state2.async_task.aplt_id.empty(), null_str);
			VALIDATE(!state2.async_task.task_id.empty(), null_str);

			// both @aplt and @task aren't nullptr, see 'tbg_task2::req_task_can_start'
			const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets, state2.async_task.aplt_id);
			VALIDATE(aplt != nullptr, null_str);

			VALIDATE(aplt->tasks.count(state2.async_task.task_id) != 0, null_str);
			const tapplet::ttask* task = &aplt->tasks.find(state2.async_task.task_id)->second;

			state2.async_task.aplt = aplt;
			state2.async_task.task = task;
		}

		symbols["state"] = cpp_pair.state_names[state2.state];
		if (!state2.async_task.position1_uuid.empty()) {
			symbols["position"] = state2.async_task.position1_uuid;
			if (curmap_.positions.count(state2.async_task.position1_uuid) == 0) {
				err_msg = vgettext2("Start task '$task' fail. State '$state' needs to use '$position', but isn't existed in cur map.", symbols);
				break;
			}
		}
		if (!state2.async_task.position2_uuid.empty()) {
			symbols["position"] = state2.async_task.position2_uuid;
			if (curmap_.positions.count(state2.async_task.position2_uuid) == 0) {
				err_msg = vgettext2("Start task '$task' fail. State '$state' needs to use '$position', but isn't existed in cur map.", symbols);
				break;
			}
		}
	}

	if (!err_msg.empty()) {
		// clear_2th();
	}

	return err_msg;
}

void tcfg_cpp_api::app_task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task, bool result)
{
	if (speech_driver_.installed()) {
		speech_driver_.set_allow_short_voice(false);
	}
}

size_t hit_task_from_pinyin(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const std::string& pinyin, const size_t off, bool allow_cpp, ttask_pair& pair)
{
	pair.aplt = nullptr;
	pair.task = nullptr;
	size_t task_off = nposm;
	if (allow_cpp) {
		const aplt::tapplet& bonus_aplt = aplt::fake_aplt;
		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it = bonus_aplt.tasks.begin(); it != bonus_aplt.tasks.end(); ++ it) {
			const aplt::tapplet::ttask& task = it->second;

			size_t pos = pinyin.find(task.py_name, off);
			// if (pos != std::string::npos) {
			if (pos == off) {
				pair.aplt = &bonus_aplt;
				pair.task = &task;
				task_off = pos + task.py_name.size();
				return task_off;
			}
		}
	}
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets.begin(); pair.task == nullptr && it != applets.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it2 = aplt.tasks.begin(); it2 != aplt.tasks.end(); ++ it2) {
			const aplt::tapplet::ttask& task = it2->second;
			if (!allow_cpp && task.type == task_cpp) {
				continue;
			}
			size_t pos = pinyin.find(task.py_name, off);
			// if (pos != std::string::npos) {
			if (pos == off) {
				pair.aplt = &aplt;
				pair.task = &task;
				task_off = pos + task.py_name.size();
				break;
			}
		}
	}
	return task_off;
}

const aplt::taplt_task* tcfg_cpp_api_core::find_matched_speech_sensor(const std::string& result, 
	std::map<std::string, std::string>& matched_vals, const aplt::tspeech_sensor** matched_sensor, bool& nick_found) const
{
	matched_vals.clear();
	*matched_sensor = nullptr;
	nick_found = false;

	struct tpy_val
	{
		tpy_val()
			: is_py(false)
			, start(nposm)
			, end(nposm)
		{}

		void set_py(const std::string& _name, const std::string& _val, int _start, int _end)
		{
			VALIDATE(!_name.empty(), null_str);
			name = _name;
			VALIDATE(!_val.empty(), null_str);
			val = _val;

			is_py = true;
			start = _start;
			end = _end;
		}

		void set_str(const std::string& _name, const std::string& _val)
		{
			VALIDATE(!_name.empty(), null_str);
			name = _name;
			VALIDATE(!_val.empty(), null_str);
			val = _val;

			is_py = false;
			start = nposm;
			end = nposm;
		}

		std::string name;
		std::string val;
		bool is_py;
		int start; // pinyin[start] is first byte of this val.
		int end; // pinyin[end] is first byte of next val.
	};
	std::vector<tpy_val> py_vals;

	const std::string pinyin = pinyin_.from_utf8str2(result, tone_, eng_lowercase_);
	if (pinyin.empty()) {
		return nullptr;
	}

	// must start with 'nick'
	int text_offset = 0;
	if (!nick_.empty()) {
		const std::string nick_pinyin = pinyin_.from_utf8str2(nick_, tone_, eng_lowercase_);
		if (pinyin.find(nick_pinyin) != 0) {
			return nullptr;
		}
		text_offset = nick_pinyin.size();
		nick_found = true;
	}

	const std::map<taplt_task_key, taplt_task>& speech_tasks = bg_task_.get_tasks(taplt_task::type_speech);
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = speech_tasks.begin(); it != speech_tasks.end(); ++ it) {
		const taplt_task& aplt_task = it->second;
		if (speech_sensors_.count(aplt_task.speech_id) == 0) {
			continue;
		}
		const aplt::tspeech_sensor& sensor = speech_sensors_.find(aplt_task.speech_id)->second;
		int offset = text_offset;
		
		// 1/2)first_words
		if (!sensor.first_words.empty()) {
			bool found = false;
			for (std::vector<std::string>::const_iterator it = sensor.py_first_words.begin(); it != sensor.py_first_words.end(); ++ it) {
				const std::string& word = *it;
				size_t pos = pinyin.find(word, offset);
				if (pos == offset) {
					offset += word.size();
					found = true;
					break;
				}
			}
			if (!found) {
				continue;
			}
		}

		bool matched = true;
		const int valid_vars = sensor.valid_vars();
		if (!sensor.py_major_word.empty() || !sensor.py_minor_words.empty()) {
			// 2.1/2)use major_key, minor_keys, strategy
			VALIDATE(valid_vars == 0, null_str);
			const std::string new_pinyin = pinyin.substr(offset);
			matched = major_minor_words_match(new_pinyin, sensor.py_major_word, sensor.py_minor_words, sensor.strategy, nullptr);
		} else {
			// 2.2/2)use vars
			VALIDATE(valid_vars > 0, null_str);
			bool end = false;
			bool fail = false;
			for (int at = 0; at < valid_vars; at ++) {
				VALIDATE(!end && !fail, null_str);
				const tspeech_sensor::tvar& var = sensor.var(at);
				bool found = false;
				// prefix_words
				if ((at != 0 || !var.first_words_is_prefix) && !var.prefix_words.empty()) {
					for (std::vector<std::string>::const_iterator it = var.py_prefix_words.begin(); it != var.py_prefix_words.end(); ++ it) {
						const std::string& word = *it;
						size_t pos = pinyin.find(word, offset);
						if (pos == std::string::npos) {
							// cannot find prefix

						} else {
							offset = pos + word.size();
							found = true;
							break;
						}
					}
				} else if (offset < (int)pinyin.size()) {
					found = true;
				}

				if (!found) {
					end = true;
					if (!var.optional) {
						fail = true;
					}
					break;
				}
				// postfix_words
				found = false;
				// std::string maybe_var_py_val;
				tpy_val maybe_var_py_val;
				int end_pos = nposm;
				if (!var.postfix_words.empty()) {
					for (std::vector<std::string>::const_iterator it = var.py_postfix_words.begin(); it != var.py_postfix_words.end(); ++ it) {
						const std::string& word = *it;
						size_t pos = pinyin.find(word, offset);
						if (pos == std::string::npos) {
							// cannot find postfix.

						} else if (pos == offset) {
							// var must not be empty.
							// [0]hai2you3] [1][you3]
							// once 'hai2you3', break
							break;

						} else {
							maybe_var_py_val.set_py(var.name2(), pinyin.substr(offset, pos), offset, pos);
							end_pos = pos + word.size();
							found = true;
							break;
						}
					}
				} else if (offset < (int)pinyin.size()) {
					// The variable cannot be empty, 
					// which naturally requires that behind has more bytes.
					const bool log_buildin_pos = false;
					bool buildin = false;
					if (var.aplt_id == aplt::fake_aplt.bundleid) {
						if (var.name == "position" || var.name == "position2") {
							buildin = true;

							for (std::map<std::string, tmap_position>::const_iterator it = curmap_.positions.begin(); it != curmap_.positions.end(); ++ it) {
								const tmap_position& position = it->second;
								std::string name_pinyin = pinyin_.from_utf8str2(position.name, tone_, eng_lowercase_);
								size_t pos = std::string::npos;
								if (!name_pinyin.empty()) {
									pos = pinyin.find(name_pinyin, offset);
								}
								if (pos == offset) {
									const std::string& uuid = it->first;
									VALIDATE(uuid == position.uuid, null_str);

									end_pos = offset + name_pinyin.size();
									if (log_buildin_pos) {
										maybe_var_py_val.set_py(var.name2(), name_pinyin, offset, end_pos);
									} else {
										maybe_var_py_val.set_str(var.name2(), uuid);
									}
									found = true;
									break;
								}
							}
							

						} else if (var.name == "task") {
							buildin = true;

							ttask_pair hit_pair;
							size_t task_off = hit_task_from_pinyin(applets_, pinyin, offset, false, hit_pair);

							const aplt::tapplet::ttask* hit_task = hit_pair.task;
							const aplt::tapplet* hit_task_aplt = hit_pair.aplt;
							if (hit_task != nullptr) {
								VALIDATE(hit_task_aplt != nullptr, null_str);
								VALIDATE(task_off != nposm, null_str);

								if (log_buildin_pos) {
									maybe_var_py_val.set_py(var.name2(), hit_task->py_name, offset, task_off);
								} else {
									maybe_var_py_val.set_str(var.name2(), utils::join_app_prefix_id(hit_task_aplt->id, hit_task->id));
								}
								end_pos = task_off;
								found = true;
							}
						}
					}
					if (!buildin) {
						end_pos = (int)pinyin.size();
						maybe_var_py_val.set_py(var.name2(), pinyin.substr(offset), offset, end_pos);
						found = true;
					}
				}

				if (found && (!var.py_major_word.empty() || !var.py_minor_words.empty())) {
					VALIDATE(maybe_var_py_val.is_py, null_str);
					if (!major_minor_words_match(maybe_var_py_val.val, var.py_major_word, var.py_minor_words, var.strategy, nullptr)) {
						found = false;
					}
				}

				if (!found) {
					end = true;
					if (!var.optional) {
						fail = true;
					}
					break;
				}
				VALIDATE(end_pos != nposm && end_pos <= (int)pinyin.size(), null_str);
				offset = end_pos;
				py_vals.push_back(maybe_var_py_val);

				// -- for (int at = 0; at < valid_vars; at ++) {
			}
			if (fail) {
				VALIDATE(end, null_str);
				py_vals.clear();
			}
			matched = !py_vals.empty();

			// --2th: valid_vars > 0
		}

		if (matched) {
			if (!py_vals.empty()) {
				// change 'py_vals' to part that in result.
				tint32data_C pos_data;
				const std::string pinyin2 = sensor.py_from_utf8str(result, &pos_data);
				VALIDATE(pinyin2 == pinyin, null_str);

				VALIDATE(pos_data.ptr != nullptr, null_str);
				std::vector<int> set;
				if (game_config::os == os_windows) {
					for (int at = 0; at < pos_data.len; at ++) {
						set.push_back(pos_data.ptr[at]);
					}
				}

				int at = 0;
				SDL_Log("result: %s", result.c_str());
				for (std::vector<tpy_val>::const_iterator it = py_vals.begin(); it != py_vals.end(); ++ it, at ++) {
					const tpy_val& py_val = *it;
					if (py_val.is_py) {
						int first_char = nposm;
						int last_char_puls1 = nposm;

						for (int at = 0; at < pos_data.len; at ++) {
							if (py_val.start < pos_data.ptr[at]) {
								first_char = at;
								break;
							}
						}

						for (int at = pos_data.len - 1; at >= 0; at --) {
							if (py_val.end >= pos_data.ptr[at]) {
								last_char_puls1 = at + 1;
								break;
							}
						}
						VALIDATE(first_char != nposm && last_char_puls1 != nposm && last_char_puls1 > first_char, null_str);

						std::string span = utils::utf8str_substr(result, first_char, last_char_puls1 - first_char);
						VALIDATE(utils::is_utf8str(span.c_str(), span.size()), null_str);
						SDL_Log("#%i val: %s", at, span.c_str());
						matched_vals.insert(std::make_pair(py_val.name, span));

					} else {
						matched_vals.insert(std::make_pair(py_val.name, py_val.val));
					}
				}
				if (pos_data.ptr != nullptr) {
					free(pos_data.ptr);
				}
			}
			*matched_sensor = &sensor;
			return &aplt_task;
		}

	}
	return nullptr;
}
*/
void tcfg_cpp_api_core::verity_var_sensors() const
{
	const std::map<taplt_task_key, taplt_task>& var_tasks = bg_task_.var_tasks();
	VALIDATE(var_tasks.size() == var_sensors_.size(), null_str);

	int var_at = 0;
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = var_tasks.begin(); it != var_tasks.end(); ++ it) {
		const taplt_task& task = it->second;
		// var_taks is sorted by aux_key_id, not by var_at. task.var_at maybe not equal var_at.
		// VALIDATE (task.var_at == var_at, null_str);

		const tvar_sensor& sensor = var_sensors_[var_at];
		VALIDATE(sensor.at == var_at, null_str);

		var_at ++;
	}
}

void tcfg_cpp_api_core::insert_var_sensor(int new_var_at, int builtin_at, const std::string& name)
{
	VALIDATE(new_var_at == (int)var_sensors_.size(), null_str);
	if (builtin_at == nposm) {
		VALIDATE(!name.empty(), null_str);
		var_sensors_.push_back(aplt::tvar_sensor());
		var_sensors_.back().name = name;

	} else {
		VALIDATE(builtin_at >= 0 && builtin_at < (int)buildin_var_sensors_.size(), null_str);
		VALIDATE(name.empty(), null_str);
		var_sensors_.push_back(buildin_var_sensors_[builtin_at]);
	}
	var_sensors_.back().at = new_var_at;
	save_klink_pb(aplt::tbg_task::misc_cfg_var_sensor);

	verity_var_sensors();
}

void tcfg_cpp_api_core::erase_var_sensor(int at)
{
	VALIDATE(at >= 0 && at < (int)var_sensors_.size(), null_str);

	std::vector<tvar_sensor>::iterator erase_it = var_sensors_.begin();
	if (at != 0) {
		std::advance(erase_it, at);
	}
	var_sensors_.erase(erase_it);

	for (std::vector<tvar_sensor>::iterator it = var_sensors_.begin(); it != var_sensors_.end(); ++ it) {
		tvar_sensor& sensor = *it;
		// pb_at does not participate in sort, modify it here, it should be safe
		if (sensor.at > at) {
			sensor.at --;
		}
	}
	save_klink_pb(aplt::tbg_task::misc_cfg_var_sensor);

	verity_var_sensors();
}

void tcfg_cpp_api_core::set_var_sensors(const std::vector<tvar_sensor>& var_sensors)
{
	var_sensors_ = var_sensors;
	save_klink_pb(aplt::tbg_task::misc_cfg_var_sensor);

	verity_var_sensors();
}

//
// timed sensor
//
bool tcfg_cpp_api_core::string_to_timed_sensors(const std::string& stream, bool sync_with_timed_tasks, std::vector<ttimed_sensor>& result) const
{
	VALIDATE(result.empty(), null_str);

	config root_cfg;
	if (!read_config_ex(stream, true, root_cfg)) {
		return false;
	}

	return cfg_to_timed_sensors(root_cfg, sync_with_timed_tasks, result);
}

bool tcfg_cpp_api_core::cfg_to_timed_sensors(const config& root_cfg, bool sync_with_timed_tasks, std::vector<ttimed_sensor>& result) const
{	
	// sync bg_task_.timed_tasks_ with timed_sensors_
	// must make sure 'timed_tasks_[at].timed_at' == 'timed_sensors_[at].at'
	const std::map<taplt_task_key, taplt_task>& timed_tasks = bg_task_.timed_tasks();
	std::map<taplt_task_key, taplt_task>::const_iterator task_it = timed_tasks.begin();

	int at = 0;
	// if timed_sensor.'at' is right, this timed_sensor will not exist.
	BOOST_FOREACH(const config::any_child &timed_sensor, root_cfg.all_children_range()) {
		if (timed_sensor.key == "timed_sensor") {
			if (sync_with_timed_tasks) {
				if (at == (int)timed_tasks.size()) {
					break;
				}
				const taplt_task& task = task_it->second;
				VALIDATE(task.timed_at == at, null_str);
			}

			result.push_back(ttimed_sensor());
			ttimed_sensor& sensor = result.back();

			sensor.at = timed_sensor.cfg["at"].to_int(nposm);

			const config& ipupt_vars_cfg = timed_sensor.cfg.child("input_vars");
			if (ipupt_vars_cfg) {
				for (const config::attribute &v: ipupt_vars_cfg.attribute_range()) {
					const std::string& key = v.first;
					const std::string& val = v.second;
					if (key.empty()) {
						continue;
					}
					sensor.input_vars.insert(std::make_pair(key, val));
				}
			}
			if (sync_with_timed_tasks) {
				if (sensor.at == at) {

				} else {
					sensor.at = at;
				}
				at ++;
				++ task_it;

			} else {
				sensor.at = nposm;
				bool fail = false;
				if (fail) {
					std::vector<ttimed_sensor>::iterator erase_it = result.begin();
					if (result.size() != 1) {
						std::advance(erase_it, result.size() - 1);
					}
					result.erase(erase_it);
				}
			}
		}
	}

	if (sync_with_timed_tasks && timed_tasks.size() != result.size()) {
		VALIDATE(timed_tasks.size() >= result.size(), null_str);
		while (timed_tasks.size() != result.size()) {
			result.push_back(ttimed_sensor());
			ttimed_sensor& sensor = result.back();
			sensor.at = result.size() - 1;
		}
		verity_timed_sensors();
	}

	return true;
}

void tcfg_cpp_api_core::timed_sensors_to_cfg(const std::vector<ttimed_sensor>& sensors, const std::set<int>& extract_ats, config& cfg) const
{
	int at = 0;
	int valid_at = 0;
	for (std::vector<ttimed_sensor>::const_iterator it = sensors.begin(); it != sensors.end(); ++ it, at ++) {
		const ttimed_sensor& sensor = *it;
		VALIDATE(sensor.at == at, null_str);

		if (extract_ats.count(at) == 0) {
			continue;
		}

		ttimed_sensor sensor2 = sensor;
		sensor2.at = valid_at ++;
		sensor2.to_cfg(cfg);
	}
}

void tcfg_cpp_api_core::timed_sensors_to_cfg(const std::vector<ttimed_sensor>& sensors, config& cfg) const
{
	// cfg["nick"].from_string(nick, true);

	int at = 0;
	for (std::vector<ttimed_sensor>::const_iterator it = sensors.begin(); it != sensors.end(); ++ it, at ++) {
		const ttimed_sensor& sensor = *it;
		VALIDATE(sensor.at == at, null_str);
		sensor.to_cfg(cfg);

	}
}

void tcfg_cpp_api_core::timed_sensors_to_stringstream(const std::vector<ttimed_sensor>& sensors, std::stringstream& result) const
{
	result.str("");
	config cfg;

	timed_sensors_to_cfg(sensors, cfg);

	if (!cfg.empty()) {
		write(result, cfg);
	}
}

void tcfg_cpp_api_core::verity_timed_sensors() const
{
	const std::map<taplt_task_key, taplt_task>& timed_tasks = bg_task_.timed_tasks();
	VALIDATE(timed_tasks.size() == timed_sensors_.size(), null_str);

	int timed_at = 0;
	for (std::map<taplt_task_key, taplt_task>::const_iterator it = timed_tasks.begin(); it != timed_tasks.end(); ++ it) {
		const taplt_task& task = it->second;
		// timed_taks is sorted by aux_key_id, not by timed_at. task.timed_at maybe not equal 'timed_at'.
		// VALIDATE(task.timed_at == timed_at, null_str);

		const ttimed_sensor& sensor = timed_sensors_[timed_at];
		VALIDATE(sensor.at == timed_at, null_str);

		timed_at ++;
	}
}

void tcfg_cpp_api_core::insert_timed_sensor(int new_timed_at, int builtin_at, const std::map<std::string, std::string>& input_vals)
{
	VALIDATE(new_timed_at == (int)timed_sensors_.size(), null_str);
	if (builtin_at == nposm) {
		timed_sensors_.push_back(aplt::ttimed_sensor());

	} else {
		VALIDATE(builtin_at >= 0 && builtin_at < (int)buildin_timed_sensors_.size(), null_str);
		timed_sensors_.push_back(buildin_timed_sensors_[builtin_at]);
	}
	timed_sensors_.back().at = new_timed_at;
	if (!input_vals.empty()) {
		timed_sensors_.back().input_vars = input_vals;
	}
	save_klink_pb(aplt::tbg_task::misc_cfg_timed_sensor);

	verity_timed_sensors();
}

void tcfg_cpp_api_core::erase_timed_sensor(int at)
{
	VALIDATE(at >= 0 && at < (int)timed_sensors_.size(), null_str);

	std::vector<ttimed_sensor>::iterator erase_it = timed_sensors_.begin();
	if (at != 0) {
		std::advance(erase_it, at);
	}
	timed_sensors_.erase(erase_it);

	for (std::vector<ttimed_sensor>::iterator it = timed_sensors_.begin(); it != timed_sensors_.end(); ++ it) {
		ttimed_sensor& sensor = *it;
		// pb_at does not participate in sort, modify it here, it should be safe
		if (sensor.at > at) {
			sensor.at --;
		}
	}
	save_klink_pb(aplt::tbg_task::misc_cfg_timed_sensor);

	verity_timed_sensors();
}

void tcfg_cpp_api_core::set_timed_sensors(const std::vector<ttimed_sensor>& timed_sensors)
{
	timed_sensors_ = timed_sensors;
	save_klink_pb(aplt::tbg_task::misc_cfg_timed_sensor);

	verity_timed_sensors();
}

//
// base scene
//
const aplt::tbase_scene* tcfg_cpp_api_core::base_scene_from_id(const std::string& id, bool must_exist) const
{
	VALIDATE(!id.empty(), null_str);

	for (std::vector<tbase_scene>::const_iterator it = base_scenes_.begin(); it != base_scenes_.end(); ++ it) {
		const tbase_scene& scene = *it;
		if (scene.id == id) {
			return &scene;
		}
	}

	VALIDATE(!must_exist, null_str);

	return nullptr;
}

void tcfg_cpp_api_core::set_base_scenes(const std::vector<tbase_scene>& base_scenes)
{
	base_scenes_ = base_scenes;
	save_klink_pb(aplt::tbg_task::misc_cfg_base_scene);
}

//
// add_timed_task
//
void tcfg_cpp_api_core::set_add_timed_tasks(const std::map<int64_t, tb_api::tadd_timed_task>& tasks)
{
	handle_add_timed_task(handle_type_clear);

	add_timed_tasks_ = tasks;


	for (std::map<int64_t, tb_api::tadd_timed_task>::iterator it = add_timed_tasks_.begin(); it != add_timed_tasks_.end(); ++ it) {
		tb_api::tadd_timed_task& task = it->second;
		VALIDATE(it->first == task.time, null_str);

		// if @tasks is imported klink.cfg from klink directly, 'added' or 'deleted' maybe true, require keep there values.
		task.added = false;
		task.deleted = false;
	}
	save_klink_pb(aplt::tbg_task::misc_cfg_add_timed_task);

	if (!add_timed_tasks_.empty()) {
		// call handle immediately.
		add_timed_tasks_last_shedule_zerotz_t_ = nposm;
	}
}

void tcfg_cpp_api_core::add_timed_task_sliced()
{
	if (bg_task_.is_ing()) {
		return;
	}

	const int elapse_today = utils::calculate_hour24_time(time(nullptr));

	const int ticks_20sec = 20 * 1000;
	if (add_timed_tasks_last_shedule_zerotz_t_ == nposm || elapse_today < add_timed_tasks_last_shedule_zerotz_t_) {
		handle_add_timed_task(handle_type_slice);

	} else if (!add_timed_task_20sec_handled_ && SDL_GetTicks() >= ticks_20sec) {
		handle_add_timed_task(handle_type_slice);
		add_timed_task_20sec_handled_ = true;
	}

	add_timed_tasks_last_shedule_zerotz_t_ = elapse_today;
}

void tcfg_cpp_api_core::did_handle_add_timed_task_quit(const bool& dirty)
{
	if (dirty) {
		save_klink_pb(aplt::tbg_task::misc_cfg_add_timed_task);
		instance->klink_tasks_changed2();
	}
}

void tcfg_cpp_api_core::handle_add_timed_task(int type)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(type >= 0 && type < handle_type_count, null_str);
	
	if (type == handle_type_slice) {
		VALIDATE(!bg_task_.is_ing(), null_str);
	}

	// SDL_Log("%u call handle_add_timed_task()", SDL_GetTicks());

	std::map<int, const aplt::taplt_task*> zerotz_t_tasks;

	const std::map<aplt::taplt_task_key, aplt::taplt_task>& timed_tasks = bg_task_.timed_tasks();
	for (std::map<aplt::taplt_task_key, aplt::taplt_task>::const_iterator it = timed_tasks.begin(); it != timed_tasks.end(); ++ it) {
		const aplt::taplt_task& task = it->second;
		zerotz_t_tasks.insert(std::make_pair(task.zerotz_t, &task));
	}

	const int64_t ts = time(nullptr);
	int64_t today_min_ts = utils::calculate_0h0m0s_ts(ts);
	int64_t today_max_ts = today_min_ts + ONE_DAY_SECONDS - 1;

	SDL_Log("today_min_ts: %s, today_max_ts: %s", 
		utils::format_time_ymdhms(today_min_ts).c_str(), utils::format_time_ymdhms(today_max_ts).c_str());

	bool dirty = false;
	tauto_destruct_executor destruct_executor(std::bind(&tcfg_cpp_api_core::did_handle_add_timed_task_quit, this, std::ref(dirty)));

	std::map<int64_t, tb_api::tadd_timed_task>& tasks = add_timed_tasks_;
	// 1/3: delete
	for (std::map<int64_t, tb_api::tadd_timed_task>::iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		tb_api::tadd_timed_task& add_task = it->second;

		// Add the unscheduled ones that belong to today to the list.
		if (add_task.deleted) {
			continue;
		}
		if (!add_task.added) {
			continue;
		}

		if (type == handle_type_slice) {
			if (add_task.time >= today_min_ts) {
				continue;
			}
		}

		const int zerotz_t = utils::calculate_hour24_time(add_task.time);
		if (zerotz_t_tasks.count(zerotz_t) == 0) {
			continue;
		}

		const taplt_task& aplt_task = *zerotz_t_tasks.find(zerotz_t)->second;
		if (!aplt_task_is_same_add_task(aplt_task, add_task)) {
			continue;
		}

		int timed_at = aplt_task.timed_at;
		bg_task_.erase_task2(aplt_task.type, aplt_task);

		erase_timed_sensor(timed_at);
		add_task.deleted = true;

		// this aplt_task is delete from klink's timed_tasks.
		zerotz_t_tasks.erase(zerotz_t_tasks.find(zerotz_t));

		dirty = true;
	}

	if (type == handle_type_clear) {
		return;
	}

	// 2/3: add
	std::map<int64_t, int64_t> time_adjusted_tasks;
	for (std::map<int64_t, tb_api::tadd_timed_task>::iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		tb_api::tadd_timed_task& add_task = it->second;

		// Add the unscheduled ones that belong to today to the list.
		if (add_task.added) {
			continue;
		}
		VALIDATE(!add_task.deleted, null_str);

		if (add_task.time < today_min_ts || add_task.time > today_max_ts) {
			continue;
		}

		const int original_zerotz_t = utils::calculate_hour24_time(add_task.time);
		int zerotz_t = original_zerotz_t;
		while (zerotz_t_tasks.count(zerotz_t) != 0) {
			zerotz_t ++;
		}
		if (zerotz_t >= ONE_DAY_SECONDS) {
			zerotz_t = original_zerotz_t;
			while (zerotz_t_tasks.count(zerotz_t) != 0) {
				zerotz_t --;
			}
		}
		if (zerotz_t != original_zerotz_t) {
			time_adjusted_tasks.insert(std::make_pair(add_task.time, add_task.time + (zerotz_t - original_zerotz_t)));
		}

		int new_timed_at = bg_task_.timed_tasks().size();
		const aplt::taplt_task* new_task = bg_task_.insert_timed_task(add_task.aplt_id, add_task.task_id, add_task.ble_device_id, aplt::taplt_task::state_fresh, zerotz_t);
		VALIDATE(new_task != nullptr, null_str);

		insert_timed_sensor(new_timed_at, nposm, add_task.input_vars);

		VALIDATE(zerotz_t_tasks.count(zerotz_t) == 0, null_str);
		zerotz_t_tasks.insert(std::make_pair(zerotz_t, new_task));
		add_task.added = true;

		dirty = true;
	}

	// 3/3: time conflict
	while (!time_adjusted_tasks.empty()) {
		VALIDATE(dirty, null_str);
		std::map<int64_t, int64_t>::iterator it = time_adjusted_tasks.begin();
		int64_t old_time = it->first;
		VALIDATE(tasks.count(old_time) != 0, null_str);

		// erase old
		std::map<int64_t, tb_api::tadd_timed_task>::iterator tasks_it = tasks.find(old_time);
		tb_api::tadd_timed_task new_task = tasks_it->second;
		new_task.time = it->second;
		tasks.erase(tasks_it);

		// insert new
		std::pair<std::map<int64_t, tb_api::tadd_timed_task>::iterator, bool> ins = tasks.insert(std::make_pair(new_task.time, new_task));
		VALIDATE(ins.second, null_str);

		time_adjusted_tasks.erase(it);
	}

	// if (dirty) {
	//	save_klink_pb(aplt::tbg_task::misc_cfg_add_timed_task);
	// }
}

bool tcfg_cpp_api_core::aplt_task_is_same_add_task(const taplt_task& aplt_task, const tb_api::tadd_timed_task& add_task) const
{
	VALIDATE(aplt_task.type == taplt_task::type_timed, null_str);

	if (aplt_task.aplt_id != add_task.aplt_id || aplt_task.task_id != add_task.task_id || aplt_task.ble_device_id != add_task.ble_device_id ||
		aplt_task.position1 != add_task.position1 || aplt_task.position2 != add_task.position2) {
		return false;
	}
	VALIDATE(aplt_task.timed_at >= 0 && aplt_task.timed_at < (int)timed_sensors_.size(), null_str);
	const ttimed_sensor& sensor = timed_sensors_[aplt_task.timed_at];
	if (sensor.input_vars != add_task.input_vars) {
		return false;
	}
	return true;
}

bool tcfg_cpp_api_core::aplt_task_is_from_add_timed_tasks(const taplt_task& aplt_task) const
{
	VALIDATE(aplt_task.type == taplt_task::type_timed, null_str);

	const int64_t ts = time(nullptr);
	int64_t today_min_ts = utils::calculate_0h0m0s_ts(ts);
	int64_t task_time = today_min_ts + aplt_task.zerotz_t;

	if (add_timed_tasks_.count(task_time) == 0) {
		return false;
	}

	const tb_api::tadd_timed_task& add_task = add_timed_tasks_.find(task_time)->second;
	return aplt_task_is_same_add_task(aplt_task, add_task);
}

bool tcfg_cpp_api_core::string_to_add_timed_tasks(const std::string& stream, std::map<int64_t, tb_api::tadd_timed_task>& result) const
{
	VALIDATE(result.empty(), null_str);

	config root_cfg;
	if (!read_config_ex(stream, true, root_cfg)) {
		return false;
	}

	return cfg_to_add_timed_tasks2(root_cfg, result);
}

bool tcfg_cpp_api_core::cfg_to_add_timed_tasks2(const config& root_cfg, std::map<int64_t, tb_api::tadd_timed_task>& result) const
{	
	bool ret = cfg_to_add_timed_tasks(root_cfg, result);
	if (ret) {
		verity_add_timed_tasks();
	}
	return ret;
}

void tcfg_cpp_api_core::add_timed_tasks_to_stringstream(const std::map<int64_t, tb_api::tadd_timed_task>& tasks, std::stringstream& result) const
{
	result.str("");
	config cfg;

	add_timed_tasks_to_cfg(tasks, cfg);
	if (!cfg.empty()) {
		write(result, cfg);
	}
}

void tcfg_cpp_api_core::verity_add_timed_tasks() const
{
	const std::map<int64_t, tb_api::tadd_timed_task>& tasks = add_timed_tasks_;
	for (std::map<int64_t, tb_api::tadd_timed_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const tb_api::tadd_timed_task& task = it->second;
		VALIDATE(it->first == task.time, null_str);
	}
}

bool tcourselist::from_cfg(const config& cfg)
{
	clear();

	std::string courses_str;
	if (cfg.has_child("courselist")) {
		const config& sub_cfg = cfg.child("courselist");
		courses_str = sub_cfg["courses"].str();
	}

	tcourse tmp_course;
	std::vector<std::string> vstr = utils::split(courses_str, course_separator_);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& str = *it;
		tmp_course.from_string(str);
		if (tmp_course.valid() && !is_existed(tmp_course.uid, tmp_course.title)) {
			key_2_course_ats_.insert(std::make_pair(tkey(tmp_course.uid, tmp_course.title), (int)courses_.size()));
			courses_.push_back(tmp_course);
		}
	}

	verify();
	return true;
}

void tcourselist::to_cfg(config& cfg) const
{
	config& sub_cfg = cfg.add_child("courselist");

	std::stringstream ss;
	for (std::vector<tcourse>::const_iterator it = courses_.begin(); it != courses_.end(); ++ it) {
		const tcourse& course = *it;
		VALIDATE(course.valid(), null_str);
		if (!ss.str().empty()) {
			ss << "|";
		}
		ss << course.to_string();
	}
	sub_cfg["courses"] = ss.str();
}

void tcourselist::assign(const tcourselist& that)
{
	courses_ = that.courses_;
	key_2_course_ats_ = that.key_2_course_ats_;

	verify();
}

void tcourselist::insert_course(int64_t uid, const std::string& title, int new_at)
{
	// VALIDATE(new_at == (int)courses_.size(), null_str);
	VALIDATE(new_at == courses_.size(), "now support add end only");

	VALIDATE(is_valid_courseware_uid(uid), null_str);
	VALIDATE(!title.empty(), null_str);

	VALIDATE(!is_existed(uid, title), null_str);

	key_2_course_ats_.insert(std::pair(tkey(uid, title), courses_.size()));
	courses_.push_back(tcourse(uid, title));

	verify();
}

int tcourselist::erase_course(int64_t uid, const std::string& title)
{
	VALIDATE(is_valid_courseware_uid(uid), null_str);
	VALIDATE(!title.empty(), null_str);

	VALIDATE(is_existed(uid, title), null_str);

	const tkey key(uid, title);
	std::map<tkey, int>::iterator it = key_2_course_ats_.find(key);
	int at = it->second;

	key_2_course_ats_.erase(it);
	for (std::map<tkey, int>::iterator it = key_2_course_ats_.begin(); it != key_2_course_ats_.end(); ++ it) {
		const tkey& key = it->first;
		int& this_at = it->second;
		if (this_at > at) {
			this_at --;
		}
	}

	std::vector<tcourse>::iterator course_it = courses_.begin();
	if (at != 0) {
		std::advance(course_it, at);
	}
	courses_.erase(course_it);

	verify();
	return at;
}

const tcourselist::tcourse& tcourselist::get_course(int64_t uid, const std::string& title) const
{
	const tkey key(uid, title);
	std::map<tkey, int>::const_iterator it = key_2_course_ats_.find(key);
	VALIDATE(it != key_2_course_ats_.end(), null_str);
	return courses_[it->second];
}

int tcourselist::course_which_at(int64_t uid, const std::string& title) const
{
	const tkey key(uid, title);
	std::map<tkey, int>::const_iterator it = key_2_course_ats_.find(key);
	if (it == key_2_course_ats_.end()) {
		return nposm;
	}

	return it->second;
}

void tcourselist::verify() const
{
	std::set<int> course_ats;
	int course_size = courses_.size();
	for (std::map<tkey, int>::const_iterator it = key_2_course_ats_.begin(); it != key_2_course_ats_.end(); ++ it) {
		const tkey& key = it->first;
		int at = it->second;

		VALIDATE(course_ats.count(at) == 0, null_str);
		course_ats.insert(at);
		VALIDATE(at < course_size, null_str);
		const tcourse& course = courses_[at];
		VALIDATE(key.uid == course.uid, null_str);
		VALIDATE(key.title == course.title, null_str);
	}
	VALIDATE(key_2_course_ats_.size() == course_size, null_str);
}

//
// courselist
//
bool tcfg_cpp_api_core::string_to_courselist(const std::string& stream, tcourselist& result) const
{
	result.clear();

	config root_cfg;
	if (!read_config_ex(stream, true, root_cfg)) {
		return false;
	}

	return result.from_cfg(root_cfg);
}

void tcfg_cpp_api_core::courselist_to_stringstream(const tcourselist& courselist, std::stringstream& result) const
{
	result.str("");
	config cfg;
	courselist.to_cfg(cfg);

	if (!cfg.empty()) {
		write(result, cfg);
	}
}

void tcfg_cpp_api_core::insert_course(int64_t uid, const std::string& title, int new_at)
{
	int new_at2 = new_at == nposm? courselist_.courses().size(): new_at;
	courselist_.insert_course(uid, title, new_at2);
	app_did_insert_course(new_at2);
	// speech_driver_.listen_did_course_changed(true, new_at2);

	save_klink_pb(aplt::tbg_task::misc_cfg_courselist);
}

void tcfg_cpp_api_core::erase_course(int64_t uid, const std::string& title)
{
	int erased_at = courselist_.erase_course(uid, title);
	app_did_erase_course(erased_at);
	// speech_driver_.listen_did_course_changed(false, erased_at);

	save_klink_pb(aplt::tbg_task::misc_cfg_courselist);
}

void tcfg_cpp_api_core::set_courselist(const tcourselist& courselist)
{
	courselist_.assign(courselist);
	save_klink_pb(aplt::tbg_task::misc_cfg_courselist);
}

// 
// taskpoint
//
void tcfg_cpp_api_core::save_taskpoint(const taplt_task& klink_cpp_aplt_task2, const aplt::tapplet::ttask& cfg_task, const ttask_vars& task_vars)
{
	VALIDATE(bg_task_.is_ing(), null_str);
	VALIDATE(bg_task_.bg_task2().in_task_cpp(), null_str);

	taskpoint_.reset(new ttaskpoint(klink_cpp_aplt_task2, cfg_task, app_ros_cpp_api_curr_state(), task_vars));
}

void tcfg_cpp_api_core::clear_taskpoint()
{
	VALIDATE(taskpoint_.get() != nullptr, null_str);
	taskpoint_.reset();
}

bool tcfg_cpp_api_core::use_taskpoint(const aplt::taplt_task& aplt_task) const
{
	if (taskpoint_.get() == nullptr) {
		return false;
	}
	const ttaskpoint& taskpoint = *taskpoint_.get();
	return taskpoint.task_cpp_type == aplt_task.type && taskpoint.task_cpp_pb_at == aplt_task.pb_at;
}

void iot_device_to_cfg(const aplt::tiot_device& device, config& cfg)
{
	cfg.clear();

	VALIDATE(aplt::iot_sources.count(device.src) != 0, null_str);
	const aplt::tiot_src2& iot_src2 = aplt::iot_sources.find(device.src)->second;
	cfg["src"].from_string(iot_src2.id, true);
	cfg["device_id"].from_string(device.device_id, true);

	cfg["alias"].from_string(device.alias, true);
	cfg["icon"].from_string(device.icon, true);
		
	// int64_t ts;
	// int pb_at;

	// int number;
	// std::string alias_name;
}

bool iot_device_from_cfg(const config& cfg, std::vector<aplt::tiot_device>& devices)
{
	int src = aplt::iot_src_from_str(cfg["src"].str());
	if (src == nposm) {
		return false;
	}
	std::string device_id = cfg["device_id"].str();
	if (device_id.empty()) {
		return false;
	}
	std::string alias = cfg["alias"].str();
	if (!alias.empty() && !isvalid_normal_utf8_name224(alias)) {
		return false;
	}
	std::string icon = cfg["icon"].str();

	int64_t ts = nposm;
	int pb_at = devices.size();
	devices.push_back(aplt::tiot_device(src, device_id, alias, icon, ts, pb_at));
	return true;
}

std::string tcfg_cpp_api_core::import_klink_cfg(const std::string& filename)
{
	const tcfg_4field& cfg_4field = klink_cfgs.find(cfgtype_klink)->second;
	utils::string_map symbols;
	symbols["type_cfg"] = cfg_4field.name;
	symbols["file"] = filename;

	std::string stream;
	{
		const int max_task_cpp_cfg_size = 512 * 1024; // 512K bytes
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize == 0 || fsize > max_task_cpp_cfg_size) {
			return vgettext2("Cann't find $file, or size must be <= 512K bytes", symbols);
		}

		bool all_is_utf8 = utils::is_utf8str(file.data, fsize);
		if (!all_is_utf8) {
			return vgettext2("$file isn't utf-8 format", symbols);
		}
		stream.assign(file.data, fsize);
	}

	config top_cfg;
	if (!aplt::read_config_ex(stream, true, top_cfg)) {
		return _("Import failed. Not a valid WML format file");
	}

	const std::string cfg_type = top_cfg["type"].str();
	if (cfg_type != cfg_4field.id) {
		symbols["type"] = cfg_4field.id;
		return vgettext2("Import failed. for $type_cfg, 'type' value must be $type", symbols);
	}

	const std::string scene_name = top_cfg["scene_name"].str();
	if (scene_name.empty()) {
		symbols["aplt_task"] = symbols["klink_cfg"];
		symbols["key"] = "scene_name";
		symbols["value"] = _("Empty");
		return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
	}

	std::map<aplt::taplt_task_key, aplt::taplt_task> iot_tasks;
	std::map<aplt::taplt_task_key, aplt::taplt_task> speech_tasks;
	std::map<aplt::taplt_task_key, aplt::taplt_task> var_tasks;
	std::map<aplt::taplt_task_key, aplt::taplt_task> timed_tasks;

	std::map<int, std::map<aplt::taplt_task_key, aplt::taplt_task>*> all_tasks = {
		{aplt::taplt_task::type_iot, &iot_tasks},
		{aplt::taplt_task::type_speech, &speech_tasks},
		{aplt::taplt_task::type_var, &var_tasks},
		{aplt::taplt_task::type_timed, &timed_tasks},
	};

	std::vector<aplt::tiot_device> vec_iot_devices;
	BOOST_FOREACH(const config::any_child &task, top_cfg.all_children_range()) {
		if (task.key == "aplt_task") {
			const std::string type_str = task.cfg["type"].str();
			int type = aplt::aplt_task_type_from_str(type_str);
			if (type == nposm) {
				symbols["aplt_task"] = "[aplt_task]";
				symbols["key"] = "type";
				symbols["value"] = type_str;
				return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
			}
			int priority = task.cfg["priority"].to_int(nposm);

			const std::string aplt_id = task.cfg["aplt_id"].str();
			const std::string task_id = task.cfg["task_id"].str();
			const std::string ble_device_id = task.cfg["ble_device_id"].str();
			const std::string position1 = task.cfg["position1"].str();
			const std::string position2 = task.cfg["position2"].str();

			int state = aplt::taplt_task::state_finished_expired;
			int pb_at = all_tasks.find(type)->second->size();

			char task_str[64];
			SDL_snprintf(task_str, sizeof(task_str), "type: %s, #%i", type_str.c_str(), pb_at + 1);
			symbols["aplt_task"] = task_str;
			if (type == aplt::taplt_task::type_iot) {
				if (!IS_VALID_APLT_TASK_NONTIMED_PRIORITY(priority)) {
					symbols["key"] = "priority";
					symbols["value"] = task.cfg["priority"].str();
					return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
				}
				int iot_src = aplt::iot_src_from_str(task.cfg["iot_src"].str());
				if (iot_src == nposm) {
					symbols["key"] = "iot_src";
					symbols["value"] = task.cfg["iot_src"].str();
					return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
				}
				int src_evt = aplt::iot_event_from_str(task.cfg["src_evt"].str());
				if (src_evt == nposm) {
					symbols["key"] = "iot_src";
					symbols["value"] = task.cfg["src_evt"].str();
					return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
				}
				const std::string src_device_id = task.cfg["src_device_id"].str();
				iot_tasks.insert(std::make_pair(aplt::taplt_task_key(iot_src, src_evt, src_device_id),
					aplt::taplt_task(priority, aplt_id, task_id, ble_device_id, position1, position2,
					state, pb_at, iot_src, src_evt, src_device_id)));

			} else if (type == aplt::taplt_task::type_speech) {
				if (!IS_VALID_APLT_TASK_NONTIMED_PRIORITY(priority)) {
					symbols["key"] = "priority";
					symbols["value"] = task.cfg["priority"].str();
					return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
				}
				const std::string speech_id = task.cfg["speech_id"].str();
				speech_tasks.insert(std::make_pair(aplt::taplt_task_key(speech_id),
					aplt::taplt_task(priority, aplt_id, task_id, ble_device_id, position1, position2,
					state, pb_at, speech_id)));

			} else if (type == aplt::taplt_task::type_var) {
				if (!IS_VALID_APLT_TASK_TIMED_PRIORITY(priority)) {
					symbols["key"] = "priority";
					symbols["value"] = task.cfg["priority"].str();
					return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
				}
				int var_at = task.cfg["var_at"].to_int();
				if (var_at != (int)var_tasks.size()) {
					symbols["key"] = "var_at";
					symbols["value"] = task.cfg["var_at"].str();
					return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
				}
				int aux_key_id = next_aux_key_id(var_tasks);
				var_tasks.insert(std::make_pair(aplt::taplt_task_key(aux_key_id),
					aplt::taplt_task(aplt_id, task_id, null_str, null_str, state, pb_at, var_at, aux_key_id)));

			} else {
				VALIDATE(type == aplt::taplt_task::type_timed, null_str);
				if (!IS_VALID_APLT_TASK_TIMED_PRIORITY(priority)) {
					symbols["key"] = "priority";
					symbols["value"] = task.cfg["priority"].str();
					return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
				}
				int zerotz_t = task.cfg["zerotz_t"].to_int();
				if (!IS_VALID_APLT_TASK_TIMED_ZEROTZ_T(zerotz_t)) {
					symbols["key"] = "zerotz_t";
					symbols["value"] = task.cfg["zerotz_t"].str();
					return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
				}
				int timed_at = task.cfg["timed_at"].to_int();
				if (timed_at != (int)timed_tasks.size()) {
					symbols["key"] = "timed_at";
					symbols["value"] = task.cfg["timed_at"].str();
					return vgettext2("Import failed. In '$aplt_task', '$key' cannot be $value", symbols);
				}
				int state = aplt::taplt_task::state_fresh;
				int aux_key_id = next_aux_key_id(timed_tasks);
				timed_tasks.insert(std::make_pair(aplt::taplt_task_key(aplt_id, task_id, ble_device_id, aux_key_id),
					aplt::taplt_task(aplt_id, task_id, ble_device_id, position1, position2, state, pb_at, zerotz_t, timed_at, aux_key_id)));

			}

		} else if (task.key == "iot_device") {
			iot_device_from_cfg(task.cfg, vec_iot_devices);
		}
	}

	//
	// var task
	//
	std::vector<aplt::tvar_sensor> var_sensors;
	cfg_to_var_sensors(top_cfg, false, var_sensors);
	if (var_sensors.size() != var_tasks.size()) {
		symbols["task_count"] = str_cast(var_tasks.size());
		symbols["sensor_count"] = str_cast(var_sensors.size());
		return vgettext2("Import failed. The number($task_count) of var_task is different from the number($sensor_count) of var_sensor.", symbols);
	}
	int at = 0;
	for (std::vector<aplt::tvar_sensor>::iterator it = var_sensors.begin(); it != var_sensors.end(); ++ it, at ++) {
		aplt::tvar_sensor& sensor = *it;
		sensor.at = at;
	}

	//
	// timed task
	//
	std::vector<aplt::ttimed_sensor> timed_sensors;
	cfg_to_timed_sensors(top_cfg, false, timed_sensors);
	if (timed_sensors.size() != timed_tasks.size()) {
		symbols["speech_count"] = str_cast(timed_tasks.size());
		symbols["sensor_count"] = str_cast(timed_sensors.size());
		return vgettext2("Import failed. The number($speech_count) of $speech type tasks is different from the number($sensor_count) of var_densor tasks", symbols);
	}
	at = 0;
	for (std::vector<aplt::ttimed_sensor>::iterator it = timed_sensors.begin(); it != timed_sensors.end(); ++ it, at ++) {
		aplt::ttimed_sensor& sensor = *it;
		sensor.at = at;
	}

	//
	// add_timed_tasks
	//
	std::map<int64_t, tb_api::tadd_timed_task> add_timed_tasks;
	cfg_to_add_timed_tasks(top_cfg, add_timed_tasks);

	//
	// courselist
	//
	tcourselist courselist;
	courselist.from_cfg(top_cfg);

	std::vector<aplt::tbase_scene> base_scenes;
	cfg_to_base_scenes(top_cfg, base_scenes);

	bg_task_.set_4tasks(scene_name, iot_tasks, speech_tasks, var_tasks, timed_tasks, vec_iot_devices, false);
	set_var_sensors(var_sensors);
	set_timed_sensors(timed_sensors);

	set_add_timed_tasks(add_timed_tasks);
	set_courselist(courselist);

	//
	// below stop/start subtask logic is same as store.cpp
	// 
	const std::string scene_id = base_driver_.scene_id();
	int subtask_state = nposm;
	if (!scene_id.empty()) {
		// Why need to enter here even when 'subtask_state == sts_idle'?
		// -- when sts_idle, apltsotype_base2th's ref_count is 1. 
		//    However, during uninstall, apltsotype_base2th maybe to uninstalled.
		subtask_state = base_driver_.subtask_state();
		VALIDATE(subtask_state == aplt::sts_idle || subtask_state == aplt::sts_ing, null_str);
		base_driver_.stop_subtask(true);
	}
	set_base_scenes(base_scenes);
	const aplt::tbase_scene* scene = nullptr;
	if (!scene_id.empty()) {
		VALIDATE(base_driver_.installed(), null_str);
		scene = base_scene_from_id(scene_id, false);
	}
	if (scene != nullptr) {
		if (subtask_state == aplt::sts_ing) {
			base_driver_.start_subtask_from_nposm(*scene);

		} else {
			VALIDATE(subtask_state == aplt::sts_idle, null_str);
			base_driver_.idle_subtask_from_empty(*scene);
		}
	}
	return null_str;
}

std::string tcfg_cpp_api_core::import_task_cpp_cfg(const std::string& filename, std::map<std::string, aplt::ttask_cpp_pair>& pairs_from_cfg)
{
	pairs_from_cfg.clear();

	const tcfg_4field& cfg_4field = klink_cfgs.find(cfgtype_task_cpp)->second;
	utils::string_map symbols;
	symbols["type_cfg"] = cfg_4field.name;
	symbols["file"] = filename;

	std::string stream;
	{
		const int max_task_cpp_cfg_size = 512 * 1024; // 512K bytes
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize == 0 || fsize > max_task_cpp_cfg_size) {
			return vgettext2("Cann't find $file, or size must be <= 512K bytes", symbols);
		}

		bool all_is_utf8 = utils::is_utf8str(file.data, fsize);
		if (!all_is_utf8) {
			return vgettext2("$file isn't utf-8 format", symbols);
		}
		stream.assign(file.data, fsize);
	}

	config top_cfg;
	if (!aplt::read_config_ex(stream, true, top_cfg)) {
		return _("Import failed. Not a valid WML format file");
	}

	const std::string cfg_type = top_cfg["type"].str();
	if (cfg_type != cfg_4field.id) {
		symbols["type"] = cfg_4field.id;
		return vgettext2("Import failed. for $type_cfg, 'type' value must be $type", symbols);
	}

	// std::map<std::string, aplt::ttask_cpp_pair> pairs_from_cfg;
	bool retval = cfg_to_task_pairs(top_cfg, pairs_from_cfg);
	if (!retval) {
		// parse config fail.
		return vgettext2("Parse $file error", symbols);
	}

	if (pairs_from_cfg.empty()) {
		return vgettext2("There is no task in $file.", symbols);
	}

	return null_str;
}

std::string tcfg_cpp_api_core::import_speech_cfg(const std::string& filename, std::string& nick, std::map<std::string, aplt::tspeech_sensor>& sensors_from_cfg)
{
	nick.clear();
	sensors_from_cfg.clear();

	const tcfg_4field& cfg_4field = klink_cfgs.find(cfgtype_speech)->second;
	utils::string_map symbols;
	symbols["type_cfg"] = cfg_4field.name;
	symbols["file"] = filename;

	std::string stream;
	{
		const int max_task_cpp_cfg_size = 512 * 1024; // 512K bytes
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize == 0 || fsize > max_task_cpp_cfg_size) {
			return vgettext2("Cann't find $file, or size must be <= 512K bytes", symbols);
		}

		bool all_is_utf8 = utils::is_utf8str(file.data, fsize);
		if (!all_is_utf8) {
			return vgettext2("$file isn't utf-8 format", symbols);
		}
		stream.assign(file.data, fsize);
	}

	config top_cfg;
	if (!aplt::read_config_ex(stream, true, top_cfg)) {
		return _("Import failed. Not a valid WML format file");
	}

	const std::string cfg_type = top_cfg["type"].str();
	if (cfg_type != cfg_4field.id) {
		symbols["type"] = cfg_4field.id;
		return vgettext2("Import failed. for $type_cfg, 'type' value must be $type", symbols);
	}

	bool retval = cfg_to_speech_sensors(top_cfg, nick, sensors_from_cfg);
	if (!retval) {
		// parse config fail.
		return vgettext2("Parse $file error", symbols);
	}

	if (sensors_from_cfg.empty()) {
		return vgettext2("There is no task in $file.", symbols);
	}

	return null_str;
}

}