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

#define GETTEXT_DOMAIN "aplt_leagor_khome-lib"

#include "kpose.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>
#include "rose_sdl_utils.hpp"
#include "rose_font.hpp"
#include "kdl/utilities/utility.h"
#include <angles/angles.h>
#include "aplt_api.hpp"

#include "common.hpp"
#include "camera_task.hpp"

using namespace std::placeholders;

// #define MEDIAPIPE_POSTURE_MIN_INTERVAL	150 // 150ms

namespace aplt {

//
// tkpose
//
#define DEF_SPINE_X_RANGE_MIN	0.4f
#define DEF_SPINE_X_RANGE_MAX	0.6f
#define DEF_SHOULDER_WIDTH_RANGE_MIN	0.25f
#define DEF_SHOULDER_WIDTH_RANGE_MAX	0.42f
#define DEF_EAR_WIDTH_MIN				0.10f
#define DEF_EAR_DIFF_THRESHOLD			0.05f
#define DEF_HEAD_FORWARD_THRESHOLD		0.25f

#define DEF_SS_SPINE_LENGTH_MIN		0.25f
#define DEF_SS_SPINE_LENGTH_MAX		0.60f
#define	DEF_SS_SHOULDER_WIDTH_MAX	0.13f
#define DEF_SS_IMPROPER_SPINE_ANGLE_MIN		268 // requrie '> DEF_SS_NOPERSON_SPINE_ANGLE_MIN'
#define DEF_SS_IMPROPER_SPINE_ANGLE_MAX		280 // require '< DEF_SS_NOPERSON_SPINE_ANGLE_MAX'
#define DEF_SS_SHOULDER_NOSE_ANGLE_THRESHOLD	45
#define DEF_SS_NOPERSON_SPINE_ANGLE_MIN		240 // 270 - 30
#define DEF_SS_NOPERSON_SPINE_ANGLE_MAX		300 // 270 + 30

#define DEF_IMPROPER_TIME		8
#define DEF_NOPERSON_TIME		120
#define DEF_SEDENTARY_TIME		30
#define	DEF_SEDENTARY_NOPERSON_JITTLE_THRESHOLD	60

static int64_t make_sn_py_text_key(int section, int type)
{
	return posix_mki64(section, type);
}

tkpose::tkpose(const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
	: thelper_camera_task_slot(*lua_camera, *curr_aplt, cfg_task, task_vars)
	, imwrite_interval_s_(60)
	, var_name_timestamp_(utils::join_app_prefix_id(aplt_.bundleid, "timestamp"))
	, var_name_png_(utils::join_app_prefix_id(aplt_.bundleid, "png"))
	, var_name_count_(utils::join_app_prefix_id(aplt_.bundleid, "count"))
	, var_name_not_send_(utils::join_app_prefix_id(aplt_.bundleid, "not_send"))
	, var_type_for_code_(var_env_basesubtask_code)
	, var_type_for_improper_msg_(var_env_basesubtask_str)
	, flip_h_(false)
	, sedentary_label_(_("Sedentary"))
	, types_{{"sit_front", type_sit_front}, {"sit_lside", type_sit_lside}, {"sit_rside", type_sit_rside}, {"shoulder_neck", type_shoulder_neck}}
	, sitgood_new_one_sent_(false)
	, reset_samples_threshold_ms_(5000) // 5s
	, sn_countdown_s_(20)
	, sn_remind_breath_s_(17)
	, sn_unsatisfied_s_threshold_(3)
	, sn_2th_unsatisfied_s_threshold_(8)
	, sn_remind_s_threshold_(15)
	, sn_repeat_finished_s_(10)
	, workout_start_sent_(false)
{
	VALIDATE(MAX_POSTURE_SAMPLES >= 3, null_str);

	// sitmsg_sf_isnan
	sit_msgstrs_.push_back(_("The camera did not capture the shoulders, ears, or nose"));
	// sitmsg_sf_missing
	sit_msgstrs_.push_back(_("The camera did not capture the shoulders, ears, or nose"));
	// sitmsg_good;
	sit_msgstrs_.push_back(_("sit^good"));
	// sitmsg_improper
	sit_msgstrs_.push_back(_("sit^improper"));
	// sitmsg_noperson
	sit_msgstrs_.push_back(_("sit^noperson"));

	// sitmsg_shoulder_x_in_middle
	sit_msgstrs_.push_back(_("Shoulders locate in middle of the camera"));
	// sitmsg_parallel_not_too_crooked
	sit_msgstrs_.push_back(_("Shoulder cannot be too crooked"));
	// sitmsg_field_parallel_rate,
	sit_msgstrs_.push_back(_("Shoulder parallel"));
	// sitmsg_not_too_close
	sit_msgstrs_.push_back(_("Not too close"));
	// sitmsg_field_shoulder_width
	sit_msgstrs_.push_back(_("Shoulder width"));
	// sitmsg_face_not_too_crooked
	sit_msgstrs_.push_back(_("Face cannot be too crooked"));
	// sitmsg_field_ear_diff_y
	sit_msgstrs_.push_back(_("Height difference between two ears"));
	// sitmsg_head_not_too_low
	sit_msgstrs_.push_back(_("Head cannot be too low"));
	// sitmsg_field_head_forward
	sit_msgstrs_.push_back(_("Height from nose to shoulder midpoint"));

	// sitmsg_no_hip
	sit_msgstrs_.push_back(_("Don't capture hip"));
	// sitmsg_ear_not_too_narrow
	sit_msgstrs_.push_back(_("Ear width canot be too narrow"));
	// sitmsg_shoulder_not_too_narrow
	sit_msgstrs_.push_back(_("Shoulder width canot be too narrow"));
	// sitmsg_nose_in_shoulders
	sit_msgstrs_.push_back(_("Nose locates in the shoulder area"));

	//
	// sit side(right)
	//
	// sitmsg_ss_isnan
	sit_msgstrs_.push_back(_("The camera did not capture right ear, shoulder, or hip"));
	// sitmsg_ss_missing
	sit_msgstrs_.push_back(_("The camera did not capture right ear, shoulder, or hip"));

	// sitmsg_head_not_too_forward
	sit_msgstrs_.push_back(_("Head cannot be too forward"));
	// sitmsg_field_shoulder_nose_angle
	sit_msgstrs_.push_back(_("Shoulder-nose angle"));
	// sitmsg_spine_not_too_bending
	sit_msgstrs_.push_back(_("Spine cannot be too bending"));
	// sitmsg_field_shoulder_hip_angle
	sit_msgstrs_.push_back(_("Shoulder-hip angle"));
	// sitmsg_shoulder_width_not_too_broad
	sit_msgstrs_.push_back(_("Shoulder cannot be too broad"));
	// sitmsg_spine_not_too_long
	sit_msgstrs_.push_back(_("Spine cannot be too long"));
	// sitmsg_field_spine_length
	sit_msgstrs_.push_back(_("Spine length"));
	// sitmsg_hip_not_too_high
	sit_msgstrs_.push_back(_("Hip cannot be too high"));
	// sitmsg_field_hip_y
	sit_msgstrs_.push_back(_("Hip position"));

	// ---noperson---
	// sitmsg_spine_not_too_short
	sit_msgstrs_.push_back(_("Spine cannot be too short"));
	// sitmsg_spine_angle_in_range
	sit_msgstrs_.push_back(_("Spine angle within a specific range"));
	// sitmsg_mouth_right_of_shoulders
	sit_msgstrs_.push_back(_("Mouth is to the right of shoulders"));

	VALIDATE(sit_msgstrs_.size() == (int)sitmsg_count, null_str);

	VALIDATE(sn_countdown_s_ > sn_remind_breath_s_, null_str);
	VALIDATE(sn_2th_unsatisfied_s_threshold_ >= sn_unsatisfied_s_threshold_, null_str);
	VALIDATE(sn_remind_s_threshold_ > sn_unsatisfied_s_threshold_, null_str);

	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_0desc, pytype_desc), _("sn^0desc")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_0_20, pytype_unsatisfied), _("sn^0unsatisfied")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_0_20, pytype_remind), _("sn^0remind")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_0_20, pytype_finished), _("sn^0finished")));

	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_1desc, pytype_desc), _("sn^1desc")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_1_20, pytype_unsatisfied), _("sn^1unsatisfied")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_1_20, pytype_remind), _("sn^1remind")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_1_20, pytype_finished), _("sn^1finished")));

	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_2desc, pytype_desc), _("sn^2desc")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_2_20, pytype_unsatisfied), _("sn^2unsatisfied")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_2_20, pytype_remind), _("sn^2remind")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_2_20, pytype_finished), _("sn^2finished")));

	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_3desc, pytype_desc), _("sn^3desc")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_3_20, pytype_unsatisfied), _("sn^3unsatisfied")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_3_20, pytype_remind), _("sn^3remind")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_3_20, pytype_finished), _("sn^3finished")));

	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_4desc, pytype_desc), _("sn^4desc")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_4_20, pytype_unsatisfied), _("sn^4unsatisfied")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_4_20, pytype_remind), _("sn^4remind")));
	sn_py_texts_.insert(std::make_pair(make_sn_py_text_key(snsect_4_20, pytype_finished), _("sn^4finished")));
	VALIDATE(sn_py_texts_.size() == 5 * pytype_count, null_str);

	reload_pref_fields(cpp_id_save_sf_6fields);
	reload_pref_fields(cpp_id_save_ss_4fields);
	reload_pref_fields(cpp_id_save_sshare_3fields);

	clear_kpose();
}

