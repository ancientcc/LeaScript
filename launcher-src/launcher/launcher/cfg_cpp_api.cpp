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

#define GETTEXT_DOMAIN "launcher-lib"

#include "cfg_cpp_api.hpp"
#include "game_config.hpp"

#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
// #include "gui/dialogs/messagefs.hpp"
#include "serialization/parser.hpp"
#include "chinese.hpp"
#include "base_driver.hpp"
#include "speech_driver.hpp"
#include "base_instance.hpp"

using namespace std::placeholders;


namespace aplt {
/*
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
*/
tcfg_cpp_api::tcfg_cpp_api(const std::map<aplt::taplt_key, aplt::tapplet>& applets, tros_map& curmap, 
	aplt::tbg_task& bg_task, tbase_driver& base_driver, tspeech_driver& speech_driver)
	: tcfg_cpp_api_core(applets, bg_task, base_driver)
	, curmap_(curmap)
	, speech_driver_(speech_driver)
	, pinyin_(aplt::get_curr_pinyin())
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
/*
	, add_timed_task_20sec_handled_(false)
	, add_timed_tasks_last_shedule_zerotz_t_(nposm)
*/
{}

tcfg_cpp_api::~tcfg_cpp_api()
{
	// VALIDATE(taskpoint_.get() == nullptr, null_str);
}

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

const aplt::taplt_task* tcfg_cpp_api::find_matched_speech_sensor(const std::string& result, 
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

void tcfg_cpp_api::app_did_insert_course(int new_at)
{
	speech_driver_.listen_did_course_changed(true, new_at);
}

void tcfg_cpp_api::app_did_erase_course(int erased_at)
{
	speech_driver_.listen_did_course_changed(false, erased_at);
}

}