#ifndef LIBROSE_HEALTH_HPP
#define LIBROSE_HEALTH_HPP

#include "rose_filesystem_dll.hpp"
#include "thread.hpp"
#include "rose_sdl_utils.hpp"
#include "wml_exception.hpp"
#include "aplt2.hpp"

#define WORKOUT_MAX_FLOW_STATES		70	// must >= workoutn32_max_states
#define WKO_MAX_REP_COUNT_PER_REP_STATE		50
#define WKO_MAX_UNSATISFIED_REASON_PER_REP	16

#define WKO_MAX_SEG_COUNT_PER_TIME_STATE	12
#define WKO_MAX_UNSATISFIED_REASON_PER_SEG	48

#define WKO_MIN_REPORT_GENERATION_DURATION_S	30

#define WKO_MAX_CHECKIN_GAP_DAYS	10

int64_t start_of_file_day_from_filename(const std::string& filename, bool* is_bak_ptr);

namespace aplt {

class twkoscript;

#define HEALTH_DAT_VER	1

struct treason_C
{
	uint16_t ts_ms;
	uint8_t r; // reason
};

struct trepetition_C
{
	int active_start_ms[WKO_MAX_PHASE_COUNT];
	int cooldown_start_ms[WKO_MAX_PHASE_COUNT];
	int phase_count;
	int rep_complete_ms;

	// The reason for unsatisfaction only needs to be known during the active period, 
	// not during the cooldown period. Therefore, only one sub-phase is needed here.
	treason_C unsatisfied_reasons[WKO_MAX_PHASE_COUNT][WKO_MAX_UNSATISFIED_REASON_PER_REP];
	int unsatisfied_reason_count[WKO_MAX_PHASE_COUNT];
};

struct tsegment_C
{
	int start_ms;
	int duration_ms;

	int satisfied_duration_ms;
	int unsatisfied_duration_ms;

	// The reason for unsatisfaction only needs to be known during the active period, 
	// not during the cooldown period. Therefore, only one sub-phase is needed here.
	treason_C unsatisfied_reasons[WKO_MAX_UNSATISFIED_REASON_PER_SEG];
	int unsatisfied_reason_count;
};

#define MIN_REP_STEP	1
#define MAX_REP_STEP	2
#define VALIDATE_REP_STEP(step)	VALIDATE((step) >= MIN_REP_STEP && (step) <= MAX_REP_STEP, null_str)


struct tflow_state_C
{
	int state;
	int start_ms;
	int end_ms;
	int satisfied_duration_ms;
	int unsatisfied_duration_ms;
	int alerts;

	union {
		tsegment_C segs[WKO_MAX_SEG_COUNT_PER_TIME_STATE];
		trepetition_C reps[WKO_MAX_REP_COUNT_PER_REP_STATE];
	};
	// 'seg_count' and 'rep_count' should be mutually exclusive, meaning at most one of them isn't 0.
	int seg_count;

	int rep_count;
	int rep_step;
};

inline int calc_flow_state_rep_cols2(int rep_count, int rep_step)
{
	VALIDATE_REP_STEP(rep_step);

	// return state.rep_count / state.rep_step + (state.rep_count % state.rep_step != 0? 1: 0);

	// Since it can be determined that both a and b are positive numbers, 
	// to speed up the calculation, this classic formula for integer ceiling division can be used.
	return (rep_count + rep_step - 1) / rep_step;
}

inline int calc_flow_state_rep_cols(const tflow_state_C& state)
{
	return calc_flow_state_rep_cols2(state.rep_count, state.rep_step);
}

#pragma pack(1)
struct twko_tlv_header_C
{
	int32_t type;
	int32_t length;
};

struct twko_tlv_history_C
{
	int32_t type; // T
	int32_t length; // L

	int seconds_since0; // wkoscript_index_seconds_since0. V's start
	int64_t start_of_lastday;
	SDL_Range last_range_ms;
	uint32_t days;
	uint32_t workouts;
	uint32_t reps;
	uint32_t duration_s;

	int32_t reserved0;
	int32_t reserved1;
	int32_t reserved2;
	int32_t reserved3;
};

enum {wko_tlv_type_history, wko_tlv_type_count};

#define WKO_TLV_HISTORY_LEN		(sizeof(aplt::twko_tlv_history_C) - offsetof(aplt::twko_tlv_history_C, seconds_since0))
#define init_wko_tlv_history(history)	\
	(history).type = aplt::wko_tlv_type_history; \
	(history).length = WKO_TLV_HISTORY_LEN; \
	(history).seconds_since0 = nposm;

#define wko_tlv_history_is_valid(history)		((history).seconds_since0 != nposm)

#pragma pack()

twko_tlv_history_C history_add(const twko_tlv_history_C& last_history, int64_t start_of_lastday, 
	SDL_Range last_range_ms, const twkoscript& this_script, int64_t start_of_thisday);

class thealth
{
public:

#pragma pack(1)
	struct thealth_header
	{
		uint32_t fourcc;
		uint32_t version;
		uint32_t n32_events;
		uint32_t workout_cfg_strs;
		uint32_t zip_workouts;
		uint32_t unzip_workout;
		uint32_t workout_tlvs;
		uint32_t reserve0;
		uint32_t reserve1;
		uint32_t reserve2;
		int64_t start_of_today;
	};