std::string tkpose::start_task()
{
	std::string err_msg;
	utils::string_map symbols;
	validate_nposm();

	int type = type_sit_front;
	const std::string var_name_type = utils::join_app_prefix_id(aplt_.bundleid, "type");
	if (task_vars_.existed(var_name_type)) {
		const std::string type_str = task_vars_.get_string(var_name_type);
		if (types_.count(type_str) != 0) {
			type = types_.find(type_str)->second;
		} else {
			symbols["type"] = type_str;
			err_msg = vgettext2("Unknow type: $type", symbols);
			return err_msg;
		}
	}
	VALIDATE(type != nposm, null_str);

	if (type == type_sit_lside) {
		symbols["type"] = "sit_lside";
		err_msg = vgettext2("This version does not support type: $type", symbols);
		return err_msg;
	}

	api_ptr_.reset(mediapipe::rose_create_pose_tracking_api());
	if (!api_ptr_->graph_initialized()) {
		err_msg = _("Initialize mediapipe graph fail");
	} else {
		type_ = type;
		// if (game_config::os == os_windows) {
		//	api_ptr_->set_min_next_interval_no_cpu_saver(MEDIAPIPE_POSTURE_MIN_INTERVAL);
		// }
	}

	return err_msg;
}

void tkpose::task_finished(const tapplet::ttask& cfg_task)
{
	clear_kpose();
}

