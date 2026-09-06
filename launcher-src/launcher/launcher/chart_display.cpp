/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
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
#define GETTEXT_DOMAIN "launcher-lib"

#include "chart_display.hpp"
#include "chart_controller.hpp"
#include "chart_unit_map.hpp"
#include "gui/dialogs/chart_scene.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/report.hpp"
#include "game_config.hpp"
#include "filesystem.hpp"
#include "halo.hpp"
#include "formula_string_utils.hpp"


int chart_display::bottom_gap = 0; // 36

chart_display::chart_display(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, chart_controller& controller, chart_unit_map& units, CVideo& video, const tmap& map, int initial_zoom)
	: display(game_config::tile_square, controller, video, &map, gui2::tchart_scene::NUM_REPORTS, initial_zoom)
	, rdpd_mgr_(rdpd_mgr)
	, pble_(pble)
	, privacy_(privacy)
	, controller_(controller)
	, units_(units)
	, pos_max_level_halo_(halo::NO_HALO)
	, zero_level_halo_(halo::NO_HALO)
	, neg_max_level_halo_(halo::NO_HALO)
	, voice_threshold_halo_(halo::NO_HALO)
	, music_cursor_halo_(halo::NO_HALO)
	, stop_cursor_halo_(halo::NO_HALO)
	, current_halo_(halo::NO_HALO)
	, cursor_pos_(0)
	, cursor_time_(0)
	, stop_pos_(0)
{
	// bottom_gap = 36 * gui2::twidget::hdpi_scale;
	bottom_gap = 0 * gui2::twidget::hdpi_scale;

	// min_zoom_ = 64;
	min_zoom_ = initial_zoom;

	// max_zoom_ = 1024;
	max_zoom_ = min_zoom_; // don't support zoom+/-
	always_bottom_ = true;

	show_hover_over_ = false;

	set_max_empty_draws(5);
	// must use grid.
	// set_grid_style(display::grid_2layer);

	// set_draw_coordinates(true);
}

chart_display::~chart_display()
{
}

gui2::tdialog* chart_display::app_create_scene_dlg()
{
	return new gui2::tchart_scene(rdpd_mgr_, pble_, privacy_, *this, controller_);
}

void chart_display::app_post_initialize()
{
}

void chart_display::draw_hex(const map_location& loc)
{
	display::draw_hex(loc);
}

void chart_display::draw_sidebar()
{
	// Fill in the terrain report
	if (map_->on_board_with_border(mouseoverHex_)) {
		refresh_report(gui2::tchart_scene::POSITION, reports::report(lexical_cast<std::string>(mouseoverHex_), null_str));
	}
	std::stringstream ss;
	ss << zoom_ << "(" << int(get_zoom_factor() * 100) << "%)";
	refresh_report(gui2::tchart_scene::ZOOM, reports::report(ss.str(), null_str));
}

void chart_display::set_mouse_overlay(surface& image_fg)
{
	if (!image_fg) {
		set_mouseover_hex_overlay(NULL);
		return;
	}

	// Create a transparent surface of the right size.
	surface image = create_compatible_surface(image_fg, image_fg->w, image_fg->h);
	sdl_fill_rect(image, NULL, SDL_MapRGBA(image->format, 0, 0, 0, 0));

	// For efficiency the size of the tile is cached.
	// We assume all tiles are of the same size.
	// The zoom factor can change, so it's not cached.
	// NOTE: when zooming and not moving the mouse, there are glitches.
	// Since the optimal alpha factor is unknown, it has to be calculated
	// on the fly, and caching the surfaces makes no sense yet.
	static const Uint8 alpha = 196;
	static const int half_size = zoom_ / 2;
	static const int offset = 2;
	static const int new_size = half_size - 2;

	// Blit left side
	image_fg = scale_surface(image_fg, new_size, new_size);
	SDL_Rect rcDestLeft = create_rect(offset, offset, 0, 0);
	sdl_blit ( image_fg, NULL, image, &rcDestLeft );

	// Add the alpha factor and scale the image
	image = adjust_surface_alpha(image, alpha);

	// Set as mouseover
	set_mouseover_hex_overlay(image);
}

void chart_display::app_post_set_zoom(int last_zoom)
{
	cursor_pos_ = cursor_pos_ * zoom_ / last_zoom;
	controller_.fill_unit_pixel();
	clear_haloes();
}

