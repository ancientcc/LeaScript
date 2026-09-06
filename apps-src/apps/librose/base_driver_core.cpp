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

#include "base_driver_core.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "aplt.hpp"
#include "drivers_core.hpp"
#include "chinese.hpp"
#include "base_instance.hpp"
#include "cfg_cpp_api_core.hpp"
#include "health.hpp"
#include "gui/dialogs/message.hpp"

using namespace std::placeholders;

// #define MEDIAPIPE_POSTURE_MIN_INTERVAL	150 // 150ms

tros_base_node_core::tros_base_node_core()
	: serial_baudrate_(nposm)
{
}

tros_base_node_core::~tros_base_node_core()
{
	VALIDATE(!started(), null_str);
	// VALIDATE(!registered, null_str);
}

void tros_base_node_core::start(aplt::tbase_slot& slot, const std::string& serial_dev, int baudrate, const std::string& serial_model)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(thread_.get() == nullptr, null_str);

	slot.aplt_pre_start_ros_node(serial_model, serial_dev, baudrate);

	serial_dev_ = serial_dev;
	serial_baudrate_ = baudrate;
	serial_model_ = serial_model;
	thread_.reset(new net::tworker(std::bind(&aplt::tbase_slot::aplt_start_ros_node, &slot, _1, serial_dev, baudrate), NULL, NULL, NULL, "base_driver_node"));
}

void tros_base_node_core::stop(aplt::tbase_slot& slot)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(thread_.get() != nullptr, null_str);

	thread_.reset(nullptr);

	slot.aplt_post_stop_ros_node();
}

void tprivacy::set_protect(bool enable)
{
	if (enable) {
		VALIDATE(!protect_, null_str);
		if (next_protect_ticks_ != 0) {
			reset_next_protect_ticks();
		}
	} else {
		VALIDATE(protect_, null_str);
		VALIDATE(next_protect_ticks_ == 0, null_str);
	}
	protect_ = enable;

	if (base_driver_.slot != nullptr) {
		aplt::tbase_ext_lamp* lamp = base_driver_.slot->query_ext_lamp();
		if (lamp != nullptr) {
			lamp->lamp_ctrl_led(aplt::tbase_ext_lamp::ledtype_privacy, 
				enable? aplt::tbase_ext_lamp::ledact_on: aplt::tbase_ext_lamp::ledact_off);
		}
	}
}

bool tprivacy::protect() const
{
	if (protect_) {
		VALIDATE(next_protect_ticks_ == 0, null_str);
	}
	return protect_;
}

std::string tprivacy::auto_protect_msgstr(int threshold_s) const
{
	utils::string_map symbols;

	symbols["threshold"] = utils::format_elapse_hms(threshold_s);
	return vgettext2("privacy^automatic enter protection $threshold", symbols);
}

std::string tprivacy::desc() const
{
	std::stringstream ss;
	if (protect_) {
		ss << _("privacy^protect");
	} else {
		// ss << _("privacy^not protect");
	}

	if (auto_protect_threshold_s_ != nposm) {
		ss << auto_protect_msgstr(auto_protect_threshold_s_);
	}
	return ss.str();
}

tbase_driver_core::tbase_driver_core(std::map<aplt::taplt_key, aplt::tapplet>& applets, 
	tros_base_node_core& base_node_core, tcamera& camera, aplt::tbg_task& bg_task, 
	aplt::tcfg_cpp_api_core& cfg_cpp_api, aplt::thealth& health, tdrivers_core& drivers, tprivacy& privacy)
	: applets_(applets)
	// , ros_instance_(ros_instance)
	, base_node_core_(base_node_core)
	// , robot_imu_(robot_imu)
	, camera_(camera)
	, bg_task_(bg_task)
	, cfg_cpp_api_(cfg_cpp_api)
	, health_(health)
	, drivers_(drivers)
	, privacy_(privacy)
	, slot(nullptr)
	, mode_(nposm)
	, last_install_ticks_(0)
	, flip_battery_level_ticks_(0)
	// https://item.jd.com/10098013425671.html
	// cutoff voltage: 9V
	// nominal voltage: 11.1V
	// full charge voltage: 12.6V
	, low_battery_level_(10.4)
	, low_battery_ticks_(0)
	, long_no_battery_mask_ms_(20000)
	, long_no_battery_ticks_(0)
	, task_api_(nullptr)
	, camera_api_(nullptr)
	, subtask_state_(aplt::sts_nposm)
	, subtask_finished_(false)
	, disable_start_subtask_(false)
	, allow_restart_subtask_when_bg_ing_(false)
{
	memset(&battery_info_, 0, sizeof(tbattery_info_C));
}

