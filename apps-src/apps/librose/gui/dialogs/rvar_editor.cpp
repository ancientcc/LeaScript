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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/rvar_editor.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/edit_box.hpp"

#include "rose_config.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"

using namespace std::placeholders;


namespace gui2 {

REGISTER_DIALOG(rose, rvar_editor)

trvar_editor::trvar_editor(/*net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const tros_map& curmap,*/ tslot& slot, const std::string& title, const std::string& remark,
	const aplt::tapplet::ttask* cfg_task, const std::vector<std::string>& freq_vals, const std::string& name_prefix, std::map<std::string, std::string>& map_vals)
	: slot_(slot) // tstatusbar(rdpd_mgr, pble, privacy)
	// , curmap_(curmap)
	, title_(title)
	, remark_(remark)
	, cfg_task_(cfg_task)
	, freq_vals_(freq_vals)
	, name_prefix_(name_prefix)
	, original_map_vals_(map_vals)
	, map_vals_(map_vals)
	, keep_map_vals_name_(cfg_task_ != nullptr)
	, var_list_(nullptr)
{
	slot_.rvar_editor_ = this;
	set_timer_interval(1000);

	if (cfg_task_ != nullptr) {
		std::set<std::string> valid_vars;
		for (std::vector<aplt::tapplet::tvar>::const_iterator it = cfg_task_->vars.begin(); it != cfg_task_->vars.end(); ++ it) {
			const aplt::tapplet::tvar& var = *it;
			if (!var.input) {
				continue;
			}
			if (map_vals_.count(var.name) == 0) {
				map_vals_.insert(std::make_pair(var.name, null_str));
			}
			valid_vars.insert(var.name);
		}
		for (std::map<std::string, std::string>::const_iterator it = map_vals_.begin(); it != map_vals_.end(); ) {
			const std::string& name = it->first;
			if (valid_vars.count(name) == 0) {
				map_vals_.erase(it ++);
			} else {
				++ it;
			}
		}
	}
}

trvar_editor::~trvar_editor()
{
}

void trvar_editor::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	// tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	label->set_label(title_);

	if (!remark_.empty()) {
		find_widget<tlabel>(window_, "remark", false, true)->set_label(remark_);
	}

	tbutton* button = find_widget<tbutton>(window_, "insert_var", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&trvar_editor::click_insert_var
			, this, std::ref(*button)));
	if (keep_map_vals_name_) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(window_, "back", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&trvar_editor::click_back
			, this));


	tlistbox* list = find_widget<tlistbox>(window_, "var_list", false, true);
	list->enable_select(false);
	var_list_ = list;

	reload_var_list();

	slot_.window_ = window_;
	slot_.rvar_editor_pre_show(*window_);
}

void trvar_editor::post_show()
{
}

void trvar_editor::click_back()
{
/*
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
*/
	for (std::map<std::string, std::string>::const_iterator it = map_vals_.begin(); it != map_vals_.end(); ) {
		const std::string& val = it->second;
		if (val.empty()) {
			map_vals_.erase(it ++);
		} else {
			++ it;
		}
	}

	window_->set_retval(original_map_vals_ != map_vals_? twindow::OK: twindow::CANCEL);
}

bool trvar_editor::varify_new_var_name(const std::string& label) const
{
	if (!isvalid_normal_id_or_var_name224(label)) {
		return false;
	}
	if (map_vals_.count(label) != 0) {
		return false;
	}
	return true;
}

void trvar_editor::click_insert_var(tbutton& widget)
{
	std::string title = _("New variable");
	std::string placeholder = _("Alpha, digit, or '_'");

	// utils::string_map symbols;
	// symbols["min"] = str_cast(MAX_NORMAL_ID_OR_VAR_NAME_BYTES);
	// symbols["max"] = str_cast(MAX_NORMAL_ID_OR_VAR_NAME_BYTES);
	// std::string remark = vgettext2("variable name remark [$min, $max]", symbols);
	// const std::string prefix = name_prefix_ + "__";
	const std::string prefix;
	std::string remark;
	gui2::tedit_box_param param(title, prefix, placeholder, null_str, remark, null_str, _("OK"), MAX_NORMAL_ID_OR_VAR_NAME_BYTES, gui2::tedit_box_param::show_cancel);
	param.did_text_changed = std::bind(&trvar_editor::varify_new_var_name, this, _1);
	{
		gui2::tedit_box dlg(param);
		dlg.show(nposm, window_->get_height() / 8);
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}

	// const std::string var_name = utils::join_app_prefix_id(name_prefix_, param.result);
	const std::string var_name = param.result;
	std::pair<std::map<std::string, std::string>::iterator, bool> ins = map_vals_.insert(std::make_pair(var_name, null_str));
	VALIDATE(ins.second, null_str);

	reload_var_list();
}

static std::string val_name_label(const std::string& val)
{
	if (!val.empty()) {
		return val;
	}

	std::stringstream ss;
	ss << "[" << _("Empty") << "]";
	return ss.str();
}

