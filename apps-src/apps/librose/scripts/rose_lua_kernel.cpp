/*
   Copyright (C) 2009 - 2018 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Provides a Lua interpreter, to be embedded in WML.
 *
 * @note Naming conventions:
 *   - intf_ functions are exported in the wesnoth domain,
 *   - impl_ functions are hidden inside metatables,
 *   - cfun_ functions are closures,
 *   - luaW_ functions are helpers in Lua style.
 */

#include "gettext.hpp"
#include "filesystem.hpp"
#include "rose_version.hpp"

#include "scripts/rose_lua_kernel.hpp"

#include "scripts/lua_common.hpp"

#include <cassert>                      // for assert
#include <cstring>                      // for strcmp
#include <iterator>                     // for distance, advance
#include <map>                          // for map, map<>::value_type, etc
#include <new>                          // for operator new
#include <set>                          // for set
#include <sstream>                      // for operator<<, basic_ostream, etc
#include <utility>                      // for pair
#include <algorithm>
#include <vector>                       // for vector, etc
#include "lua/lauxlib.h"                // for luaL_checkinteger, etc
#include "lua/lua.h"                    // for lua_setfield, etc

#include "serialization/string_utils.hpp"
#include "scripts/gui/dialogs/rldialog.hpp"
#include "scripts/gui/widgets/vwidget.hpp"
#include "scripts/vsurface.hpp"
#include "scripts/vpreferences.hpp"
#include "scripts/vble.hpp"


#ifdef DEBUG_LUA
#include "scripts/debug_lua.hpp"
#endif

#include "rose_prefs.hpp"

// #include "serialization/parser.hpp"
// using namespace std::placeholders;

/*
static int impl_vaplt_gettext(lua_State *L)
{
	const char* textdomain = luaL_checkstring(L, 1);
	const char* msgid = luaL_checkstring(L, 2);

	lua_getglobal(L, lua_bundleid.c_str());

	lua_pushstring(L, dsgettext(textdomain, msgid));
	return 1;
}
*/

rose_lua_kernel::rose_lua_kernel()
	: lua_kernel_base()
{
	lua_State *L = mState;

	lua_settop(L, 0);
	tstack_size_lock lock(L, 0);

	// Create the vconfig metatable.
	lua_common::register_vconfig_metatable(L);

	load_core();

	load_lua("lua/std.lua");

	gui2::register_gui2_metatable(L);
	register_ble_metatable(L);
}

void rose_lua_kernel::preprocess_window_table(lua_State *L, trapplet& aplt)
{
	tstack_size_lock lock(L, 0);

	for (std::set<std::string>::const_iterator it = aplt.window_ids.begin(); it != aplt.window_ids.end(); ++ it) {
		const std::string tbl_name = utils::join_app_prefix_id(aplt.lua_bundleid, *it);

		lua_getglobal(L, tbl_name.c_str());
		VALIDATE(lua_istable(L, -1), "must load <lua_bundleid>.lua before call preprocess_aplt_table");


		luaW_pushvsurface(L, null_str);
		lua_setfield(L, -2, "tmp_surf_");

		luaW_pushvtexture(L);
		lua_setfield(L, -2, "tmp_tex_");

		lua_pop(L, 1); // table: aplt_leagor_iaccess__terminal
	}

}

bool is_lua_type_equal_cfg_type(int lua_type, int cfg_type)
{

	if (lua_type == LUA_TBOOLEAN) {
		return cfg_type == var_type_bool;

	} else if (lua_type == LUA_TNUMBER) {
		return cfg_type == var_type_integer || cfg_type == var_type_double;

		// if (lua_isinteger(L, stack_idx)) {
		//	return var_type_integer;

		// } else {
		//	return var_type_double;
		// }

	} else if (lua_type == LUA_TSTRING) {
		return cfg_type == var_type_string || cfg_type == var_type_tstring;
	}

	// LUA_TNIL
	// LUA_TTABLE
	// ...
	return cfg_type == var_type_nposm;
}

