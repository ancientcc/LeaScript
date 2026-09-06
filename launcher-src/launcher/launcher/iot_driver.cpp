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

#include "iot_driver.hpp"
#include "game_config.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "aplt.hpp"

#include <openssl/sha.h>
// #include <net/url_request/url_request_http_job_rose.hpp>

#include "rose_net_api.hpp"

using namespace std::placeholders;

namespace rtc {
void worker_thread::DoWork() { worker_.DoWork(); }
void worker_thread::OnWorkStart() { worker_.OnWorkStart(); }
void worker_thread::OnWorkDone() { worker_.OnWorkDone(); }
}


tiot_driver::tiot_driver(aplt::tslot_subscriber& subscriber)
	: slot(nullptr)
{
}

void tiot_driver::set_slot(const std::string& _aplt_id, aplt::tiot_slot* _slot)
{
	if (slot != nullptr) {
		if (started()) {
			stop_iot();
		}
		delete slot;
		slot = nullptr;
	}
	VALIDATE(thread_.get() == nullptr, null_str);

	if (_slot != nullptr) {
		VALIDATE(!_aplt_id.empty(), null_str);
		slot = _slot;

	} else {
		VALIDATE(_aplt_id.empty(), null_str);
	}
	aplt_id_ = _aplt_id;

	if (slot != nullptr) {
		start_iot();
	}
}

void tiot_driver::start_iot()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	VALIDATE(thread_.get() == nullptr, null_str);

	slot->pre_start_iot();
	thread_.reset(new net::tworker(std::bind(&aplt::tiot_slot::start_iot, slot, _1), NULL, NULL, NULL, "iot_driver_node"));
}

void tiot_driver::stop_iot()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	VALIDATE(thread_.get() != nullptr, null_str);

	thread_.reset(nullptr);
	slot->post_stop_iot();
}

void tiot_driver::slice()
{
	if (slot != nullptr) {
		slot->slice();
	}
}
