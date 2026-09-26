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

#ifndef LIBROSE2_WKOSCRIPT_HPP
#define LIBROSE2_WKOSCRIPT_HPP

#include "aplt2.hpp"
#include "aplt_api.hpp"

namespace aplt {

extern LIB3RDPARTY_DECL const SDL_FPoint fake_landmarks[fake_lmk_count];

extern LIB3RDPARTY_DECL std::map<int, tcode3> wko_task_types;
extern LIB3RDPARTY_DECL std::map<int, tcode3> wko_pose_types;
extern LIB3RDPARTY_DECL std::map<int, tcode3> wko_operand_types;
extern LIB3RDPARTY_DECL std::map<int, tcode3> wko_ang_ranges;
extern LIB3RDPARTY_DECL std::map<int, tcode3> wko_time_rules;
extern LIB3RDPARTY_DECL std::map<int, tcode3> wko_time_tones;

// The value should be 'WKO_MAX_POSES_PER_TRACK * 2', but to avoid performing the multiplication every time it is used unnecessarily, 
// change it to a constant.
#define WKO_MAX_ANTI_SHAKE_FIELDS	48

struct tanti_shake_sample_C
{
	double d[WKO_MAX_ANTI_SHAKE_FIELDS]; // data
	int fields;
};

#define MAX_ANTI_SHAKE_SAMPLES			4 // 4

#define WKO_MAX_REFERENCE_BYTES			64
class taction_tpl2;

class DECLSPEC twkoscript // wko: WorKOut
{
public:
	class tstate2;

	enum {tasktype_speak, tasktype_time_counter, tasktype_rep_counter, tasktype_count};
	class DECLSPEC ttask_base
	{
	public:
		ttask_base(tstate2& state2, int type)
			: type(type)
			, state2_(&state2)
			, b_api_(aplt::get_b_api())
			, pinyin_(aplt::get_curr_pinyin())
			, finished_(false)
		{}
/*
		ttask_base(const ttask_base& that)
			: type(that.type)
			, state2_(that.state2_)
			, b_api_(that.b_api_)
			, pinyin_(that.pinyin_)
			, finished_(that.finished_)
		{}
*/
		virtual ~ttask_base() {}

		virtual bool from_cfg(const config& cfg) = 0;
		virtual void to_cfg(config& cfg) const
		{
			cfg["type"] = wko_task_types.find(type)->second.id;
		}

		bool operator==(const ttask_base& that) const
		{
			if (type != that.type) {
				return false;
			}

			return is_cfg_equal(that);
		}
		bool operator!=(const ttask_base& that) const { return !operator==(that); }

		virtual void did_enter_state() = 0;
		virtual void did_exit_state() = 0;
		virtual void did_mediapipe_new_frame(bool satisfied) {}
		virtual void did_satisfied_to_unsatisfied() {}
		virtual void slice() {}

		void set_finished()
		{
			VALIDATE(!finished_, null_str);
			finished_ = true;
		}

		bool finished() const { return finished_; }

	private:
		virtual bool is_cfg_equal(const ttask_base& that) const = 0;

	public:
		int type;
		// Why is a pointer needed instead of a reference? 
		// -- Sometimes, after calling "operator=", the value of state2_ needs to be modified, 
		// -- which cannot be done with a reference. 
		// -- Of course, this pointer always points to an object.
		// -- see twkoscript::from_state2_cfg when '!state2.action_tpl2_id.empty()'.
		tstate2* state2_;

	protected:
		tb_api& b_api_;
		aplt::tpinyin& pinyin_;
		bool finished_;
	};

	struct DECLSPEC ttask_speak: public ttask_base
	{
	public:
		ttask_speak(tstate2& state2)
			: ttask_base(state2, tasktype_speak)
			, repeat_s(nposm)
			, min_state_duration_s(nposm)
			, next_speak_ticks_(0)
			, last_speak_s_(0)
		{}

		~ttask_speak() {}

		bool from_cfg(const config& cfg) override;
		void to_cfg(config& cfg) const override;

		ttask_speak& operator=(const ttask_speak& that)
		{
			VALIDATE(type == that.type, null_str);
			VALIDATE(next_speak_ticks_ == 0, null_str);
			VALIDATE(last_speak_s_ == 0, null_str);

			msgstr = that.msgstr;
			repeat_s = that.repeat_s;
			min_state_duration_s = that.min_state_duration_s;
			return *this;
		}

	private:
		bool is_cfg_equal(const ttask_base& _that) const override
		{
			const ttask_speak& that = *static_cast<const ttask_speak*>(&_that);
			if (msgstr != that.msgstr) {
				return false;
			}

			if (repeat_s != that.repeat_s) {
				return false;
			}

			if (min_state_duration_s != that.min_state_duration_s) {
				return false;
			}

			return true;
		}

		void did_enter_state() override;
		void did_exit_state() override;
		void slice() override;