void rose_lua_kernel::sync_lua_preferences_def(lua_State* L, trose_prefs& prefs)
{
	// tstack_size_lock lock(L, 0);

	config& cfg = prefs.mutable_cfg();

	// preferences_def
	lua_getfield(L, -1, "preferences_def");
	const int tbl_idx = lua_absindex(L, -1);
	bool changed = false;
	std::set<std::string> lua_keys;
	// add lua existed but C no.
	for (lua_pushnil(L); lua_next(L, tbl_idx); lua_pop(L, 1)) {
		// -1: value
		// -2: key
		const int type = lua_type(L, -1);
		const std::string key = luaL_checkstring(L, -2);
		if (!cfg.has_attribute(key) || !is_lua_type_equal_cfg_type(type, cfg[key].type())) {
			// vpref->set_value(1, -2, false);
			set_cfg_value_from_stack_idx(L, -1, key, cfg);
			changed = true;
		}
		lua_keys.insert(key);
	}

	// remove lua not existed but C has.
	bool preferences_dirty = true;
	while (preferences_dirty) {
		preferences_dirty = false;
		for (const config::attribute& attr: cfg.attribute_range()) {
			const std::string& key = attr.first;

			if (lua_keys.count(key) == 0) {
				cfg.remove_attribute(key);
				preferences_dirty = true;

				changed = true;
			}
		}
	}

	if (changed) {
		prefs.write_prefs();
	}

	lua_pop(L, 1); // table: preferences_def
}

void rose_lua_kernel::load_aplt_prefs(aplt::tapplet& aplt)
{
	VALIDATE(aplt.prefs.cfg().empty(), null_str);

	std::string lua_bundleid = bundleid_2_lua_bundleid(aplt.bundleid);

	create_directory_if_missing(get_aplt_user_data_dir(lua_bundleid));
	std::string prefs_file = (get_aplt_prefs_file(lua_bundleid));

	trose_prefs& prefs = aplt.prefs;
	// 1/2: read from file: preferences
	prefs.load_prefs(prefs_file);

	// 2/2: read preferences_def form main.lua
	{
		lua_State* L = mState;
		tstack_size_lock lock(L, 0);

		config aplt_cfg;
		config& sub_cfg = aplt_cfg.add_child("binary_path");
		sub_cfg["path"] = binary_paths_manager::full_path_indicator + aplt.res_path;
		const binary_paths_manager bin_paths_manager(aplt_cfg);

		//
		// load main.lua
		// 
		// call_lua_breakpoint();

		// (1/4)create and load table: _ENV.aplt_leagor_basic
		load_lua("lua/main.lua");
	
		// call_lua_breakpoint();

		// (2/4)push _ENV.aplt_leagor_basic.preferences_def into C stack
		lua_getglobal(L, lua_bundleid.c_str());
		VALIDATE(lua_istable(L, -1), "must load <lua_bundleid>.lua before call sync_lua_preferences_def");
		// call_lua_breakpoint();

		sync_lua_preferences_def(L, prefs);

		// (3/4)pop _ENV.aplt_leagor_basic.preferences_def from C stack
		lua_pop(L, 1);
		// call_lua_breakpoint();

		// (4/4)delete table: _ENV.aplt.leagor.basic
		lua_pushnil(L);
		lua_setglobal(L, lua_bundleid.c_str());
		// call_lua_breakpoint();

		// Enforce a complete garbage collection
		lua_gc(L, LUA_GCCOLLECT);

		// call_lua_breakpoint();
	}
}

