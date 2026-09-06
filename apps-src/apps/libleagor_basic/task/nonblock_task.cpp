#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "rose_global.hpp"
#include "gettext.hpp"
#include "nonblock_task.hpp"
#include "rose_config_3rdparty.hpp"

#include "rose_ros/utils.hpp"

#include <SDL.h>
#include <SDL_log.h>
#include "aplt_clazz.hpp"

#include <rose_ros/aplt.hpp>
#include "base_slot.hpp"
#include "rose_net_api.hpp"

#include "common.hpp"

using namespace std::placeholders;

const char vnonblock2MetatableKey[] = "cpp.vnonblock2";


namespace aplt {

extern tai_slot* ai_slot_singleton;

//
// tdeepseek
//
tdeepseek2::tdeepseek2(tapplet& aplt, const tapplet::ttask& cfg_task, ttask_vars& task_vars, bool& finished)
	: thelper_nonblock_task_slot(*lua_nonblock, false, aplt, cfg_task, task_vars, finished)
	, var_name_ds_retbool_(utils::join_app_prefix_id(aplt_.bundleid, "ds_retbool"))
	, var_name_ds_answer_(utils::join_app_prefix_id(aplt_.bundleid, "ds_answer"))
	// , deepseek_(aplt_.preferences_dir)
{
	clear();

	output_var_keys_.push_back(var_name_ds_retbool_);
	output_var_keys_.push_back(var_name_ds_answer_);
}

std::string tdeepseek2::pre_start_task()
{
	// std::string err_msg;
	utils::string_map symbols;
	validate_nposm();

	if (ai_slot_singleton == nullptr) {
		return _("This applet is not set to current AI driver.");
	}

	// deepseek
	const std::string var_name_ds_question = utils::join_app_prefix_id(aplt_.bundleid, "ds_question");
	const std::string var_name_ds_image = utils::join_app_prefix_id(aplt_.bundleid, "ds_image");

	std::vector<std::string> var_names;
	var_names.push_back(var_name_ds_question);
	var_names.push_back(var_name_ds_image);

	std::string ds_question;
	std::string ds_image;

	std::string op;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = var_names.begin(); it != var_names.end(); ++ it, at ++) {
		const std::string& var_name = *it;
		symbols["var"] = var_name;
		if (task_vars_.existed(var_name) == 0) {
				if (var_name == var_name_ds_question) {
					return vgettext2("Missing input var: $var", symbols);
				} else {
					continue;
				}
		}

		const ttask_var& var = task_vars_.get_var(var_name);
		if (var.is_array) {
			return vgettext2("Input var($var) must be single var", symbols);
		}
		const int val_type = var.val.type();

		if (val_type != var_type_string) {
			symbols["type"] = var_types[var_type_string].name;
			return vgettext2("Input var($var)'s type must be $type", symbols);
		}

		const std::string var_val = var.val.str();
		if (var_name == var_name_ds_image) {

		} else if (var_val.empty()) {
			return vgettext2("Input var($var)'s cannot be empty", symbols);
		}

		if (var_name == var_name_ds_question) {
			ds_question = task_vars_.get_string(var_name);

		} else if (var_name == var_name_ds_image) {
			ds_image = task_vars_.get_string(var_name);
		}
	}

	std::string err_msg;
	// err_msg = block2->deepseek().xmit(ds_question, task_vars);

	// surface surf = image::rose_get_image(aplt.preferences_dir + "/owner-ydc1.jpg");
	// surface surf = image::rose_get_image(aplt.preferences_dir + "/owner-sc-1.jpg");
	// 
	surface surf;
	if (!ds_image.empty()) {
		surf = image::rose_get_image(ds_image);
		if (surf.get() == nullptr) {
			symbols["file"] = ds_image;
			err_msg = vgettext2("$file isn't a valid image file", symbols);
		}
	}
	if (err_msg.empty()) {
		question_ = ds_question;
		surf_= surf;

		// ai_slot_singleton->set_aplt_task_did_nlp_answer(std::bind(&tdeepseek2::did_nlp_answer, this, _1, _2, _3, _3));
		ai_slot_singleton->send_nlp_question(aplt::chatsrc_aplt_task, true, question_, surf_, 
			std::bind(&tdeepseek2::did_nlp_answer, this, _1, _2, _3, _3));
	}

