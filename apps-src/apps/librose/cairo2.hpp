#ifndef LIBROSE_CAIRO2_HPP
#define LIBROSE_CAIRO2_HPP

#include <cairo/cairo.h>
#include "rose_sdl_utils.hpp"
// #include "aplt2.hpp"
#include "health.hpp"

#define IS_MULTIPLE_OF_4(x) (((x) & 3) == 0)

// Sometimes the 'share' button overlaps with other buttons. 
// Place pl_btn_share first, give the 'share' button the highest priority.
enum {pl_btn_share, pl_btn_play, pl_btn_pause, pl_btn_restart, pl_btn_stop, 
	pl_btn_step_backward, pl_btn_step_forward, pl_btn_progressbar, pl_btn_count};

enum {lmk_btn_phase0, lmk_btn_phase1, lmk_btn_open_image, lmk_btn_fake_landmark, lmk_btn_lmk33_mode, lmk_btn_clear, lmk_btn_count};
enum {lmkmode_both, lmkmode_left_only, lmkmode_right_only, lmkmode_count};
enum {lmkside_center, lmkside_left, lmkside_right, lmkside_count};

namespace cairo {

#define WKO_PLAN_WORKOUT_DURATION_S		120 // 30 second

enum {wkomattype_health, wkomattype_image, wkomattype_vlog, wkomattype_count};
enum {multcolmattype_start_history, multcolmattype_poses, multcolmattype_finish_history, multcolmattype_count};

enum {shapetype_circle_solid, shapetype_circle_hollow, 
	shapetype_rect_solid, shapetype_rect_hollow, 
	shapetype_surf, shapetype_count};
struct ticon
{
public:
	ticon()
		: shape_type(nposm)
	{}

	ticon(int _shape_type, const SDL_DColor& _color, const SDL_DSize& _size, double _border_width, double _padding)
		: shape_type(nposm)
	{
		set_cairo_shape(_shape_type, _color, _size, _border_width, _padding);
	}

	void set_cairo_shape(int _shape_type, const SDL_DColor& _color, const SDL_DSize& _size, double _border_width, double _padding);

	void set_surf(const surface& surf, double _padding);

	void set_xy(double _x, double _y)
	{
		x = _x;
		y = _y;
	}

	void draw(cairo_t *cr) const;

public:		
	int shape_type;
	SDL_DColor color;
	SDL_DSize size;
	double border_width;
	double padding;

	surface surf;

	double x;
	double y;
};

void draw_rounded_rectangle(cairo_t* cr, double x, double y, double width, double height, double radius,
	bool ltop = true, bool rtop = true, bool lbottom = true, bool rbottom = true);

void draw_rounded_rectangle2(const SDL_DColor* fill_color, const SDL_DColor* line_color, double line_width, cairo_t* cr, double x, double y, double width, double height, double radius,
    bool ltop = true, bool rtop = true, bool lbottom = true, bool rbottom = true);

void draw_rounded_rectangle3(const ticon* icon, const SDL_DColor* fill_color, const SDL_DColor* line_color, double line_width,  cairo_t* cr, double x, double y, double width, double height, double radius,
    bool ltop, bool rtop, bool lbottom, bool rbottom);

void draw_line_with_arrow(cairo_t* cr, const SDL_DPoint& start, const SDL_DPoint& end, const SDL_DColor& color,
   double arrow_size = 15.0, double line_width = 2.0, double arrow_width_ratio = 0.45);

void draw_canvas(cairo_t* cr, int width, int height, double radius, const SDL_DColor& color,
    const std::function<void (cairo_t* cr, int width, int height, double red, double green, double blue)>& did_draw = NULL,
	bool ltop = true, bool rtop = true, bool lbottom = true, bool rbottom = true);

cv::Mat argb_mat_from_CAIRO_FORMAT_ARGB32(cairo_surface_t* cairo_surf, uint8_t* data, int width, int height);

surface draw_obj(int width, int height, const SDL_DColor& canvas_color,
    const std::function<void (cairo_t* cr, int width, int height)>& did_draw_obj);
/*
surface draw_rounded_rectangle3(const SDL_Size& bg_size, const SDL_DColor& canvas_color, const SDL_DColor* fill_color, const SDL_DColor* line_color, double line_width, const SDL_Rect* rect, double radius,
    bool ltop = true, bool rtop = true, bool lbottom = true, bool rbottom = true);
*/
surface draw_rounded_rectangle_for_shape(const SDL_Size& bg_size, int margin, bool sel, const SDL_DColor& canvas_color, const SDL_DColor* fill_color, const SDL_DColor* line_color, double line_width, double radius,
    bool ltop = true, bool rtop = true, bool lbottom = true, bool rbottom = true);

surface draw_line_width_arrow_for_shape(const SDL_Point& start, const SDL_Point& end, const SDL_DColor& color,
	double arrow_size = 15.0, double line_width = 2.0, double arrow_width_ratio = 0.45, SDL_Rect* _enclose_rect = nullptr);

surface draw_rounded_rectangle_for_text_surf(const surface& text_surf, const SDL_Size& margin, 
	ticon* icon, const SDL_DColor& fill_color, const SDL_DColor& line_color, double line_width, double radius,
    bool ltop, bool rtop, bool lbottom, bool rbottom);

struct tsdl_field
{
	tsdl_field()
		: name_font_size(nposm)
		, name_text_size(0, 0)
		, val_font_size(nposm)
		, val_text_size(0, 0)
		, desire_size({0, 0})
		, margin({0, 0})
		, gap({0, 0})
		, cairo_color{0.0, 0.0, 0.0, 1.0}
		, offset({0, 0})
	{}

