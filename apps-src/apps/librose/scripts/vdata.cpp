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

#include "base_instance.hpp"

#include "scripts/vdata.hpp"
#include "scripts/rose_lua_kernel.hpp"
#include "scripts/lua_common.hpp"
#include "scripts/vconfig.hpp"

#include "lua/lauxlib.h"
#include "lua/lua.h"
#include "lua/lobject.h"

using namespace std::placeholders;

/**
 * Destroys a vhttp_agent object before it is collected (__gc metamethod).
 */

static int impl_vjson_collect(lua_State *L)
{
	vjson *v = static_cast<vjson *>(lua_touserdata(L, 1));
	v->~vjson();
	return 0;
}

static int impl_vjson_get(lua_State *L)
{
	vjson *v = static_cast<vjson *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vjson_get_value(lua_State *L)
{
	vjson *v = static_cast<vjson *>(luaL_checkudata(L, 1, vjson::metatableKey));

	const char* field = luaL_checkstring(L, 2);
	int type = luaL_checkinteger(L, 3);
	v->json_value(field, type);
	return 1;
}

static int impl_vjson_array_size(lua_State *L)
{
	vjson *v = static_cast<vjson *>(luaL_checkudata(L, 1, vjson::metatableKey));

	int s = v->json_array_size();
	lua_pushinteger(L, s);
	return 1;
}

static int impl_vjson_array_at(lua_State *L)
{
	vjson *v = static_cast<vjson *>(luaL_checkudata(L, 1, vjson::metatableKey));

	int at = luaL_checkinteger(L, 2);
	v->json_array_at(at);
	return 1;
}

void luaW_pushvjson_bh(lua_State* L)
{
	tstack_size_lock lock(L, 0);
	VALIDATE(lua_type(L, -1) == LUA_TUSERDATA, null_str);
	if (luaL_newmetatable(L, vjson::metatableKey)) {
	// luaL_newmetatable(L, vjson::metatableKey);
		luaL_Reg metafuncs[] {
			{"__gc", impl_vjson_collect},
			{"__index", impl_vjson_get},
			{"value", impl_vjson_get_value},
			{"array_size", impl_vjson_array_size},
			{"array_at", impl_vjson_array_at},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vjson::metatableKey);
	}
	lua_setmetatable(L, -2);
}

void luaW_pushvjson(lua_State* L, const Json::Value& value)
{
	new(L) vjson(instance->lua(), value);
	luaW_pushvjson_bh(L);
}

const char vjson::metatableKey[] = "vjson";
vjson::vjson(rose_lua_kernel& lua)
	: L(lua.get_state())
	, lua_(lua)
{
}

vjson::vjson(rose_lua_kernel& lua, const Json::Value& value)
	: L(lua.get_state())
	, lua_(lua)
	, value_(value)
{}

vjson::~vjson()
{
	// SDL_Log("[lua.gc]---vjson::~vjson()---");
}

void vjson::json_value(const char* field, int type)
{
	VALIDATE(field != nullptr && field[0] != '\0', null_str);

	if (type == LUA_TBOOLEAN) {
		lua_pushboolean(L, value_[field].asBool());

	} else if (type == LUA_TSTRING) {
		lua_pushstring(L, value_[field].asString().c_str());

	} else if (type == LUA_VNUMFLT) {
		lua_pushnumber(L, value_[field].asDouble());

	} else if (type == LUA_VNUMINT) {
		lua_pushinteger(L, value_[field].asInt64());

	} else if (type == LUA_TUSERDATA) {
		Json::Value& result = value_[field];
		if (result.isObject() || result.isArray()) {
			luaW_pushvjson(L, result);
		} else {
			lua_pushnil(L);
		}

	} else {
		VALIDATE(false, null_str);
	}
}

int vjson::json_array_size() const
{
	if (!value_.isArray()) {
		return nposm;
	}
	return value_.size();
}

void vjson::json_array_at(int at)
{
	Json::Value& results = value_[at];
	luaW_pushvjson(L, results);
}

/**
 * Destroys a vhttp_agent object before it is collected (__gc metamethod).
 */
static int impl_vdata_collect(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));
	v->~vdata();
	return 0;
}

