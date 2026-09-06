#define GETTEXT_DOMAIN "launcher-lib"

#include "rose_global.hpp"
#include "bg_task2.hpp"
#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
#include <iomanip>

#include "base_instance.hpp"
#include "chinese.hpp"
#include "ai_driver.hpp"

#include <angles/angles.h>

using namespace std::placeholders;

namespace aplt {

//
// temp task
//
tbg_task2::tbg_task2(std::map<aplt::taplt_key, aplt::tapplet>& applets, tdrivers& drivers, tros_map& curmap, aplt::tcfg_cpp_api& cfg_cpp_api, 
	aplt::tbg_task& bg_task, tcamera& camera, const tflite::tscript& def_script, const tflite::ttflite& def_tflite, tros_instance& ros_instance, 
	tbase_driver& base_driver, tdcamera_driver& dcamera_driver, tai_driver& ai_driver)
	: aplt::tbg_task::tbase_bg_task2(bg_task)
	, applets_(applets)
	, drivers_(drivers)
	, curmap_(curmap)
	, cfg_cpp_api_(cfg_cpp_api)
	, camera_(camera)
	, def_script_(def_script)
	, def_tflite_(def_tflite)
	, ros_instance_(ros_instance)
	, base_driver_(base_driver)
	, dcamera_driver_(dcamera_driver)
	, ai_driver_(ai_driver)
	, req_task_dirty_(false)
	, task_require_start_ticks_(0)
	, task_cpp_api_(nullptr)
	, task_cpp_aplt_(nullptr)
	, task_cpp_cfg_task_(nullptr)
	, retry_aplt_task_(nullptr)
	, camera_api_(nullptr)
	, nonblock_api_(nullptr)
	, klink_aplt_task_(nullptr)
	, trigger_by_gui_(false)
	// , center_dlg_(nullptr)
{
}

tbg_task2::~tbg_task2()
{
	VALIDATE(camera_viewer_ == nullptr, null_str);
}

void tbg_task2::set_req_task(const aplt::treq_task& task, const aplt::ttask_vars& task_vars)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(task.valid(), null_str);
	req_task_.assign10(task);
	req_task_.task_vars = task_vars;
	set_req_task_dirty(true);
}

std::string format_absence_permission(aplt::per_t per)
{
	utils::string_map symbols;

	VALIDATE(aplt::sys_permissions.count(per) != 0, null_str);
	symbols["permission"] = aplt::sys_permissions.find(per)->second.name;
	return vgettext2("Unable to get $permission permission", symbols);
}

void tbg_task2::modify_state_in_shedule(int state) const
{
	VALIDATE(state == taplt_task::state_apltnotfound || state == taplt_task::state_verdismatch, null_str);
	if (klink_aplt_task_ == nullptr) {
		return;
	}
	const taplt_task& aplt_task = *klink_aplt_task_;
	VALIDATE(aplt_task.state == taplt_task::state_running, null_str);
	bg_task_.modify_task_state(aplt_task, state, false);
}

std::string tbg_task2::single_task_can_start(const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task) const
{
	VALIDATE(aplt::is_single_task(cfg_task.type), null_str);

	// check version
	std::string err_msg = aplt::aplt_can_run(aplt);
	if (!err_msg.empty()) {
		modify_state_in_shedule(taplt_task::state_verdismatch);
		return err_msg;
	}

	// check driver installed
	if (req_task_.task->type == aplt::task_moveit && !ros_instance_.moveit_driver().installed()) {
		err_msg = _("The task requires the use of moveit, but there is no moveit drive set up");
		if (!err_msg.empty()) {
			return err_msg;
		}
	}

	return null_str;
}

