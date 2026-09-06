/* $Id: dialog.cpp 50956 2011-08-30 19:41:22Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "charge.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"

#include <rose_ros/utils.hpp>


#include "rose_qr_code.hpp"
#include "rose_sdl_utils.hpp"
#include "rose_font.hpp"

#include <kdl/utilities/utility.h>
#include <angles/angles.h>
#include <base_local_planner/line_iterator.h>

using namespace std::placeholders;

namespace aplt {

//
// charge task
//
tkcharge::tkcharge()
	: aplt_(*curr_aplt)
	, b_api_(aplt::get_b_api())
	, r_api_(aplt::get_r_api())
	, move_robot_threshold_(3000) // 3 second
	, laser_y_diff_error_(0.01)
	, abs_min_90swing_theta_(DEG2RAD(70)) // 84.920, -79.460
	, next_vel_sample_index_(0)
	, next_finished_sample_index_(0)
	, max_line_samples_{MAX_VEL_LINE_SAMPLES, MAX_FINISHED_LINE_SAMPLES}
{
	memset(&check_2th_data_, 0, sizeof(check_2th_data_));
	clear();
}

tkcharge::~tkcharge()
{
	validate_nposm();
}

std::string tkcharge::app_start_task(ttask_vars& vars)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(b_api_.navigation_node_started(), null_str);

	const tapplet& aplt = *curr_aplt;

	VALIDATE(start_ticks_ == 0, null_str);
	VALIDATE(scan_frames_ == 0, null_str);
	validate_nposm();

	// VALIDATE(task_id_ == charge_task_id_charge, null_str);
	start_ticks_ = SDL_GetTicks();
	reset_line_samples();

	return null_str;
}

void tkcharge::app_task_finished(const tapplet::ttask& cfg_task)
{
	clear();
}

bool tkcharge::slice()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(start_ticks_ != 0, null_str);

	return finish_state_ == finish_finished;
}

int simple_2point_dist(int x1, int y1, int x2, int y2)
{
	int x_diff = x1 - x2;
	int y_diff = y1 - y2;
	return posix_abs(x_diff) + posix_abs(y_diff);
}

void line_from_2point(tkcharge::tline_C& line, int x1, int y1, int x2, int y2, const SDL_Point& origin)
{
/*
	// first calculate @theta, @height_m
	if (y1 != y2) {
		if (x1 != x2) {
			int deltax = x2 - x1;
			int deltay = y2 - y1;

			if (x1 > x2) {
				deltax = x1 - x2;
				deltay = y1 - y2;
			}
			double theta = atan2(deltay, deltax);

			// atan2's return value: >0(clockwise), <0(anticlockwise). 
			// It is the opposite of what mathematics teaches. but this is image-coord, first 'y' is 0.
			line.theta = -1 * theta;

			line.theta = angles::normalize_angle(line.theta);

		} else {
			line.theta = DEG2RAD(90);
		}

		int small_y = y1 < y2? y1: y2;
		int large_y = y1 < y2? y2: y1;
		const int height_pixel = large_y - small_y;
		line.height_m = height_pixel / PIXEL_2_METER;

	} else {
		line.theta = 0;
		line.height_m = 1 / PIXEL_2_METER;
	}
*/
	if (x1 < x2 || (x1 == x2 && y1 < y2)) {
		// x1 != x2: smaller x -> start
		// x1 == x2: smaller y -> start 
		line.start.x = x1;
		line.start.y = y1;
		line.end.x = x2;
		line.end.y = y2;

	} else {
		line.start.x = x2;
		line.start.y = y2;
		line.end.x = x1;
		line.end.y = y1;
	}


	line.start_dist_m = hypot(line.start.x - origin.x, line.start.y - origin.y) / PIXEL_2_METER;
	line.end_dist_m = hypot(line.end.x - origin.x, line.end.y - origin.y) / PIXEL_2_METER;
	line.center.x = line.start.x + (line.end.x - line.start.x + 1) / 2.0;
	line.center.y = line.start.y + (line.end.y - line.start.y + 1) / 2.0;
	line.width_m = hypot(line.start.x - line.end.x, line.start.y - line.end.y) / PIXEL_2_METER;
	if (KDL::Equal(line.width_m, 0)) {
		line.width_m = 1 / PIXEL_2_METER;
	}

	if (line.start.y != line.end.y) {
		if (line.start.x != line.end.x) {
			int deltax = line.end.x - line.start.x;
			int deltay = line.end.y - line.start.y;

			if (line.start.x > line.end.x) {
				deltax = line.start.x - line.end.x;
				deltay = line.start.y - line.end.y;
			}
			double theta = atan2(deltay, deltax);

			// atan2's return value: >0(clockwise), <0(anticlockwise). 
			// It is the opposite of what mathematics teaches. but this is image-coord, first 'y' is 0.
			line.theta = -1 * theta;

			line.theta = angles::normalize_angle(line.theta);

		} else {
			line.theta = DEG2RAD(90);
		}

		int small_y = line.start.y < line.end.y? line.start.y: line.end.y;
		int large_y = line.start.y < line.end.y? line.end.y: line.start.y;
		const int height_pixel = large_y - small_y;
		line.height_m = height_pixel / PIXEL_2_METER;

	} else {
		line.theta = 0;
		line.height_m = 1 / PIXEL_2_METER;
	}

	if (game_config::os == os_windows) {
		// Theoretically, 'line.theta' should be in [-90, 90], but for fear of accidents, more testing is needed.
		VALIDATE(fabs(RAD2DEG(line.theta)) <= 90.01, null_str);
	}
	
	line.y_diff_m = (line.center.y - origin.y) / PIXEL_2_METER;
}

struct tline2_C
{
	int at;
	tkcharge::tline_C line;
	SDL_FPoint top_center;
	SDL_FPoint bottom_center;
	SDL_Rect rect;
};

bool line2_is_valid_line(const tline2_C& line2)
{
	return !KDL::Equal(line2.line.width_m, 0);
}

bool line2_is_valid_rect(const tline2_C& line2)
{
	return line2.rect.w != 0;
}

void line2_calc_rect(tline2_C& result)
{
	VALIDATE(line2_is_valid_line(result), null_str);
	VALIDATE(!line2_is_valid_rect(result), null_str);

	tkcharge::tline_C& line = result.line;

	cv::Point2f center(line.center.x, line.center.y);
	cv::Size2f size(line.width_m * PIXEL_2_METER, 4);
	cv::RotatedRect rotatedRect(center, size, RAD2DEG(line.theta));

	// The order is bottomLeft, topLeft, topRight, bottomRight
	enum {pt_bl, pt_tl, pt_tr, pt_br};
	cv::Point2f pts[4];
	rotatedRect.points(pts);
	result.top_center = SDL_FPoint{pts[pt_tl].x + (pts[pt_tr].x - pts[pt_tl].x) / 2, pts[pt_tl].y + (pts[pt_tr].y - pts[pt_tl].y) / 2};
	result.bottom_center = SDL_FPoint{pts[pt_bl].x + (pts[pt_br].x - pts[pt_bl].x) / 2, pts[pt_bl].y + (pts[pt_br].y - pts[pt_bl].y) / 2};

	cv::Rect rect = rotatedRect.boundingRect();
	result.rect = SDL_Rect{rect.x, rect.y, rect.width, rect.height};
}

