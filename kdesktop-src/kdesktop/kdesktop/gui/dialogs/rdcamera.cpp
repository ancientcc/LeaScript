#define GETTEXT_DOMAIN "kdesktop-lib"

#include "gui/dialogs/rdcamera.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/wko_settings.hpp"
#include "gettext.hpp"
#include "font.hpp"
#include "formula_string_utils.hpp"
#include "base_instance.hpp"
#include "cairo2.hpp"

#include <opencv2/opencv.hpp>
#include "qr_code.hpp"
/*
#include "dcamera_driver.hpp"
*/

#include "health_controller.hpp"

using namespace std::placeholders;

timage_digits::timage_digits(int height)
	: std_width_(48)
	, std_height_(72)
	, std_colon_w_(16)
	, std_gap_w_(4)
	, has_hour_(false)
	, height_(nposm)
	, width_(nposm)
	, colon_w_(nposm)
	, gap_w_(nposm)
{
	apply_new_height(height == nposm? 72: height);
}

void timage_digits::apply_new_height(int height)
{
	VALIDATE(height > 0, null_str);
	if (height == height_) {
		return;
	}
	height_ = height;

	SDL_Rect roi{(std_height_ - std_width_) / 2, 0, std_width_, std_height_};
	double ratio = 1.0 * height_ / std_height_;
	width_ = std_width_ * ratio;
	colon_w_ = std_colon_w_ * ratio;
	gap_w_ = std_gap_w_ * ratio;

	char buf[32];
	surface surf;
	for (int at = 0; at < 10; at ++) {
		SDL_snprintf(buf, sizeof(buf), "misc/digit%i.png", at);
		surf = image::get_image(buf);
		VALIDATE(surf.get() != nullptr, null_str);
		VALIDATE(surf->w == std_height_ && surf->h == std_height_, null_str);

		surf = cut_surface(surf, roi);

		surf = scale_surface(surf, width_, height_);
		digit_surfs_.push_back(surf);
	}

	roi = SDL_Rect{(std_height_ - std_colon_w_) / 2, 0, std_colon_w_, std_height_};
	surf = image::get_image("misc/colon.png");
	VALIDATE(surf.get() != nullptr, null_str);
	VALIDATE(surf->w == std_height_ && surf->h == std_height_, null_str);
	surf = cut_surface(surf, roi);
	colon_surf_ = scale_surface(surf, colon_w_, height_);

	SDL_Size s = get_size();
	bg_surf_ = create_neutral_surface(s.w, s.h);
}

SDL_Size timage_digits::get_size()
{
	// digit_size: 48x72, colon_width = 16, gap = 4
	// 0:00:00
	int w = 3 * (width_ + gap_w_) + width_ + 1 * (colon_w_ + gap_w_);
	if (has_hour_) {
		w += (width_ + gap_w_) + colon_w_ + gap_w_;
	}
	return SDL_Size{w, height_};
}

const surface& timage_digits::get_surf(int elapse)
{
	fill_surface(bg_surf_, 0x0);

	bool is_nagtive = false;
	if (elapse < 0) {
		is_nagtive = true;
		elapse *= -1;
	}

	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	// 0:12:34

	// 0
	SDL_Rect dst_rect{0, 0, width_, height_};
	if (has_hour_) {
		sdl_blit(digit_surfs_[hour % 10], nullptr, bg_surf_, &dst_rect);
		dst_rect.x += width_ + gap_w_;

		// :
		sdl_blit(colon_surf_, nullptr, bg_surf_, &dst_rect);
		dst_rect.x += colon_w_ + gap_w_;
	}

	// 12
	sdl_blit(digit_surfs_[min / 10], nullptr, bg_surf_, &dst_rect);
	dst_rect.x += width_ + gap_w_;
	sdl_blit(digit_surfs_[min % 10], nullptr, bg_surf_, &dst_rect);
	dst_rect.x += width_ + gap_w_;

	// :
	sdl_blit(colon_surf_, nullptr, bg_surf_, &dst_rect);
	dst_rect.x += colon_w_ + gap_w_;

	// 34
	sdl_blit(digit_surfs_[sec / 10], nullptr, bg_surf_, &dst_rect);
	dst_rect.x += width_ + gap_w_;
	sdl_blit(digit_surfs_[sec % 10], nullptr, bg_surf_, &dst_rect);
	dst_rect.x += width_;

	VALIDATE(dst_rect.x == bg_surf_->w, null_str);

	return bg_surf_;
}

bool FaceOverlayTracker::is_landmark_valid(const SDL_FPoint* landmarks) {
    if (landmarks == nullptr) return false;

    int check_indices[4] = {0, 7, 8, 10}; 
    
    for (int i = 0; i < 4; ++i) {
        int idx = check_indices[i];
        float x = landmarks[idx].x;
        float y = landmarks[idx].y;

        // 使用封装好的函数，清除 NaN 和 Inf
        if (!is_valid_float(x) || !is_valid_float(y)) {
            return false; 
        }

        // 排除极端异常值
        if (x < -1.0f || x > 2.0f || y < -1.0f || y > 2.0f) {
            return false;
        }
    }

    // 排除全 0 假阳性
    if (KDL_Equal(landmarks[0].x, 0.0f) && KDL_Equal(landmarks[0].y, 0.0f) && 
        KDL_Equal(landmarks[7].x, 0.0f) && KDL_Equal(landmarks[7].y, 0.0f)) {
        return false; 
    }

    return true;
}

// Helper function: compute the bounding box of a fixed-length SDL_FPoint array (in normalized coordinates)
// Pass in the keypoints, the index array to be checked, and the number of indices.
inline void landmarks_compute_bbox(const SDL_FPoint* landmarks, const int* indices, int num_indices,
	float& out_min_x, float& out_min_y, float& out_max_x, float& out_max_y) 
{
    // Initialize Min/Max with the 0th point.
    int first_idx = indices[0];
    out_min_x = out_max_x = landmarks[first_idx].x;
    out_min_y = out_max_y = landmarks[first_idx].y;

    // Traverse the remaining points.
    for (int i = 1; i < num_indices; ++i) {
        int idx = indices[i];
        float x = landmarks[idx].x;
        float y = landmarks[idx].y;

        // Compare and update.
        if (x < out_min_x) out_min_x = x;
        if (x > out_max_x) out_max_x = x;
        if (y < out_min_y) out_min_y = y;
        if (y > out_max_y) out_max_y = y;
    }
}

void FaceOverlayTracker::render_avatar(const SDL_FPoint* landmarks, 
                    const cv::Mat& avatar_img, 
                    cv::Mat& background_img, 
                    int img_width, int img_height)
{
    VALIDATE(landmarks != nullptr && !avatar_img.empty() && !background_img.empty(), null_str);

    const int FACE_INDICES[] = {0, 2, 5, 7, 8, 9, 10};    
    float min_x, min_y, max_x, max_y;
    // compute the bounding box.
    landmarks_compute_bbox(landmarks, FACE_INDICES, sizeof(FACE_INDICES) / sizeof(FACE_INDICES[0]), min_x, min_y, max_x, max_y);

    // ---------------------------------------------------------
    // 3. 计算像素目标尺寸与中心
    // ---------------------------------------------------------
    float bbox_w = max_x - min_x;
    float bbox_h = max_y - min_y;
    
    const float MIN_NORM_SIZE = 0.05f; // 兜底保护，防止头像消失
    if (bbox_w < MIN_NORM_SIZE) bbox_w = MIN_NORM_SIZE;
    if (bbox_h < MIN_NORM_SIZE) bbox_h = MIN_NORM_SIZE;

    int center_x = static_cast<int>((min_x + max_x) / 2.0f * img_width);
    int center_y = static_cast<int>((min_y + max_y) / 2.0f * img_height);

    // 目标遮盖尺寸 (扩大 1.5 倍确保盖住脸颊边缘)
    const float SCALE_FACTOR = 2.25f; 
    int target_w = static_cast<int>(bbox_w * img_width * SCALE_FACTOR);
    int target_h = static_cast<int>(bbox_h * img_height * SCALE_FACTOR);
    if (target_w <= 0) target_w = 1;
    if (target_h <= 0) target_h = 1;

    // ---------------------------------------------------------
    // 4. Cover 防拉伸缩放算法
    // ---------------------------------------------------------
    float ratio_w = static_cast<float>(target_w) / avatar_img.cols;
    float ratio_h = static_cast<float>(target_h) / avatar_img.rows;
    float max_ratio = (ratio_w > ratio_h) ? ratio_w : ratio_h;

    int final_w = static_cast<int>(avatar_img.cols * max_ratio);
    int final_h = static_cast<int>(avatar_img.rows * max_ratio);

    cv::Mat avatar_resized;
    cv::resize(avatar_img, avatar_resized, cv::Size(final_w, final_h), 0, 0, cv::INTER_LINEAR);

    // ---------------------------------------------------------
    // 5. 计算对齐坐标 (头像中心对齐脸部中心)
    // ---------------------------------------------------------
    int final_x = center_x - final_w / 2;

	// 使头像的垂直中心比脸部中心向上偏移高度的 10%
	int final_y = center_y - final_h / 2 - static_cast<int>(final_h * 0.25f);

	surface avatar_surf(avatar_resized);
	surface bg_surf(background_img);

	SDL_Rect dstrect{final_x, final_y, avatar_resized.cols, avatar_resized.rows};
	sdl_blit(avatar_surf, nullptr, bg_surf, &dstrect);
	return;

    // ---------------------------------------------------------
    // 6. 安全边界裁剪与重叠区提取
    // ---------------------------------------------------------
    cv::Rect roi_rect(final_x, final_y, avatar_resized.cols, avatar_resized.rows);
    cv::Rect bg_rect(0, 0, background_img.cols, background_img.rows);
    roi_rect = roi_rect & bg_rect; // 取交集，防止数组越界崩溃
    
    if (roi_rect.width <= 0 || roi_rect.height <= 0) {
        return;
    }

    // 直接提取重叠区的图像矩阵 (浅拷贝，零开销)
    cv::Mat roi = background_img(roi_rect);

    // 计算头像图上的对应偏移偏移
    int crop_x = roi_rect.x - final_x;
    int crop_y = roi_rect.y - final_y;
    cv::Rect overlay_crop_rect(crop_x, crop_y, roi_rect.width, roi_rect.height);

    // 提取头像重叠区的图像矩阵
    cv::Mat overlay = avatar_resized(overlay_crop_rect);

    // =========================================================
    // 7. 极速 Alpha 绝对覆盖 (由于全是 4 通道)
    // =========================================================
    // 提取 Alpha 通道并生成二值掩膜
    cv::Mat mask;
    cv::extractChannel(overlay, mask, 3);
    cv::threshold(mask, mask, 10, 255, cv::THRESH_BINARY);

    // 将 overlay 的颜色覆盖到 roi 上 (mask 为 0 的区域自动保留背景)
    overlay.copyTo(roi, mask);

    // 3. 【修正】将 ROI 的 Alpha 通道全部强制设为 255
	// roi.col(3).setTo(cv::Scalar(255));
}