std::string tbg_task2::req_task_can_start() const
{
	VALIDATE(req_task_dirty_, null_str);

	// when fail_retry result 2th 'shedule', 
	// if you want to test whether the 'req_task_can_start' is false, it will cause problems.
	const bool dbg_retry_fail = false;
	if ((game_config::os == os_windows && dbg_retry_fail) && aplt_task != nullptr) {
		return _("Timing is not right, it cannot be executed");
	}

	std::string err_msg;
	if (!in_task_cpp() && aplt::new_klink_task_disabled2(nullptr)) {
		// disallowed();
		err_msg = _("Timing is not right, it cannot be executed");
		return err_msg;
	}

	if (!base_driver_.node_started()) {
		err_msg = _("Valid base-drive needs to be set up, as well as serial's path and baudrate");
		return err_msg;
	}

/*
	if (task_require_start_ticks_ != 0 && SDL_GetTicks() >= task_require_start_ticks_) {
		if (game_config::os == os_windows) {
			// maybe other reason, windows is debug, try find more read.
			VALIDATE(bg_aplt == nullptr, null_str);
		}
		err_msg = _("The system task has not been started for a long time. it may be that the base-driver is not set");
		return err_msg;
	}
*/
	
	VALIDATE(req_task_.valid(), null_str);

	utils::string_map symbols;
	if (req_task_.task->type == aplt::task_cpp) {
		if (req_task_.aplt == &fake_aplt) {
			// Check if the called applet has been installed.
			// Check those applet versions.
			std::map<std::string, aplt::ttask_cpp_pair>& task_pairs = cfg_cpp_api_.task_pairs();
			VALIDATE(task_pairs.count(req_task_.task->id) != 0, null_str);
			const aplt::ttask_cpp_pair& cpp_pair = task_pairs.find(req_task_.task->id)->second;
			symbols["task"] = cpp_pair.name;
			for (std::map<int, aplt::tcpp_api::tstate2>::const_iterator it = cpp_pair.states.begin(); it != cpp_pair.states.end(); ++ it) {
				const aplt::tcpp_api::tstate2& state2 = it->second;
				if (state2.async_task.valid()) {
					VALIDATE(!state2.async_task.aplt_id.empty(), null_str);
					VALIDATE(!state2.async_task.task_id.empty(), null_str);

					const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets_, state2.async_task.aplt_id);
					if (aplt == nullptr) {
						symbols["aplt"] = state2.async_task.aplt_id;
						err_msg = vgettext2("Start task '$task' fail. It requires the use of applet '$aplt', but the it is not installed.", symbols);
						modify_state_in_shedule(taplt_task::state_apltnotfound);
						return err_msg;
					}

					symbols["aplt"] = aplt->name2();
					if (aplt->tasks.count(state2.async_task.task_id) == 0) {
						symbols["aplt_task"] = state2.async_task.task_id;
						err_msg = vgettext2("Start task '$task' fail. It needs to use task '$aplt_task' of applet '$aplt', which is installed, but does not have this task.", symbols);
						modify_state_in_shedule(taplt_task::state_apltnotfound);
						return err_msg;
					}

					const aplt::tapplet::ttask* task = &aplt->tasks.find(state2.async_task.task_id)->second;
					if (!base_driver_.slot->moveable()) {
						if (!state2.async_task.position1_if_block.branches.empty()) {
							symbols["aplt_task"] = task->name;
							err_msg = vgettext2("Start task '$task' fail. Task '$aplt_task' of applet '$aplt' require move, but base works in unmovable mode.", symbols);
							return err_msg;
						}
					}

					err_msg = single_task_can_start(*aplt, *task);
					if (!err_msg.empty()) {
						return err_msg;
					}
				}
			} // end for (@cpp_pair.states
		}

	} else if (!in_task_cpp()) {
		err_msg = single_task_can_start(*req_task_.aplt, *req_task_.task);
		if (!err_msg.empty()) {
			return err_msg;
		}
		if (!base_driver_.slot->moveable()) {
			if (!req_task_.position1_uuid.empty()) {
				symbols["aplt"] = req_task_.aplt->name2();
				symbols["aplt_task"] = req_task_.task->name;
				err_msg = vgettext2("Start task '$task' fail. Task '$aplt_task' of applet '$aplt' require move, but base works in unmovable mode.", symbols);
				return err_msg;
			}
		}
	}
	VALIDATE(err_msg.empty(), null_str);

	const ttemp_task_type& task_type = game_config::temp_task_types.find(task_type_aplt_task)->second;

	const std::set<aplt::per_t>* permissions_ptr = &task_type.permissions;

	permissions_ptr = &req_task_.task->permissions;

	const aplt::tapplet* fg_aplt = instance->fg_aplt();

	if (fg_aplt != nullptr) {
		if (req_task_.aplt == fg_aplt) {
			err_msg = _("The applet in which the task resides is running");
			return err_msg;
		}

		aplt::per_t per = aplt::conflicted_permission(*permissions_ptr, fg_aplt->permissions);
		if (per != aplt::per_nposm) {
			err_msg = format_absence_permission(per);
			return err_msg;
		}
	}

	return null_str;
}

void tbg_task2::cancel_req_task(const std::string& err_msg)
{
	VALIDATE(!err_msg.empty(), null_str);

	task_vars_.clear();

	if (aplt_task != nullptr) {
		VALIDATE(aplt_task->fails != 0, null_str);
		VALIDATE(curr_aplt == nullptr, null_str);

		aplt::taplt_task& task = *aplt_task;
		// if (task.is_klink()) {
			// this aplt_task is in klink list.
		//	set_klink_aplt_task_state(task, task.state);
		// }
		bool exe_result2 = task.state == aplt::taplt_task::state_finished_fail;
			
		nullptr_aplt_task();

		if (!in_task_cpp()) {
			drivers_.unload_deps();
		}
	}
	if (in_task_cpp()) {
		VALIDATE(in_pure_task_cpp(), null_str);
		task_cpp_api_->single_task_finished(req_task_, false);
	}

	VALIDATE(req_task_dirty_, null_str);
	VALIDATE(req_task_.valid(), null_str);

	set_req_task_dirty(false);

	if (klink_aplt_task_ == nullptr) {
		// trigger in gui2-'center'
		chinese::curr_pinyin.speak(err_msg);
	}
	instance->add_aplt_task_log(req_task_.aplt->id, req_task_.task->id, req_task_.ble_device_id, time(nullptr), err_msg, 0, false);

	req_task_.clear();
}