void line2_clear_rect(tline2_C& line2)
{
	memset(&line2.rect, 0, sizeof(line2.rect));
}

bool line2_overlap(const tline2_C& line2, const SDL_FPoint& top_center, const SDL_FPoint& bottom_center)
{
	// if any one is overlap, return true.
	if (point_in_rect(top_center.x, top_center.y, line2.rect)) {
		return true;
	}
	return point_in_rect(bottom_center.x, bottom_center.y, line2.rect);
}

bool tkcharge::line_a_is_better_b(const cv::Mat& dst, const tline2_C& a_line2, const tline2_C& b_line2, const SDL_Point& origin, bool verbose_png) const
{
	const tline_C& a_line = a_line2.line;
	const tline_C& b_line = b_line2.line;


	SDL_DPoint center;
	center.x = a_line.center.x + (b_line.center.x - a_line.center.x) / 2;
	center.y = a_line.center.y + (b_line.center.y - a_line.center.y) / 2;

	double theta = a_line.theta + (b_line.theta - a_line.theta) / 2;
	if (maybe_90swing(a_line.theta, b_line.theta)) {
		// a_theta_deg: -82.234, b_theta_deg: 82.092
		double diff = a_line.theta + b_line.theta;
		theta = M_PI / 2 - fabs(diff) / 2;
		if (diff > 0) {
			theta *= -1;
		}
		// ==>(deg/) diff: -0.141, theta: 89.929
		if (verbose_png) {
			SDL_Log("a.theta: %.3f, b.theta: %.3f ==> diff: %.3f, theta: %.3f", RAD2DEG(a_line.theta), RAD2DEG(b_line.theta), RAD2DEG(diff), RAD2DEG(theta));
		}
	}
	const double abs_theta_deg = fabs(RAD2DEG(theta));

	const int to_xx = (a_line.center.y < origin.y && b_line.center.y < origin.y && abs_theta_deg < 45) || 
		(a_line.center.y > origin.y && b_line.center.y > origin.y && abs_theta_deg < 45)? 0: 90;

	const double abs_to_xx_theta_deg = fabs(to_xx - abs_theta_deg);

	SDL_DPoint zeroed_a_line_center{a_line.center.x - center.x, (a_line.center.y - center.y) * -1};
	SDL_DPoint zeroed_b_line_center{b_line.center.x - center.x, (b_line.center.y - center.y) * -1};

	double theta2 = theta > 0? DEG2RAD(abs_to_xx_theta_deg): -1 * DEG2RAD(abs_to_xx_theta_deg);
	if (to_xx == 0) {
		theta2 *= -1;
	}
	SDL_DPoint zeroed_a_line_center2 = utils::transform_xy(0, 0, theta2, zeroed_a_line_center);
	SDL_DPoint zeroed_b_line_center2 = utils::transform_xy(0, 0, theta2, zeroed_b_line_center);

	SDL_DPoint a_line_center2{zeroed_a_line_center2.x + center.x, (-1 * zeroed_a_line_center2.y) + center.y};
	SDL_DPoint b_line_center2{zeroed_b_line_center2.x + center.x, (-1 * zeroed_b_line_center2.y) + center.y};

	if (game_config::os == os_windows && verbose_png) {
		cv::Mat dst2 = dst.clone();
		cv::Scalar color = cv::Scalar(0x80, 0x80, 0x80); // gray
		cv::line(dst2, cv::Point(a_line.center.x, a_line.center.y), cv::Point(b_line.center.x, b_line.center.y), color, 1, cv::LINE_AA);

		color = cv::Scalar(0x00, 0x00, 0xff); // red
		cv::line(dst2, cv::Point(a_line_center2.x, a_line_center2.y), cv::Point(b_line_center2.x, b_line_center2.y), color, 1, cv::LINE_AA);

		color = cv::Scalar(0x00, 0xff, 0x00); // green
		cv::Point pt1(center.x, center.y);
		cv::rectangle(dst2, pt1, pt1, color, 1);

		char buf[256];
		SDL_snprintf(buf, sizeof(buf), "%s/is_better_line_%i-%i.png", aplt_.preferences_dir.c_str(), a_line2.at, b_line2.at);
		imwrite(dst2, buf);

		SDL_DPoint tl{center.x - 100, center.y - 100};
		if (tl.x < 0) {
			tl.x = 0;
		}
		if (center.x + (center.x - tl.x) >= dst2.cols) {
			tl.x = center.x - (dst2.cols - 1 - center.x);
		}
		if (tl.y < 0) {
			tl.y = 0;
		}
		if (center.y + (center.y - tl.y) >= dst2.rows) {
			tl.y = center.y - (dst2.rows - 1 - center.y);
		}

		cv::Rect roi(tl.x, tl.y, 2 * (center.x - tl.x), 2 * (center.y - tl.y));
		cv::Mat dst3;
		cv::cvtColor(dst2(roi), dst3, cv::COLOR_BGR2RGBA);
		dst3 = rotate_mat(dst3, RAD2DEG(theta2), nullptr, 0);
		SDL_snprintf(buf, sizeof(buf), "%s/is_better_line_%i-%i-roi.png", aplt_.preferences_dir.c_str(), a_line2.at, b_line2.at);
		imwrite(dst3, buf);
	}

	bool better = origin.x > center.x? a_line_center2.x > b_line_center2.x: a_line_center2.x < b_line_center2.x;
	if (to_xx == 0) {
		better = origin.y > center.y? a_line_center2.y > b_line_center2.y: a_line_center2.y < b_line_center2.y;
	}
	if (verbose_png) {
		char buf[256];
		SDL_snprintf(buf, sizeof(buf), "%u to(%i) origin(%i, %i) center(%.3f, %.3f), theta: %.3f(abs_to_90: %.3f)), theta2: %.3f", SDL_GetTicks(), 
			to_xx, origin.x, origin.y, center.x, center.y, RAD2DEG(theta), abs_to_xx_theta_deg, RAD2DEG(theta2));

		if (to_xx == 0) {
			SDL_Log("%s ==> a.y(%i): %.3f, b.y(%i): %.3f, is_better: %s",
				buf, a_line2.at, a_line_center2.y, b_line2.at, b_line_center2.y, better? "true": "false");

		} else {
			SDL_Log("%s ==> a.x(%i): %.3f, b.x(%i): %.3f, is_better: %s",
				buf, a_line2.at, a_line_center2.x, b_line2.at, b_line_center2.x, better? "true": "false");
		}
	}

	return better;
}

