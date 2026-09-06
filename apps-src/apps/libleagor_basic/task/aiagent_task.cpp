#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "rose_global.hpp"
#include "gettext.hpp"
#include "aiagent_task.hpp"
#include "rose_config_3rdparty.hpp"

#include "rose_ros/utils.hpp"

#include <SDL.h>
#include <SDL_log.h>
#include "aplt_clazz.hpp"

#include <rose_ros/aplt.hpp>
#include "base_slot.hpp"
#include "rose_net_api.hpp"

#include "common.hpp"
#include "rose_md4c.hpp"


using namespace std::placeholders;

const char vaiagentMetatableKey[] = "cpp.vaiagent";


namespace aplt {

class ttable_render: public md4c::trender
{
public:
	ttable_render()
		: md4c::trender(false, true)
	{
		clear();
	}

	void clear()
	{
		in_TBODY_ = false;
		in_TR_ = false;
		TD_at_ = nposm;

		rows_.clear();
		parse_fail_ = false;
	}

	struct tstr3_row
	{
		std::string date;
		std::string time;
		std::string action;
	};
	const std::vector<tstr3_row>& rows() const { return rows_; }

	void dump() const;

private:
	void did_enter_block(MD_BLOCKTYPE type, void* detail) override;
    void did_leave_block(MD_BLOCKTYPE type, void* detail) override;

    // virtual void did_enter_span(MD_SPANTYPE type, void* detail) {}
    // virtual void did_leave_span(MD_SPANTYPE type, void* detail) {}
    void did_text(MD_TEXTTYPE type, const std::string& text) override;

private:
	bool in_TBODY_;
	bool in_TR_;
	int TD_at_;