std::string tbg_task2::shedule()
{
	if (!req_task_dirty_) {
		// if fail_retry result, don't call cpp.slice().
		// if (in_task_cpp()) {
		//	task_cpp_api_->slice();
		// }
	} else {

	}
	if (!req_task_dirty_) {
		return _("req_task_dirty_ is false");
	}

	req_task_.validate();

	VALIDATE(aplt_task == nullptr, null_str);

	std::string err_msg;
	if (retry_aplt_task_ == nullptr) {
		err_msg = req_task_can_start();
		if (!err_msg.empty()) {
			cancel_req_task(err_msg);
			return err_msg;
		}
	}

	aplt::tapplet* task_aplt = const_cast<aplt::tapplet*>(req_task_.aplt);
	const aplt::tapplet::ttask* cfg_task = req_task_.task;

	// below if/else block may return false. 
	// avoid to change back to the original value if failures, use temporary variables.
	aplt::tapplet* tmp_curr_aplt = task_aplt;
	bool tmp_now_non_task_cpp = true;

	VALIDATE(req_task_.valid(), null_str);

	if (cfg_task->type == aplt::task_cpp) {
		VALIDATE(retry_aplt_task_ == nullptr, null_str);
		VALIDATE(camera_api_ == nullptr, null_str);
		VALIDATE(nonblock_api_ == nullptr, null_str);
		VALIDATE(req_task_.position1_uuid.empty() && req_task_.position2_uuid.empty(), null_str);
		VALIDATE(task_cpp_aplt_ == nullptr, null_str);
		VALIDATE(klink_cpp_aplt_task2 == nullptr, null_str);
		VALIDATE(klink_single_aplt_task2 == nullptr, null_str);

		aplt::tcpp_api* cpp_api = nullptr;
		if (task_aplt->fake) {
			cpp_api = &cfg_cpp_api_;
		} else {
			cpp_api = drivers_.create_task_api(*task_aplt)->cpp;
		}

		VALIDATE(task_vars_.empty(), null_str);

		bool use_taskpoint = klink_aplt_task_ != nullptr && cfg_cpp_api_.use_taskpoint(*klink_aplt_task_);
		const aplt::ttaskpoint* taskpoint = nullptr;
		if (!use_taskpoint) {
			task_vars_ = req_task_.task_vars;
		} else {
			task_vars_ = req_task_.task_vars;
			// VALIDATE(req_task_.task_vars.data().size() == cfg_cpp_api_.taskpoint().task_vars.data().size(), null_str);
			VALIDATE(req_task_.task_vars == cfg_cpp_api_.taskpoint().task_vars, null_str);
			taskpoint = &cfg_cpp_api_.taskpoint();
		}
		err_msg = cpp_api->start_task(req_task_.task->id, task_vars_, taskpoint);
		if (use_taskpoint) {
			cfg_cpp_api_.clear_taskpoint();
		}

		if (!err_msg.empty()) {
			cancel_req_task(err_msg);
			if (!task_aplt->fake) {
				drivers_.unload_deps();
			}
			return err_msg;
		}
		task_cpp_api_ = cpp_api;

		task_cpp_aplt_ = task_aplt;
		task_cpp_cfg_task_ = cfg_task;
		klink_cpp_aplt_task2 = klink_aplt_task_;

		tmp_now_non_task_cpp = false;

		// base rule: make sure both curr_aplt and aplt_task are nullptr.
		tmp_curr_aplt = nullptr;

	} else {
		VALIDATE(is_single_task(cfg_task->type), null_str);
		// if (aplt_task == nullptr) {
		if (retry_aplt_task_ == nullptr) {
			if (klink_aplt_task_ != nullptr) {
				VALIDATE(task_vars_.empty(), null_str);
				task_vars_ = req_task_.task_vars;

				if (klink_aplt_task_->type == aplt::taplt_task::type_iot) {
					aplt_task = new aplt::taplt_task(klink_aplt_task_->priority, req_task_.aplt->id, req_task_.task->id, req_task_.ble_device_id, req_task_.position1_uuid, req_task_.position2_uuid, aplt::taplt_task::state_running, 
						klink_aplt_task_->pb_at, klink_aplt_task_->iot_src, klink_aplt_task_->src_evt, klink_aplt_task_->src_device_id);

				} else if (klink_aplt_task_->type == aplt::taplt_task::type_speech) {
					aplt_task = new aplt::taplt_task(klink_aplt_task_->priority, req_task_.aplt->id, req_task_.task->id, req_task_.ble_device_id, req_task_.position1_uuid, req_task_.position2_uuid, aplt::taplt_task::state_running, 
						klink_aplt_task_->pb_at, klink_aplt_task_->speech_id);

				} else if (klink_aplt_task_->type == aplt::taplt_task::type_var) {
					VALIDATE(req_task_.ble_device_id.empty(), null_str);
					aplt_task = new aplt::taplt_task(req_task_.aplt->id, req_task_.task->id, req_task_.position1_uuid, req_task_.position2_uuid, aplt::taplt_task::state_running, 
						klink_aplt_task_->pb_at, klink_aplt_task_->var_at, klink_aplt_task_->aux_key_id);

				} else {
					VALIDATE(klink_aplt_task_->type == aplt::taplt_task::type_timed, null_str);
					aplt_task = new aplt::taplt_task(req_task_.aplt->id, req_task_.task->id, req_task_.ble_device_id, req_task_.position1_uuid, req_task_.position2_uuid, aplt::taplt_task::state_running, 
						klink_aplt_task_->pb_at, klink_aplt_task_->zerotz_t, klink_aplt_task_->timed_at, klink_aplt_task_->aux_key_id);

				} 
			} else {
				if (trigger_by_gui_) {
					VALIDATE(task_vars_.empty(), null_str);
					task_vars_ = req_task_.task_vars;
				}
				aplt_task = new aplt::taplt_task(req_task_.aplt->id, req_task_.task->id, req_task_.ble_device_id, req_task_.position1_uuid, req_task_.position2_uuid, aplt::taplt_task::state_running);
				if (ai_driver_.is_aiagent_tasking()) {
					aplt_task->priority = aplt::priority_preempt_aiagent;
				}
			}
			klink_single_aplt_task2 = klink_aplt_task_;

		} else {
			// fail retry.
			VALIDATE(tmp_curr_aplt == req_task_.aplt, null_str);
			VALIDATE(klink_aplt_task_ == nullptr, null_str);
			VALIDATE(!trigger_by_gui_, null_str);

			aplt_task = retry_aplt_task_;
			retry_aplt_task_ = nullptr;

			VALIDATE(aplt_task->state == aplt::taplt_task::state_running, null_str);
			VALIDATE(aplt_task->fails > 0, null_str);

			if (aplt_task->is_klink()) {
				// this aplt_task is in klink-tasks list.
				set_klink_aplt_task_state(*aplt_task, aplt_task->state);
			}
		}

	}

	curr_aplt = tmp_curr_aplt;

	if (req_task_.task->type == aplt::task_ble) {
		// for task_ble, make sure exist xx.xx.xx__device_id
		const std::string var_name = aplt::ble_builtin_var_name(req_task_.aplt->bundleid, aplt::ble_var_device_id);
		std::string var_val;
		bool insert = false;
		if (in_task_cpp()) {
			if (!task_vars_.existed(var_name)) {
				insert = true;
			}
		} else if (klink_aplt_task_ != nullptr || trigger_by_gui_) {
			var_val = aplt_task->ble_device_id;
			insert = true;
		}
		if (insert) {
			task_vars_.insert_string(var_name, false, var_val);
		}
		VALIDATE(task_vars_.existed(var_name), null_str);
	}

	const ttemp_task_type& task_type = game_config::temp_task_types.find(task_type_aplt_task)->second;
	permissions = task_type.permissions;

	set_req_task_dirty(false);
	return null_str;

}

