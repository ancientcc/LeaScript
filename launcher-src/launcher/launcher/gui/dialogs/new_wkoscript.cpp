#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/new_wkoscript.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "game_config.hpp"
#include "font.hpp"

using namespace std::placeholders;

extern aplt::twkoscript::tpose& insert_preset_pose(const aplt::tpreset_pose& preset,
	const SDL_FPoint* landmarks, bool landmarks_valid, aplt::twkoscript::tstate2& state2);
extern bool calc_surf_landmarks(mediapipe::tpose_tracking_api& api, const surface& surf, SDL_FPoint* landmarks);
extern surface save_lmk33_png_from_surf(const surface& src_surf, aplt::twkoscript::tstate2& state2, const std::string& phase_surf_dir, int phase_at);

extern void win_ShellExecuteW_open(const std::string& url);

namespace gui2 {

REGISTER_DIALOG(launcher, new_wkoscript)

tnew_wkoscript::tnew_wkoscript(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, aplt::twkoscript& script,
	const std::map<int, aplt::tpreset_pose>& preset_poses, const std::string& wkoscript_dir)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, script_(script)
	// , mediapipe_api_(mediapipe_api)
	, preset_poses_(preset_poses)
	, wkoscript_dir_(wkoscript_dir)
	, new_dir_(wkoscript_dir + "/__new") // new_dir_name = "__new";
	, cam_positions_{
		{cam_pos_front, {cam_pos_front, "pos_front", _("cam^pos_front")}},
		{cam_pos_right, {cam_pos_right, "pos_right", _("cam^pos_right")}}
	}
	, action_list_(nullptr)
	, cam_pos_(nposm)
{
	set_timer_interval(1000);
	script_.clear();
}

void tnew_wkoscript::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	find_widget<tlabel>(window_, "title", false).set_label(_("file^New from benchmark"));

	tbutton* button = find_widget<tbutton>(window_, "back", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tnew_wkoscript::click_back
			, this
			, std::ref(*button)));
	button->set_icon("misc/back.png");

	tlabel* label = find_widget<tlabel>(window_, "remark", false, true);
	utils::string_map symbols;
	symbols["working_dir"] = _("Working directory");
	label->set_label(vgettext2("generate_wkoscript_from_benchmarks remark, $working_dir", symbols));

	button = find_widget<tbutton>(window_, "working_dir", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tnew_wkoscript::click_working_dir
			, this
			, std::ref(*button)));
	if (game_config::os != os_windows) {
		button->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(window_, "new_dirs", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tnew_wkoscript::click_new_dirs
			, this
			, std::ref(*button)));
	std::set<std::string> new_dirs;
	aplt::list_wkoscript_files_by_type(wkoscript_dir_, aplt::type_wkoscript_new_dirs, new_dirs);
	if (!new_dirs.empty()) {
		// curr_new_dir_ = *new_dirs.begin();
		// button->set_label(curr_new_dir_);
	}

	button = find_widget<tbutton>(window_, "generate", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tnew_wkoscript::click_generate
			, this
			, std::ref(*button)));
	if (curr_new_dir_.empty()) {
		button->set_active(false);
	}
	generate_widget_ = button;

	treport* report = find_widget<treport>(window_, "cam_position", false, true);
	for (std::map<int, tcode3>::const_iterator it = cam_positions_.begin(); it != cam_positions_.end(); ++ it) {
		const tcode3& pos = it->second;
		report->insert_item(null_str, pos.name).set_cookie(pos.code);
	}
	report->set_did_item_changed(std::bind(&tnew_wkoscript::did_cam_pos_changed, this, _2));
	report->select_item(0);
	// pose_type_report_ = report;

	tlistbox* list = find_widget<tlistbox>(window_, "action_list", false, true);
	list->enable_select(false);
	// list->set_did_row_pre_change(std::bind(&tcourseware2::did_file_list_row_pre_change, this, _1, _2));
	// list->set_did_row_changed(std::bind(&tcourseware2::did_file_list_row_changed, this, _1, _2));
	// list->set_did_can_drag(std::bind(&tcourseware2::did_file_list_can_drag, this, _1, _2));
	action_list_ = list;
}

void tnew_wkoscript::post_show()
{
}

void tnew_wkoscript::click_back(tbutton& widget)
{
	std::string err_msg;
	if (action_list_->rows() == 0) {
		err_msg = _("No script generated. Cannot enter the state graph.");
	}

	if (err_msg.empty()) {
		uint64_t ret = script_.is_valid2(err_msg, nullptr);
		if (ret != TCOOKIE3F_CHECK_OK) {
			err_msg = _("There are images where landmarks could not be recognized. Please replace them first.");
		}
	}
	if (!err_msg.empty()) {
		gui2::show_message(null_str, err_msg);
		return;
	}

	int retval = twindow::OK;
	window_->set_retval(retval);
}

