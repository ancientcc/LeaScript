#define GETTEXT_DOMAIN "rose-lib"

#include "rose_ros/aplt.hpp"
#include "rose_exception.hpp"
#include "rose_config_3rdparty.hpp"
#include "rose_filesystem.hpp"
#include "gettext.hpp"
#include <boost/foreach.hpp>

#include <rose_ros/values.hpp>
#include <tf2/utils.h>

static bool two_dots(char a, char b) { return a == '.' && b == '.'; }

namespace utils {

class string_map_variable_set : public variable_set
{
public:
	string_map_variable_set(const string_map& map) : map_(map) {}

	virtual config::attribute_value get_variable_const(const std::string &key) const
	{
		config::attribute_value val;
		const string_map::const_iterator itor = map_.find(key);
		if (itor != map_.end())
			val = itor->second;
		return val;
	}

	bool empty() const { return map_.empty(); }

private:
	const string_map& map_;

};
}

static std::string do_interpolation2(const aplt::ttask_vars& task_vars, const std::string &str, const utils::string_map_variable_set& set, bool func_only, aplt::tfunction::tresult& result)
{
	result.validate_nposm();

	std::string res = str;
	// This needs to be able to store negative numbers to check for the while's condition
	// (which is only false when the previous '$' was at index 0)
	int rfind_dollars_sign_from = (int)res.size();
	while(rfind_dollars_sign_from >= 0) {
		// Going in a backwards order allows nested variable-retrieval, e.g. in arrays.
		// For example, "I am $creatures[$i].user_description!"
		const std::string::size_type var_begin_loc = res.rfind('$', rfind_dollars_sign_from);

		// If there are no '$' left then we're done.
		if(var_begin_loc == std::string::npos) {
			break;
		}

		// For the next iteration of the loop, search for more '$'
		// (not from the same place because sometimes the '$' is not replaced)
		rfind_dollars_sign_from = int(var_begin_loc) - 1;


		const std::string::iterator var_begin = res.begin() + var_begin_loc;

		// The '$' is not part of the variable name.
		const std::string::iterator var_name_begin = var_begin + 1;
		std::string::iterator var_end = var_name_begin;

		if(var_name_begin == res.end()) {
			// Any '$' at the end of a string is just a '$'
			continue;
		} else if(*var_name_begin == '(') {
			// The $( ... ) syntax invokes a formula
			int paren_nesting_level = 0;
			bool in_string = false,
				in_comment = false;
			do {
				switch(*var_end) {
				case '(':
					if(!in_string && !in_comment) {
						++paren_nesting_level;
					}
					break;
				case ')':
					if(!in_string && !in_comment) {
						--paren_nesting_level;
					}
					break;
				case '#':
					if(!in_string) {
						in_comment = !in_comment;
					}
					break;
				case '\'':
					if(!in_comment) {
						in_string = !in_string;
					}
					break;
				// TODO: support escape sequences when/if they are allowed in FormulaAI strings
				}
			} while(++var_end != res.end() && paren_nesting_level > 0);
			if(paren_nesting_level > 0) {
				// Formula in WML string cannot be evaluated due to a missing closing parenthesis:\n\t--> $std::string(var_begin, var_end)
				res.replace(var_begin, var_end, "");
				continue;
			}
			// try {
				// const game_logic::formula form(std::string(var_begin+2, var_end-1));
				// res.replace(var_begin, var_end, form.evaluate().string_cast());
			const std::string statement(var_begin+2, var_end-1);
			if (!statement.empty()) {
				// aplt::tros_function func;
				result = aplt::curr_func->calculate(task_vars, std::string(var_begin+2, var_end-1));
				if (!result.is_ok()) {
					return res;
				}
				res.replace(var_begin, var_end, result.val.str());
			}

			// } catch(game_logic::formula_error& e) {
				// Formula in WML string cannot be evaluated due to $e.type --> $e.formula
				// res.replace(var_begin, var_end, "");
			// }
			continue;
		}

		if (func_only) {
			continue;
		}

		// Find the maximum extent of the variable name (it may be shortened later).
		for(int bracket_nesting_level = 0; var_end != res.end(); ++var_end) {
			const char c = *var_end;
			if(c == '[') {
				++bracket_nesting_level;
			}
			else if(c == ']') {
				if(--bracket_nesting_level < 0) {
					break;
				}
			}
			// isascii() breaks on mingw with -std=c++0x
			else if (!(((c) & ~0x7f) == 0)/*isascii(c)*/ || (!isalnum(c) && c != '.' && c != '_')) {
				break;
			}
		}

		// Two dots in a row cannot be part of a valid variable name.
		// That matters for random=, e.g. $x..$y
		var_end = std::adjacent_find(var_name_begin, var_end, two_dots);

		// If the last character is '.', then it can't be a sub-variable.
		// It's probably meant to be a period instead. Don't include it.
		// Would need to do it repetitively if there are multiple '.'s at the end,
		// but don't actually need to do so because the previous check for adjacent '.'s would catch that.
		// For example, "My score is $score." or "My score is $score..."
		if(*(var_end-1) == '.'
		// However, "$array[$i]" by itself does not name a variable,
		// so if "$array[$i]." is encountered, then best to include the '.',
		// so that it more closely follows the syntax of a variable (if only to get rid of all of it).
		// (If it's the script writer's error, they'll have to fix it in either case.)
		// For example in "$array[$i].$field_name", if field_name does not exist as a variable,
		// then the result of the expansion should be "", not "." (which it would be if this exception did not exist).
		&& *(var_end-2) != ']') {
			--var_end;
		}

		const std::string var_name(var_name_begin, var_end);

		if(var_end != res.end() && *var_end == '|') {
			// It's been used to end this variable name; now it has no more effect.
			// This can allow use of things like "$$composite_var_name|.x"
			// (Yes, that's a WML 'pointer' of sorts. They are sometimes useful.)
			// If there should still be a '|' there afterwards to affect other variable names (unlikely),
			// just put another '|' there, one matching each '$', e.g. "$$var_containing_var_name||blah"
			++var_end;
		}


		if (var_name == "") {
			// Allow for a way to have $s in a string.
			// $| will be replaced by $.
			res.replace(var_begin, var_end, "$");
		}
		else {
			// The variable is replaced with its value.
			config::attribute_value val = set.get_variable_const(var_name);
			if (val.blank()) {
				if (result.is_ok()) {
					result.set_err_var_1var(aplt::tfunction::err_not_exist_var, var_name);
				} else {
					VALIDATE(result.err == aplt::tfunction::err_not_exist_var, null_str);
				}
			}
			res.replace(var_begin, var_end, val);
		}
	}

	return res;
}

