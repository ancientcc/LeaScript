/*
   Copyright (C) 2014 - 2018 by Chris Beck <render787@gmail.com>
   

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "scripts/lua_kernel_base.hpp"

// #include "game_config.hpp"
#include "game_errors.hpp"
// #include "gui/core/gui_definition.hpp"
// #include "log.hpp"
#include "lua_jailbreak_exception.hpp"  // for lua_jailbreak_exception
#include "random.hpp"
// #include "seed_rng.hpp"
// #include "deprecation.hpp"

#ifdef DEBUG_LUA
#include "scripts/debug_lua.hpp"
#endif

#include "scripts/lua_common.hpp"
#include "scripts/lua_fileops.hpp"
#include "scripts/gui/dialogs/rldialog.hpp"
#include "scripts/vnet.hpp"
#include "scripts/vdata.hpp"
#include "scripts/vprotobuf.hpp"
#include "scripts/vsurface.hpp"
#include "scripts/vble.hpp"

// #include "formula/string_utils.hpp"
#include "serialization/string_utils.hpp"
#include "serialization/parser.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "base_instance.hpp"
#include "qr_code.hpp"
#include "scripts/vconfig.hpp" // for config_variable_set

#include <cstring>
#include <exception>
#include <new>
#include <string>
#include <sstream>
#include <vector>

#include <openssl/aes.h>

#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "wkoscript.hpp"

using namespace std::placeholders;

extern int rose_str_dump (lua_State* L);


// Callback implementations

/**
 * Compares 2 version strings - which is newer.
 * - Args 1,3: version strings
 * - Arg 2: comparison operator (string)
 * - Ret 1: comparison result
 */
/*
static int intf_compare_versions(lua_State* L)
{
	char const *v1 = luaL_checkstring(L, 1);

	const VERSION_COMP_OP vop = parse_version_op(luaL_checkstring(L, 2));
	if(vop == OP_INVALID) return luaL_argerror(L, 2, "unknown version comparison operator - allowed are ==, !=, <, <=, > and >=");

	char const *v2 = luaL_checkstring(L, 3);

	const bool result = do_version_check(version_info(v1), vop, version_info(v2));
	lua_pushboolean(L, result);

	return 1;
}
*/
/**
 * Replacement print function -- instead of printing to std::cout, print to the command log.
 * Intended to be bound to this' command_log at registration time.
 */

int lua_kernel_base::intf_print(lua_State* L)
{
	SDL_Log("intf_print called:");
	size_t nargs = lua_gettop(L);

	lua_getglobal(L, "tostring");
	for (size_t i = 1; i <= nargs; ++i) {
		lua_pushvalue(L, -1); // function to call: "tostring"
		lua_pushvalue(L, i); // value to pass through tostring() before printing
		lua_call(L, 1, 1);
		const char * str = lua_tostring(L, -1);
		if (!str) {
			SDL_Log("'tostring' must return a value to 'print'");
			str = "";
		}
		if (i > 1) {
			cmd_log_ << "\t"; //separate multiple args with tab character
		}
		cmd_log_ << str;
		SDL_Log("'%s'", str);
		lua_pop(L, 1); // Pop the output of tostrring()
	}
	lua_pop(L, 1); // Pop 'tostring' global

	cmd_log_ << "\n";
	SDL_Log("");

	return 0;
}

/**
 * Replacement load function. Mostly the same as regular load, but disallows loading binary chunks
 * due to CVE-2018-1999023.
 */

static int intf_load(lua_State* L)
{
	std::string chunk = luaL_checkstring(L, 1);
	const char* name = luaL_optstring(L, 2, chunk.c_str());
	std::string mode = luaL_optstring(L, 3, "t");
	bool override_env = !lua_isnone(L, 4);

	if(mode != "t") {
		return luaL_argerror(L, 3, "binary chunks are not allowed for security reasons");
	}

	int result = luaL_loadbufferx(L, chunk.data(), chunk.length(), name, "t");
	if(result != LUA_OK) {
		lua_pushnil(L);
		// Move the nil as the first return value, like Lua's own load() does.
		lua_insert(L, -2);

		return 2;
	}

	if(override_env) {
		// Copy "env" to the top of the stack.
		lua_pushvalue(L, 4);
		// Set "env" as the first upvalue.
		const char* upvalue_name = lua_setupvalue(L, -2, 1);
		if(upvalue_name == nullptr) {
			// lua_setupvalue() didn't remove the copy of "env" from the stack, so we need to do it ourselves.
			lua_pop(L, 1);
		}
	}

	return 1;
}

// Same for loadstring.

static int intf_loadstring(lua_State* L)
{
	std::string string = luaL_checkstring(L, 1);
	const char* name = luaL_optstring(L, 2, string.c_str());

	// deprecated_message("loadstring()", DEP_LEVEL::FOR_REMOVAL, {1, 15, 0}, "Use load() instead.");

	int result = luaL_loadbufferx(L, string.data(), string.length(), name, "t");
	if(result != LUA_OK) {
		lua_pushnil(L);
		lua_insert(L, -2);
		return 2;
	}

	return 1;
}

// The show lua console callback is similarly a method of lua kernel
int lua_kernel_base::intf_show_lua_console(lua_State* L)
{
	if (cmd_log_.external_log_) {
		std::string message = "There is already an external logger attached to this lua kernel, you cannot open the lua console right now.";
		log_error(message.c_str());
		cmd_log_ << message << "\n";
		return 0;
	}

	return 0;
	// return lua_gui2::show_lua_console(L, this);
}

/**
* Returns the time stamp, exactly as [set_variable] time=stamp does.
* - Ret 1: integer
*/
static int intf_get_time_stamp(lua_State* L) {
	lua_pushinteger(L, SDL_GetTicks());
	return 1;
}

static int intf_format(lua_State* L)
{
	config cfg = luaW_checkconfig(L, 2);
	config_variable_set variables(cfg);
	if (lua_isstring(L, 1)) {
		std::string str = lua_tostring(L, 1);
		luaW_pushtstring(L, utils::interpolate_variables_into_string(str, variables));
		return 1;
	}
	t_string str = luaW_checktstring(L, 1);
	luaW_pushtstring(L, utils::interpolate_variables_into_tstring(str, variables));
	return 1;
}

static int intf_webrtc_post(lua_State* L)
{
	gui2::vdialog* vdlg = static_cast<gui2::vdialog*>(luaL_checkudata(L, 1, gui2::vdialog::metatableKey));
	int msg = luaL_checkinteger(L, 2);
	config cfg = luaW_checkconfig(L, 3);

	gui2::trldialog::tmsg_data_cfg* pdata = new gui2::trldialog::tmsg_data_cfg(cfg);
	rtc::Thread::Current()->Post(RTC_FROM_HERE, &vdlg->dialog(), msg, pdata);
	return 0;
}

// extern void win_ShellExecuteW_open(const std::string& url);

static int intf_open_url(lua_State* L)
{
	const char* url = luaL_checkstring(L, 1);
	SDL_OpenUrl(url);
	return 0;
}

void wkoscript_lua_push_device(lua_State* L, const std::string& filename)
{
	aplt::twkoscript script;
	script.from_file(filename);

	tstack_size_lock lock(L, 1);

	lua_createtable(L, 4, 0);
	lua_pushstring(L, utils::extract_file(filename).c_str());
	lua_rawseti(L, -2, 1); // <== 0: file

	std::string name;
	std::string author;
	std::string reference;

	if (script.valid()) {
		name = script.name;
		author = script.author;
		reference = script.reference;
	}
	lua_pushstring(L, name.c_str());
	lua_rawseti(L, -2, 2); // <== 1: name

	lua_pushstring(L, author.c_str());
	lua_rawseti(L, -2, 3); // <== 2: author

	lua_pushstring(L, reference.c_str());
	lua_rawseti(L, -2, 4); // <== 3: reference

	// lua_pushstring(L, utils::join(item.brands, ";").c_str());
	// lua_rawseti(L, -2, 5); // <== 4: brands
}

