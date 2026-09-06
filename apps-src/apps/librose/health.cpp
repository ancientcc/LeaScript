/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
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
#define GETTEXT_DOMAIN "rose-lib"

#include "health.hpp"
#include "rose_config.hpp"
#include "rose_var.hpp"
#include "gettext.hpp"
#include "wkoscript.hpp"

using namespace std::placeholders;

extern void collect_health_files(const std::string& saves_health_dir, int max_days, bool valid, std::set<std::string>& filenames);

// #include <time.h>   // clock_gettime
// #include <stdio.h>


int64_t SDL_GetTimeMs(void)
{
#if defined(_WIN32)
    FILETIME ft;
    ULARGE_INTEGER uli;
    
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    
    // Convert to Unix epoch milliseconds
    const uint64_t UNIX_EPOCH_DIFFERENCE = 116444736000000000ULL;
    return (int64_t)((uli.QuadPart - UNIX_EPOCH_DIFFERENCE) / 10000);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
#endif
}

namespace aplt {

twko_tlv_history_C history_add(const twko_tlv_history_C& last_history, int64_t start_of_lastday, 
	SDL_Range last_range_ms, const twkoscript& this_script, int64_t start_of_thisday)
{
	twko_tlv_history_C new_history = {0};
	init_wko_tlv_history(new_history);

	if (wko_tlv_history_is_valid(last_history)) {
		new_history = last_history;
	}
	new_history.start_of_lastday = start_of_lastday;
	new_history.last_range_ms = last_range_ms;
	if (start_of_lastday != start_of_thisday) {
		VALIDATE(start_of_lastday < start_of_thisday, null_str);
		new_history.days += 1;
	}
	new_history.workouts += 1;

	this_script.history_after_one_finish(new_history.last_range_ms, new_history.reps, new_history.duration_s);

	return new_history;
}

twko_tlv_history_C thealth::tworkout_result2::history_add_me(int64_t start_of_meday, const twkoscript& me_script, int64_t start_of_thisday) const
{
	twko_tlv_history_C new_history = {0};
	init_wko_tlv_history(new_history);

	if (wko_tlv_history_is_valid(history)) {
		new_history = history;
	}
	VALIDATE(start_of_meday == start_of_today, null_str);
	new_history.start_of_lastday = start_of_meday;
	new_history.last_range_ms = range_ms;
	if (start_of_meday != start_of_thisday) {
		VALIDATE(start_of_meday < start_of_thisday, null_str);
		new_history.days += 1;
	}
	new_history.workouts += 1;

	me_script.history_after_one_finish(new_history.last_range_ms, new_history.reps, new_history.duration_s);

	twko_tlv_history_C new_history2 = history_add(history, start_of_meday, range_ms, me_script, start_of_thisday);
	VALIDATE(memcmp(&new_history, &new_history2, sizeof(new_history2)) == 0, null_str);

	return new_history;
}

tuint8cdata_C thealth::thealth_result2::find_zip_workout(int start_s) const
{
	const uint8_t* data = zip_workouts.data;
	int vsize = zip_workouts.vsize;
	int pos = 0;
	int min_one_size = 9; // 4 + 4 + 1

	tuint8cdata_C result;
	memset(&result, 0, sizeof(result));

	int this_segment_size;
	int this_start_s;
	while (pos < vsize) {
		if (vsize - pos < min_one_size) {
			return result; 
		}
		// [(4bytes)segment_size] [(4bytes)start_s] [(segment_size-4bytes)zip data] ...
		memcpy(&this_segment_size, data + pos, 4);
		pos += 4;
		if (vsize - pos < this_segment_size) {
			return result;
		}
		memcpy(&this_start_s, data + pos, 4);
		if (this_start_s == start_s) {
			result.ptr = data + pos + 4;
			result.len = this_segment_size - 4;
			break;
		}
		pos += this_segment_size;
	}

	return result;
}

std::string thealth::thealth_result2::to_msg_sit_period() const
{
	std::stringstream ss;
	ss << utils::format_elapse_hm2(sit_period.min);
	ss << "-";
	ss << utils::format_elapse_hm2(sit_period.max);
	return ss.str();
}

std::string thealth::thealth_result2::to_msg_sit_duration() const
{
	int duration2 = 0;
	for (int at = 0; at < ONE_DAY_HOURS; at ++) {
		duration2 += sit_durations[at];
	}
/*
	if (duration2 >= 3600) {
		duration2 = posix_align_ceil2(duration2 + 59, 60);
	}
	std::string s = utils::format_elapse_hms(duration2);
*/
	std::string s = utils::format_elapse_hm_or_ms(duration2, true, utils::timesep_i18n);
	std::string from = utils::format_elapse_hm2(sit_period.min);
	std::string to = utils::format_elapse_hm2(sit_period.max);

	char buf[64];
	// SDL_snprintf(buf, sizeof(buf), "%s(%s - %s)", s.c_str(), from.c_str(), to.c_str());
	SDL_snprintf(buf, sizeof(buf), "%s", s.c_str());
	return buf;
}

std::string thealth::thealth_result2::to_msg_alert_count(bool improper) const
{
	int count = sedentary_alert_count;
	if (improper) {
		count = improper_alert_count();
	}

	utils::string_map symbols;
	symbols["count"] = str_cast(count);
	return vgettext2("$count times", symbols);
}

std::string thealth::thealth_result2::to_msg_workouts() const
{
	utils::string_map symbols;
	symbols["count"] = str_cast(workouts.size());

	std::stringstream result_ss; 
	result_ss << vgettext2("$count times", symbols);

	if (!workouts.empty()) {
		int total_ms = 0;
		for (std::vector<tworkout_result2>::const_iterator it = workouts.begin(); it != workouts.end(); ++ it) {
			const tworkout_result2& workout = *it;
			total_ms += workout.range_ms.max - workout.range_ms.min;
		}
		result_ss << "(" << utils::format_elapse_hms(total_ms / 1000) << ")";
	}
	return result_ss.str();
}

int thealth::thealth_result2::calc_total(int type) const
{
	const int* array = nullptr;
	if (type == type_sit_duration) {
		array = sit_durations;

	} else if (type == type_improper_duration) {
		array = improper_durations;

	} else if (type == type_improper_alert) {
		array = improper_alerts;

	} else {
		VALIDATE(false, null_str);
	}

	int total = 0;
	const int count = sizeof(sit_durations) / sizeof(sit_durations[0]);
	for (int at = 0; at < count; at ++) {
		total += array[at];
	}
	return total;
}

int thealth::thealth_result2::calc_workout_total(int type) const
{
	int total = 0;

	for (std::vector<tworkout_result2>::const_iterator it = workouts.begin();  it != workouts.end(); ++ it) {
		const tworkout_result2& result2 = *it;
		if (type == type_workout_duration) {
			total += result2.range_ms.max - result2.range_ms.min;

		} else if (type == type_workout_times) {
			total ++;

		} else {
			VALIDATE(false, null_str);
		}
	}

	return total;
}

std::string thealth::thealth_result2::to_msg_workout_max_range() const
{
	SDL_Range result{nposm, nposm};

	int max_duration = 0;
	for (std::vector<tworkout_result2>::const_iterator it = workouts.begin();  it != workouts.end(); ++ it) {
		const tworkout_result2& result2 = *it;
		int duration = result2.range_ms.max - result2.range_ms.min;
		if (duration > max_duration) {
			max_duration = duration;
			result = result2.range_ms;
		}
	}

	if (result.min == nposm) {
		return null_str;
	}

	result.min /= 1000;
	result.max /= 1000;

	std::stringstream ss;
	int hour_min = (result.min / 3600) % 24;
	int hour_max = (result.max / 3600) % 24;
	// if (hour_min != hour_max) {
	if (true) {
		ss << utils::format_elapse_hms2(result.min);
		ss << "-";
		ss << utils::format_elapse_hms2(result.max);

	} else {
		// result.min -= hour_min * 3600;
		result.max -= hour_max * 3600;
		ss << utils::format_elapse_hms2(result.min, false);
		ss << "-";
		ss << utils::format_elapse_ms2(result.max, false);
	}
	return ss.str();
}

#pragma pack(1)
	struct thealth_old_header
	{
		uint32_t fourcc;
		uint32_t version;
		uint32_t n32_events;
		uint32_t workout_cfg_strs;
		uint32_t zip_workouts;
		uint32_t unzip_workout;
		uint32_t reserve0;
		int64_t start_of_today;
	};

#pragma pack()

bool thealth::did_load_old_header_health_dat(tfile& file, int64_t dsize, thealth_result& result) const
{
	VALIDATE(result.start_of_today == 0, null_str);
	if (dsize < sizeof(thealth_old_header)) {
		return false;
	}
	thealth_old_header header;
	posix_fread(file.fp, &header, sizeof(header));
	if (header.fourcc != SDL_FOURCC('H', 'L', 'D', 'T')) {
		return false;
	}
	if (header.version != HEALTH_DAT_VER) {
		return false;
	}

	if (header.start_of_today <= 0) {
		return false;
	}

	if (dsize != (int)(sizeof(header) + header.n32_events * result.event_items.elem_sz) +
		header.workout_cfg_strs + header.zip_workouts + header.unzip_workout + header.reserve0) {
		return false;
	}

	posix_fseek(file.fp, sizeof(header));

	struct titem_C
	{
		telem_array_C* item;
		int vsize;
	};

	titem_C items[] = {{&result.event_items, (int)header.n32_events},
		{&result.workout_cfg_strs, (int)header.workout_cfg_strs},
		{&result.zip_workouts, (int)header.zip_workouts},
		{&result.unzip_workout, (int)header.unzip_workout},
	};
	const int NUM = sizeof(items) / sizeof(items[0]);
	for (int at = 0; at < NUM; at ++) {
		titem_C& item = items[at];
		if (item.vsize != 0) {
			item.item->resize_data(item.vsize, 0);
			posix_fread(file.fp, item.item->data, item.vsize * item.item->elem_sz);
		}
		item.item->vsize = item.vsize;
	}

	result.start_of_today = header.start_of_today;

	return true;
}

void thealth::upgrade_health_file(const std::string& filename)
{
	thealth_result result;
	result.clear();

	{
		// 1.1 load, use old thealth_header2
		tsha1reader reader(filename, true, std::bind(&thealth::did_load_old_header_health_dat, this, _1, _2, std::ref(result)));
		reader.read();

		if (result.start_of_today > 0) {
			result.filename = filename;
		}
	
		VALIDATE(result.start_of_today > 0, null_str);
	}

	h_.start_of_today = result.start_of_today;
	h_.event_items.assign_data(result.event_items);
	h_.workout_cfg_strs.assign_data(result.workout_cfg_strs);
	h_.zip_workouts.assign_data(result.zip_workouts);
	h_.unzip_workout.assign_data(result.unzip_workout);

	// 1.2 save, use new thealth_header
	int backup_type = nposm;
	tsha1writer writer(filename, backup_type, std::bind(&thealth::did_write_health_dat, this, _1, std::ref(h_)));
	writer.write();
}

thealth::thealth()
	: health_dir_(game_config::preferences_dir + "/saves/health")
	, max_save_days_(30)
	, max_parse_workouts_(20)
	, backup_type_(backup_on_fixed_delay)
	, good_mask_ms_(1000)  // 1second
	, next_good_valid_ticks_(0)
	, improper_mask_ms_(1000)  // 1second
	, next_improper_valid_ticks_(0)
	, landmarks_mask_ms_(1000)  // 5second(if this workout's final state is notrack_pose, this is effict)
	, next_satisfied_landmarks_valid_ticks_(0)
	, next_unsatisfied_landmarks_valid_ticks_(0)
	, next_can_write_health_dat_ticks_(0)
	, health_dat_dirty_(false)
	, write_file_helper_(sizeof(uint8_t))
{
/*
	{
		int max_days = max_save_days_;
		std::set<std::string> filenames;
		collect_health_files(health_dir_, max_days, true, filenames);

		for (std::set<std::string>::const_iterator it = filenames.begin(); it != filenames.end(); ++ it) {
			const std::string& file = *it;
			// SDL_DeleteFiles(file.c_str());
			upgrade_health_file(file);
		}

		// upgrade_health_file(health_dir_ + "/health-test.dat");
		int ii = 0;
	}

	VALIDATE(false, null_str);
*/

	VALIDATE(max_save_days_ > 0, null_str);
	const time_t t = time(nullptr);

	thealth_result result;
	load_health_dat(t, result);

	int64_t today_0h0m0s = utils::calculate_0h0m0s_ts(t);
	if (result.valid() && result.start_of_today == today_0h0m0s) {
		h_.start_of_today = result.start_of_today;

		h_.event_items.assign_data(result.event_items);
		h_.workout_cfg_strs.assign_data(result.workout_cfg_strs);
		h_.zip_workouts.assign_data(result.zip_workouts);
		h_.unzip_workout.assign_data(result.unzip_workout);
		h_.workout_tlvs.assign_data(result.workout_tlvs);

		move_unzip_workout_to_zip_today();

		get_existed_workout_cfg_strs();
	}
}

thealth::~thealth()
{
	// if (game_config::os == os_windows) {
		if (health_dat_dirty_) {
			write_health_dat();
		}
	// }
}

const std::string thealth::health_dat_filename(time_t t) const
{
	const struct tm* timeptr = localtime(&t);
	if (timeptr == nullptr) {
		// if t is too lareg, for error with ms unit, timeptr maybe nullptr.
		return null_str;
	}
	char buf[512];
	SDL_snprintf(buf, sizeof(buf), "%s/health%04d%02d%02d.dat", health_dir_.c_str(), 1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday);
	return buf;
}

void thealth::slice()
{
	if (health_dat_dirty_ && SDL_GetTicks() >= next_can_write_health_dat_ticks_) {
		write_health_dat();
	}
}

void thealth::did_start_or_stop_base_subtask(bool start)
{
	if (!start) {
		// In start_subtask_internal(), between the start and the call to this function, 
		// some this workout's events have already been sent, such as workoutn32_wkoscript_index_min.
		move_unzip_workout_to_zip_today();
	}
}

void thealth::evaluate_start_of_today()
{
	VALIDATE(h_.start_of_today == 0, null_str);

	VALIDATE(h_.event_items.vsize == 0, null_str);
	VALIDATE(h_.zip_workouts.vsize == 0, null_str);
	VALIDATE(h_.unzip_workout.vsize == 0, null_str);
	h_.start_of_today = utils::calculate_0h0m0s_ts(time(nullptr));
}

int thealth::health_push_n32_event(int type, int ctx)
{
	VALIDATE(type >= 0 && type < healthevt_count, null_str);

	bool write_immediately = true;

	bool is_may_mask = false;
	uint32_t now = SDL_GetTicks();
	if (type == sitevttype_good) {
		is_may_mask = ctx == sitgood_new_one;
		if (is_may_mask) {
			if (now < next_good_valid_ticks_) {
				return nposm;
			}
			next_good_valid_ticks_ = now + good_mask_ms_;
		}

	} else if (type == sitevttype_improper) {
		VALIDATE(ctx != 0, null_str);
		is_may_mask = ctx < sitimproper_not_reason_min;
		if (is_may_mask) {
			if (now < next_improper_valid_ticks_) {
				return nposm;
			}
			next_improper_valid_ticks_ = now + improper_mask_ms_;
		}

	} else if (type == workoutevt_n32) {
		is_may_mask = wkon32_ctx_is_analyze_track_pose_result(ctx);
		if (is_may_mask) {
			uint32_t& next_landmarks_valid_ticks = 
				ctx == workoutn32_satisfied_landmarks? next_satisfied_landmarks_valid_ticks_: next_unsatisfied_landmarks_valid_ticks_;
			if (now < next_landmarks_valid_ticks) {
				// If more information is needed during the playback debugging phase, 
				// all posture detection events should be written to health.dat. 
				// For now, every 'workoutevt_n32' event will be written; 
				// if the file becomes too large in the future, it can be modified then.

				// return nposm;
			}
			next_landmarks_valid_ticks = now + landmarks_mask_ms_;

			// SDL_Log("%u ctx: 0x%x(workoutn32_satisfied_landmarks: 0x%x), next_satisfied_landmarks_valid_ticks_: %u, next_unsatisfied_landmarks_valid_ticks_: %u",
			//	now, ctx, workoutn32_satisfied_landmarks, next_satisfied_landmarks_valid_ticks_, next_unsatisfied_landmarks_valid_ticks_);
		} else if (ctx <= workoutn32_max_states) {
			// for 'enter_state' must write immediately.
			VALIDATE(!is_may_mask && write_immediately, null_str);
			health_push_n32_to_unzip_workout(wkotype_enter_state, ctx);

		} else if (ctx <= workoutn32_wkoscript_index_max) {
			// after 'workoutn32_wokscript_index',  will immediately receive a 'enter_state' afterwards
			is_may_mask = true;
		}
	}
	if (is_may_mask) {
		write_immediately = false;
	}
	// SDL_Log("%u {dbg-health}health_push_n32_event(type: %i[good: %i, improper: %i, workoutevt_n32: %i], ctx: 0x%x)", 
	//	SDL_GetTicks(), type, sitevttype_good, sitevttype_improper, workoutevt_n32, ctx);

	if (type < sitevttype_count) {
		new_day_if_necessary();
	}

	if (h_.start_of_today == 0) {
		evaluate_start_of_today();
	}

	tevent_item* result = (tevent_item*)h_.event_items.append_1();
	int64_t t = SDL_GetTimeMs();
	int ms_since0 = t - h_.start_of_today * 1000;
	result->ms_since0 = ms_since0;
	result->type = type;
	result->ctx = ctx;

	set_health_dirty(write_immediately);

	return ms_since0 / 1000;
}

void thealth::verify_workout_cfg_strs() const
{
	uint8_t* data = h_.workout_cfg_strs.data;
	int pos = 0;
	int wkoscript_index = 0;
	while (pos < h_.workout_cfg_strs.vsize) {
		VALIDATE(pos + sizeof(int) < h_.workout_cfg_strs.vsize, null_str);
		int size = 0;
		memcpy(&size, data + pos, sizeof(int));
		pos += sizeof(int);
		VALIDATE(pos + size <= h_.workout_cfg_strs.vsize, null_str);

		std::map<std::string, int>::const_iterator it = existed_workout_cfg_strs_.find(std::string((const char*)data + pos, size));
		VALIDATE(it != existed_workout_cfg_strs_.end(), null_str);
		VALIDATE(it->second == wkoscript_index, null_str);

		wkoscript_index ++;
		pos += size;
	}
	VALIDATE(pos == h_.workout_cfg_strs.vsize, null_str);
}

void thealth::workout_cfg_strs_to_map(const telem_array_C& cfg_strs, std::map<std::string, int>& result) const
{
	result.clear();

	uint8_t* data = cfg_strs.data;
	int pos = 0;
	while (pos < cfg_strs.vsize) {
		VALIDATE(pos + sizeof(int) < cfg_strs.vsize, null_str);
		int size = 0;
		memcpy(&size, data + pos, sizeof(int));
		pos += sizeof(int);
		VALIDATE(pos + size <= cfg_strs.vsize, null_str);

		const std::string that_str((const char*)data + pos, size);
		VALIDATE(result.count(that_str) == 0, null_str);
		result.insert(std::make_pair(that_str, result.size()));

		pos += size;
	}
	VALIDATE(pos == cfg_strs.vsize, null_str);
}

void thealth::workout_cfg_strs_to_map2(const telem_array_C& cfg_strs, std::map<int, std::string>& result) const
{
	result.clear();

	std::map<std::string, int> tmp;
	workout_cfg_strs_to_map(cfg_strs, tmp);
	for (std::map<std::string, int>::const_iterator it = tmp.begin(); it != tmp.end(); ++ it) {
		result.insert(std::make_pair(it->second, it->first));
	}
}

void thealth::get_existed_workout_cfg_strs()
{
	VALIDATE(existed_workout_cfg_strs_.empty(), null_str);
	workout_cfg_strs_to_map(h_.workout_cfg_strs, existed_workout_cfg_strs_);
}


void thealth::workout_tlvs_to_historys(const telem_array_C& workout_tlvs, std::map<int, twko_tlv_history_C>& historys) const
{
	historys.clear();

	const uint8_t* ptr = workout_tlvs.data;
	int remaining = workout_tlvs.vsize;

	twko_tlv_header_C header;
	while (remaining >= sizeof(header)) { // Ensure that both T and L can be read.
		memcpy(&header, ptr, sizeof(header)); // Read 8 bytes at once.
		// ptr += sizeof(header);
		remaining -= sizeof(header);

        // 1. Basic check: must not go out of bounds.
        if (header.length > remaining) {
			VALIDATE(false, null_str);
            break; // File corrupted.
        }
    
		// Check data validity (using history's own length field).
		if (header.type == wko_tlv_type_history) {
			VALIDATE(header.length == WKO_TLV_HISTORY_LEN, null_str);
			
			const twko_tlv_history_C* hist = (twko_tlv_history_C*)ptr;
			VALIDATE(historys.count(hist->seconds_since0) == 0, null_str);
			historys.insert(std::make_pair(hist->seconds_since0, *hist));
		}
    
		ptr += sizeof(header) + header.length;
        remaining -= header.length;
	}
	VALIDATE(remaining == 0, null_str);
}

void thealth::new_day_if_necessary()
{
	if (h_.start_of_today > 0) {
		int64_t start_of_today = utils::calculate_0h0m0s_ts(time(nullptr));
		if (start_of_today != h_.start_of_today) {
			SDL_Log("start_of_today(%s) != h_.start_of_today(%s), it is new day, h_.unzip_workout.vsize: %i",
				utils::format_time_ymdhms3(start_of_today).c_str(), utils::format_time_ymdhms3(h_.start_of_today).c_str(), 
				h_.unzip_workout.vsize);

			// If use the kdesktop app on the first day, don't use it on the second day, 
			// and then use it again on the third day, the interval is 48 hours, not 24.
			// VALIDATE(start_of_today - h_.start_of_today == ONE_DAY_SECONDS, null_str);

			move_unzip_workout_to_zip_today();

			h_.clear();
			existed_workout_cfg_strs_.clear();
		}
	}
}

void thealth::health_push_str_event(int type, int ctx, const std::string& str, const std::string& aux_str)
{
	VALIDATE(!str.empty(), null_str);
	
	bool dirty = false;
	if (type == workoutevt_str && ctx == workoutstr_start) {
		// it is first event of this workout.
		// std::string id = aplt::wkoscript_extract_id(str);
		const std::string& id = aux_str;
		VALIDATE(!id.empty(), null_str);

		new_day_if_necessary();

		std::map<std::string, int>::iterator it = existed_workout_cfg_strs_.find(str);
		if (it == existed_workout_cfg_strs_.end()) {
			std::pair<std::map<std::string, int>::iterator, bool> ins = existed_workout_cfg_strs_.insert(std::make_pair(str, existed_workout_cfg_strs_.size()));
			it = ins.first;
		
			const int str_size = str.size();
			h_.workout_cfg_strs.resize_data(h_.workout_cfg_strs.vsize + sizeof(int) + str_size, h_.workout_cfg_strs.vsize);
			memcpy(h_.workout_cfg_strs.data + h_.workout_cfg_strs.vsize, &str_size, sizeof(int));
			h_.workout_cfg_strs.vsize += sizeof(int);
			memcpy(h_.workout_cfg_strs.data + h_.workout_cfg_strs.vsize, str.c_str(), str_size);
			h_.workout_cfg_strs.vsize += str_size;

			verify_workout_cfg_strs();

			dirty = true;
		}

		int wkoscript_index = it->second;
		int seconds_since0 = health_push_n32_event(workoutevt_n32, workoutn32_wkoscript_index_min + wkoscript_index);
		VALIDATE(seconds_since0 != nposm, null_str);
		push_wko_tlv_history(h_.start_of_today, seconds_since0, id, h_);

		move_unzip_workout_to_zip_today();
		health_push_n32_to_unzip_workout(wkotype_start, seconds_since0);
	}

	if (!dirty) {
		return;
	}

	if (h_.start_of_today == 0) {
		evaluate_start_of_today();
	}

	bool write_immediately = false;
	set_health_dirty(write_immediately);
}

void thealth::health_push_landmarks(const SDL_U16Point* landmarks, int unsatisfied_reason)
{
	VALIDATE(wkon32_ctx_is_analyze_track_pose_result(unsatisfied_reason), null_str);

	health_push_n32_event(workoutevt_n32, unsatisfied_reason);

	if (h_.start_of_today == 0) {
		evaluate_start_of_today();
	}

	h_.unzip_workout.resize_data(h_.unzip_workout.vsize + 3 * mediapipe::kNumPoseLandmarks, h_.unzip_workout.vsize);
	int original_vsize = h_.unzip_workout.vsize;
	uint8_t& count = h_.unzip_workout.data[h_.unzip_workout.vsize ++];
	// 'count' maybe dirty, isn't 0.
	count = 0;
	for (int at = 0; at < mediapipe::kNumPoseLandmarks; at ++) {
		const SDL_U16Point& src = landmarks[at];
		if (src.x != rose_u16_nan) {
			h_.unzip_workout.data[h_.unzip_workout.vsize ++] = (uint8_t)at;
			memcpy(h_.unzip_workout.data + h_.unzip_workout.vsize, &src, sizeof(SDL_U16Point));
			h_.unzip_workout.vsize += sizeof(SDL_U16Point);
			count ++;
		}
	}
	VALIDATE(original_vsize + 1 + (1 + sizeof(SDL_U16Point)) * count == h_.unzip_workout.vsize, null_str);

	bool write_immediately = false;
	set_health_dirty(write_immediately);
}

void thealth::health_push_n32_to_unzip_workout(uint8_t type, int ctx)
{
	VALIDATE(type >= 0 && type < wkotype_count, null_str);

	if (type == wkotype_start) {
		// unzip_workout data must begin with 'wkotype_start'
		VALIDATE(h_.unzip_workout.vsize == 0, null_str);
	}

	int original_vsize = h_.unzip_workout.vsize;
	int& pos = h_.unzip_workout.vsize;
	h_.unzip_workout.resize_data(h_.unzip_workout.vsize + 6, h_.unzip_workout.vsize);
	h_.unzip_workout.data[pos ++] = WORKOUT_TYPE_PREFIX;
	h_.unzip_workout.data[pos ++ ] = type;
	memcpy(h_.unzip_workout.data + pos, &ctx, 4);
	pos += 4;

	VALIDATE(original_vsize + 6 == h_.unzip_workout.vsize, null_str);
}

// if @start_s is nposm, find last twko_tlv_history.
twko_tlv_history_C thealth::find_wko_history(int start_s) const
{
	const uint8_t* ptr = h_.workout_tlvs.data;
	int remaining = h_.workout_tlvs.vsize;

	twko_tlv_history_C result = {0};
	result.seconds_since0 = nposm;

	twko_tlv_header_C header;
	while (remaining >= sizeof(header)) { // Ensure that both T and L can be read.
		memcpy(&header, ptr, sizeof(header)); // Read 8 bytes at once.
		// ptr += sizeof(header);
		remaining -= sizeof(header);

        // 1. Basic check: must not go out of bounds.
        if (header.length > remaining) {
			VALIDATE(false, null_str);
            break; // File corrupted.
        }
    
		// Check data validity (using history's own length field).
		if (header.type == wko_tlv_type_history) {
			VALIDATE(header.length == WKO_TLV_HISTORY_LEN, null_str);
			
			const twko_tlv_history_C* hist = (twko_tlv_history_C*)ptr;
			memcpy(&result, hist, sizeof(twko_tlv_history_C));
			if (start_s == hist->seconds_since0) {
				return result;
			}
		}
    
		ptr += sizeof(header) + header.length;
        remaining -= header.length;
	}
	VALIDATE(remaining == 0, null_str);

	if (start_s != nposm) {
		result.seconds_since0 = nposm;
	}
	return result;
}

bool is_same_workout_type(const std::string& id1, const std::string& id2) 
{
    if (id1.empty() || id2.empty()) {
        return false;
    }

    // 2. Find the position of the second underscore "_".
    size_t pos1 = id1.find('_');
    if (pos1 == std::string::npos) {
		return false; // At least there is no separator between the first and second segments.
	}
    pos1 = id1.find('_', pos1 + 1);
    
    size_t pos2 = id2.find('_');
    if (pos2 == std::string::npos) {
		return false;
	}
    pos2 = id2.find('_', pos2 + 1);

    // 3. Calculate the effective length of the first two segments.
    // If there is no third segment (the second underscore is not found), the length extends to the end of the string.
    size_t len1 = (pos1 != std::string::npos) ? pos1 : id1.length();
    size_t len2 = (pos2 != std::string::npos) ? pos2 : id2.length();

    // 4. If the lengths of the first two segments are different, they are certainly not the same exercise.
    if (len1 != len2) {
        return false;
    }

    // 5. Directly use low-level memory comparison (fastest).
    return memcmp(id1.c_str(), id2.data(), len1) == 0;
}

twko_tlv_history_C thealth::get_wko_tlv_history(int64_t start_of_today, int wko_seconds_since0, 
	const std::string& wkoscript_id) const
{
	twko_tlv_history_C history = {0};
	init_wko_tlv_history(history);

	int max_days = WKO_MAX_CHECKIN_GAP_DAYS;
	aplt::thealth::thealth_result2 result2;
	twkoscript script;
	const aplt::thealth::tworkout_result2* hit_workout_result2 = nullptr;
	for (int day = -1; day < max_days; day ++) {		
		load_health_data_4_report(start_of_today - (day + 1) * ONE_DAY_SECONDS, result2);
		bool retbool = result2.valid();

		const int64_t desire_start_of_this_day = start_of_today - (day + 1) * ONE_DAY_SECONDS;
		if (retbool) {
			retbool = result2.start_of_today == desire_start_of_this_day;
			if (!retbool) {
				result2.clear();
			}
		}

		if (!retbool) {
			continue;
		}

		// int workout_at = 0;
		for (std::vector<aplt::thealth::tworkout_result2>::const_reverse_iterator rit = result2.workouts.rbegin(); rit != result2.workouts.rend(); ++ rit /*workout_at ++*/) {
			const aplt::thealth::tworkout_result2& workout_result2 = *rit;
			if (day == -1) {
				// today
				if (workout_result2.start_s >= wko_seconds_since0) {
					continue;
				}
			}
			if (result2.workout_cfgs.count(workout_result2.wkoscript_index) != 0) {
				const std::string& wkoscript_cfg_str = result2.workout_cfgs.find(workout_result2.wkoscript_index)->second;
				std::string id = aplt::wkoscript_extract_id(wkoscript_cfg_str);

				if (!is_same_workout_type(id, wkoscript_id)) {
					continue;
				}
				// Check if it has finished.
				config wkoscript_cfg;
				aplt::read_config_ex(wkoscript_cfg_str, true, wkoscript_cfg);
				VALIDATE(!wkoscript_cfg.empty(), null_str);

				bool ret = script.from_cfg(wkoscript_cfg);
				if (!ret) {
					// May fail. The old format is too outdated, and the new format no longer supports certain syntaxes. 
					// For example, angle2p now requires both 'min' and 'max' to be set.
					continue;
				}

				if (script.states.size() != workout_result2.flow_states2.size()) {
					continue;
				}

				hit_workout_result2 = &workout_result2;
				break;
				
			} else {
				VALIDATE(false, null_str);
			}
		}
		if (hit_workout_result2 == nullptr) {
			continue;
		}
		break;
	}

	if (hit_workout_result2 != nullptr) {
		history = hit_workout_result2->history_add_me(result2.start_of_today, script, start_of_today);
	}
	VALIDATE(history.type == wko_tlv_type_history, null_str);
	VALIDATE(history.length == WKO_TLV_HISTORY_LEN, null_str);

	history.seconds_since0 = wko_seconds_since0;

	return history;
}

void thealth::push_wko_tlv_history(int64_t start_of_today, int wko_seconds_since0, 
	const std::string& wkoscript_id, thealth_result& h) const
{
	twko_tlv_history_C history = get_wko_tlv_history(start_of_today, wko_seconds_since0, wkoscript_id);
	h.workout_tlvs.put_size(&history, sizeof(twko_tlv_history_C));
}

bool thealth::migrate_health_dat_for_history(const std::string& filename) const
{
	bool is_bak = false;
	int64_t t = start_of_file_day_from_filename(utils::extract_file(filename), &is_bak);
	if (t == nposm || is_bak) {
		return false;
	}

	aplt::thealth::thealth_result result;
	load_health_dat(t, result);
	if (!result.valid()) {
		return false;
	}

	if (result.workout_tlvs.vsize != 0) {
		result.workout_tlvs.clear();
	}

	thealth_result2 result2;
	if (!health_result_to_result2(result, result2)) {
		return false;
	}

	std::map<int, twko_tlv_history_C> historys;
	
	int col_count = 0;
	for (std::vector<tworkout_result2>::iterator it = result2.workouts.begin(); it != result2.workouts.end(); ++ it) {
		tworkout_result2& workout = *it;
		VALIDATE(workout.start_of_today == result2.start_of_today, null_str);
		VALIDATE(!wko_tlv_history_is_valid(workout.history), null_str);

		VALIDATE(result2.workout_cfgs.count(workout.wkoscript_index) != 0, null_str);

		const std::string& wkoscript_cfg_str = result2.workout_cfgs.find(workout.wkoscript_index)->second;
		std::string id = aplt::wkoscript_extract_id(wkoscript_cfg_str);
		if (id != "leagor_pushup") {
			// continue;
		}
		push_wko_tlv_history(result2.start_of_today, workout.start_s, id, result);

		// Allow later workout on the same day to use it.
		int backup_type = nposm;
		tsha1writer writer(filename, backup_type, std::bind(&thealth::did_write_health_dat, this, _1, std::ref(result)));
		writer.write();
	}

	return true;
}

void thealth::set_health_dirty(bool write_immediately)
{
	health_dat_dirty_ = true;
	if (write_immediately || SDL_GetTicks() >= next_can_write_health_dat_ticks_) {
		write_health_dat();
	}
}

void thealth::write_file_helper_put(const uint8_t* data, int len) const
{
	VALIDATE(data != nullptr && len > 0, null_str);

	telem_array_C& helper = write_file_helper_;
	helper.resize_data(helper.vsize + len, helper.vsize);
	memcpy(helper.data + write_file_helper_.vsize, data, len);
	write_file_helper_.vsize += len;
}

bool thealth::did_write_health_dat(tfile& file, const thealth_result& result) const
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(result.start_of_today > 0, null_str);

