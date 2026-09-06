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

#ifndef LEAGOR_CAMERA_TASK_HPP
#define LEAGOR_CAMERA_TASK_HPP

#include "rose_lua.hpp"
#include <opencv2/core/mat.hpp>

#include "rose_sdl_utils.hpp"
#include "so_aplt_task_helper.hpp"

namespace aplt {


// tcamera_task_slot* create_camera_task_slot(int code, const tapplet::ttask& cfg_task, ttask_vars& task_vars);

class tlua_camera: public thelper_lua_camera
{
public:
	tlua_camera();
	~tlua_camera();
/*
	void set_task_slot(tcamera_task_slot* slot)
	{
		if (slot != nullptr) {
			VALIDATE(slot_ == nullptr, null_str);
		} else {
			VALIDATE(slot_ != nullptr, null_str);
		}
		slot_ = slot;
	}

private:
	const tcamera_task_slot* slot_;
*/
};

void faceprint_allocate(tapplet& aplt);
void faceprint_free();

int impl_vcamera2_kface_reload(lua_State* L);
int impl_vcamera2_erase_face(lua_State* L);
int impl_vcamera2_modify_face(lua_State* L);
void luaW_pushvcamera2(lua_State* L, tlua_camera& widget);

}

#endif

