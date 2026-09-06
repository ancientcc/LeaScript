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

#ifndef LIBROSE_APLT_HPP
#define LIBROSE_APLT_HPP

#include "aplt2.hpp"
#include "aplt_clazz.hpp"
#include "scripts/lua_kernel_base.hpp" // for lua_kernel_base
#include "scripts/gui/widgets/vwidget.hpp"
#include "rose_version.hpp"
#include "filesystem.hpp"
#include "klink.pb.h"


class base_instance;

#define APPLET_ICON		"icon_192.png"
#define MAX_APPLETS		99
enum {navigation_bh_luafunc, navigation_bh_count};

namespace aplt {

enum {builtinid_store = MAX_APPLETS, builtinid_settings, builtinid_klink, builtinid_speech, builtinid_task, builtinid_courseware, builtinid_mkscript, builtinid_health, builtinid_map, 
	builtinid_moveit, builtinid_dnn, builtinid_explorer, builtinid_dcamera, builtinid_center, builtinid_mic, builtinid_count};

struct tbuildin
{
	tbuildin(int id, const std::string& icon, const std::string& name)
		: id(id)
		, icon(icon)
		, name(name)
	{
		VALIDATE(id >= 0 && id < builtinid_count, null_str);
	}

	bool valid() const { return id >= 0 && id < builtinid_count && !icon.empty() && !name.empty(); }

	int id;
	const std::string icon;
	const std::string name;
};

extern const version_info min_aplt_rose_ver;

extern const std::string file_launcher_android;
extern const std::string file_kdesktop_android;
extern const std::string file_chinese_pinyin;
extern const std::string file_latex_data;

extern std::map<int, tbuildin> all_fake_applets;
extern std::map<int, tcode3> aplt_drivers;
extern char base_subtask_states[sts_count][48];

class tdisable_new_klink_task_lock
{
public:
	// why remark reason_fg_aplt? 
	//  --I'm not sure if want to allow a fg applet and klink_tasks to run at the same time.
	enum {reason_map, reason_settings, reason_trigger, reason_task, reason_dnn, reason_moveit, reason_explorer, reason_camera, 
		// reason_fg_aplt,
		reason_netxmit, reason_count};

	static bool disabled() { return reason != nposm; }
	static std::string desc();

	tdisable_new_klink_task_lock(int _reason, bool _only_disable_fake_aplt_task = false);
	~tdisable_new_klink_task_lock();


private:
	static int reason;
	// only disable fake_aplt_task(aplt::fake_aplt's task)
	static bool only_disable_fake_aplt_task;
	static std::map<int, std::string> reasons;

	int original_;
	bool original_only_disable_fake_aplt_task_;
};

bool new_klink_task_disabled2(std::string* desc);

void get_settings_cfg(const std::string& aplt_path, bool alert_valid, tapplet& aplt);
config get_distribution_cfg(const std::string& aplt_path);
void initial();
void load_applets_from_disk(std::map<taplt_key, tapplet>& applets);
per_t conflicted_permission(const std::set<per_t>& pers1, const std::set<per_t>& pers2);
std::string aplt_can_run(const tapplet& aplt);
int app_code_from_str(const std::string& app);
void aplt_set_msgstr(tapplet& aplt);
void aplt_set_pinyin(tapplet& aplt);
void setup_aplt_user_data_dir(const std::string& aplt_preferences_dir);

class texecutor
{
public:
	texecutor(rose_lua_kernel& lua, const tapplet& aplt);

	void run();

private:
	rose_lua_kernel& lua_;
	const tapplet& aplt_;
	std::string lua_bundleid_;
	lua_State* L;
};


// enum {tasks_type_speech, tasks_iot, tasks_type_timing, tasks_type_count};
#define MAX_KLINK_SCENE_NAME_BYTES		24

class tbg_task
{
public:
	class tbase_bg_task2
	{
	public:
		tbase_bg_task2(aplt::tbg_task& bg_task)
			: bg_task_(bg_task)
			, curr_aplt(nullptr)
			, aplt_task(nullptr)
			, klink_cpp_aplt_task2(nullptr)
			, klink_single_aplt_task2(nullptr)
		{
		}
		virtual ~tbase_bg_task2() {}

