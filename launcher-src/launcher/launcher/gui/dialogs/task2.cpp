#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/task2.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/tree.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/messagefs.hpp"
#include "gui/dialogs/if_block2.hpp"

#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "serialization/parser.hpp"

#include "game_config.hpp"

using namespace std::placeholders;

namespace typefield {

enum {type_global, type_state2, type_key_2_state};
enum {field_typeself, field_id, field_name, field_reception_state, field_nonpreemptive, field_recoverable,
	field_startup_state, field_threshold_s, field_doing, field_finished,
	field_async_task, field_aplt_id, field_task_id, field_device_id, field_position1, field_position2,
	field_from_state, field_major_word, field_minor_words, field_strategy,

	field_input_min = 50, field_input_max = 99,
};

}

namespace aplt {

static std::string get_field_str(int type, int field)
{
	if (type == typefield::type_global) {
		if (field == typefield::field_id) {
			return "id";
		} else if (field == typefield::field_name) {
			return _("Name");
		} else if (field == typefield::field_reception_state) {
			return _("Reception state");
		} else if (field == typefield::field_recoverable) {
			return _("Recoverable");
		} else if (field == typefield::field_nonpreemptive) {
			return _("Non-preemptive");
		} else if (field == typefield::field_startup_state) {
			return _("Startup state");
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typefield::type_state2) {
		if (field == typefield::field_typeself) {
			return _("State");
		} else if (field == typefield::field_threshold_s) {
			return _("Overflow time(S)");
		} else if (field == typefield::field_doing) {
			return _("Doing");
		} else if (field == typefield::field_finished) {
			return _("Finished");
		} else if (field == typefield::field_async_task) {
			return _("Async task");
		} else if (field == typefield::field_aplt_id) {
			return _("Applet");
		} else if (field == typefield::field_task_id) {
			return _("Task");

		} else if (field >= typefield::field_input_min && field <= typefield::field_input_max) {
			// const std::pair<std::string, aplt::tif_block>& input = state2.async_task.inputs[field - field_input_min];
			// ss << field_str << ": " << tif_block_to_string(input.second, if_block_input_var, pair, curmap_);
			return null_str;

		} else if (field == typefield::field_device_id) {
			return _("Device ID");
		} else if (field == typefield::field_position1) {
			return _("Possition1");
		} else if (field == typefield::field_position2) {
			return _("Possition2");
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typefield::type_key_2_state) {
		if (field == typefield::field_typeself) {
			return _("key_2_state");
		} else if (field == typefield::field_from_state) {
			return _("From state");
		} else if (field == typefield::field_major_word) {
			return _("Major word");
		} else if (field == typefield::field_minor_words) {
			return _("Minor words");
		} else if (field == typefield::field_strategy) {
			return _("minor_words^Strategy");
		} else {
			VALIDATE(false, null_str);
		}
	} else {
		VALIDATE(false, null_str);
	}

	return null_str;
}

bool tif_block_state_has_nposm(const aplt::tif_block& state)
{
	const std::vector<aplt::tif_branch>& branches = state.branches;
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it) {
		const aplt::tif_branch& branch = *it;
		if (branch.do_to_state == nposm) {
			return true;
		}
	}
	return false;
}

std::string state_is_valid(const aplt::tif_block& state, int states, const std::string& field_str, bool allow_empty, bool allow_nposm, bool allow_same)
{
	state.validate_state(states);
	std::string err_msg = state.is_valid(allow_empty);
	if (!err_msg.empty()) {
		return err_msg;
	}

	utils::string_map symbols;
	symbols["to_state"] = field_str;
	if (!allow_same && state.all_are_same_state()) {
		err_msg = vgettext2("all branches of $to_state can't be in the same state", symbols);
		return err_msg;
	}
	if (!allow_nposm) {
		// if (state.state_has_nposm()) {
		if (tif_block_state_has_nposm(state)) {
			symbols["idle"] = label_state_nposm;
			err_msg = vgettext2("all branches of $to_state must not be in an $idle state", symbols);
			return err_msg;
		}
	}
	return err_msg;
}

uint64_t task_cpp_is_valid(const aplt::ttask_cpp_pair& pair, std::string& err_msg)
{
	SDL_Range task_name_chars_range{MIN_NORMAL_UTF8_NAME_CHARS, MAX_NORMAL_UTF8_NAME_CHARS}; // {2, 12}
	SDL_Range threshold_s_range{TASK_CPP_THRESHOLD_S_MIN, TASK_CPP_THRESHOLD_S_MAX}; // {5, 10*60}

	utils::string_map symbols;
	char buf[256];

	err_msg.clear();
	// std::string id;
	if (!isvalid_normal_id_or_var_name224(pair.id)) {
		return tcookie3f(0, typefield::type_global, typefield::field_id).u64;
	}
	// std::string name;
	// int name_chars = utils::utf8str_len(pair.name);
	// if (name_chars < task_name_chars_range.min || name_chars > task_name_chars_range.max) {
	if (!isvalid_normal_utf8_name224(pair.name)) {
		return tcookie3f(0, typefield::type_global, typefield::field_name).u64;
	}
	// int reception_state;
	if (pair.reception_state < 0 || pair.reception_state >= (int)pair.states.size()) {
		if (pair.reception_state != nposm) {
			return tcookie3f(0, typefield::type_global, typefield::field_reception_state).u64;
		}
	} else {
		const aplt::tcpp_api::tstate2& state2 = pair.states.find(pair.reception_state)->second;
		if (state2.async_task.is_aplt_task) {
			symbols["reception"] = get_field_str(typefield::type_global, typefield::field_reception_state);
			err_msg = vgettext2("$reception must be speech state", symbols);
			return tcookie3f(0, typefield::type_global, typefield::field_reception_state).u64;
		}
	}

	if (false) {
		return tcookie3f(0, typefield::type_global, typefield::field_recoverable).u64;
	}

	err_msg = state_is_valid(pair.startup_state, pair.state_names.size(), get_field_str(typefield::type_global, typefield::field_startup_state), 
		false, true, true);
	if (!err_msg.empty()) {
		return tcookie3f(0, typefield::type_global, typefield::field_startup_state).u64;
	}

	// std::vector<std::string> state_names;
	
	// std::map<int, tcpp_api::tstate2> states;
	for (std::map<int, aplt::tcpp_api::tstate2>::const_iterator it = pair.states.begin(); it != pair.states.end(); ++ it) {
		const aplt::tcpp_api::tstate2& state2 = it->second;

		VALIDATE(state2.state < (int)pair.state_names.size(), null_str);
		VALIDATE(state2.async_task.position1_uuid.empty(), null_str);
		VALIDATE(state2.async_task.position2_uuid.empty(), null_str);

		const std::string& state_name = pair.state_names[state2.state];
		if (state_name.empty() || !utils::is_utf8str(state_name.c_str(), state_name.size())) {
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_typeself).u64;
		}
		if (state2.threshold_s < threshold_s_range.min || state2.threshold_s > threshold_s_range.max) {
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_threshold_s).u64;
		}
		if (!utils::is_utf8str(state2.doing.c_str(), state2.doing.size())) {
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_doing).u64;
		}
		err_msg = state2.finished.is_valid();
		if (!err_msg.empty()) {
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_finished).u64;
		}

		// if (state2.async_task.is_aplt_task) {
		//	err_msg = "async_task's type must be type_aplt_task";
		//	return tcookie3f(state2.state, type_state2, field_async_task).u64;
		// }
		// The subsequent operation will call req_task.validate(), 
		// which strictly checks req_task.aplt and req_task.task, here must make sure them always nullptr. 
		// As for aplt_id and task_id, these will be ignored after tcfg_cpp_api::app_start_task().
		VALIDATE(state2.async_task.aplt == nullptr && state2.async_task.task == nullptr, null_str);

		symbols["position1"] = get_field_str(typefield::type_state2, typefield::field_position1);
		symbols["position2"] = get_field_str(typefield::type_state2, typefield::field_position2);

		err_msg = state2.async_task.position1_if_block.is_valid();
		if (!err_msg.empty()) {
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_position1).u64;
		}
		err_msg = state2.async_task.position2_if_block.is_valid();
		if (!err_msg.empty()) {
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_position2).u64;
		}

		// position is valid
		if (!state2.async_task.position1_if_block.is_valid_position()) {
			err_msg = _("Position format is either a UUID or a variable");
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_position1).u64;
		}
		if (!state2.async_task.position2_if_block.is_valid_position()) {
			err_msg = _("Position format is either a UUID or a variable");
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_position2).u64;
		}

		// if branches >=2, cannot all are same
		if (state2.async_task.position1_if_block.all_are_same_str()) {
			err_msg = vgettext2("all branches of position can't be in the same value", symbols);
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_position1).u64;
		}
		if (state2.async_task.position2_if_block.all_are_same_str()) {
			err_msg = vgettext2("all branches of position can't be in the same value", symbols);
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_position2).u64;
		}

		if (state2.async_task.is_aplt_task) {
			if (!is_bundleid(state2.async_task.aplt_id)) {
				return tcookie3f(state2.state, typefield::type_state2, typefield::field_aplt_id).u64;
			}
			if (!isvalid_normal_id_or_var_name224(state2.async_task.task_id)) {
				return tcookie3f(state2.state, typefield::type_state2, typefield::field_task_id).u64;
			}

			int input_var_index = 0;
			for (std::vector<std::pair<std::string, aplt::tif_block> >::const_iterator it = state2.async_task.input_vars.begin(); it != state2.async_task.input_vars.end(); ++ it, input_var_index ++) {
				const aplt::tif_block& input_var = it->second;
				err_msg = input_var.is_valid();
				if (!err_msg.empty()) {
					return tcookie3f(state2.state, typefield::type_state2, typefield::field_input_min + input_var_index).u64;
				}
			}

			if (!state2.async_task.position1_if_block.str_may_empty()) {
				if (pair.recoverable && !state2.async_task.position2_if_block.str_are_empty()) {
					err_msg = vgettext2("if task is recoverable, $position2 must be empty", symbols);
					return tcookie3f(state2.state, typefield::type_state2, typefield::field_position2).u64;
				}

				const std::set<std::string> position1_s = state2.async_task.position1_if_block.may_strs();
				const std::set<std::string> position2_s = state2.async_task.position2_if_block.may_strs();
				if (position1_s.size() == 1 && position2_s.size() == 1 && *position1_s.begin() == *position2_s.begin()) {
					err_msg = vgettext2("$position1 and $position2 cannot be the same when both have only one position", symbols);
					return tcookie3f(state2.state, typefield::type_state2, typefield::field_position2).u64;
				}
				
			} else {
				if (!state2.async_task.position2_if_block.str_are_empty()) {
					err_msg = vgettext2("When $position1 may be empty, $position2 must be empty", symbols);
					return tcookie3f(state2.state, typefield::type_state2, typefield::field_position2).u64;
				}
			}

		} else {
			
		}

		// rule check
		if (state2.threshold_s < threshold_s_range.min || state2.threshold_s > threshold_s_range.max) {
			SDL_snprintf(buf, sizeof(buf), "[%i, %i]", threshold_s_range.min, threshold_s_range.max);
			symbols["range"] = buf;
			err_msg = vgettext2("This state to perform a single task, the single task is uninterruptible, and the overflow time must be in the range '$range'.", symbols);
			return tcookie3f(state2.state, typefield::type_state2, typefield::field_threshold_s).u64;

		} else if (state2.async_task.is_aplt_task) {
			if (state2.doing.empty()) {
				err_msg = vgettext2("This state to perform a single task, the single task is uninterruptible, and the doing speech must not be empty.", symbols);
				return tcookie3f(state2.state, typefield::type_state2, typefield::field_doing).u64;
			}

		} else {
			// it is interruptable
			if (state2.doing.empty()) {
				err_msg = vgettext2("This state is interruptable, and the doing speech must not be empty.", symbols);
				return tcookie3f(state2.state, typefield::type_state2, typefield::field_doing).u64;
			}
		}
	}

	int index = 0;
	int startup_loops = 0;
	int from_state = nposm;
	int last_from_state_loops = nposm;

	// std::vector<tcpp_api::tkey_2_state> key_2_states;
	std::set<uint64_t> existed_transitions;
	index = 0;
	for (std::vector<aplt::tcpp_api::tkey_2_state>::const_iterator it = pair.key_2_states.begin(); it != pair.key_2_states.end(); ++ it, index ++) {
		const aplt::tcpp_api::tkey_2_state& key_2_state = *it;

		symbols["from_state"] = get_field_str(typefield::type_key_2_state, typefield::field_from_state);

		bool minor_words_is_empty = key_2_state.minor_words.branches.size() == 1 && key_2_state.minor_words.branches[0].do_str.empty();
		if (key_2_state.major_word.empty() && minor_words_is_empty) {
			symbols["major_word"] = get_field_str(typefield::type_key_2_state, typefield::field_major_word);
			symbols["minor_words"] = get_field_str(typefield::type_key_2_state, typefield::field_minor_words);
			err_msg = vgettext2("During the key_2_state, $major_word or $minor_words, at least one of them can not be empty", symbols);
			return tcookie3f(index, typefield::type_key_2_state, typefield::field_major_word).u64;
		}

		if (!utils::is_utf8str(key_2_state.major_word.c_str(), key_2_state.major_word.size())) {
			return tcookie3f(index, typefield::type_key_2_state, typefield::field_major_word).u64;
		}

		err_msg = key_2_state.minor_words.is_valid();
		if (!err_msg.empty()) {
			return tcookie3f(index, typefield::type_key_2_state, typefield::field_minor_words).u64;
		}

		if (!key_2_state.minor_words.is_utf8str()) {
			return tcookie3f(index, typefield::type_key_2_state, typefield::field_minor_words).u64;
		}
	}

	return TCOOKIE3F_CHECK_OK;
}

}