/*
void LaserScan_write(const sensor_msgs::LaserScan& scan_msg, const std::string& path)
{
	VALIDATE(!path.empty(), null_str);

	ros::SerializedMessage serialized = ros::serialization::serializeMessage<sensor_msgs::LaserScan>(scan_msg);
	int msg_len = serialized.num_bytes - (serialized.message_start - serialized.buf.get());
	const uint8_t* serialized_msg_data = serialized.message_start;

	std::string filename = path;
	if (!SDL_IsFromRootPath(path.c_str())) {
		filename = game_config::preferences_dir + "/" + path;
	}

	write_file(filename, (const char*)serialized_msg_data, msg_len);
}

bool LaserScan_read(const std::string& path, sensor_msgs::LaserScan& scan_msg)
{
	std::string filename = path;
	if (!SDL_IsFromRootPath(path.c_str())) {
		filename = game_config::preferences_dir + "/" + path;
	}

	scan_msg.header.frame_id.clear();

	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	if (!file.valid()) {
		return false;
	}
	int fsize = posix_fsize(file.fp);
	if (fsize == 0) {
		return false;
	}

	posix_fseek(file.fp, 0);

	uint8_t* p = new uint8_t[fsize];
	posix_fread(file.fp, p, fsize);
	boost::shared_array<uint8_t> buf(p);
	ros::SerializedMessage serialized(buf, fsize);

	ros::serialization::deserializeMessage<sensor_msgs::LaserScan>(serialized, scan_msg);
	// if (scan_msg.header.frame_id != "laser") {
	//	return false;
	// }
	return true;
}
*/
bool tkcharge::find_line(const sensor_msgs::LaserScan& msg, const cv::Mat& laser_scan_raw, const cv::Mat& laser_scan, const cv::Mat& dilate, const tapplet& aplt, const SDL_Point& origin, bool verbose_png, tline_C& result)
{
	// SDL_Point origin = origin2;
	// origin.y = origin2.y + 2;

	VALIDATE(laser_scan.cols == dilate.cols && laser_scan.rows == dilate.rows, null_str);
	VALIDATE(laser_scan.channels() == 1 && dilate.channels() == 1, null_str);

	result.width_m = float_nposm;

	// lines = cv2.HoughLinesP(edges, 1, np.pi / 180, threshold=100, minLineLength=100, maxLineGap=10)
	// cv::Mat lines;
	std::vector<cv::Vec4f> plines;
	uint32_t start_ticks = SDL_GetTicks();
	// cv::HoughLinesP(dilate, plines, 1, CV_PI / 180, 60, 0, 10); // <- chare_width(0.27)

	cv::HoughLinesP(dilate, plines, 1, CV_PI / 180, 25, 0, 10); // 40/35/(30/25) 30/20
	// cv::HoughLinesP(dilate, plines, 2, CV_PI / 180, 25, 0, 10); // 40/35/(30/25) 30/20
	int ms = SDL_GetTicks() - start_ticks;

	const int line_count = plines.size();
	if (line_count == 0) {
		return false;
	}

	const int cells = laser_scan.cols * laser_scan.rows;
	utils::resize_uint8data(check_2th_data_, cells, 0);

	cv::Mat check_2th_mat(laser_scan.rows, laser_scan.cols, CV_8UC1, check_2th_data_.ptr);

	tline2_C* line2s = (tline2_C*)malloc(line_count * sizeof(tline2_C));
	memset(line2s, 0, line_count * sizeof(tline2_C));

	cv::Mat dst;
	cv::cvtColor(dilate, dst, cv::COLOR_GRAY2BGR);

	for (int at = 0; at < line_count; at ++) {
		cv::Scalar color(rand() % 128, rand() % 128, rand() % 128);
        cv::Vec4f hline = plines[at];
		if (verbose_png || game_config::os == os_windows) {
			cv::line(dst, cv::Point(hline[0], hline[1]), cv::Point(hline[2], hline[3]), color, 3, cv::LINE_AA);

			// red
			cv::Point pt1(hline[0], hline[1]);
			cv::rectangle(dst, pt1, pt1, cv::Scalar(0, 0, 0xff));
			// red
			cv::Point pt2(hline[2], hline[3]);
			cv::rectangle(dst, pt2, pt2, cv::Scalar(0, 0, 0xff));
		}
		
		line2s[at].at = at;
		tline_C& line = line2s[at].line;
		line_from_2point(line, (int)hline[0], (int)hline[1], (int)hline[2], (int)hline[3], origin);
    }

	// chare_width   laser_y_diff_error_
	// 0.27          0.01                cv::HoughLinesP(dilate, plines, 1, CV_PI / 180, 60, 0, 10)
	// 0.175
	// 0.13
	// const double charge_width = 0.175;
	const double charge_width = charge_width_;
	const SDL_DRange bonus{0.04, 0.05};
	const double dist_threshold = 0.75; // 1.0/0.80

	for (int index = 0; index < line_count; index ++) {
		tline2_C& this_line2 = line2s[index];
		VALIDATE(line2_is_valid_line(this_line2), null_str);

		const tline_C& line = this_line2.line;

		if (line.width_m < charge_width - bonus.min) {
			// SDL_Log("#%i [%i/%i](> min)charge_width: %.5f, line.width_m: %.5f", scan_frames_, index, line_count, charge_width, line.width_m);
			continue;
		}

		line2_calc_rect(this_line2);

		int better_at = nposm;
		const tline_C* better_line = nullptr; 
		for (int at = 0; at < line_count; at ++) {
			tline2_C& history = line2s[at];
			if (at >= index) {
				if (at > index) {
					VALIDATE(!line2_is_valid_rect(history), null_str);
				}
				continue;
			}
			VALIDATE(line2_is_valid_rect(this_line2), null_str);
			if (!line2_is_valid_rect(history)) {
				continue;
			}
			if (line2_overlap(history, this_line2.top_center, this_line2.bottom_center) ||
				line2_overlap(this_line2, history.top_center, history.bottom_center)) {
				if (line.width_m > history.line.width_m) {
					// this is better than history, clear history.
					// SDL_Log("#%i [%i/%i]line.width_m(%.5f), it is better than history line(index:%i).width_m(%.5f), discard history", scan_frames_, index, line_count, line.width_m, at, history.line.width_m);
					line2_clear_rect(history);

				} else {
					// history is better than this, this should keep not valid.
					better_at = at;
					better_line = &history.line;
				}

			} else {
				// SDL_Log("#%i [%i/%i] top(%i, %i), bottom(%i, %i)->line(index:%i, rect[%i, %i, %i, %i]) not overlap", scan_frames_, index, line_count, 
				//	(int)this_line2.top_center.x, (int)this_line2.top_center.y, (int)this_line2.bottom_center.x, (int)this_line2.bottom_center.y,
				//	at, history.rect.x, history.rect.y, history.rect.w, history.rect.h);
			}
		}
		if (better_at != nposm) {
			VALIDATE(better_line != nullptr, null_str);
			// SDL_Log("#%i [%i/%i]line.width_m(%.5f) has better line(index:%i).width_m(%.5f), discard it", scan_frames_, index, line_count, line.width_m, better_at, better_line->width_m);
			line2_clear_rect(this_line2);
		}
	}

	if (verbose_png) {
		const int dst_channels = dst.channels();
		const uint8_t* laser_scan_data = laser_scan.ptr<uint8_t>(0);

		uint8_t* dst_data = dst.ptr<uint8_t>(0);
		for (int y = 0; y < laser_scan.rows; y ++) {
			for (int x = 0; x < laser_scan.cols; x ++) {
				int index = x + y * laser_scan.cols;
				if (laser_scan_data[index] != 0) {
					int dst_index = dst_channels * index;
					// blue
					dst_data[dst_index] = 0xff;
					dst_data[dst_index + 1] = 0x00;
					dst_data[dst_index + 2] = 0x00;
				}
			}
		}
	}

	const tline2_C* best_line2 = nullptr;
	double abs_best_theta = 0;
	for (int at = 0; at < line_count; at ++) {
		const tline2_C& line2 = line2s[at];
		if (!line2_is_valid_rect(line2)) {
			continue;
		}
		const tline_C& line = line2.line;
/*
		SDL_Log("#%i [%i/%i]ms: %i, charge_width: %.3f, line.width_m: %.5f, line.y_diff_m: %.5f, start_dist: %.5f(%i, %i), end_dist: %.5f(%i, %i), theta: %.3f",
			scan_frames_, at, line_count, ms, charge_width, line.width_m, line.y_diff_m, 
			line.start_dist_m, line.start.x, line.start.y, 
			line.end_dist_m, line.end.x, line.end.y, RAD2DEG(line.theta));
*/
		if (verbose_png || game_config::os == os_windows) {
			const SDL_Rect& rect = line2.rect;
			cv::Scalar color = cv::Scalar(0x80, 0x80, 0x80);
			cv::rectangle(dst, cv::Rect(rect.x, rect.y, rect.w, rect.h), color, 1);

			surface bg_surf(dst);

			const int font_size = 9; // 7
			SDL_Color font_color = uint32_to_color(0xffffffff);
			SDL_Rect dstrect;

			surface text_surf = font::rose_get_rendered_text(str_cast(at), 0, font_size, font_color);

			dstrect.x = line.center.x - text_surf->w / 2;
			dstrect.y = line.center.y - text_surf->h / 2;
			dstrect.w = text_surf->w;
			dstrect.h = text_surf->h;
			SDL_BlitSurface(text_surf.get(), nullptr, bg_surf.get(), &dstrect);
		}

		if (line.width_m > charge_width + bonus.max) {
			// SDL_Log("#%i [%i/%i](> max)charge_width: %.5f, line.width_m: %.5f", scan_frames_, at, line_count, charge_width, line.width_m);
			continue;
		}

		if (line.start_dist_m > dist_threshold) {
			// SDL_Log("#%i [%i/%i]line.start_dist(%.5f) > dist_threshold(%.5f), should discard", scan_frames_, at, line_count, line.start_dist_m, dist_threshold);
			continue;
		}

		if (line.end_dist_m > dist_threshold) {
			// SDL_Log("#%i [%i/%i]line.end_dist(%.5f) > dist_threshold(%.5f), should discard", scan_frames_, at, line_count, line.end_dist_m, dist_threshold);
			continue;
		}

/*
		SDL_Log("#%i [%i/%i]ms: %i, charge_width: %.3f, line.width_m: %.5f, line.y_diff_m: %.5f, start_dist: %.5f(%i, %i), end_dist: %.5f(%i, %i), theta: %.3f",
			scan_frames_, at, line_count, ms, charge_width, line.width_m, line.y_diff_m, 
			line.start_dist_m, line.start.x, line.start.y, 
			line.end_dist_m, line.end.x, line.end.y, RAD2DEG(line.theta));
*/
		if (best_line2 != nullptr) {
			double abs_theta = fabs(line.theta);

			double ang_diff = angles::shortest_angular_distance(best_line2->line.theta, line.theta);
			if (maybe_90swing(best_line2->line.theta, line.theta)) {
				ang_diff = M_PI - abs_best_theta - abs_theta;
			}
			double wall_threshold_deg = 25.0; // has seen 15.67
			double abs_ang_diff_deg = fabs(RAD2DEG(ang_diff));

			double dist = hypot(line.center.x - best_line2->line.center.x, line.center.y - best_line2->line.center.y) / PIXEL_2_METER;
			// {a, b, c} b is charge, but a and b is line2 or best_line2.
			double dist_threshold = (charge_width + bonus.max) * 2 + 0.10;
			if (abs_ang_diff_deg < wall_threshold_deg && dist < dist_threshold) {
				const double abs_best_theta_deg = RAD2DEG(abs_best_theta);
				const bool verbose_png2 = verbose_png && false;
				if (!line_a_is_better_b(dst, line2, *best_line2, origin, verbose_png2)) {
					// SDL_Log("#%i [%i/%i]abs_ang_diff_deg(%.3f) < wall_threshold_deg(%.3f), is not better than best(%i), should discard",
					//	scan_frames_, at, line_count, abs_ang_diff_deg, wall_threshold_deg, best_line2->at);
					continue;
				}

			} else if (fabs(line.theta) < abs_best_theta) {
				// SDL_Log("#%i [%i/%i]line.theat(%.3f) < abs_best_theta(%.3f), should discard", scan_frames_, at, line_count, RAD2DEG(line.theta), RAD2DEG(abs_best_theta));
				continue;
			}
		}

/*
		SDL_Log("#%i [%i/%i]ms: %i, charge_width: %.3f, line.width_m: %.5f, line.y_diff_m: %.5f, start_dist: %.5f(%i, %i), end_dist: %.5f(%i, %i), theta: %.3f",
			scan_frames_, at, line_count, ms, charge_width, line.width_m, line.y_diff_m, 
			line.start_dist_m, line.start.x, line.start.y, 
			line.end_dist_m, line.end.x, line.end.y, RAD2DEG(line.theta));
*/
		best_line2 = &line2;
		abs_best_theta = fabs(line.theta);
	}

	if (verbose_png) {
		const std::string HoughLinesP_png = aplt.preferences_dir + "/HoughLinesP.png";
		imwrite(dst, HoughLinesP_png);
	}

	
	// const bool capture_not_find_LaserScan = game_config::os == os_windows;
	const bool capture_not_find_LaserScan = false;
	if (capture_not_find_LaserScan) {
		bool save_LaserScan = false;
		if (best_line2 != nullptr) {
			double start_dist_m = 0.25279;
			double end_dist_m = 0.28880;
			const tline_C& line = best_line2->line;
			if (fabs(line.start_dist_m - start_dist_m) > 0.06 || fabs(line.end_dist_m - end_dist_m) > 0.06) {
				save_LaserScan = true;
			}

		} else {
			save_LaserScan = true;
		}
		if (save_LaserScan) {
			const std::string path = game_config::preferences_dir + "/aplt_leagor_basic__documents/LaserScan.msg";
			ros::LaserScan_write(msg, nullptr, path);
		}
	}

	if (best_line2 != nullptr) {
		result = best_line2->line;

		tline_C& line = result;

		// =================
		memset(check_2th_data_.ptr, 0, cells);

		std::vector<SDL_Point> waypoints1;

		cv::Point pt1(line.start.x, line.start.y);
		cv::Point pt2(line.end.x, line.end.y);
		cv::LineIterator li(dilate, pt1, pt2);
		for (int j = 0; j < li.count; j++, ++ li) {
            const cv::Point p = li.pos();
			waypoints1.push_back(SDL_Point{p.x, p.y});
			int index = p.y * laser_scan.cols + p.x;
			check_2th_data_.ptr[index] = 255;
		}

		cv::Mat dilate2_mat;
		int radius = 4; // 4 * 0.04(1.6cm)
		cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * radius + 1, 2 * radius + 1), cv::Point(radius, radius));
		cv::dilate(check_2th_mat, dilate2_mat, element);

		if (verbose_png) {
			const std::string check_2th_png = aplt.preferences_dir + "/check_2th.png";
			imwrite_gray(check_2th_mat, check_2th_png);

			if (!dilate2_mat.empty()) {
				const std::string check_dilate_2th_png = aplt.preferences_dir + "/check_dilate_2th.png";
				imwrite_gray(dilate2_mat, check_dilate_2th_png);
			}
		}
		const uint8_t* dilate2_mat_data = dilate2_mat.ptr<uint8_t>(0);

		const uint8_t* laser_scan_raw_mat_data = laser_scan_raw.ptr<uint8_t>(0);
		int best_dist_2_start = nposm;
		SDL_Point best_p_2_start;

		int best_dist_2_end = nposm;
		SDL_Point best_p_2_end;
		for (int y = 0; y < check_2th_mat.rows; y ++) {
			int x_start = y * check_2th_mat.cols;
			for (int x = 0; x < check_2th_mat.cols; x ++) {
				int index = x_start + x;
				if (dilate2_mat_data[index] != 0 && laser_scan_raw_mat_data[index] != 0) {
					int dist_2_start = simple_2point_dist(x, y, line.start.x, line.start.y);
					if (dist_2_start >= best_dist_2_start) {
						best_dist_2_start = dist_2_start;
						best_p_2_start.x = x;
						best_p_2_start.y = y;
					}
					int dist_2_end = simple_2point_dist(x, y, line.end.x, line.end.y);
					if (dist_2_end >= best_dist_2_end) {
						best_dist_2_end = dist_2_end;
						best_p_2_end.x = x;
						best_p_2_end.y = y;
					}
				}
			}
		}

		line_from_2point(line, best_p_2_end.x, best_p_2_end.y, best_p_2_start.x, best_p_2_start.y, origin);
		// SDL_Log("#%i ms: %i, charge_width: %.3f, line.width_m: %.5f(%.5f), line.y_diff_m: %.5f(%.5f)(error: %.5f), start_dist: %.5f(%.5f), end_dist: %.5f(%.5f), theta: %.3f(%.3f)",
		//	scan_frames_, ms, charge_width, 
		//	line.width_m, hit_line->width_m, line.y_diff_m, hit_line->y_diff_m, laser_y_diff_error_,
		//	line.start_dist_m, hit_line->start_dist_m, line.end_dist_m, hit_line->end_dist_m, RAD2DEG(line.theta), RAD2DEG(hit_line->theta));
		line.y_diff_m -= laser_y_diff_error_;

	} else {
		VALIDATE(is_float_nposm(result.width_m), null_str);
	}

	free(line2s);

	return !is_float_nposm(result.width_m);
}

