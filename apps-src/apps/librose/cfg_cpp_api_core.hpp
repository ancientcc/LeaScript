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

#ifndef LIBROSE_CFG_CPP_API_CORE_HPP
#define LIBROSE_CFG_CPP_API_CORE_HPP

#include "aplt_api.hpp"
#include "aplt.hpp"

class tbase_driver_core;

enum {cfgtype_klink, cfgtype_task_cpp, cfgtype_speech, klink_cfgtype_count};
struct tcfg_4field
{
	tcfg_4field(int type, const std::string& prefix, const std::string& id)
		: type(type)
		, prefix(prefix)
		, id(id)
	{}

	const int type;
	const std::string prefix;
	const std::string id;
	std::string name;
};
extern std::map<int, tcfg_4field> klink_cfgs;

std::string get_klink_cfg_dir(int type, bool preferences);
void collect_klink_cfg_files(int type, std::set<std::string>& files);

namespace aplt {

#define MAX_SPEECH_SENSOR_VARS		3


class tspeech_sensor
{
public:
	class tvar
	{
	public:
		tvar()
		{
			clear();
		}

		bool from_cfg(aplt::tpinyin& pinyin, int tone, bool eng_lowercase, const config& cfg, int at);
		void to_cfg(config& cfg) const;

		bool operator==(const tvar& that) const
		{
			if (aplt_id != that.aplt_id) {
				return false;
			}
			if (name != that.name) {
				return false;
			}

			if (optional != that.optional) {
				return false;
			}

			if (first_words_is_prefix != that.first_words_is_prefix) {
				return false;
			}

			if (prefix_words != that.prefix_words || py_prefix_words != that.py_prefix_words || 
				postfix_words != that.postfix_words || py_postfix_words != that.py_postfix_words) {
				return false;
			}

			if (major_word != that.major_word || py_major_word != that.py_major_word || 
				minor_words != that.minor_words || py_minor_words != that.py_minor_words ||
				strategy != that.strategy) {
				return false;
			}

			return true;
		}

		bool operator!=(const tvar& that) const { return !operator==(that); }

		bool valid() const
		{
			return !name.empty() && is_bundleid(aplt_id);
		}

		bool is_nposm() const 
		{
			return aplt_id.empty() && name.empty() && !first_words_is_prefix && prefix_words.empty() && postfix_words.empty();
		}

		std::string name2() const
		{
			VALIDATE(valid(), null_str);
			return utils::join_app_prefix_id(aplt_id, name);
		}

		void clear()
		{
			aplt_id.clear();
			name.clear();

			optional = false;

			first_words_is_prefix = false;
			prefix_words.clear();
			py_prefix_words.clear();
			postfix_words.clear();
			py_postfix_words.clear();

			major_word.clear();
			py_major_word.clear();
			minor_words.clear();
			py_minor_words.clear();
			// set a valid strategy. it is necessary when tvariable::tvariable().
			strategy = mkeys_any_one;
		}

	public:
		// com.kos.launcher__position/aplt.leagor.basic__fruit_result
		std::string aplt_id; // com.kos.launcher
		std::string name; // position
		bool optional;

		bool first_words_is_prefix;
		std::vector<std::string> prefix_words;
		std::vector<std::string> py_prefix_words;
		std::vector<std::string> postfix_words;
		std::vector<std::string> py_postfix_words;

		std::string major_word;
		std::string py_major_word;
		std::vector<std::string> minor_words;
		std::vector<std::string> py_minor_words;
		int strategy;
	};

	tspeech_sensor();

	bool from_cfg(const config& cfg);
	void to_cfg(config& cfg) const;
	void green();

	std::string name2() const 
	{
		std::stringstream ss;
		ss << name << "(" << id << ")";

		return ss.str();
	}

	const tvar& var(int at) const
	{
		if (at == 0) {
			return var1;
		} else if (at == 1) {
			return var2;
		}
		VALIDATE(at == 2, null_str);
		return var3;
	}

	tvar& mutable_var(int at)
	{
		if (at == 0) {
			return var1;
		} else if (at == 1) {
			return var2;
		}
		VALIDATE(at == 2, null_str);
		return var3;
	}

