#ifndef GUI_DIALOGS_RDCAMERA_HPP_INCLUDED
#define GUI_DIALOGS_RDCAMERA_HPP_INCLUDED

#include "base_instance.hpp"
#include "rose_mediapipe_api.hpp"
#include "base_driver_core.hpp"
#include "wkoscript.hpp"
#include "health.hpp"
#include "wkocourse.hpp"

struct tvlog_cfg;

inline bool is_valid_float(float f)
{
    // Under -O2/-O3, the compiler will combine these two statements into a single underlying CPU instruction, with zero overhead.
    return !std::isnan(f) && !std::isinf(f);
}

// webrtc
// #include <rtc_base/event.h>

class timage_digits
{
public:
	timage_digits(int height = nposm);

	void apply_new_height(int height);

	// digit_size: 48x72, colon_width = 16, gap = 4
	SDL_Size get_size();
	const surface& get_surf(int elapse_s);

	// void render(SDL_Renderer* renderer, int elapse);

private:
	const int std_width_;
	const int std_height_;
	const int std_colon_w_;
	const int std_gap_w_;
	bool has_hour_;
	int height_;
	int width_;
	int colon_w_;
	int gap_w_;

	surface bg_surf_;
	std::vector<surface> digit_surfs_;
	surface colon_surf_;
};


class FaceOverlayTracker {
public:
    // miss_threshold: 连续检测不到人脸的最大帧数
    FaceOverlayTracker(int miss_threshold = 15) 
        : miss_threshold_(miss_threshold) 
    {
        memset(last_valid_landmarks_, 0, sizeof(last_valid_landmarks_));
        miss_counter_ = 0;
        has_valid_face_ = false;
    }

    // 核心渲染函数 (纯指针传递，避免拷贝)
    // landmarks: 指向 33 个归一化 SDL_FPoint 的数组指针
    // avatar_img: 遮脸头像图片 (OpenCV Mat)
    // background_img: 待叠加的背景图 (OpenCV Mat)
    void render(const SDL_FPoint* landmarks, 
                const cv::Mat& avatar_img, 
                cv::Mat& background_img) 
    {
        // 直接从 background_img 获取宽高
        int img_width = background_img.cols;
        int img_height = background_img.rows;

        // --- 1. 判定有效性 (依靠背景图宽高来计算阈值) ---
        bool detection_success = is_landmark_valid(landmarks);

        // --- 2. 状态机逻辑 (防闪屏抖动) ---
        if (detection_success) {
            // 检测成功：将当前帧的坐标存储到历史缓存中
			memcpy(last_valid_landmarks_, landmarks, sizeof(SDL_FPoint) * mediapipe::kNumPoseLandmarks);
            miss_counter_ = 0;
            has_valid_face_ = true;
        } else {
            // 检测失败：计数器+1
            miss_counter_++;
            if (miss_counter_ > miss_threshold_) {
                // 连续丢失帧数超过阈值，判定用户确实离开画面
                has_valid_face_ = false;
                return; 
            }
            // 如果未超过阈值，但历史从未检测到过人（比如刚启动App），直接返回不渲染
            if (!has_valid_face_) {
                return; 
            }
            // 否则使用缓存的 last_valid_landmarks_ 继续进行后续绘制
        }

        // --- 3. 执行头像绘制 (此时 landmarks 必定有效，取缓存值) ---
        render_avatar(last_valid_landmarks_, avatar_img, background_img, img_width, img_height);
    }

private:
	bool is_landmark_valid(const SDL_FPoint* landmarks);

    // 实际的 2D 渲染逻辑
    void render_avatar(const SDL_FPoint* landmarks, 
                       const cv::Mat& avatar_img, 
                       cv::Mat& background_img, 
                       int img_width, int img_height);

private:
	int miss_threshold_;                 // 允许的丢失帧数阈值
    int miss_counter_;                   // 当前丢失帧计数器
    bool has_valid_face_;                // 是否曾经成功检测到人脸
	SDL_FPoint last_valid_landmarks_[mediapipe::kNumPoseLandmarks];
};