void trvar_editor::reload_var_list()
{
	tlistbox& list = *var_list_;
	list.clear();

	int at = 0;
	for (std::map<std::string, std::string>::const_iterator it = map_vals_.begin(); it != map_vals_.end(); ++ it, at ++) {
		const std::string& var_name = it->first;
		const std::string& var_val = it->second;

		std::map<std::string, std::string> data;
		char buf[32];

		// SDL_snprintf(buf, sizeof(buf), "%i/%i", at + 1, func.params.size());
		// if add 'func.params.size()', when 'did_var_exp_text_changed', require 'index' when modify.
		SDL_snprintf(buf, sizeof(buf), "%i", at + 1); 
		data["index"] = buf;
		data["name"] = var_name; // utils::join_app_prefix_id(name_prefix_, var_name);
		data["val"] = val_name_label(var_val);

		ttoggle_panel& row = list.insert_row(data);

		tbutton* button = find_widget<tbutton>(&row, "freq_val", false, true);
		button->set_label(_("Freq value"));
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&trvar_editor::click_freq_val
				, this
				, std::ref(list), std::ref(row), std::ref(*button)));
		if (freq_vals_.empty()) {
			button->set_visible(twidget::INVISIBLE);
		}

		button = find_widget<tbutton>(&row, "edit", false, true);
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&trvar_editor::click_edit_var_val
				, this
				, std::ref(list), std::ref(row), std::ref(*button)));

		button = find_widget<tbutton>(&row, "erase", false, true);
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&trvar_editor::click_erase_var
				, this
				, std::ref(list), std::ref(row)));
		if (keep_map_vals_name_) {
			button->set_visible(twidget::INVISIBLE);
		}

		ttext_box* text_box = find_widget<ttext_box>(&row, "val", false, true);
		text_box->set_border(null_str);
		text_box->set_active(false);
	}
}

void trvar_editor::did_var_val_changed(ttoggle_panel& row, const std::string& new_val)
{
	std::map<std::string, std::string>::iterator hit_it = map_vals_.begin();
	if (row.at() != 0) {
		std::advance(hit_it, row.at());
	}

	hit_it->second = new_val;

	// const std::string new_label = get_freq_val_alias(curmap_, new_val);
	find_widget<ttext_box>(&row, "val", false, true)->set_label(val_name_label(new_val));
}

void trvar_editor::click_freq_val(tlistbox& list, ttoggle_panel& row, tbutton& widget)
{
	int at = row.at();

	std::map<std::string, std::string>::const_iterator hit_it = map_vals_.begin();
	if (at != 0) {
		std::advance(hit_it, at);
	}
	const std::string& curr_val = hit_it->second;

	std::string new_val;
	std::string new_alias;
	if (!select_freq_val(slot_, widget, freq_vals_, false, curr_val, new_val, new_alias)) {
		return;
	}

	did_var_val_changed(row, new_val);
}

bool trvar_editor::did_verify_var_val(const std::string& label, const std::string& initial) const
{
	if (label == initial) {
		return false;
	}
	return true;
}

void trvar_editor::click_edit_var_val(tlistbox& list, ttoggle_panel& row, tbutton& widget)
{
	int at = row.at();
	std::map<std::string, std::string>::const_iterator hit_it = map_vals_.begin();
	if (at != 0) {
		std::advance(hit_it, at);
	}
	const std::string& curr_val = hit_it->second;

	std::string title = hit_it->first;
	std::string placeholder = null_str;
	int max_chars = 64;

	gui2::tedit_box_param param(title, null_str, placeholder, curr_val, null_str, null_str, 
		_("OK"), max_chars, gui2::tedit_box_param::show_cancel);
	param.did_text_changed = std::bind(&trvar_editor::did_verify_var_val, this, _1, std::ref(curr_val));
	{
		gui2::tedit_box dlg(param);
		dlg.show(nposm, window_->get_height() / 10);
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}

	did_var_val_changed(row, param.result);
}

void trvar_editor::click_erase_var(tlistbox& list, ttoggle_panel& row)
{
	int at = row.at();
	std::map<std::string, std::string>::const_iterator hit_it = map_vals_.begin();
	if (at != 0) {
		std::advance(hit_it, at);
	}

	utils::string_map symbols;
	symbols["var"] = hit_it->first;
	const std::string msg = vgettext2("Do you want to delete variable($var)?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}
	map_vals_.erase(hit_it);

	reload_var_list();
}

void trvar_editor::app_timer_handler(uint32_t now)
{
	// refresh_statusbar_grid(now);
	slot_.rvar_timer_handler(now);
}

//
// gui2 api
//
bool select_freq_val(const trvar_editor::tslot& slot, const twidget& widget, const std::vector<std::string>& freq_vals, bool allow_last, const std::string& curr_val, std::string& result_val, std::string& result_alias)
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

		const std::string name = slot.get_freq_val_alias(special);
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
	result_alias = slot.get_freq_val_alias(result_val);

	return true;
}

} // namespace gui2

