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

#ifndef LIBROSE_BASE_DRIVER_CORE_HPP
#define LIBROSE_BASE_DRIVER_CORE_HPP

#include "base_slot.hpp"
#include <memory>
#include "aplt_clazz.hpp"
#include "thread.hpp"
#include "wml_exception.hpp"
#include "camera.hpp"
#include "aplt2.hpp"

// class tros_instance;
// class tros_base_node;
class tdrivers_core;
class tbase_driver_core;
class tprivacy;

namespace aplt {
class tbg_task;
class tcfg_cpp_api_core;
class thealth;
}

class tros_base_node_core
{
public:
	tros_base_node_core();
	virtual ~tros_base_node_core();
	posix_noncopyable(tros_base_node_core);

	const std::string& curr_serial_dev() const { return serial_dev_; }
	int curr_serial_baudrate() const { return serial_baudrate_; }
	const std::string& curr_serial_model() const { return serial_model_; }

	void start(aplt::tbase_slot& slot, const std::string& serial_path, int baudrate, const std::string& serial_model);
	void stop(aplt::tbase_slot& slot);
	bool started() const { return thread_.get() != nullptr; }

protected:
	// tros_instance& ros_instance_;

	std::string serial_dev_;
	int serial_baudrate_;
	std::string serial_model_;
	std::unique_ptr<net::tworker> thread_;
};

/*
struct trobot_imu
{
	trobot_imu()
		: has_magnetometer(false)
		, reposition_threshold(60 * 1000) // 1 min
		, require_full2_position(true)
		, has_result_ok(false)
		, last_full2_ticks(0)
	{}
	// Prevent the presence of a second trobot_imu in the system.
	posix_noncopyable(trobot_imu);

	void set_base_pitch(double pitch);

	void set_base_yaw(tbase_driver_core& base_driver, double robot_yaw);

	bool has_magnetometer;
	trpy base;

	bool require_full2_position;
	bool has_result_ok;
	const int reposition_threshold;
	uint32_t last_full2_ticks;

	trpy tmp;
};
*/

#define PRIVACY_AUTO_PROTECT_THRESHOLD_S		90

class tprivacy
{
public:
	tprivacy(tbase_driver_core& base_driver)
		: base_driver_(base_driver)
		, protect_(false)
		, rdp_(true)
		, ble_discovery_(true)
		, upload_img_(true)
		, auto_protect_threshold_s_(nposm)
		, next_protect_ticks_(0)
		, disable_slice_(false)
	{}

	// Prevent the presence of a second tprivacy in the system.
	posix_noncopyable(tprivacy);

	void set_protect(bool enable);
	bool protect() const;
	std::string auto_protect_msgstr(int threshold_s) const;
	std::string desc() const;
	bool is_nposm() const
	{
		return !protect_ && auto_protect_threshold_s_ == nposm && next_protect_ticks_ == 0;
	}

	void set_auto_protect_threshold_s(int threshold_s) 
	{ 
		VALIDATE(threshold_s == nposm || threshold_s >= 30, null_str);
		auto_protect_threshold_s_ = threshold_s;
	}
	int auto_protect_threshold_s() const { return auto_protect_threshold_s_; }

	void set_next_protect_ticks()
	{
		VALIDATE(auto_protect_threshold_s_ != nposm, null_str);
		next_protect_ticks_ = SDL_GetTicks() + auto_protect_threshold_s_ * 1000;
	}

	void reset_next_protect_ticks()
	{
		VALIDATE(next_protect_ticks_ != 0, null_str);
		next_protect_ticks_ = 0;
	}
	uint32_t next_protect_ticks() const { return next_protect_ticks_; }

	class tdisable_slice_lock
	{
	public:
		tdisable_slice_lock(tprivacy& privacy)
			: privacy_(privacy)
		{
			VALIDATE(!privacy_.disable_slice_, null_str);
			privacy_.disable_slice_ = true;
		}

		~tdisable_slice_lock()
		{
			VALIDATE(privacy_.disable_slice_, null_str);
			privacy_.disable_slice_ = false;
		}

	private:
		tprivacy& privacy_;
	};
	bool disable_slice() const { return disable_slice_; }

	void validate() const 
	{
		if (protect_ || auto_protect_threshold_s_ == nposm) {
			VALIDATE(next_protect_ticks_ == 0, null_str);
		}
	}

private:
	tbase_driver_core& base_driver_;
	bool protect_; // is enable/disable protect?

