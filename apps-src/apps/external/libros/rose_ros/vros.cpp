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


#include "rose_ros/vros.hpp"
#include <rose_ros/utils.hpp>

#include <lua/lauxlib.h>
#include <lua/lua.h>


// #include "scripts/rose_lua_kernel.hpp"
// #include "scripts/lua_common.hpp"
// #include "scripts/vdata.hpp"
// #include "scripts/vconfig.hpp"

using namespace std::placeholders;



static aplt::tr_api* singleton = nullptr;
static const char rosKey[] = "ros";

static int impl_curmap_has_position(lua_State* L)
{
	const std::string guid = luaL_checkstring(L, 1);

	const tros_map& curmap = singleton->curmap();
	bool ret = curmap.positions.count(guid) != 0;

	// return luaL_argerror(L, 1, "err_msg");

	lua_pushboolean(L, ret);
	return 1;
}

// @at: require returned table of map_position, at is C-index in table.
static void push_map_position(lua_State* L, const tmap_position& position)
{
	lua_createtable(L, 5, 0);

	lua_pushstring(L, position.uuid.c_str());
	lua_setfield(L, -2, "uuid");
	lua_pushstring(L, position.name.c_str());
	lua_setfield(L, -2, "name");

	lua_pushnumber(L, position.x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, position.y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, position.theta);
	lua_setfield(L, -2, "theta");
}

static int impl_curmap_get_position(lua_State* L)
{
	const std::string guid = luaL_checkstring(L, 1);

	const tros_map& curmap = singleton->curmap();
	bool existed = curmap.positions.count(guid) != 0;
	if (existed) {
		const tmap_position& position = curmap.positions.find(guid)->second;

		tstack_size_lock lock(L, 1);
		push_map_position(L, position);

	} else {
		lua_pushnil(L);
	}

	return 1;
}

static int impl_curmap_get_positions(lua_State* L)
{
	const tros_map& curmap = singleton->curmap();

	tstack_size_lock lock(L, 1);

	lua_createtable(L, curmap.positions.size(), 0);

	int at = 0;
	for (std::map<std::string, tmap_position>::const_iterator it = curmap.positions.begin(); it != curmap.positions.end(); ++ it, at ++) {
		const tmap_position& position = it->second;

		push_map_position(L, position);
		lua_rawseti(L, -2, at + 1);
/*
		lua_createtable(L, 5, 0);

		lua_pushstring(L, position.uuid.c_str());
		lua_setfield(L, -2, "uuid");
		lua_pushstring(L, position.name.c_str());
		lua_setfield(L, -2, "name");

		lua_pushnumber(L, position.x);
		lua_setfield(L, -2, "x");
		lua_pushnumber(L, position.y);
		lua_setfield(L, -2, "y");
		lua_pushnumber(L, position.theta);
		lua_setfield(L, -2, "theta");
		
		lua_rawseti(L, -2, at + 1);
*/
	}

	return 1;
}

static int impl_iot_sources(lua_State* L)
{
	tstack_size_lock lock(L, 1);

	lua_createtable(L, aplt::iot_src_sensor_max - aplt::iot_src_sensor_min + 1, 0);

	int at = 0;
	for (std::map<int, aplt::tiot_src2>::const_iterator it = aplt::iot_sources.begin(); it != aplt::iot_sources.end(); ++ it) {
		const aplt::tiot_src2& src2 = it->second;
		if (src2.code < aplt::iot_src_sensor_min || src2.code > aplt::iot_src_sensor_max) {
			continue;
		}
		const std::string& name = it->second.name;

		// push_map_position(L, position);
		// lua_rawseti(L, -2, at + 1);

		lua_createtable(L, 2, 0);

		lua_pushinteger(L, src2.code);
		lua_setfield(L, -2, "code");

		lua_pushstring(L, src2.name.c_str());
		lua_setfield(L, -2, "name");
		
		lua_rawseti(L, -2, at + 1);

		at ++;
	}

	return 1;
}

namespace lua {

void register_ros_metatable(lua_State* L, aplt::tr_api& r_api)
{
	VALIDATE(singleton == nullptr, null_str);
	singleton = &r_api;

	tstack_size_lock lock(L, 0);

	lua_getglobal(L, rosKey);
	if (!lua_istable(L, -1)) {
		// if rosKey table isn't existed, create it.
		lua_pop(L, 1);
		lua_newtable(L);
		lua_setglobal(L, rosKey);

		lua_getglobal(L, rosKey);
	}

	static luaL_Reg const callbacks[] {
		{ "curmap_has_position", 	&impl_curmap_has_position},
		{ "curmap_get_position", 	&impl_curmap_get_position},
		{ "curmap_get_positions", 	&impl_curmap_get_positions},

		{ "iot_sources", 			&impl_iot_sources},
		{ nullptr, nullptr }
	};
	luaL_setfuncs(L, callbacks, 0);

	// lua_pushstring(L, rosKey);
	// lua_setfield(L, -2, "__metatable");

	lua_pop(L, 1);
}
/*
void luaW_pushtask_vars(lua_State* L, const aplt::ttask_vars& task_vars)
{
	tstack_size_lock lock(L, 1);

	lua_newtable(L);

	const std::map<std::string, aplt::ttask_var>& data = task_vars.data();
	for (std::map<std::string, aplt::ttask_var>::const_iterator it = data.begin(); it != data.end(); ++ it) {
		const aplt::ttask_var& var = it->second;
		VALIDATE(!var.is_array, null_str);

		luaW_pushscalar(L, var.val);
		lua_setfield(L, -2, var.name.c_str());
	}
}
*/
}