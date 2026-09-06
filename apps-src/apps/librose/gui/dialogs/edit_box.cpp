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

#include "gui/dialogs/edit_box.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/text_box2.hpp"
#include "gui/dialogs/menu.hpp"

#include "rose_config.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, edit_box)

tedit_box* tedit_box::instance = nullptr;

tedit_box::tedit_box(tedit_box_param& param)
	: param_(param)
	, ok_(nullptr)
	, cancel_(nullptr)
	, text_box_widget_(nullptr)
	, cancel_label_(_("Cancel"))
	, will_close_ticks_(0)
	, sync_with_keyboard_(false)

{
	VALIDATE(tedit_box::instance == nullptr, "only allows a maximum of one tedit_box");
	VALIDATE(param_.cancel_strategy == tedit_box_param::hide_cancel || param_.cancel_strategy >= tedit_box_param::show_cancel, null_str);
	set_timer_interval(400);
}

tedit_box::~tedit_box()
{
	tedit_box::instance = nullptr;
}

void tedit_box::pre_show()
{
	tedit_box::instance = this;
	window_->set_canvas_variable("border", variant("default_border"));

	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	if (!param_.title.empty()) {
		label->set_label(param_.title);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	label = find_widget<tlabel>(window_, "prefix", false, true);
	if (!param_.prefix.empty()) {
		label->set_label(param_.prefix);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	label = find_widget<tlabel>(window_, "remark", false, true);
	if (!param_.remark.empty()) {
		label->set_label(param_.remark);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	tbutton* button = find_widget<tbutton>(window_, "reset", false, true);
	if (!param_.reset.empty()) {
		connect_signal_mouse_left_click(
				*button
			, std::bind(
				&tedit_box::click_reset
				, this));
	} else {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(window_, "freq_val", false, true);
	button->set_label(_("Freq value"));
	if (!param_.freq_vals.empty()) {
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&tedit_box::click_freq_val
				, this
				, std::ref(*button)));
	} else {
		button->set_visible(twidget::INVISIBLE);
	}

	ok_ = find_widget<tbutton>(window_, "ok", false, true);
	int cancel_strategy = param_.cancel_strategy;
	if (!param_.ok.empty()) {
		ok_->set_label(param_.ok);
	} else {
		ok_->set_visible(twidget::INVISIBLE);
		// forbit cancel_strategy to hide_cancel.
		cancel_strategy = tedit_box_param::hide_cancel;
	}

	cancel_ = find_widget<tbutton>(window_, "cancel", true, true);
	if (cancel_strategy == tedit_box_param::hide_cancel) {
		window_->set_escape_disabled(true);
		cancel_->set_visible(twidget::INVISIBLE);
	} else {
		VALIDATE(ok_->get_visible() == twidget::VISIBLE, null_str);
		if (cancel_strategy > tedit_box_param::show_cancel) {
			will_close_ticks_ = SDL_GetTicks() + (cancel_strategy - tedit_box_param::show_cancel) * 1000;
		}
	}

	ttext_box2* text_box2 = new ttext_box2(*window_, *find_widget<twidget>(window_, "txt", false, true));
	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(window_, "scroll_txt", false, true);

	if (!param_.scroll) {
		scroll_text_box->set_visible(twidget::INVISIBLE);

		text_box2->set_did_text_changed(std::bind(&tedit_box::text_changed_callback, this, _1));
		text_box_widget_ = text_box2->text_box();
		text_box_widget_->set_border("textbox");

	} else {
		text_box2->panel()->set_visible(twidget::INVISIBLE);

		text_box_widget_ = scroll_text_box->tb();
		// text_box_widget_->set_did_text_changed(std::bind(&tedit_box::text_changed_callback, this, _1));
		scroll_text_box->set_did_text_changed(std::bind(&tedit_box::text_changed_callback, this, _1));
	}

	ttext_box* text_box_widget = text_box_widget_;

	// if (!param_.scroll) {
		window_->keyboard_capture(text_box_widget);
		// user_widget->text_box().goto_end_of_data();  now not support, should fixed in future.

		text_box_widget->set_placeholder(param_.placeholder);
		if (param_.max_chars != nposm) {
			text_box_widget->set_maximum_chars(param_.max_chars);
		}

		if (param_.initial.empty()) {
			text_changed_callback(*text_box_widget);
		} else {
			text_box_widget->set_label(param_.initial);
		}

		sync_with_keyboard_ = ok_->get_visible() != twidget::VISIBLE;
		if (sync_with_keyboard_) {
			connect_signal_pre_key_press(*text_box_widget, std::bind(&tedit_box::signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));
			keyboard::set_visible(true);
		}
	// }
}

void tedit_box::post_show()
{
}

void tedit_box::click_reset()
{
	VALIDATE(!param_.reset.empty(), null_str);
	text_box_widget_->set_label(param_.reset);
}

void tedit_box::click_freq_val(tbutton& widget)
{
	VALIDATE(!param_.freq_vals.empty(), null_str);
	const std::vector<std::pair<std::string, std::string> >& freq_vals = param_.freq_vals;

	const std::string curr_val = text_box_widget_->label();

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	int at = 0;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = freq_vals.begin(); it != freq_vals.end(); ++ it, at ++) {
		const std::string& id = it->first;
		VALIDATE(!id.empty(), null_str);
		const std::string& alias = it->second;

		std::string name = id;
		if (!alias.empty()) {
			name.append("(" + alias + ")");
		}
		items.push_back(gui2::tmenu::titem(name, at));

		if (id == curr_val) {
			initial_sel = items.back().val;
		}
	}

	if (items.empty()) {
		return;
	}

	int new_val = nposm;

	{
		gui2::tmenu dlg(items, initial_sel, nullptr, true);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		new_val = dlg.selected_val();
	}

	text_box_widget_->set_label(freq_vals[new_val].first);
}

void tedit_box::signal_handler_sdl_key_down(bool& handled
		, bool& halt
		, const SDL_Keycode key
		, SDL_Keymod modifier
		, const Uint16 unicode)
{
	VALIDATE(ok_->get_visible() != twidget::VISIBLE, null_str);
	if (key == SDLK_RETURN) {
		window_->set_retval(twindow::OK);
	}
}

void tedit_box::text_changed_callback(ttext_box& widget)
{
	param_.result = widget.label();
	bool active = true;
	if (active && param_.did_text_changed != NULL) {
		active = param_.did_text_changed(param_.result);
	}
	ok_->set_active(active);
}

void tedit_box::app_did_keyboard_hidden(int reason)
{
	if (reason == keyboard::REASON_CLOSE) {
		if (sync_with_keyboard_) {
			window_->set_retval(twindow::OK);
		}
	}
}

void tedit_box::app_timer_handler(uint32_t now)
{
	if (will_close_ticks_ != 0) {
		std::stringstream label_ss;
		if (now < will_close_ticks_) {
			int remainder = (will_close_ticks_ - now) / 1000;
			if (remainder == 0) {
				remainder = 1;
			}
			label_ss << cancel_label_ << "(" << remainder << ")";
			cancel_->set_label(label_ss.str());

		} else {
			will_close_ticks_ = 0;
			window_->set_retval(twindow::CANCEL);
		}
	}
}

} // namespace gui2

