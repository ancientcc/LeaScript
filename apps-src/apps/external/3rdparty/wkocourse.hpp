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

#ifndef LIBROSE2_WKOCOURSE_HPP
#define LIBROSE2_WKOCOURSE_HPP

#include "aplt2.hpp"
#include "aplt_api.hpp"

namespace aplt {

class DECLSPEC twkocourse
{
public:

#define WKO_MIN_TOTAL_DAYS	1
#define WKO_MAX_TOTAL_DAYS	90
#define WKO_DFLT_TOTAL_DAYS	15
#define is_valid_total_days(days)	((days) >= WKO_MIN_TOTAL_DAYS && (days) <= WKO_MAX_TOTAL_DAYS)

#define WKO_MIN_GRACE_PERIOD_DAYS	0
#define WKO_MAX_GRACE_PERIOD_DAYS	10
#define WKO_DFLT_GRACE_PERIOD_DAYS	7
#define is_valid_grace_period_days(days)	((days) >= WKO_MIN_GRACE_PERIOD_DAYS && (days) <= WKO_MAX_GRACE_PERIOD_DAYS)

#define WKOCOURSE_MIN_PRICE		0
#define WKOCOURSE_MAX_PRICE		1000
#define WKOCOURSE_DFLT_PRICE	0
#define is_valid_wkocourse_price(price)	((price) >= WKOCOURSE_MIN_PRICE && (price) <= WKOCOURSE_MAX_PRICE)

#define WKO_MIN_ROUNDS_PER_WORKOUT	1
#define WKO_MAX_ROUNDS_PER_WORKOUT	5
#define WKO_DFLT_ROUNDS_PER_WORKOUT	1
#define is_valid_workout_rounds(rounds)	((rounds) >= WKO_MIN_ROUNDS_PER_WORKOUT && (rounds) <= WKO_MAX_ROUNDS_PER_WORKOUT)


	enum {typeid_course, typeid_day, typeid_workout};
	enum {fid_typeself, fid_id, fid_title, fid_description, fid_author, fid_reference, fid_total_days, fid_grace_period_days, 
		fid_price, fid_currency, fid_days,
		fid_workouts, fid_aplt, fid_note, fid_rounds
	};

	class tworkout
	{
	public:
		tworkout()
		{
			clear();
		}

		bool from_cfg(const config& cfg);
		void to_cfg(config& cfg) const;

		bool valid() const { return !id.empty() && is_valid_workout_rounds(rounds); } 

		bool operator==(const tworkout& that) const
		{
			if (aplt_ != that.aplt_) {
				return false;
			}

			if (id != that.id) {
				return false;
			}

			if (note != that.note) {
				return false;
			}

			if (rounds != that.rounds) {
				return false;
			}
			return true;
		}
		bool operator!=(const tworkout& that) const { return !operator==(that); }

		void clear()
		{
			aplt_.clear();
			id.clear();
			note.clear();
			rounds = nposm;
		}

		std::string aplt(const std::string& course_aplt) const
		{
			return !aplt_.empty()? aplt_: course_aplt;
		}

		std::string get_id2(const std::string& course_aplt) const
		{
			VALIDATE(is_bundleid(course_aplt), null_str);
			const std::string& aplt = !aplt_.empty()? aplt_: course_aplt;
			return utils::join_app_prefix_id(aplt, id);
		}

	public:
		std::string aplt_;
		std::string id;
		std::string note;
		int rounds;
	};

	class tday
	{
	public:
		tday()
		{
			clear();
		}

		bool from_cfg(int _day_at, const config& cfg, int total_days);
		void to_cfg(config& cfg) const;

		bool is_empty_cfg() const
		{
			return title.empty() && workouts.empty();
		}

		bool operator==(const tday& that) const
		{
			if (day_at != that.day_at) {
				return false;
			}

			if (title != that.title) {
				return false;
			}

			if (workouts.size() != that.workouts.size() || workouts != that.workouts) {
				return false;
			}
			return true;
		}
		bool operator!=(const tday& that) const { return !operator==(that); }

