/* $Id$ */
/*
   Copyright (C) 2011 Sergey Popov <loonycyborg@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/float_panel.hpp"

#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "base_instance.hpp"

using namespace std::placeholders;

#include "font.hpp"


#define ESP		0.02 // 2cm

namespace gui2 {

//
// tfloat_panel
//
tfloat_panel::tfloat_panel(twindow& window)
	: window_(window)
	, float_track_(window.float_mini_track())
	, maybe_msg_id_(nposm)
	, bar_(40 * twidget::hdpi_scale, "misc/red_line.png", RSP_MIN_WALL_WIDTH + ESP, RSP_MAX_WALL_WIDTH - ESP)
	, require_calculate_bar_xy_(true)
	, downing_pt_(construct_null_coordinate())
	, downing_theta_(float_nposm)
	, downing_bar_theta_(float_nposm)
	, moving_bar_(nullptr)
	, widget_(*dynamic_cast<ttrack*>(window.float_mini_track().widget.get()))
{
	VALIDATE(!float_track_.is_visible(), null_str);

	VALIDATE(!widget_.is_timer_enable(), null_str);
	widget_.set_did_draw(std::bind(&tfloat_panel::did_draw_bar, this, _1, _2, _3));
	widget_.set_did_left_button_down(std::bind(&tfloat_panel::did_left_button_down, this, _1, _2));
	widget_.set_did_mouse_motion(std::bind(&tfloat_panel::did_mouse_motion_paper, this, _1, _2, _3, _4));
	widget_.set_did_mouse_leave(std::bind(&tfloat_panel::did_mouse_leave_paper, this, _1, _2, _3));
}

tfloat_panel::~tfloat_panel()
{
	float_track_.set_visible(false);

	// tprogress_::top_instance = nullptr;
}

void tfloat_panel::show(int mouse_x, int mouse_y, double line_width_meter, double theta, double gui_ratio)
{
	VALIDATE(gui_ratio > 0, null_str);
	VALIDATE(!float_track_.is_visible(), null_str);

	tbar& bar = bar_;
	bar.min_line_width_gui = bar.min_line_width_meter * gui_ratio;
	bar.max_line_width_gui = bar.max_line_width_meter * gui_ratio;
	bar.gui_ratio = gui_ratio;
	bar.line_width_gui = 0;
	bar.theta = 0;
	const int line_width_gui = bar.cut_line_width_gui(line_width_meter * gui_ratio);
	recreate_bar(bar, line_width_gui, bar.gui_ratio);

	VALIDATE(bar.rect.w > 0 && bar.rect.h > 0, null_str);

	bar.std_rect.x = mouse_x - bar.size;
	bar.std_rect.y = mouse_y - bar.std_rect.h / 2;
	bar.rect.x = bar.std_rect.x;
	bar.rect.y = bar.std_rect.y;

	if (theta != 0) {
		rotate_bar(bar, theta);
	}

	set_track_rect(bar.rect);

	window_.release_mouse_focus();

	float_track_.set_visible(true);
	// absolute_draw_float_widgets();
}

void tfloat_panel::hide()
{
	VALIDATE(float_track_.is_visible(), null_str);
	
	float_track_.set_visible(false);
}

bool tfloat_panel::is_visible() const
{
	return float_track_.is_visible();
}

void tfloat_panel::scroll(int dx, int dy)
{
	tbar& bar = bar_;

	bar.std_rect.x += dx;
	bar.std_rect.y += dy;
	bar.rect.x += dx;
	bar.rect.y += dy;

	set_track_rect(bar.rect);
}

void tfloat_panel::gui_ratio_changed(int mouse_x, int mouse_y, double gui_ratio)
{
	tbar& bar = bar_;
	double origin_gui_ratio = bar.gui_ratio;
	int origin_std_rect_x = bar.std_rect.x * gui_ratio / origin_gui_ratio;
	int origin_std_rect_y = bar.std_rect.y * gui_ratio / origin_gui_ratio;

	bar.min_line_width_gui = bar.min_line_width_meter * gui_ratio;
	bar.max_line_width_gui = bar.max_line_width_meter * gui_ratio;
	bar.gui_ratio = gui_ratio;
	bar.line_width_gui = 0;
	double theta = bar.theta;
	bar.theta = 0;

	int line_width_gui = bar.cut_line_width_gui(bar.line_width_meter * gui_ratio);
	recreate_bar(bar, line_width_gui, bar.gui_ratio);

	VALIDATE(bar.rect.w > 0 && bar.rect.h > 0, null_str);

	// bar.std_rect.x = mouse_x - bar.size;
	// bar.std_rect.y = mouse_y - bar.std_rect.h / 2;
	// bar.std_rect.x = origin_std_rect_x;
	// bar.std_rect.y = origin_std_rect_y;
	
	// bar.std_rect.x = mouse_x;
	// bar.std_rect.y = mouse_y;

	bar.std_rect.x = mouse_x - bar.size;
	bar.std_rect.y = mouse_y - bar.std_rect.h / 2;

	bar.rect.x = bar.std_rect.x;
	bar.rect.y = bar.std_rect.y;

	if (theta != 0) {
		rotate_bar(bar, theta);
	}

	set_track_rect(bar.rect);
}

SDL_Point tfloat_panel::anchor_mouse_xy() const
{
    const tbar& bar = bar_;
    int x = bar.std_rect.x + bar.size;
    int y = bar.std_rect.y + bar.std_rect.h / 2;

	// int x = bar.std_rect.x;
    // int y = bar.std_rect.y;

	return SDL_Point{x, y};
}

void tfloat_panel::recreate_bar(tbar& bar, int line_width_gui, double gui_ratio)
{
	VALIDATE(bar.size > 0 && bar.delta.x == 0 && bar.delta.y == 0, null_str);
	VALIDATE(!bar.line_png.empty(), null_str);
	VALIDATE(line_width_gui >= bar.min_line_width_gui, null_str);
	VALIDATE(line_width_gui <= bar.max_line_width_gui, null_str);
	VALIDATE(gui_ratio > 0, null_str);

	if (line_width_gui == bar.line_width_gui) {
		VALIDATE(false, null_str);
		return;
	}

	bar.line_width_gui = line_width_gui;
	bar.line_width_meter = line_width_gui / gui_ratio;

	int bg_width = bar.size * 2 + bar.line_width_gui;
	int bg_height = bar.size * 2 + 32 * twidget::hdpi_scale;

	// VALIDATE(!bar.btns.empty(), null_str);

	bar.std_surf = nullptr;
	bar.surf = nullptr;
	bar.tex = nullptr;
	
	// VALIDATE(!bar.btns.empty(), null_str);
	surface bg_surf = create_neutral_surface(bg_width, bg_height);
	VALIDATE(bg_surf.get(), null_str);

	const uint32_t bg_color = 0xffffc04d;
	SDL_Rect dst_rect{0, 0, bg_surf->w, bg_surf->h};
	// sdl_fill_rect(bg_surf, &dst_rect, bg_color);

	surface surf = image::get_image(bar.line_png);
	bar.line_height = surf->h;
	surf = scale_surface(surf, bg_width - 2 * bar.size, surf->h);
	dst_rect = ::create_rect(bar.size, (bg_height - surf->h) / 2, surf->w, surf->h);
	sdl_blit(surf, nullptr, bg_surf, &dst_rect);

	for (std::map<int, tdraw_item>::iterator it = draw_items_.begin(); it != draw_items_.end(); ++ it) {
		tdraw_item& item = it->second;
		surf = image::get_image(item.img);
		VALIDATE(surf.get(), null_str);

		if (item.msg_id == POST_MSG_CANCEL) {
			dst_rect = ::create_rect(0, 0, bar.size, bar.size);

		} else if (item.msg_id == POST_MSG_ROTATE) {
			dst_rect = ::create_rect(bg_width - bar.size, 0, bar.size, bar.size);

		} else if (item.msg_id == POST_MSG_OK) {
			dst_rect = ::create_rect(0, bg_height - bar.size, bar.size, bar.size);

		} else {
			VALIDATE(item.msg_id == POST_MSG_SCALE, null_str);
			dst_rect = ::create_rect(bg_width - bar.size, bg_height - bar.size, bar.size, bar.size);
		}
		item.std_rect = dst_rect;
		item.rect = item.std_rect;

		surf = scale_surface(surf, dst_rect.w, dst_rect.h);
		sdl_blit(surf, nullptr, bg_surf, &dst_rect);
	}


	bar.rect = ::create_rect(nposm, nposm, bg_surf->w, bg_surf->h);
	bar.std_rect = bar.rect;
	// VALIDATE(bar.rect.h == bar.size, null_str);
	bar.std_surf = bg_surf;
	bar.surf = clone_surface(bar.std_surf);
}

void tfloat_panel::rotate_bar(tbar& bar, double theta)
{
	surface surf = bar.std_surf;
	SDL_Point fixed_pt[5] = {bar.size, surf->h / 2};
	int at = 1;
	for (std::map<int, tdraw_item>::const_iterator it = draw_items_.begin(); it != draw_items_.end(); ++ it, at ++) {
		const tdraw_item& item = it->second;
		fixed_pt[at].x = item.std_rect.x + item.std_rect.w / 2;
		fixed_pt[at].y = item.std_rect.y + item.std_rect.h / 2;
	}
	VALIDATE(at == sizeof(fixed_pt) / sizeof(fixed_pt[0]), null_str);
	SDL_Point new_fixed_pt[5];
	memcpy(new_fixed_pt, fixed_pt, sizeof(fixed_pt));

	surface rotated_surf = rotate_surface(surf, RAD2DEG(theta), new_fixed_pt, at);
	bar.theta = theta;
	bar.surf = rotated_surf;
	bar.tex = nullptr;

	bar.rect.x = bar.std_rect.x - new_fixed_pt[0].x + fixed_pt[0].x;
	bar.rect.y = bar.std_rect.y - new_fixed_pt[0].y + fixed_pt[0].y;
	bar.rect.w = rotated_surf->w;
	bar.rect.h = rotated_surf->h;
	// moving_bar_->rect.x = last.x - moving_bar_->delta.x;
	// moving_bar_->rect.y = last.y - moving_bar_->delta.y;
	// set_track_rect(bar.rect);

	at = 1;
	for (std::map<int, tdraw_item>::iterator it = draw_items_.begin(); it != draw_items_.end(); ++ it, at ++) {
		tdraw_item& item = it->second;
		item.rect.x = new_fixed_pt[at].x - item.std_rect.w / 2;
		item.rect.y = new_fixed_pt[at].y - item.std_rect.h / 2;
	}
	VALIDATE(at == sizeof(fixed_pt) / sizeof(fixed_pt[0]), null_str);
}

void tfloat_panel::did_left_button_down(ttrack& widget, const tpoint& coordinate)
{
	VALIDATE(is_null_coordinate(downing_pt_), null_str);
	VALIDATE(moving_bar_ == nullptr, null_str);
	VALIDATE(maybe_msg_id_ == nposm, null_str);
	
	// downing_pt_.x = bar_.rect.x + bar_.size;
    // downing_pt_.y = bar_.rect.y + bar_.rect.h / 2;
	downing_pt_.x = bar_.std_rect.x + bar_.size;
    downing_pt_.y = bar_.std_rect.y + bar_.std_rect.h / 2;
	downing_theta_ = -1 * atan2(coordinate.y - downing_pt_.y, coordinate.x - downing_pt_.x);
	downing_bar_theta_ = bar_.theta;

	if (point_in_rect(coordinate.x, coordinate.y, bar_.rect)) {
		const int relative_x = coordinate.x - bar_.rect.x;
		const int relative_y = coordinate.y - bar_.rect.y;

		for (std::map<int, tdraw_item>::const_iterator it = draw_items_.begin(); it != draw_items_.end(); ++ it) {
			const tdraw_item& item = it->second;
			if (point_in_rect(relative_x, relative_y, item.rect)) {
				maybe_msg_id_ = item.msg_id;
				break;
			}
		}

		if (maybe_msg_id_ == nposm) {
			moving_bar_ = &bar_;
			moving_bar_->std_delta.x = coordinate.x - bar_.std_rect.x;
			moving_bar_->std_delta.y = coordinate.y - bar_.std_rect.y;
			moving_bar_->delta.x = coordinate.x - bar_.rect.x;
			moving_bar_->delta.y = coordinate.y - bar_.rect.y;
		}
	}
}

void tfloat_panel::did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last, const tpoint& delta)
{
	if (is_null_coordinate(first)) {
		return;
	}

	tbar& bar = bar_;
	if (maybe_msg_id_ != nposm) {
		VALIDATE(moving_bar_ == nullptr, null_str);

		if (maybe_msg_id_ == POST_MSG_ROTATE) {
			int deltax = last.x - downing_pt_.x;
			int deltay = last.y - downing_pt_.y;
			// if (deltax != 0) {
				// if deltax is 0 always, atan2 is (+/-)pi/2 always.
				double theta = atan2(deltay, deltax);

				// atan2's return value: >0(clockwise), <0(anticlockwise). 
				// It is the opposite of what mathematics teaches
				theta = -1 * theta;
				double ros_theta = theta - downing_theta_;
				// SDL_Log("did_draw_paper, downing_theta_: %.5f delta: (%i, %i) theta: %.5f, [ros]theta: %.5f", 
				//	RAD2DEG(downing_theta_), deltax, deltay, RAD2DEG(theta), RAD2DEG(ros_theta));

				rotate_bar(bar_, ros_theta + downing_bar_theta_);
				set_track_rect(bar.rect);

			// }

		} else if (maybe_msg_id_ == POST_MSG_SCALE) {
			int now_deltax = last.x - downing_pt_.x;
			int now_deltay = last.y - downing_pt_.y;

			int before_deltax = (last.x - delta.x) - downing_pt_.x;
			int before_deltay = (last.y - delta.y) - downing_pt_.y;

			int now_dist = now_deltax * now_deltax + now_deltay * now_deltay;
			int before_dist = before_deltax * before_deltax + before_deltay * before_deltay;

			int dist_delta = (now_deltax * now_deltax + now_deltay * now_deltay) - 
				(before_deltax * before_deltax + before_deltay * before_deltay);

			int increase = 0;
			if (dist_delta > 0) {
				increase = 1 * twidget::hdpi_scale;
			} else if (dist_delta < 0) {
				increase = -1 * twidget::hdpi_scale;
			}
			int new_line_width_gui = bar.line_width_gui + increase;
			new_line_width_gui = bar.cut_line_width_gui(new_line_width_gui);
			if (new_line_width_gui != bar.line_width_gui) {
				const SDL_Point bar_std_rect_xy{bar.std_rect.x, bar.std_rect.y};

				double origin_width = bar.line_width_meter;
				int origin_width_gui = bar.line_width_gui;

				recreate_bar(bar, new_line_width_gui, bar.gui_ratio);
				// SDL_Log("width_meter(%.3f --> %.3f) width_gui(%i --> %i) [%i / %.3f = %.3f]", 
				//	origin_width, bar.line_width_meter, origin_width_gui, bar.line_width_gui,
				//	bar.line_width_gui, bar.gui_ratio, bar.line_width_gui / bar.gui_ratio);
				bar.std_rect.x = bar_std_rect_xy.x;
				bar.std_rect.y = bar_std_rect_xy.y;

				double theta = bar.theta;
				bar.theta = 0;
				rotate_bar(bar_, theta);
				set_track_rect(bar.rect);
			}


		} else {
			const tdraw_item& item = draw_items_.find(maybe_msg_id_)->second;
			const int relative_x = last.x - bar_.rect.x;
			const int relative_y = last.y - bar_.rect.y;

			bool in = point_in_rect(relative_x, relative_y, item.rect);

			if (in) {
				SDL_Point delta{last.x - first.x, last.y - first.y};
				const int threshold = 3 * twidget::hdpi_scale; // moving_bar_->size / 4
				in = posix_abs(delta.x) <= threshold && posix_abs(delta.y) <= threshold;
			}
			if (!in) {
				maybe_msg_id_ = nposm;
			}
		}

	} else if (moving_bar_ != nullptr) {
		// VALIDATE(moving_bar_->delta.y == 0, null_str);

		// moving_bar_->delta.x = last.x - first.x;

		moving_bar_->std_rect.x = last.x - moving_bar_->std_delta.x;
		moving_bar_->std_rect.y = last.y - moving_bar_->std_delta.y;
		moving_bar_->rect.x = last.x - moving_bar_->delta.x;
		moving_bar_->rect.y = last.y - moving_bar_->delta.y;
		set_track_rect(moving_bar_->rect);
/*
		if (moving_btn_ != nposm) {
			const int threshold = 3 * twidget::hdpi_scale; // moving_bar_->size / 4
			moving_btn_ = posix_abs(moving_bar_->delta.x) <= threshold? moving_btn_: nposm;	
		}
		if (moving_btn_ != nposm) {
			if (last.y < moving_bar_->rect.y || last.y >= moving_bar_->rect.y + moving_bar_->rect.h) {
				moving_btn_ = nposm;
			}
		}
*/
	}
}