	// threading::lock lock(file_mutex_);

	thealth_header header;
	memset(&header, 0, sizeof(thealth_header));
	header.fourcc = SDL_FOURCC('H', 'L', 'D', 'T');
	header.version = HEALTH_DAT_VER;
	header.n32_events = result.event_items.vsize;
	header.workout_cfg_strs = result.workout_cfg_strs.vsize;
	header.zip_workouts = result.zip_workouts.vsize;
	header.unzip_workout = result.unzip_workout.vsize;
	header.workout_tlvs = result.workout_tlvs.vsize;
	header.start_of_today = result.start_of_today;

	int total_size = sizeof(header);
	const telem_array_C* items[] = {&result.event_items, &result.workout_cfg_strs, 
		&result.zip_workouts, &result.unzip_workout, &result.workout_tlvs,
	};
	const int NUM = sizeof(items) / sizeof(items[0]);
	for (int at = 0; at < NUM; at ++) {
		const telem_array_C& item = *items[at];
		if (item.vsize != 0) {
			total_size += item.vsize * item.elem_sz;
		}
	}

	const bool one_write = total_size <= 2 * CONSTANT_1M;
	// do wirte...
	if (one_write) {
		write_file_helper_.clear();
		write_file_helper_put((uint8_t*)&header, sizeof(header));
	} else {
		posix_fwrite(file.fp, &header, sizeof(header));
	}

