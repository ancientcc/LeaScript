#define GETTEXT_DOMAIN "launcher-lib"

#include "map_unit.hpp"
#include "gettext.hpp"
#include "map_display.hpp"
#include "map_controller.hpp"
#include "gui/dialogs/map_scene.hpp"

#include <opencv2/imgproc.hpp>
#include <rose_ros/cartographer_utils.h>

map_unit::map_unit(map_controller& controller, map_display& disp, map_unit_map& units)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
	, points_(nullptr)
{
}

map_unit::~map_unit()
{
	if (points_ != nullptr) {
		free(points_);
		points_ = nullptr;
	}
}

void map_unit::app_draw_unit(const int xsrc, const int ysrc)
{
	int mode = controller_.cur_mode().mode;

	const cv::Mat& full = controller_.last_display_map();
	if (full.rows == 0 || full.cols == 0) {
		return;
	}
	bool full_is_gray = full.channels() == 1;

	const int unit_size = disp_.zoom() * UNIT_LOCS;
	const SDL_Point& map_offset = controller_.last_map_offset();

	const nav_msgs::MapMetaData& last_map = controller_.last_map();
	const int simple_per_meter = 1 / last_map.resolution;
	VALIDATE(simple_per_meter == 20, null_str);
	const int simple_per_meters = simple_per_meter * UNIT_LOCS;

	const int mapx = loc_.x * simple_per_meter;
	const int mapy = loc_.y * simple_per_meter;
	const int w = simple_per_meters;
	const int h = simple_per_meters;

	const SDL_Rect& raw_rose_map_rect{mapx, mapy, w, h};
	const float scale_ratio = 1.0 * unit_size / simple_per_meters;
/*
	if (loc_.x == 0 && loc_.y == 0) {
		const bool cartographer_verbose = true;
		if (cartographer_verbose) {
			char buf[384];
			SDL_snprintf(buf, sizeof(buf), "%s\nodom:%s\nodom_2_laser:%.3f");
			surface surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);
			disp_.drawing_buffer_add(display::LAYER_UNIT_MOVE_DEFAULT, loc_, surf,
				xsrc, ysrc,
				0, 0);
		}
	}
*/
	SDL_Rect roi2 = calculte_roi(mapx, mapy, w, h, full.cols, full.rows, map_offset);
	if (roi2.x == nposm) {
		return;
	}
	cv::Rect roi(roi2.x, roi2.y, roi2.w, roi2.h);

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
		}
	}
	if (draw_width == 0 || draw_height == 0) {
		// roi.width(1) * unit_size(144) / simple_per_meters(200)
		return;
	}

	roi.x -= map_offset.x;
	roi.y -= map_offset.y;
	
	cv::Size cvSize(draw_width, draw_height);
	cv::Mat argb;
	if (full_is_gray) {
		cv::Mat zoomed_gray;
		cv::resize(full(roi), zoomed_gray, cvSize);
		cv::cvtColor(zoomed_gray, argb, cv::COLOR_GRAY2BGRA);
	} else {
		cv::resize(full(roi), argb, cvSize);
	}

	surface surf(argb);
	disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, surf, x, y, 0, 0);

	// draw map origin
	const SDL_Point& map_origin = controller_.last_map_origin();
	int offsetx = map_offset.x + map_origin.x;
	int offsety = map_offset.y + map_origin.y;
	if (point_in_rect(offsetx, offsety, raw_rose_map_rect)) {
		surf = image::get_image("misc/map_origin.png");
		int deltax = (offsetx - mapx) * unit_size / simple_per_meters - surf->w / 2;
		int deltay = (offsety - mapy) * unit_size / simple_per_meters - surf->h / 2;
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, surf,
			xsrc + deltax, ysrc + deltay,
			0, 0);
	}

	// draw submap origin
	char buf[128];
	for (int at = 0; at < cartographer::rose_slot.submap_count(); at ++) {
		const cartographer::trose_slot::tsubmap& submap = cartographer::rose_slot.submap(at);
		int orgin_x = map_origin.x + submap.origin_x * simple_per_meter;
		int orgin_y = map_origin.y + (-1 * submap.origin_y) * simple_per_meter;

		offsetx = map_offset.x + orgin_x;
		offsety = map_offset.y + orgin_y;
		if (point_in_rect(offsetx, offsety, raw_rose_map_rect)) {
			SDL_snprintf(buf, sizeof(buf), "misc/submap%i_origin.png", at);
			surf = image::get_image(buf);
			int deltax = (offsetx - mapx) * unit_size / simple_per_meters - surf->w / 2;
			int deltay = (offsety - mapy) * unit_size / simple_per_meters - surf->h / 2;
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, surf,
				xsrc + deltax, ysrc + deltay,
				0, 0);
		}
	}

	// draw marker fix point
	if (controller_.cur_mode().mode == mode_position) {
		const std::map<std::string, tmap_marker>& marker = controller_.curmap().markers;
		const std::string& exclude_marker = controller_.exclude_marker_halo();
		for (std::map<std::string, tmap_marker>::const_iterator it = marker.begin(); it != marker.end(); ++ it) {
			const tmap_marker& marker = it->second;
			if (!exclude_marker.empty() && SDL_strcmp(exclude_marker.c_str(), marker.rsp.uuid) == 0) {
				continue;
			}
			int fix_pt_x = map_origin.x + marker.rsp.x * simple_per_meter;
			int fix_pt_y = map_origin.y + (-1 * marker.rsp.y) * simple_per_meter;

			offsetx = map_offset.x + fix_pt_x;
			offsety = map_offset.y + fix_pt_y;
			if (point_in_rect(offsetx, offsety, raw_rose_map_rect)) {
				surf = image::get_image("misc/translucent_red_circle.png");
				int deltax = (offsetx - mapx) * unit_size / simple_per_meters - surf->w / 2;
				int deltay = (offsety - mapy) * unit_size / simple_per_meters - surf->h / 2;
				disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, surf,
					xsrc + deltax, ysrc + deltay,
					0, 0);
			}
		}
	}

	// draw laser_origin
	{
		const SDL_Point& laser_origin = controller_.last_laser_origin();
		offsetx = map_offset.x + laser_origin.x;
		offsety = map_offset.y + laser_origin.y;
		if (laser_origin.x != nposm && point_in_rect(offsetx, offsety, raw_rose_map_rect)) {
			surf = controller_.laser_png_surf();
			const SDL_Point& rotate_point = controller_.laser_png_rotate_point();
			int deltax = (offsetx - mapx) * unit_size / simple_per_meters - rotate_point.x;
			int deltay = (offsety - mapy) * unit_size / simple_per_meters - rotate_point.y;
			// laser.png overlay map_origin.png always. use higher layer.
			disp_.drawing_buffer_add(display::LAYER_UNIT_MOVE_DEFAULT, loc_, surf,
				xsrc + deltax, ysrc + deltay,
				0, 0);
		}
	}

	// draw current_goal
	const SDL_Point& current_goal = controller_.current_goal_offset();
	offsetx = map_offset.x + current_goal.x;
	offsety = map_offset.y + current_goal.y;
	if (current_goal.x != nposm && point_in_rect(offsetx, offsety, SDL_Rect{mapx, mapy, w, h})) {
		surf = controller_.current_goal_surf();
		const SDL_Point& rotate_point = controller_.current_goal_png_rotate_point();
		int deltax = (offsetx - mapx) * unit_size / simple_per_meters - rotate_point.x;
		int deltay = (offsety - mapy) * unit_size / simple_per_meters - rotate_point.y;
		// laser.png overlay map_origin.png always. use higher layer.
		disp_.drawing_buffer_add(display::LAYER_UNIT_MOVE_DEFAULT, loc_, surf,
			xsrc + deltax, ysrc + deltay,
			0, 0);
	}

	if (controller_.with_laserscan()) {
		draw_unit_laser(xsrc, ysrc);
	}
	draw_unit_points(true, xsrc, ysrc, raw_rose_map_rect, scale_ratio);
	draw_unit_points(false, xsrc, ysrc, raw_rose_map_rect, scale_ratio);
}

