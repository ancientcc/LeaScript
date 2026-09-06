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

#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "leagor_ai.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include "rose_ros/utils.hpp"
#include "../common.hpp"

using namespace std::placeholders;


namespace aplt {

tai_slot* ai_slot_singleton = nullptr;

class trevise_essay: public tai_slot::tfollowup
{
public:
	trevise_essay(const tai_slot::tscene& scene)
		: tfollowup(scene)
		, start_question_(tai_slot::tprompt(_("revise_essay^start_question"), _("revise_essay^start_tip")))
		, main_problem_question_(tai_slot::tprompt(_("revise_essay^main_problem_question"), _("revise_essay^main_problem_tip")))
		, method_example_question_(tai_slot::tprompt(_("revise_essay^method_example_question"), _("revise_essay^method_example_tip")))
		, make_better_question_(tai_slot::tprompt(_("revise_essay^make_better_question"), _("revise_essay^make_better_tip")))
		, is_start_(true)
	{
		reference_ = _("revise essay's reference");
	}

private:
	std::vector<tai_slot::tprompt> all_prompts() override;

	std::vector<tai_slot::tprompt> next_prompts() override;

	void feed_answer(const std::string& answer) override {}

	void back_starting_point() override 
	{
		is_start_ = true;
	}

private:
	const tai_slot::tprompt start_question_;
	const tai_slot::tprompt main_problem_question_;
	const tai_slot::tprompt method_example_question_;
	const tai_slot::tprompt make_better_question_;
	bool is_start_;
};

std::vector<tai_slot::tprompt> trevise_essay::all_prompts()
{
	std::vector<tai_slot::tprompt> result;

	result.push_back(start_question_);
	result.push_back(main_problem_question_);
	result.push_back(method_example_question_);
	result.push_back(make_better_question_);

	return result;
}

std::vector<tai_slot::tprompt> trevise_essay::next_prompts()
{
	std::vector<tai_slot::tprompt> result;

	if (is_start_) {
		result.push_back(start_question_);
		is_start_ = false;

	} else {
		result.push_back(main_problem_question_);
		result.push_back(method_example_question_);
		result.push_back(make_better_question_);
	}

	return result;
}

class tpoem_video: public tai_slot::tfollowup
{
public:
	tpoem_video(const tai_slot::tscene& scene)
		: tfollowup(scene)
		, storyboards_question_(tai_slot::tprompt(_("revise_essay^storyboards_question"), _("revise_essay^storyboards_tip")))
		, painting_prompts_question_(tai_slot::tprompt(_("revise_essay^painting_prompts_question"), _("revise_essay^painting_prompts_tip")))
		, is_start_(true)
	{
		reference_ = _("Make poem video's reference");
	}

private:
	std::vector<tai_slot::tprompt> all_prompts() override;

	std::vector<tai_slot::tprompt> next_prompts() override;

	void feed_answer(const std::string& answer) override {}