	for (int at = 0; at < NUM; at ++) {
		const telem_array_C& item = *items[at];
		if (item.vsize != 0) {
			if (one_write) {
				write_file_helper_put(item.data, item.vsize * item.elem_sz);
			} else {
				posix_fwrite(file.fp, item.data, item.vsize * item.elem_sz);
			}
		}
	}

	if (one_write) {
		if (write_file_helper_.vsize != 0) {
			posix_fwrite(file.fp, write_file_helper_.data, write_file_helper_.vsize * write_file_helper_.elem_sz);
		}
		VALIDATE(total_size == write_file_helper_.vsize, null_str);
	}

	return true;
}

void thealth::write_health_dat()
{
	VALIDATE(health_dat_dirty_, null_str);
	VALIDATE(h_.start_of_today > 0, null_str);
/*
	tsha1writer writer(health_dat_filename(time(nullptr)), backup_type_, std::bind(&thealth::did_write_health_dat, this, _1, std::ref(h_)));
*/
	tsha1writer writer(health_dat_filename(h_.start_of_today), backup_type_, std::bind(&thealth::did_write_health_dat, this, _1, std::ref(h_)));

	writer.write();

	const int write_threshold_ms = 10000;
	next_can_write_health_dat_ticks_ = SDL_GetTicks() + write_threshold_ms;

	health_dat_dirty_ = false;
	// SDL_Log("%u, {dbg-health}write_health_dat, event_items_.vsize: %i", SDL_GetTicks(), h_.event_items.vsize);
}

