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

#ifndef LIBROSE_APLT2_HPP_INCLUDED
#define LIBROSE_APLT2_HPP_INCLUDED

#include "rose_version.hpp"
#include "rose_filesystem_dll.hpp"
#include "rose_prefs.hpp"
#include "config.hpp"
#include "rose_exception.hpp"
#include "rose_var.hpp"

class surface;

namespace cv {
class Mat;
};

namespace tflite {
class tresult;
}

enum {moveit_op_grasp, moveit_op_press_top, moveit_op_press_middle, moveit_op_count};

namespace aplt {

class taplt_task;
class tpinyin;
class twkoscript;

extern LIB3RDPARTY_DECL const bool per_conflicted_always;

enum {src_distribution, src_development, src_studio, src_count};
extern LIB3RDPARTY_DECL const std::map<int, std::string> sources;

// Some scenes require 'task_xxx' be a flag-style value, but it will be stored in 'int' type. 
// Although 'task_xxx' will not exceed 0x7fffffff, it is defined as a continuous value starting from 0 for easy reading.
// In order for the flag-style to be generated, the first valid value must start at 0 and not exceed 31 at most.
enum task_type_t {task_type_nposm = -1, task_moveit, task_ble, task_camera, task_block, 
	task_nonblock, task_aiagent, task_cpp, task_type_count};
extern LIB3RDPARTY_DECL const std::map<task_type_t, std::string> task_types;

LIB3RDPARTY_DECL bool is_single_task(int type);
LIB3RDPARTY_DECL std::string src_bundleid_2_id(int source, const std::string& bundleid);

enum {bundleid_leagor_basic, bundleid_leagor_khome, bundleid_leagor_basiclua, bundleid_leagor_khomelua};
LIB3RDPARTY_DECL std::string get_bundleid(int type);

enum {taskid_basic_alert, taskid_basic_base_scene, taskid_basic_timed_task, taskid_workout, taskid_reserved_count};
extern LIB3RDPARTY_DECL std::map<int, tcode3> reserved_tasks;

enum per_t {per_nposm = -1, per_navigation, per_ble, per_camera, per_moveit, per_count};
LIB3RDPARTY_DECL void fill_per_if_necessary(std::set<per_t>& permissions);

struct tpermission
{
	tpermission(per_t per, const std::string& id, const std::string& name)
		: per(per)
		, id(id)
		, name(name)
	{}

	const per_t per;
	const std::string id;
	const std::string name;
};

extern LIB3RDPARTY_DECL std::map<int, tpermission> sys_permissions;

enum {app_kdesktop, app_launcher, app_count};
extern LIB3RDPARTY_DECL const std::map<int, std::string> apps;

#define APLT_DISTRIBUTION_CFG	"distribution.cfg"

struct taplt_key
{
	taplt_key(int _source, const std::string& _bundleid)
		: source(_source)
		, bundleid(_bundleid)
	{}

	bool operator<(const taplt_key& that) const noexcept
	{
		if (source != that.source) {
			return source < that.source;
		}
		return SDL_strcmp(bundleid.c_str(), that.bundleid.c_str()) < 0;
	}

	const int source;
	const std::string bundleid;
};

struct taplt_var_pair;

class LIB3RDPARTY_DECL tapplet
{
public:
	struct LIB3RDPARTY_DECL tvar
	{
		tvar(const std::string& name, bool input, var_type_t type, bool optional);

		// bool operator==(const tvar& that) const
		// {
		//	return name == that.name && input == that.input && type == that.type && optional == that.optional;
		// }
		// bool operator!=(const tvar& that) const { return !operator==(that); }

		std::string name;
		bool input;
		var_type_t type;
		bool optional;
	};

	struct LIB3RDPARTY_DECL ttask
	{
		ttask(const std::string& id, task_type_t type, int subtype, bool nonpreemptive, bool recoverable, int iot_src, const std::set<per_t>& _permissions, int max_fails, const std::vector<tvar>& vars, bool upload_image, bool no_swap_wh_for_screen);
		std::string name2() const;

		std::string id;
		task_type_t type;
		int subtype;
		bool nonpreemptive;
		bool recoverable;
		int iot_src;
		std::string name; // id's msgstr
		std::set<per_t> permissions;
		int max_fails;
		std::vector<tvar> vars;
		bool upload_image;
		bool no_swap_wh_for_screen;

		std::string py_name;
	};

	tapplet()
		: source(nposm)
		, ts(0)
		, base_driver(false)
		, laser_driver(false)
		, moveit_driver(false)
		, dcamera_driver(false)
		, iot_driver(false)
		, speech_driver(false)
		, ai_driver(false)
		, lua_base_slot(false)
		, lua_task_api(false)
		, fake(false)
		, prefs()
	{
		memset(&rspheader, 0, RSP_HEADER_BYTES);
	}

	~tapplet()
	{
		// if (prefs != nullptr) {
		//	delete prefs;
		// }
	}

	void set_id(int _source, const std::string& _bundleid)
	{
		VALIDATE(id.empty(), null_str);
		VALIDATE(_source >= 0 && _source < src_count, null_str);
		VALIDATE(!_bundleid.empty(), null_str);

		source = _source;
		bundleid = _bundleid;
		id = src_bundleid_2_id(source, bundleid);

		const std::string lua_bundleid = bundleid_2_lua_bundleid(bundleid);
		preferences_dir = get_aplt_user_data_dir(lua_bundleid);
	}

	void set(const std::string& _name, const std::string& _subtitle, const std::string& _username, int64_t _ts, 
		const std::string& _res_path, const std::string& _icon, const std::string& _rose_version)
	{
		VALIDATE(!id.empty(), null_str);

		name = _name;
		subtitle = _subtitle;
		username = _username;
		ts = _ts;
		res_path = _res_path;
		icon = _icon;
		rose_version = _rose_version;
	}

	bool valid() const { return source != nposm && !bundleid.empty() && version.is_rose_recommended() && !res_path.empty(); }

	std::string name2() const
	{
		std::stringstream ss;
		ss << name;
		if (source != src_distribution) {
			ss << "(" + sources.find(source)->second + ")";
		}
		return ss.str();
	}

	void fill_input_output_vars();

	void write_distribution_cfg() const;

	void clear_vars(ttask_vars& task_vars, const std::string& task_id, bool input) const;

