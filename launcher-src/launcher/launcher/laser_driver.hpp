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

#ifndef LASER_DRIVER_HPP_INCLUDED
#define LASER_DRIVER_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>
#include "thread.hpp"
#include "wml_exception.hpp"

class tlaser_driver
{
public:
	tlaser_driver();

	~tlaser_driver()
	{
		VALIDATE(thread_.get() == nullptr, null_str);
		if (slot != nullptr) {
			delete slot;
			slot = nullptr;
		}
	}
	posix_noncopyable(tlaser_driver);

	void set_slot(const std::string& _aplt_id, aplt::tlaser_slot* slot);
	const std::string& aplt_id() const { return aplt_id_; }

	bool started() const { return thread_.get() != nullptr; }

	void start_laser(const std::string& serial_path, int baudrate);
	void stop_laser();

public:
	aplt::tlaser_slot* slot;

private:
	std::unique_ptr<net::tworker> thread_;
	std::string aplt_id_;
};


#endif