		void update_next_speak_ticks();
		void min_duration_slice();

	public:
		std::string msgstr;
		int repeat_s;
		int min_state_duration_s;

	private:
		uint32_t next_speak_ticks_;
		int last_speak_s_;
	};

	enum {timerule_total, timerule_satisfied, timerule_strict, timerule_count};
	enum {timetone_full, timetone_split, timetone_silent, timetone_count};
	struct DECLSPEC ttime_counter: public ttask_base
	{
	public:
		ttime_counter(tstate2& state2)
			: ttask_base(state2, tasktype_time_counter)
			, rule(nposm)
			, tone(nposm)
			, max_count(nposm)
			, satisfied_threshold_s(nposm)
			, sn_last_speak_s_(nposm)
			, last_frame_ticks_(0)
			, total_satisfied_ms_(nposm)
			, satisfied_trigger_flag_(bool_set_none)
			, tone_state_(nposm)
		{}
		~ttime_counter() {}

		bool from_cfg(const config& cfg) override;
		bool from_cfg_override(const config& cfg);
		void to_cfg(config& cfg) const;
		void to_cfg_override(const ttime_counter& tpl2_task, config& cfg) const;

		ttime_counter& operator=(const ttime_counter& that)
		{
			VALIDATE(type == that.type, null_str);
			VALIDATE(sn_last_speak_s_ == nposm, null_str);
			VALIDATE(last_frame_ticks_ == 0, null_str);
			VALIDATE(total_satisfied_ms_ == nposm, null_str);
			VALIDATE(satisfied_trigger_flag_ == bool_set_none, null_str);
			VALIDATE(tone_state_ == nposm, null_str);
			VALIDATE(last_speak_text_.empty(), null_str);

			rule = that.rule;
			tone = that.tone;
			max_count = that.max_count;
			satisfied_threshold_s = that.satisfied_threshold_s;
			satisfied_msgstr = that.satisfied_msgstr;
			return *this;
		}

		int total_satisfied_ms() const;

	private:
		bool is_cfg_equal(const ttask_base& _that) const override
		{
			const ttime_counter& that = *static_cast<const ttime_counter*>(&_that);
			if (rule != that.rule) {
				return false;
			}
			if (tone != that.tone) {
				return false;
			}

			if (max_count != that.max_count) {
				return false;
			}

			if (satisfied_threshold_s != that.satisfied_threshold_s) {
				return false;
			}
			if (satisfied_msgstr != that.satisfied_msgstr) {
				return false;
			}

			return true;
		}

		void did_enter_state() override;
		void did_exit_state() override;
		void did_mediapipe_new_frame(bool satisfied) override;
		void did_satisfied_to_unsatisfied() override;
		void slice() override;

		void set_countdown(int _countdown_s);
		void clear_countdown();
		void countdown_slice_total();
		void countdown_slice_satisfied();
		void countdown_slice_strict();

		void tone_speak(int total_sec, int curr_sec);

	public:
		int rule;
		int tone;
		int max_count;

		int satisfied_threshold_s;
		std::string satisfied_msgstr;

	private:
		int sn_last_speak_s_;
		uint32_t last_frame_ticks_;
		int total_satisfied_ms_;

		bool_set_t satisfied_trigger_flag_;
		enum {tonestate_first10s, tonestate_mid1th, tonestate_mid2th, tonestate_last10s, tonestate_count};
		int tone_state_;
		std::string last_speak_text_;
	};

	struct tphase
	{
	public:
		tphase()
			: min_duration_ms(nposm)
			, cooldowned_ms(nposm)
		{}

		bool valid(bool last) const 
		{
			if (last) {
				return min_duration_ms > 0;
			}
			return !action_msg.empty() && min_duration_ms > 0 && cooldowned_ms > 0; 
		}

		bool operator==(const tphase& that) const
		{
			if (action_msg != that.action_msg) {
				return false;
			}
			if (min_duration_ms != that.min_duration_ms) {
				return false;
			}
			if (cooldowned_ms != that.cooldowned_ms) {
				return false;
			}

			return true;
		}
		bool operator!=(const tphase& that) const { return !operator==(that); }

	public:
		// approach/escape_msg
		std::string action_msg;
		int min_duration_ms;
		int cooldowned_ms;
	};

	// rep: repetition
	struct DECLSPEC trep_counter: public ttask_base
	{
	public:
		trep_counter(tstate2& state2)
			: ttask_base(state2, tasktype_rep_counter)
			, max_count(nposm)
			// , curr_phase_(state2.track_pose.curr_phase_)
			, first_active_period_start_sent_(false)
			, next_satisfied_ticks_(0)
			, next_cooldowned_ticks_(0)
			, escape_threshold_ms_(3000)
			, next_speak_escape_ticks_(0)
			, count_(nposm)
			, delay_speak_threshold_ms_(350)
			, log_file_(nullptr)
			, last_log_ticks_(0)
		{}
		~trep_counter();

