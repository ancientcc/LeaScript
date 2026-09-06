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

#ifndef LIBROSE2_SO_APLT_TASK_HELPER_HPP
#define LIBROSE2_SO_APLT_TASK_HELPER_HPP

#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "aplt2.hpp"
#include "rose_lua.hpp"
#include "rose_sdl_utils.hpp"

namespace aplt {

class thelper_lua_nonblock;
class thelper_lua_block;
class tb_api;
// class tr_api;

//
// nonblock api
//
class DECLSPEC thelper_nonblock_task_slot
{
public:
	thelper_nonblock_task_slot(thelper_lua_nonblock& lua_nonblock, bool use_work_thread, 
		tapplet& aplt, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars, bool& finished);
	virtual ~thelper_nonblock_task_slot();

	virtual std::string pre_start_task() = 0;
	virtual void nonmain_start_task(bool& exit) = 0;
	virtual void task_finished(const tapplet::ttask& cfg_task) = 0;

	virtual bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) = 0;

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

	void clear_output_vars(ttask_vars& task_vars) const
	{
		for (std::vector<std::string>::const_iterator it = output_var_keys_.begin(); it != output_var_keys_.end(); ++ it) {
			const std::string& key = *it;
			if (task_vars.existed(key) != 0) {
				task_vars.erase(key);
			}
		}
	}

	bool use_work_thread() const { return use_work_thread_; }

protected:
	thelper_lua_nonblock& lua_nonblock_;
	threading::mutex variable_mutex_;
	bool use_work_thread_;
	tapplet& aplt_;
	const aplt::tapplet::ttask& cfg_task_;
	bool& finished_;
	ttask_vars& task_vars_;
	aplt::tpinyin& pinyin_;
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;

	std::vector<std::string> output_var_keys_;
};

class DECLSPEC thelper_lua_nonblock
{
public:
	thelper_lua_nonblock()
		: slot_(nullptr)
	{}

	virtual ~thelper_lua_nonblock()
	{
		VALIDATE(slot_ == nullptr, null_str);
	}

	void set_task_slot(thelper_nonblock_task_slot* slot)
	{
		if (slot != nullptr) {
			VALIDATE(slot_ == nullptr, null_str);
		} else {
			VALIDATE(slot_ != nullptr, null_str);
		}
		slot_ = slot;
	}

protected:
	const thelper_nonblock_task_slot* slot_;
};

class DECLSPEC thelper_nonblock_api: public tnonblock_api
{
public:
	thelper_nonblock_api(tapplet& aplt);
	virtual ~thelper_nonblock_api();

private:
	std::string app_start_task(ttask_vars& vars) override;
	void app_task_finished(const tapplet::ttask& cfg_task) override;
	bool slice() override;

	virtual void OnWorkStart() {}
	virtual void OnWorkDone() {}
	virtual void OnTriggerExit() {}
	virtual void DoWork(bool& exit);

	virtual thelper_nonblock_task_slot* app_create_nonblock_task_slot(int code, const tapplet::ttask& cfg_task, ttask_vars& task_vars, bool& finished) = 0;

	void clear()
	{
		VALIDATE(work_thread_.get() == nullptr, null_str);
		curr_code_ = nposm;
		slot_ = nullptr;
		finished_ = false;

	}

	void validate_nposm() const
	{
		VALIDATE(work_thread_.get() == nullptr, null_str);
		VALIDATE(curr_code_ == nposm, null_str);
		VALIDATE(slot_ == nullptr, null_str);
		VALIDATE(!finished_, null_str);
	}

protected:
	tapplet& aplt_;
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;

	std::map<std::string, int> task_ids_;
	int curr_code_;
	thelper_nonblock_task_slot* slot_;
	bool finished_;

	std::unique_ptr<trose_thread> work_thread_;
};

//
// block api
// 

class DECLSPEC thelper_block_task_slot
{
public:
	thelper_block_task_slot(thelper_lua_block& lua_block, tapplet& aplt);
	virtual ~thelper_block_task_slot();

	virtual std::string start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars) = 0;

	void clear_output_vars(ttask_vars& task_vars) const
	{
		for (std::vector<std::string>::const_iterator it = output_var_keys_.begin(); it != output_var_keys_.end(); ++ it) {
			const std::string& key = *it;
			if (task_vars.existed(key) != 0) {
				task_vars.erase(key);
			}
		}
	}