// Calculate midpoint coordinates
SDL_FPoint mid_point(const SDL_FPoint& p1, 
                                 const SDL_FPoint& p2)
{
	return {(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
}

bool AnalyzePosture(const SDL_FPoint* landmarks)
{
	// Mid point of shoulder
	SDL_FPoint shoulder_mid = mid_point(landmarks[11], landmarks[12]);
	// Mid point of hip
	SDL_FPoint hip_mid = mid_point(landmarks[23], landmarks[24]);
  
	// Vertebral tilt angle (X-axis difference)
	float spine_angle = std::abs(shoulder_mid.x - hip_mid.x);
  
	// Shoulder horizontal difference (Y-axis difference)
	float shoulder_diff = std::abs(landmarks[11].y - landmarks[12].y);
  
	// Is the head tilted forward (comparing the Y-axis of the nose and shoulders)
	bool head_forward = landmarks[0].y < shoulder_mid.y;

	// judging condition
	if (spine_angle > 0.1 || shoulder_diff > 0.05 || head_forward) {
		// SDL_Log("Improper sitting posture! Please adjust your posture!");
		return false;

	} else {
		// SDL_Log("Good sitting posture");
		return true;
	}
}

void tkpose::reset_posture_samples()
{
	SDL_Log("---reset_posture_samples---");

	next_sample_index_ = 0;
	memset(posture_samples_, 0, sizeof(posture_samples_));

	last_recv_sample_ticks_ = 0;
	recv_samples_ = 0;
}

float calculate_one_avg(const tkpose::tposture_C* samples, int max_line_samples, int fidx)
{
	// Remove the maximum and minimum values, and take the average of the middle ones.
	const tkpose::tposture_C& line0 = samples[0];
	float min_val = line0.d[fidx];
	float max_val = line0.d[fidx];
	int min_at = 0;
	int max_at = 0;
	for (int at = 1; at < max_line_samples; at ++) {
		const tkpose::tposture_C& line = samples[at];

		if (line.d[fidx] < min_val) {
			min_val = line.d[fidx];
			min_at = at;

		} else if (line.d[fidx] > max_val) {
			max_val = line.d[fidx];
			max_at = at;
		}
	}
	float sum = 0;
	for (int at = 0; at < max_line_samples; at ++) {
		if (at == min_at || at == max_at) {
			continue;
		}
		const tkpose::tposture_C& line = samples[at];
		sum += line.d[fidx];
	}

	return sum / (max_line_samples - 2);
}

static float remove_two_farthest_iterative(const tkpose::tposture_C* samples, int max_samples, int fidx, float* temp)
{   
	VALIDATE(max_samples > 2, null_str);

    for (int i = 0; i < max_samples; i++) {
		const tkpose::tposture_C& sample = samples[i];
		temp[i] = sample.d[fidx];
	}
    int size = max_samples;
    
	float sum = 0.0f;
    // Remove the two farthest points
    for (int remove = 0; remove < 2; remove ++) {
        // Calculate the average
        sum = 0.0f;
        for (int i = 0; i < size; i++) {
			sum += temp[i];
		}
        float mean = sum / size;
        
        // Find the farthest
        int farthest_idx = 0;
        float max_dist = fabs(temp[0] - mean);
        for (int i = 1; i < size; i++) {
            float dist = fabs(temp[i] - mean);
            if (dist > max_dist) {
                max_dist = dist;
                farthest_idx = i;
            }
        }
        
        // Delete this element
        for (int i = farthest_idx; i < size - 1; i++) {
            temp[i] = temp[i + 1];
        }
        size --;
    }
    
    // copy result
	sum = 0.0;
	for (int i = 0; i < size; i++) {
		sum += temp[i];
	}

    float mean = sum / size;
    return mean;
}

tkpose::tposture_C tkpose::average_sample(const tposture_C& newly)
{
	//
	// step1: add newly_sample to samples.
	//
	if (last_recv_sample_ticks_ != 0 && (int)(SDL_GetTicks() - last_recv_sample_ticks_) > reset_samples_threshold_ms_) {
		reset_posture_samples();
	}

	const bool verbose = false;

	tposture_C* samples = posture_samples_;
	const int max_line_samples = MAX_POSTURE_SAMPLES;
	int& next_sample_index = next_sample_index_;

	tposture_C& cur_line_sample = samples[next_sample_index];
	cur_line_sample = newly;

	if (verbose) {
		SDL_Log("-----{%s} next_sample_index: %i---", 
			"vel", next_sample_index);
		for (int at = 0; at < max_line_samples; at ++) {
			const tposture_C& line = samples[at];
			SDL_Log("[%i/%i]nose_x: %.3f, shoulder_mid: %.3f, shoulder_width: %.3f, ear_diff: %.3f, head_forward: %.3f", at, max_line_samples, 
				line.d[fidx_nose_x], line.d[fidx_shoulder_mid_x], line.d[fidx_shoulder_width], line.d[fidx_ear_diff_y], line.d[fidx_head_forward]);
		}
		SDL_Log("--------");
	}

	next_sample_index ++;
	next_sample_index %= max_line_samples;

	last_recv_sample_ticks_ = SDL_GetTicks();
	recv_samples_ ++;

	//
	// step2: calculate average sample.
	//
	if (recv_samples_ < MAX_POSTURE_SAMPLES) {
		return newly;
	}

	tposture_C result;
	// memset(&result, 0, sizeof(result));
	for (int at = 0; at < posture_fields; at ++) {
		// result.d[at] = calculate_one_avg(samples, MAX_POSTURE_SAMPLES, at);
		result.d[at] = remove_two_farthest_iterative(samples, MAX_POSTURE_SAMPLES, at, temp_4_average_);
	}
	return result;
}

const std::string& tkpose::sit_msgstr(int type) const
{
	VALIDATE(type >= 0 && type < sitmsg_count, null_str);
	return sit_msgstrs_[type];
}

int tkpose::analyze_code_2_sitmsg_code(int code) const
{
	if (code == analyze_noperson) {
		return sitmsg_noperson;

	} else if (code == analyze_posture_good) {
		return sitmsg_good;
	}
	VALIDATE(code == analyze_posture_improper, null_str);
	return sitmsg_improper;
}

int tkpose::sedentary_elapse_s() const
{
	if (sedentary_start_ticks_ == 0) {
		return 0;
	}
	return (SDL_GetTicks() - sedentary_start_ticks_) / 1000;
}

int tkpose::sedentary_noperson_s() const
{
	if (sedentary_start_noperson_ticks_ == 0) {
		return 0;
	}
	return (SDL_GetTicks() - sedentary_start_noperson_ticks_) / 1000;
}

void tkpose::clear_sedentary_ticks()
{
	sedentary_start_ticks_ = 0;
	sedentary_start_noperson_ticks_ = 0;
}

bool is_landmarks_captured(const SDL_FPoint* landmarks, const int* desired, int size)
{
	float bonus = 0.0f; // 0.05f
	float min = bonus;
	float max = 1.0f - bonus;

	for (int at = 0; at < size; at ++) {
		int n = desired[at];
		VALIDATE(n >= 0 && n < mediapipe::kNumPoseLandmarks, null_str);
		if (landmarks[n].x < min || landmarks[n].x > max || landmarks[n].y < min || landmarks[n].y > max) {
			return false;
		}
	}
	return true;
}

bool is_landmark_captured(const SDL_FPoint& landmark)
{
	float bonus = 0.0f; // 0.05f
	float min = bonus;
	float max = 1.0f - bonus;

	if (landmark.x < min || landmark.x > max || landmark.y < min || landmark.y > max) {
		return false;
	}

	return true;
}


// Analyze Posture
int tkpose::sf_analyze_posture(const SDL_FPoint* landmarks, std::string& overlay_msg, std::string& improper_msg, uint32_t& improper_flags)
{
	improper_msg.clear();
	improper_flags = 0;

	// Must be sufficient to accommodate the maximum string length. 
	// Because it contains Chinese characters, if truncated, it may become not a valid UTF-8 string.
	char buf[384];

	// char result_str[][20] = {"noperson", "good", "improper"};
	// VALIDATE(sizeof(result_str) / sizeof(result_str[0]) == analyze_count, null_str);
	char dismatch_str[] = {"[X]"};

	int result = nposm;
	if (std::isnan(landmarks[0].x) ||
		std::isnan(landmarks[7].x) ||
		std::isnan(landmarks[8].x) ||
		std::isnan(landmarks[11].x) ||
		std::isnan(landmarks[12].x)) {
		SDL_snprintf(buf, sizeof(buf), "%i ms %s: %s/%i\n%s\n:%s", spent_ms_, sedentary_label_.c_str(), utils::format_elapse_hms(sedentary_elapse_s()).c_str(), sedentary_noperson_s(),
			sit_msgstr(sitmsg_sf_isnan).c_str(), sit_msgstr(sitmsg_noperson).c_str());
		overlay_msg = buf;
		return analyze_noperson;
	}

	// When the required landsmarks are not captured, it is difficult to determine whether to return 'noperson or 'improper'. 
	// So, we have to let the following code to judge. 
	// Note that the landmark's value may be negative or greater than 1.0.
	const int desired[] = {0, 7, 8, 11, 12};
	bool is_all_captured = is_landmarks_captured(landmarks, desired, sizeof(desired) / sizeof(desired[0]));
	
	// Mid point of shoulder
	SDL_FPoint shoulder_mid = mid_point(landmarks[11], landmarks[12]);
	// Mid point of hip
	SDL_FPoint hip_mid = mid_point(landmarks[23], landmarks[24]);
  
	// Vertebral tilt angle (X-axis difference)
	// float spine_angle = fabs(shoulder_mid.x - hip_mid.x);
  
	float shoulder_width = fabs(landmarks[11].x - landmarks[12].x);

	// Shoulder horizontal difference (Y-axis difference)
	float shoulder_diff_y = fabs(landmarks[11].y - landmarks[12].y);

	float ear_width = fabs(landmarks[7].x - landmarks[8].x);
	float ear_diff_y = fabs(landmarks[7].y - landmarks[8].y);
  
	// Is the head tilted forward (comparing the Y-axis of the nose and shoulders)
	float head_forward = shoulder_mid.y - landmarks[0].y;

	float nose_x = landmarks[0].x;

	tposture_C posture;
	posture.d[fidx_nose_x] = nose_x;
	posture.d[fidx_shoulder_mid_x] = shoulder_mid.x;
	posture.d[fidx_shoulder_width] = shoulder_width;
	posture.d[fidx_left_shoulder_x] = landmarks[11].x;
	posture.d[fidx_right_shoulder_x] = landmarks[12].x;
	posture.d[fidx_ear_width] = ear_width;
	posture.d[fidx_ear_diff_y] = ear_diff_y;
	posture.d[fidx_head_forward] = head_forward;

	tposture_C avg = average_sample(posture);
	const bool noperson_no_hip = !is_landmark_captured(landmarks[23]) && !is_landmark_captured(landmarks[24]);
	const bool noperson_nose_in_shoulders = avg.d[fidx_nose_x] >= avg.d[fidx_right_shoulder_x] && avg.d[fidx_nose_x] <= avg.d[fidx_left_shoulder_x];
	const bool ear_not_too_narrow = avg.d[fidx_ear_width] >= ear_width_min_;
	const bool shoulder_not_too_narrow = avg.d[fidx_shoulder_width] >= shoulder_width_range_.min;
	if (!noperson_no_hip || !noperson_nose_in_shoulders ||
		!ear_not_too_narrow || !shoulder_not_too_narrow) {
		result = analyze_noperson;

		SDL_snprintf(buf, sizeof(buf), "%i ms %s: %s/%i\n%s%s\n%s%s: %.3f[%.3f, %.3f]\n%s%s: %.3f(>=)%.3f\n%s%s: %.3f(>=)%.3f\n:%s", 
			spent_ms_, sedentary_label_.c_str(), utils::format_elapse_hms(sedentary_elapse_s()).c_str(), sedentary_noperson_s(),
			noperson_no_hip? "": dismatch_str, sit_msgstr(sitmsg_no_hip).c_str(),
			noperson_nose_in_shoulders? "": dismatch_str, sit_msgstr(sitmsg_nose_in_shoulders).c_str(), nose_x, avg.d[fidx_right_shoulder_x], avg.d[fidx_left_shoulder_x],
			ear_not_too_narrow? "": dismatch_str, sit_msgstr(sitmsg_ear_not_too_narrow).c_str(), ear_width, ear_width_min_,
			shoulder_not_too_narrow? "": dismatch_str, sit_msgstr(sitmsg_shoulder_not_too_narrow).c_str(), shoulder_width, shoulder_width_range_.min,
			sit_msgstr(analyze_code_2_sitmsg_code(result)).c_str());

	} else {
		const float parallel_rate_threshold = 0.60f;
		float parallel_left = fabs(posture.d[fidx_nose_x] - posture.d[fidx_right_shoulder_x]);
		float parallel_right = fabs(posture.d[fidx_left_shoulder_x] - posture.d[fidx_nose_x]);
		float parallel_rate = parallel_left <= parallel_right? parallel_left / parallel_right: parallel_right / parallel_left;

		const bool shoulder_x_in_middle = avg.d[fidx_shoulder_mid_x] >= spine_x_range_.min && avg.d[fidx_shoulder_mid_x] <= spine_x_range_.max;
		bool parallel_not_too_crooked = parallel_rate >= parallel_rate_threshold;
		bool not_too_close = avg.d[fidx_shoulder_width] <= shoulder_width_range_.max;
		bool face_not_too_crooked = avg.d[fidx_ear_diff_y] <= ear_diff_threshold_;
		bool head_not_too_low = avg.d[fidx_head_forward] >= head_forward_threshold_;
		result = analyze_posture_improper;

		// const int desired[] = {0, 7, 8, 11, 12};
		if (!is_all_captured) {
			improper_msg = sit_msgstr(sitmsg_sf_missing);

		} else if (!shoulder_x_in_middle) {
			improper_msg = sit_msgstr(sitmsg_shoulder_x_in_middle);

		} else if (!not_too_close) {
			improper_msg = sit_msgstr(sitmsg_not_too_close);

		} else if (!face_not_too_crooked) {
			improper_msg = sit_msgstr(sitmsg_face_not_too_crooked);

		} else if (!head_not_too_low) {
			improper_msg = sit_msgstr(sitmsg_head_not_too_low);

		} else if (!parallel_not_too_crooked) {
			improper_msg = sit_msgstr(sitmsg_parallel_not_too_crooked);

		} else {
			// Good sitting posture
			result = analyze_posture_good;
		}

		if (result == analyze_posture_improper) {
			if (!is_all_captured) { improper_flags |= BIT_IDX_MASK(sitimproper_all_captured); }
			if (!shoulder_x_in_middle) { improper_flags |= BIT_IDX_MASK(sitimproper_shoulder_x_in_middle); }
			if (!not_too_close) { improper_flags |= BIT_IDX_MASK(sitimproper_not_too_close); }
			if (!face_not_too_crooked) { improper_flags |= BIT_IDX_MASK(sitimproper_face_not_too_crooked); }
			if (!head_not_too_low) { improper_flags |= BIT_IDX_MASK(sitimproper_head_not_too_low); }
			if (!parallel_not_too_crooked) { improper_flags |= BIT_IDX_MASK(sitimproper_parallel_not_too_crooked); }
		}

		if (is_all_captured) {
			SDL_snprintf(buf, sizeof(buf), "%i ms %s: %s/%i\n%s%s: %.3f[%.3f, %.3f]\n%s(%s)%s: %.3f(<=)%.3f\n%s(%s)%s: %.3f(<=)%.3f\n%s(%s)%s: %.3f(>=)%.3f\n%s(%s)%s: %.3f(>=)%.3f\n:%s", 
				spent_ms_, sedentary_label_.c_str(), utils::format_elapse_hms(sedentary_elapse_s()).c_str(), sedentary_noperson_s(),
				shoulder_x_in_middle? "": dismatch_str, sit_msgstr(sitmsg_shoulder_x_in_middle).c_str(), shoulder_mid.x, spine_x_range_.min, spine_x_range_.max,
				not_too_close? "": dismatch_str, sit_msgstr(sitmsg_not_too_close).c_str(), sit_msgstr(sitmsg_field_shoulder_width).c_str(), shoulder_width, shoulder_width_range_.max,
				face_not_too_crooked? "": dismatch_str, sit_msgstr(sitmsg_face_not_too_crooked).c_str(), sit_msgstr(sitmsg_field_ear_diff_y).c_str(), ear_diff_y, ear_diff_threshold_,
				head_not_too_low? "": dismatch_str, sit_msgstr(sitmsg_head_not_too_low).c_str(), sit_msgstr(sitmsg_field_head_forward).c_str(), head_forward, head_forward_threshold_,
				parallel_not_too_crooked? "": dismatch_str, sit_msgstr(sitmsg_parallel_not_too_crooked).c_str(), sit_msgstr(sitmsg_field_parallel_rate).c_str(), parallel_rate, parallel_rate_threshold,
				sit_msgstr(analyze_code_2_sitmsg_code(result)).c_str());

		} else {
			SDL_snprintf(buf, sizeof(buf), "%i ms %s: %s/%i\n%s\n:%s", 
				spent_ms_, sedentary_label_.c_str(), utils::format_elapse_hms(sedentary_elapse_s()).c_str(), sedentary_noperson_s(),
				sit_msgstr(sitmsg_sf_missing).c_str(), sit_msgstr(analyze_code_2_sitmsg_code(result)).c_str());
		}
	}

	VALIDATE(result >= 0 && result < analyze_count, null_str);
	overlay_msg = buf;

	return result;
}

double calculate_angle_3FPoint(const SDL_FPoint& a, const SDL_FPoint& b, const SDL_FPoint& c)
{
	return utils::imgcoor_calculate_angle_3p_pi(a.x, a.y, b.x, b.y, c.x, c.y);
}

double calculate_angle_3DPoint(const SDL_DPoint& a, const SDL_DPoint& b, const SDL_DPoint& c)
{
	return utils::imgcoor_calculate_angle_3p_pi(a.x, a.y, b.x, b.y, c.x, c.y);
}
/*
double imgcoor_calculate_angle_2p(double start_x, double start_y, double end_x, double end_y)
{
	double deltax = end_x - start_x;
	double deltay = end_y - start_y;

	if (fabs(deltax) < 1e-10 && fabs(deltay) < 1e-10) {
		// if both x and y are 0, atan2's result is undefined.
		// The result depends on the specific library implementation. Like MSVC, it is likely to be 0.
		return 0;		
	}

	double theta = atan2(deltay, deltax);

	// atan2's return value: >0(anticlockwise), <0(clockwise). 
	// It is what mathematics teaches
	// but @start_x/start_y, @end_x/end_y are image-coor, it' Y is opsitive of mathematics-coor.
	// Multiplying by -1 corrects this, making the final angle follow math convention:
	// >0 means anticlockwise, <0 means clockwise.
	return -1 * theta;
}
*/
double imgcoor_calculate_angle_2FPoint(const SDL_FPoint& start, const SDL_FPoint& end)
{
	return utils::imgcoor_calculate_angle_2p(start.x, start.y, end.x, end.y);
}

double imgcoor_calculate_angle_2DPoint(const SDL_DPoint& start, const SDL_DPoint& end)
{
	return utils::imgcoor_calculate_angle_2p(start.x, start.y, end.x, end.y);
}

double distance(const SDL_FPoint& p1, const SDL_FPoint& p2)
{
	return hypot(p1.x - p2.x, p1.y - p2.y);
}

int tkpose::ss_analyze_posture(const SDL_FPoint* landmarks, bool left, std::string& overlay_msg, std::string& improper_msg, uint32_t& improper_flags)
{
	VALIDATE(!left, null_str);

	improper_msg.clear();
	improper_flags = 0;

	// Must be sufficient to accommodate the maximum string length. 
	// Because it contains Chinese characters, if truncated, it may become not a valid UTF-8 string.
	char buf[384];

	char dismatch_str[] = {"[X]"};

	// Left arm angle detection
	const int nose_at = 0;
	const int ear_at = left? 7: 8;
	const int shoulder_at = left? 11: 12;
	const int hip_at = left? 23: 24;

	const SDL_FPoint& nose = landmarks[0];
	const SDL_FPoint& ear = left? landmarks[7]: landmarks[8];
	const SDL_FPoint& shoulder = left? landmarks[11]: landmarks[12];
	const SDL_FPoint& hip = left? landmarks[23]: landmarks[24];

	if (std::isnan(nose.x) ||
		std::isnan(ear.x) ||
		std::isnan(shoulder.x) ||
		std::isnan(hip.x)) {
		SDL_snprintf(buf, sizeof(buf), "%i ms %s: %s/%i\n%s\n:%s", spent_ms_, sedentary_label_.c_str(), utils::format_elapse_hms(sedentary_elapse_s()).c_str(), sedentary_noperson_s(),
			sit_msgstr(sitmsg_ss_isnan).c_str(), sit_msgstr(sitmsg_noperson).c_str());
		overlay_msg = buf;
		return analyze_noperson;
	}

	// When the required landsmarks are not captured, it is difficult to determine whether to return 'noperson or 'improper'. 
	// So, we have to let the following code to judge. 
	// Note that the landmark's value may be negative or greater than 1.0.
	const int desired[] = {nose_at, ear_at, 11, 12, hip_at};
	bool is_all_captured = is_landmarks_captured(landmarks, desired, sizeof(desired) / sizeof(desired[0]));

	int result = nposm;

	  
	// Vertebral tilt angle (X-axis difference)
	double spine_rad = imgcoor_calculate_angle_2FPoint(shoulder, hip);
	// ==> in [0, 2*pi)
	spine_rad = angles::normalize_angle_positive(spine_rad);
	double spine_angle = RAD2DEG(spine_rad);

	// When thinking, fingers may support the nose.
	double shoulder_nose_rad = imgcoor_calculate_angle_2FPoint(shoulder, nose);
    shoulder_nose_rad = angles::normalize_angle_positive(shoulder_nose_rad);
	double shoulder_nose_angle = RAD2DEG(shoulder_nose_rad);

	// double spine_length = distance(landmarks[12], landmarks[24]);
	double spine_length = fabs(landmarks[12].y - landmarks[24].y);
	float shoulder_width = fabs(landmarks[11].x - landmarks[12].x);
	SDL_FPoint mouth_mid = mid_point(landmarks[9], landmarks[10]);

	tposture_C posture;
	posture.d[fidx_spine_angle] = spine_angle;
	posture.d[fidx_shoulder_nose_angle] = shoulder_nose_angle;
	posture.d[fidx_spine_length] = spine_length;
	posture.d[fidx_shoulder_width] = shoulder_width;
	posture.d[fidx_left_shoulder_x] = landmarks[11].x;
	posture.d[fidx_right_shoulder_x] = landmarks[12].x;
	posture.d[fidx_mouth_mid_x] = mouth_mid.x;
	posture.d[fidx_hip_y] = landmarks[24].y;

	tposture_C avg = average_sample(posture);

	const SDL_FRange& spine_length_range = ss_spine_length_range_;
	const SDL_Range& noperson_spine_angle_range = ss_noperson_spine_angle_range_;
	const bool mouth_right_of_shoulders = avg.d[fidx_mouth_mid_x] >= avg.d[fidx_right_shoulder_x] && avg.d[fidx_mouth_mid_x] >= avg.d[fidx_left_shoulder_x];
	const bool spine_not_too_short = avg.d[fidx_spine_length] >= spine_length_range.min;
	const bool spine_angle_in_range = avg.d[fidx_spine_angle] >= noperson_spine_angle_range.min && avg.d[fidx_spine_angle] <= noperson_spine_angle_range.max;
	if (!mouth_right_of_shoulders || !spine_not_too_short || !spine_angle_in_range) {
		result = analyze_noperson;

		SDL_snprintf(buf, sizeof(buf), "%i ms %s: %s/%i\n%s%s: %.3f>=(%.3f, %.3f)\n%s%s: %.3f(>=)%.3f\n%s%s: %.3f[%i, %i]\n:%s", 
			spent_ms_, sedentary_label_.c_str(), utils::format_elapse_hms(sedentary_elapse_s()).c_str(), sedentary_noperson_s(),
			mouth_right_of_shoulders? "": dismatch_str, sit_msgstr(sitmsg_mouth_right_of_shoulders).c_str(), mouth_mid.x, landmarks[11].x, landmarks[12].x,
			spine_not_too_short? "": dismatch_str, sit_msgstr(sitmsg_spine_not_too_short).c_str(), spine_length, spine_length_range.min,
			spine_angle_in_range? "": dismatch_str, sit_msgstr(sitmsg_spine_angle_in_range).c_str(), spine_angle, noperson_spine_angle_range.min, noperson_spine_angle_range.max,
			sit_msgstr(analyze_code_2_sitmsg_code(result)).c_str());

	} else {
		float hip_y_max = 0.70f;

		bool head_not_too_forward = avg.d[fidx_shoulder_nose_angle] >= ss_shoulder_nose_angle_threshold_;
		bool spine_not_too_bending = avg.d[fidx_spine_angle] >= ss_improper_spine_angle_range_.min && avg.d[fidx_spine_angle] <= ss_improper_spine_angle_range_.max;
		bool shoulder_width_not_too_broad = avg.d[fidx_shoulder_width] <= ss_shoulder_width_max_;
		bool spine_not_too_long = avg.d[fidx_spine_length] <= spine_length_range.max;
		bool hip_not_too_high = avg.d[fidx_hip_y] >= hip_y_max;
		result = analyze_posture_improper;

		if (!is_all_captured) {
			improper_msg = sit_msgstr(sitmsg_ss_missing);

		} else if (!head_not_too_forward) {
			improper_msg = sit_msgstr(sitmsg_head_not_too_forward);

		} else if (!spine_not_too_bending) {
			improper_msg = sit_msgstr(sitmsg_spine_not_too_bending);

		} else if (!shoulder_width_not_too_broad) {
			improper_msg = sit_msgstr(sitmsg_shoulder_width_not_too_broad);

		} else if (!spine_not_too_long) {
			improper_msg = sit_msgstr(sitmsg_spine_not_too_long);

		} else if (!hip_not_too_high) {
			improper_msg = sit_msgstr(sitmsg_hip_not_too_high);

		} else {
			// Good sitting posture
			result = analyze_posture_good;
		}

		if (result == analyze_posture_improper) {
			if (!is_all_captured) { improper_flags |= BIT_IDX_MASK(sitimproper_all_captured); }
			if (!head_not_too_forward) { improper_flags |= BIT_IDX_MASK(sitimproper_head_not_too_forward); }
			if (!spine_not_too_bending) { improper_flags |= BIT_IDX_MASK(sitimproper_spine_not_too_bending); }
			if (!shoulder_width_not_too_broad) { improper_flags |= BIT_IDX_MASK(sitimproper_shoulder_width_not_too_broad); }
			if (!spine_not_too_long) { improper_flags |= BIT_IDX_MASK(sitimproper_spine_not_too_long); }
			if (!hip_not_too_high) { improper_flags |= BIT_IDX_MASK(sitimproper_hip_not_too_high); }
		}

		if (is_all_captured) {
			SDL_snprintf(buf, sizeof(buf), "%i ms %s: %s/%i\n%s(%s)%s: %.3f(>=)%i\n%s(%s)%s: %.3f[%i, %i]\n%s(%s)%s: %.3f(<=)%.3f\n%s(%s)%s: %.3f(<=)%.3f\n%s(%s)%s: %.3f(>=)%.3f\n:%s", 
				spent_ms_, sedentary_label_.c_str(), utils::format_elapse_hms(sedentary_elapse_s()).c_str(), sedentary_noperson_s(),
				head_not_too_forward? "": dismatch_str, sit_msgstr(sitmsg_head_not_too_forward).c_str(), sit_msgstr(sitmsg_field_shoulder_nose_angle).c_str(), shoulder_nose_angle, ss_shoulder_nose_angle_threshold_,
				spine_not_too_bending? "": dismatch_str, sit_msgstr(sitmsg_spine_not_too_bending).c_str(), sit_msgstr(sitmsg_field_shoulder_hip_angle).c_str(), spine_angle, ss_improper_spine_angle_range_.min, ss_improper_spine_angle_range_.max,
				shoulder_width_not_too_broad? "": dismatch_str, sit_msgstr(sitmsg_shoulder_width_not_too_broad).c_str(), sit_msgstr(sitmsg_field_shoulder_width).c_str(), shoulder_width, ss_shoulder_width_max_,
				spine_not_too_long? "": dismatch_str, sit_msgstr(sitmsg_spine_not_too_long).c_str(), sit_msgstr(sitmsg_field_spine_length).c_str(), spine_length, spine_length_range.max,
				hip_not_too_high? "": dismatch_str, sit_msgstr(sitmsg_hip_not_too_high).c_str(), sit_msgstr(sitmsg_field_hip_y).c_str(), landmarks[24].y, hip_y_max,
				sit_msgstr(analyze_code_2_sitmsg_code(result)).c_str());

		} else {
			SDL_snprintf(buf, sizeof(buf), "%i ms %s: %s/%i\n%s\n:%s", 
				spent_ms_, sedentary_label_.c_str(), utils::format_elapse_hms(sedentary_elapse_s()).c_str(), sedentary_noperson_s(),
				sit_msgstr(sitmsg_ss_missing).c_str(), sit_msgstr(analyze_code_2_sitmsg_code(result)).c_str());
		}
	}

	VALIDATE(result >= 0 && result < analyze_count, null_str);
	overlay_msg = buf;

	return result;
}

void tkpose::latest_image_nopersion()
{
	if (noperson_start_ticks_ == 0) {
		noperson_start_ticks_ = SDL_GetTicks();
	}
	improper_start_ticks_ = 0;

	if (sedentary_start_ticks_ != 0) {
		if (sedentary_start_noperson_ticks_ == 0) {
			sedentary_start_noperson_ticks_ = SDL_GetTicks();
		}
	} else {
		VALIDATE(sedentary_start_noperson_ticks_ == 0, null_str);
	}
}

bool tkpose::slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects)
{
	int env_var_value = 0;
	const aplt::ttask_var* env_var = b_api_.aplt_get_env_var(var_type_for_code_);
	if (env_var != nullptr) {
		env_var_value = env_var->val.to_int();
	}

	if (type_ == type_sit_front || type_ == type_sit_lside || type_ == type_sit_rside) {
		slice_sit(env_var_value, result_str, classifier_rects);

	} else {
		VALIDATE(type_ == type_shoulder_neck, null_str);
		slice_shoulder_neck(env_var_value, result_str, classifier_rects);
	}

	return failed_;
}

void tkpose::slice_sit(int env_var_value, std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects)
{
	VALIDATE(type_ == type_sit_front || type_ == type_sit_lside || type_ == type_sit_rside, null_str);
	{

		threading::lock lock(variable_mutex_);

		if (env_var_value != 0) {

		} else if (new_frame_state_ == frame_ok) {
			std::string overlay_msg;
			std::string improper_msg;
			uint32_t improper_flags;
			// the latest image has 33kp.
			int analyze_result = nposm;
			if (type_ == type_sit_front) {
				analyze_result = sf_analyze_posture(xy_landmarks_, overlay_msg, improper_msg, improper_flags);
			} else {
				analyze_result = ss_analyze_posture(xy_landmarks_, type_ == type_sit_lside, overlay_msg, improper_msg, improper_flags);
			}
			if (!overlay_msg.empty()) {
				sn_last_msg_ = overlay_msg;
			}
			last_improper_msg_ = improper_msg;

			if (analyze_result == analyze_posture_good) {
				// Good sitting posture
				b_api_.health_push_n32_event(sitevttype_good, sitgood_new_one);
				if (!sitgood_new_one_sent_) {
					sitgood_new_one_sent_ = true;
				}

				improper_start_ticks_ = 0;

				noperson_start_ticks_ = 0;

				if (sedentary_start_ticks_ == 0) {
					sedentary_start_ticks_ = SDL_GetTicks();
				}
				sedentary_start_noperson_ticks_ = 0;

			} else if (analyze_result == analyze_posture_improper) {
				// Improper sitting posture! Please adjust your posture!
				// if now is in 'noperson', keep it.
				VALIDATE(improper_flags != 0, null_str);

				if (noperson_start_ticks_ == 0) {
					if (sitgood_new_one_sent_) {
						b_api_.health_push_n32_event(sitevttype_improper, improper_flags);
					}
					if (improper_start_ticks_ == 0) {
						improper_start_ticks_ = SDL_GetTicks();
					}
				} else {
					VALIDATE(improper_start_ticks_ == 0, null_str);
				}
				// noperson_start_ticks_ = 0;
				// sedentary_start_noperson_ticks_ = 0;

			} else {
				// Although there are 33 points, they can be error kp.
				VALIDATE(analyze_result == analyze_noperson, null_str);
				latest_image_nopersion();
			}

		} else if (new_frame_state_ == frame_fail) {
			// the latest image hasn't 33kp. think no person
			latest_image_nopersion();
		}
		new_frame_state_ = nposm;
	}

	// if (spent_ms_ != nposm && recv_samples_ != 0) {
	if (spent_ms_ != nposm) {
		// why requrie 'recv_samples_ != 0'? 
		// -- Even if 33kp isn't detected in the first frame, spent_ms_ will be set to 'not nposm'.
		//    now nose_x etc are float_nposm.
		result_str = sn_last_msg_;
	}

	enum {report_improper = 1, report_noperson, report_sedentary};
	config::attribute_value val;

	uint32_t now = SDL_GetTicks();
	if (noperson_start_ticks_ != 0) {
		VALIDATE(improper_start_ticks_ == 0, null_str);

		int noperson_threshold_ms = noperson_time_ * 1000;
		const int elapse_ms = now - noperson_start_ticks_;
		if (elapse_ms >= noperson_threshold_ms) {
			SDL_Log("%u {posture_report}noperson: %i ms", SDL_GetTicks(), elapse_ms);
			val.from_int64(report_noperson);
			b_api_.aplt_set_env_var(var_type_for_code_, val);
			noperson_start_ticks_ = 0;

			b_api_.health_push_n32_event(sitevttype_noperson, sitnoperson_alert);
		}
	}

	if (improper_start_ticks_ != 0) {
		VALIDATE(noperson_start_ticks_ == 0, null_str);

		int improper_threshold_ms = improper_time_ * 1000;
		const int elapse_ms = now - improper_start_ticks_;
		if (elapse_ms >= improper_threshold_ms) {
			SDL_Log("%u {posture_report}improper: %i ms", SDL_GetTicks(), elapse_ms);
			val.from_int64(report_improper);
			b_api_.aplt_set_env_var(var_type_for_code_, val);

			val.from_string(last_improper_msg_, true);
			b_api_.aplt_set_env_var(var_type_for_improper_msg_, val);
			improper_start_ticks_ = 0;

			b_api_.health_push_n32_event(sitevttype_improper, sitimproper_alert);
		}
	}

	if (sedentary_start_noperson_ticks_ != 0) {
		VALIDATE(sedentary_start_ticks_ != 0, null_str);
		int noperson_jittle_threshold = sedentary_noperson_threshold_ * 1000; // 1 min
		if (now >= sedentary_start_noperson_ticks_ + noperson_jittle_threshold) {
			clear_sedentary_ticks();
		}
	}

	if (sedentary_start_ticks_ != 0) {
		int threshold_ms = sedentary_time_ * 60 * 1000;
		if (game_config::os == os_windows) {
			// threshold_ms = 2 * 60 * 1000;
			int ii = 0;
		}
		if (now >= sedentary_start_ticks_ + threshold_ms && noperson_start_ticks_ == 0) {
			// If is in 'noperson' state, avoid entering into 'sedentary' state.
			// This 'noperson' state could potentially last more than @noperson_jittle_threshold.
			val.from_int64(report_sedentary);
			b_api_.aplt_set_env_var(var_type_for_code_, val);

			clear_sedentary_ticks();

			b_api_.health_push_n32_event(sitevttype_sedentary, sitsedentary_alert);
		}
	} else {
		VALIDATE(sedentary_start_noperson_ticks_ == 0, null_str);
	}
}

void check_raised_hands(const SDL_FPoint* landmarks, double& left_arm_angle, double& right_arm_angle)
{
	// Left arm angle detection
	const SDL_FPoint& l_shoulder = landmarks[11];
	const SDL_FPoint& l_elbow = landmarks[13];
	const SDL_FPoint& l_wrist = landmarks[15];
	double left_arm_rad = calculate_angle_3FPoint(l_shoulder, l_elbow, l_wrist);

	left_arm_angle = RAD2DEG(left_arm_rad);

	// Right arm angle detection
	const SDL_FPoint& r_shoulder = landmarks[12];
	const SDL_FPoint& r_elbow = landmarks[14];
	const SDL_FPoint& r_wrist = landmarks[16];
	double right_arm_rad = calculate_angle_3FPoint(r_shoulder, r_elbow, r_wrist);
	right_arm_angle = RAD2DEG(right_arm_rad);

	// SDL_Log("%u [right]12(%.3f, %.3f), 14(%.3f, %.3f), 16(%.3f, %.3f) => %.3f", 
	//	SDL_GetTicks(), 
	//	r_shoulder.x, r_shoulder.y, r_elbow.x, r_elbow.y, r_wrist.x, r_wrist.y, right_arm_angle);
}

bool tkpose::sn_analyze_section0(const SDL_FPoint* landmarks, std::string& msg)
{
	if (std::isnan(landmarks[11].x) ||
		std::isnan(landmarks[12].x) ||
		std::isnan(landmarks[13].x) ||
		std::isnan(landmarks[14].x) ||
		std::isnan(landmarks[15].x) ||
		std::isnan(landmarks[16].x)) {
		unsatisfied_msg_ = _("Have the camera capture shoulders, elbows, and wrists");
		return false;
	}

	const int desired[] = {11, 12, 13, 14, 15, 16};
	if (!is_landmarks_captured(landmarks, desired, sizeof(desired) / sizeof(desired[0]))) {
		unsatisfied_msg_ = _("Have the camera capture shoulders, elbows, and wrists");
		return false;
	}

	double left_arm_angle;
	double right_arm_angle;
	check_raised_hands(landmarks, left_arm_angle, right_arm_angle);

	// Mid point of shoulder
	SDL_FPoint shoulder_mid = mid_point(landmarks[11], landmarks[12]);

	float shoulder_width = std::abs(landmarks[11].x - landmarks[12].x);

	// Mid point of elbow
	// SDL_FPoint elbow_mid = mid_point(landmarks[13], landmarks[14]);

	// Mid point of wrist
	SDL_FPoint wrist_mid = mid_point(landmarks[15], landmarks[16]);
	float wrist_width = std::abs(landmarks[15].x - landmarks[16].x);

	tposture_C posture;
	posture.d[fidx_shoulder_mid_x] = shoulder_mid.x;
	posture.d[fidx_shoulder_width] = shoulder_width;
	posture.d[fidx_left_arm_angle] = left_arm_angle;
	posture.d[fidx_right_arm_angle] = right_arm_angle;
	posture.d[fidx_wrist_mid_x] = wrist_mid.x;
	posture.d[fidx_wrist_width] = wrist_width;

	tposture_C avg = average_sample(posture);

	float mid_x_diff = fabs(avg.d[fidx_shoulder_mid_x] - avg.d[fidx_wrist_mid_x]);
	float width_diff = fabs(avg.d[fidx_shoulder_width] - avg.d[fidx_wrist_width]);

	// Wrist height detection
	bool left_high_enough = landmarks[15].y < landmarks[11].y;
	bool right_high_enough = landmarks[16].y < landmarks[12].y;

	const float arm_angle_threshold = 145;
	const float mid_x_threahold = 0.05f;
	const float width_threahold = 0.10f;
	// bool satisfied = avg.d[fidx_left_arm_angle] >= arm_angle_threshold && avg.d[fidx_right_arm_angle] >= arm_angle_threshold &&
    //     (mid_x_diff <= mid_x_threahold) && (width_diff <= width_threahold) && left_high_enough && right_high_enough;
	utils::string_map symbols;
	bool satisfied = false;
	if (avg.d[fidx_left_arm_angle] < arm_angle_threshold) {
		symbols["lr"] = _("left");
		unsatisfied_msg_ = vgettext2("Raise $lr elbow more straight", symbols);

	} else if (avg.d[fidx_right_arm_angle] < arm_angle_threshold) {
		symbols["lr"] = _("right");
		unsatisfied_msg_ = vgettext2("Raise $lr elbow more straight", symbols);

	} else if (mid_x_diff > mid_x_threahold) {
		unsatisfied_msg_ = _("Place the centers of both wrists and shoulders on the same vertical line as much as possible");

	} else if (width_diff > width_threahold) {
		unsatisfied_msg_ = _("Cross your fingers and reduce the width formed by your wrists");

	} else if (!left_high_enough) {
		symbols["lr"] = _("left");
		unsatisfied_msg_ = vgettext2("The $lr wrist should be inside the $lr shoulder", symbols);

	} else if (!right_high_enough) {
		symbols["lr"] = _("right");
		unsatisfied_msg_ = vgettext2("The $lr wrist should be inside the $lr shoulder", symbols);

	} else {
		satisfied = true;
	}

	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "%i ms===section0===\nleft_angle: %.3f (>=)%.3f\nright_angle: %.3f (>=)%.3f\n[mid_x]shoulder: %.3f, wrist: %.3f, diff: %.3f (<=)%.3f\n[width]shoulder: %.3f, wrist: %.3f, diff: %.3f (<=)%.3f\n%s", 
		spent_ms_, left_arm_angle, arm_angle_threshold, right_arm_angle, arm_angle_threshold,
		shoulder_mid.x, wrist_mid.x, mid_x_diff, mid_x_threahold, 
		shoulder_width, wrist_width, width_diff, width_threahold, 
		satisfied? "satisfied": "fail");
	msg = buf;

	return satisfied;
}

