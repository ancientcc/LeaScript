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

#include "camera_task.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>
#include "rose_sdl_utils.hpp"
#include "rose_font.hpp"
#include <rose_ros/kidnap.hpp>
#include "kface.hpp"
#include "kpose.hpp"
#include "wkocamera_task.hpp"
#include "rose_tflite.hpp"

#include "common.hpp"
#include "task_api.hpp"

using namespace std::placeholders;

const char vcamera2MetatableKey[] = "cpp.vcamera2";

namespace aplt {

class tsnapshot: public thelper_camera_task_slot
{
public:
	tsnapshot(const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars);
	~tsnapshot() {}

private:
	std::string start_task() override;
	void task_finished(const tapplet::ttask& cfg_task) override;

	bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) override;

	void camera_work_frame(const surface& surf, const cv::Mat& argb) override;

	void clear_snapshot();
	void validate_snapshot_nposm() const;

	// multiple
	double get_stable_imu(int rpy, bool verbose, bool* fail_ptr = nullptr) const;
	void rtyaw_slice();
	void rotate_360_public_vel(double ang_diff);

private:
	aplt::tr_api& r_api_;
	const std::string var_name_timestamp_;
	const std::string var_name_png_;
	const std::string var_name_count_;
	const std::string var_name_not_send_;

	int count_;
	std::vector<timage_pair> images_;
	cv::Mat snapshot_mat_;
	uint32_t next_snapshot_ticks_;
	bool snapshot_finished_;

	trotate_to_yaw rtyaw_;
	const int move_robot_threshold_;
};

tsnapshot::tsnapshot(const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
	: thelper_camera_task_slot(*lua_camera, *curr_aplt, cfg_task, task_vars)
	, r_api_(aplt::get_r_api())
	, var_name_timestamp_(utils::join_app_prefix_id(aplt_.bundleid, "timestamp"))
	, var_name_png_(utils::join_app_prefix_id(aplt_.bundleid, "png"))
	, var_name_count_(utils::join_app_prefix_id(aplt_.bundleid, "count"))
	, var_name_not_send_(utils::join_app_prefix_id(aplt_.bundleid, "not_send"))
	, move_robot_threshold_(1500) // 1.5 second
{
	clear_snapshot();
}

std::string tsnapshot::start_task()
{
	std::string err_msg;
	validate_snapshot_nposm();

	int count = 1;
	if (task_vars_.existed(var_name_count_)) {
		count = task_vars_.get_int(var_name_count_);
		SDL_Range count_range{1, 4};
		if (count < count_range.min) {
			count = count_range.min;

		} else if (count > count_range.max) {
			count = count_range.max;
		}
	}
	count_ = count;
	if (count_ != 1) {
		rtyaw_.set_step_degree(360 / count);
	}

	return err_msg;
}

void tsnapshot::task_finished(const tapplet::ttask& cfg_task)
{
	if (rtyaw_.is_360_rotating()) {
		rtyaw_.stop_360_rotate();
	}
	clear_snapshot();
}

bool tsnapshot::slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects)
{
	VALIDATE(!snapshot_finished_, null_str);
	if (!snapshot_mat_.empty()) {
		SDL_Log("%u [snapshot(2/2)]write to file", SDL_GetTicks());
		threading::lock lock(variable_mutex_);
			
		time_t t = time(nullptr);
		// const std::string filename("saves/snapshot-" + utils::format_time_ymdhms2(t) + ".png");
		// imwrite(snapshot_mat_, aplt_.preferences_dir + "/" + filename);

		surface surf = snapshot_mat_;

		bool timestamp = false;
		if (task_vars_.existed(var_name_timestamp_)) {
			timestamp = task_vars_.get_bool(var_name_timestamp_);
		}
		if (timestamp) {
			surface ts_surf = font::rose_get_rendered_text(utils::format_time_ymdhms(t), 0, font::SIZE_DEFAULT, font::BIGMAP_COLOR);
			SDL_Rect dst_rect{8, 8, ts_surf->w, ts_surf->h};
			sdl_blit(ts_surf, nullptr, surf, &dst_rect);
		}


		bool png = false;
		if (task_vars_.existed(var_name_png_)) {
			png = task_vars_.get_bool(var_name_png_);
		}

		utils::string_map symbols;
		std::string desc;

		int image_format = png? img_png: img_jpg;
		symbols["number"] = str_cast(images_.size() + 1);
		desc = vgettext2("Image$number", symbols);
		images_.push_back(timage_pair(desc, surf, image_format));

		std::string no_imu_msg;
		if ((int)images_.size() < count_) {
			if (kidnap.has_imu()) {
				snapshot_mat_ = cv::Mat();
				// 1/2 tell work thread, don¡¯t take pictures yet. Set a large value, but not 0.
				next_snapshot_ticks_ = SDL_GetTicks() + 3600 * 1000;

				// 2/2 tell robot rotate.
				int no_object_threshold = 0;
				std::string msg;
				rtyaw_.did_reach_near(no_object_threshold, msg);
				return false;

			} else {
				symbols["count"] = str_cast(count_);
				no_imu_msg = vgettext2("Hope to capture $count images. but because there is no IMU, can only capture one.", symbols);
			}
		}
		const std::vector<timage_pair>& images = images_;

		bool to_server = true;
		if (task_vars_.existed(var_name_not_send_)) {
			to_server = !task_vars_.get_bool(var_name_not_send_);
		}

		std::string local_reason;
		symbols["task"] = cfg_task_.name;
		desc = vgettext2("The image captured by the task($task).", symbols);
		if (!no_imu_msg.empty()) {
			desc += " " + no_imu_msg;
		}
		if (to_server) {
			if (b_api_.is_privacy_protecting()) {
				local_reason = privacy_protect_msgstr;

			} else {
				bool ret = b_api_.cswamp_addevent(t, desc, images, true);
				if (!ret) {
					local_reason = _("Send fail");
				}
			}
		} else {
			local_reason = _("Do not send");
		}

		if (!local_reason.empty()) {
			std::stringstream desc_ss;
			desc_ss << "[" << local_reason << "]" << desc;
			std::string devicename;
			b_api_.aplt_add_msg_log(t, join_to_log_msg(t, devicename, desc_ss.str(), images), 0, false);
		}

		snapshot_finished_ = true;

	} else {
		rtyaw_slice();
	}

	return snapshot_finished_;
}

