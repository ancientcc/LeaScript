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

// #include "rose_ros/aplt.hpp"
#include "aplt_api.hpp"
#include "so_aplt_task_helper.hpp"
#include "gettext.hpp"

using namespace std::placeholders;

namespace aplt {

//
// nonblock api
//
thelper_nonblock_task_slot::thelper_nonblock_task_slot(thelper_lua_nonblock& lua_nonblock, bool use_work_thread, tapplet& aplt, 
	const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars, bool& finished)
	: lua_nonblock_(lua_nonblock)
	, use_work_thread_(use_work_thread)
	, aplt_(aplt)
	, cfg_task_(cfg_task)
	, task_vars_(task_vars)
	, finished_(finished)
	, pinyin_(aplt::get_curr_pinyin())
	, b_api_(aplt::get_b_api())
	// , r_api_(aplt::get_r_api())
{
	lua_nonblock_.set_task_slot(this);
}

thelper_nonblock_task_slot::~thelper_nonblock_task_slot()
{
	lua_nonblock_.set_task_slot(nullptr);
}

//
// thelper_nonblock_api
//
thelper_nonblock_api::thelper_nonblock_api(tapplet& aplt)
	: aplt_(aplt)
	, b_api_(aplt::get_b_api())
	// , r_api_(aplt::get_r_api())
{
	clear();
}

thelper_nonblock_api::~thelper_nonblock_api()
{
	validate_nposm();
}

std::string thelper_nonblock_api::app_start_task(ttask_vars& vars)
{
	VALIDATE_IN_MAIN_THREAD();
	if (subtype_ == subtype_charge) {
		VALIDATE(b_api_.navigation_node_started(), null_str);
	}
	validate_nposm();

	const tapplet& aplt = aplt_;

	if (task_ids_.count(task_id_) == 0) {
		utils::string_map symbols;
		symbols["task"] = task_id_;
		return vgettext2("Unknonw task: $task", symbols);
	}

	int code = task_ids_.find(task_id_)->second;

	const tapplet::ttask& cfg_task = aplt.tasks.find(task_id_)->second;
	ttask_vars& task_vars = vars;
	VALIDATE(curr_code_ == nposm, null_str);
	VALIDATE(slot_ == nullptr, null_str);

	slot_ = app_create_nonblock_task_slot(code, cfg_task, task_vars, finished_);
	VALIDATE(slot_ != nullptr, null_str);
	slot_->clear_output_vars(task_vars);

	std::string err_msg = slot_->pre_start_task();
	curr_code_ = code;

	if (!err_msg.empty()) {
		// if start_task, launcher will not call my 'task_finished'
		app_task_finished(cfg_task);

	} else {
		if (slot_->use_work_thread()) {
			work_thread_.reset(create_rose_thread(std::bind(&thelper_nonblock_api::DoWork, this, _1), NULL, NULL, NULL, "nonblock work thread"));
		}
	}

	return err_msg;
}

void thelper_nonblock_api::app_task_finished(const tapplet::ttask& cfg_task)
{
	if (slot_->use_work_thread()) {
		if (work_thread_.get() != nullptr) {
			work_thread_.reset();
		}
	}

	VALIDATE(curr_code_ != nposm, null_str);
	VALIDATE(slot_ != nullptr, null_str);

	slot_->task_finished(cfg_task);
	
	delete slot_;
	slot_ = nullptr;

	curr_code_ = nposm;

	clear();
}

bool thelper_nonblock_api::slice()
{
	VALIDATE_IN_MAIN_THREAD();
	// VALIDATE(start_ticks_ != 0, null_str);

	return finished_;
}

void thelper_nonblock_api::DoWork(bool& exit)
{
	VALIDATE(slot_ != nullptr, null_str);
	VALIDATE(slot_->use_work_thread(), null_str);

	slot_->nonmain_start_task(exit);
}

//
// block api
//
thelper_block_task_slot::thelper_block_task_slot(thelper_lua_block& lua_block, tapplet& aplt)
	: lua_block_(lua_block)
	, aplt_(aplt)
	, pinyin_(aplt::get_curr_pinyin())
	, b_api_(aplt::get_b_api())
	// , r_api_(aplt::get_r_api())
{
	// lua_block_.set_task_slot(this);
}

thelper_block_task_slot::~thelper_block_task_slot()
{
	// lua_block_.set_task_slot(nullptr);
}

//
// thelper_nonblock_api
//
thelper_block_api::thelper_block_api(thelper_lua_block& lua_block, tapplet& aplt)
	: lua_block_(lua_block)
	, aplt_(aplt)
	, b_api_(aplt::get_b_api())
	// , r_api_(aplt::get_r_api())
	, slot_(nullptr)
{
}

thelper_block_api::~thelper_block_api()
{
	validate_nposm();
}

std::string thelper_block_api::app_start_task(ttask_vars& vars)
{
	VALIDATE_IN_MAIN_THREAD();
	validate_nposm();

	const tapplet& aplt = aplt_;

	if (task_ids_.count(task_id_) == 0) {
		utils::string_map symbols;
		symbols["task"] = task_id_;
		return vgettext2("Unknonw task: $task", symbols);
	}

	int code = task_ids_.find(task_id_)->second;

	const tapplet::ttask& cfg_task = aplt.tasks.find(task_id_)->second;
	ttask_vars& task_vars = vars;

	VALIDATE(slot_ == nullptr, null_str);

	bool help_api_delete = true;
	slot_ = app_create_block_task_slot(code, help_api_delete);
	VALIDATE(slot_ != nullptr, null_str);
	lua_block_.set_task_slot(slot_);
	slot_->clear_output_vars(task_vars);

	const std::string err_msg = slot_->start_task(code, cfg_task, task_vars);

	if (help_api_delete) {
		delete slot_;
	}
	lua_block_.set_task_slot(nullptr);
	slot_ = nullptr;

	return err_msg;
}

//
// aiagent api
//
thelper_aiagent_task_slot::thelper_aiagent_task_slot(thelper_lua_aiagent& lua_aiagent, bool use_work_thread, tapplet& aplt,
	const aplt::tapplet::ttask& cfg_task, const std::string& question, const surface& surf, ttask_vars& task_vars, bool& finished)
	: lua_aiagent_(lua_aiagent)
	, use_work_thread_(use_work_thread)
	, aplt_(aplt)
	, cfg_task_(cfg_task)
	, task_vars_(task_vars)
	, question_(question)
	, surf_(surf)
	, finished_(finished)
	, pinyin_(aplt::get_curr_pinyin())
	, b_api_(aplt::get_b_api())
	// , r_api_(aplt::get_r_api())
{
	lua_aiagent_.set_task_slot(this);
}

thelper_aiagent_task_slot::~thelper_aiagent_task_slot()
{
	lua_aiagent_.set_task_slot(nullptr);
}

std::string thelper_aiagent_task_slot::request_aplt_task(const tapplet& aplt, const tapplet::ttask& cfg_task, const aplt::ttask_vars& task_vars,
		const std::function<void (const aplt::ttask_vars& task_vars)>& did_task_finished)
{
	aplt::treq_task req_task;

	req_task.set_aplt_task(aplt, cfg_task, null_str, null_str, null_str);
	return b_api_.request_task(req_task, task_vars, did_task_finished);
}

//
// thelper_aiagent_api
//
thelper_aiagent_api::thelper_aiagent_api(tapplet& aplt)
	: aplt_(aplt)
	, b_api_(aplt::get_b_api())
	// , r_api_(aplt::get_r_api())
	// , task_ids_({{"deepseek", aiagent_deepseek}})
{
	clear();
}

thelper_aiagent_api::~thelper_aiagent_api()
{
	validate_nposm();
}

std::string thelper_aiagent_api::app_start_task(const std::string& question, const surface& surf, ttask_vars& vars)
{
	VALIDATE_IN_MAIN_THREAD();
	validate_nposm();

	const tapplet& aplt = aplt_;

	if (task_ids_.count(task_id_) == 0) {
		utils::string_map symbols;
		symbols["task"] = task_id_;
		return vgettext2("Unknonw task: $task", symbols);
	}

	int code = task_ids_.find(task_id_)->second;

	const tapplet::ttask& cfg_task = aplt.tasks.find(task_id_)->second;
	ttask_vars& task_vars = vars;
	VALIDATE(curr_code_ == nposm, null_str);
	VALIDATE(slot_ == nullptr, null_str);

	slot_ = app_create_aiagent_task_slot(code, cfg_task, question, surf, task_vars, finished_);
	VALIDATE(slot_ != nullptr, null_str);
	slot_->clear_output_vars(task_vars);

	std::string err_msg = slot_->pre_start_task();
	curr_code_ = code;

	if (!err_msg.empty()) {
		// if start_task, launcher will not call my 'task_finished'
		app_task_finished(cfg_task);

	}
	else {
		if (slot_->use_work_thread()) {
			work_thread_.reset(create_rose_thread(std::bind(&thelper_aiagent_api::DoWork, this, _1), NULL, NULL, NULL, "anagent work thread"));
		}
	}

	return err_msg;
}

void thelper_aiagent_api::app_task_finished(const tapplet::ttask& cfg_task)
{
	if (slot_->use_work_thread()) {
		if (work_thread_.get() != nullptr) {
			work_thread_.reset();
		}
	}

	VALIDATE(curr_code_ != nposm, null_str);
	VALIDATE(slot_ != nullptr, null_str);

	slot_->task_finished(cfg_task);

	delete slot_;
	slot_ = nullptr;

	curr_code_ = nposm;

	clear();
}

bool thelper_aiagent_api::slice()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot_ != nullptr, null_str);

	slot_->slice();

	return finished_;
}