static int intf_wkoscript_list_files_metadata(lua_State* L)
{
	// tlua_block* v = *static_cast<tlua_block **>(lua_touserdata(L, 1));

	const std::string wkoscript_dir = luaL_checkstring(L, 1);
	std::set<std::string> cfgfiles;
	aplt::list_wkoscript_files_by_type(wkoscript_dir, aplt::type_wkoscript_cfgfiles, cfgfiles);

	lua_pushinteger(L, cfgfiles.size());

	// items
	int lua_cfgfile_count = cfgfiles.size();
	lua_createtable(L, lua_cfgfile_count, 0);

	{
		tstack_size_lock lock(L, 0);
		int at = 0;
		for (std::set<std::string>::const_iterator it = cfgfiles.begin(); it != cfgfiles.end(); ++ it, at ++) {
			std::string filename(wkoscript_dir);
			filename.append("/" + *it);

			wkoscript_lua_push_device(L, filename);
			lua_rawseti(L, -2, at + 1);
		}
	}

	return 2;
}

/**
* Dumps a wml table or userdata wml object into a pretty string.
* - Arg 1: wml table or vconfig userdata
* - Ret 1: string
*/
static int intf_wml_tostring(lua_State* L) {
	const config& arg = luaW_checkconfig(L, 1);
	std::ostringstream stream;
	write(stream, arg);
	lua_pushstring(L, stream.str().c_str());
	return 1;
}

static int intf_log(lua_State* L)
{
	const char* str = luaL_checkstring(L, 1);
	SDL_Log("{lua}[%u]%s", SDL_GetTicks(), str);
	return 0;
}

static int intf_breakpoint(lua_State* L)
{
	SDL_Log("for debug. app can insert breakpoint at this line");
	// return luaL_argerror(L, 1, "found a null string argument to rose require");
	return 0;
}

static int intf_os(lua_State* L)
{
	lua_pushinteger(L, game_config::os);
	return 1;
}

static int intf_gettext(lua_State* L)
{
	const char* textdomain = luaL_checkstring(L, 1);
	const char* msgid = luaL_checkstring(L, 2);

	lua_pushstring(L, dsgettext(textdomain, msgid));
	return 1;
}

static int intf_vgettext(lua_State* L)
{
	const char* textdomain = luaL_checkstring(L, 1);
	const char* msgid = luaL_checkstring(L, 2);
	vconfig vcfg = luaW_checkvconfig(L, 3);
	utils::string_map symbols;
	for (const config::attribute& a: vcfg.get_config().attribute_range()) {
		symbols[a.first] = a.second.str(); 
	}

	lua_pushstring(L, vgettext(textdomain, msgid, symbols).c_str());
	return 1;
}

static int intf_mk_integer(lua_State* L)
{
	int l1type = lua_type(L, 1);

	int n[8];
	bool n_is_nil[8];
	for (int at = 0; at < 8; at ++) {
		n_is_nil[at] = lua_isnone(L, at + 2);
		if (at < 2 && n_is_nil[at]) {
			return luaL_argerror(L, at + 2, "type must be integer");
		}
		n[at] = n_is_nil[at]? 0: luaL_checkinteger(L, at + 2);
	}
	
	uint64_t result;
	if (l1type == LUA_TSTRING) {
		size_t len;
		const char* src = luaL_checklstring(L, 1, &len);
		uint8_t val[8];
		for (int at = 0; at < 8; at ++) {
			if (!n_is_nil[at] && n[at] >= (uint8_t)len) {
				return luaL_argerror(L, at + 2, "invalid value");
			}
			val[at] = n_is_nil[at]? 0: src[n[at]];
		}

		result = posix_mku64(posix_mku32(posix_mku16(val[0], val[1]), posix_mku16(val[2], val[3])), 
			posix_mku32(posix_mku16(val[4], val[5]), posix_mku16(val[6], val[7])));

	} else {
		return luaL_argerror(L, 1, "invalid type");
	}
	
	lua_pushinteger(L, result);
	return 1;
}

static int intf_integer_tostr(lua_State* L)
{
	int64_t val = luaL_checkinteger(L, 1);
	int64_t type = luaL_checkinteger(L, 2);

	std::string result;
	if (type == FMT_IPV4) {
		result = utils::from_ipv4(val);

	} else if (type == FMT_TIME) {
		result = utils::format_time_date(val);

	} else if (type == FMT_TIME_HHcMMcSS) {
		result = utils::format_time_hms(val);

	} else if (type == FMT_ELAPSE) {
		result = utils::format_elapse_hms(val);

	} else {
		return luaL_argerror(L, 2, "unknown type");
	}
	lua_pushstring(L, result.c_str());
	return 1;
}

static int intf_hex_string(lua_State* L)
{
	size_t len = 0;
	const char* data = luaL_checklstring(L, 1, &len);

	std::string ret = utils::hex_encode(data, len);
	lua_pushstring(L, ret.c_str());
	return 1;
}

namespace utils {
void lua_split(lua_State* L, std::string const &val, const char c, const int flags)
{
	lua_newtable(L);
	int index = 0;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
			
	while (i2 != val.end()) {
		if (*i2 == c) {
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip_end(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty()) {
				lua_pushstring(L, new_val.c_str());
				lua_rawseti(L, -2, ++ index);
				// res.push_back(new_val);
			}
			++i2;
			if (flags & STRIP_SPACES) {
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}

			i1 = i2;
		} else {
			++i2;
		}
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip_end(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty()) {
		lua_pushstring(L, new_val.c_str());
		lua_rawseti(L, -2, ++ index);
		// res.push_back(new_val);
	}

	// return res;
}
}

static int intf_split(lua_State* L)
{
	const char* src = luaL_checkstring(L, 1);
	int c = luaL_checkinteger(L, 2);
	int flags = luaL_optinteger(L, 3, utils::REMOVE_EMPTY | utils::STRIP_SPACES);
	utils::lua_split(L, src, c, flags);
	return 1;
}

static int intf_normalize_path(lua_State* L)
{
	const char* src = luaL_checkstring(L, 1);
	bool dosstyle = luaW_toboolean(L, 2);

	const std::string dst = utils::normalize_path(src, dosstyle);
	lua_pushlstring(L, dst.c_str(), dst.size());
	return 1;
}

// ==> rose.convert_string
enum {STRCVT_EXTRACT_DIRECTORY, STRCVT_STRIP};

static int intf_convert_string(lua_State* L)
{
	const std::string src = luaL_checkstring(L, 1);
	int code = luaL_checkinteger(L, 2);
	std::string dst;

	if (code == STRCVT_EXTRACT_DIRECTORY) {
		dst = utils::extract_directory(src);

	} else if (code == STRCVT_STRIP) {
		dst = src;
		utils::strip(dst);

	} else {
		char buf[128];
		SDL_snprintf(buf, sizeof(buf), "this Rose verion's 'convert' doesn't support code: %i", code);
		return luaL_argerror(L, 2, buf);
	}
	lua_pushlstring(L, dst.c_str(), dst.size());
	return 1;
}

static int intf_truncate_to_max_bytes(lua_State* L)
{
	const char* c_str = luaL_checkstring(L, 1);
	int max_bytes = luaL_checkinteger(L, 2);
	bool ellipsis = luaL_checkboolean(L, 3);

	int size = SDL_strlen(c_str);
	if (size <= max_bytes) {
		lua_pushlstring(L, c_str, size);

	} else {
		std::string new_str = utils::truncate_to_max_bytes2(c_str, max_bytes, ellipsis);
		lua_pushlstring(L, new_str.c_str(), new_str.size());
	}

	return 1;
}

static int intf_truncate_to_max_chars(lua_State* L)
{
	const char* c_str = luaL_checkstring(L, 1);
	int max_chars = luaL_checkinteger(L, 2);
	bool ellipsis = luaL_checkboolean(L, 3);

	std::string new_str = utils::truncate_to_max_chars2(c_str, max_chars, ellipsis);
	lua_pushlstring(L, new_str.c_str(), new_str.size());

	return 1;
}

enum {FMT_APLT_IOT_ALIAS_ID = FMT_ELAPSE + 1};

static int intf_is_format(lua_State* L)
{
	const std::string src = luaL_checkstring(L, 1);
	int code = luaL_checkinteger(L, 2);
	
	bool ret = false;
	if (code == FMT_UUID) {
		bool line = lua_isnoneornil(L, 3)? false: luaW_toboolean(L, 3);
		ret = utils::is_uuid(src, line);

	} else if (code == FMT_APLT_IOT_ALIAS_ID) {
		ret = aplt::is_valid_iot_alias_id(src);

	} else {
		VALIDATE(false, "This version doesn't support");
	}
	lua_pushboolean(L, ret);
	return 1;
}

static int intf_pack_rect(lua_State* L)
{
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);
	int w = luaL_checkinteger(L, 3);
	int h = luaL_checkinteger(L, 4);

