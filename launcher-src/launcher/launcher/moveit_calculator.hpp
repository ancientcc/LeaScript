#ifndef MOVEIT_CALCULATOR_HPP_INCLUDED
#define MOVEIT_CALCULATOR_HPP_INCLUDED

#include "camera.hpp"
#include "ros_instance.hpp"
#include "depthcapture.hpp"
#include <angles/angles.h>

// webrtc
// #include <rtc_base/event.h>

class tdcamera_driver;
class tdcamera_slot_impl;

#define MAX_DIST_SAMPLES		4

class tmoveit_calculator
{
public:
	tmoveit_calculator(std::map<aplt::taplt_key, aplt::tapplet>& applets, tdcamera_driver& dcamera_driver, tros_instance& ros_instance, tdcamera_slot_impl& dcamera_slot_impl);

	~tmoveit_calculator();

	void did_started(const tdcintrinsics_C& intrinsics, int width, int height);
	void did_stopped();
	bool is_started() const { return intrinsics_.valid; }

	// const tdcintrinsics_C& intrinsics() const { return intrinsics_; }

	bool find_reference_point(const cv::Mat& surf, const int16_t* depth_data, double depth_scale, SDL_Rect* result);
	// std::vector<SDL_Point3> calculate(const int16_t* depth_data, double depth_scale, const std::vector<SDL_Point>& color_points);
	std::vector<SDL_Point3> calculate(const int16_t* depth_data, double depth_scale, const std::vector<SDL_2Point>& color_points);
	bool dbg_load_files(const std::string& short_dbg_surf_png, const std::string& short_dbg_depth_data_dat, surface* surf, tdcdepth_data& dcdepth_data) const;
	void dbg_find_reference_point();
	bool dbg_save_d2c_png(gui2::tprogress_& progress, const tdcdepth_data& dcdepth_data, const uint8_t* tex_pixels, int task, bool all_rows);
	void dbg_depthdata_2_png(bool d2c, bool all_rows);

	void OnWorkStart() {}
	void OnWorkDone() {}
	void OnTriggerExit();
	void DoWork(bool& exit);

	enum {state_distance, state_ik, state_align, state_claw, state_grasp, state_count};
	bool require_wakeup_thread(int state) const 
	{
		VALIDATE(state >= 0 && state < state_count, null_str);
		return state == state_ik || state == state_claw || state == state_grasp; 
	}

	enum {grasp_horizontal, grasp_vertical, grasp_oblique};

	// moving for ik
	struct tm4ik
	{
		tm4ik()
		{
			clear();
		}

		bool confirming() const { return !is_float_nposm(TP_PRP_diff_x); }

		void clear()
		{
			stop_ticks = 0;
			TP_PRP_diff_x = float_nposm;
			move_dist = float_nposm;
			moveit_theta = float_nposm;
		}

		uint32_t stop_ticks;
		double TP_PRP_diff_x;
		double move_dist;
		double moveit_theta;
	};

	enum {scene_normal, scene_always_distance, scene_always_initial, scene_always_near, scene_calibrate_near, scene_count};
	bool is_normal_distance_state(int scene) const { return scene == scene_normal || scene == scene_always_distance; }
	struct toperate
	{
		toperate()
			: task_api(nullptr)
			, aplt(nullptr)
			, cfg_task(nullptr)
			, state(nposm)
			, near_move_finished(false)
			, ik_times(0)
		{}

		int scene;
		aplt::ttask_api* task_api;
		const aplt::tapplet* aplt;
		const aplt::tapplet::ttask* cfg_task;
		// std::string task_id;
		aplt::tmoveit_target_info_C target_info;
		geometry_msgs::Pose initial_fk;
		geometry_msgs::Pose near_fk;

		int state;
		uint32_t start_ticks;
		uint32_t goto_near_ticks;
		SDL_DPoint3 RP;
		SDL_DPoint3 TP;
		SDL_DPoint3 PRP;
		bool grasp;
		bool adjust_TP;
		int claw_tip_pos;
		double set_ik_dcpitch;
		int grasp_method;
		tpose3d ik_query;
		std::vector<double> ik_result;
		bool near_move_finished;
		bool ik_has_success;
		geometry_msgs::Pose near_moved_fk;
		double initial_fk_TP_PRP_diff_x;
		bool nearing;
		tm4ik m4ik;
		trotate_to_yaw rtyaw;
		bool claw_fail;