	void clear()
	{
		source = nposm;
		bundleid.clear();
		id.clear();

		res_path.clear();
		preferences_dir.clear();

		permissions.clear();
		// objects.clear();
		tasks.clear();

		prefs.clear();
		// cfg.clear();

		input_vars.clear();
		output_vars.clear();
	}

public:
	int source; // src_studio
	std::string bundleid; // aplt.leagor.basic
	std::string id; // aplt.leagor.basic(studio)
	version_info version;
	std::string name;
	std::string subtitle;
	std::string username;
	int64_t ts;
	version_info rose_version;
	// only temporary storage for net cswamp-api: findapplet
	// see thttp_cswamp_device_findapplet::post(...)
	std::string app;

	std::string res_path;
	std::string preferences_dir;
	std::string icon;
	trsp_header rspheader;
	std::set<per_t> permissions;
	std::map<std::string, tapplet::ttask> tasks;
	bool base_driver;
	bool laser_driver;
	bool moveit_driver;
	bool dcamera_driver;
	bool iot_driver;
	bool speech_driver;
	bool ai_driver;
	bool lua_base_slot;
	bool lua_task_api;
	bool fake;
	// Why not use trose_prefs? When operator=, operator==, it is too complicated.
	// config cfg;
	trose_prefs prefs;

	std::vector<taplt_var_pair> input_vars;
	std::vector<taplt_var_pair> output_vars;
};


extern LIB3RDPARTY_DECL tapplet fake_aplt;
extern LIB3RDPARTY_DECL const std::string fake_task_id_empty;
extern LIB3RDPARTY_DECL std::map<std::string, std::string> fake_fixed_tasks;
LIB3RDPARTY_DECL void fake_aplt_erase_nonfixed_tasks();

struct taplt_var_pair
{
public:
	taplt_var_pair(const tapplet& _aplt, const std::string& _task_id, const std::string& _var, const std::string& desc)
		: aplt(&_aplt)
		, task_id(_task_id)
		, var(_var)
		, desc(desc)
		, var2(utils::join_app_prefix_id(_aplt.bundleid, _var))
	{
		if (aplt != &fake_aplt) {
			VALIDATE(!task_id.empty(), null_str);
		}
		VALIDATE(!_var.empty(), null_str);
	}

	bool operator<(const taplt_var_pair& that) const noexcept
	{
		bool this_is_fake_aplt = aplt->bundleid == aplt::fake_aplt.bundleid;
		bool that_is_fake_aplt = that.aplt->bundleid == aplt::fake_aplt.bundleid;
		if (this_is_fake_aplt && !that_is_fake_aplt) {
			return true;
		} else if (!this_is_fake_aplt && that_is_fake_aplt) {
			return false;
		}

		int cmp = SDL_strcmp(aplt->bundleid.c_str(), that.aplt->bundleid.c_str());
		if (cmp < 0) {
			return true;
		} else if (cmp > 0) {
			return false;
		}

		return SDL_strcmp(var.c_str(), that.var.c_str()) < 0;
	}


public:
	const tapplet* aplt;
	// althong @tasks is using std::map<std::string, tapplet::ttask> in tapplet, 
	// for safe in future, here use std::string.
	std::string task_id;
	// id
	std::string var;
	std::string desc;

	// aplt.leagor.khome__id
	std::string var2;
};

LIB3RDPARTY_DECL const tapplet* aplt_from_id(const std::map<taplt_key, tapplet>& applets, const std::string& id);
LIB3RDPARTY_DECL const tapplet* aplt_from_id_ex(const std::map<taplt_key, tapplet>& applets, const std::string& id);
LIB3RDPARTY_DECL tapplet* mutable_aplt_from_id(std::map<taplt_key, tapplet>& applets, const std::string& id);
LIB3RDPARTY_DECL const tapplet* aplt_from_id2(const std::map<taplt_key, tapplet>& applets, int src, const std::string& bundleid);
LIB3RDPARTY_DECL tapplet* mutable_aplt_from_id2(std::map<taplt_key, tapplet>& applets, int src, const std::string& bundleid);
LIB3RDPARTY_DECL const tapplet* aplt_from_bundleid(const std::map<taplt_key, tapplet>& applets, const std::string& bundleid);
LIB3RDPARTY_DECL const tapplet* aplt_from_bundleid_ex(const std::map<taplt_key, tapplet>& applets, const std::string& bundleid);
LIB3RDPARTY_DECL tapplet* mutable_aplt_from_bundleid(std::map<taplt_key, tapplet>& applets, const std::string& bundleid);
LIB3RDPARTY_DECL const tapplet& aplt_from_at(const std::map<taplt_key, tapplet>& applets, int at);
LIB3RDPARTY_DECL tapplet& mutable_aplt_from_at(std::map<taplt_key, tapplet>& applets, int at);

LIB3RDPARTY_DECL void calculate_id_aplt_map(const std::map<taplt_key, tapplet>& applets, std::map<std::string, const tapplet*>& result);


struct ttask_pair
{
	ttask_pair()
		: aplt(nullptr)
		, task(nullptr)
	{}

	ttask_pair(const tapplet& _aplt, const tapplet::ttask& _task)
		: aplt(&_aplt)
		, task(&_task)
	{}

	const tapplet* aplt;
	const tapplet::ttask* task;
};

LIB3RDPARTY_DECL ttask_pair task_pair_from_task_id(const tapplet& aplt, const std::string& task_id, bool must_valid);
LIB3RDPARTY_DECL ttask_pair task_pair_from_2_id(const std::map<taplt_key, tapplet>& applets, const std::string& aplt_id, const std::string& task_id,
	bool include_fake_aplt, bool aplt_id_is_bundleid);

LIB3RDPARTY_DECL ttask_pair task_pair_for_move(const std::map<taplt_key, tapplet>& applets);

LIB3RDPARTY_DECL ttask_pair split_aplt_task_id2(const std::map<taplt_key, tapplet>& applets, const std::string& task_id2, bool aplt_id_is_bundleid);

LIB3RDPARTY_DECL std::string task_name2_from_3id(const std::map<aplt::taplt_key, aplt::tapplet>& applets, 
	const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, bool aplt_id_is_bundleid);

#define task_name2_from_aplt_task(applets, task)	\
	task_name2_from_3id(applets, (task).aplt_id, (task).task_id, (task).ble_device_id, false);


struct ttask_item3f
{
	ttask_item3f(const tapplet& _aplt, const tapplet::ttask& _task, const std::string& _device_id)
		: aplt(&_aplt)
		, task(&_task)
		, device_id(_device_id)
	{}

	const tapplet* aplt;
	const tapplet::ttask* task;
	const std::string device_id;
};

class DECLSPEC treq_task
{
public:
	treq_task();
	virtual ~treq_task() {}