void tsnapshot::camera_work_frame(const surface& surf, const cv::Mat& argb)
{
	if (next_snapshot_ticks_ == 0) {
		// Some cameras may be dark at first, but slowly the colors will be normal.
		int delay_s = 5;
		next_snapshot_ticks_ = SDL_GetTicks() + delay_s * 1000;
		SDL_Log("%u [snapshot(1/2)]set next_snapshot_ticks_", SDL_GetTicks());
	}

	threading::lock lock(variable_mutex_);
	if (SDL_GetTicks() >= next_snapshot_ticks_) {
		if (!snapshot_finished_) {
			snapshot_mat_ = argb.clone();
		} else {
			// here snapshot_mat_ maybe not empty. 
			// for example, at this time main-thread's slice() change snapshot_finished_ from false to true.
			VALIDATE(!snapshot_mat_.empty(), null_str);
		}
	} else {
		VALIDATE(snapshot_mat_.empty(), null_str);
	}
}

void tsnapshot::clear_snapshot()
{
	count_ = nposm;
	images_.clear();
	snapshot_mat_ = cv::Mat();
	next_snapshot_ticks_ = 0;
	snapshot_finished_ = false;

	rtyaw_.clear();
	rtyaw_.searched360 = false;
}

void tsnapshot::validate_snapshot_nposm() const
{
	VALIDATE(count_ == nposm, null_str);
	VALIDATE(images_.empty(), null_str);
	VALIDATE(snapshot_mat_.empty(), null_str);
	VALIDATE(next_snapshot_ticks_ == 0, null_str);
	VALIDATE(!snapshot_finished_, null_str);

	VALIDATE(!rtyaw_.searched360, null_str);
}

