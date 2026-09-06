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
#include "game_config.hpp"
#include <SDL_filesystem.h>

#include "wml_exception.hpp"

namespace game_config
{
std::string absolute_path;
std::string apps_src_path;
}

namespace preferences {
std::string window_cfg_path()
{
	std::string path = preferences::get_str("window_cfg_path");
	if (!SDL_IsDirectory(path.c_str())) {
		path = game_config::preferences_dir + "/editor/maps";
	}
	return path;
}

void set_window_cfg_path(const std::string& value)
{
	VALIDATE(!value.empty(), null_str);
	if (window_cfg_path() != value) {
		preferences::set_str("window_cfg_path", value);
	}
}
}