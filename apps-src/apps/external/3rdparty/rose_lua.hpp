/* $Id: string_utils.hpp 56274 2013-02-10 18:59:33Z boucman $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2013 by Guillaume Melquiond <guillaume.melquiond@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE2_LUA_HPP_INCLUDED
#define LIBROSE2_LUA_HPP_INCLUDED

#include "rose_util.hpp"
#include "rose_exception.hpp"
#include "config.hpp"

#include "lua/lua.h"
#include "lua/lauxlib.h"

#define APLT_LUA_SHORT_CPP_KEY		"cpp"
#define APLT_LUA_SHORT_CPPK_KEY		"cppk"

namespace aplt {
class ttask_vars;
}

extern LIB3RDPARTY_DECL const char tstringKey[];

struct tstack_size_lock
{
	tstack_size_lock(lua_State* _L, int diff)
		: L(_L)
		, original(lua_gettop(_L))
		, diff(diff)
	{}
	~tstack_size_lock()
	{
		int s = lua_gettop(L);
		if (original + diff != s) {
			std::stringstream err;
			err << "detected static size(" << s << ") != desire stack size(" << (original + diff) << ")";
			VALIDATE(false, err.str());
		}
	}
	lua_State* L;
	const int original;
	const int diff;
};

LIB3RDPARTY_DECL void* operator new(size_t sz, lua_State *L);
LIB3RDPARTY_DECL void operator delete(void* p, lua_State *L);

#define luaW_toboolean(L, n)	(lua_toboolean(L, n) != 0)

LIB3RDPARTY_DECL bool luaW_getmetafield(lua_State *L, int idx, const char* key);
LIB3RDPARTY_DECL bool luaL_checkboolean(lua_State *L, int arg);
LIB3RDPARTY_DECL void nil_metatable(lua_State* L, const std::string& tname);

/**
 * Pushes a t_string on the top of the stack.
 */
LIB3RDPARTY_DECL void luaW_pushtstring(lua_State *L, const t_string& v);

/**
 * Converts an attribute value into a Lua object pushed at the top of the stack.
 */
LIB3RDPARTY_DECL void luaW_pushscalar(lua_State *L, const config::attribute_value& v);

/**
 * Converts the value at the top of the stack to an attribute value
 */
LIB3RDPARTY_DECL bool luaW_toscalar(lua_State *L, int index, bool allow_TUSERDATA, config::attribute_value& v);

LIB3RDPARTY_DECL void luaW_pushtask_vars(lua_State* L, const aplt::ttask_vars& task_vars);

LIB3RDPARTY_DECL void nil_applet_cpp_tables(lua_State* L, const std::string& lua_bundleid);

#endif

