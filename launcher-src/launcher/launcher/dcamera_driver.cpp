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

#define GETTEXT_DOMAIN "launcher-lib"

#include "dcamera_driver.hpp"
#include "game_config.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "aplt.hpp"
#include "depthcapture.hpp"

using namespace std::placeholders;

tdcamera_driver::tdcamera_driver(tcamera& camera)
	: slot(nullptr)
	, camera_(camera)
	, desire_task_(nposm)
{}

void tdcamera_driver::set_slot(const std::string& _aplt_id, aplt::tdcamera_slot* _slot)
{
	if (slot != nullptr) {
		if (camera_.is_avcapture_started()) {
			// is using depth camera.
			stop_avcapture2();
		}
		camera_.set_depth_slot(nullptr);
		desire_task_ = nposm;

		delete slot;
		slot = nullptr;
	} else {
		VALIDATE(thread_.get() == nullptr, null_str);
	}

	if (_slot != nullptr) {
		VALIDATE(!_aplt_id.empty(), null_str);
		slot = _slot;

	} else {
		VALIDATE(_aplt_id.empty(), null_str);
	}
	aplt_id_ = _aplt_id;

	if (slot != nullptr) {
		VALIDATE(desire_task_ == nposm, null_str);
		if (camera_.is_avcapture_started()) {
			// is using non-depth camera.
			stop_avcapture2();
		}
		camera_.set_depth_slot(this);
	}
}

void tdcamera_driver::ros_start_dcamera_node()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	VALIDATE(thread_.get() == nullptr, null_str);

	if (camera_.is_avcapture_started()) {
		stop_avcapture2();
	}

	slot->ros_pre_start();
	thread_.reset(new net::tworker(std::bind(&aplt::tdcamera_slot::ros_dcamera_node, slot, _1), NULL, NULL, NULL, "dcamera_driver_node"));
}

void tdcamera_driver::ros_stop_dcamera_node()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(thread_.get() != nullptr, null_str);

	thread_.reset(nullptr);
}

bool tdcamera_driver::main_start(int task, aplt::tdcamera_slot::treceiver& receiver)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	VALIDATE(!slot->main_tasking(), null_str);

	return slot->main_start(task, receiver);
}

void tdcamera_driver::main_slice()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	slot->main_slice();
}

void tdcamera_driver::main_stop()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	slot->main_stop();
}

bool tdcamera_driver::main_tasking() const
{
	VALIDATE(slot != nullptr, null_str);
	return slot->main_tasking();
}

int tdcamera_driver::main_curr_task() const
{
	VALIDATE(slot != nullptr, null_str);
	return slot->curr_task();
}

bool tdcamera_driver::get_intrinsics(bool depth, tdcintrinsics_C& result)
{
	VALIDATE(slot != nullptr, null_str);
	return slot->get_intrinsics(depth, result);
}

void tdcamera_driver::stop_avcapture2()
{
	VALIDATE(camera_.is_avcapture_started(), null_str);
	camera_.stop_avcapture();
	VALIDATE(!slot->main_tasking(), null_str);
}

void tdcamera_driver::set_desire_depth_task(int task)
{
	VALIDATE(task >= 0 && task < dctask_count, null_str);
	VALIDATE(!camera_.tasking(), null_str);
	VALIDATE(slot != nullptr, null_str);

	int curr_task = slot->curr_task();
	if (curr_task == task) {
		return;
	}

	if (curr_task != nposm) {
		stop_avcapture2();
	}
	desire_task_ = task;
}

trtc_client* tdcamera_driver::camera_create_avcapture(int id, rtc::MessageHandler& dlg_handler, trtc_client::tadapter& adapter, const tpoint& desire_size)
{
    VALIDATE(desire_task_ >= 0 && desire_task_ < dctask_count, null_str);
	return new tdepthcapture(camera_, *this, desire_task_, id, dlg_handler, adapter, desire_size);
}

trtc_client::VideoRenderer* tdcamera_driver::camera_create_video_renderer(trtc_client& client, webrtc::VideoTrackInterface* track, const std::string& name, bool remote, int at, bool encode)
{
	return new tdepthcapture::VideoRenderer2(client, track, name, remote, at, encode);
}