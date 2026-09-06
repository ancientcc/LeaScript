/* $Id: mp_login.cpp 50955 2011-08-30 19:41:15Z mordante $ */
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

#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/func_editor.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/var_editor.hpp"

#include "rose_config.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"

using namespace std::placeholders;


namespace gui2 {

REGISTER_DIALOG(launcher, func_editor)


std::string get_freq_val_alias(const tros_map& curmap, const std::string& curr_val)
{
	if (curr_val.empty()) {
		return null_str;
	}
	if (curmap.positions.count(curr_val) != 0) {
		return curmap.positions.find(curr_val)->second.name;
	}
	return curr_val;
}

/*
bool select_freq_val(const tros_map& curmap, const twidget& widget, const std::vector<std::string>& freq_vals, bool allow_last, const std::string& curr_val, std::string& result_val, std::string& result_alias)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	int at = 0;
	for (std::vector<std::string>::const_iterator it = freq_vals.begin(); it != freq_vals.end(); ++ it, at ++) {
		const std::string& special = *it;
		VALIDATE(!special.empty(), null_str);
		if (!allow_last && special[0] == '$') {
			const aplt::ttask_var* var = aplt::get_env_var(special.substr(1));
			if (var != nullptr && var_type_is_BI_env_last(var->type)) {
				continue;
			}
		}

		const std::string name = get_freq_val_alias(curmap, special);
		items.push_back(gui2::tmenu::titem(name, at));

		if (special == curr_val) {
			initial_sel = items.back().val;
		}
	}

	if (items.empty()) {
		return false;
	}

	int new_val = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return false;
		}

		new_val = dlg.selected_val();
	}

	result_val = freq_vals[new_val];
	result_alias = get_freq_val_alias(curmap, result_val);

	return true;
}
*/
tfunc_editor::tfunc_editor(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const tros_map& curmap, const std::vector<std::string>& freq_vals, const std::string& initial)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, curmap_(curmap)
	, freq_vals_(freq_vals)
	, initial_(initial)
	, curr_func_(nullptr)
	, func_id_widget_(nullptr)
	, func_desc_widget_(nullptr)
	, param_list_(nullptr)
	, var_exp_widget_(nullptr)
	, will_close_ticks_(0)
	, ignore_var_exp_text_changed_(false)
	, ignore_param_val_text_changed_(false)
{
	set_timer_interval(1000);
}

tfunc_editor::~tfunc_editor()
{
}

void tfunc_editor::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	label->set_label(_("var^function"));

	tbutton* button = find_widget<tbutton>(window_, "func_id", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tfunc_editor::click_func_id
			, this, std::ref(*button)));
	func_id_widget_ = button;

	func_desc_widget_ = find_widget<tlabel>(window_, "func_desc", false, true);

	button = find_widget<tbutton>(window_, "back", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tfunc_editor::click_back
			, this));


	tlistbox* list = find_widget<tlistbox>(window_, "param_list", false, true);
	list->enable_select(false);
	param_list_ = list;

	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(window_, "var_exp", false, true);
	scroll_text_box->set_did_text_changed(std::bind(&tfunc_editor::did_var_exp_text_changed, this, _1));
	var_exp_widget_ = scroll_text_box->tb();


	ttext_box* text_box = var_exp_widget_;

	text_box->set_border("textbox");
	window_->keyboard_capture(text_box);
	// user_widget->text_box().goto_end_of_data();  now not support, should fixed in future.

	text_box->set_placeholder("");
	const int max_chars = 256;
	text_box->set_maximum_chars(max_chars);

	text_box->set_label(initial_);
}

void tfunc_editor::post_show()
{
}

void tfunc_editor::click_back()
{
	std::string msg;
	if (curr_func_ == nullptr) {
		if (!var_exp_widget_->label().empty()) {
			msg = _("An expression has been entered, but cannot generate valid function. Do you want to continue exiting without saving?");
			
		} else {
			window_->set_retval(twindow::CANCEL);
			return;
		}

	} else if (params_.size() > curr_func_->params.size()) {
		utils::string_map symbols;
		symbols["exp"] = str_cast(params_.size());
		symbols["func"] = str_cast(curr_func_->params.size());
		msg = vgettext2("There are $exp parameters in the expression, more than the $func required by the function. Do you want to continue exiting without saving?", symbols);
	}

	if (!msg.empty()) {
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}
		window_->set_retval(twindow::CANCEL);
		return;
	}

	window_->set_retval(twindow::OK);
}

void tfunc_editor::insert_row(tlistbox& list, const aplt::tfunction_code& func, int at, const std::string& val)
{
	VALIDATE(at < (int)func.params.size(), null_str);
	std::map<std::string, std::string> data;
	char buf[32];

	// SDL_snprintf(buf, sizeof(buf), "%i/%i", at + 1, func.params.size());
	// if add 'func.params.size()', when 'did_var_exp_text_changed', require 'index' when modify.
	SDL_snprintf(buf, sizeof(buf), "%i", at + 1); 
	data["index"] = buf;
	data["name"] = func.params[at];
	data["val"] = get_freq_val_alias(curmap_, val);

	ttoggle_panel& row = list.insert_row(data);

	tbutton* button = find_widget<tbutton>(&row, "freq_val", false, true);
	button->set_label(_("Freq value"));
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tfunc_editor::click_freq_val
			, this
			, std::ref(list), std::ref(row), std::ref(*button)));

	button = find_widget<tbutton>(&row, "edit", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tfunc_editor::click_edit_param
			, this
			, std::ref(list), std::ref(row), std::ref(*button)));

	ttext_box* text_box = find_widget<ttext_box>(&row, "val", false, true);
	text_box->set_active(false);
	// text_box->set_did_text_changed(std::bind(&tfunc_editor::did_param_val_changed, this, _1));
}

