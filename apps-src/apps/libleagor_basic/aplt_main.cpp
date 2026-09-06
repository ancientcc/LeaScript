#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "aplt_common.hpp"
#include "rose_exception.hpp"
#include "aplt2.hpp"
#include <rose_ros/aplt.hpp>
#include "task/nonblock_task.hpp"
#include "task/aiagent_task.hpp"

namespace aplt {
aplt::tapplet* curr_aplt = nullptr;

tlua_nonblock* lua_nonblock = nullptr;
tlua_aiagent* lua_aiagent = nullptr;

static int impl_cpp_lua_nonblock(lua_State* L)
{
	// twidget* widget = *static_cast<twidget **>(luaL_checkudata(L, 1, vwidgetMetatableKey));
	// char const* id = luaL_checkstring(L, 2);
	// bool must_be_active = luaW_toboolean(L, 3);
	// bool must_exist = luaW_toboolean(L, 4);

	// twidget* result = find_widget<twidget>(widget, id, must_be_active, must_exist);
	// if (result != nullptr) {
	//	luaW_pushvwidget(L, *result);
	// } else {
	//	lua_pushnil(L);
	// }
	VALIDATE(lua_nonblock != nullptr, null_str);
	luaW_pushvnonblock2(L, *lua_nonblock);
	return 1;
}

static int impl_cpp_lua_aiagent(lua_State* L)
{
	// twidget* widget = *static_cast<twidget **>(luaL_checkudata(L, 1, vwidgetMetatableKey));
	// char const* id = luaL_checkstring(L, 2);
	// bool must_be_active = luaW_toboolean(L, 3);
	// bool must_exist = luaW_toboolean(L, 4);

	// twidget* result = find_widget<twidget>(widget, id, must_be_active, must_exist);
	// if (result != nullptr) {
	//	luaW_pushvwidget(L, *result);
	// } else {
	//	lua_pushnil(L);
	// }
	VALIDATE(lua_aiagent != nullptr, null_str);
	luaW_pushvaiagent(L, *lua_aiagent);
	return 1;
}

void register_cpp_metatable(lua_State* L, const tapplet& aplt)
{
	const std::string lua_bundleid = bundleid_2_lua_bundleid(aplt.bundleid);
	const std::string cppKey = utils::join_app_prefix_id(lua_bundleid, APLT_LUA_SHORT_CPP_KEY);
	const std::string cppkKey = utils::join_app_prefix_id(lua_bundleid, APLT_LUA_SHORT_CPPK_KEY);

	tstack_size_lock lock(L, 0);

	// lua_getglobal(L, gui2Key);
	lua_getglobal(L, cppKey.c_str());
	bool cppKey_use_lua_bundleid = false;
	if (!cppKey_use_lua_bundleid) {
		VALIDATE(lua_isnil(L, -1), "*.lua must not define table: gui2");
		// this nil is pushed
		lua_pop(L, 1); // pop nil

		lua_newtable(L);

	} else {
		VALIDATE(lua_istable(L, -1), "*.lua must define table: gui2");
	}

	// lua_getglobal(L, "gui2");
	// VALIDATE(lua_istable(L, -1), "must load lua/gui2.lua before call register_gui2_metatable");

	static luaL_Reg const callbacks[] {
		{ "cpp_nonblock", 	    &impl_cpp_lua_nonblock},
		{ "cpp_aiagent", 	    &impl_cpp_lua_aiagent},
		// { "show_message",       &impl_gui2_show_message},
		// { "show_messagefs",     &impl_gui2_show_messagefs},
		// { "show_menu",          &impl_gui2_show_menu},
		{ nullptr, nullptr }
	};
	luaL_setfuncs(L, callbacks, 0);
	if (!cppKey_use_lua_bundleid) {
		lua_setglobal(L, cppKey.c_str());
	} else {
		lua_pop(L, 1); // pop nil
	}
	VALIDATE(lua_gettop(L) == 0, null_str);

	// gui2k
	// const char* gui2kKey = "cppkKey";
	lua_getglobal(L, cppkKey.c_str());
	VALIDATE(lua_isnil(L, -1), "*.lua must not define table: gui2k");
	// this nil is pushed
	lua_pop(L, 1); // pop nil

	lua_newtable(L);
	VALIDATE(lua_gettop(L) == 1, null_str);

	// lua_getglobal(L, gui2Key);

	std::map<std::string, int> values;
	// values.insert(std::make_pair("tmessage_auto_close", tmessage::auto_close));
	// values.insert(std::make_pair("tmessage_ok_button", tmessage::ok_button));
	// values.insert(std::make_pair("tmessage_yes_no_buttons", tmessage::yes_no_buttons));

	lua_setglobal(L, cppkKey.c_str());
	// lua_pop(L, 1);
}

}

void aplt_load(int src, const char* bundleid)
{
    aplt::tb_api& b_api = aplt::get_b_api();
    const std::map<aplt::taplt_key, aplt::tapplet>& applets = b_api.const_applets();

    const aplt::tapplet* aplt = aplt::aplt_from_id2(applets, src, bundleid);
    VALIDATE(aplt != nullptr, null_str);
    aplt::curr_aplt = const_cast<aplt::tapplet*>(aplt);

	aplt::register_cpp_metatable(b_api.get_lua_State(), *aplt);

	VALIDATE(aplt::lua_nonblock == nullptr, null_str);
	aplt::lua_nonblock = new aplt::tlua_nonblock;

	VALIDATE(aplt::lua_aiagent == nullptr, null_str);
	aplt::lua_aiagent = new aplt::tlua_aiagent;
}

void aplt_unload()
{
    VALIDATE(aplt::curr_aplt != nullptr, null_str);

	aplt::tb_api& b_api = aplt::get_b_api();

	// b_api.call_lua_breakpoint();
	nil_applet_cpp_tables(b_api.get_lua_State(), bundleid_2_lua_bundleid(aplt::curr_aplt->bundleid));
	// b_api.call_lua_breakpoint();

	delete aplt::lua_nonblock;
	aplt::lua_nonblock = nullptr;

	delete aplt::lua_aiagent;
	aplt::lua_aiagent = nullptr;

    aplt::curr_aplt = nullptr;
}