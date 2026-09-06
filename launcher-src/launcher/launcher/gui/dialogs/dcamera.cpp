#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/dcamera.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "font.hpp"
#include "formula_string_utils.hpp"
#include "base_instance.hpp"

#include <opencv2/opencv.hpp>
#include "qr_code.hpp"
#include "dcamera_driver.hpp"

using namespace std::placeholders;


bool aplt_task_id2_is_valid(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const std::string& task_id2)
{
	return aplt::split_aplt_task_id2(applets, task_id2, true).aplt != nullptr;
}

std::string preferences_moveit_task_id2(const std::map<aplt::taplt_key, aplt::tapplet>& applets)
{
	std::string task_id2 = preferences::moveit_task_id2();
	if (!aplt_task_id2_is_valid(applets, task_id2)) {
		task_id2.clear();
	}
	return task_id2;
}

namespace gui2 {

REGISTER_DIALOG(launcher, dcamera)

tdcamera::tdcamera(std::map<aplt::taplt_key, aplt::tapplet>& applets, net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tdcamera_driver& dcamera_driver, tdrivers& drivers, 
	aplt::tbg_task2& bg_task2, tros_instance& ros_instance, tcamera& camera, std::unique_ptr<tmoveit_aplt_task>& moveit_aplt_task)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, tdcamera_slot_impl(applets, dcamera_driver, ros_instance, camera)
	, dcamera_driver_(dcamera_driver)
	, base_driver_(ros_instance.base_driver())
	, moveit_driver_(ros_instance.moveit_driver())
	, drivers_(drivers)
	, bg_task2_(bg_task2)
	, ros_instance_(ros_instance)
	, camera_(camera)
	, moveit_aplt_task_(moveit_aplt_task)
	, case_(nposm)
	, original_camera_flags_(camera.get_flags())
	, use_mediapipe_(fake_libkosapi_so) // fake_libkosapi_so
	, spent_ms_(nposm)
	, total_valid_frames_(0)
	, total_spent_ms_(0)
	, mediapipe_flip_h_(false)
	, new_frame_state_(nposm)
	, save_work_frame_(false)
	, start_widget_(nullptr)
	, moveit_aplt_widget_(nullptr)
	, paper_(nullptr)
	, reach_dcpitch_widget_(nullptr)
	, set_ik_diff_widget_(nullptr)
	, status_widget_(nullptr)
	, rpy_widget_(nullptr)
	, highlight_ticks_(0)
	, offset_percent_threshold_(5) // width * 5%
	, base_scene_result_cancel_(false)
	, set_vlcsnap_surf_(false)
	, vlcsnap_surf_times_(0)
{
	set_timer_interval(200);

	if (moveit_aplt_task.get() != nullptr || instance->bg_task().in_task_cpp2()) {
		case_ = case_moveit;
		// if before navigation is fail, ros_instance_.has_task() is false. but ros_instance_.started_ is true.
		// VALIDATE(ros_instance_.has_task(), null_str);
		VALIDATE(ros_instance_.started(), null_str);

	} else if (!base_driver_.scene_id().empty()) {
		// sts_ing or sts_idle
		case_ = case_base_subtask;
	}

	if (case_ != case_base_subtask) {
		disable_new_aplt_lock_.reset(new aplt::tdisable_new_klink_task_lock(aplt::tdisable_new_klink_task_lock::reason_camera));
	}

	// enum {moveit_op_grasp, moveit_op_press_top, moveit_op_press_middle, moveit_op_count};
	moveit_ops_.insert(std::make_pair(moveit_op_grasp, "grasp"));
	moveit_ops_.insert(std::make_pair(moveit_op_press_top, "press_top"));
	moveit_ops_.insert(std::make_pair(moveit_op_press_middle, "press_middle"));
	VALIDATE(moveit_ops_.size() == moveit_op_count, null_str);

	if (case_ == nposm) {
		if (use_mediapipe_) {
			mediapipe_api_ptr_.reset(mediapipe::rose_create_pose_tracking_api());
			if (!mediapipe_api_ptr_->graph_initialized()) {
				SDL_Log("%s", _("Initialize mediapipe graph fail"));
			}
		}

		camera_.set_slot(this);

		if (dcamera_driver_.installed() && moveit_driver_.installed()) {
			ros_instance_.register_slot(*this);
			ros_instance_.start_moveit_node(false);

			moveit_calculator_.deps_0lock().reset(new tdeps_0lock(ros_instance_.drivers()));
		}

	} else if (case_ == case_base_subtask) {
		// tcamera::tslot& slot = get_camera_slot();
		// VALIDATE(&slot == &base_driver_, null_str);

		if (camera_.tasking()) {
			VALIDATE(camera_.curr_taskid() == tcamera::taskid_base_subtask, null_str);
			camera_.set_flags(camera_.get_flags() & ~tcamera::FLAG_SHOW_CANCEL);
		}

		base_driver_.set_camera_viewer(this);
		bg_task2_.set_camera_viewer(this);
	}
}

tdcamera::~tdcamera()
{
	if (case_ == nposm) {
		if (dcamera_driver_.installed() && moveit_driver_.installed()) {
			ros_instance_.deregister_slot(*this);
		}

	} else if (case_ == case_base_subtask) {
		bg_task2_.set_camera_viewer(nullptr);
		base_driver_.set_camera_viewer(nullptr);

		if (camera_.tasking() && camera_.curr_taskid() == tcamera::taskid_base_subtask) { 
			camera_.set_flags(camera_.get_flags() | tcamera::FLAG_SHOW_CANCEL);
		}
	}

	VALIDATE(base_driver_.camera_viewer() == nullptr, null_str);
	VALIDATE(bg_task2_.camera_viewer() == nullptr, null_str);

	// VALIDATE(camera_.get_flags() == original_camera_flags_, null_str);
}

void tdcamera::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	find_widget<tlabel>(window_, "title", false, true)->set_label(aplt::all_fake_applets.find(aplt::builtinid_dcamera)->second.name);

