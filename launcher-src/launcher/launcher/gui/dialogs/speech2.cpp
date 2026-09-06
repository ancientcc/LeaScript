#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/speech2.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/tree.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/messagefs.hpp"

#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "serialization/parser.hpp"

#include "game_config.hpp"

using namespace std::placeholders;


namespace typefield {

enum {type_global, type_var};
enum {field_typeself, field_id, field_name, field_countdown_s, field_first_words, 
	field_major_word, field_minor_words, field_strategy,
	field_optional, field_aplt_id, field_first_words_is_prefix, field_prefix_words, field_postfix_words
};

}

namespace aplt {
enum {match_method_var, match_method_simple_word, match_method_count};
static std::map<int, std::string> match_methods_;

static void init_match_methods()
{
	if (match_methods_.empty()) {
		match_methods_.insert(std::make_pair(match_method_var, _("matchmethod^var")));
		match_methods_.insert(std::make_pair(match_method_simple_word, _("matchmethod^simple word")));
	}
	VALIDATE(match_methods_.size() == match_method_count, null_str);
}

static std::string get_field_str(int type, int field)
{
	if (type == typefield::type_global) {
		if (field == typefield::field_id) {
			return "id";
		} else if (field == typefield::field_name) {
			return _("Name");
		} else if (field == typefield::field_countdown_s) {
			return _("Countdown(S)");
		} else if (field == typefield::field_first_words) {
			return _("Fist words");
		} else if (field == typefield::field_major_word) {
			return _("Major word");
		} else if (field == typefield::field_minor_words) {
			return _("Minor words");
		} else if (field == typefield::field_strategy) {
			return _("minor_words^Strategy");
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typefield::type_var) {
		if (field == typefield::field_typeself) {
			return _("Variable");
		} else if (field == typefield::field_aplt_id) {
			return _("Applet");
		} else if (field == typefield::field_name) {
			return _("Name");
		} else if (field == typefield::field_optional) {
			return _("Optional");
		} else if (field == typefield::field_first_words_is_prefix) {
			return _("First words is prefix");
		} else if (field == typefield::field_prefix_words) {
			return _("Prefix words");
		} else if (field == typefield::field_postfix_words) {
			return _("Postfix words");
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

uint64_t speech_sensor_is_valid(const aplt::tspeech_sensor& sensor, std::string& err_msg)
{
	init_match_methods();

	SDL_Range task_name_chars_range{MIN_NORMAL_UTF8_NAME_CHARS, MAX_NORMAL_UTF8_NAME_CHARS}; // {2, 12}

	utils::string_map symbols;
	// char buf[256];

	err_msg.clear();
	// std::string id;
	if (!isvalid_normal_id_or_var_name224(sensor.id)) {
		return tcookie3f(0, typefield::type_global, typefield::field_id).u64;
	}
	// std::string name;
	// int name_chars = utils::utf8str_len(sensor.name);
	// if (name_chars < task_name_chars_range.min || name_chars > task_name_chars_range.max) {
	if (!isvalid_normal_utf8_name224(sensor.name)) {
		return tcookie3f(0, typefield::type_global, typefield::field_name).u64;
	}

	// std::string first_words
	const std::string first_words_str = utils::join(sensor.first_words);
	if (!utils::is_utf8str(first_words_str.c_str(), first_words_str.size())) {
		return tcookie3f(0, typefield::type_global, typefield::field_first_words).u64;
	}

	// std::string major_word
	if (!utils::is_utf8str(sensor.major_word.c_str(), sensor.major_word.size())) {
		return tcookie3f(0, typefield::type_global, typefield::field_major_word).u64;
	}

	// std::string minor_words
	const std::string minor_words_str = utils::join(sensor.minor_words);
	if (!utils::is_utf8str(minor_words_str.c_str(), minor_words_str.size())) {
		return tcookie3f(0, typefield::type_global, typefield::field_minor_words).u64;
	}

	const int valid_vars = sensor.valid_vars();
	if (valid_vars == 0) {
		if (sensor.major_word.empty() && sensor.minor_words.empty()) {
			symbols["method"] = match_methods_.find(match_method_simple_word)->second;
			symbols["major_word"] = get_field_str(typefield::type_global, typefield::field_major_word);
			symbols["minor_words"] = get_field_str(typefield::type_global, typefield::field_minor_words);
			err_msg = vgettext2("Using $method, $major_word or $minor_words on left, at least one of them can not be empty", symbols);
			return tcookie3f(0, typefield::type_global, typefield::field_major_word).u64;
		}

	} else {
		if (!sensor.major_word.empty() || !sensor.minor_words.empty()) {
			symbols["method"] = match_methods_.find(match_method_var)->second;
			symbols["major_word"] = get_field_str(typefield::type_global, typefield::field_major_word);
			symbols["minor_words"] = get_field_str(typefield::type_global, typefield::field_minor_words);
			err_msg = vgettext2("Using $method, both $major_word and $minor_words on left must be empty", symbols);
			return tcookie3f(0, typefield::type_global, typefield::field_major_word).u64;
		}
	}

	// var1, var2, var3
	std::set<std::string> existed_name2s;
	bool has_optional = false;
	for (int at = 0; at < valid_vars; at ++) {
		const aplt::tspeech_sensor::tvar& var = sensor.var(at);
		std::string name2 = utils::join_app_prefix_id(var.aplt_id, var.name);
		if (existed_name2s.count(name2) != 0) {
			err_msg = _("During the variable, There cannot be two identical name2");
			return tcookie3f(at, typefield::type_var, typefield::field_aplt_id).u64;
		}
		existed_name2s.insert(name2);

		if (!var.optional) {
			if (has_optional) {
				err_msg = _("Once a variable is optional, subsequent variables must be optional");
				return tcookie3f(at, typefield::type_var, typefield::field_optional).u64;
			}

		} else {
			if (at == 0) {
				err_msg = _("The first variable cannot be optional");
				return tcookie3f(at, typefield::type_var, typefield::field_optional).u64;
			}
			if (!has_optional) {
				has_optional = true;
			}
		}

		if (var.first_words_is_prefix) {
			if (at != 0) {
				symbols["first_words_is_prefix"] = get_field_str(typefield::type_var, typefield::field_first_words_is_prefix);
				err_msg = vgettext2("Only first variable can be '$first_words_is_prefix' is true", symbols);
				return tcookie3f(at, typefield::type_var, typefield::field_first_words_is_prefix).u64;
			}
		}
		// prefix_words
		const std::string prefix_words_str = utils::join(var.prefix_words);
		if (!utils::is_utf8str(prefix_words_str.c_str(), prefix_words_str.size())) {
			return tcookie3f(at, typefield::type_var, typefield::field_minor_words).u64;
		}
		// postfix_words
		const std::string postfix_words_str = utils::join(var.postfix_words);
		if (!utils::is_utf8str(postfix_words_str.c_str(), postfix_words_str.size())) {
			return tcookie3f(at, typefield::type_var, typefield::field_minor_words).u64;
		}
	}

	return TCOOKIE3F_CHECK_OK;
}

}

namespace gui2 {

extern int mkey_strategy_get_menu_items(int curr_strategy, std::vector<gui2::tmenu::titem>& items);

REGISTER_DIALOG(launcher, speech2)

tspeech2::tspeech2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, std::map<aplt::taplt_key, aplt::tapplet>& applets, 
		const tros_map& curmap, aplt::tcfg_cpp_api& cfg_cpp_api, aplt::tbg_task& bg_task)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, applets_(applets)
	, curmap_(curmap)
	, cfg_cpp_api_(cfg_cpp_api)
	, nick_(cfg_cpp_api.nick())
	, speech_sensors_(cfg_cpp_api.speech_sensors())
	, bg_task_(bg_task)
	, label_req_task_nposm_(_("Empty"))
	, startup_from_state_index_nposm_(255)
	, cookie3f_nposm_(0, 255, 0) // A value that would normally not be possible
	, task_name_chars_range_(SDL_Range{MIN_NORMAL_UTF8_NAME_CHARS, MAX_NORMAL_UTF8_NAME_CHARS}) // {2, 12}
	, countdown_s_range_(SDL_Range{0, 30})
	, msgstr_notempty_and_utf8str_(_("Value must not be empty, and utf-8 format string"))
	, msgstr_empty_or_utf8str_(_("Value is empty, or utf-8 format string"))
	, disable_new_aplt_lock_(aplt::tdisable_new_klink_task_lock::reason_trigger)
	, save_widget_(nullptr)
	, nick_widget_(nullptr)
	, insert_task_widget_(nullptr)
	, erase_task_widget_(nullptr)
	, insert_var_widget_(nullptr)
	, erase_widget_(nullptr)
	, task_widget_(nullptr)
	, curr_tmp_sensor_(nullptr)
	, show_py_key_(false)
	, var_val_stack_(nullptr)
	, var_name_widget_(nullptr)
	, var_val_label_(nullptr)
	, var_val_editbox_(nullptr)
	, var_val_dropdown_(nullptr)
	, status_widget_(nullptr)
	, l_tree_(nullptr)
	, r_tree_(nullptr)
	, ignore_var_val_text_changed_(false)
{
	set_timer_interval(1000);

	aplt::init_match_methods();
}

tspeech2::~tspeech2()
{
}

static std::string unique_sensor_id(const std::map<std::string, aplt::tspeech_sensor>& speech_sensors)
{
	const std::string uuid = utils::create_uuid(false);
	const std::string prefix = uuid.substr(0, 7);
	int number = 1;
	std::stringstream ss;

	while (true) {
		ss.str("");
		ss << prefix << number;

		if (speech_sensors.count(ss.str()) == 0) {
			break;
		}
		number ++;
	}
	return ss.str();
}

static std::string unique_sensor_name(const std::map<std::string, aplt::tspeech_sensor>& speech_sensors)
{
	const std::string prefix = _("Untitle");
	int number = 1;
	std::stringstream ss;

	while (true) {
		ss.str("");
		ss << prefix << number;

		bool found = false;
		for (std::map<std::string, aplt::tspeech_sensor>::const_iterator it = speech_sensors.begin(); it != speech_sensors.end(); ++ it) {
			const aplt::tspeech_sensor& sensor = it->second;
			if (sensor.name == ss.str()) {
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

static std::string unique_var_name(const aplt::tspeech_sensor& sensor)
{
	const std::string prefix = "var";
	int number = 1;
	std::stringstream ss;

	int valid_vars = sensor.valid_vars();
	while (true) {
		ss.str("");
		ss << prefix << number;

		bool found = false;
		for (int at = 0; at < valid_vars; ++ at) {
			const aplt::tspeech_sensor::tvar& var = sensor.var(at);
			const std::string& name = var.name;
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

void tspeech2::pre_show()
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

	tbutton* button = find_widget<tbutton>(window_, "return", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_back
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(window_, "save", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_save
			, this
			, std::ref(*button)));
	button->set_active(false);
	save_widget_ = button;

	utils::string_map symbols;

	ttext_box* text_box = find_widget<ttext_box>(window_, "nick", false, true);
	const int max_nick_chars = 5;
	text_box->set_maximum_chars(max_nick_chars);
	symbols["maxnick"] = str_cast(max_nick_chars);
	text_box->set_placeholder(vgettext2("Nick, up to $maxnick characters", symbols));
	text_box->set_label(nick_);
	// tmp_nick_ is loaded by new_speech_sensors_loaded().
	text_box->set_did_text_changed(std::bind(&tspeech2::did_nick_changed, this, _1));
	nick_widget_ = text_box;

	button = find_widget<tbutton>(window_, "insert_speech_sensor", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_insert_task
			, this
			, std::ref(*button)));
	insert_task_widget_ = button;

	button = find_widget<tbutton>(window_, "erase_speech_sensor", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_erase_task
			, this
			, std::ref(*button)));
	erase_task_widget_ = button;

	button = find_widget<tbutton>(window_, "insert_var", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_insert
			, this
			, std::ref(*button), insert_var));
	insert_var_widget_ = button;

	button = find_widget<tbutton>(window_, "erase", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_erase
			, this
			, std::ref(*button)));
	erase_widget_ = button;

	button = find_widget<tbutton>(window_, "import", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_import
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(window_, "export", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_export
			, this
			, std::ref(*button)));

	status_widget_ = find_widget<tlabel>(window_, "status", false, true);

	button = find_widget<tbutton>(window_, "speech_sensor", false, true);
	button->set_border("textbox");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_task
			, this
			, std::ref(*button)));
	task_widget_ = button;


	ttree* tree = find_widget<ttree>(window_, "left_tree", false, true);
	tree->set_did_node_changed(std::bind(&tspeech2::did_node_changed, this, _2));
	l_tree_ = tree;

	tree = find_widget<ttree>(window_, "right_tree", false, true);
	tree->set_did_node_changed(std::bind(&tspeech2::did_node_changed, this, _2));
	r_tree_ = tree;

	empty_val_stack();
	VALIDATE(tmp_nick_.empty(), null_str);
	new_speech_sensors_loaded();

	refresh_toolbar_active(nullptr);
}

void tspeech2::post_show()
{
}

bool tspeech2::speech_sensors_dirty() const
{
	if (nick_ != tmp_nick_) {
		return true;
	}

	if (speech_sensors_.size() != tmp_speech_sensors_.size()) {
		return true;
	}
	if (speech_sensors_.empty()) {
		VALIDATE(tmp_speech_sensors_.empty(), null_str);
		return false;
	}

	std::map<std::string, aplt::tspeech_sensor>::const_iterator it = speech_sensors_.begin();
	std::map<std::string, aplt::tspeech_sensor>::const_iterator it2 = tmp_speech_sensors_.begin();
	for (; it != speech_sensors_.end(); ++ it, ++ it2) {
		if (!it->second.equal(it2->second)) {
			return true;
		}
	}

	return false;
}

void tspeech2::pre_var_val_label(tgrid& grid)
{
	tlabel* label = find_widget<tlabel>(&grid, "var_val_label", false, true);
	var_val_label_ = label;
}

void tspeech2::pre_var_val_editbox(tgrid& grid)
{
	ttext_box2* box2 = new ttext_box2(*window_, *find_widget<tcontrol>(&grid, "var_val_editbox", false, true), 
		"textbox", null_str);
	box2->text_box()->set_maximum_chars(64);
	// box2->text_box()->set_placeholder(_("Operator username"));
	box2->set_did_text_changed(std::bind(&tspeech2::did_var_val_text_changed, this, _1));
	var_val_editbox_ = box2;
}

void tspeech2::set_status_label(const std::string& label, bool add_t) const
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

void tspeech2::pre_var_val_dropdown(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "var_val_dropdown", false, true);
	button->set_border("textbox");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeech2::click_var_val_dropdown
			, this
			, std::ref(*button)));
	var_val_dropdown_ = button;
}

static const aplt::ttask_cpp_pair* task_pair_by_name(const std::map<std::string, aplt::ttask_cpp_pair>& task_pairs, const std::string& name, const aplt::ttask_cpp_pair* exclude_pair)
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

static const aplt::tspeech_sensor* speech_sensor_by_name(const std::map<std::string, aplt::tspeech_sensor>& speech_sensors, const std::string& name, const aplt::tspeech_sensor* exclude_sensor)
{
	for (std::map<std::string, aplt::tspeech_sensor>::const_iterator it = speech_sensors.begin(); it != speech_sensors.end(); ++ it) {
		const aplt::tspeech_sensor& sensor = it->second;
		if (&sensor == exclude_sensor) {
			continue;
		}
		if (sensor.name == name) {
			return &sensor;
		}
	}
	return nullptr;
}

static bool is_state_name_existed(const std::vector<std::string>& names, const std::string& desire_name, const int exclude_at)
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

void tspeech2::new_speech_sensors_loaded()
{
	// tmp_speech_sensors_ = speech_sensors_;
	aplt::evaluate_speech_sensors(nick_, speech_sensors_, tmp_nick_, tmp_speech_sensors_);

	aplt::tspeech_sensor* to_sensor = nullptr;
	if (!tmp_speech_sensors_.empty()) {
		std::map<std::string, aplt::tspeech_sensor>::iterator it = tmp_speech_sensors_.begin();
		// std::advance(it, widget.at());

		to_sensor = &it->second;
	}
	curr_tmp_sensor_change_to(to_sensor);
}

extern std::string truncate_for_task_id_label(const std::string& id, bool task_cpp);
extern std::string truncate_for_task_name_label(const std::string& id, bool task_cpp);

static std::string truncate_for_speech_name2_label(const aplt::tspeech_sensor& speech) 
{
	std::stringstream ss;
	ss << truncate_for_task_name_label(speech.name, false) << "(" << truncate_for_task_id_label(speech.id, false) << ")";

	return ss.str();
}

void tspeech2::curr_tmp_sensor_change_to(aplt::tspeech_sensor* sensor_ptr)
{
	curr_tmp_sensor_ = sensor_ptr;
	if (sensor_ptr != nullptr) {
		// ttask_cpp_pair_assign(tmp_pair_, *curr_pair_);
		sensor_update_to_tree(*curr_tmp_sensor_);

		task_widget_->set_label(truncate_for_speech_name2_label(*curr_tmp_sensor_));

	} else {
		task_widget_->set_label(null_str);

		l_tree_->clear();
		r_tree_->clear();
	}
}

void tspeech2::click_back(tbutton& widget)
{
	if (speech_sensors_dirty()) {
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

	for (std::map<std::string, aplt::tspeech_sensor>::iterator it = speech_sensors_.begin(); it != speech_sensors_.end(); ++ it) {
		aplt::tspeech_sensor& sensor = it->second;
		sensor.green();
	}

	// cfg_cpp_api_.sync_fake_aplt_tasks(task_pairs_);

	window_->set_retval(twindow::CANCEL);
}

bool tspeech2::do_save(tbutton& widget, bool is_back)
{
	VALIDATE(speech_sensors_dirty(), null_str);
	if (!tmp_speech_sensors_.empty()) {
		VALIDATE(curr_tmp_sensor_ != nullptr, null_str);
	} else {
		VALIDATE(curr_tmp_sensor_ == nullptr, null_str);
	}

	std::string err_msg;
	std::set<std::string> existed_names;
	for (std::map<std::string, aplt::tspeech_sensor>::const_iterator it = tmp_speech_sensors_.begin(); it != tmp_speech_sensors_.end(); ++ it) {
		const aplt::tspeech_sensor& sensor = it->second;
		uint64_t res = speech_sensor_is_valid(sensor, err_msg);
		if (res == TCOOKIE3F_CHECK_OK) {
			VALIDATE(err_msg.empty(), null_str);
			VALIDATE(existed_names.count(sensor.name) == 0, null_str);
		}
		existed_names.insert(sensor.name);

		if (res != TCOOKIE3F_CHECK_OK) {
			tcookie3f cookie3f(res);
			if (curr_tmp_sensor_ == &sensor) {
				ttree& tree = (cookie3f.type == typefield::type_global)? *l_tree_: *r_tree_;
				ttree_node* node = tree.get_root_node().find_node_from_cookie(res);
				VALIDATE(node != nullptr, null_str);
				tree.select_node(node);
				tree.scroll_to_node(*node);
			}
			std::stringstream err;
			if (curr_tmp_sensor_ != &sensor) {
				err << "[" << ht::generate_format(sensor.name2(), 0xffff0000) << "]";
			}
			if (err_msg.empty()) {
				err << get_error_msg(sensor, cookie3f.type, cookie3f.field);
			} else {
				err << err_msg;
			}
			set_status_label(err.str(), true);
			return false;
		}
	}
/*
	// speech_sensors_ = tmp_speech_sensors_;
	aplt::evaluate_speech_sensors(tmp_nick_, tmp_speech_sensors_, nick_, speech_sensors_);
	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_speech_sensor);
*/
	cfg_cpp_api_.save_speech_sensors(tmp_nick_, tmp_speech_sensors_);
	VALIDATE(!speech_sensors_dirty(), null_str);

	widget.set_active(false);
	return true;
}

void tspeech2::click_save(tbutton& widget)
{
	do_save(widget, false);
}

void tspeech2::click_import(tbutton& widget)
{
	const std::string filename = get_klink_cfg_dir(cfgtype_speech, true) + "/speech_import.cfg";

	const tcfg_4field& cfg_4field = klink_cfgs.find(cfgtype_speech)->second;
	utils::string_map symbols;
	symbols["type_cfg"] = cfg_4field.name;
	symbols["file"] = filename;

	std::string err_msg;
	tauto_destruct_executor destruct_executor(std::bind(&did_post_show_err_message, std::ref(err_msg)));

	const std::string msg = vgettext2("Do you want to import tasks from file($file)", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	std::string nick;
	std::map<std::string, aplt::tspeech_sensor> sensors_from_cfg;
	err_msg = cfg_cpp_api_.import_speech_cfg(filename, nick, sensors_from_cfg);
	if (!err_msg.empty()) {
		return;
	}
/*
	std::string stream;
	{
		const int max_task_cpp_cfg_size = 512 * 1024; // 512K bytes
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize == 0 || fsize > max_task_cpp_cfg_size) {
			gui2::show_message(null_str, vgettext2("Cann't find $file, or size must be <= 512K bytes", symbols));
			return;
		}

		bool all_is_utf8 = utils::is_utf8str(file.data, fsize);
		if (!all_is_utf8) {
			gui2::show_message(null_str, vgettext2("$file isn't utf-8 format", symbols));
			return;
		}
		stream.assign(file.data, fsize);
	}

	config top_cfg;
	if (!aplt::read_config_ex(stream, true, top_cfg)) {
		err_msg = _("Import failed. Not a valid WML format file");
		return;
	}

	const std::string cfg_type = top_cfg["type"].str();
	if (cfg_type != cfg_4field.id) {
		symbols["type"] = cfg_4field.id;
		err_msg = vgettext2("Import failed. for $type_cfg, 'type' value must be $type", symbols);
		return;
	}

	std::string nick;
	std::map<std::string, aplt::tspeech_sensor> sensors_from_cfg;
	bool retval = cfg_cpp_api_.cfg_to_speech_sensors(top_cfg, nick, sensors_from_cfg);
	if (!retval) {
		// parse config fail.
		err_msg = vgettext2("Parse $file error", symbols);
		return;
	}

	if (sensors_from_cfg.empty()) {
		err_msg = vgettext2("There is no task in $file.", symbols);
		return;
	}
*/
	// import task_pairs maybe equal task_pairs_.
	// tmp_speech_sensors_ = sensors_from_cfg;
	aplt::evaluate_speech_sensors(nick, sensors_from_cfg, tmp_nick_, tmp_speech_sensors_);

	nick_widget_->set_label(nick);
	curr_tmp_sensor_change_to(&tmp_speech_sensors_.begin()->second);

	empty_val_stack();
	refresh_toolbar_active(nullptr);

	bool dirty = speech_sensors_dirty();
	// if state2 has req_task, and req_task.id is different, @dirty will false always.
	save_widget_->set_active(dirty);
}

void tspeech2::click_export(tbutton& widget)
{
	std::stringstream out;
	cfg_cpp_api_.speech_sensors_to_stringstream(tmp_nick_, tmp_speech_sensors_, out);

	if (!out.str().empty()) {
		const std::string filename = get_klink_cfg_dir(cfgtype_speech, true) + "/speech_export.cfg";
		write_file(filename, out.str().c_str(), out.str().size());

		utils::string_map symbols;
		symbols["file"] = filename;
		gui2::show_message(null_str, vgettext2("Export finished. file: $file", symbols));
	}
}

void tspeech2::click_task(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;
    
	std::vector<aplt::tspeech_sensor*> task_pairs_v;

	std::map<std::string, aplt::tspeech_sensor>& tmp_speech_sensors = tmp_speech_sensors_;

	std::map<std::string, std::string> data;
	int at = 0;
	for (std::map<std::string, aplt::tspeech_sensor>::iterator it = tmp_speech_sensors.begin(); it != tmp_speech_sensors.end(); ++ it, at ++) {
		aplt::tspeech_sensor& sensor = it->second;
		items.push_back(gui2::tmenu::titem(sensor.name2(), items.size()));
		task_pairs_v.push_back(&sensor);
		if (curr_tmp_sensor_->id == sensor.id) {
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
	aplt::tspeech_sensor* curr_sensor = task_pairs_v[cursel];

	curr_tmp_sensor_change_to(curr_sensor);

	empty_val_stack();
	refresh_toolbar_active(nullptr);
}

void tspeech2::varabile_update_to_tree(ttree_node& branch, int index, const aplt::tspeech_sensor::tvar& var)
{
	std::map<std::string, std::string> data;

	const std::string label = get_node_label_var(var, typefield::field_typeself);
	data["label"] = label + str_cast(index + 1);
	ttree_node& htvi_particular = branch.insert_node("default", data);
	htvi_particular.set_child_icon("label", "misc/variable.png");
	htvi_particular.set_cookie(tcookie3f(index, typefield::type_var, typefield::field_typeself).u64);

	data["label"] = get_node_label_var(var, typefield::field_aplt_id);
	ttree_node* htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_var, typefield::field_aplt_id).u64);

	data["label"] = get_node_label_var(var, typefield::field_name);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_var, typefield::field_name).u64);

	data["label"] = get_node_label_var(var, typefield::field_optional);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_var, typefield::field_optional).u64);

	data["label"] = get_node_label_var(var, typefield::field_first_words_is_prefix);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_var, typefield::field_first_words_is_prefix).u64);

	data["label"] = get_node_label_var(var, typefield::field_prefix_words);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_var, typefield::field_prefix_words).u64);

	data["label"] = get_node_label_var(var, typefield::field_postfix_words);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_var, typefield::field_postfix_words).u64);

	data["label"] = get_node_label_var(var, typefield::field_major_word);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_var, typefield::field_major_word).u64);

	data["label"] = get_node_label_var(var, typefield::field_minor_words);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_var, typefield::field_minor_words).u64);

	data["label"] = get_node_label_var(var, typefield::field_strategy);
	htvi = &htvi_particular.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(index, typefield::type_var, typefield::field_strategy).u64);

	htvi_particular.unfold();
}

