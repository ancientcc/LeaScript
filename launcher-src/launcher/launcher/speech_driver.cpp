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

#define GETTEXT_DOMAIN "launcher-lib"

#include "speech_driver.hpp"
#include "sound.hpp"
#include "rose_config.hpp"
#include "wml_exception.hpp"
#include "chinese.hpp"
#include "gettext.hpp"
#include "aplt.hpp"
#include "base_instance.hpp"
#include "cfg_cpp_api.hpp"

using namespace std::placeholders;


void tspeech_driver::tplay::did_played(int size)
{
	int text_size = text.size();
	VALIDATE(size >= 0 && size <= text_size, null_str);

	if (size < text_size) {
		text = text.substr(size);

	} else {
		text.clear();
	}
}

tspeech_driver::tspeech_driver(tdrivers& drivers, aplt::tcfg_cpp_api& cfg_cpp_api, const std::string& saves_courseware_dir)
	: gui2::tbase_courseware(saves_courseware_dir)
	, slot(nullptr)
	, drivers_(drivers)
	, cfg_cpp_api_(cfg_cpp_api)
	, pinyin_(chinese::curr_pinyin)
	, is_listening_(false)
	, listen_id_(speakid_nposm)
	, unplayed_aux_val_(nposm)
	, played_aux_val_(0)
{
	VALIDATE(unplayed_aux_val_ != played_aux_val_, null_str);
}

void tspeech_driver::set_slot(const std::string& _aplt_id, aplt::tspeech_slot* _slot)
{
	if (slot != nullptr) {
		stop();
		delete slot;
		slot = nullptr;
	} else {
		VALIDATE(!is_capturing(), null_str);
	}

	if (_slot != nullptr) {
		VALIDATE(!_aplt_id.empty(), null_str);
		slot = _slot;
		// slot->set_receiver(*this);

	} else {
		VALIDATE(_aplt_id.empty(), null_str);
	}
	aplt_id_ = _aplt_id;

	if (slot != nullptr) {
		start();
	}
}

void tspeech_driver::pre_start()
{
	slot->pre_start();
	taudio_capturer::pre_start();
}

void tspeech_driver::did_capture_audio(uint8_t* stream, int len)
{
	// it is called in SDLAudio thread
	// 
	slot->did_capture_audio(stream, len);

	taudio_capturer::did_capture_audio(stream, len);
}

void tspeech_driver::start()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);

	// const std::string fname = game_config::preferences_dir + "/speech.wav";
	const std::string fname = null_str;
	start_capture(fname, slot->channels(), slot->freq());
	if (is_capturing()) {
		thread_.reset(new net::tworker(std::bind(&aplt::tspeech_slot::DoWork, slot, _1), std::bind(&aplt::tspeech_slot::OnWorkStart, slot), 
			std::bind(&aplt::tspeech_slot::OnWorkDone, slot), std::bind(&aplt::tspeech_slot::OnTriggerExit, slot), "SpeechThread"));
	}
}

void tspeech_driver::stop()
{
	VALIDATE_IN_MAIN_THREAD();
	if (!is_capturing()) {
		VALIDATE(thread_.get() == nullptr, null_str);
		return;
	}

	thread_.reset();
	stop_capture();
}

void tspeech_driver::restart()
{
	VALIDATE_IN_MAIN_THREAD();
	if (aplt_id_.empty()) {
		VALIDATE(slot == nullptr, null_str);
		return;
	}

	VALIDATE(slot != nullptr, null_str);
	set_slot(null_str, nullptr);
	drivers_.refresh(false);
}

void tspeech_driver::slice()
{
	if (is_listening()) {
		if (!pinyin_.is_speaking()) {
			if (play_.text.empty()) {
				int at = pick_course(nullptr, courseware_);
				if (at != nposm) {
					// SDL_Log("{listen}tspeech_driver::slice, call play_course");
					play_course(at, courseware_, play_);

				} else {
					stop_listen();
				}

			} else {
				listen_id_ = pinyin_.speak(play_.text);
			}
		}
	}

	if (slot != nullptr) {
		slot->slice();
	}
}

void tspeech_driver::set_allow_short_voice(bool val)
{
	VALIDATE(slot != nullptr, null_str);
	slot->set_allow_short_voice(val);
}

const aplt::tspeech_slot::tvisual_info_C& tspeech_driver::get_visual_info()
{
	VALIDATE(slot != nullptr, null_str);
	return 	slot->get_visual_info();
}

bool tspeech_driver::send_request(const std::string& req)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(slot != nullptr, null_str);

	return slot->send_request(req);
	// return true;
}

void tspeech_driver::pinyin_did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start)
{
	VALIDATE_IN_MAIN_THREAD();

	if (id != listen_id_) {
		return;
	}
	VALIDATE(is_listening(), null_str);

	// remenber unplaed_start.
	VALIDATE(unplayed_start <= text.size(), null_str);
	
	const std::string substr = text.substr(0, unplayed_start);
	VALIDATE(utils::is_utf8str(substr.c_str(), substr.size()), null_str);
	VALIDATE(text == play_.text, null_str);

	// SDL_Log("{listen}pinyin_did_speak_stopped, listen_id_: %u, unplayed_start: %i, str: %s", 
	//	listen_id_, unplayed_start, substr.c_str());

	play_.did_played(unplayed_start);
	listen_id_ = speakid_nposm;
}