void tbase_driver_core::set_slot(const std::string& _aplt_id, aplt::tbase_slot* _slot)
{
	SDL_Log("{dbg-kdesktop}tbase_driver_core::set_slot, _aplt_id, _slot: %p", _slot);

	if (slot != nullptr) {
		VALIDATE(mode_ == nposm, null_str);

		if (base_node_core_.started()) {
			// VALIDATE(base_node_.registered, null_str);
			stop_node();
		}
		// if (base_node_.registered) {
		//	ros_instance_.deregister_slot(base_node_);
		// }

		delete slot;
		slot = nullptr;

		aplt::valuex.nposm_NMTHREAD_battery_level();
		aplt::valuex.battery_level = float_nposm;

	} else {
		// VALIDATE(slot->external_imu() == nullptr, null_str);
	}

	if (_slot != nullptr) {
		VALIDATE(!_aplt_id.empty(), null_str);
		slot = _slot;

	} else {
		VALIDATE(_aplt_id.empty(), null_str);
	}
	aplt_id_ = _aplt_id;

	if (slot != nullptr) {
		// robot_imu_.has_magnetometer = slot->has_magnetometer();
		battery_info_ = slot->get_battery_info();

		last_install_ticks_ = SDL_GetTicks();
		long_no_battery_ticks_ = SDL_GetTicks() + long_no_battery_mask_ms_;
/*
		// game_config::os == os_windows? "COM4": "/dev/ttyS7";
		std::string serial_path = preferences::im948serial();
		if (!serial_path.empty() && slot->use_external_imu()) {
			VALIDATE(slot->external_imu() == nullptr, null_str);
			slot->open_external_imu(serial_path, 115200, robot_imu_.has_magnetometer);
		}
*/
	} else {
		last_install_ticks_ = 0;
	}
}

const tbattery_info_C& tbase_driver_core::get_battery_info()
{
	VALIDATE(slot != nullptr, null_str);
	return battery_info_;
}

static std::string join_aplt_id(int source, const std::string& bundleid)
{
	std::stringstream ss;
	ss << bundleid;
	if (source != aplt::src_distribution) {
		ss << "(" << aplt::sources.find(source)->second << ")";
	}
	return ss.str();
}

void tbase_driver_core::did_start_node_quited(std::string& err_msg, bool empty_to_idle)
{
	VALIDATE(subtask_state_ == aplt::sts_nposm || subtask_state_ == aplt::sts_idle || subtask_state_ == aplt::sts_preempted, null_str);
	VALIDATE(!scene_id_.empty(), null_str);
	const aplt::tbase_scene* curr_scene = cfg_cpp_api_.base_scene_from_id(scene_id_, true);
	int original_subtask_state = subtask_state_;

	bool start_fail = false;
	if (!err_msg.empty()) {
		const library& lib = drivers_.find_by_type(apltsotype_base2th);
		if (lib.get() != nullptr) {
			drivers_.set_special_applet(apltsotype_base2th, nullptr);
		}

		utils::string_map symbols;
		symbols["reason"] = err_msg;

		instance->add_msg_only_log(logtype_warn, vgettext2("Start base's subtask fail. $reason", symbols), 0, false);
		scene_id_.clear();
		subtask_state_ = aplt::sts_nposm;

		start_fail = true;

	} else if (empty_to_idle) {
		subtask_state_ = aplt::sts_idle;

	} else {
		subtask_state_ = aplt::sts_ing;
	}

	if (subtask_state_ != original_subtask_state || start_fail) {
		instance->base_scene_state_changed(*curr_scene, subtask_state_);
	}
}