	uint64_t result = lua_pack_rect(x, y, w, h);
	lua_pushinteger(L, result);
	return 1;
}

static int intf_unpack_rect(lua_State* L)
{
	uint64_t u64 = luaL_checkinteger(L, 1);
	uint32_t lo32 = posix_lo32(u64);
	uint32_t hi32 = posix_hi32(u64);

	int x = (short)posix_lo16(lo32);
	int y = (short)posix_hi16(lo32);
	int w = (short)posix_lo16(hi32);
	int h = (short)posix_hi16(hi32);

	lua_pushinteger(L, x);
	lua_pushinteger(L, y);
	lua_pushinteger(L, w);
	lua_pushinteger(L, h);
	return 4;
}

static int intf_pack_point(lua_State* L)
{
	int x = luaL_checkinteger(L, 1);
	int y = luaL_checkinteger(L, 2);

	uint64_t result = lua_pack_point(x, y);
	lua_pushinteger(L, result);
	return 1;
}

static int intf_unpack_point(lua_State* L)
{
	uint64_t u64 = luaL_checkinteger(L, 1);
	SDL_Point point = lua_unpack_point(u64);

	lua_pushinteger(L, point.x);
	lua_pushinteger(L, point.y);
	return 2;
}

static int intf_calculate_adaption_ratio_size(lua_State* L)
{
	int outer_w = luaL_checkinteger(L, 1);
	int outer_h = luaL_checkinteger(L, 2);
	int inner_w = luaL_checkinteger(L, 3);
	int inner_h = luaL_checkinteger(L, 4);

	tpoint result = calculate_adaption_ratio_size(outer_w, outer_h, inner_w, inner_h);
	lua_pushinteger(L, result.x);
	lua_pushinteger(L, result.y);
	return 2;
}

/**
 * Creates a vhttp_agent containing the WML table.
 * - Arg 1: WML table.
 * - Ret 1: vconfig userdata.
 */
static int intf_tovhttp_agent(lua_State* L)
{
	vconfig vcfg = luaW_checkvconfig(L, 1);
	luaW_pushvhttp_agent(L, vcfg.get_config());
	return 1;
}

static int intf_tovtcpcmgr(lua_State* L)
{
	// vconfig vcfg = luaW_checkvconfig(L, 1);
	luaW_pushvtcpcmgr(L);
	return 1;
}

static int intf_tovdata(lua_State* L)
{
	int initial_size = luaL_optinteger(L, 1, nposm);
	luaW_pushvrwdata(L, initial_size);
	return 1;
}

// vlipdp* luaW_pushvlipdp(lua_State* L, const std::set<int>& recv_cmds, const std::string& clazz, const std::string& did_read_method)

static int intf_tovlipdp(lua_State* L)
{
	std::set<int> recv_cmds;
	{
		// tstack_size_lock lock(L, 0);
		int cmds_index = 1;
		luaL_checktype(L, cmds_index, LUA_TTABLE);
		for (int i = 1, i_end = lua_rawlen(L, cmds_index); i <= i_end; ++i) {
			lua_rawgeti(L, cmds_index, i);
			recv_cmds.insert(luaL_checkinteger(L, -1));
		}
		lua_pop(L, (int)recv_cmds.size());
	}

	const std::string clazz = luaL_checkstring(L, 2);
	const std::string method = luaL_checkstring(L, 3);
	luaW_pushvlipdp(L, recv_cmds, clazz, method);
	return 1;
}

static int intf_tovprotobuf(lua_State* L)
{
	const char* lua_bundleid = luaL_checkstring(L, 1);
	const char* filepath = luaL_checkstring(L, 2);
	luaW_pushvprotobuf(L, get_aplt_user_data_dir(lua_bundleid) + "/" + filepath);
	return 1;
}

static int intf_tovsurface(lua_State* L)
{
	std::string image;
	if (lua_type(L, 1) == LUA_TSTRING) {
		image = luaL_checkstring(L, 1);
	}
	luaW_pushvsurface(L, image);
	return 1;
}

static int intf_tovtexture(lua_State* L)
{
	int type = lua_type(L, 1);
	if (type == LUA_TNONE) {
		luaW_pushvtexture(L);

	} else if (type == LUA_TUSERDATA) {
		vsurface* v = static_cast<vsurface *>(lua_touserdata(L, 1));
		luaW_pushvtexture(L, v->get());  // value is a userdata with wrong metatable

	} else {
		VALIDATE(false, null_str);
	}

	return 1;
}

static int intf_tovble(lua_State* L)
{
	const char* lua_bundleid = luaL_checkstring(L, 1);

	luaW_pushvble(L, lua_bundleid);
	return 1;
}

static int intf_write_qrcode(lua_State* L)
{
	const char* text = luaL_checkstring(L, 1);
	int width = luaL_checkinteger(L, 2);
	int height = luaL_checkinteger(L, 3);
	vsurface *v = static_cast<vsurface *>(luaL_checkudata(L, 4, vsurface::metatableKey));
	VALIDATE(width > 0 && height > 0 && v != nullptr, null_str);

	surface surf = generate_qr(text, width);
	v->reset(&surf);
	return 0;
}

static int intf_aes_internal(lua_State* L, bool encrypt)
{
	int type = luaL_checkinteger(L, 1);
	VALIDATE(type == AES256, null_str);
	size_t l = 0;
	const char* key = luaL_checklstring(L, 2, &l);
	VALIDATE(l == 32, null_str);
	const char* iv = luaL_checklstring(L, 3, &l);
	VALIDATE(l == AES_BLOCK_SIZE, null_str);
	vdata* in = static_cast<vdata *>(lua_touserdata(L, 4));

	vdata* out = luaW_pushvrwdata(L, nposm);
	if (encrypt) {
		utils::aes256_encrypt((const uint8_t*)key, (const uint8_t*)iv, in->data_ptr(), in->data_vsize(), out->data_ptr()); 
	} else {
		utils::aes256_decrypt((const uint8_t*)key, (const uint8_t*)iv, in->data_ptr(), in->data_vsize(), out->data_ptr()); 
	}
	out->set_data_vsize(in->data_vsize());
	
	return 1;
}

static int intf_aes_encrypt(lua_State* L)
{
	return intf_aes_internal(L, true);
}

static int intf_aes_decrypt(lua_State* L)
{
	return intf_aes_internal(L, false);
}

