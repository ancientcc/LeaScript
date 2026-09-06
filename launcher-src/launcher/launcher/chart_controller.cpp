/* $Id: editor_controller.cpp 47755 2010-11-29 12:57:31Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/
#define GETTEXT_DOMAIN "launcher-lib"

#include "chart_controller.hpp"
#include "chart_display.hpp"

#include "SDL_mixer.h"
#include "gettext.hpp"
#include "integrate.hpp"
#include "formula_string_utils.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "filesystem.hpp"
#include "hotkeys.hpp"
#include "config_cache.hpp"
#include "preferences_display.hpp"
#include "gui/dialogs/chart_scene.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/browse.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/scroll_panel.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "serialization/parser.hpp"
#include "sdl_image.h"

using namespace std::placeholders;

const twave_pcm_hdr_44bytes to_wav_hdr = 
{
	{ 'R', 'I', 'F', 'F' },
	0,
	{'W', 'A', 'V', 'E'},
	{'f', 'm', 't', ' '},
	16,
	1,
	1,
	16000,
	32000,
	2,
	16,
	{'d', 'a', 't', 'a'},
	0  
};


#define calculate_interval(music_during, map_area_w, max_map_area_w)	\
	(1.0 * (music_during) * (map_area_w) / (max_map_area_w))

tnot_main_base_msg_subscriber* capture_audio2_singleton = nullptr;

int chart_controller::original_width = 12; // MAX_USED_UNITS
int chart_controller::original_height = 8;

int chart_controller::calculate_divisor(int sum, size_t count)
{
	VALIDATE(count, null_str);

	if (count == 1) {
		return sum;
	}
	int divisor = sum / count;
	if (sum % count) {
		// divisor is 0---N-2 units, in other word, except last unit.
		divisor = sum / (count - 1);
	}
	return divisor;
}

void verbose_speech_wav(tuint8data_C& result)
{
	VALIDATE(game_config::os == os_windows, null_str);

	tfile file(game_config::preferences_dir + "/speech.wav", GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data();
	VALIDATE(fsize > sizeof(twave_pcm_hdr_44bytes), null_str);

	const uint8_t* abuf = (const uint8_t*)(file.data + sizeof(twave_pcm_hdr_44bytes));
	int alen = fsize - sizeof(twave_pcm_hdr_44bytes);

	for (int at = 0; at < 200; at +=2) {
		int val = posix_mki16(abuf[at], abuf[at + 1]);
		SDL_Log("#%i val: %i", at / 2, val);
	}

	result.ptr = (uint8_t*)malloc(alen);
	memcpy(result.ptr, abuf, alen);
}

chart_controller::chart_controller(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tspeech_driver& speech_driver, const config& app_cfg, CVideo& video)
	: base_controller(SDL_GetTicks(), app_cfg, video)
	, rdpd_mgr_(rdpd_mgr)
	, pble_(pble)
	, privacy_(privacy)
	, speech_driver_(speech_driver)
	, gui_(nullptr)
	, dlg_(nullptr)
	, window_(nullptr)
	, voice_maybe_start_widget_(nullptr)
	, map_(null_str)
	, units_(*this, map_, false)
	, frequency_lock_(NULL)
	, hook_music_finished_(NULL)
	// , hook_music_playing_(NULL)
	, wav_file_name_(game_config::preferences_dir + "/speech.wav")
	, music_during_(0)
	, play_from_(nposm)
	, played_(nposm)
	, notify_finished_(false)
	, disable_cursor_pos_(0)
	, cursor_draging_(false)
	, start_drag_x_(nposm)
	, seting_gain_(false)
	, last_drag_x_(0)
	, first_coordinate_(nposm, nposm)
	, current_gain_(0)
	, mercury_height_(nposm)
	, next_1second_ticks_(0)
	, allow_draw_(false)
	, show_mic_wave_(true)
	, show_nagtive_(false)
	, avg_bytes_per_sec_(0)
	, speech_data_(nullptr)
	, speech_data_size_(0)
	, speech_data_vsize_(0)
	, filled_units_(0)
	, max_rec_speech_bytes_(1024 * 1024) // 1M bytes
	, rec_speech_bytes_(0)
	, my_visual_info_(aplt::tspeech_slot::tvisual_info_C{nposm, false, false})
	, curr_visual_info_(my_visual_info_)
	, dbg_speech_data_(tuint8data_C{nullptr, 0})
	, dbg_speech_data_byte_pos_(0)
{
	if (show_mic_wave_ && game_config::os == os_windows) {
		// verbose_speech_wav(dbg_speech_data_);
	}

	if (show_mic_wave_) {
		const SDL_AudioSpec& using_spec = speech_driver_.using_spec();
		const int bits_per_sample = 16;
		int block_align_ = using_spec.channels * bits_per_sample / 8;
		avg_bytes_per_sec_ = using_spec.freq * block_align_;

	} else {
		VALIDATE(game_config::os == os_windows, null_str);
		VALIDATE(!speech_driver_.installed(), null_str);

		wav_file_.reset(new tfile(wav_file_name_, GENERIC_READ, OPEN_EXISTING));
		VALIDATE(wav_file_->valid(), null_str);

		pinch_noisc_time_ = 800;
/*	
		if (caf_.valid()) {
			if (!frequency_lock_) {
				frequency_lock_ = new sound::tfrequency_lock(8000, 1024);
			}
			valid_caf_file(caf_, &caf_data_offset_, &caf_data_size_);
		}
*/
		tfile& wav_file = *wav_file_.get();
		if (wav_file.valid()) {
			int fsize = posix_fsize(wav_file.fp);

			posix_fseek(wav_file.fp, 0);
			posix_fread(wav_file.fp, &wav_hdr_, sizeof(wav_hdr_));

			VALIDATE(fsize == sizeof(wav_hdr_) + wav_hdr_.data_size, null_str);
		}

		original_width = 32;
/*
		int frequency, channels;
		Uint16 format;
		int numtimesopened = Mix_QuerySpec(&frequency, &format, &channels);
		VALIDATE(numtimesopened > 0, "Must open SDL audio!");
		audio_bytes_per_sec_ = ((format & SDL_AUDIO_MASK_BITSIZE) / 8) * frequency * channels;
		sound_buffer_size_ = 1.0 * sound::get_buffer_size() * audio_bytes_per_sec_ / 8000;
*/
	}
	map_ = tmap(generate_map_data(original_width, original_height, false, square_terrain_blue));

	units_.create_coor_map(map_.w(), map_.h());
}