void tnew_wkoscript::click_working_dir(tbutton& widget)
{
	win_ShellExecuteW_open(wkoscript_dir_);
}

void tnew_wkoscript::click_new_dirs(tbutton& widget)
{
	std::set<std::string> new_dirs;
	aplt::list_wkoscript_files_by_type(wkoscript_dir_, aplt::type_wkoscript_new_dirs, new_dirs);

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	for (std::set<std::string>::const_iterator it = new_dirs.begin(); it != new_dirs.end(); ++ it) {
		const std::string& dir = *it;
		items.push_back(gui2::tmenu::titem(dir, items.size()));
		if (dir == curr_new_dir_) {
			initial_sel = items.back().val;
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
	std::set<std::string>::const_iterator curr_it = new_dirs.begin();
	if (cursel != 0) {
		std::advance(curr_it, cursel);
	}

	curr_new_dir_ = *curr_it;
	widget.set_label(curr_new_dir_);

	generate_widget_->set_active(true);
}

void tnew_wkoscript::did_cam_pos_changed(ttoggle_button& widget)
{
	cam_pos_ = widget.at();
}

void tnew_wkoscript::click_generate(tbutton& widget)
{
	const std::string& wkoscript_dir = wkoscript_dir_;

	VALIDATE(!curr_new_dir_.empty(), null_str);
	const std::string new_dir = wkoscript_dir + "/" + curr_new_dir_;

	actions_.clear();

	VALIDATE(cam_pos_ >= 0 && cam_pos_ < cam_pos_count, null_str);

	std::set<std::string> benchmarks;
	aplt::list_wkoscript_files_by_type(new_dir, aplt::type_wkoscript_new_benchmarks, benchmarks);
	std::map<uint64_t, std::string> v_benchmarks;
	for (std::set<std::string>::const_iterator it = benchmarks.begin(); it != benchmarks.end(); ++ it) {
		const std::string& png = *it;
		std::vector<std::string> v_str = utils::split(utils::file_stem_name(png), '_');
		VALIDATE(v_str.size() == 2, null_str);
		uint64_t key = posix_mku64(utils::to_int(v_str[0]), utils::to_int(v_str[1]));
		v_benchmarks.insert(std::make_pair(key, png));
	}

	int max_action_count = aplt::workoutn32_max_states / 2;
	// char buf[32];
	int benchmark_at = 0;
	const int benchmark_count = benchmarks.size();
	taction tmp_action;
	std::vector<taction>& actions = actions_;
	for (int at = 1; at < max_action_count; at ++) {
		tmp_action.clear();
		int phase_at = 0;
		for (; phase_at < WKO_MAX_PHASE_COUNT && benchmark_at < benchmark_count; phase_at ++) {
			uint64_t key = posix_mku64(at, phase_at);
			if (v_benchmarks.count(key) == 0) {
				break;
			}
			tmp_action.pngs.push_back(v_benchmarks.find(key)->second);
			benchmark_at ++;
		}

		VALIDATE((int)tmp_action.pngs.size() == phase_at, null_str);
		if (phase_at == 0) {
			// is end
			break;
		} else {
			tmp_action.task_type = phase_at == 1? aplt::twkoscript::tasktype_time_counter: aplt::twkoscript::tasktype_rep_counter;
			actions.push_back(tmp_action);
		}
	}

	if (actions.empty()) {
		return;
	}

	std::unique_ptr<mediapipe::tpose_tracking_api> api_ptr_;
	api_ptr_.reset(mediapipe::rose_create_pose_tracking_api());
	bool retbool = api_ptr_->graph_initialized();
	VALIDATE(retbool, null_str);

	utils::string_map symbols;
	aplt::twkoscript::tstate2* new_state2 = nullptr;
	aplt::twkoscript::ttask_speak* speak_task = nullptr;
	aplt::twkoscript::ttime_counter* time_counter = nullptr;
	aplt::twkoscript::trep_counter* rep_counter = nullptr;

	aplt::twkoscript& script = script_;
	script_.clear();
	std::set<std::string> existed;
	aplt::list_wkoscript_files_by_type(wkoscript_dir, aplt::type_wkoscript_ids, existed);

	script.id = utils::unique_untitle_id(existed, "workout", null_str, 1);
	script.title = utils::unique_untitle_name(std::set<std::string>(), null_str, 1);
	const std::string phase_surf_dir = script.build_phase_surf_dir(wkoscript_dir);
	SDL_MakeDirectory(phase_surf_dir.c_str());
	mediapipe::tpose_tracking_api& api = *api_ptr_.get();
	// SDL_FPoint landmark2s[WKO_MAX_PHASE_COUNT][mediapipe::kNumPoseLandmarks];
	// bool landmark2_valid[WKO_MAX_PHASE_COUNT];
	
	// 0: speak opening
	new_state2 = &script.insert_state(nposm, _("wko^Speak opening"), aplt::twkoscript::tasktype_speak, script.states.size());
	speak_task = static_cast<aplt::twkoscript::ttask_speak*>(new_state2->task);
	std::string msgstr;
	if (cam_pos_ == cam_pos_front) {
		speak_task->msgstr = _("wko^speak opening msgstr, cam_pos_front");
	} else if (cam_pos_ == cam_pos_right) {
		speak_task->msgstr = _("wko^speak opening msgstr, cam_pos_right");
	}
	// 1: setup
	new_state2 = &script.insert_state(nposm, _("wko^Setup"), aplt::twkoscript::tasktype_time_counter, script.states.size());
	{
		time_counter = static_cast<aplt::twkoscript::ttime_counter*>(new_state2->task);
		time_counter->rule = aplt::twkoscript::timerule_strict;
		time_counter->tone = aplt::twkoscript::timetone_full;
		// time_counter->max_count = 5;
/*
		const aplt::tpreset_pose& preset = preset_poses_.find(aplt::posemetric_body_tilt_angle)->second;
		const taction& action0 = actions[0];
		aplt::twkoscript::tpose& new_pose = insert_preset_pose(preset, action0.landmarks[0], action0.landmark_valid[0], *new_state2);
		new_pose.phase_mask = BIT_IDX_MASK(0);
*/
	}

	for (std::vector<taction>::iterator it = actions.begin(); it != actions.end(); ++ it) {
		taction& action = *it;
		// make this script can update lmk33_png. requrie set statd2.lmk33_png_at

		symbols["number"] = str_cast(script.states.size() / 2);
		std::string state_name = vgettext2("wko^$number action opening", symbols);
		std::string msgstr = vgettext2("wko^$number action msgstr", symbols);
		
		VALIDATE(!state_name.empty() && !msgstr.empty(), null_str);
		new_state2 = &script.insert_state(nposm, state_name, aplt::twkoscript::tasktype_speak, script.states.size());
		speak_task = static_cast<aplt::twkoscript::ttask_speak*>(new_state2->task);
		speak_task->msgstr = msgstr;

		state_name = vgettext2("wko^$number action", symbols);
		new_state2 = &script.insert_state(nposm, state_name, action.task_type, script.states.size());
		action.state_at = new_state2->state;
		// copy lmk33.png
		int png_count = action.pngs.size();
		for (int at = 0; at < png_count; at ++) {
			const std::string src_png = new_dir + "/" + action.pngs[at];
			// surface surf = image::get_image(src_png);
			surface surf = image::get_image_withoutcache(src_png);
			if (surf.get() != nullptr) {
				// landmark2_valid[at] = calc_surf_landmarks(api, surf, landmark2s[at]);
				surf = save_lmk33_png_from_surf(surf, *new_state2, phase_surf_dir, at);
				action.landmark_valid[at] = calc_surf_landmarks(api, surf, action.landmarks[at]);

				SDL_Log("lamdmarks(state: %i, phase_at: %i) => %s", new_state2->state, at, action.landmark_valid[at]? "true": "false");

			} else {
				action.landmark_valid[at] = false;
			}
		}
		// insert poses
#define MAX_METRICS		8
		int recommand_metrics[MAX_METRICS];
		int metric_count = 0;
		if (cam_pos_ == cam_pos_front) {
			recommand_metrics[metric_count ++] = aplt::posemetric_body_tilt_angle;
			recommand_metrics[metric_count ++] = aplt::posemetric_header_tilt_angle;

			recommand_metrics[metric_count ++] = aplt::posemetric_left_elbow_angle;
			recommand_metrics[metric_count ++] = aplt::posemetric_right_elbow_angle;

			recommand_metrics[metric_count ++] = aplt::posemetric_left_forearm_angle;
			recommand_metrics[metric_count ++] = aplt::posemetric_right_forearm_angle;

			// recommand_metrics[metric_count ++] = aplt::posemetric_left_wrist_on_right_shoulder;
			// recommand_metrics[metric_count ++] = aplt::posemetric_right_wrist_on_left_shoulder;

		} else {
			VALIDATE(cam_pos_ == cam_pos_right, null_str);
			recommand_metrics[metric_count ++] = aplt::posemetric_right_upper_arm_angle;
			recommand_metrics[metric_count ++] = aplt::posemetric_right_elbow_angle;
			recommand_metrics[metric_count ++] = aplt::posemetric_right_hip_angle;
			recommand_metrics[metric_count ++] = aplt::posemetric_right_torso_angle;
			recommand_metrics[metric_count ++] = aplt::posemetric_right_thigh_angle;
		}
		new_state2->track_pose.poses.clear();

		for (int phase_at = 0; phase_at < png_count; phase_at ++) {
			for (int metric_at = 0; metric_at < metric_count; metric_at ++) {
				VALIDATE(preset_poses_.count(metric_at) != 0, null_str);
				int metric = recommand_metrics[metric_at];
				const aplt::tpreset_pose& preset = preset_poses_.find(metric)->second;

				// SDL_Log("{dbg-preset}state: %i, phase_at: %i, metric; %i", new_state2->state, phase_at, preset.metric);
				aplt::twkoscript::tpose& new_pose = insert_preset_pose(preset, action.landmarks[phase_at], action.landmark_valid[phase_at], *new_state2);
				new_pose.phase_mask = BIT_IDX_MASK(phase_at);
			}
		}

		if (new_state2->task->type == aplt::twkoscript::tasktype_time_counter) {
			// new_state2->unsatisfied_2th_msgstr = "unsatisfied_2th";
			time_counter = static_cast<aplt::twkoscript::ttime_counter*>(new_state2->task);
			time_counter->max_count = 10;

		} else {
			VALIDATE(new_state2->task->type == aplt::twkoscript::tasktype_rep_counter, null_str);
			rep_counter = static_cast<aplt::twkoscript::trep_counter*>(new_state2->task);
			rep_counter->max_count = 10;
		}
	}

	new_state2 = &script.insert_state(nposm, _("Already ended"), aplt::twkoscript::tasktype_speak, script.states.size());
	speak_task = static_cast<aplt::twkoscript::ttask_speak*>(new_state2->task);
	speak_task->msgstr = _("wko^speak ended msgstr");
	speak_task->repeat_s = 10;

	for (std::map<int, aplt::twkoscript::tstate2>::iterator it = script.states.begin(); it != script.states.end(); ++ it) {
		aplt::twkoscript::tstate2& state2 = it->second;
		if (state2.state < (int)(script.states.size() - 1)) {
			state2.set_next_to_state(state2.state + 1);
		}
	}

	std::string err_msg;
	uint64_t retval = script.is_valid2(err_msg, nullptr, false);

	VALIDATE(script.is_valid2(err_msg, nullptr, false) == TCOOKIE3F_CHECK_OK, null_str);

	reload_action_list(*action_list_);
}

void tnew_wkoscript::reload_action_list(tlistbox& list)
{
	list.clear();

	VALIDATE(!curr_new_dir_.empty(), null_str);
	const std::string new_dir = wkoscript_dir_ + "/" + curr_new_dir_;
	aplt::twkoscript& script = script_;
	const std::string phase_surf_dir = script.build_phase_surf_dir(wkoscript_dir_);

	char buf[32];
	std::vector<tformula_blit> blits;
	std::map<std::string, std::string> data;
	for (int at = 0; at < actions_.size(); at ++) {
		const taction& action = actions_.at(at);

		data["number"] = str_cast(at + 1);
		data["task"] = aplt::wko_task_types.find(action.task_type)->second.name;

		ttoggle_panel& row = list.insert_row(data);

		for (int phase_at = 0; phase_at < action.pngs.size(); phase_at ++) {
			const std::string& png = action.pngs[phase_at];

			SDL_snprintf(buf, sizeof(buf), "phase_png%i", phase_at);
			tbutton* png_widget = find_widget<tbutton>(&row, buf, false, true);

			aplt::twkoscript::tstate2& state2 = script.states.find(action.state_at)->second;
			std::string filename = state2.build_lmk33_png_filename(phase_surf_dir, phase_at);

			surface surf = image::get_image_withoutcache(filename);

			blits.clear();
			blits.push_back(gui2::tformula_blit(surf, null_str, null_str, "(width)", "(height)"));
			uint32_t color = action.landmark_valid[phase_at]? color_to_uint32(font::GOOD_COLOR): color_to_uint32(font::BAD_COLOR);
			blits.push_back(gui2::tformula_blit(image::BLITM_FRAME, null_str, null_str, "(width)", "(height)", color));
			png_widget->set_blits(blits);
		}
	}
	// operand_list_->select_row(new_sel);
}

void tnew_wkoscript::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