	bool rdp_;
	bool ble_discovery_;
	bool upload_img_;
	int auto_protect_threshold_s_; // if nposm, not auto enter protection.
	uint32_t next_protect_ticks_;

	bool disable_slice_;
};

enum {mode_buildmap, mode_navigation, mode_maxros = mode_navigation, mode_position, mode_count};

class tbase_driver_core: public tcamera::tslot
{
public:
	tbase_driver_core(std::map<aplt::taplt_key, aplt::tapplet>& applets, 
		tros_base_node_core& base_node_core, tcamera& camera, aplt::tbg_task& bg_task, 
		aplt::tcfg_cpp_api_core& cfg_cpp_api, aplt::thealth& health, tdrivers_core& drivers, tprivacy& privacy);

	virtual ~tbase_driver_core()
	{
		VALIDATE(!node_started(), null_str);
		VALIDATE(mode_ == nposm, null_str);
		if (slot != nullptr) {
			delete slot;
			slot = nullptr;
		}
	}
	posix_noncopyable(tbase_driver_core);

	virtual void set_slot(const std::string& _aplt_id, aplt::tbase_slot* _slot);
	const std::string& aplt_id() const { return aplt_id_; }
	bool installed() const { return slot != nullptr; }

	bool use_external_imu() const { return slot != nullptr && slot->use_external_imu(); }
	bool imu_can_read() const { return slot != nullptr && slot->imu_can_read(); }

	const tbattery_info_C& get_battery_info();

	// current, base node isn't started.
	void start_node(const std::string& serial_dev, int baudrate, const std::string& serial_model);
	void stop_node();
	bool node_started() const;

	// stop, then start with old serial_dev and serial_baudrate
	void restart_node();
	// current, base node may be started or not. if started, restart_node() will stop first.
	void apply_new_serial(const std::string& dev, int baudrate, const std::string& model);

	void idle_subtask_from_empty(const aplt::tbase_scene& scene);
	void idle_or_preempt_subtask(bool idle);
	void resume_subtask();
	void start_subtask_from_preferences();
	void start_subtask_from_nposm(const aplt::tbase_scene& scene);
	void stop_subtask(bool sts_is_idle_or_ing);
	void stop_subtask_and_empty_pref();
	void restart_subtask();
	bool stop_subtask_if_runing(const std::string& warnning, const std::string& log = null_str);
	void start_or_stop_subtask(const aplt::tbase_scene& desire_scene, bool stop_is_idle);

	// const tros_base_node& base_node() const
	// {
	//	VALIDATE(base_worker_.get() != nullptr, null_str);
	//	return *base_worker_.get();
	// }

	void enter_navigation(bool buildmap, tpose2d& laser_tf, tpose2d& dcamera_tf);
	void exit_navigation();

	// void set_thread(bool buildmap, net::tshared_worker& thread);
	// net::tshared_worker& get_thread() { return thread_; }

	void slice();

	uint32_t last_install_ticks() const { return last_install_ticks_; }
	// bool navigation_started() const { return mode_ != nposm; }

	void set_last_navigation_rspfile(const std::string& rspfile)
	{
		last_navigation_rspfile_ = utils::normalize_path(rspfile);
	}

	bool is_same_navigation_rspfile(const std::string& rspfile) const
	{
		return last_navigation_rspfile_ == utils::normalize_path(rspfile);
	}

	aplt::tcamera_api& camera_api()
	{ 
		VALIDATE(camera_api_ != nullptr, null_str);
		return *camera_api_; 
	}
	const aplt::ttask_pair& subtask_pair() const { return subtask_pair_; }

	int subtask_state() const 
	{
		VALIDATE(subtask_state_ >= 0 && subtask_state_ < aplt::sts_count, null_str);
		if (subtask_state_ == aplt::sts_nposm) {
			VALIDATE(scene_id_.empty(), null_str);
		} else {
			VALIDATE(!scene_id_.empty(), null_str);
		}
		return subtask_state_;
	}

	int is_ing() const { return subtask_state() == aplt::sts_ing; }

	const std::string& scene_id() const 
	{ 
		if (scene_id_.empty()) {
			VALIDATE(subtask_state_ == aplt::sts_nposm, null_str);
		} else {
			VALIDATE(subtask_state_ != aplt::sts_nposm, null_str);
		}
		return scene_id_; 
	}