std::string tspeech2::get_node_label_global(const aplt::tspeech_sensor& sensor, int field) const
{
	std::stringstream ss;
	const std::string field_str = aplt::get_field_str(typefield::type_global, field);
	if (field == typefield::field_id) {
		ss << field_str << ": " << sensor.id;

	} else if (field == typefield::field_name) {
		ss << field_str << ": " << sensor.name;

	} else if (field == typefield::field_countdown_s) {
		ss << field_str << ": " << sensor.countdown_s;

	} else if (field == typefield::field_first_words) {
		if (show_py_key_) {
			ss << field_str << ":" << utils::join(sensor.py_first_words);
		} else {
			ss << field_str << ":" << utils::join(sensor.first_words);
		}

	} else if (field == typefield::field_major_word) {
		ss << field_str << ": ";
		if (show_py_key_) {
			ss << sensor.py_major_word;
		} else {
			ss << sensor.major_word;
		}

	} else if (field == typefield::field_minor_words) {
		ss << field_str << ": ";
		if (show_py_key_) {
			ss << utils::join(sensor.py_minor_words);
		} else {
			ss << utils::join(sensor.minor_words);
		}

	} else if (field == typefield::field_strategy) {
		VALIDATE(aplt::minor_key_strategies.count(sensor.strategy) != 0, null_str);
		ss << field_str << ": " << aplt::minor_key_strategies.find(sensor.strategy)->second;

	} else {
		VALIDATE(false, null_str);
	}

	return ss.str();
}