void thelper_aiagent_api::DoWork(bool& exit)
{
	VALIDATE(slot_ != nullptr, null_str);
	VALIDATE(slot_->use_work_thread(), null_str);

	slot_->nonmain_start_task(exit);
}

//
// thelper_camera_api
//
thelper_camera_task_slot::thelper_camera_task_slot(thelper_lua_camera& lua_camera, tapplet& aplt, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
	: lua_camera_(lua_camera)
	, aplt_(aplt)
	, cfg_task_(cfg_task)
	, task_vars_(task_vars)
	, pinyin_(aplt::get_curr_pinyin())
	, b_api_(aplt::get_b_api())
	// , r_api_(aplt::get_r_api())
{
	lua_camera_.set_task_slot(this);
}

thelper_camera_task_slot::~thelper_camera_task_slot()
{
	lua_camera_.set_task_slot(nullptr);
}

//
// thelper_camera_api
//
thelper_camera_api::thelper_camera_api(/*thelper_lua_camera& lua_camera,*/ tapplet& aplt)
	: aplt_(aplt)
	, b_api_(aplt::get_b_api())
	// , r_api_(aplt::get_r_api())
{
	clear();
}

thelper_camera_api::~thelper_camera_api()
{
	validate_nposm();
}

std::string thelper_camera_api::app_start_task(ttask_vars& vars)
{
	VALIDATE_IN_MAIN_THREAD();
	const tapplet& aplt = aplt_;

	if (task_ids_.count(task_id_) == 0) {
		utils::string_map symbols;
		symbols["task"] = task_id_;
		return vgettext2("Unknonw task: $task", symbols);
	}

	int code = task_ids_.find(task_id_)->second;

	std::string err_msg = app_pre_create_slot(code);
	if (!err_msg.empty()) {
		return err_msg;
	}

	const tapplet::ttask& cfg_task = aplt.tasks.find(task_id_)->second;
	ttask_vars& task_vars = vars;
	VALIDATE(curr_code_ == nposm, null_str);
	VALIDATE(slot_ == nullptr, null_str);

	slot_ = app_create_camera_task_slot(code, cfg_task, task_vars);
	slot_->clear_output_vars(task_vars);

	err_msg = slot_->start_task();

	curr_code_ = code;
	if (!err_msg.empty()) {
		// if start_task, launcher will not call my 'task_finished'
		app_task_finished(cfg_task);
	}

	return err_msg;
}

void thelper_camera_api::app_task_finished(const tapplet::ttask& cfg_task)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(curr_code_ != nposm, null_str);
	VALIDATE(slot_ != nullptr, null_str);

	slot_->task_finished(cfg_task);
	
	delete slot_;
	slot_ = nullptr;

	curr_code_ = nposm;
}

bool thelper_camera_api::slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(slot_ != nullptr, null_str);
	return slot_->slice(result_str, classifier_rects);
}

bool thelper_camera_api::camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(slot_ != nullptr, null_str);
	return slot_->camera_use_cv_frame(argb, cv_argb);
}

void thelper_camera_api::set_wko_task_slot(twko_task_slot* slot)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(slot_ != nullptr, null_str);
	return slot_->set_wko_task_slot(slot);
}

void thelper_camera_api::camera_work_frame(const surface& surf, const cv::Mat& argb)
{
	VALIDATE_NOT_MAIN_THREAD();

	VALIDATE(slot_ != nullptr, null_str);
	slot_->camera_work_frame(surf, argb);
}

void thelper_camera_api::fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(slot_ != nullptr, null_str);
	slot_->fg_aplt_send_cpp_id(window_id, cpp_id);
}

bool thelper_camera_api::is_wko_task_finished()
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(slot_ != nullptr, null_str);
	return slot_->is_wko_task_finished();
}

}