chart_controller::~chart_controller()
{
	{
		// why execute below as soon as enter, see 'game_instance::speech_did_capture_audio'.
		threading::lock lock(subscriber_mutex_);
		capture_audio2_singleton = nullptr;
	}

	if (rec_file_.get() != nullptr) {
		stop_rec(true);
	}

	if (!show_mic_wave_) {
		// in order to sync more safe and sample, place Mix_FreeMusic at main thread!
		stop_music();
		sound::set_music_volume(0);
	}

	if (frequency_lock_) {
		delete frequency_lock_;
		frequency_lock_ = NULL;
	}

	if (gui_) {
		delete gui_;
		gui_ = NULL;
	}

	if (speech_data_ != nullptr) {
		free(speech_data_);
		speech_data_ = nullptr;
		speech_data_size_ = 0;
		speech_data_vsize_ = 0;
	}

	if (dbg_speech_data_.ptr != nullptr) {
		free(dbg_speech_data_.ptr);
	}
}

void chart_controller::app_create_display(int initial_zoom)
{
	gui_ = new chart_display(rdpd_mgr_, pble_, privacy_, *this, units_, video_, map_, initial_zoom);
}

void chart_controller::app_post_initialize()
{
	dlg_ = static_cast<gui2::tchart_scene*>(gui_->get_theme());
	window_ = dlg_->get_window();

	gui2::ttoggle_button* toggle = gui2::find_widget<gui2::ttoggle_button>(window_, "show_nagtive", false, true);
	toggle->set_did_state_changed(std::bind(&chart_controller::did_show_nagtive_changed, this, _1));
	toggle->set_value(show_nagtive_);

	voice_maybe_start_widget_ = gui2::find_widget<gui2::tcontrol>(window_, "icon_voice_maybe_start", false, true);

	if (!show_mic_wave_) {
		// initial set cursor at middle of view.
/*
		if (analysis_.audio_samples()) {
			music_during_ = analysis_.music_duration();
		} else {
			music_during_ = mdat_header_.duration;
		}
*/	
		const SDL_Rect& _max_map_area = gui_->main_map_rect();
		const SDL_Rect& _map_area = gui_->main_map_view_rect();
		int offset = _map_area.w / 2;
		time_t time = 1.0 * offset * music_during_ / _max_map_area.w;
		gui_->set_cursor_pos(offset, time);

		// fill chart_data at current zoom.
		fill_unit_pixel();
		gui_->recalculate_minimap();
/*
		std::stringstream ss;
		gui2::tlabel* label = dynamic_cast<gui2::tlabel*>(gui_->get_theme_object("date"));
		ss << _("Capture time") << ": " << format_time_ymd(current_chart_time);
		ss << "(" << format_time_hm(current_chart_time) << "-" << format_time_hm(current_chart_time + music_during_) << ")";
		ss << "\n";
		ss << _("Duration") << ": " << format_elapse_hms2(music_during_);
		label->set_label(ss.str());
*/

	} else {
		const SDL_AudioSpec& using_spec = speech_driver_.using_spec();
		VALIDATE(using_spec.channels == 1, null_str);
		VALIDATE(using_spec.format == AUDIO_S16LSB, null_str);
		const int bits_per_sample = 16;

		utils::string_map symbols;
		symbols["channel"] = _("Mono");
		symbols["freq"] = str_cast(using_spec.freq);
		symbols["bitspersample"] = str_cast(bits_per_sample);

		std::string msg = vgettext2("$channel, $freq Hz\n$bitspersample|bit PCM", symbols);
		set_format_report(msg);

		set_status_report(default_status_msg_);

		update_rec_ui(false);

		// set_voice_maybe_start_report(true);
	}

	{
		// gui2::tbutton* play = dynamic_cast<gui2::tbutton*>(gui_->get_theme_object("play"));
		// play->set_active(false);
	}
}

void chart_controller::app_play_slice()
{
	// SDL_Log("%u app_play_slice", SDL_GetTicks());

	if (show_mic_wave_) {
		if (speech_data_vsize_ == 0) {
			return;
		}

		curr_visual_info_ = speech_driver_.get_visual_info();
		if (curr_visual_info_.voice_maybe_start != my_visual_info_.voice_maybe_start) {
			set_icon_report(gui2::tchart_scene::VOICE_MAYBE_START, curr_visual_info_.voice_maybe_start);
			my_visual_info_.voice_maybe_start = curr_visual_info_.voice_maybe_start;
		}
		if (curr_visual_info_.allow_short_voice != my_visual_info_.allow_short_voice) {
			set_icon_report(gui2::tchart_scene::ALLOW_SHORT_VOICE, curr_visual_info_.allow_short_voice);
			my_visual_info_.allow_short_voice = curr_visual_info_.allow_short_voice;
		}

		threading::lock lock(subscriber_mutex_);
		speech_fill_mic_data(speech_driver_.using_spec());
	}

	uint32_t now = SDL_GetTicks();
    if (now > next_1second_ticks_) {
        dlg_->refresh_statusbar_grid(now);

        const int threshold_1s = 1000;
        next_1second_ticks_ = now + threshold_1s;
    }

	// SDL_Delay(50);
}

events::mouse_handler_base& chart_controller::get_mouse_handler_base()
{
	return *this;
}

bool chart_controller::finger_coordinate_valid(int x, int y) const
{
	if (!base_controller::finger_coordinate_valid(x, y)) {
		return false;
	}
	if (cursor_draging_) {
		return false;
	}
	return true;
}

void chart_controller::slice_before_scroll()
{
	if (played_ != nposm) {
		update_play_cursor(played_);
		played_ = nposm;
	}
	if (notify_finished_) {
		notify_finished_ = false;
		update_play_ui(false);
	} 
}

void chart_controller::app_execute_command(int command, const std::string& sparam)
{
	using namespace gui2;
	chart_unit* u = units_.find_unit(selected_hex_);

	switch (command) {
		case tchart_scene::HOTKEY_RETURN:
			goto_title();
			break;

		case tchart_scene::HOTKEY_RECORD:
			click_record();
			break;

		case tchart_scene::HOTKEY_PLAY:
			play_music(wav_file_name_);
			break;

		case HOTKEY_ZOOM_IN:
			gui_->set_zoom(gui_->zoom());
			break;
		case HOTKEY_ZOOM_OUT:
			gui_->set_zoom(- gui_->zoom() / 2);
			break;

		case HOTKEY_SYSTEM:
			break;

		default:
			base_controller::app_execute_command(command, sparam);
	}
}

