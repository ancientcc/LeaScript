/* $Id: scroll_label.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/input_view.hpp"

#include "gui/widgets/settings.hpp"
#include "gui/widgets/panel.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/text_box.hpp"
#include "chinese.hpp"
#include "gettext.hpp"
#include "font.hpp"
#include "sound.hpp"
#include "filesystem.hpp"

#include "rose_config.hpp"
#include "config_cache.hpp"

#include <algorithm>
#include <iomanip>

using namespace std::placeholders;

namespace gui2 {

tinput_view::tinput_view(twidget& widget)
	: window_(nullptr)
	, widget_(static_cast<tpanel*>(&widget))
	, scroll_txt_(find_widget<tscroll_text_box>(widget_, "scroll_txt", false, true))
	, text_box_(scroll_txt_->tb())
	, use_tex_buf(false)
{
	widget_->set_can_invalidate_layout();
}

tinput_view::~tinput_view()
{
}

void tinput_view::set_label(const std::string& label)
{
	scroll_txt_->set_label(label);
}

const std::string& tinput_view::label() const
{
	return scroll_txt_->label();
}

ttext_box* tinput_view::get_keyboard_capture_text_box()
{
	VALIDATE(window_ != nullptr, null_str);
	twidget* focus = window_->keyboard_capture_widget();
	if (focus == nullptr || !focus->is_text_box()) {
		return nullptr;
	}
	return static_cast<ttext_box*>(focus);
}

void tinput_view::did_shown(twindow& window)
{
	window_ = &window;
}

void tinput_view::did_hidden()
{
	window_ = nullptr;
}

void tinput_view::clear_texture()
{
	tex_buf = nullptr;
}

} // namespace gui2