void tbase_driver_core::start_subtask_internal(const aplt::tbase_scene& scene, bool empty_to_idle)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(!(bg_task_.is_ing() && bg_task_.in_which_single_task() == aplt::task_camera), null_str);

	VALIDATE(!slot->moveable(), null_str);
	if (empty_to_idle) {
		VALIDATE(subtask_state_ == aplt::sts_nposm, null_str);
	} else {
		VALIDATE(subtask_state_ == aplt::sts_nposm || subtask_state_ == aplt::sts_idle || subtask_state_ == aplt::sts_preempted, null_str);
	}
	VALIDATE(scene_id_ == scene.id, null_str);

	utils::string_map symbols;
	std::string err_msg;
	tauto_destruct_executor destruct_executor(std::bind(&tbase_driver_core::did_start_node_quited, this, std::ref(err_msg), empty_to_idle));

	std::string aplt_id = scene.aplt;
	std::string task_id = scene.task;
	aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, aplt_id, task_id, false, true);
	if (pair.aplt == nullptr) {
		symbols["aplt"] = aplt_id;
		if (aplt_id == aplt::fake_aplt.bundleid) {
			err_msg = vgettext2("The applet cannot be '$aplt', because it would not provide 'camera' type tasks.", symbols);
		} else {
			err_msg = vgettext2("It requires the use of applet '$aplt', but the it is not installed.", symbols);
		}

	} else if (pair.task == nullptr) {
		symbols["aplt"] = pair.aplt->name2();
		symbols["aplt_task"] = task_id;
		err_msg = vgettext2("It needs to use task '$aplt_task' of applet '$aplt', which is installed, but does not have this task.", symbols);

	} else if (pair.task->type != aplt::task_camera) {
		symbols["aplt"] = pair.aplt->name2();
		symbols["aplt_task"] = task_id;
		err_msg = vgettext2("Desire task '$aplt_task' of applet '$aplt' isn't 'camera' type.", symbols);
	}
	if (!err_msg.empty()) {
		return;
	}

	aplt::tapplet& aplt = *const_cast<aplt::tapplet*>(pair.aplt);
	const library& lib = drivers_.find_by_type(apltsotype_base2th);

	symbols["aplt"] = pair.aplt->name2();
	symbols["aplt_task"] = task_id;
	if (subtask_state_ == aplt::sts_nposm) {
		VALIDATE(lib.get() == nullptr, null_str);
		drivers_.set_special_applet(apltsotype_base2th, &aplt);
		if (lib.get() == nullptr) {
			err_msg = vgettext2("Cannot load applet '$aplt_task' located in *. so.", symbols);
			return;
		}
	}
	VALIDATE(lib.get() != nullptr, null_str);
	
	if (empty_to_idle) {
		return;
	}

	trose_library* rose_lib = lib.get();
	void* v_task = nullptr;
	if (rose_lib->create_task_api != nullptr) {
		v_task = rose_lib->create_task_api(&aplt);
	}
	if (v_task == nullptr) {
		err_msg = _("Not implete 'aplt_create_task_api' method.");
		return;
	}

	aplt::ttask_api* task_api = reinterpret_cast<aplt::ttask_api*>(v_task);
	if (task_api->camera != nullptr) {
		aplt::setup_aplt_user_data_dir(get_aplt_user_data_dir(bundleid_2_lua_bundleid(aplt.bundleid)));

		task_vars_ = aplt::clone_env_vars(true);
		for (std::map<std::string, std::string>::const_iterator it = scene.input_vars.begin(); it != scene.input_vars.end(); ++ it) {
			const std::string& name = it->first;
			const std::string& val = it->second;
			task_vars_.insert_string(utils::join_app_prefix_id(pair.aplt->bundleid, name), false, val);
		}
		err_msg = task_api->camera->start_task(*pair.aplt, *pair.task, task_vars_);
		// if tcpp_api/tcamera_api.start_task() fail, don't let tcpp_api/tcamera_api.task_finished() be called.
		if (err_msg.empty()) {
			instance->set_desire_depth_task(dctask_color);

			task_api_ = task_api;
			camera_api_ = task_api->camera;
			subtask_pair_ = pair;
			// scene_id_ = scene.id;

			camera_.set_slot(this);
			camera_.enter_task(tcamera::taskid_base_subtask, tcamera::FLAG_SHOW_CANCEL, pair.task->no_swap_wh_for_screen);

			if (camera_.is_avcapture_started()) {
				trtc_client& avcapture = camera_.get_avcapture();
				if (avcapture.using_vidcap_count() > 0) {
					trtc_client::VideoRenderer* sink = avcapture.vrenderer(false, 0);
					// if (game_config::os == os_windows) {
						// Now 'mediapipe + tensorflowlite' still consumes quite a bit of CPU.
						// On Windows, the launcher is an additional app that requires strict CPU usage.
						sink->set_min_interval_cpu_saver(MEDIAPIPE_POSTURE_MIN_INTERVAL);
					// }
				}
			}

		} else {
			delete task_api;
			task_vars_.clear();
		}

	} else {
		delete task_api;
		err_msg = _("Not implete aplt::tcamera_api object.");
	}

	if (err_msg.empty()) {
		// stop current speak
		chinese::curr_pinyin.speak(null_str);

		aplt::tbase_ext_lamp* lamp = slot->query_ext_lamp();
		if (lamp != nullptr) {
			lamp->lamp_ctrl_led(aplt::tbase_ext_lamp::ledtype_scene, aplt::tbase_ext_lamp::ledact_on);
		}

		health_.did_start_or_stop_base_subtask(true);

		VALIDATE(chinese::curr_pinyin.get_amp_mode() == aplt::ampmode_1x, null_str);
		if (scene.amp != aplt::ampmode_1x) {
			chinese::curr_pinyin.set_amp_mode(scene.amp);
		}
	}
}

