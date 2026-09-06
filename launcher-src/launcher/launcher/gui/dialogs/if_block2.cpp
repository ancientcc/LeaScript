#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/if_block2.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/messagefs.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/func_editor.hpp"
#include "gui/dialogs/var_editor.hpp"
#include "gettext.hpp"

#include "serialization/parser.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"

using namespace std::placeholders;

std::string if_branch_do_to_string(const aplt::tif_branch& branch, int type, const aplt::ttask_cpp_pair& cpp_pair, const tros_map& curmap)
{
	std::stringstream ss;

	if (type == if_block_startup_state || type == if_block_finished) {
		ss << branch.do_str << "|";
		if (branch.do_to_state != nposm) {
			VALIDATE(branch.do_to_state >= 0 && branch.do_to_state < (int)cpp_pair.state_names.size(), null_str);
			ss << cpp_pair.state_names[branch.do_to_state];
		} else {
			ss << label_state_nposm;
		}

	} else if (type == if_block_input_var) {
		ss << branch.do_str;

	} else if (type == if_block_position) {
		if (curmap.positions.count(branch.do_str) != 0) {
			ss << curmap.positions.find(branch.do_str)->second.name;

		} else {
			ss << branch.do_str;
		}

	} else if (type == if_block_minor_words) {
		ss << branch.do_str;

	} else {
		VALIDATE(type == if_block_var_task, null_str);
		VALIDATE(branch.do_str.empty(), null_str);
		if (branch.do_bool_set == bool_set_true) {
			ss << _("Execute") << "|";
		} else {
			VALIDATE(branch.do_bool_set == bool_set_none, null_str);
		}
		if (!branch.do_map_vals.empty()) {
			ss << "(" << branch.do_map_vals.size() << ")";
		}
		for (std::map<std::string, std::string>::const_iterator it = branch.do_map_vals.begin(); it != branch.do_map_vals.end(); ++ it) {
			if (it != branch.do_map_vals.begin()) {
				ss << ", ";
			}
			ss << it->first << "=" << it->second;
		}
	} 

	return ss.str();
}

std::string tif_block_to_string(const aplt::tif_block& if_block, int type, const aplt::ttask_cpp_pair& cpp_pair, const tros_map& curmap)
{
	const std::vector<aplt::tif_branch>& branches = if_block.branches;

	utils::string_map symbols;
	symbols["count"] = str_cast(branches.size());
	std::stringstream ss;
	int at = 0;
	for (std::vector<aplt::tif_branch>::const_iterator it = branches.begin(); it != branches.end(); ++ it, at ++) {
		const aplt::tif_branch& branch = *it;
		if (it != branches.begin()) {
			ss << " ";
		}
		if (branches.size() > 1) {
			ss << (at + 1) << ")";
		}
		if (type == if_block_var_task) {
			ss << branch.judges_to_string(at) << " ";
		}
		ss << if_branch_do_to_string(branch, type, cpp_pair, curmap);
	}
	symbols["branches"] = ss.str();

	return vgettext2("$count branches. $branches", symbols);
}

namespace gui2 {

REGISTER_DIALOG(launcher, if_block2)

tif_block2::tif_block2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const std::set<aplt::taplt_var_pair>& vars, 
	const aplt::ttask_cpp_pair& cpp_pair, const tros_map& curmap, const std::string& title, const aplt::tapplet::ttask* cfg_task, aplt::tif_block& if_block, int type)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, vars_(vars)
	, cpp_pair_(cpp_pair)
	, curmap_(curmap)
	, title_(title)
	, cfg_task_(cfg_task)
	, orig_if_block_(if_block)
	, if_block_(if_block)
	, type_(type)
	, dirty_(false)
	, label_logic_nposm_(_("Empty"))
	, if_branch_list_(nullptr)
	, detail_grid_(nullptr)
	, detail_list_(nullptr)
	, do_stack_(nullptr)
	, do_finished_str_widget_(nullptr)
	, do_finished_to_state_widget_(nullptr)
	, do_special_constant_widget_(nullptr)
	, do_input_var_widget_(nullptr)
	, do_position_str_widget_(nullptr)
	, do_position_map_position_widget_(nullptr)
	, minor_words_widget_(nullptr)
	, do_var_task_execute_widget_(nullptr)
{
	set_timer_interval(1000);

	if (type_ == if_block_var_task) {
		VALIDATE(cfg_task_ != nullptr, null_str);
	} else {
		VALIDATE(cfg_task_ == nullptr, null_str);
	}

	freq_vals_.push_back("true");
	freq_vals_.push_back("false");

	for (std::set<aplt::taplt_var_pair>::const_iterator it = vars_.begin(); it != vars_.end(); ++ it) {
		const aplt::taplt_var_pair& var = *it;
		freq_vals_.push_back("$" + var.var2);
	}

	for (std::map<std::string, tmap_position>::const_iterator it = curmap_.positions.begin(); it != curmap_.positions.end(); ++ it) {
		const tmap_position& position = it->second;
		freq_vals_.push_back(position.uuid);
	}
}