void set_255(uint8_t* data, int x, int y, int width, int height)
{
	if (x < 0 || x >= width) {
		return;
	}
	if (y < 0 || y >= height) {
		return;
	}
	// int index = x + (height - y - 1) * width;
	int index = x + y * width;
	data[index] = 255;
}

void tkcharge::reset_line_samples()
{
	SDL_Log("---reset_line_samples---");

	next_vel_sample_index_ = 0;
	next_finished_sample_index_ = 0;
	memset(line_samples_, 0, sizeof(line_samples_));
}
/*
bool tkcharge::is_sample_stable(int ltype, const tline_C& target_point)
{
	VALIDATE(ltype >= 0 && ltype < ltype_count, null_str);

	// const bool verbose = game_config::os == os_windows;
	// const bool verbose = false;
	const bool verbose = ltype == ltype_finished;

	tline_C* samples = line_samples_[ltype];
	const int max_line_samples = max_line_samples_[ltype];
	int& next_sample_index = ltype == ltype_vel? next_vel_sample_index_: next_finished_sample_index_;

	tline_C& cur_line_sample = samples[next_sample_index];
	cur_line_sample = target_point;

	if (verbose) {
		SDL_Log("-----{%s} next_sample_index: %i---", 
			ltype == ltype_vel? "vel": "finished", next_sample_index);
		for (int at = 0; at < max_line_samples; at ++) {
			const tline_C& line = samples[at];
			SDL_Log("[%i/%i]start: (%i, %i), end(%i, %i), width_m(%.6f), y_diff_m: %.5f, theta: %.3f(%.3f)", at, max_line_samples, 
				line.start.x, line.start.y, line.end.x, line.end.y, line.width_m, line.y_diff_m, line.theta, RAD2DEG(line.theta));
		}
		SDL_Log("--------");
	}

	next_sample_index ++;
	next_sample_index %= max_line_samples;

	const int threshold = 10; // 10 * 4mm
	for (int at1 = 0; at1 < max_line_samples; at1 ++) {
		const tline_C& line1 = samples[at1];
		for (int at2 = at1 + 1; at2 < max_line_samples; at2 ++) {
			const tline_C& line2 = samples[at2];

			SDL_Point start_diff{posix_abs(line1.start.x - line2.start.x), posix_abs(line1.start.y - line2.start.y)};
			SDL_Point end_diff{posix_abs(line1.end.x - line2.end.x), posix_abs(line1.end.y - line2.end.y)};
			if (verbose) {
				SDL_Log("is_sample_stable: [%i] - [%i]: start(%i, %i), end(%i, %i)", at1, at2, start_diff.x, start_diff.y, end_diff.x, end_diff.y);
			}
			if (start_diff.x > threshold || start_diff.y > threshold || end_diff.x > threshold || end_diff.y > threshold) {
				// if (verbose) {
					SDL_Log("{%s}is_sample_stable: [%i] - [%i]: start(%i, %i), end(%i, %i) > threshold(%i), return false", 
						ltype == ltype_vel? "vel": "finished", at1, at2, start_diff.x, start_diff.y, end_diff.x, end_diff.y, threshold);
				// }
				return false;
			}
		}
	}

	return true;
}
*/

