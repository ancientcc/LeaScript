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

#ifndef LIBROSE2_BASE_SLOT_HPP
#define LIBROSE2_BASE_SLOT_HPP

#include "aplt_common.hpp"
#include "aplt_clazz.hpp"
#include "rose_thread.hpp"
#include "3rdparty_decl.h"
#include "rose_util.hpp"
#include <memory>
#include <set>
#include "ros_values.hpp"
#include "aplt_api.hpp"

typedef int (*faplt_serial_driver_main2)(bool& exit, const std::string& node, int baudrate);

namespace aplt {

class DECLSPEC texternal_imu
{
public:
	texternal_imu(bool use_magnetometer)
		: use_magnetometer_(use_magnetometer)
	{}
	// Since ~texternal_imu() does nothing, it must also be implemented. 
	// see tbase_slot::external_imu_, ~tim948serial() must be called.
	virtual ~texternal_imu() {}

	virtual void slice() = 0;
	virtual bool can_read() const = 0;

protected:
	const bool use_magnetometer_;
};


class DECLSPEC tbase_ext_lamp
{
public:
	tbase_ext_lamp()
		: lamp_longpress_threshold_(20)
	{}

	virtual ~tbase_ext_lamp() {}

	int lamp_longpress_threshold() const { return lamp_longpress_threshold_; }

	virtual void lamp_set_global_settings(int longpress_mul10) = 0;
	virtual void lamp_set_brightness(int value) = 0;
	virtual void lamp_set_color_temperature(int index) = 0;

	virtual void lamp_did_status_report(bool first, int CB_ver, int manufacturer, int product, int temperature, int reserve, int custom0, int custom1) = 0;

	enum {ledtype_scene, ledtype_privacy, ledtype_lefteye, ledtype_righteye, ledtype_count};
	enum {ledact_off = 0, ledact_on = 1, ledact_nothing = 0xff};
	virtual void lamp_ctrl_led(int type, uint8_t value) = 0;
	virtual void lamp_set_button_threshold(int power, int non_power) = 0;

	enum {btntype_scene, btntype_privacy, btntype_count};
	enum {btnact_short_press = 1, btnact_long_press = 2};
	virtual void lamp_did_button_pressed(int type, uint8_t value) = 0;

	virtual tcode2 lamp_product_name() const = 0;

	virtual void lamp_turn_off(bool delay) {}
	virtual bool lamp_in_turn_off() const { return false; }
	virtual void lamp_clear_turn_off() {}

protected:
	// _mul10 --> second
	// 15 -> 1.5(s), 20 -> 2.0(s), 25 -> 2.5(s) ...
	int lamp_longpress_threshold_;
};

#define BATTERY_STILL_THRESHOLD_MS		10000 // 10 second

class DECLSPEC tbase_slot
{
public:
	tbase_slot(const std::string& sn, const std::string& cpuid);
	virtual ~tbase_slot();

	virtual bool has_magnetometer() const { return false; }
	virtual bool start_calib_magnetometer() { return false; }
	virtual bool stop_calib_magnetometer() { return false; }
/*
	virtual bool start_calib_magnetometer1() { return false; }
	virtual bool stop_calib_magnetometer1() { return false; }
	virtual bool start_calib_magnetometer2() { return false; }
	virtual bool stop_calib_magnetometer2() { return false; }
	virtual bool start_calib_magnetometer3() { return false; }
	virtual bool stop_calib_magnetometer3() { return false; }
	virtual bool start_calib_magnetometer4() { return false; }
	virtual bool stop_calib_magnetometer4() { return false; }
	virtual bool start_calib_magnetometer5() { return false; }
	virtual bool stop_calib_magnetometer5() { return false; }
*/
	virtual bool has_angular_velocity() const { return false; }

	virtual bool use_external_imu() const { return false; }
	virtual void open_external_imu(const std::string& serial_path, int baudrate, bool use_magnetometer) {}
	texternal_imu* external_imu() const { return external_imu_; }
	virtual void close_external_imu();