void map_unit::draw_unit_laser(const int xsrc, const int ysrc)
{
	const cv::Mat& full = controller_.last_laser_mat();
	if (full.rows == 0 || full.cols == 0) {
		return;
	}

	const int unit_size = disp_.zoom() * UNIT_LOCS;
	const SDL_Point& map_offset = controller_.last_laser_offset();

	// const nav_msgs::MapMetaData& last_map = controller_.last_map();
	const int simple_per_meter = controller_.laser_pixels_per_meter();
	// VALIDATE(simple_per_meter == 20, null_str);
	const int simple_per_meters = simple_per_meter * UNIT_LOCS;

	const int mapx = loc_.x * simple_per_meter;
	const int mapy = loc_.y * simple_per_meter;
	const int w = simple_per_meters;
	const int h = simple_per_meters;

	SDL_Rect roi2 = calculte_roi(mapx, mapy, w, h, full.cols, full.rows, map_offset);
	if (roi2.x == nposm) {
		return;
	}
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
	if (draw_height == 0) {
		return;
	}
	if (roi.y - mapy != 0) {
		const int gap = ysrc % unit_size;
		const int not_draw = (y - gap) % unit_size;
		int noise = (unit_size - not_draw) - draw_height;
		y += noise;
	}
	if (draw_width == 0 || draw_height == 0) {
		// roi.width(1) * unit_size(144) / simple_per_meters(200)
		return;
	}

	roi.x -= map_offset.x;
	roi.y -= map_offset.y;
	
	cv::Size cvSize(draw_width, draw_height);
	cv::Mat zoomed_argb;
	cv::resize(full(roi), zoomed_argb, cvSize);

	surface surf(zoomed_argb);
	disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, surf, x, y, 0, 0);
}