namespace gui2 {

REGISTER_DIALOG(launcher, task2)

std::set<aplt::taplt_var_pair> collect_var_names(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::ttask_cpp_pair& pair)
{
	std::set<aplt::taplt_var_pair> result;

	for (std::map<int, tcode3>::const_iterator it = aplt::BI_env_vars.begin(); it != aplt::BI_env_vars.end(); ++ it) {
		const tcode3& code3 = it->second;
		if (!var_type_is_BI_env_normal(code3.code)) {
			continue;
		}
		const std::string short_var_name = utils::split_app_prefix_id(code3.id).second;
		result.insert(aplt::taplt_var_pair(aplt::fake_aplt, null_str, short_var_name, null_str));
	}

	for (std::map<int, tcode3>::const_iterator it = aplt::BI_task_vars.begin(); it != aplt::BI_task_vars.end(); ++ it) {
		const std::string short_var_name = utils::split_app_prefix_id(it->second.id).second;
		result.insert(aplt::taplt_var_pair(aplt::fake_aplt, null_str, short_var_name, null_str));
	}

	for (std::map<int, aplt::tcpp_api::tstate2>::const_iterator it = pair.states.begin(); it != pair.states.end(); ++ it) {
		const aplt::tcpp_api::tstate2& state2 = it->second;
		const aplt::treq_task& async = state2.async_task;
		if (!state2.async_task.is_aplt_task) {
			continue;
		}

		const aplt::tapplet* aplt = aplt::aplt_from_bundleid(applets, state2.async_task.aplt_id);
		if (aplt == nullptr || aplt->tasks.count(state2.async_task.task_id) == 0) {
			continue;
		}
		const aplt::tapplet::ttask& task = aplt->tasks.find(state2.async_task.task_id)->second;
		for (std::vector<aplt::tapplet::tvar>::const_iterator var_it = task.vars.begin(); var_it != task.vars.end(); ++ var_it) {
			const aplt::tapplet::tvar& var = *var_it;
			if (!var.input) {
				result.insert(aplt::taplt_var_pair(*aplt, task.id, var.name, null_str));
			}
		}

	}
/*
	std::set<std::string> colleced_bundleids;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		if (colleced_bundleids.count(aplt.bundleid) != 0) {
			continue;
		}
		colleced_bundleids.insert(aplt.bundleid);

		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator task_it = aplt.tasks.begin(); task_it != aplt.tasks.end(); ++ task_it) {
			const aplt::tapplet::ttask& task = task_it->second;
			for (std::vector<aplt::tapplet::tvar>::const_iterator var_it = task.vars.begin(); var_it != task.vars.end(); ++ var_it) {
				const aplt::tapplet::tvar& var = *var_it;
				if (!var.input) {
					result.insert(aplt::taplt_var_pair(aplt, var.name, null_str));
				}
			}
		}
	}
*/
	return result;
}

ttask2::ttask2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, std::map<aplt::taplt_key, aplt::tapplet>& applets, 
		const tros_map& curmap, aplt::tcfg_cpp_api& cfg_cpp_api, aplt::tbg_task& bg_task)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, applets_(applets)
	, curmap_(curmap)
	, cfg_cpp_api_(cfg_cpp_api)
	, task_pairs_(cfg_cpp_api.task_pairs())
	, bg_task_(bg_task)
	, label_reception_state_nposm_(std::string("_") + _("Not set"))
	, label_req_task_nposm_(_("Empty"))
	, startup_from_state_index_nposm_(255)
	, cookie3f_nposm_(0, 255, 0) // A value that would normally not be possible
	, task_name_chars_range_(SDL_Range{MIN_NORMAL_UTF8_NAME_CHARS, MAX_NORMAL_UTF8_NAME_CHARS}) // {2, 12}
	, threshold_s_range_(SDL_Range{TASK_CPP_THRESHOLD_S_MIN, TASK_CPP_THRESHOLD_S_MAX}) // {5, 10*60}
	, msgstr_notempty_and_utf8str_(_("Value must not be empty, and utf-8 format string"))
	, msgstr_empty_or_utf8str_(_("Value is empty, or utf-8 format string"))
	, disable_new_klink_task_lock_(aplt::tdisable_new_klink_task_lock::reason_task)
	, pinyin_(aplt::get_curr_pinyin())
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
	, save_widget_(nullptr)
	, insert_task_widget_(nullptr)
	, erase_task_widget_(nullptr)
	, insert_state_widget_(nullptr)
	, insert_key_2_state_widget_(nullptr)
	, move_down_widget_(nullptr)
	, erase_widget_(nullptr)
	, task_widget_(nullptr)
	, curr_tmp_pair_(nullptr)
	, var_val_stack_(nullptr)
	, var_name_widget_(nullptr)
	, var_val_label_(nullptr)
	, var_val_editbox_(nullptr)
	, var_val_dropdown_(nullptr)
	, var_val_dlg_label_(nullptr)
	, var_val_dlg_edit_(nullptr)
	, status_widget_(nullptr)
	, l_tree_(nullptr)
	, r_tree_(nullptr)
	, ignore_var_val_text_changed_(false)
{
	set_timer_interval(1000);

	async_task_types_.insert(std::make_pair(async_task_type_nposm, label_req_task_nposm_));
	async_task_types_.insert(std::make_pair(async_task_type_aplt_task, _("req_task^aplt_task")));
	VALIDATE(async_task_types_.size() == async_task_count, null_str);
}

ttask2::~ttask2()
{
}

std::string unique_task_id(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs)
{
	const std::string uuid = utils::create_uuid(false);
	const std::string prefix = uuid.substr(0, 7);
	int number = 1;
	std::stringstream ss;

	while (true) {
		ss.str("");
		ss << prefix << number;

		if (task_pairs.count(ss.str()) == 0) {
			break;
		}
		number ++;
	}
	return ss.str();
}

std::string unique_task_name(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs)
{
	const std::string prefix = _("Untitle");
	int number = 1;
	std::stringstream ss;

	while (true) {
		ss.str("");
		ss << prefix << number;

		bool found = false;
		for (std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = task_pairs.begin(); it != task_pairs.end(); ++ it) {
			const aplt::ttask_cpp_pair& pair = it->second;
			if (pair.name == ss.str()) {
				found = true;
				break;
			}
		}
		if (!found) {
			break;
		}
		number ++;
	}

	return ss.str();
}

std::string unique_state_name(const aplt::ttask_cpp_pair& pair)
{
	const std::string prefix = _("Untitle");
	int number = 1;
	std::stringstream ss;

	while (true) {
		ss.str("");
		ss << prefix << number;

		bool found = false;
		for (std::vector<std::string>::const_iterator it = pair.state_names.begin(); it != pair.state_names.end(); ++ it) {
			const std::string& name = *it;
			if (name == ss.str()) {
				found = true;
				break;
			}
		}
		if (!found) {
			break;
		}
		number ++;
	}

	return ss.str();
}

void ttask2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	find_widget<tlabel>(window_, "warnning", false, true)->set_label(ht::generate_format(disable_timing_warnning(), 0xffff0000));

	// find_widget<tlabel>(window_, "title", false, true)->set_label(aplt::all_fake_applets.find(aplt::builtinid_task)->second.name);

	tstack* stack = find_widget<tstack>(window_, "var_val_stack", false, true);
	var_val_stack_ = stack;
	var_name_widget_ = find_widget<tlabel>(window_, "var_name", false, true);
	pre_var_val_label(*stack->layer(VAR_VAL_LABEL_LAYER));
	pre_var_val_editbox(*stack->layer(VAR_VAL_EDITBOX_LAYER));
	pre_var_val_dropdown(*stack->layer(VAR_VAL_DROPDOWN_LAYER));
	pre_var_val_dlg(*stack->layer(VAR_VAL_DLG_LAYER));

	tbutton* button = find_widget<tbutton>(window_, "return", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_back
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(window_, "save", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_save
			, this
			, std::ref(*button)));
	button->set_active(false);
	save_widget_ = button;

	button = find_widget<tbutton>(window_, "insert_task", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_insert_task
			, this
			, std::ref(*button)));
	insert_task_widget_ = button;

	button = find_widget<tbutton>(window_, "erase_task", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_erase_task
			, this
			, std::ref(*button)));
	erase_task_widget_ = button;

	button = find_widget<tbutton>(window_, "insert_state", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_insert
			, this
			, std::ref(*button), insert_state));
	insert_state_widget_ = button;

	button = find_widget<tbutton>(window_, "insert_key_2_state", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_insert
			, this
			, std::ref(*button), insert_key_2_state));
	insert_key_2_state_widget_ = button;

	button = find_widget<tbutton>(window_, "move_down", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_move_down
			, this
			, std::ref(*button)));
	move_down_widget_ = button;

	button = find_widget<tbutton>(window_, "erase", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_erase
			, this
			, std::ref(*button)));
	erase_widget_ = button;

	button = find_widget<tbutton>(window_, "import", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_import
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(window_, "export", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_export
			, this
			, std::ref(*button)));

	status_widget_ = find_widget<tlabel>(window_, "status", false, true);

	button = find_widget<tbutton>(window_, "task", false, true);
	// button->set_border("textbox");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_task
			, this
			, std::ref(*button)));
	task_widget_ = button;


	ttree* tree = find_widget<ttree>(window_, "left_tree", false, true);
	tree->set_did_node_changed(std::bind(&ttask2::did_node_changed, this, _2));
	l_tree_ = tree;

	tree = find_widget<ttree>(window_, "right_tree", false, true);
	tree->set_did_node_changed(std::bind(&ttask2::did_node_changed, this, _2));
	r_tree_ = tree;

	empty_val_stack();
	new_task_pairs_loaded();

	refresh_toolbar_active(nullptr);

	bool dirty = task_pairs_dirty();
	save_widget_->set_active(dirty);
}

void ttask2::post_show()
{
}

bool ttask2::task_pairs_dirty() const
{
	if (task_pairs_.size() != tmp_task_pairs_.size()) {
		return true;
	}
	if (task_pairs_.empty()) {
		VALIDATE(tmp_task_pairs_.empty(), null_str);
		return false;
	}

	std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = task_pairs_.begin();
	std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it2 = tmp_task_pairs_.begin();
	for (; it != task_pairs_.end(); ++ it, ++ it2) {
		if (!it->second.equal(it2->second)) {
			return true;
		}
	}

	return false;
}

void ttask2::pre_var_val_label(tgrid& grid)
{
	tlabel* label = find_widget<tlabel>(&grid, "var_val_label", false, true);
	var_val_label_ = label;
}

void ttask2::pre_var_val_editbox(tgrid& grid)
{
	ttext_box2* box2 = new ttext_box2(*window_, *find_widget<tcontrol>(&grid, "var_val_editbox", false, true), 
		"textbox", null_str);
	box2->text_box()->set_maximum_chars(64);
	// box2->text_box()->set_placeholder(_("Operator username"));
	box2->set_did_text_changed(std::bind(&ttask2::did_var_val_text_changed, this, _1));
	var_val_editbox_ = box2;
}

void ttask2::set_status_label(const std::string& label, bool add_t)
{
	if (add_t) {
		std::stringstream ss;
		time_t t = time(nullptr);
		ss << utils::format_time_hms(t) << " ";
		ss << label;
		status_widget_->set_label(ss.str());

	} else {
		status_widget_->set_label(label);
	}
}

void ttask2::pre_var_val_dropdown(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "var_val_dropdown", false, true);
	// button->set_border("textbox");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_var_val_dropdown
			, this
			, std::ref(*button)));
	var_val_dropdown_ = button;
}

void ttask2::pre_var_val_dlg(tgrid& grid)
{
	tlabel* label = find_widget<tlabel>(&grid, "var_val_dlg_label", false, true);
	var_val_dlg_label_ = label;

	tbutton* button = find_widget<tbutton>(&grid, "var_val_dlg_edit", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttask2::click_var_val_dlg_edit
			, this
			, std::ref(*button)));
	var_val_dlg_edit_ = button;
}

const aplt::ttask_cpp_pair* task_pair_by_name(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs, const std::string& name, const aplt::ttask_cpp_pair* exclude_pair)
{
	for (std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = task_pairs.begin(); it != task_pairs.end(); ++ it) {
		const aplt::ttask_cpp_pair& pair = it->second;
		if (&pair == exclude_pair) {
			continue;
		}
		if (pair.name == name) {
			return &pair;
		}
	}
	return nullptr;
}

bool is_state_name_existed(const std::vector<std::string>& names, const std::string& desire_name, const int exclude_at)
{
	int at = 0;
	for (std::vector<std::string>::const_iterator it = names.begin(); it != names.end(); ++ it, at ++) {
		if (at == exclude_at) {
			continue;
		}
		const std::string& name = *it;
		if (name == desire_name) {
			return true;
		}
	}
	return false;
}

