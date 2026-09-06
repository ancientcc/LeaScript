#define GETTEXT_DOMAIN "aplt_leagor_khome-lib"

#include "aplt_common.hpp"
#include "rose_exception.hpp"
#include "aplt2.hpp"
#include <rose_ros/aplt.hpp>

#include "rose_lua.hpp"

#include "block_task.hpp"
#include "camera_task.hpp"
#include "wkoscript.hpp"

namespace aplt {
aplt::tapplet* curr_aplt = nullptr;

tlua_block* lua_block = nullptr;
tlua_camera* lua_camera  = nullptr;

static int impl_cpp_block2(lua_State* L)
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
	VALIDATE(lua_block != nullptr, null_str);
	luaW_pushvblock2(L, *lua_block);
	return 1;
}

static int impl_cpp_camera2(lua_State* L)
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
	VALIDATE(lua_camera != nullptr, null_str);
	luaW_pushvcamera2(L, *lua_camera);
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
		{ "cpp_block2", 	    &impl_cpp_block2},
		{ "cpp_camera2", 	    &impl_cpp_camera2},
		// { "run_with_progress",  &impl_gui2_run_with_progress},
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
/*
	// <librose>/gui/widgets/widget.hpp
	values.insert(std::make_pair("tvisible_VISIBLE", 0));
	values.insert(std::make_pair("tvisible_HIDDEN", 1));
	values.insert(std::make_pair("tvisible_INVISIBLE", 2));

	// <librose>/gui/widgets/window.hpp
	values.insert(std::make_pair("twindow_OK", -1));
	values.insert(std::make_pair("twindow_CANCEL", -2));

	// <librose/gui/dialogs/dialog.hpp>
	values.insert(std::make_pair("tdialog_POST_MSG_MIN_APP", 100));

	// <librose/gui/widgets/widget.hpp>
	// tbase_tpl_widget type
	values.insert(std::make_pair("tpl_text_box2", 1));

	values.insert(std::make_pair("hdpi_scale", 2));
	// dialogs = {};

	for (std::map<std::string, int>::const_iterator it = values.begin(); it != values.end(); ++ it) {
		lua_pushinteger(L, it->second);
		lua_setfield(L, -2, it->first.c_str());
	}
*/
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

	VALIDATE(aplt::lua_block == nullptr && aplt::lua_camera == nullptr, null_str);
    aplt::lua_block = new aplt::tlua_block;

	aplt::register_cpp_metatable(b_api.get_lua_State(), *aplt);

	aplt::lua_camera = new aplt::tlua_camera;

	// std::string filename = aplt::curr_aplt->preferences_dir + "/saves/workout_shoulder_neck.cfg";
	std::string filename = aplt::curr_aplt->preferences_dir + "/saves/workout_pushup1.cfg";
	aplt::twkoscript script;
	// script.from_file(filename);
	std::string to_filename = aplt::curr_aplt->preferences_dir + "/saves/workout_export.cfg";
	// script.to_file(to_filename);

	// b_api.call_lua_breakpoint();
}

void aplt_unload()
{
	VALIDATE(aplt::curr_aplt != nullptr, null_str);

	aplt::tb_api& b_api = aplt::get_b_api();

	// b_api.call_lua_breakpoint();
	nil_applet_cpp_tables(b_api.get_lua_State(), bundleid_2_lua_bundleid(aplt::curr_aplt->bundleid));
	// b_api.call_lua_breakpoint();

    delete aplt::lua_block;
	aplt::lua_block = nullptr;

	delete aplt::lua_camera;
	aplt::lua_camera = nullptr;


    aplt::curr_aplt = nullptr;
}