	if (disable_new_aplt_lock_.get() != nullptr) {
		find_widget<tlabel>(window_, "warnning", false, true)->set_label(ht::generate_format(disable_timing_warnning(), 0xffff0000));
	}
	set_ik_diff_widget_ = find_widget<tlabel>(window_, "set_ik_diff", false, true);
	status_widget_ = find_widget<tlabel>(window_, "camera_k", false, true);
	rpy_widget_ = find_widget<tlabel>(window_, "rpy", false, true);

	paper_ = find_widget<ttrack>(window_, "paper", false, true);
	ttrack& paper = *paper_;
	paper.set_did_draw(std::bind(&tdcamera::did_draw_paper, this, _1, _2, _3));
	paper.set_did_left_button_down(std::bind(&tcamera::did_left_button_down_paper, &camera_, _1, _2));
	paper.set_did_mouse_motion(std::bind(&tcamera::did_mouse_motion_paper, &camera_, _1, _2, _3));
	paper.set_did_mouse_leave(std::bind(&tdcamera::did_mouse_leave_paper, this, _1, _2, _3));

	// paper_->set_did_create_background_tex(std::bind(&tdcamera::did_create_background_tex, this, _1, _2));

	tbutton* button = find_widget<tbutton>(window_, "start", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tdcamera::click_start
			, this, std::ref(*button)));
	start_widget_ = button;

	button = find_widget<tbutton>(window_, "moveit_aplt", false, true);
	button->set_border("textbox");
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tdcamera::click_moveit_aplt
			, this, std::ref(*button)));
	const std::string curr_task_id2 = preferences_moveit_task_id2(applets_);
	if (curr_task_id2.empty()) {
		start_widget_->set_active(false);
	}
	button->set_label(curr_task_id2);
	moveit_aplt_widget_ = button;

	button = find_widget<tbutton>(window_, "dctask", false, true);
	button->set_border("textbox");
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tdcamera::click_view
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "snapshot_depth", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tdcamera::click_snapshot_depth
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "snapshot", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tdcamera::click_snapshot
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "dbg", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tdcamera::click_dbg_RP
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "reach_dcpitch", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tdcamera::click_reach_dcpitch
			, this, std::ref(*button)));
	reach_dcpitch_widget_ = button;

	if (case_ == nposm) {
		if (dcamera_driver_.installed() && !dcamera_driver_.main_tasking()) {
			int def_depth_task = dctask_d2c;
			dcamera_driver_.set_desire_depth_task(def_depth_task);
		}

		if (!moveit_driver_.installed()) {
			start_widget_->set_visible(twidget::HIDDEN);

			reach_dcpitch_widget_->set_visible(twidget::INVISIBLE);
		}

		bool no_swap_wh_for_screen = false;
		camera_.enter_task(tcamera::taskid_dcamera, 0, no_swap_wh_for_screen);

	} else {
		if (case_ == case_moveit) {
			if (moveit_aplt_task_.get() != nullptr) {
				set_did_draw_slice_bh_in_viewer();

			} else {
				VALIDATE(instance->bg_task().in_task_cpp2(), null_str);
			}

		} else {
			VALIDATE(case_ == case_base_subtask, null_str);

			if (!set_vlcsnap_surf_) {
				moveit_aplt_widget_->set_visible(twidget::HIDDEN);
			}
		}

		start_widget_->set_visible(twidget::HIDDEN);
		find_widget<tgrid>(window_, "ctrl_grid", false, true)->set_visible(twidget::INVISIBLE);

		// requrie set timer_interval here. else did_draw_paper will not called.
		paper_->set_timer_interval(30);
	}
}

