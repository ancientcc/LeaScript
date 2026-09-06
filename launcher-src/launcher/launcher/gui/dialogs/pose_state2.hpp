#ifndef GUI_DIALOGS_POSE_STATE2_HPP
#define GUI_DIALOGS_POSE_STATE2_HPP

#include "gui/dialogs/statusbar.hpp"
#include "wkoscript.hpp"
#include "mediapipe/rose/mediapipe_api.hpp"
#include "cairo2.hpp"

namespace gui2 {

class treport;
class tstack;
class ttrack;
class tlist;
class ttoggle_panel;
class tlabel;
class tbutton;
class ttoggle_button;
class ttext_box;

class tpose_state2: public tdialog, public tstatusbar, public twko_state2
{
public:
	enum {STATE2_BASE_LAYER, STATE2_POSE_LAYER, STATE2_LAYER_COUNT};
	enum {TASK_TIME_COUNTER_LAYER, TASK_REP_COUNTER_LAYER};
	tpose_state2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, aplt::twkoscript::tstate2& state2, std::vector<std::string>& state_names, 
		const std::map<int, aplt::tpreset_pose>& preset_poses, int sdl_field_small_font_size, const std::string& phase_surf_dir);

	const aplt::twkoscript::tstate2& get_new_state2() const { return state2_; }
	bool phase_surf_updated() const { return phase_surf_updated_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void app_resize_screen() override;

	void click_back(tbutton& widget);
	void state2_stack_set_radio_layer(int layer);
	void pre_state2_base(tgrid& grid);
	void pre_state2_pose(tgrid& grid);

	void set_did_text_changed2(ttext_box& widget, int type, int fid, int index = nposm);
	void did_fid_text_changed(ttext_box& widget, int index, int fid);
	void set_min_max_placeholder(int pose_type);

	//
	// state2_base layer
	//
	void did_state2_bool_field_changed(ttoggle_button& widget, int fid);
	void update_unsatisfied_remark_label() const;
	void update_satisfied_remark_label() const;
	void did_task_type_report_item_changed(tgrid& grid, ttoggle_button& widget);
	int task_type_to_task_layer(int type) const;
	void pre_task_time_counter(tgrid& grid);
	void pre_task_rep_counter(tgrid& grid);
	void did_time_rule_report_item_changed(tgrid& grid, ttoggle_button& widget);
	void did_time_tone_report_item_changed(tgrid& grid, ttoggle_button& widget);
	void update_task_remark_label() const;

	//
	// state2_pose layer
	//
	void did_pose_report_item_changed(tgrid& grid, ttoggle_button& widget);
	std::string generate_pose_name_form_report(const aplt::twkoscript::tpose& pose, int at, int size) const;
	void update_pose_report_names();
	void click_phase_mask(tbutton& widget);
	void click_insert_pose(tbutton& widget);
	void click_erase_pose(tbutton& widget);
	void click_move_left_or_right(tbutton& widget, bool left);

	enum {recttype_landmark, recttype_btn, recttype_count};
	SDL_Point in_which_rect(int x, int y) const;

	void did_draw_landmark(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);
	void did_mouse_motion_landmark(ttrack& widget, const tpoint& first, const tpoint& last);
	void did_left_button_down_landmark(ttrack& widget, const tpoint& coordinate);
	void did_mouse_leave_landmark(ttrack& widget, const tpoint& first, const tpoint& last);

	void clear_rects()
	{
		memset(landmark_rects_, 0, sizeof(landmark_rects_));
		btn_clear_rect_ = empty_rect;
	}

	void clear_landmark_mat(bool draw);

	const aplt::twkoscript::tpose& curr_pose() const
	{
		const aplt::twkoscript::ttrack_pose& track_pose = state2_.track_pose;
		return track_pose.poses[curr_pose_at_];
	}

	aplt::twkoscript::tpose& mutable_curr_pose()
	{
		aplt::twkoscript::ttrack_pose& track_pose = state2_.track_pose;
		return track_pose.poses[curr_pose_at_];
	}

	enum {posetype2_angle3p, posetype2_angle3p_360, posetype2_angle2p, 
		posetype2_diff_point, posetype2_diff_x, posetype2_diff_y, posetype2_diff_dist_x, posetype2_diff_dist_y,
		posetype2_count};
#define posetype2_has_abs(type2) ((type2) == posetype2_diff_x || (type2) == posetype2_diff_y || \
	(type2) == posetype2_diff_dist_x || (type2) == posetype2_diff_dist_y)

	int pose_2_pose_type2(const aplt::twkoscript::tpose& pose) const;
	void pose_type2_2_pose(int type2, aplt::twkoscript::tpose& pose) const;
	std::string operand_result_desc(const twko_operand& operand, int pose_type2) const;
	void did_pose_type_report_item_changed(tgrid& grid, ttoggle_button& widget);