static int intf_get_rendered_text(lua_State* L)
{
	const char* text = luaL_checkstring(L, 1);
	int maximum_width = luaL_checkinteger(L, 2);
	int font_size = luaL_checkinteger(L, 3);
	int color = luaL_checkinteger(L, 4);
	vsurface* v = static_cast<vsurface *>(lua_touserdata(L, 5));
	VALIDATE(maximum_width >= 0 && font_size > 0 && v != nullptr, null_str);

	if (text[0] != '\0') {
		surface surf = font::get_rendered_text(text, maximum_width, font_size, uint32_to_color(color));
		v->reset(&surf);
	} else {
		v->reset(nullptr);
	}
	return 0;
}

static int intf_render_surface(lua_State* L)
{
	uint64_t renderer = luaL_checkinteger(L, 1);
	vsurface* v = static_cast<vsurface *>(lua_touserdata(L, 2));
	SDL_Rect src, dst;
	SDL_Rect* srcrect = nullptr;
	SDL_Rect* dstrect = nullptr;
	if (lua_type(L, 3) == LUA_TNUMBER) {
		uint64_t u64n = lua_tointeger(L, 3);
		src = lua_unpack_rect(u64n);
		srcrect = &src;

	}
	if (lua_type(L, 4) == LUA_TNUMBER) {
		uint64_t u64n = lua_tointeger(L, 4);
		dst = lua_unpack_rect(u64n);
		dstrect = &dst;

	}

	render_surface(get_renderer(), v->get(), srcrect, dstrect);

	return 0;
}

static int intf_SDL_GetTicks(lua_State* L)
{
	lua_pushinteger(L, SDL_GetTicks());
	return 1;
}

static int intf_SDL_RenderCopy(lua_State* L)
{
	uint64_t renderer = luaL_checkinteger(L, 1);
	vtexture* v = static_cast<vtexture *>(lua_touserdata(L, 2));
	SDL_Rect src, dst;
	SDL_Rect* srcrect = nullptr;
	SDL_Rect* dstrect = nullptr;
	if (lua_type(L, 3) == LUA_TNUMBER) {
		uint64_t u64n = lua_tointeger(L, 3);
		src = lua_unpack_rect(u64n);
		srcrect = &src;

	}
	if (lua_type(L, 4) == LUA_TNUMBER) {
		uint64_t u64n = lua_tointeger(L, 4);
		dst = lua_unpack_rect(u64n);
		dstrect = &dst;

	}

	SDL_RenderCopy(get_renderer(), v->get().get(), srcrect, dstrect);
	return 0;
}

static int intf_SDL_GetTtyUSB(lua_State* L)
{
	SDL_ttyUSB* ttyUSBs;
	int count = instance->sdl_GetTtyUSB(&ttyUSBs);
	// if (count == 0) {
	//	return 0;
	// }

	tstack_size_lock lock(L, 1);

	lua_createtable(L, count, 0);

	for (int at = 0; at < count; at ++) {
		const SDL_ttyUSB& tty = ttyUSBs[at];
		// if (tty.node[0] == '\0' || tty.path[0] == '\0') {
		//	continue;
		// }
		lua_createtable(L, 5, 0);

		lua_pushstring(L, tty.dev_node);
		lua_setfield(L, -2, "dev_node");
		lua_pushstring(L, tty.name);
		lua_setfield(L, -2, "name");
		lua_pushinteger(L, tty.pid);
		lua_setfield(L, -2, "pid");
		lua_pushinteger(L, tty.vid);
		lua_setfield(L, -2, "vid");

		lua_pushstring(L, tty.path);
		lua_setfield(L, -2, "path");
		
		lua_rawseti(L, -2, at + 1);
	}

	if (count != 0) {
		SDL_free(ttyUSBs);
	}
	return 1;
}

static int intf_SDL_IsFile(lua_State* L)
{
	const char* file = luaL_checkstring(L, 1);
	SDL_bool ret = SDL_IsFile(file);
	lua_pushboolean(L, ret);
	return 1;
}

static int intf_SDL_IsDirectory(lua_State* L)
{
	const char* file = luaL_checkstring(L, 1);
	SDL_bool ret = SDL_IsDirectory(file);
	lua_pushboolean(L, ret);
	return 1;
}

static int intf_ble_uuid_equal(lua_State* L)
{
	const char* uuid1 = luaL_checkstring(L, 1);
	const char* uudi2 = luaL_checkstring(L, 2);

	SDL_bool ret = SDL_BleUuidEqual(uuid1, uudi2); 
	lua_pushboolean(L, ret? 1: 0);
	return 1;
}

static int intf_aplt_task_log(lua_State* L)
{
	const char* str = luaL_checkstring(L, 1);
	SDL_Log("{lua}[%u]%s", SDL_GetTicks(), str);
	instance->bg_task().add_log2(time(nullptr), str, 0, false);
	return 0;
}

static int intf_set_luafunc_finished(lua_State* L)
{
	const char* str = luaL_optstring(L, 1, "");
	if (str[0] != '\0') {
		instance->bg_task().add_log2(time(nullptr), str, 0, false);
	}
	instance->bg_task().set_luafunc_finished();
	return 0;
}

static int intf_set_task_var(lua_State* L)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(instance->bg_task().is_ing(), null_str);

	const char* var_name = luaL_checkstring(L, 1);
	std::pair<std::string, std::string> pair = utils::split_app_prefix_id(var_name);
	if (pair.first.empty() || pair.second.empty()) {
		return luaL_argerror(L, 1, "Variable name's format is invalid");
	}

	bool is_array = luaL_checkboolean(L, 2);

	config::attribute_value val;
	if (!luaW_toscalar(L, 3, false, val)) {
		return luaL_argerror(L, 3, "Variable value's format is invalid");
	}
	aplt::tbg_task::tbase_bg_task2& sys_task = instance->bg_task().mutable_bg_task2();
	sys_task.set_task_var(var_name, is_array, val);

	return 0;
}

static int intf_erase_task_var(lua_State* L)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(instance->bg_task().is_ing(), null_str);

	const char* var_name = luaL_checkstring(L, 1);
	std::pair<std::string, std::string> pair = utils::split_app_prefix_id(var_name);
	if (pair.first.empty() || pair.second.empty()) {
		return luaL_argerror(L, 1, "Variable name's format is invalid");
	}

	aplt::tbg_task::tbase_bg_task2& sys_task = instance->bg_task().mutable_bg_task2();
	sys_task.erase_task_var(var_name);

	return 0;
}

// End Callback implementations

// Template which allows to push member functions to the lua kernel base into lua as C functions, using a shim
typedef int (lua_kernel_base::*member_callback)(lua_State* L);

template <member_callback method>
int dispatch(lua_State* L) {
	return ((lua_kernel_base::get_lua_kernel<lua_kernel_base>(L)).*method)(L);
}

#include "lua/lobject.h"
static void test(lua_State* L)
{
	// stackDump(L, "test, pre");
	size_t s = sizeof(Udata);
	t_string *t = new(L) t_string;
	// stackDump(L, "test, post");
	t->t_string::~t_string();
}

bool lua_kernel_base::lua_closing = false;

