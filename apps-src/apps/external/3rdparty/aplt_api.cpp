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

#define GETTEXT_DOMAIN "rose-lib"

#include "aplt_api.hpp"
#include "gettext.hpp"
#include <boost/foreach.hpp>

namespace aplt {

//
// tb_api
//
static tb_api* b_api = nullptr;

tb_api& get_b_api()
{
	VALIDATE(b_api != nullptr, null_str);
	return *b_api;
}

tb_api::tb_api()
{
	VALIDATE(b_api == nullptr, null_str);
	b_api = this;
}

tb_api::~tb_api()
{
	VALIDATE(b_api != nullptr, null_str);
	b_api = nullptr;
}

const tbase_scene* handle_base_scene(aplt::tb_api& ros, int desire_at, const std::string& name, int action, std::string& err_msg)
{
	const std::vector<tbase_scene>& scenes = ros.aplt_base_scenes();
	if (desire_at != nposm) {
		VALIDATE(desire_at >= 0 && desire_at <= (int)scenes.size(), null_str);
	}

	if (scenes.empty()) {
		err_msg = _("Scene not created.");
		return nullptr;
	}

	const tbase_scene* desire_scene = nullptr;
	int to_state = sts_ing;
	int curr_at = nposm;

	utils::string_map symbols;
	if (desire_at != nposm) {
		curr_at = desire_at;
		desire_scene = &scenes[curr_at];

	} else if (!name.empty()) {
		// Switch to the scene that matches the 'desire_name'.
		std::string py_desire_name;
		if (!name.empty()) {
			tpinyin& pinyin = aplt::get_curr_pinyin();
			py_desire_name = pinyin.from_utf8str2(name, PINYIN_DEF_TONE, PINYIN_DEF_ENG_LOWERCASE);
		}
		bool equal = py_desire_name == scenes[0].py_name;
		int s1 = py_desire_name.size();
		int s2 = scenes[0].py_name.size();
		size_t pos = py_desire_name.find(scenes[0].py_name);

		for (std::vector<tbase_scene>::const_iterator it = scenes.begin(); it != scenes.end(); ++ it) {
			const tbase_scene& scene = *it;
			if (py_desire_name.find(scene.py_name) != std::string::npos) {
				curr_at = std::distance(scenes.begin(), it);
				break;
			}
		}
		if (curr_at == nposm) {
			symbols["scene"] = name;
			err_msg = vgettext2("Scene not found: $scene.", symbols);
			return nullptr;
		}
		VALIDATE(curr_at >= 0 && curr_at < (int)scenes.size(), null_str);
		desire_scene = &scenes[curr_at];

	} else {
		std::pair<const tbase_scene*, int> pair = ros.aplt_curr_base_scene();
		const tbase_scene* curr_scene = pair.first;
		if (action == bs_action_switch_to_next) {
			// swith to next
			if (curr_scene != nullptr) {
				for (std::vector<tbase_scene>::const_iterator it = scenes.begin(); it != scenes.end(); ++ it) {
					const tbase_scene& scene = *it;
					if (scene.get_id() == curr_scene->get_id()) {
						curr_at = std::distance(scenes.begin(), it);
						break;
					}
				}
				VALIDATE(curr_at != nposm, null_str);
				if (scenes.size() == 1) {
					// new_scene = &scenes[0];
					VALIDATE(curr_at == 0, null_str);
				} else {
					curr_at ++;
					curr_at %= scenes.size();
				}
			} else {
				curr_at = 0;
			}
			VALIDATE(curr_at >= 0 && curr_at < (int)scenes.size(), null_str);
			desire_scene = &scenes[curr_at];

		} else if (action == bs_action_idle) {
			// idle current
			if (curr_scene == nullptr || pair.second != sts_ing) {
				err_msg = vgettext2("No scenes available to suspend.", symbols);
				return nullptr;
			}
			desire_scene = curr_scene;
			to_state = sts_idle;

		} else {
			// resumme current
			VALIDATE(action == bs_action_resume, null_str);
			if (curr_scene == nullptr || pair.second != sts_idle) {
				err_msg = vgettext2("No scenes available to resume.", symbols);
				return nullptr;
			}
			desire_scene = curr_scene;
			to_state = sts_ing;
		}
	}
	VALIDATE(desire_scene != nullptr, null_str);
	VALIDATE(to_state == sts_ing || to_state == sts_idle, null_str);

	return ros.aplt_set_base_scene(*desire_scene, to_state, err_msg);
}

void add_timed_tasks_to_cfg(const std::map<int64_t, tb_api::tadd_timed_task>& tasks, config& cfg)
{
	int at = 0;
	for (std::map<int64_t, tb_api::tadd_timed_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it, at ++) {
		const tb_api::tadd_timed_task& task = it->second;
		config& task_cfg = cfg.add_child("add_timed_task");

		task_cfg["persist"].from_bool(task.persist);
		task_cfg["aplt_id"].from_string(task.aplt_id, true);
		task_cfg["task_id"].from_string(task.task_id, true);
		task_cfg["ble_device_id"].from_string(task.ble_device_id, true);
		task_cfg["position1"].from_string(task.position1, true);
		task_cfg["position2"].from_string(task.position2, true);
		task_cfg["time"].from_int64(task.time);

		config& input_vars_cfg = task_cfg.add_child("input_vars");
		for (std::map<std::string, std::string>::const_iterator it = task.input_vars.begin(); it != task.input_vars.end(); ++ it) {
			const std::string& key = it->first;
			const std::string& val = it->second;
			VALIDATE(!key.empty(), null_str);
			input_vars_cfg[key].from_string(val, true);
		}

		task_cfg["added"].from_bool(task.added);
		task_cfg["deleted"].from_bool(task.deleted);
	}
}

bool cfg_to_add_timed_tasks(const config& root_cfg, std::map<int64_t, tb_api::tadd_timed_task>& result)
{	
	result.clear();

	// sync bg_task_.timed_tasks_ with timed_sensors_
	// must make sure 'timed_tasks_[at].timed_at' == 'timed_sensors_[at].at'
	// const std::map<taplt_task_key, taplt_task>& timed_tasks = bg_task_.timed_tasks();
	// std::map<taplt_task_key, taplt_task>::const_iterator task_it = timed_tasks.begin();

	// int at = 0;
	// if timed_sensor.'at' is right, this timed_sensor will not exist.
	std::map<std::string, std::string> input_vars;
	BOOST_FOREACH (const config& task_cfg, root_cfg.child_range("add_timed_task")) {
		bool persist = task_cfg["persist"].to_bool();
		const std::string aplt_id = task_cfg["aplt_id"].str();
		const std::string task_id = task_cfg["task_id"].str();
		const std::string ble_device_id = task_cfg["ble_device_id"].str();
		const std::string position1 = task_cfg["position1"].str();
		const std::string position2 = task_cfg["position2"].str();
		int64_t time = task_cfg["time"].to_int64();

		input_vars.clear();
		const config& ipupt_vars_cfg = task_cfg.child("input_vars");
		if (ipupt_vars_cfg) {
			for (const config::attribute &v: ipupt_vars_cfg.attribute_range()) {
				const std::string& key = v.first;
				const std::string& val = v.second;
				if (key.empty()) {
					continue;
				}
				input_vars.insert(std::make_pair(key, val));
			}
		}
		std::pair<std::map<int64_t, tb_api::tadd_timed_task>::iterator, bool> ins = result.insert(std::make_pair(time, tb_api::tadd_timed_task(false, aplt_id, task_id, ble_device_id, position1, position1, time, input_vars)));
		if (!ins.second) {
			continue;
		}
		tb_api::tadd_timed_task& new_task = ins.first->second;
		new_task.added = task_cfg["added"].to_bool();
		new_task.deleted = task_cfg["deleted"].to_bool();
	}

	// verity_add_timed_tasks();
	return true;
}

}