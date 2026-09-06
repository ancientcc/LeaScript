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

#define GETTEXT_DOMAIN "rose-lib"

#include "wkocamera_task.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include "rose_sdl_utils.hpp"
#include "rose_font.hpp"


// using namespace std::placeholders;


namespace aplt {

//
// twkocamera_task
//

twkocamera_task::twkocamera_task(thelper_lua_camera& lua_camera, tapplet& aplt, const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
	: thelper_camera_task_slot(lua_camera, aplt, cfg_task, task_vars)
	, rpy_sensor_(get_rpy_sensor())
	, imwrite_interval_s_(60)
	, var_name_timestamp_(utils::join_app_prefix_id(aplt_.bundleid, "timestamp"))
	, var_name_png_(utils::join_app_prefix_id(aplt_.bundleid, "png"))
	, var_name_count_(utils::join_app_prefix_id(aplt_.bundleid, "count"))
	, var_name_not_send_(utils::join_app_prefix_id(aplt_.bundleid, "not_send"))
	, var_type_for_code_(var_env_basesubtask_code)
	, flip_h_(false)
	// , reset_samples_threshold_ms_(5000) // 5s
	, required_landmarks_{true, false, true, false, false, true, false, true, true, false, false, // 0 - 10
		true, true, true, true ,true, true, false, false, false, false, false, false, // 11 - 22
		true, true, true, true, false, false, false, false, false, false // 23 - 33
	}
	, depended_landmarks_{nposm, nposm, nposm, nposm, nposm, nposm, nposm, nposm, nposm, nposm, nposm, // 0 - 10
		nposm, nposm, nposm, nposm, nposm, nposm, nposm, nposm, nposm, nposm, nposm, nposm, // 11 - 22
		nposm, nposm, nposm, nposm, 25, 26, nposm, nposm, nposm, nposm // 23 - 33
	}
	, curr_state2_(nullptr)
	, tasks_suspended_(false)
	, workout_first_satisfied_ticks_(0)
	, workout_first_satisfied_ts_(0)
	, vertical_check_threshold_ms_(15000)
	, next_vertical_check_ticks_(0)
	, slot_(nullptr)
{
	VALIDATE(MAX_ANTI_SHAKE_SAMPLES >= 3, null_str);

	clear_kpose();
}

std::string twkocamera_task::start_task()
{
	std::string err_msg;
	utils::string_map symbols;
	validate_nposm();

	const std::string var_name_file = utils::join_app_prefix_id(aplt_.bundleid, "file");
	if (task_vars_.existed(var_name_file)) {
		const std::string val = task_vars_.get_string(var_name_file);
/*
		std::string filename = aplt_.preferences_dir + "/wkoscript/" + val;
		script_.from_file(filename);
		if (!script_.valid()) {
			filename = aplt_.res_path + "/wkoscript/" + val;
			script_.from_file(filename);
		}
		if (!script_.valid()) {
			return i18n::freq_msgstr_3str(i18n::msgid_load_file_fail, _("Action script"), filename, null_str);
		}
*/
		std::string err_msg = script_.from_aplt_file(aplt_, val);
		if (!script_.valid()) {
			return err_msg;
		}
		const std::string& filename = err_msg;
		SDL_Log("start wkoscript task from: %s, states: %i", filename.c_str(), (int)script_.states.size());

	} else {
		symbols["var"] = utils::split_app_prefix_id(var_name_file).second;
		return vgettext2("Input var($var)'s value cannot be empty", symbols);
	}
	

	script_.enable_run();

	api_ptr_.reset(mediapipe::rose_create_pose_tracking_api());
	if (!api_ptr_->graph_initialized()) {
		err_msg = _("Initialize mediapipe graph fail");
	} else {
		// if (game_config::os == os_windows) {
			// compare to 'is_cpu_saver', -10 ms
			api_ptr_->set_min_next_interval_no_cpu_saver(MEDIAPIPE_POSTURE_MIN_INTERVAL - 10);
		// }
	}

	script_task_vars_ = clone_env_vars(false);
	curr_state2_ = &script_.states.find(0)->second;
	b_api_.health_push_str_event(workoutevt_str, workoutstr_start, script_.cfg_str, script_.id);
	// It is starting up and is not suitable for executing task operations. 
	// For example, if the speak will stopped later, the 'task_speak' task will become invalid.
	tasks_suspended_ = true;

	return err_msg;
}

void twkocamera_task::task_finished(const tapplet::ttask& cfg_task)
{
	if (curr_state2_ != nullptr) {
		// Whether to call this here is questionable, 
		// because some tasks may not be suitable at this moment ¡ª for example, the camera was already be 'camera_.exit_task'. 
		// Whether to keep this in the end depends on whether the tasks could cause issues.
		curr_state2_->did_exit_state();
	}
	clear_kpose();
}

void twkocamera_task::enter_state(twkoscript::tstate2& state2)
{
	b_api_.health_push_n32_event(workoutevt_n32, state2.state);
	state2.did_enter_state();
	// script_.reset_anti_shike_samples(posture_fields);
}

bool twkocamera_task::slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects)
{
	int env_var_value = 0;
	const aplt::ttask_var* env_var = b_api_.aplt_get_env_var(var_type_for_code_);
	if (env_var != nullptr) {
		env_var_value = env_var->val.to_int();
	}

	VALIDATE(curr_state2_ != nullptr, null_str);
	twkoscript::tstate2& state2 = *curr_state2_;

	if (tasks_suspended_) {
		VALIDATE(state2.state == 0, null_str);
		tasks_suspended_ = false;
		enter_state(state2);
	}

	if (state2.track_pose.valid()) {
		std::string overlay_msg;
		{
			threading::lock lock(variable_mutex_);

			twko_analyze_result_C analyze_result = {0};
			if (env_var_value != 0) {

			} else if (new_frame_state_ == frame_ok) {
				// the latest image has 33kp.

				const int unsatisfied_reason = script_.analyze_track_pose(state2, xy_landmarks_, inference_ms_, unsatisfied_msg_, overlay_msg, &analyze_result);
				VALIDATE(wkon32_ctx_is_analyze_track_pose_result(unsatisfied_reason), null_str);

				const bool satisfied = unsatisfied_reason == workoutn32_satisfied_landmarks;

				if (slot_ != nullptr) {
					slot_->did_analyze_result(analyze_result);
				}

				if (!overlay_msg.empty()) {
					// If 'overlay_msg' is not generated this time, display the 'overlay_msg' from the last time.
					if (slot_ == nullptr || slot_->is_overlay_analyze_msg()) {
						sn_last_msg_ = overlay_msg;
					} else {
						sn_last_msg_.clear();
					}
				}

				if (satisfied) {
					if (workout_first_satisfied_ticks_ == 0 && !state2.is_setup) {
						VALIDATE(workout_first_satisfied_ts_ == 0, null_str);
						b_api_.health_push_n32_event(workoutevt_n32, workoutn32_first_satisfied);
						workout_first_satisfied_ticks_ = SDL_GetTicks();
						workout_first_satisfied_ts_ = time(nullptr);
						if (slot_ != nullptr) {
							slot_->did_first_satisfied_frame(workout_first_satisfied_ticks_, workout_first_satisfied_ts_);
						}

						VALIDATE(next_vertical_check_ticks_ == 0, null_str);
						// Check immediately.
						// why has 'delay_ms'? 
						// There will be a lot of speaking at this moment, so wait a few seconds before announcing. 
						// This prevents it from being preempted by other speaks.
						const int delay_ms = SDL_min(5000, vertical_check_threshold_ms_);
						next_vertical_check_ticks_ = SDL_GetTicks() + delay_ms;
					}
				}

				{
					float bonus = 0.0f; // 0.05f
					float min = bonus;
					float max = 1.0f - bonus;

					uint16_t tmp_u16;
					bool valid = false;
					for (int at = 0; at < mediapipe::kNumPoseLandmarks; at ++) {
						const SDL_FPoint& l = xy_landmarks_[at];
						valid = false;

						if (std::isnan(l.x) || std::isnan(l.y)) {
							int ii = 0;

						} else if (required_landmarks_[at]) {
							valid = true;

						} else if (depended_landmarks_[at] != nposm) {
							const SDL_FPoint& depended = xy_landmarks_[depended_landmarks_[at]];
							if (depended.x >= min && depended.x <= max && depended.y >= min && depended.y <= max) {
								valid = true;
							}
						}

						SDL_U16Point& dest = u16landmarks_[at];
						if (valid) {
							tmp_u16 = float_2_u16::quantize_n05p15(l.x);
							dest.x = tmp_u16 != rose_u16_nan? tmp_u16: 1;

							tmp_u16 = float_2_u16::quantize_n05p15(l.y);
							dest.y = tmp_u16 != rose_u16_nan? tmp_u16: 1;

						} else {
							dest.x = rose_u16_nan;
							dest.y = rose_u16_nan;
						}
					}

					const bool enable_validate = true;
					if (enable_validate) {
						for (int at = 0; at < mediapipe::kNumPoseLandmarks; at ++) {
							const SDL_U16Point& l = u16landmarks_[at];
							if (l.x == rose_u16_nan) {
								VALIDATE(l.y == rose_u16_nan, null_str);
								continue;
							}
							SDL_FPoint dq{float_2_u16::dequantize_n05p15(l.x), float_2_u16::dequantize_n05p15(l.y)};

							const SDL_FPoint& l2 = xy_landmarks_[at];
							if (l2.x >= N05P15_FLOAT_MIN && l2.x <= N05P15_FLOAT_MAX && l2.y >= N05P15_FLOAT_MIN && l2.y <= N05P15_FLOAT_MAX) {
								VALIDATE(fabsf(dq.x - l2.x) < 0.00002, null_str);
								VALIDATE(fabsf(dq.y - l2.y) < 0.00002, null_str);
							}
						}
					}

					if (workout_first_satisfied_ticks_ != 0) {
						b_api_.health_push_landmarks(u16landmarks_, unsatisfied_reason);
					}
				}

				state2.did_mediapipe_new_frame(satisfied, unsatisfied_msg_);

			} else if (new_frame_state_ == frame_fail) {
				// the latest image hasn't 33kp. think no person
				if (slot_ != nullptr) {
					slot_->did_analyze_result(analyze_result);
				}
			}
			new_frame_state_ = nposm;
		}

		if (inference_ms_ != nposm && script_.recv_samples() != 0) {
			result_str = sn_last_msg_;
		}
	}
	state2.slice();

	aplt::tif_block::tresult result;
	if (!curr_state2_->next.branches.empty()) {
		result = curr_state2_->next.calculate(script_task_vars_);
		if (result.do_to_state != nposm) {
			VALIDATE(result.do_to_state >= 0 && result.do_to_state < (int)script_.states.size(), null_str);
			curr_state2_->did_exit_state();

			twkoscript::tstate2& new_state2 = script_.states.find(result.do_to_state)->second;
			enter_state(new_state2);
			curr_state2_ = &new_state2;
			if (slot_ != nullptr) {
				slot_->did_enter_state_and_phase(new_state2.state, new_state2.track_pose.curr_phase_);
			}
		}
	}

	if (next_vertical_check_ticks_ != 0 && SDL_GetTicks() >= next_vertical_check_ticks_) {
		int vertical_level = rpy_sensor_.pitch_vertical_level(nullptr);
		if (rpy_sensor_.valid() && vertical_level == trpy_sensor::level_fail) {
			std::string msg = _("Use landscape mode, perpendicular to ground, rear camera on top.");
			pinyin_.speak(msg);
		}

		next_vertical_check_ticks_ = SDL_GetTicks() + vertical_check_threshold_ms_;
	}

	return failed_;
}



bool twkocamera_task::camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb)
{
	threading::lock lock(variable_mutex_);

	cv_argb = argb.clone();
	bool overlay = frame_ok_received_;
	if (new_frame_state_ == frame_ok) {
		

	} else if (new_frame_state_ == frame_fail) {
		overlay = false;
	}

	if (overlay) {
		if (slot_ == nullptr || slot_->is_overlay_landmarks()) {
			mediapipe::overlay_pose_landmarks(xy_landmarks_, mediapipe::kNumPoseLandmarks, flip_h_, cv_argb, nullptr);
		}
	}
	if (slot_ != nullptr) {
		slot_->render_face_overlay(xy_landmarks_, mediapipe::kNumPoseLandmarks, flip_h_, cv_argb);
	}
	return true;
}