protected:
	thelper_lua_block& lua_block_;
	tapplet& aplt_;
	aplt::tpinyin& pinyin_;
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;

	std::vector<std::string> output_var_keys_;
};

class DECLSPEC thelper_lua_block
{
public:
	thelper_lua_block()
		: task_slot_(nullptr)
	{}

	virtual ~thelper_lua_block()
	{
		VALIDATE(task_slot_ == nullptr, null_str);
	}

	void set_task_slot(const thelper_block_task_slot* slot)
	{
		if (slot != nullptr) {
			VALIDATE(task_slot_ == nullptr, null_str);
		} else {
			VALIDATE(task_slot_ != nullptr, null_str);
		}
		task_slot_ = slot;
	}

	// const thelper_block_task_slot* task_slot() const { return task_slot_; }

protected:
	const thelper_block_task_slot* task_slot_;
};

class DECLSPEC thelper_block_api: public tblock_api
{
public:
	thelper_block_api(thelper_lua_block& lua_block, tapplet& aplt);
	virtual ~thelper_block_api();

private:
	std::string app_start_task(ttask_vars& vars) override;
	virtual thelper_block_task_slot* app_create_block_task_slot(int task_code, bool& help_api_delete) = 0;

	void validate_nposm() const
	{
		VALIDATE(slot_ == nullptr, null_str);
	}

protected:
	thelper_lua_block& lua_block_;
	tapplet& aplt_;
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;

	std::map<std::string, int> task_ids_;
	thelper_block_task_slot* slot_;
};

//
// aiagent api
//
class thelper_lua_aiagent;

class DECLSPEC thelper_aiagent_task_slot
{
public:
	thelper_aiagent_task_slot(thelper_lua_aiagent& lua_aiagent, bool use_work_thread, tapplet& aplt, 
		const aplt::tapplet::ttask& cfg_task, const std::string& question, const surface& surf, ttask_vars& task_vars, bool& finished);
	virtual ~thelper_aiagent_task_slot();

	virtual std::string pre_start_task() = 0;
	virtual void nonmain_start_task(bool& exit) = 0;
	virtual void task_finished(const tapplet::ttask& cfg_task) = 0;

	virtual void slice() = 0;

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

	void clear_output_vars(ttask_vars& task_vars) const
	{
		for (std::vector<std::string>::const_iterator it = output_var_keys_.begin(); it != output_var_keys_.end(); ++it) {
			const std::string& key = *it;
			if (task_vars.existed(key) != 0) {
				task_vars.erase(key);
			}
		}
	}

	bool use_work_thread() const { return use_work_thread_; }

protected:
	std::string request_aplt_task(const tapplet& aplt, const tapplet::ttask& cfg_task, const aplt::ttask_vars& task_vars,
		const std::function<void (const aplt::ttask_vars& task_vars)>& did_task_finished);

protected:
	thelper_lua_aiagent& lua_aiagent_;
	threading::mutex variable_mutex_;
	bool use_work_thread_;
	tapplet& aplt_;
	const aplt::tapplet::ttask& cfg_task_;
	const std::string question_;
	const surface surf_;
	bool& finished_;
	ttask_vars& task_vars_;
	aplt::tpinyin& pinyin_;
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;

	std::vector<std::string> output_var_keys_;
};

class DECLSPEC thelper_lua_aiagent
{
public:
	thelper_lua_aiagent()
		: slot_(nullptr)
	{
	}

	virtual ~thelper_lua_aiagent()
	{
		VALIDATE(slot_ == nullptr, null_str);
	}

	void set_task_slot(thelper_aiagent_task_slot* slot)
	{
		if (slot != nullptr) {
			VALIDATE(slot_ == nullptr, null_str);
		}
		else {
			VALIDATE(slot_ != nullptr, null_str);
		}
		slot_ = slot;
	}

protected:
	const thelper_aiagent_task_slot* slot_;
};

class DECLSPEC thelper_aiagent_api : public taiagent_api
{
public:
	thelper_aiagent_api(tapplet& aplt);
	virtual ~thelper_aiagent_api();

private:
	std::string app_start_task(const std::string& question, const surface& surf, ttask_vars& vars) override;
	void app_task_finished(const tapplet::ttask& cfg_task) override;
	bool slice() override;