/*
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
*/
namespace gui2 {

REGISTER_DIALOG(kdesktop, rdcamera)

trdcamera::trdcamera(tslot& slot, aplt::thealth& health, std::map<aplt::taplt_key, aplt::tapplet>& applets, /*net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tdcamera_driver& dcamera_driver, tdrivers_core& drivers, */ tbase_driver_core& base_driver,
	/*aplt::tbg_task2& bg_task2, tros_instance& ros_instance,*/ tcamera& camera/*, std::unique_ptr<tmoveit_aplt_task>& moveit_aplt_task*/, tvlog_cfg& vlog_cfg, int sdl_field_small_font_size)
	: slot_(slot)
	, b_api_(aplt::get_b_api())
	, health_(health)
	/*tstatusbar(rdpd_mgr, pble, privacy)
	, tdcamera_slot_impl(applets, dcamera_driver, ros_instance, camera)
	, dcamera_driver_(dcamera_driver)
	, base_driver_(ros_instance.base_driver())
	, moveit_driver_(ros_instance.moveit_driver())
	, drivers_(drivers)
	, bg_task2_(bg_task2)
	, ros_instance_(ros_instance)
*/
	, base_driver_(base_driver)
	, camera_(camera)
	, vlog_cfg_(vlog_cfg)
	, sdl_field_small_font_size_(sdl_field_small_font_size)
	, rpy_sensor_(aplt::get_rpy_sensor())
	, pinyin_(aplt::get_curr_pinyin())
/*
	, moveit_aplt_task_(moveit_aplt_task)
*/
	, case_(slot.case_)
	, original_camera_flags_(camera.get_flags())
/*
	, use_mediapipe_(fake_libkosapi_so) // fake_libkosapi_so
*/
	, use_mediapipe_(false)
	, spent_ms_(nposm)
	, total_valid_frames_(0)
	, total_spent_ms_(0)
	, mediapipe_flip_h_(false)
	, new_frame_state_(nposm)
	, save_work_frame_(false)
	// , start_widget_(nullptr)
	, moveit_aplt_widget_(nullptr)
	, paper_(nullptr)
	, reach_dcpitch_widget_(nullptr)
	, pitch_widget_(nullptr)
	, status_widget_(nullptr)
	, rpy_widget_(nullptr)
	, highlight_ticks_(0)
	, offset_percent_threshold_(5) // width * 5%
	, base_scene_result_cancel_(false)
	// , use_fake_pitch_val_(game_config::os == os_windows)
	, use_fake_pitch_val_(false)
	, fake_pitch_vals_({-84.1f, -85.1f, -86.1f, -87.1f, 84.1f, 85.1f, 86.1f, 87.1f})
	, fake_pitch_val_at_(0)
	, vlog_mode_(false)
	, change_vlog_mode_pending_(false)
	, image_digits_(48 * twidget::hdpi_scale)
{
	slot_.rdcamera_ = this;

	set_timer_interval(200);

	surface avatar_surf = image::get_image("misc/avatar.png");
	VALIDATE(avatar_surf.get() != nullptr, null_str);
	{
		tsurface_2_mat_lock lock(avatar_surf);
		avatar_mat_ = lock.mat.clone();
		VALIDATE(avatar_mat_.channels() == 4, null_str);
	}

	start_caption_surf_ = image::get_image("misc/start_caption.png");
	VALIDATE(start_caption_surf_.get() != nullptr, null_str);

	finish_title_surf_ = image::get_image("misc/finish_title.png");
	VALIDATE(finish_title_surf_.get() != nullptr, null_str);
	
	/* if (moveit_aplt_task.get() != nullptr || instance->bg_task().in_task_cpp2()) {
		case_ = case_moveit;
		// if before navigation is fail, ros_instance_.has_task() is false. but ros_instance_.started_ is true.
		// VALIDATE(ros_instance_.has_task(), null_str);
		VALIDATE(ros_instance_.started(), null_str);

	} else */ if (!base_driver_.scene_id().empty()) {
		// sts_ing or sts_idle
		case_ = case_base_subtask;
	}

	if (case_ != case_base_subtask) {
		disable_new_aplt_lock_.reset(new aplt::tdisable_new_klink_task_lock(aplt::tdisable_new_klink_task_lock::reason_camera));
	}
/*
	// enum {moveit_op_grasp, moveit_op_press_top, moveit_op_press_middle, moveit_op_count};
	moveit_ops_.insert(std::make_pair(moveit_op_grasp, "grasp"));
	moveit_ops_.insert(std::make_pair(moveit_op_press_top, "press_top"));
	moveit_ops_.insert(std::make_pair(moveit_op_press_middle, "press_middle"));
	VALIDATE(moveit_ops_.size() == moveit_op_count, null_str);
*/
	if (case_ == nposm) {
		if (use_mediapipe_) {
			mediapipe_api_ptr_.reset(mediapipe::rose_create_pose_tracking_api());
			if (!mediapipe_api_ptr_->graph_initialized()) {
				SDL_Log("%s", _("Initialize mediapipe graph fail"));
			}
		}

		camera_.set_slot(this);
/*
		if (dcamera_driver_.installed() && moveit_driver_.installed()) {
			ros_instance_.register_slot(*this);
			ros_instance_.start_moveit_node(false);

			moveit_calculator_.deps_0lock().reset(new tdeps_0lock(ros_instance_.drivers()));
		}
*/
	} else if (case_ == case_base_subtask) {
		// tcamera::tslot& slot = get_camera_slot();
		// VALIDATE(&slot == &base_driver_, null_str);

		if (camera_.tasking()) {
			VALIDATE(camera_.curr_taskid() == tcamera::taskid_base_subtask, null_str);
			camera_.set_flags(camera_.get_flags() & ~tcamera::FLAG_SHOW_CANCEL);
		}

		base_driver_.set_camera_viewer(this);
/*
		bg_task2_.set_camera_viewer(this);
*/
	}
}

trdcamera::~trdcamera()
{
	VALIDATE(vlog_.script == nullptr, null_str);

	if (case_ == nposm) {
/*
		if (dcamera_driver_.installed() && moveit_driver_.installed()) {
			ros_instance_.deregister_slot(*this);
		}
*/
	} else if (case_ == case_base_subtask) {
/*
		bg_task2_.set_camera_viewer(nullptr);
*/
		base_driver_.set_camera_viewer(nullptr);

		if (camera_.tasking() && camera_.curr_taskid() == tcamera::taskid_base_subtask) { 
			camera_.set_flags(camera_.get_flags() | tcamera::FLAG_SHOW_CANCEL);
		}
	}

	VALIDATE(base_driver_.camera_viewer() == nullptr, null_str);
/*
	VALIDATE(bg_task2_.camera_viewer() == nullptr, null_str);
*/
	// VALIDATE(camera_.get_flags() == original_camera_flags_, null_str);
}

void trdcamera::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
/*
	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());
*/
	// find_widget<tlabel>(window_, "title", false, true)->set_label(aplt::all_fake_applets.find(aplt::builtinid_dcamera)->second.name);
	// find_widget<tlabel>(window_, "title", false, true)->set_label(_("kDesktop"));


	if (game_config::app_code == aplt::app_kdesktop) {
		find_widget<tlabel>(window_, "warning", false, true)->set_label(ht::generate_format(_("dcamera^warning"), 0xffff0000));
	}

	if (disable_new_aplt_lock_.get() != nullptr) {
/*
		find_widget<tlabel>(window_, "warning", false, true)->set_label(ht::generate_format(disable_timing_warnning(), 0xffff0000));
*/
	}
	pitch_widget_ = find_widget<tlabel>(window_, "pitch", false, true);
	status_widget_ = find_widget<tlabel>(window_, "camera_k", false, true);
	rpy_widget_ = find_widget<tlabel>(window_, "rpy", false, true);

	paper_ = find_widget<ttrack>(window_, "paper", false, true);
	ttrack& paper = *paper_;
	paper.set_did_draw(std::bind(&trdcamera::did_draw_paper, this, _1, _2, _3));
	paper.set_did_left_button_down(std::bind(&tcamera::did_left_button_down_paper, &camera_, _1, _2));
	paper.set_did_mouse_motion(std::bind(&tcamera::did_mouse_motion_paper, &camera_, _1, _2, _3));
	paper.set_did_mouse_leave(std::bind(&trdcamera::did_mouse_leave_paper, this, _1, _2, _3));
	paper_->connect_signal<event::LONGPRESS>(
			std::bind(
				&trdcamera::signal_handler_longpress_paper
				, this
				, _4, _5)
			, event::tdispatcher::back_child);

	// paper_->set_did_create_background_tex(std::bind(&trdcamera::did_create_background_tex, this, _1, _2));

	tbutton* button = find_widget<tbutton>(window_, "wko_settings", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&trdcamera::click_wko_settings
			, this, std::ref(*button)));
/*
	button = find_widget<tbutton>(window_, "start", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&trdcamera::click_start
			, this, std::ref(*button)));
	start_widget_ = button;
*/
	button = find_widget<tbutton>(window_, "moveit_aplt", false, true);
	button->set_border("textbox");
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			// &trdcamera::click_moveit_aplt
			&trdcamera::click_switch_scene
			, this, std::ref(*button)));