	void back_starting_point() override 
	{
		is_start_ = true;
	}

private:
	const tai_slot::tprompt storyboards_question_;
	const tai_slot::tprompt painting_prompts_question_;
	bool is_start_;
};

std::vector<tai_slot::tprompt> tpoem_video::all_prompts()
{
	std::vector<tai_slot::tprompt> result;

	result.push_back(storyboards_question_);
	result.push_back(painting_prompts_question_);

	return result;
}

std::vector<tai_slot::tprompt> tpoem_video::next_prompts()
{
	std::vector<tai_slot::tprompt> result;

	if (is_start_) {
		result.push_back(storyboards_question_);
		is_start_ = false;

	} else {
		result.push_back(storyboards_question_);
		result.push_back(painting_prompts_question_);
	}

	return result;
}

tleagor_ai::tleagor_ai(tslot_subscriber& _subscriber)
	: tai_slot(_subscriber)
	, pinyin_(aplt::get_curr_pinyin())
	, b_api_(aplt::get_b_api())
	// , ros_(aplt::get_r_api())
	// , tone_(0)
	// , eng_lowercase_(true)
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
	, revise_essay_id_("revise_essay")
	, poem_video_id_("poem_video")
	, nlp_model_type_(nlp_model_deepseek)
	// , task_event_(rose_create_event(false, false))
	, ds_finished_(false)
	, deepseek_(*this, curr_aplt->preferences_dir, cpp_id_mutex_)
	, qianfan_(*this, curr_aplt->preferences_dir)
	, ds_chatsrc_(nposm)
	, ds_retbool_(false)
{
	tscene revise_essay(revise_essay_id_, _("Revise essay"), scenetype_followup);
	scenes_.insert(std::make_pair(revise_essay.id, revise_essay));

	tscene poem_video(poem_video_id_, _("Make poem video"), scenetype_followup);
	scenes_.insert(std::make_pair(poem_video.id, poem_video));

	reload_ai_1fields();

	VALIDATE(ai_slot_singleton == nullptr, null_str);
	ai_slot_singleton = this;
}

tleagor_ai::~tleagor_ai()
{
	VALIDATE(ai_slot_singleton != nullptr, null_str);
	ai_slot_singleton = nullptr;

	if (ds_thread_.get() != nullptr) {
		stop_nlp_question();
	}

	if (curr_followup_ != nullptr) {
		delete curr_followup_;
		curr_followup_ = nullptr;
	}
}

tnlp_model& tleagor_ai::curr_nlp_model()
{
	if (nlp_model_type_ == nlp_model_deepseek) {
		return deepseek_;

	}
	VALIDATE(nlp_model_type_ == nlp_model_qianfan, null_str);
	return qianfan_;
}

void tleagor_ai::DoWork_deepseek(bool& exit, int src)
{
	VALIDATE(src < chatsrc_count && src != chatsrc_summary, null_str);
	VALIDATE(!ds_question_.empty(), null_str);

	tnlp_model& model = curr_nlp_model();
	model.send_question(b_api_, src, ds_new_conversation_, ds_question_, ds_surf_, exit);

	ds_finished_ = true;
}

tai_slot::tfollowup* tleagor_ai::app_create_scene(const std::string& id)
{
	VALIDATE(curr_followup_ == nullptr, null_str);

	if (id == revise_essay_id_) {
		return new trevise_essay(scenes_.find(revise_essay_id_)->second);

	} else if (id == poem_video_id_) {
		return new tpoem_video(scenes_.find(poem_video_id_)->second);
	}

	VALIDATE(false, null_str);
	return nullptr;
}

void tleagor_ai::app_slice()
{
	VALIDATE_IN_MAIN_THREAD();

	// 1/1) check deepseek
	if (ds_finished_) {
		stop_nlp_question();
	}
}

void tleagor_ai::send_nlp_question(int chatsrc, bool new_conversation, const std::string& question, const surface& surf,
	const std::function<void (bool retbool, const std::string& answer, int input_tokens, int output_tokens)>& did_nlp_answer)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(chatsrc != chatsrc_summary, null_str);
	VALIDATE(!question.empty(), null_str);

	if (ds_thread_.get() != nullptr) {
		stop_nlp_question();
	}
	VALIDATE(!ds_finished_, null_str);

	ds_chatsrc_ = chatsrc;
	ds_new_conversation_ = new_conversation;
	ds_question_ = question;
	ds_surf_ = surf;
	did_nlp_answer_ = did_nlp_answer;

	ds_thread_.reset(create_rose_thread(std::bind(&tleagor_ai::DoWork_deepseek, this, _1, ds_chatsrc_), NULL, NULL, NULL, "leagor_aiaget deepseek thread"));
}

bool tleagor_ai::is_nlp_questioning() const
{
	return ds_thread_.get() != 0;
}

void tleagor_ai::stop_nlp_question()
{
	VALIDATE(ds_thread_.get() != nullptr, null_str);

	ds_thread_.reset();

	tnlp_model& model = curr_nlp_model();
	subscriber_.aiagent_did_nlp_answer(ds_chatsrc_, model.ds_retbool(), model.ds_answer(), model.ds_input_tokens(), model.ds_output_tokens());

	if (did_nlp_answer_ != NULL) {
		did_nlp_answer_(model.ds_retbool(), model.ds_answer(), model.ds_input_tokens(), model.ds_output_tokens());
	}

	if (ds_chatsrc_ == chatsrc_speech) {
		pinyin_.speak(model.ds_answer());
	}

	ds_finished_ = false;
}

void tleagor_ai::reload_ai_1fields()
{
	trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

	deepseek_.set_api_key(aplt_prefs.get_str("ds_api_key"));
}

void tleagor_ai::fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id)
{
	VALIDATE_IN_MAIN_THREAD();

	if (cpp_id == cpp_id_sys_dlg_closed) {

	} else if (cpp_id == cpp_id_save_ai_1fields) {
		threading::lock lock(cpp_id_mutex_);
		reload_ai_1fields();
	}
}

}

void* aplt_create_ai_slot(void* _subscriber)
{
	aplt::tslot_subscriber* receier = reinterpret_cast<aplt::tslot_subscriber*>(_subscriber);

	utils::string_map symbols;
	aplt::tr_api& ros = aplt::get_r_api();

	aplt::tleagor_ai* leagor = new aplt::tleagor_ai(*receier);
/*
	if (!leagor->xfyun().libmsc_loaded()) {
		delete leagor;

		symbols["so"] = LIBROSEAPLT2_SO;
		ros.aplt_add_msg_log(time(nullptr), vgettext2("[Leagor]load xfyun's $so(libmsc) fail", symbols), 0, false);
		return nullptr;
	}
*/
	aplt::tai_slot* result = static_cast<aplt::tai_slot*>(leagor);
	return result;
}