void chart_controller::app_first_drawn()
{
	if (!show_mic_wave_) {
		return;
	}

	SDL_Rect widget_rect = gui_->main_map_widget_rect();

	int zoom = gui_->zoom();

	int map_w = (widget_rect.w - 64) / zoom;
	int map_h = (widget_rect.h - 64) / zoom;

	if (map_h & 1) {
		// make sure leve_0 at grid's line.
		map_h -= 1;
	}

	if (map_w != map_.w() || map_h != map_.h()) {
		reload_map(map_w, map_h);

		original_width = map_.w();
		original_height = map_.h();

	} else {
		VALIDATE(original_width == map_.w(), null_str);
		VALIDATE(original_height == map_.h(), null_str);
	}

	VALIDATE(!allow_draw_, null_str);
	allow_draw_ = true;

	VALIDATE(capture_audio2_singleton == nullptr, null_str);
	capture_audio2_singleton = this;
}

void chart_controller::app_resize_screen()
{
	gui_->app_resize_screen();
}

void chart_controller::reload_map(int w, int h)
{
	VALIDATE(filled_units_ == 0, null_str);

    const int original_w = map_.w();
    const int original_h = map_.h();

	map_ = tmap(generate_map_data(w, h, false, square_terrain_blue));
	gui_->reload_map();
	units_.create_coor_map(map_.w(), map_.h());

    // VALIDATE(w * h == units_.size() * UNIT_LOCS * UNIT_LOCS, null_str);

    std::stringstream ss;
    ss << "reload_map(" << w << ", " << h << ")";
    // units_.dump(ss.str());
}

void chart_controller::set_format_report(const std::string& msg)
{
    gui_->refresh_report(gui2::tchart_scene::FORMAT, 
        reports::report(msg, null_str));
}

void chart_controller::set_status_report(const std::string& msg)
{
    gui_->refresh_report(gui2::tchart_scene::STATUS, 
        reports::report(msg, null_str));
}

void chart_controller::set_icon_report(int num, bool visible)
{
	VALIDATE(num == gui2::tchart_scene::VOICE_MAYBE_START || num == gui2::tchart_scene::ALLOW_SHORT_VOICE, null_str);
	surface surf;
	if (visible) {
		if (num == gui2::tchart_scene::VOICE_MAYBE_START) {
			surf = image::get_image("misc/valid_speech.png");

		} else {
			VALIDATE(num == gui2::tchart_scene::ALLOW_SHORT_VOICE, null_str);
			surf = image::get_image("misc/short_speech.png");
		}
	}

	gui_->refresh_report(num, reports::report(surf));	 
}

void chart_controller::speech_resize_data(int size)
{
	size = posix_align_ceil(size, 4096);
	if (size > speech_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (speech_data_ != nullptr) {
			if (speech_data_vsize_) {
				memcpy(tmp, speech_data_, speech_data_vsize_);
			}
			free(speech_data_);
		}
		speech_data_ = tmp;
		speech_data_size_ = size;
	}
}

void chart_controller::speech_did_recognition_result2(const std::string& result)
{
	std::stringstream ss;
	ss << utils::format_time_hms(time(nullptr)) << " " << result;

	set_status_report(ss.str());
}

void chart_controller::speech_did_capture_audio2(const uint8_t* stream, int len)
{
	VALIDATE(show_mic_wave_, null_str);

	// it is called in SDLAudio-thread
	// threading::lock lock(speech_data_mutex_);

	const int bytes_per_sec = avg_bytes_per_sec_;
	const int max_cached_silent_bytes = bytes_per_sec * 3;

	// if (maybe_start_pos_ == nposm && data_vsize_ > max_cached_silent_bytes) {
	if (speech_data_vsize_ > max_cached_silent_bytes) {
		memcpy(speech_data_, speech_data_ + max_cached_silent_bytes, speech_data_vsize_ - max_cached_silent_bytes);
		SDL_Log("%u {speech_did_capture_audio2}consume too slow, transcat vsize from %i to %i", SDL_GetTicks(), speech_data_vsize_, speech_data_vsize_ - max_cached_silent_bytes);
		speech_data_vsize_ -= max_cached_silent_bytes;
	}

	speech_resize_data(speech_data_vsize_ + len);
	memcpy(speech_data_ + speech_data_vsize_, stream, len);
	speech_data_vsize_ += len;
}