bool thealth::did_load_health_dat(tfile& file, int64_t dsize, thealth_result& result) const
{
	VALIDATE(result.start_of_today == 0, null_str);
	if (dsize < sizeof(thealth_header)) {
		return false;
	}
	thealth_header header;
	posix_fread(file.fp, &header, sizeof(header));
	if (header.fourcc != SDL_FOURCC('H', 'L', 'D', 'T')) {
		return false;
	}
	if (header.version != HEALTH_DAT_VER) {
		return false;
	}

	if (header.start_of_today <= 0) {
		return false;
	}

	if (dsize != (int)(sizeof(header) + header.n32_events * result.event_items.elem_sz) +
		header.workout_cfg_strs + header.zip_workouts + header.unzip_workout + header.workout_tlvs + 
		header.reserve0 + header.reserve1 + header.reserve2) {
		return false;
	}

	posix_fseek(file.fp, sizeof(header));

	struct titem_C
	{
		telem_array_C* item;
		int vsize;
	};

	titem_C items[] = {{&result.event_items, (int)header.n32_events},
		{&result.workout_cfg_strs, (int)header.workout_cfg_strs},
		{&result.zip_workouts, (int)header.zip_workouts},
		{&result.unzip_workout, (int)header.unzip_workout},
		{&result.workout_tlvs, (int)header.workout_tlvs},
	};
	const int NUM = sizeof(items) / sizeof(items[0]);
	for (int at = 0; at < NUM; at ++) {
		titem_C& item = items[at];
		if (item.vsize != 0) {
			item.item->resize_data(item.vsize, 0);
			posix_fread(file.fp, item.item->data, item.vsize * item.item->elem_sz);
		}
		item.item->vsize = item.vsize;
	}

	result.start_of_today = header.start_of_today;

	return true;
}