void ttask2::new_task_pairs_loaded()
{
	// tmp_task_pairs_ = task_pairs_;
	aplt::evaluate_task_pairs(task_pairs_, tmp_task_pairs_);

	for (std::map<std::string, aplt::ttask_cpp_pair>::iterator it = tmp_task_pairs_.begin(); it != tmp_task_pairs_.end(); ++ it) {
		aplt::ttask_cpp_pair& pair = it->second;
		pair.sync_task_input_vars(applets_);
	}

	aplt::ttask_cpp_pair* to_pair = nullptr;
	if (!tmp_task_pairs_.empty()) {
		std::map<std::string, aplt::ttask_cpp_pair>::iterator it = tmp_task_pairs_.begin();
		// std::advance(it, widget.at());

		to_pair = &it->second;
	}
	curr_tmp_pair_change_to(to_pair);
}

std::string truncate_for_task_id_label(const std::string& id, bool task_cpp)
{
	VALIDATE(!id.empty(), null_str);
	const int max_bytes = task_cpp? 15: 14;
	bool ellipsis = true;
	return utils::truncate_to_max_bytes2(id, max_bytes, ellipsis);
}

std::string truncate_for_task_name_label(const std::string& name, bool task_cpp)
{
	VALIDATE(!name.empty(), null_str);
	const int max_bytes = task_cpp? 23: 20;
	bool ellipsis = true;
	return utils::truncate_to_max_bytes2(name, max_bytes, ellipsis);
}

static std::string truncate_for_task_name2_label(const aplt::ttask_cpp_pair& pair) 
{
	std::stringstream ss;
	ss << truncate_for_task_name_label(pair.name, true) << "(" << truncate_for_task_id_label(pair.id, true) << ")";

	return ss.str();
}

void ttask2::curr_tmp_pair_change_to(aplt::ttask_cpp_pair* pair_ptr)
{
	curr_tmp_pair_ = pair_ptr;
	if (pair_ptr != nullptr) {
		// ttask_cpp_pair_assign(tmp_pair_, *curr_pair_);
		pair_update_to_tree(*curr_tmp_pair_);

		task_widget_->set_label(truncate_for_task_name2_label(*curr_tmp_pair_));

	} else {
		task_widget_->set_label(null_str);

		l_tree_->clear();
		r_tree_->clear();
	}
}

void ttask2::click_back(tbutton& widget)
{
	if (task_pairs_dirty()) {
		std::string msg = _("Is there change in the task data, do you want to save");
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) == gui2::twindow::OK) {
			if (!do_save(*save_widget_, true)) {
				msg = _("There is a data error, and cannot save. Do you want to keep exiting without saving?");
				const int res = gui2::show_messagefs(msg, _("Yes"), _("No"));
				if (res != gui2::twindow::OK) {
					return;
				}
			}
		}
	}

	cfg_cpp_api_.did_gui2task2_back();

	window_->set_retval(twindow::CANCEL);
}

bool ttask2::do_save(tbutton& widget, bool is_back)
{
	VALIDATE(task_pairs_dirty(), null_str);
	if (!tmp_task_pairs_.empty()) {
		VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	} else {
		VALIDATE(curr_tmp_pair_ == nullptr, null_str);
	}

	std::string err_msg;
	for (std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = tmp_task_pairs_.begin(); it != tmp_task_pairs_.end(); ++ it) {
		const aplt::ttask_cpp_pair& pair = it->second;
		uint64_t res = aplt::task_cpp_is_valid(pair, err_msg);
		if (res != TCOOKIE3F_CHECK_OK) {
			tcookie3f cookie3f(res);
			if (curr_tmp_pair_ == &pair) {
				ttree& tree = (cookie3f.type == typefield::type_global || cookie3f.type == typefield::type_state2)? *l_tree_: *r_tree_;
				ttree_node* node = tree.get_root_node().find_node_from_cookie(res);
				VALIDATE(node != nullptr, null_str);
				tree.select_node(node);
				tree.scroll_to_node(*node);
			}
			std::stringstream err;
			if (curr_tmp_pair_ != &pair) {
				err << "[" << ht::generate_format(pair.name2(), 0xffff0000) << "]";
			}
			if (err_msg.empty()) {
				err << get_error_msg(pair, cookie3f.type, cookie3f.field);
			} else {
				err << err_msg;
			}
			set_status_label(err.str(), true);
			return false;
		}
	}

	cfg_cpp_api_.save_task_pairs(tmp_task_pairs_);
	VALIDATE(!task_pairs_dirty(), null_str);

	widget.set_active(false);
	return true;
}

void ttask2::click_save(tbutton& widget)
{
	do_save(widget, false);
}

void ttask2::click_import(tbutton& widget)
{
	const std::string filename = get_klink_cfg_dir(cfgtype_task_cpp, true) + "/task_cpp_import.cfg";

	const tcfg_4field& cfg_4field = klink_cfgs.find(cfgtype_task_cpp)->second;
	utils::string_map symbols;
	symbols["type_cfg"] = cfg_4field.name;
	symbols["file"] = filename;

	const std::string msg = vgettext2("Do you want to import tasks from file($file)", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	std::string err_msg;
	tauto_destruct_executor destruct_executor(std::bind(&did_post_show_err_message, std::ref(err_msg)));

	std::map<std::string, aplt::ttask_cpp_pair> pairs_from_cfg;
	err_msg = cfg_cpp_api_.import_task_cpp_cfg(filename, pairs_from_cfg);
	if (!err_msg.empty()) {
		return;
	}

	// import task_pairs maybe equal task_pairs_.
	// tmp_task_pairs_ = pairs_from_cfg;
	aplt::evaluate_task_pairs(pairs_from_cfg, tmp_task_pairs_);

	curr_tmp_pair_change_to(&tmp_task_pairs_.begin()->second);

	empty_val_stack();
	refresh_toolbar_active(nullptr);

	bool dirty = task_pairs_dirty();
	// if state2 has req_task, and req_task.id is different, @dirty will false always.
	save_widget_->set_active(dirty);
}

void ttask2::click_export(tbutton& widget)
{
	std::stringstream out;
	cfg_cpp_api_.task_pairs_to_stringstream(tmp_task_pairs_, out);

	if (!out.str().empty()) {
		const std::string filename = get_klink_cfg_dir(cfgtype_task_cpp, true) + "/task_cpp_export.cfg";
		write_file(filename, out.str().c_str(), out.str().size());

		utils::string_map symbols;
		symbols["file"] = filename;
		gui2::show_message(null_str, vgettext2("Export finished. file: $file", symbols));
	}
}

void ttask2::click_task(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;
    
	std::vector<aplt::ttask_cpp_pair*> task_pairs_v;

	std::map<std::string, aplt::ttask_cpp_pair>& tmp_task_pairs = tmp_task_pairs_;

	std::map<std::string, std::string> data;
	int at = 0;
	for (std::map<std::string, aplt::ttask_cpp_pair>::iterator it = tmp_task_pairs.begin(); it != tmp_task_pairs.end(); ++ it, at ++) {
		aplt::ttask_cpp_pair& pair = it->second;
		items.push_back(gui2::tmenu::titem(pair.name2(), items.size()));
		task_pairs_v.push_back(&pair);
		if (curr_tmp_pair_->id == pair.id) {
			initial_sel = items.size() - 1;
		}
	}

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();
	aplt::ttask_cpp_pair* curr_pair = task_pairs_v[cursel];

	curr_tmp_pair_change_to(curr_pair);

	empty_val_stack();
	refresh_toolbar_active(nullptr);
}

void ttask2::req_task_update_to_tree(ttree_node& htvi_req_task, int index, const aplt::ttask_cpp_pair& pair, const aplt::tcpp_api::tstate2& state2) const
{
	std::map<std::string, std::string> data;
	ttree_node* htvi = nullptr;

	if (state2.async_task.is_aplt_task) {
		{
			data["label"] = get_node_label_state2(pair, state2, typefield::field_aplt_id);
			htvi = &htvi_req_task.insert_node("default", data);
			htvi->set_child_icon("label", "misc/property.png");
			htvi->set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_aplt_id).u64);
		}

		{
			data["label"] = get_node_label_state2(pair, state2, typefield::field_task_id);
			htvi = &htvi_req_task.insert_node("default", data);
			htvi->set_child_icon("label", "misc/property.png");
			htvi->set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_task_id).u64);
		}

		{
			int input_index = 0;
			for (std::vector<std::pair<std::string, aplt::tif_block> >::const_iterator it = 
				state2.async_task.input_vars.begin(); it != state2.async_task.input_vars.end(); ++ it, input_index ++) {
				const std::string& var_name = it->first;
				const aplt::tif_block& if_block = it->second;

				int field = typefield::field_input_min + input_index;

				data["label"] = get_node_label_state2(pair, state2, field);
				htvi = &htvi_req_task.insert_node("default", data);
				// htvi->set_child_icon("label", "misc/property.png");
				htvi->set_child_icon("label", "misc/variable.png");
				htvi->set_cookie(tcookie3f(index, typefield::type_state2, field).u64);
			}
		}

		{
			data["label"] = get_node_label_state2(pair, state2, typefield::field_device_id);
			htvi = &htvi_req_task.insert_node("default", data);
			htvi->set_child_icon("label", "misc/property.png");
			htvi->set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_device_id).u64);
		}

		{
			data["label"] = get_node_label_state2(pair, state2, typefield::field_position1);
			htvi = &htvi_req_task.insert_node("default", data);
			htvi->set_child_icon("label", "misc/property.png");
			htvi->set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_position1).u64);
		}

		{
			data["label"] = get_node_label_state2(pair, state2, typefield::field_position2);
			htvi = &htvi_req_task.insert_node("default", data);
			htvi->set_child_icon("label", "misc/property.png");
			htvi->set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_position2).u64);
		}

		htvi_req_task.unfold();
	}
}

#define MAX_NODE_CHARS			80
#define MAX_VAL_LABEL_CHARS		256
static std::string truncate_for_field_label(const std::string& label, int max_chars = MAX_NODE_CHARS, bool ellipsis = false)
{
	// text_surface::get_surfaces() have a maximum width limit. see 'max_text_line_width'(8192).
	// some field maybe more characters, the width of the image generated by it can exceed max_text_line_width.
	// for exmaple: tstate2.finished. 
	VALIDATE(max_chars >= MAX_NODE_CHARS, null_str);
	if (label.empty()) {
		return label;
	}
	return utils::truncate_to_max_chars2(label, max_chars, ellipsis);
}

void ttask2::state2_update_to_tree(ttree_node& branch, int index, const aplt::ttask_cpp_pair& pair, const aplt::tcpp_api::tstate2& state2) const
{
	std::map<std::string, std::string> data;

	data["label"] = get_node_label_state2(pair, state2, typefield::field_typeself);
	ttree_node& htvi_state2 = branch.insert_node("default", data);
	htvi_state2.set_child_icon("label", "misc/state.png");
	htvi_state2.set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_typeself).u64);

	data["label"] = get_node_label_state2(pair, state2, typefield::field_threshold_s);
	ttree_node* htvi = &htvi_state2.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_threshold_s).u64);

	data["label"] = get_node_label_state2(pair, state2, typefield::field_doing);
	htvi = &htvi_state2.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_doing).u64);

	// text_surface::get_surfaces() have a maximum width limit. see 'max_text_line_width'(8192).
	// tstate2.finished maybe more characters. The width of the image generated by it can exceed max_text_line_width.
	data["label"] = truncate_for_field_label(get_node_label_state2(pair, state2, typefield::field_finished));
	htvi = &htvi_state2.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_finished).u64);

	data["label"] = get_node_label_state2(pair, state2, typefield::field_async_task);
	ttree_node& htvi_req_task = htvi_state2.insert_node("default", data);
	htvi_req_task.set_child_icon("label", "misc/associate.png");
	htvi_req_task.set_cookie(tcookie3f(index, typefield::type_state2, typefield::field_async_task).u64);
	req_task_update_to_tree(htvi_req_task, index, pair, state2);

	htvi_state2.unfold();
}

void ttask2::key_2_state_update_to_tree(ttree_node& branch, int index, const aplt::ttask_cpp_pair& pair, const std::vector<std::string>& state_names, const aplt::tcpp_api::tkey_2_state& key_2_state) const
{
	std::map<std::string, std::string> data;

	data["label"] = get_node_label_key_2_state(pair, key_2_state, typefield::field_typeself);
	ttree_node& htvi_particular = branch.insert_node("default", data);
	htvi_particular.set_child_icon("label", "misc/speech.png");
	htvi_particular.set_cookie(tcookie3f(index, typefield::type_key_2_state, typefield::field_typeself).u64);

	data["label"] = get_node_label_key_2_state(pair, key_2_state, typefield::field_from_state);
	ttree_node* htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_key_2_state, typefield::field_from_state).u64);

	data["label"] = get_node_label_key_2_state(pair, key_2_state, typefield::field_major_word);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_key_2_state, typefield::field_major_word).u64);

	data["label"] = get_node_label_key_2_state(pair, key_2_state, typefield::field_minor_words);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_key_2_state, typefield::field_minor_words).u64);

	data["label"] = get_node_label_key_2_state(pair, key_2_state, typefield::field_strategy);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_key_2_state, typefield::field_strategy).u64);

	htvi_particular.unfold();
}

