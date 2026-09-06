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

#define GETTEXT_DOMAIN "aplt_nlsd_basic-lib"

#include "block_task.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>


#include "common.hpp"

#include <kdl/utilities/utility.h>

using namespace std::placeholders;

// it is insert to 'aplt_nlsd_basic__cpp' table, so allow same name.
const char vlua_blockMetatableKey[] = "cpp.vlua_block";

namespace aplt {

text_lamp_task::text_lamp_task(tapplet& aplt)
	: thelper_block_task_slot(*lua_block, aplt)
{
	std::vector<std::string>& output_var_keys = output_var_keys_;
}

std::string text_lamp_task::start_task(int code, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
{
	if (nlsd::lamp == nullptr) {
		return _("It is not current base driver.");
	}

	const std::string var_name_delay = utils::join_app_prefix_id(aplt_.bundleid, "delay");
	bool delay = false;
	if (task_vars.existed(var_name_delay)) {
		delay = task_vars.get_bool(var_name_delay);
	}

	nlsd::lamp->lamp_turn_off(delay);

	return null_str;
}

tcode2 text_lamp_task::product_name() const
{
	if (nlsd::lamp == nullptr) {
		return tcode2(nposm, _("It is not current base driver."));
	}
	return nlsd::lamp->lamp_product_name();
}

void text_lamp_task::set_button_threshold(int power, int non_power)
{
	if (nlsd::lamp == nullptr) {
		std::string err_msg = _("It is not current base driver.");
		SDL_Log("%s", err_msg.c_str());
		return;
	}
	nlsd::lamp->lamp_set_button_threshold(power, non_power);
}

tlua_block::tlua_block()
	: aplt_(*curr_aplt)
	, ext_lamp_(nullptr)
{
}

tlua_block::~tlua_block()
{
	if (ext_lamp_ != nullptr) {
		delete(ext_lamp_);
		ext_lamp_ = nullptr;
	}
}

static int impl_value_block_collect(lua_State* L)
{
	// twidget *v = *static_cast<twidget **>(lua_touserdata(L, 1));
	// v->~vwidget();
	return 0;
}

static int impl_value_block_get(lua_State* L)
{
	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);

	return ret? 1: 0;
}

static int impl_lamp_product_name(lua_State* L)
{
	tlua_block* v = *static_cast<tlua_block **>(lua_touserdata(L, 1));

	tcode2 product = v->ext_lamp().product_name();
	lua_pushstring(L, product.id.c_str());
	lua_pushinteger(L, product.code);
	return 2;
}

static int impl_lamp_set_button_threshold(lua_State* L)
{
	tlua_block* v = *static_cast<tlua_block **>(lua_touserdata(L, 1));

	int power = luaL_checkinteger(L, 2);
	int non_power = luaL_checkinteger(L, 3);

	v->ext_lamp().set_button_threshold(power, non_power);
	return 0;
}

void luaW_pushvlua_block(lua_State* L, tlua_block& widget)
{
	aplt::tb_api& b_api = aplt::get_b_api();

	tstack_size_lock lock(L, 1);
	// new(L) vwidget(L, widget);
	tlua_block** v = (tlua_block**)lua_newuserdata(L, sizeof(tlua_block*));
	*v = &widget;

	// b_api.call_lua_breakpoint();
	nil_metatable(b_api.get_lua_State(), vlua_blockMetatableKey);
	// b_api.call_lua_breakpoint();
	
	// see https://www.cswamp.com/post/261
	if (luaL_newmetatable(L, vlua_blockMetatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_value_block_collect},
			{"__index", impl_value_block_get},

			// text_lamp
			{"lamp_product_name", impl_lamp_product_name},
			{"lamp_set_button_threshold", impl_lamp_set_button_threshold},
/*
			{"query_set_position", impl_vblock2_query_set_position},

			// tcooking
			{"cooking_reload", impl_vblock2_cooking_reload},
			{"cooking_set_position", impl_vblock2_cooking_set_position},

			// tkbook
			{"kbook_reload", impl_vblock2_kbook_reload},
*/
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vlua_blockMetatableKey);

	} else {
		VALIDATE(false, null_str);
	}

	lua_setmetatable(L, -2);
}

}