#define GETTEXT_DOMAIN "launcher-lib"

#include "mkscript_unit.hpp"
#include "gettext.hpp"
#include "mkscript_display.hpp"
#include "mkscript_controller.hpp"
#include "gui/dialogs/mkscript_scene.hpp"
#include "rose_sdl_utils.hpp"
#include <opencv2/imgproc.hpp>
#include "cairo2.hpp"

namespace visio {

void clamp_shape_size(int& width, int& height)
{
	// SDL_Size state_size{(int)(64 * 3.5 * gui2::twidget::hdpi_scale), (int)(64 * 2.5 * gui2::twidget::hdpi_scale)};

	const int DEF_SHAPE_HEIGHT = 128 * gui2::twidget::hdpi_scale;
	const int DEF_SHAPE_WIDTH = DEF_SHAPE_HEIGHT * 1.4;

	const SDL_Range size_range{128, 1024};

	if (width < size_range.min || width > size_range.max) {
		width = DEF_SHAPE_WIDTH;
	}
	if (height < size_range.min || height > size_range.max) {
		height = DEF_SHAPE_HEIGHT;
	}
}

int SHAPE_MARGIN = 8;	// when hdpi_scale is 1.25, it is 8.

void calculate_stretch_rects(const SDL_Rect& obj_rect, SDL_Rect* rects, SDL_Point* points)
{
	const int size = SHAPE_MARGIN * 2;

	// 4 vertices
    rects[0] = SDL_Rect{0, 0, size, size};   // ltop
	rects[1] = SDL_Rect{obj_rect.w - size, 0, size, size};     // rtop
    rects[2] = SDL_Rect{obj_rect.w - size, obj_rect.h - size, size, size};  // rbottom
    rects[3] = SDL_Rect{0, obj_rect.h - size, size, size};      // lbottom
    // Midpoints of the 4 sides
	rects[4] = SDL_Rect{(obj_rect.w - size) / 2, 0, size, size};       // top-mid
    rects[5] = SDL_Rect{obj_rect.w - size, (obj_rect.h - size) / 2, size, size};	// right-mid
	rects[6] = SDL_Rect{(obj_rect.w - size) / 2, obj_rect.h - size, size, size};	// bottom-mid
	rects[7] = SDL_Rect{0, (obj_rect.h - size) / 2, size, size};    // left-mid

	// 4 vertices
	points[0] = SDL_Point{SHAPE_MARGIN, SHAPE_MARGIN};	// ltop
	points[1] = SDL_Point{obj_rect.w - SHAPE_MARGIN, SHAPE_MARGIN};	// rtop
	points[2] = SDL_Point{obj_rect.w - SHAPE_MARGIN, obj_rect.h - SHAPE_MARGIN};	// rbottom
	points[3] = SDL_Point{SHAPE_MARGIN, obj_rect.h - SHAPE_MARGIN};	// lbottom
	// Midpoints of the 4 sides
	points[4] = SDL_Point{obj_rect.w / 2, SHAPE_MARGIN};	// top-mid
	points[5] = SDL_Point{obj_rect.w - SHAPE_MARGIN, obj_rect.h / 2};	// right-mid
	points[6] = SDL_Point{obj_rect.w / 2, obj_rect.h - SHAPE_MARGIN};	// bottom-mid
	points[7] = SDL_Point{SHAPE_MARGIN, obj_rect.h / 2};	// lbottom

	for (int at = 0; at < SHAPE_STRETCH_RECT_COUNT; at ++) {
		SDL_Rect& rect = rects[at];
		rect.x += obj_rect.x;
		rect.y += obj_rect.y;

		SDL_Point& point = points[at];
		point.x += obj_rect.x;
		point.y += obj_rect.y;
	}
}

surface tshape::get_surf(int width, int height) const
{
	clamp_shape_size(width, height);

	const SDL_Surface* sdl_surf = surf_.get();
	if (sdl_surf != nullptr && sdl_surf->w == width && sdl_surf->h == height) {
		return surf_;
	}

	SDL_Size bg_size{width, height};
	SDL_DColor canvas_color{1.0, 1, 1.0, 0.0};
    SDL_DColor fill_color{1.0, 1.0, 1.0, 1.0};
    double line_width = 1.0;
    double radius = 20;

	surf_ = cairo::draw_rounded_rectangle_for_shape(bg_size, SHAPE_MARGIN, false, canvas_color, &fill_color, &line_color, 
		line_width, radius);
	return surf_;
}

surface tshape::get_sel_surf(int width, int height) const
{
	clamp_shape_size(width, height);

	const SDL_Surface* sdl_sel_surf = sel_surf_.get();
	if (sdl_sel_surf != nullptr && sdl_sel_surf->w == width && sdl_sel_surf->h == height) {
		return sel_surf_;
	}

	SDL_Size bg_size{width, height};
	SDL_DColor canvas_color{1.0, 1, 1.0, 0.0};
    SDL_DColor fill_color{1.0, 1.0, 1.0, 1.0};
    double line_width = 1.0;
    double radius = 20;

	sel_surf_ = cairo::draw_rounded_rectangle_for_shape(bg_size, SHAPE_MARGIN, true, canvas_color, &fill_color, &line_color, 
		line_width, radius);
	return sel_surf_;
}

}