void tbase_driver_core::start_node(const std::string& serial_dev, int baudrate, const std::string& serial_model)
{
	SDL_Log("{dbg-kdesktop}tbase_driver_core::start_node, serial_dev: %s, baudrate: %i, serial_model: %s", serial_dev.c_str(), baudrate, serial_model.c_str());

	VALIDATE(slot != nullptr, null_str);
	VALIDATE(!base_node_core_.started(), null_str);
	VALIDATE(!camera_.tasking(), null_str);
	// instance->fg_aplt() maybe != nullptr.
	// VALIDATE(instance->fg_aplt() == nullptr, null_str);
	if (!slot->moveable()) {
		if (bg_task_.is_ing()) {
			// for example: aplt.leagor.basic__base_scene
			VALIDATE(subtask_state_ != aplt::sts_preempted, null_str);
		} else {
			VALIDATE(drivers_.deps.empty(), null_str);
		}
	}
	VALIDATE(task_vars_.empty(), null_str);
	VALIDATE(subtask_state_ == aplt::sts_nposm, null_str);
	VALIDATE(!subtask_finished_, null_str);
/*
	if (!base_node_.registered) {
		// After one start, it stays at 'ros_instance.slots' all the time. Even if it was stopped.
		ros_instance_.register_slot(base_node_);
	}
*/
	app_base_node_start_pre();

	base_node_core_.start(*slot, serial_dev, baudrate, serial_model);

	app_base_node_start_post();

	aplt::tbase_ext_lamp* lamp = slot->query_ext_lamp();
	if (lamp != nullptr) {
		lamp->lamp_ctrl_led(aplt::tbase_ext_lamp::ledtype_privacy, 
			privacy_.protect()? aplt::tbase_ext_lamp::ledact_on: aplt::tbase_ext_lamp::ledact_off);
	}

	// first 'scene' is load below, so needn't set ledtype_scene.

	if (slot->moveable()) {
		return;
	}

	if (disable_start_subtask_) {
		// Only used for special occasions(install app).
		return;
	}

	start_subtask_from_preferences();
}