void tfloat_panel::did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	tpoint downing_pt = downing_pt_;
    set_null_coordinate(downing_pt_);

	const int maybe_msg_id = maybe_msg_id_;
	maybe_msg_id_ = nposm;
	if (is_null_coordinate(last)) {
		return;
	}

	const int widget_x = widget.get_x();
	int width = widget.get_width();

	if (maybe_msg_id != nposm) {
		VALIDATE(moving_bar_ == nullptr, null_str);

		const tdraw_item& item = draw_items_.find(maybe_msg_id)->second;
		did_item_clicked(item);

	} else if (moving_bar_ != nullptr) {
		// VALIDATE(moving_bar_->delta.y == 0, null_str);

		// last maybe is (MAGIC_COORDINATE_X, 0)
		// moving_bar_->rect.x = last.x - moving_bar_->delta.x;
		// moving_bar_->rect.y = last.y - moving_bar_->delta.y;
		// set_track_rect(moving_bar_->rect);

		moving_bar_->std_delta.x = 0;
		moving_bar_->std_delta.y = 0;
		moving_bar_->delta.x = 0;
		moving_bar_->delta.y = 0;

		if (moving_bar_->rect.x < widget_x) {
			moving_bar_->rect.x = widget_x;
		} else if (moving_bar_->rect.x > widget_x + width - moving_bar_->rect.w) {
			moving_bar_->rect.x = widget_x + width - moving_bar_->rect.w;
		}

		moving_bar_ = nullptr;
	}
}