#define MAX_IMU_SAMPLES		5

double tsnapshot::get_stable_imu(int rpy, bool verbose, bool* fail_ptr) const
{
	VALIDATE(rpy == rpy_pitch || rpy == rpy_yaw, null_str);

	const char rpy_name[][12] = {"roll", "pitch", "yaw"};

	int next_sample_index = 0;
	double samples[MAX_IMU_SAMPLES];
	// must not use 0. current pitch maybe is 0. I don't think there will be a 9999
	// memset(samples, 0, sizeof(samples));
	for (int at = 0; at < MAX_IMU_SAMPLES; at ++) {
		samples[at] = float_nposm;
	}

	uint32_t start_ticks = SDL_GetTicks();
	const int threshold = 5000; // 10 seconds
	while ((int)(SDL_GetTicks() - start_ticks) < threshold) {
		if (rpy == rpy_pitch) {
			samples[next_sample_index] = r_api_.get_imu_rpy().pitch;
		} else {
			// rpy_yaw
			samples[next_sample_index] = r_api_.get_imu_rpy().yaw;
		}

		if (verbose) {
			SDL_Log("-----%u {get_stable_imu}next_sample_index: %i---", SDL_GetTicks(), next_sample_index);
			for (int at = 0; at < MAX_IMU_SAMPLES; at ++) {
				const double dist = samples[at];
				SDL_Log("[%i/%i]value: %.6f(deg:%.4f)", at, MAX_IMU_SAMPLES, samples[at], RAD2DEG(samples[at]));
			}
			SDL_Log("--------");
		}

		bool satisfied = true;
		const double threshold = DEG2RAD(0.3);
		for (int at1 = 0; at1 < MAX_IMU_SAMPLES && satisfied; at1 ++) {
			const double value1 = samples[at1];
			for (int at2 = at1 + 1; at2 < MAX_IMU_SAMPLES; at2 ++) {
				const double value2 = samples[at2];

				double diff = fabs(value1 - value2);
				if (verbose) {
					SDL_Log("get_stable_imu: [%i] - [%i]: %.4f(deg:%.4f)", at1, at2, diff, RAD2DEG(diff));
				}
				if (diff > threshold) {
					if (verbose) {
						SDL_Log("get_stable_imu: [%i] - [%i]: %.4f(deg:%.4f) > threshold(%.6f), return false", 
							at1, at2, diff, RAD2DEG(diff), threshold);
					}
					satisfied = false;
					break;
				}
			}
		}

		if (satisfied) {
			SDL_Log("get_stable_imu[%s] ok, elapse %i ms. %.6f(deg: %.3f)", rpy_name[rpy], (int)(SDL_GetTicks() - start_ticks), samples[next_sample_index], RAD2DEG(samples[next_sample_index]));
			if (fail_ptr != nullptr) {
				*fail_ptr = true;
			}
			return samples[next_sample_index];
		}

		// I think the interval will not exceed 100 milliseconds at most.
		const int ms = 120;
		SDL_Delay(ms);

		next_sample_index ++;
		next_sample_index %= MAX_IMU_SAMPLES;
	}

	if (fail_ptr != nullptr) {
		*fail_ptr = false;
	}
	SDL_Log("get_stable_imu[%s] fail, elapse %i ms, time overflow.", rpy_name[rpy], (int)(SDL_GetTicks() - start_ticks));
	int index = next_sample_index > 0? next_sample_index - 1: MAX_IMU_SAMPLES - 1;
	return samples[index];
}

