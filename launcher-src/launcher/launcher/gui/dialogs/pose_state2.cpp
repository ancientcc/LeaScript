#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/pose_state2.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/menu.hpp"
#include "gettext.hpp"
#include "font.hpp"
#include "rose_mediapipe_api.hpp"
#include "cairo2.hpp"
#include <angles/angles.h>
#include <opencv2/imgproc.hpp>

using namespace std::placeholders;

extern std::string handle_browse_file(bool read_only, const std::string& _title, const std::string& ext_name);

#define pose_metric_is_angle3p(metric)	\
	((metric) == aplt::posemetric_left_elbow_angle || (metric) == aplt::posemetric_right_elbow_angle ||	\
	(metric) == aplt::posemetric_left_knee_angle || (metric) == aplt::posemetric_right_knee_angle || \
	(metric) == aplt::posemetric_right_hip_angle)

#define pose_metric_is_angle2p(metric)	\
	((metric) == aplt::posemetric_body_tilt_angle || (metric) == aplt::posemetric_header_tilt_angle ||	\
	(metric) == aplt::posemetric_left_upper_arm_angle || (metric) == aplt::posemetric_right_upper_arm_angle ||	\
	(metric) == aplt::posemetric_left_forearm_angle || (metric) == aplt::posemetric_right_forearm_angle ||	\
	(metric) == aplt::posemetric_left_thigh_angle || (metric) == aplt::posemetric_right_thigh_angle ||	\
	(metric) == aplt::posemetric_left_lower_leg_angle || (metric) == aplt::posemetric_right_lower_leg_angle ||	\
	(metric) == aplt::posemetric_left_leg_angle || (metric) == aplt::posemetric_right_leg_angle)

#define pose_metric_keep_range(metric) \
	((metric) == aplt::posemetric_left_wrist_on_right_shoulder || (metric) == aplt::posemetric_right_wrist_on_left_shoulder)

extern surface save_lmk33_png_from_surf(const surface& src_surf, aplt::twkoscript::tstate2& state2, const std::string& phase_surf_dir, int phase_at);

aplt::twkoscript::tpose& insert_preset_pose(const aplt::tpreset_pose& preset,
	const SDL_FPoint* landmarks, bool landmarks_valid, aplt::twkoscript::tstate2& state2)
{
	state2.track_pose.poses.push_back(preset);
	aplt::twkoscript::tpose& new_pose = state2.track_pose.poses.back();

	if (landmarks_valid) {
		SDL_DPoint result = preset.calc_result_may_invalid(landmarks);
		VALIDATE(!is_float_nposm(result.x), null_str);

		// SDL_Log("{dbg-preset}metric: %i, result.y: %.3f", preset.metric, result.y);
		SDL_DRange range{float_nposm, float_nposm};
		if (pose_metric_keep_range(preset.metric)) {
			range = preset.range;

		} else if (!is_float_nposm(preset.tolerance)) {
			range.min = result.y - preset.tolerance;
			range.max = result.y + preset.tolerance;
			if (pose_metric_is_angle3p(preset.metric)) {
				range.min = (int)range.min;
				range.max = (int)range.max;

				if (range.min < 1.1) {
					range.min = float_nposm;
				}
				if (range.max > 178.9) {
					range.max = float_nposm;
				}

			} else if (pose_metric_is_angle2p(preset.metric)) {
				double degree = result.y;
				if (result.y < 45.0 || result.y > 315) {
					// use [-M_PI, +M_PI]
					double rad = angles::normalize_angle(DEG2RAD(result.y));
					degree = RAD2DEG(rad);
				}
				range.min = (int)(degree - preset.tolerance);
				range.max = (int)(degree + preset.tolerance);
			}
		}
		if (!is_float_nposm(range.min) || !is_float_nposm(range.max)) {
			new_pose.range = range;
		} else {
			int ii = 0;
		}
	} else {
		int ii = 0;
	}

	return new_pose;
}

extern bool calc_surf_landmarks(mediapipe::tpose_tracking_api& api, const surface& surf, SDL_FPoint* landmarks);

namespace gui2 {

REGISTER_DIALOG(launcher, pose_state2)

tpose_state2::tpose_state2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, aplt::twkoscript::tstate2& state2, std::vector<std::string>& state_names, 
	const std::map<int, aplt::tpreset_pose>& preset_poses, int sdl_field_small_font_size, const std::string& phase_surf_dir)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, twko_state2(state2.state, state_names)
	, original_state2_(state2)
	, state2_(state2)
	// , state_name_(state_name)
	, preset_poses_(preset_poses)
	, action_tpl2s_(aplt::action_tpl2s)
	, sdl_field_small_font_size_(sdl_field_small_font_size)
	, phase_surf_dir_(phase_surf_dir)
	, pose_report_name_max_chars_(10)
	, time_rule_descs_({
		{aplt::twkoscript::timerule_total, _("desc^timerule_total")},
		{aplt::twkoscript::timerule_satisfied, _("desc^timerule_satisfied")},
		{aplt::twkoscript::timerule_strict, _("desc^timerule_strict")},
	})
	, time_tone_descs_({
		{aplt::twkoscript::timetone_full, _("desc^timetone_full")},
		{aplt::twkoscript::timetone_split, _("desc^timetone_split")},
		{aplt::twkoscript::timetone_silent, _("desc^timetone_silent")},
	})
	// , mediapipe_api_(mediapipe_api)
	, state2_stack_(nullptr)
	, unsatisfied_remark_widget_(nullptr)
	, task_type_report_(nullptr)
	, task_stack_(nullptr)
	, time_rule_report_(nullptr)
	, time_tone_report_(nullptr)
	, satisfied_remark_widget_(nullptr)	
	, task_remark_widget_(nullptr)
	, landmark_track_(nullptr)
	, pose_type_report_(nullptr)
	, pose_report_(nullptr)
	, phase_mask_widget_(nullptr)
	, operand_list_(nullptr)
	, abs_diff_widget_(nullptr)
	, min_widget_(nullptr)
	, max_widget_(nullptr)
	, move_left_widget_(nullptr)
	, move_right_widget_(nullptr)
	, status_widget_(nullptr)
	// state2_base layer
	, curr_state2_layer_(nposm)
	// state2_pose layer
	, curr_pose_at_(nposm)
	, lmk33_mode_(lmkmode_both)
	, interest_hiting_({nposm, nposm})
	, pose_type2s_({
		{posetype2_angle3p, tcode3(posetype2_angle3p, "angle3p(180)", _("desc^angle3p(180)"))},
		{posetype2_angle3p_360, tcode3(posetype2_angle3p_360, "angle3p(360)", _("desc^angle3p(360)"))},
		{posetype2_angle2p, tcode3(posetype2_angle2p, "angle2p", _("desc^angle2p"))},
		{posetype2_diff_point, tcode3(posetype2_diff_point, "diff(point)", _("desc^diff(point)"))},
		{posetype2_diff_x, tcode3(posetype2_diff_x, "diff(x)", _("desc^diff(x)"))},
		{posetype2_diff_y, tcode3(posetype2_diff_y, "diff(y)", _("desc^diff(y)"))},
		{posetype2_diff_dist_x, tcode3(posetype2_diff_dist_x, "diff(dist_x)", _("desc^diff(dist_x)"))},
		{posetype2_diff_dist_y, tcode3(posetype2_diff_dist_y, "diff(dist_y)", _("desc^diff(dist_y)"))},
	})
	, curr_pose_type2_(nposm)
	, curr_phase_at_(0)
	, surf_landmarks_valid_(false)
	, phase_surf_updated_(false)
{
	VALIDATE(time_rule_descs_.size() == aplt::wko_time_rules.size(), null_str);
	VALIDATE(time_tone_descs_.size() == aplt::wko_time_tones.size(), null_str);

	set_timer_interval(1000);
	clear_rects();

	memset(surf_landmarks_, 0, sizeof(surf_landmarks_));

	api_ptr_.reset(mediapipe::rose_create_pose_tracking_api());
	bool retbool = api_ptr_->graph_initialized();
	VALIDATE(retbool, null_str);

	VALIDATE(curr_phase_at_ == 0, null_str);
	reload_curr_phase_surf(false);
}

void tpose_state2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());
	twko_state2::pre_show(*window_, find_widget<ttext_box>(window_, "state_name", false));

	// find_widget<tlabel>(window_, "title", false).set_label(state_name_);

	tbutton* button = find_widget<tbutton>(window_, "back", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tpose_state2::click_back
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(window_, "enter_pose", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tpose_state2::state2_stack_set_radio_layer
			, this
			, STATE2_POSE_LAYER));

	button = find_widget<tbutton>(window_, "exit_pose", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tpose_state2::state2_stack_set_radio_layer
			, this
			, STATE2_BASE_LAYER));

	tstack* stack = find_widget<tstack>(window_, "state2_stack", false, true);
	pre_state2_base(*stack->layer(STATE2_BASE_LAYER));
	pre_state2_pose(*stack->layer(STATE2_POSE_LAYER));
	state2_stack_ = stack;

	status_widget_ = find_widget<tlabel>(window_, "status", false, true);
	if (game_config::os == os_windows) {
		set_status_label(_("ime warning"));
	}

	// 
	state2_stack_set_radio_layer(STATE2_BASE_LAYER);

	task_type_report_->select_item(state2_.task->type);
	task_stack_->set_radio_layer(task_type_to_task_layer(state2_.task->type));

	if (pose_report_->items() != 0) {
		pose_report_->select_item(0);
	}
}

void tpose_state2::post_show()
{
	state2_.green();

	api_ptr_.reset();
}

void tpose_state2::app_resize_screen()
{
	clear_landmark_mat(false);
}

