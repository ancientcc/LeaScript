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

#include "ai_driver.hpp"
#include "rose_config.hpp"
#include "wml_exception.hpp"
#include "chinese.hpp"
#include "gettext.hpp"
#include "aplt.hpp"
#include "base_instance.hpp"

using namespace std::placeholders;


tai_driver::tai_driver(tdrivers& drivers, aplt::tbg_task& bg_task)
	: slot(nullptr)
	, bg_task_(bg_task)
	, drivers_(drivers)
	, task_api_(nullptr)
	, aiagent_api_(nullptr)
	, aiagent_caller_task_api_(nullptr)
	, aiagent_task_finished_(false)
{}

void tai_driver::set_slot(const std::string& _aplt_id, aplt::tai_slot* _slot)
{
	if (slot != nullptr) {
		if (is_aiagent_tasking()) {
			stop_aiagent_task();
		}
		delete slot;
		slot = nullptr;
	} else {
		// VALIDATE(!is_capturing(), null_str);
	}

	if (_slot != nullptr) {
		VALIDATE(!_aplt_id.empty(), null_str);
		slot = _slot;
		// slot->set_receiver(*this);

	} else {
		VALIDATE(_aplt_id.empty(), null_str);
	}
	aplt_id_ = _aplt_id;

	if (slot != nullptr) {
		// start();
	}
}

void tai_driver::did_start_aiagent_task_quited(const aplt::ttask_api* caller_task_api, std::string& err_msg)
{
	// VALIDATE(subtask_state_ == aplt::sts_nposm || subtask_state_ == aplt::sts_idle || subtask_state_ == aplt::sts_preempted, null_str);
	// VALIDATE(!scene_id_.empty(), null_str);
	// const aplt::tbase_scene* curr_scene = cfg_cpp_api_.base_scene_from_id(scene_id_, true);
	// int original_subtask_state = subtask_state_;

	if (!err_msg.empty()) {
		VALIDATE(task_api_ == nullptr, null_str);
		VALIDATE(aiagent_api_ == nullptr, null_str);
		VALIDATE(aiagent_caller_task_api_ == nullptr, null_str);
		VALIDATE(aiagent_task_pair_.aplt == nullptr && aiagent_task_pair_.task == nullptr, null_str);

		const library& lib = drivers_.find_by_type(apltsotype_aiagent_task);
		if (caller_task_api == nullptr && lib.get() != nullptr) {
			drivers_.set_special_applet(apltsotype_aiagent_task, nullptr);
		}

		utils::string_map symbols;
		symbols["reason"] = err_msg;

		instance->add_msg_only_log(logtype_warn, vgettext2("Start aiagent task fail. $reason", symbols), 0, false);
	}
}

void tai_driver::start_aiagent_task(aplt::ttask_api* caller_task_api, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task, const std::string& question, const surface& surf)
{
	VALIDATE_IN_MAIN_THREAD();
	// VALIDATE(!(bg_task_.is_ing() && bg_task_.in_which_single_task() == aplt::task_camera), null_str);


	utils::string_map symbols;
	std::string err_msg;
	tauto_destruct_executor destruct_executor(std::bind(&tai_driver::did_start_aiagent_task_quited, this, caller_task_api, std::ref(err_msg)));

	const library& lib = drivers_.find_by_type(apltsotype_aiagent_task);

	symbols["aplt"] = aplt.name2();
	symbols["aplt_task"] = cfg_task.id;
	if (caller_task_api == nullptr) {
		VALIDATE(lib.get() == nullptr, null_str);
		drivers_.set_special_applet(apltsotype_aiagent_task, &aplt);
		if (lib.get() == nullptr) {
			err_msg = vgettext2("Cannot load applet '$aplt_task' located in *. so.", symbols);
			return;
		}
	}
	VALIDATE(lib.get() != nullptr, null_str);


	aplt::ttask_api* task_api = nullptr;
	if (caller_task_api == nullptr) {
		trose_library* rose_lib = lib.get();
		void* v_task = nullptr;
		if (rose_lib->create_task_api != nullptr) {
			v_task = rose_lib->create_task_api(const_cast<aplt::tapplet*>(&aplt));
		}
		if (v_task == nullptr) {
			err_msg = _("Not implete 'aplt_create_task_api' method.");
			return;
		}

		task_api = reinterpret_cast<aplt::ttask_api*>(v_task);

	} else {
		task_api = caller_task_api;
		VALIDATE(task_api->aiagent != nullptr, null_str);
	}
	if (task_api->aiagent != nullptr) {
		aplt::setup_aplt_user_data_dir(get_aplt_user_data_dir(bundleid_2_lua_bundleid(aplt.bundleid)));

		task_vars_ = aplt::clone_env_vars(true);
/*
		for (std::map<std::string, std::string>::const_iterator it = scene.input_vars.begin(); it != scene.input_vars.end(); ++it) {
			const std::string& name = it->first;
			const std::string& val = it->second;
			task_vars_.insert_string(utils::join_app_prefix_id(pair.aplt->bundleid, name), false, val);
		}
*/

		err_msg = task_api->aiagent->start_task(aplt, cfg_task, question, surf, task_vars_);
		// if tcpp_api/tcamera_api.start_task() fail, don't let tcpp_api/tcamera_api.task_finished() be called.
		if (err_msg.empty()) {
			task_api_ = task_api;
			aiagent_api_ = task_api->aiagent;
			aiagent_task_pair_ = aplt::ttask_pair(aplt, cfg_task);
			aiagent_caller_task_api_ = caller_task_api;
			// scene_id_ = scene.id;

		} else {
			if (caller_task_api == nullptr) {
				delete task_api;
			}
			task_vars_.clear();
		}

	} else {
		VALIDATE(caller_task_api == nullptr, null_str);
		delete task_api;
		err_msg = _("Not implete aplt::taiagent_api object.");
	}
/*
	if (err_msg.empty()) {
		// stop current speak
		chinese::curr_pinyin.speak(null_str);

		aplt::tbase_ext_lamp* lamp = slot->query_ext_lamp();
		if (lamp != nullptr) {
			lamp->lamp_ctrl_led(aplt::tbase_ext_lamp::ledtype_scene, aplt::tbase_ext_lamp::ledact_on);
		}
	}
*/
}

