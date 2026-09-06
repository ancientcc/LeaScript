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

#include "chart_unit.hpp"
#include "gettext.hpp"
#include "chart_display.hpp"
#include "chart_controller.hpp"
#include "gui/dialogs/chart_scene.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "game_config.hpp"
#include "filesystem.hpp"


const int chart_unit::alert_radius = 32;
const int chart_unit::top_margin = 0;
const int chart_unit::initial_zoom = 64;
// const int chart_unit::initial_zoom = 32;
// const int chart_unit::initial_zoom = 16;

chart_unit::chart_unit(chart_controller& controller, chart_display& disp, chart_unit_map& units, const SDL_Rect& rect, int samples, int pixels_per_sample, int number, const uint8_t* chart_data)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
	, number_(number)
	, samples_()
	, pixels_per_sample_()
	, chart_data_size_(0)
	, chart_data_vsize_(0)
	, chart_data_(nullptr)
{
	VALIDATE(samples > 0, null_str);
	VALIDATE(pixels_per_sample == 1, null_str);

	rect_ = rect;

	samples_ = samples;
	pixels_per_sample_ = pixels_per_sample;
		
	int bytes_per_sample = 2;
	int audio_bytes = samples_ * bytes_per_sample;
	resize_data(audio_bytes);
	memcpy(chart_data_, chart_data, audio_bytes);
	chart_data_vsize_ = audio_bytes;

	// SDL_Log("chart_unit::chart_unit, number_: %i", number_);
}

chart_unit::~chart_unit()
{
	if (chart_data_ != nullptr) {
		free(chart_data_);
	}
}

void chart_unit::set_samples(int samples, int pixels_per_sample, const uint8_t* chart_data)
{
	VALIDATE(samples > 0, "want set sample = 0, use clear_samples()");

	samples_ = samples;
	pixels_per_sample_ = pixels_per_sample;

	int bytes_per_sample = 2;
	int audio_bytes = samples_ * bytes_per_sample;
	resize_data(audio_bytes);
	memcpy(chart_data_, chart_data, audio_bytes);
	chart_data_vsize_ = audio_bytes;
}

void chart_unit::clear_samples()
{
	samples_ = 0;
}

void chart_unit::resize_data(int size)
{
	size = posix_align_ceil(size, 4096);
	if (size > chart_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (chart_data_ != nullptr) {
			// if (chart_data_vsize_) {
			//	memcpy(tmp, chart_data_, chart_data_vsize_);
			// }
			free(chart_data_);
		}
		chart_data_ = tmp;
		chart_data_size_ = size;
	}
}

SDL_Point chart_unit::calculate_text_xy_offset(bool show_nagtive, const int xsrc, const int ysrc, const surface& text_surf, int volume)
{
	int rect_y2 = 0;
	int rect_h2 = rect_.h;

	if (show_nagtive) {
		rect_h2 = rect_.h / 2;
	}

	int max_range = MAX_AUDIO_RANGE;
	int h = rect_h2 * volume / max_range;

	int delta_x = 0;
	int delta_y = rect_y2 + rect_h2 - h;

	int text_h = text_surf->h;
	if (delta_y < text_h / 2) {
		// all text is below 'std-line'

	} else if (delta_y + text_h / 2 > rect_y2 + rect_h2) {
		// all text is above 'std-line'
		delta_y -= text_h;

	} else {
		// delta_y -= text_h / 2;
	}

	int off_x = xsrc + delta_x;
	int off_y = ysrc + delta_y;

	return SDL_Point{off_x, off_y};
}