bool tkpose::sn_analyze_section1(const SDL_FPoint* landmarks, std::string& msg)
{
	if (std::isnan(landmarks[11].x) ||
		std::isnan(landmarks[12].x) ||
		std::isnan(landmarks[13].x) ||
		std::isnan(landmarks[14].x) ||
		std::isnan(landmarks[15].x) ||
		std::isnan(landmarks[16].x)) {
		unsatisfied_msg_ = _("Have the camera capture shoulders, elbows, and wrists");
		return false;
	}

	const int desired[] = {11, 12, 13, 14, 15, 16};
	if (!is_landmarks_captured(landmarks, desired, sizeof(desired) / sizeof(desired[0]))) {
		unsatisfied_msg_ = _("Have the camera capture shoulders, elbows, and wrists");
		return false;
	}

	// Mid point of shoulder
	SDL_FPoint shoulder_mid = mid_point(landmarks[11], landmarks[12]);

	float head_forward = shoulder_mid.y - landmarks[0].y;

	float left_shoulder_wrist_diff_xy = fabs(landmarks[11].x - landmarks[16].x) + fabs(landmarks[11].y - landmarks[16].y);
	float right_shoulder_wrist_diff_xy = fabs(landmarks[12].x - landmarks[15].x) + fabs(landmarks[12].y - landmarks[15].y);

	tposture_C posture;
	posture.d[fidx_head_forward] = head_forward;
	posture.d[fidx_left_shoulder_wrist_diff_xy] = left_shoulder_wrist_diff_xy;
	posture.d[fidx_right_shoulder_wrist_diff_xy] = right_shoulder_wrist_diff_xy;

	tposture_C avg = average_sample(posture);

	const float head_forward_threshold = 0.16f;
	const float diff_xy_threahold = 0.16f;
	// bool satisfied = avg.d[fidx_head_forward] <= head_forward_threshold &&
    //     avg.d[fidx_left_shoulder_wrist_diff_xy] <= diff_xy_threahold && avg.d[fidx_right_shoulder_wrist_diff_xy] <= diff_xy_threahold;
	bool satisfied = false;
	if (avg.d[fidx_head_forward] > head_forward_threshold) {
		unsatisfied_msg_ = _("Lower your head a bit");

	} else if (avg.d[fidx_left_shoulder_wrist_diff_xy] > diff_xy_threahold) {
		unsatisfied_msg_ = _("Bring the right wrist closer to the left shoulder");

	} else if (avg.d[fidx_right_shoulder_wrist_diff_xy] > diff_xy_threahold) {
		unsatisfied_msg_ = _("Bring the left wrist closer to the right shoulder");

	} else {
		satisfied = true;
	}


	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "%i ms===section1===\nhead_forward: %.3f (<=)%.3f\nleft diff_xy: %.3f (<=)%.3f\nright diff_xy: %.3f (<=)%.3f\n%s", 
		spent_ms_, head_forward, head_forward_threshold,
		left_shoulder_wrist_diff_xy, diff_xy_threahold,
		right_shoulder_wrist_diff_xy, diff_xy_threahold,
		satisfied? "satisfied": "fail");
	msg = buf;

	return satisfied;
}