/*
	const std::string curr_task_id2 = preferences_moveit_task_id2(applets_);
	if (curr_task_id2.empty()) {
		start_widget_->set_active(false);
	}
	button->set_label(curr_task_id2);
*/
	std::pair<const aplt::tbase_scene*, int> curr_scene = b_api_.aplt_curr_base_scene();
	if (curr_scene.first != nullptr) {
		button->set_label(curr_scene.first->name());
	}

	moveit_aplt_widget_ = button;
/*
	button = find_widget<tbutton>(window_, "dctask", false, true);
	button->set_border("textbox");
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&trdcamera::click_view
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "snapshot_depth", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&trdcamera::click_snapshot_depth
			, this, std::ref(*button)));
*/
	button = find_widget<tbutton>(window_, "snapshot", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&trdcamera::click_snapshot
			, this, std::ref(*button)));
/*
	button = find_widget<tbutton>(window_, "dbg", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&trdcamera::click_dbg_RP
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "reach_dcpitch", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&trdcamera::click_reach_dcpitch
			, this, std::ref(*button)));
	reach_dcpitch_widget_ = button;
*/
	if (case_ == nposm) {
/*
		if (dcamera_driver_.installed() && !dcamera_driver_.main_tasking()) {
			int def_depth_task = dctask_d2c;
			dcamera_driver_.set_desire_depth_task(def_depth_task);
		}

		if (!moveit_driver_.installed()) {
			start_widget_->set_visible(twidget::HIDDEN);

			reach_dcpitch_widget_->set_visible(twidget::INVISIBLE);
		}
*/
		bool no_swap_wh_for_screen = true;
		camera_.enter_task(tcamera::taskid_dcamera, 0, no_swap_wh_for_screen);

		// start_widget_->set_visible(twidget::INVISIBLE);
		find_widget<tgrid>(window_, "ctrl_grid", false, true)->set_visible(twidget::INVISIBLE);

	} else {
/*
		if (case_ == case_moveit) {
			if (moveit_aplt_task_.get() != nullptr) {
				set_did_draw_slice_bh_in_viewer();

			} else {
				VALIDATE(instance->bg_task().in_task_cpp2(), null_str);
			}

		} else */ {
			VALIDATE(case_ == case_base_subtask, null_str);
			if (base_driver_.is_ing()) {
				// Why does the slot need to be set even (!vlog_mode_)? 
				// --Some values, such as sfx_enabled(), need to be passed to twkoscript regardless of whether is in vlog mode.
				base_driver_.set_wko_task_slot(this);
			}
		}
		// start_widget_->set_visible(twidget::INVISIBLE);

		find_widget<tgrid>(window_, "ctrl_grid", false, true)->set_visible(twidget::INVISIBLE);

		// requrie set timer_interval here. else did_draw_paper will not called.
		paper_->set_timer_interval(30);
	}

	if (case_ != case_moveit) {
		// set_ik_diff_widget_->set_visible(twidget::INVISIBLE);
	}

	moveit_aplt_widget_->set_visible(twidget::VISIBLE);
}

void trdcamera::post_show()
{
	if (vlog_mode_) {
		change_vlog_mode(false);
	}
	if (base_driver_.is_ing()) {
		base_driver_.set_wko_task_slot(nullptr);
	}

	if (case_ == nposm) {
		if (!base_scene_result_cancel_) {
			camera_.exit_task(tcamera::taskid_dcamera);
			camera_.set_slot(nullptr);
		}

		if (mediapipe_api_ptr_.get() != nullptr) {
			mediapipe_api_ptr_.reset();
		}

	} /* else if (moveit_aplt_task_.get() != nullptr) {
		paper_->set_timer_interval(0);

		moveit_aplt_task_->set_did_draw_slice_bh(NULL);
	} */
}

void trdcamera::click_wko_settings(tbutton& widget)
{
	{
		gui2::twko_settings dlg(vlog_cfg_);
		dlg.show();
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
		const tvlog_cfg& new_vlog_cfg = dlg.get_new_vlog_cfg();
		if (vlog_cfg_ != new_vlog_cfg) {
			vlog_cfg_ = new_vlog_cfg;
			vlog_cfg_.write_pref();
		}
	}
}

void trdcamera::snapshot_last_frame()
{
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

void trdcamera::click_switch_scene(tbutton& widget)
{
	VALIDATE(!vlog_mode_, null_str);
/*
	{
		if (!SDL_IsScreenRecording()) {
			SDL_StartScreenRecording("kdesktop");

		} else {
			char full_filename[256];
			SDL_bool ret = SDL_StopScreenRecording(full_filename, sizeof(full_filename) / sizeof(full_filename[0]));
			SDL_Log("SDL_StopScreenRecording, ret: %s, full_filename: %s", ret? "true": "false", full_filename);
		}

		return;
	}
*/

	if (use_fake_pitch_val_) {
		fake_pitch_val_at_ ++;
		fake_pitch_val_at_ = fake_pitch_val_at_ % fake_pitch_vals_.size();
		return;
	}

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	std::pair<const aplt::tbase_scene*, int> curr_scene = b_api_.aplt_curr_base_scene();
	const std::string current_scene_id = curr_scene.first != nullptr? curr_scene.first->id: null_str;
	const std::vector<aplt::tbase_scene>& scenes = b_api_.aplt_base_scenes();
	const int scene_count = scenes.size();

	if (curr_scene.first != nullptr) {
		std::string name("[");
		name.append(_("Stop")).append("]");
		items.push_back(gui2::tmenu::titem(name, scene_count));
	}

	for (int at = 0; at < scene_count; at ++) {
		const aplt::tbase_scene& scene = scenes[at];

		if (scene.id == current_scene_id) {
			initial_sel = at;
			continue;
		}
		items.push_back(gui2::tmenu::titem(scene.name(), at));
	}

	{
		std::string name("{");
		name.append(_("Snapshot")).append("}");
		items.push_back(gui2::tmenu::titem(name, scene_count + 1));
	}

	if (items.empty()) {
		return;
	}

	int sel_at = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		sel_at = dlg.selected_val();
	}

	if (base_driver_.is_ing()) {
		base_driver_.set_wko_task_slot(nullptr);
	}

	std::string new_label;
	if (sel_at < scene_count) {
		const aplt::tbase_scene& scene = scenes[sel_at];

		std::string err_msg;
		const aplt::tbase_scene* new_scene = aplt::handle_base_scene(b_api_, sel_at, scene.name(), nposm, err_msg);

		if (new_scene != nullptr) {
			new_label = new_scene->name();

			VALIDATE(base_driver_.is_ing(), null_str);
			base_driver_.set_wko_task_slot(this);

		} else {
			new_label = err_msg;
		}
		// init_wko_tlv_history(last_tlv_history_);
		// init_wko_tlv_history(finished_tlv_history_);

	} else if (sel_at == scene_count) {
		base_driver_.stop_subtask_and_empty_pref();

	} else {
		VALIDATE(sel_at == scene_count + 1, null_str);
		snapshot_last_frame();
		return;
	}

	widget.set_label(utils::truncate_to_max_chars2(new_label, 11, true));
}


const aplt::tapplet* scene_aplt_obj(const aplt::tbase_scene& scene, const std::map<aplt::taplt_key, aplt::tapplet>& applets)
{
	bool aplt_id_is_bundleid = true;
	const aplt::tapplet* aplt_obj = aplt_from_bundleid_ex(applets, scene.aplt);
	return aplt_obj;
}

void trdcamera::change_vlog_mode(bool enter)
{
	VALIDATE_IN_MAIN_THREAD();

	tgrid* title_grid = find_widget<tgrid>(window_, "title_grid", false, true);
	tlabel* warning_label = find_widget<tlabel>(window_, "warning", false, true);
	tgrid* status_grid = find_widget<tgrid>(window_, "status_grid", false, true);

	if (enter) {
		VALIDATE(!vlog_mode_, null_str);
		// First set the previous 'wko_task_slot' to nullptr, 
		// so that information such as script, state_at, and phase_at can be received later.
		base_driver_.set_wko_task_slot(nullptr);

		title_grid->set_visible(twidget::INVISIBLE);
		warning_label->set_visible(twidget::INVISIBLE);
		status_grid->set_visible(twidget::INVISIBLE);

		VALIDATE(vlog_.script == nullptr, null_str);
		VALIDATE(vlog_.chart_mat.empty(), null_str);
		vlog_mode_ = true;
		base_driver_.set_wko_task_slot(this);
		VALIDATE(vlog_.script != nullptr && vlog_.script->valid(), null_str);
		vlog_.pose_state_count = vlog_.script->pose_state_count(nullptr);

		vlog_.original_camera_flags = camera_.get_flags();
		camera_.set_flags(vlog_.original_camera_flags | tcamera::FLAG_HIDE_SWITCH);

		vlog_.show_rec_toast_ticks_ = SDL_GetTicks() + vlog_.rec_toast_threshold_s * 1000;

		// vlog_mode_ = true;

	} else {
		VALIDATE(vlog_mode_, null_str);
		title_grid->set_visible(twidget::VISIBLE);
		warning_label->set_visible(twidget::VISIBLE);
		status_grid->set_visible(twidget::VISIBLE);

		camera_.set_flags(vlog_.original_camera_flags);

		VALIDATE(vlog_.script != nullptr && vlog_.script->valid(), null_str);
		vlog_.script = nullptr;
		vlog_.clear();

		// Do not set the slot to nullptr; when (!vlog_mode_), continue using this slot.
		// base_driver_.set_wko_task_slot(nullptr);

		vlog_mode_ = false;
	}
}

