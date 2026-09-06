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

#ifndef LEAGOR_KPOSE_HPP_INCLUDED
#define LEAGOR_KPOSE_HPP_INCLUDED

#include "mediapipe/rose/mediapipe_api.hpp"
#include "so_aplt_task_helper.hpp"

namespace aplt {

//
// kpose
//
#define MAX_POSTURE_SAMPLES			4

class tkpose: public thelper_camera_task_slot
{
public:
	enum {fidx_nose_x, fidx_shoulder_mid_x, fidx_shoulder_width, 
		fidx_left_shoulder_x, fidx_right_shoulder_x, fidx_ear_width, fidx_ear_diff_y, fidx_head_forward, // 
		fidx_left_arm_angle, fidx_right_arm_angle, fidx_wrist_mid_x, fidx_wrist_width,
		fidx_left_shoulder_wrist_diff_xy, fidx_right_shoulder_wrist_diff_xy,
		fidx_spine_angle, fidx_shoulder_nose_angle, fidx_spine_length,
		fidx_mouth_mid_x, fidx_hip_y,
		posture_fields};
	struct tposture_C {
		float d[posture_fields];
	};

	tkpose(const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars);
	~tkpose() {}

private:
	std::string start_task() override;
	void task_finished(const tapplet::ttask& cfg_task) override;

	bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) override;
	bool camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb) override;
	void camera_work_frame(const surface& surf, const cv::Mat& argb) override;
	void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) override;

	void reload_pref_fields(int cpp_id);
	void clear_kpose();
	void validate_nposm() const;

	void reset_posture_samples();
	tposture_C average_sample(const tposture_C& newly);

	int sf_analyze_posture(const SDL_FPoint* landmarks, std::string& overlay_msg, std::string& improper_msg, uint32_t& improper_flags);
	int ss_analyze_posture(const SDL_FPoint* landmarks, bool left, std::string& overlay_msg, std::string& improper_msg, uint32_t& improper_flags);
	void latest_image_nopersion();
	void slice_sit(int env_var_value, std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects);

	const std::string& sit_msgstr(int type) const;
	int analyze_code_2_sitmsg_code(int code) const;
	int sedentary_elapse_s() const;
	int sedentary_noperson_s() const;
	void clear_sedentary_ticks();

	std::string sn_section_py_text(int section, int type) const;

	enum {snsect_0prelude, snsect_0desc, snsect_0_20, snsect_1prelude, snsect_1desc, snsect_1_20,
		snsect_2prelude, snsect_2desc, snsect_2_20, snsect_3prelude, snsect_3desc, snsect_3_20,
		snsect_4prelude, snsect_4desc, snsect_4_20, snsect_finished};
	enum {pytype_desc, pytype_unsatisfied, pytype_remind, pytype_finished, pytype_count};

	bool snsect_is_prelude(int sect) const
	{
		return sect == snsect_0prelude || sect == snsect_1prelude || sect == snsect_2prelude || 
			sect == snsect_3prelude || sect == snsect_4prelude;
	}

	bool snsect_is_desc(int sect) const
	{
		return sect == snsect_0desc || sect == snsect_1desc || sect == snsect_2desc || 
			sect == snsect_3desc || sect == snsect_4desc;
	}

	bool snsect_is_countdown(int sect) const
	{
		return sect == snsect_0_20 || sect == snsect_1_20 || sect == snsect_2_20 || 
			sect == snsect_3_20 || sect == snsect_4_20;
	}

	bool sn_analyze_section0(const SDL_FPoint* landmarks, std::string& msg);
	bool sn_analyze_section1(const SDL_FPoint* landmarks, std::string& msg);
	bool sn_analyze_section23(const SDL_FPoint* landmarks, bool first, std::string& msg);
	bool sn_analyze_section4(const SDL_FPoint* landmarks, std::string& msg);
	void sn_set_countdown(int _countdown_s);
	void sn_clear_countdown();
	void sn_countdown();
	void sn_update_next_remind_ticks();
	void sn_update_next_repeat_finished_ticks();
	void sn_update_next_unsatisfied_ticks();
	void sn_speak_unsatisfied_msg();
	void slice_shoulder_neck(int env_var_value, std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects);