bool tkpose::sn_analyze_section23(const SDL_FPoint* landmarks, bool first, std::string& msg)
{
	utils::string_map symbols;
	symbols["lr"] = first? _("right"): _("left");
	if (std::isnan(landmarks[11].x) ||
		std::isnan(landmarks[12].x) ||
		std::isnan(landmarks[13].x) ||
		std::isnan(landmarks[14].x) ||
		std::isnan(landmarks[15].x) ||
		std::isnan(landmarks[16].x)) {
		unsatisfied_msg_ = vgettext2("Have the camera capture $lr shoulder, elbow, and wrist", symbols);
		return false;
	}

	int desired[] = {12, 14, 16};
	if (!first) {
		desired[0] = 11;
		desired[1] = 13;
		desired[2] = 15;
	}
	if (!is_landmarks_captured(landmarks, desired, sizeof(desired) / sizeof(desired[0]))) {
		unsatisfied_msg_ = vgettext2("Have the camera capture $lr shoulder, elbow, and wrist", symbols);
		return false;
	}

	float ear_diff_y = std::abs(landmarks[7].y - landmarks[8].y);

	// Mid point of shoulder
	double left_arm_angle;
	double right_arm_angle;
	check_raised_hands(landmarks, left_arm_angle, right_arm_angle);

	bool elbow_outof_shoulder = first? landmarks[14].x < landmarks[12].x: landmarks[13].x > landmarks[11].x;

	tposture_C posture;
	posture.d[fidx_ear_diff_y] = ear_diff_y;
	posture.d[fidx_left_arm_angle] = left_arm_angle;
	posture.d[fidx_right_arm_angle] = right_arm_angle;

	tposture_C avg = average_sample(posture);

	const float arm_angle = first? posture.d[fidx_right_arm_angle]: posture.d[fidx_left_arm_angle];

	const float ear_diff_threshold = 0.015f;
	const SDL_FRange arm_angle_threshold{60, 110};
	// bool satisfied = avg.d[fidx_ear_diff_y] >= ear_diff_threshold && 
	//	arm_angle >= arm_angle_threshold.min && arm_angle <= arm_angle_threshold.max && elbow_outof_shoulder;
	bool satisfied = false;
	if (!elbow_outof_shoulder) {
		unsatisfied_msg_ = _("The elbow should be outside the shoulder");

	} else if (avg.d[fidx_ear_diff_y] < ear_diff_threshold) {
		unsatisfied_msg_ = _("Make both ears tilt more");

	} else if (arm_angle < arm_angle_threshold.min) {
		unsatisfied_msg_ = _("Increase elbow angle");

	} else if (arm_angle > arm_angle_threshold.max) {
		unsatisfied_msg_ = _("Reduce elbow angle");

	} else {
		satisfied = true;
	}

	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "%i ms===section%i===\near_diff: %.3f (>=)%.3f\narm angle: %.3f (in)[%.3f, %.3f]\nelbow_outof_shoulder: %s\n%s", 
		spent_ms_, first? 2: 3, ear_diff_y, ear_diff_threshold,
		arm_angle, arm_angle_threshold.min, arm_angle_threshold.max,
		elbow_outof_shoulder? "true": "false",
		satisfied? "satisfied": "fail");
	msg = buf;

	return satisfied;
}