		// virtual bool shedule() { return false; }

		virtual void aplt_task_pre_navigation() {}
		virtual void aplt_task_navigation_stopped(bool nav2th, bool result) {}
		virtual void task_finished(const aplt::taplt_task* aplt_task) {}

		virtual bool in_task_cpp() const { return false; }
		virtual bool in_pure_task_cpp() const { return false; }
		virtual ttask_pair task_cpp_pair() const { return ttask_pair(); }

		virtual const ttaskpoint* get_taskpoint() const { return nullptr; }
		virtual const tapplet::ttask* get_aiagent_cfg_task() const { return nullptr; }

		int in_which_single_task(const std::map<taplt_key, tapplet>& applets) const;

		const ttask_vars& task_vars() const { return task_vars_; }
		ttask_vars& mutable_task_vars() { return task_vars_; }

		void set_task_var(const std::string& name, bool is_array, const config::attribute_value& val);
		void erase_task_var(const std::string& name);

	public:
		tapplet* curr_aplt;
		std::set<per_t> permissions;

		// @aplt_task must be pointer to a single task.
		aplt::taplt_task* aplt_task;

		//
		// klink_xxx_aplt_task2: if not nullptr, this task is requested from klink-gui.
		//   A task can only be 'cpp' or 'single', these two variables are mutually exclusive.
		// In other words, one is not-nullptr, and the other can only be nullptr.
		//   There may be other places that will use 'klink_xxx_aplt_task' as variable name, 
		// in order to reduce the name-confusion, the suffix '2' was added.
		const aplt::taplt_task* klink_cpp_aplt_task2;
		const aplt::taplt_task* klink_single_aplt_task2;

	protected:
		aplt::tbg_task& bg_task_;
		ttask_vars task_vars_;
	};

	tbg_task(base_instance& _instance, std::map<taplt_key, tapplet>& applets);
	~tbg_task();
	posix_noncopyable(tbg_task);

	void set_bg_task2(tbase_bg_task2& bg_task2);

	const tbase_bg_task2& bg_task2() const 
	{
		VALIDATE(is_ing_, null_str);
		return *bg_task2_; 
	}
	tbase_bg_task2& mutable_bg_task2() 
	{ 
		VALIDATE(is_ing_, null_str);
		return *bg_task2_; 
	}

	// Try not to use it. You should know the consequences when using it
	const tbase_bg_task2& bg_task2_unsafe() const 
	{
		return *bg_task2_; 
	}

	void set_lua(rose_lua_kernel& lua);

	const std::string& scene_name() const { return scene_name_; }
	void set_scene_name(const std::string& name, bool write_pb); 
	
	const std::map<taplt_task_key, taplt_task>& get_tasks(int tasks_type) const
	{
		if (tasks_type == taplt_task::type_iot) {
			return iot_tasks_;
		} else if (tasks_type == taplt_task::type_speech) {
			return speech_tasks_;
		} else if (tasks_type == taplt_task::type_var) {
			return var_tasks_;
		} 
		VALIDATE(tasks_type == taplt_task::type_timed, null_str);
		return timed_tasks_;
	}

	std::map<taplt_task_key, taplt_task>& get_mutable_tasks(int tasks_type)
	{
		if (tasks_type == taplt_task::type_iot) {
			return iot_tasks_;
		} else if (tasks_type == taplt_task::type_speech) {
			return speech_tasks_;
		} else if (tasks_type == taplt_task::type_var) {
			return var_tasks_;
		} 
		VALIDATE(tasks_type == taplt_task::type_timed, null_str);
		return timed_tasks_;
	}
	bool in_4tasks(const taplt_task& aplt_task) const;
	bool in_4tasks_use_ptr(const taplt_task& aplt_task) const;