// Ctor, initialization
lua_kernel_base::lua_kernel_base()
 : mState(luaL_newstate())
{
	get_lua_kernel_base_ptr(mState) = this;
	lua_State* L = mState;

	SDL_Log("Initializing %s...", my_name().c_str());

	// Open safe libraries.
	// Debug and OS are not, but most of their functions will be disabled below.
	SDL_Log("Adding standard libs...");

	static const luaL_Reg safe_libs[] {
		{ "",       luaopen_base   },
		{ "table",  luaopen_table  },
		{ "string", luaopen_string },
		{ "math",   luaopen_math   },
		{ "coroutine",   luaopen_coroutine   },
		{ "debug",  luaopen_debug  },
		{ "os",     luaopen_os     },
		{ "utf8",	luaopen_utf8   }, // added in Lua 5.3
		{ nullptr, nullptr }
	};
	for (luaL_Reg const* lib = safe_libs; lib->func; ++lib) {
		luaL_requiref(L, lib->name, lib->func, 1);
		lua_pop(L, 1);  /* remove lib */
	}
	VALIDATE(lua_gettop(L) == 0, null_str);

	// Disable functions from os which we don't want.
	lua_getglobal(L, "os");
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		lua_pop(L, 1);
		char const* function = lua_tostring(L, -1);
		if(strcmp(function, "clock") == 0 || strcmp(function, "date") == 0
			|| strcmp(function, "time") == 0 || strcmp(function, "difftime") == 0) continue;
		lua_pushnil(L);
		lua_setfield(L, -3, function);
	}
	lua_pop(L, 1);

	// Disable functions from debug which we don't want.
	lua_getglobal(L, "debug");
	lua_pushnil(L);
	while(lua_next(L, -2) != 0) {
		lua_pop(L, 1);
		char const* function = lua_tostring(L, -1);
		if(strcmp(function, "traceback") == 0 || strcmp(function, "getinfo") == 0) continue;	//traceback is needed for our error handler
		lua_pushnil(L);										//getinfo is needed for ilua strict mode
		lua_setfield(L, -3, function);
	}
	lua_pop(L, 1);
	VALIDATE(lua_gettop(L) == 0, null_str);

	SDL_Log("Redirecting/Delete/New _ENV.xxx function...");

	// Delete dofile and loadfile.
	// _ENV.dofile => nil
	lua_pushnil(L);
	lua_setglobal(L, "dofile");

	// _ENV.loadfile => nil
	lua_pushnil(L);
	lua_setglobal(L, "loadfile");

	// _ENV.print => nil
	// if want to log, use rose.log("")
	lua_pushnil(L);
	lua_setglobal(L, "print");

	// _ENV.load => intf_load
	lua_pushcfunction(L, intf_load);
	lua_setglobal(L, "load");

	// _ENV.load => intf_loadstring
	lua_pushcfunction(L, intf_loadstring);
	lua_setglobal(L, "loadstring");
	VALIDATE(lua_gettop(L) == 0, null_str);

	// Store the error handler.
	SDL_Log("Adding error handler...");

	push_error_handler(L);
	VALIDATE(lua_gettop(L) == 0, null_str);

	// Create the gettext metatable.
	lua_common::register_gettext_metatable(L);

	// Create the tstring metatable.
	lua_common::register_tstring_metatable(L);
	VALIDATE(lua_gettop(L) == 0, null_str);

	// Add some callback from the rose lib
	SDL_Log("Registering basic rose API...");

	static luaL_Reg const callbacks[] {
		// { "compare_versions",         &intf_compare_versions         		},
		{ "debug",                    &intf_wml_tostring    },
		{ "log",					  &intf_log             },
		{ "cpp_breakpoint",           &intf_breakpoint      },
		{ "os",                       &intf_os              },
		{ "gettext",				  &intf_gettext   		},
		{ "vgettext2",				  &intf_vgettext   		},
		{ "mk_integer",               &intf_mk_integer      },
		{ "integer_tostr",            &intf_integer_tostr   },
		{ "hex_string",               &intf_hex_string      },
		{ "split",                    &intf_split           },
		{ "normalize_path",           &intf_normalize_path  },
		{ "convert_string",           &intf_convert_string },
		{ "is_format",                &intf_is_format },
		{ "truncate_to_max_bytes",    &intf_truncate_to_max_bytes },
		{ "truncate_to_max_chars",    &intf_truncate_to_max_chars },
		{ "pack_rect",                &intf_pack_rect       },
		{ "unpack_rect",              &intf_unpack_rect     },
		{ "pack_point",               &intf_pack_point       },
		{ "unpack_point",             &intf_unpack_point     },
		{ "calculate_adaption_ratio_size", &intf_calculate_adaption_ratio_size},

		{ "textdomain",               &lua_common::intf_textdomain   		},
		{ "tovconfig",                &lua_common::intf_tovconfig		},
		{ "tovhttp_agent",            &intf_tovhttp_agent		},
		{ "tovtcpcmgr",               &intf_tovtcpcmgr		},
		{ "tovdata",                  &intf_tovdata		},
		{ "tovlipdp",                 &intf_tovlipdp	},
		{ "tovprotobuf",              &intf_tovprotobuf },
		{ "tovsurface",               &intf_tovsurface },
		{ "tovtexture",               &intf_tovtexture },
		{ "tovble",                   &intf_tovble },
		{ "write_qrcode",             &intf_write_qrcode },
		{ "aes_encrypt",	          &intf_aes_encrypt },
		{ "aes_decrypt",	          &intf_aes_decrypt },
		{ "get_rendered_text",        &intf_get_rendered_text },
		{ "render_surface",           &intf_render_surface },
		{ "SDL_GetTicks",             &intf_SDL_GetTicks   },
		{ "SDL_RenderCopy",           &intf_SDL_RenderCopy },
		{ "SDL_GetTtyUSB",            &intf_SDL_GetTtyUSB },
		{ "SDL_IsFile", 		      &intf_SDL_IsFile},
		{ "SDL_IsDirectory", 	      &intf_SDL_IsDirectory},

		{ "ble_uuid_equal",           &intf_ble_uuid_equal },
		{ "aplt_task_log",            &intf_aplt_task_log },
		{ "set_luafunc_finished",     &intf_set_luafunc_finished },
		{ "set_task_var",             &intf_set_task_var },
		{ "erase_task_var",           &intf_erase_task_var },

		{ "have_file",                &lua_fileops::intf_have_file          },
		{ "dofile",                   &dispatch<&lua_kernel_base::intf_dofile>           },
		{ "require",                  &dispatch<&lua_kernel_base::intf_require>          },
		{ "kernel_type",              &dispatch<&lua_kernel_base::intf_kernel_type>          },

		{ "show_lua_console",	      &dispatch<&lua_kernel_base::intf_show_lua_console> },

		{ "get_time_stamp",           &intf_get_time_stamp},
		{ "format",                   &intf_format},

		{ "webrtc_post",              &intf_webrtc_post},
		{ "open_url",                 &intf_open_url},

		{ "wkoscript_list_files_metadata",	&intf_wkoscript_list_files_metadata},
		//
		// aplt
		//
		{ nullptr, nullptr }
	};

	lua_getglobal(L, "rose");
	VALIDATE(lua_isnil(L, -1), "*.lua must not define table: rose");
	// this nil is pushed
	lua_pop(L, 1); // pop nil

	lua_newtable(L);
	luaL_setfuncs(L, callbacks, 0);
	lua_setglobal(L, "rose");
	VALIDATE(lua_gettop(L) == 0, null_str);

	register_rose_const_value();
	VALIDATE(lua_gettop(L) == 0, null_str);

	SDL_Log("Initializing package repository...");
	// Create the package table.
	lua_getglobal(L, "rose");
	lua_newtable(L);
	lua_setfield(L, -2, "package");
	lua_pop(L, 1);
	VALIDATE(lua_gettop(L) == 0, null_str);

	lua_pushstring(L, "lua/package.lua");
	int res = intf_require(L);
	VALIDATE(res == 1, "Error: Failed to load lua/package.lua");

	VALIDATE(lua_gettop(L) == 0, null_str);
}

lua_kernel_base::~lua_kernel_base()
{
	lua_closing = true;
	lua_close(mState);
}

