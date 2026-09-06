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

#ifndef _LIBLEAGOR_BASIC_LEAGOR_NONBLOCK_TASK_HPP_
#define _LIBLEAGOR_BASIC_LEAGOR_NONBLOCK_TASK_HPP_

#include <memory>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "aplt2.hpp"
#include "so_aplt_task_helper.hpp"

namespace aplt {

class tleagor_nonblock_api: public thelper_nonblock_api
{
public:
	tleagor_nonblock_api();

private:
	thelper_nonblock_task_slot* app_create_nonblock_task_slot(int code, const tapplet::ttask& cfg_task, ttask_vars& task_vars, bool& finished) override;
};

}

#endif

