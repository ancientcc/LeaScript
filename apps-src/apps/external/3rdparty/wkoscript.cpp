/*
   Copyright (C) 2009 - 2018 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "wkoscript.hpp"

#include "rose_string_utils_dll.hpp"
#include "rose_config_3rdparty.hpp"
#include "gettext.hpp"
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_sdl_utils.hpp"

#include <angles/angles.h>

#include <boost/foreach.hpp>

using namespace std::placeholders;

namespace utils {

std::string format_time_ms(time_t t, bool align)
{
	// mm:ss => 15:42
	const struct tm* timeptr = localtime(&t);
	if (timeptr == nullptr) {
		// if t is too lareg, for error with ms unit, timeptr maybe nullptr.
		return null_str;
	}
	
	char buf[32];
	if (align) {
		SDL_snprintf(buf, sizeof(buf), "%02d:%02d", timeptr->tm_min, timeptr->tm_sec);
	} else {
		SDL_snprintf(buf, sizeof(buf), "%d:%02d", timeptr->tm_min, timeptr->tm_sec);
	}

	return buf;
}
}

namespace aplt {

const SDL_FPoint fake_landmarks[fake_lmk_count] = {{0.5, 0.5}, // center
	{0.0, 0.0}, // top-left
	{1.0, 0.0}, // top-right
	{1.0, 1.0}, // bottom-right
	{0.0, 1.0} // bottom-left
};

std::map<int, tcode3> wko_task_types;
std::map<int, tcode3> wko_pose_types;
std::map<int, tcode3> wko_operand_types;
std::map<int, tcode3> wko_ang_ranges;
std::map<int, tcode3> wko_time_rules;
std::map<int, tcode3> wko_time_tones;

int wko_task_type_from_str(const std::string& str)
{
	for (std::map<int, tcode3>::const_iterator it = wko_task_types.begin(); it != wko_task_types.end(); ++ it) {
		const std::string& that = it->second.id;
		if (that == str) {
			return it->first;
		}
	}
	return nposm;
}

int wko_pose_type_from_str(const std::string& str)
{
	for (std::map<int, tcode3>::const_iterator it = wko_pose_types.begin(); it != wko_pose_types.end(); ++ it) {
		const std::string& that = it->second.id;
		if (that == str) {
			return it->first;
		}
	}
	return nposm;
}

int wko_operand_type_from_str(const std::string& str)
{
	for (std::map<int, tcode3>::const_iterator it = wko_operand_types.begin(); it != wko_operand_types.end(); ++ it) {
		const std::string& that = it->second.id;
		if (that == str) {
			return it->first;
		}
	}
	return nposm;
}

int wko_ang_range_from_str(const std::string& str)
{
	for (std::map<int, tcode3>::const_iterator it = wko_ang_ranges.begin(); it != wko_ang_ranges.end(); ++ it) {
		const std::string& that = it->second.id;
		if (that == str) {
			return it->first;
		}
	}
	return nposm;
}

int wko_time_rule_from_str(const std::string& str)
{
	for (std::map<int, tcode3>::const_iterator it = wko_time_rules.begin(); it != wko_time_rules.end(); ++ it) {
		const std::string& that = it->second.id;
		if (that == str) {
			return it->first;
		}
	}
	return nposm;
}

int wko_time_tone_from_str(const std::string& str)
{
	for (std::map<int, tcode3>::const_iterator it = wko_time_tones.begin(); it != wko_time_tones.end(); ++ it) {
		const std::string& that = it->second.id;
		if (that == str) {
			return it->first;
		}
	}
	return nposm;
}

bool twkoscript::ttask_speak::from_cfg(const config& cfg)
{
	msgstr = cfg["msgstr"].str();
	repeat_s = cfg["repeat_s"].to_int(nposm);
	min_state_duration_s = cfg["min_state_duration_s"].to_int(nposm);
	return true;
}

void twkoscript::ttask_speak::to_cfg(config& cfg) const
{
	cfg["msgstr"].from_string(msgstr, true);
	if (repeat_s != nposm) {
		cfg["repeat_s"].from_int(repeat_s);
	}
	if (min_state_duration_s != nposm) {
		cfg["min_state_duration_s"].from_int(min_state_duration_s);
	}
}

void twkoscript::ttask_speak::did_enter_state()
{
	VALIDATE(next_speak_ticks_ == 0, null_str);
	VALIDATE(last_speak_s_ == 0, null_str);

	pinyin_.speak(msgstr);
	if (repeat_s != nposm) {
		update_next_speak_ticks();
	}
}

void twkoscript::ttask_speak::did_exit_state()
{
	if (pinyin_.is_speaking() && !b_api_.is_listen_speaking()) {
		pinyin_.stop_speak2();
	}
	next_speak_ticks_ = 0;
}

void twkoscript::ttask_speak::slice()
{
	if (finished_) {
		// The 'next' state may not have been set. 
		// However, if 'repeat_s' is enabled, below next_speak_ticks_ code must still be executed.
		
		// Only after 'next' state has been set, finished_ will take effect for externally.
		// return;
	}

	if (!finished_ && !pinyin_.is_speaking()) {
		if (min_state_duration_s == nposm) {
			finished_ = true;

		} else if (SDL_GetTicks() < state2_.enter_state_ticks_ + min_state_duration_s * 1000) {
			min_duration_slice();

		} else {
			finished_ = true;
		}
	}


	if (next_speak_ticks_ != 0 && SDL_GetTicks() >= next_speak_ticks_) {
		pinyin_.speak(msgstr);
		update_next_speak_ticks();
	}
}

void twkoscript::ttask_speak::update_next_speak_ticks()
{
	VALIDATE(repeat_s != nposm, null_str);
	next_speak_ticks_ = SDL_GetTicks() + repeat_s * 1000;
}

void twkoscript::ttask_speak::min_duration_slice()
{
	VALIDATE(min_state_duration_s != nposm, null_str);
	uint32_t now = SDL_GetTicks();
	const uint32_t desire_finish_ticks = state2_.enter_state_ticks_ + min_state_duration_s * 1000;
	if (desire_finish_ticks > now) {
		int diff = desire_finish_ticks - now;
		int integer = diff / 1000 + 1;

		if (integer >= 20) {
			if ((integer % 20) != 0) {
				return;
			}
		} else if (integer >= 10) {
			if ((integer % 10) != 0) {
				return;
			}
		} else if (integer > 5) {
			return;
		}

		if (integer != last_speak_s_) {
			std::string msg;
			if (integer >= 10) {
				utils::string_map symbols;
				symbols["number"] = str_cast(integer);
				msg = vgettext2("$number seconds remaining", symbols);
				msg = str_cast(integer);

			} else if (integer == 1) {
				msg = _("Start");

			} else if (!pinyin_.is_speaking()) {
				msg = str_cast(integer);
			}

			if (!msg.empty()) {
				pinyin_.speak(msg);
				last_speak_s_ = integer;
			}
		}
	}
}

//
// ttime_counter
//
bool twkoscript::ttime_counter::from_cfg(const config& cfg)
{
	rule = wko_time_rule_from_str(cfg["rule"].str());
	if (rule == nposm) {
		return false;
	}

	std::string str = cfg["tone"].str();
	if (!str.empty()) {
		tone = wko_time_tone_from_str(str);
	} else {
		// Legacy compatibility. 
		// Introduced on 2026-08-07, planned for removal no earlier than 2026-08-07.
		tone = timetone_full;
	}
	if (tone == nposm) {
		return false;
	}

	upcount = cfg["upcount"].to_bool();
	max_count = cfg["max_count"].to_int(nposm);
	if (max_count <= 0) {
		return false;
	}

	satisfied_threshold_s = cfg["satisfied_threshold_s"].to_int(nposm);
	satisfied_msgstr = cfg["satisfied_msgstr"].str();
	if (!satisfied_msgstr.empty()) {
		if (satisfied_threshold_s < 0) {
			return 0;
		}
	}

	return true;
}

void twkoscript::ttime_counter::to_cfg(config& cfg) const
{
	VALIDATE(wko_time_rules.count(rule) != 0, null_str);
	cfg["rule"] = wko_time_rules.find(rule)->second.id;

	VALIDATE(wko_time_tones.count(tone) != 0, null_str);
	cfg["tone"] = wko_time_tones.find(tone)->second.id;

	if (upcount) {
		cfg["upcount"].from_bool(upcount);
	}
	
	if (max_count != nposm) {
		cfg["max_count"].from_int(max_count);
	}

	if (satisfied_threshold_s != nposm) {
		cfg["satisfied_threshold_s"].from_int(satisfied_threshold_s);
	}
	if (!satisfied_msgstr.empty()) {
		cfg["satisfied_msgstr"].from_string(satisfied_msgstr, true);
	}
}

void twkoscript::ttime_counter::did_enter_state()
{
	if (!satisfied_msgstr.empty()) {
		VALIDATE(satisfied_threshold_s != nposm, null_str);
		state2_.set_satisfied_msgstr(satisfied_threshold_s, satisfied_msgstr);
	}

	if (rule == timerule_total || rule == timerule_satisfied) {
		state2_.speak_satisfied_msgstr1_only_once_ = bool_set_true;
	}

	VALIDATE(satisfied_trigger_flag_ == bool_set_none, null_str);
	VALIDATE(tone_state_ == nposm, null_str);
	VALIDATE(last_speak_text_.empty(), null_str);
}

void twkoscript::ttime_counter::did_exit_state()
{
	satisfied_trigger_flag_ = bool_set_none;
	tone_state_ = nposm;
	last_speak_text_.clear();
}

void twkoscript::ttime_counter::did_mediapipe_new_frame(bool satisfied)
{
	const uint32_t now = SDL_GetTicks();
	if (satisfied) {
		if (satisfied_trigger_flag_ == bool_set_true) {
			satisfied_trigger_flag_ = bool_set_false;
		}

		if (rule == timerule_satisfied) {
			if (total_satisfied_ms_ == nposm) {
				total_satisfied_ms_ = 0;

			} else {
				total_satisfied_ms_ += now - last_frame_ticks_;
			}

		}
		if (sn_last_speak_s_ == nposm) {
			set_countdown(max_count);
		}
	}
/*
	if (total_satisfied_ms_ != nposm) {
		SDL_Log("%u {dbg-timerule}satisfied: %s, now - last_frame_ticks_: %u, total_satisfied_ms_: %i", 
			now, satisfied? "true": "false", now - last_frame_ticks_, total_satisfied_ms_);
	}
*/
	last_frame_ticks_ = now;
}

void twkoscript::ttime_counter::did_satisfied_to_unsatisfied()
{
	satisfied_trigger_flag_ = bool_set_true;
	if (rule == timerule_strict) {
		clear_countdown();
	}
}

void twkoscript::ttime_counter::slice()
{
	if (finished_) {
		return;
	}

	if (sn_last_speak_s_ != nposm) {
		if (rule == timerule_total) {
			countdown_slice_total();

		} else if (rule == timerule_satisfied) {
			countdown_slice_satisfied();

		} else if (rule == timerule_strict) {
			countdown_slice_strict();
		}
		if (sn_last_speak_s_ == nposm) {
			finished_ = true;
		}
	}
}

void twkoscript::ttime_counter::set_countdown(int _countdown_s)
{
	VALIDATE(sn_last_speak_s_ == nposm, null_str);
	VALIDATE(tone_state_ == nposm, null_str);
	VALIDATE(last_speak_text_.empty(), null_str);

	// why initial set '_countdown_s + 1'? avoid speak '_countdown_s + 1'.
	// see 'int integer = diff / 1000 + 1' in slice(). 
	sn_last_speak_s_ = _countdown_s + 1;

	// Start speaking the countdown count as soon as possible.
	pinyin_.speak(null_str);
}

void twkoscript::ttime_counter::clear_countdown()
{
	VALIDATE(sn_last_speak_s_ != nposm, null_str);

	sn_last_speak_s_ = nposm;
	tone_state_ = nposm;
	last_speak_text_.clear();
}

int twkoscript::ttime_counter::total_satisfied_ms() const 
{
	if (rule == timerule_total) {
		if (state2_.first_satisfied_ticks_ != 0) {
			return SDL_GetTicks() - state2_.first_satisfied_ticks_;

		} else {
			return 0;
		}

	} else if (rule == timerule_satisfied) {
		return total_satisfied_ms_;

	} else {
		VALIDATE(rule == timerule_strict, null_str);
		if (state2_.threshold_first_satisfied_ticks_ != 0) {
			return SDL_GetTicks() - state2_.threshold_first_satisfied_ticks_;

		} else {
			return 0;
		}
	}
}

void twkoscript::ttime_counter::tone_speak(int total_sec, int curr_sec)
{
	// curr_sec = total_satisfied_ms_ / 1000, it is in floor. integer don't '+1'.
	VALIDATE(curr_sec < total_sec, null_str);

	bool use_hms = false;
	bool can_speak = false;

	bool preempt = false;
	if (satisfied_trigger_flag_ == bool_set_false) {
		// The user needs to know immediately when the posture changes from "unsatisfied" to "satisfied" 
		// — this has the highest priority.
		satisfied_trigger_flag_ = bool_set_none;

		// Make the 'use_hms' consistent with 'timetone_xxx'.
		// use_hms = true;

		can_speak = true;
		sn_last_speak_s_ = nposm;
		preempt = true;

	}
	if (tone == timetone_full) {
		can_speak = true;

	} else if (tone == timetone_split) {
		if (total_sec <= 20) {
			can_speak = true;

		} else if (preempt) {
			use_hms = curr_sec > 10 && curr_sec < total_sec - 10;

		} else if (curr_sec <= 10) {
			// 1. first 10s
			if (tone_state_ == nposm) {
				tone_state_ = tonestate_first10s;
			}
			can_speak = true;

		} else if (tone_state_ <= tonestate_mid1th && curr_sec >= total_sec / 3) {
			// 2. middle: 1/3 and 2/3
#define MIN_ONE_MID_SECS	40
#define MAX_ONE_MID_SECS	59
			if (tone_state_ < tonestate_mid1th) {
				if (total_sec > MAX_ONE_MID_SECS) {
					tone_state_ = tonestate_mid1th;

				} else if (total_sec >= MIN_ONE_MID_SECS) {
					tone_state_ = tonestate_mid1th;

				} else {
					// total_sec < 50
					// no mid1th and mid2th
					tone_state_ = tonestate_last10s;
				}
			}
			if (tone_state_ == tonestate_mid1th) {
				if (total_sec <= MAX_ONE_MID_SECS) {
					VALIDATE(total_sec >= MIN_ONE_MID_SECS, null_str);
					if (curr_sec >= total_sec / 2) {
						if (!pinyin_.is_speaking()) {
							can_speak = true;
							tone_state_ = tonestate_last10s;

							use_hms = true;

							// make sure can speak.
							sn_last_speak_s_ = nposm;
						}
					}

				} else {
					if (!pinyin_.is_speaking()) {
						can_speak = true;
						tone_state_ = tonestate_mid2th;

						use_hms = true;

						// make sure can speak.
						sn_last_speak_s_ = nposm;
					}
				}
			}

		} else if (tone_state_ == tonestate_mid2th && curr_sec >= (total_sec * 2) / 3) {
			if (!pinyin_.is_speaking()) {
				can_speak = true;
				tone_state_ = tonestate_last10s;

				use_hms = true;
			}

		} else if (curr_sec >= total_sec - 10) {
			// 3. last 10s
			can_speak = true;	
		}

	} else {
		VALIDATE(tone == timetone_silent, null_str);
		if (preempt) {
			use_hms = true;
		}
	}

	if (!can_speak) {
		return;
	}

	// curr_sec = total_satisfied_ms_ / 1000, it is in floor. integer don't '+1'.
	int integer = total_sec - curr_sec; // + 1;
	if (integer != sn_last_speak_s_) {
		if (preempt || !pinyin_.is_speaking()) {
			// When reporting certain 'integer', they may exceed 1 second, 
			// so the only option is to skip broadcasting for that second..
			if (!use_hms) {
				last_speak_text_ = str_cast(integer);

			} else {
				utils::string_map symbols;
				symbols["time"] = utils::format_elapse_hms(integer, utils::timesep_i18n, true);
				last_speak_text_ = vgettext2("$time remaining", symbols);
			}
			pinyin_.speak(last_speak_text_);
		}
		// Skip this second to avoid the sound being too dense.
		sn_last_speak_s_ = integer;
	}
}

void twkoscript::ttime_counter::countdown_slice_total()
{
	VALIDATE(rule == timerule_total, null_str);
	VALIDATE(state2_.first_satisfied_ticks_ > 0, null_str);

	uint32_t now = SDL_GetTicks();
	int total_satisfied_ms1 = now - state2_.first_satisfied_ticks_;
	int diff = max_count * 1000 - total_satisfied_ms1;
	if (diff > 0) {
		tone_speak(max_count, total_satisfied_ms1 / 1000);

	} else {
		clear_countdown();
	}
}

void twkoscript::ttime_counter::countdown_slice_satisfied()
{
	VALIDATE(rule == timerule_satisfied, null_str);
	VALIDATE(total_satisfied_ms_ >= 0, null_str);

	uint32_t now = SDL_GetTicks();
	int diff = max_count * 1000 - total_satisfied_ms_;
	if (diff > 0) {
		tone_speak(max_count, total_satisfied_ms_ / 1000);

	} else {
		clear_countdown();
	}

}

void twkoscript::ttime_counter::countdown_slice_strict()
{
	VALIDATE(rule == timerule_strict, null_str);

	uint32_t now = SDL_GetTicks();
	int total_satisfied_ms1 = now - state2_.threshold_first_satisfied_ticks_;
	int diff = max_count * 1000 - total_satisfied_ms1;
	if (diff > 0) {
		tone_speak(max_count, total_satisfied_ms1 / 1000);

	} else {
		clear_countdown();
	}
}

//
// trep_counter
//
twkoscript::trep_counter::~trep_counter()
{
	VALIDATE(log_file_ == nullptr, null_str);
}

bool twkoscript::trep_counter::from_cfg(const config& cfg)
{
	clear();

	upcount = cfg["upcount"].to_bool();
	max_count = cfg["max_count"].to_int(nposm);
	if (max_count <= 0) {
		return false;
	}

	BOOST_FOREACH (const config &phase_cfg, cfg.child_range("phase")) {
		phases.push_back(tphase());
		tphase& phase = phases.back();

		phase.min_duration_ms = phase_cfg["min_duration_ms"].to_int(nposm);
		phase.action_msg = phase_cfg["action_msg"].str();
		phase.cooldowned_ms = phase_cfg["cooldowned_ms"].to_int(nposm);

		if (phase.min_duration_ms <= 0 || phase.action_msg.empty() || phase.cooldowned_ms <= 0) {
			return false;
		}
	}

	int phase_count = phases.size();
	if (phase_count != 2) {
		return false;
	}
	for (int at = 0; at < phase_count; at ++) {
		const tphase& phase = phases[at];

		if (!phase.valid(at == phase_count - 1)) {
			return false;
		}
	}

	return true;
}