	void set(const std::string& _icon, const std::string& _name, int _name_font_size, const std::string& _val, int _val_font_size, const SDL_Point& _margin, const SDL_Point& _gap);

	void clear()
	{
		icon.clear();

		name.clear();
		name_font_size = nposm;
		name_text_size = SDL_Point{0, 0};

		val.clear();
		val_font_size = nposm;
		val_text_size = SDL_Point{0, 0};
	}

	std::string icon;
	std::string name;
	int name_font_size;
	tpoint name_text_size;
	SDL_Point desire_size;

	std::string val;
	int val_font_size;
	tpoint val_text_size;
	SDL_Point margin;
	SDL_Point gap;
	// used for cairo module.
	SDL_DColor cairo_color;

	// [out]
	SDL_Point offset;
};

struct theader_fields
{
public:
	enum {fid_sitting_period, fid_sitting_duration, fid_improper_alert, fid_sedentary_alert, fid_workout, fid_count};
	theader_fields()
		: share(bool_set_false)
		, pl_btn_rects(nullptr)
	{
		arrays[fid_sitting_period] = &sitting_period;
		arrays[fid_sitting_duration] = &sitting_duration;
		arrays[fid_improper_alert] = &improper_alert;
		arrays[fid_sedentary_alert] = &sedentary_alert;
		arrays[fid_workout] = &workout;
	}

public:
	bool_set_t share;

	tsdl_field sitting_period;
	tsdl_field sitting_duration;
	tsdl_field improper_alert;
	tsdl_field sedentary_alert;
	tsdl_field workout;
	tsdl_field* arrays[fid_count];

	SDL_Rect* pl_btn_rects;
};

// To avoid confusion, do not use 'cairo::tdays_posture_fields::fid_title'; instead, use 'fields.fid_title'.  
// The values of 'fid_title' value under different 'txxx_fields' are different.  
// When writing code, frequent copying and pasting,  using 'fields.fid_title' will naturally select the correct 'fid_title'.

#define Y_AXIS_TITLE_GAP_Y	8 // 14

struct tsit_fields
{
public:
	enum {fid_title, fid_left_y_axis, fid_right_y_axis, fid_legend_sit_duration, fid_legend_improper_duration, fid_legend_improper_alert,
		fid_today, fid_chart_remark, fid_count};

