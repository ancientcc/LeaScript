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

#ifndef LIBROS_ROSE_APLT_HPP_
#define LIBROS_ROSE_APLT_HPP_

#include <SDL.h>
#include <string>
#include <memory>
#include "aplt_api.hpp"
#include <sensor_msgs/LaserScan.h>

#include "lua/lua.h"
#include <angles/angles.h>

class tros_map;
class timage_pair;

namespace net {
struct tcswamp_table_result;
}

// rotate to yaw
struct DECLSPEC trotate_to_yaw
{
	trotate_to_yaw(int degree = 45)
		: searched360(false)
	{
		clear();
		set_step_degree(degree);
	}

	void set_step_degree(int degree)
	{
		VALIDATE(!rotating(), null_str);
		VALIDATE(!reaching_goal, null_str);
		VALIDATE(!searched360, null_str);

		VALIDATE(degree <= 180, null_str);
		if (degree == 180) {
			// avoid angles::shortest_angular_distance(...) get '-180'.
			degree = 179;
		}
		step_degree = degree;
	}

	void start_one_rotate(double _goal, int move_robot_threshold)
	{
		VALIDATE(!rotating(), null_str);
		goal = _goal;
		move_end_ticks = SDL_GetTicks() + move_robot_threshold;
	}

	void start_360_rotate(double curr_yaw, int move_robot_threshold, std::string& msg)
	{
		VALIDATE(!rotating(), null_str);
		VALIDATE(!reaching_goal, null_str);
		// object_frames maybe not 0.

		initial_yaw = curr_yaw;
		rotated_degree = 0;
		goal = curr_yaw + DEG2RAD(step_degree);
		move_end_ticks = SDL_GetTicks() + move_robot_threshold;
		next_rotate_ticks = 0;
		reaching_goal = true;
		object_frames = 0;

		char buf[128];
		SDL_snprintf(buf, sizeof(buf), "{360 rotate}start, initial_yaw: %.3f", RAD2DEG(initial_yaw));
		msg = buf;
		SDL_Log("%s", msg.c_str());
	}

	bool start_next_360_rotate(double curr_yaw, int move_robot_threshold, std::string& msg)
	{
		VALIDATE(!searched360, null_str);
		VALIDATE(is_360_rotating(), null_str);

		char buf[128];

		rotated_degree += step_degree;
		if (rotated_degree >= 360) {
		// if (rotated_degree >= 90) {
			SDL_snprintf(buf, sizeof(buf), "{360 rotate}rotated %i, because all degree, stop", rotated_degree);
			msg = buf;
			SDL_Log("%s", msg.c_str());
			stop_360_rotate();
			return false;
		}

		// goal = curr_yaw + DEG2RAD(step_degree);
		goal = angles::normalize_angle(initial_yaw + DEG2RAD(rotated_degree + step_degree));

		move_end_ticks = SDL_GetTicks() + move_robot_threshold;
		next_rotate_ticks = 0;
		reaching_goal = true;
		object_frames = 0;

		SDL_snprintf(buf, sizeof(buf), "{360 rotate}start next, rotated: %i goal: %.3f", rotated_degree, RAD2DEG(goal));
		msg = buf;
		SDL_Log("%s", msg.c_str());
		return true;
	}

	void stop_360_rotate()
	{
		VALIDATE(!searched360, null_str);
		VALIDATE(is_360_rotating(), null_str);

		searched360 = true;
		clear();
	}

	void did_reach_near(int no_object_threshold, std::string& msg)
	{
		if (searched360) {
			VALIDATE(!is_360_rotating(), null_str);
			return;
		}
		next_rotate_ticks = SDL_GetTicks() + no_object_threshold;
		msg = "{360 rotate}set next_rotate_ticks";
		SDL_Log("%s", msg.c_str());
	}

	void did_receive_object_frame(std::string& msg)
	{
		object_frames ++;

		const int min_valid_frames = 3;
		if (object_frames >= min_valid_frames) {
			char buf[128];
			if (is_360_rotating()) {
				SDL_snprintf(buf, sizeof(buf), "{360 rotate}rotated_degree: %i, because valid frames, stop", rotated_degree);
				stop_360_rotate();

			} else {
				// not starte 360 rotate, received the enogh valid frame.
				SDL_strlcpy(buf, "{360 rotate}received enough frame. set searched360 = true", sizeof(buf));
				searched360 = true;
				// require call clear(). for example set next_rotate_ticks to 0.
				clear();
			}

			msg = buf;
			SDL_Log("%s", msg.c_str());
		}
	}

	bool rotating() const
	{
		if (move_end_ticks != 0) {
			VALIDATE(!is_float_nposm(goal), null_str);
			return true;

		} else {
			VALIDATE(is_float_nposm(goal), null_str);
			return false;
		}
	}

	bool is_360_rotating() const
	{
		bool ret = !is_float_nposm(initial_yaw);
		if (ret) {
			VALIDATE(rotating(), null_str);
			VALIDATE(!searched360, null_str);
		} else {
			// may in once rotate
			// VALIDATE(!rotating(), null_str);
		}
		return ret;
	}

	void clear()
	{
		goal = float_nposm;
		move_end_ticks = 0;

		initial_yaw = float_nposm;
		rotated_degree = 0;
		next_rotate_ticks = 0;
		reaching_goal = false;
		object_frames = 0;
	}

