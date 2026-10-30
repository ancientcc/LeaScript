/* $Id: message.cpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#include "gui/dialogs/messagefs.hpp"

#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "rose_config.hpp"

// #include <boost/foreach.hpp>

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, messagefs)

tmessagefs::tmessagefs(const std::string& msg, int msg_halign, const std::string& ok_caption, const std::string& cancel_caption, const std::string& extra_caption)
	: msg_(msg)
	, msg_halign_(msg_halign)
	, extra_caption_(extra_caption)
	, ok_caption_(ok_caption.empty()? _("OK"): ok_caption)
	, cancel_caption_(cancel_caption.empty()? _("Cancel"): cancel_caption)
{
	VALIDATE(msg_halign_ == halign_left || msg_halign_ == halign_center, null_str);
}

void tmessagefs::pre_show()
{
	window_->set_canvas_variable("border", variant("default_border"));

	// ***** ***** ***** ***** Set up the widgets ***** ***** ***** *****
	tlabel* label_widget = nullptr;
	if (msg_halign_ == halign_left) {
		label_widget = find_widget<tlabel>(window_, "left_label", false, true);
		find_widget<tlabel>(window_, "center_label", false, true)->set_visible(twidget::INVISIBLE);

	} else {
		// When using 'halign_center', the caller must ensure that msg_ can fits on a single line.
		find_widget<tlabel>(window_, "left_label", false, true)->set_visible(twidget::INVISIBLE);
		label_widget = find_widget<tlabel>(window_, "center_label", false, true);
	}
	label_widget->set_label(msg_);

	// The label might not always be a scroll_label but the capturing
	// shouldn't hurt.
	// window_->keyboard_capture(label_widget);

	tbutton& ok = find_widget<tbutton>(window_, "ok", false);
	ok.set_label(ok_caption_);

	tbutton& cancel = find_widget<tbutton>(window_, "cancel", false);
	cancel.set_label(cancel_caption_);

	tbutton& extra = find_widget<tbutton>(window_, "extra", false);
	if (!extra_caption_.empty()) {
		connect_signal_mouse_left_click(
				extra
			, std::bind(
			&tmessagefs::click_extra
			, this, std::ref(extra)));
		extra.set_label(extra_caption_);

	} else {
		find_widget<tgrid>(window_, "extra_grid", false).set_visible(twidget::INVISIBLE);
	}
}

void tmessagefs::click_extra(tbutton& widget)
{
	window_->set_retval(twindow::EXTRA1_BUTTON);
}

int show_messagefs(const std::string& msg, const std::string& ok_caption, const std::string& cancel_caption, const std::string& extra_caption, int msg_halign)
{
	tmessagefs dlg(msg, msg_halign, ok_caption, cancel_caption, extra_caption);
	dlg.show();
	return dlg.get_retval();
}


} // namespace gui2