	tsit_fields()
		: legend_2legend_gap_y(5)
		, y_axis_title_gap_y(Y_AXIS_TITLE_GAP_Y)
		, share(bool_set_false)
		, pl_btn_rects(nullptr)
	{
		memset(arrays, 0, sizeof(arrays));

		arrays[fid_title] = &title;
		arrays[fid_today] = &today;
		arrays[fid_left_y_axis] = &left_y_axis;
		arrays[fid_right_y_axis] = &right_y_axis;
		arrays[fid_legend_sit_duration] = &legend_sit_duration;
		arrays[fid_legend_improper_duration] = &legend_improper_duration;
		arrays[fid_legend_improper_alert] = &legend_improper_alert;
		arrays[fid_chart_remark] = &chart_remark;

		for (int at = 0; at < fid_count; at ++) {
			VALIDATE(arrays[at] != nullptr, null_str);
		}
	}

public:
	int title_height;
	int legend_height;
	const int legend_2legend_gap_y;
	const int y_axis_title_gap_y;
	int time_labels_height;
	bool_set_t share;

	tsdl_field title;
	tsdl_field today;
	tsdl_field left_y_axis;
	tsdl_field right_y_axis;
	tsdl_field legend_sit_duration;
	tsdl_field legend_improper_duration;
	tsdl_field legend_improper_alert;
	tsdl_field chart_remark;
	tsdl_field* arrays[fid_count];
	
	int sit_durations[ONE_DAY_HOURS];
	int improper_durations[ONE_DAY_HOURS];
	struct ttype_improper
	{
		ttype_improper(int type)
			: type(type)
		{}

		int type;
		int durations[ONE_DAY_HOURS];
		SDL_DColor color;
		tsdl_field label;
	};
	std::vector<ttype_improper> type_impropers;

	int improper_alerts[ONE_DAY_HOURS];

	SDL_Rect* pl_btn_rects;
};

#define VALIDATE_POSTURE_DAYS(days)	VALIDATE((days) >= 2 && (days) <= MAX_HEALTH_DAYS, null_str)

struct tdays_posture_fields
{
public:
	enum {fid_title, fid_left_y_axis, fid_right_y_axis, fid_legend_sit_duration, fid_legend_improper_duration, fid_legend_improper_alert,
		fid_this_days, fid_count};

	tdays_posture_fields()
		: Y_axis_label_width(33)
		, Y_axis_label_chart_gap(20)
		, legend_2legend_gap_y(5)
		, y_axis_title_gap_y(Y_AXIS_TITLE_GAP_Y)
	{
		memset(arrays, 0, sizeof(arrays));

		arrays[fid_title] = &title;
		arrays[fid_this_days] = &this_days;
		arrays[fid_left_y_axis] = &left_y_axis;
		arrays[fid_right_y_axis] = &right_y_axis;
		arrays[fid_legend_sit_duration] = &legend_sit_duration;
		arrays[fid_legend_improper_duration] = &legend_improper_duration;
		arrays[fid_legend_improper_alert] = &legend_improper_alert;

		for (int at = 0; at < fid_count; at ++) {
			VALIDATE(arrays[at] != nullptr, null_str);
		}
	}

	void clear()
	{
		bar_4labels.clear();
	}

public:
	const int Y_axis_label_width;
	const int Y_axis_label_chart_gap;
	int title_height;
	int legend_height;
	const int legend_2legend_gap_y;
	const int y_axis_title_gap_y;
	int day_labels_height;

	tsdl_field title;
	tsdl_field this_days;
	tsdl_field left_y_axis;
	tsdl_field right_y_axis;
	tsdl_field legend_sit_duration;
	tsdl_field legend_improper_duration;
	tsdl_field legend_improper_alert;
	tsdl_field* arrays[fid_count];

	int sit_durations[MAX_HEALTH_DAYS];
	int improper_durations[MAX_HEALTH_DAYS];
	int improper_alerts[MAX_HEALTH_DAYS];

	struct tbar_4label
	{
		tbar_4label()
		{}

		tsdl_field sit;
		tsdl_field improper;
		tsdl_field improper_alert;
		tsdl_field day;
	};
	std::vector<tbar_4label> bar_4labels;
};

enum {coltype_voice_state, coltype_pose_state, coltype_rep, coltype_seg, coltype_count};

#define misc_btn_open_url	pl_btn_stop

struct tworkout_fields
{
public:
	enum {fid_title, fid_this_days, fid_history, fid_left_y_axis, fid_right_y_axis, 
		fid_legend_nonpose_state_duration, fid_legend_pose_state_duration, 
		fid_legend_satisfied_duration, fid_legend_unsatisfied_duration, fid_legend_improper_alert,
		fid_legend_seg_duration, fid_legend_rep_duration, fid_legend_active_period, fid_legend_cooldown_period,
		fid_unsatisfied_msg, fid_chart_remark, fid_count};