void chart_controller::speech_fill_mic_data(const SDL_AudioSpec& using_spec)
{
	VALIDATE(speech_data_vsize_ > 0, null_str);

	VALIDATE(filled_units_ != original_width, nullptr);
	if (filled_units_ == original_width) {
		const int get_rid_of_bytes = speech_data_vsize_;
		memcpy(speech_data_, speech_data_ + get_rid_of_bytes, speech_data_vsize_ - get_rid_of_bytes);
		speech_data_vsize_ -= get_rid_of_bytes;
		return;
	}


	// min zoom is 64.
	const int unit_h = gui_->max_unit_height(gui_->main_map_view_rect().h);
	int zoom = gui_->zoom();
	int samples_per_pixel;
	// int used_units;
	int pixels_per_unit;
	bool full_pixel = true;

	samples_per_pixel = 16;
	// samples_per_pixel = 1;

	// used_units = original_width; // ??
	pixels_per_unit = zoom;


	int64_t last_fread_byte = 0; // data pos, not file pos!

	// point_per_unit semantics
	// full_pixel == true: pixels per unit
	// full_pixel == false: samples per unit
	int point_per_unit = pixels_per_unit;
	const int bits_per_sample = 16;
	const int sample_bytes = bits_per_sample / 2;

	int used_units = speech_data_vsize_ / (samples_per_pixel * pixels_per_unit * sample_bytes);
	if (used_units == 0) {
		return;
	}

	if (filled_units_ + used_units > original_width) {
		used_units = original_width - filled_units_;
	}
	const int get_rid_of_bytes = samples_per_pixel * pixels_per_unit * sample_bytes * used_units;
	VALIDATE(speech_data_vsize_ >= get_rid_of_bytes, null_str);

	// static int times = 0;
	// SDL_Log("#%i {left_shift}total units: %i, filled_units_: %i, this used_units: %i", times, original_width, filled_units_, used_units);
	// times ++;

	uint8_t* chart_data = (uint8_t*)malloc(pixels_per_unit * sample_bytes);
	int pixels_per_sample = pixels_per_unit / point_per_unit;

	int64_t parsed_size = 0;
	int64_t last_parsed_size = parsed_size;

	int x = filled_units_ * pixels_per_unit;
	for (int num = 0; num < used_units; num ++) {
		// SDL_Rect rect = create_rect(x, zoom * original_height - unit_h - chart_display::bottom_gap, pixels_per_unit, unit_h);
		SDL_Rect rect = create_rect(x, zoom * original_height - unit_h, pixels_per_unit, unit_h);

		const uint8_t* unit_src_ptr = speech_data_ + samples_per_pixel * pixels_per_unit * sample_bytes * num;

		int pixel;
		for (pixel = 0; pixel < point_per_unit; pixel ++) {
			const uint8_t* data_src_ptr = unit_src_ptr + samples_per_pixel * sample_bytes * pixel;
			// Max value, not average value!
			int total = 0;
			bool nagative = false;
			for (int n = 0; n < samples_per_pixel; n ++) {
				int data;

				if (sample_bytes == 1) {
					data = data_src_ptr[0];

				} else {
					if (dbg_speech_data_.ptr == nullptr) {
						data = posix_mki16(data_src_ptr[0], data_src_ptr[1]);

					} else {
						const uint8_t* src2 = dbg_speech_data_.ptr + dbg_speech_data_byte_pos_;
						data = posix_mki16(src2[0], src2[1]);
						dbg_speech_data_byte_pos_ += 2;
					}
				}

				parsed_size += sample_bytes;
				data_src_ptr += sample_bytes;

				if (data >= 0) {
					if (data > total) {
						total = data;
						nagative = false;
					}
				} else {
					int abs_data = -1 * data;
					if (abs_data > total) {
						total = abs_data;
						nagative = true;
					}
				}

			}
			
			VALIDATE(total >= 0, null_str);
			if (nagative) {
				total *= -1;
			}

			if (sample_bytes == 1) {
				chart_data[pixel] = total;
			} else {
				chart_data[pixel * 2] = posix_lo8(total);
				chart_data[pixel * 2 + 1] = posix_hi8(total);
			}
		}

		const int num2 = filled_units_ + num;
		chart_unit* u = nullptr;
		if (units_.size() < original_width) {
			VALIDATE(num2 == units_.size(), null_str);
			u = new chart_unit(*this, *gui_, units_, rect, pixel, pixels_per_sample, num2, chart_data);
			units_.insert2(*gui_, u);
		} else {
			u = units_.find_unit(num2);
			u->set_rect(rect);
			u->set_samples(pixel, pixels_per_sample, chart_data);
		}
		u->set_hidden(false);

		x += pixels_per_unit;
		last_parsed_size = parsed_size;
	}
	VALIDATE(parsed_size == get_rid_of_bytes, null_str);
	VALIDATE(used_units <= original_width, null_str);
	filled_units_ += used_units;

	free(chart_data);
	chart_data = nullptr;

	if (rec_file_.get() != nullptr) {
		rec_speech_bytes_ += get_rid_of_bytes;

		tfile& file = *rec_file_.get();
		posix_fwrite(file.fp, speech_data_, get_rid_of_bytes);

		if (rec_speech_bytes_ >= max_rec_speech_bytes_) {
			stop_rec(false);
		}
	}

	if (filled_units_ == original_width) {
		if (dbg_speech_data_.ptr != nullptr) {
			VALIDATE(dbg_speech_data_byte_pos_ == samples_per_pixel * pixels_per_unit * sample_bytes * filled_units_, null_str);
		}

		const int shift_count = 8;
		units_.left_shift_units(shift_count);
		filled_units_ -= shift_count;
	}

	memcpy(speech_data_, speech_data_ + get_rid_of_bytes, speech_data_vsize_ - get_rid_of_bytes);
	speech_data_vsize_ -= get_rid_of_bytes;
}

void chart_controller::goto_title()
{
	do_quit_ = true;
	if (Mix_PlayingMusic()) {
		// if only stop snore music, not require empty_playlist. add it maybe exist config play.
		sound::empty_playlist();
		stop_music();
	}
}

void chart_controller::stop_music()
{
	if (hook_music_finished_) {
		delete hook_music_finished_;
		hook_music_finished_ = NULL;
	}
	// if (hook_music_playing_) {
	//	delete hook_music_playing_;
	//	hook_music_playing_ = NULL;
	// }

	sound::stop_music();
}

void music_finished()
{
	chart_display* disp = dynamic_cast<chart_display*>(display::get_singleton());
	chart_controller& controller = disp->get_controller();
	// this function run in audio_data thread!
	controller.notify_finished();
	// must call stop_music! 1)set finish hook to NULL, 2)set playing hook to NULL, 3)clear music_cache
	VALIDATE(!Mix_PlayingMusic(), "sdl_mixer must has set playingmusic NULL!");
}

void music_playing(int played, int filled)
{
	chart_display* disp = dynamic_cast<chart_display*>(display::get_singleton());
	chart_controller& controller = disp->get_controller();
	// this function run in audio_data thread!
	controller.set_played(played);
}

void chart_controller::set_played(int played)
{
	{
		static Uint32 last_ticks = 0;
		Uint32 now_ticks = SDL_GetTicks();
		if (now_ticks - last_ticks >= 5000) {
			SDL_Log("chart_controller::set_played, played: %i\n", played);
			last_ticks = now_ticks;
		}
	}

	played_ = played;
}

void chart_controller::notify_finished()
{
	notify_finished_ = true;
}

void chart_controller::update_play_cursor(int played)
{
	if (!played && play_from_ != nposm) {
		Mix_SetMusicPosition(play_from_);
		play_from_ = nposm;
		return;
	}

	const SDL_Rect& _max_map_area = gui_->main_map_rect();

	if (played > sound_buffer_size_ * 2 / 5) {
		played -= sound_buffer_size_ * 2 / 5;
	}

	double normal = (1.0 * played * 8000) / audio_bytes_per_sec_;
	// | 18 | 18 | 18 | 17 | 17 | .....
	// first some pixel are 17 + 1, 1 is spread sample.
	// now minus thease "1".
	int inserted = normal / (play_samples_per_pixel_ + 1);
	if (inserted > play_spread_part_) {
		inserted = play_spread_part_;
	}
	normal -= inserted;

	int x;
	time_t time;
	x = 1.0 * normal * _max_map_area.w / play_integer_part_;
	time = 1.0 * normal * music_during_ / play_integer_part_;

	bool auto_scroll = disable_cursor_pos_? false: true;
	gui_->set_cursor_pos(x, time, auto_scroll);

	if (disable_cursor_pos_) {
		if (SDL_GetTicks() >= disable_cursor_pos_) {
			disable_cursor_pos_ = 0;
		}
	}
}