void trdcamera::signal_handler_longpress_paper(bool& halt, const tpoint& coordinate)
{
	halt = true;

	std::pair<const aplt::tbase_scene*, int> curr_scene = b_api_.aplt_curr_base_scene();
	bool scene_tasking = curr_scene.first != nullptr;
	if (!vlog_mode_) {
		if (scene_tasking) {
			change_vlog_mode(true);

		} else {
			SDL_Log("receive longpress event, but not in scene tasking, do nothing.");
		}

	} else {
		change_vlog_mode(false);
	}

/*
	if (change_vlog_mode_pending_) {
		return;
	}

	// long-press requires complex processes, current maybe pending other app_message task.
	// use desire_enter_settingsmode_, dont' waste this long-press.

	change_vlog_mode_pending_ = true;

	bool enter = !vlog_mode_;
	tmsg_data_change_vlog_mode* pdata = new tmsg_data_change_vlog_mode(enter);
	rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_CHANGE_VLOG_MODE, nullptr);
*/
}

bool trdcamera::is_overlay_landmarks() const 
{ 
	if (!vlog_mode_) {
		return true;
	}
	return !vlog_cfg_.hides[vlog_cfg_.hid_landmarks];
}

bool trdcamera::is_overlay_analyze_msg() const 
{ 
	if (!vlog_mode_) {
		return true;
	}
	return false;
}

void trdcamera::set_curr_script(const aplt::twkoscript& script, int state_at, int phase_at, uint32_t first_satisfied_ticks, int64_t first_satisfied_ts)
{
	if (!vlog_mode_) {
		return;
	}
	VALIDATE(script.valid(), null_str);
	VALIDATE(vlog_.script == nullptr, null_str);

	vlog_.script = &script;

	vlog_.state_at = state_at;
	vlog_.phase_at = phase_at;
	vlog_.first_satisfied_ticks = first_satisfied_ticks;
	vlog_.first_satisfied_ts = first_satisfied_ts;

	VALIDATE(!wko_tlv_history_is_valid(vlog_.last_tlv_history), null_str);
	if (state_at >= 2) {
		// 0: speak, 1:setup
		vlog_.last_tlv_history = health_.find_wko_history(nposm);
		VALIDATE(wko_tlv_history_is_valid(vlog_.last_tlv_history), null_str);
	}
}

void trdcamera::did_first_satisfied_frame(uint32_t ticks, int64_t ts)
{
	if (!vlog_mode_) {
		return;
	}
	VALIDATE(vlog_.script != nullptr, null_str);
	VALIDATE(vlog_.first_satisfied_ticks == 0, null_str);
	VALIDATE(vlog_.first_satisfied_ts == 0, null_str);

	vlog_.first_satisfied_ticks = ticks;
	vlog_.first_satisfied_ts = ts;
}

void trdcamera::did_enter_state_and_phase(int state_at, int phase_at)
{
	if (!vlog_mode_) {
		return;
	}
	VALIDATE(vlog_.script != nullptr, null_str);
	vlog_.state_at = state_at;
	vlog_.phase_at = phase_at;

	memset(&vlog_.analyze_result, 0, sizeof(vlog_.analyze_result));
}

void trdcamera::did_analyze_result(const aplt::twko_analyze_result_C& result)
{
	if (!vlog_mode_) {
		return;
	}
	VALIDATE(vlog_.script != nullptr, null_str);

	memcpy(&vlog_.analyze_result, &result, sizeof(result));
}

void trdcamera::render_face_overlay(const SDL_FPoint* landmarks, int count, bool flip_h, cv::Mat& cv_argb)
{
	if (!vlog_mode_) {
		return;
	}
	if (!vlog_cfg_.hides[vlog_cfg_.hid_face_cover]) {
		face_overlay_tracker_.render(landmarks, avatar_mat_, cv_argb);
	}
}

bool trdcamera::sfx_enabled() const
{
	return !vlog_cfg_.misc_sfx_disabled;
}

/*
tcamera::tslot& trdcamera::get_camera_slot()
{
	VALIDATE(case_ == case_base_subtask, null_str);

	tcamera::tslot* slot = &bg_task2_;
	if (base_driver_.subtask_state() == aplt::sts_ing || base_driver_.subtask_state() == aplt::sts_idle) {
		slot = &base_driver_;
	}
	return *slot;
}

void trdcamera::set_did_draw_slice_bh_in_viewer()
{
	VALIDATE(case_ == case_moveit, null_str);
	VALIDATE(moveit_aplt_task_.get() != nullptr, null_str);

	moveit_aplt_task_->set_did_draw_slice_bh(std::bind(&trdcamera::did_draw_slice_bh, this, _1, _2, _3, _4, _5, _6));
}

void trdcamera::click_start(tbutton& widget)
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

void trdcamera::did_operate_stopped()
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

void trdcamera::click_moveit_aplt(tbutton& widget)
{
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

void trdcamera::click_view(tbutton& widget)
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
	camera_.enter_task(tcamera::taskid_dcamera, 0);
}

bool gbackground_tex_dirty = false;

void trdcamera::click_snapshot_depth(tbutton& widget)
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
*/

void trdcamera::click_snapshot(tbutton& widget)
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
		snapshot_last_frame();
/*
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
*/
	}
}
/*
void trdcamera::click_dbg_RP(tbutton& widget)
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

void trdcamera::click_reach_dcpitch(tbutton& widget)
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
*/

void trdcamera::set_highlight(const std::string& msg, int threshold)
{
	VALIDATE(!msg.empty(), null_str);
	VALIDATE(threshold >= 0, null_str);

	highlight_msg_ = msg;
	highlight_ticks_ = SDL_GetTicks() + threshold;
}
/*
void trdcamera::set_set_ik_diff_label(double x_diff, double z_diff)
{
	char buf[64] = "---";
	if (!is_float_nposm(x_diff)) {
		SDL_snprintf(buf, sizeof(buf), "%.6f, %.6f", x_diff, z_diff);
	}
	set_ik_diff_widget_->set_label(buf);
}
*/

void trdcamera::set_pitch_label(const std::string& msg)
{
	pitch_widget_->set_label(msg);
}

void trdcamera::set_status_label(const std::string& msg)
{
	status_widget_->set_label(msg);
}
/*
void trdcamera::set_rpy_label()
{
	rpy_widget_->set_label(ros_instance_.get_imu_desc());
}
*/
texture trdcamera::did_create_background_tex(ttrack& widget, const SDL_Rect& draw_rect)
{
	surface bg_surf = create_neutral_surface(draw_rect.w, draw_rect.h);
	// uint32_t bg_color = 0xfffdfdfd;
    uint32_t bg_color = 0xffff0000;
/*
	if (gbackground_tex_dirty) {
		bg_color = 0xff00ff00;
		gbackground_tex_dirty = false;
	}
*/
	fill_surface(bg_surf, bg_color);

    SDL_Renderer* renderer = get_renderer();

	return SDL_CreateTextureFromSurface2(renderer, bg_surf.get());
}

void trdcamera::did_draw_paper(gui2::ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
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

	} /* else if (case_ == case_moveit) {
		if (moveit_aplt_task_.get() != nullptr) {
			if (!moveit_aplt_task_->is_set_did_draw_slice_bh()) {
				set_did_draw_slice_bh_in_viewer();
			}
			camera_.slice(widget_rect, true);
		}

	} */ else if (case_ == case_base_subtask) {
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

void trdcamera::did_mouse_leave_paper(gui2::ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(last)) {
		return;
	}

	camera_.did_mouse_leave_paper(widget, first, last);
}
/*
void trdcamera::did_draw_slice_bh(tmoveit_calculator& calculator, trtc_client::VideoRenderer& vsink, const SDL_Rect& draw_rect, const std::string& msg, const std::vector<SDL_2Point>& new_qrcode_corners, const std::vector<SDL_Rect>&reference_rects)
*/
void trdcamera::did_draw_slice_bh(trtc_client::VideoRenderer& vsink, const SDL_Rect& draw_rect, const std::string& msg, const std::vector<SDL_2Point>& new_qrcode_corners, const std::vector<SDL_Rect>&reference_rects)
{
	SDL_Renderer* renderer = get_renderer();

	ttexture_2_mat_lock mat_lock(vsink.tex_);

	const cv::Mat& frame = mat_lock.mat;

	if (!msg.empty()) {
		set_highlight(msg, 5000);
	}

	tpoint ratio_size(0, 0);
	SDL_Rect video_dst;
	if (!vsink.use_no_swap_wh_tex()) {
		ratio_size = calculate_adaption_ratio_size(draw_rect.w, draw_rect.h, frame.cols, frame.rows);
		video_dst = SDL_Rect{draw_rect.x + (draw_rect.w - ratio_size.x) / 2, draw_rect.y + (draw_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};

		SDL_RenderCopy(renderer, vsink.tex_.get(), nullptr, &video_dst);

	} else {
		cv::Mat frame2;
		vsink.fill_no_swap_wh_tex(frame, frame2);
		// imwrite(frame2, "1.png");
		ratio_size = calculate_adaption_ratio_size(draw_rect.w, draw_rect.h, frame2.cols, frame2.rows);
		video_dst = SDL_Rect{draw_rect.x + (draw_rect.w - ratio_size.x) / 2, draw_rect.y + (draw_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};

		SDL_RenderCopy(renderer, vsink.no_swap_wh_tex_.get(), nullptr, &video_dst);
	}

	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "%i x %i, rotaion: %i", frame.cols, frame.rows, vsink.rotation_);
	status_msg_ = buf;

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
/*
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
*/

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

void trdcamera::camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
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
/*
	std::string msg = impl_did_draw_slice(vsink, new_qrcode_corners, reference_rects);
*/
	std::string msg;
/*
	did_draw_slice_bh(moveit_calculator_, vsink, draw_rect, msg, new_qrcode_corners, reference_rects);
*/
	did_draw_slice_bh(vsink, draw_rect, msg, new_qrcode_corners, reference_rects);
}

bool trdcamera::camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb)
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

void trdcamera::camera_work_frame(const surface& surf)
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
/*
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
*/
}
/*
void trdcamera::dcamera_did_OnFrame(int task, const tdcframe_C* frames, int count)
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
*/
void trdcamera::camera_post_enter_task()
{
/*
	VALIDATE(!moveit_calculator_.is_started(), null_str);
*/
	start_avcapture_message_ = _("Starting Camera");

	VALIDATE(window_ != nullptr, null_str);
	if (window_->drawn()) {
		gui2::absolute_draw();
	}

	paper_->set_timer_interval(30);
}

