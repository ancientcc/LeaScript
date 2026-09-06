/* $Id: dialog.cpp 50956 2011-08-30 19:41:22Z mordante $ */
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

#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "leagor_dcamera.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>

using namespace std::placeholders;

extern aplt::tdcamera_device* orbbec_create_device();

namespace aplt {

tleagor_dcamera::tleagor_dcamera()
	: device_(nullptr)
{
#ifdef _WIN32
	SDL_Log("tleagor_dcamera::tleagor_dcamera, for debug, orbbec_create_device() is nullptr");
	// device_ = orbbec_create_device();

#else
	device_ = orbbec_create_device();
#endif
}

void tleagor_dcamera::ros_pre_start()
{
	VALIDATE_IN_MAIN_THREAD();
	const config& cfg = aplt::curr_aplt->prefs.cfg();
    product_ = aplt::curr_aplt->prefs.get_str("dcamera_product");
}

void tleagor_dcamera::ros_dcamera_node(bool& exit)
{
	VALIDATE_NOT_MAIN_THREAD();
	// bool rplidar = product_ != "N10";

	device_->ros_dcamera_node(exit);
}

bool tleagor_dcamera::app_main_start(int task, treceiver& receiver)
{
	return device_->main_start(task, receiver);
}

void tleagor_dcamera::main_slice()
{
	device_->main_slice();
}

void tleagor_dcamera::app_main_stop()
{
	device_->main_stop();
}

bool tleagor_dcamera::get_intrinsics(bool depth, tdcintrinsics_C& result)
{
	return device_->get_intrinsics(depth, result);
}

}

void* aplt_create_dcamera_slot()
{
	aplt::tleagor_dcamera* leagor = new aplt::tleagor_dcamera();

	aplt::tb_api& ros = aplt::get_b_api();

	const bool support_windows = false;
	if (!support_windows && game_config::os == os_windows) {
		delete leagor;

		ros.aplt_add_msg_log(time(nullptr), _("[Leagor]because load OrbbecSDK.dll, don't support dcamera remark"), 0, false);
		return nullptr;
	}

	aplt::tdcamera_slot* result = static_cast<aplt::tdcamera_slot*>(leagor);
	return result;
}