void twkocamera_task::set_wko_task_slot(twko_task_slot* _slot)
{
	if (_slot != nullptr) {
		VALIDATE(slot_ == nullptr, null_str);
	} else {
		VALIDATE(slot_ != nullptr, null_str);
	}
	VALIDATE(script_.valid(), null_str);

	slot_ = _slot;
	script_.set_wko_task_slot(slot_);

	if (slot_ != nullptr) {
		VALIDATE(curr_state2_ != nullptr, null_str);
		const twkoscript::tstate2& state2 = *curr_state2_;

		slot_->set_curr_script(script_, state2.state, state2.track_pose.curr_phase_, 
			workout_first_satisfied_ticks_, workout_first_satisfied_ts_);
	}
}

void twkocamera_task::camera_work_frame(const surface& surf, const cv::Mat& argb)
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
	int inference_ms = SDL_GetTicks() - start_ticks;

	// SDL_Log("%u, pose api.next_image, output_mat: (%i x %i), spent %i ms", SDL_GetTicks(), output_frame_mat.cols, output_frame_mat.rows, spent_ms);

	if (!output_frame_mat.empty()) {
		threading::lock lock(variable_mutex_);
		VALIDATE(output_frame_mat.u != nullptr, null_str);

		memcpy(xy_landmarks_, xy_landmarks, sizeof(xy_landmarks));
		inference_ms_ = inference_ms;

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

void twkocamera_task::fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id)
{
}

