/* $Id$ */
/*
   Copyright (C) 2011 by Sergey Popov <loonycyborg@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_FLOAT_PANEL_HPP_INCLUDED
#define GUI_DIALOGS_FLOAT_PANEL_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/control.hpp"

namespace gui2 {

class tbutton;

class tfloat_panel
{
public:
	enum {POST_MSG_MIN_MINI = 200, POST_MSG_ROTATE = POST_MSG_MIN_MINI, POST_MSG_SCALE, POST_MSG_OK, POST_MSG_CANCEL};

	tfloat_panel(twindow& window);
	virtual ~tfloat_panel();

	void show(int mouse_x, int mouse_y, double line_width_meter, double theta, double gui_ratio);
	void hide();
	bool is_visible() const;
	SDL_Point anchor_mouse_xy() const;
	void set_anchor_mainmap_xy(const SDL_Point& mainmap_xy) { bar_.anchor_mainmap_xy = mainmap_xy; }
	const SDL_Point& anchor_mainmap_xy() const { return bar_.anchor_mainmap_xy; }
	double gui_ratio() const { return bar_.gui_ratio; }

	void scroll(int dx, int dy);
	void gui_ratio_changed(int mouse_x, int mouse_y, double gui_ratio);

protected:
	void set_track_rect(const SDL_Rect& rect_in_screen);

	void did_left_button_down(ttrack& widget, const tpoint& coordinate);
	void did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last, const tpoint& delta);
	void did_mouse_leave_paper(ttrack& widget, const tpoint&, const tpoint& /*last_coordinate*/);

	void did_draw_bar(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);

	struct tdraw_item {
		tdraw_item(int _msg_id, const std::string& _img)
			: msg_id(_msg_id)
			, img(_img)
		{}

		int msg_id;
		std::string img;
		SDL_Rect std_rect;
		SDL_Rect rect;
		texture txt;
	};
	virtual void did_item_clicked(const tdraw_item& item) = 0;

	struct tbar
	{
		tbar(int size, const std::string& _line_png, double _min_line_width_meter, double _max_line_width_meter)
			: size(size)
			, line_png(_line_png)
			, min_line_width_meter(_min_line_width_meter)
			, max_line_width_meter(_max_line_width_meter)
			, min_line_width_gui(0)
			, max_line_width_gui(0)
			, gui_ratio(0)
			, line_width_meter(0)
			, line_width_gui(0)
			, line_height(0)
			, std_rect(empty_rect)
			, rect(empty_rect)
			, anchor_mainmap_xy(SDL_Point{0, 0})
			, std_delta(SDL_Point{0, 0})
			, delta(SDL_Point{0, 0})
			, theta(0)
		{}

		int cut_line_width_gui(int width) const
		{
			int result = width;
			if (result < min_line_width_gui) {
				result = min_line_width_gui;

			} else if (result > max_line_width_gui) {
				result = max_line_width_gui;
			}
			return result;
		}

		const int size;
		const std::string line_png;
		const double min_line_width_meter;
		const double max_line_width_meter;
		int min_line_width_gui;
		int max_line_width_gui;
		double gui_ratio;
		double line_width_meter;
		int line_width_gui;
		int line_height;
		SDL_Rect std_rect;
		SDL_Rect rect;
		SDL_Point anchor_mainmap_xy;
		SDL_Point std_delta;
		SDL_Point delta;
		double theta;
		surface std_surf;
		surface surf;
		texture tex;
	};
	void recreate_bar(tbar& bar, int line_width_gui, double gui_ratio);
	void rotate_bar(tbar& bar, double theta);

protected:
	twindow& window_;
	tfloat_widget& float_track_;
	
	SDL_Rect track_direct_rect_;

	std::map<int, tdraw_item> draw_items_;
	int maybe_msg_id_;


	tbar bar_;
	bool require_calculate_bar_xy_;

	tpoint downing_pt_;
	double downing_theta_;
	double downing_bar_theta_;
	tbar* moving_bar_;

private:
	ttrack& widget_;
};

} // namespace gui2

#endif