bool tkcharge::maybe_90swing(double theta1, double theta2) const
{
	bool same_sign = theta1 * theta2 >= 0;
	return !same_sign && fabs(theta1) >= abs_min_90swing_theta_ && fabs(theta2) >= abs_min_90swing_theta_;
}

bool tkcharge::is_sample_stable(int ltype, const tline_C& target_point)
{
	VALIDATE(ltype >= 0 && ltype < ltype_count, null_str);

	// const bool verbose = game_config::os == os_windows;
	const bool verbose = false;
	// const bool verbose = ltype == ltype_finished;

	tline_C* samples = line_samples_[ltype];
	const int max_line_samples = max_line_samples_[ltype];
	int& next_sample_index = ltype == ltype_vel? next_vel_sample_index_: next_finished_sample_index_;

	tline_C& cur_line_sample = samples[next_sample_index];
	cur_line_sample = target_point;

	if (verbose) {
		SDL_Log("-----{%s} next_sample_index: %i---", 
			ltype == ltype_vel? "vel": "finished", next_sample_index);
		for (int at = 0; at < max_line_samples; at ++) {
			const tline_C& line = samples[at];
			SDL_Log("[%i/%i]start_dist: %.5f, end_dist: %.5f, width_m(%.6f), y_diff_m: %.5f, theta: %.3f(%.3f)", at, max_line_samples, 
				line.start_dist_m, line.end_dist_m, line.width_m, line.y_diff_m, line.theta, RAD2DEG(line.theta));
		}
		SDL_Log("--------");
	}

	next_sample_index ++;
	next_sample_index %= max_line_samples;

	const double threshold = 0.03; // 3cm
	for (int at1 = 0; at1 < max_line_samples; at1 ++) {
		const tline_C& line1 = samples[at1];
		for (int at2 = at1 + 1; at2 < max_line_samples; at2 ++) {
			const tline_C& line2 = samples[at2];

			double start_dist_diff = fabs(line1.start_dist_m - line2.start_dist_m);
			double end_dist_diff = fabs(line1.end_dist_m - line2.end_dist_m);

			if (verbose) {
				SDL_Log("is_sample_stable: [%i] - [%i]: start_dist: %.5f, end_dist: %.5f", at1, at2, start_dist_diff, end_dist_diff);
			}
			if (start_dist_diff > threshold || end_dist_diff > threshold) {
				bool fail = true;
				if (maybe_90swing(line1.theta, line2.theta)) {
					double start_dist_diff2 = fabs(line1.start_dist_m - line2.end_dist_m);
					double end_dist_diff2 = fabs(line1.end_dist_m - line2.start_dist_m);
					fail = start_dist_diff2 > threshold || end_dist_diff2 > threshold;
				}
				if (fail) {
					// if (verbose) {
						SDL_Log("{%s}is_sample_stable: [%i] - [%i]: start_dist: %.5f, end_dist: %.5f > threshold(%.5f), return false", 
							ltype == ltype_vel? "vel": "finished", at1, at2, start_dist_diff, end_dist_diff, threshold);
					// }
					return false;
				}
			}
		}
	}

	return true;
}

