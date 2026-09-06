#define GETTEXT_DOMAIN "launcher-lib"

#include "map_display.hpp"
#include "map_controller.hpp"
#include "map_unit_map.hpp"
#include "gui/dialogs/map_scene.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "halo.hpp"
#include "formula_string_utils.hpp"

#include <opencv2/imgproc.hpp>

map_display::map_display(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tros_instance& ros_instance, tdcamera_driver& dcamera_driver, tcamera& camera, map_controller& controller, map_unit_map& units, CVideo& video, const tmap& map, int initial_zoom)
	: display(game_config::tile_square, controller, video, &map, gui2::tmap_scene::NUM_REPORTS, initial_zoom)
	, rdpd_mgr_(rdpd_mgr)
	, pble_(pble)
	, privacy_(privacy)
	, ros_instance_(ros_instance)
	, dcamera_driver_(dcamera_driver)
	, camera_(camera)
	, controller_(controller)
	, units_(units)
{
	min_zoom_ = 48;
	max_zoom_ = 144;

	show_hover_over_ = false;
	set_grid_style(display::grid_frame);
}

map_display::~map_display()
{
}

gui2::tdialog* map_display::app_create_scene_dlg()
{
	return new gui2::tmap_scene(rdpd_mgr_, pble_, privacy_, ros_instance_, dcamera_driver_, camera_, controller_);
}

void map_display::app_post_initialize()
{
}

void map_display::add_haloes()
{
	controller_.add_haloes();
}

void map_display::did_post_scroll(int dx, int dy)
{
	controller_.did_post_scroll(dx, dy);
}

void map_display::draw_sidebar()
{
	// Fill in the terrain report
	if (map_->on_board_with_border(mouseoverHex_)) {
		refresh_report(gui2::tmap_scene::POSITION, reports::report(lexical_cast<std::string>(mouseoverHex_), null_str));
	}
	std::stringstream ss;
	ss << zoom_ << "(" << int(get_zoom_factor() * 100) << "%)";
	refresh_report(gui2::tmap_scene::ZOOM, reports::report(ss.str(), null_str));
}

void map_display::app_pre_set_zoom(int new_zoom)
{
}

void map_display::app_post_set_zoom(int old_zoom)
{
	display::app_post_set_zoom(old_zoom);

	controller_.post_set_zoom();
}

void map_display::app_draw_minimap_units(surface& screen)
{
	const cv::Mat& src = controller_.last_src_gray();
	if (src.rows == 0 || src.cols == 0) {
		return;
	}

	const bool use_adaption = true;
	if (use_adaption) {
		const tpoint ratio_size = calculate_adaption_ratio_size(minimap_location_.w, minimap_location_.h, 
			src.cols, src.rows);
		SDL_Rect minimap_dst {minimap_location_.x + (minimap_location_.w - ratio_size.x) / 2, 
			minimap_location_.y + (minimap_location_.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};

		cv::Size cvSize(ratio_size.x, ratio_size.y);
		cv::Mat argb;
		cv::Mat zoomed_gray;
		cv::resize(src, zoomed_gray, cvSize);
		cv::cvtColor(zoomed_gray, argb, cv::COLOR_GRAY2BGRA);
		surface surf(argb);

		sdl_blit(surf, nullptr, screen, &minimap_dst);

	} else {
		cv::Size cvSize(minimap_location_.w, minimap_location_.h);
		cv::Mat argb;
		cv::Mat zoomed_gray;
		cv::resize(src, zoomed_gray, cvSize);
		cv::cvtColor(zoomed_gray, argb, cv::COLOR_GRAY2BGRA);
		surface surf(argb);

		sdl_blit(surf, nullptr, screen, &minimap_location_);
	}
}