bool twkocamera_task::is_wko_task_finished()
{
	// if (curr_state2_ == nullptr || !curr_state2_->track_pose.valid()) {
	if (curr_state2_ == nullptr || curr_state2_->task == nullptr) {
		return false;
	}
	twkoscript::tstate2& state2 = *curr_state2_;
	VALIDATE(state2.task != nullptr, null_str);

	return state2.task->finished();
}

void twkocamera_task::clear_kpose()
{
	curr_state2_ = nullptr;
	tasks_suspended_ = false;

	api_ptr_.reset();
	inference_ms_ = nposm;
	output_mat_ = cv::Mat();
	failed_ = false;

	new_frame_state_ = nposm;
	frame_ok_received_ = false;

	sn_last_msg_.clear();
	next_vertical_check_ticks_ = 0;
}

void twkocamera_task::validate_nposm() const
{
	VALIDATE(curr_state2_ == nullptr, null_str);
	VALIDATE(!tasks_suspended_, null_str);

	VALIDATE(api_ptr_.get() == nullptr, null_str);
	VALIDATE(inference_ms_ == nposm, null_str);
	VALIDATE(output_mat_.empty(), null_str);
	VALIDATE(!failed_, null_str);

	VALIDATE(new_frame_state_ == nposm, null_str);

	VALIDATE(sn_last_msg_.empty(), null_str);
	VALIDATE(next_vertical_check_ticks_ == 0, null_str);
}

} // namespace aplt