		bool stopping;
		int ik_times;
		SDL_DPoint3 calibrate_near_PRP_diff;

		double last_moveit_theta;
		double last_dcamera_theta_y0;
		double min_abs_dcamera_theta_y0;

		bool use_task_api;

		double first_dcpitch;
		double second_dcpitch;
		uint32_t goto_second_dcpitch_ticks;
		bool in_second_dcpitch;
	};
	void start_operate(int scene, aplt::ttask_api& task_api, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task);
	void stop_operate();
	std::string operate_slice(tdepthcapture::VideoRenderer2& vsink2, const std::vector<SDL_2Point>* object_points_ptr);

	bool operating() const { return operate_.state != nposm; }
	const toperate& operate() const { return operate_; }
	// if task_finished() is true, operating() must be true.
	bool task_finished() const { return task_finished_; }
	texture& get_state_tex(int state) { return state_texs_.find(state)->second; }
	struct tscene_desc
	{
		tscene_desc(int scene, const std::string& name)
			: scene(scene)
			, name(name)
		{}

		int scene;
		std::string name;
		texture tex;
	};
	const tscene_desc& get_scene_desc(int scene) { return scenes_.find(scene)->second; }

	std::vector<SDL_Rect> get_reference_rects()
	{
		std::vector<SDL_Rect> result = reference_rects_;
		reference_rects_.clear();
		return result;
	}

	bool reach_dcpitch_use_PRP_joint(const std::string& scene, double desire_dcpitch);
	void dbg_claw_joint();
	std::unique_ptr<tdeps_0lock>& deps_0lock() { return deps_0lock_; }

private:
	void resize_coor_data(bool is_x, int size);
	void reset_dist_samples();
	bool is_sample_stable(int type, const SDL_DPoint3& target_point);
	void verbose_arm_error(const SDL_DPoint3& RP, const SDL_DPoint3& PRP, const SDL_DPoint3& TP, const geometry_msgs::Pose curr_fk);
	void set_ik(const SDL_DPoint3& PRP_offset, const SDL_DPoint3& RP, const SDL_DPoint3& PRP, const SDL_DPoint3& TP);
	bool do_ik();
	bool handle_distance(tdepthcapture::VideoRenderer2& vsink2, const std::vector<SDL_2Point>& object_points, std::string& msg);
	bool reach_dcpitch_use_joint(const std::string& scene, const std::string& joint_name, double desire_dcpitch, double error_threshold);
	void save_dbg_d2c(const cv::Mat& argb_mat, const int16_t* depth_data, double depth_scale, bool is_RP) const;
	double calculate_desire_move_dist() const;
	void will_goto_state_distance();
	void move_public_vel(double dist, double moveit_theta, bool first, std::string& msg);
	void rotate_360_public_vel(double ang_diff);
	double moveit_theta_from_joint1_value() const;
	double zero_joint1_if_necessary();
	bool rotate_joint1_if_necessary(const std::string& scene, const double moveit_theta, std::string& msg);
	bool rotate_joint1_4_align(const std::string& scene, double moveit_theta, double dcamera_theta_y0, std::string& msg);
	bool rotate_robot_if_necessary(double moveit_theta, bool in_initial_fk, const SDL_DPoint& dcamera_origin, std::string& msg);
	void post_rotate_robot_or_joint1();
	void set_goto_near_ticks(bool force);
	void set_goto_second_dcpitch_ticks(bool force);
	void clear_second_dcpitch(const std::string& scene);
	double fail_revert_to_initial_state();
	SDL_DPoint3 calculate_TP_PRP_diff_diff(const SDL_DPoint3& PRP, const SDL_DPoint3& TP);
	bool rotate_joint1_by(double moveit_theta, double& desire_joint1_value);

	struct tclaw_snapshot
	{
		tclaw_snapshot()
		{
			clear();
		}

		void set(int _lower_index, int _upper_index, double _halfgap, double _value, double _dist)
		{
			lower_index = _lower_index;
			upper_index = _upper_index;

			halfgap = _halfgap;
			value = _value;
			dist = _dist;
		}

