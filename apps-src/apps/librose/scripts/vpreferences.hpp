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

#ifndef LIBROSE_SCRIPTS_PREFERENCES_HPP
#define LIBROSE_SCRIPTS_PREFERENCES_HPP

#include "config.hpp"
#include "lua/lua.h"
#include <rose_prefs.hpp>

class rose_lua_kernel;
class tfile;

class vpreferences
{
public:
	static const char metatableKey[];

	vpreferences(rose_lua_kernel& lua, const std::string& lua_bundleid, trose_prefs& aplt_prefs);
	virtual ~vpreferences();

	const trose_prefs& prefs() const { return aplt_prefs_; }
	trose_prefs& mutable_prefs() { return aplt_prefs_; }

	void write_prefs();
	bool value(const std::string& key) const;
	std::string set_value(int count, int first_key_stack_idx, bool must_exist);
	
protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	const std::string lua_bundleid_;
	trose_prefs& aplt_prefs_;
	const std::string prefs_file_;
};

vpreferences* luaW_pushvpreferences(lua_State* L, const std::string& lua_bundleid, trose_prefs& aplt_prefs);

#endif