		bool from_cfg(const config& cfg) override;
		bool from_cfg_override(const config& cfg);
		void to_cfg(config& cfg) const;
		void to_cfg_override(const trep_counter& tpl2_task, config& cfg) const;

		trep_counter& operator=(const trep_counter& that)
		{
			VALIDATE(type == that.type, null_str);
			VALIDATE(count_ == nposm, null_str);

			max_count = that.max_count;
			phases = that.phases;
			// phase_count = that.phase_count;
			return *this;
		}

		bool first_active_period_start_sent() const { return first_active_period_start_sent_; }
		void set_first_active_period_start_sent(bool val) { first_active_period_start_sent_ = val; }

		int rt_count() const { return count_; }

	private:
		bool is_cfg_equal(const ttask_base& _that) const override
		{
			const trep_counter& that = *static_cast<const trep_counter*>(&_that);

			if (max_count != that.max_count) {
				return false;
			}

			if (phases.size() != that.phases.size() || phases != that.phases) {
				return false;
			}

			return true;
		}

		void clear()
		{
			max_count = nposm;

			phases.clear();
			// phase_count = nposm;
		}

		void did_enter_state() override;
		void did_exit_state() override;
		void did_mediapipe_new_frame(bool satisfied) override;
		void did_satisfied_to_unsatisfied() override;
		void slice() override;

	private:
		int& curr_phase() { return state2_->track_pose.curr_phase_; }
		void to_next_phase();
		void update_next_satisfied_ticks(const std::string& scene);
		void zero_next_satisfied_ticks(const std::string& scene);
		void update_next_cooldowned_ticks();
		void zero_next_cooldowned_ticks();

		void set_delay_speak(uint32_t ticks, const std::string& msg);

		void to_log_file(const char *fmt, ...);

	public:
		int max_count;

		std::vector<tphase> phases;
		// The @phase_count value is always 'phases.size() + 1'. 
		// To avoid mistakenly assuming it is 'phases.size()', a dedicated variable is provided.
		// int phase_count;

	private:
		std::string original_unsatisfied_2th_msgstr_;
		// int& curr_phase_;
		bool first_active_period_start_sent_;
		uint32_t next_satisfied_ticks_;
		uint32_t next_cooldowned_ticks_;
		const int escape_threshold_ms_;
		uint32_t next_speak_escape_ticks_;
		int count_;

		struct tdelay_speak 
		{
			tdelay_speak()
				: ticks(0)
			{}

			void clear()
			{
				ticks = 0;
				msg.clear();
			}

			uint32_t ticks;
			std::string msg;
		};
		tdelay_speak delay_speak_;
		const int delay_speak_threshold_ms_;

		tfile* log_file_;
		uint32_t last_log_ticks_;

	};

	enum {posetype_angle3p, posetype_angle2p, posetype_diff, posetype_count};
	enum {operandtype_point, operandtype_x, operandtype_y, operandtype_dist_x, operandtype_dist_y, operandtype_count};
#define pose_has_abs(pose) ((pose).type == posetype_diff && (pose).operand_type != operandtype_point)

#define operandtype_must_point(pose_type)	((pose_type) == posetype_angle3p || (pose_type) == posetype_angle2p)
#define operand_must_2landmark(operand_type) ((operand_type) == operandtype_dist_x || (operand_type) == operandtype_dist_y)

	enum {angrange_180, angrange_360, angrange_count};
#define def_pose_ang_range		angrange_180
	struct DECLSPEC tpose
	{
	public:
		tpose()
			: type(nposm)
			, validate_range_(true)
		{
			clear();
		}
		virtual ~tpose() {}

		virtual bool from_cfg(const config& cfg);
		bool from_cfg_override(const config& cfg);

		virtual void to_cfg(config& cfg) const;
		void to_cfg_override(const tpose& tpl2_pose, config& cfg) const;

		virtual bool valid() const { return type != nposm && operand_type != nposm; }
		uint64_t is_valid2_range(int index, std::string& err_msg) const;

		virtual bool operator==(const tpose& that) const;
		bool operator!=(const tpose& that) const { return !operator==(that); }

		void calc_point_operand(const SDL_FPoint* landmarks33, SDL_DPoint* result) const;
		void calc_1d_operand(const SDL_FPoint* landmarks33, double* result) const;

#define MAX_OPERANDS_PER_POSE		3
#define MAX_LANDMARKS_PER_OPERAND	2
		virtual void clear()
		{
			// don't touch 'validate_range_'.

			type = nposm;
			operand_type = nposm;
			for (int at = 0; at < MAX_OPERANDS_PER_POSE; at ++) {
				operands[at] = twko_operand{nposm, nposm};
			}
			for (int at = 0; at < MAX_OPERANDS_PER_POSE; at ++) {
				divisor[at] = 1.0;
			}
			for (int at = 0; at < MAX_OPERANDS_PER_POSE; at ++) {
				addend[at] = 0.0;
			}
			abs = false;
			ang_range = nposm;
			range = SDL_DRange{float_nposm, float_nposm};

			phase_mask = nposm;
			name.clear();
			unsatisfied_msgstr.clear();
			unsatisfied_rmax_msgstr.clear();
		}