void tbase_driver_core::stop_subtask_internal(bool idle_or_preempt)
{
	VALIDATE_IN_MAIN_THREAD();

	// Indeed, when a task_camera's bg_task is running, 
	// base_subtask related operations, including this stop_subtask_internal(), cannot be executed. 
	// However, during app_did_navigation_bh_use_cpp(), switching the state to sts_preempted precisely satisfies this condition.
	// VALIDATE(!(bg_task_.is_ing() && bg_task_.in_which_single_task() == aplt::task_camera), null_str);

	VALIDATE(!slot->moveable(), null_str);
	VALIDATE(subtask_state_ == aplt::sts_ing || sts_is_idle2(subtask_state_), null_str);

	if (subtask_state_ == aplt::sts_ing) {
		VALIDATE(camera_.tasking(), null_str);
		VALIDATE(task_api_ != nullptr, null_str);
		VALIDATE(camera_api_ != nullptr, null_str);
		VALIDATE(subtask_pair_.aplt != nullptr && subtask_pair_.task != nullptr, null_str);
		VALIDATE(subtask_state_ == aplt::sts_ing, null_str);

		// Must first exit camera.
		// because camera_work_frame() requires some resources to be valid, such as mediapipe_ptr.
		// mediapipe_ptr will be nullptr when subsequent camera_api_->task_finished.
		camera_.exit_task(tcamera::taskid_base_subtask);
		camera_.set_slot(nullptr);

		const aplt::tbase_scene* scene = cfg_cpp_api_.base_scene_from_id(scene_id_, true);
		camera_api_->task_finished(*subtask_pair_.aplt, *subtask_pair_.task);

		delete task_api_;

		task_api_ = nullptr;
		camera_api_ = nullptr;
		subtask_pair_.aplt = nullptr;
		subtask_pair_.task = nullptr;

		task_vars_.clear();
		subtask_finished_ = false;

		VALIDATE(chinese::curr_pinyin.get_amp_mode() == scene->amp, null_str);
		if (scene->amp != aplt::ampmode_1x) {
			chinese::curr_pinyin.set_amp_mode(aplt::ampmode_1x);
		}

	} else {
		VALIDATE(!camera_.tasking(), null_str);
		VALIDATE(!idle_or_preempt, null_str);
	}

	subtask_state_ = aplt::sts_nposm;
	if (!idle_or_preempt) {
		scene_id_.clear();
	}

	if (!idle_or_preempt) {
		drivers_.set_special_applet(apltsotype_base2th, nullptr);
	}

	// stop current speak
	if (!aplt::get_b_api().is_listen_speaking()) {
		chinese::curr_pinyin.speak(null_str);
	}

	aplt::tbase_ext_lamp* lamp = slot->query_ext_lamp();
	if (lamp != nullptr) {
		lamp->lamp_ctrl_led(aplt::tbase_ext_lamp::ledtype_scene, aplt::tbase_ext_lamp::ledact_off);
	}

	health_.did_start_or_stop_base_subtask(false);
}

void tbase_driver_core::stop_node()
{
	stop_subtask(false);

	VALIDATE(base_node_core_.started(), null_str);
	base_node_core_.stop(*slot);
}

bool tbase_driver_core::node_started() const 
{ 
	return base_node_core_.started();
}

void tbase_driver_core::restart_node()
{
	VALIDATE(base_node_core_.started(), null_str);

	// It may be unplugged in the middle and restarted with the original dev and baudrate.
	const std::string serial_dev = base_node_core_.curr_serial_dev();
	int serial_baudrate = base_node_core_.curr_serial_baudrate();
	const std::string serial_model = base_node_core_.curr_serial_model();

	stop_node();
	start_node(serial_dev, serial_baudrate, serial_model);
}

void tbase_driver_core::apply_new_serial(const std::string& serial_dev, int serial_baudrate, const std::string& serial_model)
{
	if (!serial_dev.empty() && serial_baudrate != nposm) {
		// new serial's dev and baudrate are valid. 
		bool restart = !base_node_core_.started();
		if (!restart) {
			if (serial_dev != base_node_core_.curr_serial_dev() || serial_baudrate != base_node_core_.curr_serial_baudrate() || serial_model != base_node_core_.curr_serial_model()) {
				restart = true;
			}
		}
		if (restart) {
			if (base_node_core_.started()) {
				stop_node();
			}
			start_node(serial_dev, serial_baudrate, serial_model);
		}

	} else if (node_started()) {
		stop_node();
	}
}

void tbase_driver_core::idle_subtask_from_empty(const aplt::tbase_scene& scene)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(subtask_state_ == aplt::sts_nposm, null_str);
	VALIDATE(scene_id_.empty(), null_str);

	scene_id_ = scene.id;
	start_subtask_internal(scene, true);
	instance->base_scene_state_changed(scene, aplt::sts_idle);
}

void tbase_driver_core::idle_or_preempt_subtask(bool idle)
{
	VALIDATE(subtask_state_ == aplt::sts_ing, null_str);
	const aplt::tbase_scene* curr_scene = cfg_cpp_api_.base_scene_from_id(scene_id_, true);

	stop_subtask_internal(true);

	VALIDATE(subtask_state_ == aplt::sts_nposm, null_str);
	if (idle) {
		// applet task: aplt.leagor.basic__base_scene
		// VALIDATE(!bg_task_.is_ing(), null_str);
		subtask_state_ = aplt::sts_idle;

	} else {
		// VALIDATE(bg_task_.is_ing(), null_str);
		subtask_state_ = aplt::sts_preempted;

	}
	const library& lib = drivers_.find_by_type(apltsotype_base2th);
	VALIDATE(lib.get() != nullptr, null_str);

	instance->base_scene_state_changed(*curr_scene, subtask_state_);
}