void xxx_state2_click_back(twko_state2& wko_state2, aplt::twkoscript::tstate2& state2, gui2::twindow& window)
{
	std::string err_msg = wko_state2.can_update();
	if (err_msg.empty()) {
		uint64_t res = state2.is_valid2(0, err_msg);
		if (res != TCOOKIE3F_CHECK_OK) {
			err_msg = aplt::twkoscript::fomrat_is_valid2_result(res, err_msg);
		}
	}

	int retval = twindow::OK;
	if (!err_msg.empty()) {
		std::stringstream err;
		err << err_msg;

		err << "\n\n";
		err << _("There is a data error, and cannot save. Do you want to keep exiting without saving?");
		if (gui2::show_message2(null_str, err.str(), gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}
		retval = twindow::CANCEL;

	} else {
		wko_state2.do_update();
	}

	window.set_retval(retval);
}

void tpose_state2::click_back(tbutton& widget)
{
	xxx_state2_click_back(*this, state2_, *window_);
/*
	int retval = twindow::OK;
	std::string err_msg;
	uint64_t res = state2_.is_valid2(err_msg);
	if (res != TCOOKIE3F_CHECK_OK) {
		tcookie3f cookie3f(res);
		
		std::stringstream err;
		if (err_msg.empty()) {
			err << state2_.get_error_msg(cookie3f);
		} else {
			err << err_msg;
		}

		if (cookie3f.type == aplt::twkoscript::typeid_pose) {
			utils::string_map symbols;
			symbols["number"] = str_cast(cookie3f.index + 1);
			symbols["msg"] = err.str();
			err.str("");
			err << vgettext2("In $number pose, $msg", symbols);
		}

		err << "\n\n";
		err << _("There is a data error, and cannot save. Do you want to keep exiting without saving?");
		if (gui2::show_message2(null_str, err.str(), gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}
		retval = twindow::CANCEL;
	}

	window_->set_retval(retval);
*/
}

void tpose_state2::state2_stack_set_radio_layer(int layer)
{
	VALIDATE(layer >= 0 && layer < STATE2_LAYER_COUNT, null_str);

	curr_state2_layer_ = layer;
	state2_stack_->set_radio_layer(layer);
}

void tpose_state2::pre_state2_base(tgrid& grid)
{
	ttoggle_button* toggle = find_widget<ttoggle_button>(&grid, "is_setup", false, true);
	toggle->set_label(dgettext("rose-lib", "wko^is_setup label"));
	toggle->set_value(state2_.is_setup);
	if (state2_.task->type == aplt::twkoscript::tasktype_time_counter) {
		toggle->set_did_state_changed(std::bind(&tpose_state2::did_state2_bool_field_changed, this, _1,
			aplt::twkoscript::fid_is_setup));
	} else {
		toggle->set_visible(twidget::INVISIBLE);
	}

	toggle = find_widget<ttoggle_button>(&grid, "debug_skip", false, true);
	toggle->set_label(dgettext("rose-lib", "wko^debug_skip label"));
	toggle->set_value(state2_.debug_skip);
	toggle->set_did_state_changed(std::bind(&tpose_state2::did_state2_bool_field_changed, this, _1,
		aplt::twkoscript::fid_debug_skip));

	tbutton* button = find_widget<tbutton>(&grid, "action_tpl2_id", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tpose_state2::click_action_tpl2_id
			, this
			, std::ref(*button)));
	if (!state2_.action_tpl2_id.empty()) {
		VALIDATE(action_tpl2s_.count(state2_.action_tpl2_id) != 0, null_str);
		button->set_label(action_tpl2s_.find(state2_.action_tpl2_id)->second.name2());
	}

	//
	// base
	//
	if (state2_.task->type == aplt::twkoscript::tasktype_rep_counter) {
		tlabel* label = find_widget<tlabel>(&grid, "unsatisfied_threshold_ms_label", false, true);
		label->set_label(_("rep_counter, unsatisfied_threshold_ms label"));
	}

	ttext_box* text_box = find_widget<ttext_box>(&grid, "unsatisfied_threshold_ms", false, true);
	text_box->set_label(str_cast(state2_.unsatisfied_threshold_ms));
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_global, aplt::twkoscript::fid_unsatisfied_threshold_ms);

	text_box = find_widget<ttext_box>(&grid, "unsatisfied_2th_threshold_s", false, true);
	text_box->set_label(str_cast(state2_.unsatisfied_2th_threshold_s));
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_global, aplt::twkoscript::fid_unsatisfied_2th_threshold_s);

	text_box = find_widget<ttext_box>(&grid, "unsatisfied_2th_msgstr", false, true);
	text_box->set_label(state2_.unsatisfied_2th_msgstr);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_global, aplt::twkoscript::fid_unsatisfied_2th_msgstr);

	if (state2_.task->type == aplt::twkoscript::tasktype_rep_counter) {
		find_widget<tlabel>(&grid, "unsatisfied_2th_msgstr_label", false, true)->set_visible(twidget::INVISIBLE);
		text_box->set_visible(twidget::INVISIBLE);
	}
	

	tlabel* label = find_widget<tlabel>(window_, "unsatisfied_remark", false, true);
	unsatisfied_remark_widget_ = label;
	update_unsatisfied_remark_label();

	//
	// task
	//
	utils::string_map symbols;
	symbols["type"] = aplt::wko_task_types.find(state2_.task->type)->second.name;
	find_widget<tlabel>(&grid, "task_type_label", false, true)->set_label(vgettext2("$type task", symbols));

	treport* report = find_widget<treport>(&grid, "task_type_report", false, true);
	// std::map<int, tcode3> wko_task_types
	for (std::map<int, tcode3>::const_iterator it = aplt::wko_task_types.begin(); it != aplt::wko_task_types.end(); ++ it) {
		const tcode3& type = it->second;
		report->insert_item(null_str, type.name).set_cookie(type.code);
	}
	report->set_did_item_changed(std::bind(&tpose_state2::did_task_type_report_item_changed, this, std::ref(grid), _2));
	// report->select_item(state2_.task->type);
	task_type_report_ = report;
	task_type_report_->set_visible(twidget::INVISIBLE);

	tstack* stack = find_widget<tstack>(&grid, "task_stack", false, true);
	if (state2_.task->type == aplt::twkoscript::tasktype_time_counter) {
		pre_task_time_counter(*stack->layer(TASK_TIME_COUNTER_LAYER));

	} else if (state2_.task->type == aplt::twkoscript::tasktype_rep_counter) {
		pre_task_rep_counter(*stack->layer(TASK_REP_COUNTER_LAYER));

	} else {
		VALIDATE(false, null_str);
	}
	task_stack_ = stack;

	update_task_remark_label();
}

void tpose_state2::pre_state2_pose(tgrid& grid)
{
	const aplt::twkoscript::ttrack_pose& track_pose = state2_.track_pose;
	VALIDATE(!track_pose.poses.empty(), null_str);

	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(&grid, "track_pose_remark", false, true);
	scroll_text_box->set_label(_("track_pose remark"));
	// scroll_text_box->set_active(false);
	scroll_text_box->tb()->set_active(false);
	scroll_text_box->tb()->set_default_color(0xff7b7b7b);
	scroll_text_box->tb()->set_text_font_size(font::SIZE_SMALL);

	scroll_text_box->set_border(null_str);

	//
	// landmark track
	//
	ttrack* track = find_widget<ttrack>(&grid, "landmark_track", false, true);
	track->disable_rose_draw_bg();
	track->set_did_draw(std::bind(&tpose_state2::did_draw_landmark, this, _1, _2, _3));
	track->set_did_mouse_motion(std::bind(&tpose_state2::did_mouse_motion_landmark, this, _1, _2, _3));
	track->set_did_left_button_down(std::bind(&tpose_state2::did_left_button_down_landmark, this, _1, _2));
	track->set_did_mouse_leave(std::bind(&tpose_state2::did_mouse_leave_landmark, this, _1, _2, _3));
	landmark_track_ = track;

	//
	// poses
	//
	treport* report = find_widget<treport>(&grid, "pose_report", false, true);

	int pose_at = 0;
	for (std::vector<aplt::twkoscript::tpose>::const_iterator it = track_pose.poses.begin(); it != track_pose.poses.end(); ++ it, pose_at ++) {
		const aplt::twkoscript::tpose& pose = *it;
		report->insert_item(null_str, generate_pose_name_form_report(pose, pose_at, track_pose.poses.size()));
	}
	report->set_did_item_changed(std::bind(&tpose_state2::did_pose_report_item_changed, this, std::ref(grid), _2));
	pose_report_ = report;

	tbutton* button = find_widget<tbutton>(&grid, "phase_mask", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tpose_state2::click_phase_mask
			, this
			, std::ref(*button)));
	if (!wko_allow_multiple_phase_from_task_type(state2_.task->type)) {
		find_widget<tgrid>(&grid, "phase_mask_grid", false, true)->set_visible(twidget::INVISIBLE);
	}
	phase_mask_widget_ = button;

	button = find_widget<tbutton>(&grid, "insert", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tpose_state2::click_insert_pose
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(&grid, "move_left", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tpose_state2::click_move_left_or_right
			, this
			, std::ref(*button), true));
	move_left_widget_ = button;

	button = find_widget<tbutton>(&grid, "move_right", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tpose_state2::click_move_left_or_right
			, this
			, std::ref(*button), false));
	move_right_widget_ = button;

	button = find_widget<tbutton>(&grid, "erase_pose", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tpose_state2::click_erase_pose
			, this
			, std::ref(*button)));

	ttext_box* text_box = find_widget<ttext_box>(&grid, "pose_name", false, true);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_pose, aplt::twkoscript::fid_pose_name);

	report = find_widget<treport>(&grid, "pose_type_report", false, true);
	for (std::map<int, tcode3>::const_iterator it = pose_type2s_.begin(); it != pose_type2s_.end(); ++ it) {
		const tcode3& type = it->second;
		report->insert_item(null_str, type.id).set_cookie(type.code);
	}
	report->set_did_item_changed(std::bind(&tpose_state2::did_pose_type_report_item_changed, this, std::ref(grid), _2));
	// report->select_item(0);
	pose_type_report_ = report;

	// operand list
	tlistbox* list = find_widget<tlistbox>(&grid, "operand_list", false, true);
	// list->enable_select(false);
	list->set_did_row_pre_change(std::bind(&tpose_state2::did_operand_list_row_pre_change, this, _1, _2));
	list->set_did_row_changed(std::bind(&tpose_state2::did_operand_list_row_changed, this, _1, _2));
	operand_list_ = list;

	ttoggle_button* toggle = find_widget<ttoggle_button>(&grid, "abs_diff", false, true);
	toggle->set_did_state_changed(std::bind(&tpose_state2::did_abs_diff_changed, this, _1));
	abs_diff_widget_ = toggle;

	text_box = find_widget<ttext_box>(&grid, "pose_min", false, true);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_pose, aplt::twkoscript::fid_pose_min);
	min_widget_ = text_box;

	text_box = find_widget<ttext_box>(&grid, "pose_max", false, true);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_pose, aplt::twkoscript::fid_pose_max);
	max_widget_ = text_box;

	utils::string_map symbols;
	symbols["generic"] = state2_.get_field_str(aplt::twkoscript::typeid_pose, aplt::twkoscript::fid_pose_unsatisfied_msgstr);
	symbols["max"] = state2_.get_field_str(aplt::twkoscript::typeid_pose, aplt::twkoscript::fid_pose_unsatisfied_rmax_msgstr);
	std::string msg = vgettext2("pose_unsatisfied_msgstr remark, $generic, $max", symbols);
	find_widget<tlabel>(&grid, "pose_unsatisfied_msgstr_remark", false, true)->set_label(msg);

	text_box = find_widget<ttext_box>(&grid, "pose_unsatisfied_msgstr", false, true);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_pose, aplt::twkoscript::fid_pose_unsatisfied_msgstr);

	text_box = find_widget<ttext_box>(&grid, "pose_unsatisfied_rmax_msgstr", false, true);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_pose, aplt::twkoscript::fid_pose_unsatisfied_rmax_msgstr);
}