void tai_driver::stop_aiagent_task()
{
	VALIDATE_IN_MAIN_THREAD();

	// Indeed, when a task_camera's bg_task is running, 
	// base_subtask related operations, including this stop_subtask_internal(), cannot be executed. 
	// However, during app_did_navigation_bh_use_cpp(), switching the state to sts_preempted precisely satisfies this condition.
	// VALIDATE(!(bg_task_.is_ing() && bg_task_.in_which_single_task() == aplt::task_camera), null_str);

	// VALIDATE(subtask_state_ == aplt::sts_ing || sts_is_idle2(subtask_state_), null_str);

	const aplt::ttask_api* caller_task_api = aiagent_caller_task_api_;
	{
		// VALIDATE(camera_.tasking(), null_str);
		VALIDATE(task_api_ != nullptr, null_str);
		VALIDATE(aiagent_api_ != nullptr, null_str);
		VALIDATE(aiagent_task_pair_.aplt != nullptr && aiagent_task_pair_.task != nullptr, null_str);
		// VALIDATE(subtask_state_ == aplt::sts_ing, null_str);

		// Must first exit camera.
		// because camera_work_frame() requires some resources to be valid, such as mediapipe_ptr.
		// mediapipe_ptr will be nullptr when subsequent camera_api_->task_finished.
		// camera_.exit_task(tcamera::taskid_base_subtask);
		// camera_.set_slot(nullptr);

		aiagent_api_->set_terminating();

		if (slot->is_nlp_questioning()) {
			slot->stop_nlp_question();
		}
		if (bg_task_.is_ing()) {
			const aplt::taplt_task* aplt_task = bg_task_.bg_task2().aplt_task;
			if (aplt_task != nullptr && aplt_task->priority == aplt::priority_preempt_aiagent) {
				instance->stop_bg_task_if_runing(null_str, _("User cancel aiagent task, stop the current one"));
			}
		}

		// cfg_cpp_api_.base_scene_from_id(scene_id_, true);
		aiagent_api_->task_finished(*aiagent_task_pair_.aplt, *aiagent_task_pair_.task);

		if (caller_task_api == nullptr) {
			delete task_api_;
		}

		task_api_ = nullptr;
		aiagent_api_ = nullptr;
		aiagent_task_pair_.aplt = nullptr;
		aiagent_task_pair_.task = nullptr;
		aiagent_caller_task_api_ = nullptr;

		task_vars_.clear();
		aiagent_task_finished_ = false;
	}

	if (caller_task_api == nullptr) {
		drivers_.set_special_applet(apltsotype_aiagent_task, nullptr);
	}
/*
	// stop current speak
	chinese::curr_pinyin.speak(null_str);

	aplt::tbase_ext_lamp* lamp = slot->query_ext_lamp();
	if (lamp != nullptr) {
		lamp->lamp_ctrl_led(aplt::tbase_ext_lamp::ledtype_scene, aplt::tbase_ext_lamp::ledact_off);
	}
*/
}
/*
void tai_driver::restart()
{
	VALIDATE_IN_MAIN_THREAD();
	if (aplt_id_.empty()) {
		VALIDATE(slot == nullptr, null_str);
		return;
	}

	VALIDATE(slot != nullptr, null_str);
	set_slot(null_str, nullptr);
	drivers_.refresh(false);
}
*/
void tai_driver::slice()
{
	if (slot == nullptr) {
		return;
	}

	if (aiagent_api_ != nullptr && !aiagent_task_finished_) {
		// VALIDATE(req_task_.aplt != nullptr && req_task_.task != nullptr && req_task_.task->type == aplt::task_camera, null_str);
/*
		if (camera_viewer_ == nullptr) {
			// is no temporary slot, tbg_task2 derminate when to call camera_.slice(...)
			camera_.slice(empty_rect, false);
		} else {
			// camera_.slice is called by gui2::tcenter::did_draw_paper(...)
		}
*/
		std::string result_str;
		std::vector<std::pair<float, SDL_Rect> > classifier_rects;
		bool finished = aiagent_api_->slice();
		// VALIDATE(!finished, null_str);
		aiagent_task_finished_ = finished;
		if (finished) {
			// const std::string err_msg = _("Aiagent task has ended early, and AI driver will enter a no aiagent task state");
			// instance->add_msg_only_log(logtype_warn, err_msg, 0, false);
			// chinese::curr_pinyin.speak(err_msg);

			stop_aiagent_task();
		}
	}

	slot->slice();
}

void tai_driver::send_nlp_question(int chatsrc, bool new_conversation, const std::string& question, const surface& surf)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);

	slot->send_nlp_question(chatsrc, new_conversation, question, surf, NULL);
}

bool tai_driver::is_nlp_questioning() const
{
	VALIDATE(slot != nullptr, null_str);
	return slot->is_nlp_questioning();
}

void tai_driver::stop_nlp_question()
{
	VALIDATE(slot != nullptr, null_str);
	slot->stop_nlp_question();
}