void tbase_driver_core::resume_subtask()
{
	const aplt::tbase_scene* scene = nullptr;

	VALIDATE(!scene_id_.empty(), null_str);
	scene = cfg_cpp_api_.base_scene_from_id(scene_id_, true);
	VALIDATE(subtask_state_ == aplt::sts_idle || subtask_state_ == aplt::sts_preempted, null_str);
	VALIDATE(task_api_ == nullptr, null_str);

	start_subtask_internal(*scene, false);

	// VALIDATE(subtask_state_ == aplt::sts_ing, null_str);
}

void tbase_driver_core::start_subtask_from_preferences()
{
	VALIDATE(scene_id_.empty(), null_str);
	VALIDATE(subtask_state_ == aplt::sts_nposm, null_str);

	const std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.base_scenes();
	std::string scene_id = preferences::base_scene_id();
	const aplt::tbase_scene* scene = nullptr;
	std::string err_msg;
	if (scene_id.empty() && !scenes.empty()) {
		bool use0_if_empty = false;
		if (use0_if_empty) {
			scene = &scenes[0];
			scene_id = scene->id;

			preferences::set_base_scene_id(scene_id);
		} else {
			err_msg = _("Don't start base's subtask. pref's base_scene_id is empty.");
			// instance->add_msg_only_log(logtype_warn, err_msg, 0, false);
			return;
		}
	}

	utils::string_map symbols;
	if (scene_id.empty()) {
		err_msg = vgettext2("Start base's subtask fail. No available scenarios.", symbols);
		instance->add_msg_only_log(logtype_warn, err_msg, 0, false);
		return;
	}

	scene = cfg_cpp_api_.base_scene_from_id(scene_id, false);
	if (scene == nullptr) {
		symbols["id"] = scene_id;
		err_msg = vgettext2("Start base's subtask fail. No scene that id is $id.", symbols);
		instance->add_msg_only_log(logtype_warn, err_msg, 0, false);
		return;
	}

	scene_id_ = scene_id;
	start_subtask_internal(*scene, false);
}

void tbase_driver_core::start_subtask_from_nposm(const aplt::tbase_scene& scene)
{
	VALIDATE(scene_id_.empty(), null_str);
	VALIDATE(subtask_state_ == aplt::sts_nposm, null_str);

	scene_id_ = scene.id;
	start_subtask_internal(scene, false);
}

void tbase_driver_core::stop_subtask(bool sts_is_idle_or_ing)
{
	VALIDATE(subtask_state_ != aplt::sts_preempted, null_str);

	const aplt::tbase_scene* curr_scene = nullptr;
	if (subtask_state_ != aplt::sts_nposm) {
		curr_scene = cfg_cpp_api_.base_scene_from_id(scene_id_, true);
	}
	const int original_subtask_state = subtask_state_;

	if (sts_is_idle_or_ing) {
		VALIDATE(subtask_state_ == aplt::sts_ing || subtask_state_ == aplt::sts_idle, null_str);
		stop_subtask_internal(false);

		VALIDATE(subtask_state_ == aplt::sts_nposm, null_str);
		VALIDATE(!bg_task_.is_ing(), null_str);

	} else {
		if (!slot->moveable()) {
			if (bg_task_.is_ing()) {
				// for example: aplt.leagor.basic__base_scene
				VALIDATE(subtask_state_ != aplt::sts_preempted, null_str);
			} else {
				VALIDATE(drivers_.deps.empty(), null_str);
			}
		}

		if (camera_.tasking()) {
			if (subtask_state_ != aplt::sts_nposm) {
				stop_subtask_internal(false);

			} else {
				VALIDATE(camera_.curr_taskid() == tcamera::taskid_dcamera, str_cast(camera_.curr_taskid()));
				camera_.exit_task(tcamera::taskid_dcamera);
				camera_.set_slot(nullptr);
			}

		} else if (subtask_state_ != aplt::sts_nposm) {
			VALIDATE(sts_is_idle2(subtask_state_), null_str);
			stop_subtask_internal(false);

		}

		VALIDATE(!camera_.tasking(), null_str);
		VALIDATE(task_vars_.empty(), null_str);

		VALIDATE(task_api_ == nullptr, null_str);
		VALIDATE(camera_api_ == nullptr, null_str);
		VALIDATE(subtask_pair_.aplt == nullptr && subtask_pair_.task == nullptr, null_str);
		VALIDATE(subtask_state_ != aplt::sts_ing, null_str);
		VALIDATE(scene_id_.empty(), null_str);
		VALIDATE(!subtask_finished_, null_str);
	}
	
	const library& lib = drivers_.find_by_type(apltsotype_base2th);
	VALIDATE(lib.get() == nullptr, null_str);

	if (subtask_state_ != original_subtask_state) {
		VALIDATE(curr_scene != nullptr, null_str);
		VALIDATE(subtask_state_ == aplt::sts_nposm, null_str);
		instance->base_scene_state_changed(*curr_scene, subtask_state_);
	}
}