	std::string calc_result_may_invalid2(const aplt::twkoscript::tpose& pose, const SDL_FPoint* landmarks) const;
	void update_runtime_calc_result_label();
	void did_operand_landmark_changed(int operand_at, const twko_operand& operand);
	void insert_lmk_to_operand(int lmk, int operand_at);
	void erase_lmk_from_operand(int lmk, int operand_at);
	void clear_operand(int operand_at);
	std::string operand_landmarks_label(const twko_operand& operand) const;

	void reload_curr_phase_surf(bool load_always);
	void update_phase_surf_file();
	void click_fake_landmark(const SDL_Rect& rect);

	bool did_operand_list_row_pre_change(tlistbox& list, ttoggle_panel& row);
	void did_operand_list_row_changed(tlistbox& list, ttoggle_panel& row);
	void reload_operand_list(tlistbox& list);
	void did_abs_diff_changed(ttoggle_button& widget);

	void set_status_label(const std::string& msg);

	void app_timer_handler(uint32_t now) override;

	enum {MSG_RELOAD_OPERAND_LIST = POST_MSG_MIN_APP, MSG_BROWSE_FILE, MSG_CLICK_FAKE_LANDMARK};
	enum {browsefile_phase_surf_file, browsefile_count};
	struct tmsg_data_browse_file: public rtc::MessageData {
		tmsg_data_browse_file(int scene)
			: scene(scene)
		{
			VALIDATE(scene >= 0 && scene < browsefile_count, null_str);
		}

		~tmsg_data_browse_file() {}

		int scene;
	};

	struct tmsg_data_click_fake_landmark: public rtc::MessageData {
		tmsg_data_click_fake_landmark(const SDL_Rect& rect)
			: rect(rect)
		{
			// VALIDATE(scene >= 0 && scene < browsefile_count, null_str);
		}

		~tmsg_data_click_fake_landmark() {}

		SDL_Rect rect;
	};
	void app_OnMessage(rtc::Message* msg) override;

private:
	const aplt::twkoscript::tstate2& original_state2_;
	aplt::twkoscript::tstate2 state2_;
	// const std::string state_name_;
	const std::map<int, aplt::tpreset_pose>& preset_poses_;
	const int sdl_field_small_font_size_;
	const std::string phase_surf_dir_;
	// mediapipe::tpose_tracking_api& mediapipe_api_;
	const int pose_report_name_max_chars_;
	const std::map<int, std::string> time_rule_descs_;
	const std::map<int, std::string> time_tone_descs_;
	tstack* state2_stack_;
	tlabel* unsatisfied_remark_widget_;
	treport* task_type_report_;
	tstack* task_stack_;
	treport* time_rule_report_;
	treport* time_tone_report_;
	tlabel* satisfied_remark_widget_;
	tlabel* task_remark_widget_;
	ttrack* landmark_track_;
	treport* pose_report_;
	tbutton* phase_mask_widget_;
	treport* pose_type_report_;
	tlistbox* operand_list_;
	ttoggle_button* abs_diff_widget_;
	ttext_box* min_widget_;
	ttext_box* max_widget_;
	tbutton* move_left_widget_;
	tbutton* move_right_widget_;
	tlabel* status_widget_;

	//
	// state2_base layer
	//
	int curr_state2_layer_;

	//
	// state2_pose layer
	//
	// don't use 'aplt::twkoscript::tpose* curr_pose_'. std:vector<>'s pointer is unsafe.
	int curr_pose_at_;
	std::vector<std::string> poses_;

	cv::Mat landmark_mat_;
	SDL_Rect landmark_rects_[mediapipe::kNumPoseLandmarks];
	SDL_Rect btn_clear_rect_;
	int lmk33_mode_;

	class tclear_interest_hiting_lock
	{
	public:
		tclear_interest_hiting_lock(tpose_state2& state2)
			: state2_(state2)
		{}

		~tclear_interest_hiting_lock()
		{
			state2_.interest_hiting_ = SDL_Point{nposm, nposm};
		}

	private:
		tpose_state2& state2_;
	};
	SDL_Point interest_hiting_;

	std::map<int, tcode3> pose_type2s_;
	int curr_pose_type2_;

	SDL_Rect btn_rects_[lmk_btn_count];
	surface phase0_surf_;
	surface phase1_surf_;
	int curr_phase_at_;
	bool surf_landmarks_valid_;
	SDL_FPoint surf_landmarks_[mediapipe::kNumPoseLandmarks];
	bool phase_surf_updated_;

	std::unique_ptr<mediapipe::tpose_tracking_api> api_ptr_;
};

} // namespace gui2

#endif