std::string tspeech2::get_node_label_var(const aplt::tspeech_sensor::tvar& var, int field) const
{
	std::stringstream ss;

	const std::string field_str = aplt::get_field_str(typefield::type_var, field);
	if (field == typefield::field_typeself) {
		ss << field_str;

	} else if (field == typefield::field_aplt_id) {
		ss << field_str << ": ";
		const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets_, var.aplt_id);
		if (aplt != nullptr) {
			ss << aplt->name2();

		} else {
			ss << var.aplt_id;
		}

	} else if (field == typefield::field_name) {
		ss << field_str << ": " << var.name;

	} else if (field == typefield::field_optional) {
		ss << field_str << ": " << (var.optional? _("Yes"): _("No"));

	} else if (field == typefield::field_first_words_is_prefix) {
		ss << field_str << ": " << (var.first_words_is_prefix? _("Yes"): _("No"));

	} else if (field == typefield::field_prefix_words) {
		ss << field_str << ": ";
		if (show_py_key_) {
			ss << utils::join(var.py_prefix_words);
		} else {
			ss << utils::join(var.prefix_words);
		}

	} else if (field == typefield::field_postfix_words) {
		ss << field_str << ": ";
		if (show_py_key_) {
			ss << utils::join(var.py_postfix_words);
		} else {
			ss << utils::join(var.postfix_words);
		}

	} else if (field == typefield::field_major_word) {
		ss << field_str << ": ";
		if (show_py_key_) {
			ss << var.py_major_word;
		} else {
			ss << var.major_word;
		}

	} else if (field == typefield::field_minor_words) {
		ss << field_str << ": ";
		if (show_py_key_) {
			ss << utils::join(var.py_minor_words);
		} else {
			ss << utils::join(var.minor_words);
		}

	} else if (field == typefield::field_strategy) {
		VALIDATE(aplt::minor_key_strategies.count(var.strategy) != 0, null_str);
		ss << field_str << ": " << aplt::minor_key_strategies.find(var.strategy)->second;

	} else {
		VALIDATE(false, null_str);
	}

	return ss.str();
}

