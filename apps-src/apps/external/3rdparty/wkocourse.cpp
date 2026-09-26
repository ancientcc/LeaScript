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

#include "wkocourse.hpp"

#include "rose_string_utils_dll.hpp"
#include "rose_config_3rdparty.hpp"
#include "gettext.hpp"
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"
#include "rose_sdl_utils.hpp"

#include <angles/angles.h>

#include <boost/foreach.hpp>

using namespace std::placeholders;

namespace aplt {

//
// twkocourse
//
bool twkocourse::tworkout::from_cfg(const config& cfg)
{
	clear();

	tauto_destruct_executor destruct_executor(std::bind(&tworkout::clear, this));

	aplt_ = cfg["aplt2"].str();
	if (!aplt_.empty()) {
		// now version, aplt_ must be empty.
		return false;
	}
	if (!aplt_.empty() && !is_bundleid(aplt_)) {
		return false;
	}

	id = cfg["id"].str();
	if (id.empty()) {
		return false;
	}
	note = cfg["note"].str();

	rounds = cfg["rounds"].to_int(WKO_DFLT_ROUNDS_PER_WORKOUT);
	if (!is_valid_workout_rounds(rounds)) {
		return false;
	}

	destruct_executor.cancel_execute();
	return true;
}

void twkocourse::tworkout::to_cfg(config& cfg) const
{
	VALIDATE(valid(), null_str);

	if (!aplt_.empty()) {
		cfg["aplt"] = aplt_;
	}

	cfg["id"] = id;

	if (!note.empty()) {
		cfg["note"] = note;
	}
	if (rounds != WKO_DFLT_ROUNDS_PER_WORKOUT) {
		cfg["rounds"].from_int(rounds);
	}
}

bool twkocourse::tday::from_cfg(int _day_at, const config& cfg, int total_days)
{
	VALIDATE(total_days > 0, null_str);
	clear();

	tauto_destruct_executor destruct_executor(std::bind(&tday::clear, this));
/*
	day_at = cfg["days"].to_int(nposm) - 1;
	if (day_at < 0 || day_at >= total_days) {
		return false;
	}
*/
	VALIDATE(_day_at >= 0 && day_at < total_days, null_str);
	day_at = _day_at;

	title = cfg["title"].str();
	if (title.empty()) {
		return false;
	}

	std::set<std::string> existed_workout_id2s;
	BOOST_FOREACH (const config &workout_cfg, cfg.child_range("workout")) {
		workouts.push_back(tworkout());
		tworkout& workout = workouts.back();

		if (!workout.from_cfg(workout_cfg)) {
			return false;
		}

		const std::string id2 = utils::join_app_prefix_id(workout.aplt_, workout.id);
		// must make sure 'workout.id2' is unique.
		if (existed_workout_id2s.count(id2) != 0) {
			return false;
		}
		existed_workout_id2s.insert(id2);
	}

	destruct_executor.cancel_execute();
	return true;
}

void twkocourse::tday::to_cfg(config& cfg) const
{
	// cfg["days"].from_int(day_at + 1);
	cfg["days"].from_string(str_cast(day_at + 1), true);

	if (!title.empty()) {
		cfg["title"].from_string(title, true);
	}

	for (std::vector<tworkout>::const_iterator it = workouts.begin(); it != workouts.end(); ++ it) {
		const tworkout& workout = *it;
		config& subcfg = cfg.add_child("workout");
		workout.to_cfg(subcfg);
	}
}

bool twkocourse::from_cfg(const config& cfg)
{
	clear();

	tauto_destruct_executor destruct_executor(std::bind(&twkocourse::clear, this));

	id = cfg["id"].str();
	if (id.empty()) {
		return false;
	}
	title = cfg["title"].str();
	if (title.empty()) {
		return false;
	}

	description = cfg["description"].str();
	if (description.empty()) {
		return false;
	}

	author = cfg["author"].str();
	reference = cfg["reference"].str();
	// if (reference.empty()) {
	//	return false;
	// }

	total_days = cfg["total_days"].to_int(nposm);
	if (!is_valid_total_days(total_days)) {
		return false;
	}

	grace_period_days = cfg["grace_period_days"].to_int(WKO_DFLT_GRACE_PERIOD_DAYS);
	if (!is_valid_grace_period_days(grace_period_days)) {
		return false;
	}

	price = cfg["price"].to_int(WKOCOURSE_DFLT_PRICE);
	if (!is_valid_wkocourse_price(price)) {
		return false;
	}

	std::string currency_rmb = "CNY";
	currency = cfg["currency"].str();
	if (currency.empty()) {
		currency = currency_rmb;
	}
	if (currency != currency_rmb) {
		return false;
	}

	days.resize(total_days);
	std::set<int> existed_days;
	std::vector<std::string> v_str;
	BOOST_FOREACH (const config &day_cfg, cfg.child_range("day")) {
		v_str = utils::split(day_cfg["days"].str(), ',');
		if (v_str.size() != 1) {
			return false;
		}
		int day_at = utils::to_int(v_str[0]) - 1;
		if (day_at < 0 || day_at >= total_days) {
			return false;
		}
		
		if (existed_days.count(day_at) != 0) {
			return false;
		}
		existed_days.insert(day_at);

		tday& day = days[day_at];
		if (!day.from_cfg(day_at, day_cfg, total_days)) {
			return false;
		}
		VALIDATE(day_at == day.day_at, null_str);
	}

	destruct_executor.cancel_execute();
	return true;
}

void twkocourse::to_cfg(config& cfg) const
{
	VALIDATE(valid(), null_str);

	cfg["id"] = id;
	cfg["title"] = title;
	if (!author.empty()) {
		cfg["author"] = author;
	}
	if (!description.empty()) {
		cfg["description"] = description;
	}
	if (!reference.empty()) {
		cfg["reference"] = reference;
	}

	cfg["total_days"].from_int(total_days);
	cfg["grace_period_days"].from_int(grace_period_days);
	cfg["price"].from_int(price);
	cfg["currency"].from_string(currency, true);

	for (std::vector<tday>::const_iterator it = days.begin(); it != days.end(); ++ it) {
		const tday& day = *it;
		if (day.is_empty_cfg()) {
			continue;
		}
		config& day_cfg = cfg.add_child("day");
		day.to_cfg(day_cfg);
	}
}

bool twkocourse::from_string(const std::string& str)
{
	clear();

	config top_cfg;
	if (!read_config_ex(str, true, top_cfg)) {
		return false;
	}

	const config& course_cfg = top_cfg.child("wkocourse");
	if (!course_cfg) {
		return false;
	}

	return from_cfg(course_cfg);
}

bool twkocourse::is_id_same_filename(const std::string& filename) const
{
	const std::string short_filename = utils::extract_file(filename);
	return utils::file_stem_name(short_filename) == id;
}

void twkocourse::from_file(const std::string& filename, bool id_must_same)
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

void twkocourse::to_file(const std::string& filename) const
{
	VALIDATE(!filename.empty(), null_str);

	VALIDATE(valid(), null_str);
	std::stringstream result;
	result.str("");
	config top_cfg;

	config& pair_cfg = top_cfg.add_child("wkocourse");
	to_cfg(pair_cfg);

	if (!top_cfg.empty()) {
		write_config(result, top_cfg);
	}

	write_file(filename, result.str().c_str(), result.str().size());
}

std::string twkocourse::from_aplt_file(const aplt::tapplet& aplt, const std::string& file)
{
	clear();

	std::string filename = aplt.preferences_dir + "/wkocourse/" + file;
	from_file(filename);
	if (!valid()) {
		filename = aplt.res_path + "/wkocourse/" + file;
		from_file(filename);
	}
	if (!valid()) {
		// if fail, return value is err message.
		return i18n::freq_msgstr_3str(i18n::msgid_load_file_fail, _("Course script"), filename, null_str);
	}
	// if ok, return value is full filename.
	return filename;
}

std::string twkocourse::build_course_filename(const std::string& wkocourse_dir) const
{
	VALIDATE(!wkocourse_dir.empty(), null_str);
	VALIDATE(isvalid_normal_id_or_var_name224(id), null_str);

	return wkocourse_dir + "/" + id + ".cfg";
}

void twkocourse::resize_days_by_total_days()
{
	VALIDATE(is_valid_total_days(total_days), null_str);
	days.resize(total_days);

	for (int day_at = 0; day_at < total_days; day_at ++) {
		tday& day = days[day_at];
		if (day.day_at != day_at) {
			day.day_at = day_at;
		}
	}
	validate_days();
}

void twkocourse::validate_days() const
{
	VALIDATE((int)days.size() == total_days, null_str);

	for (int day_at = 0; day_at < total_days; day_at ++) {
		const tday& day = days[day_at];
		VALIDATE(day.day_at == day_at, null_str);
	}
}

bool twkocourse::equal(const aplt::twkocourse& that) const
{
	if (id != that.id || title != that.title || author != that.author || description != that.description ||
		reference != that.reference) {
		return false;
	}
	// don't compare 'version'.

	if (total_days != that.total_days || grace_period_days != that.grace_period_days) {
		return false;
	}

	if (price != that.price || currency != that.currency) {
		return false;
	}

	if (days.size() != that.days.size() || days != that.days) {
		return false;
	}

	return true;
}

void twkocourse::assign(const aplt::twkocourse& that)
{
	// don't inclide cfg_str. it isn't cfg key.
	// cfg_str = that.cfg_str;

	id = that.id;
	title = that.title;
	author = that.author;
	description = that.description;
	reference = that.reference;
	total_days = that.total_days;
	grace_period_days = that.grace_period_days;
	price = that.price;
	currency = that.currency;

	days = that.days;
}

uint64_t twkocourse::is_valid2(std::string& err_msg, const tday** err_day) const
{
	if (err_day != nullptr) {
		*err_day = nullptr;
	}

	if (!isvalid_normal_id_or_var_name224(id)) {
		return tcookie3f(0, typeid_course, fid_id).u64;
	}
	if (!isvalid_short_utf8_name216(title)) {
		return tcookie3f(0, typeid_course, fid_title).u64;
	}
	if (description.empty() || !utils::is_utf8str(description.c_str(), description.size())) {
		return tcookie3f(0, typeid_course, fid_description).u64;
	}
	if (!is_valid_total_days(total_days)) {
		return tcookie3f(0, typeid_course, fid_total_days).u64;
	}
	if (!is_valid_grace_period_days(grace_period_days)) {
		return tcookie3f(0, typeid_course, fid_grace_period_days).u64;
	}
	if (!is_valid_wkocourse_price(price)) {
		return tcookie3f(0, typeid_course, fid_price).u64;
	}
	std::string currency_rmb = "CNY";
	if (currency != currency_rmb) {
		return tcookie3f(0, typeid_course, fid_currency).u64;
	}

	if ((int)days.size() != total_days) {
		err_msg = _("At least one  is required.");
		return tcookie3f(0, typeid_course, fid_days).u64;
	}

	for (int day_at = 0; day_at < (int)days.size(); day_at ++) {
		const tday& day = days[day_at];
		if (day.title.empty() || !utils::is_utf8str(day.title.c_str(), day.title.size())) {
			return tcookie3f(day_at, typeid_day, fid_title).u64;
		}
	}

	return TCOOKIE3F_CHECK_OK;
}

std::string twkocourse::get_field_str(int type, int field)
{
	if (type == typeid_course) {
		if (field == fid_id) {
			return "ID";
		} else if (field == fid_title) {
			return _("Title");
		} else if (field == fid_description) {
			return _("wkocourse^description label");
		} else if (field == fid_author) { 
			return _("wkocourse^author label");
		} else if (field == fid_reference) { 
			return _("wkocourse^reference label");
		} else if (field == fid_total_days) {
			return _("wkocourse^total_days label");
		} else if (field == fid_grace_period_days) {
			return _("wkocourse^grace_period_days label");
		} else if (field == fid_price) {
			return _("wkocourse^price label");
		} else if (field == fid_currency) {
			return _("wkocourse^currency label");
		} else if (field == fid_days) {
			return _("Day");
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_day) {
		if (field == fid_title) {
			return _("wkocousre^day_title label");
		} else if (field == fid_workouts) {
			
		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_workout) {
		if (field == fid_typeself) {
			return _("Task");
		} else if (field == fid_aplt) {
			return _("Type");

		} else if (field == fid_id) {
			return _("wko^workout_id label");

		} else if (field == fid_note) {
			return _("wkocousre^workout_note label");

		} else if (field == fid_rounds) {
			return _("wkocourse^workout_rounds label");
			
		} else {
			VALIDATE(false, null_str);
		}

	} else {
		VALIDATE(false, null_str);
	}

	return null_str;
}

std::string twkocourse::get_placeholder_msg(int type, int field)
{
	std::string placeholder;
	if (type == typeid_course) {
		if (field == fid_id) {
			placeholder = i18n::freq_msgstr(i18n::msgid_isvalid_normal_id_or_var_name);

		} else if (field == fid_title) {
			placeholder = i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

		} else if (field == fid_description) {
			placeholder = i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

		} else if (field == fid_days) {

		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_day) {
		if (field == fid_title) {
			return i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

		} else if (field == fid_workouts) {
			return _("Value can not be empty.");

		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == typeid_workout) {
		if (field == fid_typeself) {

		} else if (field == fid_aplt) {

		// task_speak
		} else if (field == fid_note) {
			return i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

		} else if (field == fid_rounds) {
			return i18n::freq_msgstr(i18n::msgid_empty_or_greater_than_0);

		} else {
			VALIDATE(false, null_str);
		}

	} else {
		VALIDATE(false, null_str);
	}
	
	return placeholder;
}

std::string twkocourse::get_error_msg(const tcookie3f& cookie3f)
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

std::string twkocourse::fomrat_is_valid2_result(uint64_t res, const std::string& err_msg)
{
	VALIDATE(res != TCOOKIE3F_CHECK_OK, null_str);

	tcookie3f cookie3f(res);

	std::stringstream err;
	if (err_msg.empty()) {
		err << get_error_msg(cookie3f);
	} else {
		err << err_msg;
	}
/*
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
*/
	return err.str();
}

// @day_at: 
//  course_total_days - 1: today
//  course_total_days - 2: yesterday
//  course_total_days - 3: The day before yesterday.
//  ...
int day_at_to_course_day_at(int day_at, int64_t enroll_active, int course_total_days)
{
	VALIDATE(enroll_active != nposm, null_str);
	VALIDATE(is_valid_total_days(course_total_days), null_str);
	// VALIDATE(day_at >= 0 && day_at < course_total_days, null_str);
	VALIDATE(day_at < course_total_days, null_str);

	int64_t today_0h0m0s = utils::calculate_0h0m0s_ts(time(nullptr));
	int64_t active_0h0m0s = utils::calculate_0h0m0s_ts(enroll_active);
	int64_t diff_s = today_0h0m0s - active_0h0m0s;

	// Due to the time zone(game_config::equation_of_time), 
	// 'today_0h0m0s' and 'active_0h0m0s' may not be divisible by ONE_DAY_SECONDS, 
	// but their difference(diff_s) can be divisible.
	VALIDATE((diff_s % ONE_DAY_SECONDS) == 0, null_str);
	int diff_days = diff_s / ONE_DAY_SECONDS;
	int course_day_at = diff_days + (day_at - (course_total_days - 1));
	return course_day_at;
}

int MAX_HEALTH_day_at_to_course_day_at(int day_at, int64_t enroll_active, int course_total_days)
{
	VALIDATE(day_at >= 0 && day_at < MAX_HEALTH_DAYS, null_str);

	int day_at2 = day_at;
	day_at2 += course_total_days - MAX_HEALTH_DAYS;

	return day_at_to_course_day_at(day_at2, enroll_active, course_total_days);
}

bool did_walk_wkocourse(const std::string& dir, const SDL_dirent2* dirent, int type, const std::set<std::string>& ext_names, 
	std::set<std::string>& result_set, const std::string& root)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		std::string name = utils::lowercase(dirent->name);
		if (type == type_wkocourse_ids || type == type_wkocourse_cfgfiles) {
			std::string ext_name = utils::file_ext_name(name);
			if (ext_names.count(ext_name) != 0) {
				if (type == type_wkocourse_ids) {
					std::string stem_name = name.substr(0, name.size() - 4);
					result_set.insert(stem_name);

				} else if (type == type_wkocourse_cfgfiles) {
					result_set.insert(dirent->name);
				}
			}
		} else if (type == type_wkocourse_new_benchmarks) {
			VALIDATE(false, null_str);
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
		VALIDATE(false, null_str);
/*
		std::string name = utils::lowercase(dirent->name);
		if (type == type_wkocourse_new_dirs) {
			if (name.size() > wko_new_dir_prefix.size() && name.find(wko_new_dir_prefix) == 0) {
				result_set.insert(dirent->name);
			}
		}
*/
	}
	return true;
}

void list_wkocourse_files_by_type(const std::string& wkocourse_dir2, int type, std::set<std::string>& result_set)
{
	result_set.clear();

	std::set<std::string> ext_names;
	if (type == type_wkocourse_ids || type == type_wkocourse_cfgfiles) {
		ext_names.insert("cfg");

	} else if (type == type_wkocourse_new_benchmarks) {
		ext_names.insert("png");
		ext_names.insert("jpg");
	}
	walk_dir(wkocourse_dir2, false, std::bind(&did_walk_wkocourse, _1, _2, type, std::ref(ext_names), std::ref(result_set), std::ref(wkocourse_dir2)));
}

}