void tif_block2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());
	
	find_widget<tlabel>(window_, "title", false).set_label(title_);

	tbutton* button = find_widget<tbutton>(window_, "return", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_back
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(window_, "insert_if_branch", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_insert_if_branch
			, this
			, std::ref(*button)));

	tlistbox* list = find_widget<tlistbox>(window_, "if_branch_list", false, true);
	list->set_did_row_changed(std::bind(&tif_block2::did_if_branch_changed, this, _1, _2));
	// list->set_did_can_drag(std::bind(&tif_block2::did_if_branchs_can_drag, this, _1, _2));

	button = dynamic_cast<tbutton*>(list->left_drag_grid()->find("insert_if_judge", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_insert_if_judge
			, this
			, std::ref(*list)));

	button = dynamic_cast<tbutton*>(list->left_drag_grid()->find("downward", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_downward_if_branch
			, this
			, std::ref(*list)));

	button = dynamic_cast<tbutton*>(list->left_drag_grid()->find("erase", true));
	button->set_icon("misc/bg_ff0000.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_erase_if_branch
			, this
			, std::ref(*list)));
	if_branch_list_ = list;

	tgrid* grid = find_widget<tgrid>(window_, "detail_grid", false, true);
	detail_grid_ = grid;

	list = find_widget<tlistbox>(window_, "detail_list", false, true);
	list->enable_select(false);
	detail_list_ = list;

	tstack* stack = find_widget<tstack>(window_, "do_stack", false, true);
	do_stack_ = stack;

	pre_finished(*stack->layer(FINISHED_LAYER));
	pre_input_var(*stack->layer(INPUT_VAR_LAYER));
	pre_position(*stack->layer(POSITION_LAYER));
	pre_minor_words(*stack->layer(MINOR_WORDS_LAYER));
	pre_var_task(*stack->layer(VAR_TASK_LAYER));

	if (type_ == if_block_startup_state) {
		stack->set_radio_layer(FINISHED_LAYER);

	} else if (type_ == if_block_finished) {
		stack->set_radio_layer(FINISHED_LAYER);

	} else if (type_ == if_block_input_var) {
		stack->set_radio_layer(INPUT_VAR_LAYER);

	} else if (type_ == if_block_position) {
		stack->set_radio_layer(POSITION_LAYER);

	} else if (type_ == if_block_minor_words) {
		stack->set_radio_layer(MINOR_WORDS_LAYER);

	} else {
		VALIDATE(type_ == if_block_var_task, null_str);
		stack->set_radio_layer(VAR_TASK_LAYER);

	} 

	reload_if_branch_list(!if_block_.branches.empty()? 0: nposm);
	if (if_block_.branches.empty()) {
		detail_grid_->set_visible(twidget::INVISIBLE);
	}
}

void tif_block2::post_show()
{
}

void tif_block2::pre_finished(tgrid& grid)
{
	const int type = type_ == if_block_startup_state? if_block_startup_state: if_block_finished;

	ttext_box* text_box = find_widget<ttext_box>(&grid, "speak", false, true);
	text_box->set_did_text_changed(std::bind(&tif_block2::did_do_str_text_changed, this, _1, type));
	do_finished_str_widget_ = text_box;

	tbutton* button = find_widget<tbutton>(&grid, "scroll_txt", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_do_scroll_txt
			, this
			, std::ref(*button), std::ref(*do_finished_str_widget_)));

	if (type == if_block_startup_state) {
		find_widget<tlabel>(&grid, "to_state_label", false, true)->set_label(_("Startup state"));
	}

	button = find_widget<tbutton>(&grid, "to_state", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_do_startup_state
			, this
			, std::ref(*button), type));
	do_finished_to_state_widget_ = button;
}

void tif_block2::pre_input_var(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "freq_val", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_do_freq_val
			, this
			, std::ref(*button), if_block_input_var));
	do_special_constant_widget_ = button;

	ttext_box* text_box = find_widget<ttext_box>(&grid, "input_var", false, true);
	text_box->set_did_text_changed(std::bind(&tif_block2::did_do_str_text_changed, this, _1, if_block_input_var));
	do_input_var_widget_ = text_box;
}

void tif_block2::pre_position(tgrid& grid)
{
	ttext_box* text_box = find_widget<ttext_box>(&grid, "str", false, true);
	text_box->set_did_text_changed(std::bind(&tif_block2::did_do_str_text_changed, this, _1, if_block_position));
	do_position_str_widget_ = text_box;

	tbutton* button = find_widget<tbutton>(&grid, "map_position", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_do_position_map_position
			, this
			, std::ref(*button)));
	do_position_map_position_widget_ = button;
}