bool tkpose::sn_analyze_section4(const SDL_FPoint* landmarks, std::string& msg)
{
	if (std::isnan(landmarks[11].x) ||
		std::isnan(landmarks[12].x) ||
		std::isnan(landmarks[13].x) ||
		std::isnan(landmarks[14].x) ||
		std::isnan(landmarks[15].x) ||
		std::isnan(landmarks[16].x)) {
		unsatisfied_msg_ = _("Have the camera capture shoulders, elbows, and wrists");
		return false;
	}

	const int desired[] = {11, 12, 13, 14, 15, 16};
	if (!is_landmarks_captured(landmarks, desired, sizeof(desired) / sizeof(desired[0]))) {
		unsatisfied_msg_ = _("Have the camera capture shoulders, elbows, and wrists");
		return false;
	}

	// Is the head tilted forward (comparing the Y-axis of the nose and shoulders)
	// Mid point of shoulder
	SDL_FPoint shoulder_mid = mid_point(landmarks[11], landmarks[12]);

	float head_forward = shoulder_mid.y - landmarks[0].y;

	double left_arm_angle;
	double right_arm_angle;
	check_raised_hands(landmarks, left_arm_angle, right_arm_angle);

	bool elbow_outof_shoulder = landmarks[14].x < landmarks[12].x && landmarks[13].x > landmarks[11].x;

	tposture_C posture;
	posture.d[fidx_head_forward] = head_forward;
	posture.d[fidx_left_arm_angle] = left_arm_angle;
	posture.d[fidx_right_arm_angle] = right_arm_angle;

	tposture_C avg = average_sample(posture);

	const float head_forward_threshold = 0.05f; // nose.y: the more you tilt back, the more inaccurate it becomes.
	const SDL_FRange arm_angle_threshold{15, 65};
	// bool satisfied = avg.d[fidx_head_forward] >= head_forward_threshold && 
	//	left_arm_angle >= arm_angle_threshold.min && left_arm_angle <= arm_angle_threshold.max && 
	//	right_arm_angle >= arm_angle_threshold.min && right_arm_angle <= arm_angle_threshold.max && 
	//	elbow_outof_shoulder;
	utils::string_map symbols;
	bool satisfied = false;
	if (avg.d[fidx_head_forward] < head_forward_threshold) {
		unsatisfied_msg_ = _("Tilt your head upwards");

	} else if (left_arm_angle < arm_angle_threshold.min) {
		symbols["lr"] = _("left");
		symbols["inc_red"] = _("increase");
		unsatisfied_msg_ = vgettext2("Place $lr elbow on the back of neck, unfold it, and $inc_red the angle", symbols);

	} else if (left_arm_angle > arm_angle_threshold.max) {
		symbols["lr"] = _("left");
		symbols["inc_red"] = _("reduce");
		unsatisfied_msg_ = vgettext2("Place $lr elbow on the back of neck, unfold it, and $inc_red the angle", symbols);

	} else if (right_arm_angle < arm_angle_threshold.min) {
		symbols["lr"] = _("right");
		symbols["inc_red"] = _("increase");
		unsatisfied_msg_ = vgettext2("Place $lr elbow on the back of neck, unfold it, and $inc_red the angle", symbols);

	} else if (right_arm_angle > arm_angle_threshold.max) {
		symbols["lr"] = _("right");
		symbols["inc_red"] = _("reduce");
		unsatisfied_msg_ = vgettext2("Place $lr elbow on the back of neck, unfold it, and $inc_red the angle", symbols);

	} else if (!elbow_outof_shoulder) {
		unsatisfied_msg_ = _("The elbow should be outside the shoulder");

	} else {
		satisfied = true;
	}

	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "%i ms===section4===\nhead_forward: %.3f (>=)%.3f\nleft/right arm angle: %.3f/%.3f (in)[%.3f, %.3f]\nelbow_outof_shoulder: %s\n%s", 
		spent_ms_, head_forward, head_forward_threshold,
		left_arm_angle, right_arm_angle, arm_angle_threshold.min, arm_angle_threshold.max,
		elbow_outof_shoulder? "true": "false",
		satisfied? "satisfied": "fail");
	msg = buf;

	return satisfied;
}