void twkoscript::trep_counter::to_cfg(config& cfg) const
{
	if (upcount) {
		cfg["upcount"].from_bool(upcount);
	}
	
	if (max_count != nposm) {
		cfg["max_count"].from_int(max_count);
	}


	const int phase_count = phases.size();
	for (int at = 0; at < phase_count; at ++) {
		const tphase& phase = phases[at];
		VALIDATE(phase.valid(at == phase_count - 1), null_str);

		config& sub_cfg = cfg.add_child("phase");
		if (!phase.action_msg.empty()) {
			sub_cfg["action_msg"].from_string(phase.action_msg, true);
		}
		if (phase.min_duration_ms != nposm) {
			sub_cfg["min_duration_ms"].from_int(phase.min_duration_ms);
		}
		if (phase.cooldowned_ms != nposm) {
			sub_cfg["cooldowned_ms"].from_int(phase.cooldowned_ms);
		}
	}
}

void twkoscript::trep_counter::did_enter_state()
{
	VALIDATE(curr_phase_ == 0, null_str);
	VALIDATE(next_satisfied_ticks_ == 0, null_str);
	VALIDATE(next_cooldowned_ticks_ == 0, null_str);
	VALIDATE(next_speak_escape_ticks_ == 0, null_str);
	VALIDATE(count_ == nposm, null_str);
	VALIDATE(delay_speak_.ticks == 0, null_str);
	VALIDATE(log_file_ == nullptr, null_str);

	count_ = 0;
	original_unsatisfied_2th_msgstr_ = state2_.unsatisfied_2th_msgstr;

	state2_.unsatisfied_2th_msgstr = phases[curr_phase_].action_msg;

	// const bool enable_log_file = game_config::os == os_windows;
	const bool enable_log_file = false;
	if (enable_log_file) {
		const std::string log_file_name = game_config::preferences_dir + "/0_rep_counter.log";
		log_file_ = new tfile(log_file_name, GENERIC_WRITE, CREATE_ALWAYS);

		to_log_file("did_enter_state---");
	}
}

void twkoscript::trep_counter::did_exit_state()
{
	if (log_file_ != nullptr) {
		to_log_file("---did_exit_state");
		delete log_file_;
		log_file_ = nullptr;
		last_log_ticks_ = 0;
	}
	state2_.unsatisfied_2th_msgstr = original_unsatisfied_2th_msgstr_;

	first_active_period_start_sent_ = false;
	next_satisfied_ticks_ = 0;
	next_cooldowned_ticks_ = 0;
	next_speak_escape_ticks_ = 0;
	count_ = nposm;
	delay_speak_.clear();
}

void twkoscript::trep_counter::did_mediapipe_new_frame(bool satisfied)
{
	uint32_t now = SDL_GetTicks();
	int phase_count = phases.size();

	if (satisfied) {
		if (next_satisfied_ticks_ == 0 && next_cooldowned_ticks_ == 0) {
			if (!first_active_period_start_sent_) {
				first_active_period_start_sent_ = true;
				if (log_file_ != nullptr) {
					to_log_file("send first active_period_start");
				}
				b_api_.health_push_n32_event(workoutevt_n32, workoutn32_active_period_start);
			}
			update_next_satisfied_ticks("received one satisfied frame");

		} else if (next_satisfied_ticks_ != 0) {
			VALIDATE(next_cooldowned_ticks_ == 0, null_str);
			if (now >= next_satisfied_ticks_) {
				zero_next_satisfied_ticks("curr phase satisfied");
				if (curr_phase_ + 1 == phase_count) {
					const std::string msg = str_cast(++ count_);
					if (state2_.script_->sfx_enabled()) {
						sound::rose_play_sound_simple("rep_full.wav");
						set_delay_speak(now + delay_speak_threshold_ms_, msg);

					} else {
						pinyin_.speak(msg);
					}
					if (log_file_ != nullptr) {
						to_log_file("++ count, speak(%i)", count_);
					}
					b_api_.health_push_n32_event(workoutevt_n32, workoutn32_rep_complete);

					if (count_ == max_count) {
						// why push it? 
						// --To facilitate the calculation of the duration of the last repetition in health.dat.
						b_api_.health_push_n32_event(workoutevt_n32, workoutn32_cooldown_period_start);

						finished_ = true;
						return;
					}
					// ???it is necessary?
					// state2_.track_pose.update_landmarks(*state2_.script_);
					state2_.unsatisfied_2th_msgstr = phases[curr_phase_].action_msg;

					update_next_cooldowned_ticks();
					
				} else {
					// 1/2)The first time the voice msgstr for the current phase is played is at the beginning of the cooldown period. 
					const std::string& action_msg = phases[curr_phase_ + 1].action_msg;

					if (state2_.script_->sfx_enabled()) {
						sound::rose_play_sound_simple("rep_phase1.wav");
						set_delay_speak(now + delay_speak_threshold_ms_, action_msg);

					} else {
						pinyin_.speak(action_msg);
					}
					if (log_file_ != nullptr) {
						to_log_file("curr phase satisfied, begin cooldown, speak(%s)", action_msg.c_str());
					}
					update_next_cooldowned_ticks();
				}
				b_api_.health_push_n32_event(workoutevt_n32, workoutn32_cooldown_period_start);
			}
			
		} else {
			VALIDATE(next_cooldowned_ticks_ != 0, null_str);
			VALIDATE(next_speak_escape_ticks_ != 0, null_str);
			/* if (now >= next_cooldowned_ticks_) {
				to_next_phase();

			} else */ if (now >= next_speak_escape_ticks_) {
				int next_phase = curr_phase_ + 1;
				if (next_phase == phase_count) {
					next_phase = 0;
				}
				const std::string& action_msg = phases[next_phase].action_msg;
				// if (!pinyin_.is_speaking()) {
					pinyin_.speak(action_msg);
				// }
				// pinyin_.speak(str_cast(20));
				if (log_file_ != nullptr) {
					to_log_file("escape curr pose, speak(%s)", action_msg.c_str());
				}

				int escape_threshold = escape_threshold_ms_;
				next_speak_escape_ticks_ = SDL_GetTicks() + escape_threshold;
			}
		}

	} else if (next_satisfied_ticks_ != 0) {
		VALIDATE(next_cooldowned_ticks_ == 0, null_str);
		// if (phases[curr_phase_].min_duration_ms <= state2_.unsatisfied_threshold_ms) {
			zero_next_satisfied_ticks("received one unsatisfied frame");
		// }

	} else if (next_cooldowned_ticks_ != 0) {
		VALIDATE(next_satisfied_ticks_ == 0, null_str);
		if (now >= next_cooldowned_ticks_) {
			to_next_phase();
		}
	}
}

void twkoscript::trep_counter::did_satisfied_to_unsatisfied()
{
	if (next_satisfied_ticks_ != 0) {
		zero_next_satisfied_ticks("shouldn't occur");
	}
}

void twkoscript::trep_counter::slice()
{
	if (finished_) {
		return;
	}

	uint32_t now = SDL_GetTicks();
	if (delay_speak_.ticks != 0 && now >= delay_speak_.ticks) {
		// SDL_Log("{dbg-delay-speak}%u, [2]speak %s", now, delay_speak_.msg.c_str());
		pinyin_.speak(delay_speak_.msg);
		delay_speak_.clear();
	}
}

void twkoscript::trep_counter::to_next_phase()
{
	VALIDATE(next_cooldowned_ticks_ != 0, null_str);
	zero_next_cooldowned_ticks();

	int phase_count = phases.size();
	curr_phase_ ++;
	if (curr_phase_ == phase_count) {
		curr_phase_ = 0;
	}
	state2_.script_->did_phase_changed(state2_.state, curr_phase_);
	if (log_file_ != nullptr) {
		if (curr_phase_ != 0) {
			to_log_file("end cooldown, to next_phase");
		} else {
			to_log_file("end cooldown, to next_phase, it is begin of this count.");
		}
	}
	b_api_.health_push_n32_event(workoutevt_n32, workoutn32_active_period_start);
	state2_.track_pose.update_landmarks(*state2_.script_);
	const std::string& action_msg = phases[curr_phase_].action_msg;
	// 2/2)The first time the voice msgstr for the current phase is played is at the beginning of the cooldown period. 
	// The cooldown period is often very short; 
	// it should not be played at the end to prevent the user from hearing two prompts of the same message with a very short interval.
	// pinyin_.speak(action_msg);
	state2_.unsatisfied_2th_msgstr = action_msg;

	update_next_satisfied_ticks("to_next_phase");
}

void twkoscript::trep_counter::update_next_satisfied_ticks(const std::string& scene)
{
	VALIDATE(next_cooldowned_ticks_ == 0, null_str);
	VALIDATE(curr_phase_ < (int)phases.size(), null_str);
	next_satisfied_ticks_ = SDL_GetTicks() + phases[curr_phase_].min_duration_ms;

	if (log_file_ != nullptr) {
		to_log_file("post update_next_satisfied_ticks, scene: %s, (+ %i ms) = next_ticks: %u", 
			scene.c_str(), phases[curr_phase_].min_duration_ms, next_satisfied_ticks_);
	}
}

void twkoscript::trep_counter::zero_next_satisfied_ticks(const std::string& scene)
{
	next_satisfied_ticks_ = 0;

	if (log_file_ != nullptr) {
		to_log_file("post zero_next_satisfied_ticks, scene: %s", scene.c_str());
	}
}

void twkoscript::trep_counter::update_next_cooldowned_ticks()
{
	VALIDATE(next_satisfied_ticks_ == 0, null_str);
	VALIDATE(curr_phase_ < (int)phases.size(), null_str);
	next_cooldowned_ticks_ = SDL_GetTicks() + phases[curr_phase_].cooldowned_ms;

	if (log_file_ != nullptr) {
		to_log_file("post update_next_cooldowned_ticks, (+ %i ms) = next_ticks: %u", 
			phases[curr_phase_].cooldowned_ms, next_cooldowned_ticks_);
	}

	int escape_threshold = escape_threshold_ms_;
	next_speak_escape_ticks_ = SDL_GetTicks() + escape_threshold;
}

void twkoscript::trep_counter::zero_next_cooldowned_ticks()
{
	VALIDATE(next_cooldowned_ticks_ != 0, null_str);
	next_cooldowned_ticks_ = 0;
	next_speak_escape_ticks_ = 0;
}

void twkoscript::trep_counter::set_delay_speak(uint32_t ticks, const std::string& msg)
{
	// SDL_Log("{dbg-delay-speak}%u, [1]ticks: %u, speak %s", SDL_GetTicks(), ticks, msg.c_str());

	VALIDATE(ticks > 0 && !msg.empty(), null_str);
	delay_speak_.ticks = ticks;
	delay_speak_.msg = msg;
}

void twkoscript::trep_counter::to_log_file(const char *fmt, ...)
{
	VALIDATE(log_file_ != nullptr, null_str);
	va_list ap;

	char msg[256];
    va_start(ap, fmt);
	int len = SDL_vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

	const uint32_t now = SDL_GetTicks();
	int diff = 0;
	if (last_log_ticks_ != 0) {
		diff = now - last_log_ticks_;
	}
	char msg2[256];
	len = SDL_snprintf(msg2, sizeof(msg2), "%u(diff: %i) phase: %i, count: %i, msg: %s\n", 
		now, diff, curr_phase_, count_, msg);

	posix_fwrite(log_file_->fp, msg2, len);
	last_log_ticks_ = now;
}

//
// tpose
//
bool twkoscript::tpose::from_cfg(const config& cfg)
{
	clear();

	// bool fail = false;
	type = wko_pose_type_from_str(cfg["type"].str());
	if (type == nposm) {
		return false;
	}
	operand_type = wko_operand_type_from_str(cfg["operand"].str());
	if (operandtype_must_point(type) && operand_type != operandtype_point) {
		operand_type = nposm;
	}
	if (operand_type == nposm) {
		return false;
	}

	std::vector<std::string> v_str = utils::split(cfg["landmarks"].str(), ';');
	if (v_str.size() > MAX_OPERANDS_PER_POSE) {
		return false;
	}
	if (type == posetype_angle3p) {
		if (v_str.size() != 3) {
			return false;
		}
	} else {
		if (v_str.size() != 2) {
			return false;
		}
	}
	int segment_at = 0;
	for (std::vector<std::string>::const_iterator it = v_str.begin(); it != v_str.end(); ++ it, segment_at ++) {
		std::vector<std::string> v_str2 = utils::split(*it);
		if (v_str2.empty() || v_str2.size() > MAX_LANDMARKS_PER_OPERAND) {
			return false;
		}
		if (operand_must_2landmark(operand_type) && v_str2.size() != 2) {
			return false;
		}

		int at = 0;
		for (std::vector<std::string>::const_iterator it2 = v_str2.begin(); it2 != v_str2.end(); ++ it2, at ++) {
			const std::string& str = *it2;
			int landmark = utils::to_int(str);
			// if (landmark < 0 || landmark >= mediapipe::kNumPoseLandmarks && is_fake_lmk(landmark)) {
			if (!is_valid_lmk_all(landmark)) {
				return false;
			}
			if (at == 0) {
				operands[segment_at].lmk0 = landmark;
			} else {
				operands[segment_at].lmk1 = landmark;
			}
		}
	}

	// divisor
	v_str = utils::split(cfg["divisor"].str(), ';');
	if (v_str.size() > MAX_OPERANDS_PER_POSE) {
		return false;
	}
	segment_at = 0;
	for (std::vector<std::string>::const_iterator it = v_str.begin(); it != v_str.end(); ++ it, segment_at ++) {
		char* eptr;
		double d = strtod(it->c_str(), &eptr);
		if (*eptr != '\0') {
			return false;
		}
		divisor[segment_at] = d;		
	}

	// divisor
	v_str = utils::split(cfg["addend"].str(), ';');
	if (v_str.size() > MAX_OPERANDS_PER_POSE) {
		return false;
	}
	segment_at = 0;
	for (std::vector<std::string>::const_iterator it = v_str.begin(); it != v_str.end(); ++ it, segment_at ++) {
		char* eptr;
		double d = strtod(it->c_str(), &eptr);
		if (*eptr != '\0') {
			return false;
		}
		addend[segment_at] = d;		
	}

	abs = cfg["abs"].to_bool();
	ang_range = wko_ang_range_from_str(cfg["ang_range"].str());
	if (ang_range == nposm) {
		ang_range = def_pose_ang_range;
	}
	VALIDATE(ang_range >= 0 && ang_range < angrange_count, null_str);
	range.min = cfg["min"].to_double(float_nposm);
	range.max = cfg["max"].to_double(float_nposm);

	if (validate_range_) {
		std::string err_msg;
		if (is_valid2_range(0, err_msg) != TCOOKIE3F_CHECK_OK) {
			return false;
		}
/*
		if (is_float_nposm(range.min) && is_float_nposm(range.max)) {
			return false;
		}
*/
	}

	int max_phase_mask = BIT_IDX_MASK(WKO_MAX_PHASE_COUNT) - 1;
	phase_mask = cfg["phase_mask"].to_int(1);
	if (phase_mask < 1 || phase_mask > max_phase_mask) {
		return false;
	}
	name = cfg["name"].str();
	unsatisfied_msgstr = cfg["unsatisfied_msgstr"].str();
	unsatisfied_rmax_msgstr = cfg["unsatisfied_rmax_msgstr"].str();
	legend = cfg["legend"].str();

	// if (fail) {
		// clear();
	// }
	return true;
}

void twkoscript::tpose::to_cfg(config& cfg) const
{
	cfg.clear();

	// VALIDATE(isvalid_normal_id_or_var_name224(type), null_str);
	VALIDATE(type >= 0 && type < posetype_count, null_str);
	cfg["type"] = wko_pose_types[type].id;
	cfg["operand"] = wko_operand_types[operand_type].id;

	// landmarks
	std::stringstream ss;
	ss.str("");
	for (int at = 0; at < MAX_OPERANDS_PER_POSE; at ++) {
		if (operands[at].lmk0 == nposm) {
			break;
		}
		if (at != 0) {
			ss << "; ";
		}
		VALIDATE(is_valid_lmk_all(operands[at].lmk0), null_str);
		ss << operands[at].lmk0;

		if (operands[at].lmk1 != nposm) {
			VALIDATE(is_valid_lmk_all(operands[at].lmk1), null_str);
			ss << "," << operands[at].lmk1;
		}
	}
	cfg["landmarks"] = ss.str();

	// divisor
	ss.str("");
	int to_count = 0;
	for (int at = 0; at < MAX_OPERANDS_PER_POSE; at ++) {
		if (!KDL_Equal(divisor[at], 1.0)) {
			to_count = at + 1;
		}
	}

	for (int at = 0; at < to_count; at ++) {
		if (at != 0) {
			ss << "; ";
		}
		ss << divisor[at];
	}
	if (!ss.str().empty()) {
		cfg["divisor"].from_string(ss.str(), true);
	}

	// addend
	ss.str("");
	to_count = 0;
	for (int at = 0; at < MAX_OPERANDS_PER_POSE; at ++) {
		if (!KDL_Equal(addend[at], 0.0)) {
			to_count = at + 1;
		}
	}
	for (int at = 0; at < to_count; at ++) {
		if (at != 0) {
			ss << "; ";
		}
		ss << addend[at];
	}
	if (!ss.str().empty()) {
		cfg["addend"].from_string(ss.str(), true);
	}

	if (abs) {
		cfg["abs"].from_bool(abs);
	}
	VALIDATE(wko_ang_ranges.count(ang_range) != 0, null_str);
	if (ang_range != def_pose_ang_range) {
		cfg["ang_range"].from_string(wko_ang_ranges.find(ang_range)->second.id, true);
	}

	if (!is_float_nposm(range.min)) {
		cfg["min"].from_double(range.min);
	}

	if (!is_float_nposm(range.max)) {
		cfg["max"].from_double(range.max);
	}

	VALIDATE(phase_mask > 0, null_str);
	if (phase_mask != 1) {
		cfg["phase_mask"].from_int(phase_mask);
	}
	if (!name.empty()) {
		cfg["name"].from_string(name, true);
	}
	if (!unsatisfied_msgstr.empty()) {
		cfg["unsatisfied_msgstr"].from_string(unsatisfied_msgstr, true);
	}
	if (!unsatisfied_rmax_msgstr.empty()) {
		cfg["unsatisfied_rmax_msgstr"].from_string(unsatisfied_rmax_msgstr, true);
	}
	if (!legend.empty()) {
		cfg["legend"].from_string(legend, true);
	}
}