void tpose_state2::set_did_text_changed2(ttext_box& widget, int type, int fid, int index)
{
	std::string placeholder = state2_.get_placeholder_msg(type, fid);
	if (fid == aplt::twkoscript::fid_unsatisfied_2th_msgstr) {
		if (state2_.task->type == aplt::twkoscript::tasktype_rep_counter) {
			placeholder = _("rep_counter, unsatisfied_2th_msgstr placeholder");
		}

	} else if (fid == aplt::twkoscript::fid_pose_unsatisfied_msgstr || fid == aplt::twkoscript::fid_pose_unsatisfied_rmax_msgstr) {
		std::stringstream ss;
		ss << "(" << state2_.get_field_str(type, fid) << ")";
		ss << placeholder;
		placeholder = ss.str();
	}
	widget.set_placeholder(placeholder);
	widget.set_maximum_chars(64);
	widget.set_did_text_changed(std::bind(&tpose_state2::did_fid_text_changed, this, _1, index, fid));
}

void tpose_state2::did_fid_text_changed(ttext_box& widget, int index, int fid)
{
	VALIDATE(curr_pose_at_ >= 0 && curr_pose_at_ < (int)state2_.track_pose.poses.size(), null_str);

	aplt::twkoscript::ttime_counter* time_counter = nullptr;
	aplt::twkoscript::trep_counter* rep_counter = nullptr;
	if (state2_.task->type == aplt::twkoscript::tasktype_time_counter) {
		time_counter = static_cast<aplt::twkoscript::ttime_counter*>(state2_.task);
			
	} else if (state2_.task->type == aplt::twkoscript::tasktype_rep_counter) {
		rep_counter = static_cast<aplt::twkoscript::trep_counter*>(state2_.task);
	}

	const std::string& label = widget.label();
	aplt::twkoscript::tpose& pose = mutable_curr_pose();

	bool unsatisfied_remark_dirty = false;
	bool task_dirty = false;
	if (fid == aplt::twkoscript::fid_unsatisfied_threshold_ms) {
		int threshold_s = utils::to_int(label);
		if (state2_.unsatisfied_threshold_ms != threshold_s) {
			state2_.unsatisfied_threshold_ms = threshold_s;
			unsatisfied_remark_dirty = true;
		}

	} else if (fid == aplt::twkoscript::fid_unsatisfied_2th_threshold_s) {
		int threshold_s = utils::to_int(label);
		if (state2_.unsatisfied_2th_threshold_s != threshold_s) {
			state2_.unsatisfied_2th_threshold_s = threshold_s;
			unsatisfied_remark_dirty = true;
		}

	} else if (fid == aplt::twkoscript::fid_unsatisfied_2th_msgstr) {
		if (state2_.unsatisfied_2th_msgstr != label) {
			state2_.unsatisfied_2th_msgstr = label;
			unsatisfied_remark_dirty = true;
		}

	} else if (fid == aplt::twkoscript::fid_task_satisfied_threshold_s) {
		time_counter->satisfied_threshold_s = utils::to_int(label);

	} else if (fid == aplt::twkoscript::fid_task_satisfied_msgstr) {
		time_counter->satisfied_msgstr = label;

	} else if (fid == aplt::twkoscript::fid_time_counter_max_count) {
		int new_max_count = utils::to_int(label);
		int& max_count = time_counter != nullptr? time_counter->max_count: rep_counter->max_count;
		if (new_max_count != max_count) {
			max_count = new_max_count;
			task_dirty = true;
		}

	} else if (fid == aplt::twkoscript::fid_phase_action_msg) {
		VALIDATE(index >= 0 && index < 2, null_str);
		rep_counter->phases[index].action_msg = label;

	} else if (fid == aplt::twkoscript::fid_phase_min_duration_ms) {
		VALIDATE(index >= 0 && index < 2, null_str);
		rep_counter->phases[index].min_duration_ms = label.empty()? nposm: utils::to_int(label);

	} else if (fid == aplt::twkoscript::fid_phase_cooldowned_ms) {
		VALIDATE(index >= 0 && index < 2, null_str);
		rep_counter->phases[index].cooldowned_ms = label.empty()? nposm: utils::to_int(label);

	} else if (fid == aplt::twkoscript::fid_pose_name) {
		if (pose.name != label) {
			pose.name = label;
			// if (!label.empty()) {
				pose_report_->item(curr_pose_at_).set_label(generate_pose_name_form_report(pose, curr_pose_at_, state2_.track_pose.poses.size()));
			// }
		}

	} else if (fid == aplt::twkoscript::fid_pose_min) {
		pose.range.min = utils::to_double(label, float_nposm);

	} else if (fid == aplt::twkoscript::fid_pose_max) {
		pose.range.max = utils::to_double(label, float_nposm);

	} else if (fid == aplt::twkoscript::fid_pose_unsatisfied_msgstr) {
		pose.unsatisfied_msgstr = label;

	} else if (fid == aplt::twkoscript::fid_pose_unsatisfied_rmax_msgstr) {
		pose.unsatisfied_rmax_msgstr = label;

	} else {
		VALIDATE(false, null_str);
	}

	if (unsatisfied_remark_dirty) {
		update_unsatisfied_remark_label();
	}

	if (task_dirty) {
		update_task_remark_label();
	}
}

void tpose_state2::set_min_max_placeholder(int pose_type)
{
	std::string placeholder;

	// now ttext_box don't support change placeholder runtime.
	const bool dyn_change = false;
	if (dyn_change) {
		bool is_angle = wko_is_angle_from_pose_type(pose_type);
		if (is_angle) {
			placeholder	= _("Angle in degrees. Can be empty");
		} else {
			placeholder	= _("Length as a float, typically < 2.0. Can be empty");
		}
	} else {
		placeholder	= _("Angle in degrees, or length as a float (<2.0). Can be empty");
	}

	min_widget_->set_placeholder(placeholder);
	// min_widget_->set_dirty();
	max_widget_->set_placeholder(placeholder);
	// min_widget_->set_dirty();
}

//
// state2_base layer
//
void tpose_state2::click_action_tpl2_id(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	{
		items.push_back(gui2::tmenu::titem(_("Empty"), action_tpl2s_.size()));
		items.back().separator = true;

		if (state2_.action_tpl2_id.empty()) {
			initial_sel = items.back().val;
		}
	}

	int action_tlp2_at = 0;
	for (std::map<std::string, aplt::taction_tpl2>::const_iterator it = action_tpl2s_.begin(); it != action_tpl2s_.end(); ++ it, action_tlp2_at ++) {
		const aplt::taction_tpl2& action_tpl = it->second;
		items.push_back(gui2::tmenu::titem(action_tpl.name2(), action_tlp2_at));

		if (state2_.action_tpl2_id == action_tpl.id) {
			initial_sel = action_tlp2_at;
		}
	}

	if (items.empty()) {
		return;
	}

	int new_sel = nposm;
	{
		gui2::tmenu dlg(items, initial_sel);
		// dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		new_sel = dlg.selected_val();
	}

	const aplt::taction_tpl2* p_action_tpl2 = nullptr;
	if (new_sel < (int)action_tpl2s_.size()) {
		std::map<std::string, aplt::taction_tpl2>::const_iterator sel_it = action_tpl2s_.begin();
		if (new_sel != 0) {
			std::advance(sel_it, new_sel);
		}

		p_action_tpl2 = &sel_it->second;
	}

	state2_.action_tpl2_id = p_action_tpl2 != nullptr? p_action_tpl2->id: null_str;

	widget.set_label(p_action_tpl2 != nullptr? p_action_tpl2->name2(): null_str);
}