static int impl_vdata_get(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vdata_read_json(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int offset = luaL_checkinteger(L, 2);
	int len = luaL_optinteger(L, 3, nposm);

	v->read_json(offset, len);
	return 1;
}

static int impl_vdata_read_lstring(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int offset = luaL_checkinteger(L, 2);
	int len = luaL_optinteger(L, 3, nposm);

	v->read_lstring(offset, len);
	return 1;
}

static int impl_vdata_read_vdata(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int offset = luaL_checkinteger(L, 2);
	int len = luaL_optinteger(L, 3, nposm);
	bool ro = lua_isnoneornil(L, 4)? false: luaW_toboolean(L, 4);

	v->read_vdata(offset, len, ro);
	return 1;
}

static int impl_vdata_read_lipdp(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int offset = luaL_checkinteger(L, 2);
	int len = luaL_optinteger(L, 3, nposm);

	v->read_lipdp(offset, len);
	return 1;
}

static int impl_vdata_write_lstring(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	size_t size = 0;
	const char* data = luaL_checklstring(L, 2, &size);

	int wrote = v->write_lstring(data, size);
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_write_hexstring(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	size_t size = 0;
	const char* data = luaL_checklstring(L, 2, &size);

	if (!utils::is_hexstring(data, size)) {
		return luaL_argerror(L, 2, "isn't a valid hexstring");
	}
	int wrote = v->write_hexstring(data, size);
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_write_json(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	vconfig vcfg = luaW_checkvconfig(L, 2);

	int wrote = v->write_json(vcfg.get_config());
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_write_align(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int align = luaL_checkinteger(L, 2);
	int val = luaL_optinteger(L, 3, '\0');

	int wrote = v->write_align(align, val);
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_write_nbyte(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int n = luaL_checkinteger(L, 2);
	int val = luaL_optinteger(L, 3, '\0');

	int wrote = v->write_nbyte(n, val);
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_write_lipdp(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int cmd = luaL_checkinteger(L, 2);

	int items_index = 3;
	luaL_checktype(L, items_index, LUA_TTABLE);
	int item_count = lua_rawlen(L, items_index);
	VALIDATE(item_count > 0, null_str);

	tlipdp_item* items = nullptr;
	tlipdp_items_lock lock(item_count, &items);
	int index = 0;
	int payload_len = 0;
	{
		// If has error, result to call luaL_argerror, it will make tstack_size_lock throw an exception.
		tstack_size_lock lock(L, 0);
		size_t lsize = 0;
		for (int i = 1; i <= item_count; ++i) {
			lua_rawgeti(L, items_index, i);
			VALIDATE(lua_istable(L, -1), null_str);

			const int fields = lua_rawlen(L, -1);

			lua_rawgeti(L, -1, 1);
			int type = luaL_checkinteger(L, -1);
			if (type == lipdp_tn8 || type == lipdp_tn32 || type == lipdp_tn64) {
				VALIDATE(fields == 2, null_str);
				// field#1 push to stack, so table's index changes to -2.
				lua_rawgeti(L, -2, 2);
				lua_Integer val = luaL_checkinteger(L, -1);
				if (type == lipdp_tn8) {
					LIPDP_PUSH_ITEM_n8((uint32_t)val);

				} else if (type == lipdp_tn32) {
					LIPDP_PUSH_ITEM_n32(val);

				} else {
					// type == lipdp_tn64
					LIPDP_PUSH_ITEM_n64(val);
				}

			} else if (type == lipdp_tstring || type == lipdp_thexstring) {
				VALIDATE(fields == 2, null_str);
				// field#1 push to stack, so table's index changes to -2.
				lua_rawgeti(L, -2, 2);
				const char* data = luaL_checklstring(L, -1, &lsize);

				if (type == lipdp_tstring) {
					LIPDP_PUSH_ITEM_string(data, lsize);
				} else {
					if (!utils::is_hexstring(data, lsize)) {
						return luaL_argerror(L, items_index, "has a invalid hexstring");
					}
					LIPDP_PUSH_ITEM_hexstring(data, lsize);
				}

			} else if (type == lipdp_tip) {
				// field#1 push to stack, so table's index changes to -2.
				lua_rawgeti(L, -2, 2);
				uint8_t af = (uint32_t)luaL_checkinteger(L, -1);

				if (af == LEAGOR_BLE_AF_ERR_VER || af == LEAGOR_BLE_AF_ERR_PRIVACY) {
					LIPDP_PUSH_ITEM_ipunspec(af);

				} else if (af == LEAGOR_BLE_AF_INET) {
					// field#1 and field#2 push to stack, so table's index changes to -3.
					lua_rawgeti(L, -3, 3);
					uint32_t ipv4 = luaL_checkinteger(L, -1);
					LIPDP_PUSH_ITEM_ipv4(ipv4);

				} else {
					VALIDATE(af == LEAGOR_BLE_AF_INET, null_str);
					// field#1 and field#2 push to stack, so table's index changes to -3.
					lua_rawgeti(L, -3, 3);
					const char* data = luaL_checklstring(L, -1, &lsize);
					VALIDATE(lsize == 16, null_str);
					LIPDP_PUSH_ITEM_ipv6(data);
				}

			} else {
				VALIDATE(false, "unkown lipdp type");
			}
				
			lua_pop(L, 1 + fields); // 1table + 3fields
		}
	}
	VALIDATE(index == item_count, null_str);
	
	int wrote = v->write_lipdp(cmd, payload_len, items, item_count);
	lua_pushinteger(L, v->data_vsize());
	lua_pushinteger(L, wrote);
	return 2;
}

static int impl_vdata_to_file(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int offset = luaL_checkinteger(L, 2);
	int len = luaL_checkinteger(L, 3);
	int type = luaL_checkinteger(L, 4);
	const char* path = luaL_checkstring(L, 5);

	int ret = v->to_file(offset, len, type, path);
	lua_pushinteger(L, ret);
	return 1;
}

static int impl_vdata_set_vsize(lua_State *L)
{
	vdata *v = static_cast<vdata *>(lua_touserdata(L, 1));

	int vsize = luaL_checkinteger(L, 2);

	v->set_data_vsize(vsize);
	return 0;
}

static void luaW_pushvdata_bh(lua_State* L)
{
	// tstack_size_lock lock(L, 0);
	if (luaL_newmetatable(L, vdata::metatableKey)) {
	// luaL_newmetatable(L, vdata::metatableKey);
		luaL_Reg metafuncs[] {
			{"__gc", impl_vdata_collect},
			{"__index", impl_vdata_get},
			{"read_json", impl_vdata_read_json},
			{"read_lstring", impl_vdata_read_lstring},
			{"read_vdata", impl_vdata_read_vdata},
			{"read_lipdp", impl_vdata_read_lipdp},
			{"write_lstring", impl_vdata_write_lstring},
			{"write_hexstring", impl_vdata_write_hexstring},
			{"write_json", impl_vdata_write_json},
			{"write_align", impl_vdata_write_align},
			{"write_nbyte", impl_vdata_write_nbyte},
			{"write_lipdp", impl_vdata_write_lipdp},

			{"to_file", impl_vdata_to_file},
			{"set_vsize", impl_vdata_set_vsize},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vdata::metatableKey);
	}
	lua_setmetatable(L, -2);
}

vdata* luaW_pushvrwdata(lua_State* L, int initial_size)
{
	vdata* v = new(L) vdata(instance->lua(), initial_size);
	luaW_pushvdata_bh(L);
	return v;
}

void luaW_pushvrodata(lua_State* L, const uint8_t* data, int len)
{
	new(L) vdata(instance->lua(), data, len);
	luaW_pushvdata_bh(L);
}

const char vdata::metatableKey[] = "vdata";
vdata::vdata(rose_lua_kernel& lua, const uint8_t* data, int size)
	: L(lua.get_state())
	, lua_(lua)
	, data_ptr_(const_cast<uint8_t*>(data))
	, data_size_(size)
	, data_vsize_(size)
	, read_only_(true)
{
}

vdata::vdata(rose_lua_kernel& lua, int initial_size)
	: L(lua.get_state())
	, lua_(lua)
	, data_ptr_(nullptr)
	, data_size_(0)
	, data_vsize_(0)
	, read_only_(false)
{
	if (initial_size != nposm) {
		VALIDATE(initial_size >= min_writeable_size, null_str);
	} else {
		initial_size = min_writeable_size;
	}
	resize_data(initial_size, 0);
}

vdata::~vdata()
{
	// SDL_Log("[lua.gc]---vdata::~vdata()---");
	if (!read_only_) {
		free(data_ptr_);
	}
}

int vdata::validate_and_ajdust(int offset, int size) const
{
	VALIDATE(offset >= 0 && offset < data_vsize_, null_str);
	if (size != nposm) {
		VALIDATE(size > 0 && offset + size <= data_vsize_, null_str);
	} else {
		size = data_vsize_ - offset;
	}
	return size;
}

void vdata::set_data_vsize(int vsize)
{
	VALIDATE(!read_only_, null_str);
	VALIDATE(vsize >= 0 && vsize < data_size_, null_str);
	data_vsize_ = vsize;
}

void vdata::reset_ro_data(const uint8_t* data, int _size)
{
	VALIDATE(read_only_, null_str);
	data_ptr_ = const_cast<uint8_t*>(data);
	data_size_ = _size;
	data_vsize_ = _size;
}

void vdata::read_json(int offset, int size) const
{
	size = validate_and_ajdust(offset, size);

	tstack_size_lock lock(L, 1);
	std::stringstream err;
	try {
		Json::Reader reader;
		
		vjson* json = new(L) vjson(instance->lua());
		if (!reader.parse((const char*)data_ptr_ + offset, (const char*)data_ptr_ + offset + size, json->value())) {
			err << "Invalid json request";
			lua_pop(L, 1);
		} else {
			luaW_pushvjson_bh(L);
			return;
		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	// as if fail, return nil, let lua know fail.
	lua_pushnil(L);
}

void vdata::read_lstring(int offset, int size) const
{
	size = validate_and_ajdust(offset, size);
	lua_pushlstring(L, (const char*)data_ptr_ + offset, size);
}

void vdata::read_vdata(int offset, int size, bool ro) const
{
	size = validate_and_ajdust(offset, size);
	if (ro) {
		luaW_pushvrodata(L, data_ptr_ + offset, size);
	} else {
		vdata* sub = luaW_pushvrwdata(L, nposm);
		sub->write_lstring((const char*)data_ptr_ + offset, size);
	}
}

static void luaW_pushlipdptable(lua_State* L, const tlipdp_parser& parser)
{
	lua_createtable(L, parser.count, 0);
	for (int at = 0; at < parser.count; at ++) {
		const tlipdp_item& item = parser.items[at];
		int fields = 2;
		if (item.type == lipdp_tip) {
			if (item.u8 == LEAGOR_BLE_AF_INET || item.u8 == LEAGOR_BLE_AF_INET6) {
				fields = 3;
			}
		}
		lua_createtable(L, fields, 0);

		lua_pushinteger(L, item.type);
		lua_rawseti(L, -2, 1);

		if (item.type == lipdp_tn8) {
			lua_pushinteger(L, item.u8);
			lua_rawseti(L, -2, 2);

		} else if (item.type == lipdp_tn32) {
			lua_pushinteger(L, item.int32);
			lua_rawseti(L, -2, 2);

		} else if (item.type == lipdp_tn64) {
			lua_pushinteger(L, item.int64);
			lua_rawseti(L, -2, 2);

		} else if (item.type == lipdp_tstring) {
			lua_pushlstring(L, (const char*)item.data, item.int32);
			lua_rawseti(L, -2, 2);

		} else if (item.type == lipdp_tbinary) {
			lua_pushlstring(L, (const char*)item.data, item.int32);
			lua_rawseti(L, -2, 2);

		} else if (item.type == lipdp_tip) {
			lua_pushinteger(L, item.u8);
			lua_rawseti(L, -2, 2);
			if (item.u8 == LEAGOR_BLE_AF_INET) {
				// ipv4
				lua_pushinteger(L, item.int32);
				lua_rawseti(L, -2, 3);

			} else if (item.u8 == LEAGOR_BLE_AF_INET6) {
				// ipv6
				lua_pushlstring(L, (const char*)item.data, 16);
				lua_rawseti(L, -2, 3);

			} else if (item.u8 == LEAGOR_BLE_AF_ERR_VER || item.u8 == LEAGOR_BLE_AF_ERR_PRIVACY) {
				// unspec

			} else {
				VALIDATE(false, null_str);
			}
		} else {
			// lipdp_thexstring or unknown type
			SDL_Log("luaW_pushlipdptable, unsupport type: %i", item.type);
		}
		lua_rawseti(L, -2, at + 1);
	}
}

void vdata::read_lipdp(int offset, int size)
{
	size = validate_and_ajdust(offset, size);

	tstack_size_lock lock(L, 1);
	parser_.handle(data_ptr_ + offset, size);

	luaW_pushlipdptable(L, parser_);
}

void vdata::resize_data(int size, int vsize)
{
	VALIDATE(!read_only_, null_str);

	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (data_ptr_) {
			if (vsize) {
				memcpy(tmp, data_ptr_, vsize);
			}
			free(data_ptr_);
		}
		data_ptr_ = tmp;
		data_size_ = size;
	}
}

int vdata::write_lstring(const char* data, int size)
{
	VALIDATE(size > 0, null_str);
	resize_data(data_vsize_ + size, data_vsize_);
	memcpy(data_ptr_ + data_vsize_, data, size);
	data_vsize_ += size;
	return size;
}

int vdata::write_hexstring(const char* data, int size)
{
	VALIDATE(size > 0 && size % 2 == 0, null_str);
	int hsize = size / 2;
	const int post_vsize = data_vsize_ + hsize;
	resize_data(post_vsize, data_vsize_);
	utils::hex_decode_prealloc(data, size, data_ptr_ + data_vsize_);
	data_vsize_ += hsize;
	return hsize;
}

int vdata::write_json(const config& cfg)
{
	Json::Value json;
	cfg.to_json(json);

	Json::FastWriter writer;
	const std::string str = writer.write(json);
	return write_lstring(str.c_str(), str.size());
}

#define is_power_2(x)	((x > 0) && (0 == (x & (x - 1))))

int vdata::write_align(int align, int val)
{
	VALIDATE(align > 0, null_str);
	int should_vsize;
	if (is_power_2(align)) {
		should_vsize = posix_align_ceil(data_vsize_, align);
	} else {
		should_vsize = posix_align_ceil2(data_vsize_, align);
	}

	int expend = should_vsize - data_vsize_;
	if (expend != 0) {
		VALIDATE(expend > 0, null_str);
		resize_data(should_vsize, data_vsize_);
		memset(data_ptr_ + data_vsize_, val, expend);
		data_vsize_ += expend;
	}
	return expend;
}

int vdata::write_nbyte(int n, int val)
{
	VALIDATE(n > 0, null_str);

	resize_data(data_vsize_ + n, data_vsize_);
	memset(data_ptr_ + data_vsize_, val, n);
	data_vsize_ += n;
	return n;
}

int vdata::write_lipdp(int cmd, int payload_len, const tlipdp_item* items, int count)
{
	VALIDATE(cmd >= 0 && payload_len > 0 && items != nullptr && count > 0, null_str);
	resize_data(data_vsize_ + LEAGOR_BLE_MTU_HEADER_SIZE + payload_len, data_vsize_);
	const int packet_len = packer_.items_2_data(cmd, payload_len, items, count, data_ptr_ + data_vsize_);
	data_vsize_ += packet_len;
	return packet_len;
}

int vdata::to_file(int offset, int size, int type, const std::string& path) const
{
	size = validate_and_ajdust(offset, size);
	if (type == PATHTYPE_ABS) {
		VALIDATE(game_config::os == os_windows, null_str);
		write_file(path, (const char*)data_ptr_ + offset, size);

	} else {
		VALIDATE(false, null_str);
	}
	return size;
}

//
// vlipdp
//

// Destroys a vlipdp object before it is collected (__gc metamethod).
static int impl_vlipdp_collect(lua_State *L)
{
	vlipdp *v = static_cast<vlipdp *>(lua_touserdata(L, 1));
	v->~vlipdp();
	return 0;
}

static int impl_vlipdp_get(lua_State *L)
{
	vlipdp *v = static_cast<vlipdp *>(lua_touserdata(L, 1));

	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);
	return ret? 1: 0;
}

static int impl_vlipdp_enqueue(lua_State* L)
{
	vlipdp *v = static_cast<vlipdp *>(lua_touserdata(L, 1));

	size_t len = 0;
	const char* data = luaL_checklstring(L, 2, &len);

	v->enqueue((const uint8_t*)data, len);
	return 0;
}

const char vlipdp::metatableKey[] = "vlipdp";

vlipdp::vlipdp(rose_lua_kernel& lua, const std::set<int>& recv_cmds, const std::string& clazz, const std::string& did_read_method)
	: L(lua.get_state())
	, lua_(lua)
	, clazz_(clazz)
	, did_read_method_(did_read_method)
{
	VALIDATE(!recv_cmds.empty(), null_str);
	VALIDATE(!clazz.empty(), null_str);
	VALIDATE(!did_read_method.empty(), null_str);

	recv_cmds_ = recv_cmds;
	set_did_read(std::bind(&vlipdp::did_read, this, _1, _2, _3));
}

vlipdp::~vlipdp()
{
	SDL_Log("---vlipdp::~vlipdp()---");
}

static void luaW_pushvlipdp_bh(lua_State* L)
{
	// tstack_size_lock lock(L, 0);
	if (luaL_newmetatable(L, vlipdp::metatableKey)) {
	// luaL_newmetatable(L, vlipdp::metatableKey);
		luaL_Reg metafuncs[] {
			{"__gc", impl_vlipdp_collect},
			{"__index", impl_vlipdp_get},
			{"enqueue", impl_vlipdp_enqueue},
			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vlipdp::metatableKey);
	}
	lua_setmetatable(L, -2);
}

void vlipdp::did_read(int cmd, const uint8_t* data, int len)
{
	parser_.handle(data, len);

	luaW_getglobal(L, clazz_, did_read_method_);
	lua_pushinteger(L, cmd);
	luaW_pushlipdptable(L, parser_);
	lua_pushlstring(L, (const char*)data, len);
	lua_.protected_call(3, 0);
}

vlipdp* luaW_pushvlipdp(lua_State* L, const std::set<int>& recv_cmds, const std::string& clazz, const std::string& did_read_method)
{
	vlipdp* v = new(L) vlipdp(instance->lua(), recv_cmds, clazz, did_read_method);
	luaW_pushvlipdp_bh(L);
	return v;
}