	int valid_vars() const
	{
		if (var3.valid()) {
			VALIDATE(var1.valid(), null_str);
			VALIDATE(var2.valid(), null_str);
			return 3;
		}
		if (var2.valid()) {
			VALIDATE(var1.valid(), null_str);
			return 2;
		}
		return var1.valid()? 1: 0;
	}

	bool operator==(const tspeech_sensor& that) const
	{
		if (id != that.id) {
			return false;
		}
		if (name != that.name) {
			return false;
		}

		if (countdown_s != that.countdown_s) {
			return false;
		}
		if (first_words != that.first_words || py_first_words != that.py_first_words) {
			return false;
		}

		if (major_word != that.major_word || py_major_word != that.py_major_word || 
			minor_words != that.minor_words || py_minor_words != that.py_minor_words ||
			strategy != that.strategy) {
			return false;
		}

		if (var1 != that.var1 || var2 != that.var2 || var3 != that.var3) {
			return false;
		}
		return true;
	}

	bool equal(const tspeech_sensor& that) const { return operator==(that); }

	void clear()
	{
		id.clear();
		name.clear();

		countdown_s = 0;
		first_words.clear();
		py_first_words.clear();

		major_word.clear();
		py_major_word.clear();
		minor_words.clear();
		py_minor_words.clear();
		// set a valid strategy. it is necessary when tspeech_sensor::tspeech_sensor().
		strategy = mkeys_any_one;

		var1.clear();
		var2.clear();
		var3.clear();
	}

	std::string py_from_utf8str(const std::string& str, tint32data_C* pos_data = nullptr) const;

public:
	std::string id;
	std::string name;

	int countdown_s;
	std::vector<std::string> first_words;
	std::vector<std::string> py_first_words;

	std::string major_word;
	std::string py_major_word;
	std::vector<std::string> minor_words;
	std::vector<std::string> py_minor_words;
	int strategy;

	tvar var1;
	tvar var2;
	tvar var3;

private:
	//
	// parse pinyin
	//
	aplt::tpinyin& pinyin_;
	const int tone_;
	const bool eng_lowercase_;
};

void evaluate_task_pairs(const std::map<std::string, aplt::ttask_cpp_pair>& from, std::map<std::string, aplt::ttask_cpp_pair>& to);
void evaluate_speech_sensors(const std::string& from_nick, const std::map<std::string, aplt::tspeech_sensor>& from, std::string& to_nick, std::map<std::string, aplt::tspeech_sensor>& to);

// uint64_t task_cpp_is_valid(const aplt::ttask_cpp_pair& pair, std::string& err_msg);
// uint64_t speech_sensor_is_valid(const aplt::tspeech_sensor& sensor, std::string& err_msg);

class tvar_sensor
{
public:
	tvar_sensor()
		: at(nposm)
	{}

public:
	int at;
	std::string name;
	tif_block if_block;
	std::set<std::string> sync_vars;
};

class ttimed_sensor
{
public:
	ttimed_sensor()
		: at(nposm)
	{}

	void to_cfg(config& cfg) const
	{
		config& sensor_cfg = cfg.add_child("timed_sensor");
		sensor_cfg["at"].from_int(at);

		config& input_vars_cfg = sensor_cfg.add_child("input_vars");
		for (std::map<std::string, std::string>::const_iterator it = input_vars.begin(); it != input_vars.end(); ++ it) {
			const std::string& key = it->first;
			const std::string& val = it->second;
			VALIDATE(!key.empty(), null_str);
			input_vars_cfg[key].from_string(val, true);
		}
	}

public:
	int at;
	std::map<std::string, std::string> input_vars;
};

class tcourselist
{
public:
	struct tcourse
	{
		tcourse() {}

		tcourse(int64_t _uid, const std::string& _title)
			: uid(_uid)
			, title(_title)
		{
			VALIDATE(is_valid_courseware_uid(uid), null_str);
			VALIDATE(!title.empty(), null_str);
		}

		void from_string(const std::string& str)
		{
			std::pair<std::string, std::string> pair = utils::split_app_prefix_id(str);
			uid = utils::to_int64(pair.first);
			title = pair.second;
		}

		std::string to_string() const { return utils::join_app_prefix_id(str_cast(uid), title); }

