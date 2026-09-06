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

#include "audio_capturer.hpp"
#include "wml_exception.hpp"
#include <SDL.h>

using namespace std::placeholders;


// ::sound::tpersist_xmit_audio_lock* persist_xmit_audio_lock_;

// wav file header
const wave_pcm_hdr default_wav_hdr = 
{
	{ 'R', 'I', 'F', 'F' },
	0,
	{'W', 'A', 'V', 'E'},
	{'f', 'm', 't', ' '},
	16, // fmt_size
	1,  // format_tag: PCM
	1,
	16000,
	32000,
	2, // block_align
	16,
	{'d', 'a', 't', 'a'},
	0  
};

void write_s16bit_wav(const std::string& filename, int channels, int freq, const uint8_t* data, int len)
{
	VALIDATE(data != nullptr && len > 0, null_str);

	wave_pcm_hdr wav_hdr = default_wav_hdr;
	VALIDATE(wav_hdr.bits_per_sample == 16, null_str);

	wav_hdr.channels = channels;
	wav_hdr.samples_per_sec = freq;
	wav_hdr.block_align = wav_hdr.channels * wav_hdr.bits_per_sample / 8;
	wav_hdr.avg_bytes_per_sec = freq * wav_hdr.block_align;

	wav_hdr.data_size = len;
	wav_hdr.size_8 = wav_hdr.data_size + (sizeof(wav_hdr) - 8);

	tfile file(filename, GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(file.valid(), null_str);

	posix_fwrite(file.fp, &wav_hdr, sizeof(wav_hdr));
	posix_fwrite(file.fp, data, len);
}

static void did_capture_audio(void* userdata, uint8_t* stream, int len)
{
	taudio_capturer* capturer = reinterpret_cast<taudio_capturer*>(userdata);
	capturer->did_capture_audio(stream, len);
}

void taudio_capturer::flush_wav_file()
{
	VALIDATE(wav_file_.get() != nullptr, null_str);
	tfile& file = *wav_file_.get();

	wav_hdr_.data_size = (int32_t)received_audio_bytes_;

	wav_hdr_.size_8 = wav_hdr_.data_size + (sizeof(wav_hdr_) - 8);
	/// overwrite new size_8
	posix_fseek(file.fp, 4);
	posix_fwrite(file.fp, &wav_hdr_.size_8, sizeof(wav_hdr_.size_8)); // write size_8 value

	// overwrite new data_size
	int wav_header_size = sizeof(wave_pcm_hdr);
	int wav_header_size_sub4 = wav_header_size - 4;
	VALIDATE(wav_header_size_sub4 == 40, null_str);

	posix_fseek(file.fp, wav_header_size_sub4);
	posix_fwrite(file.fp, &wav_hdr_.data_size, sizeof(wav_hdr_.data_size)); // write data_size value

	posix_fseek(file.fp, wav_header_size + (int32_t)received_audio_bytes_);
	no_flush_audio_times_ = 0;

	SDL_Log("flush_wav_file, wav_hdr_.size_8: %i wav_hdr_.data_size: %i", wav_hdr_.size_8, wav_hdr_.data_size);
}

void taudio_capturer::did_capture_audio(uint8_t* stream, int len)
{
	// it is called in SDLAudio thread

	received_audio_bytes_ += len;
	no_flush_audio_times_ ++;

	if (wav_file_.get() != nullptr) {
		posix_fwrite(wav_file_->fp, stream, len);
		if (no_flush_audio_times_ == 10) {
			flush_wav_file();
		}
	}

	uint32_t now = SDL_GetTicks();
    if (startup_verbose_ms_ == 0) {
		startup_verbose_ms_ = now;
		last_verbose_ms_ = now;
    }
    if (now - last_verbose_ms_ >= 10000) {
		uint32_t elapsed_second = (now - last_verbose_ms_) / 1000;
		// SDL_Log("[taudio_capturer]%s, #%i received %i byte during last %u second", 
		//	format_elapse_hms2((now - startup_verbose_ms_) / 1000, false).c_str(), no_flush_audio_times_, 
		//	(int)(received_audio_bytes_ - last_verbose_bytes_), elapsed_second);
		last_verbose_ms_ = now;
		last_verbose_bytes_ = received_audio_bytes_;
    }
}

void taudio_capturer::pre_start()
{
	received_audio_bytes_ = 0;
	no_flush_audio_times_ = 0;

	startup_verbose_ms_ = 0;
	last_verbose_ms_ = 0;
	last_verbose_bytes_ = 0;
}

void taudio_capturer::start_capture(const std::string& filename, int channels, int freq)
{
	// VALIDATE(persist_xmit_audio_lock_, null_str);
	// delete persist_xmit_audio_lock_;
	// persist_xmit_audio_lock_ = NULL;

	VALIDATE(channels == 1 || channels == 2, null_str);
	VALIDATE(freq == 8000 || freq == 16000 || freq == 32000 || freq == 44100 || freq == 48000, null_str);

	SDL_Log("[speech]start_capture, channels: %i freq: %i", channels, freq);
	VALIDATE(capture_audio_id_ == nposm, null_str);

	pre_start();

	SDL_AudioSpec desired, mixer;
	// Set the desired format and frequency
	desired.freq = freq;
	desired.format = AUDIO_S16LSB;
	desired.channels = channels;
	desired.samples = 4096 * desired.channels;
	desired.callback = ::did_capture_audio;
	desired.userdata = this;

	capture_audio_id_ = SDL_OpenAudioDevice(NULL, 1, &desired, &mixer, 0/*SDL_AUDIO_ALLOW_ANY_CHANGE*/);
	if (capture_audio_id_ < 0) {
		SDL_Log("[speech]start_capture fail, SDL_OpenAudioDevice fail");
		return;
	}
	using_spec_ = mixer;

	// generate wave_hdr
	wav_hdr_ = default_wav_hdr;
	wav_hdr_.channels = channels;
	wav_hdr_.samples_per_sec = freq;
	wav_hdr_.block_align = wav_hdr_.channels * wav_hdr_.bits_per_sample / 8;
	wav_hdr_.avg_bytes_per_sec = freq * wav_hdr_.block_align;

	if (!filename.empty()) {
		wav_file_.reset(new tfile(filename, GENERIC_WRITE, CREATE_ALWAYS));
		if (wav_file_->valid()) {
			posix_fwrite(wav_file_->fp, &wav_hdr_, sizeof(wav_hdr_));
		} else {
			wav_file_.reset();
			SDL_Log("start audio capture, Cannot create %s", filename.c_str());
		}
	}

	SDL_PauseAudioDevice(capture_audio_id_, 0);
}

void taudio_capturer::stop_capture()
{
	SDL_Log("[speech]stop_capture, time: %u ms", SDL_GetTicks() - startup_verbose_ms_);
	VALIDATE(capture_audio_id_ >= 0, null_str);
	// stop capture audio
	SDL_CloseAudioDevice(capture_audio_id_);
	capture_audio_id_ = nposm;

	if (wav_file_.get() != nullptr) {
		SDL_Log("[speech]post wav_file, received_audio_bytes_: %i", (int)received_audio_bytes_);
		VALIDATE(wav_file_.get() != nullptr, null_str);
		flush_wav_file();

		wav_file_.reset();
	}
}

const SDL_AudioSpec& taudio_capturer::using_spec() const 
{
	VALIDATE(capture_audio_id_ != nposm, null_str);
	return using_spec_;
}