void chart_unit::app_draw_unit(const int xsrc, const int ysrc)
{
	bool show_nagtive = controller_.show_nagtive();
	int zoom = disp_.zoom();
	surface surf = create_neutral_surface(rect_.w, rect_.h);

	if (samples_ != 0) {
		redraw_unit2(show_nagtive, surf, samples_, pixels_per_sample_, chart_data_, true, true);
	}

	disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, surf, xsrc, ysrc, 0, 0);

	// draw time remark.
	std::string str;
	if (samples_ != 0) {
		// if exist audio, use audio calculate time.
		const int sample_rate = CAF_SAMPLES_RATE;
		const int min_interval = initial_zoom * 2;
/*
		if (!(audio_start_samples_ % min_interval)) {
			int n = 0;
			do {
				int start_samples = (number_ + 1.0 * n * min_interval / zoom) * (1.0 * controller_.analysis().audio_samples() / MAX_USED_UNITS);
				str = format_time_hms(controller_.mdat_header().start + start_samples / sample_rate);

				surface text_surf = font::get_rendered_text(str, 0, font::SIZE_DEFAULT, font::BIGMAP_COLOR);
				SDL_Rect dst_rect = create_rect(n * min_interval, rect_.h - 60, 0, 0);

				disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, text_surf, xsrc + dst_rect.x, ysrc + dst_rect.y, 0, 0);
				// sdl_blit(text_surf, NULL, surf, &dst_rect);
			} while (++ n < zoom / min_interval);
		}
*/
	} else {
/*
		const int min_interval = initial_zoom * 2;
		int n = 0;
		double unit_time = controller_.music_duration() / controller_.original_width;
		double start_time = controller_.mdat_header().start + number_ * unit_time;
		do {
			str = format_time_hms(start_time + (unit_time * n * min_interval) / zoom);

			surface text_surf = font::get_rendered_text(str, 0, font::SIZE_DEFAULT, font::BLACK_COLOR);
			SDL_Rect dst_rect = create_rect(n * min_interval, rect_.h - 60, 0, 0);

			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, text_surf, xsrc + dst_rect.x, ysrc + dst_rect.y, 0, 0);
			// sdl_blit(text_surf, NULL, surf, &dst_rect);
		} while (++ n < zoom / min_interval);
*/
	}
	{
		// blit_integer_surface(number_, surf, 0, rect_.h - zoom + 20);
	}

	int ellipse_floating = 0;
	if (loc_ == controller_.selected_hex()) {
		disp_.drawing_buffer_add(display::LAYER_UNIT_BG, loc_,
			"misc/ellipse-top.png", image::SCALED_TO_ZOOM, xsrc, ysrc - ellipse_floating, 0, 0);
	
		disp_.drawing_buffer_add(display::LAYER_UNIT_FIRST, loc_,
			"misc/ellipse-bottom.png", image::SCALED_TO_ZOOM, xsrc, ysrc - ellipse_floating, 0, 0);
	}

	int voice_threshold = controller_.speech_voice_threshold();
	if (number_ == 0) {
		surface text;

		int values[] = {MAX_AUDIO_RANGE, voice_threshold, nposm};

		if (show_nagtive) {
			values[2] = -MAX_AUDIO_RANGE;
		} else {
			values[2] = 0;
		}

		for (int at = 0; at < sizeof(values) / sizeof(values[0]); at ++) {
			int val = values[at];
			const SDL_Color color = at != 1? font::NORMAL_COLOR: font::BIGMAP_COLOR;
			text = font::get_rendered_text(str_cast(val), 0, font::SIZE_SMALLEST, color);
			SDL_Point offset = calculate_text_xy_offset(show_nagtive, xsrc, ysrc, text, val);
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, text, offset.x, offset.y, 0, 0);
		}
	}
}

static int adjust_value(const int value, const int max_value, const int max_scale)
{
	int ret = value;

	// const int max1 = max_value / 3;
	const int max1 = max_value / 4;
	if (value >= max1) {
		return ret;
	}
	// if max_scale = 2, p1(0, 2), p2(max1, 0)
	// ===> (x - 0) / (max1 - 0) = (y - 2) / (0 - 2)  ===> y = -(2/max1)x + 2
	double ratio = -1.0 * max_scale / max1 * value + max_scale;
	ret = (int16_t)((ratio + 1) * value);
	if (ret >= max_value || ret < 0) {
		ret = max_value;
	}

	return ret;
}

