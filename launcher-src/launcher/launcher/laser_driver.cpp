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

#include "laser_driver.hpp"
#include "game_config.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "aplt.hpp"

using namespace std::placeholders;


tlaser_driver::tlaser_driver()
	: slot(nullptr)
{}

void tlaser_driver::set_slot(const std::string& _aplt_id, aplt::tlaser_slot* _slot)
{
	VALIDATE(thread_.get() == nullptr, null_str);

	if (slot != nullptr) {
		delete slot;
		slot = nullptr;
	} else {
		// VALIDATE(thread_.get() == nullptr, null_str);
	}

	if (_slot != nullptr) {
		VALIDATE(!_aplt_id.empty(), null_str);
		slot = _slot;

	} else {
		VALIDATE(_aplt_id.empty(), null_str);
	}
	aplt_id_ = _aplt_id;

	if (slot != nullptr) {
	}
}

void tlaser_driver::start_laser(const std::string& serial_path, int baudrate)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);
	VALIDATE(thread_.get() == nullptr, null_str);

	slot->pre_start_laser();
	thread_.reset(new net::tworker(std::bind(&aplt::tlaser_slot::start_laser, slot, _1, serial_path, baudrate), NULL, NULL, NULL, "laser_driver_node"));
}

void tlaser_driver::stop_laser()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(thread_.get() != nullptr, null_str);

	thread_.reset(nullptr);
}