	struct tevent_item
	{
		int ms_since0; // milliseconds_since0

		// What value 'ctx' can take is determined by 'type'. 
		// The reason 'ctx' is placed before 'type' in the definition is solely
		// to improve the efficiency of accessing the 'ctx' member.
		int ctx;
		uint8_t type;
	};

#pragma pack()

	thealth();
	~thealth();

	const std::string& health_dir() const { return health_dir_; }
	int max_save_days() const { return max_save_days_; }
	const std::string health_dat_filename(time_t t) const;

	int health_push_n32_event(int type, int ctx);
	void health_push_str_event(int type, int ctx, const std::string& str, const std::string& aux_str);
	void health_push_landmarks(const SDL_U16Point* landmarks, int unsatisfied_reason);
#define WORKOUT_TYPE_PREFIX	0xff
#define WORKOUT_TYPE_BYTES	6
	enum {wkotype_start, wkotype_enter_state, wkotype_count};
	void health_push_n32_to_unzip_workout(uint8_t type, int ctx);

	void slice();

	void did_start_or_stop_base_subtask(bool start);

	struct tworkout_result2
	{
	public:
		tworkout_result2(int wkoscript_index)
			: wkoscript_index(wkoscript_index)
			, wkon32_event_items(nullptr)
		{
			clear();
		}

		tworkout_result2(const tworkout_result2& that)
			: wkon32_event_items(nullptr)
		{
			*this = that;
		}

		tworkout_result2& operator=(const tworkout_result2 & that)
		{
			wkoscript_index = that.wkoscript_index;
			start_of_today = that.start_of_today;
			start_s = that.start_s;
			// memcpy(flow_states, that.flow_states, sizeof(flow_states));
			// flow_state_count = that.flow_state_count;
			flow_states2 = that.flow_states2;
			range_ms = that.range_ms;

			wkon32_range = that.wkon32_range;

			// Only wkon32_event_tiems has never been malloc.
			VALIDATE(that.wkon32_event_items == nullptr, null_str);
			wkon32_event_items = that.wkon32_event_items;

			history = that.history;

			return *this;
		}

		~tworkout_result2()
		{
			if (wkon32_event_items != nullptr) {
				free(wkon32_event_items);
				wkon32_event_items = nullptr;
			}
		}

		// posix_noncopyable(tworkout_result2);

		bool is_data_sufficient() const
		{
			if (range_ms.min == nposm || range_ms.max == nposm) {
				return false;
			}
			int min_flow_state_count = 2; // 2 flow_state
			// if (flow_state_count < min_flow_state_count) {
			if ((int)flow_states2.size() < min_flow_state_count) {
				return false;
			}
			int min_duration_after_first_satisfied_ms = WKO_MIN_REPORT_GENERATION_DURATION_S * 1000; // 30 second
			if (range_ms.max - range_ms.min < min_duration_after_first_satisfied_ms) {
				// If the data is correct, 'range.min' should be equal to 'first_satisfied_s'. 
				// The following condition being true means that after receiving the first satisfied frame, 
				// the workout must last at least 30 more seconds.
				return false;
			}
			return true;
		}

		void clear()
		{
			// wkoscript_index = nposm;
			start_of_today = 0;
			start_s = nposm;

			flow_states2.clear();
/*
			memset(flow_states, 0, sizeof(flow_states));
			for (int at = 0; at < WORKOUT_MAX_FLOW_STATES; at ++) {
				tflow_state_C& state = flow_states[at];
				state.state = nposm;

				for (int rep_at = 0; rep_at < WKO_MAX_REP_COUNT_PER_REP_STATE; rep_at ++) {
					trepetition_C& rep = state.reps[rep_at];
					for (int phase_at = 0; phase_at < WKO_MAX_PHASE_COUNT; phase_at ++) {
						rep.active_start_ms[phase_at] = nposm;
					}
					rep.rep_complete_ms = nposm;
				}
				state.rep_step = MIN_REP_STEP;
			}
			flow_state_count = 0;
*/
			range_ms.min = nposm;
			range_ms.max = nposm;

			wkon32_range.min = nposm;
			wkon32_range.max = nposm;

			if (wkon32_event_items != nullptr) {
				free(wkon32_event_items);
				wkon32_event_items = nullptr;
			}

			memset(&history, 0, sizeof(history));
			history.seconds_since0 = nposm;
		}