void chart_unit::redraw_unit2(bool show_nagtive, surface& canvas, int samples, int pixels_per_sample, const uint8_t* chart_data, bool audio, bool half)
{
	Uint32 mapped_col = number_ & 1? 0xff00ff00: 0xffff0000;

	int rect_y2 = top_margin;
	int rect_h2 = rect_.h - chart_display::bottom_gap - top_margin;
	{
		// up audio, down mda.
		rect_y2 = (audio? 0: (rect_.h - chart_display::bottom_gap) / 2) + top_margin;
		rect_h2 = (rect_.h - chart_display::bottom_gap) / 2 - top_margin;

		if (!show_nagtive) {
			rect_y2 = 0;
			rect_h2 = rect_.h;

		} else {
			rect_y2 = 0;
			rect_h2 = rect_.h / 2;
		}
	}

	int value;
	const int max_range = MAX_AUDIO_RANGE;
	SDL_Rect r = empty_rect;
	if (rect_.w > samples) {
		// draw with rectangle
		int can_spread_pixels = rect_.w - samples * pixels_per_sample;
		VALIDATE(can_spread_pixels >= 0, null_str);
		int r_x = 0;

		for (int i = 0; i < samples; i ++) {
			mapped_col = (number_ * samples + i) & 1? 0x800000ff: 0x80ff0000;
			int w1 = pixels_per_sample;
			if (can_spread_pixels) {
				w1 ++;
				can_spread_pixels --;
			}
			r.x = r_x;
			r.w = w1;
			if (AUDIO_BYTES_PER_SAMPLE == 1) {
				value = chart_data[i];
			} else if (audio) {
                value = posix_mki16(chart_data[2 * i], chart_data[2 * i + 1]);
            } else {
                value = posix_mku16(chart_data[2 * i], chart_data[2 * i + 1]);
			}
			if (audio) {
				value = adjust_value(value, max_range, 3);
			} else if (value >= max_range) {
				value = max_range - 1;
			}
			r.h = rect_h2 * value / max_range;
			if (!r.h) {
				r.h = 1;
			}
			r.y = rect_y2 + rect_h2 - r.h;

			// disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_RECT, xsrc + r.x, ysrc + r.y, r.w, r.h, mapped_col);
			sdl_fill_rect(canvas, &r, mapped_col);

			r_x += w1;
		}
		VALIDATE(r_x == rect_.w, null_str);

	} else {
		// draw with line
		mapped_col = audio? 0xff00ff00: 0xffffff00;
		// surface_lock locker(canvas);
		int last_h = 0;

		if (number_ != 0) {
			chart_unit* last_unit = units_.find_unit(number_ - 1);

			const uint8_t* last_unit_chart_data = last_unit->chart_data_;

			int i = last_unit->rect_.w - 1;
			if (AUDIO_BYTES_PER_SAMPLE == 1) {
				value = last_unit_chart_data[i];
			} else {
				value = posix_mki16(last_unit_chart_data[2 * i], last_unit_chart_data[2 * i + 1]);
			}
			if (!show_nagtive && value < 0) {
				value *= -1;
			}

			if (value >= 0) {
				last_h = rect_h2 * value / max_range;
			} else {
				last_h = rect_h2 * value / max_range;;
			}
		}

		for (int i = 0; i < rect_.w; i ++) {
			int h;
			if (AUDIO_BYTES_PER_SAMPLE == 1) {
				value = chart_data[i];
			} else {
				value = posix_mki16(chart_data[2 * i], chart_data[2 * i + 1]);
			}
			const int origin = value;

			if (!show_nagtive && value < 0) {
				value *= -1;
			}

			// {
			//	value = 8000 - 20;
			//	int ii = 0;
			// }

			if (value >= 0) {
				h = rect_h2 * value / max_range;
			} else {
				h = rect_h2 * value / max_range;
			}

			// SDL_Log("number: %i, i: %i, value: %i(origin: %i), rect_.h: %i, rect_h2: %i, h: %i", 
			//	number_, i, value, origin, rect_.h, rect_h2, h);

			// if (!h) {
			//	h = 1;
			// }
			if (i) {
				uint32_t mapped_col2 = mapped_col;
				if (i & 1) {
					// mapped_col2 = 0xffffff00;
				};
				// disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_LINE, xsrc + i - 1, ysrc + rect_y2 + rect_h2 - last_h, xsrc + i, ysrc + rect_y2 + rect_h2 - h, mapped_col);
				draw_line(canvas, mapped_col2, i - 1, rect_y2 + rect_h2 - last_h, i, rect_y2 + rect_h2 - h);
			}
			last_h = h;
		}
	}
}

void chart_unit::get_current_value(int at, int& audio, int& motion) const
{
	if (rect_.w <= samples_) {
		audio = posix_mku16(chart_data_[2 * at], chart_data_[2 * at + 1]);
	} else {
		audio = -1;
	}
}

void chart_unit::insert_alert(int pixel, int value, bool caf)
{
	int rect_h2 = rect_.h - top_margin - chart_display::bottom_gap;
	rect_h2 = (rect_.h - chart_display::bottom_gap) / 2 - top_margin;

	const int max_range = MAX_AUDIO_RANGE;
	int h = rect_h2 * value / max_range;

	if (rect_h2 - h < alert_radius) {
		h = rect_h2 - alert_radius;

	} else if (h < alert_radius) {
		h = alert_radius;
	}
	alerts_.push_back(talert(pixel, rect_h2 - h));
}

const chart_unit::talert* chart_unit::point_in_alert(int x, int y) const
{
	const SDL_Rect& _map_area = disp_.main_map_view_rect();

	const int xsrc = _map_area.x + disp_.get_scroll_pixel_x(rect_.x);
	const int ysrc = _map_area.y + disp_.get_scroll_pixel_y(rect_.y) + top_margin;

	int at = 0;
	talert* hit = NULL;
	for (std::vector<talert>::const_iterator it = alerts_.begin(); it != alerts_.end(); ++ it, at ++) {
		const talert& alert = *it;
		const SDL_Rect rc = create_rect(xsrc + alert.x - alert_radius, ysrc + alert.y - alert_radius, 2 * alert_radius, 2 * alert_radius);
		if (point_in_rect(x, y, rc)) {
			return &alert;
		}
	}
	return NULL;
}