void tsnapshot::rotate_360_public_vel(double ang_diff)
{
	double abs_ang_diff = fabs(ang_diff);
	const double max_yaw = DEG2RAD(60);
	const double careful_yaw = DEG2RAD(20);
	const double min_yaw = DEG2RAD(5);

	double angular_z = abs_ang_diff;
	if (abs_ang_diff > max_yaw) {
		angular_z = max_yaw - DEG2RAD(10);

	} else if (abs_ang_diff > careful_yaw) {
		angular_z = abs_ang_diff - DEG2RAD(5);

	} else if (abs_ang_diff < min_yaw) {
		angular_z = min_yaw;
	}
	VALIDATE(angular_z >= 0, null_str);
	if (ang_diff < 0) {
		angular_z *= -1;
	}

	double twist[3] = {0, 0, angular_z};
	r_api_.public_vel(twist[0], twist[1], twist[2]);

	SDL_Log("%u {360 rotate}ang_diff: %.3f pulbic vel: (%.5f, %.3f, %.3f)", SDL_GetTicks(), 
		RAD2DEG(ang_diff), twist[0], twist[1], RAD2DEG(twist[2]));

	rtyaw_.move_end_ticks = SDL_GetTicks() + move_robot_threshold_;
}

void tsnapshot::rtyaw_slice()
{
	VALIDATE(!snapshot_finished_, null_str);
	VALIDATE((int)images_.size() < count_, null_str);
	VALIDATE(snapshot_mat_.empty(), null_str);

	std::string msg;
	if (rtyaw_.next_rotate_ticks != 0 && SDL_GetTicks() >= rtyaw_.next_rotate_ticks) {
		VALIDATE(!rtyaw_.searched360, null_str); 

		// ros_instance_.do_light_group_values_target(aplt::tmoveit_slot::state_recognize);

		// yaw of action_recognize and action_recognize_near is different.
		double curr_yaw = get_stable_imu(rpy_yaw, false);

		bool can = true;
		if (!rtyaw_.is_360_rotating()) {
			rtyaw_.start_360_rotate(curr_yaw, move_robot_threshold_, msg);
		} else {
			can = rtyaw_.start_next_360_rotate(curr_yaw, move_robot_threshold_, msg);
		}

		if (can) {
			double ang_diff = angles::shortest_angular_distance(curr_yaw, rtyaw_.goal);
			rotate_360_public_vel(ang_diff);

		} else {
			VALIDATE(rtyaw_.searched360, null_str);
		}
		return;
	}

	if (rtyaw_.rotating()) {
		// VALIDATE(operate_.state == state_distance, null_str);
		// VALIDATE(operate_.m4ik.stop_ticks == 0, null_str);
		if (SDL_GetTicks() < rtyaw_.move_end_ticks) {
			return;
		}

		double curr_yaw = r_api_.get_imu_rpy().yaw;
		if (rtyaw_.is_360_rotating()) {
			if (rtyaw_.reaching_goal) {
				double stable_yaw = get_stable_imu(rpy_yaw, false);

				double ang_diff = angles::shortest_angular_distance(curr_yaw, rtyaw_.goal);
				const double threshold = DEG2RAD(10);
				if (ang_diff <= threshold) { // here is not fabs(ang_diff), once ang_diff < 0, always true.
					SDL_Log("%u {360 rotate}yaw range(%.3f + %i) ang_diff: %.3f(<= 10) begin search object", 
						SDL_GetTicks(), RAD2DEG(rtyaw_.goal - rtyaw_.initial_yaw), rtyaw_.step_degree, RAD2DEG(ang_diff));
					rtyaw_.reaching_goal = false;

					// VALIDATE(operate_.nearing, null_str);
					// can goto near again
					// set_goto_near_ticks(true);
					// operate_.nearing = false;
					
					// Notification 'work thread', it¡¯s time to take a picture
					next_snapshot_ticks_ = SDL_GetTicks();

				} else {
					SDL_Log("%u {360 rotate}yaw range(%.3f + %i) ang_diff: %.3f(> 10) continue public vel", 
						SDL_GetTicks(), RAD2DEG(rtyaw_.goal - rtyaw_.initial_yaw), rtyaw_.step_degree, RAD2DEG(ang_diff));

					// If the rotation is too large, I am afraid that it will cause the robot to concuss.
					rotate_360_public_vel(ang_diff);
				}
			}

		} else {
			// it is once rotate
			SDL_Log("%u {rotate_to_yaw}finished, goal: %.3f curr: %.3f", SDL_GetTicks(), RAD2DEG(rtyaw_.goal), RAD2DEG(curr_yaw));
			rtyaw_.clear();
		}
	}
}