namespace utils {

const std::string interpolate_variables_into_string(const aplt::ttask_vars& task_vars, const std::string &str, 
	const string_map * const symbols, aplt::tfunction::tresult& result)
{
	string_map_variable_set set(*symbols);

	std::string str2 = str;
	if (str.find("$(") != std::string::npos) {
		SDL_Log("str: %s", str.c_str());
		str2 = do_interpolation2(task_vars, str, set, true, result);
		if (!result.is_ok()) {
			return str2;
		}
		SDL_Log("str2: %s", str2.c_str());
	}
	return do_interpolation2(task_vars, str2, set, false, result);
}
}

namespace aplt {

//
// tr_api
//
static tr_api* r_api = nullptr;

tr_api& get_r_api()
{
	VALIDATE(r_api != nullptr, null_str);
	return *r_api;
}

tr_api::tr_api()
{
	VALIDATE(r_api == nullptr, null_str);
	r_api = this;
}

tr_api::~tr_api()
{
	VALIDATE(r_api != nullptr, null_str);
	r_api = nullptr;
}
/*
void add_timed_tasks_to_cfg(const std::map<int64_t, tb_api::tadd_timed_task>& tasks, config& cfg)
{
	int at = 0;
	for (std::map<int64_t, tb_api::tadd_timed_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it, at ++) {
		const tb_api::tadd_timed_task& task = it->second;
		config& task_cfg = cfg.add_child("add_timed_task");

		task_cfg["persist"].from_bool(task.persist);
		task_cfg["aplt_id"].from_string(task.aplt_id, true);
		task_cfg["task_id"].from_string(task.task_id, true);
		task_cfg["ble_device_id"].from_string(task.ble_device_id, true);
		task_cfg["position1"].from_string(task.position1, true);
		task_cfg["position2"].from_string(task.position2, true);
		task_cfg["time"].from_int64(task.time);

		config& input_vars_cfg = task_cfg.add_child("input_vars");
		for (std::map<std::string, std::string>::const_iterator it = task.input_vars.begin(); it != task.input_vars.end(); ++ it) {
			const std::string& key = it->first;
			const std::string& val = it->second;
			VALIDATE(!key.empty(), null_str);
			input_vars_cfg[key].from_string(val, true);
		}

		task_cfg["added"].from_bool(task.added);
		task_cfg["deleted"].from_bool(task.deleted);
	}
}

bool cfg_to_add_timed_tasks(const config& root_cfg, std::map<int64_t, tb_api::tadd_timed_task>& result)
{	
	result.clear();

	// sync bg_task_.timed_tasks_ with timed_sensors_
	// must make sure 'timed_tasks_[at].timed_at' == 'timed_sensors_[at].at'
	// const std::map<taplt_task_key, taplt_task>& timed_tasks = bg_task_.timed_tasks();
	// std::map<taplt_task_key, taplt_task>::const_iterator task_it = timed_tasks.begin();

	// int at = 0;
	// if timed_sensor.'at' is right, this timed_sensor will not exist.
	std::map<std::string, std::string> input_vars;
	BOOST_FOREACH (const config& task_cfg, root_cfg.child_range("add_timed_task")) {
		bool persist = task_cfg["persist"].to_bool();
		const std::string aplt_id = task_cfg["aplt_id"].str();
		const std::string task_id = task_cfg["task_id"].str();
		const std::string ble_device_id = task_cfg["ble_device_id"].str();
		const std::string position1 = task_cfg["position1"].str();
		const std::string position2 = task_cfg["position2"].str();
		int64_t time = task_cfg["time"].to_int64();

		input_vars.clear();
		const config& ipupt_vars_cfg = task_cfg.child("input_vars");
		if (ipupt_vars_cfg) {
			for (const config::attribute &v: ipupt_vars_cfg.attribute_range()) {
				const std::string& key = v.first;
				const std::string& val = v.second;
				if (key.empty()) {
					continue;
				}
				input_vars.insert(std::make_pair(key, val));
			}
		}
		std::pair<std::map<int64_t, tb_api::tadd_timed_task>::iterator, bool> ins = result.insert(std::make_pair(time, tb_api::tadd_timed_task(false, aplt_id, task_id, ble_device_id, position1, position1, time, input_vars)));
		if (!ins.second) {
			continue;
		}
		tb_api::tadd_timed_task& new_task = ins.first->second;
		new_task.added = task_cfg["added"].to_bool();
		new_task.deleted = task_cfg["deleted"].to_bool();
	}

	// verity_add_timed_tasks();
	return true;
}
*/
tros_cpp_api::tros_cpp_api()
	: pinyin_(aplt::get_curr_pinyin())
	, b_api_(aplt::get_b_api())
	, r_api_(aplt::get_r_api())
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
{
	clear_2th();
}

tros_cpp_api::~tros_cpp_api()
{
	validate_nposm();
}

void tros_cpp_api::validate_nposm()
{
	VALIDATE(state_ == nposm, null_str);
	VALIDATE(key_2_states_.empty(), null_str);
	VALIDATE(states_.empty(), null_str);
	VALIDATE(async_request_state_ == nposm, null_str);
	VALIDATE(end_flag_ == nposm, null_str);

	VALIDATE(reception_state_ == nposm, null_str);
	VALIDATE(!recoverable_, null_str);
	VALIDATE(!nonpreemptive_, null_str);
	VALIDATE(startup_state_.branches.empty(), null_str);
	VALIDATE(state_overflow_ticks_ == 0, null_str);

	VALIDATE(speech_states_.empty(), null_str);
}

void tros_cpp_api::clear_2th()
{
	// here state_ maybe isn't nposm. for example when state_open_door, applicate is terminated force.
	state_ = nposm;
	key_2_states_.clear();
	states_.clear();
	async_request_state_ = nposm;
	end_flag_ = nposm;

	reception_state_ = nposm;
	nonpreemptive_ = false;
	recoverable_ = false;
	startup_state_.clear();
	state_overflow_ticks_ = 0;

	speech_states_.clear();
}


void tros_cpp_api::pre_2th_start_task()
{
	validate_nposm();
}

void tros_cpp_api::post_2th_start_task()
{
	// states_ maybe empty.
	// VALIDATE(!states_.empty(), null_str); 
	VALIDATE(reception_state_ == nposm || (reception_state_ >= 0 && reception_state_ < (int)states_.size()), null_str);
	VALIDATE(state_overflow_ticks_ == 0, null_str);

	for (std::map<int, tstate2>::const_iterator it = states_.begin(); it != states_.end(); ++ it) {
		const tstate2& state2 = it->second;
		if (state2.async_task.is_aplt_task) {
			VALIDATE(state2.threshold_s > 0, null_str);
			VALIDATE(!state2.doing.empty(), null_str);

		} else if (state2.threshold_s != nposm) {
			VALIDATE(!state2.doing.empty(), null_str);
			speech_states_.insert(state2.state);

		} else {
			VALIDATE(state2.doing.empty(), null_str);
		}
	}

	if (reception_state_ != nposm) {
		VALIDATE(is_speech_state(reception_state_), null_str);
	}

	tif_block::tresult calc_result = startup_state_.calculate(*task_vars_);
	if (!calc_result.do_str.empty()) {
		speak(calc_result.do_str);
	}
	const int startup_state = calc_result.do_to_state;
	if (startup_state != nposm) {
		async_request_state_ = startup_state;

	} else {
		set_end_request();
	}
	
	// zero_state_overflow_ticks("post_2th_start_task");
}

void tros_cpp_api::post_2th_task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task)
{
}