void tif_block2::pre_minor_words(tgrid& grid)
{
	ttext_box* text_box = find_widget<ttext_box>(&grid, "minor_words", false, true);
	text_box->set_did_text_changed(std::bind(&tif_block2::did_do_str_text_changed, this, _1, if_block_minor_words));
	minor_words_widget_ = text_box;
}

void tif_block2::pre_var_task(tgrid& grid)
{
	ttoggle_button* toggle = find_widget<ttoggle_button>(window_, "execute", false, true);
	toggle->set_did_state_changed(std::bind(&tif_block2::did_do_execute_changed, this, _1, if_block_var_task));
	do_var_task_execute_widget_ = toggle;

	tbutton* button = find_widget<tbutton>(&grid, "edit_vars", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tif_block2::click_do_edit_vars
			, this
			, std::ref(*button), if_block_var_task));
	button->set_label(_("Task variables to be created"));
}

void tif_block2::click_back(tbutton& widget)
{
	bool ok = true;
	const std::string err_msg = if_block_.is_valid();
	if (!err_msg.empty()) {
		utils::string_map symbols;
		symbols["err_msg"] = err_msg;
		const std::string msg = vgettext2("$err_msg. Do you want to keep exiting without saving?", symbols);
		const int res = gui2::show_messagefs(msg, _("Yes"), _("No"));
		if (res != gui2::twindow::OK) {
			return;
		}
		ok = false;

	} else {
		dirty_ = orig_if_block_ != if_block_;
		orig_if_block_ = if_block_;
	}
	
	window_->set_retval(ok? twindow::OK: twindow::CANCEL);
}

void tif_block2::click_import(tbutton& widget)
{
	const std::string filename = game_config::preferences_dir + "/1.cfg";

	utils::string_map symbols;
	symbols["file"] = filename;

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

	config root_cfg;
	if (aplt::read_config_ex(stream, true, root_cfg)) {
		return;
	}
/*
	if (!stream.empty()) {
		bool all_is_utf8 = utils::is_utf8str(stream.c_str(), stream.size());
		VALIDATE(all_is_utf8, null_str);
		try {
			aplt::read_config(stream, root_cfg);
			// read(cfg_, stream);
		// } catch (twml_exception& ) {
		} catch (...) {
			root_cfg.clear();
		}

		if (root_cfg.empty()) {
			return;
		}
	}
*/
	aplt::tif_block if_block;
	bool retval = if_block.from_cfg("finished", nposm, root_cfg);

	if (retval) {
		VALIDATE(if_block == if_block_, null_str);
		// reload_if_branch_list(!if_block_.branchs.empty()? 0: nposm);

		// if (if_block_.branchs.empty()) {
		//	detail_grid_->set_visible(twidget::INVISIBLE);
		// } else {
		//	detail_grid_->set_visible(twidget::VISIBLE);
		// }
	}
}

void tif_block2::click_export(tbutton& widget)
{
	config cfg;
	if_block_.to_cfg("finished", nposm, cfg);

	std::stringstream result;
	write(result, cfg);
		
	write_file(game_config::preferences_dir + "/1.cfg", result.str().c_str(), result.str().size());
}

std::string get_state_name(const std::vector<std::string>& state_names, int state)
{
	std::string label = label_state_nposm;
	if (state != nposm) {
		VALIDATE(state >= 0 && state < (int)state_names.size(), null_str);
		label = state_names[state];
	}
	return label;
}

void tif_block2::did_if_branch_changed(tlistbox& list, ttoggle_panel& row)
{
	const int at = row.at();
	VALIDATE(at >= 0 && at < (int)if_block_.branches.size(), null_str);

	aplt::tif_branch& branch = if_block_.branches[at];

	reload_detial_list(branch);
	if (type_ == if_block_startup_state || type_ == if_block_finished) {
		do_finished_str_widget_->set_label(branch.do_str);

		std::string label = get_state_name(cpp_pair_.state_names, branch.do_to_state);
		do_finished_to_state_widget_->set_label(label);

	} else if (type_ == if_block_input_var) {
		do_input_var_widget_->set_label(branch.do_str);

	} else if (type_ == if_block_position) {
		do_position_str_widget_->set_label(branch.do_str);

	} else if (type_ == if_block_minor_words) {
		minor_words_widget_->set_label(branch.do_str);

	} else if (type_ == if_block_var_task) {
		do_var_task_execute_widget_->set_value(branch.do_bool_set == bool_set_true);
	} 

}

void tif_block2::reload_if_branch_list(int desire_sel)
{
	tlistbox& list = *if_branch_list_;
	list.clear();

	std::stringstream ss;
	std::map<std::string, std::string> data;

	char buf[32];
	const std::vector<aplt::tif_branch>& branchs = if_block_.branches;
	int branch_index = 0;
	for (std::vector<aplt::tif_branch>::const_iterator it = branchs.begin(); it != branchs.end(); ++ it, branch_index ++) {
		const aplt::tif_branch& branch = *it;

		SDL_snprintf(buf, sizeof(buf), "%i/%i", branch_index + 1, (int)branchs.size());
		data["index"] = buf;
		data["judges"] = branch.judges_to_string(branch_index);
		data["do"] = if_branch_do_to_string(branch, type_, cpp_pair_, curmap_);

		ttoggle_panel& row = list.insert_row(data);
	}

	if (desire_sel != nposm) {
		VALIDATE(desire_sel >= 0 && desire_sel < list.rows(), null_str);
		list.select_row(desire_sel);
	}
}

