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

#include "leagor_speech.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include "rose_ros/utils.hpp"

using namespace std::placeholders;

namespace aplt {

tleagor_speech::tleagor_speech(tslot_subscriber& _receiver)
	: tspeech_slot(_receiver, 1, 16000)
	, pinyin_(aplt::get_curr_pinyin())
	, b_api_(aplt::get_b_api())
	, r_api_(aplt::get_r_api())
	, xfyun_(aplt::curr_aplt->res_path, xf3params_)
	, max_positive_(0)
	, max_negative_(0)
	, max_data_vsize_(0)
	// , voice_threshold_(nposm) // 12000(to large)
	// , goaling_enable_(false)
	, maybe_start_pos_(nposm)
	, voice_len_(nposm)
	, last_voice_pos_(nposm)
	// , tone_(0)
	// , eng_lowercase_(true)
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
{
	// trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;
	// voice_threshold_ = aplt_prefs.get_int("voice_threshold", nposm);
	// 'preferences_def' in main.lua make sure voice_threahold is existed.
	// VALIDATE(voice_threshold_ != nposm, null_str);

	reload_xfyun_3fields();

	wchar_t plan_tasks_unicodes[] = {0x8ba1, 0x5212, 0x8981, 0x505a}; // ji4 hua4 yao4 zuo4
	plan_tasks_ = pinyin_.from_unicodes(plan_tasks_unicodes, sizeof(plan_tasks_unicodes) / sizeof(wchar_t), tone_, eng_lowercase_, nullptr, true);
	wchar_t next_tasks_unicodes[] = {0x63a5, 0x4e0b, 0x8981, 0x505a}; // jie1 xia4 yao4 zuo4
	next_tasks_ = pinyin_.from_unicodes(next_tasks_unicodes, sizeof(next_tasks_unicodes) / sizeof(wchar_t), tone_, eng_lowercase_, nullptr, true);
}

void tleagor_speech::pre_start()
{
	tspeech_slot::pre_start();

	// it is called in main-thread
	max_positive_ = 0;
	max_negative_ = 0;
	max_data_vsize_ = 0;
}

void tleagor_speech::did_capture_audio(uint8_t* stream, int len)
{
	// it is called in SDLAudio-thread
	threading::lock lock(data_mutex_);

	const int block_align = 2;
	VALIDATE(len % block_align == 0, null_str);

	receiver.speech_did_capture_audio(stream, len);

	// For safety reasons, 'threading::lock lock(xf3params_.mutex)' should be used here
	const int voice_threshold = xf3params_.voice_threshold;
	const bool goaling_enable = xf3params_.goaling_enable;

	if (voice_len_ != nposm) {
		return;

	} else if (pinyin_.is_speaking()) {
		reset_data_vars();
		return;

	} else if (!goaling_enable && r_api_.goaling()) {
		// When the robot moves, it produces sound. Avoid this sound entering speech recognition.
		reset_data_vars();
		return;
	}

	const int bytes_per_sec = avg_bytes_per_sec_;
	int margin_bytes = bytes_per_sec / 4;
	margin_bytes = posix_align_ceil(margin_bytes, block_align_);
	const int terminate_bytes = bytes_per_sec * 2;
	const int max_cached_silent_bytes = bytes_per_sec * 3;

	VALIDATE(terminate_bytes >= margin_bytes, null_str);
	VALIDATE(max_cached_silent_bytes > margin_bytes, null_str);

	// single-16k ==> (avg_chinese_len)9600
	// const int avg_chinese_len = 3 * bytes_per_sec / 10;  // assume one chinese is 300ms 
	// const int min_voice_len = 25 * avg_chinese_len / 10; // 2.5 chinese
	// const int max_voice_len = 20 * avg_chinese_len; // 20 chinese
	// 
	const int normal_min_voice_len = 55 * bytes_per_sec / 100; // 550ms (normal)
	const int short_min_voice_len = 10 * bytes_per_sec / 100; // 100ms (short)
	const int min_voice_len = allow_short_voice_? short_min_voice_len: normal_min_voice_len;

	const int max_voice_len = 7 * bytes_per_sec; // 7s

	if (maybe_start_pos_ == nposm && data_vsize_ > max_cached_silent_bytes) {
		const int get_rid_of_bytes = max_cached_silent_bytes - margin_bytes;
		memcpy(data_, data_ + get_rid_of_bytes, data_vsize_ - get_rid_of_bytes);
		// SDL_Log("%u no voice, transcat vsize from %i to %i", SDL_GetTicks(), data_vsize_, data_vsize_ - get_rid_of_bytes);
		data_vsize_ -= get_rid_of_bytes;
	}

	bool start_found = false;
	for (int at = 0; at < len; at += block_align) {
		int16_t val = posix_mki16(stream[at], stream[at + 1]);
		if (val >= 0) {
			if (val > max_positive_) {
				max_positive_ = val;
				// SDL_Log("%u found max_posivite_, max_positive_: %i, max_negative_: %i, max_data_vsize_: %i", 
				//	SDL_GetTicks(), max_positive_, max_negative_, max_data_vsize_);
			}

			if (val >= voice_threshold) {
				if (maybe_start_pos_ == nposm) {
					start_found = true;
					maybe_start_pos_ = data_vsize_ + at;
				}
				last_voice_pos_ = data_vsize_ + at;
			}
			
		} else {
			val = -1 * val;
			if (val > max_negative_) {
				max_negative_ = val;
				// SDL_Log("%u found max_negative_, max_positive_: %i, max_negative_: %i, max_data_vsize_: %i", 
				//	SDL_GetTicks(), max_positive_, max_negative_, max_data_vsize_);
			}
		}
	}

	resize_data(data_vsize_ + len);
	memcpy(data_ + data_vsize_, stream, len);
	data_vsize_ += len;

	if (start_found) {
		if (maybe_start_pos_ > margin_bytes) {
			const int get_rid_of_bytes = maybe_start_pos_ - margin_bytes;
			memcpy(data_, data_ + get_rid_of_bytes, data_vsize_ - get_rid_of_bytes);
			data_vsize_ -= get_rid_of_bytes;
			maybe_start_pos_ -= get_rid_of_bytes;
			VALIDATE(maybe_start_pos_ == margin_bytes, null_str);

			VALIDATE(last_voice_pos_ != nposm, null_str);
			last_voice_pos_ -= get_rid_of_bytes;
		}
	} else if (last_voice_pos_ != nposm) {
		VALIDATE(maybe_start_pos_ != nposm, null_str);
		const int voice_len2 = last_voice_pos_ - maybe_start_pos_;
		if (voice_len2 > max_voice_len) {
			SDL_Log("%u maybe voice, but voice_len2(%i) is > max_voice_len(%i), think not, reset", 
				SDL_GetTicks(), voice_len2, max_voice_len);
			pinyin_.speak(_("Voice is to long"));
			reset_data_vars();

		} else if (data_vsize_ >= last_voice_pos_ + terminate_bytes) {
			// const int voice_len = last_voice_pos_ + margin_bytes;
			if (voice_len2 >= min_voice_len) {
				voice_len_ = last_voice_pos_ + margin_bytes;
				voice_event_->Set();
				SDL_Log("%u form valid voice, voice_len_: %i, (voice_len2: %i, maybe_start_pos_: %i, last_voice_pos_: %i)", 
					SDL_GetTicks(), voice_len_, voice_len2, maybe_start_pos_, last_voice_pos_);
				if (game_config::os == os_windows) {
					// write_s16bit_wav(game_config::preferences_dir + "/voice.wav", channels_, freq_, data_, voice_len_);
				}
			} else {
				SDL_Log("%u maybe voice, but voice_len2(%i) is < min_voice_len(%i), think not, reset", 
					SDL_GetTicks(), voice_len2, min_voice_len);
				reset_data_vars();
			}

		} 
	}

	if (data_vsize_ > max_data_vsize_) {
		max_data_vsize_ = data_vsize_;
		// SDL_Log("%u found max_data_vsize_, max_positive_: %i, max_negative_: %i, max_data_vsize_: %i", 
		//	SDL_GetTicks(), max_positive_, max_negative_, max_data_vsize_);
	}
}

std::string format_elapse_hms2(time_t elapse)
{
	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	utils::string_map symbols;
	symbols["hour"] = str_cast(hour);
	symbols["min"] = str_cast(min);
	symbols["sec"] = str_cast(sec);
	
	return vgettext2("$hour hour $min min $sec sec", symbols);
}

// #define ONE_DAY_SECONDS		86400 // 24 * 3600

std::string tleagor_speech::plan_aplt_tasks_msg(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const tros_map& curmap, const std::map<aplt::taplt_task_key, aplt::taplt_task>& tasks)
{
	utils::string_map symbols;

	const int elapse_today = (time(nullptr) + game_config::equation_of_time) % ONE_DAY_SECONDS;

	std::stringstream ss_tasks;
	int valid_tasks = 0;
	const int max_speak_tasks = 2;
	for (std::map<aplt::taplt_task_key, aplt::taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const taplt_task& task = it->second;
		const aplt::tapplet* aplt = aplt::aplt_from_id(applets, task.aplt_id);
		if (aplt == nullptr) {
			continue;
		}
		if (task.zerotz_t < elapse_today) {
			continue;
		}
		if (task.state != taplt_task::state_fresh) {
			continue;
		}
		VALIDATE(aplt->tasks.count(task.task_id) != 0, null_str);

		if (valid_tasks < max_speak_tasks) {
			symbols["time"] = format_elapse_hms2(task.zerotz_t);
			if (curmap.positions.count(task.position1) != 0) {
				symbols["position"] = curmap.positions.find(task.position1)->second.name;
			}
		
			symbols["timing"] = aplt->tasks.find(task.task_id)->second.name;

			ss_tasks << vgettext2("$time, go to $position $timing.", symbols);
		}
		valid_tasks ++;
	}

	std::stringstream ss;
	if (valid_tasks != 0) {
		symbols["tasks"] = str_cast(valid_tasks);
		if (valid_tasks > max_speak_tasks) {
			utils::string_map symbols2;
			symbols2["max_speak_tasks"] = str_cast(max_speak_tasks);
			symbols["recently"] = vgettext2("recently $max_speak_tasks are,", symbols2);
		}
		ss << vgettext2("There are $tasks timing tasks today. $recently", symbols);
		ss << ss_tasks.str();
	} else {
		ss << _("There are no timing tasks today.");
	}

	SDL_Log("%s", ss.str().c_str());
	return ss.str();
}

size_t tleagor_speech::hit_task_from_pinyin(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const std::string& pinyin, const size_t off, bool allow_cpp, ttask_pair& pair) const
{
	pair.aplt = nullptr;
	pair.task = nullptr;
	size_t task_off = nposm;
	if (allow_cpp) {
		const aplt::tapplet& bonus_aplt = aplt::fake_aplt;
		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it = bonus_aplt.tasks.begin(); it != bonus_aplt.tasks.end(); ++ it) {
			const aplt::tapplet::ttask& task = it->second;

			size_t pos = pinyin.find(task.py_name, off);
			if (pos != std::string::npos) {
				pair.aplt = &bonus_aplt;
				pair.task = &task;
				task_off = pos + task.py_name.size();
				return task_off;
			}
		}
	}
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets.begin(); pair.task == nullptr && it != applets.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it2 = aplt.tasks.begin(); it2 != aplt.tasks.end(); ++ it2) {
			const aplt::tapplet::ttask& task = it2->second;
			if (!allow_cpp && task.type == task_cpp) {
				continue;
			}
			size_t pos = pinyin.find(task.py_name, off);
			if (pos != std::string::npos) {
				pair.aplt = &aplt;
				pair.task = &task;
				task_off = pos + task.py_name.size();
				break;
			}
		}
	}
	return task_off;
}