void tbg_task2::request_single_aplt_task(const aplt::treq_task& req_task)
{
	aplt::ttask_vars task_vars;
	set_req_task(req_task, task_vars);

	const bool original_in_pure_task_cpp = instance->app_in_pure_task_cpp();
	VALIDATE(original_in_pure_task_cpp, null_str);

	const std::string err_msg = shedule();
	if (err_msg.empty()) {
		bg_task_.start_sys2(*this, original_in_pure_task_cpp);
	}
}

std::string tbg_task2::request_gui_aplt_task(const aplt::treq_task& req_task, const aplt::ttask_vars& extra_task_vars,
	const std::function<void (const aplt::ttask_vars& task_vars)>& did_task_finished)
{
	VALIDATE(did_requester_task_finished_ == NULL, null_str);

	aplt::ttask_vars task_vars = aplt::clone_env_vars(true);
	if (!extra_task_vars.empty()) {
		const std::map<std::string, ttask_var>& data = extra_task_vars.data();
		for (std::map<std::string, ttask_var>::const_iterator it = data.begin(); it != data.end(); ++ it) {
			const ttask_var& var = it->second;
			VALIDATE(!var.is_array, "Now only support non-array merge");
			task_vars.insert_attribute(var.name, var.is_array, var.val);
		}
	}
	set_req_task(req_task, task_vars);

	const bool original_in_pure_task_cpp = instance->app_in_pure_task_cpp();
	VALIDATE(!original_in_pure_task_cpp, null_str);

	ttrigger_by_gui_setter setter(*this);

	const std::string err_msg = shedule();
	if (err_msg.empty()) {
		did_requester_task_finished_ = did_task_finished;
		bg_task_.start_sys2(*this, original_in_pure_task_cpp);

		return null_str;

	}

	return err_msg;
}

void tbg_task2::request_retry_aplt_task()
{
	const bool original_in_pure_task_cpp = instance->app_in_pure_task_cpp();
	// VALIDATE(!original_in_pure_task_cpp, null_str);

	VALIDATE(retry_aplt_task_ != nullptr, null_str);
	VALIDATE(aplt_task == nullptr, null_str);

	const std::string err_msg = shedule();
	VALIDATE(err_msg.empty(), null_str);
	bg_task_.start_sys2(*this, original_in_pure_task_cpp);
}

