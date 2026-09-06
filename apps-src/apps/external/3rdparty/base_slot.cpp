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

#include "base_slot.hpp"
#include "rose_exception.hpp"
#include <SDL_log.h>

#include "rose_config_3rdparty.hpp"
#include "rose_filesystem.hpp"

using namespace std::placeholders;

namespace ros {
// extern bool dwa_straight_ward;

tbase_cfg::tbase_cfg()
    : min_moveable_vel_x(ROS_DEF_MIN_MOVEABLE_VEL_X)
    , min_moveable_vel_theta(ROS_DEF_MIN_MOVEABLE_VEL_THETA)
    , max_buildmap_vel_x(ROS_DEF_MAX_BUILDMAP_VEL_X)
    , max_buildmap_vel_theta(ROS_DEF_MAX_BUILDMAP_VEL_THETA)
    , max_navigation_vel_x(ROS_DEF_MAX_NAVIGATION_VEL_X)
    , max_navigation_vel_theta(ROS_DEF_MAX_NAVIGATION_VEL_THETA)
    , move_base_controller_freq(ROS_DEF_MOVE_BASE_CONTROLLER_FREQ)
    , robot_length(ROS_DEF_ROBOT_LENGTH)
    , robot_width(ROS_DEF_ROBOT_WIDTH)
{}
tbase_cfg base_cfg;

std::string footprint_string = ROS_DEF_FOOTPRINT_STRING;

bool buildmap = false;

}