bool twkoscript::tpose::operator==(const tpose& that) const
{
	if (type != that.type || operand_type != that.operand_type) {
		return false;
	}
	if (memcmp(operands, that.operands, sizeof(operands)) != 0) {
		return false;
	}
	if (memcmp(divisor, that.divisor, sizeof(divisor)) != 0) {
		return false;
	}
	if (memcmp(addend, that.addend, sizeof(addend)) != 0) {
		return false;
	}
	if (abs != that.abs || ang_range != that.ang_range) {
		return false;
	}
	if (!KDL_Equal(range.min, that.range.min) || !KDL_Equal(range.max, that.range.max)) {
		return false;
	}
	if (phase_mask != that.phase_mask || name != that.name || unsatisfied_msgstr != that.unsatisfied_msgstr || unsatisfied_rmax_msgstr != that.unsatisfied_rmax_msgstr) {
		return false;
	}
	if (legend != that.legend) {
		return false;
	}
	return true;
}

SDL_DPoint mid_DPoint_from_2FPoint(const SDL_FPoint& p1, const SDL_FPoint& p2)
{
	return {(p1.x + p2.x) / 2, (p1.y + p2.y) / 2};
}

#define get_fake_landmark(at_base0)		fake_landmarks[(at_base0) - fake_lmk_min]

void twkoscript::tpose::calc_point_operand(const SDL_FPoint* landmarks33, SDL_DPoint* result) const
{
	result[0] = SDL_DPoint{float_nposm, float_nposm};
	result[1] = SDL_DPoint{float_nposm, float_nposm};
	result[2] = SDL_DPoint{float_nposm, float_nposm};
	
	VALIDATE(operand_type == operandtype_point, null_str);
	for (int at = 0; at < MAX_OPERANDS_PER_POSE; at ++) {
		if (operands[at].lmk0 == nposm) {
			break;
		}
		int index0 = operands[at].lmk0;
		const SDL_FPoint& p0 = is_fake_lmk(index0)? get_fake_landmark(index0): landmarks33[index0];
		if (operands[at].lmk1 != nposm) {
			int index1 = operands[at].lmk1;
			// exist tow landmarks, it means taking their center point.
			const SDL_FPoint& p1 = is_fake_lmk(index1)? get_fake_landmark(index1): landmarks33[index1];
			result[at] = mid_DPoint_from_2FPoint(p0, p1);

		} else {
			result[at].x = p0.x;
			result[at].y = p0.y;
		}

		result[at].x /= divisor[at];
		result[at].y /= divisor[at];

		result[at].x += addend[at];
		result[at].y += addend[at];
	}
}

void twkoscript::tpose::calc_1d_operand(const SDL_FPoint* landmarks33, double* result) const
{
	double vals[3] = {float_nposm, float_nposm, float_nposm} ;
	for (int at = 0; at < MAX_OPERANDS_PER_POSE; at ++) {
		if (operands[at].lmk0 == nposm) {
			break;
		}
		int index0 = operands[at].lmk0;
		const SDL_FPoint& p0 = is_fake_lmk(index0)? get_fake_landmark(index0): landmarks33[index0];
		if (operands[at].lmk1 != nposm) {
			int index1 = operands[at].lmk1;
			const SDL_FPoint& p1 = is_fake_lmk(index1)? get_fake_landmark(index1): landmarks33[index1];
			if (operand_type == operandtype_x || operand_type == operandtype_y) {
				SDL_DPoint mid = mid_DPoint_from_2FPoint(p0, p1);
				vals[at] = operand_type == operandtype_x? mid.x: mid.y;

			} else if (operand_type == operandtype_dist_x) {
				vals[at] = fabs(p0.x - p1.x);

			} else if (operand_type == operandtype_dist_y) {
				vals[at] = fabs(p0.y - p1.y);

			} else {
				VALIDATE(false, null_str);
			}
		} else {
			if (operand_type == operandtype_x) {
				vals[at] = p0.x;
			} else if (operand_type == operandtype_y) {
				vals[at] = p0.y;
			} else {
				VALIDATE(false, null_str);
			}
		}
		vals[at] /= divisor[at];
		vals[at] += addend[at]; 
	}

	memcpy(result, vals, sizeof(double) * 3);
}

// SDL_DPoint.x: if invalid, float_nposm. else 0.
// SDL_DPoint.y: result.
SDL_DPoint twkoscript::tpose::calc_result_may_invalid(const SDL_FPoint* landmarks33) const
{
#define DPOINT_X_OK		0.0
	SDL_DPoint result{float_nposm, 0};

	int operand_count = wko_operand_count_from_pose_type(type);
	for (int at = 0; at < operand_count; at ++) {
		if (operands[at].lmk0 == nposm) {
			return result;
		}
	}

	if (type == twkoscript::posetype_angle3p) {
		SDL_DPoint points[3];
		calc_point_operand(landmarks33, points);

		double rad = float_nposm;
		if (ang_range == angrange_180) {
			rad = utils::imgcoor_calculate_angle_3SDL_Point_pi(points[0], points[1], points[2]);

		} else if(ang_range == angrange_360) {
			rad = utils::imgcoor_calculate_angle_3SDL_Point_2pi(points[0], points[1], points[2]);

		} else {
			VALIDATE(false, null_str);
		}
		double degree = RAD2DEG(rad);

		result.x = DPOINT_X_OK;
		result.y = degree;

	} else if (type == twkoscript::posetype_angle2p) {
		SDL_DPoint points[3];
		calc_point_operand(landmarks33, points);

		double rad_pi_div2 = utils::imgcoor_calculate_angle_2SDL_Point(points[0], points[1]);

		bool use_pi_div2 = true;
		double degree = RAD2DEG(rad_pi_div2);
		if (use_pi_div2) {
			// ==> in [0, 2*pi)
			double rad_0_2pi = angles::normalize_angle_positive(rad_pi_div2);
			degree = RAD2DEG(rad_0_2pi);
		}
		result.x = DPOINT_X_OK;
		result.y = degree;

	} else if (type == twkoscript::posetype_diff) {
		if (operand_type == operandtype_point) {
			SDL_DPoint points[3];
			calc_point_operand(landmarks33, points);
			double dist = hypot(points[0].x - points[1].x, points[0].y - points[1].y);

			result.x = DPOINT_X_OK;
			result.y = dist;

		} else {
			if (operand_must_2landmark(operand_type)) {
				if (operands[0].lmk1 == nposm || operands[1].lmk1 == nposm) {
					return result;
				}
			}
			double vals[3] = {float_nposm, float_nposm, float_nposm};
			calc_1d_operand(landmarks33, vals);

			double result2 = vals[0] - vals[1];
			if (abs) {
				result2 = fabs(result2);
			}
			result.x = DPOINT_X_OK;
			result.y = result2;
		}

	} else {
		VALIDATE(false, null_str);
	}
	return result;
}

bool twkoscript::ttrack_pose::from_cfg(const config& cfg)
{
	clear();

	std::set<int> existed;

	BOOST_FOREACH (const config &pose_cfg, cfg.child_range("pose")) {
		const std::string type_str = pose_cfg["type"].str();
		int type = wko_pose_type_from_str(type_str);
		if (type == nposm) {
			return false;
		}

		if (poses.size() == WKO_MAX_POSES_PER_TRACK) {
			return false;
		}

		poses.push_back(tpose());
		tpose& pose = poses.back();
		bool retval = pose.from_cfg(pose_cfg);
		if (!retval) {
			return false;
		}

		if (pose.operand_type == operandtype_point) {
			posture_fields1 += 1;
		} else {
			posture_fields1 += 2;
		}
	}
	VALIDATE(landmark_count == (int)existed.size(), null_str);
	VALIDATE(poses.size() <= WKO_MAX_POSES_PER_TRACK, null_str);

	return true;
}

void twkoscript::ttrack_pose::to_cfg(config& cfg) const
{
	VALIDATE(valid(), null_str);

	for (std::vector<tpose>::const_iterator it = poses.begin(); it != poses.end(); ++ it) {
		const tpose& pose = *it;
		config& pose_cfg = cfg.add_child("pose");

		pose.to_cfg(pose_cfg);
	}
}

twkoscript::tpose& twkoscript::ttrack_pose::insert_pose(int after_at, const std::string& _name)
{
	VALIDATE(after_at == nposm || (after_at >= 0 && after_at < poses.size()), null_str);
	std::string new_name = _name;
	if (_name.empty()) {
		std::set<std::string> existed_names;
		get_pose_names(null_str, existed_names);
		new_name = utils::unique_untitle_name(existed_names, null_str, 1);
	}

	tpose* new_pose = nullptr;
	// if (after_at == nposm || (after_at + 1) == poses.size()) {
	if (after_at == nposm) {
		poses.push_back(aplt::twkoscript::tpose());
		new_pose = &poses.back();
	} else {
		const int insert_at = after_at + 1;
		std::vector<tpose>::iterator insert_it = poses.begin();
		if (insert_at != 0) {
			std::advance(insert_it, insert_at);
		}
		std::vector<tpose>::iterator new_it = poses.insert(insert_it, tpose());
		new_pose = &*new_it;
	}

	tpose& pose = *new_pose;
	pose.phase_mask = 1;
	pose.name = new_name;
	pose.type = posetype_angle3p;
	pose.operand_type = operandtype_point;

	VALIDATE(pose.phase_mask >= 1, null_str);
	return pose;
}

void twkoscript::ttrack_pose::erase_pose(int pose_at)
{
	VALIDATE(pose_at >= 0 && pose_at < (int)poses.size(), null_str);
	std::vector<tpose>::iterator erase_it = poses.begin();
	if (pose_at != 0) {
		std::advance(erase_it, pose_at);
	}
	poses.erase(erase_it);
}

void twkoscript::ttrack_pose::pose_swap(int s1, int s2)
{
	VALIDATE(s1 != nposm && s2 != nposm && s1 != s2, null_str);

	// 3) state2
	std::iter_swap(poses.begin() + s1, poses.begin() + s2);
}

std::string twkoscript::ttrack_pose::absent_landmark_msgstr(int landmark) const
{
	VALIDATE(landmark >= 0 && landmark < mediapipe::kNumPoseLandmarks, null_str);

	utils::string_map symbols;
	symbols["landmark"] = utils::landmark_name(landmark);
	return vgettext2("Have the camera capture $landmark", symbols);
}

void twkoscript::ttrack_pose::get_pose_names(const std::string& exclude, std::set<std::string>& result) const
{
	result.clear();
	for (std::vector<tpose>::const_iterator it = poses.begin(); it != poses.end(); ++ it) {
		const std::string& name = it->name;
		if (exclude.empty() || name != exclude) {
			result.insert(name);
		}
	}
}

void twkoscript::ttrack_pose::update_landmarks(twkoscript& script)
{
	int anti_shake_fields = 0;

	std::set<int> existed;
	// VALIDATE(landmark_count == 0, null_str);
	landmark_count = 0;
	const uint32_t curr_phase_mask = BIT_IDX_MASK(curr_phase_);
	for (std::vector<tpose>::const_iterator it = poses.begin(); it != poses.end(); ++ it) {
		const tpose& pose = *it;
		if ((pose.phase_mask & curr_phase_mask) == 0) {
			continue;
		}
		for (int at = 0; at < MAX_OPERANDS_PER_POSE; at ++) {
			if (pose.operands[at].lmk0 == nposm) {
				break;
			}
			int l = pose.operands[at].lmk0;
			if (is_fake_lmk(l)) {
				continue;
			}
			VALIDATE(is_mediapipe_lmk(l), null_str);
			if (existed.count(l) == 0) {
				existed.insert(l);
				landmarks[landmark_count ++] = l;
			}

			l = pose.operands[at].lmk1;
			if (is_fake_lmk(l)) {
				VALIDATE(l <= fake_lmk_max, null_str);
				continue;
			}
			if (l != nposm && existed.count(l) == 0) {
				VALIDATE(is_mediapipe_lmk(l), null_str);
				existed.insert(l);
				landmarks[landmark_count ++] = l;
			}
		}

		if (pose.operand_type == operandtype_point) {
			anti_shake_fields += 1;
		} else {
			anti_shake_fields += 2;
		}
	}
	VALIDATE(landmark_count == (int)existed.size(), null_str);

	script.reset_anti_shike_samples(anti_shake_fields);
}

void twkoscript::tstate2::to_cfg(const std::vector<std::string>& state_names, config& cfg) const
{
	cfg["state"] = state_names[state];

	if (track_pose.valid()) {
		config& track_pose_cfg = cfg.add_child("track_pose");
		track_pose.to_cfg(track_pose_cfg);
	}

	if (task != nullptr) {
		config& task_cfg = cfg.add_child("task");

		task_cfg["type"] = wko_task_types.find(task->type)->second.id;
		task->to_cfg(task_cfg);
	}

	next.to_cfg("next", state_names.size(), cfg);
/*
	if (satisfied_threshold_s != nposm) {
		cfg["satisfied_threshold_s"].from_int(satisfied_threshold_s);
	}
	if (!satisfied_msgstr.empty()) {
		cfg["satisfied_msgstr"].from_string(satisfied_msgstr, true);
	}
*/
	if (is_setup) {
		cfg["is_setup"].from_bool(is_setup);
	}
	if (debug_skip) {
		cfg["debug_skip"].from_bool(debug_skip);
	}
	if (unsatisfied_threshold_ms != nposm) {
		cfg["unsatisfied_threshold_ms"].from_int(unsatisfied_threshold_ms);
	}
	if (unsatisfied_2th_threshold_s != nposm) {
		cfg["unsatisfied_2th_threshold_s"].from_int(unsatisfied_2th_threshold_s);
	}

	if (!unsatisfied_2th_msgstr.empty()) {
		cfg["unsatisfied_2th_msgstr"].from_string(unsatisfied_2th_msgstr, true);
	}
}

void twkoscript::tstate2::green()
{
	std::vector<tpose>& poses = track_pose.poses;

	bool multiple = wko_allow_multiple_phase_from_task_type(task->type);
	for (std::vector<tpose>::iterator it2 = poses.begin(); it2 != poses.end(); ++ it2) {
		tpose& pose = *it2;

		if (!multiple) {
			pose.phase_mask = 1;
		}

		int operand_count = wko_operand_count_from_pose_type(pose.type);
		if (operand_count == 2) {
			pose.operands[2].lmk0 = nposm;
			pose.operands[2].lmk1 = nposm;
		}

		if (!pose_has_abs(pose)) {
			pose.abs = false;
		}

		if (pose.type != posetype_angle3p) {
			pose.ang_range = def_pose_ang_range;
		}
	}

	if (task->type == tasktype_rep_counter) {
		if (!unsatisfied_2th_msgstr.empty()) {
			unsatisfied_2th_msgstr.clear();
		}
	}
}

twkoscript::ttask_base* twkoscript::tstate2::new_task(int type)
{
	VALIDATE(wko_task_types.count(type) != 0, null_str);

	ttask_base* task = nullptr;
	if (type == tasktype_speak) {
		task = new ttask_speak(*this);

	} else if (type == tasktype_time_counter) {
		task = new ttime_counter(*this);

	} else if (type == tasktype_rep_counter) {
		task = new trep_counter(*this);

	} else {
		VALIDATE(false, null_str);
	}
	return task;
}

void twkoscript::tstate2::set_next_to_state(int to_state)
{
	VALIDATE(to_state == nposm || to_state >= 0, null_str);
	next.clear();

	if (to_state == nposm) {
		return;
	}

	bool is_pose_state = wko_is_pose_state2_from_task_type(task->type);
	next.branches.push_back(tif_branch());
	tif_branch& branch = next.branches.back();
	branch.do_to_state = to_state;

	int logic = -1;
	int op = tif_judge::op_bool_equal;
	// tfunction func;
	const tfunction_code* func_code = nullptr;
	std::vector<std::string> params;
	std::string var_exp;
	config::attribute_value r_val;
	if (is_pose_state) {
		func_code = &functions.find(tfunction::func_is_wko_task_finished)->second;
		// var_exp = "$(is_wko_task_finished,)";
		r_val.from_bool(true);
	} else {
		// func_code = &functions.find(tfunction::func_is_speaking)->second;
		// var_exp = "$(is_speaking,)";
		// r_val.from_bool(false);

		func_code = &functions.find(tfunction::func_is_wko_task_finished)->second;
		r_val.from_bool(true);
	}
	var_exp = curr_func->func_2_var_exp(*func_code, params, false);
	branch.judges.push_back(tif_judge(logic, op, var_exp, r_val));

}

