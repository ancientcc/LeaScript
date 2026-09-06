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

#ifndef LIBROSE_AUDIO_CAPTURER_HPP_INCLUDED
#define LIBROSE_AUDIO_CAPTURER_HPP_INCLUDED

// #include "util.hpp"
#include <SDL_audio.h>
#include <filesystem.hpp>
#include <memory>

#pragma pack(1)
// wav file header
// 0x0---0x7   | RIFF <size>
// 0x8---0x13  | WAVE fmt <size> | size general is 0x10.
// 0x14--0x24  | fmt
// 0x24--0x2c  | data <size>
struct wave_pcm_hdr
{
	char            riff[4];                // = "RIFF"
	int				size_8;                 // = FileSize - 8
	char            wave[4];                // = "WAVE"
	char            fmt[4];                 // = "fmt "
	int				fmt_size;				// = bytes of next chunk : 16

	short int       format_tag;             // = PCM : 1
	short int       channels;               // = channels : 1
	int				samples_per_sec;        // = freq : 8000 | 6000 | 11025 | 16000
	int				avg_bytes_per_sec;      // = samples_per_sec * bits_per_sample / 8
	short int       block_align;            // = channels * wBitsPerSample / 8
	short int       bits_per_sample;        // = 8 | 16

	char            data[4];                // = "data";
	int				data_size;              // = data bytes : FileSize - 44 
};
#pragma pack()

void write_s16bit_wav(const std::string& filename, int channels, int freq, const uint8_t* data, int len);

class taudio_capturer
{
public:
	taudio_capturer()
		: capture_audio_id_(nposm)
		, received_audio_bytes_(0)
		, no_flush_audio_times_(0)
		, startup_verbose_ms_(0)
		, last_verbose_ms_(0)
		, last_verbose_bytes_(0)
	{}
	virtual ~taudio_capturer()
	{
		if (is_capturing()) {
			stop_capture();
		}
	}
	posix_noncopyable(taudio_capturer);

	void start_capture(const std::string& filename, int channels, int freq);
	void stop_capture();
	bool is_capturing() const { return capture_audio_id_ != nposm; }
	const SDL_AudioSpec& using_spec() const;

	virtual void did_capture_audio(uint8_t* stream, int len);
	void flush_wav_file();

protected:
	virtual void pre_start();

protected:
	wave_pcm_hdr wav_hdr_;

private:
	SDL_AudioDeviceID capture_audio_id_;
	std::unique_ptr<tfile> wav_file_;
	int64_t received_audio_bytes_;
	int no_flush_audio_times_;

	SDL_AudioSpec using_spec_;

	uint32_t startup_verbose_ms_;
	uint32_t last_verbose_ms_;
	int64_t last_verbose_bytes_;
};

#endif