//
// recognition
//
class trecognition: public thelper_camera_task_slot
{
public:
	trecognition(const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars);
	~trecognition() {}

private:
	std::string start_task() override;
	void task_finished(const tapplet::ttask& cfg_task) override;

	bool slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects) override;

	void camera_work_frame(const surface& surf, const cv::Mat& argb) override;

	void clear_ttbh();

private:
	tflite::tslot& tflite_slot_;
	//
	// code_detachment
	//
	tflite::tscript def_script_;
	tflite::ttflite def_tflite_;

	tflite::tresult result_;
	std::vector<std::pair<float, SDL_Rect> > classifier_rects_;

	// moveto_recognition
	double initial_angle_;
	double last_angle_;
	bool angle_flipped_;
	int rotation_times_;
	std::string last_object_;
	uint32_t next_rotation_ticks_;

	// recognition
	const int recognition_threshold_;
	uint32_t end_recognition_ticks_;
	uint32_t more_precise_ticks_;
	uint32_t last_recognition_ticks_;
	int new_objects_;
};

trecognition::trecognition(const aplt::tapplet::ttask& cfg_task, ttask_vars& task_vars)
	: thelper_camera_task_slot(*lua_camera, *curr_aplt, cfg_task, task_vars)
	, tflite_slot_(tflite::get_curr_slot())
	, def_script_() // evaluate after get valid game_config::path
	, def_tflite_(null_str, false) // evaluate after get valid def_script_
	, recognition_threshold_(game_config::os == os_windows? 60000: 15000) // 15 second
	// , recognition_threshold_(5000) // 5 second
{
	clear_ttbh();
}

std::string trecognition::start_task()
{
	def_script_ = tflite::tscript(game_config::app_dir_root + "/tflites/classifier_camera.cfg", true, nullptr);
	VALIDATE(def_script_.valid(), null_str);
	def_tflite_ = tflite::ttflite(def_script_.tflite, def_script_.res);

	tflite_slot_.set_tflite(def_script_, def_tflite_);
	// if (req_task_.type == aplt::treq_task::type_moveto_recognition) {
	if (false) {
/*
		log = _("Navigation successfully, start to recognition");
		initial_angle_ = ros_instance_.get_imu_yaw(nullptr);
		initial_angle_ = angles::normalize_angle_positive(initial_angle_);
		last_angle_ = initial_angle_;
*/
	} else {
		// log = _("Start to recognition");
		end_recognition_ticks_ = SDL_GetTicks() + recognition_threshold_;
		const int more_precise_threshold = 4000;
		more_precise_ticks_ = SDL_GetTicks() + more_precise_threshold;
		VALIDATE(new_objects_ == 0, null_str);
	}
	// instance->bg_task().add_log(time(nullptr), log);

	return null_str;
}

void trecognition::task_finished(const tapplet::ttask& cfg_task)
{
	clear_ttbh();
}