void chart_unit::draw_minimap(surface& screen, const SDL_Rect& minimap_location, double xscaling, double yscaling, const uint32_t mapped_col) const
{
	if (samples_) {
		draw_minimap2(screen, minimap_location, xscaling, yscaling, mapped_col, samples_, pixels_per_sample_, chart_data_, true);
	}
}

void chart_unit::draw_minimap2(surface& screen, const SDL_Rect& minimap_location, double xscaling, double yscaling, const uint32_t mapped_col, int samples, int pixels_per_sample, const uint8_t* chart_data, bool audio) const
{
	// must be aduio minimap.
	int u_x = (int)rect_.x * xscaling;
	int u_y = (int)rect_.y * yscaling;
	int u_w = (int)rect_.w * xscaling;
	int u_h = (int)rect_.h * yscaling;

	if (!u_w || u_h < 5) {
		return;
	}

	VALIDATE(samples > 0, null_str);
	VALIDATE(rect_.w >= u_w, null_str);

	{
		// up audio, down mda.
		const int minimap_top_margin = 1;
		u_y = (audio? 0: u_h / 2) + minimap_top_margin;
		u_h = u_h / 2 - minimap_top_margin;
	}

	int samples_per_unit, samples_per_pixel, used_units, pixels_per_unit, can_spread_samples;
	bool full_pixel = chart_controller::calculate_unit_width(1, samples, u_w, samples_per_unit, samples_per_pixel, used_units, pixels_per_unit, can_spread_samples);

	int point_per_unit = full_pixel? pixels_per_unit: samples_per_unit;
	int pixels_per_point = pixels_per_unit / point_per_unit;

    const int max_range = MAX_AUDIO_RANGE;
	const int sample_bytes = audio? AUDIO_BYTES_PER_SAMPLE: 2;

	const uint8_t* chart_data_ptr = chart_data;
	int h = 0, last_h = 0;
	for (int pixel = 0; pixel < point_per_unit; pixel ++) {
		if (pixel != samples) {
			int samples_per_pixel2 = samples_per_pixel;
			if (can_spread_samples) {
				samples_per_pixel2 ++;
				can_spread_samples --;
			}
			VALIDATE(samples_per_pixel2 > 0, "samples_per_pixel2 must > 0!");

			// Max value, not average value!
			int total = 0;
			for (int n = 0; n < samples_per_pixel2; n ++) {
				int data;
				if (sample_bytes == 1) {
					data = chart_data_ptr[n];

				} else if (audio) {
                    data = posix_mki16(chart_data_ptr[n * 2], chart_data_ptr[n * 2 + 1]);
                    
                } else {
					data = posix_mku16(chart_data_ptr[n * 2], chart_data_ptr[n * 2 + 1]);
				}
				if (data > total) {
					total = data;
				}

			}

			if (audio) {
				total = adjust_value(total, max_range, 4);
			} else if (total >= max_range) {
				total = max_range - 1;
			}
			h = u_h * total / max_range;

			if (!h) {
				h = 1;
			}
			for (int pixel2 = 0; pixel2 < pixels_per_point; pixel2 ++) {
				int x = minimap_location.x + u_x + pixel * pixels_per_point + pixel2;
				if (x >= minimap_location.w) {
					// i think, it is float effect.
					continue;
				}
				draw_line(screen, mapped_col, x, minimap_location.y + u_y + u_h - h, x, minimap_location.y + u_y + u_h - 1);
			}
			chart_data_ptr += sample_bytes * samples_per_pixel2;

			last_h = h;

		} else {
			h = last_h;
			VALIDATE(!full_pixel, "End earyly, must be in full_pixel == false!");
			for (int pixel2 = pixel * pixels_per_point; pixel2 < pixels_per_point * point_per_unit; pixel2 ++) {
				int x = minimap_location.x + u_x + pixel2;
				if (x >= minimap_location.w) {
					// i think, it is float effect.
					continue;
				}
				draw_line(screen, mapped_col, x, minimap_location.y + u_y + u_h - h, x, minimap_location.y + u_y + u_h - 1);
			}
			break;
		}
	}

	// use last_h fill rest pixels if exist.
	h = last_h;
	for (int pixel2 = pixels_per_point * point_per_unit; pixel2 < u_w; pixel2 ++) {
		int x = minimap_location.x + u_x + pixel2;
		if (x >= minimap_location.w) {
			// i think, it is float effect.
			continue;
		}
		draw_line(screen, mapped_col, x, minimap_location.y + u_y + u_h - h, x, minimap_location.y + u_y + u_h - 1);
	}
}

bool chart_unit::sort_compare(const base_unit& that_base) const
{
	const chart_unit* that = dynamic_cast<const chart_unit*>(&that_base);
	if (rect_.x != that->rect_.x || rect_.y != that->rect_.y) {
		return rect_.y < that->rect_.y || (rect_.y == that->rect_.y && rect_.x < that->rect_.x);
	}
	return chart_unit::sort_compare(that_base);
}