	std::vector<tstr3_row> rows_;
	bool parse_fail_;
};

void ttable_render::did_enter_block(MD_BLOCKTYPE type, void* detail)
{
	if (type == MD_BLOCK_TBODY) {
		in_TBODY_ = true;

	} else if (type == MD_BLOCK_TR) {
		if (in_TBODY_) {
			in_TR_ = true;
		}

	} else if (type == MD_BLOCK_TD) {
		if (!in_TBODY_ || !in_TR_) {
			parse_fail_ = true;
			return;
		}

		if (TD_at_ == nposm) {
			rows_.push_back(tstr3_row());
			TD_at_ = 0;

		} else if (TD_at_ < 2) {
			TD_at_ ++;

		} else {
			parse_fail_ = true;
		}
	}
}

void ttable_render::did_leave_block(MD_BLOCKTYPE type, void* detail)
{
	if (type == MD_BLOCK_TR) {
		in_TR_ = false;
		TD_at_ = nposm;

	} else if (type == MD_BLOCK_TBODY) {
		in_TBODY_ = false;
	}
}

void ttable_render::did_text(MD_TEXTTYPE type, const std::string& text)
{
	if (type != MD_TEXT_NORMAL || TD_at_ == nposm) {
		return;
	}
	if (rows_.empty()) {
		return;
	}
	if (TD_at_ == 0) {
		rows_.back().date.append(text);

	} else if (TD_at_ == 1) {
		rows_.back().time.append(text);

	} else if (TD_at_ == 2) {
		rows_.back().action.append(text);

	} else {
		parse_fail_ = true;
	}
}

void ttable_render::dump() const
{
	for (std::vector<tstr3_row>::const_iterator it = rows_.begin(); it != rows_.end(); ++ it) {
		const tstr3_row& row = *it;
		SDL_Log("%s\t%s\t%s", row.date.c_str(), row.time.c_str(), row.action.c_str());
	}
}

void parse_physics_plan()
{
	std::string filename = game_config::preferences_dir + "/aplt_leagor_basic__documents/1.dat";

	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	VALIDATE(file.valid(), null_str);
	
	std::stringstream err;

	std::string answer;
	int fsize = file.read_2_data();
	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(file.data, json_object)) {
			VALIDATE(false, null_str);
		} else {
			const Json::Value& json_choices = json_object["choices"];
			if (json_choices.isArray()) {
				for (int at = 0; at < (int)json_choices.size(); at ++) {
					const Json::Value& json_choice = json_choices[at];
					int index = json_choice["index"].asInt();
					const Json::Value& message = json_choice["message"];
					if (message.isObject()) {
						answer = message["content"].asString();
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	SDL_Log("answer: %s", answer.c_str());
	write_file(game_config::preferences_dir + "/aplt_leagor_basic__documents/1-answer.md", answer.c_str(), answer.size());
}

//
// tdeepseek
//

ttimed_reminder::ttimed_reminder(tapplet& aplt, const tapplet::ttask& cfg_task, const std::string& question, const surface& surf, ttask_vars& task_vars, bool& finished)
	: thelper_aiagent_task_slot(*lua_aiagent, false, aplt, cfg_task, question, surf, task_vars, finished)
	// , dbg_answer_md_(game_config::os == os_windows? aplt_.preferences_dir + "/1-answer.md": null_str)
	, var_name_ds_retbool_(utils::join_app_prefix_id(aplt_.bundleid, "ds_retbool"))
	, var_name_ds_answer_(utils::join_app_prefix_id(aplt_.bundleid, "ds_answer"))
	, aplt_id_leagor_basic_(aplt::get_bundleid(bundleid_leagor_basic))
	, state_(nposm)
{
	// md4c::md2html(aplt.preferences_dir + "/1-answer.md", aplt.preferences_dir + "/1-answer.html");
	const tapplet* aplt2 = aplt_from_bundleid(b_api_.const_applets(), aplt::get_bundleid(bundleid_leagor_basic));
	if (aplt2 != nullptr) {
		aplt_id_leagor_basic_ = aplt2->id;
	}

	clear();

	output_var_keys_.push_back(var_name_ds_retbool_);
	output_var_keys_.push_back(var_name_ds_answer_);
}

std::string ttimed_reminder::pre_start_task()
{
	// std::string err_msg;
	utils::string_map symbols;
	validate_nposm();

	aplt::tnlp_4_aiagent& nlp = b_api_.nlp_4_aiagent();
/*
	if (ai_slot_singleton == nullptr) {
		return _("This applet is not set to current AI driver.");
	}
*/

	if (nlp.is_nlp_questioning()) {
		return _("An AI agent task is currently running.");
	}

	if (question_.empty()) {
		return vgettext2("'question' cannot be empty", symbols);
	}
/*
	// if has env_var, get it.
	const std::string var_name_ds_image = utils::join_app_prefix_id(aplt_.bundleid, "ds_image");

	std::vector<std::string> var_names;
	var_names.push_back(var_name_ds_image);

	std::string ds_image;

	std::string op;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = var_names.begin(); it != var_names.end(); ++ it, at ++) {
		const std::string& var_name = *it;
		symbols["var"] = var_name;
		if (task_vars_.existed(var_name) == 0) {
			continue;
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

		if (var_name == var_name_ds_image) {
			ds_image = task_vars_.get_string(var_name);
		}
	}
*/
	std::string err_msg;
/*
	surface surf;
	if (!ds_image.empty()) {
		surf = image::rose_get_image(ds_image);
		if (surf.get() == nullptr) {
			symbols["file"] = ds_image;
			err_msg = vgettext2("$file isn't a valid image file", symbols);
		}
	}
*/
	if (err_msg.empty()) {
		if (dbg_answer_md_.empty()) {
		// if (true) {
			// normal workflow
			state_ = state_send_nlp;
			nlp.send_nlp_question_4_aiagent(true, question_, nullptr, std::bind(&ttimed_reminder::did_nlp_answer, this, _1, _2, _3, _4));

		} else {
			// parse_physics_plan();
			// debug workflow. 
			// Each use of Deepseek consumes tokens. 
			// Sometimes you only want to debug later stages, like parsing add_timed_task_cfg from markdown answer.
			VALIDATE(game_config::os == os_windows, null_str);
			tfile file(dbg_answer_md_, GENERIC_READ, OPEN_EXISTING);
			int fsize = file.read_2_data();
			VALIDATE(fsize > 0, null_str);
			config cfg = answer_2_add_timed_task_cfg(file.data);

			if (!cfg.empty()) {
				add_timed_task_cfg_ = cfg;
				state_ = state_desire_add_timing_task;
				return null_str;
			}
			finished_ = true;
		}
	}

	return err_msg;
}

void ttimed_reminder::did_nlp_answer(bool retbool, const std::string& answer, int input_tokens, int output_tokens)
{
	VALIDATE(state_ == state_send_nlp, null_str);

	retbool_ = retbool;
	answer_ = answer;

	if (retbool) {
		if (!dbg_answer_md_.empty()) {
			write_file(dbg_answer_md_, answer.c_str(), answer.size());
		}
		config cfg = answer_2_add_timed_task_cfg(answer_);
		if (!cfg.empty()) {
			add_timed_task_cfg_ = cfg;
			state_ = state_desire_add_timing_task;
			return;
		}
		retbool_ = false;
	}

	VALIDATE(!retbool_, null_str);
	finished_ = true;
}

void ttimed_reminder::did_aplt_task_finished(const aplt::ttask_vars& task_vars)
{
	VALIDATE(state_ == state_call_add_timing_task, null_str);
	VALIDATE(!finished_, null_str);

	finished_ = true;
}

void ttimed_reminder::task_finished(const tapplet::ttask& cfg_task)
{
	VALIDATE_IN_MAIN_THREAD();

	aplt::tnlp_4_aiagent& nlp = b_api_.nlp_4_aiagent();
	VALIDATE(!nlp.is_nlp_questioning(), null_str);

	task_vars_.insert_bool(var_name_ds_retbool_, false, retbool_);
	task_vars_.insert_string(var_name_ds_answer_, false, answer_);

	clear();
}

config ttimed_reminder::answer_2_add_timed_task_cfg(const std::string& answer) const
{
	ttable_render render;
	// md4c::md_file(game_config::preferences_dir + "/2.md", render);
	md4c::md_data(answer.c_str(), answer.size(), render);
	// render.dump();

	std::map<int64_t, tb_api::tadd_timed_task> tasks;
	std::map<std::string, std::string> input_vals;

	time_t curtime = time(nullptr);
	struct tm tm = *localtime(&curtime);
	const int year = tm.tm_year + 1900;
	const int month = tm.tm_mon + 1;
	int today_day = tm.tm_mday;

	config result_cfg;
	int last_valid_day = INT32_MIN; // nposm is -1, date may appear -1.
	const std::vector<ttable_render::tstr3_row>& rows = render.rows();
	for (std::vector<ttable_render::tstr3_row>::const_iterator it = rows.begin(); it != rows.end(); ++ it) {
		const ttable_render::tstr3_row& row = *it;
		int day = 0;

		if (!row.date.empty()) {
			day = utils::to_int(row.date);
			last_valid_day = day;

		} else if (last_valid_day != INT32_MIN) {
			// 1	10:00 - 11:30
			//	    11:30 - 12:30
			//      12:30 - 40:00
			// if row.date is empty, use last valid day.
			day = last_valid_day;

		} else {
			return result_cfg;
		}
		// '1' is today, so "-1".
		day = today_day + day - 1;

		// 10:00-11:30
		std::string time_str = row.time;
		size_t pos = row.time.find("-");
		if (pos != std::string::npos) {
			time_str = row.time.substr(0, pos);
		}

		int hour;
		int minute;
		int second;
		bool ret = utils::from_hh_mm_ss(time_str, ':', hour, minute, second);
		if (!ret) {
			return result_cfg;
		}

		const std::string aplt_id = aplt_id_leagor_basic_;
		const std::string task_id = aplt::reserved_tasks.find(taskid_basic_alert)->second.id;

		input_vals["msg"] = row.action;
		input_vals["persist"] = "true";
		input_vals["voice"] = "true";
		int64_t time = utils::mktime2(year, month, day, hour, minute, second);
		tasks.insert(std::make_pair(time, tb_api::tadd_timed_task(false, aplt_id, task_id, null_str, null_str, null_str, time, input_vals)));
	}

	add_timed_tasks_to_cfg(tasks, result_cfg);

	return result_cfg;
}

void ttimed_reminder::slice()
{
	if (state_ == state_desire_add_timing_task) {
		VALIDATE(!add_timed_task_cfg_.empty(), null_str);
		state_ = state_call_add_timing_task;

		const std::string aplt_id = aplt_id_leagor_basic_;
		const std::string task_id = aplt::reserved_tasks.find(taskid_basic_timed_task)->second.id;

		// const std::string aplt_id = src_bundleid_2_id(src_studio, get_bundleid(bundleid_leagor_khome));
		// const std::string task_id = "recognition"; // snapshot

		ttask_pair pair = task_pair_from_2_id(b_api_.const_applets(), aplt_id, task_id, false, false);

		std::stringstream cfg_str;
		write_config(cfg_str, add_timed_task_cfg_);
		const std::string var_name_task_cfg = utils::join_app_prefix_id(pair.aplt->bundleid, "task_cfg");
		aplt::ttask_vars task_vars;
		task_vars.insert_string(var_name_task_cfg, false, cfg_str.str());

		std::string err_msg = request_aplt_task(*pair.aplt, *pair.task, task_vars, std::bind(&ttimed_reminder::did_aplt_task_finished, this, _1));
		if (!err_msg.empty()) {
			finished_ = true;
			pinyin_.speak(err_msg);
		}
	}
}

thelper_aiagent_task_slot* create_aiagent_task_slot(int code, tapplet& aplt, const tapplet::ttask& cfg_task, const std::string& question, const surface& surf, ttask_vars& task_vars, bool& finished)
{
	VALIDATE(code == aiagent_add_timed_reminder, null_str);
	return new ttimed_reminder(aplt, cfg_task, question, surf, task_vars, finished);
}

//
// lua api
//
static int impl_vaiagent_collect(lua_State* L)
{
	// twidget *v = *static_cast<twidget **>(lua_touserdata(L, 1));
	// v->~vwidget();
	return 0;
}

static int impl_vaiagent_get(lua_State* L)
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

void luaW_pushvaiagent(lua_State* L, tlua_aiagent& widget)
{
	aplt::tb_api& b_api = aplt::get_b_api();

	tstack_size_lock lock(L, 1);
	// new(L) vwidget(L, widget);
	tlua_aiagent** v = (tlua_aiagent**)lua_newuserdata(L, sizeof(tlua_aiagent*));
	*v = &widget;

	// ros.call_lua_breakpoint();
	nil_metatable(b_api.get_lua_State(), vaiagentMetatableKey);
	// ros.call_lua_breakpoint();
	
	// see https://www.cswamp.com/post/261
	if (luaL_newmetatable(L, vaiagentMetatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vaiagent_collect},
			{"__index", impl_vaiagent_get},

			// tkbook
			// {"kbook_reload", impl_vblock2_kbook_reload},

			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vaiagentMetatableKey);

	} else {
		VALIDATE(false, null_str);
	}

	lua_setmetatable(L, -2);
}

}