void handle_sync_vars(const aplt::tvar_sensor& sensor)
{
	utils::string_map symbols;
	std::string err_msg;

	for (std::set<std::string>::const_iterator it = sensor.sync_vars.begin(); it != sensor.sync_vars.end(); ++ it) {
		const std::string& sync_var_name = *it;
		symbols["var"] = sync_var_name;
		int sync_var_type = nposm;
		if (aplt::BI_env_var_id_2_types.count(sync_var_name) == 0) {
			err_msg = vgettext2("'$var' isn't environment variable, cannot sync.", symbols);

		} else {
			sync_var_type = aplt::BI_env_var_id_2_types.find(sync_var_name)->second;
			if (var_type_is_BI_task(sync_var_type)) {
				err_msg = vgettext2("'$var' is 'task' variable, cannot sync.", symbols);

			} else if (BI_var_is_auto_update(sync_var_type)) {
				err_msg = vgettext2("'$var' is automatically updated variable, cannot sync.", symbols);
			}
		}

		const aplt::ttask_var* var = nullptr;
		if (err_msg.empty()) {
			if (var_type_is_BI_env_last(sync_var_type)) {
				// it is last env variable.
				int normal_type = BI_env_var_last_2_normal(sync_var_type);
				const std::string& var_name = aplt::builtin_var(normal_type).id;
				symbols["var"] = var_name;
				var = aplt::get_env_var(var_name);

			} else {
				VALIDATE(var_type_is_BI_env_normal(sync_var_type), null_str);
				symbols["var"] = sync_var_name;
				var = aplt::get_env_var(sync_var_name);
			}
			if (var == nullptr) {
				err_msg = vgettext2("'$var' doesn't exist, cannot sync.", symbols);
			}
		}
		
		if (!err_msg.empty()) {
			chinese::curr_pinyin.speak(err_msg);
			instance->add_msg_only_log(logtype_warn, err_msg, 0, false);
			continue;
		}
		
		VALIDATE(!var->is_array, null_str);

		config::attribute_value val;
		if (var_type_is_BI_env_last(sync_var_type)) {
			// aplt::set_env_var(sync_var_type, var->val);
			val = var->val;

		} else if (BI_var_val_type_is_bool(sync_var_type)) {
			val.from_bool(false);

		} else if (BI_var_val_type_is_string(sync_var_type)) {
			val.from_string(null_str, true);

		} else {
			VALIDATE(BI_var_val_type_is_integer(sync_var_type), null_str);
			val.from_int64(0);
		}
		aplt::set_env_var(sync_var_type, val);
	}
}

aplt::tbg_task::tbase_bg_task2* tbg_task2::request_klink_aplt_task(const aplt::taplt_task& _klink_aplt_task, const aplt::ttask_vars& _task_vars)
{
	VALIDATE(did_requester_task_finished_ == NULL, null_str);

	VALIDATE(instance->fg_aplt() == nullptr && !bg_task_.is_ing(), null_str);
	VALIDATE(aplt_task == nullptr, null_str);

	const aplt::tapplet* aplt = aplt::aplt_from_id_ex(applets_, _klink_aplt_task.aplt_id);
	VALIDATE(aplt != nullptr, null_str);
	VALIDATE(aplt->tasks.count(_klink_aplt_task.task_id) != 0, _klink_aplt_task.task_id);
	VALIDATE(_klink_aplt_task.state == aplt::taplt_task::state_running, null_str);
	const aplt::tapplet::ttask& cfg_task = aplt->tasks.find(_klink_aplt_task.task_id)->second;

	std::string position1 = _klink_aplt_task.position1;
	std::string position2 = _klink_aplt_task.position2;
	if (cfg_task.type == aplt::task_cpp) {
		// For simplicity, the klink-gui may make position1/position2 not empty.
		position1.clear();
		position2.clear();
	}

	aplt::treq_task task;
	task.set_aplt_task(*aplt, cfg_task, _klink_aplt_task.ble_device_id, position1, position2);
	set_req_task(task, _task_vars);

	if (_klink_aplt_task.type == taplt_task::type_timed) {
		const std::vector<ttimed_sensor>& sensors = cfg_cpp_api_.timed_sensors();
		VALIDATE(_klink_aplt_task.timed_at >= 0 && _klink_aplt_task.timed_at < (int)sensors.size(), null_str);
		const ttimed_sensor& sensor = sensors[_klink_aplt_task.timed_at];
		for (std::map<std::string, std::string>::const_iterator it = sensor.input_vars.begin(); it != sensor.input_vars.end(); ++ it) {
			const std::string& name = it->first;
			VALIDATE(!name.empty(), null_str);
			const std::string& val = it->second;
			if (val.empty()) {
				continue;
			}
			req_task_.task_vars.insert_string(utils::join_app_prefix_id(aplt->bundleid, name), false, val);
		}
	}

	tklink_aplt_task_setter setter(*this, _klink_aplt_task);
	const std::string err_msg = shedule();
	if (err_msg.empty()) {
		return this;
	}

	if (_klink_aplt_task.type == taplt_task::type_var) {
		const int var_at = _klink_aplt_task.var_at;
		VALIDATE(var_at >= 0 && var_at < (int)cfg_cpp_api_.var_sensors().size(), null_str);
		const aplt::tvar_sensor& sensor = cfg_cpp_api_.var_sensors()[var_at];
		handle_sync_vars(sensor);
	}
	return nullptr;
}