	virtual void OnWorkStart() {}
	virtual void OnWorkDone() {}
	virtual void OnTriggerExit() {}
	virtual void DoWork(bool& exit);

	virtual thelper_aiagent_task_slot* app_create_aiagent_task_slot(int code, const tapplet::ttask& cfg_task, const std::string& question, const surface& surf, ttask_vars& task_vars, bool& finished) = 0;

	void clear()
	{
		VALIDATE(work_thread_.get() == nullptr, null_str);
		curr_code_ = nposm;
		slot_ = nullptr;
		finished_ = false;

	}

	void validate_nposm() const
	{
		VALIDATE(work_thread_.get() == nullptr, null_str);
		VALIDATE(curr_code_ == nposm, null_str);
		VALIDATE(slot_ == nullptr, null_str);
		VALIDATE(!finished_, null_str);
	}

protected:
	tapplet& aplt_;
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;

	std::map<std::string, int> task_ids_;
	int curr_code_;
	thelper_aiagent_task_slot* slot_;
	bool finished_;

	std::unique_ptr<trose_thread> work_thread_;
};

//
// camera api
//
class thelper_lua_camera;

class DECLSPEC thelper_camera_task_slot
{
public:
	thelper_camera_task_slot(thelper_lua_camera& lua_camera, tapplet& aplt, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars);
	virtual ~thelper_camera_task_slot();

	virtual std::string start_task() = 0;
	virtual void task_finished(const tapplet::ttask& cfg_task) = 0;

	virtual bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) = 0;
	virtual bool camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb) { return false; }
	virtual void set_wko_task_slot(twko_task_slot* slot) {}
	virtual void camera_work_frame(const surface& surf, const cv::Mat& argb) = 0;

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

	virtual bool is_wko_task_finished() { return false; }

	void clear_output_vars(ttask_vars& task_vars) const
	{
		for (std::vector<std::string>::const_iterator it = output_var_keys_.begin(); it != output_var_keys_.end(); ++ it) {
			const std::string& key = *it;
			if (task_vars.existed(key) != 0) {
				task_vars.erase(key);
			}
		}
	}

protected:
	thelper_lua_camera& lua_camera_;
	tapplet& aplt_;
	threading::mutex variable_mutex_;
	const aplt::tapplet::ttask& cfg_task_;
	ttask_vars& task_vars_;
	aplt::tpinyin& pinyin_;
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;

	std::vector<std::string> output_var_keys_;
};


class DECLSPEC thelper_lua_camera
{
public:
	thelper_lua_camera()
		: slot_(nullptr)
	{
	}

	virtual ~thelper_lua_camera()
	{
		VALIDATE(slot_ == nullptr, null_str);
	}

	void set_task_slot(thelper_camera_task_slot* slot)
	{
		if (slot != nullptr) {
			VALIDATE(slot_ == nullptr, null_str);
		}
		else {
			VALIDATE(slot_ != nullptr, null_str);
		}
		slot_ = slot;
	}

protected:
	const thelper_camera_task_slot* slot_;
};

class DECLSPEC thelper_camera_api: public tcamera_api
{
public:
	thelper_camera_api(/*thelper_lua_camera& lua_camera,*/ tapplet& aplt);
	virtual ~thelper_camera_api();

private:
	std::string app_start_task(ttask_vars& vars) override;
	void app_task_finished(const tapplet::ttask& cfg_task) override;
	bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) override;
	bool camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb) override;
	void set_wko_task_slot(twko_task_slot* slot) override;
	void camera_work_frame(const surface& surf, const cv::Mat& argb) override;

	void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) override;
	bool is_wko_task_finished() override;

	virtual std::string app_pre_create_slot(int code) { return null_str; }
	virtual thelper_camera_task_slot* app_create_camera_task_slot(int code, const tapplet::ttask& cfg_task, ttask_vars& task_vars) = 0;

	void clear()
	{
		curr_code_ = nposm;
		slot_ = nullptr;
		// finished_ = false;

	}

	void validate_nposm() const
	{
		VALIDATE(curr_code_ == nposm, null_str);
		VALIDATE(slot_ == nullptr, null_str);
		// VALIDATE(!finished_, null_str);
	}

protected:
	tapplet& aplt_;
	aplt::tb_api& b_api_;

	std::map<std::string, int> task_ids_;
	int curr_code_;
	thelper_camera_task_slot* slot_;
	// bool finished_;
};

}

#endif

