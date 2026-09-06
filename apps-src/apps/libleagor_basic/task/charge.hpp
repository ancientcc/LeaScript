/* $Id: dialog.hpp 50956 2011-08-30 19:41:22Z mordante $ */
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

#ifndef LIBROSE_LEAGOR_BASIC_CHARGE_HPP_INCLUDED
#define LIBROSE_LEAGOR_BASIC_CHARGE_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "aplt2.hpp"

namespace aplt {

#define MAX_LINE_SAMPLES			4
#define MAX_VEL_LINE_SAMPLES		4
#define MAX_FINISHED_LINE_SAMPLES	3
#define PIXEL_2_METER			250.0

struct tline2_C;

class tkcharge: public tnonblock_api
{
public:
	struct tline_C {
		SDL_Point start;
		double start_dist_m;
		SDL_Point end;
		double end_dist_m;
		SDL_DPoint center;
		double width_m;
		double theta; // always in [-M_PI / 2, M_PI / 2].
		double height_m;
		// if < 0, The part above the origin.y is more than the part below the origin by abs(y_diff) meter.
		double y_diff_m;
	};

	tkcharge();
	~tkcharge();

private:
	std::string app_start_task(ttask_vars& vars) override;
	void app_task_finished(const tapplet::ttask& cfg_task) override;
	bool slice() override;

	// charging = nonblock_api_->did_scan_subscribed(msg.ranges, SDL_FRange{msg.range_min, msg.range_max}, 
		//	SDL_FRange{msg.angle_min, msg.angle_max}, msg.angle_increment);

	SDL_2Point did_scan_subscribed(const void* LaserScan_msg) override;

	bool line_a_is_better_b(const cv::Mat& dst, const tline2_C& a_line2, const tline2_C& b_line2, const SDL_Point& origin, bool verbose_png) const;
	bool find_line(const sensor_msgs::LaserScan& msg, const cv::Mat& laser_scan_raw, const cv::Mat& laser_scan, const cv::Mat& dilate, const tapplet& aplt, const SDL_Point& origin, bool verbose_png, tline_C& result);
	void reset_line_samples();
	bool maybe_90swing(double theta1, double theta2) const;
	bool is_sample_stable(int type, const tline_C& target_point);
	void average_sample(tline_C& result) const;
	void move_public_vel(const tline_C& hit_line);

	void clear()
	{
		start_ticks_ = 0;
		scan_frames_ = 0;
		finish_state_ = nposm;
		forword_state_ = true;

		next_public_vel_ticks_ = 0;
		if (check_2th_data_.ptr != nullptr) {
			free(check_2th_data_.ptr);
		}
		memset(&check_2th_data_, 0, sizeof(check_2th_data_));
	}

	void validate_nposm() const
	{
		VALIDATE(start_ticks_ == 0, null_str);
		VALIDATE(scan_frames_ == 0, null_str);
		VALIDATE(finish_state_ == nposm, null_str);
		VALIDATE(forword_state_, null_str);

		VALIDATE(next_public_vel_ticks_ == 0, null_str);
		VALIDATE(check_2th_data_.ptr == nullptr, null_str);
	}

private:
	tapplet& aplt_;
	aplt::tb_api& b_api_;
	aplt::tr_api& r_api_;
	const int move_robot_threshold_;
	const double laser_y_diff_error_;
	const double abs_min_90swing_theta_;
	uint32_t start_ticks_;
	int scan_frames_;
	enum {finish_theta, finish_finished};
	int finish_state_;

	// ltype: Line TYPE
	enum {ltype_vel, ltype_finished, ltype_count};
	int next_vel_sample_index_;
	int next_finished_sample_index_;
	tline_C line_samples_[ltype_count][MAX_LINE_SAMPLES];
	const int max_line_samples_[ltype_count];

	uint32_t next_public_vel_ticks_;
	tuint8data2_C check_2th_data_;
	bool forword_state_;
};

}

#endif