		void clear()
		{
			lower_index = nposm;
			upper_index = nposm;

			halfgap = 0.0;
			value = 0.0;
			dist = 0.0;
		}

		std::string to_string() const
		{
			char buf[512];
			SDL_snprintf(buf, sizeof(buf), "{indexs: (%i, %i) value: %.5f halfgap: %.5f dist: %.5f}", 
				lower_index, upper_index, value, halfgap, dist);
			return buf;
		}

		int lower_index;
		int upper_index;

		double halfgap;
		double value;
		double dist;
	};
	double find_best_dcpitch_4_grasp(double curr_dcpitch, const SDL_DPoint3& PRP, const SDL_DPoint3& RP_offset, const SDL_DPoint3& PRP_model_center, const SDL_DPoint3& TP, const SDL_DSize3& target_size);
	double find_best_dcpitch_4_press(double curr_dcpitch, const SDL_DPoint3& PRP, const SDL_DPoint3& RP_offset, const SDL_DPoint3& PRP_model_center, const SDL_DPoint3& TP, const SDL_DSize3& target_size);

	double calculate_ik_mid(double desire_z) const;

private:
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	tdcamera_driver& dcamera_driver_;
	tros_instance& ros_instance_;
	tdcamera_slot_impl& dcamera_slot_impl_;
	// deps_0lock_ must be destructed after ~tmoveit_calculator.
	// ~tmoveit_calculator will call stop_operate(..), it require deps_ are valid.
	std::unique_ptr<tdeps_0lock> deps_0lock_;

	tdcintrinsics_C intrinsics_;
	int width_;
	int height_;

	const std::string short_dbg_contours_png_;
	const std::string short_dbg_RP_surf_png_;
	const std::string short_dbg_RP_depth_data_dat_;
	class tdbg_RP_lock
	{
	public:
		tdbg_RP_lock(tmoveit_calculator& calculator, const tdcintrinsics_C& intrinsics, int width, int height)
			: calculator_(calculator)
			, original_is_started_(calculator.is_started())
			, original_intrinsics_(calculator.intrinsics_)
			, original_width_(calculator.width_)
			, original_height_(calculator.height_)
		{
			VALIDATE(intrinsics.valid, null_str);
			if (calculator_.is_started()) {
				calculator.intrinsics_ = intrinsics;
				calculator.width_ = width;
				calculator.height_ = height;

			} else {
				calculator_.did_started(intrinsics, width, height);
			}

			VALIDATE(!calculator_.dbg_RP_, null_str);
			calculator_.dbg_RP_ = true;
		}

		~tdbg_RP_lock()
		{
			if (original_is_started_) {
				calculator_.intrinsics_ = original_intrinsics_;
				calculator_.width_ = original_width_;
				calculator_.height_ = original_height_;

			} else {
				calculator_.did_stopped();
			}
			calculator_.dbg_RP_ = false;
		}

	private:
		tmoveit_calculator& calculator_;

		const bool original_is_started_;
		const tdcintrinsics_C original_intrinsics_;
		const int original_width_;
		const int original_height_;

	};
	bool dbg_RP_;

	int16_t* coor_x_data;
	int coor_x_data_size_;

	int16_t* coor_y_data;
	int coor_y_data_size_;

	threading::mutex task_data_mutex_;
	bool task_finished_;
	rtc::Event task_event_;
	std::unique_ptr<net::tworker> task_thread_;

	toperate operate_;
	std::map<int, texture> state_texs_;
	std::map<int, tscene_desc> scenes_;

	std::vector<SDL_Rect> reference_rects_;


	
	SDL_DPoint3 verbose_last_RP_;
	SDL_DPoint3 verbose_last_PRP_;
	SDL_DPoint3 verbose_last_TP_;
	geometry_msgs::Pose verbose_last_fk_;

	enum {dcpoint_RP, dcpoint_target, dcpoint_count};
	int next_RP_sample_index_;
	int next_targrt_sample_index_;
	SDL_DPoint3 dist_samples_[dcpoint_count][MAX_DIST_SAMPLES];