void tpose_state2::did_state2_bool_field_changed(ttoggle_button& widget, int fid)
{
	if (fid == aplt::twkoscript::fid_is_setup) {
		state2_.is_setup = widget.get_value();

	} else if (fid == aplt::twkoscript::fid_debug_skip) {
		state2_.debug_skip = widget.get_value();

	} else {
		VALIDATE(false, null_str);
	}
}

void tpose_state2::update_unsatisfied_remark_label() const
{
	utils::string_map symbols;
	symbols["unsatisfied_threshold_ms"] = str_cast(state2_.unsatisfied_threshold_ms);
	symbols["unsatisfied_2th_threshold_s"] = str_cast(state2_.unsatisfied_2th_threshold_s);
	symbols["unsatisfied_2th_msgstr"] = state2_.unsatisfied_2th_msgstr;

	std::string msg;
	if (state2_.task->type == aplt::twkoscript::tasktype_rep_counter) {
		msg = vgettext2("unsatisfied remark, tasktype_rep_counter, $unsatisfied_threshold_ms, $unsatisfied_2th_threshold_s, $unsatisfied_2th_msgstr", symbols);

	} else {
		msg = vgettext2("unsatisfied remark, tasktype_time_counter, $unsatisfied_threshold_ms", symbols);

		msg.append("\n");
		if (state2_.unsatisfied_2th_msgstr.empty()) {
			msg.append(vgettext2("unsatisfied remark, tasktype_time_counter, 2th is empty, $unsatisfied_threshold_ms, $unsatisfied_2th_threshold_s, $unsatisfied_2th_msgstr", symbols));
		} else {
			msg.append(vgettext2("unsatisfied remark, tasktype_time_counter, 2th isn't empty, $unsatisfied_threshold_ms, $unsatisfied_2th_threshold_s, $unsatisfied_2th_msgstr", symbols));
		}
	}

	unsatisfied_remark_widget_->set_label(msg);
}

void tpose_state2::update_satisfied_remark_label() const
{
	VALIDATE(state2_.task->type == aplt::twkoscript::tasktype_time_counter, null_str);
	aplt::twkoscript::ttime_counter* task2 = static_cast<aplt::twkoscript::ttime_counter*>(state2_.task);

	utils::string_map symbols;
	symbols["satisfied_threshold_s"] = str_cast(task2->satisfied_threshold_s);
	symbols["satisfied_msgstr"] = task2->satisfied_msgstr;
	std::string msg = vgettext2("satisfied remark, $satisfied_threshold_s, $satisfied_2th_msgstr", symbols);

	satisfied_remark_widget_->set_label(msg);
}

void tpose_state2::did_task_type_report_item_changed(tgrid& grid, ttoggle_button& widget)
{
}

int tpose_state2::task_type_to_task_layer(int type) const
{
	if (type == aplt::twkoscript::tasktype_time_counter) {
		return TASK_TIME_COUNTER_LAYER;
	} else if (type == aplt::twkoscript::tasktype_rep_counter) {
		return TASK_REP_COUNTER_LAYER;
	}

	VALIDATE(false, null_str);
	return nposm;
}

void tpose_state2::pre_task_time_counter(tgrid& grid)
{
	VALIDATE(state2_.task->type == aplt::twkoscript::tasktype_time_counter, null_str);
	const aplt::twkoscript::ttime_counter* task = static_cast<const aplt::twkoscript::ttime_counter*>(state2_.task);

	//
	// time rule report
	//
	treport* report = find_widget<treport>(&grid, "time_rule_report", false, true);
	for (std::map<int, tcode3>::const_iterator it = aplt::wko_time_rules.begin(); it != aplt::wko_time_rules.end(); ++ it) {
		const tcode3& rule = it->second;
		report->insert_item(null_str, rule.name).set_cookie(rule.code);
	}
	report->set_did_item_changed(std::bind(&tpose_state2::did_time_rule_report_item_changed, this, std::ref(grid), _2));
	report->select_item(task->rule);
	time_rule_report_ = report;

	//
	// time tone report
	//
	report = find_widget<treport>(&grid, "time_tone_report", false, true);
	for (std::map<int, tcode3>::const_iterator it = aplt::wko_time_tones.begin(); it != aplt::wko_time_tones.end(); ++ it) {
		const tcode3& tone = it->second;
		report->insert_item(null_str, tone.name).set_cookie(tone.code);
	}
	report->set_did_item_changed(std::bind(&tpose_state2::did_time_tone_report_item_changed, this, std::ref(grid), _2));
	report->select_item(task->tone);
	time_tone_report_ = report;

	ttext_box* text_box = find_widget<ttext_box>(&grid, "satisfied_threshold_s", false, true);
	text_box->set_label(str_cast(task->satisfied_threshold_s));
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_task_satisfied_threshold_s);

	text_box = find_widget<ttext_box>(&grid, "satisfied_msgstr", false, true);
	text_box->set_label(task->satisfied_msgstr);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_task_satisfied_msgstr);

	tlabel* label = find_widget<tlabel>(window_, "satisfied_remark", false, true);
	satisfied_remark_widget_ = label;
	if (state2_.task->type == aplt::twkoscript::tasktype_time_counter) {
		update_satisfied_remark_label();
	}

	text_box = find_widget<ttext_box>(&grid, "max_count", false, true);
	text_box->set_label(str_cast(task->max_count));
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_time_counter_max_count);

	label = find_widget<tlabel>(&grid, "task_remark", false, true);
	task_remark_widget_ = label;
}

void tpose_state2::pre_task_rep_counter(tgrid& grid)
{
	VALIDATE(state2_.task->type == aplt::twkoscript::tasktype_rep_counter, null_str);
	const aplt::twkoscript::trep_counter* task = static_cast<const aplt::twkoscript::trep_counter*>(state2_.task);

	// phases
	const aplt::twkoscript::tphase* phase = &task->phases[0];
	ttext_box* text_box = find_widget<ttext_box>(&grid, "phase0_action_msg", false, true);
	text_box->set_label(phase->action_msg);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_phase_action_msg, 0);

	text_box = find_widget<ttext_box>(&grid, "phase0_min_duration_ms", false, true);
	text_box->set_label(str_cast(phase->min_duration_ms));
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_phase_min_duration_ms, 0);

	text_box = find_widget<ttext_box>(&grid, "phase0_cooldowned_ms", false, true);
	text_box->set_label(str_cast(phase->cooldowned_ms));
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_phase_cooldowned_ms, 0);

	phase = &task->phases[1];
	text_box = find_widget<ttext_box>(&grid, "phase1_action_msg", false, true);
	text_box->set_label(phase->action_msg);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_phase_action_msg, 1);

	text_box = find_widget<ttext_box>(&grid, "phase1_min_duration_ms", false, true);
	text_box->set_label(str_cast(phase->min_duration_ms));
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_phase_min_duration_ms, 1);

	text_box = find_widget<ttext_box>(&grid, "phase1_cooldowned_ms", false, true);
	text_box->set_label(str_cast(phase->cooldowned_ms));
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_phase_cooldowned_ms, 1);

	// max_count
	text_box = find_widget<ttext_box>(&grid, "max_count", false, true);
	text_box->set_label(str_cast(task->max_count));
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_time_counter_max_count);

	tlabel* label = find_widget<tlabel>(&grid, "task_remark", false, true);
	task_remark_widget_ = label;

}

void tpose_state2::did_time_rule_report_item_changed(tgrid& grid, ttoggle_button& widget)
{
	VALIDATE(state2_.task->type == aplt::twkoscript::tasktype_time_counter, null_str);
	aplt::twkoscript::ttime_counter* task2 = static_cast<aplt::twkoscript::ttime_counter*>(state2_.task);

	task2->rule = widget.at();

	tlabel* label = find_widget<tlabel>(&grid, "rule_desc", false, true);
	label->set_label(time_rule_descs_.find(task2->rule)->second);
}

void tpose_state2::did_time_tone_report_item_changed(tgrid& grid, ttoggle_button& widget)
{
	VALIDATE(state2_.task->type == aplt::twkoscript::tasktype_time_counter, null_str);
	aplt::twkoscript::ttime_counter* task2 = static_cast<aplt::twkoscript::ttime_counter*>(state2_.task);

	task2->tone = widget.at();

	tlabel* label = find_widget<tlabel>(&grid, "tone_desc", false, true);
	label->set_label(time_tone_descs_.find(task2->tone)->second);
}

void tpose_state2::update_task_remark_label() const
{
	std::string msg;
	if (state2_.task->type == aplt::twkoscript::tasktype_time_counter) {
		aplt::twkoscript::ttime_counter* task2 = static_cast<aplt::twkoscript::ttime_counter*>(state2_.task);

		utils::string_map symbols;
		symbols["max_count"] = str_cast(task2->max_count);
		msg = vgettext2("task_time_counter remark, $max_count", symbols);

	} else if (state2_.task->type == aplt::twkoscript::tasktype_rep_counter) {
		aplt::twkoscript::trep_counter* task2 = static_cast<aplt::twkoscript::trep_counter*>(state2_.task);

		utils::string_map symbols;
		symbols["max_count"] = str_cast(task2->max_count);
		msg = vgettext2("task_rep_counter remark, $max_count", symbols);
	}

	task_remark_widget_->set_label(msg);
}
/*
static inline int get_max_bit(uint32_t mask)
{
    if (mask == 0) return -1;
    int pos = 0;
    while (mask >>= 1) pos++;
    return pos;
}
*/
#define SAFE_MASK_TO_BITS_BYTES		128
// Convert the mask to a bit index string (without using any C library functions)
//  mask     buffer(bit_base=0)     buffer(bit_base=1)
//  0x11     "0,1"                  "1,2"
//  0x101£¬  "0,2"                  "1,3"
//  0x10010  "1,4"                  "2,5"
// Return value: number of characters written to the buffer (excluding the terminating '\0')
int mask_to_bits(uint32_t mask, char *buffer, int bit_base)
{
    if (mask == 0) {
        buffer[0] = '\0';
        return 0;
    }
    
    // Calculate the maximum possible length of the string
    // Worst case: all 32 bits are 1, format is "0,1,2,...,31"
    // Each number has at most 2 characters (10-31) + comma delimiter
    // At most 32 numbers, requiring 31 commas
    // The length of "0,1,2,...,31" is approximately: 32*2 + 31 = 95
    
    // Calculate the required number of digits first
    int max_bit = get_max_bit(mask);
    
    // Iterate through all bits to construct the string
    char *ptr = buffer;
    int first = 1;
    
    for (int i = 0; i <= max_bit; i++) {
        if (mask & (1U << i)) {
            if (!first) {
                *ptr++ = ',';  // Add a comma as the delimiter
            }
            first = 0;
            
			int i2 = i + bit_base;
            // Convert the number i to a character (manual conversion, without using sprintf)
            if (i2 >= 10) {
                *ptr++ = '0' + (i2 / 10);     // Tens digit
                *ptr++ = '0' + (i2 % 10);     // Units digit
            } else {
                *ptr++ = '0' + i2;             // Units digit
            }
        }
    }
    
    *ptr = '\0';
    return ptr - buffer;
}

