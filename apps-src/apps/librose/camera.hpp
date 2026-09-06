#ifndef LIBROSE_CAMERA_HPP_INCLUDED
#define LIBROSE_CAMERA_HPP_INCLUDED

#include "tflite.hpp"
#include "rtc_client.hpp"

// Originates from <librose>/gui/widgets/grid.hpp.
enum {
	valign_edge		= 1,
	valign_top		= 2,
	valign_center	= 3,
	valign_bottm	= 4,
	valign_mask		= 7,
    
	halign_edge		= 1 << 3,
	halign_left		= 2 << 3,
	halign_center	= 3 << 3,
	halign_right	= 4 << 3,
	halign_mask		= 7 << 3,    
};

class tcamera: public trtc_client::tadapter
{
public:
	// for launcher's gui2::tcenter dialog. 
	// tviewer only use to render video. so no camera_work_frame(...).
	class tviewer
	{
	public:
		// Adding '_c' is to distinguish the members in tcamera::tslot
		virtual void camera_post_enter_task_c() {}
		virtual void camera_pre_exit_task_c() {}
		virtual void camera_did_draw_slice_c(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) = 0;
		virtual SDL_Point camera_did_video_align_c() { return SDL_Point{halign_center, valign_center}; }
		// virtual void camera_work_frame_c(const surface& surf) {}
		// virtual void camera_post_switch_camera_c() {}
		virtual void camera_did_button_clicked_c(int msgid) {}
	};

	class tslot
	{
	public:
		tslot()
			: is_dcamera(false)
			, keep_DoWork_frame_(false)
			, camera_viewer_(nullptr)
		{}

		virtual void camera_post_enter_task();
		virtual void camera_pre_exit_task();
		virtual void camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect);
		virtual SDL_Point camera_did_video_align();
		virtual bool camera_use_cv_frame(const cv::Mat& argb, cv::Mat& cv_argb) { return false; }
		virtual void camera_work_frame(const surface& surf) {}
		virtual void camera_post_switch_camera() {}
		virtual void camera_did_button_clicked(int msgid);

		void set_camera_viewer(tcamera::tviewer* viewer);
		const tviewer* camera_viewer() const { return camera_viewer_; }

	public:
		bool is_dcamera;
		bool keep_DoWork_frame_;

	protected:
		tviewer* camera_viewer_;
	};

	class tdepth_slot
	{
	public:
		virtual trtc_client* camera_create_avcapture(int id, rtc::MessageHandler& dlg_handler, tadapter& adapter, const tpoint& desire_size) = 0;
		virtual trtc_client::VideoRenderer* camera_create_video_renderer(trtc_client& client, webrtc::VideoTrackInterface* track, const std::string& name, bool remote, int at, bool encode) = 0;
	};

	tcamera(const int cameraid, rtc::MessageHandler& dlg_handler, int expired_threshold);

	virtual ~tcamera()
	{
		VALIDATE(!avcapture_.get(), null_str);
		VALIDATE(slot_ == nullptr, null_str);
	}

	posix_noncopyable(tcamera);

	void set_depth_slot(tdepth_slot* depth_slot);
	void snapshot_depth(bool use_dcpitch, const std::string& png, const std::string& depth_data_file);

	void set_slot(tslot* slot);
	bool has_slot() const { return slot_ != nullptr; }
	tslot* get_slot() { return slot_; }
	void set_flags(uint32_t flags);
	uint32_t get_flags() const { return flags_; }
	void set_tflite(const tflite::tscript& script, const tflite::ttflite& tflite);

	enum {FLAG_SHOW_CANCEL = 0x1,
		FLAG_USE_TFLITE = 0x100,
		FLAG_HIDE_SWITCH = 0x200,
	};
	bool is_use_tflite(uint32_t flags) const { return flags & FLAG_USE_TFLITE; }
	bool is_use_tflite() const { return is_use_tflite(flags_); }

	enum {taskid_dnn, taskid_bgtask, taskid_base_subtask, taskid_rcamera, taskid_dcamera, taskid_moveit, taskid_aplt, taskid_min_app = 100};
	void enter_task(int taskid, uint32_t flags, bool no_swap_wh_for_screen);
	void exit_task(int taskid);
	bool tasking() const { return taskid_ != nposm; }
	int curr_taskid() const { return taskid_; }

	void main_OnFrame_when_idle();
	bool expired() const { return expired_ticks_ != 0 && SDL_GetTicks() >= expired_ticks_; }
	bool nosignal();
	int tasking_check_singal_threshold() const { return tasking_check_singal_threshold_; }

	const std::vector<std::pair<float, SDL_Rect> >& classifier_rects() const { return classifier_rects_; }

	const std::vector<std::string>& existing_cameras() const { return existing_cameras_; }
	const surface& last_image() const { return current_surf_.second; }

	enum {cameraid_base, cameraid_min_app};
	void switch_camera(int id);
	
	void slice(const SDL_Rect& draw_rect, bool immediately);
	void stop_avcapture();
	bool is_avcapture_started() const { return avcapture_.get() != nullptr; }
	trtc_client& get_avcapture()
	{
		VALIDATE(avcapture_.get() != nullptr, null_str);
		return *avcapture_.get();
	}

	void classifier_surface(const surface& surf, tflite::tresult& result);
	threading::mutex& get_variable_mutex() { return variable_mutex_; }
	const tflite::tresult& get_result() const { return result_; }
	void get_result_clear(tflite::tresult& result)
	{
		result = result_;
		result_.clear();
	}
	tflite::tresult& get_mutex_result() { return result_; }
	void clear_result();
	void set_aplt_overlay(const std::string& result_text, const std::vector<std::pair<float, SDL_Rect> >& classifier_rects);

	float score_threshold() const { return score_threshold_; }
	void set_score_threshold(float val)
	{ 
		VALIDATE(val > 0 && val < 1.0f, null_str); 
		score_threshold_ = val; 
	}

	void calculate_overlay_rects(const SDL_Rect& video_dst);
	void render_overlay_texs(SDL_Renderer* renderer, const SDL_Rect& video_dst);
	void render_classifier_result(SDL_Renderer* renderer, const cv::Mat& frame, const SDL_Rect& video_dst, bool use_no_swap_wh_tex);
	SDL_Rect camera_did_draw_slice_use_cv_frame(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect);

	bool can_switch_camera() const { return existing_cameras_.size() > 1; }
	void did_left_button_down_paper(gui2::ttrack& widget, const tpoint& coordinate);
	void did_mouse_motion_paper(gui2::ttrack& widget, const tpoint& first, const tpoint& last);
	bool did_mouse_leave_paper(gui2::ttrack& widget, const tpoint& first, const tpoint& last);
	void camera_OnMessage(rtc::Message* msg);

	struct tmsg_data_base_camera: public rtc::MessageData {
		explicit tmsg_data_base_camera(tcamera& owner, int _camera_id, int _msg_id)
			: owner(owner)
			, camera_id(_camera_id)
			, msg_id(_msg_id)
		{
			VALIDATE(owner.posting_msg_id_ == nposm, null_str);
			owner.posting_msg_id_ = msg_id;
		}
		~tmsg_data_base_camera()
		{
			VALIDATE(owner.posting_msg_id_ != nposm && owner.posting_msg_id_ == msg_id, null_str);
			owner.posting_msg_id_ = nposm;
		}

		tcamera& owner;
		const int camera_id;
		const int msg_id;
	};

