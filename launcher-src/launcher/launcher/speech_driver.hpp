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

#ifndef SPEECH_DRIVER_HPP_INCLUDED
#define SPEECH_DRIVER_HPP_INCLUDED

#include "speech_slot.hpp"
#include "audio_capturer.hpp"
#include <memory>
#include "thread.hpp"
#include "wml_exception.hpp"
#include "drivers.hpp"
#include "gui/dialogs/base_courseware.hpp"
#include "cfg_cpp_api.hpp"


class tspeech_driver: public taudio_capturer, public gui2::tbase_courseware
{
public:
	tspeech_driver(tdrivers& drivers, aplt::tcfg_cpp_api& cfg_cpp_api, const std::string& saves_courseware_dir);

	~tspeech_driver()
	{
		if (is_listening()) {
			stop_listen();
		}

		stop();
		if (slot != nullptr) {
			delete slot;
			slot = nullptr;
		}
	}
	posix_noncopyable(tspeech_driver);

	void set_slot(const std::string& _aplt_id, aplt::tspeech_slot* slot);
	const std::string& aplt_id() const { return aplt_id_; }
	bool installed() const { return slot != nullptr; }

	void start();
	void stop();
	void restart();
	void slice();

	void set_allow_short_voice(bool val);

	const aplt::tspeech_slot::tvisual_info_C& get_visual_info();
	bool send_request(const std::string& req);

	// tbase_msg_subscriber
	void pinyin_did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start);

	bool start_listen(const aplt::tcourselist::tcourse* course);
	void stop_listen();
	void listen_next_course();
	bool is_listening() const { return is_listening_; }
	bool is_listen_speaking() const
	{
		if (listen_id_ != speakid_nposm) {
			VALIDATE(is_listening_, null_str);
		} else {
			// maybe in unlisten state.
			// VALIDATE(!is_listening_, null_str);
		}
		return listen_id_ != speakid_nposm;
	}
	void listen_did_course_changed(bool insert, int at);

private:
	void pre_start() override;
	void did_capture_audio(uint8_t* stream, int len) override;

	int pick_course(const aplt::tcourselist::tcourse* desire_course, aplt::tcourseware& courseware);

	struct tplay
	{
		tplay()
			: unplayed_at(nposm)
		{}

		void set_session(const std::string& _text)
		{
			// VALIDATE(!_text.empty(), null_str);
			text = _text;
		}

		void did_played(int size);

		std::string text;
		int unplayed_at;
	};
	void play_course(int at, const aplt::tcourseware& courseware, tplay& play);

public:
	aplt::tspeech_slot* slot;

private:
	tdrivers& drivers_;
	aplt::tcfg_cpp_api& cfg_cpp_api_;
	aplt::tpinyin& pinyin_;

	// aplt::tspeech_slot::treceiver& receiver_;
	std::unique_ptr<net::tworker> thread_;
	std::string aplt_id_;

	bool is_listening_;
	uint32_t listen_id_;
	aplt::tcourseware courseware_;
	tplay play_;
	const int unplayed_aux_val_;
	const int played_aux_val_;
	std::vector<int> pick_aux_;
};


#endif