int tspeech_driver::pick_course(const aplt::tcourselist::tcourse* desire_course, aplt::tcourseware& courseware)
{
	VALIDATE(!pinyin_.is_speaking(), null_str);
	VALIDATE(listen_id_ == speakid_nposm, null_str);

	courseware.clear();

	const aplt::tcourselist& courselist = cfg_cpp_api_.courselist();
	const std::vector<aplt::tcourselist::tcourse>& courses = courselist.courses();
	VALIDATE(pick_aux_.size() == courses.size(), null_str);

	if (courses.empty()) {
		// VALIDATE(!is_listening_, null_str);
		VALIDATE(desire_course == nullptr, null_str);
		return nposm;
	}

	int at = 0;
	const int course_size = courses.size();
	if (desire_course == nullptr) {
		std::vector<int> candidates;
		int at2 = 0;
		for (std::vector<int>::const_iterator it = pick_aux_.begin(); it != pick_aux_.end(); ++ it, at2 ++) {
			int val = *it;
			if (val == unplayed_aux_val_) {
				candidates.push_back(at2);
			}
		}
		if (candidates.empty()) {
			// all played
			pick_aux_.assign(course_size, unplayed_aux_val_);
			for (at2 = 0; at2 < course_size; at2 ++) {
				candidates.push_back(at2);
			}
		}

		std::set<int> invalids;

		while (invalids.size() != candidates.size()) {
			at = candidates[rand() % candidates.size()];
			while (invalids.count(at) != 0) {
				at = candidates[rand() % candidates.size()];
			};
			const aplt::tcourselist::tcourse& course = courses[at];

			const std::string dir_name = course.to_dir_name();
			const std::string cfgfile = join_main_cfg_filename_couselist(course.uid, dir_name);

			load_courseware_cfg2(cfgfile, courseware);
			if (courseware.valid()) {
				break;

			} else {
				invalids.insert(at);

				if (invalids.size() == candidates.size() && candidates.size() < course_size) {
					// all played
					pick_aux_.assign(course_size, unplayed_aux_val_);
					candidates.clear();
					for (at2 = 0; at2 < course_size; at2 ++) {
						candidates.push_back(at2);
					}
				}
			}
		}

	} else {
		at = cfg_cpp_api_.course_which_at(desire_course->uid, desire_course->title);
		VALIDATE(at != nposm, null_str);

		const std::string dir_name = desire_course->to_dir_name();
		const std::string cfgfile = join_main_cfg_filename_couselist(desire_course->uid, dir_name);
		load_courseware_cfg2(cfgfile, courseware);
	}

	if (!courseware.valid()) {
		return nposm;
	}

	return at;
}

void tspeech_driver::play_course(int at, const aplt::tcourseware& courseware, tplay& play)
{
	VALIDATE(!pinyin_.is_speaking(), null_str);
	VALIDATE(listen_id_ == speakid_nposm, null_str);

	VALIDATE(at >= 0 && at < (int)pick_aux_.size(), null_str);
	VALIDATE(pick_aux_.size() == cfg_cpp_api_.courselist().courses().size(), null_str);

	VALIDATE(courseware.valid(), null_str);

	VALIDATE(pick_aux_[at] == unplayed_aux_val_, null_str);
	pick_aux_[at] = played_aux_val_;

	const std::string text = courseware.text_for_listen();
	play.set_session(text);

	listen_id_ = pinyin_.speak(play.text);
}

bool tspeech_driver::start_listen(const aplt::tcourselist::tcourse* course)
{
	VALIDATE(!is_listening_, null_str);
	pick_aux_.assign(cfg_cpp_api_.courselist().courses().size(), unplayed_aux_val_);

	if (pinyin_.is_speaking()) {
		pinyin_.stop_speak2();
	}

	int at = pick_course(course, courseware_);
	if (at == nposm) {
		return false;
	}

	play_course(at, courseware_, play_);
	is_listening_ = true;

	return true;
}

void tspeech_driver::stop_listen()
{
	VALIDATE(is_listening_, null_str);
	is_listening_ = false;
	listen_id_ = speakid_nposm;

	if (pinyin_.is_speaking()) {
		pinyin_.stop_speak2();
	}
}

void tspeech_driver::listen_next_course()
{
	VALIDATE(is_listening_, null_str);
	listen_id_ = speakid_nposm;
	play_.set_session(null_str);

	pinyin_.stop_speak2();
}

void tspeech_driver::listen_did_course_changed(bool insert, int at)
{
	if (!is_listening_) {
		return;
	}

	int s = cfg_cpp_api_.courselist().courses().size();
	std::vector<int>::iterator it = pick_aux_.begin();
	if (at != 0) {
		std::advance(it, at);
	}
	if (insert) {
		VALIDATE(at >= 0 && at <= pick_aux_.size(), null_str);
		VALIDATE(pick_aux_.size() == s - 1, null_str);
		
		pick_aux_.insert(it, unplayed_aux_val_);

	} else {
		VALIDATE(at >= 0 && at < pick_aux_.size(), null_str);
		VALIDATE(pick_aux_.size() == s + 1, null_str);
		pick_aux_.erase(it);
	}

	VALIDATE(pick_aux_.size() == s, null_str);
}