void chart_controller::stop_rec(bool manual)
{
	VALIDATE(rec_file_.get() != nullptr, null_str);

	twave_pcm_hdr_44bytes wav_hdr = to_wav_hdr;
	tfile& file = *rec_file_.get();
	posix_fseek(file.fp, 0);

	wav_hdr.data_size = rec_speech_bytes_;
	wav_hdr.size_8 = wav_hdr.data_size + (sizeof(wav_hdr) - 8);

	posix_fwrite(file.fp, &wav_hdr, sizeof(wav_hdr));
	rec_file_.reset();
	rec_speech_bytes_ = 0;

	update_rec_ui(false);

	set_status_report(default_status_msg_);
}

void chart_controller::did_show_nagtive_changed(gui2::ttoggle_button& widget)
{
	show_nagtive_ = widget.get_value();
	my_visual_info_.voice_threshold = nposm;

	gui_->did_show_nagtive_change();
}

void chart_controller::click_record()
{
	bool into_rec = false;

	if (rec_file_.get() == nullptr) {
		VALIDATE(rec_speech_bytes_ == 0, null_str);

		std::string short_filename = "speech.wav";
		const std::string filename = game_config::preferences_dir + "/" + short_filename;
		rec_file_.reset(new tfile(filename, GENERIC_WRITE, CREATE_ALWAYS));
		if (rec_file_.get() == nullptr) {
			rec_file_.reset();

			utils::string_map symbols;
			symbols["file"] = filename;
			gui2::show_message(null_str, vgettext2("Create '$file' fail", symbols));
			return;
		}

		twave_pcm_hdr_44bytes wav_hdr = to_wav_hdr;
		posix_fwrite(rec_file_.get()->fp, &wav_hdr, sizeof(wav_hdr));

		update_rec_ui(true);

		utils::string_map symbols;
		symbols["file"] = short_filename;
		symbols["size"] = str_cast(1.0 * max_rec_speech_bytes_ / (1024 * 1024));
		std::string msg = vgettext2("Saving sound to '$file'. Once the $size|M byte is exceeded, it will end automatically", symbols);
		set_status_report(msg);

	} else {
		stop_rec(true);
	}
}

void chart_controller::play_music(const std::string& file)
{
	gui2::show_message(null_str, "Now don't support play_music");
	return;

	bool into_play;
	if (!Mix_PlayingMusic()) {
		SDL_Log("chart_controller::play_music, will start play music, 1\n");

		hook_music_finished_ = new sound::thook_music_finished(music_finished);
		// hook_music_playing_ = new sound::thook_music_playing(music_playing);
		sound::play_music_once(file);
		into_play = true;

		SDL_Log("chart_controller::play_music, will start play music, 2\n");

	} else {
		SDL_Log("chart_controller::play_music, will stop play music, 1\n");

		stop_music();
		into_play = false;

		SDL_Log("chart_controller::play_music, will stop play music, 2\n");
	}
	update_play_ui(into_play);
}

void chart_controller::update_rec_ui(bool ing)
{
	gui2::tbutton* record = dynamic_cast<gui2::tbutton*>(gui_->get_theme_object("record"));
	record->set_label(ing? "misc/stop96.png": "misc/save.png");
}

void chart_controller::update_play_ui(bool into_play)
{
	gui2::tbutton* play = dynamic_cast<gui2::tbutton*>(gui_->get_theme_object("play"));
	if (into_play) {
		play_from_ = gui_->cursor_time();

		play->set_label("misc/pause_music.png");
		play->set_tooltip(_("Stop"));

	} else {
		play->set_label("misc/play_music.png");
		play->set_tooltip(_("Play"));
	}
}

bool chart_controller::app_mouse_motion(const int x, const int y, const bool minimap)
{
	if (minimap) {
		return true;
	}

	const SDL_Rect& _map_area = gui_->main_map_view_rect();
	if (!point_in_rect(x, y, _map_area)) {
		cursor_draging_ = false;
	}

	const map_location& mouseover_hex = gui_->mouseover_hex();
	if (cursor::get() != cursor::WAIT) {
		if (!mouseover_hex.x || !mouseover_hex.y || !map_.on_board(mouseover_hex)) {
			// cursor::set(cursor::INTERIOR);
			cursor::set(cursor::NORMAL);
			gui_->set_mouseover_hex_overlay(NULL);

		} else {
			// no selected unit or we can't move it
			cursor::set(cursor::NORMAL);
		}
	}

	if (cursor_draging_) {
		if (start_drag_x_ == nposm) { 
			const SDL_Rect& _max_map_area = gui_->main_map_rect();
			int xmap = x, ymap = y;
			gui_->screen_2_map(xmap, ymap);

			int cursor_pos = xmap;
			time_t time = music_during_ * cursor_pos / _max_map_area.w;
			gui_->set_cursor_pos(cursor_pos, time, false);
			if (Mix_PlayingMusic()) {
				Mix_SetMusicPosition(time);
			}

		} else if (x != start_drag_x_) {
			// on iOS, mouse_motion maybe receive after mini_left_mouse_down immediately.
			// avoid dithering. (mouse_down isn't move cursor, mouse_motion update cursor.)
			start_drag_x_ = nposm;
		}
	}

	return !cursor_draging_;
}

void chart_controller::reposition_cursor_pos()
{
	// determinate current view and current cursor_pos, if cursor_pos isn't in view, reposition pos.
	// reposition don't move view.
	const SDL_Rect& _max_map_area = gui_->main_map_rect();
	const SDL_Rect& _map_area = gui_->main_map_view_rect();

	int xclick = _map_area.x, yclick = _map_area.y;
	gui_->screen_2_map(xclick, yclick);
	int current_cursor_pos = gui_->cursor_pos();
	int cursor_pos = nposm;
	if (current_cursor_pos < xclick || current_cursor_pos >= xclick + _map_area.w) {
		cursor_pos = xclick + _map_area.w / 2;
	}

	if (cursor_pos == nposm) {
		return;
	}

	time_t time = music_during_ * cursor_pos / _max_map_area.w;
	gui_->set_cursor_pos(cursor_pos, time, false);

	if (!Mix_PlayingMusic()) {

	} else {
		Mix_SetMusicPosition(time);

		// exist played(time) isn't synchronous.
		// main thread: update played.
		// audio_data thread: using old played.
		// it result at jack.
		disable_cursor_pos_ = SDL_GetTicks() + 500; // 500ms
	}
}

