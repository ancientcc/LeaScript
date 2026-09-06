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

#pragma once

#include "scripts/lua_kernel_base.hpp" // for lua_kernel_base

// #include <map>
#include <stack>
#include <string>                       // for string

class config;
class version_info;

typedef int (*lua_CFunction) (lua_State *L);

class rose_lua_kernel: public lua_kernel_base
{
public:
	rose_lua_kernel();

	void load_aplt_prefs(aplt::tapplet& aplt);

	void register_applet(const aplt::tapplet& aplt, const std::string& lua_bundleid, const version_info& version, const config& cfg);
	void unregister_applet(const std::string& lua_bundleid);

	void register_timing(const aplt::tapplet& aplt, const std::string& lua_bundleid, const version_info& version, const config& cfg);
	void unregister_timing(const std::string& lua_bundleid);
	void post_show(const std::string& lua_bundleid, const std::string& window_id);

private:
	void sync_lua_preferences_def(lua_State* L, trose_prefs& prefs);
	void preprocess_env_table(lua_State *L, bool caller_is_fg_aplt, aplt::tapplet& aplt, const std::string& lua_bundleid, const version_info& version, std::vector<std::string>& files);
	void nil_applet_tables(lua_State *L, const std::string& lua_bundleid, const std::vector<std::string>& files);

	struct trapplet // Running applet
	{
		trapplet(const std::string& lua_bundleid)
			: lua_bundleid(lua_bundleid)
		{}

		const std::string lua_bundleid;
		std::vector<std::string> files;
		std::set<std::string> window_ids;
	};
	void preprocess_window_table(lua_State *L, trapplet& aplt);

	struct trtiming // Running timing
	{
		trtiming()
			: aplt(nullptr)
			, lua_bundleid()
		{}

		void clear()
		{
			aplt = nullptr;
			lua_bundleid.clear();
			files.clear();
		}

		aplt::tapplet* aplt;
		std::string lua_bundleid;
		std::vector<std::string> files;
	};

private:
	std::map<std::string, trapplet> raplts_;
	trtiming rtiming_;
};