void tleagor_speech::slice()
{
	VALIDATE_IN_MAIN_THREAD();

	// (1/2)check spark-model init wheter

	// (2/2)voice
	std::string result;
	bool new_recogzined = false;
	if (voice_result_dirty_) {
		threading::lock lock(voice_result_mutex_);
		voice_result_dirty_ = false;
		result = voice_result_;

		new_recogzined = true;
	}
	if (result.empty()) {
		if (new_recogzined) {
			std::string err_msg;
			if (xfyun_.is_AUTH_NO_ENOUGH_LICENSE()) {
				err_msg = _("Maximum times of speech recognition SDK can be used per day has been reached");
			} else if (xfyun_.is_DB_INVALID_APPID()) {
				err_msg = _("Please check if the APPID used to access iFlytek is correct");
			}
			if (!err_msg.empty()) {
				pinyin_.speak(err_msg);
			}
		}
		return;
	}

	if (receiver.speech_did_recognition_result(result)) {
		return;
	}

	const tros_map& curmap = r_api_.curmap();
	const std::map<aplt::taplt_key, aplt::tapplet>& applets = b_api_.const_applets();

	bool deliver_nlp = !result.empty();
	// const std::string pinyin = pinyin_.from_utf8str(result, tone_, eng_lowercase_, nullptr);

	if (deliver_nlp) {
		VALIDATE(!result.empty(), null_str);
		if (utils::utf8str_len(result) >= 6) {
			if (!xf3params_.disable_question) {
				receiver.speech_send_nlp_question(result);
			}

		} else {
			pinyin_.speak(result);
		}
	}
}

