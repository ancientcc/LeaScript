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

#ifndef _LIBLEAGOR_LEAGOR_TASK_API_HPP_
#define _LIBLEAGOR_LEAGOR_TASK_API_HPP_

#include "base_slot.hpp"
#include <memory>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "aplt2.hpp"

namespace aplt {

class tleagor_cpp_api: public tros_cpp_api
{
public:
	tleagor_cpp_api();
	~tleagor_cpp_api();

private:
	std::string app_start_task(const ttaskpoint* taskpoint) override;
	void app_task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task, bool result) override;

	std::string start_task_moveto2_task();
	std::string start_task_speak_var();

private:
	enum {state_first, state_count};
};

class tleagor_moveit_api: public tmoveit_api
{
public:
	tleagor_moveit_api();
	~tleagor_moveit_api();

private:
	//
	// moveit aplt session
	//
	void app_start_task(tmoveit_target_info_C& result) override;

	void camera_work_frame(const surface& surf, const cv::Mat& argb, std::vector<SDL_2Point>& corners) override;
	SDL_DPoint3 calculate_TP(const cv::Mat& argb, double dcpitch, const std::vector<SDL_2Point>& corners, const SDL_DPoint3* map_xyz) override;

private:
	// aplt::tr_api& r_api_;
};

class tleagor_block_api: public tblock_api
{
public:
	tleagor_block_api();
	~tleagor_block_api() {}

private:
	void clear_output_vars(ttask_vars& task_vars, const std::vector<std::string>& output_vars);

	std::string app_start_task(ttask_vars& vars) override;

	std::string start_task_modify_env_var(const tapplet& aplt, ttask_vars& vars);
	std::string start_task_alert(const tapplet& aplt, ttask_vars& vars);
	std::string start_task_privacy(const tapplet& aplt, ttask_vars& vars);

	void did_start_task_base_scene_quited(ttask_vars& vars, std::string& err_msg, const std::string& var_name_retbool, const std::string& var_name_result_msg);
	std::string start_task_base_scene(const tapplet& aplt, ttask_vars& vars);
	std::string start_task_timed_task(const tapplet& aplt, ttask_vars& vars);

private:
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;
	aplt::tpinyin& pinyin_;
	const int tone_;
	const bool eng_lowercase_;
};

}

#endif