void tros_cpp_api::slice()
{
	if (state_overflow_ticks_ != 0) {
		VALIDATE(state_ != nposm, null_str);
		VALIDATE(end_flag_ == nposm, null_str);
	} else {
		VALIDATE(state_ == nposm, null_str);
	}

	if (async_request_state_ != nposm) {
		const tstate2& to_state2 = states_.find(async_request_state_)->second;
		go_to_state(to_state2, 2);
		async_request_state_ = nposm;
	}

	if (state_overflow_ticks_ != 0 && SDL_GetTicks() >= state_overflow_ticks_) {
		SDL_Log("%u {stop_task}(single)[%s]>= state_overflow_ticks_: %u", 
			SDL_GetTicks(), req_task_.valid()? "req_task": "", state_overflow_ticks_);
		VALIDATE(end_flag_ == nposm, null_str);

		if (req_task_.valid()) {
			// Time overflow, end the async-task. will result single_task_finisned() be called.
			b_api_.set_task_finished();

			// It is forbidden that both state_overflow_ticks_ and stop_task_cpp_ticks_ are 0 at the same time.
			// So add a safe waiting time, to simple can be directly modified to UINT32_MAX.
			state_overflow_ticks_ = UINT32_MAX;

		} else {
			VALIDATE(is_speech_state(state_), null_str);
			// (1/2) avoid wait in speech_state always
			// Time overflow, if not in reception_state, switch to reception_state. 
			// Otherwise, end the cpp_task
			if (reception_state_ != nposm && state_ != reception_state_) {
				const tstate2& to_state2 = states_.find(reception_state_)->second;
				go_to_state(to_state2, 4);

			} else {
				state_ = nposm; //
				zero_state_overflow_ticks("slice.speech_state");
				set_end_request();
			}
		}
	}

	if (end_flag_ == end_request) {
		VALIDATE(state_overflow_ticks_ == 0, null_str);
		VALIDATE(state_ == nposm, null_str);
		SDL_Log("%u {stop_task}(cpp)end_flag_(%i)", SDL_GetTicks(), end_flag_);
		b_api_.set_task_finished();

		if (end_flag_ == end_request) {
			end_flag_ = end_ing;
		}
	}
}

