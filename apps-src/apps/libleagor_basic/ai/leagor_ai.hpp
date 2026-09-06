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

#ifndef LIBLEAGOR_BASIC_LEAGOR_AI_HPP
#define LIBLEAGOR_BASIC_LEAGOR_AI_HPP

#include "base_slot.hpp"
#include <memory>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_ros/aplt.hpp"
#include "qianfan.hpp"
#include "aplt2.hpp"
#include "rose_sdl_utils.hpp"

namespace aplt {

enum {nlp_model_deepseek, nlp_model_qianfan};

class tleagor_ai: public tai_slot
{
public:
	tleagor_ai(tslot_subscriber& receiver);
	~tleagor_ai();

private:
	tfollowup* app_create_scene(const std::string& id) override;

	void app_slice() override;
	void send_nlp_question(int chatsrc, bool new_conversation, const std::string& question, const surface& surf,
		const std::function<void (bool retbool, const std::string& answer, int input_tokens, int output_tokens)>& did_nlp_answer) override;
	bool is_nlp_questioning() const override;

	void DoWork_deepseek(bool& exit, int src);
	void stop_nlp_question() override;

	tnlp_model& curr_nlp_model();
	void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) override;

	void reload_ai_1fields();

private:
	aplt::tpinyin& pinyin_;
	aplt::tb_api& b_api_;
	// aplt::tr_api& r_api_;
	// txf3params xf3params_;
	// 
	//
	// parse pinyin
	//
	const int tone_;
	const bool eng_lowercase_;

	threading::mutex cpp_id_mutex_;

	const std::string revise_essay_id_;
	const std::string poem_video_id_;

	// threading::mutex task_data_mutex_;
	// trose_event* task_event_;
	// bool task_result_dirty_;
	// enum {taskid_deepseek};
	// int taskid_;

	int nlp_model_type_;
	std::unique_ptr<trose_thread> ds_thread_;
	bool ds_finished_;
	tdeepseek deepseek_;
	tqianfan qianfan_;
	int ds_chatsrc_;
	bool ds_new_conversation_;
	std::string ds_question_;
	surface ds_surf_;

	bool ds_retbool_;
	std::string ds_answer_;
};

}

#endif

