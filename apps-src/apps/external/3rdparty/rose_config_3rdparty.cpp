/* $Id: game_config.cpp 46969 2010-10-08 19:45:32Z mordante $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "rose_global.hpp"
#include "rose_config_3rdparty.hpp"
// #include "rose_version.hpp"

namespace game_config
{
bool pad = true;
#ifdef _WIN32
	const bool mobile = false;
	const int os = os_windows;
	const bool os_kos = false;
#ifdef  _WIN64
	const bool b64 = true;
#else
	const bool b64 = false;
#endif
#else
	const bool mobile = true;
#if defined(__APPLE__) && TARGET_OS_IPHONE
	const int os = os_ios;
	const bool os_kos = false;
#else
	const int os = os_android;
#ifdef _KOS
	const bool os_kos = true;
#else
	const bool os_kos = false;
#endif
#endif
#ifdef __LP64__
	const bool b64 = true;
#else
	const bool b64 = false;
#endif
#endif

std::string app;
std::string app_channel;
std::string app_dir; // app-<kdesktop>
std::string app_dir_root; // game_config::path + /app-<kdesktop>
int app_code = nposm; // app_launcher, app_kdesktop, nposm

version_info rose_version("0.0.0"); // it must be reload during load_config
version_info version("1.0.31");

std::string path = "";
std::string preferences_dir = "";

int equation_of_time;

// when release, dbg_flags must be 0.
enum {DBG_FLAG_CHARGE = 0x1, DBG_FLAG_NOT_MOVE = 0x2};
// const uint32_t dbg_flags = 0;
// const uint32_t dbg_flags = DBG_FLAG_CHARGE;
const uint32_t dbg_flags = os == os_windows? DBG_FLAG_NOT_MOVE: 0;

bool is_dbg_charge()
{
    return (dbg_flags & DBG_FLAG_CHARGE)? true: false;
}

bool is_dbg_not_move()
{
	return (dbg_flags & DBG_FLAG_NOT_MOVE)? true: false;
}

} // game_config