	void insert_iot_task(int priority, int state, int src, int src_evt, const std::string& src_device_id);
	void insert_speech_task(int priority, int state, const std::string& speech_id);
	const taplt_task* insert_var_task(int state);
	const taplt_task* insert_timed_task(const std::string& aplt_id, const std::string& task_id, const std::string& pin, int state, int zerotz_t);
	void erase_task2(int tasks_type, const aplt::taplt_task& task);
	// if zerotz_t == nposm, don't modify.
	// if state == nposm, don't modify.
	// Note: the original-task will be destroyed, 
	//       and caller must not use original-task, use the returned-task instead
	void modify_task(const taplt_task& task, const taplt_task& val_task, uint32_t flags, bool write_pb);
	void modify_task_state(const taplt_task& aplt_task, int state, bool write_pb);
	void modify_task_last_shedule_ticks(const taplt_task& aplt_task, uint32_t ticks);
	void set_4tasks(const std::string& scene_name, const std::map<taplt_task_key, taplt_task>& iot_tasks, const std::map<taplt_task_key, taplt_task>& speech_tasks,
		const std::map<taplt_task_key, taplt_task>& var_tasks, const std::map<taplt_task_key, taplt_task>& timed_tasks, const std::vector<aplt::tiot_device>& vec_iot_devices, bool write_pb);

	const aplt::taplt_task& task_from_at(int tasks_type, int at) const;
	const aplt::taplt_task& task_from_pb_at(int tasks_type, int pb_at) const;
	const aplt::taplt_task* iot_task_by_iot_evt(int src, int evt, const std::string& device_id);
	const aplt::taplt_task* speech_task_by_speech_id(const std::string& speech_id);

	void pump();
	void set_luafunc_finished();
	void set_nav2th_ing();
	void set_nav2th_finished();
	void set_task_finished();
	void request_klink_aplt_task(const taplt_task& aplt_task, const aplt::ttask_vars& task_vars);
	void stop2(bool fail_retry);

	// @original_in_pure_task_cpp. 
	//   what instance->app_in_pure_task_cpp() value? before call ttemp_task::shedule().
	void start_sys2(tbg_task::tbase_bg_task2& sys_task, bool original_in_pure_task_cpp);

	bool priority_can_run(const aplt::taplt_task& desire, std::string* msg_result) const;

	const std::map<taplt_task_key, taplt_task>& iot_tasks() const { return iot_tasks_; }
	const std::map<taplt_task_key, taplt_task>& speech_tasks() const { return speech_tasks_; }
	const std::map<taplt_task_key, taplt_task>& timed_tasks() const { return timed_tasks_; }
	const std::map<taplt_task_key, taplt_task>& var_tasks() const { return var_tasks_; }
	const std::map<tiot_device_key, tiot_device>& iot_devices() const { return iot_devices_; }

	bool is_ing() const { return is_ing_; }
	int in_which_single_task() const
	{
		VALIDATE(is_ing(), null_str);
		return bg_task2_->in_which_single_task(applets_);
	}

	// Why add the suffix '2', in the tbg_task, many places will use 'in_task_cpp' as a temporary variable name.
	bool in_task_cpp2() const { return is_ing_ && bg_task2_->in_task_cpp(); }

	const aplt::tapplet* task_cpp_aplt(const tapplet::ttask** task) const;
	// on ios, must not define function name with current_task(), so if want, use current_task1.

	const aplt::taplt_task* klink_task_if_ing() const;

	int finished2() const { return running_.finished2; }
	enum {finished_luafunc, ing_nav2th, finished_nav2th, finished_count};
	bool is_luafunc_finished() const { return running_.finished2 >= finished_luafunc; }

	const std::string& task_state_desc(int state) const;
	bool state_can_fresh(int state) const
	{ 
		return state == aplt::taplt_task::state_finished_ok || state == aplt::taplt_task::state_finished_fail || state == aplt::taplt_task::state_finished_expired; 
	}
	bool state_is_closed(int state) const 
	{ 
		return state == aplt::taplt_task::state_apltnotfound || state == aplt::taplt_task::state_verdismatch; 
	}

	// unit: second
	int overdue_threshold() const { return overdue_threshold_; }

	void app_load_pb(int type);
	pb2::tklink& pb_klink() { return pb_klink_; }
	void add_log2(int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent);