void rose_lua_kernel::preprocess_env_table(lua_State *L, bool caller_is_fg_aplt, aplt::tapplet& aplt, const std::string& lua_bundleid, const version_info& version, std::vector<std::string>& files)
{
	tstack_size_lock lock(L, 0);

	lua_getglobal(L, lua_bundleid.c_str());
	VALIDATE(lua_istable(L, -1), "must load <lua_bundleid>.lua before call preprocess_timing_table");

	// Although I'd love to have lua call member function of a class directly, unfortunately, no method was found.
	// aplt --> current table, aplt_leagor_blesmart

	lua_pushstring(L, lua_bundleid.c_str());
	lua_setfield(L, -2, "TAG");

	lua_pushstring(L, (lua_bundleid + "-lib").c_str());
	lua_setfield(L, -2, "GETTEXT_DOMAIN");

	lua_pushstring(L, utils::replace_all(lua_bundleid, "_", ".").c_str());
	lua_setfield(L, -2, "bundleid");

	lua_pushstring(L, aplt.id.c_str());
	lua_setfield(L, -2, "id");

	lua_pushstring(L, version.str(true).c_str());
	lua_setfield(L, -2, "version");

	lua_pushstring(L, version.str(false).c_str());
	lua_setfield(L, -2, "version3");

	lua_pushstring(L, aplt.res_path.c_str());
	lua_setfield(L, -2, "res_path");

	lua_pushstring(L, aplt.preferences_dir.c_str());
	lua_setfield(L, -2, "preferences_dir");

	vpreferences* vpref = luaW_pushvpreferences(L, lua_bundleid, aplt.prefs);
	lua_setfield(L, -2, "preferences");

	// call_lua_breakpoint();
	sync_lua_preferences_def(L, vpref->mutable_prefs());
	// call_lua_breakpoint();
	 
	// files
	// rtiming.files.clear();
	lua_getfield(L, -1, caller_is_fg_aplt? "files": "bg_task_files");

	std::set<std::string> xincludes;
	xincludes.insert("main.lua");
	if (!caller_is_fg_aplt) {
		xincludes.insert("bg_task.lua");
	}

	int tbl_idx = -1;
	for (int i = 1, i_end = lua_rawlen(L, tbl_idx); i <= i_end; ++i) {
		lua_rawgeti(L, tbl_idx, i);
		const char* file = luaL_checkstring(L, -1);
		VALIDATE(file[0] != '\0', null_str);
		VALIDATE(xincludes.count(file) == 0, null_str);

		files.push_back(file);

		// Attempt to call load_lua("lua/file.lua") here, resulting in an exception exit
		// load_lua("lua/" + file);

		lua_pop(L, 1); // 1file
	}
	lua_pop(L, 1); // table: files

	// user_data_dirs
	const std::string aplt_user_data_dir = get_aplt_user_data_dir(lua_bundleid);
	std::set<std::string> user_data_dirs;
	lua_getfield(L, -1, "user_data_dirs");

	std::set<std::string> allow_subdirs = {"saves"};
	tbl_idx = -1;
	for (int i = 1, i_end = lua_rawlen(L, tbl_idx); i <= i_end; ++i) {
		lua_rawgeti(L, tbl_idx, i);
		const std::string dir = luaL_checkstring(L, -1);
		lua_pop(L, 1); // 1dir

		VALIDATE(!dir.empty(), null_str);
		VALIDATE(user_data_dirs.count(dir) == 0, null_str);
		size_t pos = dir.find("/");
		VALIDATE(pos != std::string::npos && allow_subdirs.count(dir.substr(0, pos)), null_str);

		user_data_dirs.insert(dir);
		create_directory_if_missing(aplt_user_data_dir + "/" + dir);
	}
	lua_pop(L, 1); // table: user_data_dirs

	lua_pop(L, 1); // table: lua_bundleid[aplt.leagor.iaccess]
}

void rose_lua_kernel::register_applet(const aplt::tapplet& aplt, const std::string& lua_bundleid, const version_info& version, const config& cfg)
{
	VALIDATE(raplts_.count(lua_bundleid) == 0, null_str);
	std::pair<std::map<std::string, trapplet>::iterator, bool> ins = raplts_.insert(std::make_pair(lua_bundleid, trapplet(lua_bundleid)));
	trapplet& raplt = ins.first->second;
	aplt::tapplet* mutable_aplt = const_cast<aplt::tapplet*>(&aplt);

	lua_State *L = mState;

	// load main.lua
	load_lua("lua/main.lua");

	// 1. fill TAG/GETTEXT_DOMAIN/version/version3 etc
	// 2. get files
	raplt.files.clear();
	preprocess_env_table(L, true, *mutable_aplt, raplt.lua_bundleid, version, raplt.files);

	// load other *.lua
	std::set<std::string> files_set;
	for (std::vector<std::string>::const_iterator it = raplt.files.begin(); it != raplt.files.end(); ++ it) {
		const std::string& file = *it;
		load_lua("lua/" + file);
		files_set.insert(utils::file_stem_name(file));
	}

	BOOST_FOREACH (const config &w, cfg.child_range("window")) {
		const std::string& id = w["id"].str();
		VALIDATE(files_set.count(id) != 0, null_str);
		raplt.window_ids.insert(id);
	}
	preprocess_window_table(L, raplt);
}