void tspeech2::sensor_update_to_l_tree(const aplt::tspeech_sensor& sensor)
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

	data["label"] = get_node_label_global(sensor, typefield::field_id);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_id).u64);

	data["label"] = get_node_label_global(sensor, typefield::field_name);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_name).u64);

	data["label"] = get_node_label_global(sensor, typefield::field_countdown_s);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_countdown_s).u64);

	data["label"] = get_node_label_global(sensor, typefield::field_first_words);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_first_words).u64);

	data["label"] = get_node_label_global(sensor, typefield::field_major_word);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_major_word).u64);

	data["label"] = get_node_label_global(sensor, typefield::field_minor_words);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_minor_words).u64);

	data["label"] = get_node_label_global(sensor, typefield::field_strategy);
	htvi = &htvi_root.insert_node("default", data);
	htvi->set_child_icon("label", "misc/property.png");
	htvi->set_cookie(tcookie3f(0, typefield::type_global, typefield::field_strategy).u64);

	// htvi_root.unfold();

	validate_l_tree_cookie();
}

void tspeech2::sensor_update_to_r_tree(const aplt::tspeech_sensor& sensor)
{
	ttree& r_tree = *r_tree_;
	r_tree.clear();

	ttree_node& r_htvi_root = r_tree.get_root_node();

	const int valid_vars = sensor.valid_vars();
	for (int at = 0; at < valid_vars; at ++) {
		const aplt::tspeech_sensor::tvar& var = sensor.var(at);
		varabile_update_to_tree(r_htvi_root, at, var);
	}

	validate_r_tree_cookie();
}

void tspeech2::sensor_update_to_tree(const aplt::tspeech_sensor& sensor)
{
	sensor_update_to_l_tree(sensor);
	sensor_update_to_r_tree(sensor);
}

void tspeech2::validate_l_tree_cookie() const
{
	VALIDATE(curr_tmp_sensor_ != nullptr, null_str);
	const aplt::tspeech_sensor& sensor = *curr_tmp_sensor_;
	ttree& l_tree = *l_tree_;
	ttree_node& l_htvi_root = l_tree.get_root_node();

	const std::vector<ttree_node*>& children = l_htvi_root.children();
	const int global_atts = 7;
	VALIDATE(children.size() == global_atts, null_str);
	int child_at = global_atts;

/*
	VALIDATE(curr_tmp_pair_ != nullptr, null_str);
	const aplt::ttask_cpp_pair& pair = *curr_tmp_pair_;
	ttree& l_tree = *l_tree_;
	ttree_node& l_htvi_root = l_tree.get_root_node();

	const std::vector<ttree_node*>& children = l_htvi_root.children();
	const int global_atts = 4;
	VALIDATE(children.size() == global_atts + pair.state_names.size(), null_str);
	int child_at = global_atts;

	// startup next states
	int index = 0;
	for (std::map<int, aplt::tcpp_api::tstate2>::const_iterator it = pair.states.begin(); it != pair.states.end(); ++ it, index ++) {
		const aplt::tcpp_api::tstate2& state2 = it->second;
		VALIDATE(state2.state >= 0 && state2.state < (int)pair.state_names.size(), null_str);
		VALIDATE(state2.state == index, null_str);

		tcookie3f cookie3f(state2.state, type_state2, field_typeself);
		VALIDATE(children[child_at]->cookie() == cookie3f.u64, null_str);
		child_at ++;
	}
	VALIDATE(child_at == children.size(), null_str);
*/
}

void tspeech2::validate_r_tree_cookie() const
{
	VALIDATE(curr_tmp_sensor_ != nullptr, null_str);
	const aplt::tspeech_sensor& sensor = *curr_tmp_sensor_;
	ttree& r_tree = *r_tree_;
	ttree_node& r_htvi_root = r_tree.get_root_node();

	const std::vector<ttree_node*>& children = r_htvi_root.children();

	const int valid_vars = sensor.valid_vars();
	VALIDATE(children.size() == valid_vars, null_str);
	int child_at = 0;

	// startup next states
	for (int at = 0; at < valid_vars; at ++) {
		const aplt::tspeech_sensor::tvar& var = sensor.var(at);
		if (var.first_words_is_prefix) {
			// here allow. speech_sensor_is_valid() will check it.
			// VALIDATE(sensor.first_words == var.prefix_words && sensor.py_first_words == var.py_prefix_words, null_str);
		}

		tcookie3f cookie3f(at, typefield::type_var, typefield::field_typeself);
		VALIDATE(children[child_at]->cookie() == cookie3f.u64, null_str);
		child_at ++;
	}

	VALIDATE(child_at == children.size(), null_str);
}