		void clear_flow_state_C(tflow_state_C& state)
		{
			memset(&state, 0, sizeof(state));
			state.state = nposm;

			for (int rep_at = 0; rep_at < WKO_MAX_REP_COUNT_PER_REP_STATE; rep_at ++) {
				trepetition_C& rep = state.reps[rep_at];
				for (int phase_at = 0; phase_at < WKO_MAX_PHASE_COUNT; phase_at ++) {
					rep.active_start_ms[phase_at] = nposm;
				}
				rep.rep_complete_ms = nposm;
			}
			state.rep_step = MIN_REP_STEP;
		}

		void clear_4_time_segs(tflow_state_C& state)
		{
			// before this function, must has called clear().
			VALIDATE(state.seg_count == 0, null_str);

			// Why is this function needed? 'reps' and 'segs' use the same block of memory. 
			// clear() clears according to the requirements of 'reps', but it should be done according to 'segs'.
			memset(state.segs, 0, sizeof(state.segs));
		}

		int wkon32_event_item_count() const { return wkon32_range.max - wkon32_range.min + 1; }

		twko_tlv_history_C history_add_me(int64_t start_of_meday, const twkoscript& me_script, int64_t start_of_today) const;

	public:
		int wkoscript_index;
		// it is equal to 'thealth_result2.start_of_today' always.
		int64_t start_of_today;
		int start_s;

		// 'sizeof(tflow_state_C)' is really too large. To save memory, cannot use an array; 
		// have to use 'std::vector'.
		std::vector<tflow_state_C> flow_states2;

		// tflow_state_C flow_states[WORKOUT_MAX_FLOW_STATES];
		// int flow_state_count;

		SDL_Range range_ms;

		SDL_Range wkon32_range;
		tevent_item* wkon32_event_items;

		twko_tlv_history_C history;
	};

	struct thealth_result2
	{
	public:
		thealth_result2()
			: zip_workouts(sizeof(uint8_t))
			// , unzip_workout(sizeof(uint8_t))
		{
			clear();
		}
		~thealth_result2() {}

		bool valid() const { return start_of_today > 0; }

		void clear()
		{
			start_of_today = 0;
			sit_period = SDL_Range{0, 0};

			memset(&sit_durations, 0, sizeof(sit_durations));
			memset(&improper_durations, 0, sizeof(improper_durations));
			memset(&type_improper_durations, 0, sizeof(type_improper_durations));

			memset(&improper_alerts, 0, sizeof(improper_alerts));
			sedentary_alert_count = 0;

			workouts.clear();

			workout_cfgs.clear();

			zip_workouts.clear();
		}

		int improper_alert_count() const
		{
			const int s = sizeof(improper_alerts) / sizeof(improper_alerts[0]);
			int result = 0;
			for (int at = 0; at < s; at ++) {
				result += improper_alerts[at];
			}
			return result;
		}

		tuint8cdata_C find_zip_workout(int start_s) const;

		std::string to_msg_sit_period() const;
		std::string to_msg_sit_duration() const;
		std::string to_msg_alert_count(bool improper) const;
		std::string to_msg_workouts() const;

		enum {type_sit_duration, type_improper_duration, type_improper_alert, total_type_count};
		int calc_total(int type) const;

		enum {type_workout_duration, type_workout_times, type_workout_count};
		int calc_workout_total(int type) const;
		std::string to_msg_workout_max_range() const;

	public:
		int64_t start_of_today;
		SDL_Range sit_period;
		int sit_durations[ONE_DAY_HOURS];

		int improper_durations[ONE_DAY_HOURS];
		int type_improper_durations[aplt::sitimproper_reason_count][ONE_DAY_HOURS];

		int improper_alerts[ONE_DAY_HOURS];
		int sedentary_alert_count;

		std::vector<tworkout_result2> workouts;

		std::map<int, std::string> workout_cfgs;
		telem_array_C zip_workouts;
	};
	bool load_health_data_4_report(time_t t, thealth_result2& result2) const;

	void upgrade_health_file(const std::string& filename);

	struct thealth_result
	{
		thealth_result()
			: start_of_today(0)
			, event_items(sizeof(tevent_item))
			, workout_cfg_strs(sizeof(uint8_t))
			, zip_workouts(sizeof(uint8_t))
			, unzip_workout(sizeof(uint8_t))
			, workout_tlvs(sizeof(uint8_t))
		{}
		posix_noncopyable(thealth_result);