		// SDL_DPoint.x: if invalid, float_nposm. else 0.
		// SDL_DPoint.y: result.
		SDL_DPoint calc_result_may_invalid(const SDL_FPoint* landmarks) const;

		// for diff version.
		std::string operands_to_landmarks_str() const;
		std::string get_diff_id() const;
		bool id_can_diff(const tpose& tpl2_pose) const;

	public:
		int type;
		int operand_type;
		twko_operand operands[MAX_OPERANDS_PER_POSE];
		double divisor[MAX_OPERANDS_PER_POSE];
		double addend[MAX_OPERANDS_PER_POSE];
		bool abs;
		int ang_range;
		SDL_DRange range;
		int phase_mask;
		std::string name;
		std::string unsatisfied_msgstr;
		std::string unsatisfied_rmax_msgstr;

	protected:
		bool validate_range_;
	};

	struct DECLSPEC ttrack_pose
	{
	public:
		ttrack_pose()
		{
			clear();
		}

		bool from_cfg(const config& cfg);
		bool from_cfg_override(const config& cfg);
		void to_cfg(config& cfg) const;
		void to_cfg_override(const taction_tpl2& action_tpl2, config& cfg) const;

		bool valid() const { return !poses.empty(); }
		// int calc_posture_fields() const;

		std::string absent_landmark_msgstr(int landmark) const;
		void get_pose_names(const std::string& exclude, std::set<std::string>& result) const;
		tpose& insert_pose(int at, const std::string& name);
		void erase_pose(int pose_at);
		void pose_swap(int s1, int s2);

		bool operator==(const ttrack_pose& that) const
		{
			if (memcmp(landmarks, that.landmarks, sizeof(landmarks)) != 0) {
				return false;
			}
			if (landmark_count != that.landmark_count) {
				return false;
			}
			if (posture_fields1 != that.posture_fields1) {
				return false;
			}
			if (poses.size() != that.poses.size() || poses != that.poses) {
				return false;
			}
			return true;
		}
		bool operator!=(const ttrack_pose& that) const { return !operator==(that); }

		void clear()
		{
			memset(landmarks, 0, sizeof(landmarks));
			landmark_count = 0;
			posture_fields1 = 0;

			poses.clear();

			curr_phase_ = nposm;
		}

	public:
		void update_landmarks(twkoscript& script);

	public:
		int landmarks[mediapipe::kNumPoseLandmarks];
		int landmark_count;
		int posture_fields1;
		std::vector<tpose> poses;

	public:
		int curr_phase_;
	};

	class DECLSPEC taction_tpl
	{
	public:
		taction_tpl()
			: task(nullptr)
			, is_setup(false)
			, satisfied_threshold_s1(nposm)
			, unsatisfied_threshold_ms(nposm)
			, unsatisfied_2th_threshold_s(nposm)
		{
		}

		taction_tpl(const taction_tpl& that)
			: task(nullptr)
		{
			*this = that;
		}

		taction_tpl& operator=(const taction_tpl & that)
		{
			// Only state2 has never been run(used for parsing or editing), 
			// allow the call to 'tstate2 state2 = that'.
			// VALIDATE(script_ == nullptr, null_str);
			// VALIDATE(that.script_ == nullptr, null_str);

			// state = that.state;
			// lmk33_png_at = that.lmk33_png_at;
			track_pose = that.track_pose;

			if (that.task != nullptr) {
				if (task != nullptr && task->type != that.task->type) {
					delete task;
					task = nullptr;
				}
				if (that.task->type == tasktype_speak) {
					ttask_speak* that_task2 = static_cast<ttask_speak*>(that.task);
					if (task == nullptr) {
						task = new ttask_speak(*that_task2);
					} else {
						ttask_speak* task2 = static_cast<ttask_speak*>(task);
						*task2 = *that_task2;
					}

				} else if (that.task->type == tasktype_time_counter) {
					ttime_counter* that_task2 = static_cast<ttime_counter*>(that.task);
					if (task == nullptr) {
						task = new ttime_counter(*that_task2);
					} else {
						ttime_counter* task2 = static_cast<ttime_counter*>(task);
						*task2 = *that_task2;
					}

				} else if (that.task->type == tasktype_rep_counter) {
					trep_counter* that_task2 = static_cast<trep_counter*>(that.task);
					if (task == nullptr) {
						task = new trep_counter(*that_task2);
					} else {
						trep_counter* task2 = static_cast<trep_counter*>(task);
						*task2 = *that_task2;
					}
				} else {
					VALIDATE(false, null_str);
				}
			} else {
				VALIDATE(task == nullptr, null_str);
			}

			// track pose relative
			is_setup = that.is_setup;
			satisfied_threshold_s1 = that.satisfied_threshold_s1;
			satisfied_msgstr1 = that.satisfied_msgstr1;

			unsatisfied_threshold_ms = that.unsatisfied_threshold_ms;
			unsatisfied_2th_threshold_s = that.unsatisfied_2th_threshold_s;

			unsatisfied_2th_msgstr = that.unsatisfied_2th_msgstr;
			return *this;
		}