void thealth::load_health_dat(time_t t, thealth_result& result) const
{
	VALIDATE_IN_MAIN_THREAD();
/*
	bool_set_t load_again = bool_set_none;
	do {
		if (load_again == bool_set_true) {
			// An exception may occur, making it impossible to rewrite the source file.
			// use 'load_again', at most, rewrite once.
			load_again = bool_set_false;
		}
*/
		const std::string filename = health_dat_filename(t);
		{
			result.clear();
			// make sure after 'reader.read', this file is closed.
			tsha1reader reader(filename, true, std::bind(&thealth::did_load_health_dat, this, _1, _2, std::ref(result)));
			reader.read();
		}

		if (result.start_of_today > 0) {
			result.filename = filename;

			// if (result.unzip_workout.vsize != 0 && load_again == bool_set_none) {
			if (result.unzip_workout.vsize != 0) {
				time_t today_t = time(nullptr);
				int64_t today_0h0m0s = utils::calculate_0h0m0s_ts(t);

				if (result.start_of_today != today_0h0m0s) {
					move_unzip_workout_to_zip(result);
					// 'move_unzip_workout_to_zip' will move the data from 'unzip_workout' to 'zip_workout', 
					// so reloading is unnecessary.
					
					// load_again = bool_set_true;
				}
			}
		}
	// } while (load_again == bool_set_true);
}

void thealth::did_move_unzip_workout_to_zip_quited(telem_array_C& unzip_workout) const
{
	// VALIDATE(unzip_workout.vsize != 0, null_str);
	unzip_workout.clear();
}

bool thealth::move_unzip_workout_to_zip(thealth_result& result) const
{
	tauto_destruct_executor destruct_executor(std::bind(&thealth::did_move_unzip_workout_to_zip_quited, this, 
		std::ref(result.unzip_workout)));

	// 10: 10 seconds, 15: min landmarks per frame
	int min_landmarks = 10 * 15 * 1000 / MEDIAPIPE_POSTURE_MIN_INTERVAL;
	const int min_size = 4 + 4 + 2 * WORKOUT_TYPE_BYTES + sizeof(SDL_U16Point) * min_landmarks;
	if (result.unzip_workout.vsize < min_size) {
		return false;
	}

	uint8_t* data = result.unzip_workout.data;
	int vsize = result.unzip_workout.vsize;
	int pos = 0;

	if (data[pos ++] != WORKOUT_TYPE_PREFIX) {
		return false;
	}
	int type = data[pos ++];
	if (type != wkotype_start) {
		return false;
	}
	int seconds_since0 = 0;
	memcpy(&seconds_since0, data + pos, 4);
	pos += 4;

	telem_array_C& to = result.zip_workouts;
	const int original_to_vsize = to.vsize;
	to.vsize += 4;

	to.put_size(&seconds_since0, 4);
	// write_file(game_config::preferences_dir + "/1.dat", data, vsize);
	int compressed_size = zlib::compress(data, vsize, to, Z_BEST_COMPRESSION);
	VALIDATE(to.vsize == original_to_vsize + 8 + compressed_size, null_str);

	int segment_size = 4 + compressed_size;
	memcpy(to.data + original_to_vsize, &segment_size, 4);

	// The subsequent set_health_dirty(...) will write to a file, 
	// which will use 'result.unzip_workout'. 
	// It cannot wait for the destruct_executor's destructor to set it, 
	// so the correct value must be assigned here, i.e., vsize = 0.
	result.unzip_workout.clear();

	if (&result == &h_) {
		// bool write_immediately = true;
		// set_health_dirty(write_immediately);

	} else {
		std::string filename = health_dat_filename(result.start_of_today);
		tsha1writer writer(filename, backup_type_, std::bind(&thealth::did_write_health_dat, this, _1, std::ref(result)));
		writer.write();
	}
	return true;
}

void thealth::move_unzip_workout_to_zip_today()
{
	if (h_.unzip_workout.vsize != 0) {
		// Regardless of whether 'move_unzip_workout_to_zip' succeeds or fails, 
		// unzip_workout has be cleared.
		if (move_unzip_workout_to_zip(h_)) {
			bool write_immediately = true;
			set_health_dirty(write_immediately);
		}
	}
}