bool tif_block2::did_if_branchs_can_drag(tlistbox& list, ttoggle_panel& row)
{
	return true;
}

void tif_block2::did_if_branchs_changed(int desire_sel)
{
	reload_if_branch_list(desire_sel);
}

void tif_block2::click_insert_if_judge(tlistbox& list)
{
	if (vars_.empty()) {
		return;
	}

	const int drag_at = list.drag_at();
	aplt::tif_branch& if_branch = if_block_.branches[drag_at];

	if (if_branch.judges.size() >= MAX_IF_JUDGES) {
		utils::string_map symbols;
		symbols["max"] = str_cast(MAX_IF_JUDGES);
		gui2::show_message(null_str, vgettext2("A maximum of $max if judges per if branch", symbols));
		return;
	}

	// first, require cancel left_drag grid.
	// below will reload if_branch_list, may don't cancel_drag.
	// list.cancel_drag();

	int initial_logic = aplt::tif_judge::logic_and;
	int initial_judge_op = aplt::tif_judge::op_var_exist;
	const std::string initial_var_exp = "$" + vars_.begin()->var2;

	aplt::tif_judge new_judge(initial_logic, initial_judge_op, initial_var_exp);
	if_branch.judges.push_back(new_judge);

	did_if_branchs_changed(drag_at);
}

void tif_block2::click_insert_if_branch(tbutton& widget)
{
	if (if_block_.branches.size() >= MAX_IF_BRANCHES) {
		utils::string_map symbols;
		symbols["max"] = str_cast(MAX_IF_BRANCHES);
		gui2::show_message(null_str, vgettext2("A maximum of $max if branches per if block", symbols));
		return;
	}

	if (if_block_.branches.empty()) {
		detail_grid_->set_visible(twidget::VISIBLE);
	}

	aplt::tif_branch initial_branch(null_str);
	if (!vars_.empty()) {
		const std::string initial_var_exp = "$" + vars_.begin()->var2;
		aplt::tif_judge initial_judge(nposm, aplt::tif_judge::op_var_exist, initial_var_exp);
		initial_branch.judges.push_back(initial_judge);
	}
	if_block_.branches.push_back(initial_branch);

	did_if_branchs_changed(if_block_.branches.size() - 1);
}

void tif_block2::click_downward_if_branch(tlistbox& list)
{
	VALIDATE(if_block_.branches.size() >= 2, null_str);

	const int drag_at = list.drag_at();
	if_block_.branch_downward1(drag_at);

	int desire_sel = drag_at + 1;
	if (desire_sel == (int)if_block_.branches.size()) {
		desire_sel = 0;
	}
	did_if_branchs_changed(desire_sel);
}