	tworkout_fields(const SDL_Range& range_ms)
		: Y_axis_label_width(33)
		, Y_axis_label_chart_gap(20)
		, legend_2legend_gap_y(5)
		, y_axis_title_gap_y(Y_AXIS_TITLE_GAP_Y)
		, unsatisfied_msg_to_chart_remark_gap_y(5)
		, share(bool_set_none)
		, range_ms(range_ms)
		, cairo_draw_start_btn(false)
		, cols(nullptr)
		, col_count(0)
		, has_seg(false)
		, has_rep(false)
		, has_pose_state(false)
		, flow_state_count(0)
		, pl_btn_rects(nullptr)
		, tip_rects(nullptr)
	{
		memset(arrays, 0, sizeof(arrays));

		arrays[fid_title] = &title;
		arrays[fid_this_days] = &this_days;
		arrays[fid_history] = &history;
		arrays[fid_left_y_axis] = &left_y_axis;
		arrays[fid_right_y_axis] = &right_y_axis;
		arrays[fid_legend_nonpose_state_duration] = &legend_nonpose_state_duration;
		arrays[fid_legend_pose_state_duration] = &legend_pose_state_duration;
		arrays[fid_legend_satisfied_duration] = &legend_satisfied_duration;
		arrays[fid_legend_unsatisfied_duration] = &legend_unsatisfied_duration;
		arrays[fid_legend_improper_alert] = &legend_improper_alert;
		arrays[fid_legend_seg_duration] = &legend_seg_duration;
		arrays[fid_legend_rep_duration] = &legend_rep_duration;
		arrays[fid_legend_active_period] = &legend_active_period;
		arrays[fid_legend_cooldown_period] = &legend_cooldown_period;
		arrays[fid_unsatisfied_msg] = &unsatisfied_msg;
		arrays[fid_chart_remark] = &chart_remark;

		for (int at = 0; at < fid_count; at ++) {
			VALIDATE(arrays[at] != nullptr, null_str);
		}
	}

	~tworkout_fields()
	{
		clear();
	}

	posix_noncopyable(tworkout_fields);

	void clear()
	{
		if (cols != nullptr) {
			VALIDATE(col_count != 0, null_str);
			free(cols);
			cols = nullptr;
			col_count = 0;

		} else {
			VALIDATE(col_count == 0, null_str);
		}
		bar_4labels.clear();

		has_seg = false;
		has_rep = false;
		has_pose_state = false;
		flow_state_count = 0;

		pl_btn_rects = nullptr;
		tip_rects = nullptr;
	}

public:
	const int Y_axis_label_width;
	const int Y_axis_label_chart_gap;
	int title_height;
	int legend_height;
	const int legend_2legend_gap_y;
	const int y_axis_title_gap_y;
	const int unsatisfied_msg_to_chart_remark_gap_y;
	int day_labels_height;
	bool_set_t share;
	const SDL_Range range_ms;

	tsdl_field title;
	tsdl_field this_days;
	tsdl_field history;
	tsdl_field left_y_axis;
	tsdl_field right_y_axis;
	tsdl_field legend_nonpose_state_duration;
	tsdl_field legend_pose_state_duration;
	tsdl_field legend_satisfied_duration;
	tsdl_field legend_unsatisfied_duration;
	tsdl_field legend_improper_alert;
	tsdl_field legend_seg_duration;
	tsdl_field legend_rep_duration;
	tsdl_field legend_active_period;
	tsdl_field legend_cooldown_period;
	tsdl_field unsatisfied_msg;
	tsdl_field chart_remark;
	tsdl_field* arrays[fid_count];
	bool cairo_draw_start_btn;

	struct tcol2_C
	{
		int type;
		// int state_start_ms;
		int state_duration_ms;
		int satisfied_duration_ms;
		int unsatisfied_duration_ms;
		SDL_Point improper_alert;

		int rep_duration_ms;
		int phase2_duration_ms;
	};
	tcol2_C* cols;
	int col_count;