static std::string miss_anim_err_str = "logic can only process map or canvas animation!";

surface chart_display::minimap_surface(int w, int h)
{
	surface minimap(create_neutral_surface(w, h));
	if (minimap == NULL) {
		return surface(NULL);
	}

	Uint32 color = 0xffacb3bc;
	sdl_fill_rect(minimap, NULL, color);
	return minimap;
}

void chart_display::app_draw_minimap_units(surface& screen)
{
	std::vector<SDL_Rect> rects;
	double xscaling = 1.0 * minimap_location_.w / (map_->w() * hex_width());
	double yscaling = 1.0 * minimap_location_.h / (map_->h() * hex_width());
	if (always_bottom_) {
		yscaling = 1.0 * minimap_location_.h / max_unit_height(main_map_view_rect().h);
	}

	SDL_Color col = font::GOOD_COLOR;
	const Uint32 mapped_col = SDL_MapRGB(screen->format, col.r, col.g, col.b);

	surface_lock locker(screen);
	for (chart_unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
		const chart_unit* u = dynamic_cast<const chart_unit*>(&*it);
		if (u->hidden()) {
			continue;
		}
		
		u->draw_minimap(screen, minimap_location_, xscaling, yscaling, mapped_col);
	}
}

void chart_display::set_cursor_pos(int pos, time_t time, bool auto_scroll)
{
	cursor_pos_ = pos;
	cursor_time_ = time;

	if (auto_scroll && pos) {
		const SDL_Rect& _map_area = main_map_view_rect();
		const int xstart = _map_area.x + get_scroll_pixel_x(pos);
		if (xstart < _map_area.x || xstart >= _map_area.x + _map_area.w) {
			scroll_to_xy(xstart, _map_area.y, display::WARP);
		}
	}
}

void chart_display::get_current_value(int pos, int& audio, int& motion)
{
	int at = pos / zoom_;
	const chart_unit* u = units_.find_unit(at);
	u->get_current_value(pos % zoom_, audio, motion);
}

void chart_display::set_stop_pos(int pos)
{
	stop_pos_ = pos;
}

void chart_display::did_show_nagtive_change()
{
	if (pos_max_level_halo_ != halo::NO_HALO) {
		halo::remove(pos_max_level_halo_);
		pos_max_level_halo_ = halo::NO_HALO;
	}

	if (zero_level_halo_ != halo::NO_HALO) {
		halo::remove(zero_level_halo_);
		zero_level_halo_ = halo::NO_HALO;
	}
	
	if (neg_max_level_halo_ != halo::NO_HALO) {
		halo::remove(neg_max_level_halo_);
		neg_max_level_halo_ = halo::NO_HALO;
	}
}