namespace gui2 {

class tbutton;
class ttrack;
class tlabel;

class trdcamera: public tdialog, /*public tstatusbar, public tdcamera_slot_impl, */
	public tbase_msg_subscriber, public tcamera::tviewer, public tcamera::tslot, public aplt::twko_task_slot
{
public:
	struct tslot
	{
	public:
		tslot()
			: rdcamera_(nullptr)
			, window_(nullptr)
			, case_(nposm)
		{}
		virtual ~tslot() {}
/*
		virtual void rdcamera_pre_show(twindow& window) {}
		virtual void rdcamera_post_show() {}
		virtual void rdcamera_first_drawn() {}
		virtual void rdcamera_resize_screen() {}

		virtual void rdcamera_timer_handler(uint32_t now) {}
		virtual void rdcamera_OnMessage(rtc::Message* msg) {}
*/
	public:
		trdcamera* rdcamera_;
		twindow* window_;

		int case_;
	};

	trdcamera(tslot& slot, aplt::thealth& health, std::map<aplt::taplt_key, aplt::tapplet>& applets, /*net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tdcamera_driver& dcamera_driver, tdrivers_core& drivers,*/ tbase_driver_core& base_driver,
		const std::map<std::string, aplt::twkocourse_enroll>& wkocourse_enrolls, const std::map<std::string, aplt::twkocourse>& wkocourses,
		tcamera& camera/*, std::unique_ptr<tmoveit_aplt_task>& moveit_aplt_task*/, tvlog_cfg& vlog_cfg, int sdl_field_small_font_size);
	~trdcamera();

private:
	// Inherited from tdialog.
	void pre_show() override;

	// Inherited from tdialog.
	void post_show() override;

	// Inherited from tdialog, implemented by REGISTER_DIALOG.
	virtual const std::string& window_id() const;

	void click_wko_settings(tbutton& widget);
	void snapshot_last_frame();
	void click_switch_scene(tbutton& widget);
	void change_vlog_mode(bool enter);
	void signal_handler_longpress_paper(bool& halt, const tpoint& coordinate);
	void use_no_swap_wh_tex_render_start(SDL_Renderer* renderer, const aplt::twkoscript& script,
		const SDL_Rect& video_dst, const SDL_Size& margin, double radius);
	void use_no_swap_wh_tex_render_finish(SDL_Renderer* renderer, const aplt::twkoscript& script,
		const SDL_Rect& video_dst, const SDL_Size& margin, double radius);
	void finish_render_chart(SDL_Renderer* renderer, const SDL_Rect& video_dst);
	std::string get_xxx_caption_msg(bool start) const;
/*
	void click_start(tbutton& widget);
	void click_moveit_aplt(tbutton& widget);
	void click_view(tbutton& widget);
*/
	void click_snapshot(tbutton& widget);
/*
	void click_dbg_RP(tbutton& widget);
	void click_snapshot_depth(tbutton& widget);
	void click_reach_dcpitch(tbutton& widget);

	void did_operate_stopped() override;
	void set_set_ik_diff_label(double x_diff, double z_diff) override;
*/
	tcamera::tslot& get_camera_slot();
	void set_did_draw_slice_bh_in_viewer();

	void set_highlight(const std::string& msg, int threshold);
	void set_pitch_label(const std::string& msg);
	void set_status_label(const std::string& msg);

	void set_rpy_label();

	texture did_create_background_tex(ttrack& widget, const SDL_Rect& draw_rect);

	void did_draw_paper(gui2::ttrack& widget, const SDL_Rect& draw_rect, const bool bg_drawn);
	void did_mouse_leave_paper(gui2::ttrack& widget, const tpoint& first, const tpoint& last);
/*
	void did_draw_slice_bh(tmoveit_calculator& calculator, trtc_client::VideoRenderer& vsink, const SDL_Rect& draw_rect, const std::string& msg, const std::vector<SDL_2Point>& new_qrcode_corners, const std::vector<SDL_Rect>&reference_rects);
*/
	void did_draw_slice_bh(trtc_client::VideoRenderer& vsink, const SDL_Rect& draw_rect, const std::string& msg, const std::vector<SDL_2Point>& new_qrcode_corners, const std::vector<SDL_Rect>&reference_rects);

	// 
	// tcamera::tslot
	//
	void camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override;
	bool camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb) override;
	void camera_post_enter_task() override;
	void camera_pre_exit_task() override;
	void camera_work_frame(const surface& surf) override;
	void camera_post_switch_camera() override {}
/*
	// tdcamera_tslot
	void dcamera_did_OnFrame(int task, const tdcframe_C* frames, int count) override;
*/
	// tcamera::tviewer
	void camera_did_draw_slice_c(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override;
	SDL_Point camera_did_video_align_c() override;

	// tbase_msg_subscriber
	void bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task) override {}
	void bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task) override {}
	void base_scene_state_changed(const aplt::tbase_scene& scene, int to_state) override;

	//
	// twko_task_slot
	//
	bool is_overlay_landmarks() const override;
	bool is_overlay_analyze_msg() const override;
	void set_curr_script(const aplt::twkoscript& script, int state_at, int phase_at, uint32_t first_satisfied_ticks, int64_t first_satisfied_ts) override;
	void did_first_satisfied_frame(uint32_t ticks, int64_t ts) override;
	void did_enter_state_and_phase(int state_at, int phase_at) override;
	void did_analyze_result(const aplt::twko_analyze_result_C& result) override;
	void render_face_overlay(const SDL_FPoint* landmarks, int count, bool flip_h, cv::Mat& cv_argb) override;
	bool sfx_enabled() const override;

	void app_timer_handler(uint32_t now) override;

	enum {MSG_CALIBRATE_NEAR = POST_MSG_MIN_APP, MSG_CHANGE_VLOG_MODE};
	struct tmsg_data_calibrate_near: public rtc::MessageData {
		explicit tmsg_data_calibrate_near(const SDL_DPoint3& _PRP_diff, const SDL_DPoint3& _fk_diff)
			: PRP_diff(_PRP_diff)
			, fk_diff(_fk_diff)
		{
		}

		~tmsg_data_calibrate_near()
		{
		}

		const SDL_DPoint3 PRP_diff;
		const SDL_DPoint3 fk_diff;
	};

	void app_OnMessage(rtc::Message* msg) override;