std::string ttask2::get_node_label_global(const aplt::ttask_cpp_pair& pair, int field) const
{
	std::stringstream ss;
	const std::string field_str = aplt::get_field_str(typefield::type_global, field);
	if (field == typefield::field_id) {
		ss << field_str << ": " << pair.id;

	} else if (field == typefield::field_name) {
		ss << field_str << ": " << pair.name;

	} else if (field == typefield::field_reception_state) {
		ss << field_str << ": " << get_reception_state_label(pair.state_names, pair.reception_state);

	} else if (field == typefield::field_recoverable) {
		ss << field_str << ": " << (pair.recoverable? _("Yes"): _("No"));
	
	} else if (field == typefield::field_nonpreemptive) {
		ss << field_str << ": " << (pair.nonpreemptive? _("Yes"): _("No"));
	
	} else if (field == typefield::field_startup_state) {
		ss << field_str << ": " << tif_block_to_string(pair.startup_state, if_block_startup_state, pair, curmap_);

	} else {
		VALIDATE(false, null_str);
	}

	return ss.str();
}

std::string ttask2::get_node_label_state2(const aplt::ttask_cpp_pair& pair, const aplt::tcpp_api::tstate2& state2, int field) const
{
	const std::vector<std::string>& state_names = pair.state_names;

	std::stringstream ss;
	
	const std::string field_str = aplt::get_field_str(typefield::type_state2, field);
	if (field == typefield::field_typeself) {
		VALIDATE(state2.state >= 0 && state2.state < (int)state_names.size(), null_str);
		ss << "#" << (state2.state + 1) << ": " << state_names[state2.state];

	} else if (field == typefield::field_threshold_s) {
		ss << field_str << ": " << state2.threshold_s;

	} else if (field == typefield::field_doing) {
		ss << field_str << ": " << state2.doing;

	} else if (field == typefield::field_finished) {
		ss << field_str << ": " << tif_block_to_string(state2.finished, if_block_finished, pair, curmap_);

	} else if (field == typefield::field_async_task) {
		ss << field_str << ": ";
		if (state2.async_task.is_aplt_task) {
			ss << _("req_task^aplt_task");
		} else {
			ss << label_req_task_nposm_;
		}

	} else if (field == typefield::field_aplt_id) {
		ss << field_str << ": ";
		const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets_, state2.async_task.aplt_id);
		if (aplt != nullptr) {
			ss << aplt->name2();

		} else {
			ss << state2.async_task.aplt_id;
		}

	} else if (field == typefield::field_task_id) {
		ss << field_str << ": ";
		
		const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets_, state2.async_task.aplt_id);
		if (aplt != nullptr && aplt->tasks.count(state2.async_task.task_id) != 0) {
			ss << aplt->tasks.find(state2.async_task.task_id)->second.name;
		} else {
			ss << state2.async_task.task_id;
		}

	} else if (field >= typefield::field_input_min && field <= typefield::field_input_max) {
		const std::pair<std::string, aplt::tif_block>& input = state2.async_task.input_vars[field - typefield::field_input_min];
		std::string name = utils::split_app_prefix_id(input.first).second;
		ss << name << ": " << tif_block_to_string(input.second, if_block_input_var, pair, curmap_);

	} else if (field == typefield::field_device_id) {
		ss << field_str << ": " << state2.async_task.ble_device_id;

	} else if (field == typefield::field_position1) {
		ss << field_str << ": " << tif_block_to_string(state2.async_task.position1_if_block, if_block_position, pair, curmap_);

	} else if (field == typefield::field_position2) {
		ss << field_str << ": " << tif_block_to_string(state2.async_task.position2_if_block, if_block_position, pair, curmap_);

	} else {
		VALIDATE(false, null_str);
	}

	return ss.str();
}

std::string ttask2::get_node_label_key_2_state(const aplt::ttask_cpp_pair& pair, const aplt::tcpp_api::tkey_2_state& key_2_state, int field) const
{
	const std::vector<std::string>& state_names = pair.state_names;

	std::stringstream ss;

	const std::string field_str = aplt::get_field_str(typefield::type_key_2_state, field);
	bool show_py_word = false;
	if (field == typefield::field_typeself) {
		ss << field_str;

	} else if (field == typefield::field_from_state) {
		ss << field_str << ": " << (key_2_state.from_state != nposm? state_names[key_2_state.from_state]: label_state_nposm);

	} else if (field == typefield::field_major_word) {
		ss << field_str << ": ";
		if (show_py_word) {
			ss << key_2_state.py_major_word;
		} else {
			ss << key_2_state.major_word;
		}

	} else if (field == typefield::field_minor_words) {
		ss << field_str << ": ";
		if (show_py_word) {
			ss << utils::join(key_2_state.py_minor_words);
		} else {
			// ss << utils::join(key_2_state.minor_words);
			ss << tif_block_to_string(key_2_state.minor_words, if_block_minor_words, pair, curmap_);
		}

	} else if (field == typefield::field_strategy) {
		VALIDATE(aplt::minor_key_strategies.count(key_2_state.strategy) != 0, null_str);
		ss << field_str << ": " << aplt::minor_key_strategies.find(key_2_state.strategy)->second;

	} else {
		VALIDATE(false, null_str);
	}

	return ss.str();
}

void ttask2::pair_update_to_l_tree(const aplt::ttask_cpp_pair& pair)
{
	ttree& l_tree = *l_tree_;
	l_tree.clear();
	
	std::map<std::string, std::string> data;
	ttree_node* htvi;

	// std::stringstream ss;
	// ttree_node& htvi_root = l_tree.insert_node("default", data);
	// htvi_root.set_child_icon("label", "misc/tree/animation.png");
	// htvi_root.set_cookie(PARAM_ANIM);
	ttree_node& htvi_root = l_tree.get_root_node();

	data["label"] = get_node_label_global(pair, typefield::field_id);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_id).u64);

	data["label"] = get_node_label_global(pair, typefield::field_name);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_name).u64);

	data["label"] = get_node_label_global(pair, typefield::field_reception_state);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_reception_state).u64);

	data["label"] = get_node_label_global(pair, typefield::field_nonpreemptive);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_nonpreemptive).u64);

	data["label"] = get_node_label_global(pair, typefield::field_recoverable);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_recoverable).u64);

	data["label"] = get_node_label_global(pair, typefield::field_startup_state);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_startup_state).u64);

	int index = 0;
	for (std::map<int, aplt::tcpp_api::tstate2>::const_iterator it = pair.states.begin(); it != pair.states.end(); ++ it, index ++) {
		const aplt::tcpp_api::tstate2& state2 = it->second;
		state2_update_to_tree(htvi_root, index, pair, state2);
	}
	// htvi_root.unfold();

	validate_l_tree_cookie();
}

void ttask2::pair_update_to_r_tree(const aplt::ttask_cpp_pair& pair)
{
	ttree& r_tree = *r_tree_;
	r_tree.clear();

	ttree_node& r_htvi_root = r_tree.get_root_node();

	int from_state = nposm;
	int childs = 0;

	int index = 0;
	for (std::vector<aplt::tcpp_api::tkey_2_state>::const_iterator it = pair.key_2_states.begin(); it != pair.key_2_states.end(); ++ it, index ++) {
		const aplt::tcpp_api::tkey_2_state& key_2_state = *it;
		key_2_state_update_to_tree(r_htvi_root, index, pair, pair.state_names, key_2_state);
	}

	validate_r_tree_cookie();
}

void ttask2::pair_update_to_tree(const aplt::ttask_cpp_pair& pair)
{
	pair_update_to_l_tree(pair);
	pair_update_to_r_tree(pair);
}

void ttask2::validate_l_tree_cookie() const
{
	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	const aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;
	ttree& l_tree = *l_tree_;
	ttree_node& l_htvi_root = l_tree.get_root_node();

	const std::vector<ttree_node*>& children = l_htvi_root.children();
	const int global_atts = 6;
	VALIDATE(children.size() == global_atts + pair.state_names.size(), null_str);
	int child_at = global_atts;

	// startup next states
	int index = 0;
	for (std::map<int, aplt::tcpp_api::tstate2>::const_iterator it = pair.states.begin(); it != pair.states.end(); ++ it, index ++) {
		const aplt::tcpp_api::tstate2& state2 = it->second;
		VALIDATE(state2.state >= 0 && state2.state < (int)pair.state_names.size(), null_str);
		VALIDATE(state2.state == index, null_str);

		tcookie3f cookie3f(state2.state, typefield::type_state2, typefield::field_typeself);
		VALIDATE(children[child_at]->cookie() == cookie3f.u64, null_str);
		child_at ++;
	}
	VALIDATE(child_at == children.size(), null_str);
}

void ttask2::validate_r_tree_cookie() const
{
	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	const aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;
	ttree& r_tree = *r_tree_;
	ttree_node& r_htvi_root = r_tree.get_root_node();

	const std::vector<ttree_node*>& children = r_htvi_root.children();
	VALIDATE(children.size() == pair.key_2_states.size(), null_str);
	int child_at = 0;

	// startup next states
	int from_state = nposm;

	int index = 0;
	for (std::vector<aplt::tcpp_api::tkey_2_state>::const_iterator it = pair.key_2_states.begin(); it != pair.key_2_states.end(); ++ it, index ++) {
		const aplt::tcpp_api::tkey_2_state& key_2_state = *it;
		VALIDATE(key_2_state.from_state == nposm || (key_2_state.from_state >= 0 && key_2_state.from_state < (int)pair.state_names.size()), null_str);
		// ??
		// VALIDATE(key_2_state.to_state == nposm || (key_2_state.to_state >= 0 && key_2_state.to_state < (int)pair.state_names.size()), null_str);

		tcookie3f cookie3f(index, typefield::type_key_2_state, typefield::field_typeself);
		VALIDATE(children[child_at]->cookie() == cookie3f.u64, null_str);
		child_at ++;
	}

	VALIDATE(child_at == children.size(), null_str);
}

bool is_state_using(const aplt::ttask_cpp_pair& pair, int state, bool& using_reception_state, bool& using_startup_state, const aplt::tcpp_api::tstate2** ppusing_state2, const aplt::tcpp_api::tkey_2_state** ppkey_2_state)
{
	VALIDATE(state >= 0 && state < (int)pair.state_names.size(), null_str);

	using_reception_state = false;
	using_startup_state = false;
	if (ppusing_state2 != nullptr) {
		*ppusing_state2 = nullptr;
	}
	if (ppkey_2_state != nullptr) {
		*ppkey_2_state = nullptr;
	}

	if (pair.reception_state == state) {
		using_reception_state = true;
		return true;
	}

	if (pair.startup_state.is_using_state(state)) {
		using_startup_state = true;
		return true;
	}

	const aplt::tcpp_api::tstate2* using_state2 = nullptr;
	for (std::map<int, aplt::tcpp_api::tstate2>::const_iterator it = pair.states.begin(); it != pair.states.end(); ++ it) {
		const aplt::tcpp_api::tstate2& state2 = it->second;
		if (state2.state == state) {
			continue;
		}
		if (state2.finished.is_using_state(state)) {
			using_state2 = &state2;
		}
	}
	if (using_state2 != nullptr) {
		if (ppusing_state2 != nullptr) {
			*ppusing_state2 = using_state2;
		}
		return true;
	}

	const aplt::tcpp_api::tkey_2_state* using_key_2_state = nullptr;
	for (std::vector<aplt::tcpp_api::tkey_2_state>::const_iterator it = pair.key_2_states.begin(); 
		using_key_2_state == nullptr && it != pair.key_2_states.end(); ++ it) {
		const aplt::tcpp_api::tkey_2_state& key_2_state = *it;
		if (key_2_state.from_state == state) {
			using_key_2_state = &key_2_state;
		}
	}
	if (using_key_2_state != nullptr) {
		if (ppkey_2_state != nullptr) {
			*ppkey_2_state = using_key_2_state;
		}
		return true;
	}

	return false;
}

void ttask2::did_erase_state(aplt::ttask_cpp_pair& pair, int erase_state, tcookie3f& result_cookie3f)
{
	bool using_reception_state = false;
	bool using_startup_state = false;
	bool is_using = is_state_using(pair, erase_state, using_reception_state, using_startup_state, nullptr, nullptr);
	VALIDATE(!is_using, null_str);

	// 1)erase it from pair.states
	std::map<int, aplt::tcpp_api::tstate2>::iterator states_it = pair.states.find(erase_state);
	pair.states.erase(states_it);

	std::vector<std::string>::iterator names_it = pair.state_names.begin();
	if (erase_state != 0) {
		std::advance(names_it, erase_state);
	}
	pair.state_names.erase(names_it);

	// update states
	const int new_state_size = pair.state_names.size();
	for (int state = erase_state; state < new_state_size; state ++) {
		std::map<int, aplt::tcpp_api::tstate2>::iterator states_it = pair.states.find(state + 1);
		aplt::tcpp_api::tstate2 tmp_state2 = states_it->second;
		tmp_state2.state = state;
		pair.states.insert(std::make_pair(tmp_state2.state, tmp_state2));
		pair.states.erase(states_it);
	}
	VALIDATE(pair.states.size() == pair.state_names.size(), null_str);

	for (int state = 0; state < new_state_size; state ++) {
		std::map<int, aplt::tcpp_api::tstate2>::iterator states_it = pair.states.find(state);
		aplt::tcpp_api::tstate2& tmp_state2 = states_it->second;
		tmp_state2.finished.state_sub1(erase_state);
	}

	//
	// 2)pair.reception_state 'sub1'
	//
	if (pair.reception_state > erase_state) {
		pair.reception_state --;
	}
/*
	if (pair.startup_state.state_sub1(erase_state)) {
		// The purpose of 'sub1' is to keep the value in the 'previous' state, 
		// and the 'previous' display is already in the state of the previous value, 
		// so the display content can not be changed
	}
*/
	//
	// 3)pair.startup_state 'sub1'
	//
	if (pair.startup_state.state_sub1(erase_state)) {
		// The purpose of 'sub1' is to keep the value in the 'previous' state, 
		// and the 'previous' display is already in the state of the previous value, 
		// so the display content can not be changed
	}

	//
	// 4)although key_2_states don't have @erase_state, 
	// but these state that more than @erase_state, have to '-1' 
	//
	for (std::vector<aplt::tcpp_api::tkey_2_state>::iterator it = pair.key_2_states.begin(); it != pair.key_2_states.end(); ++ it) {
		aplt::tcpp_api::tkey_2_state& key_2_state = *it;
		if (key_2_state.from_state > erase_state) {
			key_2_state.from_state --;
		}
	}

	pair_update_to_l_tree(pair);

	int select_index = erase_state;

	if (erase_state != (int)pair.state_names.size()) {
		pair_update_to_r_tree(pair);
	} else {
		select_index --;
	}

	if (select_index >= 0) {
		ttree& tree = *l_tree_;
		result_cookie3f = tcookie3f(select_index, typefield::type_state2, typefield::field_typeself);
		ttree_node* node = tree.get_root_node().find_node_from_cookie(result_cookie3f.u64);
		VALIDATE(node != nullptr, null_str);
		tree.select_node(node);
	}
}

