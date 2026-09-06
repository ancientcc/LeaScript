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

#ifndef LIBROSE_SPEECH_SLOT_HPP_INCLUDED
#define LIBROSE_SPEECH_SLOT_HPP_INCLUDED

#include "rose_thread.hpp"
#include "3rdparty_decl.h"
#include "aplt_clazz.hpp"
#include "aplt2.hpp"
#include <memory>
#include <set>

namespace aplt {

// class treq_task;

class DECLSPEC tspeech_slot
{
public:
	struct tvisual_info_C
	{
		int voice_threshold;
		bool voice_maybe_start;
		bool allow_short_voice;
	};

	// tspeech_slot is created by aplt_create_speech_slot only, holp it no param, so don't pass @treceiver during constructor.
	// caller must call set_receiver() immediately after constructor.
	tspeech_slot(tslot_subscriber& receiver, int channels, int freq);
	virtual ~tspeech_slot();

	int channels() const { return channels_; }
	int freq() const { return freq_; }

	virtual void set_allow_short_voice(bool val) { allow_short_voice_ = val; }
	// bool allow_short_voice() const { return allow_short_voice_; }

	virtual void pre_start();
	virtual void did_capture_audio(uint8_t* stream, int len) = 0;

	virtual void OnWorkStart() {}
	virtual void OnWorkDone() {}
	virtual void OnTriggerExit();
	virtual void DoWork(bool& exit);

	virtual void slice() = 0;

	virtual bool send_request(const std::string& req) = 0;
	virtual const tvisual_info_C& get_visual_info() { return visual_info_; }

	virtual void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) {}

protected:
	void resize_data(int size);
	
	virtual void OnWorkWhileStart() {}
	virtual bool has_voice() const { return false; }
	virtual std::string recognize() { return ""; }

public:
	tslot_subscriber& receiver;

protected:
	const int channels_;
	const int freq_;
	int block_align_;
	int avg_bytes_per_sec_;

	threading::mutex data_mutex_;
	trose_event* voice_event_;
	uint8_t* data_;
	int data_size_;
	int data_vsize_;

	threading::mutex voice_result_mutex_;
	bool voice_result_dirty_;
	std::string voice_result_;

	tvisual_info_C visual_info_;

	bool allow_short_voice_;
};

}

#endif