void tfloat_panel::set_track_rect(const SDL_Rect& track_rect)
{
	tbar& bar = bar_;
	bar.rect.w = track_rect.w;
	bar.rect.h = track_rect.h;

	ttrack& track = widget_;
	float_track_.set_ref_widget(&window_, tpoint(track_rect.x, track_rect.y));
	track.set_layout_size(tpoint(track_rect.w, track_rect.h));

	// if set need_layut to true, old dirty background may appear after be invisible.
	// reference to "canvas = widget.get_canvas_tex();" in twindow::draw_float_widgets().
	float_track_.need_layout = true;
}

void tfloat_panel::did_draw_bar(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	if (window_.get_visible() != twidget::VISIBLE) {
		return;
	}

	VALIDATE(widget_rect.x == 0 && widget_rect.y == 0, null_str);
	SDL_Renderer* renderer = get_renderer();

	const bool verbose = true;
	if (verbose) {
		// render_rect_frame(renderer, widget_rect, 0x8000ff00, 4);
	}
	render_rect(renderer, widget_rect, 0x4000ff00); // 0x8000ff00

	tbar& bar = bar_;
	// if (require_calculate_bar_xy_) {
	{
		VALIDATE(bar.rect.w > 0 && bar.rect.w > 0, null_str);
		if (bar.surf.get() != nullptr) {
			bar.tex = SDL_CreateTextureFromSurface2(get_renderer(), bar.surf);
		}


		// require_calculate_bar_xy_ = false;
	}

	SDL_Rect dstrect;
	{
		const tbar& bar = bar_;

		dstrect = bar.rect;
		dstrect.x = 0;
		dstrect.y = 0;
		SDL_RenderCopy(renderer, bar.tex.get(), nullptr, &dstrect);

		if (verbose) {
			dstrect = bar.std_rect;
			dstrect.x = dstrect.x - bar.rect.x;
			dstrect.y = dstrect.y - bar.rect.y;
			render_rect_frame(renderer, dstrect, 0x80ff0000, 2);
		}
	}

	if (verbose) {
		// anchor point
		int anchor_x = bar.std_rect.x + bar.size - bar.rect.x;
		int anchor_y = bar.std_rect.y + bar.std_rect.h / 2 - bar.rect.y;
		const int radius = 10;
		SDL_Rect anchor_rect = ::create_rect(anchor_x - radius / 2, anchor_y - radius / 2, radius, radius);
		render_rect(renderer, anchor_rect, 0x800000ff);


		for (std::map<int, tdraw_item>::const_iterator it = draw_items_.begin(); it != draw_items_.end(); ++ it) {
			const tdraw_item& item = it->second;

			dstrect = item.rect;
			render_rect_frame(renderer, dstrect, 0x800000ff, 2);
		}
	}

}

} // namespace gui2

