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

#ifndef LIBROSE_DCAMERA_DEVICE_HPP_INCLUDED
#define LIBROSE_DCAMERA_DEVICE_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "aplt2.hpp"

namespace aplt {

class tdcamera_device
{
public:
	tdcamera_device() {}
	virtual ~tdcamera_device() {}

	virtual void ros_pre_start() {}
	virtual void ros_dcamera_node(bool& exit) = 0;

	virtual bool main_start(int task, aplt::tdcamera_slot::treceiver& receiver) = 0;
	virtual void main_slice() = 0;
	virtual void main_stop() = 0;

	virtual bool get_intrinsics(bool depth, tdcintrinsics_C& result) = 0;
};

}

#endif