void rose_lua_kernel::nil_applet_tables(lua_State* L, const std::string& lua_bundleid, const std::vector<std::string>& files)
{
	// delete this applet's table
	tstack_size_lock lock(L, 0);
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string tbl_name = utils::join_app_prefix_id(lua_bundleid, utils::file_stem_name(utils::extract_file(*it)));

		lua_pushnil(L);

		lua_setglobal(L, tbl_name.c_str());
	}

	lua_pushnil(L);
	lua_setglobal(L, lua_bundleid.c_str());

	// Enforce a complete garbage collection
	// lua_gc(L, LUA_GCCOLLECT);
}

void rose_lua_kernel::unregister_applet(const std::string& lua_bundleid)
{
	std::map<std::string, trapplet>::iterator it = raplts_.find(lua_bundleid);
	VALIDATE(it != raplts_.end(), null_str);
	const trapplet& aplt = it->second;

	lua_State *L = mState;

	nil_applet_tables(L, lua_bundleid, aplt.files);

	raplts_.erase(it);
}

void rose_lua_kernel::register_timing(const aplt::tapplet& aplt, const std::string& lua_bundleid, const version_info& version, const config& cfg)
{
	VALIDATE(rtiming_.aplt == nullptr, null_str);
	VALIDATE(rtiming_.lua_bundleid.empty(), null_str);
	VALIDATE(rtiming_.files.empty(), null_str);

	rtiming_.aplt = const_cast<aplt::tapplet*>(&aplt);
	rtiming_.lua_bundleid = lua_bundleid;

	lua_State *L = mState;

	// load main.lua
	load_lua("lua/main.lua");

	// 1. fill TAG/GETTEXT_DOMAIN/version/version3 etc
	// 2. get files
	rtiming_.files.push_back("bg_task.lua");
	preprocess_env_table(L, false, *rtiming_.aplt, rtiming_.lua_bundleid, version, rtiming_.files);

	// load other *.lua
	std::set<std::string> files_set;
	for (std::vector<std::string>::const_iterator it = rtiming_.files.begin(); it != rtiming_.files.end(); ++ it) {
		const std::string& file = *it;
		load_lua("lua/" + file);
		files_set.insert(utils::file_stem_name(file));
	}

/*
	BOOST_FOREACH (const config &w, cfg.child_range("window")) {
		const std::string& id = w["id"].str();
		VALIDATE(files_set.count(id) != 0, null_str);
		aplt.window_ids.insert(id);
	}
	preprocess_window_table(L, aplt);
*/
}

void rose_lua_kernel::unregister_timing(const std::string& lua_bundleid)
{
	VALIDATE(rtiming_.lua_bundleid == lua_bundleid, null_str);

	lua_State* L = mState;

	nil_applet_tables(L, lua_bundleid, rtiming_.files);

	rtiming_.clear();
}

void rose_lua_kernel::post_show(const std::string& lua_bundleid, const std::string& window_id)
{
	std::map<std::string, trapplet>::const_iterator it = raplts_.find(lua_bundleid);
	VALIDATE(it != raplts_.end(), null_str);
	const trapplet& aplt = it->second;

	lua_State *L = mState;
	tstack_size_lock lock(L, 0);

	const std::string tbl_name = utils::join_app_prefix_id(aplt.lua_bundleid, window_id);
	lua_getglobal(L, tbl_name.c_str());
	VALIDATE(lua_istable(L, -1), "must load <lua_bundleid>.lua before call preprocess_aplt_table");

	// tmp_surf_.reset(nullptr);
	lua_getfield(L, -1, "tmp_surf_");
	vsurface* vsurf = static_cast<vsurface *>(lua_touserdata(L, -1));
	vsurf->reset(nullptr);
	lua_pop(L, 1);

	// tmp_tex_.reset(nullptr);
	lua_getfield(L, -1, "tmp_tex_");
	vtexture* vtex = static_cast<vtexture *>(lua_touserdata(L, -1));
	vtex->reset(nullptr);
	lua_pop(L, 1);

	lua_pop(L, 1); // table: aplt_leagor_iaccess__terminal
}