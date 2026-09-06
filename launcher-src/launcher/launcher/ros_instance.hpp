#ifndef ROS_CTRL_HPP_INCLUDED
#define ROS_CTRL_HPP_INCLUDED

#include <sensor_msgs/LaserScan.h>
#include <nav_msgs/GetMap.h>
#include <nav_msgs/Path.h>
#include <move_base_msgs/MoveBaseGoal.h>
#include <move_base_msgs/RecoveryStatus.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <geometry_msgs/TransformStamped.h>
#include "thread.hpp"
#include "aplt_common.hpp"
#include "game_config.hpp"
#include "drivers.hpp"
#include "gui/dialogs/dialog.hpp"
#include "rose_ros/node_wrapper.hpp"
#include "base_driver.hpp"
#include "moveit_driver.hpp"
#include "laser_driver.hpp"
#include "dcamera_driver.hpp"
#include "speech_driver.hpp"
// #include "iot_driver.hpp"
#include <ros/callback_queue.h>

#include <moveit/robot_state/robot_state.h>

namespace ros {
class ttf_calculator;
};

// Reuire fix in next version
#define MOVEIT_PRODUCT	"tank_arm"


int test__talker(bool& exit);
int test__listener(bool& exit);
int test__transform(bool& exit);
SDL_DPoint3 ceres_curve_fitting(const std::vector<SDL_DPoint>& xy_data);

int tf__static_transform_publisher(bool& exit, const geometry_msgs::TransformStamped& _msg);
int rikirobot__riki_base_node(bool& exit);
int robot_localization__ekf_localization_node(bool& exit);
int move_base__move_base_node(bool& exit, bool* constructed);
int cartographer_ros__cartographer_node(bool& exit);
int cartographer_ros__cartographer_occupancy_grid_node(bool& exit);
int rrt_exploration__global_rrt_detector(bool& exit);
// navigation
int map_server__map_server(bool& exit, const nav_msgs::OccupancyGrid& curr_map);
int amcl__amcl(bool& exit);
int topic_tools__throttle(bool& exit);

int map_server__map_saver(bool& exit);
void map_server_map_saver(const nav_msgs::OccupancyGrid& map, const std::string& mapname);

int moveit_ros_move_group__move_group(bool& exit);
int robot_state_publisher__robot_state_publisher(bool& exit);
int joint_state_publisher__joint_state_publisher(bool& exit, void* void_ptr_RobotState, trose_event& e);
int rosserial_cpp__node(bool& exit, const std::string& serial_path, int baudrate);

// rosbag
int rosbag__record(bool& exit, const rosbag::trecord_input& input, rosbag::trecord_result& result);
int rosbag__play(bool& exit, const rosbag::tplay_input& input, rosbag::tplay_result& result);

enum {cartographer_start, cartographer_apply_immediately, cartographer_ok_esitmated, cartographer_count};

class tmsg_sink
{
public:
	virtual void immediate_kidnap_msg(int msg, int reason) = 0;
};

class tinstance_slot;

class tros_instance
{
public:
	static tros_instance* singleton;
	class ttask;

	class tslot
	{
	public:
		enum {type_normal, type_base_node, type_task, type_count};
		tslot(int _type = type_normal)
			: type(_type)
			, registered(false)
			, map_received(false)
			, goal_received(false)
		{
			VALIDATE(type >= 0 && type < type_count, null_str);
		}

		virtual ~tslot() {}

		virtual void did_scan_subscribed(const sensor_msgs::LaserScan& scan_msg, const SDL_2Point& charging) {}
		virtual void did_map_subscribed(const nav_msgs::OccupancyGrid& map) {}
		virtual void did_current_goal_subscribed(const geometry_msgs::PoseStamped& goal) {}
		virtual void did_global_plan_subscribed(const nav_msgs::Path& path) {}
		virtual void did_local_plan_subscribed(const nav_msgs::Path& path) {}
		virtual void did_recovery_status(const move_base_msgs::RecoveryStatus& status) {}
		virtual void did_move_base_result(const move_base_msgs::MoveBaseActionResult& result, const ttask* task) {}

	public:
		const int type;
		bool registered;

		bool map_received;
		bool goal_received;
	};

	class ttask : public tslot
	{
	public:
		ttask(const aplt::tapplet& _aplt, const std::string& _rspfile, const std::string& _object_id, int _bh, const fn_navigation_bh& _luafunc);
		~ttask()
		{
			int ii = 0;
		}