	// enum {type_aplt_task, type_count};

	void set_aplt_task(const tapplet& _aplt, const tapplet::ttask& _task, const std::string& _device_id, 
		const std::string& _position1_uuid, const std::string& _position2_uuid);
	// only used to set tcpp_api.tstate2.async_task
	// void set_aplt_task2(const tapplet& _aplt, const tapplet::ttask& _task, const std::string& _device_id, 
	//	const tif_block& _position1, const tif_block& _position2);

	void assign10(const treq_task& that);

	// virtual bool equal(const treq_task& that) const;
	bool equal10(const treq_task& that) const;

	virtual bool valid() const;
	virtual void clear();

	virtual void validate() const;

public:
	uint32_t id;
	// int type;
	std::string position1_uuid;
	std::string position2_uuid;

	const tapplet* aplt;
	const tapplet::ttask* task;

	std::string ble_device_id;

	// std::vector<std::pair<std::string, tif_block> > input_vars;
	// tif_block position1_if_block;
	// tif_block position2_if_block;
};


// 'mkeys' == abbreviation for 'minor_keys'
enum {mkeys_any_one, mkeys_all_match_and_order, mkeys_all_match_no_order, mkeys_count};

extern LIB3RDPARTY_DECL std::map<int, std::string> minor_key_strategies;
LIB3RDPARTY_DECL int minor_key_strategy_from_str(const std::string& str);

LIB3RDPARTY_DECL void parse_py_words_from_cfg_str(tpinyin& pinyin, int tone, bool eng_lowercase, const std::string& cfg_str, std::vector<std::string>& words, std::vector<std::string>& py_words);
LIB3RDPARTY_DECL bool word_match_fields_from_cfg(tpinyin& pinyin, int tone, bool eng_lowercase, const config& cfg, std::string& major_word, std::string& py_major_word, 
	std::vector<std::string>& minor_words, std::vector<std::string>& py_minor_words, int& strategy);
LIB3RDPARTY_DECL void word_match_fields_to_cfg(const std::string& major_word, const std::vector<std::string>& minor_words, int strategy, config& cfg);
LIB3RDPARTY_DECL bool major_minor_words_match(const std::string& pinyin, const std::string& py_major_word, const std::vector<std::string>& py_minor_words, int strategy, int* any_one_at);

class LIB3RDPARTY_DECL ttaskpoint
{
public:
	ttaskpoint(const taplt_task& klink_cpp_aplt_task2, const tapplet::ttask& cfg_task, int _state,
		const ttask_vars& _task_vars);
	
public:
	// once modify taplt_task's field, for exemple @state, taplt_task's address maybe changed. 
	// so don't save as 'aplt_task& klink_cpp_aplt_task2'.
	const int task_cpp_type;
	const int task_cpp_pb_at;
	const std::string task_name;

	const ttask_vars task_vars;
	const int state;
	const uint32_t ticks;
};

class LIB3RDPARTY_DECL tcpp_api
{
public:
	class LIB3RDPARTY_DECL treq_task2: public aplt::treq_task
	{
	public:
		treq_task2()
			: is_aplt_task(false)
		{}

		// only used to set tcpp_api.tstate2.async_task
		void set_aplt_task2(const tapplet& _aplt, const tapplet::ttask& _task, const std::string& _device_id, 
			const tif_block& _position1, const tif_block& _position2);

		bool gui2_task2_equal(const treq_task2& that) const;

		bool valid() const override;
		void clear() override;

	public:
		bool is_aplt_task;
		std::vector<std::pair<std::string, tif_block> > input_vars;
		tif_block position1_if_block;
		tif_block position2_if_block;

		// use for task_cpp_cfg only, convenient for gui2::ttask2 editing,
		// When parsing task_cpp_cfg, the @aplt/@task may not exist yet.
		// Remember the string format first, and then get @aplt/@task when want to run
		std::string aplt_id;
		std::string task_id;
	};

	struct LIB3RDPARTY_DECL tstate2
	{
		tstate2(int _state, int _threshold_s)
			: state(_state)
			, threshold_s(_threshold_s)
		{
			async_task.clear();
		}

		bool operator==(const tstate2& that) const
		{
			if (state != that.state || threshold_s != that.threshold_s) {
				return false;
			}
			if (!async_task.gui2_task2_equal(that.async_task)) {
				return false;
			}

			if (doing != that.doing || finished != that.finished) {
				return false;
			}

			return true;
		}
		bool operator!=(const tstate2& that) const { return !operator==(that); }

		void sync_task_input_vars(const aplt::tapplet& aplt, const aplt::tapplet::ttask& task);
		bool state_swap(int s1, int s2);

		int state;
		int threshold_s;
		treq_task2 async_task;
		std::string doing;
		tif_block finished;
	};

	class LIB3RDPARTY_DECL tkey_2_state
	{
	public:
		tkey_2_state(int _from_state)
			: from_state(_from_state)
			, strategy(mkeys_any_one)
			, py_minor_words_ready(false)
		{
			// to_state maybe nposm. 
			// VALIDATE(to_state != from_state, null_str);
			// VALIDATE(to_state != nposm, null_str);
		}

		bool state_swap(int s1, int s2);

		bool operator==(const tkey_2_state& that) const
		{
			if (from_state != that.from_state) {
				return false;
			}

			if (major_word != that.major_word || py_major_word != that.py_major_word || 
				minor_words != that.minor_words || py_minor_words != that.py_minor_words ||
				strategy != that.strategy || py_minor_words_ready != that.py_minor_words_ready) {
				return false;
			}

			return true;
		}

	public:
		int from_state;

		std::string major_word;
		std::string py_major_word;
		tif_block minor_words;
		std::vector<std::string> py_minor_words;
		int strategy;
		bool py_minor_words_ready;
	};

	struct LIB3RDPARTY_DECL tnext_state
	{
		tnext_state(int _from_state)
			: from_state(_from_state)
		{}

		tnext_state(int _from_state, const tif_block& _to_state)
			: from_state(_from_state)
			, to_state(_to_state)
		{
			VALIDATE(!to_state.branches.empty(), null_str);
		}

		bool state_swap(int s1, int s2);

		bool operator==(const tnext_state& that) const
		{
			if (from_state != that.from_state || to_state != that.to_state) {
				return false;
			}
			return true;
		}

		int from_state;
		tif_block to_state;
	};

	tcpp_api();
	virtual ~tcpp_api();