void tros_cpp_api::single_task_finished(const treq_task& req_task, bool result)
{
	VALIDATE(req_task.equal10(req_task_), null_str);
	VALIDATE(end_flag_ == nposm, null_str);

	const tstate2& state2 = states_.find(state_)->second;
	const tif_block::tresult finished_result = state2.finished.calculate(*task_vars_);
	const std::string finished_text = finished_result.do_str;
	if (!finished_text.empty()) {
		speak(finished_text);
	}

	VALIDATE(async_request_state_ == nposm, null_str);

	int next_state = finished_result.do_to_state;
	if (next_state != nposm) {
		async_request_state_ = next_state;
		// don't touch state_

	} else {
		state_ = next_state;
		zero_state_overflow_ticks("single_task_finished");
		set_end_request();
	}
}

const tros_cpp_api::tkey_2_state* tros_cpp_api::get_key_2_state(const std::string& pinyin, int from_state) const
{
	VALIDATE(from_state != nposm, null_str);
	VALIDATE(end_flag_ == nposm, null_str);

	const tkey_2_state* hit_key_2_state = nullptr;
	for (std::vector<tkey_2_state>::const_iterator it = key_2_states_.begin(); hit_key_2_state == nullptr && it != key_2_states_.end(); ++ it) {
		const tkey_2_state& key_2_state = *it;
		// const int from_state = from_always_nposm? nposm: state_;
		if (key_2_state.from_state != from_state) {
			continue;
		}

		std::vector<std::string> py_minor_words_runtime;
		if (!key_2_state.py_minor_words_ready) {
			const std::string minor_words_str = key_2_state.minor_words.calculate(*task_vars_).do_str;

			const utils::string_map& symbols = task_vars_->symbols();
			aplt::tfunction::tresult result;
			const std::string minor_words_str2 = utils::interpolate_variables_into_string(*task_vars_, minor_words_str, &symbols, result);

			std::vector<std::string> minor_words;
			aplt::parse_py_words_from_cfg_str(pinyin_, tone_, eng_lowercase_, minor_words_str2, minor_words, py_minor_words_runtime);
		}
		const std::vector<std::string>& py_minor_words = key_2_state.py_minor_words_ready? key_2_state.py_minor_words: py_minor_words_runtime;
		if (key_2_state.py_major_word.empty() && py_minor_words.empty()) {
			continue;
		}
		int any_one_at = nposm;
		bool matched = major_minor_words_match(pinyin, key_2_state.py_major_word, 
			py_minor_words, key_2_state.strategy, &any_one_at);
		if (matched) {
			if (any_one_at != nposm) {
				task_vars_->insert_integer(utils::join_app_prefix_id(aplt::fake_aplt.bundleid, "last_matched_index_"), false, any_one_at);
			}
			hit_key_2_state = &key_2_state;
		}
	}

	return hit_key_2_state;
}