void thealth::pre_health_data_4_report(const thealth_result& result, std::vector<tpre_workout>& pre_workouts) const
{
	pre_workouts.clear();
	tpre_workout* curr_pre_workout = nullptr;

	int curr_flow_state = nposm;

	std::map<int, tpre_workout> workout_split_state;
	const aplt::thealth::tevent_item* items = (aplt::thealth::tevent_item*)(result.event_items.data);
	for (int at = 0; at < result.event_items.vsize; at ++) {
		const aplt::thealth::tevent_item& item = items[at];
		const int item_seconds_since0 = item.ms_since0 / 1000;
		const int this_hour = item_seconds_since0 / 3600;
		if (this_hour < 0 || this_hour >= 24) {
			// Has the file been tampered with?
			// VALIDATE(false, null_str);
			
			// logic must same as 'load_health_data_4_report'
			break;
			// continue;
		}

		if (item.type == workoutevt_n32) {
			bool is_wkoscript_index = item.ctx >= workoutn32_wkoscript_index_min && item.ctx <= workoutn32_wkoscript_index_max;
			if (is_wkoscript_index) {
				// start
				pre_workouts.push_back(tpre_workout());
				curr_pre_workout = &pre_workouts.back();
				curr_pre_workout->start_s = item_seconds_since0;
				curr_flow_state = nposm;

			} else if (item.ctx < workoutn32_max_states) {
				// enter state
				if (curr_pre_workout != nullptr && item.ctx >= 0) {
					if (curr_flow_state != nposm) {
						curr_pre_workout->range_ms.max = item.ms_since0;
						// if (curr_flow_state < MAX_CONSIDE_SPLIT_STATES) {
							curr_pre_workout->flow_states[curr_flow_state].end_ms = item.ms_since0;
						// }
					}
					curr_flow_state ++;
					curr_pre_workout->flow_state_count = curr_flow_state + 1;
					// if (curr_flow_state < MAX_CONSIDE_SPLIT_STATES) {
						tpre_flow_state_C& new_state = curr_pre_workout->flow_states[curr_flow_state];
						new_state.state = item.ctx;
						new_state.start_ms = item.ms_since0;
					// }

					// landmarks_t0 = nposm;
					// curr_subphase.clear();
				}

			} else if (item.ctx >= workoutn32_state_subphase_min && item.ctx <= workoutn32_state_subphase_max) {
				// if (curr_pre_workout != nullptr && curr_flow_state != nposm && curr_flow_state < MAX_CONSIDE_SPLIT_STATES) {
				if (curr_pre_workout != nullptr && curr_flow_state != nposm) {
					tpre_flow_state_C& state = curr_pre_workout->flow_states[curr_flow_state];
					state.is_rep_counter = true;

					trepetition_C& rep = state.rep;
					if (item.ctx == workoutn32_cooldown_period_start) {
						// rep.cooldown_start_ms[rep.phase_count] = item.ms_since0;
						// curr_subphase.set(state.rep_count, rep.phase_count, rep_subp_cooldown);

						// rep.phase_count ++;
						if (state.rep.rep_complete_ms != nposm) {
							state.rep_count ++;

							rep.rep_complete_ms = nposm;
						}
					}
				}

			} else if (item.ctx == workoutn32_rep_complete) {
				// if (curr_pre_workout != nullptr && curr_flow_state != nposm && curr_flow_state < MAX_CONSIDE_SPLIT_STATES) {
				if (curr_pre_workout != nullptr && curr_flow_state != nposm) {
					tpre_flow_state_C& state = curr_pre_workout->flow_states[curr_flow_state];
					state.is_rep_counter = true;

					trepetition_C& rep = state.rep;
					rep.rep_complete_ms = item.ms_since0;
				}

			} else if (item.ctx == workoutn32_first_satisfied) {
				if (curr_pre_workout != nullptr && curr_flow_state != nposm) {
					curr_pre_workout->range_ms.min = item.ms_since0;

					// if (curr_flow_state < MAX_CONSIDE_SPLIT_STATES) {
						tpre_flow_state_C& state = curr_pre_workout->flow_states[curr_flow_state];
						state.is_pose_state = true;
					// }
				}

			} else if (wkon32_ctx_is_analyze_track_pose_result(item.ctx)) {
				if (curr_pre_workout != nullptr && curr_flow_state != nposm) {
					curr_pre_workout->range_ms.max = item.ms_since0;

					// if (curr_flow_state < MAX_CONSIDE_SPLIT_STATES) {
						tpre_flow_state_C& state = curr_pre_workout->flow_states[curr_flow_state];
						state.is_pose_state = true;
					// }
/*
					if (landmarks_t0 != nposm) {
						const int delta = item_seconds_since0 - landmarks_t0;
						if (delta <= landmarks_continuous_threshold) {
							int& duration = item.ctx == workoutn32_satisfied_landmarks? state.satisfied_duration: state.unsatisfied_duration;
							duration += delta;
						}
					}
					landmarks_t0 = item_seconds_since0;

					if (wkon32_ctx_is_unsatisfied_reason(item.ctx) && curr_subphase.in_active()) {
						trepetition_C& rep = state.reps[curr_subphase.rep_at];
						int& to_index = rep.unsatisfied_reason_count[curr_subphase.phase_at];
						if (to_index < WKO_MAX_UNSATISFIED_REASON_PER_REP) {
							rep.unsatisfied_reasons[curr_subphase.phase_at][to_index ++] = item.ctx;
						}
					}
*/
				}
			} 
		} // if (item.type == workoutevt_n32)
	} // for (..., at < result.event_items.vsize, ...)

	for (int at = 0; at < pre_workouts.size(); at ++) {
		tpre_workout& workout = pre_workouts[at];
		// VALIDATE(workout.split_count == 0, null_str);

		// VALIDATE(workout.flow_state_count >= 1, null_str);
		// VALIDATE(workout.range.max != nposm, null_str);
		if (workout.flow_state_count == 0) {
			continue;
		}

		tpre_flow_state_C& last_state = workout.flow_states[workout.flow_state_count - 1];
		if (last_state.end_ms < workout.range_ms.max) {
			last_state.end_ms = workout.range_ms.max;
		}

		int col_count = 0;
		int min_inter_ms = 8000; // 8 seconds
		std::map<int, int> may_divisor_states;
		for (int state_at = 0; state_at < workout.flow_state_count; state_at ++) {
			tpre_flow_state_C& state = workout.flow_states[state_at];
			VALIDATE(state.split.flow_state_at == nposm, null_str);

			col_count ++;
			if (state.rep_count != 0) {
				col_count += state.rep_count;
			}
			if (state.is_pose_state && !state.is_rep_counter) {
				col_count ++;
				if (workout.flow_state_count <= MAX_CONSIDE_SPLIT_STATES) {
					int duration_ms = state.end_ms - state.start_ms;
					if (duration_ms >= (int)(min_inter_ms * 1.5)) {
						may_divisor_states.insert(std::make_pair(state_at, duration_ms));
					}
				}
			}
		}

		const int desire_min_col_count = 17;
		// while (col_count < desire_min_col_count && !may_divisor_states.empty() && workout.split_count < MAX_SPLIT_COUNT) {
		while (col_count < desire_min_col_count && !may_divisor_states.empty()) {
			SDL_Point max_duration{nposm, nposm};
			for (std::map<int, int>::const_iterator it = may_divisor_states.begin(); it != may_divisor_states.end(); ++ it) {
				int duration = it->second;
				if (duration > max_duration.y) {
					max_duration.x = it->first;
					max_duration.y = duration;
				}
			}
			tpre_flow_state_C& state = workout.flow_states[max_duration.x];

			int adden_cols = desire_min_col_count - col_count;
			if (adden_cols == 1) {
				// Ensure split.
				adden_cols = 2;
			}
			int inter_ms = max_duration.y / adden_cols;
			if (inter_ms < min_inter_ms) {
				inter_ms = min_inter_ms;
				adden_cols = max_duration.y / inter_ms;
				if (adden_cols == 1) {
					// adden_cols changed, ensure split.
					adden_cols = 2;
				}
			}
			inter_ms = max_duration.y / adden_cols;

			// tpre_split_C& split = workout.splits[workout.split_count ++];
			tpre_split_C& split = state.split;
			VALIDATE(split.flow_state_at == nposm, null_str);
			split.flow_state_at = max_duration.x;
			split.interval_ms = inter_ms + 1; // ms
			split.segs = adden_cols;

			VALIDATE(split.interval_ms * adden_cols >= max_duration.y, null_str);
/*
			int remainder = max_duration.y % inter_ms;
			if (remainder >= min_inter_ms * 3 / 4) {
				adden_cols ++;
			}
*/
			col_count += adden_cols;
			may_divisor_states.erase(may_divisor_states.find(max_duration.x));
		}

		for (int state_at = 0; state_at < workout.flow_state_count; state_at ++) {
			tpre_flow_state_C& state = workout.flow_states[state_at];
			if (state.is_pose_state && !state.is_rep_counter && state.split.flow_state_at == nposm) {
				int duration_ms = state.end_ms - state.start_ms;
				tpre_split_C& split = state.split;
				split.flow_state_at = state_at;
				split.interval_ms = duration_ms + 1; // ms
				split.segs = 1;
			}
		}
	}
}

bool thealth::load_health_data_4_report(time_t t, thealth_result2& result2) const
{
	VALIDATE_IN_MAIN_THREAD();
	result2.clear();

	thealth_result result;
	load_health_dat(t, result);
	if (!result.valid()) {
		return false;
	}

	return health_result_to_result2(result, result2);
}