	std::string start_task(const std::string& id, ttask_vars& vars, const ttaskpoint* taskpoint);
	void task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task, bool result);
	bool task_started() const { return !task_id_.empty(); }

	virtual void slice() = 0;
	virtual bool speech_did_recognition_result(const std::string& result) { return false; }
	virtual void single_task_finished(const treq_task& req_task, bool result) {}

private:
	virtual std::string app_start_task(const ttaskpoint* taskpoint) { return null_str; }
	virtual void app_task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task, bool result) {}

	// '2th' is abbreviation for 2th class from tcpp_api, for example tros_cpp_api
	virtual void pre_2th_start_task() {}
	virtual void post_2th_start_task() {}

	virtual void post_2th_task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task) {}

	virtual void clear_2th() {}

protected:
	std::string task_id_;
	ttask_vars* task_vars_;

	// class tcfg_cpp_api has a variable member named @taskpoint_
	// const ttaskpoint* taskpoint_;
};

LIB3RDPARTY_DECL void word_match_did_minor_words_changed(tpinyin& pinyin, int tone, bool eng_lowercase, tcpp_api::tkey_2_state& key_2_state);

struct tmoveit_target_info_C
{
	int op;
	SDL_DSize3 size;

	double set_ik_dcpitch;
	double first_dcpitch;
	double second_dcpitch;
};

class LIB3RDPARTY_DECL tmoveit_api
{
public:
	tmoveit_api();
	virtual ~tmoveit_api();

	//
	// moveit aplt session
	//
	tmoveit_target_info_C start_task(const tapplet& aplt, const tapplet::ttask& cfg_task);
	void stop_task(const tapplet& aplt, const tapplet::ttask& cfg_task);

	virtual void camera_work_frame(const surface& surf, const cv::Mat& argb, std::vector<SDL_2Point>& corners) = 0;

	// @corners is copy from camera_work_frame's @corners.
	// @map_xyz is map-xyz value after rotate by dcpitch. unit: m.
	virtual SDL_DPoint3 calculate_TP(const cv::Mat& argb, double dcpitch, const std::vector<SDL_2Point>& corners, const SDL_DPoint3* map_xyz) = 0;

private:
	//
	// moveit aplt session
	//
	virtual void app_start_task(tmoveit_target_info_C& result) {}
	virtual void app_stop_task() {}

protected:
	//
	// moveit aplt session
	//
	std::string task_id_;
	tmoveit_target_info_C target_info_;
};


struct twko_analyze_result_C
{
	double d[WKO_MAX_POSES_PER_TRACK]; // data
	bool satisfied[WKO_MAX_POSES_PER_TRACK];
	int fields;
};

class LIB3RDPARTY_DECL twko_task_slot
{
public:
	virtual bool is_overlay_landmarks() const { return true; }
	virtual bool is_overlay_analyze_msg() const { return true; }

	virtual void set_curr_script(const twkoscript& script, int state_at, int phase_at, uint32_t first_satisfied_ticks, int64_t first_satisfied_ts) {}
	virtual void did_first_satisfied_frame(uint32_t ticks, int64_t ts) {}
	virtual void did_enter_state_and_phase(int state_at, int phase_at) {}
	virtual void did_analyze_result(const twko_analyze_result_C& result) {}
	virtual void render_face_overlay(const SDL_FPoint* norm_xy_landmarks, int count, bool flip_h, cv::Mat& cv_argb) {}
	virtual bool sfx_enabled() const { return false; }
};

class LIB3RDPARTY_DECL tcamera_api
{
public:
	tcamera_api()
		: task_vars_(nullptr)
	{}

	virtual ~tcamera_api() 
	{
		VALIDATE(task_id_.empty(), null_str);
		VALIDATE(task_vars_ == nullptr, null_str);
	}

	std::string start_task(const tapplet& aplt, const tapplet::ttask& cfg_task, aplt::ttask_vars& vars);
	// If the 'start_task' fails, 'task_finished' will not be called.
	void task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task);

	// virtual void post_enter_task() {}
	// virtual void pre_exit_task() {}

	// if finished, slice() return true.
	virtual bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) { return false; }
	virtual bool camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb) { return false; }
	// @argb is from 'tsurface_2_mat_lock lock(@surf)'.
	// if you want to save image, use 'imwrite(@surf)', must not 'imwrite(@argb)'.
	virtual void camera_work_frame(const surface& surf, const cv::Mat& argb) {}

	virtual void set_wko_task_slot(twko_task_slot* slot) {}
	// When tcamera_api start, it doesn't mean tcamera will call OnWorkStart() at this time. 
	// Similarly, when tcamera_api stop, it does not mean that tcamera will stop immediately. 
	// So giving these two here, I think it's no meaning.
	// virtual void OnWorkStart() {}
	// virtual void camera_work_will_stop() {}

	// why return false? 
	// --1) When called, it is usually necessary to judge it as 'true' to make the logic jump. Return 'false', let prevent the jump.
	//   2) When the function not support or return value does not match, return false as well.
	virtual bool is_wko_task_finished() { return false; }

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

private:
	// derive-class modify base @vars.
	virtual std::string app_start_task(aplt::ttask_vars& vars) = 0;
	virtual void app_task_finished(const tapplet::ttask& cfg_task) {}

protected:
	std::string task_id_;
	ttask_vars* task_vars_;
};

class LIB3RDPARTY_DECL tblock_api
{
public:
	tblock_api() {}
	virtual ~tblock_api() {}

	std::string start_task(const tapplet& aplt, const tapplet::ttask& cfg_task, aplt::ttask_vars& vars);

private:
	// derive-class modify base @vars.
	virtual std::string app_start_task(aplt::ttask_vars& vars) = 0;

protected:
	std::string task_id_;
};

extern LIB3RDPARTY_DECL const std::string charge_task_id_charge;

class LIB3RDPARTY_DECL tnonblock_api
{
public:
	enum {subtype_charge, subtype_nlp, subtype_count};

	tnonblock_api() 
		: task_vars_(nullptr)
		, charge_width_(float_nposm)
	{}
	virtual ~tnonblock_api() 
	{
		VALIDATE(task_id_.empty(), null_str);
		VALIDATE(task_vars_ == nullptr, null_str);
	}

