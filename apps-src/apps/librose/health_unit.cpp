#define GETTEXT_DOMAIN "rose-lib"

#include "health_unit.hpp"
#include "gettext.hpp"
#include "health_display.hpp"
#include "health_controller.hpp"
#include "gui/dialogs/health_scene.hpp"
#include <opencv2/imgproc.hpp>

health_unit::health_unit(health_controller& controller, health_display& disp, health_unit_map& units)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
{
}

void health_unit::app_draw_unit(const int xsrc, const int ysrc)
{
	const int unit_size = disp_.zoom() * HEAL_UNIT_LOCS;

	int pixel_per_surf_loc = disp_.zoom();
	const int simple_per_meters = pixel_per_surf_loc * HEAL_UNIT_LOCS;
	const float scale_ratio = 1.0 * unit_size / simple_per_meters;

	int item_count = nposm;
	const health_controller::tdraw_item* items = controller_.draw_items(item_count);
	for (int at = 0; at < item_count; at ++) {
		const health_controller::tdraw_item& item = items[at];

		SDL_Size mat_size = item.mat_size;
		if (mat_size.w == 0 || mat_size.h == 0) {
			if (item.mat != nullptr) {
				VALIDATE(item.mat->cols == mat_size.w && item.mat->rows == mat_size.h, null_str);
			}
			continue;
		}

		const SDL_Point& map_offset = item.offset;

		const int mapx = loc_.x * pixel_per_surf_loc;
		const int mapy = loc_.y * pixel_per_surf_loc;
		const int w = simple_per_meters;
		const int h = simple_per_meters;

		const SDL_Rect& raw_rose_map_rect{mapx, mapy, w, h};

		SDL_Rect roi2 = calculte_roi(mapx, mapy, w, h, mat_size.w, mat_size.h, map_offset);
		if (roi2.x == nposm) {
			continue;
		}

		if (item.mat == nullptr) {
			controller_.draw_workout_mat_from_cache(at);
			VALIDATE(item.mat != nullptr, null_str);
		}
		const cv::Mat& full = *item.mat;
		VALIDATE(full.cols == mat_size.w && full.rows == mat_size.h, null_str);

		cv::Rect roi(roi2.x, roi2.y, roi2.w, roi2.h);

		int x = xsrc + (roi.x - mapx) * unit_size / simple_per_meters;
		int y = ysrc + (roi.y - mapy) * unit_size / simple_per_meters;

		int draw_width = roi.width * unit_size / simple_per_meters;
		if (roi.x - mapx != 0) {
			const int gap = xsrc % unit_size;
			const int not_draw = (x - gap) % unit_size;
			int noise = (unit_size - not_draw) - draw_width;
			x += noise;
		}
		int draw_height = roi.height * unit_size / simple_per_meters;
		if (roi.y - mapy != 0) {
			const int gap = ysrc % unit_size;
			const int not_draw = (y - gap) % unit_size;
			int noise = (unit_size - not_draw) - draw_height;
			y += noise;
		}
		if (draw_width == 0 || draw_height == 0) {
			// roi.width(1) * unit_size(144) / simple_per_meters(200)
			continue;
		}

		roi.x -= map_offset.x;
		roi.y -= map_offset.y;

		cv::Size cvSize(draw_width, draw_height);
		cv::Mat argb;
		if (roi.width != cvSize.width || roi.height != cvSize.height) {
			// cv::Mat zoomed_gray;
			// cv::resize(full(roi), zoomed_gray, cvSize);
			// cv::cvtColor(zoomed_gray, argb, cv::COLOR_GRAY2BGRA);
			cv::resize(full(roi), argb, cvSize);

		} else {
			argb = full(roi).clone();
		}


		surface surf(argb);
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, surf, x, y, 0, 0);
	}
}