		virtual ~taction_tpl()
		{
			if (task != nullptr) {
				delete task;
				task = nullptr;
			}
		}

		bool from_cfg(const config& cfg, tstate2& state2);
		bool from_cfg_override(const config& cfg, tstate2& state2);

		virtual void to_cfg(config& cfg) const;
		void to_cfg_override(const taction_tpl2& action_tpl2, config& cfg) const;

		virtual bool valid() const { return track_pose.valid() && task != nullptr; }
		ttask_base* new_task(int type, tstate2& state2);

		virtual bool operator==(const taction_tpl& that) const
		{
			if (track_pose != that.track_pose) {
				return false;
			}

			if (task == nullptr) {
				if (that.task != nullptr) {
					return false;
				}
			} else if (that.task == nullptr) {
				return false;

			} else if (*task != *that.task) {
				return false;
			}

			if (is_setup != that.is_setup) {
				return false;
			}

			if (satisfied_threshold_s1 != that.satisfied_threshold_s1) {
				return false;
			}
			if (satisfied_msgstr1 != that.satisfied_msgstr1) {
				return false;
			}

			if (unsatisfied_threshold_ms != that.unsatisfied_threshold_ms) {
				return false;
			}
			if (unsatisfied_2th_threshold_s != that.unsatisfied_2th_threshold_s) {
				return false;
			}
			if (unsatisfied_2th_msgstr != that.unsatisfied_2th_msgstr) {
				return false;
			}
			return true;
		}
		bool operator!=(const taction_tpl& that) const { return !operator==(that); }

		void clear_action_tpl()
		{
			track_pose.clear();
			if (task != nullptr) {
				delete task;
				task = nullptr;
			}

			is_setup = false;
			satisfied_threshold_s1 = nposm;
			satisfied_msgstr1.clear();

			unsatisfied_threshold_ms = nposm;
			unsatisfied_2th_threshold_s = nposm;

			unsatisfied_2th_msgstr.clear();
		}

	public:
		twkoscript::ttrack_pose track_pose;
		twkoscript::ttask_base* task;

		// track pose relative
		bool is_setup;
		// satisfied_xxx isn't from cfg file, evaluate by tstate2::set_satisfied_msgstr
		int satisfied_threshold_s1;
		std::string satisfied_msgstr1;

		int unsatisfied_threshold_ms;
		int unsatisfied_2th_threshold_s;

		std::string unsatisfied_2th_msgstr;
	};

	enum {typeid_script, typeid_global, typeid_task, typeid_track_pose, typeid_pose};
	enum {fid_id, fid_name, fid_states,
		fid_action_tpl2_id, fid_is_setup, fid_debug_skip, fid_unsatisfied_threshold_ms, fid_unsatisfied_2th_threshold_s, fid_unsatisfied_2th_msgstr, 
		fid_pose_phase_mask, fid_pose_name, fid_pose_min, fid_pose_max, fid_pose_unsatisfied_msgstr, fid_pose_unsatisfied_rmax_msgstr, fid_pose_ang_range,
		fid_typeself, fid_type, fid_operand_type, fid_landmarks,

		// for tasktype_speak
		fid_task_msgstr, fid_task_repeat_s, fid_task_min_state_duration_s,

		// for tasktype_time_counter
		fid_task_satisfied_threshold_s, fid_task_satisfied_msgstr,
		fid_time_counter_rule, fid_time_counter_tone, fid_time_counter_max_count,
		fid_rep_counter_max_count,

		// for tasktype_rep_counter
		fid_phase_min, fid_phase_action_msg = fid_phase_min, fid_phase_min_duration_ms, fid_phase_cooldowned_ms, fid_phase_max = fid_phase_cooldowned_ms,
	};

	class DECLSPEC tstate2: public taction_tpl
	{
	public:
		tstate2(int _state)
			: state(_state)
			, lmk33_png_at(nposm)
			, debug_skip(false)
			, b_api_(aplt::get_b_api())
			, pinyin_(aplt::get_curr_pinyin())
			, script_(nullptr)
			, pose_state_at_(nposm)
			, enter_state_ticks_(0)
			, first_satisfied_ticks_(0)
			, threshold_first_satisfied_ticks_(0)
			, last_satisfied_ticks_(0)
			, next_speak_satisfied_msg_ticks_(0)
			, next_speak_unsatisfied_msg_ticks_(0)
			, speak_unsatisfied_msgstr_count_(0)
			, speak_satisfied_msgstr1_only_once_(bool_set_none)
		{
			next.clear();
		}

