/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef CHART_DISPLAY_HPP_INCLUDED
#define CHART_DISPLAY_HPP_INCLUDED

#include "display.hpp"
#include "rdp_server_rose.h"
#include "pble2.hpp"

class chart_controller;
class chart_unit_map;
class base_unit;
class chart_unit;

namespace gui2 {
class treport;
class ttoggle_button;
}

class chart_display : public display
{
public:
	static int bottom_gap;

	chart_display(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, chart_controller& controller, chart_unit_map& units, CVideo& video, const tmap& map, int initial_zoom);
	~chart_display();

	bool in_theme() const { return true; }
	chart_controller& get_controller() { return controller_; }
	
	void set_cursor_pos(int pos, time_t time = 0, bool auto_scroll = true);
	int cursor_pos() const { return cursor_pos_; }
	time_t cursor_time() const { return cursor_time_; }
	void set_stop_pos(int pos);

	// int max_unit_height(int map_area_height) const { return map_area_height - bottom_gap; }
	int max_unit_height(int map_area_height) const { return map_area_height; }
	void did_show_nagtive_change();

protected:
	/**
	* The editor uses different rules for terrain highlighting (e.g. selections)
	*/
	// image::TYPE get_image_type(const map_location& loc);

	void draw_hex(const map_location& loc);

	// const SDL_Rect& get_clip_rect();
	void draw_sidebar();
	void app_post_set_zoom(int last_zoom);

	void set_mouse_overlay(surface& image_fg);

	void get_current_value(int pos, int& audio, int& motion);

private:
	gui2::tdialog* app_create_scene_dlg() override;
	void app_post_initialize() override;

	surface minimap_surface(int w, int h);
	void app_draw_minimap_units(surface& screen) override;
	void add_haloes() override;
	void clear_haloes();

private:
	net::trdpd_manager& rdpd_mgr_;
	tpble2& pble_;
	tprivacy& privacy_;
	chart_controller& controller_;
	chart_unit_map& units_;

	int pos_max_level_halo_;
	int zero_level_halo_;
	int neg_max_level_halo_;
	int voice_threshold_halo_;
	int music_cursor_halo_;
	int stop_cursor_halo_;
	int current_halo_;

	int cursor_pos_;
	time_t cursor_time_;
	int stop_pos_;
};

#endif