	public:
		const aplt::tapplet& aplt;
		const std::string rspfile; // full-file name
		std::string object_id;
		double x;
		double y;
		double theta;

		int bh;
		const fn_navigation_bh luafunc;
	};

	class tmoveit_model_lock
	{
	public:
		tmoveit_model_lock(tdrivers& drivers, tros_instance& ros_instance)
			: drivers_(drivers)
			, ros_instance_(ros_instance)
		{
			library& lib = drivers_.libs[apltsotype_moveit];
			if (lib.get() != nullptr) {
				original_res_pth_ = lib.get()->res_path;
			}
		}

		~tmoveit_model_lock()
		{
			int type = apltsotype_moveit;
			std::string new_res_path;
			library& lib = drivers_.libs[type];
			if (lib.get() != nullptr) {
				new_res_path = lib.get()->res_path;
			}

			if (new_res_path != original_res_pth_ && ros_instance_.initialized_) {
				ros_instance_.load_moveit_model(new_res_path, MOVEIT_PRODUCT, true);
				if (lib.get() != nullptr && !ros_instance_.is_moveit_model_valid()) {
					drivers_.clear_by_type(type);
				}
			}
		}

	private:
		tdrivers& drivers_;
		tros_instance& ros_instance_;
		std::string original_res_pth_;
	};

	tros_instance(tdrivers& drivers, tros_map& curmap, tbase_driver& base_driver, tmoveit_driver& moveit_driver, 
		tlaser_driver& laser_driver, tdcamera_driver& dcamera_driver, tspeech_driver& speech_driver, trobot_imu& robot_imu, tinstance_slot& instance_slot);
	~tros_instance();
	posix_noncopyable(tros_instance);

	void clear_tfl();

	bool initialized() const { return initialized_; }
	void slice();

	void start_navigation_node(int mode, int rosbag, const tdrivers::tvars& vars, const std::string& rspfile);
	void stop_navigation_node();

	void start_moveit_node(bool for_navigation);
	void stop_moveit_node();

	void stop_dwa_local_planner_node();

	void subscribe_laserscan_topics();
	void shutdown_laserscan_topics();

	bool started() const { return started_; }
	bool laserscan_started() const { return laserscan_started_; }
	int mode() const { return mode_; }
	bool navigation_node_started() const { return mode_ != nposm; }
	bool moveit_node_started() const { return moveit_node_started_; }
	bool move_base_constructed() const { return move_base_constructed_; }

	uint32_t navigation_start_ticks() const { return navigation_start_ticks_; }

	void load_moveit_model(const std::string& moveit_path, const std::string& name, bool force_load);
	void clear_moveit_model();
	bool is_moveit_model_valid() const;
	void* RobotState_of_moveit_model();
	const trsp_moveit2& rsp_of_moveit_model(bool must_valid = true) const;
	const aplt::trobot_model& aplt_model_of_moveit_model() const;
	aplt::tvariable_positions& aplt_positions_of_moveit_model() const;
	ros::ttf_calculator& tf_calculator_of_moveit_model();

	geometry_msgs::Pose calculate_group_fk_tf(const std::string& group_name, const std::vector<double>* angles_ptr = nullptr);
	double* joint_position_from_name(const std::string& joint_name) const;
	const aplt::tjoint_model& joint_model_from_name(const std::string& joint_name) const;

	ros::CallbackQueue& get_cbqueue();
	bool get_laser_tf(geometry_msgs::TransformStamped& transform);
	bool get_laser_2_base_footprint_tf(geometry_msgs::TransformStamped& transform);
	bool get_source_2_target_tf(const std::string& source_frame, const std::string& target_frame, geometry_msgs::TransformStamped& transform);
	void do_riki_action(int op, double* twist_ptr);
	void public_vel(double linear_x, double linear_y, double angular_z);
	void do_JointState_action(const std::map<std::string, double>& values);
	void do_ik();
	void do_pickplace();
	void do_soymilk();
	void do_moveit_temporary();
	void do_joint_name_target(const std::string& group_name, const std::string& status_name);
	void do_joint_value_target(const std::string& group_name, const std::vector<double>& joint_values);

	void do_light_joint_value_target(const std::string& group_name, const std::vector<double>& joint_values);
	void do_light_group_values_target(int state);
	void do_light_single_joint_target(const std::string& name, double value);
	std::vector<double> do_light_joint_pose_target(const std::string& group_name, const tpose3d& target_pose, const tpose3d& bounds, bool use_as_seed, bool verbose = true);
	std::vector<double> get_group_joint_values(const std::string& group_name) const;
	double get_joint1_0degree() const;