void chart_controller::slice_end()
{
	if (scrolling_) {
		reposition_cursor_pos();
	}
}

void chart_controller::app_left_mouse_down(const int x, const int y, const bool minimap)
{
	if (minimap) {
		reposition_cursor_pos();
		return;
	}

	const map_location& mouseover_hex = gui_->mouseover_hex();
	if (!map_.on_board(mouseover_hex)) {
		return;
	}

	VALIDATE(!cursor_draging_, null_str);

	const int cursor_radius = 18 * gui2::twidget::hdpi_scale;
	int xmap = x, ymap = y;
	gui_->screen_2_map(xmap, ymap);
	
	int cursor_pos = gui_->cursor_pos();
	if (xmap >= cursor_pos - cursor_radius && xmap < cursor_pos + cursor_radius) {
		cursor_draging_ = true;
		start_drag_x_ = x;
	}
}

void chart_controller::app_left_mouse_up(const int x, const int y, const bool click)
{
	tclear_cursor_draging_lock lock(*this);

	if (!click) {
		return;
	}

	if (!point_in_rect(x, y, gui_->main_map_widget_rect())) {
		return;
	}

	if (gui_->point_in_volatiles(x, y)) {
		return;
	}

	map_location hex_clicked = gui_->screen_2_loc(x, y);
	const base_unit* u = units_.find_base_unit(x, y);
	if (u && !cursor_draging_) {
		std::vector<const chart_unit*> v;
		v.push_back(dynamic_cast<const chart_unit*>(u));
		if (u->get_map_index() >= 1) {
			v.push_back(dynamic_cast<const chart_unit*>(units_.find_base_unit(u->get_map_index() - 1)));
		} 
		if (u->get_map_index() < original_width - 1) {
			v.push_back(dynamic_cast<const chart_unit*>(units_.find_base_unit(u->get_map_index() + 1)));
		}
		for (std::vector<const chart_unit*>::const_iterator it = v.begin(); it != v.end(); ++ it) {
			const chart_unit& u = **it;
			const chart_unit::talert* alert = u.point_in_alert(x, y);
			if (alert) {
				const SDL_Rect& _max_map_area = gui_->main_map_rect();
				int offset = u.number() * gui_->zoom() + alert->x;
				if (offset) {
					offset -= 1;
				}

				if (!Mix_PlayingMusic()) {
					time_t time = 1.0 * offset * music_during_ / _max_map_area.w;
					gui_->set_cursor_pos(offset, time);

				} else {
					// ???BUG, if not integer, will result noise!
					Mix_SetMusicPosition((int)(music_during_ * offset / _max_map_area.w));
				}
				return;
			}
		}
	}
}

void chart_controller::app_right_mouse_down(const int x, const int y)
{
	do_right_click();
}

void chart_controller::do_right_click()
{
	selected_hex_ = map_location();
}

void chart_controller::pinch_event(bool out)
{
	if (out) {
		gui_->set_zoom(- gui_->zoom() / 2);
	} else {
		gui_->set_zoom(gui_->zoom());
	}

	reposition_cursor_pos();
}

void chart_controller::update_tag_ui() const
{
	gui2::tscroll_panel* panel = dynamic_cast<gui2::tscroll_panel*>(gui_->get_theme_object("tag_panel"));
	gui2::twidget* audio = panel->find("audio_tag", false);
	gui2::twidget* motion = panel->find("motion_tag", false);

	audio->set_visible(gui2::twidget::VISIBLE);
	motion->set_visible(gui2::twidget::VISIBLE);

	panel->invalidate_layout(nullptr);
}

bool chart_controller::app_in_context_menu(const std::string& id) const
{
	using namespace gui2;
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	const chart_unit* u = units_.find_unit(selected_hex_, true);

	switch(command) {
	// idle section
	case HOTKEY_ZOOM_IN:
	case HOTKEY_ZOOM_OUT:
	case HOTKEY_SYSTEM:
		return !selected_hex_.valid();

	default:
		return false;
	}

	return false;
}

bool chart_controller::actived_context_menu(const std::string& id) const
{
	using namespace gui2;
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	const chart_unit* u = units_.find_unit(selected_hex_, true);
	if (!u) {
		return true;
	}
/*
	const unit::tparent& parent = u->parent();
	const unit::tchild& child = parent.u? parent.u->child(parent.number): top_;

	switch(command) {
	// row
	case tmkwin_scene::HOTKEY_ERASE_ROW:
		return child.rows.size() >= 2;

	// column
	case tmkwin_scene::HOTKEY_ERASE_COLUMN:
		return child.cols.size() >= 2;

	}
*/
	return true;
}

int chart_controller::speech_voice_threshold_and_set(int& my_voice_threshold)
{ 
	int curr_voice_threshold = curr_visual_info_.voice_threshold;
	my_voice_threshold = my_visual_info_.voice_threshold;

	if (curr_voice_threshold != my_voice_threshold) {
		my_visual_info_.voice_threshold = curr_voice_threshold;
	}
	return curr_voice_threshold; 
}

void chart_controller::set_gain(gui2::twindow& window, const int valid_height, const int full_height)
{
	int original = current_gain_;
	current_mercury_height_ = valid_height;
	current_gain_ = 1.0 * valid_height * (MAX_AUDIO_GAIN - MIN_AUDIO_GAIN) / full_height + MIN_AUDIO_GAIN;

	if (original != current_gain_) {
		sound::set_music_volume(current_gain_);
	}
}

void chart_controller::signal_handler_left_button_down(gui2::tcontrol* control, const tpoint& coordinate)
{
	callback_set_drag_coordinate(control, coordinate);
	first_coordinate_ = coordinate;
}

bool chart_controller::callback_control_drag_detect(gui2::tcontrol* control, const tpoint& last)
{
	if (seting_gain_) {
		seting_gain_ = false;
	}
	first_coordinate_.x = first_coordinate_.y = nposm;
	return false;
}

bool chart_controller::callback_set_drag_coordinate(gui2::tcontrol* control, const tpoint& last)
{
	if (first_coordinate_.x == nposm || first_coordinate_.y == nposm) {
		return false;
	}

	if (!seting_gain_) {
		seting_gain_ = true;
		last_drag_x_ = last.x;

		gui2::ttrack& widget = *dynamic_cast<gui2::ttrack*>(control);
		did_draw_float_tag1(widget, widget.get_draw_rect(), false, current_mercury_height_);
		return false;
	}

	const int diff = last.x - last_drag_x_;
	int should_height = current_mercury_height_ + diff;

	if (should_height >= 1 && should_height < mercury_height_) {
		gui2::ttrack& widget = *dynamic_cast<gui2::ttrack*>(control);

		set_gain(*widget.get_window(), should_height, mercury_height_);
		did_draw_float_tag1(widget, widget.get_draw_rect(), false, should_height);
	}

	last_drag_x_ = last.x;

	return false;
}