void ttask2::empty_val_stack()
{
	var_val_stack_->set_radio_layer(VAR_VAL_LABEL_LAYER);
	var_name_widget_->set_label(null_str);

	utils::string_map symbols;
	symbols["async_task"] = aplt::get_field_str(typefield::type_state2, typefield::field_async_task);
	symbols["threshold"] = aplt::get_field_str(typefield::type_state2, typefield::field_threshold_s);
	const std::string label_msg = vgettext2("task2_help_label $async_task $threshold", symbols);
	var_val_label_->set_label(label_msg);

	// const std::string status_msg = vgettext2("task2_help_status $threshold", symbols);
	const std::string status_msg;
	set_status_label(status_msg);
}

void ttask2::select_r_tree_child_by_at(const aplt::ttask_cpp_pair& pair, int child_at, tcookie3f& result_cookie3f)
{
	ttree& tree = *r_tree_;
	int child_size = pair.key_2_states.size();

	ttree_node& r_htvi_root = tree.get_root_node();
	const std::vector<ttree_node*>& children = r_htvi_root.children();
	VALIDATE((int)children.size() == child_size, null_str);

	if (child_size == 0) {
		return;
	}

	if (child_at >= child_size) {
		child_at = child_size - 1;
	}

	ttree_node& node = r_htvi_root.child(child_at);
	result_cookie3f = tcookie3f(node.cookie());
	tree.select_node(&node);
}

std::string ttask2::get_placeholder_msg(int type, int field) const
{
	utils::string_map symbols;
	char buf[256];
	std::string placeholder;
	if (type == typefield::type_global) {
		if (field == typefield::field_id) {
			SDL_snprintf(buf, sizeof(buf), "[%i, %i]", MAX_NORMAL_ID_OR_VAR_NAME_BYTES, MAX_NORMAL_ID_OR_VAR_NAME_BYTES);
			symbols["range"] = buf;
			placeholder = vgettext2("Letters, numbers, or '_', or '-', and the first character must be a letter, and the number of characters is in the range $range", symbols);

		} else if (field == typefield::field_name) {
			SDL_snprintf(buf, sizeof(buf), "[%i, %i]", task_name_chars_range_.min, task_name_chars_range_.max);
			symbols["range"] = buf;
			placeholder = vgettext2("utf-8 format string, and the number of characters is in the range $range", symbols);

		} else if (field == typefield::field_reception_state) {

		} else if (field == typefield::field_recoverable) {

		} else if (field == typefield::field_nonpreemptive) {

		} else if (field == typefield::field_startup_state) {

		} 

	} else if (type == typefield::type_state2) {
		if (field == typefield::field_typeself) {
			placeholder = msgstr_notempty_and_utf8str_;

		} else if (field == typefield::field_threshold_s) {
			SDL_snprintf(buf, sizeof(buf), "[%i, %i]", threshold_s_range_.min, threshold_s_range_.max);
			symbols["range"] = buf;
			placeholder = vgettext2("Value must be in range $range", symbols);

		} else if (field == typefield::field_doing || field == typefield::field_finished) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == typefield::field_device_id) {
			
		}

	} else if (type == typefield::type_key_2_state) {
		if (field == typefield::field_major_word) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == typefield::field_minor_words) {
			placeholder = _("Empty, or multiple UTF-8 strings. Multiple times used ',' to separate");
		}
	}
	
	return placeholder;
}

std::string ttask2::get_remark_msg(int type, int field) const
{
	if (type == typefield::type_global) {
		if (field == typefield::field_id) {
			return _("global^id remark");
		} else if (field == typefield::field_name) {
			return _("global^name remark");
		} else if (field == typefield::field_reception_state) {
			return _("global^reception_state remark");
		} else if (field == typefield::field_recoverable) {
			return _("global^recoverable remark");
		} else if (field == typefield::field_nonpreemptive) {
			return _("global^nonpreemptive remark");
		} else if (field == typefield::field_startup_state) {
			return _("global^startup_state remark");
		} else {
			VALIDATE(false, null_str);
		}


	} else if (type == typefield::type_state2) {
		if (field == typefield::field_typeself) {
			return _("state2^self remark");
		} else if (field == typefield::field_threshold_s) {
			return _("state2^threshold_s remark");
		} else if (field == typefield::field_doing) {
			return _("state2^doing remark");
		} else if (field == typefield::field_finished) {
			return _("state2^finished remark");
		} else if (field == typefield::field_async_task) {
			return _("state2^async_task.self remark");
		} else if (field == typefield::field_aplt_id) {
			return _("state2^req_task.aplt_id remark");
		} else if (field == typefield::field_task_id) {
			return _("state2^req_task.task_id remark");

		} else if (field >= typefield::field_input_min && field <= typefield::field_input_max) {
			return _("state2^req_task.input_var remark");

		} else if (field == typefield::field_device_id) {
			return _("state2^req_task.device_id remark");
		} else if (field == typefield::field_position1) {
			return _("state2^req_task.position1 remark");
		} else if (field == typefield::field_position2) {
			return _("state2^req_task.position2 remark");
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typefield::type_key_2_state) {
		if (field == typefield::field_typeself) {
			return _("key_2_state^self remark");
		} else if (field == typefield::field_from_state) {
			return _("From state");
		} else if (field == typefield::field_major_word) {
			return _("key_2_state^major_word remark");
		} else if (field == typefield::field_minor_words) {
			return _("key_2_state^minor_words remark");
		} else if (field == typefield::field_strategy) {
			utils::string_map symbols;
			symbols["any_one"] = aplt::minor_key_strategies.find(aplt::mkeys_any_one)->second;
			symbols["all_match_and_order"] = aplt::minor_key_strategies.find(aplt::mkeys_all_match_and_order)->second;
			symbols["all_match_no_order"] = aplt::minor_key_strategies.find(aplt::mkeys_all_match_no_order)->second;
			return vgettext2("key_2_state^strategy remark $any_one, $all_match_and_order, $all_match_no_order", symbols);
		} else {
			VALIDATE(false, null_str);
		}
	} else {
		VALIDATE(false, null_str);
	}

	return null_str;
}

std::string ttask2::get_error_msg(const aplt::ttask_cpp_pair& pair, int type, int field) const
{
	utils::string_map symbols;
	symbols["field"] = aplt::get_field_str(type, field);
	const std::string note = get_placeholder_msg(type, field);

	std::string err_msg;
	if (note.empty()) {
		err_msg = vgettext2("Invalid '$field'", symbols);
	} else {
		symbols["note"] = note;
		err_msg = vgettext2("Invalid '$field'. $note", symbols);
	}
	return err_msg;
}

std::string ttask2::get_reception_state_label(const std::vector<std::string>& state_names, int reception_state) const
{
	if (reception_state == nposm) {
		return label_reception_state_nposm_;

	} else if (reception_state >= 0 && reception_state < (int)state_names.size()) {
		return state_names[reception_state];

	}

	return str_cast(reception_state);
}

void ttask2::click_insert_task(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::vector<const aplt::ttask_cpp_pair*> task_pair_vec;

	aplt::ttask_cpp_pair new_task_pair;
	new_task_pair.id = unique_task_id(tmp_task_pairs_);
	new_task_pair.name = unique_task_name(tmp_task_pairs_);

	const std::map<std::string, aplt::ttask_cpp_pair>& buildin_pairs = cfg_cpp_api_.buildin_task_pairs();

	task_pair_vec.push_back(&new_task_pair);
	items.push_back(gui2::tmenu::titem(_("New task pair"), items.size()));
	if (!buildin_pairs.empty()) {
		for (std::map<std::string, aplt::ttask_cpp_pair>::const_iterator it = buildin_pairs.begin(); it != buildin_pairs.end(); ++ it) {
			const aplt::ttask_cpp_pair& pair = it->second;
			task_pair_vec.push_back(&pair);
			items.push_back(gui2::tmenu::titem(pair.name2(), items.size()));
		}
	}

	int cursel;
	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		cursel = dlg.selected_val();
	}

	const aplt::ttask_cpp_pair& old_pair = *task_pair_vec[cursel];

	std::string dup_field;
	if (tmp_task_pairs_.count(old_pair.id) != 0) {
		dup_field = aplt::get_field_str(typefield::type_global, typefield::field_id);
	}
	if (dup_field.empty() && task_pair_by_name(tmp_task_pairs_, old_pair.name, nullptr) != nullptr) {
		dup_field = aplt::get_field_str(typefield::type_global, typefield::field_name);
	}
	if (!dup_field.empty()) {
		utils::string_map symbols;
		symbols["field"] = dup_field;
		std::string err_msg = vgettext2("There is a '$field' with the same name. Please delete or modify the item before inserting it.", symbols);
		gui2::show_message(null_str, err_msg);
		return;
	}


/*
	const std::string id = unique_task_id(tmp_task_pairs_);
	const std::string name = unique_task_name(tmp_task_pairs_);

	std::pair<std::map<std::string, aplt::ttask_cpp_pair>::iterator, bool> ins = 
		tmp_task_pairs_.insert(std::make_pair(id, aplt::ttask_cpp_pair()));
	VALIDATE(ins.second, null_str);

	aplt::ttask_cpp_pair& pair = ins.first->second;
	pair.id = id;
	pair.name = name;
*/
	std::pair<std::map<std::string, aplt::ttask_cpp_pair>::iterator, bool> ins = 
		tmp_task_pairs_.insert(std::make_pair(old_pair.id, old_pair));
	VALIDATE(ins.second, null_str);

	aplt::ttask_cpp_pair& pair = ins.first->second;

	task_widget_->set_label(truncate_for_task_name2_label(pair));

	curr_tmp_pair_ = &pair;
	// ttask_cpp_pair_assign(tmp_pair_, *curr_pair);
	pair_update_to_tree(*curr_tmp_pair_);

	empty_val_stack();
	refresh_toolbar_active(nullptr);

	bool dirty = task_pairs_dirty();
	save_widget_->set_active(dirty);
}

