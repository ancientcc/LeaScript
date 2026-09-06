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

#define GETTEXT_DOMAIN "rose-lib"

#include "scripts/vpreferences.hpp"
#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vdata.hpp"

#include "scripts/vconfig.hpp"

#include "lua/lauxlib.h"
#include "lua/lua.h"

#include "base_instance.hpp"
#include "serialization/parser.hpp"
using namespace std::placeholders;

const char vpreferences::metatableKey[] = "vpreferences";

/**
 * Destroys a vpreferences object before it is collected (__gc metamethod).
 */
static int impl_vpreferences_collect(lua_State *L)
{
	vpreferences *v = static_cast<vpreferences *>(lua_touserdata(L, 1));
	v->~vpreferences();
	return 0;
}

static int impl_vpreferences_get(lua_State *L)
{
	vpreferences *v = static_cast<vpreferences *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	if (strcmp(m, "_set_value") == 0 || strcmp(m, "_value") == 0) {
		bool ret = luaW_getmetafield(L, 1, m);
		return ret? 1: 0;
	}
	v->value(m);
	return 1;
}

static int impl_vpreferences_set(lua_State *L)
{
	vpreferences *v = static_cast<vpreferences *>(lua_touserdata(L, 1));

	return luaL_argerror(L, 1, "Not support vpreferences.__newindex, use _set_value");
/*
	v->set_value(1, 2, false);
	v->write_prefs();
	return 0;
*/
}

static int impl_vpreferences_value(lua_State *L)
{
	vpreferences *v = static_cast<vpreferences *>(luaL_checkudata(L, 1, vpreferences::metatableKey));

	// below v->value(key) will increase stack. so require calculate count first.
	int count = 0;
	while (true) {
		if (lua_type(L, 2 + count) == LUA_TNONE) {
			// no more key.
			break;
		}
		count ++;
	}

	for (int at = 0; at < count; at ++) {
		const char* key = luaL_checkstring(L, 2 + at);
		if (!v->value(key)) {
			char err_msg[256];
			SDL_snprintf(err_msg, sizeof(err_msg), "%s isn't a valid key. add it to preferences_def", key);
			return luaL_argerror(L, 2 + at, err_msg);
		}
	}
	return count;
}

static int impl_vpreferences_set_value(lua_State *L)
{
	VALIDATE_IN_MAIN_THREAD();

	vpreferences *v = static_cast<vpreferences *>(luaL_checkudata(L, 1, vpreferences::metatableKey));

	// Although it is in the main thread, locking is required to synchronize with reads from non-main threads.
	threading::lock lock(v->mutable_prefs().mutex);

	std::string notexist_key = v->set_value(nposm, 2, true);
	if (!notexist_key.empty()) {
		char err_msg[256];
		SDL_snprintf(err_msg, sizeof(err_msg), "%s isn't a valid key. add it to preferences_def", notexist_key.c_str());
		return luaL_argerror(L, 2, err_msg);
	}
	v->write_prefs();
	return 0;
}

vpreferences* luaW_pushvpreferences(lua_State* L, const std::string& lua_bundleid, trose_prefs& aplt_prefs)
{
	tstack_size_lock lock(L, 1);
	vpreferences* v = new(L) vpreferences(instance->lua(), lua_bundleid, aplt_prefs);
	if (luaL_newmetatable(L, vpreferences::metatableKey)) {
	// luaL_newmetatable(L, vpreferences::metatableKey);
		luaL_Reg metafuncs[] {
			{"__gc", impl_vpreferences_collect},
			{"__index", impl_vpreferences_get},
			{"__newindex", impl_vpreferences_set},
			{"_value", impl_vpreferences_value},
			{"_set_value", impl_vpreferences_set_value},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vpreferences::metatableKey);
	}
	lua_setmetatable(L, -2);
	return v;
}

vpreferences::vpreferences(rose_lua_kernel& lua, const std::string& lua_bundleid, trose_prefs& aplt_prefs)
	: L(lua.get_state())
	, lua_(lua)
	, lua_bundleid_(lua_bundleid)
	, aplt_prefs_(aplt_prefs)
	, prefs_file_(get_aplt_prefs_file(lua_bundleid))
{
	if (aplt_prefs_.cfg().empty()) {
		aplt_prefs_.load_prefs(prefs_file_);
	}

}

vpreferences::~vpreferences()
{
	// SDL_Log("[lua.gc]---vpreferences::~vpreferences()---");
}

void vpreferences::write_prefs()
{
	aplt_prefs_.write_prefs();
}

bool vpreferences::value(const std::string& key) const
{
	// use must make sure this key existed!
	const config& cfg = aplt_prefs_.cfg();
	if (!cfg.has_attribute(key)) {
		return false;
	}

	const config::attribute_value& v = cfg[key];

	var_type_t type = v.type();
	if (type == var_type_nposm) {
		VALIDATE(false, null_str);
		lua_pushnil(L);

	} else if (type == var_type_bool) {
		lua_pushboolean(L, v.to_bool());
	} else if (type == var_type_integer) {
		lua_pushinteger(L, v.to_int64());
	} else if (type == var_type_double) {
		lua_pushnumber(L, v.to_double());
	} else if (type == var_type_string) {
		lua_pushstring(L, v.str().c_str());
	} else if (type == var_type_tstring) {
		VALIDATE(false, null_str);
		luaW_pushtstring(L, v.t_str());
	} else {
		VALIDATE(false, null_str);
	}

	return true;
}

std::string vpreferences::set_value(int count, int first_key_stack_idx, bool must_exist)
{
	VALIDATE(count > 0 || count == nposm, null_str);
	if (count == nposm) {
		count = INT32_MAX;
	}

	config& cfg = aplt_prefs_.mutable_cfg();
	int stack_idx = first_key_stack_idx;
	const char* c_str = nullptr;
	for (int at = 0; at < count; at ++, stack_idx ++) {
		if (lua_type(L, stack_idx) == LUA_TNONE) {
			// no more key-value
			break;
		}
		const std::string key = luaL_checkstring(L, stack_idx);
		if (must_exist && !cfg.has_attribute(key)) {
			return key;
		}
		stack_idx ++;
		set_cfg_value_from_stack_idx(L, stack_idx, key, cfg);
	}

	return null_str;
}