void tbg_task2::aplt_task_pre_navigation()
{
	// VALIDATE(req_task_.type == aplt::treq_task::type_aplt_task, null_str);
	req_task_.validate();

	const aplt::treq_task& task = req_task_;
	const tros_map& curmap = curmap_;
	const std::map<aplt::taplt_key, aplt::tapplet>& applets = applets_;

	VALIDATE(curmap.positions.count(task.position1_uuid) != 0, null_str);
	const tmap_position& position = curmap.positions.find(task.position1_uuid)->second;
	

	utils::string_map symbols;
	symbols["position"] = position.name;
	std::string text;
	VALIDATE(task.valid(), null_str);
	VALIDATE(aplt_task != nullptr, null_str);

	symbols["task"] = task.task->name;
	if (task.position2_uuid.empty()) {
		aplt::ttask_pair for_move = aplt::task_pair_for_move(applets);

		if (task.aplt != for_move.aplt || task.task != for_move.task) {
			text = vgettext2("OK! move to $position|$task", symbols);

		} else {
			text = vgettext2("OK! move to $position", symbols);
		}

	} else {
		VALIDATE(curmap.positions.count(task.position2_uuid) != 0, null_str);
		const tmap_position& position2 = curmap.positions.find(task.position2_uuid)->second;

		symbols["position2"] = position2.name;
		text = vgettext2("OK! move to $position|$task, and send to $position2", symbols);
	}

	chinese::curr_pinyin.speak(text);
}

void tbg_task2::aplt_task_navigation_stopped(bool nav2th, bool result)
{
	// task maybe type_aplt_task, also be other, for example type_move_to
	req_task_.validate();

	const aplt::treq_task& task = req_task_;
	VALIDATE(aplt_task != nullptr, null_str);

	const tros_map& curmap = curmap_;
	const std::string& position_uuid = nav2th? task.position2_uuid: task.position1_uuid;
	VALIDATE(curmap.positions.count(position_uuid) != 0, null_str);
	const tmap_position& position = curmap.positions.find(position_uuid)->second;

	utils::string_map symbols;
	symbols["position"] = position.name;
	std::string text;
	if (result) {
		text = vgettext2("Arrived at $position", symbols);

	} else {
		text = vgettext2("Unable to reach $position", symbols);
	}

	chinese::curr_pinyin.speak(text);
}

void tbg_task2::set_klink_aplt_task_state(int type, int pb_at, int state)
{
	VALIDATE(type == aplt::taplt_task::type_iot || type == aplt::taplt_task::type_speech || 
		type == aplt::taplt_task::type_var || type == aplt::taplt_task::type_timed, null_str);
	VALIDATE(pb_at >= 0, null_str);

	// this aplt_task is in event/timing tasks list.
	const aplt::taplt_task& klink_aplt_task = bg_task_.task_from_pb_at(type, pb_at);

	const aplt::tapplet* aplt = aplt::aplt_from_id_ex(applets_, klink_aplt_task.aplt_id);
	VALIDATE(aplt != nullptr && aplt->tasks.count(klink_aplt_task.task_id) != 0, null_str);
	const aplt::tapplet::ttask& cfg_task = aplt->tasks.find(klink_aplt_task.task_id)->second;
	if (aplt::is_single_task(cfg_task.type)) {
		VALIDATE(klink_single_aplt_task2 == &klink_aplt_task, null_str);
		VALIDATE(klink_cpp_aplt_task2 == nullptr, null_str);
	} else {
		VALIDATE(klink_cpp_aplt_task2 == &klink_aplt_task, null_str);
		VALIDATE(klink_single_aplt_task2 == nullptr, null_str);
	}

	// Normally, if it's just change @state, set write_pb to false.
	// but here write_pb = true. once result is fail, avoid restart launcher immediately, this aplt_task is executed again.
	// Because don't write to pb, the state is always state_fresh.
	bg_task_.modify_task_state(klink_aplt_task, state, true);
/*
	const aplt::taplt_task* new_task = &bg_task_.modify_task_state(klink_aplt_task, state, true);

	if (klink_cpp_aplt_task2 != nullptr) {
		// in tasks_, old task is erased, pointer require to update.
		klink_cpp_aplt_task2 = new_task;
	}

	if (klink_single_aplt_task2 != nullptr) {
		// in tasks_, old task is erased, pointer require to update.
		klink_single_aplt_task2 = new_task;
	}
*/
}

void tbg_task2::set_klink_aplt_task_state(const aplt::taplt_task& aplt_task, int state)
{
	VALIDATE(aplt_task.is_klink(), null_str);
	VALIDATE(aplt_task.pb_at >= 0, null_str);

	// this aplt_task is in klink list.
	const aplt::taplt_task& klink_aplt_task = bg_task_.task_from_pb_at(aplt_task.type, aplt_task.pb_at);
	VALIDATE(aplt_task.equal8(klink_aplt_task), null_str);

	set_klink_aplt_task_state(aplt_task.type, aplt_task.pb_at, state);
}

void tbg_task2::nullptr_aplt_task()
{
	VALIDATE(aplt_task != nullptr, null_str);

	delete aplt_task;
	aplt_task = nullptr;
	VALIDATE(camera_api_ == nullptr, null_str);
	VALIDATE(nonblock_api_ == nullptr, null_str);
	klink_single_aplt_task2 = nullptr;
}