bool thealth::health_result_to_result2(const thealth_result& result, thealth_result2& result2) const
{
	VALIDATE(result.valid(), null_str);
	result2.clear();

	enum {rep_subp_active, rep_subp_cooldown, rep_subp_count};
	struct tsubphase
	{
		tsubphase()
		{
			clear();
		}

		bool in_active() const { return subp_at == rep_subp_active; }

		void set(int _rep_at, int _phase_at, int _subp_at)
		{
			VALIDATE(_rep_at >= 0 && _phase_at >= 0 && _subp_at >= 0, null_str);
			if (_subp_at == rep_subp_active) {
				if (rep_at == nposm) {
					VALIDATE(phase_at == nposm && subp_at == nposm, null_str);
				} else {
					VALIDATE(phase_at != nposm && subp_at == rep_subp_cooldown, null_str);
				}
			} else {
				VALIDATE(rep_at != nposm && phase_at != nposm && subp_at == rep_subp_active, null_str);
			}

			rep_at = _rep_at;
			phase_at = _phase_at;
			subp_at = _subp_at;
		}

		void clear()
		{
			rep_at = nposm;
			phase_at = nposm;
			subp_at = nposm;
		}

		int rep_at;
		int phase_at;
		int subp_at;
	};


	result2.start_of_today = result.start_of_today;

	const int improper_continuous_threshold = 3; // 3 second
	int type_impropers_t0[aplt::sitimproper_reason_count];

	const int sit_continuous_threshold = 3; // 3 second
	int sit_t0;
	int improper_t0;
	int t1;
	int curr_hour = nposm;
	int curr_workout_start = nposm;
	int landmarks_t0 = nposm;
	const int landmarks_continuous_threshold = 2000; // 2 second
	const aplt::thealth::tevent_item* items = (aplt::thealth::tevent_item*)(result.event_items.data);
	std::vector<tpre_workout> pre_workouts;
	pre_health_data_4_report(result, pre_workouts);

	tworkout_result2* curr_workout = nullptr;
	tpre_workout* curr_pre_workout = nullptr;
	const tpre_split_C* curr_split = nullptr;
	int curr_flow_state = nposm;
	aplt::tflow_state_C* curr_flow_state_ptr = nullptr;
	tsubphase curr_subphase;
	// SDL_Log("{dbg-health}----result.event_items.vsize: %i-----", result.event_items.vsize);
	for (int at = 0; at < result.event_items.vsize; at ++) {
		const aplt::thealth::tevent_item& item = items[at];
		const int item_seconds_since0 = item.ms_since0 / 1000;
		const int this_hour = item_seconds_since0 / 3600;
		// SDL_Log("{dbg-health}%i/%i, this_hour: %i, item.type: %i, item.ms_since0: %i, item.ctx: %i,", 
		//	at, result.event_items.vsize, this_hour, (int)item.type, item.ms_since0, item.ctx);

		if (this_hour < 0 || this_hour >= 24) {
			// VALIDATE(false, null_str);
			
			// It is possible for 'this_hour >= 24' to occur, but the subsequent logic cannot be executed,
			// because everything that follows assumes 23 is the maximum ¡ª for example, 'result2.improper_alerts'. 
			// Whether to display '>= 24' will be considered in the future.
			break;
			// continue;
		}
		const int this_seconds = item_seconds_since0 % 3600;
		if (this_hour != curr_hour) {
			for (int n = 0; n < aplt::sitimproper_reason_count; n ++) {
				type_impropers_t0[n] = -1 * (improper_continuous_threshold + 1);
			}

			sit_t0 = -1 * (sit_continuous_threshold + 1);
			improper_t0 = -1 * (improper_continuous_threshold + 1);
		}
		t1 = this_seconds;
		curr_hour = this_hour;
		bool calc_sit_duration = false;

		// const bool enable_log = this_hour == 10;
		const bool enable_log = false;
		if (enable_log) {
			SDL_Log("[10]t: %i, type: %i, ctx: %i", this_seconds, item.type, item.ctx);
		}
		if (item.type == aplt::sitevttype_improper) {
			if (item.ctx == sitimproper_alert) {
				result2.improper_alerts[curr_hour] ++;
				// SDL_Log("improper_alerts[%i] t1: %i, alerts: %i", 
				//	curr_hour, t1, result2.improper_alerts[curr_hour]);

			} else {
				for (int n = 0; n < aplt::sitimproper_reason_count; n ++) {
					if (item.ctx & BIT_IDX_MASK(n)) {
						// duration of special type improper.
						const int type_improper_delta = t1 - type_impropers_t0[n];
						if (type_improper_delta <= improper_continuous_threshold) {
							if (enable_log) {
								SDL_Log("{type_improper(%i)}, %i + %i -> %i", n, result2.type_improper_durations[n][curr_hour], 
									type_improper_delta, result2.type_improper_durations[n][curr_hour] + type_improper_delta);
							}
							result2.type_improper_durations[n][curr_hour] += type_improper_delta;
						}
						type_impropers_t0[n] = t1;
					}
				}
				// total duration of improper.
				const int improper_delta = t1 - improper_t0;
				if (improper_delta <= improper_continuous_threshold) {
					if (enable_log) {
						SDL_Log("{improper}, %i + %i -> %i", result2.improper_durations[curr_hour], 
							improper_delta, result2.improper_durations[curr_hour] + improper_delta);
					}
					result2.improper_durations[curr_hour] += improper_delta;
				}
				improper_t0 = t1;

				calc_sit_duration = true;
				if (result2.sit_period.min == 0) {
					result2.sit_period.min = item_seconds_since0;
				}
				result2.sit_period.max = item_seconds_since0;
			}

		} else if (item.type == sitevttype_good) {
			if (item.ctx == sitgood_new_one) {
				calc_sit_duration = true;

				if (result2.sit_period.min == 0) {
					result2.sit_period.min = item_seconds_since0;
				}
				result2.sit_period.max = item_seconds_since0;
			}

		} else if (item.type == sitevttype_sedentary) {
			if (item.ctx == sitsedentary_alert) {
				result2.sedentary_alert_count ++;
			}

		} else if (item.type == workoutevt_n32) {
			bool is_wkoscript_index = item.ctx >= workoutn32_wkoscript_index_min && item.ctx <= workoutn32_wkoscript_index_max;
			if (curr_workout != nullptr && !is_wkoscript_index) {
				if (curr_workout->wkon32_range.min == nposm) {
					curr_workout->wkon32_range.min = at;
				}
				curr_workout->wkon32_range.max = at;
			}

			if (is_wkoscript_index) {
				// start
				if (curr_split != nullptr) {
					VALIDATE(curr_workout != nullptr, null_str);
					VALIDATE(curr_split->flow_state_at == curr_flow_state, null_str);
					// tflow_state_C& state = curr_workout->flow_states[curr_flow_state];
					tflow_state_C& state = *curr_flow_state_ptr;
					VALIDATE(state.rep_count == 0, null_str);
					state.seg_count ++;
				}

				result2.workouts.push_back(tworkout_result2(item.ctx - workoutn32_wkoscript_index_min));
				curr_workout = &result2.workouts.back();
				curr_workout->start_of_today = result2.start_of_today;
				curr_workout->start_s = item_seconds_since0;
				curr_flow_state = nposm;

				curr_pre_workout = &pre_workouts[result2.workouts.size() - 1];
				curr_split = nullptr;

			} else if (item.ctx < workoutn32_max_states) {
				// enter state
				if (curr_workout != nullptr && item.ctx >= 0) {
					if (curr_flow_state != nposm) {
						VALIDATE(curr_flow_state_ptr != nullptr, null_str);
						curr_workout->range_ms.max = item.ms_since0;
						// curr_workout->flow_states[curr_flow_state].end_ms = item.ms_since0;
						curr_flow_state_ptr->end_ms = item.ms_since0;

						if (curr_split != nullptr) {
							VALIDATE(curr_split->flow_state_at == curr_flow_state, null_str);
							// tflow_state_C& state = curr_workout->flow_states[curr_flow_state];
							tflow_state_C& state = *curr_flow_state_ptr;
							VALIDATE(state.rep_count == 0, null_str);
							state.seg_count ++;
						}
					}
					curr_flow_state ++;
					// curr_workout->flow_state_count = curr_flow_state + 1;
					curr_workout->flow_states2.push_back(tflow_state_C());
					tflow_state_C& new_state = curr_workout->flow_states2[curr_flow_state];
					curr_workout->clear_flow_state_C(new_state);
					// 'tflow_state_C' is a plain C struct. 
					// Changing its fields will not cause 'std::vector<tflow_state_C>' to reallocate memory.
					curr_flow_state_ptr = &new_state;

					new_state.state = item.ctx;
					new_state.start_ms = item.ms_since0;

					landmarks_t0 = nposm;
					curr_subphase.clear();

					curr_split = nullptr;
					// if (curr_flow_state < MAX_CONSIDE_SPLIT_STATES) {
						const tpre_flow_state_C& pre_state = curr_pre_workout->flow_states[curr_flow_state];
						if (pre_state.split.flow_state_at != nposm) {
							VALIDATE(pre_state.split.flow_state_at == curr_flow_state, null_str);
							curr_split = &pre_state.split;
							curr_workout->clear_4_time_segs(new_state);

							tsegment_C& seg = new_state.segs[new_state.seg_count];
							// Do not set 'start_ms' at this time; set it only after receiving the first satisfied frame.
							// seg.start_ms = item.ms_since0;

							// SDL_Log("{dbg-split}workouts.size: %i, curr_flow_state: %i, segs: %i, curr_flow_state_ptr->state: %i", 
							//	(int)result2.workouts.size(), curr_flow_state, curr_split->segs, curr_flow_state_ptr->state);
						}
					// }
				}

			} else if (item.ctx >= workoutn32_state_subphase_min && item.ctx <= workoutn32_state_subphase_max) {
				if (curr_workout != nullptr && curr_flow_state != nposm) {
					// tflow_state_C& state = curr_workout->flow_states[curr_flow_state];
					tflow_state_C& state = *curr_flow_state_ptr;
					VALIDATE(curr_split == nullptr && state.seg_count == 0, null_str);
					trepetition_C& rep = state.reps[state.rep_count];
					if (item.ctx == workoutn32_active_period_start) {
						rep.active_start_ms[rep.phase_count] = item.ms_since0;
						curr_subphase.set(state.rep_count, rep.phase_count, rep_subp_active);

					} else if (item.ctx == workoutn32_cooldown_period_start) {
						rep.cooldown_start_ms[rep.phase_count] = item.ms_since0;
						curr_subphase.set(state.rep_count, rep.phase_count, rep_subp_cooldown);

						rep.phase_count ++;
						if (rep.rep_complete_ms != nposm) {
							state.rep_count ++;
						}
					} 
				}

			} else if (item.ctx == workoutn32_rep_complete) {
				if (curr_workout != nullptr && curr_flow_state != nposm) {
					// tflow_state_C& state = curr_workout->flow_states[curr_flow_state];
					tflow_state_C& state = *curr_flow_state_ptr;
					VALIDATE(curr_split == nullptr && state.seg_count == 0, null_str);

					trepetition_C& rep = state.reps[state.rep_count];
					rep.rep_complete_ms = item.ms_since0;
				}

			} else if (item.ctx == workoutn32_first_satisfied) {
				if (curr_workout != nullptr && curr_flow_state != nposm) {
					curr_workout->range_ms.min = item.ms_since0;
				}

			} else if (wkon32_ctx_is_analyze_track_pose_result(item.ctx)) {
				if (curr_workout != nullptr && curr_flow_state != nposm) {
					VALIDATE(curr_workout->range_ms.min != nposm, null_str);

					curr_workout->range_ms.max = item.ms_since0;
					// tflow_state_C& state = curr_workout->flow_states[curr_flow_state];
					tflow_state_C& state = *curr_flow_state_ptr;

					tsegment_C* seg = nullptr;
					if (curr_split != nullptr) {
						VALIDATE(state.rep_count == 0, null_str);
						VALIDATE(curr_split->flow_state_at == curr_flow_state, null_str);
						int next_seg_start_ms = state.start_ms + (state.seg_count + 1) * curr_split->interval_ms;
						bool is_next_first = false;
						if (item.ms_since0 >= next_seg_start_ms) {
							if (state.seg_count < curr_split->segs) {
								tsegment_C* old_seg = state.segs + state.seg_count;
								if (old_seg->start_ms == 0) {
									// No frames are received during this 'segment'. 
									// In a special case, if it is the first 'segment', it means the first satisfied frame has not yet been received.
									if (state.seg_count == 0) {
										// It is first segment, use the start_ms from 'state' as the start_ms for this 'segment'.
										old_seg->start_ms = state.start_ms;

									} else {
										// Use the start_ms from the previous 'segment' as the start_ms for this 'segment'.
										VALIDATE(state.seg_count >= 1, null_str);
										old_seg->start_ms = state.segs[state.seg_count - 1].start_ms;
									}
									old_seg->duration_ms = item.ms_since0 - old_seg->start_ms; 
								}
								state.seg_count ++;
								is_next_first = true;
							} else {
								int ii = 0;
								// SDL_Log("(warning)state.seg_count(%i) >= curr_split->segs(%i), This may be a programming error.", state.seg_count, curr_split->segs);
							}
						}
						seg = state.segs + state.seg_count;
						// if (is_next_first) {
						if (seg->start_ms == 0) { // 'seg[0]->start_ms == 0' or 'is_next_first == true'
							VALIDATE(seg->start_ms == 0 && seg->duration_ms == 0 && seg->unsatisfied_reason_count == 0, null_str);
							seg->start_ms = item.ms_since0;
						}
					}
					if (landmarks_t0 != nposm) {
						const int delta = item.ms_since0 - landmarks_t0;
						if (delta <= landmarks_continuous_threshold) {
							int& duration = item.ctx == workoutn32_satisfied_landmarks? state.satisfied_duration_ms: state.unsatisfied_duration_ms;
							duration += delta;

							if (seg != nullptr) {
								seg->duration_ms += delta;

								int& duration = item.ctx == workoutn32_satisfied_landmarks? seg->satisfied_duration_ms: seg->unsatisfied_duration_ms;
								duration += delta;
							}
						}

					}
					landmarks_t0 = item.ms_since0;

					if (wkon32_ctx_is_unsatisfied_reason(item.ctx)) {
						if (curr_subphase.in_active()) {
							trepetition_C& rep = state.reps[curr_subphase.rep_at];
							int& to_index = rep.unsatisfied_reason_count[curr_subphase.phase_at];
							if (to_index < WKO_MAX_UNSATISFIED_REASON_PER_REP) {
								rep.unsatisfied_reasons[curr_subphase.phase_at][to_index].ts_ms = item.ms_since0 - rep.active_start_ms[0];
								rep.unsatisfied_reasons[curr_subphase.phase_at][to_index ++].r = item.ctx;
							}
						}
						if (seg != nullptr) {
							int& to_index = seg->unsatisfied_reason_count;
							if (to_index < WKO_MAX_UNSATISFIED_REASON_PER_SEG) {
								seg->unsatisfied_reasons[to_index].ts_ms = item.ms_since0 - seg->start_ms;
								seg->unsatisfied_reasons[to_index ++].r = item.ctx;
							}
						}
					}
				}
			} else if (item.ctx == workoutn32_alert) {
				if (curr_workout != nullptr && curr_flow_state != nposm) {
					// tflow_state_C& state = curr_workout->flow_states[curr_flow_state];
					tflow_state_C& state = *curr_flow_state_ptr;
					state.alerts ++;
				}
			}
		}

		if (calc_sit_duration) {
			// sit duration
			const int sit_delta = t1 - sit_t0;
			if (sit_delta <= sit_continuous_threshold) {
				if (enable_log) {
					SDL_Log("{sit}, %i + %i -> %i", result2.sit_durations[curr_hour], 
						sit_delta, result2.sit_durations[curr_hour] + sit_delta);
				}
				result2.sit_durations[curr_hour] += sit_delta;
			}
			sit_t0 = t1;
		}

	}
	// SDL_Log("{dbg-health}--------------------");

	VALIDATE(pre_workouts.size() == result2.workouts.size(), null_str);
	for (int at = 0; at < pre_workouts.size(); at ++) {
		const tpre_workout& pre_workout = pre_workouts[at];
		const tworkout_result2& workout = result2.workouts[at];
		VALIDATE(pre_workout.start_s == workout.start_s, null_str);
		// VALIDATE(pre_workout.flow_state_count == workout.flow_state_count, null_str);
		VALIDATE(pre_workout.flow_state_count == (int)workout.flow_states2.size(), null_str);
		VALIDATE(pre_workout.range_ms.min == workout.range_ms.min && pre_workout.range_ms.max == workout.range_ms.max, null_str);
		for (int state_at = 0; state_at < pre_workout.flow_state_count; state_at ++) {
			// if (state_at == MAX_CONSIDE_SPLIT_STATES) {
			//	break;
			// }

			const aplt::tflow_state_C* flow_states = workout.flow_states2.data();
			const tpre_flow_state_C& pre_state = pre_workout.flow_states[state_at];
			// const tflow_state_C& state = workout.flow_states[state_at];
			const tflow_state_C& state = flow_states[state_at];

			VALIDATE(pre_state.state == state.state, null_str);
			VALIDATE(pre_state.start_ms == state.start_ms, null_str);
			if (pre_state.end_ms != state.end_ms) {
				VALIDATE(pre_state.end_ms == workout.range_ms.max, null_str);
			}
			VALIDATE(pre_state.rep_count == state.rep_count, null_str);
			if (pre_state.rep_count != 0) {
				VALIDATE(pre_state.is_rep_counter, null_str);
			}
		}
	}

	int workout_at = 0;
	// const int erase_front_count = 2;
	// const int erase_front_count = 27;

	// below 'result2.find_zip_workout' require result2.zip_workouts is valid.
	result2.zip_workouts.assign_data(result.zip_workouts);

	const int erase_front_count = 0;
	for (std::vector<tworkout_result2>::iterator it = result2.workouts.begin(); it != result2.workouts.end(); workout_at ++) {
		tworkout_result2& workout = *it;
		VALIDATE(workout.start_of_today == result2.start_of_today, null_str);
		bool erase = !workout.is_data_sufficient() || workout_at < erase_front_count;
		if (!erase) {
			tuint8cdata_C zip_workout = result2.find_zip_workout(workout.start_s);
			if (zip_workout.ptr == nullptr) {
				if (result2.start_of_today != h_.start_of_today) {
					erase = true;
				}
			}
		}
/*
		if (!erase) {
			if (result2.start_of_today == 1786204800 && workout_at != 21) {
				int ii = 0;
				erase = true;
			}
		}
*/
		if (erase) {
			it = result2.workouts.erase(it);
		} else {
			aplt::tflow_state_C* flow_states = workout.flow_states2.data();

			// VALIDATE(workout.flow_state_count >= 1, null_str);
			VALIDATE(workout.flow_states2.size() >= 1, null_str);
			VALIDATE(workout.range_ms.max != nposm, null_str);
			// tflow_state_C& last_state = workout.flow_states[workout.flow_state_count - 1];
			tflow_state_C& last_state = flow_states[workout.flow_states2.size() - 1];
			if (last_state.end_ms < workout.range_ms.max) {
				last_state.end_ms = workout.range_ms.max;
			}
			++ it;
		}
	}

	if (result2.workouts.size() >= 2) {
		// std::reverse(result2.workouts.begin(), result2.workouts.end());
	}

	std::map<int, twko_tlv_history_C> historys;
	workout_tlvs_to_historys(result.workout_tlvs, historys);
	
	int col_count = 0;
	for (std::vector<tworkout_result2>::iterator it = result2.workouts.begin(); it != result2.workouts.end(); ++ it) {
		tworkout_result2& workout = *it;
		VALIDATE(workout.wkon32_event_items == nullptr, null_str);
		int s = workout.wkon32_event_item_count() * sizeof(tevent_item);
		workout.wkon32_event_items = (tevent_item*)malloc(s);
		memcpy(workout.wkon32_event_items, items + workout.wkon32_range.min, s);

		if (historys.count(workout.start_s)) {
			workout.history = historys.find(workout.start_s)->second;
			// workout.history = get_wko_tlv_history(result2.start_of_today, workout.start_s, "leagor_plank", &result2);
		}
	}
	

	workout_cfg_strs_to_map2(result.workout_cfg_strs, result2.workout_cfgs);

	return true;
}

}