void tbase_driver_core::stop_subtask_and_empty_pref()
{
	stop_subtask(subtask_state_ == aplt::sts_idle || subtask_state_ == aplt::sts_ing);
	preferences::set_base_scene_id(null_str);
}

void tbase_driver_core::restart_subtask()
{
	stop_subtask(false);
	start_subtask_from_preferences();
}

bool tbase_driver_core::stop_subtask_if_runing(const std::string& warnning, const std::string& log)
{
	VALIDATE(subtask_state_ != aplt::sts_nposm, null_str);

	if (!warnning.empty()) {
		aplt::tbg_task::tdisable_stop_timing_lock lock(bg_task_);

		if (gui2::show_message2(null_str, warnning, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return false;
		}
		// During show_message2, the task may have stopeed.
		if (subtask_state_ == aplt::sts_nposm) {
			return true;
		}
	}
	if (!log.empty()) {
		// bg_task_.add_log2(time(nullptr), log, 0, false);
	}

	stop_subtask_and_empty_pref();
	return true;
}

// If 'desire_scene.id' is the current 'scene_id' of 'base_driver_core', then stop or resume the current scene. Otherwise, start the 'desire_scene'.  
// When stopping, 'stop_is_idle' indicates whether to suspend or stop (and will also clear 'scene_id' in the preferences).
void tbase_driver_core::start_or_stop_subtask(const aplt::tbase_scene& desire_scene, bool stop_is_idle)
{
	bool is_me = desire_scene.id == scene_id();

	if (is_me) {
		if (subtask_state() == aplt::sts_ing) {
			if (stop_is_idle) {
				idle_or_preempt_subtask(true);
			} else {
				stop_subtask_and_empty_pref();
			}
				
		} else {
			resume_subtask();
		}

	} else {
		preferences::set_base_scene_id(desire_scene.id);
		restart_subtask();
	}
}

void tbase_driver_core::did_scene_id_changed(const std::string& new_id)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(!scene_id_.empty(), null_str);
	VALIDATE(new_id != scene_id_, null_str);

	scene_id_ = new_id;
	preferences::set_base_scene_id(new_id);
}

void tbase_driver_core::set_wko_task_slot(aplt::twko_task_slot* slot)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(camera_api_ != nullptr, null_str);

	return camera_api_->set_wko_task_slot(slot);
}

void tbase_driver_core::enter_navigation(bool buildmap, tpose2d& laser_tf, tpose2d& dcamera_tf)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	// VALIDATE(thread_.get() == nullptr, null_str);
	VALIDATE(base_node_core_.started(), null_str);
	VALIDATE(slot->moveable(), null_str);

	mode_ = buildmap? mode_buildmap: mode_navigation;

	slot->enter_navigation(buildmap, laser_tf, dcamera_tf);
	// thread_.reset(new net::tworker(std::bind(&aplt::tbase_slot::start_navigation, slot, _1, serial_path, baudrate), NULL, NULL, NULL, "base_driver_node"));
}