//
// state2_pose layer
//
void tpose_state2::did_pose_report_item_changed(tgrid& grid, ttoggle_button& widget)
{
	int desire_layer = widget.at();

	curr_pose_at_ = desire_layer;
	// below 'operand_list_->select_row(0)' will call clear_landmark_mat
	// clear_landmark_mat(true);
	const aplt::twkoscript::tpose& pose = curr_pose();

	// phase_mask
	char bits_buf[SAFE_MASK_TO_BITS_BYTES];
	mask_to_bits(pose.phase_mask, bits_buf, 1);
	phase_mask_widget_->set_label(bits_buf);

	// name
	ttext_box* text_box = find_widget<ttext_box>(&grid, "pose_name", false, true);
	text_box->set_label(pose.name);

	// type, operand_type
	int pose_type2 = pose_2_pose_type2(pose);
	pose_type_report_->select_item(pose_type2);

	// landmarks
	reload_operand_list(*operand_list_);
	VALIDATE(operand_list_->rows() == wko_operand_count_from_pose_type(pose.type) + 1, null_str);

	// abs
	abs_diff_widget_->set_value(pose.abs);

	// min value, max value
	text_box = find_widget<ttext_box>(&grid, "pose_min", false, true);
	if (!is_float_nposm(pose.range.min)) {
		text_box->set_label(str_cast(pose.range.min));
	} else {
		text_box->set_label(null_str);
	}

	text_box = find_widget<ttext_box>(&grid, "pose_max", false, true);
	if (!is_float_nposm(pose.range.max)) {
		text_box->set_label(str_cast(pose.range.max));
	} else {
		text_box->set_label(null_str);
	}

	// unsatisfied_msgstr
	text_box = find_widget<ttext_box>(&grid, "pose_unsatisfied_msgstr", false, true);
	text_box->set_label(pose.unsatisfied_msgstr);

	// unsatisfied_rmax_msgstr
	text_box = find_widget<ttext_box>(&grid, "pose_unsatisfied_rmax_msgstr", false, true);
	text_box->set_label(pose.unsatisfied_rmax_msgstr);

	bool is_first_pose = curr_pose_at_ == 0;
	move_left_widget_->set_visible(is_first_pose? twidget::INVISIBLE: twidget::VISIBLE);

	bool is_last_pose = curr_pose_at_ == (int)state2_.track_pose.poses.size() - 1;
	move_right_widget_->set_visible(is_last_pose? twidget::INVISIBLE: twidget::VISIBLE);

	set_min_max_placeholder(pose.type);
}

std::string tpose_state2::generate_pose_name_form_report(const aplt::twkoscript::tpose& pose, int at, int size) const
{
	VALIDATE(at >= 0 && at < size, null_str);
	char buf[128];
	if (!wko_allow_multiple_phase_from_task_type(state2_.task->type)) {
		SDL_snprintf(buf, sizeof(buf), "%i/%i)%s", at + 1, size, 
			utils::truncate_to_max_chars2(pose.name, pose_report_name_max_chars_, true).c_str());
	} else {
		char bits_buf[SAFE_MASK_TO_BITS_BYTES]; // now WKO_MAX_PHASE_COUNT is 2, max is "2,1".
		mask_to_bits(pose.phase_mask, bits_buf, 1);
		SDL_snprintf(buf, sizeof(buf), "%i/%i)[%s]%s", at + 1, size, bits_buf,
			utils::truncate_to_max_chars2(pose.name, pose_report_name_max_chars_, true).c_str());
	}
	return buf;
}

void tpose_state2::update_pose_report_names()
{
	treport& report = *pose_report_;

	const aplt::twkoscript::ttrack_pose& track_pose = state2_.track_pose;
	VALIDATE(report.items() == track_pose.poses.size(), null_str);

	int pose_at = 0;
	for (std::vector<aplt::twkoscript::tpose>::const_iterator it = track_pose.poses.begin(); it != track_pose.poses.end(); ++ it, pose_at ++) {
		const aplt::twkoscript::tpose& pose = *it;
		report.item(pose_at).set_label(generate_pose_name_form_report(pose, pose_at, track_pose.poses.size()));
	}
}

void tpose_state2::click_phase_mask(tbutton& widget)
{
	aplt::twkoscript::tpose& pose = mutable_curr_pose();

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;
    
	// gui2::tcontext_menu& menu = dlg_->context_menus().front();
	// gui2::tcontrol* widget = menu.report->item(

	char bits_buf[SAFE_MASK_TO_BITS_BYTES];
	int max_phase_mask = BIT_IDX_MASK(WKO_MAX_PHASE_COUNT) - 1;
	for (int val = 1; val <= max_phase_mask; val ++) {
		mask_to_bits(val, bits_buf, 1);
		items.push_back(gui2::tmenu::titem(bits_buf, val));
		if (pose.phase_mask == val) {
			initial_sel = val;
		}
	}

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();
	pose.phase_mask = cursel;

	mask_to_bits(pose.phase_mask, bits_buf, 1);
	widget.set_label(bits_buf);

	pose_report_->item(curr_pose_at_).set_label(generate_pose_name_form_report(pose, 
		curr_pose_at_, state2_.track_pose.poses.size()));
}

void test123()
{
	SDL_Log("---normalize_angle---");
	for (double angle = -1.0; angle < 361; angle += 1.0) {
		double angle2 = angles::normalize_angle(DEG2RAD(angle));
		SDL_Log("%.3f ---> %.3f", angle, RAD2DEG(angle2));
	}
	SDL_Log("------");
}

void tpose_state2::click_insert_pose(tbutton& widget)
{
	// test123();

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	std::map<int, std::vector<gui2::tmenu::titem> > sides;
	std::map<int, std::vector<gui2::tmenu::titem> >::iterator side_it;
	for (std::map<int, aplt::tpreset_pose>::const_iterator it = preset_poses_.begin(); it != preset_poses_.end(); ++ it) {
		const aplt::tpreset_pose& pose = it->second;
		side_it = sides.find(pose.side);
		if (side_it == sides.end()) {
			std::pair<std::map<int, std::vector<gui2::tmenu::titem> >::iterator, bool> ins = sides.insert(
				std::make_pair(pose.side, std::vector<gui2::tmenu::titem>()));
			side_it = ins.first;
		}
		side_it->second.push_back(gui2::tmenu::titem(pose.name, pose.metric));
	}

	items.push_back(gui2::tmenu::titem(_("Empty"), aplt::posemetric_count, null_str));
	for (std::map<int, std::vector<gui2::tmenu::titem> >::const_iterator it = sides.begin(); it != sides.end(); ++ it) {
		int side = it->first;
		VALIDATE(aplt::pose_sides.count(side) != 0, null_str);
		tcode3& code3 = aplt::pose_sides.find(side)->second;
		items.push_back(gui2::tmenu::titem(code3.name, it->second, null_str));
	}

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();

	const bool always_last = true;
	aplt::twkoscript::tpose* new_pose = nullptr;
	if (cursel != aplt::posemetric_count) {
		const aplt::tpreset_pose& sel_preset = preset_poses_.find(cursel)->second;

		new_pose = &insert_preset_pose(sel_preset, surf_landmarks_, surf_landmarks_valid_, state2_);

	} else {
		// 
		const int insert_at = always_last? nposm: curr_pose_at_;
		new_pose = &state2_.track_pose.insert_pose(insert_at, null_str);
	}
	new_pose->phase_mask = BIT_IDX_MASK(curr_phase_at_);
	pose_report_->insert_item(null_str, new_pose->name);

	const int sel_at = always_last? state2_.track_pose.poses.size() - 1: curr_pose_at_ + 1;
	pose_report_->select_item(sel_at);

	update_pose_report_names();
}

void tpose_state2::click_erase_pose(tbutton& widget)
{
	VALIDATE(!state2_.track_pose.poses.empty(), null_str);
	if (state2_.track_pose.poses.size() == 1) {
		gui2::show_message(null_str, _("At least one pose judgment is required; this item cannot be deleted."));
		return;
	}

	const aplt::twkoscript::tpose& pose = curr_pose();
	const std::string msg = i18n::freq_msgstr_2str(i18n::msgid_confirm_delete_2str, _("track_pose^pose"), pose.name);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	state2_.track_pose.erase_pose(curr_pose_at_);
	pose_report_->erase_item(curr_pose_at_);

	VALIDATE((int)state2_.track_pose.poses.size() == pose_report_->items(), null_str);
	update_pose_report_names();
}