void ttask2::click_erase_task(tbutton& widget)
{
	VALIDATE(!tmp_task_pairs_.empty(), null_str);
	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;

	utils::string_map symbols;
	symbols["task"] = pair.name2();
	const std::string msg = vgettext2("Do you want erase task($task)?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	VALIDATE(tmp_task_pairs_.count(pair.id) != 0, null_str);
	std::map<std::string, aplt::ttask_cpp_pair>::iterator erase_it = tmp_task_pairs_.find(pair.id);

	aplt::ttask_cpp_pair* next_pair = nullptr;
	if (tmp_task_pairs_.size() > 1) {
		std::map<std::string, aplt::ttask_cpp_pair>::iterator next_id = erase_it;
		next_id ++;
		if (next_id == tmp_task_pairs_.end()) {
			next_id = tmp_task_pairs_.begin();
		}
		next_pair = &next_id->second;
	}
	tmp_task_pairs_.erase(tmp_task_pairs_.find(pair.id));

	// set task's label
	curr_tmp_pair_ = next_pair;
	if (next_pair != nullptr) {
		task_widget_->set_label(truncate_for_task_name2_label(*next_pair));
		pair_update_to_tree(*next_pair);

	} else {
		task_widget_->set_label(null_str);
		l_tree_->clear();
		r_tree_->clear();
	}

	empty_val_stack();
	refresh_toolbar_active(nullptr);

	bool dirty = task_pairs_dirty();
	save_widget_->set_active(dirty);

}

void ttask2::click_insert(tbutton& widget, int type)
{
	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;

	ttree_node* selected_node = nullptr;
	bool l_tree = true;
	if (type == insert_state) {
		int new_state = pair.state_names.size();
		int def_idle_threshold_s = 60;
		std::pair<std::map<int, aplt::tcpp_api::tstate2>::iterator, bool> ins = pair.states.insert(
			std::make_pair(new_state, aplt::tcpp_api::tstate2(new_state, def_idle_threshold_s)));
		VALIDATE(ins.second, null_str);
		pair.state_names.push_back(unique_state_name(pair));

		ttree_node& htvi_root = l_tree_->get_root_node();
		const aplt::tcpp_api::tstate2& state2 = ins.first->second;
		state2_update_to_tree(htvi_root, new_state, pair, state2);

		tcookie3f new_cookie3f(new_state, typefield::type_state2, typefield::field_typeself);
		ttree_node* node = htvi_root.find_node_from_cookie(new_cookie3f.u64);
		VALIDATE(node != nullptr, null_str);
		l_tree_->select_node(node);
		selected_node = node;

	} else {
		VALIDATE(type == insert_key_2_state, null_str);
		l_tree = false;
		if (pair.state_names.empty()) {
			gui2::show_message(null_str, _("It cannot be inserted. At least one state needs to exist."));
			return;
		}

		int initial_to_state = 0;
		pair.key_2_states.push_back(aplt::tcpp_api::tkey_2_state(nposm));
		aplt::tcpp_api::tkey_2_state& key_2_state = pair.key_2_states.back();
		// aplt::tif_branch branch;
		// branch.do_to_state = initial_to_state;

		ttree_node& htvi_root = r_tree_->get_root_node();
		// key_2_state_update_to_tree(htvi_root, pair.key_2_states.size() - 1, pair.state_names, pair.key_2_states.back());
		pair_update_to_r_tree(pair);

		tcookie3f new_cookie3f(pair.key_2_states.size() - 1, typefield::type_key_2_state, typefield::field_typeself);
		ttree_node* node = htvi_root.find_node_from_cookie(new_cookie3f.u64);
		VALIDATE(node != nullptr, null_str);
		r_tree_->select_node(node);
		selected_node = node;

	}

	if (l_tree) {
		validate_l_tree_cookie();
	} else {
		validate_r_tree_cookie();
	}

	deselect_another_tree(l_tree);
	tcookie3f tmp_cookie3f(selected_node->cookie());
	refresh_toolbar_active(&tmp_cookie3f);

	bool dirty = task_pairs_dirty();
	save_widget_->set_active(dirty);
}

void ttask2::click_erase(tbutton& widget)
{
	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	VALIDATE(cookie3f.field == typefield::field_typeself, null_str);

	tcookie3f result_cookie3f = cookie3f_nposm_;
	int select_child_at = nposm;

	bool l_tree = true;
	utils::string_map symbols;
	std::stringstream ss;
	if (cookie3f.type == typefield::type_state2) {
		std::map<int, aplt::tcpp_api::tstate2>::iterator states_it = pair.states.find(cookie3f.index);
		aplt::tcpp_api::tstate2& state2 = states_it->second;
		symbols["state"] = pair.state_names[state2.state];

		bool using_reception_state = false;
		bool using_startup_state = false;
		const aplt::tcpp_api::tstate2* using_state2 = nullptr;
		const aplt::tcpp_api::tkey_2_state* using_key_2_state = nullptr;
		bool is_using = is_state_using(pair, state2.state, using_reception_state, using_startup_state, &using_state2, &using_key_2_state);
		if (is_using) {
			std::string reason;
			if (using_reception_state) {
				reason = aplt::get_field_str(typefield::type_global, typefield::field_reception_state);

			} else if (using_startup_state) {
				reason = aplt::get_field_str(typefield::type_global, typefield::field_startup_state); // _("startup_state");

			} else if (using_state2 != nullptr) {
				reason = pair.state_names[using_state2->state];

			} else {
				VALIDATE(using_key_2_state != nullptr, null_str);
				reason = aplt::get_field_str(typefield::type_key_2_state, typefield::field_typeself); // _("key_2_state");

			}
			symbols["reason"] = reason;
			const std::string msg = vgettext2("Cannot delete '$state'. Because $reason uses that state.", symbols);
			gui2::show_message(null_str, msg);
			return;
		}

		const std::string msg = vgettext2("Do you want erase '$state'?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}

		did_erase_state(pair, state2.state, result_cookie3f);

	} else if (cookie3f.type == typefield::type_key_2_state) {
		l_tree = false;

		std::vector<aplt::tcpp_api::tkey_2_state>::iterator it = pair.key_2_states.begin();
		if (cookie3f.index != 0) {
			std::advance(it, cookie3f.index);
		}

		const aplt::tcpp_api::tkey_2_state& key_2_state = *it;
		ss.str("");
		ss << (key_2_state.from_state == nposm? label_state_nposm: pair.state_names[key_2_state.from_state]) << " --> ";
		// ss << (key_2_state.to_state == nposm? label_state_nposm: pair.state_names[key_2_state.to_state]);
		symbols["field"] = aplt::get_field_str(typefield::type_key_2_state, typefield::field_typeself);
		symbols["next_state"] = ss.str();
		const std::string msg = vgettext2("Do you want erase $field($next_state)?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}

		select_child_at = cookie3f.index;
		pair.key_2_states.erase(it);

		// now gui2::ttree doesn't not support erase single node. regenerate tree.
		pair_update_to_r_tree(pair);

	}

	if (l_tree) {
		validate_l_tree_cookie();

	} else {
		validate_r_tree_cookie();
		select_r_tree_child_by_at(pair, select_child_at, result_cookie3f);
	}

	deselect_another_tree(l_tree);
	refresh_toolbar_active(result_cookie3f != cookie3f_nposm_? &result_cookie3f: nullptr);
	if (result_cookie3f == cookie3f_nposm_) {
		empty_val_stack();
	}

	bool dirty = task_pairs_dirty();
	save_widget_->set_active(dirty);
}

void ttask2::click_move_down(tbutton& widget)
{
	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	VALIDATE(cookie3f.field == typefield::field_typeself, null_str);

	tcookie3f result_cookie3f = cookie3f_nposm_;
	int select_child_at = nposm;

	bool l_tree = true;
	utils::string_map symbols;
	std::stringstream ss;
	if (cookie3f.type == typefield::type_state2) {
		int s1 = cookie3f.index;
		int s2 = s1 + 1;
		VALIDATE(s2 < (int)pair.state_names.size(), null_str);

		pair.state_swap(s1, s2);

		pair_update_to_tree(pair);
		// pair_update_to_l_tree(pair);
		// pair_update_to_r_tree(pair);

		result_cookie3f = tcookie3f(s2, typefield::type_state2, typefield::field_typeself);

		ttree& tree = *l_tree_;
		ttree_node* node = tree.get_root_node().find_node_from_cookie(result_cookie3f.u64);
		VALIDATE(node != nullptr, null_str);
		tree.select_node(node);

	} else if (cookie3f.type == typefield::type_key_2_state) {
		VALIDATE(false, "not support");

	}

	if (l_tree) {
		validate_l_tree_cookie();

	} else {
		VALIDATE(false, null_str);
		validate_r_tree_cookie();
		select_r_tree_child_by_at(pair, select_child_at, result_cookie3f);
	}
	VALIDATE(result_cookie3f != cookie3f_nposm_, null_str);

	deselect_another_tree(l_tree);
	refresh_toolbar_active(&result_cookie3f);

	bool dirty = task_pairs_dirty();
	save_widget_->set_active(dirty);

}

void ttask2::refresh_toolbar_active(const tcookie3f* cookie3f)
{
	if (tmp_task_pairs_.empty()) {
		insert_state_widget_->set_active(false);
		insert_key_2_state_widget_->set_active(false);
		move_down_widget_->set_active(false);
		erase_widget_->set_active(false);

		erase_task_widget_->set_active(false);
		return;
	}

	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	const aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;

	// erase/erase state2/startup_next_state/key_2_state
	insert_state_widget_->set_active(true);
	insert_key_2_state_widget_->set_active(true);

	bool active_movedown = cookie3f != nullptr && cookie3f->field == typefield::field_typeself;
	if (active_movedown) {
		if (cookie3f->type == typefield::type_state2) {
			active_movedown = pair.states.size() >= 2 && cookie3f->index != (int)pair.states.size() - 1;
		} else {
			active_movedown = false;
		}
	}
	if (active_movedown) {
		move_down_widget_->set_cookie(cookie3f->u64);
	}
	move_down_widget_->set_active(active_movedown);

	bool active_erase = cookie3f != nullptr && cookie3f->field == typefield::field_typeself;
	if (active_erase) {
		erase_widget_->set_cookie(cookie3f->u64);
	}
	erase_widget_->set_active(active_erase);

	// insert/erase task
	erase_task_widget_->set_active(true);
}

void ttask2::deselect_another_tree(bool l_tree)
{
	ttree& another_tree = l_tree? *r_tree_: *l_tree_;
	another_tree.select_node(nullptr);
}

void ttask2::did_node_changed(ttree_node& node)
{
	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	const aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;

	uint64_t cookie = node.cookie();
	tcookie3f cookie3f(cookie);

	bool l_tree = true;
	const std::string field_str = aplt::get_field_str(cookie3f.type, cookie3f.field);
	const std::string status_msg = get_remark_msg(cookie3f.type, cookie3f.field);
	std::string var_name;
	std::string var_val_textbox;
	std::string var_val_dropdown;
	std::string var_val_dlg_label;
	int layer = VAR_VAL_EDITBOX_LAYER;
	if (cookie3f.type == typefield::type_global) {
		if (cookie3f.field == typefield::field_id) {
			var_name = field_str;
			var_val_textbox = pair.id;

		} else if (cookie3f.field == typefield::field_name) {
			var_name = field_str;
			var_val_textbox = pair.name;

		} else if (cookie3f.field == typefield::field_reception_state) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			var_val_dropdown = get_reception_state_label(pair.state_names, pair.reception_state);

		} else if (cookie3f.field == typefield::field_recoverable) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			var_val_dropdown = pair.recoverable? _("Yes"): _("No");

		} else if (cookie3f.field == typefield::field_nonpreemptive) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			var_val_dropdown = pair.nonpreemptive? _("Yes"): _("No");

		} else if (cookie3f.field == typefield::field_startup_state) {
			layer = VAR_VAL_DLG_LAYER;
			var_name = field_str;
			var_val_dlg_label = tif_block_to_string(pair.startup_state, if_block_startup_state, pair, curmap_);

		} else {
			VALIDATE(false, null_str);
		}

	} else if (cookie3f.type == typefield::type_state2) {
		std::map<int, aplt::tcpp_api::tstate2>::const_iterator it = pair.states.begin();
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < (int)pair.states.size(), null_str);
		if (cookie3f.index != 0) {
			std::advance(it, cookie3f.index);
		}
		const aplt::tcpp_api::tstate2& state2 = it->second;
		if (cookie3f.field == typefield::field_typeself) {
			var_name = field_str;
			var_val_textbox = curr_tmp_pair_->state_names[state2.state];

		} else if (cookie3f.field == typefield::field_threshold_s) {
			var_name = field_str;
			var_val_textbox = str_cast(state2.threshold_s);

		} else if (cookie3f.field == typefield::field_doing) {
			var_name = field_str;
			var_val_textbox = state2.doing;

		} else if (cookie3f.field == typefield::field_finished) {
			layer = VAR_VAL_DLG_LAYER;
			var_name = field_str;
			var_val_dlg_label = tif_block_to_string(state2.finished, if_block_finished, pair, curmap_);

		} else if (cookie3f.field == typefield::field_async_task) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			int type = async_task_type_nposm;
			if (state2.async_task.is_aplt_task) {
				type = async_task_type_aplt_task;
			}
			var_val_dropdown = async_task_types_.find(type)->second;

		} else if (cookie3f.field == typefield::field_aplt_id) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets_, state2.async_task.aplt_id);
			if (aplt != nullptr) {
				var_val_dropdown = aplt->name2();
			} else {
				var_val_dropdown = state2.async_task.aplt_id;
			}

		} else if (cookie3f.field == typefield::field_task_id) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets_, state2.async_task.aplt_id);
			if (aplt != nullptr && aplt->tasks.count(state2.async_task.task_id) != 0) {
				var_val_dropdown = aplt->tasks.find(state2.async_task.task_id)->second.name;
			} else {
				var_val_dropdown = state2.async_task.task_id;
			}

		} else if (cookie3f.field >= typefield::field_input_min && cookie3f.field <= typefield::field_input_max) {
			layer = VAR_VAL_DLG_LAYER;
			const std::pair<std::string, aplt::tif_block>& input = state2.async_task.input_vars[cookie3f.field - typefield::field_input_min];
			var_name = utils::split_app_prefix_id(input.first).second;
			var_val_dlg_label = tif_block_to_string(input.second, if_block_input_var, pair, curmap_);

		} else if (cookie3f.field == typefield::field_device_id) {
			var_name = field_str;
			var_val_textbox = state2.async_task.ble_device_id;

		} else if (cookie3f.field == typefield::field_position1 || cookie3f.field == typefield::field_position2) {
			layer = VAR_VAL_DLG_LAYER;

			var_name = field_str;
			const aplt::tif_block& position_if_block = cookie3f.field == typefield::field_position1? state2.async_task.position1_if_block: state2.async_task.position2_if_block;
			var_val_dlg_label = tif_block_to_string(position_if_block, if_block_position, pair, curmap_);
/*
			const std::string& position_uuid = cookie3f.field == field_position1? state2.async_task.position1_uuid: state2.async_task.position2_uuid;
			
			if (curmap_.positions.count(position_uuid) != 0) {
				var_val_dropdown = curmap_.positions.find(position_uuid)->second.name;
			} else {
				var_val_dropdown = position_uuid;
			}
*/
		}

	} else if (cookie3f.type == typefield::type_key_2_state) {
		l_tree = false;
		const aplt::tcpp_api::tkey_2_state& key_2_state = pair.key_2_states[cookie3f.index];
		if (cookie3f.field == typefield::field_typeself) {
			layer = VAR_VAL_LABEL_LAYER;
			var_name = field_str;

		} else if (cookie3f.field == typefield::field_from_state) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			const int& state = key_2_state.from_state;
			if (state != nposm) {
				var_val_dropdown = curr_tmp_pair_->state_names[state];
			} else {
				var_val_dropdown = label_state_nposm;
			}

		} else if (cookie3f.field == typefield::field_major_word) {
			var_name = field_str;
			var_val_textbox = key_2_state.major_word;

		} else if (cookie3f.field == typefield::field_minor_words) {
			layer = VAR_VAL_DLG_LAYER;

			var_name = field_str;
			var_val_dlg_label = tif_block_to_string(key_2_state.minor_words, if_block_minor_words, pair, curmap_);

		} else if (cookie3f.field == typefield::field_strategy) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			var_val_dropdown = aplt::minor_key_strategies.find(key_2_state.strategy)->second;
		}

	}

	if (!var_name.empty()) {
		var_val_stack_->set_radio_layer(layer);
		var_name_widget_->set_label(var_name);

		if (layer == VAR_VAL_EDITBOX_LAYER) {
			VALIDATE(var_val_dropdown.empty(), null_str);
			ttext_box& tb = *var_val_editbox_->text_box();
			bool original_is_empty = tb.label().empty();
			tb.set_cookie(cookie);
			tb.set_placeholder(get_placeholder_msg(cookie3f.type, cookie3f.field));

			tignore_var_val_text_changed_lock lock(*this);
			tb.set_label(var_val_textbox);

			if (original_is_empty && var_val_textbox.empty()) {
				// if both before and this are null_str, label_ in tb no changed.
				// set_dirty forec to change display different placeholder.

				tb.fix_show_placeholder();
				// tb.update_canvas();
				// tb.set_dirty();
			}

		} else if (layer == VAR_VAL_DROPDOWN_LAYER) {
			var_val_dropdown_->set_cookie(cookie);
			var_val_dropdown_->set_label(var_val_dropdown);

		} else if (layer == VAR_VAL_DLG_LAYER) {
			var_val_dlg_label_->set_label(truncate_for_field_label(var_val_dlg_label, MAX_VAL_LABEL_CHARS, true));
			var_val_dlg_edit_->set_cookie(cookie);

		} else {
			VALIDATE(layer == VAR_VAL_LABEL_LAYER, null_str);
			var_val_label_->set_label(null_str);
		}
	}
	set_status_label(status_msg);

	refresh_toolbar_active(&cookie3f);
	deselect_another_tree(l_tree);
}