		tstate2(const tstate2& that)
			: b_api_(aplt::get_b_api())
			, pinyin_(that.pinyin_)
			, script_(nullptr)
			, pose_state_at_(nposm)
			, enter_state_ticks_(0)
			, first_satisfied_ticks_(0)
			, threshold_first_satisfied_ticks_(0)
			, last_satisfied_ticks_(0)
			, next_speak_satisfied_msg_ticks_(0)
			, next_speak_unsatisfied_msg_ticks_(0)
			, speak_unsatisfied_msgstr_count_(0)
			, speak_satisfied_msgstr1_only_once_(bool_set_none)
		{
			*this = that;
		}

		tstate2& operator=(const tstate2 & that)
		{
			taction_tpl::operator=(that);

			// Only state2 has never been run(used for parsing or editing), 
			// allow the call to 'tstate2 state2 = that'.
			VALIDATE(script_ == nullptr, null_str);
			VALIDATE(that.script_ == nullptr, null_str);

			state = that.state;
			lmk33_png_at = that.lmk33_png_at;

			action_tpl2_id = that.action_tpl2_id;
			next = that.next;

			// track pose relative
			debug_skip = that.debug_skip;

			return *this;
		}

		~tstate2() {}

		void to_cfg(const std::vector<std::string>& state_names, config& cfg) const;
		void green();
		// ttask_base* new_task(int type);
		void set_next_to_state(int to_state);

		bool operator==(const taction_tpl& that) const override
		{
			// Attempt to convert to the same type.
			const tstate2* derived = dynamic_cast<const tstate2*>(&that);
			if (!derived) {
				return false;  // Types are different, cannot be equal.
			}
			if (!taction_tpl::operator==(that)) {
				return false;
			}

			if (state != derived->state) {
				return false;
			}

			if (lmk33_png_at != derived->lmk33_png_at) {
				return false;
			}

			if (action_tpl2_id != derived->action_tpl2_id) {
				return false;
			}

			if (next != derived->next) {
				return false;
			}

			if (debug_skip != derived->debug_skip) {
				return false;
			}

			return true;
		}
		// Since 'bool operator==(const tstate2& that)' is not implemented, 'bool operator!=(const tstate2& that)' should likewise not be implemented. 
		// -- This avoids potential pitfalls, such as inadvertently hiding the base class's overloaded '!=' operator in certain scenarios.

		static std::string get_field_str(int type, int field);
		static std::string get_placeholder_msg(int type, int field);
		static std::string get_error_msg(const tcookie3f& cookie3f);
		std::string diff_id_err_msg(const std::string& field_str) const;
		uint64_t is_valid2(int pose_states_parsed, std::string& err_msg, bool check_range = true) const;
		// void sync_task_input_vars(const aplt::tapplet& aplt, const aplt::tapplet::ttask& task);
		bool state_swap(int s1, int s2);
		std::string build_lmk33_png_filename(const std::string& phase_surf_dir, int phase) const;

		int did_enter_state();
		void did_exit_state();
		void did_mediapipe_new_frame(bool satisfied, const std::string& _unsatisfied_msg);
		void slice();

	public:
		void set_satisfied_msgstr(int threshold_s, const std::string& msgstr);
		void sn_update_next_unsatisfied_ticks(bool use_2th_threshold);
		void sn_speak_unsatisfied_msg();

	private:
		uint64_t is_sub_valid_speak(std::string& err_msg) const;

	public:
		int state;
		int lmk33_png_at;

		std::string action_tpl2_id;
		tif_block next;

		// track pose relative
		bool debug_skip;

	public:
		aplt::tb_api& b_api_;
		aplt::tpinyin& pinyin_;
		// If 'script_' is not nullptr, it means it will be used for running, 
		// not just for parsing and editing.
		twkoscript* script_;
		std::string unsatisfied_msg_;
		// (is_pose_state && !is_setup), Do not accumulate 'setup' state.
		int pose_state_at_;

		uint32_t enter_state_ticks_;
		uint32_t first_satisfied_ticks_;
		uint32_t threshold_first_satisfied_ticks_;
		uint32_t last_satisfied_ticks_;
		uint32_t next_speak_satisfied_msg_ticks_;
		uint32_t next_speak_unsatisfied_msg_ticks_;
		int speak_unsatisfied_msgstr_count_;
		bool_set_t speak_satisfied_msgstr1_only_once_;
	};

	static const std::string& reserved_key_id2();
	static std::string id_to_filename(const std::string& _id);
	static std::string build_lmk33_png_basename(int state, int phase);
	static std::string build_lmk33_png_filename3(const std::string& phase_surf_dir, int state, int phase, bool is_tmp);
	static void init_wkoscript();

	twkoscript();

	~twkoscript()
	{
		VALIDATE(slot_ == nullptr, null_str);
	}