void map_unit::draw_second_points(bool global, SDL_Point* points, int point_vsize)
{
	VALIDATE(point_vsize >= 2, null_str);

	const display::tdrawing_layer layer = display::LAYER_UNIT_MOVE_DEFAULT;
	const uint32_t color = global? 0xff00ff00: 0xff000cff;

	disp_.drawing_buffer_add(layer, loc_, image::BLITM_LINES, points, point_vsize, color);

	for (int at = 0; at < point_vsize; at ++) {
		points[point_vsize + at].x = points[at].x + 1;
		points[point_vsize + at].y = points[at].y + 1;
	}

	disp_.drawing_buffer_add(layer, loc_, image::BLITM_LINES, 
		points + point_vsize, point_vsize, color);
}

void map_unit::draw_unit_points(bool global, const int xsrc, const int ysrc, const SDL_Rect& raw_rose_map_rect, float scale_ratio)
{
	const SDL_Point& map_offset = controller_.last_map_offset();
	const SDL_Point* global_plan = global? controller_.global_plan(): controller_.local_plan();
	int global_plan_vsize = global? controller_.global_plan_vsize(): controller_.local_plan_vsize();

	if (global_plan_vsize == 0) {
		return;
	}

	if (!global) {
		VALIDATE(global_plan_vsize <= MAX_LOCAL_PLAN, null_str);
	}

	// for more clear, now is double.
	const int times_per_block = 2;
	if (points_ == nullptr) {
		int size = (MAX_GLOBAL_PLAN + MAX_LOCAL_PLAN) * times_per_block;
		points_ = (SDL_Point*)malloc(sizeof(SDL_Point) * size);
	}
	SDL_Point* points = global? points_: (points_ + MAX_GLOBAL_PLAN * times_per_block);
	int max_vsize = global? MAX_GLOBAL_PLAN: MAX_LOCAL_PLAN;

	int offsetx;
	int offsety;
	int point_vsize = 0;

	for (int at = 0; at < global_plan_vsize; at ++) {
		const SDL_Point& src = global_plan[at];
		offsetx = map_offset.x + src.x;
		offsety = map_offset.y + src.y;
		if (point_in_rect(offsetx, offsety, raw_rose_map_rect)) {
			int deltax = (offsetx - raw_rose_map_rect.x) * scale_ratio;
			int deltay = (offsety - raw_rose_map_rect.y) * scale_ratio;
			points[point_vsize].x = xsrc + deltax;
			points[point_vsize].y = ysrc + deltay;
			point_vsize ++;
			if (point_vsize == max_vsize) {
				break;
			}
		} else if (point_vsize > 0) {
			if (point_vsize >= 2) {
				draw_second_points(global, points, point_vsize);
			}
			point_vsize = 0;
		}
	}


	if (point_vsize >= 2) {
		draw_second_points(global, points, point_vsize);
	}
}