void trdcamera::camera_pre_exit_task()
{
	paper_->set_timer_interval(0);
/*
	if (use_calculator()) {
		moveit_calculator_.did_stopped();
	}
*/
}

cv::Mat draw_multcol_mat(int mat_type, int max_width, double radius, const SDL_DColor& fill_color, const SDL_Point& margin, 
	const std::vector<std::pair<std::string, std::string> >& cols_data, const aplt::twko_analyze_result_C& analyze_result)
{
	cairo::tmultcol_fields fields(mat_type, max_width);

	for (int at = 0; at < (int)cols_data.size(); at ++) {
		const std::pair<std::string, std::string>& col_data = cols_data[at];

		fields.cols.push_back(cairo::tmultcol_fields::tcol());
		cairo::tmultcol_fields::tcol& to_col = fields.cols.back();

		to_col.line1.set(null_str, col_data.first, font::SIZE_DEFAULT, null_str, 0, SDL_Point{0, 0}, SDL_Point{0, 0});
		to_col.line2.set(null_str, col_data.second, font::SIZE_SMALLER, null_str, 0, SDL_Point{0, 0}, SDL_Point{0, 0});
	}
	VALIDATE(fields.cols.size() == cols_data.size(), null_str);

	cv::Mat result_mat = cairo::draw_multcol_mat(radius, fill_color, margin, fields);

	surface text_surf;
	SDL_Rect dst_rect;
	{
		surface surf(result_mat);
		for (int col_at = 0; col_at < fields.cols.size(); col_at ++) {
			const cairo::tmultcol_fields::tcol& to_col = fields.cols[col_at];

			const cairo::tsdl_field* field = nullptr;
			for (int line_at = 0; line_at < 2; line_at ++) {
				SDL_Color font_color = font::GOOD_COLOR;
				if (line_at == 0) {
					if (analyze_result.fields != 0) {
						VALIDATE(analyze_result.fields > col_at, null_str);
						if (!analyze_result.satisfied[col_at]) {
							font_color = font::BAD_COLOR;
						}
					}
					field = &to_col.line1;
				} else {
					field = &to_col.line2;
					font_color = font::BIGMAP_COLOR;
				}

				text_surf = font::get_rendered_text(field->name, INT_MAX, field->name_font_size, font_color);
				dst_rect = ::create_rect(field->offset.x, field->offset.y, text_surf->w, text_surf->h);
				sdl_blit(text_surf, nullptr, surf, &dst_rect);
			}
		}
	}

	return result_mat;
}

enum {drawitem_rectangle_text_min, drawitem_rec_toast = drawitem_rectangle_text_min, drawitem_start_today, drawitem_start_caption,
	drawitem_active_statename, drawitem_active_phase, drawitem_finish_today, drawitem_finish_title,
	drawitem_rectangle_text_max = drawitem_finish_title,

	drawitem_surf_min, drawitem_start_title = drawitem_surf_min, drawitem_start_history, drawitem_active_time, drawitem_active_poses,
	drawitem_finish_caption, drawitem_finish_history, drawitem_finish_chart,
	drawitem_surf_max = drawitem_finish_chart,
};

SDL_Rect use_no_swap_wh_tex_render_rectangle_text(SDL_Renderer* renderer, int drawitem, const surface& text_surf, const SDL_Rect& video_dst, 
	const SDL_Size& margin, cairo::ticon* icon, const SDL_DColor& fill_color, const SDL_DColor& line_color, double line_width, double radius,
    bool ltop, bool rtop, bool lbottom, bool rbottom, const SDL_Point* extra_offset = nullptr)
{
	VALIDATE(drawitem >= drawitem_rectangle_text_min && drawitem <= drawitem_rectangle_text_max, null_str);
	surface rounded_text_surf = cairo::draw_rounded_rectangle_for_text_surf(text_surf, margin, 
		icon, fill_color, line_color, line_width, radius,
		ltop, rtop, lbottom, rbottom);

	SDL_Rect dstrect = video_dst;
	{
		rounded_text_surf = rotate_surface(rounded_text_surf, -90, nullptr, 0);
		if (drawitem == drawitem_rec_toast) {
			dstrect.x += video_dst.w - extra_offset->x - rounded_text_surf->w;
			dstrect.y += (video_dst.h - rounded_text_surf->h) / 2;

		} else if (drawitem == drawitem_start_today) {
			dstrect.x += video_dst.w - extra_offset->x - rounded_text_surf->w;
			dstrect.y += video_dst.h - rounded_text_surf->h - extra_offset->y;

		} else if (drawitem == drawitem_start_caption) {
			dstrect.x += (video_dst.w - rounded_text_surf->w) / 2 + extra_offset->x;
			dstrect.y += (video_dst.h - rounded_text_surf->h) / 2;

		} else if (drawitem == drawitem_active_statename) {
			dstrect.x += video_dst.w - extra_offset->x - rounded_text_surf->w;
			dstrect.y += extra_offset->y;

		} else if (drawitem == drawitem_active_phase) {
			dstrect.x += video_dst.w - extra_offset->x - rounded_text_surf->w;
			dstrect.y += extra_offset->y;

		} else if (drawitem == drawitem_finish_today) {
			dstrect.x += video_dst.w - extra_offset->x - rounded_text_surf->w;
			dstrect.y += video_dst.h - rounded_text_surf->h - extra_offset->y;

		} else if (drawitem == drawitem_finish_title) {
			dstrect.x += (video_dst.w - rounded_text_surf->w) / 2;
			dstrect.y += (video_dst.h - rounded_text_surf->h) / 2;
		} 
	}
	dstrect.w = rounded_text_surf->w;
	dstrect.h = rounded_text_surf->h;
	render_surface(renderer, rounded_text_surf, NULL, &dstrect);

	return dstrect;
}

SDL_Rect use_no_swap_wh_tex_render_surf(SDL_Renderer* renderer, int drawitem, const surface& _surf, const SDL_Rect& video_dst, 
	const SDL_Point* extra_offset = nullptr)
{
	VALIDATE(drawitem >= drawitem_surf_min && drawitem <= drawitem_surf_max, null_str);
	SDL_Rect dstrect = video_dst;
	surface surf;
	{
		surf = rotate_surface(_surf, -90, nullptr, 0);
		if (drawitem == drawitem_start_title) {
			dstrect.x += (video_dst.w - surf->w) / 2;
			dstrect.y += (video_dst.h - surf->h) / 2;

		} else if (drawitem == drawitem_start_history) {
			dstrect.x += video_dst.w / 2 - surf->w - extra_offset->x;
			dstrect.y += (video_dst.h - surf->h) / 2;

		} else if (drawitem == drawitem_active_time) {
			dstrect.x += video_dst.w - extra_offset->x - surf->w;
			dstrect.y += video_dst.h - surf->h - extra_offset->y;

		} else if (drawitem == drawitem_active_poses) {
			dstrect.x += video_dst.w - extra_offset->x - surf->w;
			dstrect.y += extra_offset->y;

		} else if (drawitem == drawitem_finish_caption) {
			dstrect.x += (video_dst.w - surf->w) / 2 + extra_offset->x;
			dstrect.y += (video_dst.h - surf->h) / 2;

		} else if (drawitem == drawitem_finish_history) {
			dstrect.x += video_dst.w / 2 - surf->w - extra_offset->x;
			dstrect.y += (video_dst.h - surf->h) / 2;

		} else if (drawitem == drawitem_finish_chart) {
			// dstrect.x += (video_dst.w - surf->w) / 2 + extra_offset->x;
			dstrect.x += (video_dst.w - surf->w) / 2;
			dstrect.y += (video_dst.h - surf->h) / 2;

		}
	}
	
	dstrect.w = surf->w;
	dstrect.h = surf->h;
	render_surface(renderer, surf, NULL, &dstrect);

	return dstrect;
}