	struct tbar_4label
	{
		tbar_4label()
		{}

		tsdl_field state;
		tsdl_field satisfied;
		tsdl_field unsatisfied;
		tsdl_field improper_alert;
		tsdl_field rep_duration;
		tsdl_field day;
	};
	std::vector<tbar_4label> bar_4labels;

	bool has_seg;
	bool has_rep;
	// Although it is a time_counter or rep_counter state, 
	// the user ended it too early, resulting in it not generating even one 'seg_count' or 'rep_count'.
	// add 'has_pose_state', make both 'legend_pose_state_duration' and 'legend_improper_alert' are added.
	bool has_pose_state;

	aplt::tflow_state_C flow_states[WORKOUT_MAX_FLOW_STATES];
	int flow_state_count;

	SDL_Rect* pl_btn_rects;
	SDL_Rect* tip_rects;
};

struct tdays_workout_fields
{
public:
	enum {fid_title, fid_this_days, fid_left_y_axis, fid_right_y_axis, 
		fid_legend_workout_duration, fid_legend_improper_alert, fid_legend_share_alert, fid_chart_remark, fid_count};

	tdays_workout_fields(bool is_sharing)
		: is_sharing(is_sharing)
		, Y_axis_label_width(33)
		, Y_axis_label_chart_gap(20)
		, legend_2legend_gap_y(5)
		, y_axis_title_gap_y(Y_AXIS_TITLE_GAP_Y)
		, share(bool_set_none)
		, pl_btn_rects(nullptr)
		, tip_rects(nullptr)
	{
		memset(arrays, 0, sizeof(arrays));

		arrays[fid_title] = &title;
		arrays[fid_this_days] = &this_days;
		arrays[fid_left_y_axis] = &left_y_axis;
		arrays[fid_right_y_axis] = &right_y_axis;
		arrays[fid_legend_workout_duration] = &legend_workout_duration;
		arrays[fid_legend_improper_alert] = &legend_improper_alert;
		arrays[fid_legend_share_alert] = &legend_share_alert;
		arrays[fid_chart_remark] = &chart_remark;

		for (int at = 0; at < fid_count; at ++) {
			VALIDATE(arrays[at] != nullptr, null_str);
		}
	}

	void clear()
	{
		bar_4labels.clear();
	}

public:
	const bool is_sharing;
	const int Y_axis_label_width;
	const int Y_axis_label_chart_gap;
	int title_height;
	int legend_height;
	const int legend_2legend_gap_y;
	const int y_axis_title_gap_y;
	int day_labels_height;
	bool_set_t share;

	tsdl_field title;
	tsdl_field this_days;
	tsdl_field left_y_axis;
	tsdl_field right_y_axis;
	tsdl_field legend_workout_duration;
	// tsdl_field legend_improper_duration;
	tsdl_field legend_improper_alert;
	tsdl_field legend_share_alert;
	tsdl_field chart_remark;
	tsdl_field* arrays[fid_count];

	int sit_durations[MAX_HEALTH_DAYS];
	// int improper_durations[MAX_HEALTH_DAYS];
	int improper_alerts[MAX_HEALTH_DAYS];
	int share_alerts[MAX_HEALTH_DAYS];

	struct tbar_4label
	{
		tbar_4label()
		{}

		tsdl_field sit;
		// tsdl_field improper;
		tsdl_field improper_alert;
		tsdl_field share_alert;
		tsdl_field day;
	};
	std::vector<tbar_4label> bar_4labels;

	SDL_Rect* pl_btn_rects;
	SDL_Rect* tip_rects;
};

struct tdays_course_summary_fields
{
public:
	enum {fid_title, fid_this_days, fid_left_y_axis, fid_right_y_axis, 
		fid_legend_workout_duration, fid_legend_plan_alert, fid_legend_actual_alert, 
		fid_legend_full_wko_id, fid_legend_incomplete_wko_id, fid_legend_pending_wko_id, fid_chart_remark, fid_count};