		std::string to_dir_name() const
		{
			if (uid == COURSEWARE_UPLOAD_UID) {
				return title;
			}
			return to_string();
		}

		bool valid() const { return is_valid_courseware_uid(uid) && !title.empty(); }

		int64_t uid;
		std::string title;
	};

	tcourselist()
		: course_separator_('|')
	{}

	bool from_cfg(const config& cfg);
	void to_cfg(config& cfg) const;

	void assign(const tcourselist& that);
	void insert_course(int64_t uid, const std::string& title, int new_at);
	int erase_course(int64_t uid, const std::string& title);

	const std::vector<tcourse>& courses() const { return courses_; }

	struct tkey
	{
		tkey(int64_t uid, const std::string& title)
			: uid(uid)
			, title(title)
		{}

		bool operator<(const tkey& that) const noexcept
		{
			if (uid != that.uid) {
				return uid < that.uid;
			}
			return SDL_strcmp(title.c_str(), that.title.c_str()) < 0;
		}

		int64_t uid;
		std::string title;
	};
	bool is_existed(int64_t uid, const std::string& title) const { return key_2_course_ats_.count(tkey(uid, title)) != 0; }
	const tcourselist::tcourse& get_course(int64_t uid, const std::string& title) const;
	int course_which_at(int64_t uid, const std::string& title) const;

	void clear()
	{
		courses_.clear();
		key_2_course_ats_.clear();
	}

private:
	void verify() const;

private:
	const char course_separator_;
	std::vector<tcourse> courses_;
	std::map<tkey, int> key_2_course_ats_;
};

class tcfg_cpp_api_core
{
public:
	tcfg_cpp_api_core(const std::map<aplt::taplt_key, aplt::tapplet>& applets, 
		aplt::tbg_task& bg_task, tbase_driver_core& base_driver);
	virtual ~tcfg_cpp_api_core();

	void load();
	void save_klink_pb(int type);

	std::map<std::string, aplt::ttask_cpp_pair>& task_pairs() { return task_cpp_pairs_; }
	const std::map<std::string, aplt::ttask_cpp_pair>& buildin_task_pairs() const { return buildin_task_cpp_pairs_; }

	bool cfg_to_task_pairs(const config& root_cfg, std::map<std::string, aplt::ttask_cpp_pair>& result) const;
	bool string_to_task_pairs(const std::string& steram, std::map<std::string, aplt::ttask_cpp_pair>& result) const;
	void task_pairs_to_stringstream(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs, std::stringstream& result) const;
	void sync_fake_aplt_tasks(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs);
	void save_task_pairs(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs);
	void did_gui2task2_back();

	//
	// speech
	//
	std::string& nick() { return nick_; }
	std::map<std::string, aplt::tspeech_sensor>& speech_sensors() { return speech_sensors_; }
	const std::map<std::string, aplt::tspeech_sensor>& buildin_speech_sensors() const { return buildin_speech_sensors_; }
	bool cfg_to_speech_sensors(const config& root_cfg, std::string& nick, std::map<std::string, aplt::tspeech_sensor>& result) const;
	bool string_to_speech_sensors(const std::string& stream, std::string& nick, std::map<std::string, aplt::tspeech_sensor>& result) const;
	void speech_sensors_to_stringstream(const std::string& nick, const std::map<std::string, aplt::tspeech_sensor>& sensors, std::stringstream& result) const;
	void save_speech_sensors(const std::string& nick, std::map<std::string, aplt::tspeech_sensor>& speech_sensors);
/*
	const aplt::taplt_task* find_matched_speech_sensor(const std::string& result,
		std::map<std::string, std::string>& matched_vals, const aplt::tspeech_sensor** matched_sensor, bool& nick_found) const;
*/
	//
	// var_sensor
	//
	std::vector<tvar_sensor>& var_sensors() { return var_sensors_; }
	const std::vector<tvar_sensor>& buildin_var_sensors() const { return buildin_var_sensors_; }
	bool string_to_var_sensors(const std::string& stream, bool sync_with_var_tasks, std::vector<tvar_sensor>& result) const;
	bool cfg_to_var_sensors(const config& root_cfg, bool sync_with_var_tasks, std::vector<tvar_sensor>& result) const;
	void var_sensors_to_cfg(const std::vector<tvar_sensor>& sensors, config& cfg) const;
	void var_sensors_to_stringstream(const std::vector<tvar_sensor>& sensors, std::stringstream& result) const;
	void verity_var_sensors() const;
	void insert_var_sensor(int new_var_at, int builtin_at, const std::string& name);
	void erase_var_sensor(int at);
	void set_var_sensors(const std::vector<tvar_sensor>& var_sensors);

