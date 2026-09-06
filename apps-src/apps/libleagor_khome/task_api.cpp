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

#define GETTEXT_DOMAIN "aplt_leagor_khome-lib"

#include "task_api.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>

#include "common.hpp"
#include "block_task.hpp"
#include "camera_task.hpp"

#include "kface.hpp"
#include "kpose.hpp"
#include "wkocamera_task.hpp"

using namespace std::placeholders;

namespace aplt {

//
// block task
//
tkhome_block_api::tkhome_block_api()
	: thelper_block_api(*lua_block, *curr_aplt)
{
	task_ids_.insert(std::make_pair("query", block_query));
	task_ids_.insert(std::make_pair("query_cooking", block_query_cooking));
	task_ids_.insert(std::make_pair("parse_time", block_parse_time));
	task_ids_.insert(std::make_pair("kbook", block_kbook));
}

thelper_block_task_slot* tkhome_block_api::app_create_block_task_slot(int task_code, bool& help_api_delete)
{
	help_api_delete = false;

	thelper_block_task_slot* slot = nullptr;
	if (task_code == block_query) {
		slot = &lua_block->query();

	} else if (task_code == block_query_cooking) {
		slot = &lua_block->cooking();

	} else if (task_code == block_parse_time) {
		slot = &lua_block->time_parser();

	} else if (task_code == block_kbook) {
		slot = &lua_block->book();

	} else {
		VALIDATE(false, null_str);
	}
	return slot;
}

//
// camera task
//
tkhome_camera_api::tkhome_camera_api()
	: thelper_camera_api(/**lua_camera,*/ *curr_aplt)
{
	task_ids_.insert(std::make_pair("snapshot", camera_snapshot));
	task_ids_.insert(std::make_pair("kface", camera_kface));
	task_ids_.insert(std::make_pair("kpose", camera_kpose));
	task_ids_.insert(std::make_pair("workout", camera_workout));
	task_ids_.insert(std::make_pair("recognition", camera_recognition));
	VALIDATE(task_ids_.size() == (int)camera_count, null_str);
}

std::string tkhome_camera_api::app_pre_create_slot(int code)
{
	if (code == camera_kface) {
#ifndef USE_DFACE
		return _("Don't use DFACE message");
#endif
	}
	return null_str;
}

}

void* aplt_create_task_api(void* aplt1)
{
	aplt::tkhome_camera_api* camera_api = new aplt::tkhome_camera_api();
	aplt::tkhome_block_api* block_api = new aplt::tkhome_block_api();

	aplt::ttask_api* leagor = new aplt::ttask_api(*aplt::curr_aplt, nullptr, nullptr, camera_api, block_api, nullptr, nullptr);
	return leagor;
}