	void insert_iot_device(int src, const std::string& device_id);
	// why not use 'std::string& device_id'?
	// --Caller might pass hit_iot_device.device_id as the value of @device_id, and hit_iot_device is to be erased. 
	//   Let erase_iot_device(..) can be safe to use @device_id, use 'std::string device_id'.
	void erase_iot_device(int src, const std::string device_id);
	void modify_iot_device(int src, const std::string& device_id, const std::string& alias, const std::string& icon, int64_t ts, uint32_t flags);
	std::vector<const tiot_device*> iot_devices_from_src(int src) const;

	enum {misc_cfg_speech_sensor, misc_cfg_var_sensor, misc_cfg_timed_sensor, misc_cfg_task_cpp, misc_cfg_base_scene, 
		misc_cfg_add_timed_task, misc_cfg_courselist, misc_cfg_count};
	void modify_misc_cfg(int type, const std::string& str);

	void apltnotfound_or_verdismatch_to_fresh(bool write_pb);

	struct trunning
	{
		trunning()
			: start_times(0)
		{
			clear();
		}

		void clear()
		{
			SDL_Log("call trunning.clear");
			// aplt_task = nullptr;
			aplt_task_max_fails = nposm;
			finished2 = nposm;
			timer_interval = 0;
			timer_next_ticks = 0;

			start_ticks = 0;
			should_stop_ticks = 0;

			task_cpp_fake_started = false;
			exe_result = false;
			stop_single_task_called = false;
		}

		void verify_pre_start()
		{
			// VALIDATE(aplt_task == nullptr, null_str);
			VALIDATE(aplt_task_max_fails == nposm, null_str);
			VALIDATE(timer_interval == 0, null_str);
			VALIDATE(timer_next_ticks == 0, null_str);
			VALIDATE(!task_cpp_fake_started, null_str);
			VALIDATE(!exe_result, null_str);
			VALIDATE(!stop_single_task_called, null_str);
		}

		class tverifier
		{
		public:
			tverifier(const tbg_task& bg_task)
				: bg_task_(bg_task)
				, is_ing_(bg_task.is_ing_)
				// , aplt_task_(bg_task.running_.aplt_task)
				, bg_task2_(bg_task.bg_task2_)
				, task_cpp_fake_started_(bg_task.running_.task_cpp_fake_started)
				, exe_result_(bg_task.running_.exe_result)
				, stop_single_task_called_(bg_task_.running_.stop_single_task_called)
				, start_times_(bg_task.running_.start_times)
			{}

			~tverifier()
			{
				VALIDATE(is_ing_ == bg_task_.is_ing_, null_str);
				// VALIDATE(aplt_task_ == bg_task_.running_.aplt_task, null_str);
				VALIDATE(bg_task2_ == bg_task_.bg_task2_, null_str);
				VALIDATE(task_cpp_fake_started_ == bg_task_.running_.task_cpp_fake_started, null_str);
				VALIDATE(exe_result_ == bg_task_.running_.exe_result, null_str);
				VALIDATE(start_times_ == bg_task_.running_.start_times, null_str);
			}

		private:
			const tbg_task& bg_task_;
			const bool is_ing_;
			// const aplt::taplt_task* aplt_task_;
			const tbg_task::tbase_bg_task2* bg_task2_;
			const bool task_cpp_fake_started_;
			const bool exe_result_;
			const bool stop_single_task_called_;
			const uint32_t start_times_;
		};


		// During execution task, it is necessary to ensure that current_task_ is valid and must not change tasks.
		// const aplt::taplt_task* aplt_task;
		int aplt_task_max_fails;
		int finished2;
		int timer_interval;
		uint32_t timer_next_ticks;

		uint32_t start_ticks;
		uint32_t should_stop_ticks;

		bool task_cpp_fake_started;
		bool exe_result;

		bool stop_single_task_called;

		// for trunning::tverifier
		uint32_t start_times;

	};
	const trunning& running() const { return running_; }
	bool single_task_nav2th_is_started() const { return running_.finished2 >= ing_nav2th; }

	class tdisable_stop_timing_lock
	{
	public:
		tdisable_stop_timing_lock(tbg_task& bg_task)
			: bg_task_(bg_task)
		{
			VALIDATE(!bg_task_.disable_stop_timing_, null_str);
			bg_task_.disable_stop_timing_ = true;
		}