void tpose_state2::click_move_left_or_right(tbutton& widget, bool left)
{
	const std::vector<aplt::twkoscript::tpose>& poses = state2_.track_pose.poses;
	int s1 = curr_pose_at_;
	int s2 = nposm;
	if (left) {
		VALIDATE(curr_pose_at_ > 0, null_str);
		s2 = s1 - 1;
		VALIDATE(s2 >= 0, null_str);

	} else {
		VALIDATE(curr_pose_at_ < (int)poses.size() - 1, null_str);
		s2 = s1 + 1;
		VALIDATE(s2 < (int)poses.size(), null_str);
	}

	state2_.track_pose.pose_swap(s1, s2);

	const aplt::twkoscript::tpose& pose1 = poses[s1];
	const aplt::twkoscript::tpose& pose2 = poses[s2];
	pose_report_->item(s1).set_label(generate_pose_name_form_report(pose1, s1, poses.size()));
	pose_report_->item(s2).set_label(generate_pose_name_form_report(pose2, s2, poses.size()));
	pose_report_->select_item(s2);
}

SDL_Point tpose_state2::in_which_rect(int x, int y) const
{
	VALIDATE(point_in_rect(x, y, landmark_track_->get_rect()), null_str);

	for (int btn = 0; btn < lmk_btn_count; btn ++) {
		const SDL_Rect& rect = btn_rects_[btn];
		if (rect.w > 0) {
			if (point_in_rect(x, y, rect)) {
				return SDL_Point{recttype_btn, btn};
			}
		}
	}
/*
	if (point_in_rect(x, y, btn_clear_rect_)) {
		return SDL_Point{recttype_btn, 0};
	}
*/
	for (int at = 0; at < mediapipe::kNumPoseLandmarks; at ++) {
		const SDL_Rect& rect = landmark_rects_[at];
		if (point_in_rect(x, y, rect)) {
			return SDL_Point{recttype_landmark, at};
		}
	}
	return SDL_Point{nposm, nposm};
}

void tpose_state2::did_draw_landmark(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	if (curr_state2_layer_ != STATE2_POSE_LAYER) {
		// return;
	}
	SDL_Renderer* renderer = get_renderer();

	const bool require_render = true;

	if (require_render && !bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), NULL, &widget_rect);
	}

	if (!landmark_mat_.empty() && (landmark_mat_.cols != widget_rect.w || landmark_mat_.rows != widget_rect.h)) {
		clear_landmark_mat(false);
	}

	const int canvas_w = widget_rect.w;
	const int canvas_h = widget_rect.h;

	SDL_Rect dst;
	const bool use_surf_landmarks = phase0_surf_.get() != nullptr;
	if (landmark_mat_.empty()) {
		SDL_FPoint xy_landmarks[mediapipe::kNumPoseLandmarks];
		SDL_Size ref_img_size;
		if (!use_surf_landmarks) {			
			const SDL_FPoint* _landmarks = mediapipe::get_default_landmarks(ref_img_size);
			memcpy(xy_landmarks, _landmarks, sizeof(xy_landmarks));

		} else {
			memcpy(xy_landmarks, surf_landmarks_, sizeof(xy_landmarks));
			ref_img_size.w = phase0_surf_->w;
			ref_img_size.h = phase0_surf_->h;
		}
		int excludes[] = {1, 3, 4, 6, 17, 18, 19, 20, 21, 22, 29, 30, 31, 32};
		int NUM = sizeof(excludes) / sizeof(excludes[0]);
		for (int at = 0; at < NUM; at ++) {
			SDL_FPoint& lmk = xy_landmarks[excludes[at]];
			lmk.x = std::numeric_limits<float>::quiet_NaN();
			lmk.y = std::numeric_limits<float>::quiet_NaN();
		}

		double radius = 0;
		SDL_Point chart_margin{0, 0};

		SDL_Size adapted_canvas_size = calculate_max_size_with_ratio(ref_img_size.w, ref_img_size.h, canvas_w, canvas_h);
		int offset_x = (canvas_w - adapted_canvas_size.w) / 2;
		int offset_y = (canvas_h - adapted_canvas_size.h) / 2;

		const SDL_Rect clip{offset_x, offset_y, adapted_canvas_size.w, adapted_canvas_size.h};
		int playstyle = nposm;

		ttoggle_panel* cursel = operand_list_->cursel();
		VALIDATE(cursel != nullptr, null_str);
		bool sel_landmarks[mediapipe::kNumPoseLandmarks];
		memset(sel_landmarks, 0, sizeof(sel_landmarks));
		const aplt::twkoscript::tpose& pose = curr_pose();
		const twko_operand& operand = pose.operands[cursel->at()];
		if (operand.lmk0 != nposm) {
			sel_landmarks[operand.lmk0] = true;
		}
		if (operand.lmk1 != nposm) {
			sel_landmarks[operand.lmk1] = true;
		}

		cairo::tlandmark_fields fields(sdl_field_small_font_size_, lmk33_mode_, false, sel_landmarks, use_surf_landmarks);
		fields.phase_count = wko_phase_count_from_task_type(state2_.task->type);
		fields.phase_sel = curr_phase_at_;

		landmark_mat_ = cairo::draw_landmarks_mat(false, canvas_w, canvas_h, radius, chart_margin,
			clip, xy_landmarks, mediapipe::kNumPoseLandmarks, nposm, nposm, playstyle, 
			btn_rects_, lmk_btn_count, fields);

		const int rect_radius = 7 * twidget::hdpi_scale;
		const bool overlay_frame = false; // for debug, require set false when release.
		for (int at = 0; at < mediapipe::kNumPoseLandmarks; at ++) {
			const SDL_Point& point = fields.points[at];
			if (point.x == nposm) {
				VALIDATE(point.y == nposm, null_str);
				continue;
			}
			SDL_Rect& rect = landmark_rects_[at];
			rect.x = widget_rect.x + point.x - rect_radius;
			rect.y = widget_rect.y + point.y - rect_radius;
			rect.w = 2 * rect_radius;
			rect.h = 2 * rect_radius;

			if (overlay_frame) {
				cv::Rect rect2(rect.x - widget_rect.x, rect.y - widget_rect.y, rect.w, rect.h);
				cv::rectangle(landmark_mat_, rect2, cv::Scalar(119, 119, 119, 255));
			}
		}

		for (int btn_at = 0; btn_at < lmk_btn_count; btn_at ++) {
			SDL_Rect& btn_rect = btn_rects_[btn_at];
			btn_rect.x += widget_rect.x;
			btn_rect.y += widget_rect.y;
			if (overlay_frame) {
				cv::Rect rect2(btn_rect.x - widget_rect.x, btn_rect.y - widget_rect.y, 
					btn_rect.w, btn_rect.h);
				// cv::rectangle(landmark_mat_, rect2, cv::Scalar(119, 119, 119, 255));
				cv::rectangle(landmark_mat_, rect2, cv::Scalar(0, 255, 0, 255));
			}
		}
		// btn_clear_rect_
	}

	VALIDATE(landmark_mat_.cols == widget_rect.w && landmark_mat_.rows == widget_rect.h, null_str);

	if (use_surf_landmarks) {
		VALIDATE(phase0_surf_, null_str);
		texture tex = SDL_CreateTextureFromSurface2(renderer, phase0_surf_.get());
		if (require_render) {
			// SDL_DColor from{0.12, 0.15, 0.25, 1.0};
			SDL_DColor to{0.06, 0.08, 0.12, 1.0};
			SDL_Color color = font::SDL_DColor_to_SDL_Color(to);
			// render_rect(renderer, widget_rect, color_to_uint32(color));

			SDL_Size adapted_canvas_size = calculate_max_size_with_ratio(phase0_surf_->w, phase0_surf_->h, canvas_w, canvas_h);
			int offset_x = (canvas_w - adapted_canvas_size.w) / 2;
			int offset_y = (canvas_h - adapted_canvas_size.h) / 2;

			const SDL_Rect clip{offset_x, offset_y, adapted_canvas_size.w, adapted_canvas_size.h};
			dst = ::create_rect(widget_rect.x + clip.x, widget_rect.y + clip.y, clip.w, clip.h);
			SDL_RenderCopy(renderer, tex.get(), NULL, &dst);
		}
	}

	surface surf(landmark_mat_);

	texture tex = SDL_CreateTextureFromSurface2(renderer, surf.get());
	if (require_render) {
		dst = widget_rect;
		SDL_RenderCopy(renderer, tex.get(), NULL, &dst);
	}
}

void tpose_state2::did_mouse_motion_landmark(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}

	if (interest_hiting_.x == nposm) {
		return;
	}

	SDL_Point now_hiting = in_which_rect(last.x, last.y);
	if (now_hiting.x != interest_hiting_.x || now_hiting.y != interest_hiting_.y) {
		interest_hiting_ = SDL_Point{nposm, nposm};
	}
}

void tpose_state2::did_left_button_down_landmark(ttrack& widget, const tpoint& coordinate)
{
	VALIDATE(interest_hiting_.x == nposm, null_str);

	SDL_Point now_hiting = in_which_rect(coordinate.x, coordinate.y);
	if (now_hiting.x == nposm) {
		return;
	}
	interest_hiting_ = now_hiting;
}

