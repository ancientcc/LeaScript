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

#ifndef LIBROSE_LEAGOR_SPEECH_HPP_INCLUDED
#define LIBROSE_LEAGOR_SPEECH_HPP_INCLUDED

#include "speech_slot.hpp"
#include <memory>
#include "xfyun.hpp"
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "aplt2.hpp"

namespace aplt {
class tleagor_req_task: public treq_task
{
public:
	std::string position_name;
	std::string task_name;
};

class tleagor_speech: public tspeech_slot
{
public:
	tleagor_speech(tslot_subscriber& receiver);
	const txfyun& xfyun() const { return xfyun_; }

private:
	void pre_start() override;
	void did_capture_audio(uint8_t* stream, int len) override;
	void slice() override;
	bool send_request(const std::string& req) override;

	bool has_voice() const override { return voice_len_ != nposm; }
	std::string recognize() override;
	void set_recognized();
	void reset_data_vars();

	//
	void DoWork(bool& exit) override;
	void OnWorkWhileStart() override;

	const tvisual_info_C& get_visual_info() override;

	size_t hit_task_from_pinyin(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const std::string& pinyin, const size_t off, bool allow_cpp, ttask_pair& pair) const;
	std::string plan_aplt_tasks_msg(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const tros_map& curmap, const std::map<aplt::taplt_task_key, aplt::taplt_task>& tasks);

	void reload_xfyun_3fields();
	void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) override;

private:
	aplt::tpinyin& pinyin_;
	aplt::tb_api& b_api_;
	aplt::tr_api& r_api_;
	txfyun xfyun_;
	tleagor_req_task req_task_;
	txf3params xf3params_;
	int16_t max_positive_;
	int16_t max_negative_;
	int max_data_vsize_;

	int maybe_start_pos_;
	int voice_len_;
	int last_voice_pos_;

	//
	// parse pinyin
	//
	const int tone_;
	const bool eng_lowercase_;

	std::string plan_tasks_;
	std::string next_tasks_;
};

}

#endif