void tdcamera::post_show()
{
	if (case_ == nposm) {
		if (!base_scene_result_cancel_) {
			camera_.exit_task(tcamera::taskid_dcamera);
			camera_.set_slot(nullptr);
		}

		if (mediapipe_api_ptr_.get() != nullptr) {
			mediapipe_api_ptr_.reset();
		}

	} else if (moveit_aplt_task_.get() != nullptr) {
		paper_->set_timer_interval(0);

		moveit_aplt_task_->set_did_draw_slice_bh(NULL);
	}
}

tcamera::tslot& tdcamera::get_camera_slot()
{
	VALIDATE(case_ == case_base_subtask, null_str);

	tcamera::tslot* slot = &bg_task2_;
	if (base_driver_.subtask_state() == aplt::sts_ing || base_driver_.subtask_state() == aplt::sts_idle) {
		slot = &base_driver_;
	}
	return *slot;
}

void tdcamera::set_did_draw_slice_bh_in_viewer()
{
	VALIDATE(case_ == case_moveit, null_str);
	VALIDATE(moveit_aplt_task_.get() != nullptr, null_str);

	moveit_aplt_task_->set_did_draw_slice_bh(std::bind(&tdcamera::did_draw_slice_bh, this, _1, _2, _3, _4, _5, _6));
}

void tdcamera::click_start(tbutton& widget)
{
	VALIDATE(case_ == nposm, null_str);

	if (moveit_calculator_.operating()) {
		moveit_calculator_.stop_operate();
        did_operate_stopped();
		return;
	}

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	for (int scene = 0; scene < tmoveit_calculator::scene_count; scene ++) {
		items.push_back(gui2::tmenu::titem(moveit_calculator_.get_scene_desc(scene).name, scene));
	}

	if (items.empty()) {
		return;
	}

	int hit_scene = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		hit_scene = dlg.selected_val();
	}

	const std::string task_id2 = preferences::moveit_task_id2();
	aplt::ttask_pair pair = aplt::split_aplt_task_id2(applets_, task_id2, true);
	if (pair.aplt == nullptr || pair.task == nullptr) {
		utils::string_map symbols;
		symbols["task"] = task_id2;
		gui2::show_message(null_str, vgettext2("Cannot find moveit task: $task", symbols));
		return;
	}
	aplt::tapplet& aplt = *const_cast<aplt::tapplet*>(pair.aplt);
	const aplt::tapplet::ttask& task = *pair.task;

	aplt::ttask_api* task_api = drivers_.create_task_api(aplt);
	if (task_api == nullptr) {
		gui2::show_message(null_str, "create_task_api fail");
		return;
	}

	if (!ros_instance_.get_imu_rpy().valid) {
		gui2::show_message(null_str, "Start fail. No IMU.");
		return;
	}

    start_widget_->set_label("misc/stop96.png");
	moveit_aplt_widget_->set_active(false);

	ros_instance_.do_light_group_values_target(aplt::tmoveit_slot::state_recognize);
	moveit_calculator_.start_operate(hit_scene, *task_api, aplt, task);
}

void tdcamera::did_operate_stopped()
{
	VALIDATE(case_ == nposm, null_str);

	start_widget_->set_label("misc/start.png");
	moveit_aplt_widget_->set_active(true);

	if (moveit_calculator_.operate().scene == tmoveit_calculator::scene_calibrate_near) {
		if (!is_float_nposm(moveit_calculator_.operate().calibrate_near_PRP_diff.x)) {
			const geometry_msgs::Point& initial_fk = moveit_calculator_.operate().initial_fk.position;
			const geometry_msgs::Point& near_fk = moveit_calculator_.operate().near_fk.position;
			SDL_DPoint3 fk_diff{near_fk.x - initial_fk.x, near_fk.y - initial_fk.y, near_fk.z - initial_fk.z};

			tmsg_data_calibrate_near* pdata = new tmsg_data_calibrate_near(moveit_calculator_.operate().calibrate_near_PRP_diff, fk_diff);
			rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_CALIBRATE_NEAR, pdata);
		}
	}
}