	// In the future, there may be a type of task that requires the rotation of the robot to go to alignment.
	std::string start_task(const tapplet& aplt, const tapplet::ttask& cfg_task, aplt::ttask_vars& vars);
	// If the 'start_task' fails, 'task_finished' will not be called.
	void task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task);

	// if finished, slice() return true.
	virtual bool slice() { return false; }

	int subtype() const { return subtype_; }

	//
	// special for subtype_charge.
	//
	virtual void set_charge_width(double charge_width);

	// if no charge-station found, return {nposm, nposm, nposm, nposm}.
	// virtual SDL_2Point did_scan_subscribed(const std::vector<float>& ranges, const SDL_FRange& range_range, 
	//	const SDL_FRange& angle_range, float angle_increment) { return SDL_2Point{nposm, nposm, nposm, nposm}; }

	// I didn't want the '3rdparty' to include any libros, 
	// so use a 'void*', LaserScan_msg's actual type is sensor_msgs::LaserScan.
	virtual SDL_2Point did_scan_subscribed(const void* LaserScan_msg) { return SDL_2Point{nposm, nposm, nposm, nposm}; }

private:
	// derive-class modify base @vars.
	virtual std::string app_start_task(aplt::ttask_vars& vars) = 0;
	virtual void app_task_finished(const tapplet::ttask& cfg_task) {}

protected:
	std::string task_id_;
	ttask_vars* task_vars_;
	int subtype_;
	double charge_width_;
};

class LIB3RDPARTY_DECL taiagent_api
{
public:
	struct LIB3RDPARTY_DECL tprompt
	{
		tprompt(const std::string& prompt, const std::string& note, bool ellipsis_at_start = false)
			: prompt(prompt)
			, note(note)
			, ellipsis_at_start(ellipsis_at_start)
		{}
		
		std::string prompt;
		std::string note;
		bool ellipsis_at_start;
	};

	taiagent_api()
		: task_vars_(nullptr)
		, terminating_(false)
	{
	}
	virtual ~taiagent_api()
	{
		VALIDATE(task_id_.empty(), null_str);
		VALIDATE(task_vars_ == nullptr, null_str);
	}

	const std::vector<tprompt>& prompts(const std::string& task_id, int subject, const std::string& header);
	// virtual std::vector<tprompt> latex_prompts(const std::string& task_id, int subject, const std::string& header);

	// In the future, there may be a type of task that requires the rotation of the robot to go to alignment.
	std::string start_task(const tapplet& aplt, const tapplet::ttask& cfg_task, const std::string& question, const surface& surf, aplt::ttask_vars& vars);
	// If the 'start_task' fails, 'task_finished' will not be called.
	void task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task);

	// if finished, slice() return true.
	virtual bool slice() { return false; }

	void set_terminating()
	{
		VALIDATE(!terminating_, null_str);
		terminating_ = true;
	}
	const bool terminating() const { return terminating_; }

private:
	virtual void app_prompts(const std::string& task_id, int subject, const std::string& header, std::vector<tprompt>& result) = 0;
	// derive-class modify base @vars.
	virtual std::string app_start_task(const std::string& question, const surface& surf, aplt::ttask_vars& vars) = 0;
	virtual void app_task_finished(const tapplet::ttask& cfg_task) {}

protected:
	std::string task_id_;
	ttask_vars* task_vars_;
	bool terminating_;

private:
	std::vector<tprompt> prompts_;
};

// don't define myself class that is derived from ttask_api.
class LIB3RDPARTY_DECL ttask_api
{
public:
	ttask_api(const tapplet& _aplt, tcpp_api* _cpp, tmoveit_api* _moveit, tcamera_api* _camera, tblock_api* _block, tnonblock_api* _nonblock, taiagent_api* aiagent);
	~ttask_api();

public:
	const tapplet& aplt;
	tcpp_api* cpp;
	tmoveit_api* moveit;
	tcamera_api* camera;
	tblock_api* block;
	tnonblock_api* nonblock;
	taiagent_api* aiagent;
};

extern LIB3RDPARTY_DECL std::map<std::string, std::set<const ttask_api*> > task_api_map;

class LIB3RDPARTY_DECL ttask_cpp_pair
{
public:
	ttask_cpp_pair();

	bool from_cfg(const config& cfg);
	bool from_state2_cfg(const config& cfg, std::map<std::string, int>& state_names_map);
	bool from_key_2_state_cfg(const config& cfg, const std::map<std::string, int>& state_names_map);
	void to_cfg(config& cfg) const;
	bool equal(const ttask_cpp_pair& that) const;

	// because member 'pinyin_', cannot use b = a. 
	void assign(const aplt::ttask_cpp_pair& that);

	std::string name2() const 
	{
		std::stringstream ss;
		ss << name << "(" << id << ")";

		return ss.str();
	}

	void sync_task_input_vars(const std::map<aplt::taplt_key, aplt::tapplet>& applets);
	bool state_swap(int s1, int s2);

	// When use gui writes task, some redundant items will be generated, such as req_task.position1, 
	// which will be eliminated in 'green()'. 
	void green();

	std::string py_from_utf8str(const std::string& str) const;

	void clear()
	{
		id.clear();
		name.clear();
		reception_state = nposm;
		nonpreemptive = false;
		recoverable = false;
		startup_state.clear();

		state_names.clear();
		states.clear();
		key_2_states.clear();
	}

public:
	std::string id;
	std::string name;
	int reception_state;
	bool nonpreemptive;
	bool recoverable;
	tif_block startup_state;

	std::vector<std::string> state_names;
	std::map<int, tcpp_api::tstate2> states;
	std::vector<tcpp_api::tkey_2_state> key_2_states;

private:
	//
	// parse pinyin
	//
	aplt::tpinyin& pinyin_;
	const int tone_;
	const bool eng_lowercase_;
	const std::string input_var_signature_;
};

//
// iot
//
#define IOT_DEVICE_ID_CHARS_MIN		4
#define IOT_DEVICE_ID_CHARS_MAX		36 // 3641E31E-36BF-4E03-8879-DE33ADC07D68

enum {iot_src_sensor_min, iot_src_doorbell = iot_src_sensor_min, iot_src_doorcontact, iot_src_ir_motion_sensor, iot_src_smoke_sensor, iot_src_gas_sensor,
	iot_src_co_sensor, iot_src_water_leak_sensor, iot_src_vibration_sensor, iot_src_infrated_emission_detector, 
	iot_src_glass_break_detector, iot_src_sos_button, iot_src_remote_controller, iot_src_keypad, iot_src_sensor_max = iot_src_keypad,

	iot_src_device_min = 1000, iot_src_lamp = iot_src_device_min, iot_src_curtain, iot_src_device_max = iot_src_curtain};

enum {iot_evt_pressdown, iot_evt_pressup, iot_evt_trigger, iot_evt_open, iot_evt_close, iot_evt_count};