	SDL_DPoint3 PRP_offset_;
	SDL_DPoint3 PRP_2offset_;
	const double RGB_diff_y_;
	const double ik_bound_;
	const int near_threshold_;
	const int move_robot_threshold_;
	const int second_dcpitch_threshold_;
	const double same_initial_fk_x_thresould_;
	const SDL_DRange no_rotate_robot_range_;
	const SDL_DRange no_rotate_joint1_range_;

	const double joint1_0degree_;
	const bool joint1_axis_negative_;
	double* joint1_position_;
	const aplt::tjoint_model* joint1_model_;
	const aplt::tjoint_model* PRP_joint_model_;

	class tclaw
	{
	public:
		enum {tip_top, tip_center, tip_bottom, tip_count};
		tclaw(const trsp_moveit2& rsp_moveit)
			: rsp_moveit_(rsp_moveit)
		{}

		bool index_pair_from_object_width(double width, tclaw_snapshot& result) const;
		SDL_DPoint3 model_grasp_point(const SDL_DSize3& size, int tip_pos, tclaw_snapshot* snapshot_ptr) const;

	private:
		const trsp_moveit2& rsp_moveit_;
	};
	tclaw claw_;
};

class tdcamera_slot_impl: public tdcamera_slot, public tros_instance::tslot
{
public:
	tdcamera_slot_impl(std::map<aplt::taplt_key, aplt::tapplet>& applets, tdcamera_driver& dcamera_driver, tros_instance& ros_instance, tcamera& camera)
		: applets_(applets)
		, dcamera_driver_(dcamera_driver)
		, ros_instance_(ros_instance)
		, camera_(camera)
		, moveit_calculator_(applets, dcamera_driver_, ros_instance_, *this)
	{}

	virtual void set_set_ik_diff_label(double x_diff, double z_diff) {}

protected:
	bool use_calculator() const { return dcamera_driver_.slot != nullptr && dcamera_driver_.main_curr_task() == dctask_d2c; }
	std::string impl_did_draw_slice(trtc_client::VideoRenderer& vsink, std::vector<SDL_2Point>& new_qrcode_corners, std::vector<SDL_Rect>& reference_rects);

	// 
	// tcamera::tslot
	//
	// void camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override;
	void camera_post_enter_task() override;
	void camera_pre_exit_task() override;
	void camera_work_frame(const surface& surf) override;
	void camera_post_switch_camera() override {}
	// tdcamera_tslot
	void dcamera_did_OnFrame(int task, const tdcframe_C* frames, int count) override {}

	virtual void did_operate_stopped() {}

protected:
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	tdcamera_driver& dcamera_driver_;
	tros_instance& ros_instance_;
	tcamera& camera_;
	tmoveit_calculator moveit_calculator_;

	// std::string qrcode_;
	// std::vector<cv::Point> qrcode_corners_;
	std::vector<SDL_2Point> qrcode_corners_;
};

class tmoveit_aplt_task: public tdcamera_slot_impl
{
public:
	tmoveit_aplt_task(std::map<aplt::taplt_key, aplt::tapplet>& applets, tdcamera_driver& dcamera_driver, tros_instance& ros_instance, tcamera& camera)
		: tdcamera_slot_impl(applets, dcamera_driver, ros_instance, camera)
	{}

	void did_navigation_bh_moveit(bool result, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task, const aplt::taplt_task& aplt_task);
	void stop_navigation_aplt_task(const aplt::taplt_task& aplt_task);

	void slice();

	void set_did_draw_slice_bh(const std::function<void (tmoveit_calculator& calculator, trtc_client::VideoRenderer& vsink, const SDL_Rect& draw_rect, const std::string& msg, const std::vector<SDL_2Point>& new_qrcode_corners, const std::vector<SDL_Rect>&reference_rects)>& fn)
	{
		// VALIDATE(fn != NULL, null_str);
		did_draw_slice_bh_ = fn;
	}

	bool is_set_did_draw_slice_bh() const { return did_draw_slice_bh_ != NULL; }

private:
	void camera_did_draw_slice(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect) override;

	void did_operate_stopped() override;

private:
	std::function<void (tmoveit_calculator& calculator, trtc_client::VideoRenderer& vsink, const SDL_Rect& draw_rect, const std::string& msg, const std::vector<SDL_2Point>& new_qrcode_corners, const std::vector<SDL_Rect>&reference_rects)> did_draw_slice_bh_;
};

#endif