void tif_block2::click_erase_if_branch(tlistbox& list)
{
	const int drag_at = list.drag_at();

	std::vector<aplt::tif_branch>::iterator erase_it = if_block_.branches.begin();
	if (drag_at != 0) {
		std::advance(erase_it, drag_at);
	}

	char buf[32];
	SDL_snprintf(buf, sizeof(buf), "%i/%i", drag_at + 1, (int)if_block_.branches.size());
	utils::string_map symbols;
	symbols["branch"] = buf;
	const std::string msg = vgettext2("Do you want to erase if branch($branch)", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	// first, require cancel left_drag grid.
	// below will reload if_branch_list, may don't cancel_drag.
	// list.cancel_drag();

	if_block_.branches.erase(erase_it);

	int desire_sel = nposm;
	if (!if_block_.branches.empty()) {
		desire_sel = drag_at;
		if (desire_sel == (int)if_block_.branches.size()) {
			desire_sel = if_block_.branches.size() - 1;
		}
	}

	did_if_branchs_changed(desire_sel);

	if (if_block_.branches.empty()) {
		detail_grid_->set_visible(twidget::INVISIBLE);
	}
}

void tif_block2::reload_detial_list(const aplt::tif_branch& branch)
{
	tlistbox& list = *detail_list_;
	list.clear();

	std::map<std::string, std::string> data;

	char buf[32];
	const std::vector<aplt::tif_judge>& if_judges = branch.judges;
	int judge_index = 0;
	for (std::vector<aplt::tif_judge>::const_iterator it = if_judges.begin(); it != if_judges.end(); ++ it, judge_index ++) {
		const aplt::tif_judge& judge = *it;

		SDL_snprintf(buf, sizeof(buf), "%i/%i", judge_index + 1, (int)if_judges.size());
		data["index"] = buf;
		std::string val = label_logic_nposm_;
		if (aplt::if_logic_ops.count(judge.logic) != 0) {
			val = aplt::if_logic_ops.find(judge.logic)->second.id;
		} else {
			VALIDATE(judge.logic == nposm, null_str);
		}
		data["logic"] = val;
		VALIDATE(aplt::if_judge_ops.count(judge.op) != 0, null_str);
		data["judge_op"] = aplt::if_judge_ops.find(judge.op)->second.id;
		data["var_exp"] = aplt::var_exp_for_browser(judge.var_exp);

		bool visible_val_widget = true;
		std::string var_val;
		if (aplt::tif_judge::op_has_operand(judge.op)) {
			if (judge.use_r_val()) {
				var_val = judge.r_val.str();
			} else {
				var_val = judge.r_exp;
			}
		} else {
			visible_val_widget = false;
		}

		data["r_exp"] = var_val;

		ttoggle_panel& row = list.insert_row(data);

		const std::map<int, std::string> fields = {
			{type_logic, "logic"},
			{type_judge_op, "judge_op"},
			{type_var_exp, "var_exp"},
			{type_r_exp, "r_exp"},
		};

		aplt::tif_judge& mutable_judge = *const_cast<aplt::tif_judge*>(&judge);
		for (std::map<int, std::string>::const_iterator it = fields.begin(); it != fields.end(); ++ it) {
			int type = it->first;
			const std::string& id = it->second;
			tbutton* button = find_widget<tbutton>(&row, id, false, true);
			connect_signal_mouse_left_click(
					  *button
					, std::bind(
					&tif_block2::click_if_judge
					, this, std::ref(row), std::ref(*button), std::ref(mutable_judge), type));

			if (type == type_r_exp && !visible_val_widget) {
				button->set_visible(twidget::HIDDEN);
			}
		}
/*
		ttext_box* text_box = find_widget<ttext_box>(&row, "var_val", false, true);
		if (visible_val_widget) {
			text_box->set_did_text_changed(std::bind(&tif_block2::did_var_val_text_changed, this, _1, judge_index));
		} else {
			text_box->set_visible(twidget::HIDDEN);
		}
*/
		tbutton* button = find_widget<tbutton>(&row, "erase", false, true);
		connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tif_block2::click_erase_if_judge
			, this, std::ref(row), std::ref(*button)));

	}
}

bool tif_block2::did_verify_var_exp(const std::string& label, const std::string& initial) const
{
	if (label == initial) {
		return false;
	}
	if (aplt::var_exp_which_type(label, nullptr) != aplt::var_exp_type_func) {
		return false;
	}
	return true;
}