void tdcamera::click_moveit_aplt(tbutton& widget)
{
	if (set_vlcsnap_surf_) {
		std::string filename;
		bool even = (vlcsnap_surf_times_ & 1) == 0;
		if (even) {
			filename = game_config::preferences_dir + "/saves/vlcsnap-1-0.png";
			// filename = game_config::preferences_dir + "/saves/vlcsnap-2-0.png";
			// filename = game_config::preferences_dir + "/saves/vlcsnap-3-0.png";
		} else {
			filename = game_config::preferences_dir + "/saves/vlcsnap-1-1.png";
			// filename = game_config::preferences_dir + "/saves/vlcsnap-2-1.png";
			// filename = game_config::preferences_dir + "/saves/vlcsnap-3-1.png";
		}
		vlcsnap_surf_times_ ++;
		surface surf = image::rose_get_image(filename);
		VALIDATE(surf.get() != nullptr, null_str);

		camera_.get_avcapture().set_vlcsnap_surface(surf);
		return;
	}

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	const std::string curr_task_id2 = preferences_moveit_task_id2(applets_);

	std::vector<std::string> task_id2s;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it2 = aplt.tasks.begin(); it2 != aplt.tasks.end(); ++ it2) {
			const aplt::tapplet::ttask& timing = it2->second;
			if (timing.type != aplt::task_moveit) {
				continue;
			}
			task_id2s.push_back(utils::join_app_prefix_id(aplt.bundleid, timing.id));
			items.push_back(gui2::tmenu::titem(task_id2s.back(), items.size()));

			if (curr_task_id2 == task_id2s.back()) {
				initial_sel = items.size() - 1;
			}
		}
	}

	if (items.empty()) {
		return;
	}

	std::string new_task_id2;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		new_task_id2 = task_id2s[dlg.selected_val()];
	}

	preferences::set_moveit_task_id2(new_task_id2);
	widget.set_label(new_task_id2);

	start_widget_->set_active(true);
}

void tdcamera::click_view(tbutton& widget)
{
	if (!dcamera_driver_.installed()) {
		return;
	}

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	int curr_task = dcamera_driver_.main_curr_task();

	for (int task = 0; task < dctask_count; task ++) {
		std::string name;
		if (task == dctask_color) {
			name = _("Only color");

		} else if (task == dctask_depth) {
			name = _("View depth");

		} else {
			VALIDATE(task == dctask_d2c, null_str);
			name = _("View d2c");
		}
		items.push_back(gui2::tmenu::titem(name, task));

		if (dcamera_driver_.main_curr_task() == task) {
			initial_sel = task;
		}
	}

	if (items.empty()) {
		return;
	}

	int task = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		task = dlg.selected_val();
		VALIDATE(task >= 0 && task < dctask_count, null_str);
	}


	if (dcamera_driver_.main_tasking()) {
		camera_.exit_task(tcamera::taskid_dcamera);
	}

	dcamera_driver_.set_desire_depth_task(task);
	camera_.enter_task(tcamera::taskid_dcamera, 0, false);
}

bool gbackground_tex_dirty = false;

void tdcamera::click_snapshot_depth(tbutton& widget)
{
	int task = dcamera_driver_.main_curr_task();

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	enum {coor_camera, coor_world};
	items.push_back(gui2::tmenu::titem("camera coor", coor_camera));
	items.push_back(gui2::tmenu::titem("world coor", coor_world));

	if (items.empty()) {
		return;
	}

	int coor = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		coor = dlg.selected_val();
	}


	std::string png;
	if (task == dctask_depth) {
		if (coor == coor_camera) {
			png = "dcamera_depth_camera.png";
		} else {
			VALIDATE(coor == coor_world, null_str);
			png = "dcamera_depth_world.png";
		}

	} else if (task == dctask_d2c) {
		if (coor == coor_camera) {
			png = "dcamera_d2c_camera.png";
		} else {
			VALIDATE(coor == coor_world, null_str);
			png = "dcamera_d2c_world.png";
		}
	}
	VALIDATE(!png.empty(), null_str);

	camera_.snapshot_depth(coor == coor_world, png, short_dbg_normal_depth_data_dat);

	gbackground_tex_dirty = true;
	// paper_->set_background_tex_dirty();
}

void tdcamera::click_snapshot(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	enum {type_work_frame, type_last_image};
	items.push_back(gui2::tmenu::titem("work frame", type_work_frame));
	items.push_back(gui2::tmenu::titem("last image", type_last_image));

	if (items.empty()) {
		return;
	}

	int type = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		type = dlg.selected_val();
	}

	if (type == type_work_frame) {
		save_work_frame_ = true;

	} else {
		VALIDATE(type == type_last_image, null_str);
		const surface& surf = camera_.last_image();
		if (surf.get() == nullptr) {
			return;
		}

		time_t t = time(nullptr);
		const std::string filename("saves/snapshot-" + utils::format_time_ymdhms2(t) + ".png");
		imwrite(surf, filename);

		char buf[128];
		SDL_snprintf(buf, sizeof(buf), "save to %s", filename.c_str());
		set_highlight(buf, 5000);

	}
}