void tros_cpp_api::go_to_state(const tstate2& to_state2, int scene)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(end_flag_ == nposm, null_str);

	SDL_Log("%u go_to_state(state: %i, threshold_s: %i, is_aplt_task: %s) scene: %i", 
		SDL_GetTicks(), to_state2.state, to_state2.threshold_s, 
		to_state2.async_task.is_aplt_task? "true": "false", scene);

	std::string err_msg;
	VALIDATE(to_state2.threshold_s != nposm, null_str);
	if (is_speech_state(to_state2.state)) {
		// VALIDATE(!to_state2.async_task.valid(), null_str);
		req_task_.clear();
		set_state_overflow_ticks("go_to_state && to_state is interruptable_state", to_state2.threshold_s * 1000);

	} else {
		// VALIDATE(to_state2.async_task.valid(), null_str);
		req_task_ = to_state2.async_task;

		const utils::string_map& symbols = task_vars_->symbols();
		aplt::tfunction::tresult result;
		if (req_task_.valid()) {
			VALIDATE(req_task_.task != nullptr, null_str);

			std::map<std::string, const aplt::tif_block*> input_vars2;
			for (std::vector<std::pair<std::string, aplt::tif_block> >::const_iterator it = to_state2.async_task.input_vars.begin(); it != to_state2.async_task.input_vars.end(); ++ it) {
				const std::string& var_name = it->first;
				const aplt::tif_block& if_block = it->second;
				input_vars2.insert(std::make_pair(var_name, &if_block));
			}

			// settings.cfg and req_task_.input_vars definition variables may be in different order, 
			// shoulde the order in use settings.cfg.
			for (std::vector<aplt::tapplet::tvar>::const_iterator var_it = req_task_.task->vars.begin(); var_it != req_task_.task->vars.end(); ++ var_it) {
				const aplt::tapplet::tvar& var = *var_it;
				if (!var.input) {
					continue;
				}
				std::string var_name = utils::join_app_prefix_id(req_task_.aplt->bundleid, var.name);
				if (input_vars2.count(var_name) == 0) {
					// other task maybe make it exist. erase it.
					task_vars_->erase(var_name);
					continue;
				}
				const tif_block& if_block = *input_vars2.find(var_name)->second;

				if (if_block.branches.empty()) {
					task_vars_->erase(var_name);
					continue;
				}

				std::string var_val = if_block.calculate(*task_vars_).do_str;
				result.clear();

				bool erase = false;
				std::string no_symboled_str;
				const config::attribute_value* exp_val = nullptr;
				int type = aplt::var_exp_which_type(var_val, &no_symboled_str);
				if (type != nposm) {
					exp_val = aplt::var_val_from_no_symboled_str(*task_vars_, type, no_symboled_str, result);
					if (exp_val != nullptr) {
						task_vars_->insert_attribute(var_name, false, *exp_val);
					} else {
						// if type == var_exp_type_var, variable isn't exsited.
						// if type == var_exp_type_func, has variable that this function used isn't existed.
						erase = true;
					}

				} else {
					std::string result_str = utils::interpolate_variables_into_string(*task_vars_, var_val, &symbols, result);
					if (result.is_ok()) {
						task_vars_->insert_string(var_name, false, result_str);

					} else {
						SDL_Log("calculate input_var(%s) fail. don't insert this var. var_val: %s", var_name.c_str(), var_val.c_str());
						erase = true;
					}
				}
				if (erase) {
					task_vars_->erase(var_name);
				}
			}
		} else {
			VALIDATE(to_state2.async_task.input_vars.empty(), null_str);
		}

		// position1_uuid, position_uuid2
		VALIDATE(req_task_.position1_uuid.empty() && req_task_.position2_uuid.empty(), null_str);
		std::string var_val = to_state2.async_task.position1_if_block.calculate(*task_vars_).do_str;
		result.clear();
		req_task_.position1_uuid = utils::interpolate_variables_into_string(*task_vars_, var_val, &symbols, result);

		var_val = to_state2.async_task.position2_if_block.calculate(*task_vars_).do_str;
		result.clear();
		req_task_.position2_uuid = utils::interpolate_variables_into_string(*task_vars_, var_val, &symbols, result);

		// both postion1_uuid and position_uuid2 maybe invalid. 
		// if invalid, ros_.request_single_task()'s err_msg is not empty.
		err_msg = b_api_.request_single_task(req_task_);
		if (err_msg.empty()) {
			VALIDATE(req_task_.valid(), null_str);
			VALIDATE(to_state2.threshold_s > 0, null_str);
			set_state_overflow_ticks("go_to_state && to_state has req_task", to_state2.threshold_s * 1000);

		} else {
			// if err_msg isn't empty, will end task. it require state_overflow_ticks_ must be 0.
			zero_state_overflow_ticks("request single_task(async_task) fail");
			speak(err_msg);
		}
	}

	if (!err_msg.empty()) {
		state_ = nposm;
		VALIDATE(state_overflow_ticks_ == 0, null_str);
		set_end_request();

	} else {
		state_ = to_state2.state;
	}

	// ros_.set_allow_short_voice(is_speech_state(state_));
}

bool tros_cpp_api::speech_did_recognition_result(const std::string& result)
{
	if (state_ == nposm || end_flag_ != nposm) {
		return true;
	}

	const std::string pinyin = pinyin_.from_utf8str2(result, tone_, eng_lowercase_);
	int hit_state = state_;
	const tkey_2_state* hit_key_2_state = get_key_2_state(pinyin, state_);
	if (hit_key_2_state == nullptr) {
		if (reception_state_ != nposm && state_ != reception_state_) {
			if (is_speech_state(state_)) {
				// (2/2) avoid wait in interrupable_state always
				hit_key_2_state = get_key_2_state(pinyin, reception_state_);
			}
		}

		if (hit_key_2_state == nullptr) {
			const std::string& resp = states_.find(state_)->second.doing;
			VALIDATE(!resp.empty(), null_str);
			speak(resp);
			return true;
		}
		hit_state = reception_state_;
	}

	const tstate2& hit_state2 = states_.find(hit_state)->second;
	tif_block::tresult calc_result = hit_state2.finished.calculate(*task_vars_);
	if (!calc_result.do_str.empty()) {
		speak(calc_result.do_str);
	}

	const int to_state = calc_result.do_to_state;
	if (to_state == nposm) {
		state_ = nposm;
		zero_state_overflow_ticks("single_task_finished");
		set_end_request();
		return true;
	}
	
	const tstate2& to_state2 = states_.find(to_state)->second;

	go_to_state(to_state2, 3);
	return true;
}

void tros_cpp_api::set_end_request()
{
	VALIDATE(state_ == nposm, null_str);
	VALIDATE(end_flag_ == nposm, null_str);
	VALIDATE(state_overflow_ticks_ == 0, null_str);
	VALIDATE(async_request_state_ == nposm, null_str);

	end_flag_ = end_request;
}

void tros_cpp_api::speak(const std::string& msg)
{
	VALIDATE(!msg.empty(), null_str);

	const utils::string_map& symbols = task_vars_->symbols();
	aplt::tfunction::tresult result;
	// const std::string msg2 = utils::rose_interpolate_variables_into_string(msg, &symbols);
	const std::string msg2 = utils::interpolate_variables_into_string(*task_vars_, msg, &symbols, result);
	pinyin_.speak(msg2);
}

void tros_cpp_api::set_state_overflow_ticks(const std::string& scene, int threshold)
{
	VALIDATE(threshold > 0, null_str);

	state_overflow_ticks_ = SDL_GetTicks() + threshold;
	SDL_Log("%u {%s}[set_ticks]set state_overflow_ticks_: %u", SDL_GetTicks(), scene.c_str(), state_overflow_ticks_);
}

void tros_cpp_api::zero_state_overflow_ticks(const std::string& scene)
{
	state_overflow_ticks_ = 0;
	SDL_Log("%u {%s}[set_ticks]zero_stop_task_cpp_ticks()", SDL_GetTicks(), scene.c_str());
}

}