void chart_controller::did_draw_tag(gui2::ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn, bool tag)
{
	if (!bg_drawn) {
		return;
	}
	SDL_Renderer* renderer = get_renderer();
	// gui2::ttrack::tdraw_lock lock(renderer, widget);

	const int xsrc = widget_rect.x;
	const int ysrc = widget_rect.y;
	if (!bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), NULL, &widget_rect);
	}
/*
	SDL_Rect dst;
	std::string str;
	int xoffset, yoffset;

	{
		yoffset = widget_rect.h / 2;
		//str = _("Snore");
		surface text_surf = font::get_rendered_text(_("Snore"), 0, font::SIZE_SMALLER, font::BIGMAP_COLOR);
		xoffset = (widget_rect.w - text_surf->w) / 2;
		//yoffset = text_surf->h;
		dst = ::create_rect(xsrc + xoffset, ysrc + text_surf->h, text_surf->w, text_surf->h);
		render_surface(renderer, text_surf, NULL, &dst);
		
		uint32_t mapped_col = 0x80ffffff;
		render_line(renderer, mapped_col, xsrc + widget_rect.w - 1, ysrc + chart_unit::top_margin, xsrc + widget_rect.w - 1, ysrc + widget_rect.h / 2);
		render_line(renderer, mapped_col, xsrc + widget_rect.w - 1, ysrc + widget_rect.h / 2 + chart_unit::top_margin, xsrc + widget_rect.w - 1, ysrc + widget_rect.h - 1);

		text_surf = font::get_rendered_text(_("Motion"), 0, font::SIZE_SMALLER, font::BIGMAP_COLOR);
		xoffset = (widget_rect.w - text_surf->w) / 2;
		yoffset = widget_rect.h / 2 +  text_surf->h;
		dst = ::create_rect(xsrc + xoffset, ysrc + yoffset, text_surf->w, text_surf->h);
		render_surface(renderer, text_surf, NULL, &dst);

	}
*/
}

void chart_controller::did_draw_float_tag1(gui2::ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn, int set_height)
{
	if (!bg_drawn && (!seting_gain_ || set_height == nposm)) {
		return;
	}


	SDL_Rect dst;
	uint32_t mapped_col = 0x80ffffff;
	
	const int xsrc = widget_rect.x;
	const int ysrc = widget_rect.y;
	
	SDL_Renderer* renderer = get_renderer();
	// gui2::ttrack::tdraw_lock lock(renderer, widget);

	if (!bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), NULL, &widget_rect);
	}
	
		const int volume_width = 15 * gui2::twidget::hdpi_scale;
		const int gap = 4 * gui2::twidget::hdpi_scale;
		surface surf = image::get_image("misc/volume_down.png");
		dst = ::create_rect(xsrc , ysrc + (widget_rect.h - surf->h) / 2, volume_width, volume_width);
		render_surface(renderer, surf, NULL, &dst);

		surf = image::get_image("misc/volume_up.png");
		dst = ::create_rect(xsrc + widget_rect.w - surf->w, ysrc + (widget_rect.h - surf->h) / 2, volume_width, volume_width);
		render_surface(renderer, surf, NULL, &dst);

		if (mercury_height_ == nposm) {
			mercury_height_ = widget_rect.w - 2 * (surf->w + gap);
			gain_yoffset_ = mercury_height_ / 2;

			current_mercury_height_ = 1.0 * (current_gain_ - MIN_AUDIO_GAIN) * mercury_height_ / (MAX_AUDIO_GAIN - MIN_AUDIO_GAIN);
			if (!current_mercury_height_) {
				current_mercury_height_ = 1;
			}
		}

		if (current_gain_ < 0) {
			mapped_col = 0xffff0000;
		} else if (current_gain_ > 0) {
			mapped_col = 0xff00ff00;
		} else {
			mapped_col = 0xffffff00;
		}

		dst.x = xsrc + volume_width + gap;
		dst.h = 5 * gui2::twidget::hdpi_scale;
		dst.y = ysrc + (widget_rect.h -dst.h) / 2;
		dst.w = current_mercury_height_;
		render_rect(renderer, dst, mapped_col);

		mapped_col = 0xffffffff;
		dst.x = xsrc + volume_width + gap + current_mercury_height_;
		dst.h = 5 * gui2::twidget::hdpi_scale;
		dst.y = ysrc + (widget_rect.h - dst.h) / 2;
		dst.w = mercury_height_ - current_mercury_height_;
		render_rect(renderer, dst, mapped_col);
/*
		surface text_surf = font::get_rendered_text(str_cast(current_gain_), 0, font::SIZE_SMALLER, font::BIGMAP_COLOR);
		int xoffset = gain_yoffset_ - text_surf->w;
		int yoffset = (widget_rect.h - text_surf->h) / 2;
		dst = ::create_rect(xsrc + xoffset, ysrc + yoffset, 0, 0);
		sdl_blit(text_surf, NULL, frame_buffer, &dst);
*/
}

static bool align_fill = true;

bool chart_controller::calculate_unit_width(int usable_units, int total_samples, int zoom, int& samples_per_unit, int& samples_per_pixel, int& used_units, int& pixels_per_unit, int& can_spread_samples)
{
	VALIDATE(usable_units >= 1, "usable units must be >= 1!");
	VALIDATE(total_samples >= usable_units, "total_samples must not less than map_w!");

	int pixels = usable_units * zoom;
	bool full_pixel;

	if (total_samples >= pixels) {
		full_pixel = true;

		samples_per_pixel = total_samples / pixels;
		can_spread_samples = total_samples % pixels;
		samples_per_unit = zoom * samples_per_pixel;
		used_units = usable_units;
		pixels_per_unit = zoom;

	} else if (align_fill) {
		full_pixel = false;

		samples_per_unit = total_samples / usable_units;
		can_spread_samples = total_samples % usable_units;
		used_units = usable_units;
		pixels_per_unit = zoom;

		samples_per_pixel = 1;	// <== fix 1.

	} else {
		full_pixel = false;

		int pixels_per_sample = pixels / total_samples;
		pixels_per_unit = (zoom / pixels_per_sample) * pixels_per_sample;
		if (usable_units > 1 && (zoom % pixels_per_sample)) {
			// if usable_units == 1, useable max pixels is zoom!
			pixels_per_unit += pixels_per_sample;
		}
		VALIDATE(!(pixels_per_unit % pixels_per_sample), null_str);

		samples_per_unit = pixels_per_unit / pixels_per_sample;

		used_units = total_samples / samples_per_unit;
		if (total_samples % samples_per_unit) {
			used_units ++;
		}
				
		samples_per_pixel = 1;	// <== fix 1.
		can_spread_samples = 0; // <== fix 0.
	}

	return full_pixel;
}