void tdcamera::click_dbg_RP(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	enum {type_RP, type_depthdata_2_depth_png, type_depthdata_2_depth_all_rows_png, type_depthdata_2_d2c_png};
	items.push_back(gui2::tmenu::titem("dbg RP", type_RP));
	ss.str("");
	ss << short_dbg_normal_depth_data_dat << " to dbg_depth.png";
	items.push_back(gui2::tmenu::titem(ss.str(), type_depthdata_2_depth_png));
	ss.str("");
	ss << short_dbg_normal_depth_data_dat << " to dbg_depth_all_rows.png";
	items.push_back(gui2::tmenu::titem(ss.str(), type_depthdata_2_depth_all_rows_png));
	ss.str("");
	ss << short_dbg_normal_depth_data_dat << " to dbg_d2c.png";
	items.push_back(gui2::tmenu::titem(ss.str(), type_depthdata_2_d2c_png));

	if (items.empty()) {
		return;
	}

	int type = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		type = dlg.selected_val();
	}

	if (type == type_RP) {
		moveit_calculator_.dbg_find_reference_point();

	} else if (type == type_depthdata_2_depth_png || type == type_depthdata_2_depth_all_rows_png) {
		moveit_calculator_.dbg_depthdata_2_png(false, type == type_depthdata_2_depth_all_rows_png);

	} else if (type == type_depthdata_2_d2c_png) {
		moveit_calculator_.dbg_depthdata_2_png(true, false);
	}

}

void tdcamera::click_reach_dcpitch(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	for (int deg = 30; deg < 80; deg += 5) {
		items.push_back(gui2::tmenu::titem(str_cast(deg), deg));
	}

	if (items.empty()) {
		return;
	}

	int hit_deg = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		hit_deg = dlg.selected_val();
	}

	const double desire_dcpitch = DEG2RAD(hit_deg);
	moveit_calculator_.reach_dcpitch_use_PRP_joint("gui click", desire_dcpitch);

	char buf[128];
	SDL_snprintf(buf, sizeof(buf), "reach dcpitch: %i", hit_deg);
	set_highlight(buf, 5000);
}


void tdcamera::set_highlight(const std::string& msg, int threshold)
{
	VALIDATE(!msg.empty(), null_str);
	VALIDATE(threshold >= 0, null_str);

	highlight_msg_ = msg;
	highlight_ticks_ = SDL_GetTicks() + threshold;
}

void tdcamera::set_set_ik_diff_label(double x_diff, double z_diff)
{
	char buf[64] = "---";
	if (!is_float_nposm(x_diff)) {
		SDL_snprintf(buf, sizeof(buf), "%.6f, %.6f", x_diff, z_diff);
	}
	set_ik_diff_widget_->set_label(buf);
}

void tdcamera::set_status_label(const std::string& msg)
{
	status_widget_->set_label(msg);
}

void tdcamera::set_rpy_label()
{
	rpy_widget_->set_label(ros_instance_.get_imu_desc());
}

texture tdcamera::did_create_background_tex(ttrack& widget, const SDL_Rect& draw_rect)
{
	surface bg_surf = create_neutral_surface(draw_rect.w, draw_rect.h);
	// uint32_t bg_color = 0xfffdfdfd;
    uint32_t bg_color = 0xffff0000;

	if (gbackground_tex_dirty) {
		bg_color = 0xff00ff00;
		gbackground_tex_dirty = false;
	}

	fill_surface(bg_surf, bg_color);

    SDL_Renderer* renderer = get_renderer();

	return SDL_CreateTextureFromSurface2(renderer, bg_surf.get());
}

void tdcamera::did_draw_paper(gui2::ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();

	if (case_ == nposm) {
		if (camera_.is_avcapture_started()) {
			camera_.slice(widget_rect, true);
			// avcapture_->draw_slice(renderer, widget_rect);

		} else if (!start_avcapture_message_.empty()) {
			surface text_surf = font::get_rendered_text(start_avcapture_message_, widget_rect.w, font::SIZE_DEFAULT, font::BAD_COLOR);
			// if (require_render) {
				SDL_Rect dst {widget_rect.x + (widget_rect.w - text_surf->w) / 2, widget_rect.y + (widget_rect.h - text_surf->h) / 2, text_surf->w, text_surf->h};
				render_surface(renderer, text_surf, nullptr, &dst);
			// }
		}

	} else if (case_ == case_moveit) {
		if (moveit_aplt_task_.get() != nullptr) {
			if (!moveit_aplt_task_->is_set_did_draw_slice_bh()) {
				set_did_draw_slice_bh_in_viewer();
			}
			camera_.slice(widget_rect, true);
		}

	} else if (case_ == case_base_subtask) {
		if (camera_.is_avcapture_started()) {
			camera_.slice(widget_rect, true);
		}
	}

	if (!work_highlight_msg_.empty()) {
		threading::lock lock(camera_.get_variable_mutex());
		set_highlight(work_highlight_msg_, 5000);
		work_highlight_msg_.clear();
	}

	// highlight message
	if (!highlight_msg_.empty()) {
		// new lighlight message task
		surface surf = font::get_rendered_text(highlight_msg_, 0, font::SIZE_DEFAULT, font::BAD_COLOR);
		highlight_tex_ = SDL_CreateTextureFromSurface2(renderer, surf);
		// now highlight_tex_ is highlight_msg_, can clear highlight_msg_.
		highlight_msg_.clear();
	}
	if (highlight_tex_.get() != nullptr) {
		int width2, height2;
		SDL_QueryTexture(highlight_tex_.get(), NULL, NULL, &width2, &height2);

		SDL_Rect dstrect{widget_rect.x + (widget_rect.w - width2) / 2, (widget_rect.y + widget_rect.h - height2) / 2, width2, height2};
		SDL_RenderCopy(renderer, highlight_tex_.get(), nullptr, &dstrect);

		if (highlight_ticks_ != 0 && SDL_GetTicks() >= highlight_ticks_) {
			highlight_tex_ = nullptr;
			highlight_ticks_ = 0;
		}
	}
}