void tfunc_editor::reload_param_list()
{
	tlistbox& list = *param_list_;
	list.clear();

	VALIDATE(curr_func_ != nullptr, null_str);
	const aplt::tfunction_code& func = *curr_func_;

	int cfg_params_size = func.params.size();
	for (int at = 0; at < cfg_params_size; at ++) {
		insert_row(list, func, at, at < (int)params_.size()? params_[at]: null_str);
	}
}

void tfunc_editor::click_func_id(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	for (std::map<int, aplt::tfunction_code>::const_iterator it = aplt::functions.begin(); it != aplt::functions.end(); ++ it) {
		const aplt::tfunction_code& func = it->second;

		items.push_back(gui2::tmenu::titem(func.id, func.code));

		if (curr_func_ != nullptr && curr_func_->code == func.code) {
			VALIDATE(initial_sel == nposm, null_str);
			initial_sel = items.back().val;
		}
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

	const aplt::tfunction_code& func = aplt::functions.find(new_val)->second;
	var_exp_widget_->set_label("$(" + func.id + ", )");
}

void tfunc_editor::did_param_val_changed(ttoggle_panel& row, const std::string& new_val)
{
	params_[row.at()] = new_val;

	tignore_text_changed_lock lock(*this, true);
	find_widget<ttext_box>(&row, "val", false, true)->set_label(get_freq_val_alias(curmap_, new_val));
	var_exp_widget_->set_label(aplt::curr_func->func_2_var_exp(*curr_func_, params_, false));
}

void tfunc_editor::click_freq_val(tlistbox& list, ttoggle_panel& row, tbutton& widget)
{
	VALIDATE(curr_func_ != nullptr, null_str);
	VALIDATE(params_.size() >= curr_func_->params.size(), null_str);

	int at = row.at();
	const std::string& curr_val = params_[at];

	tvar_editor_slot slot(rdpd_mgr_, pble_, privacy_, curmap_);
	std::string new_val;
	std::string new_alias;
	// if (!select_freq_val(curmap_, widget, freq_vals_, true, curr_val, new_val, new_alias)) {
	if (!select_freq_val(slot, widget, freq_vals_, true, curr_val, new_val, new_alias)) {
		return;
	}

	did_param_val_changed(row, new_val);
}

bool tfunc_editor::did_verify_param_val(const std::string& label, const std::string& initial) const
{
	if (label == initial) {
		return false;
	}
	return true;
}

void tfunc_editor::click_edit_param(tlistbox& list, ttoggle_panel& row, tbutton& widget)
{
	VALIDATE(curr_func_ != nullptr, null_str);
	VALIDATE(params_.size() >= curr_func_->params.size(), null_str);

	int at = row.at();
	const std::string& curr_val = params_[at];

	std::string title = curr_func_->params[at];
	std::string placeholder = null_str;
	int max_chars = 128;

	gui2::tedit_box_param param(title, null_str, placeholder, curr_val, null_str, null_str, 
		_("OK"), max_chars, gui2::tedit_box_param::show_cancel);
	param.did_text_changed = std::bind(&tfunc_editor::did_verify_param_val, this, _1, std::ref(curr_val));
	{
		gui2::tedit_box dlg(param);
		dlg.show(nposm, window_->get_height() / 10);
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}

	did_param_val_changed(row, param.result);
}

void tfunc_editor::did_var_exp_text_changed(ttext_box& widget)
{
	const std::string& label = widget.label();
	int func_code = nposm;
	std::vector<std::string> that_params;
	func_code = aplt::split_var_exp_4_func(label, that_params);
	if (!ignore_var_exp_text_changed_) {
		// fill all param's text_box
		tignore_text_changed_lock lock(*this, false);

		if (func_code != nposm) {
			const aplt::tfunction_code& func = aplt::functions.find(func_code)->second;

			bool code_changed = curr_func_ == nullptr || curr_func_->code != func.code;
			curr_func_ = &func;
			const int params_size = params_.size();
			const int that_params_size = that_params.size();

			bool reload = params_size == 0 || param_list_->rows() == 0;
			if (!reload) {
				for (int at = 0; at < (int)func.params.size(); at ++) {
					const std::string& val = at < that_params_size? that_params[at]: null_str;

					if (param_list_->rows() >= at + 1) {
						// use modify
						ttoggle_panel& row = param_list_->row_panel(at);
						if (code_changed) {
							row.set_child_label("name", func.params[at]);
						}
						row.set_child_label("val", get_freq_val_alias(curmap_, val));

					} else {
						// use insert
						insert_row(*param_list_, func, at, val);
					}
				}
				while (param_list_->rows() > (int)func.params.size()) {
					param_list_->erase_row(func.params.size());
				}
			}
			params_ = that_params;
			if (params_.size() < func.params.size()) {
				params_.resize(func.params.size());
			}
			if (reload) {
				reload_param_list();
			}
			VALIDATE(param_list_->rows() == (int)func.params.size(), null_str);

		} else {
			curr_func_ = nullptr;
			param_list_->clear();
			params_.clear();
		}

		func_id_widget_->set_label(curr_func_ != nullptr? curr_func_->id: null_str);
		func_desc_widget_->set_label(curr_func_ != nullptr? curr_func_->desc: null_str);

	} else {
		
	}
}

void tfunc_editor::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