void ttask2::curr_tmp_pair_id_changed()
{
	// id in curr_tmp_pair is changed, but first of tmp_task_pairs_ isn't change.
	// change tmp_task_pairs_.first will result resort.
	std::map<std::string, aplt::ttask_cpp_pair>::iterator hit_it;
	for (std::map<std::string, aplt::ttask_cpp_pair>::iterator it = tmp_task_pairs_.begin(); it != tmp_task_pairs_.end(); ++ it) {
		const aplt::ttask_cpp_pair& pair = it->second;
		if (&pair == curr_tmp_pair_) {
			hit_it = it;
			break;
		}
	}

	VALIDATE(hit_it != tmp_task_pairs_.end(), null_str);

	std::string original_id = hit_it->first;
	VALIDATE(original_id != curr_tmp_pair_->id, null_str);

	aplt::ttask_cpp_pair tmp_pair = *curr_tmp_pair_;

	tmp_task_pairs_.erase(hit_it);
	std::pair<std::map<std::string, aplt::ttask_cpp_pair>::iterator, bool> ins = tmp_task_pairs_.insert(std::make_pair(tmp_pair.id, tmp_pair));
	curr_tmp_pair_ = &ins.first->second;
}

void ttask2::did_var_val_text_changed(ttext_box& widget)
{
	if (ignore_var_val_text_changed_) {
		return;
	}
	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;

	std::string label = widget.label();
	utils::strip(label);

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	bool id_changed = false;
	bool id_or_name_changed = false;
	aplt::tcpp_api::tstate2* state2_name_changed = nullptr;
	std::string new_node_label;
	bool l_tree = true;
	std::string status_msg;
	int n32 = nposm;
	if (cookie3f.type == typefield::type_global) {
		if (cookie3f.field == typefield::field_id) {
			if (isvalid_normal_id_or_var_name224(label)) {
				if (label == pair.id || tmp_task_pairs_.count(label) == 0) {
					// #1(doorbell) -> #2(doorbell#)[invalid] -> #3(doorbell)
					// here, #1 is #3.
					id_changed = label != pair.id;
					id_or_name_changed = id_changed;

					pair.id = label;
					new_node_label = get_node_label_global(pair, typefield::field_id);
					
				} else {
					status_msg = _("'id' cannot be the same");
				}
			} else {
				status_msg = get_error_msg(pair, cookie3f.type, cookie3f.field);
			}

		} else if (cookie3f.field == typefield::field_name) {
			// int name_chars = utils::utf8str_len(label);
			// if (name_chars >= task_name_chars_range_.min && name_chars <= task_name_chars_range_.max) {
			if (isvalid_normal_utf8_name224(label)) {
				if (label == pair.name || task_pair_by_name(tmp_task_pairs_, label, &pair) == nullptr) {
					id_or_name_changed = label != pair.name;

					pair.name = label;
					new_node_label = get_node_label_global(pair, typefield::field_name);
					
				} else {
					status_msg = _("'name' cannot be the same");
				}
			} else {
				status_msg = get_error_msg(pair, cookie3f.type, cookie3f.field);
			}
			
		}

	} else if (cookie3f.type == typefield::type_state2) {
		std::map<int, aplt::tcpp_api::tstate2>::iterator it = pair.states.begin();
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < (int)pair.states.size(), null_str);
		if (cookie3f.index != 0) {
			std::advance(it, cookie3f.index);
		}
		aplt::tcpp_api::tstate2& state2 = it->second;
		if (cookie3f.field == typefield::field_typeself) {
			if (!label.empty()) {
				if (label == pair.state_names[state2.state] || !is_state_name_existed(pair.state_names, label, state2.state)) {
					if (label != pair.state_names[state2.state]) {
						state2_name_changed = &state2;
					}

					pair.state_names[state2.state] = label;
					
				} else {
					status_msg = _("'name' cannot be the same");
				}
			}

		} else if (cookie3f.field == typefield::field_threshold_s) {
			if (utils::isinteger(label)) {
				n32 = utils::to_int(label);
			}
			if (n32 > 0) {
				state2.threshold_s = n32;
			} else {
				status_msg = get_error_msg(pair, cookie3f.type, cookie3f.field);
			}

		} else if (cookie3f.field == typefield::field_doing) {
			if (utils::is_utf8str(label.c_str(), label.size())) {
				state2.doing = label;
			} else {
				status_msg = _("Require utf-8 frmat");
			}

		} else if (cookie3f.field == typefield::field_device_id) {
			if (utils::is_utf8str(label.c_str(), label.size())) {
				state2.async_task.ble_device_id = label;
			} else {
				status_msg = _("Require utf-8 frmat");
			}
		}

		if (status_msg.empty()) {
			new_node_label = get_node_label_state2(pair, state2, cookie3f.field);
		}

	} else if (cookie3f.type == typefield::type_key_2_state) {
		l_tree = false;
		aplt::tcpp_api::tkey_2_state* key_2_state = &pair.key_2_states[cookie3f.index];

		if (cookie3f.field == typefield::field_major_word) {
			if (!label.empty()) {
				if (!utils::is_utf8str(label.c_str(), label.size())) {
					status_msg = _("Require utf-8 frmat");
				}
			}
			if (status_msg.empty()) {
				key_2_state->major_word = label;
				// key_2_state is std::vector, change item#'s std::string member, item's ptr maybe changed.
				key_2_state = &pair.key_2_states[cookie3f.index];
				key_2_state->py_major_word = pair.py_from_utf8str(label);
			}

		} else if (cookie3f.field == typefield::field_minor_words) {
			VALIDATE(false, null_str);
			if (!label.empty()) {
				if (!utils::is_utf8str(label.c_str(), label.size())) {
					status_msg = _("Require utf-8 format");	
				}
			}
			if (status_msg.empty()) {
				std::vector<std::string> vstr = utils::split(label);
				std::vector<std::string> py_vstr;
				for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
					const std::string& key = *it;
					py_vstr.push_back(pair.py_from_utf8str(key));
				}

				// key_2_state->minor_words = vstr;

				// key_2_state is std::vector, change item#'s std::string member, item's ptr maybe changed.
				key_2_state = &pair.key_2_states[cookie3f.index];
				key_2_state->py_minor_words = py_vstr;
			}
		}

		if (status_msg.empty()) {
			key_2_state = &pair.key_2_states[cookie3f.index];
			new_node_label = get_node_label_key_2_state(pair, *key_2_state, cookie3f.field);
		}
	}

/*
	if (l_tree) {
		pair_update_to_l_tree(tmp_pair_);
		ttree_node* node = l_tree_->get_root_node().find_node_from_cookie(cookie);
		l_tree_->select_node(node);
	} else {
		pair_update_to_r_tree(tmp_pair_);
	}
*/
	if (id_changed) {
		curr_tmp_pair_id_changed();
	}

	// curr_tmp_pair_id_changed() maybe change curr_tmp_pair_, so below must use pair2
	const aplt::ttask_cpp_pair& pair2 = *curr_tmp_pair_;

	if (id_or_name_changed) {
		task_widget_->set_label(truncate_for_task_name2_label(pair2));
	}
	if (state2_name_changed != nullptr) {
		if (pair.startup_state.is_using_state(state2_name_changed->state)) {
			tcookie3f startup_state_cookie(0, typefield::type_global, typefield::field_startup_state);
			ttree_node* node = l_tree_->get_root_node().find_node_from_cookie(startup_state_cookie.u64);
			node->set_widget_label("label", get_node_label_global(pair, typefield::field_startup_state));
		}
		pair_update_to_r_tree(pair2);
	}

	if (!new_node_label.empty()) {
		ttree& tree = l_tree? *l_tree_: *r_tree_;
		ttree_node* node = tree.get_root_node().find_node_from_cookie(cookie);
		VALIDATE(node != nullptr, null_str);
		node->set_widget_label("label", new_node_label);
		// l_tree_->select_node(node);
	}
	set_status_label(status_msg);

	bool dirty = task_pairs_dirty();
	save_widget_->set_active(dirty);
}

int mkey_strategy_get_menu_items(int curr_strategy, std::vector<gui2::tmenu::titem>& items)
{
	items.clear();
	int initial_sel = nposm;

	for (std::map<int, std::string>::const_iterator it = aplt::minor_key_strategies.begin(); it != aplt::minor_key_strategies.end(); ++ it) {
		int strategy = it->first;
		const std::string& name = it->second;
		items.push_back(gui2::tmenu::titem(name, strategy));

		if (strategy == curr_strategy) {
			initial_sel = strategy;
		}
	}

	return initial_sel;
}

ttree_node* ttask2::did_async_task_aplt_or_task_changed(ttree& tree, const aplt::ttask_cpp_pair& pair, aplt::tcpp_api::tstate2& state2, const aplt::tapplet& new_aplt,
	ttree_node& aplt_or_task_node, const tcookie3f& cookie3f)
{
	state2.async_task.input_vars.clear();
	for (std::vector<aplt::taplt_var_pair>::const_iterator it = new_aplt.input_vars.begin(); it != new_aplt.input_vars.end(); ++ it) {
		const aplt::taplt_var_pair& var_pair = *it;
		if (var_pair.task_id == state2.async_task.task_id) {
			state2.async_task.input_vars.push_back(std::make_pair(var_pair.var2, aplt::tif_block()));
		}
	}
	ttree_node& async_task_node = aplt_or_task_node.parent_node();
	async_task_node.erase_children();
	req_task_update_to_tree(async_task_node, cookie3f.index, pair, state2);

	ttree_node* new_node = tree.get_root_node().find_node_from_cookie(cookie3f.u64);
	tree.select_node(new_node);

	return new_node;
}