void tfpoint3_buffer::resize_points(int _size, int _vsize)
{
	_size = posix_align_ceil(_size, 64);
    VALIDATE(_size >= 0 && _vsize >= 0, null_str);

	if (_size > size) {
	    SDL_FPoint3* tmp = (SDL_FPoint3*)malloc(_size * sizeof(SDL_FPoint3));
        // memset(tmp, 0, _size * sizeof(SDL_FPoint3));
	    if (data != nullptr) {
            if (_vsize != 0) {
		        memcpy(tmp, data, _vsize * sizeof(SDL_FPoint3));
	        }
			free(data);
		}
		data = tmp;
		size = _size;
    }
}

namespace tf2
{

void doTransform_position(const tf2::Transform& t, const SDL_DPoint3& p_in, SDL_DPoint3& p_out)
{
    // tf2::Transform save 'rotation' use rotation matrix(Matrix3x3 m_basis)
    
    // this function only position is calculated, no rotation.
    // impletement method 'copy' from:
    //   tf2::doTransform(const geometry_msgs::Pose& t_in, geometry_msgs::Pose& t_out, const geometry_msgs::TransformStamped& transform)
    // Only 3 dots are required.
    tf2::Vector3 v(p_in.x, p_in.y, p_in.z);

    // <libros>/include/tf2/LinearMath/Transform.h
    // Vector3 tf2::Transform::operator()(const Vector3& x) const
    // Only 3 vector3-dot product are required.
    tf2::Vector3 v_out = t(v);
    p_out.x = v_out.m_floats[0];
    p_out.y = v_out.m_floats[1];
    p_out.z = v_out.m_floats[2];
}

void doTransform_position(const geometry_msgs::Transform& transform, const SDL_DPoint3& p_in, SDL_DPoint3& p_out)
{
    tf2::Transform t;
    tf2::fromMsg(transform, t);

    doTransform_position(t, p_in, p_out);
}

}