void trdcamera::use_no_swap_wh_tex_render_start(SDL_Renderer* renderer, const aplt::twkoscript& script,
	const SDL_Rect& video_dst, const SDL_Size& margin, double radius)
{
	SDL_Point extra_offset = {0};

	// 0:(speak), 1:(setup), 2(speak this)
	if (!wko_tlv_history_is_valid(vlog_.last_tlv_history)) {
		vlog_.last_tlv_history = health_.find_wko_history(nposm);
		VALIDATE(wko_tlv_history_is_valid(vlog_.last_tlv_history), null_str);
	}
	int font_size = font::SIZE_DEFAULT;
	std::string text;
	surface text_surf;
	SDL_DColor fill_color;
	SDL_DColor line_color;
	double line_width = 1.5;

	// today
	if (!vlog_cfg_.hides[vlog_cfg_.hid_start_date]) {
		// text = utils::format_time_ymd3(time(nullptr));
		text = utils::format_time_ymdhms3(time(nullptr));
		text_surf = font::get_rendered_text(text, 0, font_size, font::NORMAL_COLOR);

		fill_color = SDL_DColor{0.0, 0.0, 0.0, 0.5};
		line_color = SDL_DColor{0.0, 0.0, 0.0, 0.0};
		line_width = 1.5;

		extra_offset = SDL_Point{vlog_.start_finish_margin.t, vlog_.start_finish_margin.r};
		use_no_swap_wh_tex_render_rectangle_text(renderer, drawitem_start_today, text_surf,
			video_dst, margin, nullptr, fill_color,
			line_color, line_width, radius, true, true, true, true, &extra_offset);
	}

	font_size = font::SIZE_LARGEST + font::SIZE_SMALLEST;
	surface title_surf = font::get_rendered_text(script.name, 0, font_size, font::BIGMAP_COLOR);
	const SDL_Size title_size{title_surf->w, title_surf->h};

	if (!vlog_cfg_.hides[vlog_cfg_.hid_start_caption] && !vlog_cfg_.start_caption_msg.empty()) {
		// start_caption
		font_size = font::SIZE_DEFAULT;
		text_surf = font::get_rendered_text(vlog_cfg_.start_caption_msg, 0, font_size, font::GOOD_COLOR);

		int icon_s = text_surf->h * 4 / 5;
		surface icon_surf = scale_surface(start_caption_surf_, icon_s, icon_s);
		cairo::ticon icon;
		icon.set_surf(icon_surf, vlog_.icon_surf_gap_x);

		fill_color = SDL_DColor{0.0, 1.0, 0.0, 0.5};
		line_color = SDL_DColor{0.0, 1.0, 0.0, 1.0};
		line_width = 1.5;

		extra_offset.x = title_surf->h / 2 + vlog_.start_or_finish_item_gap_x + text_surf->h / 2;
		use_no_swap_wh_tex_render_rectangle_text(renderer, drawitem_start_caption, text_surf,
			video_dst, margin, &icon, fill_color,
			line_color, line_width, radius, true, true, true, true, &extra_offset);
	}

	// title
	if (!vlog_cfg_.hides[vlog_cfg_.hid_start_workout_name]) {
		use_no_swap_wh_tex_render_surf(renderer, drawitem_start_title, title_surf, video_dst);
	}

	// history
	if (!vlog_cfg_.hides[vlog_cfg_.hid_start_history]) {
		const aplt::twko_tlv_history_C& history = vlog_.last_tlv_history;
		utils::string_map symbols;

		std::vector<std::pair<std::string, std::string> > cols_data;
		int days = history.days;
		if (history.start_of_lastday == utils::calculate_0h0m0s_ts(time(nullptr))) {
			days ++;
		}
		symbols["count"] = str_cast(days);
		cols_data.emplace_back(vgettext2("$count days", symbols), _("Total check-ins"));
		symbols["count"] = str_cast(history.workouts);
		cols_data.emplace_back(vgettext2("$count times", symbols), _("Total workouts"));
		if (history.reps != 0) {
			symbols["count"] = str_cast(history.reps);
			cols_data.emplace_back(vgettext2("$count reps", symbols), _("Total reps"));
		} else {
			cols_data.emplace_back(utils::format_elapse_hms(history.duration_s, utils::timesep_i18n, true), _("Total duration"));
		}
		const std::string last_checkin = history.start_of_lastday != 0? 
			utils::format_time_ymd3(history.start_of_lastday): "---";
		cols_data.emplace_back(last_checkin, _("Last check-in"));

		const int max_width = video_dst.h - vlog_.start_finish_margin.l - vlog_.start_finish_margin.r;
		fill_color = SDL_DColor{0.0, 0.0, 0.0, 0.5};
		SDL_Point margin2{(int)(twidget::hdpi_scale * 10), (int)(twidget::hdpi_scale * 10)};
		cv::Mat poses_mat = draw_multcol_mat(cairo::multcolmattype_start_history, max_width, radius, fill_color, margin2, cols_data, vlog_.analyze_result);

		extra_offset.x = title_size.h / 2 + vlog_.start_or_finish_history_gap_x;
		extra_offset.y = vlog_.start_finish_margin.l;
		use_no_swap_wh_tex_render_surf(renderer, drawitem_start_history, poses_mat, video_dst, &extra_offset);
	}
}

void trdcamera::use_no_swap_wh_tex_render_finish(SDL_Renderer* renderer, const aplt::twkoscript& script,
	const SDL_Rect& video_dst, const SDL_Size& margin, double radius)
{
	SDL_Point extra_offset = {0};

	// 0:(speak), 1:(setup), 2(speak this)
	VALIDATE(wko_tlv_history_is_valid(vlog_.last_tlv_history), null_str);

	int font_size = font::SIZE_DEFAULT;
	std::string text;
	surface text_surf;

	SDL_DColor fill_color{0.0, 0.0, 0.0, 0.5};
	SDL_DColor line_color{0.0, 0.0, 0.0, 0.0};
	double line_width = 1.5;

	if (!vlog_.finished_tlv_history_loaded && !wko_tlv_history_is_valid(vlog_.finished_tlv_history)) {
		VALIDATE(vlog_.first_satisfied_ts > 0, null_str);
		aplt::thealth::thealth_result2 result2;
		bool retbool = health_.load_health_data_4_report(time(nullptr), result2);
		VALIDATE(result2.valid(), null_str);
		if (!result2.workouts.empty()) {
			// This time, a valid tworkout_result2 may not be generated, 
			// for example, if the duration is less than 30 seconds.
			aplt::thealth::tworkout_result2& workout = result2.workouts.back();

			int diff = SDL_abs(result2.start_of_today + workout.range_ms.min / 1000 - vlog_.first_satisfied_ts);
			if (diff < 5000) { // 5 seconds
				// Determine whether the end time of this tworkout_result2 is "now",
				// to confirm that it was newly generated this time.
				vlog_.finished_tlv_history = aplt::history_add(vlog_.last_tlv_history, result2.start_of_today, 
					workout.range_ms, script, result2.start_of_today + ONE_DAY_SECONDS);

				tchart_metrics metrics(sdl_field_small_font_size_); // 360
				int mat_width = window_->get_height() - metrics.map_margin_.x * 2;

				SDL_Size mat_size = draw_workout_mat3(metrics, cairo::wkomattype_vlog, mat_width, result2, result2.workouts.size() - 1, nullptr);

				const SDL_Size& margin = vlog_.chart_margin;
				const tpoint ratio_size = calculate_adaption_ratio_size(video_dst.h - 2 * margin.w, video_dst.w - 2 * margin.h, mat_size.w, mat_size.h);
				int min_increase_theshold = 60 * twidget::hdpi_scale;

				int chart_height_2th = metrics.chart_height_;
				if (ratio_size.y < video_dst.w - (margin.h * 2 + min_increase_theshold)) {
					double ratio = 1.0 * ratio_size.y / mat_size.h;
					int desire_ration_h = video_dst.w - (margin.h * 2 + min_increase_theshold);
					int desire_mat_height = desire_ration_h / ratio;
					chart_height_2th += desire_mat_height - mat_size.h;
				}

				cv::Mat mat;
				tchart_metrics metrics2(sdl_field_small_font_size_, chart_height_2th / chart_height_using_hdpi_scale()); // 360
				draw_workout_mat3(metrics2, cairo::wkomattype_vlog, mat_width, result2, result2.workouts.size() - 1, &mat);
				vlog_.chart_mat = mat;

			} else {
				SDL_Log("{dbg-history}use_no_swap_wh_tex_render_finish, diff(%i) >= 5000, no history.", diff);
			}
		} else {
			SDL_Log("{dbg-history}use_no_swap_wh_tex_render_finish, result2.workouts is empty, no history.");
		}
		vlog_.finished_tlv_history_loaded = true;
	}

	if (vlog_.finish_start_ticks == 0) {
		vlog_.finish_start_ticks = SDL_GetTicks();
	}

	if (!vlog_cfg_.hides[vlog_cfg_.hid_finish_chart] && wko_tlv_history_is_valid(vlog_.finished_tlv_history)) {
		const int min_threshold_ms = 3000;
		uint32_t min_start_chart_ticks = vlog_.finish_start_ticks + min_threshold_ms;

		if (!vlog_.finish_has_no_speak) {
			vlog_.finish_has_no_speak = !pinyin_.is_speaking();
		}

		if (vlog_.start_chart_ticks != 0 || (vlog_.finish_has_no_speak && SDL_GetTicks() >= min_start_chart_ticks)) {
			finish_render_chart(renderer, video_dst);
			return;
		}

	} else {
		int ii = 0;
	}

	// today
	if (!vlog_cfg_.hides[vlog_cfg_.hid_finish_date]) {
		const aplt::twko_tlv_history_C& history = vlog_.finished_tlv_history;

		if (wko_tlv_history_is_valid(history)) {
			int64_t ts = history.start_of_lastday + history.last_range_ms.max / 1000; 
			text = utils::format_time_ymdhms3(ts);

		} else {
			text = utils::format_time_ymd3(time(nullptr));
		}
		text_surf = font::get_rendered_text(text, 0, font_size, font::NORMAL_COLOR);

		fill_color = SDL_DColor{0.0, 0.0, 0.0, 0.5};
		line_color = SDL_DColor{0.0, 0.0, 0.0, 0.0};
		line_width = 1.5;

		extra_offset = SDL_Point{vlog_.start_finish_margin.t, vlog_.start_finish_margin.r};
		use_no_swap_wh_tex_render_rectangle_text(renderer, drawitem_finish_today, text_surf,
			video_dst, margin, nullptr, fill_color,
			line_color, line_width, radius, true, true, true, true, &extra_offset);
	}

	utils::string_map symbols;
	std::string title = script.name;
	if (wko_tlv_history_is_valid(vlog_.finished_tlv_history)) {
		title.append(" . ");
		int elapse = (vlog_.finished_tlv_history.last_range_ms.max - vlog_.finished_tlv_history.last_range_ms.min) / 1000;
		symbols["time"] = utils::format_elapse_hms(elapse);
		title.append(vgettext2("Duration $time", symbols));
	}
	font_size = font::SIZE_LARGE;
	surface title_surf = font::get_rendered_text(title, 0, font_size, font::BIGMAP_COLOR);
	const SDL_Size title_size{title_surf->w, title_surf->h};

	// finish_caption
	if (!vlog_cfg_.hides[vlog_cfg_.hid_finish_caption] && !vlog_cfg_.finish_caption_msg.empty()) {
		font_size = font::SIZE_LARGEST + font::SIZE_SMALLEST;
		text_surf = font::get_rendered_text(vlog_cfg_.finish_caption_msg, 0, font_size, font::YELLOW_COLOR);

		fill_color = SDL_DColor{1.0, 1.0, 0.0, 0.5};
		line_color = SDL_DColor{1.0, 1.0, 0.0, 1.0};
		line_width = 1.5;

		extra_offset.x = title_surf->h / 2 + vlog_.start_or_finish_item_gap_x + text_surf->h / 2;
		use_no_swap_wh_tex_render_surf(renderer, drawitem_finish_caption, text_surf,
			video_dst, &extra_offset);
	}

	// title
	if (!vlog_cfg_.hides[vlog_cfg_.hid_finish_workout_summary]) {
		fill_color = SDL_DColor{0.0, 0.0, 0.0, 0.5};
		line_color = SDL_DColor{0.0, 0.0, 0.0, 0.0};
		line_width = 1.5;

		int icon_s = title_surf->h * 4 / 5;
		surface icon_surf = scale_surface(finish_title_surf_, icon_s, icon_s);
		cairo::ticon icon;
		icon.set_surf(icon_surf, vlog_.icon_surf_gap_x);

		// const SDL_Size margin2{(int)(twidget::hdpi_scale * 12), (int)(twidget::hdpi_scale * 4)};
		const SDL_Size margin2{(int)(twidget::hdpi_scale * 16), (int)(twidget::hdpi_scale * 8)};
		double radius2 = 15;

		use_no_swap_wh_tex_render_rectangle_text(renderer, drawitem_finish_title, title_surf,
			video_dst, margin2, &icon, fill_color,
			line_color, line_width, radius2, true, true, true, true, nullptr);
	}

	// history
	if (!vlog_cfg_.hides[vlog_cfg_.hid_finish_history]) {
		if (!wko_tlv_history_is_valid(vlog_.finished_tlv_history)) {
			return;
		}

		const aplt::twko_tlv_history_C& history = vlog_.finished_tlv_history;

		std::vector<std::pair<std::string, std::string> > cols_data;
		symbols["count"] = str_cast(history.days);
		cols_data.emplace_back(vgettext2("$count days", symbols), _("Total check-ins"));
		symbols["count"] = str_cast(history.workouts);
		cols_data.emplace_back(vgettext2("$count times", symbols), _("Total workouts"));
		if (history.reps != 0) {
			symbols["count"] = str_cast(history.reps);
			cols_data.emplace_back(vgettext2("$count reps", symbols), _("Total reps"));
		} else {
			cols_data.emplace_back(utils::format_elapse_hms(history.duration_s, utils::timesep_i18n, true), _("Total duration"));
		}

		// it is from last_tlv_history's checkin, not new's.
		const std::string last_checkin = vlog_.last_tlv_history.start_of_lastday != 0?
			utils::format_time_ymd3(vlog_.last_tlv_history.start_of_lastday): "---";
		cols_data.emplace_back(last_checkin, _("Last check-in"));
		
		const int max_width = video_dst.h - vlog_.start_finish_margin.l - vlog_.start_finish_margin.r;
		fill_color = SDL_DColor{0.0, 0.0, 0.0, 0.5};
		SDL_Point margin2{(int)(twidget::hdpi_scale * 10), (int)(twidget::hdpi_scale * 10)};
		cv::Mat poses_mat = draw_multcol_mat(cairo::multcolmattype_finish_history, max_width, radius, fill_color, margin2, cols_data, vlog_.analyze_result);

		extra_offset.x = title_size.h / 2 + vlog_.start_or_finish_history_gap_x;
		extra_offset.y = vlog_.start_finish_margin.l;
		use_no_swap_wh_tex_render_surf(renderer, drawitem_finish_history, poses_mat, video_dst, &extra_offset);
	}
}

