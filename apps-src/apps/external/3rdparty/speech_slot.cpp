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

#include "speech_slot.hpp"
#include "rose_exception.hpp"

using namespace std::placeholders;

namespace aplt {


tspeech_slot::tspeech_slot(tslot_subscriber& _receiver, int channels, int freq)
	: receiver(_receiver)
	, channels_(channels)
	, freq_(freq)
	, block_align_(0)
	, avg_bytes_per_sec_(0)
	, voice_event_(rose_create_event(false, false))
	, data_(nullptr)
	, data_size_(0)
	, data_vsize_(0)
	, allow_short_voice_(false)
{
	memset(&visual_info_, 0, sizeof(tvisual_info_C));

	VALIDATE(channels == 1, null_str);
	VALIDATE(freq == 8000 || freq == 16000 || freq == 32000, null_str);

	const int bits_per_sample = 16;
	block_align_ = channels * bits_per_sample / 8;
	avg_bytes_per_sec_ = freq * block_align_;
}

tspeech_slot::~tspeech_slot()
{
	// receiver = nullptr;

	if (data_ != nullptr) {
		free(data_);
		data_ = nullptr;
		data_size_ = 0;
		data_vsize_ = 0;
	}

	if (voice_event_ != nullptr) {
		delete voice_event_;
		voice_event_ = nullptr;
	}
}
/*
void tspeech_slot::set_receiver(treceiver& _receiver)
{
	VALIDATE(receiver == nullptr, null_str);
	receiver = &_receiver;
}
*/
void tspeech_slot::pre_start()
{
	// VALIDATE(receiver != nullptr, null_str);
}

void tspeech_slot::OnTriggerExit()
{
    voice_event_->Set();
}

void tspeech_slot::DoWork(bool& exit)
{
	// it is called in Speech-thread
	while (!exit) {
		OnWorkWhileStart();
		if (has_voice()) {
			const std::string result = recognize();
			// if (!result.empty()) {
			{
				// if result is empty, maybe recognize fail. in order to see fail reason, set voice_result_dirty_.
				threading::lock lock(voice_result_mutex_);
				voice_result_dirty_ = true;
				voice_result_ = result;
			}
		}

		voice_event_->Wait(trose_event::kForever);
	}
}

void tspeech_slot::resize_data(int size)
{
	size = posix_align_ceil(size, 4096);
	if (size > data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (data_ != nullptr) {
			if (data_vsize_) {
				memcpy(tmp, data_, data_vsize_);
			}
			free(data_);
		}
		data_ = tmp;
		data_size_ = size;
	}
}

}