void tkpose::sn_set_countdown(int _countdown_s)
{
	VALIDATE(sn_desire_request_ticks_ == 0, null_str);
	VALIDATE(sn_last_speak_s_ == nposm, null_str);

	sn_desire_request_ticks_ = SDL_GetTicks() + _countdown_s * 1000;

	// why initial set '_countdown_s + 1'? avoid speak '_countdown_s + 1'.
	// see 'int integer = diff / 1000 + 1' in slice(). 
	sn_last_speak_s_ = _countdown_s + 1;

	// Start speaking the countdown count as soon as possible.
	pinyin_.speak(null_str);
}

void tkpose::sn_clear_countdown()
{
	VALIDATE(sn_desire_request_ticks_ != 0, null_str);
	VALIDATE(sn_last_speak_s_ != nposm, null_str);

	sn_desire_request_ticks_ = 0;
	sn_last_speak_s_ = nposm;
}

void tkpose::sn_countdown()
{
	uint32_t now = SDL_GetTicks();
	if (sn_desire_request_ticks_ > now) {
		int diff = sn_desire_request_ticks_ - now;
		int integer = diff / 1000 + 1;

		if (integer != sn_last_speak_s_) {
			if (integer == sn_remind_breath_s_) {
				pinyin_.speak(_("sn^remind breath"));
			}
			if (!pinyin_.is_speaking()) {
				pinyin_.speak(str_cast(integer));
			}
			sn_last_speak_s_ = integer;
		}

	} else {
		sn_clear_countdown();
	}
}

void tkpose::sn_update_next_unsatisfied_ticks()
{
	int s_threshold = sn_unsatisfied_s_threshold_;
	if (sn_next_unsatisfied_ticks_ != 0) {
		// second or more, use larger threshold.
		s_threshold = sn_2th_unsatisfied_s_threshold_;
	}
	sn_next_unsatisfied_ticks_ = SDL_GetTicks() + s_threshold * 1000;
}

void tkpose::sn_speak_unsatisfied_msg()
{
	if (unsatisfied_msg_.empty()) {
		pinyin_.speak(sn_section_py_text(sn_section_, pytype_unsatisfied));
	} else {
		pinyin_.speak(unsatisfied_msg_);
	}
	sn_update_next_unsatisfied_ticks();
}

void tkpose::sn_update_next_remind_ticks()
{
	sn_next_remind_ticks_ = SDL_GetTicks() + sn_remind_s_threshold_ * 1000;
}

void tkpose::sn_update_next_repeat_finished_ticks()
{
	sn_next_repeat_finished_ticks_ = SDL_GetTicks() + sn_repeat_finished_s_ * 1000;
}

void tkpose::slice_shoulder_neck(int env_var_value, std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects)
{
	if (sn_section_ == nposm) {
		pinyin_.speak(_("sn^prelude"));
		const int first_sect = snsect_0prelude; // snsect_0prelude
		// const int first_sect = snsect_4prelude; // snsect_0prelude
		VALIDATE(snsect_is_prelude(first_sect), null_str);
		sn_section_ = first_sect;

	} else if (snsect_is_prelude(sn_section_)) {
		if (!pinyin_.is_speaking()) {
			sn_section_ += 1; // snsect_0desc, snsect_1desc
			pinyin_.speak(sn_section_py_text(sn_section_, pytype_desc));
		}

	} else if (snsect_is_desc(sn_section_)) {
		if (!pinyin_.is_speaking()) {
			sn_section_ += 1; // snsect_0_20, snsect_1_20
			VALIDATE(unsatisfied_msg_.empty(), null_str);
			VALIDATE(sn_next_unsatisfied_ticks_ == 0, null_str);
			VALIDATE(sn_next_remind_ticks_ == 0, null_str);

			sn_update_next_unsatisfied_ticks();
			sn_update_next_remind_ticks();
		}

	} else if (snsect_is_countdown(sn_section_)) {
		std::string msg;
		{
			threading::lock lock(variable_mutex_);

			if (env_var_value != 0) {

			} else if (new_frame_state_ == frame_ok) {
				// the latest image has 33kp.
				bool satisfied = false;
				if (sn_section_ == snsect_0_20) {
					satisfied = sn_analyze_section0(xy_landmarks_, msg);

				} else if (sn_section_ == snsect_1_20) {
					VALIDATE(sn_section_ == snsect_1_20, null_str);;
					satisfied = sn_analyze_section1(xy_landmarks_, msg);

				} else if (sn_section_ == snsect_2_20 || sn_section_ == snsect_3_20) {
					satisfied = sn_analyze_section23(xy_landmarks_, sn_section_ == snsect_2_20, msg);

				} else {
					VALIDATE(sn_section_ == snsect_4_20, null_str);
					satisfied = sn_analyze_section4(xy_landmarks_, msg);
				}

				if (!msg.empty()) {
					sn_last_msg_ = msg;
				}
				if (satisfied) {
					if (!workout_start_sent_) {
						// ros_.health_push_n32_event(workoutevt_n32, workoutn32_start);
						workout_start_sent_ = true;
					}
					sn_last_satisfied_ticks_ = SDL_GetTicks();
					// sn_update_next_remind_ticks();
					sn_next_unsatisfied_ticks_ = 0;
					sn_next_remind_ticks_ = 0;
					if (sn_desire_request_ticks_ == 0) {
						sn_set_countdown(sn_countdown_s_);
					}
				}

			} else if (new_frame_state_ == frame_fail) {
				// the latest image hasn't 33kp. think no person
			}
			new_frame_state_ = nposm;
		}

		if (spent_ms_ != nposm && recv_samples_ != 0) {
			result_str = sn_last_msg_;
		}

		if (sn_desire_request_ticks_ != 0) {
			VALIDATE(sn_last_satisfied_ticks_ != 0, null_str);
			VALIDATE(sn_next_unsatisfied_ticks_ == 0, null_str);
			VALIDATE(sn_next_remind_ticks_ == 0, null_str);
			if ((int)(SDL_GetTicks() - sn_last_satisfied_ticks_) <= sn_unsatisfied_s_threshold_ * 1000) {
				sn_countdown();
				if (sn_desire_request_ticks_ == 0) {
					pinyin_.speak(sn_section_py_text(sn_section_, pytype_finished));
					sn_section_ += 1; // snsect_1prelude, snsect_2prelude

					VALIDATE(sn_next_repeat_finished_ticks_ == 0, null_str);
					if (sn_section_ == snsect_finished) {
						// ros_.health_push_n32_event(workoutevt_n32, workoutn32_end);
						sn_update_next_repeat_finished_ticks();
					}

					unsatisfied_msg_.clear();
					sn_next_unsatisfied_ticks_ = 0;
					sn_next_remind_ticks_ = 0;
				}
			} else {
				sn_speak_unsatisfied_msg();

				sn_clear_countdown();
				sn_update_next_remind_ticks();
			}

		} else if (SDL_GetTicks() > sn_next_remind_ticks_) {
			VALIDATE(sn_next_remind_ticks_ != 0, null_str);
			pinyin_.speak(sn_section_py_text(sn_section_, pytype_remind));
			sn_update_next_remind_ticks();
			sn_update_next_unsatisfied_ticks();

		} else if (!pinyin_.is_speaking() && SDL_GetTicks() > sn_next_unsatisfied_ticks_) {
			VALIDATE(sn_next_unsatisfied_ticks_ != 0, null_str);
			sn_speak_unsatisfied_msg();
		}

	} else {
		VALIDATE(sn_section_ == snsect_finished, null_str);
		VALIDATE(sn_next_repeat_finished_ticks_ != 0, null_str);
		if (SDL_GetTicks() >= sn_next_repeat_finished_ticks_) {
			sn_update_next_repeat_finished_ticks();
			pinyin_.speak(sn_section_py_text(snsect_4_20, pytype_finished));
		}
	}
}