std::string twkoscript::tstate2::get_field_str(int type, int field)
{
	if (type == typeid_script) {
		if (field == fid_id) {
			return "ID";
		} else if (field == fid_name) {
			return _("object^Name");
		} else if (field == fid_states) {
			return _("State");
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_global) {
		if (field == fid_name) {
			return _("object^Name");
		} else if (field == fid_is_setup) {
			return _("wko^is_setup label");
		} else if (field == fid_debug_skip) {
			return _("wko^debug_skip label");
		} else if (field == fid_unsatisfied_threshold_ms) {
			return _("wko^unsatisfied_threshold_ms label");
		} else if (field == fid_unsatisfied_2th_threshold_s) {
			return _("wko^unsatisfied_2th_threshold_s label");
		} else if (field == fid_unsatisfied_2th_msgstr) {
			return _("wko^unsatisfied_2th_msgstr label");
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_task) {
		if (field == fid_typeself) {
			return _("Task");
		} else if (field == fid_type) {
			return _("Type");
		// task_counter
		} else if (field == fid_task_satisfied_threshold_s) {
			return _("wko^satisfied_threshold_s label");
		} else if (field == fid_task_satisfied_msgstr) {
			return _("wko^satisfied_msgstr label");
			
		} else if (field == fid_time_counter_rule) {
			return _("wko^time_counter, rule");

		} else if (field == fid_time_counter_tone) {
			return _("wko^time_counter, tone");

		} else if (field == fid_time_counter_max_count) {
			return _("wko^time_counter, max_count label");
		// rep_counter
		} else if (field == fid_rep_counter_max_count) {
			return _("wko^rep_counter, max_count label");
		// task_speak
		} else if (field == fid_task_msgstr) {
			return _("wko^task_speak, msgstr label");
		} else if (field == fid_task_repeat_s) {
			return _("wko^task_speak, repeat_s label");
		} else if (field == fid_task_min_state_duration_s) {
			return _("wko^task_speak, min_state_duration_s label");

		} else if (field == fid_phase_action_msg) {
			return _("wko^phase, action_msg label");
		} else if (field == fid_phase_min_duration_ms) {
			return _("wko^phase, min_duration_ms label");
		} else if (field == fid_phase_cooldowned_ms) {
			return _("wko^phase, cooldowned_ms label");

		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_track_pose) {
		if (field == fid_typeself) {
			return _("wko^track_pose label");
		}
	} else if (type == typeid_pose) {
		if (field == fid_pose_name) {
			return _("object^Name");
		} else if (field == fid_pose_phase_mask) {
			return _("wko^phase_mask label");
		} else if (field == fid_type) {
			return _("Type");
		} else if (field == fid_operand_type) {
			return _("wko^operand_type label");
		} else if (field == fid_landmarks) {
			return _("wko^landmarks label");
		} else if (field == fid_pose_min) {
			return _("Minimum value");
		} else if (field == fid_pose_max) {
			return _("Maximum value");
		} else if (field == fid_pose_unsatisfied_msgstr) {
			return _("wko^pose_unsatisfied_msgstr label");
		} else if (field == fid_pose_unsatisfied_rmax_msgstr) {
			return _("wko^pose_unsatisfied_rmax_msgstr label");
		} else if (field == fid_pose_legend) {
			return _("wko^pose_legend label");
		} else {
			VALIDATE(false, null_str);
		}
	} else {
		VALIDATE(false, null_str);
	}

	return null_str;
}

std::string twkoscript::tstate2::get_placeholder_msg(int type, int field) const
{
	std::string placeholder;
	if (type == typeid_script) {
		if (field == fid_id) {
			placeholder = i18n::freq_msgstr(i18n::msgid_isvalid_normal_id_or_var_name);

		} else if (field == fid_name) {
			placeholder = i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

		} else if (field == fid_states) {

		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_global) {
		if (field == fid_name) {
			return i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

		} else if (field == fid_is_setup) {

		} else if (field == fid_debug_skip) {

		} else if (field == fid_unsatisfied_threshold_ms) {
			return i18n::freq_msgstr(i18n::msgid_greater_than_0);

		} else if (field == fid_unsatisfied_2th_threshold_s) {
			utils::string_map symbols;
			symbols["that"] = get_field_str(type, fid_unsatisfied_threshold_ms);
			return vgettext2("Value must be greater than '$that'", symbols);

		} else if (field == fid_unsatisfied_2th_msgstr) {
			placeholder = i18n::freq_msgstr(i18n::msgid_empty_or_utf8str);

		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_task) {
		if (field == fid_typeself) {

		} else if (field == fid_type) {

		// task_speak
		} else if (field == fid_task_msgstr) {
			return i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

		} else if (field == fid_task_repeat_s) {
			return i18n::freq_msgstr(i18n::msgid_empty_or_greater_than_0);

		} else if (field == fid_task_min_state_duration_s) {
			return i18n::freq_msgstr(i18n::msgid_empty_or_greater_than_0);

		// task_counter
		} else if (field == fid_task_satisfied_threshold_s) {
			return i18n::freq_msgstr(i18n::msgid_greater_than_equal_to_0);

		} else if (field == fid_task_satisfied_msgstr) {
			placeholder = i18n::freq_msgstr(i18n::msgid_empty_or_utf8str);

		} else if (field == fid_time_counter_rule) {

		} else if (field == fid_time_counter_tone) {

		} else if (field == fid_time_counter_max_count) {
			return i18n::freq_msgstr(i18n::msgid_greater_than_0);

		// phase
		} else if (field == fid_phase_action_msg) {
			return i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);
		} else if (field == fid_phase_min_duration_ms) {
			return i18n::freq_msgstr(i18n::msgid_greater_than_0);
		} else if (field == fid_phase_cooldowned_ms) {
			return i18n::freq_msgstr(i18n::msgid_greater_than_0);

		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_track_pose) {
		if (type == fid_typeself) {

		}
	} else if (type == typeid_pose) {
		if (field == fid_pose_phase_mask) {

		} else if (field == fid_pose_name) {
			placeholder = i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

		} else if (field == fid_type) {

		} else if (field == fid_operand_type) {

		} else if (field == fid_landmarks) {

		} else if (field == fid_pose_min) {

		} else if (field == fid_pose_max) {

		} else if (field == fid_pose_unsatisfied_msgstr) {
			placeholder = i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

		} else if (field == fid_pose_unsatisfied_rmax_msgstr) {
			placeholder = i18n::freq_msgstr(i18n::msgid_empty_or_utf8str);

		} else if (field == fid_pose_legend) {
			placeholder = i18n::freq_msgstr(i18n::msgid_empty_or_utf8str);
		} else {
			VALIDATE(false, null_str);
		}
	} else {
		VALIDATE(false, null_str);
	}
	
	return placeholder;
}

std::string twkoscript::tstate2::get_error_msg(const tcookie3f& cookie3f) const
{
	utils::string_map symbols;
	symbols["field"] = get_field_str(cookie3f.type, cookie3f.field);
	const std::string note = get_placeholder_msg(cookie3f.type, cookie3f.field);

	std::string err_msg;
	if (note.empty()) {
		err_msg = vgettext2("Invalid '$field'", symbols);

	} else {
		symbols["note"] = note;
		err_msg = vgettext2("Invalid '$field'. $note", symbols);
	}
	return err_msg;
}

uint64_t twkoscript::tstate2::is_sub_valid_speak(std::string& err_msg) const
{
	if (task->type == tasktype_speak) {
		const ttask_speak* task2 = static_cast<const ttask_speak*>(task);
		if (task2->msgstr.empty() || !utils::is_utf8str(task2->msgstr.c_str(), task2->msgstr.size())) {
			return tcookie3f(0, typeid_task, fid_task_msgstr).u64;
		}
		if (task2->repeat_s != nposm && task2->repeat_s <= 0) {
			return tcookie3f(0, typeid_task, fid_task_repeat_s).u64;
		}
		if (task2->min_state_duration_s != nposm && task2->min_state_duration_s <= 0) {
			return tcookie3f(0, typeid_task, fid_task_min_state_duration_s).u64;
		}
	}

	return TCOOKIE3F_CHECK_OK;
}

uint64_t twkoscript::tpose::is_valid2_range(int index, std::string& err_msg) const
{
	VALIDATE(err_msg.empty(), null_str);

	utils::string_map symbols;

	bool min_is_nposm = is_float_nposm(range.min);
	bool max_is_nposm = is_float_nposm(range.max);
	if (min_is_nposm && max_is_nposm) {
		err_msg = _("At least one of the minimum or maximum values must be set.");
		return tcookie3f(index, typeid_pose, fid_pose_min).u64;

	} else if (!min_is_nposm) {
		if (!max_is_nposm && range.max <= range.min) {
			err_msg = _("The maximum value must be greater than the minimum value.");
			return tcookie3f(index, typeid_pose, fid_pose_max).u64;
		}
	}

	if (type == aplt::twkoscript::posetype_angle3p) {
		SDL_Range r{1, 179};
		if (ang_range == angrange_360) {
			r.max = 359;
		}
		// symbols["min"] = str_cast(r.min);
		// symbols["max"] = str_cast(r.max);
		if (!min_is_nposm && (range.min < r.min * 1.0 || range.min > r.max * 1.0)) {
			// symbols["value"] = twkoscript::tstate2::get_field_str(typeid_pose, fid_pose_min);
			// err_msg = vgettext2("$value range is [$min, $max].", symbols);

			const std::string val = twkoscript::tstate2::get_field_str(typeid_pose, fid_pose_min);
			err_msg = i18n::freq_msgstr_1str_2int(i18n::msgid_value_range, val, r.min, r.max);
			return tcookie3f(index, typeid_pose, fid_pose_min).u64;
		}
		if (!max_is_nposm && (range.max < r.min * 1.0 || range.max > r.max * 1.0)) {
			// symbols["value"] = twkoscript::tstate2::get_field_str(typeid_pose, fid_pose_max);
			// err_msg = vgettext2("$value range is [$min, $max].", symbols);

			const std::string val = twkoscript::tstate2::get_field_str(typeid_pose, fid_pose_max);
			err_msg = i18n::freq_msgstr_1str_2int(i18n::msgid_value_range, val, r.min, r.max);
			return tcookie3f(index, typeid_pose, fid_pose_max).u64;
		}

		if (ang_range == angrange_360 && (min_is_nposm || max_is_nposm)) {
			symbols["field"] = "angle3p(360)";
			err_msg = vgettext2("For $field, both min and max must be set simultaneously.", symbols);
			return tcookie3f(index, typeid_pose, min_is_nposm? fid_pose_min: fid_pose_max).u64;
		}

	} else if (type == aplt::twkoscript::posetype_angle2p) {
		if (min_is_nposm || max_is_nposm) {
			symbols["field"] = "angle2p";
			err_msg = vgettext2("For $field, both min and max must be set simultaneously.", symbols);
			return tcookie3f(index, typeid_pose, min_is_nposm? fid_pose_min: fid_pose_max).u64;
		}
	}

	return TCOOKIE3F_CHECK_OK;
}

uint64_t twkoscript::tstate2::is_valid2(int pose_states_parsed, std::string& err_msg, bool check_range) const
{
	err_msg.clear();

	// common fields
	if (task == nullptr) {
		return tcookie3f(0, typeid_task, fid_typeself).u64;
	} 
	if (wko_task_types.count(task->type) == 0) {
		return tcookie3f(0, typeid_task, fid_type).u64;
	}

	if (!wko_is_pose_state2_from_task_type(task->type)) {
		if (!track_pose.poses.empty()) {
			err_msg = _("For non-pose state, must not exist pose.");
			return tcookie3f(0, typeid_track_pose, fid_typeself).u64;
		}
		return is_sub_valid_speak(err_msg);
	}

	if (track_pose.poses.empty()) {
		err_msg = _("For pose state, at least one pose must exist.");
		return tcookie3f(0, typeid_track_pose, fid_typeself).u64;
	}

	if (is_setup && pose_states_parsed != 0) {
		err_msg = _("Setup can only be the first pose state.");
		return tcookie3f(0, typeid_global, fid_is_setup).u64;
	}

	utils::string_map symbols;
	//
	// base
	//
	if (unsatisfied_threshold_ms <= 0) {
		return tcookie3f(0, typeid_global, fid_unsatisfied_threshold_ms).u64;
	}
	if (unsatisfied_2th_threshold_s * 1000 <= unsatisfied_threshold_ms) {
		return tcookie3f(0, typeid_global, fid_unsatisfied_2th_threshold_s).u64;
	}
	if (!utils::is_utf8str(unsatisfied_2th_msgstr.c_str(), unsatisfied_2th_msgstr.size())) {
		return tcookie3f(0, typeid_global, fid_unsatisfied_2th_msgstr).u64;
	}

	//
	// task
	//
	if (task->type == tasktype_time_counter) {
		const ttime_counter* task2 = static_cast<const ttime_counter*>(task);

		if (wko_time_rules.count(task2->rule) == 0) {
			return tcookie3f(0, typeid_task, fid_time_counter_rule).u64;
		}
		if (wko_time_tones.count(task2->tone) == 0) {
			return tcookie3f(0, typeid_task, fid_time_counter_tone).u64;
		}
		if (task2->max_count <= 0) {
			return tcookie3f(0, typeid_task, fid_time_counter_max_count).u64;
		}

		if (task2->satisfied_threshold_s < 0) {
			return tcookie3f(0, typeid_task, fid_task_satisfied_threshold_s).u64;
		}
		if (!utils::is_utf8str(task2->satisfied_msgstr.c_str(), task2->satisfied_msgstr.size())) {
			return tcookie3f(0, typeid_task, fid_task_satisfied_msgstr).u64;
		}

	} else if (task->type == tasktype_rep_counter) {
		const trep_counter* task2 = static_cast<const trep_counter*>(task);
		if (task2->max_count <= 0) {
			return tcookie3f(0, typeid_task, fid_time_counter_max_count).u64;
		}
		for (int index = 0; index < WKO_MAX_PHASE_COUNT; index ++) {
			const tphase& phase = task2->phases[index];
			if (phase.action_msg.empty() || !utils::is_utf8str(phase.action_msg.c_str(), phase.action_msg.size())) {
				return tcookie3f(index, typeid_task, fid_phase_action_msg).u64;
			}
			if (phase.min_duration_ms <= 0) {
				return tcookie3f(index, typeid_task, fid_phase_min_duration_ms).u64;
			}
			// if (index != WKO_MAX_PHASE_COUNT - 1) {
				if (phase.cooldowned_ms <= 0) {
					return tcookie3f(index, typeid_task, fid_phase_cooldowned_ms).u64;
				}
			// }
		}
	}

	//
	// poses
	//
	const int max_phase_mask = BIT_IDX_MASK(WKO_MAX_PHASE_COUNT) - 1;
	int index = 0;
	for (std::vector<tpose>::const_iterator it = track_pose.poses.begin(); it != track_pose.poses.end(); ++ it, index ++) {
		const tpose& pose = *it;

		if (pose.phase_mask < 1 || pose.phase_mask > max_phase_mask) {
			return tcookie3f(index, typeid_pose, fid_pose_phase_mask).u64;
		}

		if (pose.name.empty() || !utils::is_utf8str(pose.name.c_str(), pose.name.size())) {
			return tcookie3f(index, typeid_pose, fid_pose_name).u64;
		}
		
		if (wko_pose_types.count(pose.type) == 0) {
			return tcookie3f(index, typeid_pose, fid_type).u64;
		}
		if (wko_operand_types.count(pose.operand_type) == 0) {
			return tcookie3f(index, typeid_pose, fid_operand_type).u64;
		}
		const int operand_count = wko_operand_count_from_pose_type(pose.type);
		if (pose.operands[0].lmk0 == nposm || pose.operands[1].lmk0 == nposm ||
			(operand_count == 3 && pose.operands[2].lmk0 == nposm)) {
			err_msg = _("Each operand needs to be set with at least one landmark.");
			return tcookie3f(index, typeid_pose, fid_landmarks).u64;
		}
		if (operand_must_2landmark(pose.operand_type) &&
			(pose.operands[0].lmk1 == nposm || pose.operands[1].lmk1 == nposm)) {
			err_msg = _("For the selected type, each operand needs to have two landmark set.");
			return tcookie3f(index, typeid_pose, fid_landmarks).u64;
		}

		if (check_range) {
			uint64_t res = pose.is_valid2_range(index, err_msg);
			if (res != TCOOKIE3F_CHECK_OK) {
				VALIDATE(!err_msg.empty(), null_str);
				return res;
			}
		}

		if (pose.unsatisfied_msgstr.empty() || !utils::is_utf8str(pose.unsatisfied_msgstr.c_str(), pose.unsatisfied_msgstr.size())) {
			return tcookie3f(index, typeid_pose, fid_pose_unsatisfied_msgstr).u64;
		}
		if (!utils::is_utf8str(pose.unsatisfied_rmax_msgstr.c_str(), pose.unsatisfied_rmax_msgstr.size())) {
			return tcookie3f(index, typeid_pose, fid_pose_unsatisfied_rmax_msgstr).u64;
		}
		if (pose.unsatisfied_msgstr.empty() && !pose.unsatisfied_rmax_msgstr.empty()) {
			symbols["generic"] = get_field_str(typeid_pose, fid_pose_unsatisfied_msgstr);
			symbols["max"] = get_field_str(typeid_pose, fid_pose_unsatisfied_rmax_msgstr);
			err_msg = vgettext2("When only one voice is required, just set the '$generic', no need to configure the '$max'.", symbols);
			return tcookie3f(index, typeid_pose, fid_pose_unsatisfied_msgstr).u64;
		}
/*
		if (!utils::is_utf8str(pose.legend.c_str(), pose.legend.size())) {
			return tcookie3f(index, typeid_pose, fid_pose_legend).u64;
		}
*/
	}

	return TCOOKIE3F_CHECK_OK;
}

std::string twkoscript::tstate2::fomrat_is_valid2_result(uint64_t res, const std::string& err_msg) const
{
	VALIDATE(res != TCOOKIE3F_CHECK_OK, null_str);

	tcookie3f cookie3f(res);

	std::stringstream err;
	if (err_msg.empty()) {
		err << get_error_msg(cookie3f);
	} else {
		err << err_msg;
	}

	if (cookie3f.field >= aplt::twkoscript::fid_phase_min && cookie3f.field <= aplt::twkoscript::fid_phase_max) {
		utils::string_map symbols;
		symbols["number"] = str_cast(cookie3f.index + 1);
		symbols["msg"] = err.str();
		err.str("");
		err << vgettext2("In $number phase, $msg", symbols);

	} else if (cookie3f.type == aplt::twkoscript::typeid_pose) {
		utils::string_map symbols;
		symbols["number"] = str_cast(cookie3f.index + 1);
		symbols["msg"] = err.str();
		err.str("");
		err << vgettext2("In $number pose, $msg", symbols);
	}
	return err.str();
}

bool twkoscript::tstate2::state_swap(int s1, int s2)
{
	VALIDATE(s1 != nposm && s2 != nposm && s1 != s2, null_str);

	bool modified = false;
	if (state == s1) {
		state = s2;
		modified = true;

	} else if (state == s2) {
		state = s1;
		modified = true;
	}

	modified |= next.state_swap(s1, s2);

	return modified;
}

// use 'lmk33_png_at' always.
std::string twkoscript::tstate2::build_lmk33_png_filename(const std::string& phase_surf_dir, int phase) const
{
	VALIDATE(!phase_surf_dir.empty(), null_str);
	VALIDATE(lmk33_png_at >= 0 && phase >= 0, null_str);

	char buf[512];
	// lmk33_1_0.png
	SDL_snprintf(buf, sizeof(buf), "%s/lmk33_%i_%i.png", phase_surf_dir.c_str(), lmk33_png_at, phase);
	return buf;
}

int twkoscript::tstate2::did_enter_state()
{
	VALIDATE(script_ != nullptr, "before this, need to call script_.enable_run()");

	int posture_fields = 0;

	if (track_pose.valid()) {
		VALIDATE(unsatisfied_2th_threshold_s * 1000 > unsatisfied_threshold_ms, null_str);

		VALIDATE(unsatisfied_msg_.empty(), null_str);
		VALIDATE(next_speak_unsatisfied_msg_ticks_ == 0, null_str);
		VALIDATE(speak_unsatisfied_msgstr_count_ == 0, null_str);

		sn_update_next_unsatisfied_ticks(false);

		VALIDATE(track_pose.curr_phase_ == nposm, null_str);
		track_pose.curr_phase_ = 0;

		track_pose.update_landmarks(*script_);
		
	}

	VALIDATE(enter_state_ticks_ == 0, nullptr);
	enter_state_ticks_ = SDL_GetTicks();
	SDL_Log("%u {dbg-min-state}did_enter_state, enter_state_ticks_: %u", SDL_GetTicks(), enter_state_ticks_);
	VALIDATE(first_satisfied_ticks_ == 0, null_str);
	VALIDATE(speak_satisfied_msgstr1_only_once_ == bool_set_none, null_str);

	if (task != nullptr) {
		task->did_enter_state();
	}
	return posture_fields;
}

void twkoscript::tstate2::did_exit_state()
{
	// reverse task
	if (task != nullptr) {
		task->did_exit_state();
	}

	if (track_pose.valid()) {
		VALIDATE(track_pose.curr_phase_ >= 0, null_str);
		track_pose.curr_phase_ = nposm;
		track_pose.landmark_count = 0;
	}

	if (!satisfied_msgstr1.empty()) {
		satisfied_threshold_s1 = nposm;
		satisfied_msgstr1.clear();
	} else {
		VALIDATE(satisfied_threshold_s1 == nposm, null_str);
	}

	SDL_Log("%u {dbg-min-state}did_exit_state, enter_state_ticks_ = 0", SDL_GetTicks());
	enter_state_ticks_ = 0;
	first_satisfied_ticks_ = 0;
	speak_satisfied_msgstr1_only_once_ = bool_set_none;
}

void twkoscript::tstate2::did_mediapipe_new_frame(bool satisfied, const std::string& _unsatisfied_msg)
{
	const uint32_t now = SDL_GetTicks();
	if (satisfied) {
		VALIDATE(_unsatisfied_msg.empty(), null_str);

		if (first_satisfied_ticks_ == 0) {
			first_satisfied_ticks_ = now;
		}
		if (threshold_first_satisfied_ticks_ == 0) {
			threshold_first_satisfied_ticks_ = now;

			if (!satisfied_msgstr1.empty() && speak_satisfied_msgstr1_only_once_ != bool_set_false) {
				next_speak_satisfied_msg_ticks_ = now + satisfied_threshold_s1 * 1000;
			}
		}

		last_satisfied_ticks_ = now;

		next_speak_unsatisfied_msg_ticks_ = 0;
		speak_unsatisfied_msgstr_count_ = 0;

	} else {
		VALIDATE(!_unsatisfied_msg.empty(), null_str);
	}

	unsatisfied_msg_ = _unsatisfied_msg;

	if (task != nullptr) {
		task->did_mediapipe_new_frame(satisfied);
	}
}

void twkoscript::tstate2::slice()
{
	VALIDATE(task != nullptr, null_str);

	if (debug_skip && !task->finished()) {
		task->set_finished();
	}

	if (next_speak_unsatisfied_msg_ticks_ != 0 && 
		!pinyin_.is_speaking() && SDL_GetTicks() > next_speak_unsatisfied_msg_ticks_) {
		sn_speak_unsatisfied_msg();
	}
	if (next_speak_satisfied_msg_ticks_ != 0 && SDL_GetTicks() > next_speak_satisfied_msg_ticks_) {
		// during this satisfied section, only speak satisfied_msgstr once.
		next_speak_satisfied_msg_ticks_ = 0;
		pinyin_.speak(satisfied_msgstr1);

		if (speak_satisfied_msgstr1_only_once_ == bool_set_true) {
			speak_satisfied_msgstr1_only_once_ = bool_set_false;
		}
	}

	VALIDATE(task != nullptr, null_str);
	if (threshold_first_satisfied_ticks_ != 0) {
		VALIDATE(last_satisfied_ticks_ != 0, null_str);
		VALIDATE(next_speak_unsatisfied_msg_ticks_ == 0, null_str);
		VALIDATE(speak_unsatisfied_msgstr_count_ == 0, null_str);
		if ((int)(SDL_GetTicks() - last_satisfied_ticks_) <= unsatisfied_threshold_ms) {
			
		} else {
			b_api_.health_push_n32_event(workoutevt_n32, workoutn32_alert);
			sn_speak_unsatisfied_msg();

			threshold_first_satisfied_ticks_ = 0;
			next_speak_satisfied_msg_ticks_ = 0;

			task->did_satisfied_to_unsatisfied();
		}
	}

	task->slice();
}

void twkoscript::tstate2::set_satisfied_msgstr(int threshold_s, const std::string& msgstr)
{
	VALIDATE(satisfied_threshold_s1 == nposm && satisfied_msgstr1.empty(), null_str);
	VALIDATE(threshold_s > 0 && !msgstr.empty(), null_str);

	satisfied_threshold_s1 = threshold_s;
	satisfied_msgstr1 = msgstr;
}

void twkoscript::tstate2::sn_update_next_unsatisfied_ticks(bool use_2th_threshold)
{
	int ms_threshold = unsatisfied_threshold_ms;
	if (use_2th_threshold) {
		// second or more, use larger threshold.
		ms_threshold = unsatisfied_2th_threshold_s * 1000;
	}
	next_speak_unsatisfied_msg_ticks_ = SDL_GetTicks() + ms_threshold;
}

void twkoscript::tstate2::sn_speak_unsatisfied_msg()
{
	// If is 'unsatisfied' from the beginning, then the first time it enters here, 
	// the 'next_speak_unsatisfied_msg_ticks_' will not be 0. 
	// Only when it changes from 'satisfied' to 'unsatisfied', will 'sn_next_unsatisfied_ticks_' be 0 for the first time.
	const bool is_first_in_unsatisfied = next_speak_unsatisfied_msg_ticks_ == 0;

	// speak_unsatisfied_msgstr_count_ >= 1 && is even.
	bool time_to_unsatisfied_2th_msgstr = speak_unsatisfied_msgstr_count_ >= 1 && (speak_unsatisfied_msgstr_count_ % 2 == 1);

	if (time_to_unsatisfied_2th_msgstr) {
		// 'unsatisfied_2th_msgstr' may be empty. If it is empty, change it to speak 'unsatisfied_msg_'.
		const std::string& msg = !unsatisfied_2th_msgstr.empty()? unsatisfied_2th_msgstr: unsatisfied_msg_;
		if (!msg.empty()) {
			pinyin_.speak(msg);
		}
	} else {
		pinyin_.speak(unsatisfied_msg_);
	}
	unsatisfied_msg_.clear();
	speak_unsatisfied_msgstr_count_ ++;

	sn_update_next_unsatisfied_ticks(true);
}

std::string twkoscript::build_lmk33_png_basename(int state, int phase)
{
	VALIDATE(state >= 0 && phase >= 0, null_str);

	char buf[256];
	// lmk33_1_0.png
	SDL_snprintf(buf, sizeof(buf), "lmk33_%i_%i.png", state, phase);
	return buf;
}

std::string twkoscript::build_lmk33_png_filename3(const std::string& phase_surf_dir, int state, int phase, bool is_tmp)
{
	VALIDATE(!phase_surf_dir.empty(), null_str);
	VALIDATE(state >= 0 && phase >= 0, null_str);

	char buf[512];
	if (is_tmp) {
		// lmk33_1_0.png.tmp
		SDL_snprintf(buf, sizeof(buf), "%s/lmk33_%i_%i.png.tmp", phase_surf_dir.c_str(), state, phase);
	} else {
		// lmk33_1_0.png
		SDL_snprintf(buf, sizeof(buf), "%s/lmk33_%i_%i.png", phase_surf_dir.c_str(), state, phase);
	}
	return buf;
}

twkoscript::twkoscript()
	: pinyin_(aplt::get_curr_pinyin())
	, tone_(PINYIN_DEF_TONE)
	, eng_lowercase_(PINYIN_DEF_ENG_LOWERCASE)
	, input_var_signature_("input_var_")
	, satisfied_msgstr_(_("Satisfied"))
	, unsatisfied_msgstr_(_("Unsatisfied"))
	, slot_(nullptr)
	, reset_samples_threshold_ms_(5000) // 5s
{
	VALIDATE(WKO_MAX_ANTI_SHAKE_FIELDS	== WKO_MAX_POSES_PER_TRACK * 2, null_str);

	clear();
	reset_anti_shike_samples(0);

	if (wko_task_types.empty()) {
		wko_task_types.insert(std::make_pair(tasktype_speak, tcode3(tasktype_speak, "speak", _("wko^tasktype_speak"))));
		wko_task_types.insert(std::make_pair(tasktype_time_counter, tcode3(tasktype_time_counter, "time_counter", _("wko^tasktype_time_counter"))));
		wko_task_types.insert(std::make_pair(tasktype_rep_counter, tcode3(tasktype_rep_counter, "rep_counter", _("wko^tasktype_rep_counter"))));
	}
	VALIDATE((int)wko_task_types.size() == tasktype_count, null_str);

	if (wko_pose_types.empty()) {
		wko_pose_types.insert(std::make_pair(posetype_angle3p, tcode3(posetype_angle3p, "angle3p", "")));
		wko_pose_types.insert(std::make_pair(posetype_angle2p, tcode3(posetype_angle2p, "angle2p", "")));
		wko_pose_types.insert(std::make_pair(posetype_diff, tcode3(posetype_diff, "diff", "")));
	}
	VALIDATE((int)wko_pose_types.size() == posetype_count, null_str);

	if (wko_operand_types.empty()) {
		wko_operand_types.insert(std::make_pair(operandtype_point, tcode3(operandtype_point, "point", "")));
		wko_operand_types.insert(std::make_pair(operandtype_x, tcode3(operandtype_x, "x", "")));
		wko_operand_types.insert(std::make_pair(operandtype_y, tcode3(operandtype_y, "y", "")));
		wko_operand_types.insert(std::make_pair(operandtype_dist_x, tcode3(operandtype_dist_x, "dist_x", "")));
		wko_operand_types.insert(std::make_pair(operandtype_dist_y, tcode3(operandtype_dist_y, "dist_y", "")));
	}
	VALIDATE((int)wko_operand_types.size() == operandtype_count, null_str);

	if (wko_ang_ranges.empty()) {
		wko_ang_ranges.insert(std::make_pair(angrange_180, tcode3(angrange_180, "180", "")));
		wko_ang_ranges.insert(std::make_pair(angrange_360, tcode3(angrange_360, "360", "")));
	}
	VALIDATE((int)wko_ang_ranges.size() == angrange_count, null_str);

	if (wko_time_rules.empty()) {
		wko_time_rules.insert(std::make_pair(timerule_total, tcode3(timerule_total, "total", _("wko^timerule_total"))));
		wko_time_rules.insert(std::make_pair(timerule_satisfied, tcode3(timerule_satisfied, "satisfied", _("wko^timerule_satisfied"))));
		wko_time_rules.insert(std::make_pair(timerule_strict, tcode3(timerule_strict, "strict", _("wko^timerule_strict"))));
	}
	VALIDATE((int)wko_time_rules.size() == timerule_count, null_str);

	if (wko_time_tones.empty()) {
		wko_time_tones.insert(std::make_pair(timetone_full, tcode3(timetone_full, "full", _("wko^timetone_full"))));
		wko_time_tones.insert(std::make_pair(timetone_split, tcode3(timetone_split, "split", _("wko^timetone_split"))));
		wko_time_tones.insert(std::make_pair(timetone_silent, tcode3(timetone_silent, "silent", _("wko^timetone_silent"))));
	}
	VALIDATE((int)wko_time_tones.size() == timetone_count, null_str);

	if (pose_sides.empty()) {
		pose_sides.insert(std::make_pair(poseside_left, tcode3(poseside_left, "left", _("Left"))));
		pose_sides.insert(std::make_pair(poseside_right, tcode3(poseside_right, "right", _("Right"))));
		pose_sides.insert(std::make_pair(poseside_other, tcode3(poseside_other, "other", _("Other"))));
	}
	VALIDATE((int)pose_sides.size() == poseside_count, null_str);

	if (pose_metrics.empty()) {
		pose_metrics.insert(std::make_pair(posemetric_body_tilt_angle, tcode3(posemetric_body_tilt_angle, 
			"body_tilt_angle", _("wko^posemetric_body_tilt_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_header_tilt_angle, tcode3(posemetric_header_tilt_angle, 
			"header_tilt_angle", _("wko^posemetric_header_tilt_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_left_upper_arm_angle, tcode3(posemetric_left_upper_arm_angle, 
			"left_upper_arm_angle", _("wko^posemetric_left_upper_arm_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_right_upper_arm_angle, tcode3(posemetric_right_upper_arm_angle, 
			"right_upper_arm_angle", _("wko^posemetric_right_upper_arm_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_left_elbow_angle, tcode3(posemetric_left_elbow_angle, 
			"left_elbow_angle", _("wko^posemetric_left_elbow_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_right_elbow_angle, tcode3(posemetric_right_elbow_angle, 
			"right_elbow_angle", _("wko^posemetric_right_elbow_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_left_forearm_angle, tcode3(posemetric_left_forearm_angle, 
			"left_forearm_angle", _("wko^posemetric_left_forearm_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_right_forearm_angle, tcode3(posemetric_right_forearm_angle, 
			"right_forearm_angle", _("wko^posemetric_right_forearm_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_left_wrist_on_right_shoulder, tcode3(posemetric_left_wrist_on_right_shoulder, 
			"left_wrist_on_right_shoulder", _("wko^posemetric_left_wrist_on_right_shoulder"))));
		pose_metrics.insert(std::make_pair(posemetric_right_wrist_on_left_shoulder, tcode3(posemetric_right_wrist_on_left_shoulder, 
			"right_wrist_on_left_shoulder", _("wko^posemetric_right_wrist_on_left_shoulder"))));
		pose_metrics.insert(std::make_pair(posemetric_left_thigh_angle, tcode3(posemetric_left_thigh_angle, 
			"left_thigh_angle", _("wko^posemetric_left_thigh_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_right_thigh_angle, tcode3(posemetric_right_thigh_angle, 
			"right_thigh_angle", _("wko^posemetric_right_thigh_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_left_knee_angle, tcode3(posemetric_left_knee_angle, 
			"left_knee_angle", _("wko^posemetric_left_knee_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_right_knee_angle, tcode3(posemetric_right_knee_angle, 
			"right_knee_angle", _("wko^posemetric_right_knee_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_left_lower_leg_angle, tcode3(posemetric_left_lower_leg_angle, 
			"left_lower_leg_angle", _("wko^posemetric_left_lower_leg_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_right_lower_leg_angle, tcode3(posemetric_right_lower_leg_angle, 
			"right_lower_leg_angle", _("wko^posemetric_right_lower_leg_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_left_leg_angle, tcode3(posemetric_left_leg_angle, 
			"left_leg_angle", _("wko^posemetric_left_leg_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_right_leg_angle, tcode3(posemetric_right_leg_angle, 
			"right_leg_angle", _("wko^posemetric_right_leg_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_right_hip_angle, tcode3(posemetric_right_hip_angle, 
			"right_hip_angle", _("wko^posemetric_right_hip_angle"))));
		pose_metrics.insert(std::make_pair(posemetric_right_torso_angle, tcode3(posemetric_right_torso_angle, 
			"right_torso_angle", _("wko^posemetric_right_torso_angle"))));

		for (std::map<int, tcode3>::const_iterator it = pose_metrics.begin(); it != pose_metrics.end(); ++ it) {
			VALIDATE(it->first == it->second.code, null_str);
		}
	}
	VALIDATE((int)pose_metrics.size() == posemetric_count, null_str);


	memset(&overlay_msg_, 0, sizeof(tuint8data2_C));
}

bool twkoscript::from_state2_cfg(const config& cfg, const std::map<std::string, int>& _state_names_map, int pose_states_parsed, tstate2** state2_result)
{
	VALIDATE(state2_result != nullptr, null_str);
	*state2_result = nullptr;

	const std::map<std::string, int>& state_names_map = _state_names_map;

	const std::string state_name = cfg["state"].str();
	if (!isvalid_normal_utf8_name224(state_name) || state_names_map.count(state_name) == 0) {
		return false;
	}
	VALIDATE(state_names_map.count(state_name) != 0, null_str);

	// int enum_state = state_names.size();
	// state_names_map.insert(std::make_pair(state_name, enum_state));
	int enum_state = state_names_map.find(state_name)->second;

	state_names.push_back(state_name);
	std::pair<std::map<int, tstate2>::iterator, bool> ins = states.insert(
		std::make_pair(enum_state, tstate2(enum_state)));
	VALIDATE(ins.second, null_str);

	tstate2& state2 = ins.first->second;
	state2.debug_skip = cfg["debug_skip"].to_bool();

	// [track_pose]
	if (cfg.has_child("track_pose")) {
		const config& track_pose_cfg = cfg.child("track_pose");
		bool retval = state2.track_pose.from_cfg(track_pose_cfg);
		if (!retval) {
			return false;
		}
	}

	// [task]
	if (cfg.has_child("task")) {
		const config& task_cfg = cfg.child("task");
		const std::string type_str = task_cfg["type"].str();
		int type = wko_task_type_from_str(type_str);
		if (type == nposm) {
			return false;
		}

		VALIDATE(state2.task == nullptr, null_str);
		ttask_base* task = state2.new_task(type);
		if (!task->from_cfg(task_cfg)) {
			delete task;
			return false;
		}
		state2.task = task;

	} else {
		return false;
	}

	// [next]
	state2.next.from_cfg("next", state_names_map.size(), cfg);

	if (state2.track_pose.valid()) {
		// track pose relative
/*
		state2.satisfied_threshold_s = cfg["satisfied_threshold_s"].to_int(nposm);
		state2.satisfied_msgstr = cfg["satisfied_msgstr"].str();
*/
		// 'state2.satisfied_msgstr' allow empty. when empty, don't update 'next_speak_satisfied_msg_ticks_'.
		// 'state2.satisfied_threshold_s' only require >= 0.
		state2.is_setup = cfg["is_setup"].to_bool();
		if (state2.is_setup && pose_states_parsed != 0) {
			// Setup can only be the first pose state.
			return false;
		}

		state2.unsatisfied_threshold_ms = cfg["unsatisfied_threshold_ms"].to_int(nposm);
		state2.unsatisfied_2th_threshold_s = cfg["unsatisfied_2th_threshold_s"].to_int(nposm);
		// state2.unsatisfied_msgstr = cfg["unsatisfied_msgstr"].str();
		if (state2.unsatisfied_threshold_ms == nposm) {
			return false;
		}

		if (state2.unsatisfied_2th_threshold_s * 1000 <= state2.unsatisfied_threshold_ms) {
			return false;
		}

		// allow 'unsatisfied_2th_msgstr' is empty.
		state2.unsatisfied_2th_msgstr = cfg["unsatisfied_2th_msgstr"].str();
	}
	*state2_result = &state2;
	return true;
}

void twkoscript::did_from_cfg_quited(const std::string& err_msg)
{
	if (!err_msg.empty()) {
		clear();
	}
}

bool twkoscript::from_cfg(const config& cfg)
{
	clear();

	std::string err_msg;
	tauto_destruct_executor destruct_executor(std::bind(&twkoscript::did_from_cfg_quited, this, std::ref(err_msg)));

	id = cfg["id"].str();
	name = cfg["name"].str();

	if (!isvalid_normal_id_or_var_name224(id) || !isvalid_short_utf8_name216(name)) {
		err_msg = "[twkoscript cfg]id or name is invalid";
		return false;
	}
	author = cfg["author"].str();
	reference = cfg["reference"].str();

	std::map<std::string, int> state_names_map;
	int enum_state = 0;
	std::vector<const config*> state2_cfgs;
	BOOST_FOREACH (const config &state2_cfg, cfg.child_range("state2")) {
		const std::string state_name = state2_cfg["state"].str();
		state_names_map.insert(std::make_pair(state_name, enum_state));
		enum_state ++;
		state2_cfgs.push_back(&state2_cfg);
	}
	int pose_states_parsed = 0;
	tstate2* curr_state2 = nullptr;
	for (std::vector<const config*>::const_iterator it = state2_cfgs.begin(); it != state2_cfgs.end(); ++ it) {
		const config& state2_cfg = **it;
		if (!from_state2_cfg(state2_cfg, state_names_map, pose_states_parsed, &curr_state2)) {
			err_msg = "[twkoscript cfg]state is invalid.";
			return false;
		}
		if (!curr_state2->track_pose.poses.empty()) {
			pose_states_parsed ++;
		}
	}

	version = version_info(cfg["version"].str());
	if (!version.is_rose_recommended()) {
		err_msg = "[twkoscript cfg]version is invalid.";
		return false;
	}

	startup_state.from_cfg("startup_state", state_names_map.size(), cfg);

	calc_pose_state_at();

	return true;
}

void twkoscript::to_cfg(config& cfg) const
{
	cfg.clear();

	VALIDATE(isvalid_normal_id_or_var_name224(id), null_str);
	cfg["id"] = id;
	cfg["name"] = name;
	if (!author.empty()) {
		cfg["author"] = author;
	}
	if (!reference.empty()) {
		cfg["reference"] = reference;
	}
	cfg["version"] = game_config::rose_version.str(true);

	for (std::map<int, tstate2>::const_iterator it = states.begin(); it != states.end(); ++ it) {
		const tstate2& state2 = it->second;

		config& state2_cfg = cfg.add_child("state2");
		state2.to_cfg(state_names, state2_cfg);
	}
}

bool twkoscript::equal(const aplt::twkoscript& that) const
{
	if (id != that.id || name != that.name || author != that.author || reference != that.reference ||
		startup_state != that.startup_state) {
		return false;
	}
	// don't compare 'version'.

	if (state_names.size() != that.state_names.size() || state_names != that.state_names) {
		return false;
	}

	if (states.size() != that.states.size() || states != that.states) {
		return false;
	}

	return true;
}

void twkoscript::assign(const aplt::twkoscript& that)
{
	// don't inclide cfg_str. it isn't cfg key.
	// cfg_str = that.cfg_str;

	id = that.id;
	name = that.name;
	author = that.author;
	reference = that.reference;
	startup_state = that.startup_state;

	state_names = that.state_names;
	states = that.states;
}

uint64_t twkoscript::is_valid2(std::string& err_msg, const tstate2** err_state, bool check_range) const
{
	if (err_state != nullptr) {
		*err_state = nullptr;
	}

	if (!isvalid_normal_id_or_var_name224(id)) {
		return tcookie3f(0, typeid_script, fid_id).u64;
	}
	if (!isvalid_short_utf8_name216(name)) {
		return tcookie3f(0, typeid_script, fid_name).u64;
	}

	if (states.empty()) {
		err_msg = _("At least one state is required.");
		return tcookie3f(0, typeid_script, fid_states).u64;
	}

	utils::string_map symbols;
	std::set<std::string> existed_state_names;
	int pose_states_parsed = 0;
	int state2_at = 0;
	for (std::map<int, aplt::twkoscript::tstate2>::const_iterator it = states.begin(); it != states.end(); ++ it, state2_at ++) {
		const aplt::twkoscript::tstate2& state2 = it->second;
		VALIDATE(state2.task != nullptr, null_str);

		const std::string& state_name = state_names[state2.state];
		uint32_t res = TCOOKIE3F_CHECK_OK;
		if (!isvalid_normal_utf8_name224(state_name)) {
			res = tcookie3f(0, typeid_global, fid_name).u64;
		}
		if (res == TCOOKIE3F_CHECK_OK) {
			if (existed_state_names.count(state_name) != 0) {
				err_msg = _("State names cannot be duplicated.");
				res = tcookie3f(0, typeid_global, fid_name).u64;

			} else {
				existed_state_names.insert(state_name);
			}
		}

		if (res == TCOOKIE3F_CHECK_OK && state2.debug_skip) {
			if (state2.state == (int)(states.size() - 1)) {
				symbols["field"] = state2.get_field_str(typeid_global, fid_debug_skip);

				err_msg = vgettext2("The last state cannot enable '$field'.", symbols);
				res = tcookie3f(0, typeid_global, fid_debug_skip).u64;
			}
		}

		if (res == TCOOKIE3F_CHECK_OK) {
			res = state2.is_valid2(pose_states_parsed, err_msg, check_range);
		}
		if (res != TCOOKIE3F_CHECK_OK) {
			if (err_state != nullptr) {
				*err_state = &state2;
			}
			return res;
		}
		if (!state2.track_pose.poses.empty()) {
			pose_states_parsed ++;
		}
	}
	return TCOOKIE3F_CHECK_OK;
}

int twkoscript::pose_state_count(bool* has_setup) const
{
	// Do not accumulate 'setup' state.
	VALIDATE(valid(), null_str);

	if (has_setup != nullptr) {
		*has_setup = false;
	}

	int result = 0;
	for (std::map<int, tstate2>::const_iterator it = states.begin(); it != states.end(); ++ it) {
		const tstate2& state2 = it->second;
		if (state2.track_pose.poses.empty()) {
			continue;
		}
		if (state2.is_setup) {
			if (has_setup != nullptr) {
				*has_setup = true;
			}
		} else {
			result ++;
		}
	}
	return result;
}

void twkoscript::calc_pose_state_at()
{
	VALIDATE(valid(), null_str);

	bool has_setup = false;
	int pose_states_parsed = 0;
	for (std::map<int, tstate2>::iterator it = states.begin(); it != states.end(); ++ it) {
		tstate2& state2 = it->second;
		VALIDATE(state2.pose_state_at_ == nposm, null_str);

		if (!state2.track_pose.poses.empty()) {
			if (state2.is_setup) {
				has_setup = true;

			} else {
				state2.pose_state_at_ = pose_states_parsed - (has_setup? 1: 0);
			}
			pose_states_parsed ++;

		} else {
			// VALIDATE(state2.pose_state_at_ == nposm, null_str);
		}
	}
}

std::string twkoscript::build_script_filename(const std::string& wkoscript_dir) const
{
	VALIDATE(!wkoscript_dir.empty(), null_str);
	VALIDATE(isvalid_normal_id_or_var_name224(id), null_str);

	return wkoscript_dir + "/" + id + ".cfg";
}

std::string twkoscript::build_phase_surf_dir(const std::string& wkoscript_dir) const
{
	VALIDATE(!wkoscript_dir.empty(), null_str);
	VALIDATE(isvalid_normal_id_or_var_name224(id), null_str);

	return wkoscript_dir + "/" + id;
}

/*
void twko_script::sync_task_input_vars(const std::map<aplt::taplt_key, aplt::tapplet>& applets)
{
	for (std::map<int, tcpp_api::tstate2>::iterator it = states.begin(); it != states.end(); ++ it) {
		tcpp_api::tstate2& state2 = it->second;
		aplt::ttask_pair task_pair = task_pair_from_2_id(applets, state2.async_task.aplt_id, state2.async_task.task_id, true, true);
		if (task_pair.task != nullptr) {
			state2.sync_task_input_vars(*task_pair.aplt, *task_pair.task);
		}
	}
}
*/

void twkoscript::get_state_names(const std::string& exclude, std::set<std::string>& result) const
{
	result.clear();
	for (std::vector<std::string>::const_iterator it = state_names.begin(); it != state_names.end(); ++ it) {
		const std::string& name = *it;
		if (exclude.empty() || name != exclude) {
			result.insert(name);
		}
	}
}

bool twkoscript::state_swap(int s1, int s2)
{
	VALIDATE(s1 != nposm && s2 != nposm && s1 != s2, null_str);
/*
	// 1)reception state
	if (reception_state == s1) {
		reception_state = s2;

	} else if (reception_state == s2) {
		reception_state = s1;
	}
*/
	// 2)startup state
	startup_state.state_swap(s1, s2);

	// 3) state2
	std::iter_swap(state_names.begin() + s1, state_names.begin() + s2);

	// !!!std::map don't support std::iter_swap().
	for (std::map<int, tstate2>::iterator it = states.begin(); it != states.end(); ++ it) {
		tstate2& state2 = it->second;
		state2.state_swap(s1, s2);
	}

	// 1/3: save all state
	tstate2 states_s1 = states.find(s1)->second;
	tstate2 states_s2 = states.find(s2)->second;
	// 2/3: erase.
	states.erase(states.find(s1));
	states.erase(states.find(s2));
	// 3/3: insert, must erase all state before insert.
	std::pair<std::map<int, tstate2>::iterator, bool> ins = states.insert(std::make_pair(states_s1.state, states_s1));
	VALIDATE(ins.second, null_str);
	ins = states.insert(std::make_pair(states_s2.state, states_s2));
	VALIDATE(ins.second, null_str);

/*		
	for (std::vector<tcpp_api::tkey_2_state>::iterator it = key_2_states.begin(); it != key_2_states.end(); ++ it) {
		tcpp_api::tkey_2_state& key_2_state = *it;
		key_2_state.state_swap(s1, s2);
	}
*/
	return true; // modified
}

twkoscript::tstate2& twkoscript::insert_state(int after_at, const std::string& _name, int task_type, int lmk33_png_at)
{
	// VALIDATE(after_at == nposm || (after_at >= 0 && after_at < states.size()), null_str);
	// now only support insert at the end.
	VALIDATE(after_at == nposm, null_str);
	VALIDATE(wko_task_types.count(task_type) != 0, null_str);
	VALIDATE(lmk33_png_at >= states.size(), null_str);

	std::string new_state_name = _name;
	if (_name.empty()) {
		std::set<std::string> existed_names;
		get_state_names(null_str, existed_names);
		new_state_name = utils::unique_untitle_name(existed_names, null_str, 1);
	}

	VALIDATE(state_names.size() == states.size(), null_str);
	int enum_state = states.size();
	state_names.push_back(new_state_name);
	std::pair<std::map<int, tstate2>::iterator, bool> ins = states.insert(
		std::make_pair(enum_state, tstate2(enum_state)));
	VALIDATE(ins.second, null_str);

	tstate2& state2 = ins.first->second;
	state2.lmk33_png_at = lmk33_png_at;

	if (wko_is_pose_state2_from_task_type(task_type)) {
		// evalue default values.
		state2.unsatisfied_threshold_ms = 1500;
		state2.unsatisfied_2th_threshold_s = 5;

		state2.track_pose.insert_pose(nposm, null_str);
	}

	state2.task = state2.new_task(task_type);
	if (state2.task->type == tasktype_time_counter) {
		ttime_counter* task2 = static_cast<ttime_counter*>(state2.task);
		task2->rule = timerule_total;
		task2->tone = timetone_full;
		task2->satisfied_threshold_s = 3;

	} else if (state2.task->type == tasktype_rep_counter) {
		// state2.unsatisfied_2th_msgstr = "fake";
		trep_counter* task2 = static_cast<trep_counter*>(state2.task);
		for (int phase_at = 0; phase_at < WKO_MAX_PHASE_COUNT; phase_at ++) {
			task2->phases.push_back(tphase());

			tphase& phase = task2->phases.back();
			phase.min_duration_ms = 1000;
			phase.cooldowned_ms = 400;
			phase.action_msg = str_cast(phase_at + 1);
		}
	}

	return state2;
}

void twkoscript::erase_state(int erase_state)
{
	VALIDATE(erase_state >= 0 && erase_state < (int)states.size(), null_str);

	// 1)erase it from script.states
	std::map<int, tstate2>::iterator states_it = states.find(erase_state);
	states.erase(states_it);

	// 2)erase it from script.state_names
	std::vector<std::string>::iterator names_it = state_names.begin();
	if (erase_state != 0) {
		std::advance(names_it, erase_state);
	}
	state_names.erase(names_it);

	// 3)update those state2.state that state > erase_state
	const int new_state_size = state_names.size();
	for (int state = erase_state; state < new_state_size; state ++) {
		std::map<int, tstate2>::iterator states_it = states.find(state + 1);
		tstate2 tmp_state2 = states_it->second;
		tmp_state2.state = state;
		states.insert(std::make_pair(tmp_state2.state, tmp_state2));
		states.erase(states_it);
	}
	VALIDATE(states.size() == state_names.size(), null_str);

	// 4)update state2.next's state that has > erase_state.
	for (int state = 0; state < new_state_size; state ++) {
		std::map<int, tstate2>::iterator states_it = states.find(state);
		tstate2& tmp_state2 = states_it->second;
		tmp_state2.next.state_sub1(erase_state);
	}

	//
	// 5)startup_state 'sub1'
	//
	if (startup_state.state_sub1(erase_state)) {
		// The purpose of 'sub1' is to keep the value in the 'previous' state, 
		// and the 'previous' display is already in the state of the previous value, 
		// so the display content can not be changed
	}

	// 4)although key_2_states don't have @erase_state, 
	// but these state that more than @erase_state, have to '-1' 
	//
}

void twkoscript::clone_state(int state_at)
{
	VALIDATE(state_at >= 0 && state_at < (int)states.size(), null_str);

	const tstate2& src_state2 = states.find(state_at)->second;
 
	std::set<std::string> existed_names;
	get_state_names(null_str, existed_names);
	std::string new_state_name = utils::unique_untitle_name(existed_names, null_str, 1);

	VALIDATE(state_names.size() == states.size(), null_str);
	int enum_state = states.size();
	state_names.push_back(new_state_name);
	std::pair<std::map<int, tstate2>::iterator, bool> ins = states.insert(
		std::make_pair(enum_state, tstate2(enum_state)));
	VALIDATE(ins.second, null_str);

	tstate2& target_state2 = ins.first->second;
	target_state2 = src_state2;

	// correct state2.state
	target_state2.state = enum_state;
	if (src_state2.lmk33_png_at != nposm) {
		target_state2.lmk33_png_at = target_state2.state;
	}


	// next is empty
	target_state2.next.clear();
}

void twkoscript::set_lmk33_png_at_equal_to_state_at()
{
	for (std::map<int, tstate2>::iterator it = states.begin(); it != states.end(); ++ it) {
		tstate2& state2 = it->second;
		state2.lmk33_png_at = state2.state;
	}
}

void twkoscript::history_after_one_finish(const SDL_Range& range_ms, uint32_t& reps, uint32_t& duration_s) const
{
	// inc_reps = 0;
	// inc_duration_s = 0;

	const twkoscript::tstate2* first_pose_state = nullptr;
	int pose_state_count = 0;
	for (std::map<int, twkoscript::tstate2>::const_iterator it = states.begin(); it != states.end(); ++ it) {
		const twkoscript::tstate2& state2 = it->second;
		if (!state2.is_setup && !state2.track_pose.poses.empty()) {
			if (first_pose_state == nullptr) {
				first_pose_state = &state2;
			}
			pose_state_count ++;
		}
	}

	if (pose_state_count == 1) {
		// For single-state scripts, distinguish between timing and rep.
		VALIDATE(first_pose_state != nullptr, null_str);
		const twkoscript::tstate2& state2 = *first_pose_state;
		if (state2.task->type == twkoscript::tasktype_time_counter) {
			const twkoscript::ttime_counter* task2 = static_cast<const twkoscript::ttime_counter*>(state2.task);
			duration_s += task2->max_count;

		} else {
			VALIDATE(state2.task->type == twkoscript::tasktype_rep_counter, null_str);
			const twkoscript::trep_counter* task2 = static_cast<const twkoscript::trep_counter*>(state2.task);
			reps += task2->max_count;
		}
			
	} else {
		// For multi-state scripts, directly add the duration of this workout session.
		duration_s += (range_ms.max - range_ms.min) / 1000;
	}
}

void twkoscript::green()
{
	VALIDATE(false, null_str);
	for (std::map<int, tstate2>::iterator it = states.begin(); it != states.end(); ++ it) {
		tstate2& state2 = it->second;
		state2.green();
	}
}

std::string twkoscript::py_from_utf8str(const std::string& str) const
{ 
	return pinyin_.from_utf8str2(str, tone_, eng_lowercase_);
}

bool cfg_to_wkoscript(const config& root_cfg, aplt::twkoscript& result)
{
	std::string err_msg;

	bool valid = false;
	const config& workout_cfg = root_cfg.child("workout");
	if (workout_cfg) {
		const std::string id = workout_cfg["id"].str();
		if (id.empty()) {
			return false;
		}

		aplt::twkoscript& pair = result;
		valid = pair.from_cfg(workout_cfg);
		if (valid) {
/*
			if (pair.version != game_config::rose_version) {
				pair.version = game_config::rose_version;
			}
*/
			std::stringstream ss;
			write_config(ss, workout_cfg);
			pair.cfg_str = ss.str();
		}
	}

	return valid;
}

bool twkoscript::from_string(const std::string& str)
{
	clear();

	config root_cfg;
	if (!read_config_ex(str, true, root_cfg)) {
		return false;
	}

	return cfg_to_wkoscript(root_cfg, *this);
}

bool twkoscript::is_id_same_filename(const std::string& filename) const
{
	const std::string short_filename = utils::extract_file(filename);
	return utils::file_stem_name(short_filename) == id;
}

void twkoscript::from_file(const std::string& filename, bool id_must_same)
{
	VALIDATE(!filename.empty(), null_str);
	clear();

	std::string stream;
	const int max_cfg_size = 75 * 1024; // 50K bytes

	// std::string filename = curr_aplt->preferences_dir + "/saves/workout_import.cfg";
	// SDL_Log("load workout scripts from: %s", filename.c_str());
	{
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (fsize > 0 && fsize <= max_cfg_size && utils::is_utf8str(file.data, fsize)) {
			stream.assign(file.data, fsize);
			from_string(stream);
		}
		if (valid() && id_must_same) {
			if (!is_id_same_filename(filename)) {
				clear();
			}
		}
	}
}

void twkoscript::to_file(const std::string& filename) const
{
	VALIDATE(!filename.empty(), null_str);

	VALIDATE(valid(), null_str);
	std::stringstream result;
	result.str("");
	config top_cfg;

	config& pair_cfg = top_cfg.add_child("workout");
	to_cfg(pair_cfg);

	if (!top_cfg.empty()) {
		write_config(result, top_cfg);
	}

	write_file(filename, result.str().c_str(), result.str().size());
}

std::string twkoscript::from_aplt_file(const aplt::tapplet& aplt, const std::string& file)
{
	clear();

	std::string filename = aplt.preferences_dir + "/wkoscript/" + file;
	from_file(filename);
	if (!valid()) {
		filename = aplt.res_path + "/wkoscript/" + file;
		from_file(filename);
	}
	if (!valid()) {
		// if fail, return value is err message.
		return i18n::freq_msgstr_3str(i18n::msgid_load_file_fail, _("Action script"), filename, null_str);
	}
	// if ok, return value is full filename.
	return filename;
}

void twkoscript::enable_run()
{
	for (std::map<int, tstate2>::iterator it = states.begin(); it != states.end(); ++ it) {
		tstate2& state2 = it->second;
		state2.script_ = this;
	}
}

void twkoscript::set_wko_task_slot(twko_task_slot* _slot)
{
	if (_slot != nullptr) {
		VALIDATE(slot_ == nullptr, null_str);
	} else {
		VALIDATE(slot_ != nullptr, null_str);
	}
	VALIDATE(valid(), null_str);
	slot_ = _slot;
}

void twkoscript::did_phase_changed(int state_at, int phase_at)
{
	if (slot_ != nullptr) {
		slot_->did_enter_state_and_phase(state_at, phase_at);
	}
}

bool twkoscript::sfx_enabled()
{
	if (slot_ != nullptr) {
		return slot_->sfx_enabled();
	}
	return false;
}

static int first_uncaptured_landmark(const SDL_FPoint* landmarks, const int* desired, int size)
{
	float bonus = 0.0f; // 0.05f
	float min = bonus;
	float max = 1.0f - bonus;

	for (int at = 0; at < size; at ++) {
		int n = desired[at];
		VALIDATE(n >= 0 && n < mediapipe::kNumPoseLandmarks, null_str);
		if (landmarks[n].x < min || landmarks[n].x > max || landmarks[n].y < min || landmarks[n].y > max) {
			return n;
		}
	}
	return nposm;
}

void twkoscript::set_tmp_result(const twkoscript::tpose& pose, int pose_at, double val0, double val1, tanti_shake_sample_C& posture, tanti_shake_sample_C& avg, ttmp_result& result)
{
	result.pose = &pose;
	VALIDATE(pose_at >= 0, null_str);
	result.pose_at = pose_at;

	VALIDATE(!is_float_nposm(val0), null_str);
	result.val0 = val0;
	result.avg_sample0 = avg.d + posture.fields;
	posture.d[posture.fields ++] = val0;

	if (!is_float_nposm(val1)) {
		result.val1 = val1;
		result.avg_sample1 = avg.d + posture.fields;
		posture.d[posture.fields ++] = val1;

	} else {
		result.val1 = float_nposm;
		result.avg_sample1 = nullptr;
	}
}

// 枚举关键点索引 (简略版，仅列出需要的)
enum {
    RIGHT_SHOULDER = 12,
    RIGHT_ELBOW = 14,
    RIGHT_WRIST = 16
};

// 计算两点距离
double get_distance(SDL_FPoint p1, SDL_FPoint p2) {
    return sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
}

/**
 * @brief 综合检测相机与侧身平面的平行度 (解决45度斜拍问题)
 * @return double 返回 综合误差百分比 (越小越好，<15.0 可认为是平行)
 */
double check_camera_parallelism_advanced(const SDL_FPoint landmarks[33], int use_right, std::string& msg) {
    // 1. 提取基础点
    int shoulder_idx = use_right ? 12 : 11; // 肩膀
    int elbow_idx    = use_right ? 14 : 13; // 手肘
    int wrist_idx    = use_right ? 16 : 15; // 手腕
    int hip_idx      = use_right ? 24 : 23; // 骨盆
    int ankle_idx    = use_right ? 28 : 27; // 脚踝

    SDL_FPoint shoulder = landmarks[shoulder_idx];
    SDL_FPoint elbow    = landmarks[elbow_idx];
    SDL_FPoint wrist    = landmarks[wrist_idx];
    SDL_FPoint hip      = landmarks[hip_idx];
    SDL_FPoint ankle    = landmarks[ankle_idx];

    // 2. 计算四段关键长度
    double upper_arm_len = get_distance(shoulder, elbow); // 上臂
    double forearm_len   = get_distance(elbow, wrist);    // 前臂
    double torso_len     = get_distance(shoulder, hip);   // 躯干 (上半身)
    double leg_len       = get_distance(hip, ankle);      // 下肢 (下半身)

    // 3. 归一化数据安全校验
	char buf[256];
    if (upper_arm_len < 0.02 || forearm_len < 0.02 || torso_len < 0.05 || leg_len < 0.05) {
		SDL_snprintf(buf, sizeof(buf), "(error)upper_arm_len(%.3f) < 0.02 || forearm_len(%.3f) < 0.02 || torso_len(%.3f) < 0.05 || leg_len(%.3f) < 0.05", 
			upper_arm_len, forearm_len, torso_len, leg_len);
        return -1.0; // 关键点检测失败
    }

    // 4. 计算两个比例
    double arm_ratio      = upper_arm_len / forearm_len; // 理想是 1.0
    double body_depth_ratio = torso_len / leg_len;       // 理想是 1.0 (正侧面)

    // 5. 计算误差百分比 (取最大值作为严重度)
    double arm_error      = fabs(1.0 - arm_ratio) * 100.0;
    double body_depth_error = fabs(1.0 - body_depth_ratio) * 100.0;

    // 【核心修正】：取两个误差中的最大值
    // 如果是从斜后方拍，上肢离镜头近，下肢离镜头远：
    // arm_error 可能只有 8%，但 body_depth_error 会高达 40% 甚至更多！
    double max_error = (arm_error > body_depth_error) ? arm_error : body_depth_error;

	SDL_snprintf(buf, sizeof(buf), "upper_arm_len: %.3f, forearm_len: %.3f, arm_ratio: %.3f, arm_error: %.3f\n"
		"torso_len: %.3f, leg_len: %.3f, body_depth_ratio: %.3f, body_depth_error: %.3f\n"
		"max_error: %.3f",
		upper_arm_len, forearm_len, arm_ratio, arm_error,
		torso_len, leg_len, body_depth_ratio, body_depth_error, max_error);
	msg = buf;

    return max_error;
}

/**
 * @brief 最终版检测：利用身体主轴与水平面的倾角来判断相机倾斜
 * 
 * @param landmarks 
 * @param use_right 使用哪一侧的身体
 * @param msg 输出调试信息
 * @return double 返回身体主轴偏离水平线的角度（度）。越接近 0 越平行。
 */
double check_camera_parallelism_final(const SDL_FPoint landmarks[33], int use_right, std::string& msg) {
    // 1. 提取基础点 (只用肩和踝)
    int shoulder_idx = use_right ? 12 : 11; 
    int ankle_idx    = use_right ? 28 : 27;

    SDL_FPoint shoulder = landmarks[shoulder_idx];
    SDL_FPoint ankle    = landmarks[ankle_idx];

    // 2. 计算身体主轴向量 (肩膀 -> 脚踝)
    double vx = ankle.x - shoulder.x;
    double vy = ankle.y - shoulder.y;

    // 3. 数据校验（防止关键点跑到外面去）
    double body_len = get_distance(shoulder, ankle);
	char buf[512];
    if (body_len < 0.05) {
		SDL_snprintf(buf, sizeof(buf), "body_len(%.3f) < 0.05", 
			body_len);
        return -1.0; // 身体没有完整检测到
    }

    // 4. 计算该向量与图像水平线(X轴)的夹角
    // 水平线的向量是 (1.0, 0.0)
    // 利用点积计算夹角：cos(θ) = (vx*1 + vy*0) / (|v| * |水平线|)
    double angle_rad = acos(vx / body_len); 
    double angle_deg = angle_rad * (180.0 / M_PI);

    // 5. 将角度校准到"偏离水平线的度数"
    // 如果是正侧面俯卧撑，肩膀和脚踝基本在同一水平线上，vx 趋近于 body_len，角度应该是 0度。
    // 如果画面中脚比肩膀高很多，vx 变小，角度会变大。
    // 因为 y 轴往下是正方向，所以如果脚比肩膀低，vy是负的，这会导致 acos 计算结果超过90度。
    // 为了统一度量，我们只关心它偏离水平线多少度。
    double horizontal_deviation = 0.0;
    if (angle_deg <= 90.0) {
        horizontal_deviation = angle_deg; // 脚比肩膀高
    } else {
        horizontal_deviation = 180.0 - angle_deg; // 脚比肩膀低
    }

    // 调试信息 (抛弃了之前的 arm_error 和 body_depth_error，不再混淆视听)
    SDL_snprintf(buf, sizeof(buf), 
        "body_len: %.3f, body_angle: %.3f deg, horizontal_deviation: %.3f deg",
        body_len, angle_deg, horizontal_deviation);
    msg = buf;

    return horizontal_deviation;
}

// ------------------------------------------------
// 整合调用示例
// ------------------------------------------------
std::string evaluate_pushup_setup(const SDL_FPoint* landmarks)
{
	std::string msg;
    // 图中人物的脸朝右，且右手靠近镜头，所以我们检测右手（use_right = 1）
    // double error = check_camera_parallelism_advanced(landmarks, 1, msg);
	double error = check_camera_parallelism_final(landmarks, 1, msg);


    if (error < 0) {
        // SDL_Log("错误：未能清晰检测到手臂关节，请确保手臂在画面中。");
        return msg;
    }

    // SDL_Log("当前手臂投影畸变误差: %.2f%%", error);

    // 阈值建议 (考虑到盲操，容错率较高)
    if (error <= 8.0) { 
		int ii = 0;
        // 8%以内的误差，相当于像素长度差不到10%，人眼很难察觉透视畸变
        // SDL_Log("【校准通过】视角完美，可以直接开始。");
    } 
    else if (error <= 20.0) {
		int ii = 0;
        // SDL_Log("【轻微偏差】视角有轻微透视。可以进入训练，但动作判定会有点严。");
    } 
    else {
		int ii = 0;
        // 说明上臂明显比前臂长很多，或者短很多，极大概率是相机没对正侧面
        // SDL_Log("【严重偏差】请将手机向正侧面移动，并确保镜头正对身体！当前镜头存在极大倾斜。");
    }
	return msg;
}

bool is_use_pi_div2(const SDL_DRange& range, bool avg_val)
{
	bool use_pi_div2 = false;
	if (is_float_nposm(range.min)) {
		VALIDATE(!is_float_nposm(range.max), null_str);
		if (avg_val) {
			VALIDATE(range.max >= 0.0, null_str);
			if (range.max <= 90 || range.max >= 270) {
				// both the minimum value is in the I or III quadrant.
				use_pi_div2 = true;
			}

		} else {
			if (range.max >= 270) {
				// both the minimum value is in the I or III quadrant.
				use_pi_div2 = true;
			}
		}

	} else if (is_float_nposm(range.max)) {
		VALIDATE(!is_float_nposm(range.min), null_str);
		if (avg_val) {
			VALIDATE(range.min >= 0.0, null_str);
			if (range.min <= 90 || range.min >= 270) {
				// both the minimum value is in the I or III quadrant.
				use_pi_div2 = true;
			}

		} else {
			if (range.min >= 270) {
				// both the minimum value is in the I or III quadrant.
				use_pi_div2 = true;
			}
		}

	} else {
		VALIDATE(!is_float_nposm(range.min) && !is_float_nposm(range.max), null_str);
		if (avg_val) {
			VALIDATE(range.min >= 0.0 && range.max >= 0.0, null_str);
			if (range.min <= 90 && range.max <= 90) {
				// both the minimum and maximum values are in the I quadrant.
				use_pi_div2 = true;

			} else if (range.min >= 270 && range.max >= 270) {
				// both the minimum and maximum values are in the III quadrant.
				use_pi_div2 = true;
			}

		} else {
			if (range.min >= 270 && range.max >= 270) {
				// both the minimum and maximum values are in the III quadrant.
				use_pi_div2 = true;
			}
		}
	}

	return use_pi_div2;
}

int twkoscript::analyze_track_pose(const twkoscript::tstate2& state2, const SDL_FPoint* landmarks33, int inference_ms,
	std::string& unsatisfied_msg, std::string& overlay_msg, twko_analyze_result_C* result_out)
{
	VALIDATE(state2.track_pose.valid(), null_str);
	const bool playback = inference_ms == nposm;

	unsatisfied_msg.clear();
	overlay_msg.clear();
	if (result_out != nullptr) {
		result_out->fields = 0;
	}
	for (int at = 0; at < state2.track_pose.landmark_count; at ++) {
		VALIDATE(at >= 0 && at < mediapipe::kNumPoseLandmarks, null_str);
		int l = state2.track_pose.landmarks[at];
		if (std::isnan(landmarks33[l].x)) {
			unsatisfied_msg = state2.track_pose.absent_landmark_msgstr(l);
			return workoutn32_unsatisfied_isnan;
		}
	}

	const int* desired = state2.track_pose.landmarks;
	const int absend_landmark = first_uncaptured_landmark(landmarks33, desired, state2.track_pose.landmark_count);
	if (absend_landmark != nposm) {
		unsatisfied_msg = state2.track_pose.absent_landmark_msgstr(absend_landmark);
		return workoutn32_unsatisfied_absend_landmark;
	}

	ttmp_result tmp_results[WKO_MAX_ANTI_SHAKE_FIELDS];
	int tmp_result_at = 0;

	tanti_shake_sample_C posture;
	tanti_shake_sample_C avg;
	posture.fields = 0;
	const std::vector<twkoscript::tpose>& poses = state2.track_pose.poses;
	int pose_at = 0;
	int phase_miss = 0;
	VALIDATE(state2.track_pose.curr_phase_ >= 0, null_str);
	const uint32_t curr_phase_mask = BIT_IDX_MASK(state2.track_pose.curr_phase_);
	for (std::vector<twkoscript::tpose>::const_iterator it = poses.begin(); it != poses.end(); ++ it, pose_at ++) {
		const twkoscript::tpose& pose = *it;
		if ((pose.phase_mask & curr_phase_mask) == 0) {
			phase_miss ++;
			continue;
		}
		ttmp_result& tmp_result = tmp_results[tmp_result_at];
		if (pose.type == twkoscript::posetype_angle3p) {
			SDL_DPoint points[3];
			pose.calc_point_operand(landmarks33, points);

			double rad = float_nposm;
			if (pose.ang_range == angrange_180) {
				rad = utils::imgcoor_calculate_angle_3SDL_Point_pi(points[0], points[1], points[2]);

			} else if (pose.ang_range == angrange_360) {
				rad = utils::imgcoor_calculate_angle_3SDL_Point_2pi(points[0], points[1], points[2]);

			} else {
				VALIDATE(false, null_str);
			}
			double degree = RAD2DEG(rad);

			set_tmp_result(pose, pose_at, degree, float_nposm, posture, avg, tmp_result);

		} else if (pose.type == twkoscript::posetype_angle2p) {
			SDL_DPoint points[3];
			pose.calc_point_operand(landmarks33, points);

			// double rad_pi_div2 = imgcoor_calculate_angle_2DPoint(points[0], points[1]);
			double rad_pi_div2 = utils::imgcoor_calculate_angle_2SDL_Point(points[0], points[1]);

			bool use_pi_div2 = false;
			if (!is_float_nposm(pose.range.min) && pose.range.min < 0) {
				use_pi_div2 = true;
			}
			if (!is_float_nposm(pose.range.max) && pose.range.max < 0) {
				use_pi_div2 = true;
			}

			if (!use_pi_div2 && !is_float_nposm(pose.range.min) && !is_float_nposm(pose.range.max)) {
				VALIDATE(pose.range.min >= 0.0 && pose.range.max >= 0.0, null_str);
				if (pose.range.min <= 90 && pose.range.max <= 90) {
					// both the minimum and maximum values are in the I quadrant.
					use_pi_div2 = true;

				} else if (pose.range.min >= 270 && pose.range.max >= 270) {
					// both the minimum and maximum values are in the III quadrant.
					use_pi_div2 = true;
				}
			}
			

			double degree = RAD2DEG(rad_pi_div2);
			if (!use_pi_div2) {
				// ==> in [0, 2*pi)
				double rad_0_2pi = angles::normalize_angle_positive(rad_pi_div2);
				degree = RAD2DEG(rad_0_2pi);
			}

			set_tmp_result(pose, pose_at, degree, float_nposm, posture, avg, tmp_result);
			memcpy(tmp_result.dbg_points, points, sizeof(points));

		} else if (pose.type == twkoscript::posetype_diff) {
			double vals[3];
			if (pose.operand_type == operandtype_point) {
				SDL_DPoint points[3];
				pose.calc_point_operand(landmarks33, points);
				vals[0] = hypot(points[0].x - points[1].x, points[0].y - points[1].y);
				vals[1] = float_nposm;

			} else {
				pose.calc_1d_operand(landmarks33, vals);
			}

			set_tmp_result(pose, pose_at, vals[0], vals[1], posture, avg, tmp_result);

		} else {
			VALIDATE(false, null_str);
		}
		tmp_result_at ++;
	}
	VALIDATE(tmp_result_at + phase_miss == (int)poses.size(), null_str);
	avg = average_sample(posture);
	if (playback) {
		memcpy(&avg, &posture, sizeof(avg));
	}

	overlay_msg_.vsize = 0;

	const int maxlen = 256;
	utils::resize_uint8data(overlay_msg_, maxlen, overlay_msg_.vsize);

	// std::stringstream ss;
	// ss << std::fixed << std::setprecision(3);
	// ss << inference_ms << " ms===" << script_.state_names[state2.state] << "===";
	const char dismatch_str[] = {"[X]"};
	char* msg_buf = (char*)overlay_msg_.ptr;
	int& msg_len = overlay_msg_.vsize;
	if (!playback) {
		msg_len = SDL_snprintf(msg_buf, maxlen, "(%s)%i ms===%s===phase: %i\n", 
			utils::format_time_ms(time(nullptr), false).c_str(), inference_ms, state_names[state2.state].c_str(),
			state2.track_pose.curr_phase_ + 1);

	} else {
		msg_len = SDL_snprintf(msg_buf, maxlen, "===%s===phase: %i\n", 
			state_names[state2.state].c_str(),
			state2.track_pose.curr_phase_ + 1);
	}

	struct tunsatisfied2
	{
		tunsatisfied2()
			: pose(nullptr)
			, at(nposm)
		{}

		void set(const twkoscript::tpose& _pose, int _at)
		{
			VALIDATE(pose == nullptr && at == nposm, null_str);
			VALIDATE(_at >= 0, null_str);
			pose = &_pose;
			at = _at;
		}
		const twkoscript::tpose* pose;
		int at;
	};
	tunsatisfied2 unsatisfied2;

	// const twkoscript::tpose* unsatisfied_pose = nullptr;
	bool is_unsatisfied_rmax = false;
	const int tmp_result_count = tmp_result_at;
	tmp_result_at = 0;
	int avg_d_at = 0;
	for (int at = 0; at < tmp_result_count; at ++) {
		const ttmp_result& tmp = tmp_results[at];
		double avg_val0 = *tmp.avg_sample0;
		double avg_val1 = tmp.avg_sample1 != nullptr? *tmp.avg_sample1: float_nposm;

		VALIDATE(KDL_Equal(avg_val0, avg.d[avg_d_at ++]), null_str);
		if (tmp.avg_sample1 != nullptr) {
			VALIDATE(KDL_Equal(avg_val1, avg.d[avg_d_at ++]), null_str);
		}

		utils::resize_uint8data(overlay_msg_, msg_len + maxlen, msg_len);
		msg_buf = (char*)overlay_msg_.ptr + msg_len;
		bool this_satisfied = true;
		double rt_result = float_nposm;

		// Why clone to a new variable? 
		// --To correctly compare angles, it is sometimes necessary to convert [0, 360) to [180, -180).
		SDL_DRange range2 = tmp.pose->range;

		bool val0_only = tmp.pose->type == posetype_angle3p || tmp.pose->type == posetype_angle2p ||
			(tmp.pose->type == posetype_diff && tmp.pose->operand_type == operandtype_point);
		if (val0_only) {
			if (tmp.pose->type == twkoscript::posetype_angle2p) {
				// There are three cases that need to be compared in the range [-180, 180):
				// 1. The (min, max) contains a negative number. Already in [-180, 180), no further conversion is needed.
				// 2. Both (min, max) are in the I quadrant. The values are unchanged in either range.
				// 3. Both (min, max) are in the III quadrant. Here, an additional conversion is required.
				if (!is_float_nposm(range2.min) && !is_float_nposm(range2.max)) {
					bool use_nagtive_degree = false;
					if (range2.min >= 270 && range2.max >= 270) {
						// both the minimum and maximum values are in the III quadrant.
						// change range from [0, 360)->[-180, 180)
						// for avg_val0, above 'use_pi_div2' make sure is in [-180, 180).
						SDL_DRange range_pi_div2_rad;
						range_pi_div2_rad.min = angles::normalize_angle(DEG2RAD(range2.min));
						range_pi_div2_rad.max = angles::normalize_angle(DEG2RAD(range2.max));

						range2.min = RAD2DEG(range_pi_div2_rad.min);
						range2.max = RAD2DEG(range_pi_div2_rad.max);
					}
				}

			} else if (tmp.pose->type == posetype_diff) {
				VALIDATE(tmp.pose->operand_type == operandtype_point, null_str);
			}
			if (!is_float_nposm(range2.min)) {
				// ss << "(>=)" << range2.min;
				if (avg_val0 < range2.min) {
					this_satisfied = false;
					if (unsatisfied2.pose == nullptr) {
						unsatisfied2.set(*tmp.pose, tmp.pose_at);
					}
				}
			}
			if (!is_float_nposm(range2.max)) {
				// ss << "(<=)" << range2.max;
				if (avg_val0 > range2.max) {
					this_satisfied = false;
					if (unsatisfied2.pose == nullptr) {
						unsatisfied2.set(*tmp.pose, tmp.pose_at);
						is_unsatisfied_rmax = !is_float_nposm(range2.min);
					}
				}
			}

			rt_result = tmp.val0;

		} else {
			VALIDATE(tmp.pose->type == twkoscript::posetype_diff, null_str);
			// ss << "\n" << tmp.pose->name << ": " << (tmp.val0 - tmp.val1);

			double result = avg_val0 - avg_val1;
			if (tmp.pose->abs) {
				result = fabs(result);
			}
			if (!is_float_nposm(range2.min)) {
				// ss << "(>=)" << range2.min;
				if (result < range2.min) {
					this_satisfied = false;
					if (unsatisfied2.pose == nullptr) {
						unsatisfied2.set(*tmp.pose, tmp.pose_at);
					}
				}
			}
			if (!is_float_nposm(range2.max)) {
				// ss << "(<=)" << range2.max;
				if (result > range2.max) {
					this_satisfied = false;
					if (unsatisfied2.pose == nullptr) {
						unsatisfied2.set(*tmp.pose, tmp.pose_at);
						is_unsatisfied_rmax = !is_float_nposm(range2.min);
					}
				}
			}

			rt_result = tmp.val0 - tmp.val1;
			if (tmp.pose->abs) {
				rt_result = fabs(rt_result);
			}
			// rt_result = result;

		}

		if (result_out != nullptr) {
			result_out->d[result_out->fields] = rt_result;
			result_out->satisfied[result_out->fields ++] = this_satisfied;
		}

		// generate overlay message
		VALIDATE(!is_float_nposm(rt_result), null_str);
		if (!this_satisfied) {
			msg_len += SDL_strlcpy(msg_buf, dismatch_str, sizeof(dismatch_str));
			msg_buf = (char*)overlay_msg_.ptr + msg_len;
		}
		if (!is_float_nposm(range2.min) && !is_float_nposm(range2.max)) {
			msg_len += SDL_snprintf(msg_buf, maxlen, "%s: %.3f([%.3f, %.3f])\n", 
				tmp.pose->name.c_str(), rt_result, range2.min, range2.max);

		} else if (!is_float_nposm(range2.min)) {
			msg_len += SDL_snprintf(msg_buf, maxlen, "%s: %.3f(>=)%.3f\n", 
				tmp.pose->name.c_str(), rt_result, range2.min);

		} else {
			VALIDATE(!is_float_nposm(range2.max), null_str);
			msg_len += SDL_snprintf(msg_buf, maxlen, "%s: %.3f(<=)%.3f\n", 
				tmp.pose->name.c_str(), rt_result, range2.max);
		}

		bool enable_dbg_points = tmp.pose->type == twkoscript::posetype_angle2p;
		enable_dbg_points = false;
		if (enable_dbg_points) {
			utils::resize_uint8data(overlay_msg_, msg_len + maxlen, msg_len);
			msg_buf = (char*)overlay_msg_.ptr + msg_len;
			msg_len += SDL_snprintf(msg_buf, maxlen, "dbg_points[0](%.3f, %.3f), [1][0](%.3f, %.3f), [2](%.3f, %.3f)\n", 
				tmp.dbg_points[0].x, tmp.dbg_points[0].y,
				tmp.dbg_points[1].x, tmp.dbg_points[1].y,
				tmp.dbg_points[2].x, tmp.dbg_points[2].y);
		}
	}
	// VALIDATE(tmp_result_at == posture.fields, null_str);
	if (unsatisfied2.pose != nullptr) {
		unsatisfied_msg = unsatisfied2.pose->unsatisfied_msgstr;
		if (is_unsatisfied_rmax && !unsatisfied2.pose->unsatisfied_rmax_msgstr.empty()) {
			unsatisfied_msg = unsatisfied2.pose->unsatisfied_rmax_msgstr;
		}
	}

	bool satisfied = unsatisfied2.pose == nullptr;

	utils::resize_uint8data(overlay_msg_, msg_len + maxlen, msg_len);
	msg_buf = (char*)overlay_msg_.ptr + msg_len;
	msg_len += SDL_snprintf(msg_buf, maxlen, "%s", satisfied? satisfied_msgstr_.c_str(): unsatisfied_msgstr_.c_str());
	// ss << "\n" << (satisfied? "satisfied": "fail");
	overlay_msg.assign((char*)overlay_msg_.ptr, msg_len);

	// std::string setup_msg = evaluate_pushup_setup(landmarks33);
	// overlay_msg.append("\n" + setup_msg);

	return satisfied? workoutn32_satisfied_landmarks: (workoutn32_unsatisfied_pose_min + unsatisfied2.at);
}

void twkoscript::reset_anti_shike_samples(int fields)
{
	// SDL_Log("%u ---reset_anti_shike_samples---", SDL_GetTicks());
	VALIDATE(fields >= 0 && fields <= WKO_MAX_ANTI_SHAKE_FIELDS, null_str);

	next_sample_index_ = 0;
	memset(anti_shake_samples_, 0, sizeof(anti_shake_samples_));
	if (fields != 0) {
		for (int at = 0; at < MAX_ANTI_SHAKE_SAMPLES; at ++) {
			anti_shake_samples_[at].fields = fields;
		}
	}

	last_recv_sample_ticks_ = 0;
	recv_samples_ = 0;
}

static float calculate_one_avg(const tanti_shake_sample_C* samples, int max_line_samples, int fidx)
{
	// Remove the maximum and minimum values, and take the average of the middle ones.
	const tanti_shake_sample_C& line0 = samples[0];
	float min_val = line0.d[fidx];
	float max_val = line0.d[fidx];
	int min_at = 0;
	int max_at = 0;
	for (int at = 1; at < max_line_samples; at ++) {
		const tanti_shake_sample_C& line = samples[at];

		if (line.d[fidx] < min_val) {
			min_val = line.d[fidx];
			min_at = at;

		} else if (line.d[fidx] > max_val) {
			max_val = line.d[fidx];
			max_at = at;
		}
	}
	float sum = 0;
	for (int at = 0; at < max_line_samples; at ++) {
		if (at == min_at || at == max_at) {
			continue;
		}
		const tanti_shake_sample_C& line = samples[at];
		sum += line.d[fidx];
	}

	return sum / (max_line_samples - 2);
}

static float remove_two_farthest_iterative(const tanti_shake_sample_C* samples, int max_samples, int fidx, float* temp)
{   
	VALIDATE(max_samples > 2, null_str);

    for (int i = 0; i < max_samples; i++) {
		const tanti_shake_sample_C& sample = samples[i];
		temp[i] = sample.d[fidx];
	}
    int size = max_samples;
    
	float sum = 0.0f;
    // Remove the two farthest points
    for (int remove = 0; remove < 2; remove ++) {
        // Calculate the average
        sum = 0.0f;
        for (int i = 0; i < size; i++) {
			sum += temp[i];
		}
        float mean = sum / size;
        
        // Find the farthest
        int farthest_idx = 0;
        float max_dist = fabs(temp[0] - mean);
        for (int i = 1; i < size; i++) {
            float dist = fabs(temp[i] - mean);
            if (dist > max_dist) {
                max_dist = dist;
                farthest_idx = i;
            }
        }
        
        // Delete this element
        for (int i = farthest_idx; i < size - 1; i++) {
            temp[i] = temp[i + 1];
        }
        size --;
    }
    
    // copy result
	sum = 0.0;
	for (int i = 0; i < size; i++) {
		sum += temp[i];
	}

    float mean = sum / size;
    return mean;
}

tanti_shake_sample_C twkoscript::average_sample(const tanti_shake_sample_C& newly)
{
	const int fields = anti_shake_samples_[0].fields;
	VALIDATE(fields > 0 && fields <= WKO_MAX_ANTI_SHAKE_FIELDS, null_str);
	VALIDATE(newly.fields == fields, null_str);

	//
	// step1: add newly_sample to samples.
	//
	if (last_recv_sample_ticks_ != 0 && (int)(SDL_GetTicks() - last_recv_sample_ticks_) > reset_samples_threshold_ms_) {
		reset_anti_shike_samples(fields);
	}

	const bool verbose = false;

	tanti_shake_sample_C* samples = anti_shake_samples_;
	const int max_line_samples = MAX_ANTI_SHAKE_SAMPLES;
	int& next_sample_index = next_sample_index_;

	tanti_shake_sample_C& cur_line_sample = samples[next_sample_index];
	cur_line_sample = newly;

	if (verbose) {
		SDL_Log("-----{%s} next_sample_index: %i---", 
			"vel", next_sample_index);
		for (int at = 0; at < max_line_samples; at ++) {
			const tanti_shake_sample_C& line = samples[at];
			VALIDATE(line.fields == fields, null_str);
			// SDL_Log("[%i/%i]nose_x: %.3f, shoulder_mid: %.3f, shoulder_width: %.3f, ear_diff: %.3f, head_forward: %.3f", at, max_line_samples, 
			//	line.d[fidx_nose_x], line.d[fidx_shoulder_mid_x], line.d[fidx_shoulder_width], line.d[fidx_ear_diff_y], line.d[fidx_head_forward]);
		}
		SDL_Log("--------");
	}

	next_sample_index ++;
	next_sample_index %= max_line_samples;

	last_recv_sample_ticks_ = SDL_GetTicks();
	recv_samples_ ++;

	//
	// step2: calculate average sample.
	//
	if (recv_samples_ < MAX_ANTI_SHAKE_SAMPLES) {
		return newly;
	}

	tanti_shake_sample_C result;
	result.fields = fields;
	for (int at = 0; at < fields; at ++) {
		// result.d[at] = calculate_one_avg(samples, MAX_ANTI_SHAKE_SAMPLES, at);
		result.d[at] = remove_two_farthest_iterative(samples, MAX_ANTI_SHAKE_SAMPLES, at, temp_4_average_);
	}
	return result;
}

std::map<int, tcode3> pose_sides;
std::map<int, tcode3> pose_metrics;

int pose_side_from_str(const std::string& str)
{
	VALIDATE(!pose_sides.empty(), null_str);
	for (std::map<int, tcode3>::const_iterator it = pose_sides.begin(); it != pose_sides.end(); ++ it) {
		const std::string& that = it->second.id;
		if (that == str) {
			return it->first;
		}
	}
	return nposm;
}

int pose_metric_from_str(const std::string& str)
{
	VALIDATE(!pose_metrics.empty(), null_str);
	for (std::map<int, tcode3>::const_iterator it = pose_metrics.begin(); it != pose_metrics.end(); ++ it) {
		const std::string& that = it->second.id;
		if (that == str) {
			return it->first;
		}
	}
	return nposm;
}

void tpreset_pose::did_from_cfg_quited(const std::string& err_msg)
{
	if (!err_msg.empty()) {
		clear();
	}
}

bool tpreset_pose::from_cfg(const config& cfg)
{
	clear();

	std::string err_msg;
	tauto_destruct_executor destruct_executor(std::bind(&tpreset_pose::did_from_cfg_quited, this, std::ref(err_msg)));

	bool retbool = tpose::from_cfg(cfg);
	if (!retbool) {
		err_msg = "invalid file in tpose";
		return false;
	}
	metric = pose_metric_from_str(cfg["metric"].str());
	if (metric == nposm) {
		err_msg = "invalid metric";
		return false;
	}
	side = pose_side_from_str(cfg["side"].str());
	if (side == nposm) {
		err_msg = "invalid side";
		return false;
	}

	tolerance = cfg["tolerance"].to_double(float_nposm);
	if (!is_float_nposm(tolerance) && tolerance < 0) {
		err_msg = "invalid tolerance";
		return false;
	}
	return true;
}

void tpreset_pose::to_cfg(config& cfg) const
{
	VALIDATE(valid(), null_str);

	tpose::to_cfg(cfg);

	cfg["side"] = pose_sides.find(side)->second.id;
	cfg["metric"] = pose_metrics.find(metric)->second.id;
	cfg["tolerance"].from_double(tolerance);
}


const std::string wko_new_dir_prefix = "__new_";

bool did_walk_wkoscript(const std::string& dir, const SDL_dirent2* dirent, int type, const std::set<std::string>& ext_names, 
	std::set<std::string>& result_set, const std::string& root)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		std::string name = utils::lowercase(dirent->name);
		if (type == type_wkoscript_ids || type == type_wkoscript_cfgfiles) {
			std::string ext_name = utils::file_ext_name(name);
			if (ext_names.count(ext_name) != 0) {
				if (type == type_wkoscript_ids) {
					std::string stem_name = name.substr(0, name.size() - 4);
					result_set.insert(stem_name);

				} else if (type == type_wkoscript_cfgfiles) {
					result_set.insert(dirent->name);
				}
			}
		} else if (type == type_wkoscript_new_benchmarks) {
			std::string ext_name = utils::file_ext_name(name);
			if (ext_names.count(ext_name) != 0) {
				std::string stem_name = name.substr(0, name.size() - 4);
				std::vector<std::string> v_str = utils::split(stem_name, '_');
				if (v_str.size() != 2 || v_str[1].size() != 1) {
					return true;
				}
				// 12_0.png
				const char* c_str = v_str[0].c_str();
				int s = v_str[0].size();

				for (int at = 0; at < s; at ++) {
					char ch = c_str[at];
					if (ch < '0' || ch > '9') {
						return true;
					}
				}

				c_str = v_str[1].c_str();
				if (c_str[0] != '0' && c_str[0] != '1') {
					return true;
				}
				result_set.insert(dirent->name);
			}
		}
	} else {
		std::string name = utils::lowercase(dirent->name);
		if (type == type_wkoscript_new_dirs) {
			if (name.size() > wko_new_dir_prefix.size() && name.find(wko_new_dir_prefix) == 0) {
				result_set.insert(dirent->name);
			}
		}
	}
	return true;
}

std::string wkoscript_extract_id(const std::string& cfg_str)
{
	// std::vector<std::string> ids;
	std::string id;
    const std::string keyword = "id";
    const std::string equalSign = "=";
    const std::string quote = "\"";
    
    size_t pos = 0;
    while (true) {
        // find "id"
        size_t idPos = cfg_str.find(keyword, pos);
        if (idPos == std::string::npos) {
			break;
		}
        
        // find "="
        size_t eqPos = cfg_str.find(equalSign, idPos + keyword.length());
        if (eqPos == std::string::npos) {
			break;
		}
        
        // find first quote
        size_t quote1 = cfg_str.find(quote, eqPos + 1);
        if (quote1 == std::string::npos) break;
        
        // find second quote
        size_t quote2 = cfg_str.find(quote, quote1 + 1);
        if (quote2 == std::string::npos) break;
        
        // extra 'id' value
        id = cfg_str.substr(quote1 + 1, quote2 - quote1 - 1);
		break;
        // ids.push_back(id);
        
        // pos = quote2 + 1;
    }
    
    return id;
}

void list_wkoscript_files_by_type(const std::string& wkoscript_dir2, int type, std::set<std::string>& result_set)
{
	result_set.clear();

	std::set<std::string> ext_names;
	if (type == type_wkoscript_ids || type == type_wkoscript_cfgfiles) {
		ext_names.insert("cfg");

	} else if (type == type_wkoscript_new_benchmarks) {
		ext_names.insert("png");
		ext_names.insert("jpg");
	}
	walk_dir(wkoscript_dir2, false, std::bind(&did_walk_wkoscript, _1, _2, type, std::ref(ext_names), std::ref(result_set), std::ref(wkoscript_dir2)));
}

}