/* $Id: rose_config.hpp 47641 2010-11-21 13:58:24Z mordante $ */
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
#ifndef LIBROSE_CONFIG_3RDAPRTY_H_INCLUDED
#define LIBROSE_CONFIG_3RDAPRTY_H_INCLUDED

class version_info;

#include <string>

#include <SDL_types.h>
#include "3rdparty_decl.h"
#include "rose_version.hpp"

//basic game configuration information is here.
namespace game_config {

extern LIB3RDPARTY_DECL const bool mobile;
extern LIB3RDPARTY_DECL const int os;
extern LIB3RDPARTY_DECL const bool os_kos;
extern LIB3RDPARTY_DECL const bool b64;
extern LIB3RDPARTY_DECL bool pad;
extern LIB3RDPARTY_DECL std::string app;
extern LIB3RDPARTY_DECL std::string app_channel;
extern LIB3RDPARTY_DECL std::string app_dir; // app-<kdesktop>
extern LIB3RDPARTY_DECL std::string app_dir_root; // game_config::path + /app-<kdesktop>
extern LIB3RDPARTY_DECL int app_code;

extern LIB3RDPARTY_DECL version_info rose_version;
extern LIB3RDPARTY_DECL version_info version;

extern LIB3RDPARTY_DECL std::string path;
extern LIB3RDPARTY_DECL std::string preferences_dir;

extern LIB3RDPARTY_DECL int equation_of_time;

LIB3RDPARTY_DECL bool is_dbg_charge();
LIB3RDPARTY_DECL bool is_dbg_not_move();

}

#endif