void tdcamera::did_mouse_leave_paper(gui2::ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(last)) {
		return;
	}

	camera_.did_mouse_leave_paper(widget, first, last);
}

void tdcamera::did_draw_slice_bh(tmoveit_calculator& calculator, trtc_client::VideoRenderer& vsink, const SDL_Rect& draw_rect, const std::string& msg, const std::vector<SDL_2Point>& new_qrcode_corners, const std::vector<SDL_Rect>&reference_rects)
{
	SDL_Renderer* renderer = get_renderer();

	ttexture_2_mat_lock mat_lock(vsink.tex_);

	const cv::Mat& frame = mat_lock.mat;

	if (!msg.empty()) {
		set_highlight(msg, 5000);
	}

	const tpoint ratio_size = calculate_adaption_ratio_size(draw_rect.w, draw_rect.h, frame.cols, frame.rows);
	const SDL_Rect video_dst {draw_rect.x + (draw_rect.w - ratio_size.x) / 2, draw_rect.y + (draw_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};

	SDL_RenderCopy(renderer, vsink.tex_.get(), nullptr, &video_dst);

	camera_.render_overlay_texs(renderer, video_dst);

	double xratio = 1.0 * video_dst.w / frame.cols;
	double yratio = 1.0 * video_dst.h / frame.rows;

	for (std::vector<SDL_Rect>::const_iterator it = reference_rects.begin(); it != reference_rects.end(); ++ it) {
		const SDL_Rect& src = *it;
		SDL_Rect rect{video_dst.x + (int)(src.x * xratio), video_dst.y + (int)(src.y * yratio), src.w, src.h};
		render_rect_frame(renderer, rect, 0xffff0000, 1);

		// color_points.push_back(SDL_Point{src.x + src.w / 2, src.y + src.h / 2});
	}

	if (!new_qrcode_corners.empty()) {
		VALIDATE(new_qrcode_corners.size() == 4, null_str);

		// center
		render_line(renderer, 0xffff0000, video_dst.x + new_qrcode_corners[0].x1 * xratio, video_dst.y + new_qrcode_corners[0].y1 * yratio, 
			video_dst.x + new_qrcode_corners[1].x1 * xratio, video_dst.y + new_qrcode_corners[1].y1 * yratio);

		render_line(renderer, 0xffff0000, video_dst.x + new_qrcode_corners[1].x1 * xratio, video_dst.y + new_qrcode_corners[1].y1 * yratio, 
			video_dst.x + new_qrcode_corners[2].x1 * xratio, video_dst.y + new_qrcode_corners[2].y1 * yratio);

		render_line(renderer, 0xffff0000, video_dst.x + new_qrcode_corners[2].x1 * xratio, video_dst.y + new_qrcode_corners[2].y1 * yratio, 
			video_dst.x + new_qrcode_corners[3].x1 * xratio, video_dst.y + new_qrcode_corners[3].y1 * yratio);

		render_line(renderer, 0xffff0000, video_dst.x + new_qrcode_corners[3].x1 * xratio, video_dst.y + new_qrcode_corners[3].y1 * yratio, 
			video_dst.x + new_qrcode_corners[0].x1 * xratio, video_dst.y + new_qrcode_corners[0].y1 * yratio);
	}

	if (calculator.operating()) {
		const tmoveit_calculator::tscene_desc& scene = calculator.get_scene_desc(calculator.operate().scene);
		int width, height;
		SDL_Rect rect{video_dst.x, video_dst.y, 0, 0};
		if (scene.tex.get() != nullptr) {
			SDL_QueryTexture(scene.tex.get(), nullptr, nullptr, &width, &height);
			rect.w = width;
			rect.h = height;
			SDL_RenderCopy(renderer, scene.tex.get(), nullptr, &rect);

			rect.x += width;
		}

		texture& tex = calculator.get_state_tex(calculator.operate().state);
		SDL_QueryTexture(tex.get(), nullptr, nullptr, &width, &height);
		rect.w = width;
		rect.h = height;
		SDL_RenderCopy(renderer, tex.get(), nullptr, &rect);
	}


	threading::lock lock(camera_.get_variable_mutex());
	
	{
		// center
		render_line(renderer, 0xff00ff00, video_dst.x + frame.cols / 2 * xratio, video_dst.y + 0 * yratio, 
			video_dst.x + frame.cols / 2 * xratio, video_dst.y + frame.rows * yratio);

		render_line(renderer, 0xff00ff00, video_dst.x + 0 * xratio, video_dst.y + frame.rows / 2 * yratio, 
			video_dst.x + frame.cols * xratio, video_dst.y + frame.rows / 2 * yratio);

		// offset percent
		int pixel_offset = frame.cols / 2 - frame.cols * offset_percent_threshold_ / 100;
		render_line(renderer, 0xff0000ff, video_dst.x + pixel_offset * xratio, video_dst.y + 0 * yratio, 
			video_dst.x + pixel_offset * xratio, video_dst.y + frame.rows * yratio);

		pixel_offset = frame.cols / 2 + frame.cols * offset_percent_threshold_ / 100;
		render_line(renderer, 0xff0000ff, video_dst.x + pixel_offset * xratio, video_dst.y + 0 * yratio, 
			video_dst.x + pixel_offset * xratio, video_dst.y + frame.rows * yratio);
	}
}

void tdcamera::camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	VALIDATE(locals_count == 1, null_str);

	if (use_mediapipe_) {
		VALIDATE(case_ == nposm, null_str);
		if (new_frame_state_ == frame_ok) {
			// the latest image has 33kp.
			VALIDATE(total_valid_frames_ > 0, null_str);
			int avg_spent_ms = total_spent_ms_ / total_valid_frames_;
			char buf[32];
			SDL_snprintf(buf, sizeof(buf), "%i ms(avg: %i ms)", spent_ms_, avg_spent_ms);
			last_mediapipe_msg_ = buf;

			new_frame_state_ = nposm;
		}
		if (spent_ms_ != nposm) {
			std::vector<std::pair<float, SDL_Rect> > classifier_rects;
			camera_.set_aplt_overlay(last_mediapipe_msg_, classifier_rects);
		}
		if (mediapipe_api_ptr_.get() != nullptr) {
			camera_.camera_did_draw_slice_use_cv_frame(id, renderer, locals, locals_count, remotes, remotes_count, draw_rect);
		}
		return;
	}

	trtc_client::VideoRenderer& vsink = *locals[0];

	std::vector<SDL_2Point> new_qrcode_corners;
	std::vector<SDL_Rect> reference_rects;
	std::string msg = impl_did_draw_slice(vsink, new_qrcode_corners, reference_rects);

	did_draw_slice_bh(moveit_calculator_, vsink, draw_rect, msg, new_qrcode_corners, reference_rects);
}

