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

#ifndef DCAMERA_DRIVER_HPP_INCLUDED
#define DCAMERA_DRIVER_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>
#include "thread.hpp"
#include "wml_exception.hpp"
#include "camera.hpp"

class tdcamera_driver: public tcamera::tdepth_slot
{
public:
	tdcamera_driver(tcamera& camera);

	~tdcamera_driver()
	{
		VALIDATE(thread_.get() == nullptr, null_str);
		if (slot != nullptr) {
			delete slot;
			slot = nullptr;
		}
	}
	posix_noncopyable(tdcamera_driver);

	void set_slot(const std::string& _aplt_id, aplt::tdcamera_slot* slot);
	const std::string& aplt_id() const { return aplt_id_; }
	bool installed() const { return slot != nullptr; }

	bool started() const { return thread_.get() != nullptr; }

	void ros_start_dcamera_node();
	void ros_stop_dcamera_node();

	bool main_start(int task, aplt::tdcamera_slot::treceiver& receiver);
	void main_slice();
	void main_stop();
	bool main_tasking() const;
	int main_curr_task() const;
	bool get_intrinsics(bool depth, tdcintrinsics_C& result);

	void set_desire_depth_task(int task);

private:
	void stop_avcapture2();
	//
	// tcamera::tdepth_slot
	//
	trtc_client* camera_create_avcapture(int id, rtc::MessageHandler& dlg_handler, trtc_client::tadapter& adapter, const tpoint& desire_size) override;
	trtc_client::VideoRenderer* camera_create_video_renderer(trtc_client& client, webrtc::VideoTrackInterface* track, const std::string& name, bool remote, int at, bool encode) override;

public:
	aplt::tdcamera_slot* slot;

private:
	tcamera& camera_;
	std::unique_ptr<net::tworker> thread_;
	std::string aplt_id_;

	int desire_task_;
};


#endif