void lua_kernel_base::register_rose_const_value()
{
	lua_State* L = mState;

	tstack_size_lock lock(L, 0);

	const char* rosekKey = "rosek";
	lua_getglobal(L, rosekKey);
	VALIDATE(lua_isnil(L, -1), "*.lua must not define table: rosek");
	// this nil is pushed
	lua_pop(L, 1); // pop nil

	lua_newtable(L);
	VALIDATE(lua_gettop(L) == 1, null_str);

	// lua_getglobal(L, "rose");

	std::map<std::string, int> values;
	// os
	values.insert(std::make_pair("os_windows", os_windows));
	values.insert(std::make_pair("os_ios", os_ios));
	values.insert(std::make_pair("os_android", os_android));

	// STRingChangeType
	values.insert(std::make_pair("STRCVT_EXTRACT_DIRECTORY", STRCVT_EXTRACT_DIRECTORY));
	values.insert(std::make_pair("STRCVT_STRIP", STRCVT_STRIP));

	values.insert(std::make_pair("FMT_UUID", FMT_UUID));
	values.insert(std::make_pair("FMT_IPV4", FMT_IPV4));
	values.insert(std::make_pair("FMT_TIME", FMT_TIME));
	values.insert(std::make_pair("FMT_TIME_HHcMMcSS", FMT_TIME_HHcMMcSS));
	values.insert(std::make_pair("FMT_ELAPSE", FMT_ELAPSE));
	values.insert(std::make_pair("FMT_APLT_IOT_ALIAS_ID", FMT_APLT_IOT_ALIAS_ID));

	// path type
	values.insert(std::make_pair("PATHTYPE_ABS", PATHTYPE_ABS));
	values.insert(std::make_pair("PATHTYPE_RES", PATHTYPE_RES));
	values.insert(std::make_pair("PATHTYPE_USERDATA", PATHTYPE_USERDATA));

	values.insert(std::make_pair("SPLIT_REMOVE_EMPTY", utils::REMOVE_EMPTY));
	values.insert(std::make_pair("SPLIT_STRIP_SPACES", utils::STRIP_SPACES));

	values.insert(std::make_pair("AES128", AES128));
	values.insert(std::make_pair("AES192", AES192));
	values.insert(std::make_pair("AES256", AES256));

	values.insert(std::make_pair("lipdp_tn8", lipdp_tn8));
	values.insert(std::make_pair("lipdp_tn32", lipdp_tn32));
	values.insert(std::make_pair("lipdp_tn64", lipdp_tn64));
	values.insert(std::make_pair("lipdp_tstring", lipdp_tstring));
	values.insert(std::make_pair("lipdp_tbinary", lipdp_tbinary));
	values.insert(std::make_pair("lipdp_tip", lipdp_tip));
	values.insert(std::make_pair("lipdp_thexstring", lipdp_thexstring));

	values.insert(std::make_pair("AF_ERR_VER", LEAGOR_BLE_AF_ERR_VER));
	values.insert(std::make_pair("AF_ERR_PRIVACY", LEAGOR_BLE_AF_ERR_PRIVACY));
	values.insert(std::make_pair("AF_INET", LEAGOR_BLE_AF_INET));
	values.insert(std::make_pair("AF_INET6", LEAGOR_BLE_AF_INET6));

	// font size
	values.insert(std::make_pair("FONT_SIZE_DEFAULT", font::SIZE_DEFAULT));

	// color
	values.insert(std::make_pair("GRAY_COLOR", color_to_uint32(font::GRAY_COLOR)));

	// protobuf
	values.insert(std::make_pair("PBIDX_N32TOP0", PBIDX_N32TOP0));
	values.insert(std::make_pair("PBIDX_N32TOP1", PBIDX_N32TOP1));
	values.insert(std::make_pair("PBIDX_N32TOP2", PBIDX_N32TOP2));
	values.insert(std::make_pair("PBIDX_N64TOP0", PBIDX_N64TOP0));
	values.insert(std::make_pair("PBIDX_N64TOP1", PBIDX_N64TOP1));
	values.insert(std::make_pair("PBIDX_N64TOP2", PBIDX_N64TOP2));
	values.insert(std::make_pair("PBIDX_STRTOP0", PBIDX_STRTOP0));
	values.insert(std::make_pair("PBIDX_STRTOP1", PBIDX_STRTOP1));
	values.insert(std::make_pair("PBIDX_STRTOP2", PBIDX_STRTOP2));
	values.insert(std::make_pair("PBIDX_N32ITEM0", PBIDX_N32ITEM0));
	values.insert(std::make_pair("PBIDX_N32ITEM1", PBIDX_N32ITEM1));
	values.insert(std::make_pair("PBIDX_N32ITEM2", PBIDX_N32ITEM2));
	values.insert(std::make_pair("PBIDX_N32ITEM3", PBIDX_N32ITEM3));
	values.insert(std::make_pair("PBIDX_N32ITEM4", PBIDX_N32ITEM4));
	values.insert(std::make_pair("PBIDX_N32ITEM5", PBIDX_N32ITEM5));
	values.insert(std::make_pair("PBIDX_N32ITEM6", PBIDX_N32ITEM6));
	values.insert(std::make_pair("PBIDX_N32ITEM7", PBIDX_N32ITEM7));
	values.insert(std::make_pair("PBIDX_N32ITEM8", PBIDX_N32ITEM8));
	values.insert(std::make_pair("PBIDX_N32ITEM9", PBIDX_N32ITEM9));
	values.insert(std::make_pair("PBIDX_N64ITEM0", PBIDX_N64ITEM0));
	values.insert(std::make_pair("PBIDX_N64ITEM1", PBIDX_N64ITEM1));
	values.insert(std::make_pair("PBIDX_N64ITEM2", PBIDX_N64ITEM2));
	values.insert(std::make_pair("PBIDX_STRITEM0", PBIDX_STRITEM0));
	values.insert(std::make_pair("PBIDX_STRITEM1", PBIDX_STRITEM1));
	values.insert(std::make_pair("PBIDX_STRITEM2", PBIDX_STRITEM2));
	values.insert(std::make_pair("PBIDX_STRITEM3", PBIDX_STRITEM3));
	values.insert(std::make_pair("PBIDX_STRITEM4", PBIDX_STRITEM4));

	// net
	values.insert(std::make_pair("net_OK", net::OK));

	// lua
	values.insert(std::make_pair("LUA_TNIL", LUA_TNIL));
	values.insert(std::make_pair("LUA_TBOOLEAN", LUA_TBOOLEAN));
	values.insert(std::make_pair("LUA_TLIGHTUSERDATA", LUA_TLIGHTUSERDATA));
	// -- LUA_TNUMBER = 3,
	values.insert(std::make_pair("LUA_TSTRING", LUA_TSTRING));
	values.insert(std::make_pair("LUA_TTABLE", LUA_TTABLE));
	values.insert(std::make_pair("LUA_TFUNCTION", LUA_TFUNCTION));
	values.insert(std::make_pair("LUA_TUSERDATA", LUA_TUSERDATA));
	values.insert(std::make_pair("LUA_TTHREAD", LUA_TTHREAD));
	values.insert(std::make_pair("LUA_VNUMINT", LUA_VNUMINT)); // 0x3
	values.insert(std::make_pair("LUA_VNUMFLT", LUA_VNUMFLT)); // 0x13

	// ble
	values.insert(std::make_pair("ble_operator_write", tble::operator_write));
	values.insert(std::make_pair("ble_operator_notify", tble::operator_notify));

	values.insert(std::make_pair("ble_READ", SDL_BleCharacteristicPropertyRead));
	values.insert(std::make_pair("ble_WRITE", SDL_BleCharacteristicPropertyWrite));
	values.insert(std::make_pair("ble_NOTIFY", SDL_BleCharacteristicPropertyNotify));
	values.insert(std::make_pair("ble_INDICATE", SDL_BleCharacteristicPropertyIndicate));

	values.insert(std::make_pair("ble_devicetype_classic", SDL_BleDeviceTypeClassic));
	values.insert(std::make_pair("ble_devicetype_le", SDL_BleDeviceTypeLE));
	values.insert(std::make_pair("ble_devicetype_dual", SDL_BleDeviceTypeDual));

	// applet
	values.insert(std::make_pair("cpp_id_aplt_min", aplt::cpp_id_aplt_min));

	for (std::map<std::string, int>::const_iterator it = values.begin(); it != values.end(); ++ it) {
		lua_pushinteger(L, it->second);
		lua_setfield(L, -2, it->first.c_str());
	}
	lua_setglobal(L, rosekKey);
	// lua_pop(L, 1);
}