bool tdcamera::camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb)
{
	threading::lock lock(mediapipe_mutex_);

	cv_argb = argb.clone();
	bool overlay = true;
	if (new_frame_state_ == frame_ok) {
		

	} else if (new_frame_state_ == frame_fail) {
		overlay = false;
	}

	if (overlay) {
		mediapipe::overlay_pose_landmarks(xy_landmarks_, mediapipe::kNumPoseLandmarks, mediapipe_flip_h_, cv_argb, nullptr);
	}
	return true;
}

void tdcamera::camera_work_frame(const surface& surf)
{
	VALIDATE(surf.get() != nullptr, null_str);

	if (mediapipe_api_ptr_.get() != nullptr) {
		tsurface_2_mat_lock mat_lock(surf);
		const cv::Mat& argb = mat_lock.mat;

		SDL_FPoint xy_landmarks[mediapipe::kNumPoseLandmarks];

		mediapipe::tpose_tracking_api& api = *mediapipe_api_ptr_.get();
		uint32_t start_ticks = SDL_GetTicks();
		bool is_less_than_min_interval = false;
		cv::Mat output_frame_mat = api.next_image(argb, mediapipe_flip_h_, xy_landmarks, is_less_than_min_interval);
		if (is_less_than_min_interval) {
			return;
		}
		int spent_ms = SDL_GetTicks() - start_ticks;

		// SDL_Log("%u, pose api.next_image, output_mat: (%i x %i), spent %i ms", SDL_GetTicks(), output_frame_mat.cols, output_frame_mat.rows, spent_ms);

		if (!output_frame_mat.empty()) {
			threading::lock lock(mediapipe_mutex_);
			VALIDATE(output_frame_mat.u != nullptr, null_str);

			memcpy(xy_landmarks_, xy_landmarks, sizeof(xy_landmarks));
			spent_ms_ = spent_ms;

			total_valid_frames_ ++;
			if (total_valid_frames_ > 0) {
				total_spent_ms_ += spent_ms_;
			}

			output_mat_ = output_frame_mat;
			new_frame_state_ = frame_ok;

		} else {
			// failed_ = true;
			new_frame_state_ = frame_fail;
		}
	}

	if (save_work_frame_) {
		time_t t = time(nullptr);
		const std::string filename = short_dbg_normal_surf_png;
		imwrite(surf, filename);

		char buf[128];
		SDL_snprintf(buf, sizeof(buf), "save work_frame to %s", filename.c_str());

		threading::lock lock(camera_.get_variable_mutex());
		work_highlight_msg_ = buf;
		save_work_frame_ = false;
	}

	tdcamera_slot_impl::camera_work_frame(surf);
}