	//
	// timed_sensor
	// 
	std::vector<ttimed_sensor>& timed_sensors() { return timed_sensors_; }
	const std::vector<ttimed_sensor>& buildin_timed_sensors() const { return buildin_timed_sensors_; }
	bool string_to_timed_sensors(const std::string& stream, bool sync_with_var_tasks, std::vector<ttimed_sensor>& result) const;
	bool cfg_to_timed_sensors(const config& root_cfg, bool sync_with_timed_tasks, std::vector<ttimed_sensor>& result) const;
	void timed_sensors_to_cfg(const std::vector<ttimed_sensor>& sensors, const std::set<int>& extract_ats, config& cfg) const;
	void timed_sensors_to_cfg(const std::vector<ttimed_sensor>& sensors, config& cfg) const;
	void timed_sensors_to_stringstream(const std::vector<ttimed_sensor>& sensors, std::stringstream& result) const;
	void verity_timed_sensors() const;
	void insert_timed_sensor(int new_timed_at, int builtin_at, const std::map<std::string, std::string>& input_vals = std::map<std::string, std::string>());
	void erase_timed_sensor(int at);
	void set_timed_sensors(const std::vector<ttimed_sensor>& timed_sensors);
	
	//
	// base_scene
	//
	const std::vector<tbase_scene>& base_scenes() const { return base_scenes_; }
	std::vector<tbase_scene>& mutable_base_scenes() { return base_scenes_; }
	bool cfg_to_base_scenes(const config& root_cfg, std::vector<tbase_scene>& result) const;
	void base_scenes_to_cfg(const std::vector<tbase_scene>& scenes, config& cfg) const;
	void base_scenes_to_stringstream(const std::vector<tbase_scene>& scenes, std::stringstream& result) const;
	bool string_to_base_scenes(const std::string& stream, std::vector<tbase_scene>& result) const;
	const tbase_scene* base_scene_from_id(const std::string& id, bool must_exist) const;
	void set_base_scenes(const std::vector<tbase_scene>& base_scenes);

	//
	// add_timed_task
	//
	const std::map<int64_t, tb_api::tadd_timed_task>& add_timed_tasks() const { return add_timed_tasks_; }
	void set_add_timed_tasks(const std::map<int64_t, tb_api::tadd_timed_task>& tasks);
	void clear_add_timed_tasks() { set_add_timed_tasks(std::map<int64_t, tb_api::tadd_timed_task>()); }
	void add_timed_task_sliced();
	void did_handle_add_timed_task_quit(const bool& dirty);
	enum {handle_type_slice, handle_type_clear, handle_type_count};
	void handle_add_timed_task(int type);
	bool aplt_task_is_same_add_task(const taplt_task& aplt_task, const tb_api::tadd_timed_task& add_task) const;
	bool aplt_task_is_from_add_timed_tasks(const taplt_task& aplt_task) const;
	bool string_to_add_timed_tasks(const std::string& stream, std::map<int64_t, tb_api::tadd_timed_task>& result) const;
	bool cfg_to_add_timed_tasks2(const config& root_cfg, std::map<int64_t, tb_api::tadd_timed_task>& result) const;
	void add_timed_tasks_to_stringstream(const std::map<int64_t, tb_api::tadd_timed_task>& tasks, std::stringstream& result) const;
	void verity_add_timed_tasks() const;