void tif_block2::click_if_judge(ttoggle_panel& row, tbutton& widget, aplt::tif_judge& if_judge, int type)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	if (type == type_logic) {
		items.push_back(gui2::tmenu::titem(label_logic_nposm_, aplt::tif_judge::logic_count));
		initial_sel = if_judge.logic == nposm? items.back().val: nposm;

		for (std::map<int, tcode3>::const_iterator it = aplt::if_logic_ops.begin(); it != aplt::if_logic_ops.end(); ++ it) {
			const tcode3& logic = it->second;

			items.push_back(gui2::tmenu::titem(logic.id, logic.code));

			if (if_judge.logic == logic.code) {
				VALIDATE(initial_sel == nposm, null_str);
				initial_sel = items.back().val;
			}
		}

	} else if (type == type_judge_op) {
		for (std::map<int, tcode3>::const_iterator it = aplt::if_judge_ops.begin(); it != aplt::if_judge_ops.end(); ++ it) {
			const tcode3& judge_ops = it->second;

			items.push_back(gui2::tmenu::titem(judge_ops.id, judge_ops.code));

			if (if_judge.op == judge_ops.code) {
				VALIDATE(initial_sel == nposm, null_str);
				initial_sel = items.back().val;
			}
		}

	} else if (type == type_var_exp || type == type_r_exp) {
		std::stringstream ss;
		std::string no_symboled_str;
		int exp_type = nposm;
		if (type == type_var_exp) {
			exp_type = aplt::var_exp_which_type(if_judge.var_exp, &no_symboled_str);
		} else {
			VALIDATE(type == type_r_exp, null_str);
			if (!if_judge.use_r_val()) {
				exp_type = aplt::var_exp_which_type(if_judge.r_exp, &no_symboled_str);
				VALIDATE(exp_type != nposm, null_str);
			}
		}
		for (std::set<aplt::taplt_var_pair>::const_iterator it = vars_.begin(); it != vars_.end(); ++ it) {
			const aplt::taplt_var_pair& pair = *it;

			ss.str("");
			ss << "(" << pair.aplt->name << ")" << pair.var2;
			items.push_back(gui2::tmenu::titem(ss.str(), items.size()));

			if (exp_type == aplt::var_exp_type_var && no_symboled_str == pair.var2) {
				VALIDATE(initial_sel == nposm, null_str);
				initial_sel = items.back().val;
			}
		}

		// function
		ss.str("");
		ss << _("var^function");
		if (exp_type == aplt::var_exp_type_func) {
			const std::string& exp = type == type_var_exp? if_judge.var_exp: if_judge.r_exp;
			ss << ": " << utils::truncate_to_max_chars2(exp, 25, true);
			// don't set initial_sel, let user can change expression
			// initial_sel = items.size();
		}
		items.push_back(gui2::tmenu::titem(ss.str(), items.size()));

		// custom string
		if (type == type_r_exp) {
			ss.str("");
			ss << _("String");
			if (exp_type == nposm) {
				ss << ": " << utils::truncate_to_max_chars2(if_judge.r_val.str(), 25, true);
			} else {
				ss << ": " << utils::truncate_to_max_chars2(if_judge.r_exp, 25, true);
			}
			items.insert(items.begin(), gui2::tmenu::titem(ss.str(), items.size()));
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


	if (type == type_logic) {
		if (new_val != aplt::tif_judge::logic_count) {
			if_judge.logic = new_val;

		} else {
			if_judge.logic = nposm;
		}

	} else if (type == type_judge_op) {
		if_judge.op = new_val;

		if (aplt::tif_judge::op_has_operand(if_judge.op)) {
			if (!if_judge.r_exp.empty()) {
				VALIDATE(if_judge.r_val.type() == var_type_nposm, null_str);
				VALIDATE(str_is_var_exp(if_judge.r_exp), null_str);

			} else {
				// Modify the operand to the desired type of 'op'.
				VALIDATE(if_judge.r_exp.empty(), null_str);
				if (aplt::tif_judge::op_operand_is_string(if_judge.op)) {
					if (if_judge.r_val.type() != var_type_string) {
						// if val's type isn't string, modify it.
						if_judge.r_val.from_string(null_str, true);
					}

				} else if (aplt::tif_judge::op_operand_is_bool(if_judge.op)) {
					if (if_judge.r_val.type() != var_type_bool) {
						// if val's type isn't bool, modify it.
						if_judge.r_val.from_bool(false);
					}

				} else {
					VALIDATE(aplt::tif_judge::op_operand_is_numerical(if_judge.op), null_str);
					if (if_judge.r_val.type() != var_type_integer && if_judge.r_val.type() != var_type_double) {
						// if val's type isn't integer, modify it.
						if_judge.r_val.from_int(0);
					}
				}
			}
		}

	} else if (type == type_var_exp || type == type_r_exp) {
		std::string r_label;
		if (new_val < (int)vars_.size()) {
			std::set<aplt::taplt_var_pair>::const_iterator it = vars_.begin();
			if (new_val != 0) {
				std::advance(it, new_val);
			}
			const aplt::taplt_var_pair& pair = *it;

			if (type == type_var_exp) {
				if_judge.var_exp = "$" + pair.var2;
			} else {
				r_label = "$" + pair.var2;
			}

		} else if (new_val == (int)vars_.size()) {
			// function
			std::string initial_exp = if_judge.var_exp;
			if (type == type_r_exp) {
				if (if_judge.use_r_val()) {
					initial_exp = if_judge.r_val.str();
				} else {
					initial_exp = if_judge.r_exp;
				}
			}

			std::string result;
			{
				gui2::tfunc_editor dlg(rdpd_mgr_, pble_, privacy_, curmap_, freq_vals_, initial_exp);
				dlg.show();
				if (dlg.get_retval() != twindow::OK) {
					return;
				}
				result = dlg.get_result();
			}
			if (result != initial_exp) {
				if (type == type_var_exp) {
					if_judge.var_exp = result;
				} else {
					r_label = result;
				}
			}

		} else {
			// string
			VALIDATE(type == type_r_exp, null_str);

			std::string title = _("String");
			std::string placeholder;
			const std::string initial = if_judge.use_r_val()? if_judge.r_val.str(): if_judge.r_exp;
			int max_chars = 128;
			gui2::tedit_box_param param(title, null_str, placeholder, initial, null_str, null_str, 
				_("OK"), max_chars, gui2::tedit_box_param::show_cancel, true);
			// param.did_text_changed = std::bind(&tfi_block2::did_verify_sn, this, _1);
			{
				gui2::tedit_box dlg(param);
				dlg.show(nposm, window_->get_height() / 10);
				if (dlg.get_retval() != twindow::OK) {
					return;
				}
			}

			r_label = param.result;
		}

		if (type == type_r_exp) {
			did_var_r_val_text_changed(r_label, if_judge);
		}
	}

	ttoggle_panel* branch_cursel = if_branch_list_->cursel();
	VALIDATE(branch_cursel != nullptr, null_str);
	did_if_branchs_changed(branch_cursel->at());
}

void tif_block2::click_erase_if_judge(ttoggle_panel& row, tbutton& widget)
{
	ttoggle_panel* branch_cursel = if_branch_list_->cursel();
	VALIDATE(branch_cursel != nullptr, null_str);

	aplt::tif_branch& branch = if_block_.branches[branch_cursel->at()];

	char buf[32];
	SDL_snprintf(buf, sizeof(buf), "%i/%i", branch_cursel->at() + 1, (int)branch.judges.size());
	utils::string_map symbols;
	symbols["judge"] = buf;
	const std::string msg = vgettext2("Do you want to erase if judge($judge)", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	std::vector<aplt::tif_judge>::iterator erase_it = branch.judges.begin();
	if (row.at() != 0) {
		std::advance(erase_it, row.at());
	}
	branch.judges.erase(erase_it);

	did_if_branchs_changed(branch_cursel->at());
}

void tif_block2::did_var_r_val_text_changed(const std::string& label, aplt::tif_judge& judge)
{
	// const std::string& label = widget.label();
	if (!utils::is_utf8str(label.c_str(), label.size())) {
		return;
	}

	ttoggle_panel* branch_cursel = if_branch_list_->cursel();
	VALIDATE(branch_cursel != nullptr, null_str);

	aplt::tif_branch& branch = if_block_.branches[branch_cursel->at()];
	// VALIDATE(judge_at >= 0 && (int)branch.judges.size(), null_str); 

	// aplt::tif_judge& judge = branch.judges[judge_at];
	if (aplt::tif_judge::op_has_operand(judge.op)) {
		judge.r_exp = label;
		if (str_is_var_exp(label)) {
			judge.r_val.set_nposm();

		} else {
			if (aplt::tif_judge::op_operand_is_string(judge.op)) {
				judge.r_val.from_string(label, true);

			} else {
				judge.r_val.from_string(label, false);

				if (aplt::tif_judge::op_operand_is_bool(judge.op)) {
					if (judge.r_val.type() != var_type_bool) {
						judge.r_val.from_bool(false);
					}

				} else {
					VALIDATE(aplt::tif_judge::op_operand_is_numerical(judge.op), null_str);
					if (judge.r_val.type() != var_type_integer && judge.r_val.type() != var_type_double) {
						judge.r_val.from_int(0);
					}
				}
			}
			judge.r_exp.clear();
		}
		
	} else {
		VALIDATE(false, null_str);
	}
/*
	const std::string new_judges_label = branch.judges_to_string(branch_cursel->at());

	// did_if_branchs_changed(branch_cursel->at());
	branch_cursel->set_child_label("judges", new_judges_label);
*/
}

static bool text_widget_is_do_str(int type)
{
	return type != if_block_var_task;
}

void tif_block2::did_do_str_text_changed(ttext_box& widget, int type)
{
	VALIDATE(type == if_block_startup_state || type == if_block_finished || type == if_block_input_var || 
		type == if_block_position || type == if_block_minor_words || type == if_block_var_task, null_str);

	const std::string& label = widget.label();
	if (!utils::is_utf8str(label.c_str(), label.size())) {
		return;
	}

	ttoggle_panel* branch_cursel = if_branch_list_->cursel();
	VALIDATE(branch_cursel != nullptr, null_str);

	aplt::tif_branch& branch = if_block_.branches[branch_cursel->at()];
	if (branch.do_str == label) {
		return;
	}

	branch.do_str = label;

	// did_if_branchs_changed(branch_cursel->at());
	branch_cursel->set_child_label("do", if_branch_do_to_string(branch, type, cpp_pair_, curmap_));
}

void tif_block2::did_do_execute_changed(ttoggle_button& widget, int type)
{
	VALIDATE(type == if_block_var_task, null_str);

	ttoggle_panel* branch_cursel = if_branch_list_->cursel();
	VALIDATE(branch_cursel != nullptr, null_str);

	aplt::tif_branch& branch = if_block_.branches[branch_cursel->at()];
	if (widget.get_value()) {
		if (branch.do_bool_set == bool_set_true) {
			return;
		}
		branch.do_bool_set = bool_set_true;
	} else {
		if (branch.do_bool_set == bool_set_none) {
			return;
		}
		branch.do_bool_set = bool_set_none;
	}
	branch_cursel->set_child_label("do", if_branch_do_to_string(branch, type, cpp_pair_, curmap_));
}

void tif_block2::click_do_startup_state(tbutton& widget, int type)
{
	VALIDATE(type == if_block_startup_state || type == if_block_finished, null_str);

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	ttoggle_panel* branch_cursel = if_branch_list_->cursel();
	VALIDATE(branch_cursel != nullptr, null_str);
	ttoggle_panel& row = *branch_cursel;

	aplt::tif_branch& branch = if_block_.branches[row.at()];

	items.push_back(gui2::tmenu::titem(label_state_nposm, cpp_pair_.state_names.size()));

	for (std::vector<std::string>::const_iterator it = cpp_pair_.state_names.begin(); it != cpp_pair_.state_names.end(); ++ it) {
		const std::string& name = *it;
		items.push_back(gui2::tmenu::titem(name, items.size() - 1));
	}

	if (branch.do_to_state != nposm) {
		initial_sel = branch.do_to_state;
	} else {
		initial_sel = cpp_pair_.state_names.size();
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

	if (new_val < (int)cpp_pair_.state_names.size()) {
		branch.do_to_state = new_val;
	} else {
		branch.do_to_state = nposm;
	}

	const std::string new_label = get_state_name(cpp_pair_.state_names, branch.do_to_state);
	widget.set_label(new_label);

	// did_if_branchs_changed(branch_cursel->at());
	branch_cursel->set_child_label("do", if_branch_do_to_string(branch, type, cpp_pair_, curmap_));
}

void tif_block2::click_do_scroll_txt(tbutton& widget, ttext_box& text_widget)
{
	std::string title = _("String");
	std::string placeholder = null_str;
	int max_chars = 512;

	gui2::tedit_box_param param(title, null_str, placeholder, text_widget.label(), null_str, null_str, 
		_("OK"), max_chars, gui2::tedit_box_param::show_cancel, true);
	// param.did_text_changed = std::bind(&tfi_block2::did_verify_sn, this, _1);
	{
		gui2::tedit_box dlg(param);
		dlg.show(nposm, window_->get_height() / 10);
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}
	if (param.result != text_widget.label()) {
		text_widget.set_label(param.result);
	}
}

void tif_block2::click_do_edit_vars(tbutton& widget, int type)
{
	VALIDATE(type == if_block_var_task, null_str);

	ttoggle_panel* branch_cursel = if_branch_list_->cursel();
	VALIDATE(branch_cursel != nullptr, null_str);
	ttoggle_panel& row = *branch_cursel;

	aplt::tif_branch& branch = if_block_.branches[row.at()];

	{
		utils::string_map symbols;

		std::string title = _("Variable");
		const std::string name_prefix = aplt::fake_aplt.bundleid;
		symbols["prefix"] = name_prefix + "__";
		std::string remark = vgettext2("remark^var_task's if_block, do_map_vals, $prefix", symbols);

		tvar_editor_slot slot(rdpd_mgr_, pble_, privacy_, curmap_);
		gui2::trvar_editor dlg(slot, title, remark, nullptr, freq_vals_, name_prefix, branch.do_map_vals);
		dlg.show();
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}
	branch_cursel->set_child_label("do", if_branch_do_to_string(branch, type, cpp_pair_, curmap_));
}

void tif_block2::click_do_freq_val(tbutton& widget, int type)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	ttoggle_panel* branch_cursel = if_branch_list_->cursel();
	VALIDATE(branch_cursel != nullptr, null_str);
	ttoggle_panel& row = *branch_cursel;

	aplt::tif_branch& branch = if_block_.branches[row.at()];

	for (std::vector<std::string>::const_iterator it = freq_vals_.begin(); it != freq_vals_.end(); ++ it) {
		const std::string& special = *it;

		items.push_back(gui2::tmenu::titem(special, items.size()));

		if (special == branch.do_str) {
			VALIDATE(initial_sel == nposm, null_str);
			initial_sel = items.back().val;
		}
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

	const std::string new_special = freq_vals_[new_val];
	do_input_var_widget_->set_label(new_special);
}

void tif_block2::click_do_position_map_position(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	ttoggle_panel* branch_cursel = if_branch_list_->cursel();
	VALIDATE(branch_cursel != nullptr, null_str);
	ttoggle_panel& row = *branch_cursel;

	aplt::tif_branch& branch = if_block_.branches[row.at()];

	items.push_back(gui2::tmenu::titem(_("Empty"), curmap_.positions.size()));
	initial_sel = branch.do_str.empty()? items.back().val: nposm;
			
	for (std::map<std::string, tmap_position>::const_iterator it = curmap_.positions.begin(); it != curmap_.positions.end(); ++ it) {
		const tmap_position& position = it->second;
		// if (position.uuid == task_other_position) {
		//	continue;
		// }
		
		items.push_back(gui2::tmenu::titem(position.name, items.size() - 1));

		if (position.uuid == branch.do_str) {
			VALIDATE(initial_sel == nposm, null_str);
			initial_sel = items.back().val;
		}
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

	std::string new_position_uuid;
	if (new_val != (int)curmap_.positions.size()) {
		std::map<std::string, tmap_position>::const_iterator hit_it = curmap_.positions.begin();
		if (new_val != 0) {
			std::advance(hit_it, new_val);
		}
		const tmap_position& new_position = hit_it->second;
		new_position_uuid = new_position.uuid;
	}

	branch.do_str = new_position_uuid;

	// widget.set_label(bool_set_types.find(branch.do_bool_set)->second.name);

	// did_if_branchs_changed(branch_cursel->at());
	branch_cursel->set_child_label("do", if_branch_do_to_string(branch, if_block_position, cpp_pair_, curmap_));
}

void tif_block2::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