private:
	tslot& slot_;
	aplt::tb_api& b_api_;
	aplt::thealth& health_;
/*
	tdcamera_driver& dcamera_driver_;
*/
	tbase_driver_core& base_driver_;
/*
	tmoveit_driver& moveit_driver_;
	tdrivers& drivers_;
	aplt::tbg_task2& bg_task2_;
	tros_instance& ros_instance_;
*/
	const std::map<std::string, aplt::twkocourse_enroll>& wkocourse_enrolls_;
	const std::map<std::string, aplt::twkocourse>& wkocourses_;
	tcamera& camera_;
	tvlog_cfg& vlog_cfg_;
	const int sdl_field_small_font_size_;
	aplt::trpy_sensor& rpy_sensor_;
	aplt::tpinyin& pinyin_;
/*
	std::unique_ptr<tmoveit_aplt_task>& moveit_aplt_task_;
*/
	enum {case_moveit, case_base_subtask};
	int& case_;
	uint32_t original_camera_flags_;
	std::unique_ptr<aplt::tdisable_new_klink_task_lock> disable_new_aplt_lock_;

	// Used to obtain mediapipe time without libkosapi.so.
	const bool use_mediapipe_;
	std::unique_ptr<mediapipe::tpose_tracking_api> mediapipe_api_ptr_;
	SDL_FPoint xy_landmarks_[mediapipe::kNumPoseLandmarks];
	const bool mediapipe_flip_h_;
	int spent_ms_;
	int total_valid_frames_;
	int64_t total_spent_ms_;
	cv::Mat output_mat_;
	threading::mutex mediapipe_mutex_;
	enum {frame_ok, frame_fail};
	int new_frame_state_;
	std::string last_mediapipe_msg_;

	bool save_work_frame_;

	// tbutton* start_widget_;
	tbutton* moveit_aplt_widget_;
	gui2::ttrack* paper_;
	tbutton* reach_dcpitch_widget_;
	tlabel* pitch_widget_;
	tlabel* status_widget_;
	tlabel* rpy_widget_;
