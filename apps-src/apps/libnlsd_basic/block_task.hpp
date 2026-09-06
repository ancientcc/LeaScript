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

#ifndef NLSD_BLOCK_TASK_HPP
#define NLSD_BLOCK_TASK_HPP

#include "base_slot.hpp"
#include <memory>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "aplt2.hpp"

#include "so_aplt_task_helper.hpp"

namespace aplt {

class text_lamp_task: public thelper_block_task_slot
{
public:
	text_lamp_task(tapplet& aplt);
	~text_lamp_task() {}

	std::string start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars) override;

	tcode2 product_name() const;
	void set_button_threshold(int power, int non_power);
private:

};

class tlua_block: public thelper_lua_block
{
public:
	tlua_block();
	~tlua_block();

	text_lamp_task& ext_lamp() 
	{
		if (ext_lamp_ == nullptr) {
			ext_lamp_ = new text_lamp_task(aplt_);
		}
		return *ext_lamp_; 
	}

private:
	tapplet& aplt_;

	text_lamp_task* ext_lamp_;
};

void luaW_pushvlua_block(lua_State* L, tlua_block& widget);

}

#endif