bool tleagor_speech::send_request(const std::string& req)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(false, null_str);
	return false;
}

std::string tleagor_speech::recognize()
{
	VALIDATE(voice_len_ != nposm, null_str);

	SDL_Log("%u, tspeech::recognize---start", SDL_GetTicks());
	tauto_destruct_executor destruct_executor(std::bind(&tleagor_speech::set_recognized, this));

	std::string result;

	xfyun_.clear_errcode();
	{
		threading::lock lock(xf3params_.mutex);
		if (xf3params_.disable_recognition) {
			SDL_Log("%u, tspeech::recognize---end, lan only, result is empty always", SDL_GetTicks());
			return result;
		}
	}

	if (!xfyun_.started()) {
		xfyun_.recognition_start();
	}
	SDL_Log("%u, tspeech::recognize, post recognition_start()", SDL_GetTicks());
	if (xfyun_.started()) {
		result = xfyun_.iat_piece(data_, voice_len_);
		SDL_Log("%u, tspeech::recognize, pre recognition_stop", SDL_GetTicks());
		xfyun_.recognition_stop();
	}
	SDL_Log("%u, tspeech::recognize---end", SDL_GetTicks());

	if (game_config::os == os_windows && !result.empty()) {
		// result = _("swith scene to sit_front");
	}
	return result;
}