void trdcamera::finish_render_chart(SDL_Renderer* renderer, const SDL_Rect& video_dst)
{
	VALIDATE(!vlog_.chart_mat.empty(), null_str);

	if (vlog_.start_chart_ticks == 0) {
		VALIDATE(vlog_.chart_bg_surf.get() == nullptr, null_str);
		vlog_.start_chart_ticks = SDL_GetTicks();
	}

	surface& bg_surf = vlog_.chart_bg_surf;
	if (bg_surf.get() == nullptr || bg_surf->w != video_dst.h || bg_surf->h != video_dst.w) {
		const SDL_Size& margin = vlog_.chart_margin;
		const tpoint ratio_size = calculate_adaption_ratio_size(video_dst.h - 2 * margin.w, video_dst.w - 2 * margin.h, vlog_.chart_mat.cols, vlog_.chart_mat.rows);

		bg_surf = create_neutral_surface(video_dst.h, video_dst.w);
		fill_surface(bg_surf, 0xff000000);

		cv::Mat dst_mat;
		cv::resize(vlog_.chart_mat, dst_mat, cv::Size(ratio_size.x, ratio_size.y));

		SDL_Rect dst_rect{(video_dst.h - dst_mat.cols) / 2, (video_dst.w - dst_mat.rows) / 2, 
			dst_mat.cols, dst_mat.rows};
		sdl_blit(dst_mat, nullptr, bg_surf, &dst_rect);
	}

	uint32_t now = SDL_GetTicks();
	uint32_t opaque_ticks = vlog_.start_chart_ticks + vlog_.chart_fadein_threshold_ms;
	uint8_t alpha = 255;
	if (now < opaque_ticks) {
		alpha = 255.0 * (opaque_ticks - now) / vlog_.chart_fadein_threshold_ms;
		alpha = 255 - alpha;
		// SDL_Log("%u, opaque_ticks(%u) - now: %u -> alpha: %i", now, opaque_ticks, opaque_ticks - now, (int)alpha);
	}

	const uint8_t* pixels = (uint8_t*)bg_surf->pixels;
	int alpha_channel_at = 3;
	if (alpha != pixels[alpha_channel_at]) {
		tsurface_2_mat_lock lock(bg_surf);
		cv::Mat& tmp = lock.mat;
		tmp.reshape(1, tmp.total()).col(alpha_channel_at).setTo(alpha);
	}

	SDL_Point extra_offset{0, 0};
	use_no_swap_wh_tex_render_surf(renderer, drawitem_finish_chart, bg_surf, video_dst, &extra_offset);
}

int calc_max_gui_pose_name_chars(int pose_count)
{
	if (pose_count <= 2) {
		return 13;

	} else if (pose_count == 3) {
		return 11;

	} else if (pose_count == 4) {
		return 9;

	} else if (pose_count == 5) {
		return 8;

	} else if (pose_count == 6) {
		return 7;

	} else {
		return 6;
	}
}