namespace aplt {

tbase_slot::tbase_slot(const std::string& sn, const std::string& cpuid)
	: sn_(sn)
    , cpuid_(cpuid)
    , external_imu_(nullptr)
    , moveable_(false)
{}

tbase_slot::~tbase_slot()
{
	if (external_imu_ != nullptr) {
		delete external_imu_;
		external_imu_ = nullptr;
	}
}

void tbase_slot::close_external_imu()
{
	VALIDATE(external_imu_ != nullptr, null_str);

	delete external_imu_;
	external_imu_ = nullptr;
}

void tbase_slot::enter_navigation(bool buildmap, tpose2d& laser_tf, tpose2d& dcamera_tf)
{
	SDL_Log("tbase_slot::enter_navigation#1");
	ros::buildmap = buildmap;

	ros::base_cfg.min_moveable_vel_x = ROS_DEF_MIN_MOVEABLE_VEL_X;
	ros::base_cfg.min_moveable_vel_theta = ROS_DEF_MIN_MOVEABLE_VEL_THETA;

	double& max_mode_vel_x = buildmap? ros::base_cfg.max_buildmap_vel_x: ros::base_cfg.max_navigation_vel_x;
	double& max_mode_vel_theta = buildmap? ros::base_cfg.max_buildmap_vel_theta: ros::base_cfg.max_navigation_vel_theta;
	if (buildmap) {
		max_mode_vel_x = ROS_DEF_MAX_BUILDMAP_VEL_X;
		max_mode_vel_theta = ROS_DEF_MAX_BUILDMAP_VEL_THETA;
	} else {
		max_mode_vel_x = ROS_DEF_MAX_NAVIGATION_VEL_X;
		max_mode_vel_theta = ROS_DEF_MAX_NAVIGATION_VEL_THETA;
	}

	ros::base_cfg.move_base_controller_freq = ROS_DEF_MOVE_BASE_CONTROLLER_FREQ;
	ros::footprint_string = ROS_DEF_FOOTPRINT_STRING;

	ros::base_cfg.robot_length = ROS_DEF_ROBOT_LENGTH;
	ros::base_cfg.robot_width = ROS_DEF_ROBOT_WIDTH;

    ros::base_cfg.laser_to_base_footprint_tf.set(0, 0, 0, false);
    ros::base_cfg.dcamera_to_base_footprint_tf.set(0, 0, 0, false);

	aplt_enter_navigation(buildmap, ros::base_cfg, max_mode_vel_x, max_mode_vel_theta);

    VALIDATE(ros::base_cfg.laser_to_base_footprint_tf.valid, null_str);
    // now yaw must be 0.
    VALIDATE(KDL_Equal(ros::base_cfg.laser_to_base_footprint_tf.yaw, 0), null_str);

    if (ros::base_cfg.dcamera_to_base_footprint_tf.valid) {
        // now yaw must be 0.
        VALIDATE(KDL_Equal(ros::base_cfg.dcamera_to_base_footprint_tf.yaw, 0), null_str);
    }

	VALIDATE(ros::base_cfg.robot_length >= 0.1 && ros::base_cfg.robot_length < 1.0, null_str);
	VALIDATE(ros::base_cfg.robot_width >= 0.1 && ros::base_cfg.robot_width < 1.0, null_str);


	// calculate footprint from robot_length/robot_width
	// [[-0.13, -0.14], [-0.13, 0.14], [0.13, 0.14], [0.13, -0.14]]
	double half_length = ros::base_cfg.robot_length / 2;
	double half_width = ros::base_cfg.robot_width / 2;
	char buf[128];
	SDL_snprintf(buf, sizeof(buf), "[[%.3f, %.3f], [%.3f, %.3f], [%.3f, %.3f], [%.3f, %.3f]]",
		-half_length, -half_width, -half_length, half_width, half_length, half_width, half_length, -half_width);
	ros::footprint_string = buf;

	ros::base_cfg.robot_length += ROS_FOOTPRINT_PADDING * 2;
	ros::base_cfg.robot_width += ROS_FOOTPRINT_PADDING * 2;

    laser_tf = ros::base_cfg.laser_to_base_footprint_tf;
	dcamera_tf = ros::base_cfg.dcamera_to_base_footprint_tf;
}

tbattery_info_C tbase_slot::get_battery_info()
{
	tbattery_info_C info;
	memset(&info, 0, sizeof(tbattery_info_C));

	aplt_get_battery_info(info);

    VALIDATE(info.cutoff > 0, null_str);
    VALIDATE(info.charge > info.cutoff, null_str);
    VALIDATE(info.max > info.charge, null_str);

	return info;
}

static tbase_slot* s_base_slot = nullptr;
void set_base_slot(tbase_slot* slot)
{
	s_base_slot = slot;
}

tbase_slot* get_base_slot()
{
	return s_base_slot;
}

//
// tmoveit_slot
//
tmoveit_slot::tmoveit_slot()
{}

tmoveit_slot::~tmoveit_slot()
{
}

const std::vector<tgroup_state>& tmoveit_slot::get_common_state(int state)
{
	VALIDATE(common_states_.count(state) != 0, null_str);
	return common_states_.find(state)->second;
}

//
// tdcanera_slot
//
void set_dcamera_frame(tdcframe_C& frame, int type, int format, int width, int height, const uint8_t* data, int data_size, float scale)
{
    char err[64];
    SDL_snprintf(err, sizeof(err), "type: %i format: %i size: (%ix%i) scale: %.3f", type, format, width, height, scale);

	VALIDATE(type >= 0 && type < dcframetype_count, err);
	VALIDATE(format >= 0 && format < dcformat_count, err);
	if (type == dcframetype_color) {
		VALIDATE(width >= 640 && width <= 1280, err);
		VALIDATE(height >= 480 && height <= 720, err);
		VALIDATE(is_float_nposm(scale), err);

	} else if (type == dcframetype_depth) {
		VALIDATE(width >= 640 && width <= 1280, err);
		VALIDATE(height >= 400 && height <= 800, err);
		VALIDATE(scale > 0, null_str);
	}
	VALIDATE(data != nullptr, err);
	VALIDATE(data_size > 0, err);

	frame.type = type;
	frame.format = format;
	frame.width = width;
	frame.height = height;
	frame.data = data;
	frame.data_size = data_size;
	frame.scale = scale;
}

tdcamera_slot::~tdcamera_slot()
{
	VALIDATE(task_ == nposm, null_str);
	VALIDATE(receiver_ == nullptr, null_str);
}

bool tdcamera_slot::main_start(int task, treceiver& receiver)
{
	VALIDATE(task_ == nposm, null_str);
	VALIDATE(receiver_ == nullptr, null_str);
	VALIDATE(task >= 0 && task < dctask_count, null_str);

	if (!app_main_start(task, receiver)) {
		return false;
	}

	task_ = task;
	receiver_ = &receiver;
	return true;
}

void tdcamera_slot::main_stop()
{
	VALIDATE(task_ >= 0 && task_ < dctask_count, null_str);
	VALIDATE(receiver_ != nullptr, null_str);

	app_main_stop();
	task_ = nposm;
	receiver_ = nullptr;
}

tiot_slot::tiot_slot(tslot_subscriber& _subscriber)
    : subscriber(_subscriber)
    , event_result_dirty_(false)
{
}

tiot_slot::~tiot_slot()
{
    // VALIDATE(http_thread_ == nullptr, null_str);
}

void tiot_slot::slice()
{
	// if (!started_) {
	//	return;
	// }

	std::set<aplt::tiot_event> result;
	if (event_result_dirty_) {
		threading::lock lock(event_result_mutex_);
		result = event_result_;

		event_result_dirty_ = false;
		event_result_.clear();
	}
	if (result.empty()) {
		return;
	}

	subscriber.iot_did_events(result);
}

//
// tai_slot
//

tai_slot::tfollowup::tfollowup(const tscene& scene)
    : scene_(scene)
{
}

tai_slot::tai_slot(tslot_subscriber& subscriber)
    : subscriber_(subscriber)
    , curr_followup_(nullptr)
{
}

tai_slot::~tai_slot()
{
    VALIDATE(curr_followup_ == nullptr, null_str);
}

void tai_slot::select_scene(const std::string& id)
{
    VALIDATE(scenes_.count(id) != 0, null_str);

    if (curr_followup_ != nullptr) {
        if (curr_followup_->scene().id == id) {
            curr_followup_->back_starting_point();
            return;

        } else {
            delete curr_followup_;
		    curr_followup_ = nullptr; 
        }
    }
    curr_followup_ = app_create_scene(id);
}

void tai_slot::post_did_nlp_answer(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens)
{
    VALIDATE_NOT_MAIN_THREAD();

    VALIDATE(src == chatsrc_summary, null_str);

    threading::lock lock(nlp_thread_mutex_);
    cached_nlp_answers_.push_back(tnlp_answer(src, retbool, answer, input_tokens, output_tokens));
}

void tai_slot::slice()
{
    if (!cached_nlp_answers_.empty()) {
        std::vector<tnlp_answer> nlp_answers;
        {
            threading::lock lock(nlp_thread_mutex_);
            nlp_answers = cached_nlp_answers_;
            cached_nlp_answers_.clear();
        }
        for (std::vector<tnlp_answer>::const_iterator it = nlp_answers.begin(); it != nlp_answers.end(); ++ it) {
            const tnlp_answer& answer = *it;
            subscriber_.aiagent_did_nlp_answer(answer.src, answer.retbool, answer.answer, answer.input_tokens, answer.output_tokens);
        }
    }

    app_slice();
}

}