void lua_kernel_base::log_error(char const * msg, char const * context)
{
	SDL_Log("%s: %s", context, msg);
}

void lua_kernel_base::throw_exception(char const * msg, char const * context)
{
	VALIDATE(false, null_str);
	// throw game::lua_error(msg, context);
}

bool lua_kernel_base::protected_call(int nArgs, int nRets)
{
	error_handler eh = std::bind(&lua_kernel_base::log_error, this, _1, _2 );
	return this->protected_call(nArgs, nRets, eh);
}

bool lua_kernel_base::load_string(char const * prog)
{
	return false;
/*
	error_handler eh = std::bind(&lua_kernel_base::log_error, this, _1, _2 );
	return this->load_string(prog, eh);
*/
}

bool lua_kernel_base::protected_call(int nArgs, int nRets, error_handler e_h)
{
	return this->protected_call(mState, nArgs, nRets, e_h);
}

bool lua_kernel_base::protected_call(lua_State * L, int nArgs, int nRets, error_handler e_h)
{
	int errcode = luaW_pcall_internal(L, nArgs, nRets);

	if (errcode != LUA_OK) {
		char const * msg = lua_tostring(L, -1);

		std::string context = "When executing, ";
		if (errcode == LUA_ERRRUN) {
			context += "Lua runtime error: ";
		} else if (errcode == LUA_ERRERR) {
			context += "Lua error in attached debugger: ";
		} else if (errcode == LUA_ERRMEM) {
			context += "Lua out of memory error: ";
		} else if (errcode == LUA_ERRERR) {
			context += "Lua error in garbage collection metamethod: ";
		} else {
			context += "unknown lua error: ";
		}
		if(lua_isstring(L, -1)) {
			context +=  msg ? msg : "null string";
		} else {
			context += lua_typename(L, lua_type(L, -1));
		}

		lua_pop(L, 1);

		e_h(context.c_str(), "Lua Error");

		return false;
	}
	return true;
}

bool lua_kernel_base::load_string(char const * prog, error_handler e_h)
{

	// pass 't' to prevent loading bytecode which is unsafe and can be used to escape the sandbox.
	// todo: maybe allow a 'name' parameter to give better error messages.
	// int errcode = luaL_loadbufferx(mState, prog, strlen(prog), /*name*/ prog, "t");
/*	if (errcode != LUA_OK) {
		char const * msg = lua_tostring(mState, -1);
		std::string message = msg ? msg : "null string";

		std::string context = "When parsing a string to lua, ";

		if (errcode == LUA_ERRSYNTAX) {
			context += " a syntax error";
		} else if(errcode == LUA_ERRMEM){
			context += " a memory error";
		} else if(errcode == LUA_ERRGCMM) {
			context += " an error in garbage collection metamethod";
		} else {
			context += " an unknown error";
		}

		lua_pop(mState, 1);

		e_h(message.c_str(), context.c_str());

		return false;
	}
*/
	return true;
}

void lua_kernel_base::run_lua_tag(const config& cfg)
{
/*
	int nArgs = 0;
	if (const config& args = cfg.child("args")) {
		luaW_pushconfig(this->mState, args);
		++nArgs;
	}
	this->run(cfg["code"].str().c_str(), nArgs);
*/
}
// Call load_string and protected call. Make them throw exceptions.
//
void lua_kernel_base::throwing_run(const char * prog, int nArgs)
{
/*
	cmd_log_ << "$ " << prog << "\n";
	error_handler eh = std::bind(&lua_kernel_base::throw_exception, this, _1, _2 );
	this->load_string(prog, eh);
	lua_insert(mState, -nArgs - 1);
	this->protected_call(nArgs, 0, eh);
*/
}

// Do a throwing run, but if we catch a lua_error, reformat it with signature for this function and log it.
void lua_kernel_base::run(const char * prog, int nArgs)
{
/*
	try {
		this->throwing_run(prog, nArgs);
	} catch (const game::lua_error & e) {
		cmd_log_ << e.what() << "\n";
		lua_kernel_base::log_error(e.what(), "In function lua_kernel::run()");
	}
*/
}

// Tests if a program resolves to an expression, and pretty prints it if it is, otherwise it runs it normally. Throws exceptions.
void lua_kernel_base::interactive_run(char const * prog) {
	std::string experiment = "ilua._pretty_print(";
	experiment += prog;
	experiment += ")";
/*
	error_handler eh = std::bind(&lua_kernel_base::throw_exception, this, _1, _2 );

	try {
		// Try to load the experiment without syntax errors
		this->load_string(experiment.c_str(), eh);
	} catch (const game::lua_error &) {
		this->throwing_run(prog, 0);	// Since it failed, fall back to the usual throwing_run, on the original input.
		return;
	}
	// experiment succeeded, now run but log normally.
	cmd_log_ << "$ " << prog << "\n";
	this->protected_call(0, 0, eh);
*/
}
/**
 * Loads and executes a Lua file.
 * - Arg 1: string containing the file name.
 * - Ret *: values returned by executing the file body.
 */
int lua_kernel_base::intf_dofile(lua_State* L)
{
	luaL_checkstring(L, 1);
	lua_rotate(L, 1, -1);
	if (lua_fileops::load_file(L) != 1) return 0;
	//^ should end with the file contents loaded on the stack. actually it will call lua_error otherwise, the return 0 is redundant.
	lua_rotate(L, 1, 1);
	// Using a non-protected call here appears to fix an issue in plugins.
	// The protected call isn't technically necessary anyway, because this function is called from Lua code,
	// which should already be in a protected environment.
	lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);
	return lua_gettop(L);
}

/**
 * Loads and executes a Lua file, if there is no corresponding entry in rose.package.
 * Stores the result of the script in rose.package and returns it.
 * - Arg 1: string containing the file name.
 * - Ret 1: value returned by the script.
 */
int lua_kernel_base::intf_require(lua_State* L)
{
	tstack_size_lock lock(L, -1);
	const char * m = luaL_checkstring(L, 1);
	if (!m) {
		return luaL_argerror(L, 1, "found a null string argument to rose require");
	}

	// Check if there is already an entry.

	lua_getglobal(L, "rose");

	lua_pushstring(L, "package");

	lua_rawget(L, -2);

	lua_pushvalue(L, 1);

	lua_rawget(L, -2);
	if (!lua_isnil(L, -1)) {
		return 1;
	}

	lua_pop(L, 1);

	lua_pushvalue(L, 1);
	// stack is now [packagename] [rose] [package] [packagename]

	if(lua_fileops::load_file(L) != 1) {
		// should end with the file contents loaded on the stack. actually it will call lua_error otherwise, the return 0 is redundant.
		// stack is now [packagename] [rose] [package] [chunk]
		return 0;
	}

	SDL_Log("require: loaded a file, now calling it");

	if (!this->protected_call(L, 0, 1, std::bind(&lua_kernel_base::log_error, this, _1, _2))) {
		// historically if rose.require fails it just yields nil and some logging messages, not a lua error
		return 0;
	}
	// stack is now [packagename] [rose] [package] [results]

	lua_pushvalue(L, 1);
	lua_pushvalue(L, -2);
	// stack is now [packagename] [rose] [package] [results] [packagename] [results]
	// Add the return value to the table.

	lua_settable(L, -4);

	// stack is now [string:packagename] [table:rose] [table:package] [results]
	VALIDATE(lua_gettop(L) == 4, null_str);
	lua_pop(L, 4);

	return 1;
}
int lua_kernel_base::intf_kernel_type(lua_State* L)
{
/*
	lua_push(L, my_name());
*/
	return 1;
}
int lua_kernel_base::impl_game_config_get(lua_State* L)
{
/*
	char const *m = luaL_checkstring(L, 2);
	return_int_attrib("base_income", game_config::base_income);
	return_int_attrib("village_income", game_config::village_income);
	return_int_attrib("village_support", game_config::village_support);
	return_int_attrib("poison_amount", game_config::poison_amount);
	return_int_attrib("rest_heal_amount", game_config::rest_heal_amount);
	return_int_attrib("recall_cost", game_config::recall_cost);
	return_int_attrib("kill_experience", game_config::kill_experience);
	return_string_attrib("version", game_config::version);
	return_bool_attrib("debug", game_config::debug);
	return_bool_attrib("debug_lua", game_config::debug_lua);
	return_bool_attrib("mp_debug", game_config::mp_debug);
*/
	return 0;
}
int lua_kernel_base::impl_game_config_set(lua_State* L)
{
	std::string err_msg = "unknown modifiable property of game_config: ";
	err_msg += luaL_checkstring(L, 2);
	return luaL_argerror(L, 2, err_msg.c_str());
}
/**
 * Loads the "package" package into the Lua environment.
 * This action is inherently unsafe, as Lua scripts will now be able to
 * load C libraries on their own, hence granting them the same privileges
 * as the rose binary itself.
 */