void tpose_state2::did_mouse_leave_landmark(ttrack& widget, const tpoint& first, const tpoint& last)
{
	tclear_interest_hiting_lock lock(*this);

	// if (is_null_coordinate(last)) {
	if (is_magic_coordinate(last)) {
		return;
	}

	if (interest_hiting_.x == nposm) {
		return;
	}

	SDL_Point now_hiting = in_which_rect(last.x, last.y);
	if (now_hiting.x != interest_hiting_.x || now_hiting.y != interest_hiting_.y) {
		return;
	}

	bool landmark_mat_dirty = false;
	BOOL operand_list_dirty = false;
	int type = now_hiting.x;
	int ctx = now_hiting.y;

	const aplt::twkoscript::tpose& pose = curr_pose();
	ttoggle_panel* cursel = operand_list_->cursel();
	VALIDATE(cursel != nullptr, null_str);
	VALIDATE(cursel->at() < wko_operand_count_from_pose_type(pose.type), null_str);
	const twko_operand& operand = pose.operands[cursel->at()];
	if (type == recttype_landmark) {
		if (operand.lmk0 == now_hiting.y || operand.lmk1 == now_hiting.y) {
			erase_lmk_from_operand(now_hiting.y, cursel->at());
			landmark_mat_dirty = true;

		} else if (operand.lmk1 == nposm) {
			insert_lmk_to_operand(now_hiting.y, cursel->at());
			landmark_mat_dirty = true;
		}

	} else if (type == recttype_btn) {
		if (ctx == lmk_btn_phase0 || ctx == lmk_btn_phase1) {
			const int desire_phase_at = ctx - lmk_btn_phase0;
			VALIDATE(desire_phase_at != curr_phase_at_, null_str);

			curr_phase_at_ = desire_phase_at;
			reload_curr_phase_surf(false);

			update_runtime_calc_result_label();

			landmark_mat_dirty = true;
			// operand_list_dirty = true;
			
		} else if (ctx == lmk_btn_open_image) {
			tmsg_data_browse_file* pdata = new tmsg_data_browse_file(browsefile_phase_surf_file);
			rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_BROWSE_FILE, pdata);

		} else if (ctx == lmk_btn_fake_landmark) {
			tmsg_data_click_fake_landmark* pdata = new tmsg_data_click_fake_landmark(btn_rects_[lmk_btn_fake_landmark]);
			rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_CLICK_FAKE_LANDMARK, pdata);

		} else if (ctx == lmk_btn_lmk33_mode) {
			lmk33_mode_ = (lmk33_mode_ + 1) % lmkmode_count;

			landmark_mat_dirty = true;

		} else if (ctx == lmk_btn_clear) {
			VALIDATE(operand.lmk0 != nposm, null_str);
			clear_operand(cursel->at());

			landmark_mat_dirty = true;
		}
	}
	if (landmark_mat_dirty) {
		clear_landmark_mat(true);
	}
	if (operand_list_dirty) {
		// it in mouse-down, call reload_operand_list() will VALIDATE fail.
		rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_RELOAD_OPERAND_LIST, nullptr);
	}
}

void tpose_state2::clear_landmark_mat(bool draw)
{
	if (!landmark_mat_.empty()) {
		landmark_mat_ = cv::Mat();
		clear_rects();
		if (draw) {
			landmark_track_->immediate_draw();
		}

	} else {
		VALIDATE(btn_clear_rect_ == empty_rect, null_str);
	}
}

int tpose_state2::pose_2_pose_type2(const aplt::twkoscript::tpose& pose) const
{
	if (pose.type == aplt::twkoscript::posetype_angle3p) {
		if (pose.ang_range == aplt::twkoscript::angrange_180) {
			return posetype2_angle3p;
		} else {
			VALIDATE(pose.ang_range == aplt::twkoscript::angrange_360, null_str);
			return posetype2_angle3p_360;
		}
	} else if (pose.type == aplt::twkoscript::posetype_angle2p) {
		return posetype2_angle2p;
	}
	
	VALIDATE(pose.type == aplt::twkoscript::posetype_diff, null_str);
	if (pose.operand_type == aplt::twkoscript::operandtype_point) {
		return posetype2_diff_point;
	} else if (pose.operand_type == aplt::twkoscript::operandtype_x) {
		return posetype2_diff_x;
	} else if (pose.operand_type == aplt::twkoscript::operandtype_y) {
		return posetype2_diff_y;
	} else if (pose.operand_type == aplt::twkoscript::operandtype_dist_x) {
		return posetype2_diff_dist_x;
	}
	VALIDATE(pose.operand_type == aplt::twkoscript::operandtype_dist_y, null_str);
	return posetype2_diff_dist_y;
}

void tpose_state2::pose_type2_2_pose(int type2, aplt::twkoscript::tpose& pose) const
{
	pose.type = aplt::twkoscript::posetype_diff;
	pose.operand_type = aplt::twkoscript::operandtype_point;
	pose.ang_range = aplt::twkoscript::def_pose_ang_range;

	if (type2 == posetype2_angle3p) {
		pose.type = aplt::twkoscript::posetype_angle3p;
		pose.ang_range = aplt::twkoscript::angrange_180;

	} else if (type2 == posetype2_angle3p_360) {
		pose.type = aplt::twkoscript::posetype_angle3p;
		pose.ang_range = aplt::twkoscript::angrange_360;

	} else if (type2 == posetype2_angle2p) {
		pose.type = aplt::twkoscript::posetype_angle2p;

	} else if (type2 == posetype2_diff_point) {
		// pose.operand_type = aplt::twkoscript::operandtype_point;
	} else if (type2 == posetype2_diff_x) {
		pose.operand_type = aplt::twkoscript::operandtype_x;
	} else if (type2 == posetype2_diff_y) {
		pose.operand_type = aplt::twkoscript::operandtype_y;
	} else if (type2 == posetype2_diff_dist_x) {
		pose.operand_type = aplt::twkoscript::operandtype_dist_x;
	} else if (type2 == posetype2_diff_dist_y) {
		pose.operand_type = aplt::twkoscript::operandtype_dist_y;
	} else {
		VALIDATE(false, null_str);
	}
}

std::string tpose_state2::operand_result_desc(const twko_operand& operand, int pose_type2) const
{
	if (operand.lmk0 == nposm) {
		return null_str;
	}

	const int& type2 = pose_type2;
	if (type2 == posetype2_angle3p || type2 == posetype2_angle3p_360) {
		if (operand.lmk1 == nposm) {
			return _("Point x, y coordinates");
		} else {
			return _("Midpoint x, y coordinates");
		}

	} else if (type2 == posetype2_angle2p) {
		if (operand.lmk1 == nposm) {
			return _("Point x, y coordinates");
		} else {
			return _("Midpoint x, y coordinates");
		}

	} else if (type2 == posetype2_diff_point) {
		if (operand.lmk1 == nposm) {
			return _("Point x, y coordinates");
		} else {
			return _("Midpoint x, y coordinates");
		}
	} else if (type2 == posetype2_diff_x) {
		if (operand.lmk1 == nposm) {
			return _("Point x coordinates");
		} else {
			return _("Midpoint x coordinates");
		}
	} else if (type2 == posetype2_diff_y) {
		if (operand.lmk1 == nposm) {
			return _("Point y coordinates");
		} else {
			return _("Midpoint y coordinates");
		}
	} else if (type2 == posetype2_diff_dist_x) {
		if (operand.lmk1 == nposm) {
			return _("A second landmark must be added");
		} else {
			return _("Horizontal (x) distance between two points");
		}
	} else if (type2 == posetype2_diff_dist_y) {
		if (operand.lmk1 == nposm) {
			return _("A second landmark must be added");
		} else {
			return _("Vertical (y) distance between two points");
		}
	} else {
		VALIDATE(false, null_str);
	}
	return null_str;
}

void tpose_state2::did_pose_type_report_item_changed(tgrid& grid, ttoggle_button& widget)
{
	tlabel* label = find_widget<tlabel>(&grid, "pose_type_desc", false, true);
	label->set_label(pose_type2s_.find(widget.cookie())->second.name);

	int desire_type2 = widget.at();
	curr_pose_type2_ = desire_type2;

	aplt::twkoscript::tpose& pose = mutable_curr_pose();
	if (pose_2_pose_type2(pose) != desire_type2) {
		pose_type2_2_pose(desire_type2, pose);
		reload_operand_list(*operand_list_);
	}

	bool has_abs = posetype2_has_abs(desire_type2);
	ttoggle_button* toggle = abs_diff_widget_;
	toggle->set_visible(has_abs? twidget::VISIBLE: twidget::INVISIBLE);

	set_min_max_placeholder(pose.type);
}

std::string tpose_state2::calc_result_may_invalid2(const aplt::twkoscript::tpose& pose, const SDL_FPoint* landmarks) const
{
	if (!surf_landmarks_valid_) {
		return _("The image did not detect enough landmark.");
	}
	SDL_DPoint result = pose.calc_result_may_invalid(surf_landmarks_);
	std::string label;
	if (!is_float_nposm(result.x)) {
		if (pose.type == aplt::twkoscript::posetype_angle2p) {
			if (result.y > 270) {
				double rad = angles::normalize_angle(DEG2RAD(result.y));
				char buf[32];
				SDL_snprintf(buf, sizeof(buf), "%.3f(%.3f)", result.y, RAD2DEG(rad));
				label = buf;
			}
		}
		if (label.empty()) {
			label = str_cast(result.y);
		}
	} else {
		label = _("Calculation failed. Not enough landmarks have been set.");
	}
	return label;
}

void tpose_state2::update_runtime_calc_result_label()
{
	const aplt::twkoscript::tpose& pose = curr_pose();

	std::string result_label = calc_result_may_invalid2(pose, surf_landmarks_);

	int operand_count = wko_operand_count_from_pose_type(pose.type);
	ttoggle_panel& row_panel = operand_list_->row_panel(operand_count);
	row_panel.set_child_label("operand_result", result_label);
}

void tpose_state2::did_operand_landmark_changed(int operand_at, const twko_operand& operand)
{
	ttoggle_panel& row_panel = operand_list_->row_panel(operand_at);
	row_panel.set_child_label("operand_landmarks", operand_landmarks_label(operand));
	row_panel.set_child_label("operand_result", operand_result_desc(operand, curr_pose_type2_));

	// if (surf_landmarks_valid_) {
		update_runtime_calc_result_label();
	// }
}