bool trecognition::slice(std::string& result_str, std::vector<std::pair<float, SDL_Rect> >& classifier_rects)
{
	const double min_moveable_vel_x = 0.08;
	const double min_moveable_vel_theta = DEG2RAD(20);
	const int min_rotation_times = 5;
	const int max_rotation_times = 60; // 50
	const double angle_threshold = 2.0*M_PI;

	double abs_linear_x = 0.08;
	double linear_x = (rotation_times_ & 1)? -abs_linear_x: abs_linear_x;
	double angular_z = DEG2RAD(30);


	bool finished = false;
	bool recognition = false;
/*
	if (!camera_.has_slot()) {
		// is no temporary slot, ttemp_task derminate when to call camera_.slice(...)
		camera_.slice(empty_rect, false);
	}
*/
	uint32_t now = SDL_GetTicks();
	// if (req_task_.type == aplt::treq_task::type_moveto_recognition) {
	if (false) {
/*
		finished = !base_driver_.imu_can_read() || rotation_times_ > max_rotation_times;
		if (!finished) {
			double raw_curr_degree = ros_instance_.get_imu_yaw(nullptr);
			raw_curr_degree = angles::normalize_angle_positive(raw_curr_degree);

			if (!angle_flipped_ && rotation_times_ > min_rotation_times && last_angle_ > raw_curr_degree) {
				angle_flipped_ = true;
			}

			double curr_degree = raw_curr_degree;
			if (angle_flipped_) {
				curr_degree += angle_threshold;
			}
			if (curr_degree - initial_angle_ < angle_threshold) {
				if (now >= next_rotation_ticks_) {
					ros_instance_.public_vel(linear_x, 0.0, angular_z);
					rotation_times_ ++;
					const int one_roation_threshold = 1000; // 1second
					next_rotation_ticks_ = now + one_roation_threshold;
					SDL_Log("%u ttemp_task::slice, #%i, flipped(%s) curr_degree: %.3f(%.3f), initial_angle_: %.3f", 
						now, rotation_times_, angle_flipped_? "true": "false", RAD2DEG(curr_degree), RAD2DEG(raw_curr_degree), RAD2DEG(initial_angle_));
				}
			} else if (now >= next_rotation_ticks_) {
				finished = true;
			}
		}

		if (!finished) {
			recognition = true;	
		}
*/
	// } else if (req_task_.type == aplt::treq_task::type_recognition) {
	} else {
		// if (now >= end_recognition_ticks_ && !camera_.has_slot()) {
		if (now >= end_recognition_ticks_) {
			if (new_objects_ == 0) {
				pinyin_.speak(_("Not recognized"));
			}
			SDL_Log("%u ttemp_task this recognition is finished", SDL_GetTicks());
			finished = true;
		} else {
			if (more_precise_ticks_ != 0 && now >= more_precise_ticks_) {
				pinyin_.speak(_("Try to place the object in the center of camera's view"));
				more_precise_ticks_ = 0;
			}
			recognition = true;
		}
	}

	if (finished) {
		SDL_Log("{in_ttbh_}%u wall call exit_task() in (2.2)finished", SDL_GetTicks());
		clear_ttbh();

		return true;
	}

	{
		threading::lock lock(variable_mutex_);
		result_str = result_.to_string();
		classifier_rects = classifier_rects_;
	}

	if (recognition) {
		std::string this_object;
		{
			// threading::lock lock(variable_mutex_);
			const tflite::tresult& result = result_;
			for (std::vector<tflite::tresult::titem>::const_iterator it = result.items.begin(); it != result.items.end(); ++ it) {
				const tflite::tresult::titem& item = *it;
				if (item.score >= 0.40) { // 0.7 
					this_object = item.name;
					break;
				}
			}
		}

		const int repeat_threshold = 4000;
		if (!this_object.empty() && 
			(this_object != last_object_ || (now >= last_recognition_ticks_ + repeat_threshold))) {
			size_t pos = this_object.find('(');
			const std::string text = pos == std::string::npos? this_object: this_object.substr(0, pos);
			pinyin_.speak(text);

			if (last_object_ != this_object) {
				last_object_ = this_object;

				new_objects_ ++;
				end_recognition_ticks_ = SDL_GetTicks() + recognition_threshold_;
				more_precise_ticks_ = 0;
			}
			last_recognition_ticks_ = SDL_GetTicks();
		}
	}

	return false;
}

void trecognition::camera_work_frame(const surface& surf, const cv::Mat& argb)
{
	tflite::tresult result;
	std::vector<std::pair<float, SDL_Rect> > classifier_rects = tflite_slot_.classifier_image(argb, result);

	{
		threading::lock lock(variable_mutex_);
		result_ = result;
		classifier_rects_ = classifier_rects;
	}
}

void trecognition::clear_ttbh()
{
	SDL_Log("{in_ttbh_}%u ttemp_task::clear_ttbh() is called", SDL_GetTicks());

	initial_angle_ = 0;
	last_angle_ = float_nposm;
	angle_flipped_ = false;
	rotation_times_ = 0;
	next_rotation_ticks_ = 0;
	last_object_.clear();

	end_recognition_ticks_ = 0;
	more_precise_ticks_ = 0;
	more_precise_ticks_ = 0;
	new_objects_ = 0;
}