void tbg_task2::task_finished(const aplt::taplt_task* _aplt_task)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(retry_aplt_task_ == nullptr, null_str);

	int var_at = nposm;
	if (_aplt_task != nullptr && _aplt_task->type == aplt::taplt_task::type_var) {
		VALIDATE(IS_VALID_APLT_TASK_VAR_AT(_aplt_task->var_at), null_str);
		var_at = _aplt_task->var_at;
	}

	curr_aplt = nullptr;
	permissions.clear();

	bool unload_deps = true;
	// bool fail_retry = false;
	if (_aplt_task != nullptr) {
		VALIDATE(req_task_.valid(), null_str);
		VALIDATE(aplt_task == _aplt_task, null_str);

		bool exe_result2 = false;
		aplt::taplt_task& task = *aplt_task;
		if (task.fails == 0) {
			if (task.is_klink()) {
				// this aplt_task is in klink-tasks list.
				set_klink_aplt_task_state(task, task.state);
			}
			exe_result2 = task.state == aplt::taplt_task::state_finished_ok;
			
			nullptr_aplt_task();

			if (in_task_cpp()) {
				// this is single task, and in task_cpp, uload_deps when task_cpp finished.
				unload_deps = false;
				task_cpp_api_->single_task_finished(req_task_, exe_result2);
			}

		} else {
			// when fail retry.
			retry_aplt_task_ = aplt_task;
			aplt_task = nullptr;

			// fail_retry = true;
			exe_result2 = false;
			VALIDATE(task.state == aplt::taplt_task::state_running, null_str);
			if (task.is_klink()) {
				set_klink_aplt_task_state(task, aplt::taplt_task::state_finished_fail);
			}
			set_req_task_dirty(true);

			// ??In the midst of this, will a new req be issued, 
			// so that the fail-again-req will not be executed?
			unload_deps = false;
		}

	} else {
		// in task_cpp
		// some task type don't use taplt_task, for example type_moveto
		VALIDATE(aplt_task == nullptr, null_str);
		VALIDATE(klink_single_aplt_task2 == nullptr, null_str);

		VALIDATE(in_pure_task_cpp(), null_str);
		VALIDATE(var_at == nposm, null_str);

		// end this task_cpp
		// if trigger by gui-center, klink_cpp_aplt_task2 is nullptr.
		if (klink_cpp_aplt_task2 != nullptr) {
			if (klink_cpp_aplt_task2->type == aplt::taplt_task::type_var) {
				VALIDATE(IS_VALID_APLT_TASK_VAR_AT(klink_cpp_aplt_task2->var_at), null_str);
				var_at = klink_cpp_aplt_task2->var_at;
			}

			set_klink_aplt_task_state(klink_cpp_aplt_task2->type, klink_cpp_aplt_task2->pb_at, aplt::taplt_task::state_finished_ok);
			klink_cpp_aplt_task2 = nullptr;
		}
		task_cpp_api_->task_finished(*task_cpp_aplt_, * task_cpp_cfg_task_, true);

		task_cpp_api_ = nullptr;
		task_cpp_aplt_ = nullptr;
		task_cpp_cfg_task_ =  nullptr;
	}

	if (unload_deps) {
		if (did_requester_task_finished_ != NULL) {
			did_requester_task_finished_(task_vars_);
			did_requester_task_finished_ = NULL;
		}
		task_vars_.clear();

		drivers_.unload_deps();

		req_task_.clear();

		if (var_at != nposm) {
			const aplt::tvar_sensor& sensor = cfg_cpp_api_.var_sensors()[var_at];
			handle_sync_vars(sensor);
		}
	}
}


void tbg_task2::set_req_task_dirty(bool val)
{
	if (!val) {
		VALIDATE(req_task_dirty_, null_str);
		VALIDATE(task_require_start_ticks_ != 0, null_str);
	} else if (req_task_dirty_) {
		// both val and req_task_dirty_ are true, it is maybe happens.
	}

	req_task_dirty_ = val;
	if (val) {
		// on windows, maybe use debugpoint, more long.
		const int threshold = game_config::os == os_windows? 20000: 5000; // 5 second
		task_require_start_ticks_ = SDL_GetTicks() + threshold;
	} else {
		task_require_start_ticks_ = 0;
	}
}

aplt::ttask_pair tbg_task2::task_cpp_pair() const
{
	VALIDATE(task_cpp_aplt_ != nullptr && task_cpp_cfg_task_ != nullptr, null_str);
	return aplt::ttask_pair(*task_cpp_aplt_, *task_cpp_cfg_task_);
}

const tapplet::ttask* tbg_task2::get_aiagent_cfg_task() const
{
	const aplt::ttask_pair& pair = ai_driver_.aiagent_task_pair();
	return pair.task;
}

void tbg_task2::set_camera_api(aplt::tcamera_api* camera_api)
{
	if (camera_api != nullptr) {
		VALIDATE(camera_api_ == nullptr, null_str);
	} else {
		VALIDATE(camera_api_ != nullptr, null_str);
	}
	camera_api_ = camera_api;
}

void tbg_task2::set_nonblock_api(aplt::tnonblock_api* nonblock_api)
{
	if (nonblock_api != nullptr) {
		VALIDATE(nonblock_api_ == nullptr, null_str);
	} else {
		VALIDATE(nonblock_api_ != nullptr, null_str);
	}
	nonblock_api_ = nonblock_api;
}

std::string tbg_task2::curr_base_scene_name(int& subtask_state) const
{
	subtask_state = base_driver_.subtask_state();
	const std::string& scene_id = base_driver_.scene_id();
	if (scene_id.empty()) {
		return null_str;
	}

	const tbase_scene* scene = cfg_cpp_api_.base_scene_from_id(scene_id, true);
	return scene->name();
}