	tdays_course_summary_fields()
		: Y_axis_label_width(33)
		, Y_axis_label_chart_gap(20)
		, legend_2legend_gap_y(5)
		, y_axis_title_gap_y(Y_AXIS_TITLE_GAP_Y)
		, incomplete_bg_color{100.0 / 255.0, 100.0 / 255.0, 100.0 / 255.0, 1.0}
		, share(bool_set_none)
		, pl_btn_rects(nullptr)
		, tip_rects(nullptr)
	{
		memset(arrays, 0, sizeof(arrays));

		arrays[fid_title] = &title;
		arrays[fid_this_days] = &this_days;
		arrays[fid_left_y_axis] = &left_y_axis;
		arrays[fid_right_y_axis] = &right_y_axis;
		arrays[fid_legend_workout_duration] = &legend_workout_duration;
		arrays[fid_legend_plan_alert] = &legend_plan_alert;
		arrays[fid_legend_actual_alert] = &legend_actual_alert;
		arrays[fid_legend_full_wko_id] = &legend_full_wko_id;
		arrays[fid_legend_incomplete_wko_id] = &legend_incomplete_wko_id;
		arrays[fid_legend_pending_wko_id] = &legend_pending_wko_id;
		arrays[fid_chart_remark] = &chart_remark;

		for (int at = 0; at < fid_count; at ++) {
			VALIDATE(arrays[at] != nullptr, null_str);
		}
	}

	void clear()
	{
		bar_4labels.clear();
	}

	int get_legend_height_or_draw(cairo_t* cr, int width, const SDL_Point& margin);

public:
	const int Y_axis_label_width;
	const int Y_axis_label_chart_gap;
	int title_height;
	int legend_height;
	const int legend_2legend_gap_y;
	const int y_axis_title_gap_y;
	int day_labels_height;
	const SDL_DColor incomplete_bg_color;
	bool_set_t share;

	tsdl_field title;
	tsdl_field this_days;
	tsdl_field left_y_axis;
	tsdl_field right_y_axis;
	tsdl_field legend_workout_duration;
	// tsdl_field legend_improper_duration;
	tsdl_field legend_plan_alert;
	tsdl_field legend_actual_alert;
	tsdl_field legend_full_wko_id;
	tsdl_field legend_incomplete_wko_id;
	tsdl_field legend_pending_wko_id;
	tsdl_field chart_remark;
	tsdl_field* arrays[fid_count];

	int actual_workout_durations_sec[MAX_HEALTH_DAYS];

	int plan_workouts[MAX_HEALTH_DAYS];
	// actual_workouts.x: finished
	// actual_workouts.y: not finish
	SDL_Point actual_workouts[MAX_HEALTH_DAYS];

	struct tworkout_id
	{
		tworkout_id(const std::string& id)
			: id(id)
			, plan_workouts(0)
			, actual_workouts({0, 0})
		{}

		std::string id;
		int plan_workouts;
		SDL_Point actual_workouts;
		SDL_DColor color;
		tsdl_field label;
	};

	struct tbar_4label
	{
		tbar_4label()
		{}

		std::vector<tworkout_id> plan_workout_ids;
		std::vector<tworkout_id> actual_workout_ids;

		tsdl_field sit;
		// tsdl_field improper;
		tsdl_field plan_workout;
		tsdl_field actual_workout;
		tsdl_field day;
	};
	std::vector<tbar_4label> bar_4labels;

	struct tlegend_workout_id
	{
		tlegend_workout_id(const std::string& id2, const SDL_DColor& color)
			: id2(id2)
			, color(color)
		{}

		std::string id2;
		SDL_DColor color;
		tsdl_field label;
	};
	std::vector<tlegend_workout_id> legend_workout_ids;

	SDL_Rect* pl_btn_rects;
	SDL_Rect* tip_rects;
};

struct tlandmark_fields
{
public:
	enum {fid_phase0, fid_phase1, fid_legend_left, fid_legend_right, 
		fid_open_image, fid_fake_landmark, fid_sdl_lmk33_mode, fid_btn_clear, fid_count};

	tlandmark_fields(int small_font_size, int lmk33_mode, bool is_player, const bool* _sel_landmarks, bool use_surf_landmarks);

	void clear()
	{
		for (int at = 0; at < mediapipe::kNumPoseLandmarks; at ++) {
			SDL_Point& point = points[at];
			point.x = nposm;
			point.y = nposm;
		}
		// btn_clear_rect = empty_rect;
	}