thelper_camera_task_slot* tkhome_camera_api::app_create_camera_task_slot(int code, const tapplet::ttask& cfg_task, ttask_vars& task_vars)
{
	if (code == camera_snapshot) {
		return new tsnapshot(cfg_task, task_vars);

	} else if (code == camera_kface) {
#ifdef USE_DFACE
		return new tkface(cfg_task, task_vars);
#endif

	} else if (code == camera_kpose) {
		return new tkpose(cfg_task, task_vars);

	} else if (code == camera_workout) {
		return new twkocamera_task(*lua_camera, aplt_, cfg_task, task_vars);

	}

	VALIDATE(code == camera_recognition, null_str);
	return new trecognition(cfg_task, task_vars);
}
/*
tcamera_task_slot* create_camera_task_slot(int code, const tapplet::ttask& cfg_task, ttask_vars& task_vars)
{
	if (code == camera_snapshot) {
		return new tsnapshot(cfg_task, task_vars);

	} else if (code == camera_kface) {
#ifdef USE_DFACE
		return new tkface(cfg_task, task_vars);
#endif

	} else if (code == camera_kpose) {
		return new tkpose(cfg_task, task_vars);

	} else if (code == camera_workout) {
		return new twkocamera_task(cfg_task, task_vars);

	}

	VALIDATE(code == camera_recognition, null_str);
	return new trecognition(cfg_task, task_vars);
}
*/
//
// detect task
//

tlua_camera::tlua_camera()
	// : slot_(nullptr)
{
#ifdef USE_DFACE
	faceprint_allocate(*curr_aplt);
#endif
}

tlua_camera::~tlua_camera()
{
	VALIDATE(slot_ == nullptr, null_str);
#ifdef USE_DFACE
	faceprint_free();
#endif
}

//
// lua api
//
static int impl_vcamera2_collect(lua_State* L)
{
	// twidget *v = *static_cast<twidget **>(lua_touserdata(L, 1));
	// v->~vwidget();
	return 0;
}

static int impl_vcamera2_get(lua_State* L)
{
	const char* m = luaL_checkstring(L, 2);
	bool ret = luaW_getmetafield(L, 1, m);

	return ret? 1: 0;
}

static int impl_vblock_cooking_set_position(lua_State* L)
{
/*
	tblock2* v = *static_cast<tblock2 **>(lua_touserdata(L, 1));

	const int device_at = luaL_checkinteger(L, 2);
	const char* uuid = luaL_checkstring(L, 3);

	v->cooking().set_position(device_at, uuid);
*/
	return 0;
}

void luaW_pushvcamera2(lua_State* L, tlua_camera& widget)
{
	aplt::tb_api& b_api = aplt::get_b_api();

	tstack_size_lock lock(L, 1);
	// new(L) vwidget(L, widget);
	tlua_camera** v = (tlua_camera**)lua_newuserdata(L, sizeof(tlua_camera*));
	*v = &widget;

	// b_api.call_lua_breakpoint();
	nil_metatable(b_api.get_lua_State(), vcamera2MetatableKey);
	// b_api.call_lua_breakpoint();
	
	// see https://www.cswamp.com/post/261
	if (luaL_newmetatable(L, vcamera2MetatableKey)) {
		luaL_Reg metafuncs[] {
			{"__gc", impl_vcamera2_collect},
			{"__index", impl_vcamera2_get},

			// tkface
			{"kface_reload", impl_vcamera2_kface_reload},
			{"kface_erase_face", impl_vcamera2_erase_face},
			{"kface_modify_face", impl_vcamera2_modify_face},

			// {"cooking_set_position", impl_vblock2_cooking_set_position},


			{nullptr, nullptr},
		};
		luaL_setfuncs(L, metafuncs, 0);
		lua_pushstring(L, "__metatable");
		lua_setfield(L, -2, vcamera2MetatableKey);

	} else {
		VALIDATE(false, null_str);
	}

	lua_setmetatable(L, -2);
}

}