	//
	// courselist
	//
	tcourselist& courselist() { return courselist_; }
	bool is_existed_course(int64_t uid, const std::string& title) const { return courselist_.is_existed(uid, title); }
	const tcourselist::tcourse& get_course(int64_t uid, const std::string& title) const { return courselist_.get_course(uid, title); }
	int course_which_at(int64_t uid, const std::string& title) const { return courselist_.course_which_at(uid, title); }
	bool string_to_courselist(const std::string& stream, tcourselist& result) const;
	// bool cfg_to_courselist(const config& root_cfg, tcourselist& result) const;
	// void courselist_to_cfg(const tcourselist& courselist, config& cfg) const;
	void courselist_to_stringstream(const tcourselist& courselist, std::stringstream& result) const;
	// void verity_timed_sensors() const;
	void insert_course(int64_t uid, const std::string& title, int new_at = nposm);
	void erase_course(int64_t uid, const std::string& title);
	void set_courselist(const tcourselist& courselist);

	//
	// taskpoint
	//
	bool use_taskpoint(const aplt::taplt_task& aplt_tack) const;
	bool has_taskpoint() const { return taskpoint_.get() != nullptr; }
	ttaskpoint& taskpoint() 
	{
		VALIDATE(taskpoint_.get() != nullptr, null_str);
		return *taskpoint_.get(); 
	}
	void save_taskpoint(const taplt_task& klink_cpp_aplt_task2, const aplt::tapplet::ttask& cfg_task, const ttask_vars& task_vars);
	void clear_taskpoint();

	//
	// klink cfg
	//
	std::string import_klink_cfg(const std::string& filename);
	std::string import_task_cpp_cfg(const std::string& filename, std::map<std::string, aplt::ttask_cpp_pair>& pairs_from_cfg);
	std::string import_speech_cfg(const std::string& filename, std::string& nick, std::map<std::string, aplt::tspeech_sensor>& sensors_from_cfg);

private:
/*
	std::string app_start_task(const ttaskpoint* taskpoint) override;
	void app_task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task, bool result) override;
*/
	bool fake_aplt_tasks_dirty(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs);

private:
	virtual void app_load() {}

	virtual void app_did_insert_course(int new_at) {}
	virtual void app_did_erase_course(int erased_at) {}

	virtual int app_ros_cpp_api_curr_state() const { return nposm; }

	virtual uint64_t app_task_cpp_is_valid(const aplt::ttask_cpp_pair& pair, std::string& err_msg) const
	{
		err_msg = "should not call it";
		// return tcookie3f(0, typefield::type_global, typefield::field_id).u64;
		return tcookie3f(0, 0, 1).u64;
	}

	virtual uint64_t app_speech_sensor_is_valid(const aplt::tspeech_sensor& sensor, std::string& err_msg) const
	{
		err_msg = "should not call it";
		// return tcookie3f(0, typefield::type_global, typefield::field_id).u64;
		return tcookie3f(0, 0, 1).u64;
	}


protected:
	const std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	aplt::tbg_task& bg_task_;
	tbase_driver_core& base_driver_;
/*
	tspeech_driver& speech_driver_;
*/
/*
	//
	// parse pinyin
	//
	aplt::tpinyin& pinyin_;
	const int tone_;
	const bool eng_lowercase_;
*/
	//
	// task_cpp
	//
	std::map<std::string, aplt::ttask_cpp_pair> task_cpp_pairs_;
	std::map<std::string, aplt::ttask_cpp_pair> buildin_task_cpp_pairs_;

	//
	// trigger
	//
	std::string nick_;
	std::map<std::string, aplt::tspeech_sensor> speech_sensors_;
	std::map<std::string, aplt::tspeech_sensor> buildin_speech_sensors_;

	//
	// var_sensor
	//
	std::vector<tvar_sensor> var_sensors_;
	std::vector<tvar_sensor> buildin_var_sensors_;

	//
	// timed_sensor
	//
	std::vector<ttimed_sensor> timed_sensors_;
	std::vector<ttimed_sensor> buildin_timed_sensors_;

	//
	// base_scene
	//
	std::vector<tbase_scene> base_scenes_;

	//
	// add_timed_tasks
	//
	std::map<int64_t, tb_api::tadd_timed_task> add_timed_tasks_;
	bool add_timed_task_20sec_handled_;
	int add_timed_tasks_last_shedule_zerotz_t_;

	//
	// courselist
	//
	tcourselist courselist_;

	// taskpoint
	std::unique_ptr<ttaskpoint> taskpoint_;

private:
	//
	// parse pinyin
	//
	aplt::tpinyin& pinyin_;
	const int tone_;
	const bool eng_lowercase_;

};

}

#endif

