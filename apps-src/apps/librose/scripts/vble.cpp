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

#define GETTEXT_DOMAIN "rose-lib"

#include "scripts/vble.hpp"
#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"

#include "scripts/vconfig.hpp"
#include "scripts/vdata.hpp"

#include "lua/lauxlib.h"
#include "lua/lua.h"

#include "base_instance.hpp"
#include "ble.hpp"


const char vble::metatableKey[] = "vble";

/**
 * Destroys a vsurface object before it is collected (__gc metamethod).
 */
static int impl_vble_collect(lua_State *L)
{
	vble *v = static_cast<vble *>(luaL_checkudata(L, 1, vble::metatableKey));
	v->~vble();
	return 0;
}

static int impl_vble_get(lua_State *L)
{
	vble *v = static_cast<vble *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vble_start_scan(lua_State *L)
{
	vble *v = static_cast<vble *>(lua_touserdata(L, 1));

	const char* clazz_id = luaL_checkstring(L, 2);
	v->set_clazz_id(clazz_id);

	v->start_scan();
	return 0;
}

static int impl_vble_stop_scan(lua_State *L)
{
	vble *v = static_cast<vble *>(lua_touserdata(L, 1));
	v->stop_scan();
	return 0;
}

static int impl_vble_connect_peripheral(lua_State *L)
{
	vble *v = static_cast<vble *>(lua_touserdata(L, 1));

	const char* clazz_id = luaL_checkstring(L, 2);
	v->set_clazz_id(clazz_id);

	lua_Integer p2 = luaL_checkinteger(L, 3);
	SDL_BlePeripheral* peripheral = reinterpret_cast<SDL_BlePeripheral*>(p2);
	v->connect_peripheral(*peripheral);

	return 0;
}

static int impl_vble_disconnect_peripheral(lua_State *L)
{
	vble *v = static_cast<vble *>(lua_touserdata(L, 1));
	v->disconnect_peripheral();
	return 0;
}

static int impl_vble_insert_task(lua_State *L)
{
	vble *v = static_cast<vble *>(lua_touserdata(L, 1));

	int id = luaL_checkinteger(L, 2);

	tble::ttask& task = v->insert_task(id);
	{
		tstack_size_lock lock(L, 0);
		int tasks_index = 3;
		luaL_checktype(L, tasks_index, LUA_TTABLE);
		for (int i = 1, i_end = lua_rawlen(L, tasks_index); i <= i_end; ++i) {
			lua_rawgeti(L, tasks_index, i);
			VALIDATE(lua_istable(L, -1), null_str);

			lua_rawgeti(L, -1, 1);
			const char* service_uuid = luaL_checkstring(L, -1);
			VALIDATE(service_uuid[0] != '\0', null_str);

			// field#1 push to stack, so table's index changes to -2.
			lua_rawgeti(L, -2, 2);
			const char* chara_uuid = luaL_checkstring(L, -1);
			VALIDATE(chara_uuid[0] != '\0', null_str);

			// field#1 and field#2 push to stack, so table's index changes to -3.
			lua_rawgeti(L, -3, 3);
			// icon maybe nil.
			int op = luaL_checkinteger(L, -1);
				
			task.insert(nposm, service_uuid, chara_uuid, op);
			lua_pop(L, 4); // 1table + 3fields
		}
	}
	return 0;
}

static int impl_vble_execute_task(lua_State *L)
{
	vble *v = static_cast<vble *>(lua_touserdata(L, 1));

	int id = luaL_checkinteger(L, 2);
	tble::ttask& task = v->get_task(id);
	task.execute(*v);
	return 0;
}

static int impl_vble_set_task_step_data(lua_State* L)
{
	vble *v = static_cast<vble *>(lua_touserdata(L, 1));

	int taskid = luaL_checkinteger(L, 2);
	tble::ttask& task = v->get_task(taskid);

	int step_at = luaL_checkinteger(L, 3);

	vdata* data = static_cast<vdata *>(lua_touserdata(L, 4));
	int offset = luaL_checkinteger(L, 5);
	int len = luaL_optinteger(L, 6, nposm);

	len = data->validate_and_ajdust(offset, len);
	tble::tstep& step = task.get_step(step_at);
	step.set_data(data->data_ptr() + offset, len);
	return 0;
}

static int impl_vble_task_running(lua_State* L)
{
	vble *v = static_cast<vble *>(lua_touserdata(L, 1));

	lua_pushboolean(L, v->task_running());
	return 1;
}

void luaW_pushvble(lua_State* L, const std::string& clazz)
{
	tstack_size_lock lock(L, 1);
	new(L) vble(instance->lua(), clazz);
	int type = luaL_getmetatable(L, vble::metatableKey);
	VALIDATE(type == LUA_TTABLE, null_str);

	lua_setmetatable(L, -2);
}


void register_ble_metatable(lua_State *L)
{
	tstack_size_lock lock(L, 0);

	// 2)[registry]
	int created = luaL_newmetatable(L, vble::metatableKey);
	VALIDATE(created != 0, null_str);
	luaL_Reg metafuncs[] {
		{"__gc", impl_vble_collect},
		{"__index", impl_vble_get},
		{"start_scan", impl_vble_start_scan},
		{"stop_scan", impl_vble_stop_scan},
		{"connect_peripheral", impl_vble_connect_peripheral},
		{"disconnect_peripheral", impl_vble_disconnect_peripheral},
		{"insert_task", impl_vble_insert_task},
		{"execute_task", impl_vble_execute_task},
		{"set_task_step_data", impl_vble_set_task_step_data},
		{"task_running", impl_vble_task_running},
		{nullptr, nullptr},
	};
	luaL_setfuncs(L, metafuncs, 0);
	lua_pushstring(L, "__metatable");
	lua_setfield(L, -2, vble::metatableKey);

	lua_pop(L, 1);
}

vble::vble(rose_lua_kernel& lua, const std::string& lua_bundleid)
	: tble(instance, connector_, false)
	, L(lua.get_state())
	, lua_(lua)
	, lua_bundleid_(lua_bundleid)
{
}

vble::~vble()
{
	// SDL_Log("[lua.gc]---vsurface::~vsurface()---");
}

void vble::set_clazz_id(const std::string& id)
{
	VALIDATE(!id.empty(), null_str);
	clazz_ = utils::join_app_prefix_id(lua_bundleid_, id); 
}

static void push_peripheral(lua_State* L, const SDL_BlePeripheral& peripheral, bool push_services)
{
	tstack_size_lock lock(L, 1);

	// int64_t id = (int64_t)&peripheral;
	// SDL_Log("push_peripheral, peripheral: %p, id: %lld", &peripheral, id);
	// make sure these are not nil: name, manufacturer_data, uuid

	lua_newtable(L);
	lua_pushinteger(L, (lua_Integer)&peripheral);
	lua_setfield(L, -2, "id");

	lua_pushstring(L, peripheral.name != nullptr? peripheral.name: "");
	lua_setfield(L, -2, "name");

	lua_pushlstring(L, (const char*)peripheral.manufacturer_data, peripheral.manufacturer_data_len);
	lua_setfield(L, -2, "manufacturer_data");

	lua_pushinteger(L, peripheral.manufacturer_data_len);
	lua_setfield(L, -2, "manufacturer_data_len");

	lua_pushstring(L, peripheral.uuid != nullptr? peripheral.uuid: "");
	lua_setfield(L, -2, "uuid");

	lua_pushinteger(L, peripheral.device_type);
	lua_setfield(L, -2, "device_type");

	lua_pushinteger(L, peripheral.rssi);
	lua_setfield(L, -2, "rssi");

	int MFRID = 0;
	if (peripheral.manufacturer_data_len >= 2) {
		MFRID = posix_mku16(peripheral.manufacturer_data[0], peripheral.manufacturer_data[1]);
	}
	lua_pushinteger(L, MFRID);
	lua_setfield(L, -2, "MFRID");

	if (push_services) {
		lua_pushinteger(L, peripheral.valid_services);
		lua_setfield(L, -2, "valid_services");

		const SDL_BleService* services = peripheral.services;
		lua_createtable(L, peripheral.valid_services, 0);

		for (int at = 0; at < peripheral.valid_services; at ++) {
			const SDL_BleService& service = services[at];
			lua_createtable(L, 3, 0); // 0: uuid, 1: valid_characteristics, 2: characteristics
			lua_pushstring(L, service.uuid);
			lua_rawseti(L, -2, 1); // <== 0: uuid

			lua_pushinteger(L, service.valid_characteristics);
			lua_rawseti(L, -2, 2); // <== 1: valid_characteristics

			lua_createtable(L, service.valid_characteristics, 0);
			for (int at2 = 0; at2 < service.valid_characteristics; at2 ++) {
				const SDL_BleCharacteristic& chara = service.characteristics[at2];
				lua_createtable(L, 2, 0); // 0:uuid, 1: properties
				lua_pushstring(L, chara.uuid);
				lua_rawseti(L, -2, 1);

				lua_pushinteger(L, chara.properties);
				lua_rawseti(L, -2, 2);

				lua_rawseti(L, -2, at2 + 1);
			}
			lua_rawseti(L, -2, 3); // <== 2: characteristics

			lua_rawseti(L, -2, at + 1);
		}

		lua_setfield(L, -2, "services");
	}
}

void vble::app_discover_peripheral(SDL_BlePeripheral& peripheral)
{
	VALIDATE(!clazz_.empty(), null_str);
	luaW_getglobal(L, clazz_, "did_discover_peripheral");
	push_peripheral(L, peripheral, false);
	lua_.protected_call(1, 0);
}

void vble::app_connect_peripheral(SDL_BlePeripheral& peripheral, const int error)
{
	VALIDATE(!clazz_.empty(), null_str);
	luaW_getglobal(L, clazz_, "did_connect_peripheral");
	push_peripheral(L, peripheral, true);
	lua_pushinteger(L, error);
	lua_.protected_call(2, 0);
}

void vble::app_disconnect_peripheral(SDL_BlePeripheral& peripheral, const int error)
{
	VALIDATE(!clazz_.empty(), null_str);
	luaW_getglobal(L, clazz_, "did_disconnect_peripheral");
	push_peripheral(L, peripheral, false);
	lua_pushinteger(L, error);
	lua_.protected_call(2, 0);
}

void vble::app_discover_characteristics(SDL_BlePeripheral& peripheral, SDL_BleService& service, const int error)
{
	VALIDATE(false, null_str);
}

void vble::app_read_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const unsigned char* data, int len)
{
	VALIDATE(!clazz_.empty(), null_str);
	luaW_getglobal(L, clazz_, "did_read_characteristic");
	push_peripheral(L, peripheral, false);
	lua_pushstring(L, characteristic.uuid);
	lua_pushlstring(L, (const char*)data, len);
	lua_.protected_call(3, 0);
}

bool vble::app_task_callback(ttask& task, int step_at, bool start)
{
	VALIDATE(!clazz_.empty(), null_str);
	luaW_getglobal(L, clazz_, "did_task_callback");
	lua_pushinteger(L, task.id());
	lua_pushinteger(L, step_at);
	lua_pushboolean(L, start? 1: 0);
	lua_.protected_call(3, 1);
	
	bool ret = luaW_toboolean(L, -1);
	lua_pop(L, 1);
	return ret;
}