void tleagor_speech::set_recognized()
{
	VALIDATE(maybe_start_pos_ != nposm && voice_len_ != nposm, null_str);

	threading::lock lock(data_mutex_);
	reset_data_vars();
}

void tleagor_speech::reset_data_vars()
{
	maybe_start_pos_ = nposm;
	last_voice_pos_ = nposm;
	voice_len_ = nposm;

	data_vsize_ = 0;
}

void tleagor_speech::DoWork(bool& exit)
{
	tspeech_slot::DoWork(exit);
}

void tleagor_speech::OnWorkWhileStart()
{
}

const tspeech_slot::tvisual_info_C& tleagor_speech::get_visual_info()
{ 
	VALIDATE_IN_MAIN_THREAD();

	visual_info_.voice_threshold = xf3params_.voice_threshold;
	visual_info_.voice_maybe_start = maybe_start_pos_ != nposm;
	visual_info_.allow_short_voice = allow_short_voice_;

	return visual_info_; 
}

void tleagor_speech::reload_xfyun_3fields()
{
	trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

	xf3params_.disable_recognition = aplt_prefs.get_bool("disable_recognition", false);
	xf3params_.disable_question = aplt_prefs.get_bool("disable_question", false);
	xf3params_.voice_threshold = aplt_prefs.get_int("voice_threshold", nposm);

	xf3params_.spark_ver = aplt_prefs.get_str("spark_version");
	xf3params_.appid = aplt_prefs.get_str("spark_appid");
	xf3params_.apisecret = aplt_prefs.get_str("spark_apisecret");
	xf3params_.apikey = aplt_prefs.get_str("spark_apikey");

	int default_maxtoken = 50;
	xf3params_.maxtoken = aplt_prefs.get_int("spark_maxtoken", default_maxtoken);
}

void tleagor_speech::fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id)
{
	VALIDATE_IN_MAIN_THREAD();

	if (cpp_id == cpp_id_sys_dlg_closed) {

	} else if (cpp_id == cpp_id_save_xfyun_3fields) {

		const bool last_disable_question = xf3params_.disable_question;
		const std::string last_appid = xf3params_.appid;
		const std::string last_apisecret = xf3params_.apisecret;
		const std::string last_apikey = xf3params_.apikey;

		threading::lock lock(xf3params_.mutex);
		reload_xfyun_3fields();

		if (xf3params_.disable_question != last_disable_question || xf3params_.appid != last_appid || xf3params_.apisecret != last_apisecret || xf3params_.apikey != last_apikey) {
			voice_event_->Set();
		}
	}
}

}

void* aplt_create_speech_slot(void* _subscriber)
{
	aplt::tslot_subscriber* receier = reinterpret_cast<aplt::tslot_subscriber*>(_subscriber);

	utils::string_map symbols;
	aplt::tb_api& b_api = aplt::get_b_api();

	aplt::tleagor_speech* leagor = new aplt::tleagor_speech(*receier);
	if (!leagor->xfyun().libmsc_loaded()) {
		delete leagor;

		symbols["so"] = LIBROSEAPLT2_SO;
		b_api.aplt_add_msg_log(time(nullptr), vgettext2("[Leagor]load xfyun's $so(libmsc) fail", symbols), 0, false);
		return nullptr;
	}

	aplt::tspeech_slot* result = static_cast<aplt::tspeech_slot*>(leagor);
	return result;
}