		~tdisable_stop_timing_lock()
		{
			VALIDATE(bg_task_.disable_stop_timing_, null_str);
			bg_task_.disable_stop_timing_ = false;
		}

	private:
		tbg_task& bg_task_;
	};

	bool disable_stop_timing() const { return disable_stop_timing_; }


private:
	void shedule();
	void handle_0clock(int zerotz_t);
	void timer_handler();
	void load_pb_tasks2(int type);
	void verify_tasks(const std::map<taplt_task_key, taplt_task>& tasks, int pb_tasks_size);
	void verify_4tasks();
	void verify_iot_devices();

	void add_pb_task(const aplt::taplt_task& task);
	void start_aplt_task(tbg_task::tbase_bg_task2& sys_task);
	void pre_start(tbg_task::tbase_bg_task2& sys_task);
	bool stop_aplt_task(bool fail_retry);
	bool stop_non_aplt_task();
	void start_task_cpp_fake(const tbase_bg_task2& sys_task);
	void nullptr_task_cpp_fake();

	bool did_navigation_bh_single_task(bool result, const aplt::tapplet& aplt, const aplt::taplt_task& aplt_task, const aplt::tapplet::ttask& cfg_task);
	void call_start_single_task(const aplt::taplt_task& task, const tapplet::ttask& cfg_task);
	bool call_stop_single_task(const aplt::taplt_task& task, const tapplet& aplt, const tapplet::ttask& cfg_task);
	bool did_nav2th_finished(bool result);

	const taplt_task* insert_task(int tasks_type, int priority, const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, int state, 
		int src, int src_evt, const std::string& src_device_id, const std::string& speech_id, int zerotz_t);
	// const taplt_task& modify_task(const taplt_task& _task, int state, const std::string& position1, const std::string& position2, int zerotz_t,
	//	const std::string& aplt_id, const std::string& task_id, int priority, bool write_pb);
	void add_pb_iot_device(const tiot_device& iot_device);

	void add_log7(const std::string& aplt_id, const std::string& task_id, const std::string& device_id, int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent);

private:
	base_instance& instance_;
	std::map<taplt_key, tapplet>& applets_;
	// During A background task is executing, and will using bg_task2_.
	
	// Although bg_task2_ are a pointer, it can be assumed that it is always valid. 
	// But it should only be used when 'bg_task_.is_ing()' is true.
	tbase_bg_task2* bg_task2_;
	rose_lua_kernel* lua_;
	lua_State* L;
	std::string scene_name_;
	std::map<taplt_task_key, taplt_task> iot_tasks_;
	std::map<taplt_task_key, taplt_task> speech_tasks_;
	std::map<taplt_task_key, taplt_task> var_tasks_;
	std::map<taplt_task_key, taplt_task> timed_tasks_; // not sorted by 'zerotz_t'
	std::map<tiot_device_key, tiot_device> iot_devices_;

	bool is_ing_;
	std::string lua_bundleid_;
	std::string lua_aplt_timing_clazz_;
	
	trunning running_;
	bool disable_stop_timing_;

	// unit: second
	const int overdue_threshold_;

	struct tstop_lock
	{
	public:
		tstop_lock(tbg_task& owner)
			: owner_(owner)
		{
			VALIDATE(!owner_.stopping_, null_str);
			owner_.stopping_ = true;
		}

		~tstop_lock()
		{
			VALIDATE(owner_.stopping_, null_str);
			owner_.stopping_ = false;
		}

	private:
		tbg_task& owner_;
	};
	bool stopping_;
	std::map<int, std::string> state_descs_;
	int last_shedule_zerotz_t_;

	// klink.pb
	int pb_klink_type_;
	const int pb_klink_backup_type_;
	pb2::tklink pb_klink_;
};

class tdef_bg_task2: public tbg_task::tbase_bg_task2
{
public:
	tdef_bg_task2(tbg_task& bg_task)
		: tbg_task::tbase_bg_task2(bg_task)
	{}
};

}

#endif