	return err_msg;
}

void tdeepseek2::did_nlp_answer(bool retbool, const std::string& answer, int input_tokens, int output_tokens)
{
	retbool_ = retbool;
	answer_ = answer;

	finished_ = true;
}

void tdeepseek2::task_finished(const tapplet::ttask& cfg_task)
{
	VALIDATE_IN_MAIN_THREAD();

	if (ai_slot_singleton->is_nlp_questioning()) {
		ai_slot_singleton->stop_nlp_question();
	}

	task_vars_.insert_bool(var_name_ds_retbool_, false, retbool_);
	task_vars_.insert_string(var_name_ds_answer_, false, answer_);

	clear();
}


thelper_nonblock_task_slot* create_nonblock_task_slot(int code, tapplet& aplt, const tapplet::ttask& cfg_task, ttask_vars& task_vars, bool& finished)
{
	VALIDATE(code == nonblock_deepseek, null_str);
	return new tdeepseek2(aplt, cfg_task, task_vars, finished);
}

//
// lua api
//
static int impl_vcharge2_collect(lua_State* L)
{
	// twidget *v = *static_cast<twidget **>(lua_touserdata(L, 1));
	// v->~vwidget();
	return 0;
}

static int impl_vcharge2_get(lua_State* L)
{
	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);

	return ret? 1: 0;
}

/*
static int impl_vcharge2_kbook_reload(lua_State* L)
{
	tblock2* v = *static_cast<tblock2 **>(lua_touserdata(L, 1));

	bool from_file = luaL_checkboolean(L, 2);
	int max_disp_size = luaL_checkinteger(L, 3);

	VALIDATE(max_disp_size <= 200, null_str);

	std::string err_msg;
	if (from_file) {
		VALIDATE(false, null_str);
		err_msg = v->book().reload_from_file();
	}
	err_msg = v->book().reload_from_file();
	
	lua_pushstring(L, err_msg.c_str());
	if (err_msg.empty()) {
		const std::map<int64_t, tkbook::tquestion2>& questions = v->book().questions();
		lua_pushinteger(L, questions.size());

		// questions
		int lua_item_size = SDL_min(max_disp_size, (int)questions.size());
		// const tkbook::tquestion2* items_ptr = &questions[0];
		lua_createtable(L, lua_item_size, 0);

		{
			tstack_size_lock lock(L, 0);
			int at = 0;
			for (std::map<int64_t, tkbook::tquestion2>::const_iterator it = questions.begin(); it != questions.end(); ++ it, at ++) {
				const tkbook::tquestion2& question2 = it->second;
				v->book().lua_push_question2(L, question2);

				lua_rawseti(L, -2, at + 1);
			}
		}

	} else {
		const int count = 0;
		lua_pushinteger(L, count); // count
		lua_createtable(L, count, 0); // questions
		// lua_pushnil(L); // positions
	}
	return 3;
}
*/

void luaW_pushvnonblock2(lua_State* L, tlua_nonblock& widget)
{
	aplt::tb_api& b_api = aplt::get_b_api();

	tstack_size_lock lock(L, 1);
	// new(L) vwidget(L, widget);
	tlua_nonblock** v = (tlua_nonblock**)lua_newuserdata(L, sizeof(tlua_nonblock*));
	*v = &widget;

	// b_api.call_lua_breakpoint();
	nil_metatable(b_api.get_lua_State(), vnonblock2MetatableKey);
	// b_api.call_lua_breakpoint();
	
	// see https://www.cswamp.com/post/261
	if (luaL_newmetatable(L, vnonblock2MetatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vcharge2_collect},
			{"__index", impl_vcharge2_get},

			// tkbook
			// {"kbook_reload", impl_vblock2_kbook_reload},

			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vnonblock2MetatableKey);

	} else {
		VALIDATE(false, null_str);
	}

	lua_setmetatable(L, -2);
}

}