void tspeech2::empty_val_stack()
{
	var_val_stack_->set_radio_layer(VAR_VAL_LABEL_LAYER);
	var_name_widget_->set_label(null_str);

	utils::string_map symbols;
	symbols["method_var"] = aplt::match_methods_.find(aplt::match_method_var)->second;
	symbols["method_simpleword"] = aplt::match_methods_.find(aplt::match_method_simple_word)->second;
	symbols["major_word"] = aplt::get_field_str(typefield::type_global, typefield::field_major_word);
	symbols["minor_words"] = aplt::get_field_str(typefield::type_global, typefield::field_minor_words);
	const std::string label_msg = vgettext2("trigger2_help_label $method_var, $method_simpleword, $major_word, $minor_words", symbols);
	var_val_label_->set_label(label_msg);

	const std::string status_msg = _("trigger2_help_status");
	set_status_label(status_msg);
}

void tspeech2::select_r_tree_child_by_at(const aplt::tspeech_sensor& sensor, int child_at, tcookie3f& result_cookie3f)
{
	ttree& tree = *r_tree_;
	int child_size = sensor.valid_vars();

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

std::string tspeech2::get_placeholder_msg(int type, int field) const
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

		} else if (field == typefield::field_countdown_s) {
			SDL_snprintf(buf, sizeof(buf), "[%i, %i]", countdown_s_range_.min, countdown_s_range_.max);
			symbols["range"] = buf;
			placeholder = vgettext2("Value must be in range $range", symbols);

		} else if (field == typefield::field_first_words) {
			placeholder = _("Empty, or multiple UTF-8 strings. Multiple times used ',' to separate");

		} else if (field == typefield::field_major_word) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == typefield::field_minor_words) {
			placeholder = _("Empty, or multiple UTF-8 strings. Multiple times used ',' to separate");
		}

	} else if (type == typefield::type_var) {
		if (field == typefield::field_typeself) {
			placeholder = msgstr_notempty_and_utf8str_;

		} else if (field == typefield::field_prefix_words) {
			placeholder = _("Empty, or multiple UTF-8 strings. Multiple times used ',' to separate");

		} else if (field == typefield::field_postfix_words) {
			placeholder = _("Empty, or multiple UTF-8 strings. Multiple times used ',' to separate");

		} else if (field == typefield::field_major_word) {
			placeholder = msgstr_empty_or_utf8str_;

		} else if (field == typefield::field_minor_words) {
			placeholder = _("Empty, or multiple UTF-8 strings. Multiple times used ',' to separate");
		}

	}
	
	return placeholder;
}