void tbase_driver_core::exit_navigation()
{
	VALIDATE_IN_MAIN_THREAD();
	// VALIDATE(thread_.get() != nullptr, null_str);
	VALIDATE(base_node_core_.started(), null_str);

	VALIDATE(mode_ != nposm, null_str);
	mode_ = nposm;

	// thread_.reset(nullptr);
}
/*
void tbase_driver_core::set_thread(bool buildmap, net::tshared_worker& thread)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	VALIDATE(thread_.get() == nullptr, null_str);
	VALIDATE(thread.get() != nullptr, null_str);

	slot->pre_start_navigation(buildmap);
	// VALIDATE(driver_main != nullptr, null_str);

	thread_ = thread;
}
*/
void tbase_driver_core::slice()
{
	if (slot == nullptr) {
		return;
	}

	if (slot->external_imu() != nullptr) {
		slot->external_imu()->slice();
	}

	if (SDL_GetTicks() >= flip_battery_level_ticks_) {
		aplt::valuex.flip_battery_level();
		const int interval_ms = 3000; // 3 seconds
		flip_battery_level_ticks_ = SDL_GetTicks() + interval_ms;

		// SDL_Log("%u last_NMTHREAD_battery_level_ticks: %u", SDL_GetTicks(), aplt::valuex.last_NMTHREAD_battery_level_ticks());
	}

	bool spoken = false;
	const uint32_t now = SDL_GetTicks();
	if (HAS_BATTERY() && aplt::valuex.battery_level <= battery_info_.charge) {
		if (now >= low_battery_ticks_) {
			const int low_battery_mask_ms = 6000;
			low_battery_ticks_ = SDL_GetTicks() + low_battery_mask_ms;

			utils::string_map symbols;
			symbols["level"] = str_cast(aplt::valuex.battery_level);
			const std::string msg = vgettext2("The battery voltage is $level volts, please charge", symbols);
			chinese::curr_pinyin.speak(msg);
		}
		spoken = true;
	}
	
	// if base_driver isn't installed, will not call tbase_driver_core::slice().
	if (!spoken && now >= long_no_battery_ticks_) {
		// aplt::valuex.last_NMTHREAD_battery_level_ticks() has a lock operate, reduce it call times.
		int still_ms = SDL_GetTicks() - aplt::valuex.last_NMTHREAD_battery_level_ticks();
		// const int battery_still_threshold = 10000; // 30 second
		if (still_ms >= BATTERY_STILL_THRESHOLD_MS) {
			// Sometimes, it may really be to save power and want to debug without connecting base. 
			// Of course, just limited to Windows
			const aplt::tapplet& aplt = *aplt::aplt_from_id(applets_, aplt_id_);
			const bool warning = !aplt.lua_base_slot;
			if (warning) {
				std::string msg = _("The battery voltage has not been received for a long time, check the base serial's connection");
				chinese::curr_pinyin.speak(msg);
			}

			// SDL_Log("%u msg_long_no_battery", SDL_GetTicks());
		}

		long_no_battery_ticks_ = SDL_GetTicks() + long_no_battery_mask_ms_;
	}

	if (camera_api_ != nullptr && !subtask_finished_) {
		// VALIDATE(req_task_.aplt != nullptr && req_task_.task != nullptr && req_task_.task->type == aplt::task_camera, null_str);
		if (camera_viewer_ == nullptr) {
			// is no temporary slot, tbg_task2 derminate when to call camera_.slice(...)
			camera_.slice(empty_rect, false);
		} else {
			// camera_.slice is called by gui2::tcenter::did_draw_paper(...)
		}
		std::string result_str;
		std::vector<std::pair<float, SDL_Rect> > classifier_rects;
		bool finished = camera_api_->slice(result_str, classifier_rects);
		camera_.set_aplt_overlay(result_str, classifier_rects);
		// VALIDATE(!finished, null_str);
		subtask_finished_ = finished;
		if (subtask_finished_) {
			const std::string err_msg = _("Base subtask has ended early, and the camera will enter a no task state");
			instance->add_msg_only_log(logtype_warn, err_msg, 0, false);
			chinese::curr_pinyin.speak(err_msg);
		}

		// if (finished) {
		//	finish_task_camera_or_charge(false, *req_task_.task, aplt::task_camera);
		//	bg_task_.set_luafunc_finished();
		// }
	}

	slot->slice();
}

void tbase_driver_core::camera_work_frame(const surface& surf)
{
	tsurface_2_mat_lock lock(surf);
	if (camera_api_ != nullptr && !subtask_finished_) {
		// bg_task_.set_luafunc_finished() maybe call by tros_cpp_api, because cpp_task's time expires.
		// VALIDATE(!bg_task_.is_luafunc_finished(), null_str);
		// VALIDATE(req_task_.aplt != nullptr && req_task_.task != nullptr && req_task_.task->type == aplt::task_camera, null_str);
		camera_api_->camera_work_frame(surf, lock.mat);
	}
}

bool tbase_driver_core::camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb)
{
	VALIDATE(camera_viewer_ != nullptr, null_str);
	VALIDATE(camera_api_ != nullptr, null_str);

	return camera_api_->camera_use_cv_frame(argb, cv_argb);
}