	virtual bool imu_can_read() const { return false; }

	virtual int get_serial_path(std::string& path, std::string& model) = 0;

	virtual void slice() {}

	virtual void aplt_pre_start_ros_node(const std::string& model, const std::string& serial_dev, int baudrate) {}
	// for navigation, start_ros_node() is before pre_start_navigation(...) always. 
	virtual void aplt_start_ros_node(bool& exit, const std::string& serial_dev, int baudrate) = 0;
	virtual void aplt_post_stop_ros_node() {};

	// to_base_footprint_tf(...) must be behind it.
	void enter_navigation(bool buildmap, tpose2d& laser_tf, tpose2d& dcamera_tf);
	// void to_base_footprint_tf(tpose2d& laser, tpose2d& dcamera);

	tbattery_info_C get_battery_info();

	bool moveable() const { return moveable_; }

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

	virtual tbase_ext_lamp* query_ext_lamp() { return nullptr; }

private:
	virtual void aplt_enter_navigation(bool buildmap, ros::tbase_cfg& base_cfg, double& max_mode_vel_x, double& max_mode_vel_theta) = 0;

	virtual void aplt_get_battery_info(tbattery_info_C& info) = 0;

protected:
	const std::string sn_;
	const std::string cpuid_;
	texternal_imu* external_imu_;
	bool moveable_;
};

DECLSPEC void set_base_slot(tbase_slot* slot);
DECLSPEC tbase_slot* get_base_slot();

class DECLSPEC tgroup_state
{
public:
	tgroup_state(const std::string& name, const std::vector<double>& values)
		: name(name)
		, values(values)
	{}

	const std::string name;
	std::vector<double> values;
};

class DECLSPEC tmoveit_slot
{
public:
	enum {state_navigation, state_recognize, state_recognize_near, state_overlook, state_place, state_place_right, state_count};

	tmoveit_slot();
	virtual ~tmoveit_slot();

	virtual int get_serial_path(std::string& path) = 0;
	virtual void pre_start_moveit() = 0;
	virtual void start_moveit(bool& exit, const std::string& serial_path, int baudrate) = 0;

	virtual const std::vector<tgroup_state>& get_common_state(int state);

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

protected:
	std::map<int, std::vector<tgroup_state> > common_states_;
};

class DECLSPEC tlaser_slot
{
public:
	virtual ~tlaser_slot() {}

	// virtual bool start_calib_laser() { return false; }
	// virtual bool stop_calib_laser() { return false; }

	virtual int get_serial_path(std::string& path) = 0;
	virtual void pre_start_laser() {}
	virtual void start_laser(bool& exit, const std::string& serial_path, int baudrate) = 0;

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}
};

class DECLSPEC tdcamera_slot
{
public:
	class DECLSPEC treceiver
	{
	public:
		treceiver() {}
		virtual ~treceiver() {}
		virtual void dcamera_did_frames(int task, const tdcframe_C* frames, int count) {}
	};

	tdcamera_slot()
		: task_(nposm)
		, receiver_(nullptr)
	{}
	virtual ~tdcamera_slot();

	virtual void ros_pre_start() {}
	virtual void ros_dcamera_node(bool& exit) = 0;

	bool main_start(int task, treceiver& receiver);
	virtual void main_slice() = 0;
	void main_stop();
	bool main_tasking() const { return task_ != nposm; }
	int curr_task() const { return task_; }

	virtual bool get_intrinsics(bool depth, tdcintrinsics_C& result) = 0;

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

private:
	virtual bool app_main_start(int task, treceiver& receiver) = 0;
	virtual void app_main_stop() = 0;

protected:
	int task_;
	treceiver* receiver_;
};

DECLSPEC void set_dcamera_frame(tdcframe_C& frame, int type, 
	int format, int width, int height, const uint8_t* data, int data_size, float scale = float_nposm);