void chart_controller::fill_unit_pixel()
{
	std::vector<chart_unit*> top_units;
	// don't use units_, set_rect maybe change map_'s sequence.
	for (chart_unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
		chart_unit* u = dynamic_cast<chart_unit*>(&*it);
		top_units.push_back(u);
	}
/*
	// construct audio chart
		play_samples_per_pixel_ = analysis_.audio_samples() / max_chart_data_size;
		play_spread_part_ = analysis_.audio_samples() % max_chart_data_size;
		play_integer_part_ = analysis_.audio_samples() - play_spread_part_;
*/	
		fill_unit_pixel2(top_units, true);
}

void chart_controller::fill_unit_pixel2(const std::vector<chart_unit*>& top_units, bool audio)
{
	VALIDATE(audio, null_str);

	// min zoom is 64.
	const int unit_h = gui_->max_unit_height(gui_->main_map_view_rect().h);
	int zoom = gui_->zoom();
	// bool full_pixel = calculate_unit_width(original_width, total_samples_, zoom, samples_per_unit, samples_per_pixel, used_units, pixels_per_unit, can_spread_samples);
	int used_units = original_width;
	int pixels = used_units * zoom;

	bool full_pixel = true;

	const int sample_bytes = wav_hdr_.bits_per_sample / 8;
	int block_align = wav_hdr_.channels * sample_bytes;
	int total_samples = wav_hdr_.data_size / block_align;

	int samples_per_pixel = total_samples / pixels;
	int can_spread_samples = total_samples % pixels;
	{
		// samples_per_pixel = 1;
		// can_spread_samples = 0;
	}
	int samples_per_unit = zoom * samples_per_pixel;
	int pixels_per_unit = zoom;

	int x = 0;

	int64_t last_fread_byte = 0; // data pos, not file pos!

	// point_per_unit semantics
	// full_pixel == true: pixels per unit
	// full_pixel == false: samples per unit
	int point_per_unit = full_pixel? pixels_per_unit: samples_per_unit;
	const int one_read_bytes = 512 * 1024;
	int last_unit_lack_samples = 0;

	tfile& file = *wav_file_.get();

	file.resize_data(one_read_bytes);
	posix_fseek(file.fp, sizeof(wav_hdr_));

	uint8_t* chart_data = (uint8_t*)malloc(pixels_per_unit * sample_bytes);
	const int data_size = wav_hdr_.data_size;
	char* data_ptr = nullptr;
	int pixels_per_sample = pixels_per_unit / point_per_unit;

	int max_value = 0;
	int64_t parsed_size = 0;
	int64_t last_parsed_size = parsed_size;
	for (int num = 0; num < used_units; num ++) {
		// SDL_Rect rect = create_rect(x, zoom * original_height - unit_h - chart_display::bottom_gap, pixels_per_unit, unit_h);
		SDL_Rect rect = create_rect(x, zoom * original_height - unit_h, pixels_per_unit, unit_h);

		int pixel;
		for (pixel = 0; pixel < point_per_unit; pixel ++) {
			if (parsed_size == data_size) {
				// maybe not is last unit
				VALIDATE(!full_pixel, "End earyly, must be in full_pixel == false!");
				last_unit_lack_samples = point_per_unit - pixel;
				break;
			}
			int samples_per_pixel2 = samples_per_pixel;
			if (can_spread_samples) {
				samples_per_pixel2 ++;
				can_spread_samples --;
			}
			VALIDATE(samples_per_pixel2 > 0, "samples_per_pixel2 must > 0!");

			// Max value, not average value!
			int total = 0;
			bool nagative = false;
			for (int n = 0; n < samples_per_pixel2; n ++) {
				int data;

				if (!data_ptr || data_ptr == file.data + last_fread_byte) {
					int size = one_read_bytes;
					if (data_size - parsed_size < one_read_bytes) {
						size = data_size - parsed_size;
					}
					last_fread_byte = posix_fread(file.fp, file.data, size);
					data_ptr = file.data;
				}

				if (sample_bytes == 1) {
					data = data_ptr[0];

				} else {
					data = posix_mki16(data_ptr[0], data_ptr[1]);
				}


				parsed_size += sample_bytes;
				data_ptr += sample_bytes;

				if (data >= 0) {
					if (data > total) {
						total = data;
						nagative = false;
					}
				} else {
					int abs_data = -1 * data;
					if (abs_data > total) {
						total = abs_data;
						nagative = true;
					}
				}

			}

			VALIDATE(total >= 0, null_str);
			if (nagative) {
				total *= -1;
			}

			if (sample_bytes == 1) {
				chart_data[pixel] = total;
			} else {
				chart_data[pixel * 2] = posix_lo8(total);
				chart_data[pixel * 2 + 1] = posix_hi8(total);
			}

			if (total > max_value) {
				max_value = total;
			}
		}

		chart_unit* u = NULL;
		if (top_units.empty()) {
			u = new chart_unit(*this, *gui_, units_, rect, pixel, pixels_per_sample, num, chart_data);
			units_.insert2(*gui_, u);
		} else {
			u = top_units[num];
			u->set_rect(rect);
			u->set_samples(pixel, pixels_per_sample, chart_data);
		}
		u->set_hidden(false);

		x += pixels_per_unit;
		last_parsed_size = parsed_size;
	}
	// VALIDATE(parsed_size == wav_hdr_.data_size, null_str);
	{
		VALIDATE(parsed_size == samples_per_unit * sample_bytes * used_units, null_str);
	}
	VALIDATE(used_units == original_width, null_str);

	free(chart_data);
	chart_data = nullptr;
/*
	int stop_pos = pixels_per_unit * used_units - 1;
	if (last_unit_lack_samples) {
		VALIDATE(!full_pixel, "Must be in !full_pixel state!");
		stop_pos -= pixels_per_sample_ * last_unit_lack_samples;
	}
	gui_->set_stop_pos(stop_pos);
*/
}