void chart_display::add_haloes()
{
	if (!controller_.allow_draw()) {
		return;
	}

	int zoom = hex_width();
	const SDL_Rect& _max_map_area = main_map_rect();
	const SDL_Rect& area = main_map_view_rect();
	const int unit_h = max_unit_height(area.h);

	uint32_t mapped_col;
	image::tblit blit;
	surface surf;

	bool show_nagtive = controller_.show_nagtive();
	int rect_h2 = show_nagtive? unit_h / 2: unit_h;

	int my_voice_threshold;
	int curr_voice_threshold = controller_.speech_voice_threshold_and_set(my_voice_threshold);

	const uint32_t range_threshold_color = 0x80ff0000;
	const uint32_t voice_threshold_color = 0x80ffffff;
	const uint32_t zero_color = 0x8000ff00;
	const uint32_t aux_scale_color = 0x80000000;

	if (pos_max_level_halo_ == halo::NO_HALO) {
		mapped_col = range_threshold_color; // always is gap, red.
		blit = image::tblit(image::BLITM_LINE, 0, 0, _max_map_area.w - 1, 0, mapped_col);
		pos_max_level_halo_ = halo::add(0, 0, false, blit);
	}

	if (zero_level_halo_ == halo::NO_HALO) {
		mapped_col = show_nagtive? zero_color: aux_scale_color;
		blit = image::tblit(image::BLITM_LINE, 0, 0, _max_map_area.w - 1, 0, mapped_col);
		zero_level_halo_ = halo::add(0, (_max_map_area.h - unit_h) + (unit_h - bottom_gap) / 2, false, blit);
	}

	if (neg_max_level_halo_ == halo::NO_HALO) {
		mapped_col = show_nagtive? range_threshold_color: zero_color; // 
		blit = image::tblit(image::BLITM_LINE, 0, 0, _max_map_area.w - 1, 0, mapped_col);
		neg_max_level_halo_ = halo::add(0, (_max_map_area.h - unit_h - bottom_gap) + unit_h - 1, false, blit);
	}

	if (curr_voice_threshold != my_voice_threshold) {
		if (voice_threshold_halo_ != halo::NO_HALO) {
			halo::remove(voice_threshold_halo_);
		}

		const int max_range = MAX_AUDIO_RANGE;
		// int voice_threshold_h = (unit_h / 2) * curr_voice_threshold / max_range;
		int voice_threshold_h = rect_h2 * curr_voice_threshold / max_range;

		mapped_col = 0x80ffffff; // 0x80ffffff
		blit = image::tblit(image::BLITM_LINE, 0, 0, _max_map_area.w - 1, 0, mapped_col);
		voice_threshold_halo_ = halo::add(0, (_max_map_area.h - unit_h - bottom_gap) + (rect_h2 - voice_threshold_h), false, blit);
	}

	// music cursor
	{
		if (music_cursor_halo_ != halo::NO_HALO) {
			halo::remove(music_cursor_halo_);
		}
		mapped_col = controller_.cursor_draging()? 0xffff0000: 0xffffffff;
		blit = image::tblit(image::BLITM_LINE, 0, 0, 0, area.h - 1, mapped_col);
		music_cursor_halo_ = halo::add(cursor_pos_, ypos_, false, blit);
	}

	// stop cursor
	if (stop_cursor_halo_ == halo::NO_HALO) {
		// mapped_col = 0x20ffffff;
		mapped_col = 0x20ff0000;
		blit = image::tblit(image::BLITM_LINE, 0, 0, 0, area.h - 1, mapped_col);
		stop_cursor_halo_ = halo::add(stop_pos_, ypos_, false, blit);
	}


/*
	{
		if (current_halo_ != halo::NO_HALO) {
			halo::remove(current_halo_);
		}
		if (true) {
			surf = font::get_rendered_text(format_time_hms(controller_.mdat_header().start + cursor_time_), 0, font::SIZE_SMALLER, font::BIGMAP_COLOR);
		} else {
			std::stringstream ss;
			ss << format_time_hms(controller_.mdat_header().start + cursor_time_);
			int audio, motion;
			get_current_value(cursor_pos_, audio, motion);
			ss << "\n" << audio << "/" << MAX_AUDIO_RANGE << ", " << motion << "/" << MDATA_THRESHOLD;
			surf = font::get_rendered_text(ss.str(), 0, font::SIZE_SMALLER, font::BLUE_COLOR);
		}
		blit = image::tblit(surf, 0, 0, 0, 0);

		int current_x = cursor_pos_;
		if (current_x + surf->w > _max_map_area.w) {
			current_x = _max_map_area.w - surf->w;
		}
		current_halo_ = halo::add(current_x, _max_map_area.h - surf->h + 2, false, blit);
	}
*/
}

void chart_display::clear_haloes()
{
	if (pos_max_level_halo_ != halo::NO_HALO) {
		halo::remove(pos_max_level_halo_);
		pos_max_level_halo_ = halo::NO_HALO;
	}
	if (zero_level_halo_ != halo::NO_HALO) {
		halo::remove(zero_level_halo_);
		zero_level_halo_ = halo::NO_HALO;
	}
	if (neg_max_level_halo_ != halo::NO_HALO) {
		halo::remove(neg_max_level_halo_);
		neg_max_level_halo_ = halo::NO_HALO;
	}
	if (voice_threshold_halo_ != halo::NO_HALO) {
		halo::remove(voice_threshold_halo_);
		voice_threshold_halo_ = halo::NO_HALO;
	}
	if (music_cursor_halo_ != halo::NO_HALO) {
		halo::remove(music_cursor_halo_);
		music_cursor_halo_ = halo::NO_HALO;
	}
	if (stop_cursor_halo_ != halo::NO_HALO) {
		halo::remove(stop_cursor_halo_);
		stop_cursor_halo_ = halo::NO_HALO;
	}
	if (current_halo_ != halo::NO_HALO) {
		halo::remove(current_halo_);
		current_halo_ = halo::NO_HALO;
	}
}