/*
	std::map<int, std::string> moveit_ops_;
*/
	std::string start_avcapture_message_;

	std::string highlight_msg_;
	texture highlight_tex_;
	uint32_t highlight_ticks_;

	std::string work_highlight_msg_;

	const int offset_percent_threshold_;
	bool base_scene_result_cancel_;

	std::string status_msg_;

	const bool use_fake_pitch_val_;
	const std::vector<float> fake_pitch_vals_;
	int fake_pitch_val_at_;

	bool vlog_mode_;
	bool change_vlog_mode_pending_;
	struct tvlog
	{
	public:
		tvlog()
			: margin({(int)(12 * twidget::hdpi_scale), (int)(12 * twidget::hdpi_scale), 
				(int)(28 * twidget::hdpi_scale), (int)(12 * twidget::hdpi_scale)})
			, start_finish_margin({(int)(48 * twidget::hdpi_scale), (int)(48 * twidget::hdpi_scale), 
				(int)(28 * twidget::hdpi_scale), (int)(12 * twidget::hdpi_scale)})
			, item_gap_x(6 * twidget::hdpi_scale)
			, poses_bottom_gap_x(item_gap_x)
			, start_or_finish_item_gap_x(20 * twidget::hdpi_scale)
			, start_or_finish_history_gap_x(80 * twidget::hdpi_scale)
			, icon_surf_gap_x(4 * twidget::hdpi_scale)
			, rec_toast_threshold_s(4)
			, chart_margin({(int)(12 * twidget::hdpi_scale), (int)(16 * twidget::hdpi_scale)})
			, chart_fadein_threshold_ms(1200) // 1.2s
			, script(nullptr)
		{
			clear();
		}

		void clear()
		{
			VALIDATE(script == nullptr, null_str);

			pose_state_count = 0;
			state_at = nposm;
			phase_at = nposm;
			first_satisfied_ticks = 0;
			first_satisfied_ts = 0;
			memset(&analyze_result, 0, sizeof(analyze_result));

			init_wko_tlv_history(last_tlv_history);
			init_wko_tlv_history(finished_tlv_history);
			finished_tlv_history_loaded = false;

			original_camera_flags = nposm;
			show_rec_toast_ticks_ = 0;

			finish_start_ticks = 0;
			finish_has_no_speak = false;

			chart_mat = cv::Mat();
			start_chart_ticks = 0;
			chart_bg_surf = nullptr;
		}

	public:
		const tspace4 margin;
		const tspace4 start_finish_margin;
		const int item_gap_x;
		const int poses_bottom_gap_x;
		const int start_or_finish_item_gap_x;
		const int start_or_finish_history_gap_x;
		const int icon_surf_gap_x;
		const int rec_toast_threshold_s;
		const SDL_Size chart_margin;
		const int chart_fadein_threshold_ms;

		const aplt::twkoscript* script;
		int pose_state_count;
		int state_at;
		int phase_at;
		uint32_t first_satisfied_ticks;
		int64_t first_satisfied_ts;
		aplt::twko_analyze_result_C analyze_result;

		aplt::twko_tlv_history_C last_tlv_history;
		aplt::twko_tlv_history_C finished_tlv_history;
		bool finished_tlv_history_loaded;

		uint32_t original_camera_flags;
		uint32_t show_rec_toast_ticks_;

		uint32_t finish_start_ticks;
		bool finish_has_no_speak;

		cv::Mat chart_mat;
		uint32_t start_chart_ticks;
		surface chart_bg_surf;
	};
	tvlog vlog_;

	timage_digits image_digits_;
	cv::Mat avatar_mat_;
	FaceOverlayTracker face_overlay_tracker_;
	surface start_caption_surf_;
	surface finish_title_surf_;
};

} // namespace gui2

#endif