void tpose_state2::insert_lmk_to_operand(int lmk, int operand_at)
{
	// VALIDATE(lmk >= 0 && lmk < mediapipe::kNumPoseLandmarks, null_str);
	VALIDATE(is_valid_lmk_all(lmk), null_str);
	aplt::twkoscript::tpose& pose = mutable_curr_pose();
	VALIDATE(operand_at >= 0 && operand_at < wko_operand_count_from_pose_type(pose.type), null_str);
	twko_operand& operand = pose.operands[operand_at];

	VALIDATE(operand.lmk1 == nposm, null_str);
	if (operand.lmk0 == nposm) {
		operand.lmk0 = lmk;

	} else {
		operand.lmk1 = lmk;
	}

	did_operand_landmark_changed(operand_at, operand);
}

void tpose_state2::erase_lmk_from_operand(int lmk, int operand_at)
{
	// VALIDATE(lmk >= 0 && lmk < mediapipe::kNumPoseLandmarks, null_str);
	VALIDATE(is_valid_lmk_all(lmk), null_str);
	aplt::twkoscript::tpose& pose = mutable_curr_pose();
	VALIDATE(operand_at >= 0 && operand_at < wko_operand_count_from_pose_type(pose.type), null_str);
	twko_operand& operand = pose.operands[operand_at];

	if (operand.lmk0 == lmk) {
		operand.lmk0 = operand.lmk1;
		operand.lmk1 = nposm;

	} else if (operand.lmk1 == lmk) {
		operand.lmk1 = nposm;
	}

	did_operand_landmark_changed(operand_at, operand);
}

void tpose_state2::clear_operand(int operand_at)
{
	aplt::twkoscript::tpose& pose = mutable_curr_pose();
	VALIDATE(operand_at >= 0 && operand_at < wko_operand_count_from_pose_type(pose.type), null_str);
	twko_operand& operand = pose.operands[operand_at];

	VALIDATE(operand.lmk0 != nposm, null_str);
	operand.lmk0 = nposm;
	if (operand.lmk1 != nposm) {
		operand.lmk1 = nposm;
	}

	did_operand_landmark_changed(operand_at, operand);
}

std::string tpose_state2::operand_landmarks_label(const twko_operand& operand) const
{
	std::stringstream ss;
	if (operand.lmk0 != nposm) {
		ss << utils::landmark_name(operand.lmk0);
	}
	if (operand.lmk1 != nposm) {
		ss << ", " << utils::landmark_name(operand.lmk1);
	}
	return ss.str();
}

void tpose_state2::reload_curr_phase_surf(bool load_always)
{
	VALIDATE(curr_phase_at_ >= 0 && curr_phase_at_ < wko_phase_count_from_task_type(state2_.task->type), null_str);

	std::string filename = state2_.build_lmk33_png_filename(phase_surf_dir_, curr_phase_at_);
	// phase0_surf_ = image::get_image(filename, load_always);
	phase0_surf_ = image::get_image_withoutcache(filename);
	if (phase0_surf_.get() == nullptr) {
		surf_landmarks_valid_ = false;
		return;
	}

	if (phase0_surf_->w != 1280 && phase0_surf_->h != 720) {
		return;
	}

	// surf_landmarks_valid_ = calc_surf_landmarks(mediapipe_api_, phase0_surf_, surf_landmarks_);
	surf_landmarks_valid_ = calc_surf_landmarks(*api_ptr_.get(), phase0_surf_, surf_landmarks_);
/*
	{
		tsurface_2_mat_lock lock(phase0_surf_);
		bool flip_h_ = false;

		mediapipe::tpose_tracking_api& api = *api_ptr_.get();
		bool is_less_than_min_interval = false;
		cv::Mat output_frame_mat = api.next_image(lock.mat, flip_h_, surf_landmarks_, is_less_than_min_interval);
		VALIDATE(!is_less_than_min_interval, null_str);
		surf_landmarks_valid_ = !output_frame_mat.empty();
		for (int at = 0; at < mediapipe::kNumPoseLandmarks && surf_landmarks_valid_; at ++) {
			int lmk = state2_.track_pose.landmarks[at];
			if (std::isnan(surf_landmarks_[at].x)) {
				surf_landmarks_valid_ = false;
			}
		}
	}
*/
}

void tpose_state2::update_phase_surf_file()
{
	std::string filename = handle_browse_file(true, null_str, null_str);
	if (filename.empty()) {
		return;
	}
	// surface surf = image::get_image(filename);
	surface surf = image::get_image_withoutcache(filename);
	if (surf.get() == nullptr) {
		return;
	}

	save_lmk33_png_from_surf(surf, state2_, phase_surf_dir_, curr_phase_at_);

	reload_curr_phase_surf(true);
	reload_operand_list(*operand_list_);

	clear_landmark_mat(true);

	phase_surf_updated_ = true;
}

void tpose_state2::click_fake_landmark(const SDL_Rect& rect)
{
	aplt::twkoscript::tpose& pose = mutable_curr_pose();

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
    
	ttoggle_panel* cursel = operand_list_->cursel();
	VALIDATE(cursel != nullptr, null_str);
	const twko_operand& operand = pose.operands[cursel->at()];

	items.push_back(gui2::tmenu::titem(_("Empty"), fake_lmk_max + 1));

	char buf[128];
	for (int lmk_at = fake_lmk_min; lmk_at <= fake_lmk_max; lmk_at ++) {
		const SDL_FPoint& lmk = aplt::fake_landmarks[lmk_at - fake_lmk_min];
		SDL_snprintf(buf, sizeof(buf), "%s(%.1f, %.1f)", 
			utils::landmark_name(lmk_at).c_str(), lmk.x, lmk.y);
		items.push_back(gui2::tmenu::titem(buf, lmk_at));
		if (lmk_at == operand.lmk0 || lmk_at == operand.lmk1) {
			initial_sel = lmk_at;
		}
	}

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(rect.x + rect.w, rect.y);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int new_sel = dlg.selected_val();
	if (initial_sel != nposm) {
		erase_lmk_from_operand(initial_sel, cursel->at());
	}
	if (is_fake_lmk(new_sel)) {
		if (operand.lmk1 == nposm) {
			insert_lmk_to_operand(new_sel, cursel->at());
		}
	}

	// pose_report_->item(curr_pose_at_).set_label(generate_pose_name_form_report(pose, 
	//	curr_pose_at_, state2_.track_pose.poses.size()));
}

bool tpose_state2::did_operand_list_row_pre_change(tlistbox& list, ttoggle_panel& row)
{
	const aplt::twkoscript::tpose& pose = curr_pose();
	int operand_count = wko_operand_count_from_pose_type(pose.type);
	return row.at() < operand_count;
}

void tpose_state2::did_operand_list_row_changed(tlistbox& list, ttoggle_panel& row)
{
	const aplt::twkoscript::tpose& pose = curr_pose();
	int operand_count = wko_operand_count_from_pose_type(pose.type);
	VALIDATE(row.at() < operand_count, null_str);

	if (row.at() == operand_count) {
		return;
	}

	clear_landmark_mat(true);
}

void tpose_state2::reload_operand_list(tlistbox& list)
{
	VALIDATE(curr_pose_type2_ != nposm, null_str);

	ttoggle_panel* cursel = list.cursel();
	const int previous_sel = cursel != nullptr? cursel->at(): nposm;

	list.clear();

	utils::string_map symbols;

	char buf[32];
	std::map<std::string, std::string> data;
	const aplt::twkoscript::tpose& pose = curr_pose();
	int operand_count = wko_operand_count_from_pose_type(pose.type);
	for (int at = 0; at < operand_count; at ++) {
		const twko_operand& operand = pose.operands[at];
		SDL_snprintf(buf, sizeof(buf), "%c", 'A' + at);
		// symbols["number"] = str_cast(at + 1);
		symbols["number"] = buf;
		data["operand_name"] = vgettext2("Operand $number", symbols);
		data["operand_landmarks"] = operand_landmarks_label(operand);
		data["operand_result"] = operand_result_desc(operand, curr_pose_type2_);

		list.insert_row(data);
	}
	// if (surf_landmarks_valid_) {
		std::string result_label = calc_result_may_invalid2(pose, surf_landmarks_);

		data["operand_name"] = _("Real-time value");
		data["operand_landmarks"] = null_str;
		data["operand_result"] = result_label;
		list.insert_row(data);
	// }



	int new_sel = previous_sel;
	if (previous_sel == nposm || previous_sel >= operand_count /*list.rows()*/) {
		// don't select lat row.
		new_sel = 0;
	}
	operand_list_->select_row(new_sel);
}

void tpose_state2::did_abs_diff_changed(ttoggle_button& widget)
{
	aplt::twkoscript::tpose& pose = mutable_curr_pose();
	pose.abs = widget.get_value();

	update_runtime_calc_result_label();
}

void tpose_state2::set_status_label(const std::string& msg)
{
	status_widget_->set_label(msg);
}

void tpose_state2::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

void tpose_state2::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case MSG_RELOAD_OPERAND_LIST:
		{
			reload_operand_list(*operand_list_);
		}
		break;

	case MSG_BROWSE_FILE:
		{
			tmsg_data_browse_file* pdata = static_cast<tmsg_data_browse_file*>(msg->pdata);
			VALIDATE(pdata->scene == browsefile_phase_surf_file, null_str);
			update_phase_surf_file();
		}
		break;

	case MSG_CLICK_FAKE_LANDMARK:
		{
			tmsg_data_click_fake_landmark* pdata = static_cast<tmsg_data_click_fake_landmark*>(msg->pdata);
			click_fake_landmark(pdata->rect);
		}
		break;

	default:
		VALIDATE(false, null_str);
	}

	if (msg->pdata != nullptr) {
		delete msg->pdata;
	}
}

} // namespace gui2