void tkcharge::average_sample(tline_C& result) const
{
	memset(&result, 0, sizeof(result));

	int positives = 0; // line.theta >= 0
	int negatives = 0; // line.theta < 0

	const int ltype = ltype_vel;
	const tline_C* samples = line_samples_[ltype];
	const int max_line_samples = max_line_samples_[ltype];

	for (int at = 0; at < max_line_samples; at ++) {
		const tline_C& line = samples[at];

		result.start.x += line.start.x;
		result.start.y += line.start.y;
		result.start_dist_m += line.start_dist_m;
		result.end.x += line.end.x;
		result.end.y += line.end.y;
		result.end_dist_m += line.end_dist_m;
		result.center.x += line.center.x;
		result.center.y += line.center.y;
		result.width_m += line.width_m;
		result.theta += line.theta;
		result.height_m += line.height_m;
		result.y_diff_m += line.y_diff_m;

		if (line.theta >= 0) {
			positives ++;
		} else {
			negatives ++;
		}
	}

	if (positives == max_line_samples || negatives == max_line_samples) {
		result.start.x /= max_line_samples;
		result.start.y /= max_line_samples;
		result.start_dist_m /= max_line_samples;
		result.end.x /= max_line_samples;
		result.end.y /= max_line_samples;
		result.end_dist_m /= max_line_samples;
		result.center.x /= max_line_samples;
		result.center.y /= max_line_samples;
		result.width_m /= max_line_samples;
		result.theta /= max_line_samples;
		result.height_m /= max_line_samples;
		result.y_diff_m /= max_line_samples;

	} else {
		// theta: -88.994, -88.994, -88.994, 88.958 --> if average, will be -44.506
		// get last sample
		result = line_samples_[ltype][max_line_samples - 1];
	}
}

// (x) / max_x = t / max_t  => (x) 
// 'dcalc' = double calculate
double dcalc_scale_x(double max_x, double t, double max_t)
{
	return max_x * t / max_t;
}

/*
float fcalc_scale_x(float max_x, float t, float max_t)
{
	return max_x * t / max_t;
}
*/

void calc_twist_large_theta(const tkcharge::tline_C& line, double abs_to_vertical_theta_deg, 
	double dist, double a_dist, double b_dist, bool forword_state, double* twist)
{
	VALIDATE(KDL::Equal(twist[1], 0), null_str);

	const double abs_x_linear = 0.01;
	double abs_angular_z_deg = abs_to_vertical_theta_deg + 20;

	if (forword_state) {
		twist[0] = dist < b_dist? abs_x_linear: 0;
	} else {
		twist[0] = dist > a_dist? -1 * abs_x_linear: 0;
	}

	if (line.theta > 0) {
		// must clockwise
		if (KDL::Equal(twist[0], 0)) {
			abs_angular_z_deg += 20;
		}

		abs_angular_z_deg = SDL_min(abs_angular_z_deg, 90);
		twist[2] = -1 * DEG2RAD(abs_angular_z_deg);

	} else {
		// must anti-clockwise
		if (KDL::Equal(twist[0], 0)) {
			abs_angular_z_deg += 20;
		}

		abs_angular_z_deg = SDL_min(abs_angular_z_deg, 90);
		twist[2] = DEG2RAD(abs_angular_z_deg);
	}
}