std::string tspeech2::get_remark_msg(int type, int field) const
{
	utils::string_map symbols;

	if (type == typefield::type_global) {
		if (field == typefield::field_id) {
			return _("global^speech_sensor id remark");
		} else if (field == typefield::field_name) {
			return _("global^speech_sensor name remark");
		} else if (field == typefield::field_countdown_s) {
			return _("global^countdown remark");
		} else if (field == typefield::field_first_words) {
			return _("global^first_words remark");
		} else if (field == typefield::field_major_word) {
			return _("key_2_state^major_word remark");
		} else if (field == typefield::field_minor_words) {
			return _("key_2_state^minor_words remark");
		} else if (field == typefield::field_strategy) {
			symbols["any_one"] = aplt::minor_key_strategies.find(aplt::mkeys_any_one)->second;
			symbols["all_match_and_order"] = aplt::minor_key_strategies.find(aplt::mkeys_all_match_and_order)->second;
			symbols["all_match_no_order"] = aplt::minor_key_strategies.find(aplt::mkeys_all_match_no_order)->second;
			return vgettext2("key_2_state^strategy remark $any_one, $all_match_and_order, $all_match_no_order", symbols);
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typefield::type_var) {
		if (field == typefield::field_typeself) {
			return _("var^self remark");
		} else if (field == typefield::field_aplt_id) {
			return _("var^aplt_id remark");
		} else if (field == typefield::field_name) {
			return _("var^name remark");
		} else if (field == typefield::field_optional) {
			return _("var^optional remark");
		} else if (field == typefield::field_first_words_is_prefix) {
			symbols["first_words"] = aplt::get_field_str(typefield::type_global, typefield::field_first_words);
			symbols["prefix_words"] = aplt::get_field_str(typefield::type_var, typefield::field_prefix_words);
			return vgettext2("var^first_words_is_prefix remark, $first_words, $prefix_words", symbols);

		} else if (field == typefield::field_prefix_words) {
			return _("var^prefix_words remark");
		} else if (field == typefield::field_postfix_words) {
			return _("var^postfix_words remark");
		} else if (field == typefield::field_major_word) {
			return _("var^major_word remark");
		} else if (field == typefield::field_minor_words) {
			return _("var^minor_words remark");
		} else if (field == typefield::field_strategy) {
			symbols["any_one"] = aplt::minor_key_strategies.find(aplt::mkeys_any_one)->second;
			symbols["all_match_and_order"] = aplt::minor_key_strategies.find(aplt::mkeys_all_match_and_order)->second;
			symbols["all_match_no_order"] = aplt::minor_key_strategies.find(aplt::mkeys_all_match_no_order)->second;
			return vgettext2("var^strategy remark $any_one, $all_match_and_order, $all_match_no_order", symbols);
		} else {
			VALIDATE(false, null_str);
		}

	} else {
		VALIDATE(false, null_str);
	}

	return null_str;
}

std::string tspeech2::get_error_msg(const aplt::tspeech_sensor& sensor, int type, int field) const
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

void tspeech2::click_insert_task(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::vector<const aplt::tspeech_sensor*> sensor_vec;

	aplt::tspeech_sensor new_sensor;
	new_sensor.id = unique_sensor_id(tmp_speech_sensors_);
	new_sensor.name = unique_sensor_name(tmp_speech_sensors_);

	const std::map<std::string, aplt::tspeech_sensor>& buildin_sensors = cfg_cpp_api_.buildin_speech_sensors();

	sensor_vec.push_back(&new_sensor);
	items.push_back(gui2::tmenu::titem(_("New speech sensor"), items.size()));
	if (!buildin_sensors.empty()) {
		for (std::map<std::string, aplt::tspeech_sensor>::const_iterator it = buildin_sensors.begin(); it != buildin_sensors.end(); ++ it) {
			const aplt::tspeech_sensor& sensor = it->second;
			sensor_vec.push_back(&sensor);
			items.push_back(gui2::tmenu::titem(sensor.name2(), items.size()));
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

	const aplt::tspeech_sensor& old_sensor = *sensor_vec[cursel];

	std::string dup_field;
	if (tmp_speech_sensors_.count(old_sensor.id) != 0) {
		dup_field = aplt::get_field_str(typefield::type_global, typefield::field_id);
	}
	if (dup_field.empty() && speech_sensor_by_name(tmp_speech_sensors_, old_sensor.name, nullptr) != nullptr) {
		dup_field = aplt::get_field_str(typefield::type_global, typefield::field_name);
	}
	if (!dup_field.empty()) {
		utils::string_map symbols;
		symbols["field"] = dup_field;
		std::string err_msg = vgettext2("There is a '$field' with the same name. Please delete or modify the item before inserting it.", symbols);
		gui2::show_message(null_str, err_msg);
		return;
	}



	std::pair<std::map<std::string, aplt::tspeech_sensor>::iterator, bool> ins = 
		tmp_speech_sensors_.insert(std::make_pair(old_sensor.id, old_sensor));
	VALIDATE(ins.second, null_str);
	aplt::tspeech_sensor& sensor = ins.first->second;

	task_widget_->set_label(truncate_for_speech_name2_label(sensor));

	curr_tmp_sensor_ = &sensor;
	// ttask_cpp_pair_assign(tmp_pair_, *curr_pair);
	sensor_update_to_tree(*curr_tmp_sensor_);

	empty_val_stack();
	refresh_toolbar_active(nullptr);

	bool dirty = speech_sensors_dirty();
	save_widget_->set_active(dirty);
}

void tspeech2::click_erase_task(tbutton& widget)
{
	VALIDATE(!tmp_speech_sensors_.empty(), null_str);
	VALIDATE(curr_tmp_sensor_ != nullptr, null_str);
	aplt::tspeech_sensor& pair = *curr_tmp_sensor_;

	utils::string_map symbols;
	symbols["sensor"] = pair.name2();
	const std::string msg = vgettext2("Do you want erase speech sensor($sensor)?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	VALIDATE(tmp_speech_sensors_.count(pair.id) != 0, null_str);
	std::map<std::string, aplt::tspeech_sensor>::iterator erase_it = tmp_speech_sensors_.find(pair.id);

	aplt::tspeech_sensor* next_pair = nullptr;
	if (tmp_speech_sensors_.size() > 1) {
		std::map<std::string, aplt::tspeech_sensor>::iterator next_id = erase_it;
		next_id ++;
		if (next_id == tmp_speech_sensors_.end()) {
			next_id = tmp_speech_sensors_.begin();
		}
		next_pair = &next_id->second;
	}
	tmp_speech_sensors_.erase(tmp_speech_sensors_.find(pair.id));

	// set task's label
	curr_tmp_sensor_ = next_pair;
	if (next_pair != nullptr) {
		task_widget_->set_label(truncate_for_speech_name2_label(*next_pair));
		sensor_update_to_tree(*next_pair);

	} else {
		task_widget_->set_label(null_str);
		l_tree_->clear();
		r_tree_->clear();
	}

	empty_val_stack();
	refresh_toolbar_active(nullptr);

	bool dirty = speech_sensors_dirty();
	save_widget_->set_active(dirty);

}

void tspeech2::click_insert(tbutton& widget, int type)
{
	VALIDATE(curr_tmp_sensor_ != nullptr, null_str);
	aplt::tspeech_sensor& sensor = *curr_tmp_sensor_;

	ttree_node* selected_node = nullptr;
	bool l_tree = true;
	if (type == insert_var) {
		l_tree = false;
		// if (pair.state_names.empty()) {
		//	gui2::show_message(null_str, _("It cannot be inserted. At least one state needs to exist."));
		//	return;
		// }
		const int valid_vars = sensor.valid_vars();
		VALIDATE(valid_vars < MAX_SPEECH_SENSOR_VARS, null_str);

		const std::string initial_aplt_id = aplt::fake_aplt.bundleid;
		aplt::tspeech_sensor::tvar& new_var = sensor.mutable_var(valid_vars);
		VALIDATE(new_var.is_nposm(), null_str);
		new_var.aplt_id = initial_aplt_id;
		new_var.name = unique_var_name(sensor);

		ttree_node& htvi_root = r_tree_->get_root_node();
		sensor_update_to_r_tree(sensor);

		tcookie3f new_cookie3f(sensor.valid_vars() - 1, typefield::type_var, typefield::field_typeself);
		ttree_node* node = htvi_root.find_node_from_cookie(new_cookie3f.u64);
		VALIDATE(node != nullptr, null_str);
		r_tree_->select_node(node);
		selected_node = node;

	} else {
		VALIDATE(false, null_str);
	}

	if (l_tree) {
		validate_l_tree_cookie();
	} else {
		validate_r_tree_cookie();
	}

	deselect_another_tree(l_tree);
	tcookie3f tmp_cookie3f(selected_node->cookie());
	refresh_toolbar_active(&tmp_cookie3f);

	bool dirty = speech_sensors_dirty();
	save_widget_->set_active(dirty);
}

void tspeech2::click_erase(tbutton& widget)
{
	VALIDATE(curr_tmp_sensor_ != nullptr, null_str);
	aplt::tspeech_sensor& sensor = *curr_tmp_sensor_;

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	VALIDATE(cookie3f.field == typefield::field_typeself, null_str);

	tcookie3f result_cookie3f = cookie3f_nposm_;
	int select_child_at = nposm;

	bool l_tree = true;
	utils::string_map symbols;
	std::stringstream ss;
	if (cookie3f.type == typefield::type_var) {
		l_tree = false;

		const int valid_vars = sensor.valid_vars();
		VALIDATE(cookie3f.index >= 0 && cookie3f.index < valid_vars, null_str);

		const aplt::tspeech_sensor::tvar& desire_var = sensor.var(cookie3f.index);

		ss.str("");
		symbols["var"] = utils::join_app_prefix_id(desire_var.aplt_id, desire_var.name);
		const std::string msg = vgettext2("Do you want erase $var?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}

		select_child_at = cookie3f.index;
		// 'erase' desire_var
		for (int at = cookie3f.index; at < valid_vars - 1; at ++) {
			aplt::tspeech_sensor::tvar& dest = sensor.mutable_var(at);
			aplt::tspeech_sensor::tvar& src = sensor.mutable_var(at + 1);
			dest = src;
		}
		if (valid_vars > 0) {
			sensor.mutable_var(valid_vars - 1).clear();
		}

		// now gui2::ttree doesn't not support erase single node. regenerate tree.
		sensor_update_to_r_tree(sensor);

	} else {
		VALIDATE(false, null_str);
	}

	if (l_tree) {
		validate_l_tree_cookie();

	} else {
		validate_r_tree_cookie();
		select_r_tree_child_by_at(sensor, select_child_at, result_cookie3f);
	}

	deselect_another_tree(l_tree);
	refresh_toolbar_active(result_cookie3f != cookie3f_nposm_? &result_cookie3f: nullptr);
	if (result_cookie3f == cookie3f_nposm_) {
		empty_val_stack();
	}

	bool dirty = speech_sensors_dirty();
	save_widget_->set_active(dirty);
}

void tspeech2::refresh_toolbar_active(const tcookie3f* cookie3f)
{
	if (tmp_speech_sensors_.empty()) {
		insert_var_widget_->set_active(false);
		erase_widget_->set_active(false);

		erase_task_widget_->set_active(false);
		return;
	}

	VALIDATE(curr_tmp_sensor_ != nullptr, null_str);
	const aplt::tspeech_sensor& sensor = *curr_tmp_sensor_;

	insert_var_widget_->set_active(sensor.valid_vars() < MAX_SPEECH_SENSOR_VARS);

	bool active_erase = cookie3f != nullptr && cookie3f->field == typefield::field_typeself;
	if (active_erase) {
		erase_widget_->set_cookie(cookie3f->u64);
	}
	erase_widget_->set_active(active_erase);

	// insert/erase task
	erase_task_widget_->set_active(true);
}

void tspeech2::deselect_another_tree(bool l_tree)
{
	ttree& another_tree = l_tree? *r_tree_: *l_tree_;
	another_tree.select_node(nullptr);
}

void tspeech2::did_node_changed(ttree_node& node)
{
	VALIDATE(curr_tmp_sensor_ != nullptr, null_str);
	const aplt::tspeech_sensor& sensor = *curr_tmp_sensor_;

	utils::string_map symbols;
	uint64_t cookie = node.cookie();
	tcookie3f cookie3f(cookie);

	bool l_tree = true;
	const std::string field_str = aplt::get_field_str(cookie3f.type, cookie3f.field);
	const std::string status_msg = get_remark_msg(cookie3f.type, cookie3f.field);
	std::string var_name;
	std::string var_val_textbox;
	std::string var_val_dropdown;
	std::string var_val_label_msg;

	int layer = VAR_VAL_EDITBOX_LAYER;
	if (cookie3f.type == typefield::type_global) {
		if (cookie3f.field == typefield::field_id) {
			var_name = field_str;
			var_val_textbox = sensor.id;

		} else if (cookie3f.field == typefield::field_name) {
			var_name = field_str;
			var_val_textbox = sensor.name;

		} else if (cookie3f.field == typefield::field_countdown_s) {
			var_name = field_str;
			var_val_textbox = str_cast(sensor.countdown_s);

		} else if (cookie3f.field == typefield::field_first_words) {
			var_name = field_str;
			var_val_textbox = utils::join(sensor.first_words);

		} else if (cookie3f.field == typefield::field_major_word) {
			var_name = field_str;
			var_val_textbox = sensor.major_word;

		} else if (cookie3f.field == typefield::field_minor_words) {
			var_name = field_str;
			var_val_textbox = utils::join(sensor.minor_words);

		} else if (cookie3f.field == typefield::field_strategy) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			var_val_dropdown = aplt::minor_key_strategies.find(sensor.strategy)->second;
		} else {
			VALIDATE(false, null_str);
		}

	} else if (cookie3f.type == typefield::type_var) {
		l_tree = false;
		const aplt::tspeech_sensor::tvar& var = sensor.var(cookie3f.index);

		if (cookie3f.field == typefield::field_typeself) {
			layer = VAR_VAL_LABEL_LAYER;
			var_name = field_str;

			// var_name = field_str;
			// var_val_textbox = curr_tmp_pair_->state_names[state2.state];

		} else if (cookie3f.field == typefield::field_aplt_id) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			const aplt::tapplet* aplt = aplt::aplt_from_bundleid_ex(applets_, var.aplt_id);
			if (aplt != nullptr) {
				var_val_dropdown = aplt->name2();
			} else {
				var_val_dropdown = var.aplt_id;
			}

		} else if (cookie3f.field == typefield::field_name) {
			var_name = field_str;
			var_val_textbox = var.name;
/*
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			const aplt::tapplet* aplt = aplt::aplt_from_bundleid(applets_, state2.async_task.aplt_id);
			if (aplt != nullptr && aplt->tasks.count(state2.async_task.task_id) != 0) {
				var_val_dropdown = aplt->tasks.find(state2.async_task.task_id)->second.name;
			} else {
				var_val_dropdown = state2.async_task.task_id;
			}
*/
		} else if (cookie3f.field == typefield::field_optional) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			var_val_dropdown = var.optional? _("Yes"): _("No");

		} else if (cookie3f.field == typefield::field_first_words_is_prefix) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			var_val_dropdown = var.first_words_is_prefix? _("Yes"): _("No");

		} else if (cookie3f.field == typefield::field_prefix_words) {
			if (!var.first_words_is_prefix) {
				var_name = field_str;
				var_val_textbox = utils::join(var.prefix_words);

			} else {
				layer = VAR_VAL_LABEL_LAYER;

				var_name = field_str;
				symbols["first_words_is_prefix"] = aplt::get_field_str(typefield::type_var, typefield::field_first_words_is_prefix);
				// symbols["prefix_words"] = get_field_str(type_var, field_prefix_words);
				symbols["first_words"] = aplt::get_field_str(typefield::type_global, typefield::field_first_words);
				var_val_label_msg = vgettext2("'$first_words_is_prefix' is selected, it will change to '$first_words' after save", symbols);
			}

		} else if (cookie3f.field == typefield::field_postfix_words) {
			var_name = field_str;
			var_val_textbox = utils::join(var.postfix_words);

		} else if (cookie3f.field == typefield::field_major_word) {
			var_name = field_str;
			var_val_textbox = var.major_word;

		} else if (cookie3f.field == typefield::field_minor_words) {
			var_name = field_str;
			var_val_textbox = utils::join(var.minor_words);

		} else if (cookie3f.field == typefield::field_strategy) {
			layer = VAR_VAL_DROPDOWN_LAYER;
			var_name = field_str;
			var_val_dropdown = aplt::minor_key_strategies.find(var.strategy)->second;
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

		} else {
			VALIDATE(layer == VAR_VAL_LABEL_LAYER, null_str);
			var_val_label_->set_label(var_val_label_msg);
		}
	}
	set_status_label(status_msg);

	refresh_toolbar_active(&cookie3f);
	deselect_another_tree(l_tree);
}

void tspeech2::curr_tmp_sensor_id_changed()
{
	// id in curr_tmp_pair is changed, but first of tmp_task_pairs_ isn't change.
	// change tmp_task_pairs_.first will result resort.
	std::map<std::string, aplt::tspeech_sensor>::iterator hit_it;
	for (std::map<std::string, aplt::tspeech_sensor>::iterator it = tmp_speech_sensors_.begin(); it != tmp_speech_sensors_.end(); ++ it) {
		const aplt::tspeech_sensor& sensor = it->second;
		if (&sensor == curr_tmp_sensor_) {
			hit_it = it;
			break;
		}
	}

	VALIDATE(hit_it != tmp_speech_sensors_.end(), null_str);

	std::string original_id = hit_it->first;
	VALIDATE(original_id != curr_tmp_sensor_->id, null_str);

	aplt::tspeech_sensor tmp_sensor = *curr_tmp_sensor_;

	tmp_speech_sensors_.erase(hit_it);
	std::pair<std::map<std::string, aplt::tspeech_sensor>::iterator, bool> ins = tmp_speech_sensors_.insert(std::make_pair(tmp_sensor.id, tmp_sensor));
	curr_tmp_sensor_ = &ins.first->second;
}

void tspeech2::did_nick_changed(ttext_box& widget)
{
	tmp_nick_ = widget.label();

	bool dirty = speech_sensors_dirty();
	save_widget_->set_active(dirty);
}

void tspeech2::did_var_val_text_changed(ttext_box& widget)
{
	if (ignore_var_val_text_changed_) {
		return;
	}
	VALIDATE(curr_tmp_sensor_ != nullptr, null_str);
	aplt::tspeech_sensor& sensor = *curr_tmp_sensor_;

	utils::string_map symbols;
	std::string label = widget.label();
	utils::strip(label);

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	bool id_changed = false;
	bool id_or_name_changed = false;
	bool state2_name_changed = false;
	std::string new_node_label;
	bool l_tree = true;
	std::string status_msg;
	int n32 = nposm;
	if (cookie3f.type == typefield::type_global) {
		if (cookie3f.field == typefield::field_id) {
			if (isvalid_normal_id_or_var_name224(label)) {
				if (label == sensor.id || tmp_speech_sensors_.count(label) == 0) {
					// #1(doorbell) -> #2(doorbell#)[invalid] -> #3(doorbell)
					// here, #1 is #3.
					id_changed = label != sensor.id;
					id_or_name_changed = id_changed;

					sensor.id = label;
					// new_node_label = get_node_label_global(sensor, field_id);
					
				} else {
					status_msg = _("'id' cannot be the same");
				}
			} else {
				status_msg = get_error_msg(sensor, cookie3f.type, cookie3f.field);
			}

		} else if (cookie3f.field == typefield::field_name) {
			// int name_chars = utils::utf8str_len(label);
			// if (name_chars >= task_name_chars_range_.min && name_chars <= task_name_chars_range_.max) {
			if (isvalid_normal_utf8_name224(label)) {
				if (label == sensor.name || speech_sensor_by_name(tmp_speech_sensors_, label, &sensor) == nullptr) {
					id_or_name_changed = label != sensor.name;

					sensor.name = label;
					// new_node_label = get_node_label_global(sensor, field_name);
					
				} else {
					symbols["field"] = aplt::get_field_str(cookie3f.type, cookie3f.field);
					status_msg = vgettext2("'$field' cannot be the same", symbols);
				}
			} else {
				status_msg = get_error_msg(sensor, cookie3f.type, cookie3f.field);
			}
			
		} else if (cookie3f.field == typefield::field_countdown_s) {
			if (utils::isinteger(label)) {
				n32 = utils::to_int(label);
			}
			if (n32 >= countdown_s_range_.min && n32 <= countdown_s_range_.max) {
				sensor.countdown_s = n32;
			} else {
				status_msg = get_error_msg(sensor, cookie3f.type, cookie3f.field);
			}

		} else if (cookie3f.field == typefield::field_first_words) {
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
					py_vstr.push_back(sensor.py_from_utf8str(key));
				}

				sensor.first_words = vstr;

				// key_2_state is std::vector, change item#'s std::string member, item's ptr maybe changed.
				sensor.py_first_words = py_vstr;
			}
		} else if (cookie3f.field == typefield::field_major_word) {
			if (!label.empty()) {
				if (!utils::is_utf8str(label.c_str(), label.size())) {
					status_msg = _("Require utf-8 frmat");
				}
			}
			if (status_msg.empty()) {
				sensor.major_word = label;
				// key_2_state is std::vector, change item#'s std::string member, item's ptr maybe changed.
				sensor.py_major_word = sensor.py_from_utf8str(label);
			}

		} else if (cookie3f.field == typefield::field_minor_words) {
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
					py_vstr.push_back(sensor.py_from_utf8str(key));
				}

				sensor.minor_words = vstr;

				// key_2_state is std::vector, change item#'s std::string member, item's ptr maybe changed.
				sensor.py_minor_words = py_vstr;
			}
		}

		if (status_msg.empty()) {
			new_node_label = get_node_label_global(sensor, cookie3f.field);
		}

	} else if (cookie3f.type == typefield::type_var) {
		l_tree = false;

		aplt::tspeech_sensor::tvar& var = sensor.mutable_var(cookie3f.index);

		if (cookie3f.field == typefield::field_typeself) {
/*
			if (!label.empty()) {
				if (label == pair.state_names[state2.state] || !is_state_name_existed(pair.state_names, label, state2.state)) {
					state2_name_changed = label != pair.state_names[state2.state];

					pair.state_names[state2.state] = label;
					
				} else {
					status_msg = _("'name' cannot be the same");
				}
			}
*/
		} else if (cookie3f.field == typefield::field_name) {
			if (!label.empty()) {
				if (!utils::is_utf8str(label.c_str(), label.size())) {
					status_msg = _("Require utf-8 frmat");
				}
			}
			if (status_msg.empty()) {
				var.name = label;
			}
/*
			if (!label.empty()) {
				if (label == pair.state_names[state2.state] || !is_state_name_existed(pair.state_names, label, state2.state)) {
					state2_name_changed = label != pair.state_names[state2.state];

					pair.state_names[state2.state] = label;
					
				} else {
					status_msg = _("'name' cannot be the same");
				}
			}
*/

		} else if (cookie3f.field == typefield::field_prefix_words) {
			if (var.first_words_is_prefix) {
				VALIDATE(false, null_str);
				// symbols["first_words_is_prefix"] = get_field_str(type_var, field_first_words_is_prefix);
				// symbols["prefix_words"] = get_field_str(type_var, field_prefix_words);
				// symbols["first_words"] = get_field_str(type_global, field_first_words);
				// status_msg = vgettext2("$first_words_is_prefix is selected, can not modify $prefix_words. it will change to $first_words after save", symbols);

			} else if (!label.empty()) {
				if (!utils::is_utf8str(label.c_str(), label.size())) {
					status_msg = _("Require utf-8 format");	
				}
			}
			if (status_msg.empty()) {
				std::vector<std::string> vstr = utils::split(label);
				std::vector<std::string> py_vstr;
				for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
					const std::string& key = *it;
					py_vstr.push_back(sensor.py_from_utf8str(key));
				}

				var.prefix_words = vstr;

				// key_2_state is std::vector, change item#'s std::string member, item's ptr maybe changed.
				var.py_prefix_words = py_vstr;
			}

		} else if (cookie3f.field == typefield::field_postfix_words) {
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
					py_vstr.push_back(sensor.py_from_utf8str(key));
				}

				var.postfix_words = vstr;

				// key_2_state is std::vector, change item#'s std::string member, item's ptr maybe changed.
				var.py_postfix_words = py_vstr;
			}

		} else if (cookie3f.field == typefield::field_major_word) {
			if (!label.empty()) {
				if (!utils::is_utf8str(label.c_str(), label.size())) {
					status_msg = _("Require utf-8 frmat");
				}
			}
			if (status_msg.empty()) {
				var.major_word = label;
				// key_2_state is std::vector, change item#'s std::string member, item's ptr maybe changed.
				var.py_major_word = sensor.py_from_utf8str(label);
			}

		} else if (cookie3f.field == typefield::field_minor_words) {
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
					py_vstr.push_back(sensor.py_from_utf8str(key));
				}

				var.minor_words = vstr;

				// key_2_state is std::vector, change item#'s std::string member, item's ptr maybe changed.
				var.py_minor_words = py_vstr;
			}
		}

		if (status_msg.empty()) {
			new_node_label = get_node_label_var(var, cookie3f.field);
		}

	}