std::string tkpose::sn_section_py_text(int section, int type) const
{
	int64_t key = make_sn_py_text_key(section, type);
	VALIDATE(sn_py_texts_.count(key) != 0, null_str);
	return sn_py_texts_.find(key)->second;
}

bool tkpose::camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb)
{
	threading::lock lock(variable_mutex_);

	cv_argb = argb.clone();
	bool overlay = frame_ok_received_;
	if (new_frame_state_ == frame_ok) {
		

	} else if (new_frame_state_ == frame_fail) {
		overlay = false;
	}

	if (overlay) {
		mediapipe::overlay_pose_landmarks(xy_landmarks_, mediapipe::kNumPoseLandmarks, flip_h_, cv_argb, nullptr);
	}
	return true;
}

void tkpose::camera_work_frame(const surface& surf, const cv::Mat& argb)
{
	SDL_FPoint xy_landmarks[mediapipe::kNumPoseLandmarks];

	VALIDATE(api_ptr_.get() != nullptr, null_str);

	mediapipe::tpose_tracking_api& api = *api_ptr_.get();
	uint32_t start_ticks = SDL_GetTicks();
	bool is_less_than_min_interval = false;
	cv::Mat output_frame_mat = api.next_image(argb, flip_h_, xy_landmarks, is_less_than_min_interval);
	if (is_less_than_min_interval) {
		return;
	}
	int spent_ms = SDL_GetTicks() - start_ticks;

	// SDL_Log("%u, pose api.next_image, output_mat: (%i x %i), spent %i ms", SDL_GetTicks(), output_frame_mat.cols, output_frame_mat.rows, spent_ms);

	if (!output_frame_mat.empty()) {
		threading::lock lock(variable_mutex_);
		VALIDATE(output_frame_mat.u != nullptr, null_str);

		memcpy(xy_landmarks_, xy_landmarks, sizeof(xy_landmarks));
		spent_ms_ = spent_ms;

		output_mat_ = output_frame_mat;
		new_frame_state_ = frame_ok;

		if (!frame_ok_received_) {
			frame_ok_received_ = true;
		}

	} else {
		failed_ = true;
		new_frame_state_ = frame_fail;
	}
}

void tkpose::fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id)
{
	if (cpp_id == cpp_id_save_sf_6fields || cpp_id == cpp_id_save_ss_4fields || cpp_id == cpp_id_save_sshare_3fields) {
		threading::lock lock(variable_mutex_);
		reload_pref_fields(cpp_id);
	}
}

float get_float_field2(trose_prefs& aplt_prefs, const std::string& key, const SDL_FRange& range, float def)
{
	float val = aplt_prefs.get_double(key, def);
	if (val < range.min || val > range.max) {
		val = def;
	}
	return val;
}

int get_int_field2(trose_prefs& aplt_prefs, const std::string& key, const SDL_Range& range, int def)
{
	int val = aplt_prefs.get_int(key, def);
	if (val < range.min || val > range.max) {
		val = def;
	}
	return val;
}

void tkpose::reload_pref_fields(int cpp_id)
{
	const SDL_FRange ratio_range{0.05f, 0.95f};
	const SDL_Range time_range{5, 1000};
	const SDL_Range angle_range{10, 350};
	const SDL_Range time_range5_90{5, 90};

	trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;
	if (cpp_id == cpp_id_save_sf_6fields) {
		spine_x_range_.min = get_float_field2(aplt_prefs, "sf_spine_x_min", ratio_range, DEF_SPINE_X_RANGE_MIN);
		spine_x_range_.max = get_float_field2(aplt_prefs, "sf_spine_x_max", ratio_range, DEF_SPINE_X_RANGE_MAX);
		shoulder_width_range_.min = get_float_field2(aplt_prefs, "sf_shoulder_width_min", ratio_range, DEF_SHOULDER_WIDTH_RANGE_MIN);
		shoulder_width_range_.max = get_float_field2(aplt_prefs, "sf_shoulder_width_max", ratio_range, DEF_SHOULDER_WIDTH_RANGE_MAX);
		ear_width_min_ = get_float_field2(aplt_prefs, "sf_ear_width_min", ratio_range, DEF_EAR_WIDTH_MIN);
		ear_diff_threshold_ = get_float_field2(aplt_prefs, "sf_ear_diff_threshold", ratio_range, DEF_EAR_DIFF_THRESHOLD);
		head_forward_threshold_ = get_float_field2(aplt_prefs, "sf_head_forward_threshold", ratio_range, DEF_HEAD_FORWARD_THRESHOLD);

	} else if (cpp_id == cpp_id_save_ss_4fields) {
		ss_spine_length_range_.min = get_float_field2(aplt_prefs, "ss_spine_length_min", ratio_range, DEF_SS_SPINE_LENGTH_MIN);
		ss_spine_length_range_.max = get_float_field2(aplt_prefs, "ss_spine_length_max", ratio_range, DEF_SS_SPINE_LENGTH_MAX);
		ss_shoulder_width_max_ = get_float_field2(aplt_prefs, "ss_shoulder_width_max", ratio_range, DEF_SS_SHOULDER_WIDTH_MAX);
		ss_improper_spine_angle_range_.min = get_int_field2(aplt_prefs, "ss_improper_spine_angle_min", angle_range, DEF_SS_IMPROPER_SPINE_ANGLE_MIN);
		ss_improper_spine_angle_range_.max = get_int_field2(aplt_prefs, "ss_improper_spine_angle_max", angle_range, DEF_SS_IMPROPER_SPINE_ANGLE_MAX);
		ss_shoulder_nose_angle_threshold_ = get_int_field2(aplt_prefs, "ss_shoulder_nose_angle_threshold", angle_range, DEF_SS_SHOULDER_NOSE_ANGLE_THRESHOLD);

		ss_noperson_spine_angle_range_.min = get_int_field2(aplt_prefs, "ss_noperson_spine_angle_min", angle_range, DEF_SS_NOPERSON_SPINE_ANGLE_MIN);
		ss_noperson_spine_angle_range_.max = get_int_field2(aplt_prefs, "ss_noperson_spine_angle_max", angle_range, DEF_SS_NOPERSON_SPINE_ANGLE_MAX);

	} else if (cpp_id == cpp_id_save_sshare_3fields) {	
		improper_time_ = get_int_field2(aplt_prefs, "sshare_improper_time", time_range, DEF_IMPROPER_TIME);
		noperson_time_ = get_int_field2(aplt_prefs, "sshare_noperson_time", time_range, DEF_NOPERSON_TIME);
		sedentary_time_ = get_int_field2(aplt_prefs, "sshare_sedentary_time", time_range, DEF_SEDENTARY_TIME);
		sedentary_noperson_threshold_ = get_int_field2(aplt_prefs, "sshare_sedentary_noperson_threshold", time_range5_90, DEF_SEDENTARY_NOPERSON_JITTLE_THRESHOLD);
	}
}

void tkpose::clear_kpose()
{
	type_ = nposm;
	api_ptr_.reset();
	spent_ms_ = nposm;
	output_mat_ = cv::Mat();
	improper_start_ticks_ = 0;
	noperson_start_ticks_ = 0;
	sedentary_start_ticks_ = 0;
	sedentary_start_noperson_ticks_ = 0;
	failed_ = false;

	new_frame_state_ = nposm;
	frame_ok_received_ = false;
	// analyze_result_ = nposm;
	last_improper_msg_.clear();

	reset_posture_samples();

	sn_section_ = nposm;
	sn_last_msg_.clear();
	sn_desire_request_ticks_ = 0;
	sn_last_speak_s_ = nposm;
	sn_last_satisfied_ticks_ = 0;
	sn_next_unsatisfied_ticks_ = 0;
	sn_next_remind_ticks_ = 0;
	sn_next_repeat_finished_ticks_ = 0;
}

void tkpose::validate_nposm() const
{
	VALIDATE(type_ == nposm, null_str);
	VALIDATE(api_ptr_.get() == nullptr, null_str);
	VALIDATE(spent_ms_ == nposm, null_str);
	VALIDATE(output_mat_.empty(), null_str);
	VALIDATE(improper_start_ticks_ == 0, null_str);
	VALIDATE(noperson_start_ticks_ == 0, null_str);
	VALIDATE(sedentary_start_ticks_ == 0, null_str);
	VALIDATE(sedentary_start_noperson_ticks_ == 0, null_str);
	VALIDATE(!failed_, null_str);

	VALIDATE(new_frame_state_ == nposm, null_str);
	// VALIDATE(analyze_result_ == nposm, null_str);
	VALIDATE(last_improper_msg_.empty(), null_str);

	VALIDATE(sn_section_ == nposm, null_str);
	VALIDATE(sn_last_msg_.empty(), null_str);
	VALIDATE(sn_desire_request_ticks_ == 0, null_str);
	VALIDATE(sn_last_speak_s_ == nposm, null_str);
	VALIDATE(sn_last_satisfied_ticks_ == 0, null_str);
	VALIDATE(sn_next_unsatisfied_ticks_ == 0, null_str);
	VALIDATE(sn_next_remind_ticks_ == 0, null_str);
	VALIDATE(sn_next_repeat_finished_ticks_ == 0, null_str);
}

} // namespace aplt