// why use suffix '2'? 
//   --is made possible to distinguish between the occurrence of 'int iot_src'.
struct tiot_src2
{
	tiot_src2(int code, const std::string& id, const std::string& name, const std::set<int>& events)
		: code(code)
		, id(id)
		, name(name)
		, events(events)
	{}

	const int code;
	const std::string id;
	const std::string name;
	std::set<int> events;
};

extern LIB3RDPARTY_DECL std::map<int, tiot_src2> iot_sources;
extern LIB3RDPARTY_DECL std::map<int, tcode3> iot_events;
LIB3RDPARTY_DECL int iot_src_from_str(const std::string& str);
LIB3RDPARTY_DECL int iot_event_from_str(const std::string& str);

class LIB3RDPARTY_DECL tiot_heartbeat
{
public:
	tiot_heartbeat(int64_t _t, int _src, const std::string& _device_id, const std::string& _icon)
		: t(_t)
		, src(_src)
		, device_id(_device_id)
		, icon(_icon)
	{}

	bool operator<(const tiot_heartbeat& that) const noexcept;

public:
	int64_t t;
	int src;
	std::string device_id;
	std::string icon;
};

class LIB3RDPARTY_DECL tiot_event
{
public:
	tiot_event(int64_t _t, int _src, int _evt, const std::string& _device_id, const std::string& _icon)
		: t(_t)
		, src(_src)
		, evt(_evt)
		, device_id(_device_id)
		, icon(_icon)
	{}

	bool operator<(const tiot_event& that) const noexcept;

public:
	int64_t t;
	int src;
	int evt;
	std::string device_id;
	std::string icon;
};

enum {chatsrc_speech, chatsrc_aplt_task, chatsrc_ai_chat, chatsrc_summary, chatsrc_aiagent, chatsrc_count};

//
// tslot__subscriber
//
class LIB3RDPARTY_DECL tslot_subscriber
{
public:
	tslot_subscriber() {}
	virtual ~tslot_subscriber() {}

	// 
	// tiot_slot
	//
	virtual void iot_did_heartbeats(const std::set<tiot_heartbeat>& heartbeats) = 0;
	virtual void iot_did_events(const std::set<tiot_event>& events) = 0;

	//
	// tspeech_slot
	//
	virtual void speech_did_capture_audio(const uint8_t* stream, int len) {}
	virtual void speech_send_nlp_question(const std::string& question) {}
	virtual void aiagent_did_nlp_answer(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens) {}

	// if @retval is true, indicate receiver handle @result, speech_driver don't handle it again.
	virtual bool speech_did_recognition_result(const std::string& result) { return false; }

	// if @retval is true, speech_driver must not call ros_.request_task(req_task_).
	virtual bool speech_can_request_task() { return true; }
};


enum priority_t {priority_temp = nposm, priority_nontimed_min = 1, priority_timed = 3, 
	priority_nontimed_def = 6, priority_preempt_aiagent = 8, priority_nontimed_max = 9};

extern LIB3RDPARTY_DECL std::map<int, std::string> nontimed_priorities;
extern LIB3RDPARTY_DECL const std::string& nontimed_priority_name(int priority);


//
// taplt_task
//
class LIB3RDPARTY_DECL taplt_task
{
public:
	enum {type_temp, type_iot, type_tasks_min = type_iot, type_speech, type_var, type_timed, type_tasks_max = type_timed, type_count};
	enum {state_fresh, state_running, state_finished_ok, state_finished_fail, state_finished_expired,
		state_apltnotfound, state_verdismatch, state_count};

	// <aosp>/libcore/ojluni/src/main/java/java/lang/Thread.java
	// enum {priority_min = 1, priority_normal = 5, priority_max = 10};

	taplt_task()
		: type(nposm)
		, priority(nposm)
	{}

	// for ttemp_aplt_task
	taplt_task(const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, 
		const std::string& position1, const std::string& position2, int state);

	// for tevent_aplt_task
	taplt_task(int priority, const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, 
		const std::string& position1, const std::string& position2, int state, int pb_at, 
		int iot_src, int src_evt, const std::string& src_device_id);

	// for tspeech_aplt_task
	taplt_task(int priority, const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, 
		const std::string& position1, const std::string& position2, int state, int pb_at, 
		const std::string speech_id);

	// for tvar_aplt_task
	// To distinguish ttimed_aplt_task, declaring function do not take @ble_device_id, it always null_str when tvar_aplt_task.
	taplt_task(const std::string& aplt_id, const std::string& task_id,
		const std::string& position1, const std::string& position2, int state, int pb_at, 
		int var_at, int aux_key_id);

	// for ttimed_aplt_task
	taplt_task(const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, 
		const std::string& position1, const std::string& position2, int state, int pb_at, 
		int zerotz_t, int timed_at, int aux_key_id);

	~taplt_task() {}

	// bool operator<(const taplt_task& that) const noexcept;

	bool is_klink() const { return type == type_iot || type == type_speech || type == type_timed || type == type_var; }
	bool is_nontimed_priority() const { return type == type_iot || type == type_speech; }

	bool equal8(const aplt::taplt_task& that) const;

	void to_cfg(config& cfg) const;

private:
	// compaire equal8(), no zerotz_t
	bool equal7(const aplt::taplt_task& that) const
	{
		return pb_at == that.pb_at && priority == that.priority &&
			aplt_id == that.aplt_id && task_id == that.task_id && ble_device_id == that.ble_device_id &&
			position1 == that.position1 && position2 == that.position2;
	}

public:
	enum {FLAG_PRIORITY = 0x1, FLAG_APLT_ID = 0x2, FLAG_TASK_ID = 0x4, FLAG_BLE_DEVICE_ID = 0x8, 
		FLAG_POSITION1 = 0x10, FLAG_POSITION2 = 0x20, FLAG_STATE = 0x40,
		FLAG_ZEROTZ_T = 0x1000,
	};

	int type;
	int priority;
	std::string aplt_id;
	std::string task_id;
	std::string position1;
	std::string position2;
	int state;

	// task in std::set<ttask> is sorted, but in pb is not sorted, 
	// there needs to be a field to tell which one to correspond.
	int pb_at;

	// Number of failures
	int fails;

	// last shedule ticks
	uint32_t last_shedule_ticks;

	// only for tiot_aplt_task
	int iot_src;
	int src_evt;
	std::string src_device_id;

	// only for tspeech_aplt_task
	std::string speech_id;