/*
	if (l_tree) {
		sensor_update_to_l_tree(sensor);
		ttree_node* node = l_tree_->get_root_node().find_node_from_cookie(cookie);
		l_tree_->select_node(node);
	} else {
		sensor_update_to_r_tree(sensor);
	}
*/
	if (id_changed) {
		curr_tmp_sensor_id_changed();
	}

	// curr_tmp_sensor_id_change() maybe change curr_tmp_sensor_, so below must use sensor2
	const aplt::tspeech_sensor& sensor2 = *curr_tmp_sensor_;
	if (id_or_name_changed) {
		task_widget_->set_label(truncate_for_speech_name2_label(sensor2));
	}
	if (state2_name_changed) {
		VALIDATE(false, null_str);
		sensor_update_to_r_tree(sensor2);
	}

	if (!new_node_label.empty()) {
		ttree& tree = l_tree? *l_tree_: *r_tree_;
		ttree_node* node = tree.get_root_node().find_node_from_cookie(cookie);
		VALIDATE(node != nullptr, null_str);
		node->set_widget_label("label", new_node_label);
		// l_tree_->select_node(node);
	}
	set_status_label(status_msg);

	bool dirty = speech_sensors_dirty();
	save_widget_->set_active(dirty);
}

void tspeech2::click_var_val_dropdown(tbutton& widget)
{
	aplt::tspeech_sensor& sensor = *curr_tmp_sensor_;

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	// std::vector<const tmap_position*> vec_positions;
	std::vector<const aplt::tapplet*> vec_aplts;
	// std::vector<const aplt::tapplet::ttask*> vec_tasks;

	uint64_t cookie = widget.cookie();
	tcookie3f cookie3f(cookie);

	enum {index_yes, index_no};
	bool l_tree = true;
	aplt::tspeech_sensor::tvar* var_ptr = nullptr;

	if (cookie3f.type == typefield::type_global) {
		if (cookie3f.field == typefield::field_strategy) {
			initial_sel = mkey_strategy_get_menu_items(sensor.strategy, items);
		} 
		 
	} else if (cookie3f.type == typefield::type_var) {
		l_tree = false;

		aplt::tspeech_sensor::tvar& var = sensor.mutable_var(cookie3f.index);
		var_ptr = &var;


		if (cookie3f.field == typefield::field_optional) {
			items.push_back(gui2::tmenu::titem(_("Yes"), index_yes));
			items.push_back(gui2::tmenu::titem(_("No"), index_no));
			
			if (var.optional) {
				initial_sel = index_yes;
			} else {
				initial_sel = index_no;
			}

		} else if (cookie3f.field == typefield::field_first_words_is_prefix) {
			items.push_back(gui2::tmenu::titem(_("Yes"), index_yes));
			items.push_back(gui2::tmenu::titem(_("No"), index_no));
			
			if (var.first_words_is_prefix) {
				initial_sel = index_yes;
			} else {
				initial_sel = index_no;
			}

		} else if (cookie3f.field == typefield::field_aplt_id) {
			std::set<std::string> bundleids;

			vec_aplts.push_back(&aplt::fake_aplt);
			for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
				const aplt::tapplet& aplt = it->second;
				if (bundleids.count(aplt.bundleid) != 0) {
					continue;
				}
				bundleids.insert(aplt.bundleid);
				vec_aplts.push_back(&aplt);
			}

			for (std::vector<const aplt::tapplet*>::const_iterator it = vec_aplts.begin(); it != vec_aplts.end(); ++ it) {
				const aplt::tapplet& aplt = **it;

				items.push_back(gui2::tmenu::titem(aplt.name2(), items.size()));
				if (var.aplt_id == aplt.bundleid) {
					initial_sel = items.size() - 1;
				}
			}

		} /* else if (cookie3f.field == field_name) {
			const aplt::tapplet* aplt = aplt::aplt_from_bundleid(applets_, state2->async_task.aplt_id);

			for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it = aplt->tasks.begin(); it != aplt->tasks.end(); ++ it) {
				const aplt::tapplet::ttask& task = it->second;

				vec_tasks.push_back(&task);
				items.push_back(gui2::tmenu::titem(task.name, items.size()));
				if (state2->async_task.task_id == task.id) {
					initial_sel = items.size() - 1;
				}
			}

		} */ else if (cookie3f.field == typefield::field_strategy) {
			initial_sel = mkey_strategy_get_menu_items(var.strategy, items);

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
		if (cookie3f.field == typefield::field_strategy) {
			sensor.strategy = new_val;
			new_dropdown_label = aplt::minor_key_strategies.find(sensor.strategy)->second;

		} 

		new_node_label = get_node_label_global(sensor, cookie3f.field);

	} else if (cookie3f.type == typefield::type_var) {
		if (cookie3f.field == typefield::field_optional) {
			var_ptr->optional = new_val == index_yes;

			new_dropdown_label = var_ptr->optional? _("Yes"): _("No");

		} else if (cookie3f.field == typefield::field_first_words_is_prefix) {
			var_ptr->first_words_is_prefix = new_val == index_yes;

			new_dropdown_label = var_ptr->first_words_is_prefix? _("Yes"): _("No");

		} else if (cookie3f.field == typefield::field_aplt_id) {
			const aplt::tapplet* new_aplt = vec_aplts[new_val];
			new_dropdown_label = new_aplt->name2();

			var_ptr->aplt_id = new_aplt->bundleid;
/*
			// aplt_id changed, current task_id myabe not exist.
			ttree_node* node2 = tree.get_root_node().find_node_from_cookie(tcookie3f(cookie3f.index, cookie3f.type, field_task_id).u64);
			VALIDATE(node2 != nullptr, null_str);
			std::string new_node2_label = get_node_label_var(*var_ptr, field_task_id);
			node2->set_widget_label("label", new_node2_label);
*/
		} /* else if (cookie3f.field == field_name) {
			const aplt::tapplet::ttask* new_task = vec_tasks[new_val];
			new_dropdown_label = new_task->name;

			state2->async_task.task_id = new_task->id;

		} */ else if (cookie3f.field == typefield::field_strategy) {
			var_ptr->strategy = new_val;
			new_dropdown_label = aplt::minor_key_strategies.find(var_ptr->strategy)->second;

		} 
		new_node_label = get_node_label_var(*var_ptr, cookie3f.field);

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

	bool dirty = speech_sensors_dirty();
	save_widget_->set_active(dirty);
}

void tspeech2::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}


} // namespace gui2

