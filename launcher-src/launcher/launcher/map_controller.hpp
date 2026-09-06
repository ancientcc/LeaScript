#ifndef MAP_CONTROLLER_HPP_INCLUDED
#define MAP_CONTROLLER_HPP_INCLUDED

#include "base_controller.hpp"
#include "mouse_handler_base.hpp"
#include "map_display.hpp"
#include "map_unit_map.hpp"
#include "map.hpp"
#include "game_config.hpp"
#include "gui/dialogs/map_scene.hpp"
#include "ros_instance.hpp"
#include <rose_ros/utils.hpp>
#include "moveit_calculator.hpp"

#include "halo.hpp"

#define MAX_DENOISE_AREAS		253 // 1--253
#define MIN_DENOISE_AREA_INDEX	1
#define MAX_DENOISE_AREA_INDEX	MAX_DENOISE_AREAS

namespace gui2 {
class tfloat_panel;
}

class map_controller : public base_controller, public events::mouse_handler_base, public tros_instance::tslot, public tmsg_sink
{
public:
	static map_controller* sigleton;
	map_controller(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tdrivers& drivers, tros_instance& ros_instance, tdcamera_driver& dcamera_driver, tcamera& camera,
		tros_map& curmap, tbase_driver& base_driver, tmoveit_driver& moveit_driver, const trobot_imu& robot_imu, const config &app_cfg, CVideo& video, 
		std::unique_ptr<tmoveit_aplt_task>& moveit_aplt_task, const std::string& saves_map_dir);
	~map_controller();

	map_display& gui() { return *gui_; }
	const map_display& gui() const { return *gui_; }
	events::mouse_handler_base& get_mouse_handler_base() override { return *this; }
	map_display& get_display() override { return *gui_; }
	const map_display& get_display() const override { return *gui_; }

	map_unit_map& get_units() override { return units_; }
	const map_unit_map& get_units() const override { return units_; }

	bool mapviewer() const { return mapviewer_; }
	void set_cur_mode(const tgui_mode& mode);
	const tgui_mode& cur_mode() const { return cur_mode_; }
	bool can_swtich_to_mode(int desire_mode) const;

	bool set_navigation_rspfile(const std::string& rspfile);
	void did_rspfile_deleted(const std::string& rspfile);
	void did_position_entered();
	void did_position_insert(const std::string& name, double theta);
	void did_position_edit(const std::string& uuid, const std::string& new_name, double new_theta);
	void did_position_erase(const std::string& uuid);
	void click_debug(gui2::tbutton& widget);

	const std::string& saves_map_dir() const { return saves_map_dir_; }
	const std::set<std::string>& files() const { return files_; }
	const tros_map& curmap() const { return curmap_; }
	tmap_position* position_from_uuid(const std::string& uuid);

	tros_instance& get_ros_instance() { return ros_instance_; }
	tty2& get_tty2() { return drivers_.get_tty2(); }

	void did_scan_subscribed(const sensor_msgs::LaserScan& scan_msg, const SDL_2Point& charging) override;
	void did_map_subscribed(const nav_msgs::OccupancyGrid& map) override;
	void did_current_goal_subscribed(const geometry_msgs::PoseStamped& goal) override;
	void did_global_plan_subscribed(const nav_msgs::Path& path) override;
	void did_local_plan_subscribed(const nav_msgs::Path& path) override;
	void did_recovery_status(const move_base_msgs::RecoveryStatus& status) override;
	void did_move_base_result(const move_base_msgs::MoveBaseActionResult& result, const tros_instance::ttask* task) override;

	const cv::Mat& last_display_map() const { return denoising_map_? last_denoise_bgra_: last_src_gray_; }
	const cv::Mat& last_src_gray() const { return last_src_gray_; }
	const SDL_Point& last_map_offset() { return last_map_offset_; }
	const SDL_Point& last_map_origin() { return last_map_origin_; }
	const nav_msgs::MapMetaData& last_map() const { return last_map_; }
	const surface& laser_png_surf() const { return laser_png_surf_; }
	const SDL_Point& laser_png_rotate_point() const { return laser_png_rotate_point_; }
	const surface& current_goal_surf() const { return current_goal_surf_; }
	const SDL_Point& current_goal_png_rotate_point() const { return current_goal_png_rotate_point_; }
	const SDL_Point& current_goal_offset() const { return current_goal_offset_; }

