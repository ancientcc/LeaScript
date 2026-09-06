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

#ifndef LIBROSE_LEAGOR_DCAMERA_HPP_INCLUDED
#define LIBROSE_LEAGOR_DCAMERA_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "aplt2.hpp"
#include "dcamera_device.hpp"

namespace aplt {

class tleagor_dcamera: public tdcamera_slot
{
public:
	tleagor_dcamera();

	~tleagor_dcamera()
	{
		if (device_ != nullptr) {
			delete device_;
			device_ = nullptr;
		}
	}

private:
	void ros_pre_start() override;
	void ros_dcamera_node(bool& exit) override;

	bool app_main_start(int task, treceiver& receiver) override;
	void main_slice() override;
	void app_main_stop() override;

	bool get_intrinsics(bool depth, tdcintrinsics_C& result) override;

private:
	tdcamera_device* device_;
	std::string product_;
};

}

#endif