tdepth_sector_result depth_sector_area(const tdcintrinsics_C& intrinsics, sensor_msgs::LaserScan& scan_msg, int width, int height, const uint16_t* depth_data, double depth_scale, uint8_t* mutable_pixels, tfpoint3_buffer* points, double dcpitch)
{
    float fdx = 1 / intrinsics.fx;
    float fdy = 1 / intrinsics.fy;
    float u0 = intrinsics.cx;
    float v0 = intrinsics.cy;

    bool verbose = false;
    if (mutable_pixels != nullptr) {
        VALIDATE(points == nullptr, null_str);
        memset(mutable_pixels, 0xff, width * height * 4);
        verbose = true;

    } else {
        VALIDATE(points != nullptr, null_str);
        points->vsize = 0;
    }

    tf2::Quaternion q;
	q.setRPY(0, dcpitch, 0);
    const tf2::Transform transform(q);

    int dcamera_bonus_degree = 20;
    const int min_angle_degree = -(MOVEIT_SECTOR_HALF_ANGLE_DEG + dcamera_bonus_degree);
    const int max_angle_degree = MOVEIT_SECTOR_HALF_ANGLE_DEG + dcamera_bonus_degree;
    VALIDATE(max_angle_degree > min_angle_degree, null_str);
    // desire 2 point per 1 degree.
    const int node_count = (max_angle_degree - min_angle_degree) * 3;

    float max_distance = 2.0; // ?2.0
    // I don't known RPLIDAR A1M8's range_min. but I see 0.11400. so set it 0.1
    scan_msg.range_min = 0.07f; // 0.15f. when debug on windows, has line at tail. right: 0.1
    scan_msg.range_max = max_distance;// A1M8: 12.0;

    scan_msg.angle_min = DEG2RAD(min_angle_degree);
    scan_msg.angle_max = DEG2RAD(max_angle_degree);
    // scan_msg.angle_increment = (scan_msg.angle_max - scan_msg.angle_min) / (double)(node_count - 1);
    scan_msg.angle_increment = (scan_msg.angle_max - scan_msg.angle_min) / node_count;

    scan_msg.intensities.resize(node_count);

    scan_msg.ranges.resize(node_count);
    float* range_data = &scan_msg.ranges[0];
    memset(range_data, 0, sizeof(float) * node_count);

    const float MIN_DISTANCE = 5.0;
    const float MAX_DISTANCE = 10000.0;
    
    const float min_depth = MIN_DISTANCE / depth_scale;
    const float max_depth = MAX_DISTANCE / depth_scale;

    // const float height_from_ground = 0.5f; // 50cm, state_navigation result it. 
    // const float safe_obstacle_height = ros::dwa_straight_ward? 0.18f: 0.05f; // 0.05(5cm) safe: 0.20f, 0.10 is too small, 0.10 maybe safe
    // const float safe_obstacle_height = 0.05f; // 5cm
    // const float safe_obstacle_height = 0.18f; // 18cm
    float min_z = -1 * (ros::dcamera_height_from_ground - ros::safe_obstacle_height);
    if (KDL::Equal(ros::safe_obstacle_height, 0.18)) {
        // SDL_Log("%u depth_sector_area, safe_obstacle_height: %.3f min_z: %.3f", SDL_GetTicks(), safe_obstacle_height, min_z);
    }

    if (points != nullptr) {
        points->resize_points(node_count, 0);
        memset(points->data, 0, node_count * sizeof(SDL_FPoint3));
        points->vsize = node_count;
    }
    
    // const bool check_noise = mutable_pixels != nullptr;
    const bool check_noise = true;
    int y_inc = 1;
    if (mutable_pixels != nullptr) {
        // for overlay pixel(mutable_pixels != nullptr), require overlay all line.
        VALIDATE(y_inc == 1, null_str);
    }
    const int noise_require_min_cols = 5; // [1/3] 5(release using), 6(dbg)
    const int noise_require_min_lines = 2; // [2/3] 2(release using), 2(dbg)

    const int check_half_cols = 10;
    VALIDATE(check_half_cols > noise_require_min_cols, null_str);
    int noise_start_x = width / 2 - check_half_cols;
    int noise_end_x = noise_start_x + check_half_cols * 2 - 1;
    const double noise_xz_threshold = 0.05; // [3/3] 0.05(release using), 0.20(dbg)
    int noise_max_check_lines = 5; // 
    int maybe_noise_start_y = nposm;
    int noise_lines = 0;
    SDL_DPoint3 last_line_xyz2[check_half_cols * 2]; // left + right
    for (int at = 0; at < check_half_cols * 2; at ++) {
        last_line_xyz2[at].x = float_nposm;
    }

    // no_depth relative
    VALIDATE(width <= 1280, null_str);
    VALIDATE(height <= 800, null_str);
    // int no_depth_per_lines[800] = {0};
    int no_depth_per_cols[1280] = {0};
    const int max_rows_check_no_depth = height / 2;

    tdepth_sector_result result(scan_msg.angle_min, scan_msg.angle_max, scan_msg.angle_increment, min_z);
    bool depth_file_saved = false;
    for (int y = 0; y < height; y += y_inc) {
        const int y_start = y * width;
        int noise_cols = 0;
        for (int x = 0; x < width; x++) {
            const int check_x = check_noise && !result.noised && (x >= noise_start_x && x <= noise_end_x)? x - noise_start_x: nposm;

            int index = y_start + x;
            uint16_t z16 = depth_data[index];

            if (z16 < min_depth) {
                // no_depth_per_lines[y] ++;
                if (y < max_rows_check_no_depth) {
                    no_depth_per_cols[x] ++;
                }

                if (check_x != nposm) {
                    last_line_xyz2[check_x].x = float_nposm;
                }
                continue;
            }

            float xf = (x - u0) * fdx;
            float yf = (y - v0) * fdy;
            float zf = z16 * depth_scale;
            
            float c_x = zf * xf;
            float c_y = zf * yf;

            SDL_DPoint3 camera_xyz{c_x, c_y, zf};
            // change camera-coor to world-coor
            SDL_DPoint3 world_xyz{camera_xyz.z / 1000.0, camera_xyz.x / -1000.0, camera_xyz.y / -1000.0};

            SDL_DPoint3 world_xyz2 = world_xyz;
            if (!is_float_nposm(dcpitch)) {
                tf2::doTransform_position(transform, world_xyz, world_xyz2);
            }

            if (check_x != nposm) {
                const SDL_DPoint3& last = last_line_xyz2[check_x];
                if (verbose) {
                    if (check_x == check_half_cols) {
                        SDL_Log("{depth_sector_area}xy: (%i, %i) last(%.3f, %.3f, %.3f) this(%.3f, %.3f, %.3f)", 
                            x, y, last.x, last.y, last.z, world_xyz2.x, world_xyz2.y, world_xyz2.z);
                    }
                }
                // if (KDL::Equal(world_xyz2.z, -0.427, 0.0009)) {
                //    int ii = 0;
                // }
                if (last.x != float_nposm) {
                    if (fabs(last.x - world_xyz2.x) > noise_xz_threshold || fabs(last.z - world_xyz2.z) > noise_xz_threshold) {
                        noise_cols ++;
                        if (verbose) {
                            SDL_Log("{depth_sector_area}xy: (%i, %i) last(%.3f, %.3f, %.3f) <=> this(%.3f, %.3f, %.3f) fabs_diff_xz(%.3f, %.3f) now noise_cols: %i", 
                                x, y, last.x, last.y, last.z, world_xyz2.x, world_xyz2.y, world_xyz2.z,
                                fabs(last.x - world_xyz2.x), fabs(last.z - world_xyz2.z), noise_cols);
                        }
                    }
                }
                // update last_line_xyz2
                last_line_xyz2[check_x] = world_xyz2;
            }

            if (world_xyz2.z >= min_z) {
                float theta = atan2(world_xyz2.y, world_xyz2.x);
                if (theta >= scan_msg.angle_min && theta <= scan_msg.angle_max) {
                    float dist_meter = hypot(world_xyz2.x, world_xyz2.y);

                    float angle_positivization = theta - scan_msg.angle_min;
                    int at = angle_positivization / scan_msg.angle_increment;
                    if (at >= node_count) {
                        at = node_count - 1;
                    }
                    VALIDATE(dist_meter > 0, null_str);

                    if (dist_meter >= scan_msg.range_min && dist_meter <= scan_msg.range_max) {
                        if (range_data[at] == 0 || dist_meter < range_data[at]) {
                        
                            range_data[at] = dist_meter;

                            if (points != nullptr) {
                                SDL_FPoint3& tmp_point = points->data[at];
                                tmp_point.x = world_xyz2.x;
                                tmp_point.y = world_xyz2.y;
                                tmp_point.z = world_xyz2.z;
/*
                                if (world_xyz2.x < 0.15 && fabs(world_xyz2.y) < 0.05 && !depth_file_saved) {
                                    char depth_file[256];
                                    SDL_snprintf(depth_file, sizeof(depth_file), "%s/dbg_depth_data-%ix%i.dat", 
                                        game_config::preferences_dir.c_str(), width, height);
                                    save_y16_depth_file(intrinsics, width, height, (const int16_t*)depth_data, depth_scale, dcpitch, depth_file);
                                    depth_file_saved = true;
                                }
*/
                            }

                        }
                        if (mutable_pixels != nullptr) {
                            // ok: yellow
                            mutable_pixels[index * 4] = 0x00;
                            mutable_pixels[index * 4 + 1] = 0xff;
                            mutable_pixels[index * 4 + 2] = 0xff;
                        }

                    } else if (mutable_pixels != nullptr) {
                        // fail: (cyan)becase distance
                        mutable_pixels[index * 4] = 0xff;
                        mutable_pixels[index * 4 + 1] = 0xff;
                        mutable_pixels[index * 4 + 2] = 0x00;
                    }

                } else {
                    // fail: (gray)becase angle
                    if (mutable_pixels != nullptr) {
                        mutable_pixels[index * 4] = 0xa0;
                        mutable_pixels[index * 4 + 1] = 0xa0;
                        mutable_pixels[index * 4 + 2] = 0xa0;
                        // uint32_t color = 0xffa0a0a0;
                        // memcpy(mutable_pixels + index * 4, &color, 4);

                        // ((uint32_t*)mutable_pixels)[index] = 0xffa0a0a0;
                    }
                }
            }
        }

        if (result.noised) {

        } else if (noise_cols >= noise_require_min_cols) {
            // this line maybe noised
            if (maybe_noise_start_y == nposm) {
                // enter check noise
                if (verbose) {
                    SDL_Log("{depth_sector_area}y: %i, this line maybe noised, enter check noise. has noise_lines: %i", y, noise_lines);
                }
                VALIDATE(noise_lines == 0, null_str);
                maybe_noise_start_y = y;
                y_inc = 1;
            } else if (verbose) {
                SDL_Log("{depth_sector_area}y: %i, this line maybe noised. has noise_lines: %i", y, noise_lines);
            }
            noise_lines ++;
            if (noise_lines >= noise_require_min_lines) {
                result.noised = true;
                if (points != nullptr) {
                    // Since it's already noised, can speed it up
                    // but for overlay pixel(mutable_pixels != nullptr), require overlay all line.
                    y_inc = 2;
                }
                if (verbose) {
                    SDL_Log("{depth_sector_area}y: %i, this depth-frame is noised", y);
                }
            }
        } else if (maybe_noise_start_y != nposm) {
            // this line isn't noised
            if (y - maybe_noise_start_y >= noise_max_check_lines - 1) {
                // exit check noise
                if (verbose) {
                    SDL_Log("{depth_sector_area}y: %i, this line isn't noised, exit check noise", y);
                }
                maybe_noise_start_y = nposm;
                // y_inc = 2;
                noise_lines = 0;
            }
        }
    }

    std::map<float, float> theta_dist_ranges;
    if (verbose) {
        for (int at = 0; at < node_count; at ++) {
            float first = (at * scan_msg.angle_increment) + scan_msg.angle_min;
            theta_dist_ranges.insert(std::make_pair(RAD2DEG(first), range_data[at]));
        }
    }

    if (depth_file_saved && !result.noised) {
        int ii = 0;
    }

    const std::string short_dbg_normal_depth_data_dat = "dbg_depth_data.dat";
    if (game_config::os == os_windows && points != nullptr && result.noised && !depth_file_saved) {
        // const std::string short_dbg_normal_depth_data_dat = "dbg_noised_depth_data.dat";
        char depth_file[256];
        SDL_snprintf(depth_file, sizeof(depth_file), "%s/%s", 
            game_config::preferences_dir.c_str(), short_dbg_normal_depth_data_dat.c_str());
        if (game_config::os == os_windows) {
            save_y16_depth_file(intrinsics, width, height, (const int16_t*)depth_data, depth_scale, dcpitch, depth_file);
        }
        depth_file_saved = true;
    }

    const int per50 = width / 2;
    const int hper40 = (width / 2) * 40 / 100;
    const int no_depth_threshold = max_rows_check_no_depth * 65 / 100; // 65
    int left_no_depth_cols = 0;
    for (int x = 0; x < per50; x ++) {
        if (no_depth_per_cols[x] >= no_depth_threshold) {
            left_no_depth_cols ++;
        }
    }
    int right_no_depth_cols = 0;
    for (int x = width - per50; x < width; x ++) {
        if (no_depth_per_cols[x] >= no_depth_threshold) {
            right_no_depth_cols ++;
        }
    }

    result.left_no_depth = left_no_depth_cols >= hper40;
    result.right_no_depth = right_no_depth_cols >= hper40;

    const bool save_dbg_depth_data = result.left_no_depth || result.right_no_depth;
    if (game_config::os == os_windows && points != nullptr && !depth_file_saved && save_dbg_depth_data) {
        char depth_file[256];
        SDL_snprintf(depth_file, sizeof(depth_file), "%s/%s", 
            game_config::preferences_dir.c_str(), short_dbg_normal_depth_data_dat.c_str());
        save_y16_depth_file(intrinsics, width, height, (const int16_t*)depth_data, depth_scale, dcpitch, depth_file);
        depth_file_saved = true;
    }

    return result;
}