void tkcharge::move_public_vel(const tline_C& line)
{
	VALIDATE(next_public_vel_ticks_ == 0, null_str);
	VALIDATE(finish_state_ != finish_finished, null_str);

	const double laser_2_tail_dist = 0.20; // 20cm
	const double bind_move_dist = 0.06; // 4cm
	const double a_dist = laser_2_tail_dist + bind_move_dist;
	const double b_dist = 0.50; // 50cm

	VALIDATE(b_dist > a_dist + 0.15, null_str);

	double dist = SDL_min(line.start_dist_m, line.end_dist_m);
	VALIDATE(dist >= 0, null_str);
	
	double twist[3] = {float_nposm, 0, float_nposm};

	if (game_config::os == os_windows) {
		// Theoretically, 'line.theta' should be in [-90, 90], but for fear of accidents, more testing is needed.
		VALIDATE(fabs(RAD2DEG(line.theta)) <= 90.01, null_str);
	}
	const double abs_to_90_theta_deg = fabs(90 - fabs(RAD2DEG(line.theta)));
	
	const double abs_y_diff_threshold = 0.02; // 2cm
	double abs_y_diff_threshold_in_vert = 0.005; // 5mm
	VALIDATE(abs_y_diff_threshold_in_vert < abs_y_diff_threshold, null_str);

	const double must_vertical_theta_deg = 20; // 20
	const double must_vertical_theta_deg_in_vert = 15; // 15
	VALIDATE(must_vertical_theta_deg_in_vert < must_vertical_theta_deg, null_str);

	double abs_y_diff_m = fabs(line.y_diff_m);

	enum step_type_t {step_nposm = -1, y0_1th_largetheta, y0_2th_forward, y0_3th_backward, 
		vert_1th_backward, vert_2th_largetheta, finish_1th_theta, step_type_count};
	char step_types[][24] = {"y0_1th_largetheta", "y0_2th_forward", "y0_3th_backward", 
		"vert_1th_backward", "vert_2th_largetheta", "finish_1th_theta"};
	VALIDATE(sizeof(step_types) / sizeof(step_types[0]) == step_type_count, null_str);

	step_type_t step_type = step_nposm;

	bool stable = is_sample_stable(ltype_finished, line);
	if (stable && dist < a_dist && abs_y_diff_m < abs_y_diff_threshold && abs_to_90_theta_deg < must_vertical_theta_deg_in_vert) {
		// finish_state_: nposm -> finish_theta -> finish_finished
		if (finish_state_ == nposm) {
			finish_state_ = finish_theta;
			reset_line_samples();

		} else if (finish_state_ == finish_theta) {
			finish_state_ = finish_finished;
			r_api_.public_vel(0, 0, 0);
			return;
		}
	} else if (finish_state_ != nposm) {
		if (dist >= a_dist || abs_y_diff_m >= abs_y_diff_threshold || abs_to_90_theta_deg >= must_vertical_theta_deg_in_vert) {
			finish_state_ = nposm;
		}
	}

	if (finish_state_ != nposm) {
		VALIDATE(finish_state_ == finish_theta, null_str);
		step_type = finish_1th_theta;

		const double abs_x_linear = 0.01;
		double abs_angular_z_deg = SDL_max(abs_to_90_theta_deg, 8);

		twist[0] = -1 * abs_x_linear;

		if (line.theta > 0) {
			// must clockwise
			twist[2] = -1 * DEG2RAD(abs_angular_z_deg);

		} else {
			// must anti-clockwise
			twist[2] = DEG2RAD(abs_angular_z_deg);
		}


	} else if (abs_y_diff_m > abs_y_diff_threshold) {
		// y0 align
		step_type = y0_1th_largetheta;
		if (abs_to_90_theta_deg > must_vertical_theta_deg) {
			calc_twist_large_theta(line, abs_to_90_theta_deg, dist, a_dist, b_dist, forword_state_, twist);

		} else {
			if (dist < a_dist) {
				// must use forward
				forword_state_ = true;

			} else if (dist > b_dist) {
				// must use backward
				forword_state_ = false;
			}

			step_type = forword_state_? y0_2th_forward: y0_3th_backward;

			// 1/3: linear_x
			const SDL_DRange linear_x_range{0.05, 0.12}; // max(0.2) is too large.

			double abs_linear_x = linear_x_range.max;
			const double min_scale_y_diff_m = 0.12; // 8cm
			if (abs_y_diff_m <= min_scale_y_diff_m) {
				abs_linear_x = (linear_x_range.max - linear_x_range.min) * abs_y_diff_m / min_scale_y_diff_m;
				double abs_linear_x2 = dcalc_scale_x(linear_x_range.max - linear_x_range.min, abs_y_diff_m, min_scale_y_diff_m);
				VALIDATE(KDL::Equal(abs_linear_x, abs_linear_x2), null_str);

				abs_linear_x += linear_x_range.min;
			}

			// 2/3: angular_z
			const double max_abs_angular_z_large = DEG2RAD(30);
			const double max_abs_angular_z_normal = DEG2RAD(10);
			// const double max_abs_angular_z_small = DEG2RAD(8);

			// const double max_small_y_diff_m = 0.06; // 6cm

			double abs_angular_z = max_abs_angular_z_normal;
			if (abs_y_diff_m > min_scale_y_diff_m) {
				abs_angular_z = max_abs_angular_z_large;

			} /* else if (abs_y_diff_m <= max_small_y_diff_m) {
				abs_angular_z = max_abs_angular_z_small;
			} */

			// 3/3: evaluate to twist[].
			if (forword_state_) {
				twist[0] = abs_linear_x;
				twist[2] = line.y_diff_m > 0? -1 * abs_angular_z: abs_angular_z;

			} else {
				twist[0] = -1 * abs_linear_x;
				twist[2] = line.y_diff_m > 0? abs_angular_z: -1 * abs_angular_z;
			}
		}
	} else {
		// vertical align
		if (abs_to_90_theta_deg < must_vertical_theta_deg_in_vert) {
			step_type = vert_1th_backward;
			// must use backward
			forword_state_ = false;

			const double abs_linear_x_vert = 0.05;
			double linear_x = -1 * abs_linear_x_vert;
			twist[0] = linear_x;

			double abs_angular_z = DEG2RAD(4);
			if (abs_y_diff_m > abs_y_diff_threshold_in_vert) {
				twist[2] = line.y_diff_m > 0? abs_angular_z: -1 * abs_angular_z;

			} else {
				twist[2] = 0;
			}

		} else {
			step_type = vert_2th_largetheta;
			calc_twist_large_theta(line, abs_to_90_theta_deg, dist, a_dist, b_dist, forword_state_, twist);
		}
	}

	SDL_Log("%u %s, line(width_m: %.5f, y_diff_m: %.5f, theta: %.3f(to_90: %.3f)), dist: %.5f ==> vel: (%.5f, %.1f, %.3f)", SDL_GetTicks(), 
		step_types[step_type], line.width_m, line.y_diff_m, RAD2DEG(line.theta), abs_to_90_theta_deg, dist, twist[0], twist[1], RAD2DEG(twist[2]));


	VALIDATE(!is_float_nposm(twist[0]) && KDL::Equal(twist[1], 0) && !is_float_nposm(twist[2]), null_str);

	// bool disble_public_vel = game_config::is_dbg_charge();
	bool disble_public_vel = false;
	if (!disble_public_vel) {
		r_api_.public_vel(twist[0], twist[1], twist[2]);
	}

	next_public_vel_ticks_ = SDL_GetTicks() + move_robot_threshold_;
}

void zero_lonely(cv::Mat& src, int lonely_radius)
{
	VALIDATE(src.channels() == 1, null_str);
	VALIDATE(lonely_radius > 0, null_str);

	const uint8_t sured_val = 255;

	cv::Mat sured_mat = cv::Mat::zeros(src.rows, src.cols, CV_8UC1);
	uint8_t* sured_data = sured_mat.ptr<uint8_t>(0);

	uint8_t* src_data = src.ptr<uint8_t>(0);
	const int calc_width = src.cols - lonely_radius;
	const int calc_height = src.rows - lonely_radius;
	int index;
	int index2;
	for (int y = lonely_radius; y < calc_height; y ++) {
		int row_start = y * src.cols;
		for (int x = lonely_radius; x < calc_width; x ++) {
			index = row_start + x;

			if (src_data[index] != 0 && sured_data[index] == 0) {
				bool has_nzloc = false;
				uint8_t orginal = src_data[index];
				src_data[index] = 0;

				const int y2_end = y + lonely_radius + 1;
				const int x2_end = x + lonely_radius + 1;
				for (int y2 = y - lonely_radius; !has_nzloc && (y2 < y2_end); y2 ++) {
					int row_start2 = y2 * src.cols;
					VALIDATE(y2 < src.rows, null_str);
					for (int x2 = x - lonely_radius; !has_nzloc && (x2 < x2_end); x2 ++) {
						VALIDATE(x2 < src.cols, null_str);
						index2 = row_start2 + x2;
						if (src_data[index2] != 0) {
							has_nzloc = true;
							sured_data[index2] = sured_val;
						}
					}
				}
				if (has_nzloc) {
					src_data[index] = orginal;
					sured_data[index] = sured_val;
				}
			}
		}
	}
}

