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

#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "leagor_nonblock.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include "nonblock_task.hpp"
#include "common.hpp"

using namespace std::placeholders;

namespace aplt {

//
// charge task
//
tleagor_nonblock_api::tleagor_nonblock_api()
	: thelper_nonblock_api(*curr_aplt)
{
	task_ids_.insert(std::make_pair("deepseek", nonblock_deepseek));
}

thelper_nonblock_task_slot* tleagor_nonblock_api::app_create_nonblock_task_slot(int code, const tapplet::ttask& cfg_task, ttask_vars& task_vars, bool& finished)
{
	return create_nonblock_task_slot(code, aplt_, cfg_task, task_vars, finished);
}

}