void ttask2::click_var_val_dropdown(tbutton& widget)
{
	aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	std::vector<const tmap_position*> vec_positions;
	std::vector<const aplt::tapplet*> vec_aplts;
	std::vector<const aplt::tapplet::ttask*> vec_tasks;

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	enum {index_yes, index_no};
	bool l_tree = true;
	aplt::tcpp_api::tstate2* state2 = nullptr;
	aplt::tcpp_api::tkey_2_state* key_2_state = nullptr;

	if (cookie3f.type == typefield::type_global) {
		if (cookie3f.field == typefield::field_reception_state) {
			items.push_back(gui2::tmenu::titem(label_reception_state_nposm_, pair.state_names.size()));

			for (std::vector<std::string>::const_iterator it = pair.state_names.begin(); it != pair.state_names.end(); ++ it) {
				const std::string& name = *it;
				items.push_back(gui2::tmenu::titem(name, items.size() - 1));
			}

			const int& state = pair.reception_state;
			if (state >= 0 && state < (int)pair.state_names.size()) {
				initial_sel = state;

			} else if (state == nposm) {
				initial_sel = pair.state_names.size();
			}

		} else if (cookie3f.field == typefield::field_recoverable || cookie3f.field == typefield::field_nonpreemptive) {
			items.push_back(gui2::tmenu::titem(_("Yes"), index_yes));
			items.push_back(gui2::tmenu::titem(_("No"), index_no));
			
			bool value = cookie3f.field == typefield::field_recoverable? pair.recoverable: pair.nonpreemptive;
			if (value) {
				initial_sel = index_yes;
			} else {
				initial_sel = index_no;
			}
		} else {
			VALIDATE(false, null_str);
		}

	} else if (cookie3f.type == typefield::type_state2) {
		std::map<int, aplt::tcpp_api::tstate2>::iterator it = curr_tmp_pair_->states.begin();
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < (int)curr_tmp_pair_->states.size(), null_str);
		if (cookie3f.index != 0) {
			std::advance(it, cookie3f.index);
		}
		state2 = &it->second;

		if (cookie3f.field == typefield::field_async_task) {
			for (std::map<int, std::string>::const_iterator it = async_task_types_.begin(); it != async_task_types_.end(); ++ it) {
				int type = it->first;
				const std::string& name = it->second;

				items.push_back(gui2::tmenu::titem(name, type));
			}
			if (state2->async_task.is_aplt_task) {
				initial_sel = async_task_type_aplt_task;
			} else {
				initial_sel = async_task_type_nposm;
			}

		} else if (cookie3f.field == typefield::field_aplt_id) {
			std::set<std::string> bundleids;

			std::vector<const aplt::tapplet*> aplts;
			aplts.push_back(&aplt::fake_aplt);
			for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
				const aplt::tapplet& aplt = it->second;
				aplts.push_back(&aplt);
			}

			// for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
			for (std::vector<const aplt::tapplet*>::const_iterator it = aplts.begin(); it != aplts.end(); ++ it) {
				const aplt::tapplet& aplt = **it;
				if (bundleids.count(aplt.bundleid) != 0) {
					continue;
				}
				bundleids.insert(aplt.bundleid);

				vec_aplts.push_back(&aplt);
				items.push_back(gui2::tmenu::titem(aplt.name2(), items.size()));
				if (state2->async_task.aplt_id == aplt.bundleid) {
					initial_sel = items.size() - 1;
				}
			}

		} else if (cookie3f.field == typefield::field_task_id) {
			const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets_, state2->async_task.aplt_id);
			// applet of 'async_task.aplt_id' may not be installed. it will result 'aplt == nulltpr'.

			if (aplt != nullptr) {
				for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it = aplt->tasks.begin(); it != aplt->tasks.end(); ++ it) {
					const aplt::tapplet::ttask& task = it->second;
					if (task.type == aplt::task_cpp) {
						continue;
					}

					vec_tasks.push_back(&task);
					items.push_back(gui2::tmenu::titem(task.name, items.size()));
					if (state2->async_task.task_id == task.id) {
						initial_sel = items.size() - 1;
					}
				}
			}

		} else if (cookie3f.field == typefield::field_position1 || cookie3f.field == typefield::field_position2) {
			const bool p1 = cookie3f.field == typefield::field_position1;

			const std::string& task_this_position = p1? state2->async_task.position1_uuid: state2->async_task.position2_uuid;
			const std::string& task_other_position = p1? state2->async_task.position2_uuid: state2->async_task.position1_uuid;

			if (!p1 || task_other_position.empty()) {
				items.push_back(gui2::tmenu::titem(_("Empty"), curmap_.positions.size()));
				initial_sel = task_this_position.empty()? items.back().val: nposm;
			}
			for (std::map<std::string, tmap_position>::const_iterator it = curmap_.positions.begin(); it != curmap_.positions.end(); ++ it) {
				const tmap_position& position = it->second;
				if (position.uuid == task_other_position) {
					continue;
				}
		
				items.push_back(gui2::tmenu::titem(position.name, vec_positions.size()));
				vec_positions.push_back(&position);

				if (position.uuid == task_this_position) {
					VALIDATE(initial_sel == nposm, null_str);
					initial_sel = vec_positions.size() - 1;
				}
			}

		} else {
			VALIDATE(false, null_str);
		}

	} else if (cookie3f.type == typefield::type_key_2_state) {
		l_tree = false;

		key_2_state = &curr_tmp_pair_->key_2_states[cookie3f.index];
		if (cookie3f.field == typefield::field_from_state) {
			items.push_back(gui2::tmenu::titem(label_state_nposm, pair.state_names.size()));

			for (std::vector<std::string>::const_iterator it = pair.state_names.begin(); it != pair.state_names.end(); ++ it) {
				const std::string& name = *it;
				items.push_back(gui2::tmenu::titem(name, items.size() - 1));
			}

			const int& state = key_2_state->from_state;
			if (state != nposm) {
				initial_sel = state;
			} else {
				initial_sel = pair.state_names.size();
			}

		} else if (cookie3f.field == typefield::field_strategy) {
			initial_sel = mkey_strategy_get_menu_items(key_2_state->strategy, items);

		} else {
			VALIDATE(false, null_str);
		}

	} else {
		VALIDATE(false, null_str);
	}

	if (items.empty()) {
		return;
	}

	int new_val = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		new_val = dlg.selected_val();
	}

	std::string new_node_label;
	ttree& tree = l_tree? *l_tree_: *r_tree_;
	ttree_node* node = tree.get_root_node().find_node_from_cookie(cookie);
	VALIDATE(node != nullptr, null_str);

	std::string new_dropdown_label;

	if (cookie3f.type == typefield::type_global) {
		if (cookie3f.field == typefield::field_reception_state) {
			int& state = pair.reception_state;

			int new_type = new_val;
			if (new_val != pair.state_names.size()) {
				new_dropdown_label = pair.state_names[new_val];
			} else {
				new_dropdown_label = label_reception_state_nposm_;
				new_type = nposm;
			}
			state = new_type;

		} else if (cookie3f.field == typefield::field_recoverable || cookie3f.field == typefield::field_nonpreemptive) {
			bool& target = cookie3f.field == typefield::field_recoverable? pair.recoverable: pair.nonpreemptive;
			target = new_val == index_yes;
			new_dropdown_label = target? _("Yes"): _("No");
		}
		new_node_label = get_node_label_global(pair, cookie3f.field);

	} else if (cookie3f.type == typefield::type_state2) {
		if (cookie3f.field == typefield::field_async_task) {
			int new_type = new_val;
			VALIDATE(new_type >= 0 && new_type < async_task_count, null_str);

			if (new_type == async_task_type_aplt_task) {
				state2->async_task.is_aplt_task = true;
			} else {
				VALIDATE(new_type == async_task_type_nposm, null_str);
				state2->async_task.is_aplt_task = false;
			}
			new_dropdown_label = async_task_types_.find(new_type)->second;

			node->erase_children();
			req_task_update_to_tree(*node, cookie3f.index, *curr_tmp_pair_, *state2);

		} else if (cookie3f.field == typefield::field_aplt_id) {
			const aplt::tapplet* new_aplt = vec_aplts[new_val];
			new_dropdown_label = new_aplt->name2();

			state2->async_task.aplt_id = new_aplt->bundleid;

			// aplt_id changed, current task_id myabe not exist.
			ttree_node* node2 = tree.get_root_node().find_node_from_cookie(tcookie3f(cookie3f.index, cookie3f.type, typefield::field_task_id).u64);
			VALIDATE(node2 != nullptr, null_str);
			std::string new_node2_label = get_node_label_state2(pair, *state2, typefield::field_task_id);
			node2->set_widget_label("label", new_node2_label);

			node = did_async_task_aplt_or_task_changed(tree, *curr_tmp_pair_, *state2, *new_aplt, *node, cookie3f);

		} else if (cookie3f.field == typefield::field_task_id) {
			const aplt::tapplet::ttask* new_task = vec_tasks[new_val];
			new_dropdown_label = new_task->name;

			state2->async_task.task_id = new_task->id;

			const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets_, state2->async_task.aplt_id);
			node = did_async_task_aplt_or_task_changed(tree, *curr_tmp_pair_, *state2, *aplt, *node, cookie3f);

		} else if (cookie3f.field == typefield::field_position1 || cookie3f.field == typefield::field_position2) {
			std::string new_position_uuid;
			if (new_val != curmap_.positions.size()) {
				const tmap_position& new_position = *vec_positions[new_val];
				new_position_uuid = new_position.uuid;

				new_dropdown_label = curmap_.positions.find(new_position_uuid)->second.name;
			}

			std::string& position_key = cookie3f.field == typefield::field_position1? state2->async_task.position1_uuid: state2->async_task.position2_uuid;
			position_key = new_position_uuid;
		}
		new_node_label = get_node_label_state2(pair, *state2, cookie3f.field);

	} else if (cookie3f.type == typefield::type_key_2_state) {
		if (cookie3f.field == typefield::field_from_state) {
			int& state = key_2_state->from_state;

			int new_type = new_val;
			if (new_val != pair.state_names.size()) {
				new_dropdown_label = pair.state_names[new_val];
			} else {
				new_dropdown_label = label_state_nposm;
				new_type = nposm;
			}
			state = new_type;

		} else if (cookie3f.field == typefield::field_strategy) {
			key_2_state->strategy = new_val;

			new_dropdown_label = aplt::minor_key_strategies.find(key_2_state->strategy)->second;

		} else {
			VALIDATE(false, null_str);
		}
		new_node_label = get_node_label_key_2_state(pair, *key_2_state, cookie3f.field);

	} else {
		VALIDATE(false, null_str);
	}

	node->set_widget_label("label", new_node_label);
	widget.set_label(new_dropdown_label);

	if (l_tree) {
		validate_l_tree_cookie();
	} else {
		validate_r_tree_cookie();
	}

	bool dirty = task_pairs_dirty();
	save_widget_->set_active(dirty);
}

bool ttask2::show_if_block2_dlg(const aplt::ttask_cpp_pair& pair, const std::string& title, aplt::tif_block& if_block, int if_block_type)
{
	std::set<aplt::taplt_var_pair> vars = collect_var_names(applets_, pair);
	gui2::tif_block2 dlg(rdpd_mgr_, pble_, privacy_, vars, pair, curmap_, title, nullptr, if_block, if_block_type);
	dlg.show();
	if (dlg.get_retval() != twindow::OK) {
		return false;
	}
	return true;
}

void ttask2::click_var_val_dlg_edit(tbutton& widget)
{
	aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;

	std::stringstream ss;

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);
	const std::string field_str = aplt::get_field_str(cookie3f.type, cookie3f.field);

	std::string new_node_label;
	bool l_tree = true;
	if (cookie3f.type == typefield::type_global) {
		if (cookie3f.field == typefield::field_startup_state) {
			if (!show_if_block2_dlg(pair, aplt::get_field_str(typefield::type_global, typefield::field_startup_state), pair.startup_state, if_block_startup_state)) {
				return;
			}
		}
		new_node_label = get_node_label_global(pair, cookie3f.field);

	} else if (cookie3f.type == typefield::type_state2) {
		std::map<int, aplt::tcpp_api::tstate2>::iterator it = curr_tmp_pair_->states.begin();
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < (int)curr_tmp_pair_->states.size(), null_str);
		if (cookie3f.index != 0) {
			std::advance(it, cookie3f.index);
		}
		aplt::tcpp_api::tstate2& state2 = it->second;

		if (cookie3f.field >= typefield::field_input_min && cookie3f.field <= typefield::field_input_max) {
			std::pair<std::string, aplt::tif_block>& input = state2.async_task.input_vars[cookie3f.field - typefield::field_input_min];
			if (!show_if_block2_dlg(pair, input.first, input.second, if_block_input_var)) {
				return;
			}

		} else if (cookie3f.field == typefield::field_finished) {
			if (!show_if_block2_dlg(pair, aplt::get_field_str(typefield::type_state2, typefield::field_finished), state2.finished, if_block_finished)) {
				return;
			}

		} else if (cookie3f.field == typefield::field_position1 || cookie3f.field == typefield::field_position2) {
			const bool p1 = cookie3f.field == typefield::field_position1;
			aplt::tif_block& if_block = p1? state2.async_task.position1_if_block: state2.async_task.position2_if_block;

			if (!show_if_block2_dlg(pair, field_str, if_block, if_block_position)) {
				return;
			}
		}

		new_node_label = get_node_label_state2(pair, state2, cookie3f.field);

	} else if (cookie3f.type == typefield::type_key_2_state) {
		l_tree = false;

		aplt::tcpp_api::tkey_2_state& key_2_state = pair.key_2_states[cookie3f.index];

		if (cookie3f.field == typefield::field_minor_words) {
			const aplt::tif_block orig_minor_words = key_2_state.minor_words;
			if (!show_if_block2_dlg(pair, field_str, key_2_state.minor_words, if_block_minor_words)) {
				return;
			}
			if (key_2_state.minor_words != orig_minor_words) {
				aplt::word_match_did_minor_words_changed(pinyin_, tone_, eng_lowercase_, key_2_state);
			}
			
		}

		new_node_label = get_node_label_key_2_state(pair, key_2_state, cookie3f.field);

	} else {
		VALIDATE(false, null_str);
	}

	ttree& tree = l_tree? *l_tree_: *r_tree_;
	ttree_node* node = tree.get_root_node().find_node_from_cookie(cookie);
	VALIDATE(node != nullptr, null_str);

	node->set_widget_label("label", truncate_for_field_label(new_node_label));
	var_val_dlg_label_->set_label(truncate_for_field_label(new_node_label, MAX_VAL_LABEL_CHARS, true));

	if (l_tree) {
		validate_l_tree_cookie();
	} else {
		validate_r_tree_cookie();
	}

	bool dirty = task_pairs_dirty();
	save_widget_->set_active(dirty);
}

void ttask2::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