	void pre_fill_sdl_fields();
	void post_render_sdl_fields(cv::Mat& mat);

public:
	const int small_font_size;
	const int lmk33_mode;
	const bool is_player;
	const bool* sel_landmarks;
	int sel_count;
	const bool use_surf_landmarks;
	const int legend_to_right_gap_x;
	const int legend_2legend_gap_y;
	const int legend_to_top_gap_y;
	SDL_DColor nose_color;
	bool_set_t share;
	std::map<int, std::string> lmk33_modes;

	tsdl_field phase0;
	tsdl_field phase1;
	tsdl_field legend_left;
	tsdl_field legend_right;
	tsdl_field open_image;
	tsdl_field fake_landmark;
	tsdl_field sdl_lmk33_mode;
	tsdl_field btn_clear;
	tsdl_field* arrays[fid_count];

	SDL_Point points[mediapipe::kNumPoseLandmarks];
	
	// pose_state2 special
	int phase_count;
	int phase_sel;
};

struct ttip_fields
{
public:
	enum {fid_header, fid_left_label, fid_right_label, fid_count};

	ttip_fields(int small_font_size)
		: small_font_size(small_font_size)
		, hdr_label_gap_y(4)
		, lr_gap_x(0)
	{
		clear();

		memset(arrays, 0, sizeof(arrays));

		arrays[fid_header] = &header;
		arrays[fid_left_label] = &left_label;
		arrays[fid_right_label] = &right_label;
		for (int at = 0; at < fid_count; at ++) {
			VALIDATE(arrays[at] != nullptr, null_str);
		}
	}

	void clear()
	{
	}

public:
	const int small_font_size;
	const int hdr_label_gap_y;
	int lr_gap_x;

	tsdl_field header;
	tsdl_field left_label;
	tsdl_field right_label;
	tsdl_field* arrays[fid_count];

	// SDL_Point points[mediapipe::kNumPoseLandmarks];
	
	// pose_state2 special
	// int phase_count;
	// int phase_sel;
};

struct tmultcol_fields
{
public:
	tmultcol_fields(int mat_type, int max_width)
		: mat_type(mat_type)
		, max_width(max_width)
	{
		VALIDATE(mat_type >= 0 && mat_type < multcolmattype_count, null_str);
	}

public:
	const int mat_type;
	const int max_width;

	struct tcol
	{
		tcol()
		{}

		tsdl_field line1;
		tsdl_field line2;
	};
	std::vector<tcol> cols;
};

cv::Mat draw_header_mat(bool to_image, int width, int height, double radius, const SDL_Point& margin, theader_fields& fields);
cv::Mat draw_sit_dual_axis_stacked_bar_chart(bool to_image, int width, int height, double radius, const SDL_Point& margin,
	tsit_fields& fields);
cv::Mat draw_days_posture_dual_axis_stacked_bar_chart(bool to_image, int width, int height, double radius, const SDL_Point& margin,
	int days, tdays_posture_fields& fields);
cv::Mat draw_workout_bar_chart(int mat_type, int width, int height, double radius, const SDL_Point& margin,
	tworkout_fields& fields);

cv::Mat draw_landmarks_mat(bool to_image, int width, int height, double radius, const SDL_Point& margin,
	const SDL_Rect& video_clip, const SDL_FPoint* landmarks, int count, int played_ms, int duration_ms, int playstyle,
	SDL_Rect* btn_rects, int btn_count, tlandmark_fields& fields);

cv::Mat draw_days_workout_dual_axis_stacked_bar_chart(bool to_image, int width, int height, double radius, const SDL_Point& margin,
	int days, tdays_workout_fields& fields);

cv::Mat draw_days_course_summary_mat(bool to_image, int width, int height, double radius, const SDL_Point& margin,
	int days, tdays_course_summary_fields& fields);

cv::Mat draw_tip_mat(bool to_image, int width, int height, double radius, const SDL_Point& margin,
	int shadow_blur_radius, ttip_fields& fields);

cv::Mat draw_multcol_mat(double radius, const SDL_DColor& fill_color, const SDL_Point& margin, tmultcol_fields& fields);

}

#endif