void tbg_task2::did_scan_subscribed(const sensor_msgs::LaserScan& msg, SDL_2Point& charging)
{
	VALIDATE_IN_MAIN_THREAD();

	if (nonblock_api_ != nullptr) {
		VALIDATE(!bg_task_.is_luafunc_finished(), null_str);
		VALIDATE(req_task_.aplt != nullptr && req_task_.task != nullptr && req_task_.task->type == aplt::task_nonblock, null_str);
		// charging = nonblock_api_->did_scan_subscribed(msg.ranges, SDL_FRange{msg.range_min, msg.range_max}, 
		//	SDL_FRange{msg.angle_min, msg.angle_max}, msg.angle_increment);
		charging = nonblock_api_->did_scan_subscribed(&msg);

	} else {
		charging = SDL_2Point{nposm, nposm, nposm, nposm};
	}
}

bool tbg_task2::speech_did_recognition_result(const std::string& result)
{
	VALIDATE(task_cpp_api_ != nullptr, null_str);
	return task_cpp_api_->speech_did_recognition_result(result);
}

void tbg_task2::finish_task_camera_or_charge(bool when_stop_aplt_task, const tapplet& aplt, const aplt::tapplet::ttask& cfg_task, int task_type)
{
	if (task_type == aplt::task_camera) {
		// Must first exit camera.
		// because camera_work_frame() requires some resources to be valid,
		// resources will be nullptr when subsequent camera_api_->task_finished.
		if (!when_stop_aplt_task || camera_.tasking()) {
			camera_.exit_task(tcamera::taskid_bgtask);
			camera_.set_slot(nullptr);
		}

		if (!when_stop_aplt_task || camera_api() != nullptr) {
			camera_api()->task_finished(aplt, cfg_task);
			set_camera_api(nullptr);
		}

		if (when_stop_aplt_task) {
			if (base_driver_.subtask_state() == sts_preempted) {
				base_driver_.resume_subtask();
			}
		}

	} else { 
		VALIDATE(task_type == aplt::task_nonblock, null_str);

		if (!when_stop_aplt_task || nonblock_api() != nullptr) {
			nonblock_api()->task_finished(aplt, cfg_task);
			set_nonblock_api(nullptr);
		}
	}
}

void tbg_task2::slice()
{
	if (!req_task_dirty_) {
		// if fail_retry result, don't call cpp.slice().
		if (in_task_cpp()) {
			// can not call task_api_->cpp.slice() in tbg_task2::shedule().
			// because if 'bg_task_.running().task_cpp_fake_started == true', tbg_task2::shedule() will not called always.
			VALIDATE(task_cpp_api_->task_started(), null_str);
			task_cpp_api_->slice();
		}
		if (camera_api_ != nullptr && !bg_task_.is_luafunc_finished()) {
			VALIDATE(req_task_.aplt != nullptr && req_task_.task != nullptr && req_task_.task->type == aplt::task_camera, null_str);
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
			if (finished) {
				finish_task_camera_or_charge(false, *req_task_.aplt, *req_task_.task, aplt::task_camera);
				bg_task_.set_luafunc_finished();
			}
		}
		if (nonblock_api_ != nullptr && !bg_task_.is_luafunc_finished()) {
			VALIDATE(req_task_.aplt != nullptr && req_task_.task != nullptr && req_task_.task->type == aplt::task_nonblock, null_str);

			bool finished = nonblock_api_->slice();
			if (finished) {
				finish_task_camera_or_charge(false, *req_task_.aplt, *req_task_.task, aplt::task_nonblock);
				bg_task_.set_luafunc_finished();
				if (req_task_.task->type == aplt::task_nonblock && req_task_.task->subtype == aplt::tnonblock_api::subtype_charge) {
					chinese::curr_pinyin.speak(_("Start charging"));
				}
			}
		}
	}
}

void tbg_task2::camera_work_frame(const surface& surf)
{
	tsurface_2_mat_lock lock(surf);
	if (camera_api_ != nullptr && !bg_task_.is_luafunc_finished()) {
		// bg_task_.set_luafunc_finished() maybe call by tros_cpp_api, because cpp_task's time expires.
		// VALIDATE(!bg_task_.is_luafunc_finished(), null_str);
		VALIDATE(req_task_.aplt != nullptr && req_task_.task != nullptr && req_task_.task->type == aplt::task_camera, null_str);
		camera_api_->camera_work_frame(surf, lock.mat);
	}
}

bool tbg_task2::can_save_taskpoint(const aplt::taplt_task* new_klink_aplt_task) const
{
	VALIDATE(bg_task_.is_ing(), null_str);
	if (new_klink_aplt_task != nullptr) {
		VALIDATE(bg_task_.in_4tasks(*new_klink_aplt_task), null_str);
	}

	if (klink_cpp_aplt_task2 == nullptr) {
		return false;
	}

	if (klink_cpp_aplt_task2 == new_klink_aplt_task) {  
		// why don't 'return false, see (main.cpp)game_instance::request_klink_aplt_task2
		// return false;
	}

	VALIDATE(in_task_cpp(), null_str);
	return task_cpp_cfg_task_->recoverable;
}

}