protected:
	void start_avcapture(bool no_swap_wh_for_screen);
	void load_tflite_mode();
	void example_detector_internal(surface& surf);
	void load_overlay_texs();
	void release_overlay_texs();
	void deliver_frame_to_worker(trtc_client::VideoRenderer& vsink);

	std::vector<trtc_client::tusing_vidcap> app_video_capturer(int id, bool remote, const std::vector<std::string>& device_names) override;
	void did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override;
	trtc_client::VideoRenderer* app_create_video_renderer(trtc_client& client, webrtc::VideoTrackInterface* track, const std::string& name, bool remote, int at, bool encode) override;

private:
	void DoWork();
	void OnWorkStart();
	void OnWorkDone();

protected:
	const int cameraid_;
	rtc::MessageHandler& dlg_handler_;
	const int expired_threshold_;
	tslot* slot_;
	tflite::tscript script_;
	tflite::ttflite tflite_;

	tflite::tresult result_;
	threading::mutex variable_mutex_;
	std::string result_str_;
	std::vector<std::pair<float, SDL_Rect> > classifier_rects_;
	std::unique_ptr<trtc_client> avcapture_;

	texture cancel_tex_;
	SDL_Rect cancel_rect_;

	texture switch_camera_tex_;
	SDL_Rect switch_camera_rect_;
	int posting_msg_id_;

	tdepth_slot* depth_slot_;

private:
	tflite::tsession current_session_;
	int taskid_;
	uint32_t flags_;
	uint32_t render_overlay_texs_ticks_;
	const int tasking_check_singal_threshold_;
	const int idle_check_singal_threshold_;
	int frames_x_sec_ago_;
	uint32_t check_nosignal_ticks_;
	uint32_t expired_ticks_;
	std::pair<bool, surface> current_surf_; // first: valid. now can recognition.

	threading::mutex setting_mutex_;
	bool DoWork_running_;

	class tsetting_lock
	{
	public:
		tsetting_lock(tcamera& home)
			: home_(home)
			, original_setting_(home_.setting_)
		{
			home_.setting_ = true;
		}
		~tsetting_lock()
		{
			VALIDATE(home_.setting_, null_str);
			home_.setting_ = original_setting_;
		}

	private:
		tcamera& home_;
		const bool original_setting_;
	};
	bool setting_;

	surface persist_surf_;
	surface temperate_surf_;

	int next_recognize_frame_;

	std::unique_ptr<net::tworker> executor_;
	const int draw_interval_;
	uint32_t next_draw_ticks_;
	float score_threshold_;
	std::vector<std::string> existing_cameras_;

	tpoint last_coordinate_;
};

class tcamera_slot_lock
{
public:
	tcamera_slot_lock(tcamera& camera, tcamera::tslot& slot)
		: camera_(camera)
	{
		camera_.set_slot(&slot);
	}

	~tcamera_slot_lock()
	{
		camera_.set_slot(nullptr);
	}

private:
	tcamera& camera_;
};

#endif