private:
	const int imwrite_interval_s_;
	const std::string var_name_timestamp_;
	const std::string var_name_png_;
	const std::string var_name_count_;
	const std::string var_name_not_send_;
	const int var_type_for_code_;
	const int var_type_for_improper_msg_;
	const bool flip_h_;
	const std::string sedentary_label_;

	enum {type_sit_front, type_sit_lside, type_sit_rside, type_shoulder_neck};
	const std::map<std::string, int> types_;
	int type_;

	enum {sitmsg_sf_isnan, sitmsg_sf_missing, sitmsg_good, sitmsg_improper, sitmsg_noperson, 
		// good/improper
		sitmsg_shoulder_x_in_middle,
		sitmsg_parallel_not_too_crooked, sitmsg_field_parallel_rate,
		sitmsg_not_too_close, sitmsg_field_shoulder_width,
		sitmsg_face_not_too_crooked, sitmsg_field_ear_diff_y,
		sitmsg_head_not_too_low, sitmsg_field_head_forward,
		// noperson
		sitmsg_no_hip,
		sitmsg_ear_not_too_narrow,
		sitmsg_shoulder_not_too_narrow,
		sitmsg_nose_in_shoulders,

		//
		// sit side(right)
		//
		sitmsg_ss_isnan, sitmsg_ss_missing,
		// good/improper
		sitmsg_head_not_too_forward, sitmsg_field_shoulder_nose_angle,
		sitmsg_spine_not_too_bending, sitmsg_field_shoulder_hip_angle,
		sitmsg_shoulder_width_not_too_broad,
		sitmsg_spine_not_too_long, sitmsg_field_spine_length,
		sitmsg_hip_not_too_high, sitmsg_field_hip_y,
		// noperson
		sitmsg_spine_not_too_short,
		sitmsg_spine_angle_in_range,
		sitmsg_mouth_right_of_shoulders,

		sitmsg_count
	};

	std::vector <std::string> sit_msgstrs_;

	// sit front
	SDL_FRange spine_x_range_;
	SDL_FRange shoulder_width_range_;
	float ear_width_min_;
	float ear_diff_threshold_;
	float head_forward_threshold_;
	// sit side
	SDL_FRange ss_spine_length_range_;
	float ss_shoulder_width_max_;
	SDL_Range ss_improper_spine_angle_range_;
	int ss_shoulder_nose_angle_threshold_;
	SDL_Range ss_noperson_spine_angle_range_;
	// sit share
	int improper_time_;
	int noperson_time_;
	int sedentary_time_;
	int sedentary_noperson_threshold_;

	bool sitgood_new_one_sent_;
	const int reset_samples_threshold_ms_;
	std::string unsatisfied_msg_;

	std::unique_ptr<mediapipe::tpose_tracking_api> api_ptr_;
	SDL_FPoint xy_landmarks_[mediapipe::kNumPoseLandmarks];
	int spent_ms_;
	cv::Mat output_mat_;
	uint32_t improper_start_ticks_;
	uint32_t noperson_start_ticks_;
	uint32_t sedentary_start_ticks_;
	uint32_t sedentary_start_noperson_ticks_;
	bool failed_;

	enum {frame_ok, frame_fail};
	int new_frame_state_;
	bool frame_ok_received_;

	int next_sample_index_;
	tposture_C posture_samples_[MAX_POSTURE_SAMPLES];
	float temp_4_average_[MAX_POSTURE_SAMPLES];
	uint32_t last_recv_sample_ticks_;
	int recv_samples_;

	enum {analyze_noperson, analyze_posture_good, analyze_posture_improper, analyze_count};
	// int analyze_result_;
	std::string last_improper_msg_;

	// sn section (sn: Shoulder Neck)
	std::map<int64_t, std::string> sn_py_texts_;
	int sn_countdown_s_;
	int sn_remind_breath_s_;
	int sn_unsatisfied_s_threshold_;
	int sn_2th_unsatisfied_s_threshold_;
	int sn_remind_s_threshold_;
	int sn_repeat_finished_s_;
	int sn_section_;
	std::string sn_last_msg_;
	uint32_t sn_desire_request_ticks_;
	int sn_last_speak_s_;
	uint32_t sn_last_satisfied_ticks_;
	uint32_t sn_next_unsatisfied_ticks_;
	uint32_t sn_next_remind_ticks_;
	uint32_t sn_next_repeat_finished_ticks_;
	bool workout_start_sent_;
};

}

#endif