void trdcamera::camera_did_draw_slice_c(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	if (is_cpu_saver()) {
		return;
	}
	const SDL_Rect video_dst = camera_.camera_did_draw_slice_use_cv_frame(id, renderer, locals, locals_count, remotes, remotes_count, draw_rect);

	if (!vlog_mode_) {
		return;
	}
	trtc_client::VideoRenderer& vsink = *locals[0];
	bool use_no_swap_wh_tex = vsink.use_no_swap_wh_tex();

	VALIDATE(vlog_.script != nullptr && vlog_.script->valid(), null_str);
	const aplt::twkoscript& script = *vlog_.script;

	const SDL_Size rounded_margin{(int)(twidget::hdpi_scale * 12), (int)(twidget::hdpi_scale * 4)};
	double radius = 20;

	if (vlog_.show_rec_toast_ticks_ != 0) {
		if (SDL_GetTicks() < vlog_.show_rec_toast_ticks_) {
			int font_size = font::SIZE_LARGE;
			surface text_surf = font::get_rendered_text(_("vlog rec toast"), 0, font_size, font::YELLOW_COLOR);
			SDL_DColor fill_color{0.0, 0.0, 0.0, 0.5};
			SDL_DColor line_color{0.0, 0.0, 0.0, 0.0};
			double line_width = 1.5;

			SDL_Point extra_offset{vlog_.margin.t, 0};
			SDL_Rect dstrect = use_no_swap_wh_tex_render_rectangle_text(renderer, drawitem_rec_toast, text_surf,
				video_dst, rounded_margin, nullptr, fill_color,
				line_color, line_width, radius, true, true, true, true, &extra_offset);
		} else {
			vlog_.show_rec_toast_ticks_ = 0;
		}
	}

	const aplt::twkoscript::tstate2& state2 = script.states.find(vlog_.state_at)->second;
	if (state2.task->type == aplt::twkoscript::tasktype_speak) {
		if (state2.state == 2) {
			use_no_swap_wh_tex_render_start(renderer, script, video_dst, rounded_margin, radius);
			return;
		}
		if (state2.state == (int)script.states.size() - 1) {
			use_no_swap_wh_tex_render_finish(renderer, script, video_dst, rounded_margin, radius);
			return;
		}
	}
	const int& curr_phase = state2.track_pose.curr_phase_;

	SDL_Point extra_offset = {0};
	int offset_x = vlog_.margin.t;
	// 1/4)state_name
	if (!vlog_cfg_.hides[vlog_cfg_.hid_active_state_name]) {
		std::string state_name2;
		if (vlog_.pose_state_count > 1 && !state2.is_setup) {
			if (!state2.track_pose.poses.empty()) {
				VALIDATE(state2.pose_state_at_ != nposm, null_str);
				state_name2.append(str_cast(state2.pose_state_at_ + 1));
				// state_name2.append(" / " + str_cast(vlog_.pose_state_count) + " . ");
				state_name2.append("/" + str_cast(vlog_.pose_state_count) + " ");
			}
		}

		state_name2.append(utils::truncate_to_max_chars2(script.state_names[state2.state], 12, true));
		int font_size = font::SIZE_LARGE;
		surface text_surf = font::get_rendered_text(state_name2, 0, font_size, font::BIGMAP_COLOR);

		SDL_DColor fill_color{0.0, 0.0, 0.0, 0.5};
		SDL_DColor line_color{0.0, 0.0, 0.0, 0.0};
		double line_width = 1.5;

		cairo::ticon icon;
		double s = text_surf->h * 2 / 4;
		int vertical_level = rpy_sensor_.pitch_vertical_level(nullptr);
		SDL_DColor icon_color{1.0, 0.0, 0.0, 1.0};
		if (vertical_level == aplt::trpy_sensor::level_ok) {
			icon_color = SDL_DColor{0.0, 1.0, 0.0, 1.0};

		} else if (vertical_level == aplt::trpy_sensor::level_warn) {
			icon_color = SDL_DColor{0.0, 0.0, 1.0, 1.0};
		}
		icon.set_cairo_shape(cairo::shapetype_circle_solid, icon_color, SDL_DSize{s, s}, 0, 2 * twidget::hdpi_scale);

		extra_offset = SDL_Point{offset_x, vlog_.margin.l};
		SDL_Rect dstrect = use_no_swap_wh_tex_render_rectangle_text(renderer, drawitem_active_statename, text_surf,
			video_dst, rounded_margin, &icon, fill_color,
			line_color, line_width, radius, true, true, true, true, &extra_offset);

		offset_x += dstrect.w;
	}

	// 2/4)phase
	utils::string_map symbols;
	std::string phase_str;
	if (state2.task->type == aplt::twkoscript::tasktype_time_counter) {
		// 42/60
		const aplt::twkoscript::ttime_counter* task2 = static_cast<const aplt::twkoscript::ttime_counter*>(state2.task);
		phase_str = str_cast(task2->total_satisfied_ms() / 1000);
		phase_str.append(" / " + str_cast(task2->max_count) + dgettext("rose-lib", "time^s"));
		
	} else if (state2.task->type == aplt::twkoscript::tasktype_rep_counter) {
		const aplt::twkoscript::trep_counter* task2 = static_cast<const aplt::twkoscript::trep_counter*>(state2.task);
		phase_str = str_cast(task2->rt_count());
		symbols["count"] = str_cast(task2->max_count);
		phase_str.append(" / " + vgettext2("$count reps", symbols));

		phase_str.append(" . " + task2->phases[curr_phase].action_msg);

	} else {
		VALIDATE(state2.task->type == aplt::twkoscript::tasktype_speak, null_str);
	}
	if (!phase_str.empty() && !vlog_cfg_.hides[vlog_cfg_.hid_active_progress]) {
		int font_size = font::SIZE_DEFAULT;
		surface text_surf = font::get_rendered_text(phase_str, 0, font_size, font::YELLOW_COLOR);

		SDL_DColor fill_color{1.0, 1.0, 0.0, 0.5};
		SDL_DColor line_color{1.0, 1.0, 0.0, 1.0};
		double line_width = 1.5;

		offset_x += vlog_.item_gap_x;
		extra_offset = SDL_Point{offset_x, vlog_.margin.l};
		SDL_Rect dstrect = use_no_swap_wh_tex_render_rectangle_text(renderer, drawitem_active_phase, text_surf,
			video_dst, rounded_margin, nullptr, fill_color,
			line_color, line_width, radius, true, true, true, true, &extra_offset);

		offset_x += dstrect.w;
	}

	// 3/4)time
	if (vlog_.first_satisfied_ticks != 0 && !vlog_cfg_.hides[vlog_cfg_.hid_active_timer]) {
		int font_size = font::SIZE_LARGEST;
		int elapse = (SDL_GetTicks() - vlog_.first_satisfied_ticks) / 1000;
		// int elapse = 0 * 3600 + 15 * 60 + 34;
		// std::string time_str = utils::format_elapse_hms(elapse_s, utils::timesep_colon);
		// surface text_surf1 = font::get_rendered_text(time_str, 0, font_size, font::BIGMAP_COLOR);
		surface text_surf = image_digits_.get_surf(elapse);

		extra_offset = SDL_Point{vlog_.margin.t, vlog_.margin.r};
		use_no_swap_wh_tex_render_surf(renderer, drawitem_active_time, text_surf, video_dst, &extra_offset);
	}

	// 4/4)pose metrics
	if (!vlog_cfg_.hides[vlog_cfg_.hid_active_poses]) {
		std::vector<std::pair<std::string, std::string> > cols_data;
		int pose_count = state2.track_pose.poses.size();
		char buf[16] = {'-', '-', '-'};

		int phase_miss = 0;
		const uint32_t curr_phase_mask = BIT_IDX_MASK(state2.track_pose.curr_phase_);
		// The first 'for' is only to calculate 'max_gui_pose_name_chars'.
		for (int at = 0; at < pose_count; at ++) {
			const aplt::twkoscript::tpose& pose = state2.track_pose.poses[at];
			if ((pose.phase_mask & curr_phase_mask) == 0) {
				phase_miss ++;
				continue;
			}
		}
		const int max_gui_pose_name_chars = calc_max_gui_pose_name_chars(pose_count - phase_miss);
		// The second 'for' is only to calculate 'max_gui_pose_name_chars'.
		phase_miss = 0;
		for (int at = 0; at < pose_count; at ++) {
			const aplt::twkoscript::tpose& pose = state2.track_pose.poses[at];
			if ((pose.phase_mask & curr_phase_mask) == 0) {
				phase_miss ++;
				continue;
			}

			if (vlog_.analyze_result.fields != 0) {
				VALIDATE(vlog_.analyze_result.fields > at - phase_miss, null_str);
				snprintf(buf, sizeof(buf), "%.2f", vlog_.analyze_result.d[at - phase_miss]);
			}
			cols_data.emplace_back(buf, 
				utils::truncate_to_max_chars2(pose.name, max_gui_pose_name_chars, true));
		}
		VALIDATE((int)cols_data.size() + phase_miss == pose_count, null_str);
		if (vlog_.analyze_result.fields != 0) {
			VALIDATE(vlog_.analyze_result.fields == (int)cols_data.size(), null_str);
		}
		if (!cols_data.empty()) {
			const int max_width = video_dst.h - vlog_.margin.l - vlog_.margin.r;
			SDL_DColor fill_color{0.0, 0.0, 0.0, 0.5};
			SDL_Point margin{(int)(twidget::hdpi_scale * 10), (int)(twidget::hdpi_scale * 10)};

			cv::Mat poses_mat = draw_multcol_mat(cairo::multcolmattype_poses, max_width, radius, fill_color, margin, cols_data, vlog_.analyze_result);

			if (vlog_cfg_.poses_on_top) {
				extra_offset.x = offset_x + vlog_.item_gap_x;

			} else {
				extra_offset.x = video_dst.w - vlog_.poses_bottom_gap_x - poses_mat.rows;
			}
			extra_offset.y = vlog_.margin.l;
			use_no_swap_wh_tex_render_surf(renderer, drawitem_active_poses, poses_mat, video_dst, &extra_offset);
		}
	}
}

SDL_Point trdcamera::camera_did_video_align_c()
{
	if (vlog_mode_) {
		return SDL_Point{halign_center, vlog_cfg_.video_on_center? valign_center: valign_bottm};
	}
	return SDL_Point{halign_center, valign_center};
}

extern bool can_switch_to_camera_layer(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::tbg_task::tbase_bg_task2& sys_task);
/*
void trdcamera::bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	if (case_ != case_base_subtask) {
		return;
	}

	if (can_switch_to_camera_layer(applets_, sys_task)) {
		tcamera::tslot& slot = get_camera_slot();
		VALIDATE(&slot == &base_driver_, null_str);
	}
}

void trdcamera::bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	if (case_ != case_base_subtask) {
		return;
	}

	if (can_switch_to_camera_layer(applets_, sys_task)) {
		tcamera::tslot& slot = get_camera_slot();
		VALIDATE(&slot == &base_driver_, null_str);
	}
}
*/
void trdcamera::base_scene_state_changed(const aplt::tbase_scene& scene, int to_state)
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

/*
int pitch_level(double pitch_deg)
{
	int level = level_fail;
	if (pitch_deg <= -87) {
		level = level_ok;

	} else if (pitch_deg <= -85) {
		level = level_warn;
	}
	return level;
}
*/
std::string pitch_str(double pitch_deg, int vertical_level)
{
	char buf[16];
	SDL_snprintf(buf, sizeof(buf), "%.2f", pitch_deg);

	uint32_t color = 0xffff0000;
	if (vertical_level == aplt::trpy_sensor::level_ok) {
		color = 0xff00ff00;

	} else if (vertical_level == aplt::trpy_sensor::level_warn) {
		// why not use yellow?
		// --On a white background, yellow is not clearly visible.
		color = 0xff0000ff;
		// color = 0xffffff00;
	}

	return ht::generate_format(buf, color);
}

static int pitch_vertical_level2(float pitch)
{
	// reference: trpy_sensor::pitch_vertical_level(...) const
    int result = aplt::trpy_sensor::level_fail;
/*
    if (!has_data_) {
        return result;
    }
*/    
	if (pitch <= -87) {
		result = aplt::trpy_sensor::level_ok;

	} else if (pitch <= -85) {
		result = aplt::trpy_sensor::level_warn;
	}
	return result;
}

void trdcamera::app_timer_handler(uint32_t now)
{
/*
	refresh_statusbar_grid(now);
	set_rpy_label();
*/
	// double pitch = instance->pitch_filter().sync_val();
	float pitch_deg = 0.0f;
	int vertical_level = nposm;
	if (!use_fake_pitch_val_) {
		vertical_level = rpy_sensor_.pitch_vertical_level(&pitch_deg);

	} else {
		pitch_deg = fake_pitch_vals_[fake_pitch_val_at_];
		vertical_level = pitch_vertical_level2(pitch_deg);
	}

	// char buf[32];
	// SDL_snprintf(buf, sizeof(buf), "%.2f", pitch_deg);
	pitch_widget_->set_label(pitch_str(pitch_deg, vertical_level));

	status_widget_->set_label(status_msg_);
}

void trdcamera::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case MSG_CALIBRATE_NEAR:
		{
/*
			tmsg_data_calibrate_near* pdata = static_cast<tmsg_data_calibrate_near*>(msg->pdata);

			char buf[128];
			SDL_snprintf(buf, sizeof(buf), "(%.5f, %.5f, %.5f)", pdata->PRP_diff.x, pdata->PRP_diff.y, pdata->PRP_diff.z);
			utils::string_map symbols;
			symbols["PRP_diff"] = buf;
			SDL_snprintf(buf, sizeof(buf), "PRP_diff.x - fk_diff_.x(%.5f) = %.5f", 
				pdata->fk_diff.x, fabs(pdata->PRP_diff.x) - fabs(pdata->fk_diff.x));
			symbols["error_x"] = buf;
			gui2::show_message(null_str, vgettext2("PRP_diff: $PRP_diff\nerror_x: $error_x", symbols));
*/
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