	// only for tvar_aplt_task
	int var_at;

	// only for ttimed_aplt_task
	int zerotz_t;
	std::string ble_device_id;
	int timed_at;

	int aux_key_id;
};

// #define aplt_task_is_klink(type)	((type) == aplt::taplt_task::type_iot || (type) == aplt::taplt_task::type_speech || (type) == taplt_task::type_timed)
// #define aplt_task_is_nontimed(type) ((type) == aplt::taplt_task::type_iot || type == aplt::taplt_task::type_speech)

#define IS_VALID_APLT_TASK_NONTIMED_PRIORITY(priority)		((priority) >= aplt::priority_nontimed_min && (priority) <= aplt::priority_nontimed_max && (priority) != aplt::priority_timed)
#define IS_VALID_APLT_TASK_TIMED_PRIORITY(priority)		((priority) == aplt::priority_timed)
#define IS_VALID_APLT_TASK_PRIORITY(priority)				(IS_VALID_APLT_TASK_NONTIMED_PRIORITY(priority) || IS_VALID_APLT_TASK_TIMED_PRIORITY(priority))

#define IS_VALID_APLT_TASK_STATE(state)		((state) >= 0 && (state) < aplt::taplt_task::state_count)
#define IS_VALID_APLT_TASK_IOT_SRC(src)	((src) >= iot_src_sensor_min && (src) <= iot_src_sensor_max || (src) >= iot_src_device_min && (src) <= iot_src_device_max)
#define IS_VALID_APLT_TASK_IOT_SRC_EVT(evt)	((evt) >= 0 && (evt) < iot_evt_count)

#define MAX_KLINK_VARS	10
#define IS_VALID_APLT_TASK_VAR_AT(var_at)	((var_at) >= 0 && (var_at) < MAX_KLINK_VARS)
#define IS_VALID_APLT_TASK_TIMED_ZEROTZ_T(zerotz_t)	((zerotz_t) >= 0 && (zerotz_t) < ONE_DAY_SECONDS)

#define MAX_KLINK_SINGLE_TASKS	100
#define IS_VALID_APLT_TASK_TIMED_AT(timed_at)	((timed_at) >= 0 && (timed_at) < MAX_KLINK_SINGLE_TASKS)
#define IS_VALID_APLT_TASK_AUX_KEY_ID(aux_key_id)	((aux_key_id) >= 0 && (aux_key_id) < MAX_KLINK_SINGLE_TASKS)

extern LIB3RDPARTY_DECL const std::map<int, std::string> aplt_task_types;
LIB3RDPARTY_DECL int aplt_task_type_from_str(const std::string& str);

struct LIB3RDPARTY_DECL taplt_task_key
{
	taplt_task_key(int _iot_src, int _src_evt, const std::string& _src_device_id)
		: type(taplt_task::type_iot)
		// , priority(_priority)
		, iot_src(_iot_src)
		, src_evt(_src_evt)
		, src_device_id(_src_device_id)
		// , var_at(nposm)
		, aux_key_id(nposm)
	{
		// VALIDATE(IS_VALID_APLT_TASK_PRIORITY(priority), null_str);
		VALIDATE(IS_VALID_APLT_TASK_IOT_SRC(iot_src), null_str);
		VALIDATE(!src_device_id.empty(), null_str);
	}

	taplt_task_key(const std::string& _speech_id)
		: type(taplt_task::type_speech)
		// , priority(_priority)
		, iot_src(nposm)
		, src_evt(nposm)
		, speech_id(_speech_id)
		// , var_at(nposm)
		, aux_key_id(nposm)
	{
		// VALIDATE(IS_VALID_APLT_TASK_PRIORITY(priority), null_str);
		VALIDATE(!speech_id.empty(), null_str);
	}

	// var aplt_task
	taplt_task_key(int _aux_key_id)
		: type(taplt_task::type_var)
		// , priority(_priority)
		, iot_src(nposm)
		, src_evt(nposm)
		// , var_at(_var_at)
		, aux_key_id(_aux_key_id)
	{
		// VALIDATE(IS_VALID_APLT_TASK_PRIORITY(priority), null_str);
		// VALIDATE(IS_VALID_APLT_TASK_VAR_AT(var_at), null_str);
		VALIDATE(IS_VALID_APLT_TASK_AUX_KEY_ID(aux_key_id), null_str);
	}

	// timed aplt_task
	taplt_task_key(const std::string& _aplt_id, const std::string& _task_id, const std::string& _ble_device_id, int _aux_key_id)
		: type(taplt_task::type_timed)
		// , priority(_priority)
		, iot_src(nposm)
		, src_evt(nposm)
		// , var_at(nposm)
		, aplt_id(_aplt_id)
		, task_id(_task_id)
		, ble_device_id(_ble_device_id)
		, aux_key_id(_aux_key_id)
	{
		// VALIDATE(IS_VALID_APLT_TASK_PRIORITY(priority), null_str);
		VALIDATE(IS_VALID_APLT_TASK_AUX_KEY_ID(aux_key_id), null_str);
	}

	bool operator<(const taplt_task_key& that) const noexcept;

	const int type;
	// const int priority;

	// only for tiot_aplt_task
	const int iot_src;
	const int src_evt;
	std::string src_device_id;

	// only for tspeech_aplt_task
	const std::string speech_id;

	// only for tvar_aplt_task
	// const int var_at;

	// only for ttimed_aplt_task
	const std::string aplt_id;
	const std::string task_id;
	const std::string ble_device_id;

	const int aux_key_id;
};

LIB3RDPARTY_DECL taplt_task_key aplt_task_key_from_aplt_task(const taplt_task& task);
LIB3RDPARTY_DECL int next_aux_key_id(const std::map<taplt_task_key, taplt_task>& tasks);


struct tiot_device_key
{
	tiot_device_key(int _src, const std::string& _device_id)
		: src(_src)
		, device_id(_device_id)
	{}

	bool operator<(const tiot_device_key& that) const noexcept
	{
		if (src != that.src) {
			return src < that.src;
		}
		return SDL_strcmp(device_id.c_str(), that.device_id.c_str()) < 0;
	}

	const int src;
	const std::string device_id;
};

// TB1-
#define ROSE_IOT_ALIAS_ID_SIZE		2 // must be 2
#define MIN_ROSE_IOT_ALIAS_SIZE		5 // 4+name(name must not empty)

class LIB3RDPARTY_DECL tiot_device
{
public:
	enum {FLAG_ALIAS = 0x1, FLAG_ICON = 0x2, FLAG_TS = 0x4};