mkscript_unit::mkscript_unit(mkscript_controller& controller, mkscript_display& disp, mkscript_unit_map& units)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
{
}

void mkscript_unit::app_draw_unit(const int xsrc, const int ysrc)
{
	const int unit_size = disp_.zoom() * MKSCRIPT_UNIT_LOCS;

	int pixel_per_surf_loc = disp_.zoom();
	const int simple_per_meters = pixel_per_surf_loc * MKSCRIPT_UNIT_LOCS;
	const float scale_ratio = 1.0 * unit_size / simple_per_meters;

	// SDL_Rect this_rect{xsrc, ysrc, unit_size, unit_size};

	int item_count = nposm;
	const mkscript_controller::tdraw_item_C* items = controller_.draw_items(item_count);
	for (int at = 0; at < item_count; at ++) {
		const mkscript_controller::tdraw_item_C& item_C = items[at];

		SDL_Size item_size{item_C.rect.w, item_C.rect.h};
		VALIDATE(item_size.w > 0 && item_size.h > 0, null_str);
		// if (full.rows == 0 || full.cols == 0) {
		//	continue;
		// }

		const SDL_Point map_offset{item_C.rect.x, item_C.rect.y};

		const int mapx = loc_.x * pixel_per_surf_loc;
		const int mapy = loc_.y * pixel_per_surf_loc;
		const int w = simple_per_meters;
		const int h = simple_per_meters;

		// const SDL_Rect& raw_rose_map_rect{mapx, mapy, w, h};

		SDL_Rect roi2 = calculte_roi(mapx, mapy, w, h, item_size.w, item_size.h, map_offset);
		if (roi2.x == nposm) {
			continue;
		}
		cv::Rect roi(roi2.x, roi2.y, roi2.w, roi2.h);

		tsurface_2_mat_lock lock(item_C.obj->sel? item_C.obj->sel_surf: item_C.obj->surf);
		const cv::Mat& full = lock.mat;

		int x = xsrc + (roi.x - mapx) * unit_size / simple_per_meters;
		int y = ysrc + (roi.y - mapy) * unit_size / simple_per_meters;

		int draw_width = roi.width * unit_size / simple_per_meters;
		if (roi.x - mapx != 0) {
			VALIDATE(roi.x == map_offset.x, null_str);
			int roi_x2 = roi.x % unit_size;
			if (roi_x2 + roi.width >= w) {
				VALIDATE(roi_x2 + roi.width == w, null_str);

				const int gap = xsrc % unit_size;
				const int not_draw = (x - gap) % unit_size;
				int noise = (unit_size - not_draw) - draw_width;
				x += noise;

				if (noise != 0) {
					int ii = 0;
				}
			}
		}
		int draw_height = roi.height * unit_size / simple_per_meters;
		if (roi.y - mapy != 0) {
			VALIDATE(roi.y == map_offset.y, null_str);
			int roi_y2 = roi.y % unit_size;
			if (roi_y2 + roi.height >= h) {
				VALIDATE(roi_y2 + roi.height == h, null_str);

				const int gap = ysrc % unit_size;
				const int not_draw = (y - gap) % unit_size;
				int noise = (unit_size - not_draw) - draw_height;
				y += noise;

				if (noise != 0) {
					int ii = 0;
				}
			}
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