		bool valid() const { return !filename.empty() && start_of_today > 0; }
		void clear()
		{
			filename.clear();
			start_of_today = 0;

			event_items.clear();
			workout_cfg_strs.clear();
			zip_workouts.clear();
			unzip_workout.clear();
			workout_tlvs.clear();
		}

		std::string filename;
		int64_t start_of_today;
		

		telem_array_C event_items;
		telem_array_C workout_cfg_strs;
		telem_array_C zip_workouts;
		telem_array_C unzip_workout;
		telem_array_C workout_tlvs;
	};
	twko_tlv_history_C find_wko_history(int start_s) const;
	twko_tlv_history_C get_wko_tlv_history(int64_t start_of_today, int wko_seconds_since0, const std::string& wkoscript_id) const;
	void push_wko_tlv_history(int64_t start_of_today, int wko_seconds_since0, const std::string& id, thealth_result& h) const;
	bool migrate_health_dat_for_history(const std::string& filename) const;

private:
	void evaluate_start_of_today();
	void set_health_dirty(bool write_immediately);
	void new_day_if_necessary();

	struct tpre_split_C
	{
		int flow_state_at;
		int interval_ms;
		int segs;
	};

	struct tpre_flow_state_C
	{
		int state;
		int start_ms;
		int end_ms;

		trepetition_C rep;

		int rep_count;
		bool is_pose_state;
		bool is_rep_counter;

		tpre_split_C split;
	};

#define MAX_CONSIDE_SPLIT_STATES	12 // 6
	struct tpre_workout
	{
	public:
		tpre_workout()
		{
			clear();
		}

		void clear()
		{
			start_s = nposm;
			memset(flow_states, 0, sizeof(flow_states));
			for (int at = 0; at < WORKOUT_MAX_FLOW_STATES; at ++) {
				tpre_flow_state_C& state = flow_states[at];
				state.state = nposm;

				trepetition_C& rep = state.rep;
				rep.active_start_ms[0] = nposm;
				rep.rep_complete_ms = nposm;

				state.split.flow_state_at = nposm;
			}
			flow_state_count = 0;

			range_ms.min = nposm;
			range_ms.max = nposm;

			// memset(splits, 0, sizeof(splits));
			// split_count = 0;
		}

	public:
		int start_s;
		tpre_flow_state_C flow_states[WORKOUT_MAX_FLOW_STATES]; // MAX_CONSIDE_SPLIT_STATES
		int flow_state_count;
		SDL_Range range_ms;

#define MAX_SPLIT_COUNT		3
		// tpre_split_C splits[MAX_SPLIT_COUNT];
		// int split_count;
	};
	void pre_health_data_4_report(const thealth_result& result, std::vector<tpre_workout>& pre_workouts) const;

	void did_move_unzip_workout_to_zip_quited(telem_array_C& unzip_workout) const;
	bool move_unzip_workout_to_zip(thealth_result& result) const;
	void move_unzip_workout_to_zip_today();

	void write_file_helper_put(const uint8_t* data, int len) const;
	bool did_write_health_dat(tfile& file, const thealth_result& result) const;
	void write_health_dat();

	bool did_load_health_dat(tfile& file, int64_t dsize, thealth_result& result) const;
	void load_health_dat(time_t t, thealth_result& result) const;
	bool health_result_to_result2(const thealth_result& result, thealth_result2& result2) const;

	void verify_workout_cfg_strs() const;
	void workout_cfg_strs_to_map(const telem_array_C& cfg_strs, std::map<std::string, int>& result) const;
	void workout_cfg_strs_to_map2(const telem_array_C& cfg_strs, std::map<int, std::string>& result) const;
	void get_existed_workout_cfg_strs();

	void workout_tlvs_to_historys(const telem_array_C& workout_tlvs, std::map<int, twko_tlv_history_C>& historys) const;

	bool did_load_old_header_health_dat(tfile& file, int64_t dsize, thealth_result& result) const;

private:
	const std::string health_dir_;
	const int max_save_days_;
	const int max_parse_workouts_;
	int backup_type_;

	const int good_mask_ms_;
	uint32_t next_good_valid_ticks_;

	const int improper_mask_ms_;
	uint32_t next_improper_valid_ticks_;

	const int landmarks_mask_ms_;
	uint32_t next_satisfied_landmarks_valid_ticks_;
	uint32_t next_unsatisfied_landmarks_valid_ticks_;

	threading::mutex file_mutex_;
	uint32_t next_can_write_health_dat_ticks_;
	bool health_dat_dirty_;

	// for today. why not use 't_'? --'t_' maybe think as 'time_'
	thealth_result h_;
	std::map<std::string, int> existed_workout_cfg_strs_;

	mutable telem_array_C write_file_helper_;
	// telem_array_C write_file_helper_;
};

}

#endif