	bool valid() const { return isvalid_normal_id_or_var_name224(id); }

	bool from_cfg(const config& cfg);
	bool from_state2_cfg(const config& cfg, const std::map<std::string, int>& state_names_map, int pose_states_parsed, tstate2** state2_result);
	void to_cfg(config& cfg) const;
	bool equal(const twkoscript& that) const;

	// because member 'pinyin_', cannot use b = a. 
	void assign(const aplt::twkoscript& that);
	uint64_t is_valid2(std::string& err_msg, const tstate2** err_state, bool check_range = true) const;
	static std::string fomrat_is_valid2_result(uint64_t res, const std::string& err_msg);

	std::string title2() const 
	{
		std::stringstream ss;
		ss << title << "(" << id << ")";

		return ss.str();
	}

	int pose_state_count(bool* has_setup) const;

	std::string build_script_filename(const std::string& wkoscript_dir) const;
	std::string build_phase_surf_dir(const std::string& wkoscript_dir) const;

	void sync_task_input_vars(const std::map<aplt::taplt_key, aplt::tapplet>& applets);
	bool state_swap(int s1, int s2);
	void get_state_names(const std::string& exclude, std::set<std::string>& result) const;
	tstate2& insert_state(int at, const std::string& name, int task_type, int lmk33_png_at);
	void erase_state(int at);
	void clone_state(int at);
	void set_lmk33_png_at_equal_to_state_at();
	void history_after_one_finish(const SDL_Range& range_ms, uint32_t& reps, uint32_t& duration_s) const;
	void apply_action_tpl2(tstate2& state2, const taction_tpl2& action_tpl2);
	void copy_action_tpl2(const tstate2& state2, taction_tpl2& action_tpl2);

	// When use gui writes task, some redundant items will be generated, such as req_task.position1, 
	// which will be eliminated in 'green()'. 
	void green();

	std::string py_from_utf8str(const std::string& str) const;

	void clear()
	{
		id.clear();
		title.clear();
		author.clear();
		reference.clear();
		version = null_str;
		startup_state.clear();

		state_names.clear();
		states.clear();
	}

	bool is_id_same_filename(const std::string& filename) const;
	void from_file(const std::string& filename, bool id_must_same = true);
	void to_file(const std::string& filename) const;
	bool from_string(const std::string& str);

	std::string from_aplt_file(const aplt::tapplet& aplt, const std::string& file);

	void enable_run();
	void set_wko_task_slot(twko_task_slot* _slot);
	void did_phase_changed(int state_at, int phase_at);
	bool sfx_enabled();

	struct ttmp_result {
		const twkoscript::tpose* pose;
		int pose_at;
		double val0;
		double* avg_sample0;
		double val1;
		double* avg_sample1;

		// only used for c++ debug.
		SDL_DPoint dbg_points[3];
	};

	void set_tmp_result(const twkoscript::tpose& pose, int pose_at, double val0, double val1, tanti_shake_sample_C& posture, tanti_shake_sample_C& avg, ttmp_result& result);
	// if satisfied, return value is nposm. 
	// else is [workoutn32_unsatisfied_reason_min, workoutn32_unsatisfied_reason_max], mean is unsatisfied.
	int analyze_track_pose(const tstate2& state2, const SDL_FPoint* landmarks, int inference_ms,
		std::string& unsatisfied_msg, std::string& overlay_msg, twko_analyze_result_C* result_out);

	//
	// anti_shike_sample
	//
	void reset_anti_shike_samples(int fields);
	tanti_shake_sample_C average_sample(const tanti_shake_sample_C& newly);
	int recv_samples() const { return recv_samples_; }

private:
	void did_from_cfg_quited(const std::string& err_msg);
	void calc_pose_state_at();

public:
	// 'cfg_str' isn't existed in *.cfg.
	// use for temperal var, for example 'ros_.health_push_str_event(workoutevt_str, )'
	std::string cfg_str; 

	std::string id;
	std::string title;
	std::string author;
	std::string reference;
	version_info version;
	tif_block startup_state;

	std::vector<std::string> state_names;
	std::map<int, tstate2> states;
private:
	//
	// parse pinyin
	//
	aplt::tpinyin& pinyin_;
	const int tone_;
	const bool eng_lowercase_;
	const std::string input_var_signature_;

	const std::string satisfied_msgstr_;
	const std::string unsatisfied_msgstr_;
	twko_task_slot* slot_;

