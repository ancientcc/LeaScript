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

#ifndef LIBROSE2_WKOCAMERA_TASK_HPP
#define LIBROSE2_WKOCAMERA_TASK_HPP

#include "rose_mediapipe_api.hpp"
#include "wkoscript.hpp"
#include "so_aplt_task_helper.hpp"


namespace aplt {

//
// twkocamera_task
//

class DECLSPEC twkocamera_task: public thelper_camera_task_slot
{
public:
	struct ttmp_result {
		const twkoscript::tpose* pose;
		int pose_at;
		double val0;
		double* sample0;
		double val1;
		double* sample1;

		// only used for c++ debug.
		SDL_DPoint dbg_points[3];
	};

	twkocamera_task(thelper_lua_camera& lua_camera, tapplet& aplt, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars);
	~twkocamera_task() {}

private:
	std::string start_task() override;
	void task_finished(const tapplet::ttask& cfg_task) override;

	bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) override;
	bool camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb) override;
	void set_wko_task_slot(twko_task_slot* slot) override;
	void camera_work_frame(const surface& surf, const cv::Mat& argb) override;
	void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) override;
	bool is_wko_task_finished() override;

	void enter_state(twkoscript::tstate2& state2);

	void clear_kpose();
	void validate_nposm() const;

private:
	const trpy_sensor& rpy_sensor_;
	const int imwrite_interval_s_;
	const std::string var_name_timestamp_;
	const std::string var_name_png_;
	const std::string var_name_count_;
	const std::string var_name_not_send_;
	const int var_type_for_code_;
	const bool flip_h_;

	std::string unsatisfied_msg_;

	std::unique_ptr<mediapipe::tpose_tracking_api> api_ptr_;
	SDL_FPoint xy_landmarks_[mediapipe::kNumPoseLandmarks];

	const bool required_landmarks_[mediapipe::kNumPoseLandmarks];
	const int depended_landmarks_[mediapipe::kNumPoseLandmarks];
	SDL_U16Point u16landmarks_[mediapipe::kNumPoseLandmarks];

	int inference_ms_;
	cv::Mat output_mat_;
	bool failed_;

	enum {frame_ok, frame_fail};
	int new_frame_state_;
	bool frame_ok_received_;

	// sn section (sn: Shoulder Neck)
	twkoscript script_;
	ttask_vars script_task_vars_;
	twkoscript::tstate2* curr_state2_;
	bool tasks_suspended_;

	std::string sn_last_msg_;
	uint32_t workout_first_satisfied_ticks_;
	int64_t workout_first_satisfied_ts_;

	const int vertical_check_threshold_ms_;
	uint32_t next_vertical_check_ticks_;

	twko_task_slot* slot_;
};

}

#endif