void lua_kernel_base::load_package()
{
	lua_State* L = mState;
	lua_pushcfunction(L, luaopen_package);
	lua_pushstring(L, "package");
	lua_call(L, 1, 0);
}

void lua_kernel_base::load_core()
{
	lua_State* L = mState;
	lua_settop(L, 0);
	cmd_log_ << "Loading core...\n";
	luaW_getglobal(L, "rose", "require");
	lua_pushstring(L, "lua/core.lua");
	if(!protected_call(1, 1)) {
		cmd_log_ << "Error: Failed to load core.\n";
	}
	lua_settop(L, 0);
}

void lua_kernel_base::load_lua(const std::string& lua)
{
	VALIDATE(lua.find("lua/") == 0, null_str);

	lua_State* L = mState;

	lua_settop(L, 0);
	luaW_getglobal(L, "rose", "require");
	lua_pushstring(L, lua.c_str());
	if(!protected_call(1, 1)) {
		cmd_log_ << "Error: Failed to load core.\n";
	}
	lua_settop(L, 0);
}

/**
 * Gets all the global variable names in the Lua environment. This is useful for tab completion.
 */
std::vector<std::string> lua_kernel_base::get_global_var_names()
{
	std::vector<std::string> ret;

	lua_State* L = mState;

	int idx = lua_gettop(L);
	lua_getglobal(L, "_G");
	lua_pushnil(L);

	while (lua_next(L, idx+1) != 0) {
		if (lua_isstring(L, -2)) {
			ret.push_back(lua_tostring(L,-2));
		}
		lua_pop(L,1);
	}
	lua_settop(L, idx);
	return ret;
}

/**
 * Gets all attribute names of an extended variable name. This is useful for tab completion.
 */
std::vector<std::string> lua_kernel_base::get_attribute_names(const std::string & input)
{
	std::vector<std::string> ret;
	std::string base_path = input;
	size_t last_dot = base_path.find_last_of('.');
	std::string partial_name = base_path.substr(last_dot + 1);
	base_path.erase(last_dot);
	std::string load = "return " + base_path;

	lua_State* L = mState;
	int save_stack = lua_gettop(L);
	int result = luaL_loadstring(L, load.c_str());
	if(result != LUA_OK) {
		// This isn't at error level because it's a really low priority error; it just means the user tried to tab-complete something that doesn't exist.
		SDL_Log("Error when attempting tab completion:");
		SDL_Log("%s", luaL_checkstring(L, -1));
		// Just return an empty list; no matches were found
		lua_settop(L, save_stack);
		return ret;
	}
/*
	luaW_pcall(L, 0, 1);
	if(lua_istable(L, -1) || lua_isuserdata(L, -1)) {
		int top = lua_gettop(L);
		int obj = lua_absindex(L, -1);
		if(luaL_getmetafield(L, obj, "__tab_enum") == LUA_TFUNCTION) {
			lua_pushvalue(L, obj);
			lua_pushlstring(L, partial_name.c_str(), partial_name.size());
			luaW_pcall(L, 2, 1);
			ret = lua_check<std::vector<std::string>>(L, -1);
		} else if(lua_type(L, -1) != LUA_TTABLE) {
			SDL_Log("Userdata missing __tab_enum meta-function for tab completion");
			lua_settop(L, save_stack);
			return ret;
		} else {
			lua_settop(L, top);
			// Metafunction not found, so use lua_next to enumerate the table
			for(lua_pushnil(L); lua_next(L, obj); lua_pop(L, 1)) {
				if(lua_type(L, -2) == LUA_TSTRING) {
					std::string attr = lua_tostring(L, -2);
					if(attr.empty()) {
						continue;
					}
					if(!isalpha(attr[0]) && attr[0] != '_') {
						continue;
					}
					if(std::any_of(attr.begin(), attr.end(), [](char c){
						return !isalpha(c) && !isdigit(c) && c != '_';
					})) {
						continue;
					}
					if(attr.substr(0, partial_name.size()) == partial_name) {
						ret.push_back(base_path + "." + attr);
					}
				}
			}
		}
	}
*/
	lua_settop(L, save_stack);
	return ret;
}

lua_kernel_base*& lua_kernel_base::get_lua_kernel_base_ptr(lua_State* L)
{
	#ifdef __GNUC__
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wold-style-cast"
	#endif
	return *reinterpret_cast<lua_kernel_base**>(lua_getextraspace(L));
	#ifdef __GNUC__
		#pragma GCC diagnostic pop
	#endif
}

uint32_t lua_kernel_base::get_random_seed()
{
	return 0;
	// return seed_rng::next_seed();
}

void lua_kernel_base::call_lua_breakpoint()
{
	lua_State* L = mState;
	tstack_size_lock lock(L, 0);

	luaW_getglobal(L, "rose", "lua_breakpoint");
	protected_call(0, 0);
}

extern const TValue* lua_toTValue (lua_State* L, int idx);
void luaW_dump_stack(lua_State* L, const std::string& scene)
{
	SDL_Log("-----dump_stack, remark--------------------");
	// return;

	int i;
	int top = lua_gettop(L);
	SDL_Log("---%s, stack's size %i---", scene.c_str(), top);
	for (i = 1; i <= top; i ++) {
		int t = lua_type(L, i);
		// char buf[32];
		switch(t) {
		case LUA_TSTRING:
			SDL_Log("[#%i]{%p}TSTRING, '%s'", i, lua_toTValue(L, i), lua_tostring(L, i));
			break;
		case LUA_TBOOLEAN:
			SDL_Log("[#%i]{%p}TBOOLEAN, %s", i, lua_toTValue(L, i), lua_toboolean(L, i)? "true": "false");
			break;
		case LUA_TNUMBER:
			if (lua_isinteger(L, i)) {
				SDL_Log("[#%i]{%p}TNUMBER.integer, %lld", i, lua_toTValue(L, i), lua_tointeger(L, i));
			} else {
				SDL_Log("[#%i]{%p}TNUMBER.float, %7.2f", i, lua_toTValue(L, i), lua_tonumber(L, i));
			}
			break;
		case LUA_TFUNCTION:
			SDL_Log("[#%i]{%p}function", i, lua_toTValue(L, i));
			break;
		default:
			SDL_Log("[#%i]{%p}%s", i, lua_toTValue(L, t), lua_typename(L, t));
			break;
		}
	}
	SDL_Log("-------------------------");
}