void tdcamera::dcamera_did_OnFrame(int task, const tdcframe_C* frames, int count)
{
	char buf[256];

	VALIDATE(count != 0, null_str);

	if (task == dctask_color) {
		SDL_snprintf(buf, sizeof(buf), "%u {color}color_frame: %ix%i", SDL_GetTicks(), frames[0].width, frames[0].height);

	} else if (task == dctask_depth) {
		SDL_snprintf(buf, sizeof(buf), "%u {depth}depth_frame: %ix%i", SDL_GetTicks(), frames[0].width, frames[0].height);

	} else {
		VALIDATE(task == dctask_d2c, null_str);
		const tdcframe_C& color_frame = frames[dcframeidx_color];
        const tdcframe_C& depth_frame = frames[dcframeidx_depth];

		SDL_snprintf(buf, sizeof(buf), "%u {d2c}color_frame: %ix%i, depth_frame: %ix%i scale: %.3f", 
			SDL_GetTicks(), color_frame.width, color_frame.height, depth_frame.width, depth_frame.height, depth_frame.scale);
	}

	set_status_label(buf);
}

void tdcamera::camera_post_enter_task()
{
	VALIDATE(!moveit_calculator_.is_started(), null_str);
	start_avcapture_message_ = _("Starting Camera");

	VALIDATE(window_ != nullptr, null_str);
	if (window_->drawn()) {
		gui2::absolute_draw();
	}

	paper_->set_timer_interval(30);
}

void tdcamera::camera_pre_exit_task()
{
	paper_->set_timer_interval(0);
	if (use_calculator()) {
		moveit_calculator_.did_stopped();
	}
}

void tdcamera::camera_did_draw_slice_c(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	if (is_cpu_saver()) {
		return;
	}
	camera_.camera_did_draw_slice_use_cv_frame(id, renderer, locals, locals_count, remotes, remotes_count, draw_rect);
}

extern bool can_switch_to_camera_layer(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::tbg_task::tbase_bg_task2& sys_task);

void tdcamera::bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	if (case_ != case_base_subtask) {
		return;
	}

	if (can_switch_to_camera_layer(applets_, sys_task)) {
		tcamera::tslot& slot = get_camera_slot();
		VALIDATE(&slot == &base_driver_, null_str);
	}
}

void tdcamera::bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	if (case_ != case_base_subtask) {
		return;
	}

	if (can_switch_to_camera_layer(applets_, sys_task)) {
		tcamera::tslot& slot = get_camera_slot();
		VALIDATE(&slot == &base_driver_, null_str);
	}
}

void tdcamera::base_scene_state_changed(const aplt::tbase_scene& scene, int to_state)
{
	if (to_state == aplt::sts_ing) {
		VALIDATE(camera_.curr_taskid() == tcamera::taskid_base_subtask, null_str);
		camera_.set_flags(camera_.get_flags() & ~tcamera::FLAG_SHOW_CANCEL);
	}

	if (case_ == nposm) {
		base_scene_result_cancel_ = true;

		window_->set_retval(twindow::CANCEL);
	}
}

void tdcamera::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
	set_rpy_label();
}

void tdcamera::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case MSG_CALIBRATE_NEAR:
		{
			tmsg_data_calibrate_near* pdata = static_cast<tmsg_data_calibrate_near*>(msg->pdata);

			char buf[128];
			SDL_snprintf(buf, sizeof(buf), "(%.5f, %.5f, %.5f)", pdata->PRP_diff.x, pdata->PRP_diff.y, pdata->PRP_diff.z);
			utils::string_map symbols;
			symbols["PRP_diff"] = buf;
			SDL_snprintf(buf, sizeof(buf), "PRP_diff.x - fk_diff_.x(%.5f) = %.5f", 
				pdata->fk_diff.x, fabs(pdata->PRP_diff.x) - fabs(pdata->fk_diff.x));
			symbols["error_x"] = buf;
			gui2::show_message(null_str, vgettext2("PRP_diff: $PRP_diff\nerror_x: $error_x", symbols));
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