	double goal;
	uint32_t move_end_ticks;

	int step_degree;
	bool searched360;
	double initial_yaw;
	int rotated_degree;
	uint32_t next_rotate_ticks;
	bool reaching_goal;
	int object_frames;
};

// #pragma warning(disable:4251)

class DECLSPEC tdepth_sector_result
{
public:
    tdepth_sector_result(float angle_min, float angle_max, float _angle_increment, float _min_z)
		: angle_range(SDL_FRange{angle_min, angle_max})
		, angle_increment(_angle_increment)
		, min_z(_min_z)
    {
		clear();
	}

	bool valid() const 
	{ 
		return !is_float_nposm(angle_range.min) && !is_float_nposm(angle_range.max) && !is_float_nposm(angle_increment) && !is_float_nposm(min_z);
	}

    void clear()
    {
        noised = false;
        left_no_depth = false;
		right_no_depth = false;
    }

public:
	SDL_FRange angle_range;
	float angle_increment;
	float min_z;

    bool noised;
    bool left_no_depth;
	bool right_no_depth;
};

namespace aplt {
/*
class DECLSPEC tnlp_4_aiagent
{
public:
	virtual bool is_nlp_questioning() const = 0;

	// Even if it cannot send @question to nlp-model, the error should be placed 'did_nlp_answer'.
	virtual void send_nlp_question_4_aiagent(bool new_conversation, const std::string& question, const surface& surf,
		const std::function<void(bool retbool, const std::string& answer, int input_tokens, int output_tokens)>& did_nlp_answer) = 0;

	virtual void stop_nlp_question() = 0;
};
*/

//
// r_api: Ros API for applet calls.
//
class DECLSPEC tr_api
{
public:
	tr_api();
	virtual ~tr_api();
	posix_noncopyable(tr_api);

	virtual const tros_map& curmap() const = 0;
	virtual bool goaling() const = 0;

	virtual void set_dcamera_points(const SDL_FPoint3* points, int size, const tdepth_sector_result& result) = 0;
	virtual void laser_publish_scan(const sensor_msgs::LaserScan& msg) = 0;

	virtual void public_vel(double linear_x, double linear_y, double angular_z) = 0;
	virtual const trpy& get_imu_rpy() const = 0;
};

DECLSPEC tr_api& get_r_api();

// DECLSPEC void add_timed_tasks_to_cfg(const std::map<int64_t, tb_api::tadd_timed_task>& tasks, config& cfg);
// DECLSPEC bool cfg_to_add_timed_tasks(const config& root_cfg, std::map<int64_t, tb_api::tadd_timed_task>& result);

class DECLSPEC tros_cpp_api: public tcpp_api
{
public:
	tros_cpp_api();
	virtual ~tros_cpp_api();

	int curr_state() const { return state_; }

protected:
	void slice() override;
	bool speech_did_recognition_result(const std::string& result) override;
	void single_task_finished(const treq_task& req_task, bool result) override;

	void pre_2th_start_task() override;
	void post_2th_start_task() override;
	void post_2th_task_finished(const tapplet& aplt, const tapplet::ttask& cfg_task) override;
	void clear_2th() override;

	virtual void set_state_overflow_ticks(const std::string& scene, int threshold);
	virtual void zero_state_overflow_ticks(const std::string& scene);

	virtual bool is_speech_state(int state) const { return speech_states_.count(state) != 0; }

	virtual const tkey_2_state* get_key_2_state(const std::string& pinyin, int from_state) const;
	virtual void go_to_state(const tstate2& to_state2, int threshold_nposm_to_state);

	virtual void validate_nposm();

protected:
	void set_end_request();
	void speak(const std::string& msg);

protected:
	aplt::tpinyin& pinyin_;
	aplt::tb_api& b_api_;
	aplt::tr_api& r_api_;
	// parse pinyin
	const int tone_;
	const bool eng_lowercase_;

	int state_;
	std::map<int, tstate2> states_;

	int async_request_state_;
	enum {end_request, end_ing};
	int end_flag_;

	int reception_state_;
	bool nonpreemptive_;
	bool recoverable_;
	tif_block startup_state_;
	uint32_t state_overflow_ticks_;

	std::set<int> speech_states_;

	std::vector<tkey_2_state> key_2_states_;

	treq_task req_task_;
};

}

DECLSPEC bool get_makPlan_getRobotPose0();

class DECLSPEC tfpoint3_buffer 
{
public:
    tfpoint3_buffer()
        : data(nullptr)
        , size(0)
        , vsize(0) 
    {}

    ~tfpoint3_buffer()
    {
        if (data != nullptr) {
            // VALIDATE(size > 0, null_str);
            free(data);
        }
    }

    void resize_points(int _size, int _vsize);

public:
    SDL_FPoint3* data;
    int size;
    // tfpoint3_buffer don't use 'vsize', it's used by the user freely.
    int vsize;    
};

// #include <sensor_msgs/LaserScan.h>
DECLSPEC tdepth_sector_result depth_sector_area(const tdcintrinsics_C& intrinsics, sensor_msgs::LaserScan& scan_msg, int width, int height, const uint16_t* depth_data, double depth_scale, uint8_t* mutable_pixels, tfpoint3_buffer* points, double dcpitch);

#endif

