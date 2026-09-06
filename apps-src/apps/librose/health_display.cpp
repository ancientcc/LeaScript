#define GETTEXT_DOMAIN "rose-lib"

#include "health_display.hpp"
#include "health_controller.hpp"
#include "health_unit_map.hpp"
#include "gui/dialogs/health_scene.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "halo.hpp"
// #include "formula_string_utils.hpp"


health_display::health_display(trhealth_scene_slot& scene_slot, health_controller& controller, health_unit_map& units, CVideo& video, const tmap& map, int initial_zoom)
	: display(game_config::tile_square, controller, video, &map, gui2::thealth_scene::NUM_REPORTS, initial_zoom)
	, scene_slot_(scene_slot)
	, controller_(controller)
	, units_(units)
{
	min_zoom_ = 64; // 64
	// max_zoom_ = 1024;
	max_zoom_ = min_zoom_; // don't support zoom+/-

	show_hover_over_ = false;
	// set_grid_style(display::grid_frame);
	// set_grid_style(display::grid_2layer);
}

health_display::~health_display()
{
}

gui2::tdialog* health_display::app_create_scene_dlg()
{
	return new gui2::thealth_scene(scene_slot_, controller_);
}

void health_display::app_post_initialize()
{
}

void health_display::draw_sidebar()
{
	// Fill in the terrain report
	if (map_->on_board_with_border(mouseoverHex_)) {
		refresh_report(gui2::thealth_scene::POSITION, reports::report(lexical_cast<std::string>(mouseoverHex_), null_str));
	}
	std::stringstream ss;
	ss << zoom_ << "(" << int(get_zoom_factor() * 100) << "%)";
	refresh_report(gui2::thealth_scene::ZOOM, reports::report(ss.str(), null_str));
}

void health_display::add_haloes()
{
	if (!controller_.allow_draw()) {
		return;
	}

	int zoom = hex_width();
	const SDL_Rect& _max_map_area = main_map_rect();
	const SDL_Rect& area = main_map_view_rect();
	// const int unit_h = max_unit_height(area.h);

	uint32_t mapped_col;
	image::tblit blit;
	surface surf;

	for (int at = 0; at < chart_count; at ++) {
		tbase_halos* chart = &posture_halos_;
		std::string title = _("title^posture chart");
		if (at == chart_routine) {
			chart = &routine_halos_;
			title = _("title^routine chart");
			continue;

		} else if (at == chart_workout) {
			chart = &workout_halos_;
			title = _("title^workout chart");
			continue;
		} else {
			continue;
		}
		int first_loc_y = VERT_LOCS_PER_CHART * at;

		if (chart->title == halo::NO_HALO) {
			// surf = font::get_rendered_text(title, 0, font::SIZE_DEFAULT, font::BIGMAP_COLOR);
			surf = font::get_rendered_text(title, 0, font::SIZE_DEFAULT, font::BLACK_COLOR);
			blit = image::tblit(surf, 0, 0, 0, 0);

			int x = (_max_map_area.w - surf->w) / 2;
			// int y = zoom * first_loc_y + (zoom - surf->h);
			int y = zoom * first_loc_y + (zoom - surf->h) / 2;
			chart->title = halo::add(x, y, false, blit);
		}

		int thickness = 3;
		bool use_line = false;
		if (chart->x_axis == halo::NO_HALO) {
			if (use_line) {
				mapped_col = 0x80ff0000; // always is gap, red.
				blit = image::tblit(image::BLITM_LINE, 0, 0, _max_map_area.w - 1, thickness - 1, mapped_col);
			} else {
				surf = image::get_image("misc/red_line.png");
				surf = scale_surface(surf, _max_map_area.w - 1, surf->h);
				blit = image::tblit(surf, 0, 0, 0, 0);
			}
			chart->x_axis = halo::add(0, zoom * (first_loc_y + VERT_LOCS_PER_CHART - 1), false, blit);
		}

		if (chart->y_axis == halo::NO_HALO) {
			if (use_line) {
				mapped_col = 0x8000ff00; // always is gap, red.
				blit = image::tblit(image::BLITM_LINE, 0, 0, thickness - 1, zoom * (VERT_LOCS_PER_CHART - 1) - 1, mapped_col);
			} else {
				surf = image::get_image("misc/red_line.png");
				surf = rotate_surface(surf, 90, nullptr, 0);
				surf = scale_surface(surf, surf->w, zoom * (VERT_LOCS_PER_CHART - 2));
				blit = image::tblit(surf, 0, 0, 0, 0);
			}
			chart->y_axis = halo::add(0, zoom * (first_loc_y + 1), false, blit);
		}
	}

/*
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
*/

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

void health_display::clear_haloes()
{
	posture_halos_.clear();
	routine_halos_.clear();
	workout_halos_.clear();
}