	const cv::Mat& last_laser_mat() const { return last_laser_mat_; }
	const SDL_Point& last_laser_origin() { return last_laser_origin_; }
	const SDL_Point& last_laser_offset() { return last_laser_offset_; }
	int laser_pixels_per_meter() const { return laser_pixels_per_meter_; }
	bool with_laserscan() const { return with_laserscan_; }

	const std::string& exclude_marker_halo() const { return exclude_marker_halo_; }

	// GlobalPlanner's global plan
	const SDL_Point* global_plan() const { return global_plan_; }
	int global_plan_vsize() const { return global_plan_vsize_; }
	// DWAPlannerROS's local plan
	const SDL_Point* local_plan() const { return local_plan_; }
	int local_plan_vsize() const { return local_plan_vsize_; }

	void zoom_src_gray();

	void longpress_position(bool& halt, const tpoint& coordinate, gui2::twindow& window, gui2::ttoggle_panel& row);
	void add_haloes();
	void did_post_scroll(int dx, int dy);
	void set_anchor_mainmap_xy();
	void post_set_zoom();
	void set_status_report(const std::string& msg);

	void insert_map_marker(int type);
	void edit_map_marker(const std::string& uuid);
	void close_map_marker(int msg_id, int mouse_x, int mouse_y, double line_width, double theta, bool cancel_only);
	void cancel_edit_map_marker();

private:
	void app_create_display(int initial_zoom) override;
	void app_post_initialize() override;

	void app_execute_command(int command, const std::string& sparam) override;
	void app_play_slice() override;
	void app_left_mouse_up(const int x, const int y, const bool click) override;

	void add_marker_haloe(const tmap_marker& marker);
	void add_position_haloe(const tmap_position& position);

	void longpress_widget(bool& halt, const tpoint& coordinate, gui2::twindow& window);
	bool did_drag_mouse_motion(const int x, const int y, tpoint& new_mouse, gui2::twindow& window);
	void did_drag_mouse_leave(const int x, const int y, bool up_result);

	bool did_position_drag_mouse_motion(const int x, const int y, tpoint& new_mouse, gui2::twindow& window);
	void did_position_drag_mouse_leave(const int x, const int y, bool up_result);

	enum {track_major, track_minor, track_kidnap};
	void did_draw_cartographer(gui2::ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn, int type);
	void signal_handler_longpress_cartographer(bool& halt, const tpoint& coordinate, int type);
	texture did_create_background_tex(gui2::ttrack& widget, const SDL_Rect& draw_rect, int type);
	void draw_kidnap_track(const SDL_Rect& widget_rect);
	void save_cartographer_pose_msgs() const;
	void save_kidnap_dbg_msgs() const;

	void reload_map(int w, int h);
	void make_sure_map(int width, int height, int simple_per_meter);
	void cancel_goal();
	void click_start();
	void click_save_map();
	void click_reposition_robot(int estimate_reason);
	void click_auto_buildmap();
	void click_cancel_goal();
	void click_switch_slam();
	void click_denoising_map();
	void click_confirm(bool ok);

	void pre_start();
	void reset_movebase_variables();
	void calculate_laser_origin();
	void resize_global_plan(int size);
	bool update_mainmap_from_rspfile(const tros_map& ros_map);
	void save_map_when_buildmap(const nav_msgs::OccupancyGrid& map);
	void save_map_when_position() const;
	SDL_Point meter_2_mainmap(double x, double y) const;
	void parse_ros_global_plan();
	void parse_ros_local_plan();
	void parse_ros_current_goal();
	void send_exploration_detect();
	void set_auto_buildmap_label();

	bool require_show_help() const;
	void set_show_help(bool val);
	bool is_normal_page() const;
	bool is_kidnap_page() const;
	void set_denoising_map(bool val);
	void immediate_draw_major_slam();
	void immediate_draw_slam();

	void did_stop_denoising_map();
	void stop_denoising_map(bool ok);
	SDL_DPoint mouse_xy_2_ros_world_xy(int mouse_x, int mouse_y) const;
	SDL_Point meter_2_mouse_xy(double x, double y) const;

	void calculate_ros_map_4_server(bool verbose);

