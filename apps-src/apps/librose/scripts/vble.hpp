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

#ifndef LIBROSE_SCRIPTS_VBLE_HPP
#define LIBROSE_SCRIPTS_VBLE_HPP

#include "lua/lua.h"

#include "sdl_utils.hpp"
#include "ble.hpp"

class rose_lua_kernel;

class vble: public tble
{
public:
	static const char metatableKey[];

	vble(rose_lua_kernel& lua, const std::string& lua_bundleid);
	virtual ~vble();

	void set_clazz_id(const std::string& id);
	const std::string& clazz() const { return clazz_; }

private:
	// bool app_is_right_services() override;
	void app_discover_peripheral(SDL_BlePeripheral& peripheral) override;
	// void app_release_peripheral(SDL_BlePeripheral& peripheral) override;
	void app_connect_peripheral(SDL_BlePeripheral& peripheral, const int error) override;
	void app_disconnect_peripheral(SDL_BlePeripheral& peripheral, const int error) override;
	void app_discover_characteristics(SDL_BlePeripheral& peripheral, SDL_BleService& service, const int error) override;
	void app_read_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const unsigned char* data, int len) override;
	bool app_task_callback(ttask& task, int step_at, bool start) override;

	// void did_read_ble(int cmd, const uint8_t* data, int len);

protected:
	lua_State* L;
	rose_lua_kernel& lua_;
	const std::string lua_bundleid_;
	std::string clazz_;

	tble::tconnector connector_;
};

void luaW_pushvble(lua_State* L, const std::string& image);
void register_ble_metatable(lua_State* L);

#endif