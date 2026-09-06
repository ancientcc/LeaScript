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

#ifndef LIBROS_ROSE_ROS_VROS_HPP
#define LIBROS_ROSE_ROS_VROS_HPP


// #include "lua/lua.h"
#include "rose_lua.hpp"
#include <rose_ros/aplt.hpp>

namespace lua {

DECLSPEC void register_ros_metatable(lua_State *L, aplt::tr_api& r_api);

// DECLSPEC void luaW_pushtask_vars(lua_State* L, const aplt::ttask_vars& task_vars);

}

#endif