	// tmsg_sink
	void immediate_kidnap_msg(int msg, int reason) override;

private:
	net::trdpd_manager& rdpd_mgr_;
	tpble2& pble_;
	tprivacy& privacy_;
	tdrivers& drivers_;
	tros_instance& ros_instance_;
	tdcamera_driver& dcamera_driver_;
	tcamera& camera_;
	tros_map& curmap_;
	tbase_driver& base_driver_;
	tmoveit_driver& moveit_driver_;
	const trobot_imu& robot_imu_;
	std::unique_ptr<tmoveit_aplt_task>& moveit_aplt_task_;
	const std::string saves_map_dir_;
	const bool mapviewer_;
	aplt::tdisable_new_klink_task_lock disable_new_aplt_lock_;
	map_unit_map units_;
	tmap map_;
	map_display* gui_;
	gui2::tmap_scene* dlg_;
	gui2::twindow* window_;
	std::set<std::string> files_;
	const uint32_t reserve_rspfile_index_;

	gui2::tfloat_widget* cancel_goal_widget_;
	gui2::tfloat_widget* save_map_widget_;
	gui2::tfloat_widget* switch_slam_widget_;
	gui2::tfloat_widget* auto_buildmap_widget_;
	gui2::tfloat_widget* reposition_robot_widget_;
	gui2::tbutton* start_widget_;
	gui2::ttrack* slam_major_widget_;
	gui2::ttrack* slam_minor_widget_;

	tgui_mode cur_mode_;
	bool auto_buildmap_;
	std::string last_saved_rspfile_; // buildmap relative
	std::string navigation_rspfile_; // navigation relative
	std::map<std::string, tmap_marker> navigation_markers_;

	surface map_pin_surf_;
	SDL_Point position_tl_offset_;
	SDL_Point map_pin_pt_;
	tmap_position* longpressing_position_;
	SDL_FPoint position_downing_at_;

	struct thalo
	{
		thalo(int handle)
			: handle(handle)
		{}
		~thalo()
		{
			halo::remove(handle);
		}
		const int handle;
	};
	std::map<std::string, thalo> marker_halos_;
	std::string exclude_marker_halo_;
	std::map<std::string, thalo> position_halos_;

	uint32_t will_longpress_ticks_;
	tpoint downing_pt_;
	::nav_msgs::MapMetaData last_map_;
	SDL_Point last_map_origin_;
	SDL_Point last_map_offset_;
	move_base_msgs::MoveBaseGoal goal_;
	nav_msgs::Path ros_global_plan_;
	nav_msgs::Path ros_local_plan_;
	tpose2d ros_current_goal_;

	cv::Mat last_src_gray_;
	surface laser_png_surf_;
	SDL_Point laser_png_rotate_point_;
	surface current_goal_surf_;
	SDL_Point current_goal_png_rotate_point_;
	SDL_Point current_goal_offset_;

	// sensor_msgs::LaserScan
	cv::Mat last_laser_mat_;
	SDL_Point last_laser_origin_;
	SDL_Point last_laser_offset_;
	int laser_pixels_per_meter_;
	double last_laser_yaw_;
	tpose2d base_footprint_pose_;
	bool with_laserscan_;
	bool show_help_;
	uint32_t next_kidnap_msg_ticks_;
	int kdinap_track_top_gap_h_;

	uint32_t next_1second_ticks_;

	SDL_Point* global_plan_;
	int global_plan_size_;
	int global_plan_vsize_;

	SDL_Point local_plan_[MAX_LOCAL_PLAN];
	int local_plan_vsize_;
	SDL_Point last_local_plan_start_;

	bool map_saving_;
	std::string last_im948_serial_;
	// tros_map for map_server. MUST not changed during map_server
	// If exist markers, needs to call image::get_image(...), but don't call it on a non-main thread.
	// nav_msgs::OccupancyGrid map_4_map_server_;

	uint8_t* denoise_map_data_;
	int denoise_map_data_size_;
	bool denoising_map_;
	cv::Mat last_denoise_bgra_;
	bool areas_denoised_[MAX_DENOISE_AREAS];
	enum {color_denoise_reserve, color_denoise_erase, color_count};
	SDL_Color colors_[color_count];

	std::vector<tgui_mode> gui_modes_;

	std::unique_ptr<net::tworker> node1_;
	std::unique_ptr<net::tworker> node2_;

	std::unique_ptr<gui2::tfloat_panel> marker_placer_;

	struct tdraw_dcamera_points_settings {
		int ranges;
		double meter_per_range;
	};
	tdraw_dcamera_points_settings draw_dcamera_points_settings_;

	const bool slam_track_4_dcamera_;

	uint32_t rosbag_play_stop_ticks_; // only for debug when rosbag play.
};

#endif
