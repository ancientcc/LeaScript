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

#define GETTEXT_DOMAIN "aplt_nlsd_basic-lib"

#include "task_api.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>

#include "rose_qr_code.hpp"
#include "rose_sdl_utils.hpp"
#include "rose_font.hpp"

#include "common.hpp"
#include "block_task.hpp"

#include <kdl/utilities/utility.h>

using namespace std::placeholders;

namespace aplt {

//
// tnlsd_block_api
//
tnlsd_block_api::tnlsd_block_api()
	: thelper_block_api(*lua_block, *curr_aplt)
{
	task_ids_.insert(std::make_pair("turn_off", block_turn_off));
}

thelper_block_task_slot* tnlsd_block_api::app_create_block_task_slot(int task_code, bool& help_api_delete)
{
	help_api_delete = false;

	thelper_block_task_slot* slot = nullptr;
	if (task_code == block_turn_off) {
		slot = &lua_block->ext_lamp();

	} else {
		VALIDATE(false, null_str);
	}
	return slot;
}

}

void* aplt_create_task_api(void* aplt1)
{
	aplt::tnlsd_block_api* block_api = new aplt::tnlsd_block_api();

	aplt::ttask_api* leagor = new aplt::ttask_api(*aplt::curr_aplt, nullptr, nullptr, nullptr, block_api, nullptr, nullptr);
	return leagor;
}