	bool do_send_goal(const move_base_msgs::MoveBaseGoal& _goal);
	void do_cancel_goal();
	void makesure_cancel_goal(bool with_erase_task);
	tros_instance::ttask* moveto_guid(const aplt::tapplet& aplt, const std::string& position_uuid, const fn_navigation_bh& luafunc);
	bool moveto_guid_bh(tros_instance::ttask* task_ptr);
	void report_navigation_error(bool caller_is_fg_aplt, const std::string& msg);

	void restart_cartographer_node(int reason);
	void wait_cartographer_node_ready();
	void start_base_or_moveit_serial(bool base, bool buildmap);
	bool check_navigation_env(const tdrivers::tvars& vars, const aplt::tapplet* aplt, bool caller_is_fg_aplt_or_app);
	bool check_charge_env(bool caller_is_fg_aplt_or_app, aplt::ttask_pair* task_pair_result, int* width_result);

	bool light_moveit() const { return light_moveit_; }
	// void start_mapping();
	// void stop_mapping();
	// bool is_mapping() const { return node_wrapper_ != nullptr; }
	// void mapping_slice(bool global_planner, const nav_msgs::MapMetaData& info, const int8_t* data, const tpose2d& robot_pose);
	// void mapping_sendGoal(const move_base_msgs::MoveBaseGoal& goal);
	// void mapping_cancelGoal();
	bool exploration_detect(const tpose2d& robot_pose, geometry_msgs::Point& unk);

	void register_slot(tslot& slot);
	void deregister_slot(tslot& slot);

	void set_msg_sink(tmsg_sink* sink);
	void send_immediate_kidnap_msg(int msg, int reason);

	tbase_driver& base_driver() { return base_driver_; }
	tmoveit_driver& moveit_driver() { return moveit_driver_; }
	tdrivers& drivers() { return drivers_; }

	bool use_external_laser() const;

	void did_scan_subscribed(const sensor_msgs::LaserScan& scan_msg);
	void did_map_subscribed(const nav_msgs::OccupancyGrid& map);
	// movebase relative
	void did_current_goal_subscribed(const geometry_msgs::PoseStamped& goal);
	void did_global_plan_subscribed(const nav_msgs::Path& path);
	void did_local_plan_subscribed(const nav_msgs::Path& path);
	void did_recovery_status(const move_base_msgs::RecoveryStatus& status);
	void did_move_base_result(const move_base_msgs::MoveBaseActionResult& result);

	bool has_task() const { return task_.get() != nullptr; }
	ttask* task() const { return task_.get(); }
	void erase_task();
	bool in_map_viewer() const;

	bool goaling() const { return goaling_; }
	const trpy& get_imu_rpy() const;
	double get_imu_yaw(bool* valid_ptr) const;
	double get_stable_imu(int rpy, bool verbose, bool* fail_ptr = nullptr) const;
	std::string get_imu_desc() const;
	void refresh_map_data(const nav_msgs::OccupancyGrid& map, bool from_map_topic);
	// const ::nav_msgs::MapMetaData& last_map() const { return last_map_; }
	const int8_t* last_map_data() const { return last_map_data_; }
	int last_map_data_size() const { return last_map_data_size_; }

	void calculate_ros_map_4_server(const std::string& rspfile, bool verbose);

	void do_publish_scan(const sensor_msgs::LaserScan& scan_msg, const sensor_msgs::LaserScan& laser_scan_msg);
	void resize_dcamera_points(int size);
	void set_dcamera_points(const SDL_FPoint3* points, int size, const tdepth_sector_result& result);
	void integrate_dcamera_LaserScan(const sensor_msgs::LaserScan& msg, sensor_msgs::LaserScan& result);
	void draw_dcamera_points(gui2::ttrack& track, const SDL_Rect& bg_rect, int ranges, double meter_per_range);

	const rosbag::trecord_input& rosbag_record_input() const { return rosbag_record_input_; }
	const rosbag::trecord_result& rosbag_record_result() const { return rosbag_record_result_; }

	const rosbag::tplay_input& rosbag_play_input() const { return rosbag_play_input_; }
	rosbag::tplay_result& rosbag_play_result() { return rosbag_play_result_; }

private:
	void initialize();
	void start();
	void stop();

	void subscribe_movebase_topics();
	void shutdown_movebase_topics();
	void shutdown_map_topic();

	void generate_kestimate_map_data(const ::nav_msgs::MapMetaData& info, const int8_t* map_data);

