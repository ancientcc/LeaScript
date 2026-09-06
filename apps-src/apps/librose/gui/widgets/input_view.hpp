/* $Id: scroll_label.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_INPUT_VIEW_HPP_INCLUDED
#define GUI_WIDGETS_INPUT_VIEW_HPP_INCLUDED

#include "gui/widgets/control.hpp"

namespace gui2 {

class tpanel;
class tgrid;
class tscroll_text_box;
class ttext_box;

class tinput_view
{
public:
	explicit tinput_view(twidget& widget);
	~tinput_view();

	tpanel* panel() const { return widget_; }
	ttext_box* text_box() const { return text_box_; }

	void set_label(const std::string& text);
	const std::string& label() const;

	void did_shown(twindow& window);
	void did_hidden();
	void clear_texture();

private:
	ttext_box* get_keyboard_capture_text_box();

public:
	texture tex_buf;
	bool use_tex_buf;

private:
	tpanel* widget_;
	tscroll_text_box* scroll_txt_;
	ttext_box* text_box_;

	twindow* window_;
};

} // namespace gui2

#endif