	tuint8data2_C overlay_msg_;
	//
	// anti_shake_sample
	//
	const int reset_samples_threshold_ms_;
	int next_sample_index_;
	tanti_shake_sample_C anti_shake_samples_[MAX_ANTI_SHAKE_SAMPLES];
	float temp_4_average_[MAX_ANTI_SHAKE_SAMPLES];
	uint32_t last_recv_sample_ticks_;
	int recv_samples_;
};

#define wko_is_angle_from_pose_type(type) \
	((type) == aplt::twkoscript::posetype_angle3p || (type) == aplt::twkoscript::posetype_angle2p)

#define wko_operand_count_from_pose_type(type) \
	((type) == aplt::twkoscript::posetype_angle3p? 3: 2)

#define wko_is_pose_state2_from_task_type(type) \
	((type) == aplt::twkoscript::tasktype_time_counter || (type) == aplt::twkoscript::tasktype_rep_counter)

#define wko_allow_multiple_phase_from_task_type(type) \
	((type) == aplt::twkoscript::tasktype_rep_counter)

#define wko_phase_count_from_task_type(type) \
	((type) == aplt::twkoscript::tasktype_rep_counter? WKO_MAX_PHASE_COUNT: 1)

extern LIB3RDPARTY_DECL std::map<int, tcode3> pose_sides;
extern LIB3RDPARTY_DECL std::map<int, tcode3> pose_metrics;

enum {poseside_left, poseside_right, poseside_other, poseside_count};

enum {posemetric_body_tilt_angle, posemetric_header_tilt_angle, 
	posemetric_left_upper_arm_angle, posemetric_right_upper_arm_angle,
	posemetric_left_elbow_angle, posemetric_right_elbow_angle, 
	posemetric_left_forearm_angle, posemetric_right_forearm_angle,
	posemetric_left_wrist_on_right_shoulder, posemetric_right_wrist_on_left_shoulder,
	posemetric_left_thigh_angle, posemetric_right_thigh_angle,
	posemetric_left_knee_angle, posemetric_right_knee_angle,
	posemetric_left_lower_leg_angle, posemetric_right_lower_leg_angle,
	posemetric_left_leg_angle, posemetric_right_leg_angle, // hip->ankle
	posemetric_right_hip_angle, posemetric_right_torso_angle,
	posemetric_count
};

class DECLSPEC tpreset_pose: public twkoscript::tpose
{
public:
	tpreset_pose()
		: metric(nposm)
		, side(nposm)
		, tolerance(nposm)
	{
		validate_range_ = false;
	}

	bool from_cfg(const config& cfg) override;
	void to_cfg(config& cfg) const override;

	bool valid() const override { return side != nposm && metric != nposm && tpose::valid(); }

	bool operator==(const tpose& that) const override {
		// Attempt to convert to the same type.
		const tpreset_pose* derived = dynamic_cast<const tpreset_pose*>(&that);
		if (!derived) {
			return false;  // Types are different, cannot be equal.
		}
		if (!tpose::operator==(that)) {
			return false;
		}
		if (metric != derived->metric || side != derived->side) {
			return false;
		}
		if (!KDL_Equal(tolerance, derived->tolerance)) {
			return false;
		}
		return true;
	}

	void clear() override
	{
		tpose::clear();
		metric = nposm;
		side = nposm;
		tolerance = nposm;
	}

private:
	void did_from_cfg_quited(const std::string& err_msg);

public:
	int side;
	int metric;
	double tolerance;
};

class DECLSPEC taction_tpl2: public twkoscript::taction_tpl
{
public:
	taction_tpl2()
		: state2_(nposm)
	{}

	bool from_cfg(const config& cfg);
	void to_cfg(config& cfg) const override;

	bool valid() const override { return !id.empty() && !name.empty() && taction_tpl::valid(); }
	std::string name2() const 
	{
		std::string result = name;
		result.append("(" + id + ")");

		return result;
	};

	bool operator==(const taction_tpl& that) const override {
		// Attempt to convert to the same type.
		const taction_tpl2* derived = dynamic_cast<const taction_tpl2*>(&that);
		if (!derived) {
			return false;  // Types are different, cannot be equal.
		}
		if (!taction_tpl::operator==(that)) {
			return false;
		}
		if (id != derived->id || name != derived->name) {
			return false;
		}
		if (msg != derived->msg) {
			return false;
		}
		return true;
	}

	void clear()
	{
		clear_action_tpl();
		id.clear();
		name.clear();
		msg.clear();
	}

private:
	void did_from_cfg_quited(const std::string& err_msg);

public:
	std::string id;
	std::string name;
	std::string msg;

private:
	twkoscript::tstate2 state2_;
};
extern LIB3RDPARTY_DECL std::map<std::string, taction_tpl2> action_tpl2s;

extern LIB3RDPARTY_DECL const std::string wko_new_dir_prefix;

LIB3RDPARTY_DECL std::string wkoscript_extract_id(const std::string& cfg_str, std::string* id2 = nullptr);

enum {type_wkoscript_ids, type_wkoscript_cfgfiles, type_wkoscript_new_benchmarks, type_wkoscript_new_dirs, type_wkoscript_count};
LIB3RDPARTY_DECL void list_wkoscript_files_by_type(const std::string& wkoscript_dir2, int type, std::set<std::string>& result_set);


}

#endif