		void clear()
		{
			day_at = nposm;
			title.clear();
			workouts.clear();
		}

	public:
		int day_at;
		std::string title;
		std::vector<tworkout> workouts;
	};

public:
	twkocourse()
	{
		clear();
	}

	bool from_cfg(const config& cfg);
	void to_cfg(config& cfg) const;

	bool from_string(const std::string& str);
	bool is_id_same_filename(const std::string& filename) const;
	void from_file(const std::string& filename, bool id_must_same = true);
	void to_file(const std::string& filename) const;

	std::string from_aplt_file(const aplt::tapplet& aplt, const std::string& file);
	bool valid() const { return !id.empty(); } 

	std::string build_course_filename(const std::string& wkocourse_dir) const;
	void resize_days_by_total_days();
	void validate_days() const;

	bool equal(const aplt::twkocourse& that) const;
	void assign(const aplt::twkocourse& that);
	uint64_t is_valid2(std::string& err_msg, const tday** err_day) const;
	static std::string get_field_str(int type, int field);
	static std::string get_placeholder_msg(int type, int field);
	static std::string get_error_msg(const tcookie3f& cookie3f);
	static std::string fomrat_is_valid2_result(uint64_t res, const std::string& err_msg);

	void clear()
	{
		id.clear();
		title.clear();
		author.clear();
		description.clear();
		reference.clear();

		total_days = nposm;
		grace_period_days = nposm;
		price = nposm;
		currency.clear();
		days.clear();
	}

	int get_day_workout_count(int day_at) const
	{
		VALIDATE(day_at >= 0 && day_at < total_days, null_str);
		const tday& day = days[day_at];
		int result = 0;
		for (int at = 0; at < (int)day.workouts.size(); at ++) {
			const tworkout& workout = day.workouts[at];
			result += workout.rounds;
		}
		return result;
	}

	void get_day_workout_id2s(int day_at, const std::string& course_aplt, std::map<std::string, int>& result, bool append = false) const
	{
		if (!append) {
			result.clear();
		}
		VALIDATE(day_at >= 0 && day_at < total_days, null_str);
		const tday& day = days[day_at];
		for (int at = 0; at < (int)day.workouts.size(); at ++) {
			const tworkout& workout = day.workouts[at];
			const std::string id2 = workout.get_id2(course_aplt);
			if (result.count(id2) == 0) {
				result.insert(std::make_pair(id2, workout.rounds));

			} else {
				result.find(id2)->second += workout.rounds;
			}
		}
		return;
	}

	void get_workout_id2s(const std::string& course_aplt, std::map<std::string, int>& result) const
	{
		result.clear();

		for (int day_at = 0; day_at < total_days; day_at ++) {
			const tday& day = days[day_at];
			get_day_workout_id2s(day.day_at, course_aplt, result, true);
		}
		return;
	}

public:
	std::string id;
	std::string title;
	std::string author;
	std::string description;
	std::string reference;
	int total_days;
	int grace_period_days;
	int price;
	std::string currency;
	std::vector<tday> days;
};

LIB3RDPARTY_DECL int day_at_to_course_day_at(int day_at, int64_t enroll_active, int course_total_days);
#define today_day_at_to_course_day_at(enroll_active, course_total_days) \
	day_at_to_course_day_at((course_total_days) - 1, enroll_active, course_total_days)

LIB3RDPARTY_DECL int MAX_HEALTH_day_at_to_course_day_at(int day_at, int64_t enroll_active, int course_total_days);

enum {type_wkocourse_ids, type_wkocourse_cfgfiles, type_wkocourse_new_benchmarks, type_wkocourse_new_dirs, type_wkocourse_count};
LIB3RDPARTY_DECL void list_wkocourse_files_by_type(const std::string& wkocourse_dir2, int type, std::set<std::string>& result_set);

}

#endif