SDL_2Point tkcharge::did_scan_subscribed(const void* LaserScan_msg)
{
	const sensor_msgs::LaserScan* msg_ptr = reinterpret_cast<const sensor_msgs::LaserScan*>(LaserScan_msg);

	enum {msg_write, msg_read};
	const int msg_type = nposm;

	sensor_msgs::LaserScan msg1;
	if (msg_type == msg_write || msg_type == msg_read) {
		const std::string path = game_config::preferences_dir + "/aplt_leagor_basic__documents/LaserScan.msg";
		if (msg_type == msg_write) {
			ros::LaserScan_write(*msg_ptr, nullptr, path);

		} else {
			VALIDATE(msg_type == msg_read, null_str);
			ros::LaserScan_read(path, msg1, nullptr);
			msg_ptr = &msg1;
		}
	}
	const sensor_msgs::LaserScan& msg = *msg_ptr;

	const std::vector<float>& ranges = msg.ranges;

	SDL_2Point result{nposm, nposm, nposm, nposm};

	const int scan_msg_range_size = ranges.size();
	VALIDATE(scan_msg_range_size != 0, null_str);

	struct tangle_range {
		float angle;
		float echo;
		float x;
		float y;
		int int250_x;
		int int250_y;
	};
	tangle_range* angle_ranges = (tangle_range*)malloc(scan_msg_range_size * sizeof(tangle_range) * 2);
	memset(angle_ranges, 0, scan_msg_range_size * sizeof(tangle_range) * 2);

	int min_x = INT32_MAX;
    int min_y = INT32_MAX;
    int max_x = INT32_MIN;
    int max_y = INT32_MIN;

	int ep_count = 0;
	float angle = msg.angle_min;

	const double max_consider_range = 1.5; // 2.0(200), 1.5(250)
	const float range_max = SDL_min(msg.range_max, max_consider_range);

    for (int idx = 0; idx < scan_msg_range_size; ++ idx, angle += msg.angle_increment) {
        const float echo = ranges[idx];

        if (msg.range_min <= echo && echo <= range_max) {
			float dx = cos(angle) * echo;
			float dy = sin(angle) * echo;

			tangle_range& angle_range = angle_ranges[ep_count ++];
			angle_range.angle = angle;
			angle_range.echo = echo;
			angle_range.x = dx;
			angle_range.y = dy;

			angle_range.int250_x = (int)(dx * PIXEL_2_METER);
			angle_range.int250_y = (int)(dy * PIXEL_2_METER);

			posix_touch_i32(angle_range.int250_x, angle_range.int250_y, &min_x, &min_y, &max_x, &max_y);
		}
	}

	SDL_Rect rect{0, 0, max_x - min_x + 1, max_y - min_y + 1};
	const SDL_Point origin{0 - min_x, rect.h - (0 - min_y) - 1};

	for (int at = 0; at < ep_count; at ++) {
		tangle_range& angle_range = angle_ranges[at];
		angle_range.int250_x -= min_x;
		angle_range.int250_y -= min_y;
		VALIDATE(angle_range.int250_x >= 0 && angle_range.int250_x < rect.w, null_str);
		VALIDATE(angle_range.int250_y >= 0 && angle_range.int250_y < rect.h, null_str);
	}

	const bool verbose_png = game_config::os == os_windows && scan_frames_ == 0;
	// const bool verbose_png = false;
	int cells = rect.w * rect.h;

	cv::Mat laser_scan_mat = cv::Mat(rect.h, rect.w, CV_8UC1);
	uint8_t* laser_scan_mat_data = laser_scan_mat.ptr<uint8_t>(0);
	memset(laser_scan_mat_data, 0, cells);

    for (int at = 0; at < ep_count; at ++) {
		const tangle_range& angle_range = angle_ranges[at];
		int index = angle_range.int250_x + (rect.h - angle_range.int250_y - 1) * rect.w;
		// int index = angle_range.int250_x + angle_range.int250_y * rect.w;

		laser_scan_mat_data[index] = 255;
    }

	const cv::Mat laser_scan_raw_mat = laser_scan_mat.clone();

	if (game_config::os == os_windows) {
		set_255(laser_scan_mat_data, origin.x, origin.y, rect.w, rect.h);
		// y-1
		set_255(laser_scan_mat_data, origin.x - 1, origin.y - 1, rect.w, rect.h);
		set_255(laser_scan_mat_data, origin.x, origin.y - 1, rect.w, rect.h);
		set_255(laser_scan_mat_data, origin.x + 1, origin.y - 1, rect.w, rect.h);
		// y
		set_255(laser_scan_mat_data, origin.x - 1, origin.y, rect.w, rect.h);
		set_255(laser_scan_mat_data, origin.x + 1, origin.y, rect.w, rect.h);
		// y + 1
		set_255(laser_scan_mat_data, origin.x - 1, origin.y + 1, rect.w, rect.h);
		set_255(laser_scan_mat_data, origin.x, origin.y + 1, rect.w, rect.h);
		set_255(laser_scan_mat_data, origin.x + 1, origin.y + 1, rect.w, rect.h);
	}

	if (verbose_png) {
		imwrite_gray(laser_scan_mat, aplt_.preferences_dir + "/laser_scan.png");
	}

	const int lonely_radius = 3;
	zero_lonely(laser_scan_mat, lonely_radius);
	if (verbose_png) {
		imwrite_gray(laser_scan_mat, aplt_.preferences_dir + "/post_zero_lonely.png");
	}

	cv::Mat dilate_mat;
    int radius = 1; // 2(1cm)
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * radius + 1, 2 * radius + 1), cv::Point(radius, radius));
    cv::dilate(laser_scan_mat, dilate_mat, element);
    if (verbose_png) {
        imwrite_gray(dilate_mat, aplt_.preferences_dir + "/gray-dilate-1st.png");
    }

	tline_C hit_line;
	bool ret = find_line(msg, laser_scan_raw_mat, laser_scan_mat, dilate_mat, aplt_, origin, verbose_png, hit_line);
	bool stable = false;
	if (ret) {
		// y = rect.h - (angle_range.int100_y) - 1
		//   ==> (angle_range.int100_y) = rect.h - 1 - y
		const int y_add = min_y + rect.h - 1;
		result = SDL_2Point{hit_line.start.x + min_x, y_add - hit_line.start.y, hit_line.end.x + min_x, y_add - hit_line.end.y};

		stable = is_sample_stable(ltype_vel, hit_line);
	}
	if (stable) {
		if (next_public_vel_ticks_ != 0) {
			if (SDL_GetTicks() >= next_public_vel_ticks_) {
				// SDL_Log("%u ik fail{moving_4_ik} set next_public_vel_ticks_ = 0", SDL_GetTicks());
				next_public_vel_ticks_ = 0;
			}
		}
		if (next_public_vel_ticks_ == 0 && finish_state_ != finish_finished) {
			tline_C avg_line;
			average_sample(avg_line);
			move_public_vel(avg_line);
		}
	}

	free(angle_ranges);

	scan_frames_ ++;

	return result;
}

}
