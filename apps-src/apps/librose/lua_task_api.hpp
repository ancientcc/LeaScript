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

#ifndef LIBROSE_LUA_TASK_API_HPP
#define LIBROSE_LUA_TASK_API_HPP

#include "so_aplt_task_helper.hpp"

extern "C" void* lua_aplt_create_base_slot(const char* sn, const char* cpuid);

// extern "C" DECLSPEC void* lua_aplt_create_task_api(void* aplt1);
extern "C" void* lua_aplt_create_task_api(void* aplt1);

#endif