	void kidnap_slice(const sensor_msgs::LaserScan& scan_msg);
	bool moveto_function(gui2::tprogress_& progress, const tros_instance::ttask& task, const tdrivers::tvars& vars);

private:
	tdrivers& drivers_;
	tros_map& curmap_;
	tbase_driver& base_driver_;
	tmoveit_driver& moveit_driver_;
	tlaser_driver& laser_driver_;
	tdcamera_driver& dcamera_driver_;
	tspeech_driver& speech_driver_;
	trobot_imu& robot_imu_;
	tinstance_slot& instance_slot_;
	tmsg_sink* msg_sink_;
	const std::string node_name_;
	struct t4val {
		int x;
		int y;
		int z;
		int th;
	};
	std::map<int, t4val> moveBindings_;

	bool initialized_;
	bool started_;
	bool laserscan_started_;
	int mode_;
	int rosbag_;
	bool light_moveit_;
	bool moveit_node_started_;
	bool move_base_constructed_;
	nav_msgs::OccupancyGrid navigation_map_;
	geometry_msgs::PoseStamped navigation_goal_;

	std::set<tslot*> slots_;
	std::unique_ptr<ttask> task_;
	// true: sendGoal is done, and hasn't receive MoveBaseActionResult.
	// false: other.
	bool goaling_;
	bool require_reposition_;

	uint32_t navigation_start_ticks_;

	// std::unique_ptr<net::tworker> imu_filter_madgwick_;
	std::unique_ptr<net::tworker> base_footprint_2_imu_link_;
	// std::unique_ptr<net::tworker> riki_base_node_;
	std::unique_ptr<net::tworker> base_footprint_2_laser_;
	std::unique_ptr<net::tworker> base_footprint_2_camera_;
	std::unique_ptr<net::tworker> base_footprint_2_base_link_;
	// std::unique_ptr<net::tworker> ekf_localization_node_;
	std::unique_ptr<net::tworker> move_base_node_;
	std::unique_ptr<net::tworker> cartographer_occupancy_grid_node_;
	std::unique_ptr<net::tworker> cartographer_node_;
	std::unique_ptr<net::tworker> map_2_laser_;
	std::unique_ptr<net::tworker> global_rrt_detector_;

	// navigation
	std::unique_ptr<net::tworker> map_server_;
	// std::unique_ptr<net::tworker> amcl_;
	// std::unique_ptr<net::tworker> throttle_;

	std::unique_ptr<net::tworker> move_group_;
	std::unique_ptr<net::tworker> robot_state_publisher_;
	std::unique_ptr<net::tworker> joint_state_publisher_;

	// rosbag
	std::unique_ptr<net::tworker> rosbag_record_;
	std::unique_ptr<net::tworker> rosbag_play_;

	ros::tnode_wrapper node_wrapper_;
	// The y direction has been flipped.
	::nav_msgs::MapMetaData last_map_;
	int8_t* last_map_data_;
	int last_map_data_size_;

	struct tint8data {
		int8_t* ptr;
		int len;
	};
	tuint8data_C kestimate_map_data_;

	// tros_map for map_server. MUST not changed during map_server
	// If exist markers, needs to call image::get_image(...), but don't call it on a non-main thread.
	nav_msgs::OccupancyGrid map_4_map_server_;

	threading::mutex dcamera_points_mutex_;
	SDL_FPoint3* dcamera_points_;
	int dcamera_point_size_;
	int dcamera_point_vsize_;
	tdepth_sector_result depth_sector_result_;
	SDL_FPoint dcamera_2_laser_xy_;

	rosbag::trecord_input rosbag_record_input_;
	rosbag::trecord_result rosbag_record_result_;

	rosbag::tplay_input rosbag_play_input_;
	rosbag::tplay_result rosbag_play_result_;
};

class tinstance_slot
{
public:
	virtual void start_map_controller(tros_instance::ttask* task_ptr) = 0;
	virtual void did_scan_subscribed(const sensor_msgs::LaserScan& msg, SDL_2Point& charging) = 0;

	virtual bool show_gui_center(const tstart_aiagent* start_aiagent) = 0;

	virtual void nullptr_slot_for_aplt_drivers(const aplt::tapplet& aplt) = 0;
};

class tros_base_node: public tros_base_node_core, public tros_instance::tslot
{
public:
	tros_base_node(tros_instance& ros_instance);
	~tros_base_node();

private:
	tros_instance& ros_instance_;
};

#endif

