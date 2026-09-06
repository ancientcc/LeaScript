/*
   Copyright (C) 2009 - 2018 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_APLT_API_HPP
#define LIBROSE_APLT_API_HPP

#include "aplt2.hpp"
#include "aplt_clazz.hpp"

class timage_pair;

namespace net {
struct tcswamp_table_result;
}

#define TCOOKIE3F_CHECK_OK		nposm
struct DECLSPEC tcookie3f
{
	tcookie3f(uint64_t _u64)
		: u64(_u64)
	{
		uint32_t lo32 = posix_lo32(u64);
		uint32_t hi32 = posix_hi32(u64);

		field = (short)posix_lo16(lo32);
		type = (short)posix_hi16(lo32);

		index = (short)posix_lo16(hi32);
		int zero = (short)posix_hi16(hi32);
		VALIDATE(zero == 0, null_str);
	}

	tcookie3f(int _index, int _type, int _field)
		: index(_index)
		, type(_type)
		, field(_field)
	{
		VALIDATE(index >= 0 && type >= 0 && field >= 0, null_str);
		u64 = posix_mku64(posix_mku32(field, type), posix_mku32(index, 0));
	}

	bool operator==(const tcookie3f& that) const
	{
		return index == that.index && type == that.type && field == that.field; 
	}
	bool operator!=(const tcookie3f& that) const { return !operator==(that); }

	int index;
	int type;
	int field;

	uint64_t u64;
};

namespace aplt {

class LIB3RDPARTY_DECL tnlp_4_aiagent
{
public:
	virtual bool is_nlp_questioning() const = 0;

	// Even if it cannot send @question to nlp-model, the error should be placed 'did_nlp_answer'.
	virtual void send_nlp_question_4_aiagent(bool new_conversation, const std::string& question, const surface& surf,
		const std::function<void(bool retbool, const std::string& answer, int input_tokens, int output_tokens)>& did_nlp_answer) = 0;

	virtual void stop_nlp_question() = 0;
};

//
// b_api: Basic API for applet calls.
//
class LIB3RDPARTY_DECL tb_api
{
public:
	tb_api();
	virtual ~tb_api();
	posix_noncopyable(tb_api);

	virtual std::string request_task(const treq_task& req_task, const ttask_vars& task_vars = ttask_vars(),
		const std::function<void (const ttask_vars& task_vars)>& did_task_finished = NULL) = 0;
	// [must be main thread]must call in pure_task_cpp environment
	virtual std::string request_single_task(const treq_task& req_task) = 0;

	virtual void set_task_finished() = 0;
	virtual void set_allow_short_voice(bool val) = 0;

	virtual tnlp_4_aiagent& nlp_4_aiagent() = 0;
	// virtual const tros_map& curmap() const = 0;
	// virtual bool goaling() const = 0;
	virtual const std::map<taplt_key, tapplet>& const_applets() const = 0;
	virtual const std::map<aplt::taplt_task_key, aplt::taplt_task>& iot_tasks() const = 0;
	virtual const std::map<aplt::taplt_task_key, aplt::taplt_task>& timed_tasks() const = 0;
	struct tadd_timed_task
	{
		tadd_timed_task(bool persist, const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id,
			const std::string& position1, const std::string& position2, int64_t time, const std::map<std::string, std::string>& input_vars)
			: persist(persist)
			, aplt_id(aplt_id)
			, task_id(task_id)
			, ble_device_id(ble_device_id)
			, position1(position1)
			, position2(position2)
			, time(time)
			, input_vars(input_vars)
			, added(false)
			, deleted(false)
		{}

		bool persist;
		// why not use tapplet& aplt?
		// -- The aplt may not be installed yet.
		std::string aplt_id;
		std::string task_id;
		std::string ble_device_id;
		std::string position1;
		std::string position2;
		// 1)persist == false. It is a one-off task that gets deleted after triggering.
		// 2)persist == true. Once created, it persists.
		// In either case, the unit is seconds. it is UTC time, not the number of seconds within 24 hours.
		int64_t time;
		std::map<std::string, std::string> input_vars;

		// below used by launcher. ros.add_timed_task() set them false.
		bool added;
		bool deleted;
	};
	virtual bool add_timed_task(const std::map<int64_t, tadd_timed_task>& tasks) = 0;
	virtual const std::map<tiot_device_key, tiot_device>& iot_devices() const = 0;
	// virtual void set_dcamera_points(const SDL_FPoint3* points, int size, const tdepth_sector_result& result) = 0;
	// virtual void laser_publish_scan(const sensor_msgs::LaserScan& msg) = 0;
	virtual lua_State* get_lua_State() const = 0;
	virtual void call_lua_breakpoint() = 0;
	virtual void aplt_add_msg_log(int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent) = 0;
	virtual bool navigation_node_started() const = 0;
	// virtual void public_vel(double linear_x, double linear_y, double angular_z) = 0;
	// virtual const trpy& get_imu_rpy() const = 0;
	virtual void aplt_set_env_var(int type, const config::attribute_value& val) = 0;
	virtual const aplt::ttask_var* aplt_get_env_var(int type) = 0;
	virtual void set_privacy_protect(bool enable) = 0;
	virtual bool is_privacy_protecting() const = 0;
	// base scene
	virtual const std::vector<tbase_scene>& aplt_base_scenes() const = 0;
	virtual std::pair<const tbase_scene*, int> aplt_curr_base_scene() const = 0;
	virtual const tbase_scene* aplt_set_base_scene(const tbase_scene& scene, int to_state, std::string& err_msg) = 0;
	
	// listen state
	virtual bool is_listening() const = 0;
	virtual bool is_listen_speaking() const = 0;
	virtual bool start_listen() = 0;
	virtual void stop_listen() = 0;
	virtual void listen_next_course() = 0;

	// floating window
	virtual void push_floating_window_task(const std::string& msg, int duration_ms) = 0;

	virtual void health_push_n32_event(int type, int ctx) = 0;
	virtual void health_push_str_event(int type, int ctx, const std::string& str, const std::string& aux_str) = 0;
	virtual void health_push_landmarks(const SDL_U16Point* landmarks, int unsatisfied_reason) = 0;

	virtual bool cswamp_addevent(int64_t ts, const std::string& desc, const std::vector<timage_pair>& images, bool quiet) = 0;
	virtual bool cswamp_querytablecooking(int table, net::tcswamp_table_result& result, bool quiet) = 0;
};

LIB3RDPARTY_DECL tb_api& get_b_api();

enum {bs_action_switch_to_next, bs_action_idle, bs_action_resume};
LIB3RDPARTY_DECL const tbase_scene* handle_base_scene(aplt::tb_api& ros, int desire_at, const std::string& name, int action, std::string& err_msg);

LIB3RDPARTY_DECL void add_timed_tasks_to_cfg(const std::map<int64_t, tb_api::tadd_timed_task>& tasks, config& cfg);
LIB3RDPARTY_DECL bool cfg_to_add_timed_tasks(const config& root_cfg, std::map<int64_t, tb_api::tadd_timed_task>& result);

}


#endif