class DECLSPEC tiot_slot
{
public:
	tiot_slot(tslot_subscriber& _subscriber);
	virtual ~tiot_slot();

	// called in main-thread.
	virtual void pre_start_iot() {}
	virtual void post_stop_iot() {}

	// called in worker-thread
	virtual void start_iot(bool& exit) = 0;


	// called in main-thread.
	virtual void slice();

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

public:
	tslot_subscriber& subscriber;

protected:
	threading::mutex event_result_mutex_;
	bool event_result_dirty_;
	std::set<tiot_event> event_result_;
};

class DECLSPEC tai_slot: public tnlp_4_aiagent
{
public:
	enum {scenetype_followup, scenetype_count};

	struct tscene
	{
		tscene(const std::string& id, const std::string& name, int type)
			: id(id)
			, name(name)
			, type(type)
		{
			VALIDATE(!id.empty(), null_str);
			VALIDATE(!name.empty(), null_str);
			VALIDATE(type >= 0 && type < scenetype_count, null_str);
		}

		const std::string id;
		const std::string name;
		const int type;
	};

	struct DECLSPEC tprompt
	{
		tprompt(const std::string& _question, const std::string& _tip)
			: question(_question)
			, tip(_tip)
		{
			VALIDATE(!question.empty(), null_str);
		}

		std::string question;
		std::string tip;
	};

	class DECLSPEC tfollowup
	{
	public:
		tfollowup(const tscene& scene);
		virtual ~tfollowup() {}

		const tscene& scene() const { return scene_; }
		const std::string& reference() const { return reference_; }

		virtual std::vector<tprompt> all_prompts() = 0;

		virtual std::vector<tprompt> next_prompts() = 0;

		virtual void feed_answer(const std::string& answer) = 0;

		virtual void back_starting_point() = 0;

	protected:
		const tscene& scene_;
		std::string reference_;
	};

	tai_slot(tslot_subscriber& subscriber);
	virtual ~tai_slot();

	const std::map<std::string, tscene>& scenes() const { return scenes_; }
	void select_scene(const std::string& id);
	tfollowup* curr_followup() const { return curr_followup_; }

	// virtual bool is_nlp_questioning() const = 0;
	void post_did_nlp_answer(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens);

	// called in main-thread.
	void slice();

	// Even if it cannot send @question to nlp-model, the error should be placed 'did_nlp_answer'.
	virtual void send_nlp_question(int chatsrc, bool new_conversation, const std::string& question, const surface& surf,
	 	const std::function<void (bool retbool, const std::string& answer, int input_tokens, int output_tokens)>& did_nlp_answer) = 0;
	// virtual void stop_nlp_question() = 0;

	void send_nlp_question_4_aiagent(bool new_conversation, const std::string& question, const surface& surf,
		const std::function<void(bool retbool, const std::string& answer, int input_tokens, int output_tokens)>& did_nlp_answer) override
	{
		send_nlp_question(chatsrc_aiagent, new_conversation, question, surf, did_nlp_answer);
	}

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

private:
	virtual tfollowup* app_create_scene(const std::string& id) = 0;
	virtual void app_slice() = 0;


protected:
	tslot_subscriber& subscriber_;

	struct tnlp_answer
	{
		tnlp_answer(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens)
			: src(src)
			, retbool(retbool)
			, answer(answer)
			, input_tokens(input_tokens)
			, output_tokens(output_tokens)
		{}

		int src;
		bool retbool; 
		std::string answer; 
		int input_tokens;
		int output_tokens;
	};
	std::vector<tnlp_answer> cached_nlp_answers_;
	threading::mutex nlp_thread_mutex_;

	std::map<std::string, tscene> scenes_;
	tfollowup* curr_followup_;

	std::function<void (bool retbool, const std::string& answer, int input_tokens, int output_tokens)> did_nlp_answer_;
};

}

#endif