	tiot_device(int _src, const std::string& _device_id, const std::string& _alias, const std::string& _icon, int64_t _ts, int _pb_at, int _number = nposm)
		: src(_src)
		, device_id(_device_id)
		, alias(_alias)
		, icon(_icon)
		, ts(_ts)
		, pb_at(_pb_at)
		, number(_number)
	{
		VALIDATE(IS_VALID_APLT_TASK_IOT_SRC(src), null_str);
		VALIDATE(!device_id.empty(), null_str);
		VALIDATE(pb_at >= 0, null_str);
	}

	virtual ~tiot_device() {}

	std::string alias2() const;

	bool operator<(const tiot_device& that) const noexcept;

public:
	const int src;
	const std::string device_id;
		
	std::string alias;
	std::string icon;
	int64_t ts;
	int pb_at;

	int number;
	std::string alias_name;
};

LIB3RDPARTY_DECL bool split_iot_alias(const std::string& alias, const std::string& id, int* num_ptr, std::string* name_ptr);
LIB3RDPARTY_DECL bool is_valid_iot_alias_id(const std::string& id);
LIB3RDPARTY_DECL std::string revise_iot_alias_id(const std::string& id, const std::string& def);

// sts: SubTaskState
enum {sts_nposm, sts_ing, sts_idle, sts_preempted, sts_count};
#define sts_is_idle2(state)		((state) == aplt::sts_idle || (state) == aplt::sts_preempted)

class LIB3RDPARTY_DECL tbase_scene
{
public:
	tbase_scene();

	void set_name(const std::string& name);
	const std::string& name() const { return name_; }
	std::string id_for_gui(int max_chars = nposm) const;
	std::string name_for_gui(int max_chars = nposm) const;
	std::string file_var_val() const;

	void validate() const
	{
		VALIDATE(!id.empty() && !name_.empty() && utils::is_rose_bundleid(aplt, '.') && !task.empty(), null_str);
	}

	std::string join_input_vars() const
	{
		std::stringstream ss;
		for (std::map<std::string, std::string>::const_iterator it = input_vars.begin(); it != input_vars.end(); ++ it) {
			const std::string& key = it->first;
			const std::string& val = it->second;
			VALIDATE(!key.empty(), null_str);
			if (!ss.str().empty()) {
				ss << ", ";
			}
			ss << key << " = " << val;
		}
		return ss.str();
	}

public:
	std::string file_key;
	std::string id;
	std::string aplt;
	std::string task;
	int amp;
	std::map<std::string, std::string> input_vars;

	std::string py_name;

private:
	std::string name_;
};

// log that in logs.pb
enum {tokensflag_new_conversation = 0x1 /*tokensflag_summary_req = 0x2, tokensflag_summary_resp = 0x4*/};
LIB3RDPARTY_DECL uint64_t join_log_tokens(int input, int output, uint16_t flags);
LIB3RDPARTY_DECL uint16_t split_log_tokens(uint64_t tokens, int* input_ptr, int* output_ptr);

#define COURSEWARE_UPLOAD_UID			0
#define is_valid_courseware_uid(uid)	(is_valid_uid(uid) || (uid) == COURSEWARE_UPLOAD_UID)
 
//
// health
//
#define WKO_MAX_PHASE_COUNT		2

enum {sitevttype_good, sitevttype_improper, sitevttype_noperson, sitevttype_sedentary, sitevttype_count,
	workoutevt_n32 = sitevttype_count, workoutevt_str, healthevt_count};

// ctx of sitevttype_posture_good
enum {sitgood_new_one};
// ctx of sitevttype_posture_improper
enum {
	// common reason
	sitimproper_all_captured, 
	// sit front special reason
	sitimproper_shoulder_x_in_middle, 
	sitimproper_not_too_close,
	sitimproper_face_not_too_crooked,
	sitimproper_head_not_too_low,
	sitimproper_parallel_not_too_crooked,

	// sit side special reason
	sitimproper_head_not_too_forward,
	sitimproper_spine_not_too_bending,
	sitimproper_shoulder_width_not_too_broad,
	sitimproper_spine_not_too_long,
	sitimproper_hip_not_too_high,

	sitimproper_reason_count, // count

	// The value of sitimproper_not_reason_min cannot be produced under any of the above reasons.
	sitimproper_not_reason_min = 0x10000,
	sitimproper_alert = sitimproper_not_reason_min,
};
// ctx of sitevttype_noperson
enum {sitnoperson_alert};
// ctx of sitevttype_sedentary
enum {sitsedentary_alert};

// ctx of workoutevt_n32
enum {workoutn32_max_states = 64, // [0, 63] is enter state.

	workoutn32_wkoscript_index_min = 100, // in future, may increase workoutn32_max_states.
	workoutn32_wkoscript_index_max = 179,

	workoutn32_state_subphase_min = 180,
	workoutn32_active_period_start = workoutn32_state_subphase_min,
	workoutn32_cooldown_period_start,
	// in future, may increase subphase in state. max up to 199.
	workoutn32_state_subphase_max = workoutn32_cooldown_period_start,

	workoutn32_analyze_track_pose_result_min = 200,
	workoutn32_satisfied_landmarks = workoutn32_analyze_track_pose_result_min,
	workoutn32_reserve0,
	workoutn32_unsatisfied_reason_min,
	workoutn32_unsatisfied_isnan = workoutn32_unsatisfied_reason_min,
	workoutn32_unsatisfied_absend_landmark,
	workoutn32_unsatisfied_pose_min = 220,
	workoutn32_unsatisfied_reason_max = 299,

	workoutn32_not_range_min = 0x10000,
	workoutn32_first_satisfied = workoutn32_not_range_min,
	workoutn32_reserve1,
	workoutn32_reserve2,
	workoutn32_alert,
	workoutn32_rep_complete,
};

// ctx of workoutevt_str
enum {workoutstr_start};

#define wkon32_ctx_is_unsatisfied_reason(ctx) \
	((ctx) >= aplt::workoutn32_unsatisfied_reason_min && (ctx) <= aplt::workoutn32_unsatisfied_reason_max)
#define wkon32_ctx_is_analyze_track_pose_result(ctx) \
	((ctx) >= aplt::workoutn32_analyze_track_pose_result_min && (ctx) <= aplt::workoutn32_unsatisfied_reason_max)


// implemented by libroseaplt.so(ros_aplt_so.cpp)
// Every libroseaplt.so has their own curr_aplt.
extern tapplet* curr_aplt;

}


#endif