	void did_scene_id_changed(const std::string& new_id);
	void set_wko_task_slot(aplt::twko_task_slot* slot);

	class tdisable_start_subtask_lock
	{
	public:
		tdisable_start_subtask_lock(tbase_driver_core& base_driver)
			: base_driver_(base_driver)
		{
			VALIDATE(!base_driver_.disable_start_subtask_, null_str);
			base_driver_.disable_start_subtask_ = true;
		}
		~tdisable_start_subtask_lock()
		{
			VALIDATE(base_driver_.disable_start_subtask_, null_str);
			base_driver_.disable_start_subtask_ = false;
		}

	private:
		tbase_driver_core& base_driver_;
	};

	class tallow_restart_subtask_when_bg_ing_lock
	{
	public:
		tallow_restart_subtask_when_bg_ing_lock(tbase_driver_core& base_driver)
			: base_driver_(base_driver)
		{
			VALIDATE(!base_driver_.allow_restart_subtask_when_bg_ing_, null_str);
			base_driver_.allow_restart_subtask_when_bg_ing_ = true;
		}
		~tallow_restart_subtask_when_bg_ing_lock()
		{
			VALIDATE(base_driver_.allow_restart_subtask_when_bg_ing_, null_str);
			base_driver_.allow_restart_subtask_when_bg_ing_ = false;
		}

	private:
		tbase_driver_core& base_driver_;
	};

private:
	void start_subtask_internal(const aplt::tbase_scene& scene, bool empty_to_idle);
	void stop_subtask_internal(bool idle_or_preempt);

	// tcamera::tslot
	// void camera_post_enter_task() override {}
	// void camera_pre_exit_task() override {}
	// void camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override {}

	bool camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb) override;
	void camera_work_frame(const surface& surf) override;
	// void camera_post_switch_camera() override {}
	// void camera_did_button_clicked(int msgid) override {}

	void did_start_node_quited(std::string& err_msg, bool empty_to_idle);

	virtual void app_base_node_start_pre() {}
	virtual void app_base_node_start_post() {}

public:
	aplt::tbase_slot* slot;

protected:
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	// tros_instance& ros_instance_;
	tros_base_node_core& base_node_core_;
	// trobot_imu& robot_imu_;
	tcamera& camera_;
	aplt::tbg_task& bg_task_;
	aplt::tcfg_cpp_api_core& cfg_cpp_api_;
	aplt::thealth& health_;
	tdrivers_core& drivers_;
	tprivacy& privacy_;

	// net::tshared_worker thread_;

	std::string aplt_id_;
	int mode_;

	uint32_t last_install_ticks_;

	tbattery_info_C battery_info_;
	uint32_t flip_battery_level_ticks_;

	const double low_battery_level_;
	uint32_t low_battery_ticks_;
	const int long_no_battery_mask_ms_;
	uint32_t long_no_battery_ticks_;

	std::string last_navigation_rspfile_;

	aplt::ttask_vars task_vars_;
	aplt::ttask_api* task_api_;
	aplt::tcamera_api* camera_api_;
	aplt::ttask_pair subtask_pair_;
	int subtask_state_; 
	std::string scene_id_;
	bool subtask_finished_;

	bool disable_start_subtask_;
	bool allow_restart_subtask_when_bg_ing_;

	bool next_repeat_speak_off_;
};

class tpreempt_base_subtask_lock
{
public:
	tpreempt_base_subtask_lock(tbase_driver_core& base_driver)
		: base_driver_(base_driver)
		, original_subtask_state_(base_driver.subtask_state())
	{
		VALIDATE(base_driver_.installed(), null_str);
		if (original_subtask_state_ == aplt::sts_ing) {
			// why not place it before 'instance->set_desire_depth_task(dctask_color)'?
			// -- subtask and this task maybe in a same aplt. 
			//    rose given such a rule: at any given moment, at most one task is running.
			//    camera->start_task() is already in aplt, so base_subtask must be terminated before it.
			base_driver_.idle_or_preempt_subtask(false);
		}
	}

	~tpreempt_base_subtask_lock()
	{
		VALIDATE(base_driver_.installed(), null_str);
		if (original_subtask_state_ != aplt::sts_ing) {
			return;
		}
		VALIDATE(base_driver_.subtask_state() == aplt::sts_preempted, null_str);
		base_driver_.resume_subtask();
	}

private:
	tbase_driver_core& base_driver_;
	const int original_subtask_state_;
};

#endif

