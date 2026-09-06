#define GETTEXT_DOMAIN "launcher-lib"

#include <sstream>
#include <ros/ros.h>
#include <ros/callback_queue.h>
#include <geometry_msgs/Twist.h>
#include <nav_msgs/GetMap.h>
#include <nav_msgs/Path.h>
#include <move_base_msgs/RecoveryStatus.h>
#include <sensor_msgs/LaserScan.h>
#include <tf2_ros/transform_listener.h>
#include <riki_msgs/Battery.h>
#include <sensor_msgs/JointState.h>

#include <SDL_thread.h>
#include <SDL_log.h>
#include <SDL_timer.h>
#include "wml_exception.hpp"

#include <actionlib/client/simple_action_client.h>
#include <move_base_msgs/MoveBaseAction.h>
#include "game_config.hpp"
#include "ros_instance.hpp"
#include "gettext.hpp"
#include "font.hpp"

#include <moveit/robot_model_loader/robot_model_loader.h>
#include <moveit/robot_state/robot_state.h>

#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2/utils.h>
#include <costmap_2d/cost_values.h>
#include <angles/angles.h>
#include <rose_ros/kidnap.hpp>
#include <rose_ros/moveit_light.hpp>

#include "rose_ros/utils.hpp"

#include "base_instance.hpp"
// #include <kdl/tree.hpp>
// #include <kdl_parser/kdl_parser.hpp>

#include <future>
#include <SDL_image.h>

namespace rose_ros {

static std::unique_ptr<ros::NodeHandle> nh;
static std::unique_ptr<ros::CallbackQueue> cbqueue;
static ros::Publisher pub_twist;
static ros::Publisher pub_scan;
static ros::Publisher pub_laser_scan;
// static ros::Subscriber sub_battery;
// static ros::Subscriber sub_Twist;
// static ros::Subscriber sub_JointState;

static ros::Subscriber sub_map;
static ros::Subscriber sub_laser_scan;
// now, tf_ and tfl_ use for navigation only.
static std::unique_ptr<tf2_ros::Buffer> tf_;
static std::unique_ptr<tf2_ros::TransformListener> tfl_;

static ros::Subscriber sub_current_goal;
static ros::Subscriber sub_global_plan;
static ros::Subscriber sub_local_plan;
static ros::Subscriber sub_recovery_status;
static ros::Subscriber sub_move_base_result;
typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;
static std::unique_ptr<MoveBaseClient> client;

static ros::Publisher pub_JointState;

}

#pragma pack(1)
struct tlaser_msg_header {
	uint32_t fourcc;
	double center_x;
	double center_y;
	double imu_yaw;
	int score;
	int points;
	int unknowns;
	double pose2d_x;
	double pose2d_y;
	double pose2d_yaw;
	int msg_len;
};
#pragma pack()

void save_laser_scan(const sensor_msgs::LaserScan& scan_msg, const tpose2d& center_pose2d, const tkidnap::tresult4_C& result4, const tpose2d& pose2d, const std::string& filename)
{
	VALIDATE(!filename.empty(), null_str);
	SDL_Log("save_laser_scan(filename:%s)", filename.c_str());

	ros::SerializedMessage serialized = ros::serialization::serializeMessage<sensor_msgs::LaserScan>(scan_msg);
	int msg_len = serialized.num_bytes - (serialized.message_start - serialized.buf.get());
	const uint8_t* serialized_msg_data = serialized.message_start;

	tfile file(game_config::preferences_dir + "/" + filename, GENERIC_WRITE, CREATE_ALWAYS);
	if (!file.valid()) {
		return;
	}
	tlaser_msg_header header;
	header.fourcc = SDL_FOURCC('L', 'A', 'S', 0x1);
	header.center_x = center_pose2d.x;
	header.center_y = center_pose2d.y;
	header.imu_yaw = center_pose2d.yaw;
	header.score = result4.score;
	header.points = result4.points;
	header.unknowns = result4.unknowns;
	header.pose2d_x = pose2d.x;
	header.pose2d_y = pose2d.y;
	header.pose2d_yaw = pose2d.yaw;
	header.msg_len = msg_len;
	posix_fwrite(file.fp, &header, sizeof(tlaser_msg_header));
	posix_fwrite(file.fp, serialized_msg_data, msg_len);
}

bool load_laser_scan_from_file(const std::string& file_path, sensor_msgs::LaserScan& scan_msg, tlaser_msg_header& header)
{
	scan_msg.header.frame_id.clear();

	tfile file(file_path, GENERIC_READ, OPEN_EXISTING);
	if (!file.valid()) {
		return false;
	}
	int fsize = posix_fsize(file.fp);
	if (fsize < sizeof(tlaser_msg_header)) {
		return false;
	}

	posix_fseek(file.fp, 0);
	posix_fread(file.fp, &header, sizeof(tlaser_msg_header));

	if (fsize != sizeof(tlaser_msg_header) + header.msg_len) {
		return false;
	}
	
	uint8_t* p = new uint8_t[header.msg_len];
	posix_fread(file.fp, p, header.msg_len);
	boost::shared_array<uint8_t> buf(p);
	ros::SerializedMessage serialized(buf, header.msg_len);

	ros::serialization::deserializeMessage<sensor_msgs::LaserScan>(serialized, scan_msg);
	if (scan_msg.header.frame_id != "laser") {
		return false;
	}
	return true;
}

// tmoveit_model use RobotMode/RobotState, these use boost::bind, and cann't use in *c/*.cpp that use std::bind.
class tmoveit_model
{
public:
    bool valid() const { return model.get() != nullptr && state.get() != nullptr && rsp_moveit.valid(); }

	void clear()
	{
		state.reset();
		model.reset();
		rsp_moveit.clear();
		aplt_model.clear();
		tf_calculator.reset();
	}

public:
    // moveit::core::RobotModelPtr model;
	moveit::core::RobotModelConstPtr model;
    std::unique_ptr<robot_state::RobotState> state;
    trsp_moveit2 rsp_moveit;

	aplt::trobot_model aplt_model;
	aplt::tvariable_positions aplt_positions;
	std::unique_ptr<ros::ttf_calculator> tf_calculator;
};

static tmoveit_model moveit_model;

void RobotModel_to_aplt_model(const moveit::core::RobotModel& model, aplt::trobot_model& result)
{
	// change moveit::core::RobotModel to aplt::trobot_model
	result.clear();

	const urdf::ModelInterfaceSharedPtr& urdf = model.getURDF();

	// RobotModel's all joint
	const std::vector<const moveit::core::JointModel*>& JointModels = model.getJointModels();
	for (std::vector<const moveit::core::JointModel*>::const_iterator it = JointModels.begin(); it != JointModels.end(); ++ it) {
		const moveit::core::JointModel& JointModel = **it;
		result.joint_model_vector_.emplace_back(new aplt::tjoint_model(JointModel.getName(), JointModel.getType(), 
			0, 0));

		aplt::tjoint_model* joint = result.joint_model_vector_.back();

		urdf::JointConstSharedPtr urdf_joint = urdf->getJoint(joint->name_);
		// urdf_joint maybe is nullptr, for example: name_(ASSUMED_FIXED_ROOT_JOINT) type_(FIXED)
		if (urdf_joint.get() != nullptr) {
			// <origin xyz="-0.014795 -0.00011444 0.096773" rpy="0.79764 0 1.5708" />
			joint->origin_ = urdf_joint->parent_to_joint_origin_transform;
		
			// <axis xyz="0 0 -1" />
			joint->axis_ = urdf_joint->axis;

			// double roll, pitch, yaw;
			// joint->origin_.rotation.getRPY(roll, pitch, yaw);
			// SDL_Log("name=%s xyz=(%.5f, %.5f, %.5f) rpy=(%.5f, %.5f, %.5f) axis=(%.3f, %.3f, %.3f)", 
			//	joint->name_.c_str(), joint->origin_.position.x, joint->origin_.position.y, joint->origin_.position.z,
			//	roll, pitch, yaw, joint->axis_.x, joint->axis_.y, joint->axis_.z);
		}

		if (joint->type_ == moveit::core::JointModel::REVOLUTE) {
			const std::vector<moveit::core::VariableBounds>& bounds = JointModel.getVariableBounds();
			if (!bounds.empty()) {
				const moveit::core::VariableBounds& bound = bounds[0];
				joint->min_position_ = bound.min_position_;
				joint->max_position_ = bound.max_position_;
			}
		}
		result.joint_model_map_.insert(std::make_pair(joint->name_, joint));
	}
	VALIDATE(result.joint_model_vector_.size() == result.joint_model_map_.size(), null_str);
	
	// RobotModel's all variable
	const std::vector<std::string>& VariableNames = model.getVariableNames();
	for (std::vector<std::string>::const_iterator it = VariableNames.begin(); it != VariableNames.end(); ++ it) {
		const std::string& VariableName = *it;
		result.joint_variables_index_map_.insert(std::make_pair(VariableName, result.variable_names_.size()));
		result.variable_names_.push_back(VariableName);
	}
	VALIDATE(result.variable_names_.size() == result.joint_variables_index_map_.size(), null_str);

	// valuate tjoint_mode::variable_index_
	for (std::vector<aplt::tjoint_model*>::const_iterator it = result.joint_model_vector_.begin(); it != result.joint_model_vector_.end(); ++ it) {
		aplt::tjoint_model* joint = *it;
		if (result.joint_variables_index_map_.count(joint->name_) != 0) {
			joint->variable_index_ = result.joint_variables_index_map_.find(joint->name_)->second;
		}
	}
	
	int sum_group_variables = 0;
	const std::vector<const moveit::core::JointModelGroup*>& JointModelGroups = model.getJointModelGroups();
	for (std::vector<const moveit::core::JointModelGroup*>::const_iterator it = JointModelGroups.begin(); it != JointModelGroups.end(); ++ it) {
		const moveit::core::JointModelGroup& JointModelGroup = **it;
		std::pair<std::map<std::string, aplt::tjoint_group_model*>::iterator, bool> ins = result.joint_model_group_map_.insert(std::make_pair(JointModelGroup.getName(), new aplt::tjoint_group_model(JointModelGroup.getName())));
		VALIDATE(ins.second, null_str);
		aplt::tjoint_group_model* group = ins.first->second;

		// both joint_model_map_ and joint_model_name_vector_ is all joint, inclue FIXED joint.
		const std::vector<const moveit::core::JointModel*>& JointModels = JointModelGroup.getJointModels();
		for (std::vector<const moveit::core::JointModel*>::const_iterator it = JointModels.begin(); it != JointModels.end(); ++ it) {
			const moveit::core::JointModel* JointModel = *it;
			const std::string& name = JointModel->getName();
			VALIDATE(result.joint_model_map_.count(name) != 0, null_str);

			group->joint_model_vector_.push_back(result.joint_model_map_.find(name)->second);
			group->joint_model_name_vector_.push_back(name);
		}

		// variable_names_ and variable_index_list_ are variable joint only.
		const std::vector<std::string>& VariableNames = JointModelGroup.getVariableNames();
		const std::vector<int>& VariableIndexList = JointModelGroup.getVariableIndexList();
		const int VariableCount = JointModelGroup.getVariableCount();
		VALIDATE(VariableNames.size() == VariableCount && VariableIndexList.size() == VariableCount, null_str); 
		for (int at = 0; at < VariableCount; at ++) {
			group->variable_names_.push_back(VariableNames[at]);
			group->variable_index_list_.push_back(VariableIndexList[at]);
			group->variable_name_set_.insert(VariableNames[at]);
		}

		result.joint_model_groups_.push_back(group);
		sum_group_variables += VariableCount;
	}
	VALIDATE(result.joint_model_group_map_.size() == result.joint_model_groups_.size(), null_str);
	// *.urdf maybe defined variable-joint that no group use it.
	// Is there more than one group using the same variable-joint? If it is possible, remark the following VALIDATE().
	// VALIDATE((int)result.variable_names_.size() >= sum_group_variables, null_str);
}

void RobotState_to_aplt_positions(robot_state::RobotState& state, aplt::tvariable_positions& result)
{
	result.positions = state.getVariablePositions();
	result.count = state.getVariableCount();
}

tros_instance::ttask::ttask(const aplt::tapplet& _aplt, const std::string& _rspfile, const std::string& _object_id, int _bh, const fn_navigation_bh& _luafunc)
	: tslot(type_task)
	, aplt(_aplt)
	, rspfile(_rspfile)
	, object_id(_object_id)
	, x(0)
	, y(0)
	, theta(0)
	, bh(_bh)
	, luafunc(_luafunc)
{
	VALIDATE(SDL_IsFile(rspfile.c_str()), null_str);
	// VALIDATE(bh >= 0 && bh < navigation_bh_count, null_str);
	VALIDATE(bh == navigation_bh_luafunc, null_str);
	if (bh == navigation_bh_luafunc) {
		VALIDATE(luafunc != NULL, null_str);
	} else {
		VALIDATE(luafunc == NULL, null_str);
	}
}

tros_instance* tros_instance::singleton = nullptr;

tros_instance::tros_instance(tdrivers& drivers, tros_map& curmap, tbase_driver& base_driver, 
	tmoveit_driver& moveit_driver, tlaser_driver& laser_driver, tdcamera_driver& dcamera_driver, tspeech_driver& speech_driver, trobot_imu& robot_imu, tinstance_slot& instance_slot)
	: drivers_(drivers)
	, curmap_(curmap)
	, base_driver_(base_driver)
	, moveit_driver_(moveit_driver)
	, laser_driver_(laser_driver)
	, dcamera_driver_(dcamera_driver)
	, speech_driver_(speech_driver)
	, robot_imu_(robot_imu)
	, instance_slot_(instance_slot)
	, msg_sink_(nullptr)
	, node_name_("move_group") // launcher
	, initialized_(false)
	, started_(false)
	, laserscan_started_(false)
	, mode_(nposm)
	, rosbag_(nposm)
	// There is no official product with a moveit yet, 
	// in order to speed up the execution, disable it off first.
	// If correct, disable_moveit_ should depend on whether a robotic arm is installed on the hardware.
	, light_moveit_(true)
	, moveit_node_started_(false)
	, move_base_constructed_(false)
	, goaling_(false)
	, require_reposition_(false)
	, navigation_start_ticks_(0)
	, last_map_data_(nullptr)
	, last_map_data_size_(0)
	, kestimate_map_data_(tuint8data_C{nullptr, 0})
	, dcamera_points_(nullptr)
	, dcamera_point_size_(0)
	, dcamera_point_vsize_(0)
	, dcamera_2_laser_xy_(SDL_FPoint{float_nposm, float_nposm})
	, depth_sector_result_(float_nposm, float_nposm, float_nposm, float_nposm)
	// as far, game_config::rosbag_filename is null_str, and isnt't ready. 
	, rosbag_record_input_(null_str, 40 * 60, std::vector<std::string>())
	, rosbag_play_input_(null_str, false)
{
	singleton = this;
	navigation_map_.info.width = 0;
	navigation_map_.info.height = 0;
	navigation_goal_.header.frame_id.clear();

	moveBindings_.insert(std::make_pair(teleop_forward, t4val{1, 0, 0, 0}));
	moveBindings_.insert(std::make_pair(teleop_forwardright, t4val{1, 0, 0, -1}));
	moveBindings_.insert(std::make_pair(teleop_forwardleft, t4val{1, 0, 0, 1}));

	moveBindings_.insert(std::make_pair(teleop_backward, t4val{-1, 0, 0, 0}));
	moveBindings_.insert(std::make_pair(teleop_backwardright, t4val{-1, 0, 0, 1}));
	moveBindings_.insert(std::make_pair(teleop_backwardleft, t4val{-1, 0, 0, -1}));

	VALIDATE(game_config::rosbag_filename.empty(), null_str);
	// game_config::rosbag_filename = game_config::preferences_dir + "/test.bag";
	game_config::rosbag_filename = game_config::preferences_dir + "/rosbag-scan-20241018_101306.bag";
	rosbag_record_input_.filename = game_config::rosbag_filename;
	rosbag_play_input_.filename = game_config::rosbag_filename;

	std::vector<std::string>& topics = rosbag_record_input_.topics;
    topics.push_back("/scan");
    // topics.push_back("/tf_static");
    // topics.push_back("/tf");;
}

tros_instance::~tros_instance()
{
	VALIDATE(slots_.empty(), null_str);
	VALIDATE(!started_, null_str);
	VALIDATE(task_.get() == nullptr, null_str);

	if (last_map_data_ != nullptr) {
        VALIDATE(last_map_data_size_ > 0, null_str);
        free(last_map_data_);
    }

	if (kestimate_map_data_.ptr != nullptr) {
		VALIDATE(kestimate_map_data_.len > 0, null_str);
		free(kestimate_map_data_.ptr);
	}

	if (dcamera_points_ != nullptr) {
		VALIDATE(dcamera_point_size_ > 0, null_str);
		free(dcamera_points_);
	}

	singleton = nullptr;
}

void tros_instance::initialize()
{
	VALIDATE(!initialized_, null_str);
	// VALIDATE(!ros::isStarted(), null_str);
	VALIDATE(rose_ros::nh.get() == nullptr, null_str);

	int argc = 0;
	ros::init(argc, nullptr, node_name_);

	rose_ros::nh.reset(new ros::NodeHandle);
	rose_ros::cbqueue.reset(new ros::CallbackQueue);
	rose_ros::nh->setCallbackQueue(rose_ros::cbqueue.get());

	rose_ros::pub_twist = rose_ros::nh->advertise<geometry_msgs::Twist>("cmd_vel", 1000); // {dbg_publish} if 10, maybe has bug.
	rose_ros::pub_scan = rose_ros::nh->advertise<sensor_msgs::LaserScan>("scan", 1);
	rose_ros::pub_laser_scan = rose_ros::nh->advertise<sensor_msgs::LaserScan>("laser_scan", 1);
	// const std::string js_topic = ros::nocopy_intra? node_name_ + "/fake_controller_joint_states": "joint_states";
	const std::string js_topic = node_name_ + "/fake_controller_joint_states";
	rose_ros::pub_JointState = rose_ros::nh->advertise<sensor_msgs::JointState>(js_topic, 10);

	// rose_ros::sub_Twist = rose_ros::nh->subscribe("cmd_vel", 1, &tros_instance::did_cmd_vel_subscribed, this);
	// rose_ros::sub_JointState = rose_ros::nh->subscribe("joint_states", 1, &tros_instance::did_joint_states_subscribed, this);

	initialized_ = true;
}

void tros_instance::subscribe_movebase_topics()
{
	rose_ros::sub_map = rose_ros::nh->subscribe("map", 1, &tros_instance::did_map_subscribed, this);

	rose_ros::sub_current_goal = rose_ros::nh->subscribe(node_name_ + "/current_goal", 1, &tros_instance::did_current_goal_subscribed, this);
	rose_ros::sub_global_plan = rose_ros::nh->subscribe(node_name_ + "/GlobalPlanner/plan", 1, &tros_instance::did_global_plan_subscribed, this);
	rose_ros::sub_local_plan = rose_ros::nh->subscribe(node_name_ + "/DWAPlannerROS/local_plan", 1, &tros_instance::did_local_plan_subscribed, this);
	rose_ros::sub_recovery_status = rose_ros::nh->subscribe("move_base/recovery_status", 1, &tros_instance::did_recovery_status, this);
	rose_ros::sub_move_base_result = rose_ros::nh->subscribe("move_base/result", 1, &tros_instance::did_move_base_result, this);
}

void tros_instance::shutdown_movebase_topics()
{

	rose_ros::sub_current_goal.shutdown();
	rose_ros::sub_global_plan.shutdown();
	rose_ros::sub_local_plan.shutdown();
	rose_ros::sub_recovery_status.shutdown();
	rose_ros::sub_move_base_result.shutdown();

	shutdown_map_topic();
}

void tros_instance::subscribe_laserscan_topics()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(started() && navigation_node_started(), null_str);
	VALIDATE(!laserscan_started_, null_str);

	// rose_ros::sub_laser_scan = rose_ros::nh->subscribe("laser_scan", 1, &tros_instance::did_scan_subscribed, this);
	// {dbg_publish}laser_scan
	rose_ros::sub_laser_scan = rose_ros::nh->subscribe("scan", 1, &tros_instance::did_scan_subscribed, this);

	laserscan_started_ = true;
}

void tros_instance::shutdown_laserscan_topics()
{
	rose_ros::sub_laser_scan.shutdown();
	laserscan_started_ = false;
}

void tros_instance::start()
{
	if (started_) {
		VALIDATE(initialized_, null_str);
		return;
	}
	if (!initialized_) {
		initialize();
	}

	VALIDATE(rose_ros::nh.get() != nullptr && rose_ros::cbqueue.get() != nullptr, null_str);
	VALIDATE(!started_, null_str);
	started_ = true;

	// start tfl
	VALIDATE(rose_ros::tf_.get() == nullptr && rose_ros::tfl_.get() == nullptr, null_str);
	rose_ros::tf_.reset(new tf2_ros::Buffer());
	rose_ros::tfl_.reset(new tf2_ros::TransformListener(*rose_ros::tf_));
}

void tros_instance::stop()
{
	VALIDATE(mode_ == nposm, null_str);
	VALIDATE(!moveit_node_started_, null_str);
	VALIDATE(!laserscan_started_, null_str);
	rose_ros::client.reset();

	// stop tfl
	rose_ros::tfl_.reset();
	rose_ros::tf_.reset();

	started_ = false;

	// uint32_t max_delay = 4000;
	// uint32_t start = SDL_GetTicks();
	// while (ros::isStarted() && SDL_GetTicks() - start < max_delay) {
	//	SDL_Delay(100);
	// }
	// VALIDATE(!ros::isStarted(), null_str);
}

void tros_instance::clear_tfl()
{
	VALIDATE(rose_ros::tf_.get() != nullptr && rose_ros::tfl_.get() != nullptr, null_str);
	rose_ros::tf_->clear();
}

void tros_instance::stop_moveit_node()
{
    VALIDATE(moveit_node_started_, null_str);

	if (!light_moveit_) {
		move_group_.reset();
	}

	VALIDATE(joint_state_publisher_.get() != nullptr, null_str);
	joint_state_publisher_.reset();
	robot_state_publisher_.reset();
	moveit_driver_.stop_moveit();

	SDL_Log("%u [stop_moveit_node]pre base_footprint_2_base_link_.reset()", SDL_GetTicks());
	base_footprint_2_base_link_.reset();

	moveit_node_started_ = false;
}

void tros_instance::slice()
{
	VALIDATE(rose_ros::cbqueue.get() != nullptr, null_str);

	if (mode_ == mode_navigation) {
		for (std::set<tslot*>::iterator it = slots_.begin(); it != slots_.end(); ++ it) {
			tslot& slot = **it;
			if (!slot.map_received && navigation_map_.info.width > 0 && navigation_map_.info.height > 0) {
				slot.map_received = true;
				slot.did_map_subscribed(navigation_map_);
			}
			if (!slot.goal_received && !navigation_goal_.header.frame_id.empty()) {
				slot.goal_received = true;
				slot.did_current_goal_subscribed(navigation_goal_);
			}
		}
	}
	rose_ros::cbqueue->callAvailable(ros::WallDuration());
}

void tros_instance::shutdown_map_topic()
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(rose_ros::nh.get() != nullptr && rose_ros::cbqueue.get() != nullptr, null_str);
	VALIDATE(started_, null_str);
	rose_ros::sub_map.shutdown();
}

extern bool load_moveit_from_rsp2(const std::string& path_to_rsp, trsp_moveit2& moveit);

void tros_instance::load_moveit_model(const std::string& aplt_res_path, const std::string& name, bool force_load)
{
	// is called by moveit_driver::set_slot(slot), as far, maybe hasn't call tros_instance::start
	if (!initialized_) {
		initialize();
	}
	VALIDATE(rose_ros::nh.get() != nullptr, null_str);

	moveit_model.clear();
	if (aplt_res_path.empty()) {
		// if aplt_res_path is empty, mean to clear moveit_model only.
		return;
	}

	const std::string product_path = aplt_res_path + "/moveit/" + name;
	bool ret = false;

	ros::NodeHandle& nh = *rose_ros::nh.get();
    std::string urdf_param;
    if (!force_load) {
	    nh.param("urdf_param", urdf_param, std::string(""));
    }
	if (urdf_param.empty()) {
		ret = ros::load_urdf(nh, product_path + "/moveit.urdf");
		if (!ret) {
			return;
		}
		ret = ros::load_srdf(nh, product_path + "/moveit.srdf");
		if (!ret) {
			return;
		}
	}

    robot_model_loader::RobotModelLoader loader("robot_description", false);
    moveit_model.model = loader.getModel();
	if (moveit_model.model.get() == nullptr) {
		return;
	}

	// change moveit::core::RobotModel to aplt::trobot_model
	RobotModel_to_aplt_model(*moveit_model.model.get(), moveit_model.aplt_model);

	moveit_model.tf_calculator.reset(ros::create_kdl_tf_calculator(*loader.getURDF().get()));

    moveit_model.state.reset(new robot_state::RobotState(moveit_model.model));
    moveit_model.state->setToDefaultValues();

	const urdf::ModelInterfaceSharedPtr& urdf = loader.getURDF();
	RobotState_to_aplt_positions(*moveit_model.state, moveit_model.aplt_positions);

    const std::string rspfile = product_path + "/moveit.rsp";
    load_moveit_from_rsp2(rspfile, moveit_model.rsp_moveit);
}

void tros_instance::clear_moveit_model()
{
	moveit_model.clear();
}

bool tros_instance::is_moveit_model_valid() const
{
	return moveit_model.valid();
}

void* tros_instance::RobotState_of_moveit_model()
{
	VALIDATE(moveit_model.valid(), null_str);
	return moveit_model.state.get();
}

const trsp_moveit2& tros_instance::rsp_of_moveit_model(bool must_valid) const
{
	if (must_valid) {
		VALIDATE(moveit_model.valid(), null_str);
	}
	return moveit_model.rsp_moveit;
}

const aplt::trobot_model& tros_instance::aplt_model_of_moveit_model() const
{
	VALIDATE(moveit_model.valid(), null_str);
	return moveit_model.aplt_model;
}

aplt::tvariable_positions& tros_instance::aplt_positions_of_moveit_model() const
{
	VALIDATE(moveit_model.valid(), null_str);
	return moveit_model.aplt_positions;
}

ros::ttf_calculator& tros_instance::tf_calculator_of_moveit_model()
{
	VALIDATE(moveit_model.valid(), null_str);
	return *moveit_model.tf_calculator.get();
}

geometry_msgs::Pose tros_instance::calculate_group_fk_tf(const std::string& group_name, const std::vector<double>* angles_ptr)
{
	VALIDATE(moveit_model.valid(), null_str);
	const aplt::trobot_model& aplt_model = moveit_model.aplt_model;

	VALIDATE(aplt_model.joint_model_group_map_.count(group_name) != 0, null_str);
	const aplt::tjoint_group_model* curr_group = aplt_model.joint_model_group_map_.find(group_name)->second;

	std::vector<double> angles;
	if (angles_ptr == nullptr) {
		for (std::vector<std::string>::const_iterator it = curr_group->variable_names_.begin(); it != curr_group->variable_names_.end(); ++ it) {
			const aplt::tjoint_model* joint = aplt_model.joint_model_map_.find(*it)->second;

			double value = moveit_model.aplt_positions.positions[joint->variable_index_];
			angles.push_back(value);
		}
	} else {
		VALIDATE(angles_ptr->size() == curr_group->variable_names_.size(), null_str);
		angles = *angles_ptr;
	}

	std::vector<ros::tfk_joint> fk_joints_;
	for (std::vector<aplt::tjoint_model*>::const_iterator it = curr_group->joint_model_vector_.begin(); it != curr_group->joint_model_vector_.end(); ++ it) {
		const aplt::tjoint_model* joint_model = *it;

		bool fixed = aplt_model.joint_variables_index_map_.count(joint_model->name_) == 0;
		fk_joints_.push_back(ros::tfk_joint(joint_model->name_, fixed, joint_model->origin_, joint_model->axis_));
	}

	geometry_msgs::Pose result = moveit_model.tf_calculator->calculate_tf(fk_joints_, angles);
	return result;
}

double* tros_instance::joint_position_from_name(const std::string& joint_name) const
{
	const aplt::trobot_model& aplt_model = aplt_model_of_moveit_model();
	VALIDATE(aplt_model.joint_variables_index_map_.count(joint_name) != 0, null_str);
	int joint_index = aplt_model.joint_variables_index_map_.find(joint_name)->second;

	const aplt::tvariable_positions& aplt_position = aplt_positions_of_moveit_model();
	return aplt_position.positions + joint_index;
}

const aplt::tjoint_model& tros_instance::joint_model_from_name(const std::string& joint_name) const
{
	const aplt::trobot_model& aplt_model = aplt_model_of_moveit_model();
	VALIDATE(aplt_model.joint_model_map_.count(joint_name) != 0, null_str);
	const aplt::tjoint_model* result = aplt_model.joint_model_map_.find(joint_name)->second;

	return *result;
}

ros::CallbackQueue& tros_instance::get_cbqueue()
{
	VALIDATE(rose_ros::cbqueue.get() != nullptr, null_str);
	return *rose_ros::cbqueue.get();
}

bool tros_instance::get_laser_tf(geometry_msgs::TransformStamped& transform)
{
	VALIDATE(rose_ros::tf_.get() != nullptr && rose_ros::tfl_.get() != nullptr, null_str);
	try {
		// listener.waitForTransform("/base_footprint", "/base_link", ros::Time(), ros::Duration(3.0));
		// listener.lookupTransform("/base_footprint", "/base_link", ros::Time(), transform);

		// listener.waitForTransform("/map", "/odom", ros::Time(), ros::Duration(3.0));
		// listener.lookupTransform("/map", "/odom", ros::Time(), transform);

		// bool can = listener.waitForTransform("/odom", "/laser", ros::Time(), ros::Duration(3.0));
		// listener.lookupTransform("/laser", "/odom", ros::Time(), transform);

		tf2_ros::Buffer& tf2_buffer = *rose_ros::tf_.get();

		const std::string target_frame = "map";
		const std::string source_frame = "laser";
		// bool can = tf2_buffer.canTransform(target_frame, source_frame, ros::Time(), ros::Duration(0.0));
		// if (can) {
			transform = tf2_buffer.lookupTransform(target_frame, source_frame, ros::Time());
			return true;
		// }

	} catch (tf2::TransformException& ex) {
		ROS_ERROR("get_laser_tf: %s", ex.what());
	}
	return false;
}

bool tros_instance::get_laser_2_base_footprint_tf(geometry_msgs::TransformStamped& transform)
{
	VALIDATE(rose_ros::tf_.get() != nullptr && rose_ros::tfl_.get() != nullptr, null_str);
	try {
		// listener.waitForTransform("/base_footprint", "/base_link", ros::Time(), ros::Duration(3.0));
		// listener.lookupTransform("/base_footprint", "/base_link", ros::Time(), transform);

		// listener.waitForTransform("/map", "/odom", ros::Time(), ros::Duration(3.0));
		// listener.lookupTransform("/map", "/odom", ros::Time(), transform);

		// bool can = listener.waitForTransform("/odom", "/laser", ros::Time(), ros::Duration(3.0));
		// listener.lookupTransform("/laser", "/odom", ros::Time(), transform);

		tf2_ros::Buffer& tf2_buffer = *rose_ros::tf_.get();

		const std::string target_frame = "base_footprint";
		const std::string source_frame = "laser";
		// bool can = tf2_buffer.canTransform(target_frame, source_frame, ros::Time(), ros::Duration(0.0));
		// if (can) {
			transform = tf2_buffer.lookupTransform(target_frame, source_frame, ros::Time());
			return true;
		// }

	} catch (tf2::TransformException& ex) {
		ROS_ERROR("get_laser_2_base_footprint_tf: %s", ex.what());
	}
	return false;
}

bool tros_instance::get_source_2_target_tf(const std::string& source_frame, const std::string& target_frame, geometry_msgs::TransformStamped& transform)
{
	VALIDATE(rose_ros::tf_.get() != nullptr && rose_ros::tfl_.get() != nullptr, null_str);
	try {
		tf2_ros::Buffer& tf2_buffer = *rose_ros::tf_.get();

		// const std::string target_frame = "base_footprint";
		// const std::string source_frame = "laser";
		// bool can = tf2_buffer.canTransform(target_frame, source_frame, ros::Time(), ros::Duration(0.0));
		// if (can) {
			transform = tf2_buffer.lookupTransform(target_frame, source_frame, ros::Time());
			return true;
		// }

	} catch (tf2::TransformException& ex) {
		ROS_ERROR("get_source_2_target_tf: %s", ex.what());
	}
	return false;
}

static double calculate_speed()
{
	int velocity = preferences::velocity();
	VALIDATE(velocity != velocity_custom, null_str);
	if (velocity == velocity_slower) {
		return 0.15; // ??
	} else if (velocity == velocity_slow) {
		return 0.25;
	} else if (velocity == velocity_fast) {
		return 0.7;
	}
	return 0.5;
}

static double calculate_turn()
{
	int velocity = preferences::velocity();
	VALIDATE(velocity != velocity_custom, null_str);

	if (velocity == velocity_slower) {
		return 0.6; // ??
	} else if (velocity == velocity_slow) {
		return 0.6;
	} else if (velocity == velocity_fast) {
		return 1.0;
	}
	return 0.8;
}

void tros_instance::do_riki_action(int op, double* twist_ptr)
{
	// double speed = 0.5;
	// double turn = 1.0;
	double speed = calculate_speed();
	double turn = calculate_turn();
	double x = 0;
	double y = 0;
	double z = 0;
	double th = 0;

	VALIDATE(moveBindings_.count(op) != 0, null_str);

	const t4val& val = moveBindings_.find(op)->second;
	x = val.x;
	y = val.y;
	z = val.z;
	th = val.th;

	geometry_msgs::Twist twist;

	twist.linear.x = x*speed;
	twist.linear.y = y*speed;
	twist.linear.z = z*speed;

	twist.angular.x = 0;
	twist.angular.y = 0;
	twist.angular.z = th*turn;

	if (false) {
		twist.linear.x = -0.00; // 0.06 no
		twist.linear.y = 0;
		twist.angular.z = DEG2RAD(30); // -0.5, -0.44736, -0.28947, -0.18421, -0.02631
	}
	SDL_Log("send Twist msg: linear(%.2f, %.2f, %.2f), angular(%.2f, %.2f, %.2f)", twist.linear.x, twist.linear.y, twist.linear.z,
		twist.angular.x, twist.angular.y, twist.angular.z);

	rose_ros::pub_twist.publish(twist);

	if (twist_ptr != nullptr) {
		twist_ptr[0] = twist.linear.x;
		twist_ptr[1] = twist.linear.y;
		twist_ptr[2] = twist.angular.z;
	}
}

void tros_instance::public_vel(double linear_x, double linear_y, double angular_z)
{
	geometry_msgs::Twist twist;

	twist.linear.x = linear_x; // ros::min_moveable_vel_x;
	twist.linear.y = linear_y;
	twist.linear.z = 0;

	twist.angular.x = 0;
	twist.angular.y = 0;
	twist.angular.z = angular_z; // ros::min_moveable_vel_theta;

	rose_ros::pub_twist.publish(twist);
}

void tros_instance::do_JointState_action(const std::map<std::string, double>& values)
{
	VALIDATE(moveit_model.valid(), null_str);

	sensor_msgs::JointState state;

	std_msgs::Header& header = state.header;
	// header.seq = 0;
	// header.frame_id = "map";
	header.stamp = ros::Time::now();
	state.name = moveit_model.aplt_model.variable_names_;

	int at = 0;
	for (std::vector<std::string>::const_iterator it = state.name.begin(); it != state.name.end(); ++ it, at ++) {
		const std::string& name = *it;
		if (values.count(name) != 0) {
			state.position.push_back(values.find(name)->second);
		} else {
			state.position.push_back(moveit_model.aplt_positions.positions[at]);
		}
	}

	SDL_Log("send JointState: %s", JointState_to_string(state).c_str());

	rose_ros::pub_JointState.publish(state);
}

void tros_instance::do_ik()
{
    // double q_custom[3] = {0.1, 0.1, -0.1};
	double q_custom[3] = {1.5, 3.1, 1.4};
    ros::turdf urdf("C:/ddksample/moveit_ws/src/tank_arm/robots/tank_arm.urdf", "arm_Link", "grasping_frame", 
        3, ros::turdf::q_custom, q_custom);

	bool use_pose = true;
	if (use_pose) {
/*
		// (0.5, 0.6, -0.7)
		// urdf.push_pose(Eigen::Matrix<double, 6, 1>(0.381415, -0.000289008, 0.118199, 1.15452, -2.48807, 1.84256).data());
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289976, -0.1559, 0.2161, -0.981922, -0.615231, 2.7834).data());

		// (0, 0, 0)
		// urdf.push_pose(Eigen::Matrix<double, 6, 1>(0.298735, -0.00028825, 0.293431, 2.34394, -3.13551, 1.57701).data());
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000288915, 0.0272588, 0.279306, 3.67332e-06, -0.00869265, 3.14158).data());

		// (0, 0, -0.7)
		// urdf.push_pose(Eigen::Matrix<double, 6, 1>(0.298735, -0.00028825, 0.293431, 2.48028, -2.65714, 1.04367).data());
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000288915, 0.0272588, 0.279306, 0.00560285, -0.00664611, 2.44156).data());

		// (0, 0, 0.7)
		// urdf.push_pose(Eigen::Matrix<double, 6, 1>(0.298735, -0.00028825, 0.293431, 2.47333, 2.66762, 2.10715).data());
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000288915, 0.0272588, 0.279306, 3.136, -3.13494, 0.700008).data());
*/
		// (0, 0.77, 0.0)
		// base_link: p:(0.35825, -0.00028, 0.20344) M:(1.57395(90.18106), -3.14155(-179.99816), 1.57948(90.49790))
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000290, -0.078716, 0.259055, 2.371577, -3.135353, 0.006042).data());
/*	
		// base_link: p:(0.34825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.071732, 0.251898, 2.371577, -3.135353, 0.006042).data());

		// base_link: p:(0.33825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.064748, 0.244741, 2.371577, -3.135353, 0.006042).data());
		// base_link: p:(0.32825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.057764, 0.237584, 2.371577, -3.135353, 0.006042).data());
		// base_link: p:(0.31825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.050780, 0.230427, 2.371577, -3.135353, 0.006042).data());
		// base_link: p:(0.30825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.043796, 0.223270, 2.371577, -3.135353, 0.006042).data());
		// base_link: p:(0.29825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.036812, 0.216112, 2.371577, -3.135353, 0.006042).data());
		// base_link: p:(0.28825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.029828, 0.208955, 2.371577, -3.135353, 0.006042).data());
		// base_link: p:(0.27825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.022844, 0.201798, 2.371577, -3.135353, 0.006042).data());
		// base_link: p:(0.26825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.015860, 0.194641, 2.371577, -3.135353, 0.006042).data());
		// base_link: p:(0.25825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.008876, 0.187484, 2.371577, -3.135353, 0.006042).data());
		// base_link: p:(0.24825, -0.00028, 0.20344)
		urdf.push_pose(Eigen::Matrix<double, 6, 1>(-0.000289, -0.001892, 0.180327, 2.371577, -3.135353, 0.006042).data());
*/

	} else {
		urdf.push_js(Eigen::Vector3d(0.5, 0.6, -0.7).data());
		urdf.push_js(Eigen::Vector3d(0, 0, 0).data());
		urdf.push_js(Eigen::Vector3d(0, 0, -0.7).data());
		urdf.push_js(Eigen::Vector3d(0, 0, 0.7).data());
	}

	ros::ikfast_test(*rose_ros::nh.get(), urdf);
}

// 当action完成后会调用该回调函数一次
static void doneCb(const actionlib::SimpleClientGoalState& state,
        const move_base_msgs::MoveBaseResultConstPtr& result)
{
    ROS_INFO("[ros_do_move_client]downCb, Yay! The dishes are now clean");
    // ros::shutdown();
}

// 当action激活后会调用该回调函数一次
static void activeCb()
{
    ROS_INFO("[ros_do_move_client]activeCb, Goal just went active");
}

// 收到feedback后调用该回调函数
static void feedbackCb(const move_base_msgs::MoveBaseFeedbackConstPtr& feedback)
{
	const move_base_msgs::MoveBaseFeedback& fb = *feedback.get();
	// ROS_INFO(" [ros_do_move_client]feedbackCb, position(%.5f, %.5f, %.5f)", 
	//	fb.base_position.pose.position.x, fb.base_position.pose.position.y, fb.base_position.pose.position.z);
}

bool tros_instance::do_send_goal(const move_base_msgs::MoveBaseGoal& goal)
{
	if (moveit_node_started()) {
		// group_state: navigation
		std::vector<double> joint_values;
		bool wheeltec = true;

		std::string group;
		if (!wheeltec) {
			group = "robot_arm";
			double extra_diff = -0.1;
			joint_values.push_back(-0.77316 + extra_diff); //
			// joint_values.push_back(0.82);
			joint_values.push_back(0);
			joint_values.push_back(0);

		} else {
			group = "arm";
			joint_values.push_back(0.3); //
			joint_values.push_back(-0.02);
			joint_values.push_back(0.3);
			joint_values.push_back(0.3);
			joint_values.push_back(0.3);
		}
		if (!light_moveit_) {
			do_joint_value_target(group, joint_values);
		} else {
			// do_light_joint_value_target(group, joint_values);
		}
	}

	VALIDATE(rose_ros::nh.get() != nullptr && rose_ros::cbqueue.get() != nullptr, null_str);

	if (rose_ros::client.get() == nullptr) {
		// make doneCb/activeCb/feedbackCb executed in Main-Thread
		// 1)use nh's cbqueue, 2)don't spin new thread.
		rose_ros::client.reset(new rose_ros::MoveBaseClient(*rose_ros::nh.get(), "move_base", false));
	}
/*
	ROS_INFO("Waiting for action server to start.");
	bool started = client.waitForServer();
	if (!started) {
		return false;
	}
*/
	ROS_INFO("Action server started, sending goal.");

	// send action's goal to server
	rose_ros::client->sendGoal(goal, &doneCb, &activeCb, &feedbackCb);
	goaling_ = true;
	
    return true;
}

void tros_instance::do_cancel_goal()
{
	if (rose_ros::client.get() != nullptr) {
		rose_ros::client->cancelGoal();
	}
}

void tros_instance::makesure_cancel_goal(bool with_erase_task)
{
	VALIDATE(with_erase_task, null_str);
	VALIDATE_IN_MAIN_THREAD();
	if (goaling_) {
        do_cancel_goal();
        while (goaling_) {
            SDL_Delay(10);
            slice();
        }
    }

	if (with_erase_task) {
		// 'keep_task == true' in tros_instance::did_move_base_result, task_ is existed still.
		if (has_task()) {
			erase_task();
		}
	}
}

bool tros_instance::moveto_function(gui2::tprogress_& progress, const tros_instance::ttask& task, const tdrivers::tvars& vars)
{
	VALIDATE(task.bh == navigation_bh_luafunc, null_str);

	start_navigation_node(mode_navigation, nposm, vars, task.rspfile);

	move_base_msgs::MoveBaseGoal goal_;
	goal_.target_pose.header.seq = 0;
	// goal_.target_pose.header.seq ++;
    goal_.target_pose.header.frame_id = "map";
    goal_.target_pose.pose.position.x = task.x;
    goal_.target_pose.pose.position.y = task.y;
    goal_.target_pose.pose.position.z = 0;

    goal_.target_pose.header.stamp = ros::Time::now();
	if (!is_float_nposm(task.theta)) {
		tf2::Quaternion q;
		q.setRPY(0, 0, task.theta);
		goal_.target_pose.pose.orientation.x = q.x();
		goal_.target_pose.pose.orientation.y = q.y();
		goal_.target_pose.pose.orientation.z = q.z();
		goal_.target_pose.pose.orientation.w = q.w();

	} else {
		set_quaternion_raw_nposm(goal_.target_pose.pose.orientation);
	}

    SDL_Log("%u, did_mouse_leave_camera, deg: %.5f, position(%.5f, %.5f, %.5f), orientation(%.5f, %.5f, %.5f, %.5f)", 
        SDL_GetTicks(), RAD2DEG(task.theta), 
        goal_.target_pose.pose.position.x, goal_.target_pose.pose.position.y, goal_.target_pose.pose.position.z,
        goal_.target_pose.pose.orientation.x, goal_.target_pose.pose.orientation.y, goal_.target_pose.pose.orientation.z, goal_.target_pose.pose.orientation.w);

	const int timeout = 12000; // one instance spend: 6464(ms)
	// const int timeout = 40000; // one instance spend: 6464(ms)
	const uint32_t start_ticks = SDL_GetTicks();
	while (!move_base_constructed_) {
		if ((int)(SDL_GetTicks() - start_ticks) >= timeout) {
			move_base_constructed_ = true;
			return false;
		}
		progress.show_slice();
		SDL_Delay(10);
	}

    do_send_goal(goal_);
	return true;
}

void tros_instance::erase_task()
{
	SDL_Log("{tros_instance::erase_task} called");

	VALIDATE(task_.get() != nullptr, null_str);
	deregister_slot(*task_.get());
	task_ = nullptr;
}

bool tros_instance::in_map_viewer() const
{
	return has_task() || instance->bg_task().in_task_cpp2();
}

void tros_instance::register_slot(tslot& slot)
{
	VALIDATE(!slot.registered, null_str);
	VALIDATE(slots_.count(&slot) == 0, null_str);

	if (slots_.empty()) {
		VALIDATE(slot.type == tslot::type_base_node, null_str);
	}

	slot.registered = true;
	slot.map_received = false;
	slot.goal_received = false;
	if (slots_.empty()) {
		start();
	}
	slots_.insert(&slot);
}

void tros_instance::deregister_slot(tslot& slot)
{
	std::set<tslot*>::iterator it = slots_.find(&slot);
	VALIDATE(it != slots_.end(), null_str);
	const int erase_type = slot.type;
	slots_.erase(it);
	slot.registered = false;

	if (erase_type == tslot::type_task) {
		// if navigation.luafunc is moveit, in did_move_base_result(), keep_task is true and don't call erase_task().
		// erase_task() is called until game_instance::app_bg_task_stopped(). and is stoped before.
		if (mode_ != nposm) {
			stop_navigation_node();
		}
	}

	if (slots_.size() == 1) {
		const tslot* slot = *slots_.begin();
		VALIDATE(slot->type == tslot::type_base_node, null_str);

		if (mode_ != nposm) {
			stop_navigation_node();
		}

		if (moveit_node_started()) {
			stop_moveit_node();
		}

	} else if (slots_.empty()) {
		VALIDATE(erase_type == tslot::type_base_node, null_str);
		VALIDATE(mode_ == nposm, null_str);

		VALIDATE(!moveit_node_started(), null_str);
		stop();
		VALIDATE(initialized_ && !started_, null_str);
	}
}

void tros_instance::set_msg_sink(tmsg_sink* sink)
{
	VALIDATE_IN_MAIN_THREAD();
	if (sink != nullptr) {
		VALIDATE(msg_sink_ == nullptr, null_str);
	} else {
		VALIDATE(msg_sink_ != nullptr, null_str);
	}

	msg_sink_ = sink;
}

void tros_instance::send_immediate_kidnap_msg(int msg, int reason)
{
	VALIDATE_IN_MAIN_THREAD();
	if (msg_sink_ != nullptr) {
		msg_sink_->immediate_kidnap_msg(msg, reason);
	}
}

#include <moveit/move_group_interface/move_group_interface.h>
#include <moveit/rdf_loader/rdf_loader.h>
#include <tf2_eigen/tf2_eigen.h>

#include <moveit/planning_scene_interface/planning_scene_interface.h>
#include <moveit/common_planning_interface_objects/common_objects.h>
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>

std::string DebugString(const geometry_msgs::Pose& pose)
{
	char buf[128];
	SDL_snprintf(buf, sizeof(buf), "{p: (%.6f, %.6f, %.6f), M: (%.6fi+%.6fj+%.6fk+%.6f)}", pose.position.x, pose.position.y, pose.position.z,
		pose.orientation.x, pose.orientation.y, pose.orientation.z, pose.orientation.w);
	return buf;
}

class MoveGroupTestFixture
{
public:
	MoveGroupTestFixture(moveit::planning_interface::MoveGroupInterface& move_group, ros::CallbackQueue& cbqueue)
		: move_group_(move_group)
		, planning_scene_interface_(cbqueue)
	{
		SetUp();
	}

	void SetUp()
	{
/*
		nh_ = ros::NodeHandle("/move_group_interface_cpp_test");
		move_group_ = std::make_shared<moveit::planning_interface::MoveGroupInterface>(PLANNING_GROUP);

		// set velocity and acceleration scaling factors (full speed)
		move_group_->setMaxVelocityScalingFactor(MAX_VELOCITY_SCALE);
		move_group_->setMaxAccelerationScalingFactor(MAX_ACCELERATION_SCALE);

		// allow more time for planning
		move_group_->setPlanningTime(PLANNING_TIME_S);

		// set the tolerance for the goals to be smaller than epsilon
		move_group_->setGoalTolerance(GOAL_TOLERANCE);
*/
		/* the tf buffer is not strictly needed,
			but it's a simple way to add the codepaths to the tests */
		psm_ = std::make_shared<planning_scene_monitor::PlanningSceneMonitor>("robot_description",
																				moveit::planning_interface::getSharedTF());
		psm_->startSceneMonitor("/move_group/monitored_planning_scene");
		const std::string DEFAULT_PLANNING_SCENE_SERVICE = "get_planning_scene";
		psm_->requestPlanningSceneState(DEFAULT_PLANNING_SCENE_SERVICE);

		// give move_group_, planning_scene_interface_ and psm_ time to connect their topics
		// ros::Duration(0.5).sleep();
	}

	// run updater() and ensure at least one geometry update was processed by the `move_group` node after doing so
	void synchronizeGeometryUpdate(const std::function<void()>& updater)
	{
		SDL_Log("{MoveGroupTestFixture::synchronizeGeometryUpdate}---");
		std::promise<void> promise;
		std::future<void> future = promise.get_future();
		psm_->addUpdateCallback([this, &promise](planning_scene_monitor::PlanningSceneMonitor::SceneUpdateType t) {
			if (t & planning_scene_monitor::PlanningSceneMonitor::UPDATE_GEOMETRY) {
				SDL_Log("%u{MoveGroupTestFixture::synchronizeGeometryUpdate}pre promise.set_value()", SDL_GetTicks());
				promise.set_value();
			}
			psm_->clearUpdateCallbacks();
		});
		updater();
		// the updater must have triggered a geometry update, otherwise we can't be sure about the state of the scene anymore
		// std::future_status status = future.wait_for(std::chrono::seconds(5));
		std::future_status status = future.wait_for(std::chrono::seconds(1200));
		VALIDATE(status == std::future_status::ready, null_str);
		SDL_Log("%u---{MoveGroupTestFixture::synchronizeGeometryUpdate}X", SDL_GetTicks());
	}

	void ModifyPlanningSceneAsyncInterfaces()
	{
		SDL_Log("{MoveGroupTestFixture::ModifyPlanningSceneAsyncInterfaces}---");
		////////////////////////////////////////////////////////////////////
		// Define a collision object ROS message.
		moveit_msgs::CollisionObject collision_object;
		collision_object.header.frame_id = move_group_.getPlanningFrame();

		// The id of the object is used to identify it.
		collision_object.id = "box1";

		// Define a box to add to the world.
		shape_msgs::SolidPrimitive primitive;
		primitive.type = primitive.BOX;
		primitive.dimensions.resize(3);
		primitive.dimensions[0] = 0.1;
		primitive.dimensions[1] = 1.0;
		primitive.dimensions[2] = 1.0;

		// Define a pose for the box (specified relative to frame_id)
		geometry_msgs::Pose box_pose;
		box_pose.orientation.w = 1.0;
		box_pose.position.x = 0.5;
		box_pose.position.y = 0.0;
		box_pose.position.z = 0.5;

		collision_object.primitives.push_back(primitive);
		collision_object.primitive_poses.push_back(box_pose);
		collision_object.operation = collision_object.ADD;

		std::vector<moveit_msgs::CollisionObject> collision_objects;
		collision_objects.push_back(collision_object);
/*
		{
			collision_object.id = "box2";
			geometry_msgs::Pose box_pose2;

			box_pose2.orientation.w = 1.0;
			box_pose2.position.x = 0.30;
			box_pose2.position.y = -0.0002882;
			box_pose2.position.z = -0.3; // 0.21

			collision_object.primitive_poses.clear();
			collision_object.primitive_poses.push_back(box_pose2);
			collision_objects.push_back(collision_object);
		}
*/
		// Now, let's add the collision object into the world
		synchronizeGeometryUpdate([&]() { planning_scene_interface_.addCollisionObjects(collision_objects); });
		size_t s = planning_scene_interface_.getObjects().size();
		VALIDATE(s == collision_objects.size(), null_str);
/*
		// attach and detach collision object
		for (std::vector<moveit_msgs::CollisionObject>::const_iterator it = collision_objects.begin(); it != collision_objects.end(); ++ it) {
			const moveit_msgs::CollisionObject& object = *it;
			synchronizeGeometryUpdate([&]() { 
				bool ret = move_group_.attachObject(object.id, "base_link");
				VALIDATE(ret, null_str);
			});
		}
		s = planning_scene_interface_.getAttachedObjects().size();
		VALIDATE(s == collision_objects.size(), null_str);

		synchronizeGeometryUpdate([&]() {
			bool ret = move_group_.detachObject(collision_object.id);
			VALIDATE(ret, null_str);
		});
		s = planning_scene_interface_.getAttachedObjects().size();
		VALIDATE(s == std::size_t(0), null_str);

		// remove object from world
		const std::vector<std::string> object_ids = { collision_object.id };
		s = planning_scene_interface_.getObjects().size();
		VALIDATE(s == std::size_t(1), null_str);
		synchronizeGeometryUpdate([&]() { planning_scene_interface_.removeCollisionObjects(object_ids); });
		s = planning_scene_interface_.getObjects().size();
		VALIDATE(s == std::size_t(0), null_str);
*/
		SDL_Log("---{MoveGroupTestFixture::ModifyPlanningSceneAsyncInterfaces}X");
	}

	void attachObject()
	{
		// attach and detach collision object
			synchronizeGeometryUpdate([&]() { 
				bool ret = move_group_.attachObject("box1", "grasping_frame"); // base_link
				VALIDATE(ret, null_str);
			});
		size_t s = planning_scene_interface_.getAttachedObjects().size();
		VALIDATE(s == 1, null_str);
	}

	void clear()
	{
		size_t s = planning_scene_interface_.getAttachedObjects().size();
		VALIDATE(s == 1, null_str);

		synchronizeGeometryUpdate([&]() {
			bool ret = move_group_.detachObject("box1");
			VALIDATE(ret, null_str);
		});
		s = planning_scene_interface_.getAttachedObjects().size();
		VALIDATE(s == std::size_t(0), null_str);

		// remove object from world
		const std::vector<std::string> object_ids = { "box1" };
		s = planning_scene_interface_.getObjects().size();
		VALIDATE(s == std::size_t(1), null_str);
		synchronizeGeometryUpdate([&]() { planning_scene_interface_.removeCollisionObjects(object_ids); });
		s = planning_scene_interface_.getObjects().size();
		VALIDATE(s == std::size_t(0), null_str);
	}

private:
	moveit::planning_interface::MoveGroupInterface& move_group_;
	moveit::planning_interface::PlanningSceneInterface planning_scene_interface_;
	planning_scene_monitor::PlanningSceneMonitorPtr psm_;
};

static std::string verbose_Isometry3d(const Eigen::Isometry3d& tf)
{
    char buf[512];
    std::stringstream ss;

    const Eigen::Vector3d& trans = tf.translation();
    const Eigen::Quaterniond quat(tf.rotation());
    ss << tf.rotation();
    SDL_snprintf(buf, sizeof(buf), "{p(%.6f, %.6f, %.6f) quat(%.6fi+%.6fj+%.6fk+%.6f)}\n%s", trans.x(), trans.y(), trans.z(),
        quat.x(), quat.y(), quat.z(), quat.w(), ss.str().c_str());
    // SDL_snprintf(buf, sizeof(buf), "{p(%.6f, %.6f, %.6f) quat(%.6fi+%.6fj+%.6fk+%.6f)}", trans.x(), trans.y(), trans.z(),
    //    quat.x(), quat.y(), quat.z(), quat.w());
    return buf;
}

void fillGrasps(moveit_msgs::PickupGoal& goal, const Eigen::Isometry3d& desire_pose)
{
  // planning_scene_monitor::LockedPlanningSceneRO lscene(context_->planning_scene_monitor_);

  ROS_DEBUG_NAMED("manipulation", "Using default grasp poses");
  goal.minimize_object_distance = true;

  const Eigen::Vector3d& trans = desire_pose.translation();
  const Eigen::Quaterniond quat(desire_pose.rotation());
  SDL_Log("{fillGrasps}desire_pose: %s", verbose_Isometry3d(desire_pose).c_str());
  

  // add a number of default grasp points
  // \todo add more!
  moveit_msgs::Grasp g;
  // g.grasp_pose.header.frame_id = goal.target_name;
  g.grasp_pose.header.frame_id = "base_link";
  g.grasp_pose.pose.position.x = trans.x();
  g.grasp_pose.pose.position.y = trans.y();
  g.grasp_pose.pose.position.z = trans.z();
  g.grasp_pose.pose.orientation.x = quat.x();
  g.grasp_pose.pose.orientation.y = quat.y();
  g.grasp_pose.pose.orientation.z = quat.z();
  g.grasp_pose.pose.orientation.w = quat.w();

  // g.pre_grasp_approach.direction.header.frame_id = lscene->getPlanningFrame();
  g.pre_grasp_approach.direction.header.frame_id = "base_link";
  g.pre_grasp_approach.direction.vector.x = 1.0;
  g.pre_grasp_approach.min_distance = 0.1f;
  g.pre_grasp_approach.desired_distance = 0.2f;
  
  // g.post_grasp_retreat.direction.header.frame_id = lscene->getPlanningFrame();
  g.post_grasp_retreat.direction.header.frame_id = "base_link";
  g.post_grasp_retreat.direction.vector.z = 1.0;
  g.post_grasp_retreat.min_distance = 0.1f;
  g.post_grasp_retreat.desired_distance = 0.2f;

  {
	  int ii = 0;
	  g.pre_grasp_approach.direction.vector.x = 0.0;
	  g.post_grasp_retreat.direction.vector.z = 0.0;
  }
/*
  if (lscene->getRobotModel()->hasEndEffector(goal.end_effector))
  {
    g.pre_grasp_posture.joint_names = lscene->getRobotModel()->getEndEffector(goal.end_effector)->getJointModelNames();
    g.pre_grasp_posture.points.resize(1);
    g.pre_grasp_posture.points[0].positions.resize(g.pre_grasp_posture.joint_names.size(),
                                                   std::numeric_limits<double>::max());

    g.grasp_posture.joint_names = g.pre_grasp_posture.joint_names;
    g.grasp_posture.points.resize(1);
    g.grasp_posture.points[0].positions.resize(g.grasp_posture.joint_names.size(), -std::numeric_limits<double>::max());
  }
*/
  goal.possible_grasps.push_back(g);
}

void tros_instance::do_pickplace()
{
	VALIDATE(is_moveit_model_valid() && !light_moveit_, null_str);

	ros::CallbackQueue cbqueue;
	ros::AsyncSpinner spinner(1, &cbqueue);
	spinner.start();
	ros::NodeHandle nh;
	nh.setCallbackQueue(&cbqueue);

	VALIDATE(moveit_node_started(), null_str);

	// moveit::planning_interface::MoveGroupInterface::Options opt("robot_arm", 
	//	"robot_description", *rose_ros::nh.get());
	moveit::planning_interface::MoveGroupInterface::Options opt("robot_arm", 
		"robot_description", nh);
	opt.robot_model_ = moveit_model.model;
	moveit::planning_interface::MoveGroupInterface group(opt);

	moveit::planning_interface::MoveGroupInterface::Options opt_claw("robot_claw", 
		"robot_description", nh);
	opt.robot_model_ = moveit_model.model;
	moveit::planning_interface::MoveGroupInterface group_claw(opt_claw);

	group.setGoalPositionTolerance(0.000020); // default: 0.0001
    group.setGoalOrientationTolerance(0.00002); // default: 0.001
    group.setPlanningTime(60.0); // default: 5

	Eigen::Isometry3d base_link_desire1; // (0.5, 0.6, -0.7)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.381415, -0.000289008, 0.118199),
		Eigen::Vector3d(1.15452, -2.48807, 1.84256), base_link_desire1);

	Eigen::Isometry3d base_link_desire2; // (0, 0, 0)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.298735, -0.00028825, 0.293431),
		Eigen::Vector3d(2.34394, -3.13551, 1.57701), base_link_desire2);

	Eigen::Isometry3d base_link_desire3; // (0, 0, -0.7)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.298735, -0.00028825, 0.293431),
		Eigen::Vector3d(2.48028, -2.65714, 1.04367), base_link_desire3);

	Eigen::Isometry3d base_link_desire4; // (0, 0, 0.7)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.298735, -0.00028825, 0.293431),
		Eigen::Vector3d(2.47333, 2.66762, 2.10715), base_link_desire4);

	Eigen::Isometry3d base_link_desire5; // (0, 0.77, 0)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.35825, -0.00028, 0.20344),
		Eigen::Vector3d(1.57395, -3.14155, 1.57948), base_link_desire5);


	EigenSTL::vector_Isometry3d pose_targets;
	// pose_targets.push_back(base_link_desire1);
	// pose_targets.push_back(base_link_desire2);
	// pose_targets.push_back(base_link_desire3);
	// pose_targets.push_back(base_link_desire4);
	pose_targets.push_back(base_link_desire5);

	MoveGroupTestFixture test(group, cbqueue);
	test.ModifyPlanningSceneAsyncInterfaces();

	{
		// pick
		moveit_msgs::PickupGoal goal = group.constructPickupGoal("box1",
							   std::vector<moveit_msgs::Grasp>(), false);
		fillGrasps(goal, base_link_desire1);
		// fillGrasps(goal, base_link_desire2);
		goal.minimize_object_distance = 0;		
		moveit::core::MoveItErrorCode success = group.pick(goal);
		if (success == moveit::core::MoveItErrorCode::SUCCESS) {
			int ii = 0;
		}
	}

	{
		group_claw.setNamedTarget("claw_close");
		moveit::planning_interface::MoveGroupInterface::Plan my_plan;
		moveit::core::MoveItErrorCode success = group_claw.plan(my_plan);
		ROS_INFO("Visualizing plan 1 (pose goal) %s", success? "": "FAILED");   
		if (success == moveit::core::MoveItErrorCode::SUCCESS) {
			int ii = 0;
			group_claw.execute(my_plan);
		}
	}

	SDL_Delay(1000);

	// place
	{
		test.attachObject();

		geometry_msgs::PoseStamped pose;
		pose.header.stamp = ros::Time::now();
		pose.header.frame_id = "base_link";
		pose.pose = tf2::toMsg(base_link_desire5);

		SDL_Log("base_link_desire2: %s", verbose_Isometry3d(base_link_desire2).c_str());

		std::vector<moveit_msgs::PlaceLocation> pls = group.posesToPlaceLocations({ pose });
		moveit_msgs::PlaceLocation& pl = pls.back();
		pl.pre_place_approach.direction.vector.x = 0.0;
		pl.pre_place_approach.direction.vector.y = 0.0;
		pl.pre_place_approach.direction.vector.z = 0.0;

		pl.post_place_retreat.direction.vector.x = 0.0;
		pl.post_place_retreat.direction.vector.y = 0.0;
		pl.post_place_retreat.direction.vector.z = 0.0;

		moveit_msgs::PlaceGoal goal = group.constructPlaceGoal("box1", pls, false);
		goal.place_eef = 1;
		moveit::core::MoveItErrorCode success = group.place(goal);
		if (success == moveit::core::MoveItErrorCode::SUCCESS) {
			int ii = 0;
		}
	}

	SDL_Delay(1000);

	{
		group_claw.setNamedTarget("claw_open");
		moveit::planning_interface::MoveGroupInterface::Plan my_plan;
		moveit::core::MoveItErrorCode success = group_claw.plan(my_plan);
		ROS_INFO("Visualizing plan 1 (pose goal) %s", success? "": "FAILED");   
		if (success == moveit::core::MoveItErrorCode::SUCCESS) {
			int ii = 0;
			group_claw.execute(my_plan);
		}
	}

	test.clear();
}

void tros_instance::do_joint_name_target(const std::string& group_name, const std::string& status)
{
	VALIDATE(is_moveit_model_valid() && !light_moveit_, null_str);

	ros::CallbackQueue cbqueue;
	ros::AsyncSpinner spinner(1, &cbqueue);
	spinner.start();
	ros::NodeHandle nh;
	nh.setCallbackQueue(&cbqueue);

	VALIDATE(moveit_node_started(), null_str);

	moveit::planning_interface::MoveGroupInterface::Options opt_claw(group_name, 
		"robot_description", nh);
	moveit::planning_interface::MoveGroupInterface group_claw(opt_claw);

	group_claw.setNamedTarget(status);
	moveit::planning_interface::MoveGroupInterface::Plan my_plan;
	moveit::core::MoveItErrorCode success = group_claw.plan(my_plan);
	ROS_INFO("Visualizing plan 1 (pose goal) %s", success? "": "FAILED");   
	if (success == moveit::core::MoveItErrorCode::SUCCESS) {
		group_claw.execute(my_plan);
	}
}

void tros_instance::do_joint_value_target(const std::string& group_name, const std::vector<double>& joint_values)
{
	VALIDATE(is_moveit_model_valid() && !light_moveit_, null_str);

	ros::CallbackQueue cbqueue;
	ros::AsyncSpinner spinner(1, &cbqueue);
	spinner.start();
	ros::NodeHandle nh;
	nh.setCallbackQueue(&cbqueue);

	VALIDATE(moveit_node_started(), null_str);

	moveit::planning_interface::MoveGroupInterface::Options opt_claw(group_name, 
		"robot_description", nh);
	moveit::planning_interface::MoveGroupInterface group_claw(opt_claw);

	group_claw.setJointValueTarget(joint_values);
	moveit::planning_interface::MoveGroupInterface::Plan my_plan;
	moveit::core::MoveItErrorCode success = group_claw.plan(my_plan);
	ROS_INFO("Visualizing plan 1 (pose goal) %s", success? "": "FAILED");   
	if (success == moveit::core::MoveItErrorCode::SUCCESS) {
		group_claw.execute(my_plan);
	}
}

void tros_instance::do_light_joint_value_target(const std::string& group_name, const std::vector<double>& joint_values)
{
	VALIDATE(is_moveit_model_valid(), null_str);

	moveit::tlight_interface moveit_light(moveit_model.model, *moveit_model.state.get(), rose_ros::pub_JointState);
	moveit_light.joint_value_target(group_name, joint_values);
}

void tros_instance::do_light_group_values_target(int state)
{
	const std::vector<aplt::tgroup_state>& states = moveit_driver_.get_common_state(state);

	for (std::vector<aplt::tgroup_state>::const_iterator it = states.begin(); it != states.end(); ++ it) {
		const aplt::tgroup_state& group_state = *it;
		// if (group_state.name == group_name_arm) {
		if (false) {
			VALIDATE(group_state.values.size() == 5, null_str);

			std::vector<double> values = group_state.values;
			double joint2 = values[1];

			double first_joint2 = joint2 > 0? 0.0: joint2;
			values[1] = first_joint2;
			do_light_joint_value_target(group_state.name, values);

			if (joint2 > 0) {
				values[1] = joint2;
				do_light_joint_value_target(group_state.name, values);
			}

		} else {
			do_light_joint_value_target(group_state.name, group_state.values);
		}
	}
}

void tros_instance::do_light_single_joint_target(const std::string& name, double value)
{
	VALIDATE(is_moveit_model_valid(), null_str);
	const aplt::tjoint_group_model* hit_group = nullptr;
	for (std::vector<aplt::tjoint_group_model*>::const_iterator it = moveit_model.aplt_model.joint_model_groups_.begin(); it != moveit_model.aplt_model.joint_model_groups_.end(); ++ it) {
		const aplt::tjoint_group_model* group_model = *it;
		if (group_model->variable_name_set_.count(name) != 0) {
			hit_group = group_model;
			break;
		}
	}
	VALIDATE(hit_group != nullptr, null_str);

	const aplt::tvariable_positions& aplt_position = aplt_positions_of_moveit_model();

	std::vector<double> joint_values;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = hit_group->variable_names_.begin(); it != hit_group->variable_names_.end(); ++ it, at ++) {
		const std::string& joint_name = *it;
		if (joint_name == name) {
			joint_values.push_back(value);
		} else {
			joint_values.push_back(aplt_position.positions[hit_group->variable_index_list_[at]]);
		}
	}

	moveit::tlight_interface moveit_light(moveit_model.model, *moveit_model.state.get(), rose_ros::pub_JointState);
	moveit_light.joint_value_target(hit_group->name_, joint_values);
}

std::vector<double> tros_instance::do_light_joint_pose_target(const std::string& group_name, const tpose3d& target_pose, const tpose3d& bounds, bool use_as_seed, bool verbose)
{
	VALIDATE(is_moveit_model_valid(), null_str);

	moveit::tlight_interface moveit_light(moveit_model.model, *moveit_model.state.get(), rose_ros::pub_JointState);
	std::vector<double> result = moveit_light.joint_pose_target(group_name, target_pose, bounds, use_as_seed);
	if (verbose) {
		if (!result.empty()) {
			geometry_msgs::Pose curr_fk = calculate_group_fk_tf(group_name, &result);
			SDL_Log("{dbg_ik}exe targetPose: (%.6f, %.6f, %.6f) curr_fk: (%.6f, %.6f, %.6f)", target_pose.x, target_pose.y, target_pose.z,
				curr_fk.position.x, curr_fk.position.y, curr_fk.position.z);
			SDL_Log("{dbg_ik}exe success. result: %s bounds(%.6f, %.6f, %.6f) => diff(%.6f, %.6f, %.6f)",
				vector_double_DebugString(result).c_str(), bounds.x, bounds.y, bounds.z,
				target_pose.x - curr_fk.position.x, target_pose.y - curr_fk.position.y, target_pose.z - curr_fk.position.z);
		} else {
			SDL_Log("{dbg_ik}exe fail");
		}
	}
	return result;
}

std::vector<double> tros_instance::get_group_joint_values(const std::string& group_name) const
{
	VALIDATE(is_moveit_model_valid(), null_str);
	VALIDATE(moveit_model.aplt_model.joint_model_group_map_.count(group_name), null_str);

	const aplt::tjoint_group_model* hit_group = moveit_model.aplt_model.joint_model_group_map_.find(group_name)->second;

	const aplt::tvariable_positions& aplt_position = aplt_positions_of_moveit_model();

	std::vector<double> joint_values;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = hit_group->variable_names_.begin(); it != hit_group->variable_names_.end(); ++ it, at ++) {
		const std::string& joint_name = *it;
		joint_values.push_back(aplt_position.positions[hit_group->variable_index_list_[at]]);
	}

	return joint_values;
}

double tros_instance::get_joint1_0degree() const
{
	VALIDATE(is_moveit_model_valid(), null_str);

	const std::vector<aplt::tgroup_state>& states = moveit_driver_.get_common_state(aplt::tmoveit_slot::state_recognize);
	double result = states[0].values[0];
	return result;
}

void tros_instance::do_soymilk()
{
	do_moveit_temporary();
}

void tros_instance::do_moveit_temporary()
{
	VALIDATE(is_moveit_model_valid() && !light_moveit_, null_str);
	// <marm_planning>/src/test_custom.cpp

	ros::CallbackQueue cbqueue;
	ros::AsyncSpinner spinner(1, &cbqueue);
	spinner.start();
	ros::NodeHandle nh;
	nh.setCallbackQueue(&cbqueue);

	{
		int ii = 0;
		// return;
	}

	// moveit::planning_interface::MoveGroupInterface::Options opt("robot_arm", 
	//	"robot_description", *rose_ros::nh.get());
	moveit::planning_interface::MoveGroupInterface::Options opt("robot_arm", 
		"robot_description", nh);
	moveit::planning_interface::MoveGroupInterface group(opt);

	group.setGoalPositionTolerance(0.000020); // default: 0.0001
    group.setGoalOrientationTolerance(0.00002); // default: 0.001
    group.setPlanningTime(60.0); // default: 5

	Eigen::Isometry3d base_link_desire1; // (0.5, 0.6, -0.7)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.381415, -0.000289008, 0.118199),
		Eigen::Vector3d(1.15452, -2.48807, 1.84256), base_link_desire1);

	Eigen::Isometry3d base_link_desire2; // (0, 0, 0)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.298735, -0.00028825, 0.293431),
		Eigen::Vector3d(2.34394, -3.13551, 1.57701), base_link_desire2);

	Eigen::Isometry3d base_link_desire3; // (0, 0, -0.7)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.298735, -0.00028825, 0.293431),
		Eigen::Vector3d(2.48028, -2.65714, 1.04367), base_link_desire3);

	Eigen::Isometry3d base_link_desire4; // (0, 0, 0.7)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.298735, -0.00028825, 0.293431),
		Eigen::Vector3d(2.47333, 2.66762, 2.10715), base_link_desire4);

	Eigen::Isometry3d base_link_desire5; // (0, 0.77, 0)
	ros::isometry3d_from_xyz_rpy(Eigen::Vector3d(0.35825, -0.00028, 0.20344),
		Eigen::Vector3d(1.57395, -3.14155, 1.57948), base_link_desire5);


	EigenSTL::vector_Isometry3d pose_targets;
	// pose_targets.push_back(base_link_desire1);
	// pose_targets.push_back(base_link_desire2);
	pose_targets.push_back(base_link_desire3);
	// pose_targets.push_back(base_link_desire4);
	// pose_targets.push_back(base_link_desire5);

	MoveGroupTestFixture test(group, cbqueue);
	test.ModifyPlanningSceneAsyncInterfaces();

	enum {type_joint, type_pose, type_cart, type_collision, type_grasps, type_place};
	const int type = type_pose;
	if (type == type_joint) {
		group.setNamedTarget("arm_init");
	} else if (type == type_pose) {
		geometry_msgs::Pose target_pose1;

		SDL_Log("pose_targets[0]: %s", verbose_Isometry3d(pose_targets[0]).c_str());
		// group.setPoseTarget(base_link_desire1);
		group.setPoseTargets(pose_targets);

	} else if (type == type_cart) {
		// code reference:
		//   <moveit_ros>/planning_interface/test/move_group_interface_cpp_test.cpp
		//   TEST_F(MoveGroupTestFixture, CartPathTest)

		// Plan from current pose
		const geometry_msgs::PoseStamped start_pose = group.getCurrentPose();

		std::vector<geometry_msgs::Pose> waypoints;
		waypoints.push_back(start_pose.pose);
/*
		geometry_msgs::Pose target_waypoint = start_pose.pose;
		target_waypoint.position.z -= 0.2;
		waypoints.push_back(target_waypoint);  // down

		target_waypoint.position.y -= 0.2;
		waypoints.push_back(target_waypoint);  // right

		target_waypoint.position.z += 0.2;
		target_waypoint.position.y += 0.2;
		target_waypoint.position.x -= 0.2;
		waypoints.push_back(target_waypoint);  // up and left
*/

		std::vector<geometry_msgs::PoseStamped> pose_out(pose_targets.size());
		ros::Time tm = ros::Time::now();
		const std::string& frame_id = group.getPoseReferenceFrame();
		for (std::size_t i = 0; i < pose_targets.size(); ++i) {
			pose_out[i].pose = tf2::toMsg(pose_targets[i]);
			pose_out[i].header.stamp = tm;
			pose_out[i].header.frame_id = frame_id;
			geometry_msgs::Pose pose = tf2::toMsg(pose_targets[i]);
			waypoints.push_back(pose);  // up and left
		}
		// waypoints.push_back(start_pose.pose);

		for (int i = 0; i < (int)waypoints.size(); ++ i) {
			const geometry_msgs::Pose& pose = waypoints[i];
			SDL_Log("[%i/%i]%s", (int)i, (int)pose_targets.size(), DebugString(pose).c_str());
		}


		moveit_msgs::RobotTrajectory trajectory;
		const auto jump_threshold = 0.0;
		// const auto eef_step = 0.01;
		const auto eef_step = 3;

		// test below is meaningless if Cartesian planning did not succeed
		double fraction = group.computeCartesianPath(waypoints, eef_step, jump_threshold, trajectory);

		// Execute trajectory
		if (fraction == 1.0) {
			group.execute(trajectory); // moveit::core::MoveItErrorCode::SUCCESS
		}

	} else if (type == type_collision) {
		// code reference:
		//   <moveit_ros>/planning_interface/test/move_group_interface_cpp_test.cpp
		//   TEST_F(MoveGroupTestFixture, ModifyPlanningSceneAsyncInterfaces)
		// MoveGroupTestFixture test(group, cbqueue);
		// test.ModifyPlanningSceneAsyncInterfaces();

	} else if (type == type_grasps) {
		// moveit::core::MoveItErrorCode success = group.planGraspsAndPick("box1");
		// moveit::core::MoveItErrorCode success = group.pick("box1", true);

		moveit_msgs::PickupGoal goal = group.constructPickupGoal("box1",
                           std::vector<moveit_msgs::Grasp>(), true);
		fillGrasps(goal, base_link_desire1);
		// fillGrasps(goal, base_link_desire2);
		goal.minimize_object_distance = 0;
		moveit::core::MoveItErrorCode success = group.pick(goal);
		if (success == moveit::core::MoveItErrorCode::SUCCESS) {
			int ii = 0;
		}

	} else if (type == type_place) {
		test.attachObject();

		geometry_msgs::PoseStamped pose;
		pose.header.stamp = ros::Time::now();
		pose.header.frame_id = "base_link";
		pose.pose = tf2::toMsg(base_link_desire2);

		SDL_Log("base_link_desire2: %s", verbose_Isometry3d(base_link_desire2).c_str());

		std::vector<moveit_msgs::PlaceLocation> pls = group.posesToPlaceLocations({ pose });
		moveit_msgs::PlaceLocation& pl = pls.back();
		pl.pre_place_approach.direction.vector.x = 0.0;
		pl.pre_place_approach.direction.vector.y = 0.0;
		pl.pre_place_approach.direction.vector.z = 0.0;

		pl.post_place_retreat.direction.vector.x = 0.0;
		pl.post_place_retreat.direction.vector.y = 0.0;
		pl.post_place_retreat.direction.vector.z = 0.0;

		moveit_msgs::PlaceGoal goal = group.constructPlaceGoal("box1", pls, true);
		goal.place_eef = 1;
		moveit::core::MoveItErrorCode success = group.place(goal);
		if (success == moveit::core::MoveItErrorCode::SUCCESS) {
			int ii = 0;
		}
	}

	if (type == type_joint || type == type_pose) {
		moveit::planning_interface::MoveGroupInterface::Plan my_plan;
		moveit::core::MoveItErrorCode success = group.plan(my_plan);
		ROS_INFO("Visualizing plan 1 (pose goal) %s", success? "": "FAILED");   
		if (success == moveit::core::MoveItErrorCode::SUCCESS) {
			int ii = 0;
			group.execute(my_plan);
		}
	}
}


bool tros_instance::exploration_detect(const tpose2d& robot_pose, geometry_msgs::Point& unk)
{
	return node_wrapper_.exploration_detect(robot_pose, unk);
}

static void mapToWorld(const ::nav_msgs::MapMetaData& info, unsigned int mx, unsigned int my, double& wx, double& wy)
{
    double origin_x_ = info.origin.position.x;
    double origin_y_ = info.origin.position.y;
    double resolution_ = info.resolution;

  wx = origin_x_ + (mx + 0.5) * resolution_;
  wy = origin_y_ + (my + 0.5) * resolution_;
}

static bool worldToMap(const ::nav_msgs::MapMetaData& info, 
    double wx, double wy, unsigned int& mx, unsigned int& my)
{
    double origin_x_ = info.origin.position.x;
    double origin_y_ = info.origin.position.y;
    unsigned int size_x_ = info.width;
    unsigned int size_y_ = info.height;
    double resolution_ = info.resolution;

    if (wx < origin_x_ || wy < origin_y_)
        return false;

    mx = (int)((wx - origin_x_) / resolution_);
    my = (int)((wy - origin_y_) / resolution_);

    if (mx < size_x_ && my < size_y_) {
        return true;
    }

    return false;
}

static void worldToMapNoBounds(double origin_x_, double origin_y_, double resolution_, double wx, double wy, int& mx, int& my)
{
    mx = (int)((wx - origin_x_) / resolution_);
    my = (int)((wy - origin_y_) / resolution_);
}

#define getIndex(size_x_, mx, my)	(my) * (size_x_) + (mx)
#define MAX_UNKNOWN_CELLS	20	// Close the door when bulidmap, and open the door when navigating?

struct tes_point {
	int x;
	int y;
	bool ep; // true: end-point, false: middle-point
};

static int cauclate_match_score_negative(int origin_x, int origin_y, const ::nav_msgs::MapMetaData& info, 
	const int8_t* data, const tes_point* points, int point_count, int& unknown_cells, int& mp_score, int best_score)
{
    const int size_x_ = info.width;
    const int size_y_ = info.height;

	unknown_cells = 0;
	mp_score = 0;
	int8_t origin_value = data[getIndex(info.width, origin_x, origin_y)];
    if (origin_value == (int8_t)costmap_2d::NO_INFORMATION || origin_value >= NEAR_OBSTACLE) {
        // origin is UNKNOWN or NEAR_OBSTACLE
        return INT32_MAX;
    }

	// int unknown_cell_cost = MAX_CARTOGRAPHER_CELL_VAL * 10;
	int unknown_cell_cost = MAX_CARTOGRAPHER_CELL_VAL * 20;

    int score = 0;
    for (int at = 0; at < point_count; at ++) {
        const tes_point& point = points[at];

        int new_x = origin_x + points[at].x;
        int new_y = origin_y + points[at].y;
        if (new_x < 0 || new_x >= size_x_) {
			// There is kPaddingPixel(5) protection, think will only be limited to the map.
            return INT32_MAX;
        }
        if (new_y < 0 || new_y >= size_y_) {
			// There is kPaddingPixel(5) protection, think will only be limited to the map.
            return INT32_MAX;
        }
        int value = data[getIndex(info.width, new_x, new_y)];
        if (value == (int8_t)costmap_2d::NO_INFORMATION) {
			unknown_cells ++;
			if (unknown_cells > MAX_UNKNOWN_CELLS) {
				return INT32_MAX;
			}
            value = unknown_cell_cost;

        } else if (point.ep) {
			value = MAX_CARTOGRAPHER_CELL_VAL - value;

		} else {
			// middle-point, more little more good
			mp_score += value;
		}

        score += value;
		if (score > best_score) {
			return score;
		}
    }
    return score;
}

static const int angle_radius_360 = 360;
#define MAX_YAW_COUNT	360 // 41, 720

class treposition_range
{
public:
    treposition_range(const ::nav_msgs::MapMetaData& info, int radius_cm, double center_x, double center_y, int angle_radius, int angle_granularity)
        : map_rect({nposm, nposm, 0, 0})
        // , angle_radius(_angle_radius)
        // , angle_granularity(_angle_granularity)
        , yaw_count(1)
    {
        // translation
        if (radius_cm > 0) {
            unsigned int center_map_x;
            unsigned int center_map_y;
            bool valid = worldToMap(info, center_x, center_y, center_map_x, center_map_y);
            if (valid) {
                int cells = ceil(0.01 * radius_cm / info.resolution);
                map_rect.x = center_map_x - cells;
                map_rect.y = center_map_y - cells;
                map_rect.w = cells * 2;
                map_rect.h = cells * 2;
                if (map_rect.x < 0) {
                    map_rect.w += map_rect.x;
                    map_rect.x = 0;
                }
                if (map_rect.y < 0) {
                    map_rect.h += map_rect.y;
                    map_rect.y = 0;
                }
                const int size_x = info.width;
                const int size_y = info.height;
                if (map_rect.x + map_rect.w > size_x) {
                    map_rect.w = size_x - map_rect.x;
                }
                if (map_rect.y + map_rect.h > size_y) {
                    map_rect.h = size_y - map_rect.y;
                }
            }

        } else {
            VALIDATE(radius_cm == 0, null_str);
        }

        // angle
        memset(yaw_offset, 0, sizeof(yaw_offset));
        if (angle_radius > 0) {
            VALIDATE(angle_granularity > 0, null_str);
			int steps = posix_pages(angle_radius, angle_granularity);
			if (angle_radius != angle_radius_360) {
				yaw_count = steps * 2 + 1;
				VALIDATE(yaw_count <= MAX_YAW_COUNT, null_str);
				int at = 0;
				for (int step = steps; step > 0; step --, at ++) {
					yaw_offset[at] = DEG2RAD(-1 * step * angle_granularity);
					if (step != 0) {
						yaw_offset[yaw_count - at - 1] = DEG2RAD(step * angle_granularity);
					}
				}
			} else {
				yaw_count = steps;
				VALIDATE(yaw_count <= MAX_YAW_COUNT, null_str);
				for (int step = 0; step < steps; step ++) {
					yaw_offset[step] = DEG2RAD(step * angle_granularity);
				}
			}

        } else {
            VALIDATE(angle_radius == 0, null_str);
            VALIDATE(angle_granularity == nposm, null_str);
        }
    }

	std::string to_string() const
	{
		char buf[256];
		SDL_snprintf(buf, sizeof(buf), "{map_rect:(%i, %i, %i, %i), yaw: %i[%.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f, %.3f]}",
			map_rect.x, map_rect.y, map_rect.w, map_rect.h, yaw_count,
			RAD2DEG(yaw_offset[0]), RAD2DEG(yaw_offset[1]), RAD2DEG(yaw_offset[2]), RAD2DEG(yaw_offset[3]),
			RAD2DEG(yaw_offset[4]), RAD2DEG(yaw_offset[5]), RAD2DEG(yaw_offset[6]), RAD2DEG(yaw_offset[7]),
			RAD2DEG(yaw_offset[8]), RAD2DEG(yaw_offset[9]), RAD2DEG(yaw_offset[10]), RAD2DEG(yaw_offset[11]),
			RAD2DEG(yaw_offset[12]), RAD2DEG(yaw_offset[13]), RAD2DEG(yaw_offset[14]), RAD2DEG(yaw_offset[15]),
			RAD2DEG(yaw_offset[16]), RAD2DEG(yaw_offset[17]), RAD2DEG(yaw_offset[18]), RAD2DEG(yaw_offset[19]),
			RAD2DEG(yaw_offset[20]));
		return buf;
	}

public:
    SDL_Rect map_rect;

    // const int angle_radius; // unit: degree
    // const int angle_granularity; // unit: degree

    double yaw_offset[MAX_YAW_COUNT]; // = {DEG2RAD(-2), DEG2RAD(-1), 0, DEG2RAD(1), DEG2RAD(2)}
    int yaw_count; // = sizeof(yaw_offset) / sizeof(yaw_offset[0]);
};

class tint_topN
{
public:
	struct titem {
		int score;
		double pose2d_x;
		double pose2d_y;
		double pose2d_yaw;
		int unknowns;
		int points;
		int origin_x;
		int origin_y;
		int yaw_idx;
	};

	tint_topN(int _N, bool _ascending, int _item_bytes)
		: N(_N)
		, ascending(_ascending)
		, item_bytes(_item_bytes)
		, candiates(nullptr)
	{
		VALIDATE(N > 0, null_str);
		candiates = (titem*)malloc(N * item_bytes);
		int initial = ascending? INT32_MAX: INT32_MIN;
		for (int at = 0; at < N; at ++) {
			candiates[at].score = initial;
		}
	}

	~tint_topN()
	{
		if (candiates != nullptr) {
			free(candiates);
		}
	}

	void insert(const titem& item)
	{
		if (ascending) {
			if (item.score > candiates[N - 1].score) {
				return;
			}

			if (item.score != INT32_MAX) {
				int ii = 0;
			}

			for (int at = 0; at < N; at ++) {
				if (item.score <= candiates[at].score) {
					int n = N - at - 1;
					if (n > 0) {
						memmove(candiates + at + 1, candiates + at, n * item_bytes);
					}
					memcpy(&candiates[at], &item, item_bytes);
					return;
				}
			}
		} else {
			VALIDATE(false, "not support");
		}
	}

public:
	int N;
	bool ascending;
	int item_bytes;
	titem* candiates;
};

#define MOVEIT_SECTOR_DEFAULT_RANGE		8.543210f

static void reposition_use_laser_scan(const sensor_msgs::LaserScan& scan_msg,
    const ::nav_msgs::MapMetaData& info, const int8_t* data, double imu_yaw, const treposition_range& range, 
	tpose2d& result_pose, tkidnap::tresult4_C* result3_ptr)
{
    VALIDATE(scan_msg.range_min >= 0.f, null_str);
    VALIDATE(scan_msg.range_max >= scan_msg.range_min, null_str);
    if (scan_msg.angle_increment > 0.f) {
        VALIDATE(scan_msg.angle_max > scan_msg.angle_min, null_str);
    } else {
        VALIDATE(scan_msg.angle_min > scan_msg.angle_max, null_str);
    }

	if (game_config::os == os_windows) {
		SDL_Log("%u reposition_use_laser_scan, imu_yaw: %.3f, map_size: (%i x %i), range: %s", 
			SDL_GetTicks(), RAD2DEG(imu_yaw), (int)info.width, (int)info.height, range.to_string().c_str());
	}

    struct tyaw {
        tyaw(double yaw, const geometry_msgs::TransformStamped& transform, int data_offset)
            : yaw(yaw)
            , transform(transform)
            , data_offset(data_offset)
        {}

        const double yaw;
        const geometry_msgs::TransformStamped transform;
        const int data_offset;
    };
    std::vector<tyaw> yaws;

	uint32_t start_ticks = SDL_GetTicks();
    // (1/3)fill yaw except for tyaw.data_offset
	const double laser_shielded_angle_min = DEG2RAD(-MOVEIT_SECTOR_HALF_ANGLE_DEG);
	const double laser_shielded_angle_max = DEG2RAD(MOVEIT_SECTOR_HALF_ANGLE_DEG);

	int min_shielded_range_at = (laser_shielded_angle_min - scan_msg.angle_min) / scan_msg.angle_increment;
	int max_shielded_range_at = (laser_shielded_angle_max - scan_msg.angle_min) / scan_msg.angle_increment;

	const int scan_msg_range_size = scan_msg.ranges.size();
	struct tangle_range {
		float angle;
		float echo;
	};
	tangle_range* angle_ranges = (tangle_range*)malloc(scan_msg_range_size * sizeof(tangle_range) * 2);
	memset(angle_ranges, 0, scan_msg_range_size * sizeof(tangle_range) * 2);

	int ep_count = 0;
	int mid_count = 0;
	const float max_has_mid_range = 10.0; // 10.0m
	const float min_has_2mid_range = 67.5; // 1.5m, now don't insert 2 point.   
	const float min_has_1mid_range = 1.0; // 1.0m
	float angle = scan_msg.angle_min;

    for (int idx = 0; idx < scan_msg_range_size; ++ idx, angle += scan_msg.angle_increment) {
        const float echo = scan_msg.ranges[idx];

        if (scan_msg.range_min <= echo && echo <= scan_msg.range_max) {
			if (KDL::Equal(echo, MOVEIT_SECTOR_DEFAULT_RANGE)) {
				// VALIDATE(shielded, null_str);
				continue;
			} else {
				// VALIDATE(!shielded, null_str);
			}
			tangle_range& angle_range = angle_ranges[ep_count];
			angle_range.angle = angle;
			angle_range.echo = echo;

			ep_count ++;

			int mids = 0;
			if (echo > max_has_mid_range) {
				// i think too far result accuracy too poor.

			} else if (echo >= min_has_2mid_range) {
				mids = 2;

			} else if (echo >= min_has_1mid_range) {
				mids = 1;
			}

			for (int n = 0; n < mids && mid_count < scan_msg_range_size; n ++) {
				tangle_range& angle_range = angle_ranges[scan_msg_range_size + mid_count];
				angle_range.angle = angle;
				if (mids == 1) {
					angle_range.echo = echo / 2;

				} else {
					VALIDATE(false, null_str);
					VALIDATE(mids == 2, null_str);
					if (n == 0) {
						angle_range.echo = echo / 3;
					} else {
						angle_range.echo = echo * 2 / 3;
					}
				}

				mid_count ++;
			}
		}
	}

	const int desire_point_count = 550; // 550
	int fills = 0;
	if (ep_count < desire_point_count && mid_count > 0) {
		fills = desire_point_count - ep_count;
		fills = SDL_min(fills, mid_count);
		memmove(angle_ranges + ep_count, angle_ranges + scan_msg_range_size, fills * sizeof(tangle_range));
	}

	// fills = 0;
	const int point_count = ep_count + fills;

    for (int yaw_idx = 0; yaw_idx < range.yaw_count; yaw_idx ++) {
        geometry_msgs::TransformStamped transform;
        transform.transform.translation.x = 0;
        transform.transform.translation.y = 0;
        transform.transform.translation.z = 0;

        tf2::Quaternion q;
        double yaw = imu_yaw + range.yaw_offset[yaw_idx];
        q.setRPY(0, 0, yaw);

        transform.transform.rotation.x = q.x();
        transform.transform.rotation.y = q.y();
        transform.transform.rotation.z = q.z();
        transform.transform.rotation.w = q.w();
        yaws.push_back(tyaw(yaw, transform, yaw_idx * point_count));
    }
    const tyaw* yaws_ptr[MAX_YAW_COUNT]; // = [range.yaw_count]; but must use const
    for (int yaw_idx = 0; yaw_idx < range.yaw_count; yaw_idx ++) {
        yaws_ptr[yaw_idx] = &yaws[yaw_idx];
    }

	uint32_t start_step1_fill_points = SDL_GetTicks();

    // (2/3) fill points and fill tyaw.data_offset
	tes_point* points = (tes_point*)malloc(point_count * sizeof(tes_point) * range.yaw_count);

	int map_x;
    int map_y;
    for (int at = 0; at < point_count; at ++) {
        const tangle_range& angle_range = angle_ranges[at];
		float angle = angle_range.angle;
		float echo = angle_range.echo;

        float dx = cos(angle) * echo;
        float dy = sin(angle) * echo;

        geometry_msgs::Pose t_in;
        tf2::toMsg(tf2::Transform::getIdentity(), t_in);
        t_in.position.x = dx;
        t_in.position.y = dy;

        tf2::Quaternion r;
		r.setRPY(0, 0, angle);
        t_in.orientation.x = r.x();
        t_in.orientation.y = r.y();
        t_in.orientation.z = r.z();
        t_in.orientation.w = r.w();

		geometry_msgs::Pose t_out;
        for (int yaw_idx = 0; yaw_idx < range.yaw_count; yaw_idx ++) {
            const tyaw& yaw = *yaws_ptr[yaw_idx];

			tf2::doTransform(t_in, t_out, yaw.transform);

			float dx2 = t_out.position.x;
			float dy2 = t_out.position.y;

			worldToMapNoBounds(0, 0, info.resolution, dx2, dy2, map_x, map_y);

            points[yaw.data_offset + at] = tes_point{map_x, map_y, at < ep_count};
        }
    }

	uint32_t start_step2_best_score = SDL_GetTicks();
    // (3/3) get best match score
	tint_topN topN(50, true, sizeof(tint_topN::titem));
	tint_topN::titem item;
	// const bool dbg_topN = game_config::os == os_windows;
	const bool dbg_topN = false;

	int best_score = INT32_MAX;
    SDL_Point best_origin = {nposm, nposm};
    int best_yaw_idx = nposm;
	int best_unknown_cells = 0;

    SDL_Rect map_rect{0, 0, (int)info.width, (int)info.height};
    if (range.map_rect.x != nposm) {
        VALIDATE(range.map_rect.x >= 0 && range.map_rect.w > 0, null_str);
        VALIDATE(range.map_rect.y >= 0 && range.map_rect.h > 0, null_str);
        map_rect = range.map_rect;
    }

	// when release, special_count must be 0. Otherwise, it will take more time.
	// If you set special_count not 0, you should know what you're doing.
	// both x and y is 'map' coordinate, not 'world'.
	
	// laser_scan-full2-unkcells-topic-scan.dat
	const tpose2d_C specials[] = {tpose2d_C{80, 64, 1.343903}, tpose2d_C{45, 134, 4.537856}};
	
	// laser_scan-full2-unkcells-50-92.dat
	// const tpose2d_C specials[] = {tpose2d_C{50, 92, 0.680678}, tpose2d_C{35, 239, 5.323254}};

	// const int special_count = sizeof(specials) / sizeof(specials[0]);
	const int special_count = 0;


    const int end_x = map_rect.x + map_rect.w;
    const int end_y = map_rect.y + map_rect.h;
    for (int yaw_idx = 0; yaw_idx < range.yaw_count; yaw_idx ++) {
        const tyaw& yaw = *yaws_ptr[yaw_idx];

        for (int origin_y = map_rect.y; origin_y < end_y; origin_y ++) {
		    for (int origin_x = map_rect.x; origin_x < end_x; origin_x ++) {
            	
				int unknown_cells = 0;
				int mp_score = 0;
                int score = cauclate_match_score_negative(origin_x, origin_y, info, data, points + yaw.data_offset, point_count, unknown_cells, mp_score, dbg_topN? INT32_MAX: best_score);
				for (int si = 0; si < special_count; si ++) {
					const tpose2d_C& special = specials[si];
					if (KDL::Equal(special.x, origin_x, 0.001) && KDL::Equal(special.y, origin_y, 0.001) && KDL::Equal(special.yaw, yaws_ptr[yaw_idx]->yaw, 0.001)) {
						SDL_Log("[special]fills/point: %i/%i score: %i(best_score: %i) origin:(%i, %i) yaw:%.6f(deg:%.3f)] unknow: %i mp_score: %i yaw_idx: %i", 
							fills, point_count, score, best_score,
							origin_x, origin_y, yaws_ptr[yaw_idx]->yaw, RAD2DEG(yaws_ptr[yaw_idx]->yaw),
							unknown_cells, mp_score, yaw_idx);
						if (si == 0) {
							// score = 100;
						}
					}
				}

				if (dbg_topN) {
					item.score = score;
					item.pose2d_yaw = yaws_ptr[yaw_idx]->yaw;
					item.origin_x = origin_x;
					item.origin_y = origin_y;
					item.unknowns = unknown_cells;
					item.points = point_count;
					item.yaw_idx = yaw_idx;
					topN.insert(item);
				}
                if (score < best_score) {
                    best_score = score;
                    best_origin.x = origin_x;
                    best_origin.y = origin_y;
                    best_yaw_idx = yaw_idx;
					best_unknown_cells = unknown_cells;
                }
		    }
        }
    }

	const uint32_t end_ticks = SDL_GetTicks();
    if (best_yaw_idx != nposm) {
		double world_x;
		double world_y;
		mapToWorld(info, best_origin.x, best_origin.y, world_x, world_y);
		SDL_Log("%u {kestimate}---------->spend %u(%u+%u+%u) ms, pose2d[%.3f, %.3f, yaw:%.6f(deg:%.3f)] origin:(%i, %i) score(%i) unknown_cells(%i) point_count(%i) fills(%i)", 
			end_ticks, end_ticks - start_ticks, start_step1_fill_points - start_ticks, 
			start_step2_best_score - start_step1_fill_points, end_ticks - start_step2_best_score,
			world_x, world_y, yaws_ptr[best_yaw_idx]->yaw, RAD2DEG(yaws_ptr[best_yaw_idx]->yaw), best_origin.x, best_origin.y,
			best_score, best_unknown_cells, point_count, fills);

		if (dbg_topN) {
			for (int at = 0; at < topN.N; at ++) {
				tint_topN::titem& candiate = topN.candiates[at];
				mapToWorld(info, candiate.origin_x, candiate.origin_y, candiate.pose2d_x, candiate.pose2d_y);
				SDL_Log("[%i/%i]score: %i pose2d[%.3f, %.3f, yaw:%.6f(deg:%.3f)] origin:(%i, %i) unknow: %i yaw_idx: %i", at, topN.N, candiate.score,
					candiate.pose2d_x, candiate.pose2d_y, candiate.pose2d_yaw, RAD2DEG(angles::normalize_angle(candiate.pose2d_yaw)),
					candiate.origin_x, candiate.origin_y, candiate.unknowns, candiate.yaw_idx);
			}
		}

		result_pose.x = world_x;
		result_pose.y = world_y;
		result_pose.yaw = yaws_ptr[best_yaw_idx]->yaw;
		result_pose.valid = best_score != INT32_MAX; // best_score >= 0;

	} else {
		// If there are too many 'UNKNOWN' cells, cauclate_match_score() is likely to get a negative value. 
		// If too many 'UNKNOWN' cells for each possible position within this range, the best_yaw_idx may always be -1. 
		// -1 does not mean that the cauclate_match_score() has not been called, 
		// but all candidate points will not meet the requirements.
		SDL_Log("{kestimate}reposition_use_laser_scan, cannot find a valid pose2d");
		VALIDATE(best_score == INT32_MAX, null_str);
        result_pose.x = info.origin.position.x;
        result_pose.y = info.origin.position.y;
        result_pose.yaw = imu_yaw;
		result_pose.valid = false;
	}

	if (result3_ptr != nullptr) {
		result3_ptr->ms = end_ticks - start_ticks;
		result3_ptr->score = best_score;
		result3_ptr->points = point_count;
		result3_ptr->unknowns = best_unknown_cells;
	}

    const bool save_png = false;
    if (save_png && result_pose.valid) {
        unsigned int origin_map_x;
        unsigned int origin_map_y;
        bool valid = worldToMap(info, 0, 0, origin_map_x, origin_map_y);
        const int size_x = info.width;
        const int size_y = info.height;

        int pos = 0;
        SDL_Surface* photo = SDL_CreateRGBSurface(0, info.width, info.height, 4 * 8,
			    0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
	    uint32_t* pixels = (uint32_t*)photo->pixels;
        memset(pixels, 0, info.width * info.height);
        for (unsigned int y = 0; y < info.height; y++) {
		    for(unsigned int x = 0; x < info.width; x++) {
			    unsigned int i = x + (info.height - y - 1) * info.width;
                int raw_u8 = data[i];
                if (raw_u8 < 0) {
                    VALIDATE(raw_u8 == -1, null_str);
                    pixels[pos ++] = 0xffffffff;
                } else {
                    uint8_t u8 = raw_u8;
                    uint32_t val = posix_mku32(posix_mku16(u8, u8), posix_mku16(u8, 0xff));
                    pixels[pos ++] = val;
                }
		    }
        }
        VALIDATE(pos == info.width * info.height, null_str);
        pixels[origin_map_x + (size_y - origin_map_y - 1) * size_x] = 0xffffff00; // map's origin

		pixels[best_origin.x + (size_y - best_origin.y - 1) * size_x] = 0xff0000ff; // laser's origin

		std::set<int64_t> green_set;
		std::set<int64_t> blue_set;
        for (int at = 0; at < point_count; at ++) {
            int data_index = yaws_ptr[best_yaw_idx]->data_offset + at;
            const tes_point& point = points[data_index];
            int new_x = best_origin.x + points[data_index].x;
            int new_y = best_origin.y + points[data_index].y;
            if (new_x < 0 || new_x >= size_x) {
                continue;
            }
            if (new_y < 0 || new_y >= size_y) {
                continue;
            }
			if (point.ep) {
				pixels[new_x + (size_y - new_y - 1) * size_x] = 0xffff0000;
				blue_set.insert(posix_mki64(new_x, new_y));
			} else {
				pixels[new_x + (size_y - new_y - 1) * size_x] = 0xff00ff00;
				green_set.insert(posix_mki64(new_x, new_y));
			}
        }

        std::string file = game_config::preferences_dir + "/1.png";
        const char* c_str = file.c_str();
        IMG_SavePNG(photo, c_str);
        SDL_FreeSurface(photo);

		surface cell_value_surf = u8_data_2_cell_value_surf((const uint8_t*)data, 4, 4, info.width, info.height, maptype_OccupancyGrid, &green_set, &blue_set);
        imwrite(cell_value_surf, "1_0-cell_value.png");
    }

    free(points);
	free(angle_ranges);
}

tpose2d full2_reposition(const sensor_msgs::LaserScan& scan_msg, const ::nav_msgs::MapMetaData& info, const int8_t* map_data, tkidnap::tresult4_C& result4)
{
	SDL_Log("%u {kestimate}full2_reposition... #%i", SDL_GetTicks(), kidnap.moved_times);

	double center_x = 0.0;
    double center_y = 0.0;
    int radius_cm = 0;

    int angle_granularity = 1;

	treposition_range range(info, radius_cm, center_x, center_y, angle_radius_360, angle_granularity);
	tpose2d result_pose;
	// for full2 reposition, offset in range is final value.
	const double imu_yaw = 0;
    reposition_use_laser_scan(scan_msg, info, map_data, imu_yaw, range, result_pose, &result4);
	kidnap.add_dbg_msg_reposition(tkidnap::msg_reposition, tkidnap::reposition_full2, 
		tpose2d_C{result_pose.x, result_pose.y, result_pose.yaw, result_pose.valid}, result4);

	if (game_config::os == os_windows) {
		// save_laser_scan(scan_msg, tpose2d(0, 0, imu_yaw), result4, result_pose, "laser_scan-full2-unkcells.dat");
	}

	return result_pose;
}

tpose2d full_map_reposition(double imu_yaw, const sensor_msgs::LaserScan& scan_msg, const ::nav_msgs::MapMetaData& info, const int8_t* map_data, tkidnap::tresult4_C& result4)
{
	SDL_Log("%u {kestimate}full_map_reposition... #%i imu_yaw: %.3f", 
		SDL_GetTicks(), kidnap.moved_times, RAD2DEG(imu_yaw));

	double center_x = 0.0;
    double center_y = 0.0;
    int radius_cm = 0;

	int angle_radius = 25;
    int angle_granularity = 1;

	treposition_range range(info, radius_cm, center_x, center_y, angle_radius, angle_granularity);
	tpose2d result_pose;
    reposition_use_laser_scan(scan_msg, info, map_data, imu_yaw, range, result_pose, &result4);
	kidnap.add_dbg_msg_reposition(tkidnap::msg_reposition, tkidnap::reposition_full, 
		tpose2d_C{result_pose.x, result_pose.y, result_pose.yaw, result_pose.valid}, result4);
	return result_pose;
}

#define REPOSITION_LEVEL0_RADIUS_CM		200 // 1)last navigation's result. 2)runtime robot pose.
#define REPOSITION_COMPARE_CM		10
tpose2d center_reposition(const tpose2d& center,const sensor_msgs::LaserScan& scan_msg, const ::nav_msgs::MapMetaData& info, const int8_t* map_data, tkidnap::tresult4_C& result4)
{
	double center_x = center.x;
    double center_y = center.y;
    int radius_cm = REPOSITION_LEVEL0_RADIUS_CM; // 200cm

    int angle_radius = 25;
    int angle_granularity = 1;

	SDL_Log("%u {kestimate}center_reposition... #%i center: %s radius_cm: %i", 
		SDL_GetTicks(), kidnap.moved_times, center.to_string().c_str(), radius_cm);

	treposition_range range(info, radius_cm, center_x, center_y, angle_radius, angle_granularity);
	tpose2d result_pose;
    reposition_use_laser_scan(scan_msg, info, map_data, center.yaw, range, result_pose, &result4);
	kidnap.add_dbg_msg_reposition(tkidnap::msg_reposition, tkidnap::reposition_center, 
		tpose2d_C{result_pose.x, result_pose.y, result_pose.yaw, result_pose.valid}, result4);
	return result_pose;
}

const trpy& tros_instance::get_imu_rpy() const
{
	trpy& tmp = robot_imu_.tmp;
	tmp.valid = false;

	double result = 0;
	if (IN_MAIN_THREAD()) {
		// call it to refresh, and make aplt::valuex.euler[2] as the latest value,
		base_driver_.slice();
	}
	if (base_driver_.imu_can_read()) {
		double imu_euler0 = aplt::valuex.euler[0];
		double imu_euler1 = aplt::valuex.euler[1];
		double imu_euler2 = aplt::valuex.euler[2];

		tmp.valid = true;
		if (!robot_imu_.has_magnetometer) {
			// if (robot_imu_.last_ticks != 0) {
				result = robot_imu_.base.roll + imu_euler0;
				tmp.roll = angles::normalize_angle(result);

				VALIDATE(robot_imu_.base.pitch == 0, null_str);
				result = robot_imu_.base.pitch + imu_euler1;
				// tmp.pitch = angles::normalize_angle(result);
				tmp.pitch = result;

				result = robot_imu_.base.yaw + imu_euler2;
				tmp.yaw = angles::normalize_angle(result);
			// }
		} else {
			tmp.roll = imu_euler0;
			tmp.pitch = imu_euler1;
			tmp.yaw = imu_euler2;
		}
	}

	return tmp;
}

double tros_instance::get_imu_yaw(bool* valid_ptr) const
{
	const trpy& tmp = get_imu_rpy();
	if (valid_ptr != nullptr) {
		*valid_ptr = tmp.valid;
	}

	return tmp.yaw;
}

#define MAX_IMU_SAMPLES		5

double tros_instance::get_stable_imu(int rpy, bool verbose, bool* fail_ptr) const
{
	VALIDATE(rpy == rpy_pitch || rpy == rpy_yaw, null_str);

	const char rpy_name[][12] = {"roll", "pitch", "yaw"};

	int next_sample_index = 0;
	double samples[MAX_IMU_SAMPLES];
	// must not use 0. current pitch maybe is 0. I don't think there will be a 9999
	// memset(samples, 0, sizeof(samples));
	for (int at = 0; at < MAX_IMU_SAMPLES; at ++) {
		samples[at] = float_nposm;
	}

	uint32_t start_ticks = SDL_GetTicks();
	const int threshold = 5000; // 10 seconds
	while ((int)(SDL_GetTicks() - start_ticks) < threshold) {
		if (rpy == rpy_pitch) {
			samples[next_sample_index] = get_imu_rpy().pitch;
		} else {
			// rpy_yaw
			samples[next_sample_index] = get_imu_rpy().yaw;
		}

		if (verbose) {
			SDL_Log("-----%u {get_stable_imu}next_sample_index: %i---", SDL_GetTicks(), next_sample_index);
			for (int at = 0; at < MAX_IMU_SAMPLES; at ++) {
				const double dist = samples[at];
				SDL_Log("[%i/%i]value: %.6f(deg:%.4f)", at, MAX_IMU_SAMPLES, samples[at], RAD2DEG(samples[at]));
			}
			SDL_Log("--------");
		}

		bool satisfied = true;
		const double threshold = DEG2RAD(0.3);
		for (int at1 = 0; at1 < MAX_IMU_SAMPLES && satisfied; at1 ++) {
			const double value1 = samples[at1];
			for (int at2 = at1 + 1; at2 < MAX_IMU_SAMPLES; at2 ++) {
				const double value2 = samples[at2];

				double diff = fabs(value1 - value2);
				if (verbose) {
					SDL_Log("get_stable_imu: [%i] - [%i]: %.4f(deg:%.4f)", at1, at2, diff, RAD2DEG(diff));
				}
				if (diff > threshold) {
					if (verbose) {
						SDL_Log("get_stable_imu: [%i] - [%i]: %.4f(deg:%.4f) > threshold(%.6f), return false", 
							at1, at2, diff, RAD2DEG(diff), threshold);
					}
					satisfied = false;
					break;
				}
			}
		}

		if (satisfied) {
			SDL_Log("get_stable_imu[%s] ok, elapse %i ms. %.6f(deg: %.3f)", rpy_name[rpy], (int)(SDL_GetTicks() - start_ticks), samples[next_sample_index], RAD2DEG(samples[next_sample_index]));
			if (fail_ptr != nullptr) {
				*fail_ptr = true;
			}
			return samples[next_sample_index];
		}

		// I think the interval will not exceed 100 milliseconds at most.
		const int ms = 120;
		SDL_Delay(ms);

		next_sample_index ++;
		next_sample_index %= MAX_IMU_SAMPLES;
	}

	if (fail_ptr != nullptr) {
		*fail_ptr = false;
	}
	SDL_Log("get_stable_imu[%s] fail, elapse %i ms, time overflow.", rpy_name[rpy], (int)(SDL_GetTicks() - start_ticks));
	int index = next_sample_index > 0? next_sample_index - 1: MAX_IMU_SAMPLES - 1;
	return samples[index];
}

std::string tros_instance::get_imu_desc() const
{
	char euler_z[64] = {'-', '-'};
	const trpy& rpy = get_imu_rpy();
    if (rpy.valid) {
        if (robot_imu_.has_magnetometer) {
            // SDL_snprintf(euler_z, sizeof(euler_z), "%.3f", RAD2DEG(rpy.yaw));
            SDL_snprintf(euler_z, sizeof(euler_z), "%.3f %.3f/H:%.3f", 
				RAD2DEG(rpy.pitch), RAD2DEG(rpy.yaw), RAD2DEG(aplt::valuex.euler[2]));
        } else {
            SDL_snprintf(euler_z, sizeof(euler_z), "%.3f %.3f/H:%.3f", 
				RAD2DEG(rpy.pitch), RAD2DEG(rpy.yaw), RAD2DEG(aplt::valuex.euler[2]));
        }
    } else {
        SDL_strlcpy(euler_z, "No IMU", sizeof(euler_z));
    }

	return euler_z;
}

void tros_instance::kidnap_slice(const sensor_msgs::LaserScan& scan_msg)
{
	if (!move_base_constructed_ || !kidnap.estimating()) {
		return;
	}

	if (!kidnap.has_imu()) {
		SDL_Log("%u {kestimate}kidnap_slice, finished (no imu), clear estimate, moved_times: %i/%i", 
				SDL_GetTicks(), kidnap.moved_times, kidnap.MAX_MOVE_FAILS);
		kidnap.clear_estimate(tkidnap::clrreason_noimu);
		return;
	}

	const bool disable_kidnap = false;
	if (game_config::os == os_windows && disable_kidnap) {
		// If you are debugging the [move]+[aplt task], you want the move to be always successful. 
		// That would set the [move]'s position as the (0, 0), and disable_kidnap = true.
		SDL_Log("%u {kestimate}kidnap_slice, finished (disable), clear estimate, moved_times: %i/%i", 
				SDL_GetTicks(), kidnap.moved_times, kidnap.MAX_MOVE_FAILS);
		kidnap.clear_estimate(tkidnap::clrreason_noimu);
		return;
	}

	VALIDATE(base_driver_.imu_can_read(), null_str);

	// const int8_t* kestimate_map_data = last_map_data_;
	const int8_t* kestimate_map_data = (const int8_t*)kestimate_map_data_.ptr;
	VALIDATE(kestimate_map_data != nullptr, null_str);

	if (!robot_imu_.has_magnetometer) {
		// if (robot_imu_.last_ticks == 0) {
		if (robot_imu_.require_full2_position) {
			send_immediate_kidnap_msg(tkidnap::msg_calc_imu_yaw, tkidnap::calcimu_initial);
			tpose2d to_pose2d = full2_reposition(scan_msg, last_map_, kestimate_map_data, kidnap.result4);
			if (!to_pose2d.valid) {
				SDL_Log("%u {kestimate}kidnap_slice, finished (calculate imu), clear estimate, moved_times: %i/%i", 
					SDL_GetTicks(), kidnap.moved_times, kidnap.MAX_MOVE_FAILS);
				kidnap.clear_estimate(tkidnap::clrreason_calcimu);
				return;
			}
			robot_imu_.set_base_yaw(base_driver_, to_pose2d.yaw);

			kidnap.set_timeout_ticks(SDL_GetTicks() + kidnap.TIMEOUT_MS);
			return;
		}
	}

	const uint32_t now = SDL_GetTicks();
	// uint32_t next_move_ticks = kidnap.next_move_ticks();
	if (now >= kidnap.timeout_ticks()) {
		SDL_Log("%u {kestimate}kidnap_slice, finished (timeout), clear estimate, ticks: %u >= %u, moved_times: %i/%i", 
				SDL_GetTicks(), now, kidnap.timeout_ticks(), kidnap.moved_times, kidnap.MAX_MOVE_FAILS);
		// kidnap.set_last_ok_estimated_pose2d(kidnap.last_pose2d);
		kidnap.clear_estimate(tkidnap::clrreason_timeout);
		return;
	}
	if (kidnap.apply_immediately() && kidnap.moved_times >= 0) {
		const tpose2d& last_reposition_pose2d = kidnap.last_reposition_pose2d();
		VALIDATE(last_reposition_pose2d.valid, null_str);
		tpose2d& to_pose2d = kidnap.cartographer_pose2d;
		to_pose2d = last_reposition_pose2d;

		kidnap.set_applied();
		SDL_Log("%u {kestimate}kidnap_slice, 2.1(later) apply immediately %s, moved_times: %i/%i", 
			SDL_GetTicks(), to_pose2d.to_string().c_str(), kidnap.moved_times, kidnap.MAX_MOVE_FAILS);
		restart_cartographer_node(cartographer_apply_immediately);
		kidnap.set_position_changed();
		wait_cartographer_node_ready();
		return;
	}

	const bool dbg_laser_scan = game_config::os == os_windows;
	// const bool dbg_laser_scan = false;
	// if (next_move_ticks != 0 && now >= next_move_ticks) {
	if (kidnap.vel_state() == tkidnap::vel_finished) {
        double imu_yaw = get_imu_yaw(nullptr);

		int clear_reason = nposm;
		int reason = kidnap.estimate();
		tpose2d& to_pose2d = kidnap.cartographer_pose2d;
		to_pose2d.valid = false;
		if (reason != tkidnap::reason_manual) {
			tkidnap::tresult4_C full_map_result3;
			tpose2d full_map_pose2d = full_map_reposition(imu_yaw, scan_msg, last_map_, kestimate_map_data, full_map_result3);
			if (!full_map_pose2d.valid) {
				SDL_Log("%u {kestimate}kidnap_slice, finished (full map reposition fail), clear estimate, full_map_pose2d: %s, full_map_result3: (score:%i, points:%i)", 
					SDL_GetTicks(), full_map_pose2d.to_string().c_str(), full_map_result3.score, full_map_result3.points);
				kidnap.clear_estimate(tkidnap::clrreason_repositionfail);
				return;
			}

			if (dbg_laser_scan && full_map_result3.unknowns >= 10) {
				save_laser_scan(scan_msg, tpose2d(0, 0, imu_yaw), full_map_result3, full_map_pose2d, "laser_scan-full-unkcells.dat");
			}

			tpose2d last_reposition_pose2d = kidnap.last_reposition_pose2d();
			tpose2d center_pose2d;
			if (full_map_pose2d.valid && last_reposition_pose2d.valid) {
				tkidnap::tresult4_C center_result3;
				last_reposition_pose2d.yaw = imu_yaw;
				center_pose2d = center_reposition(last_reposition_pose2d, scan_msg, last_map_, kestimate_map_data, center_result3);
				if (center_pose2d.valid) {
					if (dbg_laser_scan && center_result3.unknowns >= 10) {
						save_laser_scan(scan_msg, last_reposition_pose2d, center_result3, center_pose2d, "laser_scan-center-unkcells.dat");
					}
					// How many scores there are here is important for judging the result. 
					// Would rather spend a little more time working out a better score, use every 1 degeree.
					// below is a sample, the angle is only one degree different, but the score is much worse
					// {kestimate}---------->spend 451 ms, origin:(1.00576, 3.99028)(64, 177)-yaw(11.25256) score(854)
					// {kestimate}---------->spend 86 ms, origin:(1.00576, 3.99028)(64, 177)-yaw(10.25256) score(3098)
					SDL_Log("%u {kestimate}kidnap_slice, #%i full_map:[%s, result4:(score:%i, points:%i)] <> center:[%s, result4:(score: %i, points:%i)]", 
						SDL_GetTicks(), kidnap.moved_times, full_map_pose2d.to_string().c_str(), full_map_result3.score, full_map_result3.points,
						center_pose2d.to_string().c_str(), center_result3.score, center_result3.points);

					double threshold = REPOSITION_COMPARE_CM * 0.01;
					double x_diff = fabs(full_map_pose2d.x - center_pose2d.x);
					double y_diff = fabs(full_map_pose2d.y - center_pose2d.y);

					bool dbg_fail = SDL_GetTicks() - robot_imu_.last_full2_ticks > 60 * 1000;
					dbg_fail = false;
					if (dbg_fail) {
						center_pose2d.valid = false;

					} else if (x_diff > threshold || y_diff > threshold) {
						const int center_bonus = dbg_laser_scan? 0: 1 * center_result3.points; // 3000
						if (full_map_result3.score >= center_result3.score - center_bonus) {
							SDL_Log("%u {kestimate}kidnap_slice, #%i compare ok(score). full:%s - center:%s = fdiff(%.3f, %.3f)", 
								SDL_GetTicks(), kidnap.moved_times, full_map_pose2d.to_string().c_str(), center_pose2d.to_string().c_str(), x_diff, y_diff);
						} else { 
							center_pose2d.valid = false;
							SDL_Log("%u {kestimate}kidnap_slice, #%i compare fail. full:%s - center:%s = fdiff(%.3f, %.3f)", 
								SDL_GetTicks(), kidnap.moved_times, full_map_pose2d.to_string().c_str(), center_pose2d.to_string().c_str(), x_diff, y_diff);
						}
					} else {
						// full_map_pose2d's yaw is calcuated by current magnetometer.
						// center_pose2d.yaw = full_map_pose2d.yaw;
						SDL_Log("%u {kestimate}kidnap_slice, #%i compare ok(distance). full:%s - center:%s = fdiff(%.3f, %.3f)", 
							SDL_GetTicks(), kidnap.moved_times, full_map_pose2d.to_string().c_str(), center_pose2d.to_string().c_str(), x_diff, y_diff);
					}
				}
			}
			if (center_pose2d.valid) {
				kidnap.increase_equals();
				if (kidnap.equals() >= kidnap.require_equals()) {
					to_pose2d = center_pose2d;
					kidnap.set_last_ok_estimated_pose2d(to_pose2d);
					SDL_Log("%u {kestimate}kidnap_slice, #%i can finished, kidnap.equals(%i) >= require_equals(%i). pose2d:%s", 
						SDL_GetTicks(), kidnap.moved_times, kidnap.equals(), kidnap.require_equals(), to_pose2d.to_string().c_str());
					if (!robot_imu_.has_magnetometer) {
						// Eliminate accumulated errors
						robot_imu_.set_base_yaw(base_driver_, to_pose2d.yaw);
					}
					clear_reason = tkidnap::clrreason_ok;

				} else {
					SDL_Log("%u {kestimate}kidnap_slice, #%i wait next compare ok, kidnap.equals(%i) < require_equals(%i). pose2d:%s", 
						SDL_GetTicks(), kidnap.moved_times, kidnap.equals(), kidnap.require_equals(), center_pose2d.to_string().c_str());
					kidnap.set_last_reposition_pose2d(center_pose2d);
					// kidnap.set_next_move_ticks(0);
					kidnap.set_vel_state(tkidnap::vel_req, nposm);
					kidnap.moved_times ++;
				}

			} else {
				SDL_Log("%u {kestimate}kidnap_slice, #%i this compare fail, %i => clear_equals()", SDL_GetTicks(), kidnap.moved_times, kidnap.equals());
				if (dbg_laser_scan) {
					save_laser_scan(scan_msg, tpose2d(0, 0, imu_yaw), full_map_result3, full_map_pose2d, "laser_scan-full-unenque.dat");
				}
				kidnap.clear_equals();
				kidnap.set_require_equals(kidnap.MIN_REQUIRE_EQUALS + 1);

				if (kidnap.moved_times <= kidnap.MAX_MOVE_FAILS) {
					kidnap.set_last_reposition_pose2d(full_map_pose2d);
					// kidnap.set_next_move_ticks(0);
					kidnap.set_vel_state(tkidnap::vel_req, nposm);
					SDL_Log("%u {kestimate}kidnap_slice, #%i No results found, continue next reposition", SDL_GetTicks(), kidnap.moved_times);
					kidnap.moved_times ++;

				} else {
					SDL_Log("%u {kestimate}kidnap_slice, #%i No results found, fails > MAX_MOVE_FAILS(%i)",
						SDL_GetTicks(), kidnap.moved_times, kidnap.MAX_MOVE_FAILS);
					VALIDATE(!to_pose2d.valid, null_str);
					// kidnap.set_last_ok_estimated_pose2d(kidnap.last_pose2d);

					bool moveout = true;
					if (!robot_imu_.has_magnetometer && (int)(SDL_GetTicks() - robot_imu_.last_full2_ticks) >= robot_imu_.reposition_threshold) {
						send_immediate_kidnap_msg(tkidnap::msg_calc_imu_yaw, tkidnap::calcimu_reposition);
						tpose2d full2_pose2d = full2_reposition(scan_msg, last_map_, kestimate_map_data, kidnap.result4);
						if (full2_pose2d.valid) {
							robot_imu_.set_base_yaw(base_driver_, full2_pose2d.yaw);
							SDL_Log("%u {kestimate}kidnap_slice, #%i calculate imu of reposition success, set moved_times to 0 and continue estimate.", 
								SDL_GetTicks(), kidnap.moved_times);
							kidnap.moved_times = 0;
							kidnap.set_timeout_ticks(SDL_GetTicks() + kidnap.TIMEOUT_MS);
						}
					}

					if (kidnap.moved_times > kidnap.MAX_MOVE_FAILS) {
						clear_reason = tkidnap::clrreason_moveout;
					}
				}
			}

			if (to_pose2d.valid) {
				VALIDATE(clear_reason != nposm, null_str);
			}

		} else {
			VALIDATE(reason == tkidnap::reason_manual, null_str);
			to_pose2d = full2_reposition(scan_msg, last_map_, kestimate_map_data, kidnap.result4);
			if (!robot_imu_.has_magnetometer && to_pose2d.valid) {
				// It will modify base_yaw.
				robot_imu_.set_base_yaw(base_driver_, to_pose2d.yaw);
			}

			clear_reason = tkidnap::clrreason_ok;

		}

		if (clear_reason != nposm) {
			if (to_pose2d.valid) {
				restart_cartographer_node(cartographer_ok_esitmated);
				kidnap.set_position_changed();
				// wait until cartographer ready.
				wait_cartographer_node_ready();
			}
			SDL_Log("%u {kestimate}kidnap_slice, finished (%s), clear estimate, moved_times: %i/%i", 
				SDL_GetTicks(), to_pose2d.valid? "success": "fail", kidnap.moved_times, kidnap.MAX_MOVE_FAILS);
			kidnap.clear_estimate(clear_reason);
		}
	}
}

void tros_instance::did_scan_subscribed(const sensor_msgs::LaserScan& scan_msg)
{
	SDL_2Point charging;
	instance_slot_.did_scan_subscribed(scan_msg, charging);

	// {dbg_publish}laser_scan
	// sensor_msgs::LaserScan scan_msg;
	// integrate_dcamera_LaserScan(_scan_msg, scan_msg);

/*
	// ---
	sensor_msgs::LaserScan scan_msg2;
	if (game_config::os == os_windows) {
		// const std::string filename = "laser_scan-full-unkcells.dat";
		// const std::string filename = "laser_scan-full-unenque.dat";
		// const std::string filename = "laser_scan-full2-unkcells.dat";
		const std::string filename = "laser_scan-full2-unkcells-topic-scan.dat";
		// const std::string filename = "laser_scan-full2-unkcells-topic_camera_scan.dat";
		// const std::string filename = "laser_scan-full2-unkcells-50-92.dat";
		tlaser_msg_header header;
		bool result = load_laser_scan_from_file(game_config::preferences_dir + "/" + filename, scan_msg2, header);
		VALIDATE(result, null_str);
	}
	// ---
*/
	kidnap_slice(scan_msg);
	// kidnap_slice(scan_msg2);

	for (std::set<tslot*>::iterator it = slots_.begin(); it != slots_.end(); ++ it) {
		tslot& slot = **it;
		slot.did_scan_subscribed(scan_msg, charging);
	}
}

extern void contours_2_map_data(int width, int height, const std::vector<std::vector<cv::Point> >& contours, const std::set<int>& indexs, std::vector<cv::Vec4i> hierarchy, uint8_t* map_data, uint8_t fill_value);

void tros_instance::generate_kestimate_map_data(const ::nav_msgs::MapMetaData& info, const int8_t* map_data)
{
    VALIDATE(map_data != nullptr, null_str);

	uint32_t start_ticks = SDL_GetTicks();

	const bool verbose_png = false;
	const int width = info.width;
    const int height = info.height;
    const int cells = width * height;

	const int desire_size = posix_align_ceil(cells, 4096);
    if (desire_size > kestimate_map_data_.len) {
		if (kestimate_map_data_.ptr != nullptr) {
			free(kestimate_map_data_.ptr);
		}
        kestimate_map_data_.ptr = (uint8_t*)malloc(desire_size);
        kestimate_map_data_.len = desire_size;
    }
	// Even if it does fail, at least the 'map_data' is saved
	memcpy(kestimate_map_data_.ptr, map_data, cells);

    if (verbose_png)  {
        surface surf = u8_data_2_cell_value_surf((const uint8_t*)map_data, 4, 4, width, height, maptype_OccupancyGrid, nullptr, nullptr);
        imwrite(surf, "1-cell_value.png");
		imwrite(surf, "1-raw.png");
    }

    cv::Mat threshold_mat = cv::Mat(height, width, CV_8UC1);
    uint8_t* threshold_mat_data = threshold_mat.ptr<uint8_t>(0);

    int pos = 0;
    for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			int i = x + (height - y - 1) * width;
            uint8_t u8 = map_data[i];
            threshold_mat_data[pos] = u8 != costmap_2d::NO_INFORMATION? u8: 0;
            pos ++;
		}
    }
    VALIDATE(pos == width * height, null_str);

    const uint8_t OBSTACLE_VALUE = 255;
    VALIDATE(OBSTACLE_VALUE == costmap_2d::NO_INFORMATION, null_str);

    double threshold_occupied = NEAR_OBSTACLE; // think >= 60 as obstacle
    cv::threshold(threshold_mat, threshold_mat, threshold_occupied - 1, OBSTACLE_VALUE, cv::THRESH_BINARY); // cv::THRESH_BINARY_INV

    if (verbose_png) {
        imwrite_gray(threshold_mat, "1-gray.png");
    }


    std::vector<std::vector<cv::Point> > contours;
    std::vector<cv::Vec4i> hierarchy;
	// cv::findContours(dilate_mat, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);
    cv::findContours(threshold_mat, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    const int max_support_contours = 254; // display only one byte, and MUST NOT display 0.
    if (contours.empty()) {
        SDL_Log("generate_kestimate_map failed. There are no contour in the map");
        return;
    }

    cv::Mat o = cv::Mat::zeros(threshold_mat.rows, threshold_mat.cols, CV_8UC3);

	const int min_obstacle_size = 10; // 8
    std::set<int> hit_indexs;
    // hierarchy[.][0]: index of next contour
    for (int index = 0; index >= 0; index = hierarchy[index][0]) {
        VALIDATE(index >= 0 && (int)contours.size(), null_str);
		cv::Rect rect = cv::boundingRect(contours[index]);
		if (rect.width >= min_obstacle_size || rect.height >= min_obstacle_size) {
			hit_indexs.insert(index);
		}
        if (verbose_png) {
            cv::Scalar color(rand() % 255, rand() % 255, rand() % 255);
            cv::drawContours(o, contours, index, color, cv::FILLED, 8, hierarchy);
        }
    }
    if (verbose_png) {
        imwrite(o, "1-contours-1nd-all.png");
    }

	const uint8_t TOUTH_VALUE = 255;
    contours_2_map_data(width, height, contours, hit_indexs, hierarchy, threshold_mat_data, TOUTH_VALUE);
    if (verbose_png) {
        surface surf = u8_data_2_cell_value_surf(threshold_mat_data, 4, 4, width, height, nposm, nullptr, nullptr);
        imwrite(surf, "1-contours-hit-cell_value.png");

		imwrite_gray(threshold_mat, "1-contours-hit-gray.png");
    }


    // 2nd dilate
	cv::Mat dilate_mat;
    int radius = 1; // (1 + 2) = 3
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * radius + 1, 2 * radius + 1), cv::Point(radius, radius));
    cv::dilate(threshold_mat, dilate_mat, element);

	uint8_t* dilate_mat_data = dilate_mat.ptr<uint8_t>(0);
    if (verbose_png) {
		surface surf = u8_data_2_cell_value_surf(dilate_mat_data, 4, 4, width, height, nposm, nullptr, nullptr);
		imwrite(surf, "1-dilate-cell_value.png");

        imwrite_gray(dilate_mat, "1-dilate-gray.png");
    }

	VALIDATE(dilate_mat.channels() == 1 && dilate_mat.cols == width && dilate_mat.rows == height, null_str);

	// old value + 20 => new value
	const uint8_t bonus = 20;
    for (int y = 0; y < height; y ++) {
		for (int x = 0; x < width; x ++) {
			int dilate_index = x + y * width;
			int map_index = x + (height - y - 1) * width;

			uint8_t u8 = map_data[map_index];
			if (dilate_mat_data[dilate_index] == TOUTH_VALUE) {
				if (u8 != costmap_2d::NO_INFORMATION) {
					u8 += bonus;
				} else {
					u8 = bonus;
				}
				if (u8 > MAX_CARTOGRAPHER_CELL_VAL) {
					u8 = MAX_CARTOGRAPHER_CELL_VAL;
				}
			}
            kestimate_map_data_.ptr[map_index] = u8;
		}
    }

	if (verbose_png) {
		surface surf = u8_data_2_cell_value_surf(kestimate_map_data_.ptr, 4, 4, width, height, maptype_OccupancyGrid, nullptr, nullptr);
		imwrite(surf, "1-kestimate_map-cell_value.png");
    }

	SDL_Log("{generate_kestimate_map_data}elapse %u ms", SDL_GetTicks() - start_ticks);
}

void tros_instance::refresh_map_data(const nav_msgs::OccupancyGrid& map, bool from_map_topic)
{
	const bool navigation_from_map_topic = mode_ == mode_navigation && from_map_topic;

	if (!navigation_from_map_topic) {
		last_map_ = map.info;
	} else {
		// mode_ == mode_navigation && from_map_topic
		// in this, the map received by the subscription 'map' topic must be the map that was previously set.

		// field: map_load_time maybe not equal.
		const ::nav_msgs::MapMetaData& info = map.info;
		VALIDATE(last_map_.width == info.width && last_map_.height == info.height, null_str);
		VALIDATE(last_map_.origin == info.origin && last_map_.resolution == info.resolution, null_str);
	}

	const int cells = map.info.width * map.info.height;
    if (cells > last_map_data_size_) {
        free(last_map_data_);
        last_map_data_ = (int8_t*)malloc(cells);
        last_map_data_size_ = cells;
    }

	const int8_t* map_data_ptr = &map.data[0];

	if (!navigation_from_map_topic) {
		memcpy(last_map_data_, map_data_ptr, cells);
	} else {
		VALIDATE(memcmp(last_map_data_, map_data_ptr, cells) == 0, null_str);
	}

	if (mode_ == mode_navigation && !from_map_topic) {
		generate_kestimate_map_data(map.info, last_map_data_);
	}
}

void tros_instance::did_map_subscribed(const nav_msgs::OccupancyGrid& map)
{
	refresh_map_data(map, true);

	for (std::set<tslot*>::iterator it = slots_.begin(); it != slots_.end(); ++ it) {
		tslot& slot = **it;
		slot.map_received = true;
		slot.did_map_subscribed(map);
	}
	if (mode_ == mode_navigation && map.info.width > 0 && map.info.height > 0) {
		// SDL_Log("Shutting down the map subscriber. it is mode_navigation");
		navigation_map_ = map;
		shutdown_map_topic();
	}
}

void tros_instance::did_current_goal_subscribed(const geometry_msgs::PoseStamped& goal)
{
	for (std::set<tslot*>::iterator it = slots_.begin(); it != slots_.end(); ++ it) {
		tslot& slot = **it;
		slot.goal_received = true;
		slot.did_current_goal_subscribed(goal);
	}
	if (!goal.header.frame_id.empty()) {
		navigation_goal_ = goal;
	}
}

void tros_instance::did_global_plan_subscribed(const nav_msgs::Path& path)
{
	for (std::set<tslot*>::iterator it = slots_.begin(); it != slots_.end(); ++ it) {
		tslot& slot = **it;
		slot.did_global_plan_subscribed(path);
	}
}

void tros_instance::did_local_plan_subscribed(const nav_msgs::Path& path)
{
	for (std::set<tslot*>::iterator it = slots_.begin(); it != slots_.end(); ++ it) {
		tslot& slot = **it;
		slot.did_local_plan_subscribed(path);
	}
}

void tros_instance::did_recovery_status(const move_base_msgs::RecoveryStatus& status)
{
	for (std::set<tslot*>::iterator it = slots_.begin(); it != slots_.end(); ++ it) {
		tslot& slot = **it;
		slot.did_recovery_status(status);
	}
}

bool tros_instance::use_external_laser() const
{
	// if true, only for debug. receive /scan topic from external *.bag
	return mode_ == mode_buildmap && rosbag_ == rosbag_play;
}

void tros_instance::resize_dcamera_points(int size)
{
	size = posix_align_ceil(size, 64);
    VALIDATE(size >= 0, null_str);

	if (size > dcamera_point_size_) {
	    SDL_FPoint3* tmp = (SDL_FPoint3*)malloc(size * sizeof(SDL_FPoint3));
	    if (dcamera_points_ != nullptr) {
			free(dcamera_points_);
		}
		dcamera_points_ = tmp;
		dcamera_point_size_ = size;
    }
}

void tros_instance::set_dcamera_points(const SDL_FPoint3* points, int size, const tdepth_sector_result& result)
{
	VALIDATE_NOT_MAIN_THREAD();

	threading::lock lock(dcamera_points_mutex_);
	resize_dcamera_points(size);
	memcpy(dcamera_points_, points, sizeof(SDL_FPoint3) * size);
	dcamera_point_vsize_ = size;
	depth_sector_result_ = result;
}

void tros_instance::do_publish_scan(const sensor_msgs::LaserScan& scan_msg, const sensor_msgs::LaserScan& laser_scan_msg)
{
	rose_ros::pub_scan.publish(scan_msg);

	if (rose_ros::pub_laser_scan.getNumSubscribers() > 0) {
		rose_ros::pub_laser_scan.publish(laser_scan_msg);
	}
}

void tros_instance::integrate_dcamera_LaserScan(const sensor_msgs::LaserScan& msg, sensor_msgs::LaserScan& result)
{
	// angle_min must be -M_PI
	// angle_max must be +M_PI
	// for A1M8, [angle_min, angle_max] is [-3.12413907, M_PI]
	VALIDATE(msg.angle_min > -M_PI - 0.0001 && msg.angle_min < -M_PI + 0.02, null_str);
	VALIDATE(msg.angle_max > M_PI - 0.02 && msg.angle_max < M_PI + 0.0001, null_str);
	VALIDATE(msg.angle_increment > 0, null_str);

	tauto_destruct_executor destruct_executor(std::bind(&tros_instance::do_publish_scan, this, std::ref(result), std::ref(msg)));
	result = msg;

	if (!moveit_driver_.installed() || !dcamera_driver_.installed()) {
		return;
	}

	const int node_count = result.ranges.size();
	float* range_data = &result.ranges[0];

	// if (has_laser_shielded()) {
	//	return;
	// }

	const float min_angle_degree = -MOVEIT_SECTOR_HALF_ANGLE_DEG;
    const float max_angle_degree = MOVEIT_SECTOR_HALF_ANGLE_DEG;
    VALIDATE(max_angle_degree > min_angle_degree, null_str);

	float clear_angle_min = (float)DEG2RAD(min_angle_degree);
    float clear_angle_max = (float)DEG2RAD(max_angle_degree);

	// (1/2)clear sector
	VALIDATE(clear_angle_min > result.angle_min, null_str);
	VALIDATE(clear_angle_max < result.angle_max, null_str);

	int min_range_at = (clear_angle_min - result.angle_min) / result.angle_increment;
	if (min_range_at >= node_count) {
		min_range_at = node_count - 1;
	}
	int max_range_at = (clear_angle_max - result.angle_min) / result.angle_increment;
	if (max_range_at >= node_count) {
		max_range_at = node_count - 1;
	}
	// SDL_Log("%u node_count: %i angle_increment: %.3f range: [%.3f, %.3f] rang_at: [%i %i]", SDL_GetTicks(), 
	//	node_count, RAD2DEG(result.angle_increment), 
	//	RAD2DEG(angle_clear_min - result.angle_min), RAD2DEG(angle_clear_max - result.angle_min), 
	//	min_range_at, max_range_at);

	// float default_range = std::numeric_limits<float>::infinity();
	// float default_range = msg.range_max - 0.01;
	// float default_range = 8 - 0.01; // must be fail. map will change again and again.
	const float default_range = MOVEIT_SECTOR_DEFAULT_RANGE;
	for (int at = min_range_at; at <= max_range_at; at ++) {
		range_data[at] = default_range;
	}

	if (dcamera_point_vsize_ == 0) {
		return;
	}

	// (2/2)fill sector
	if (is_float_nposm(dcamera_2_laser_xy_.x)) {
		geometry_msgs::TransformStamped transform;
		if (!get_source_2_target_tf(frame_id_camera, "laser", transform)) {
			return;
		}
		VALIDATE(is_float_nposm(dcamera_2_laser_xy_.y), null_str);
		dcamera_2_laser_xy_.x = transform.transform.translation.x;
		dcamera_2_laser_xy_.y = transform.transform.translation.y;
	}

	const float min_dcamera_distance = 0.10f; // 10cm
    const float max_dcamera_distance = 5.0f; // 5m

	std::map<float, float> theat_dist_map;

	threading::lock lock(dcamera_points_mutex_);
	for (int at = 0; at < dcamera_point_vsize_; at ++ ) {
		const SDL_FPoint3& point = dcamera_points_[at];
		if (point.x == 0) {
			VALIDATE(point.y == 0 && point.z == 0, null_str);
			continue;
		}

		float theta = atan2(point.y, point.x);
		// theta maybe not in [clear_angle_min, clear_angle_max]

		float laser_x = point.x + dcamera_2_laser_xy_.x;
		float laser_y = point.y + dcamera_2_laser_xy_.y;

		float dist = hypot(laser_x, laser_y);
		VALIDATE(dist > 0, null_str);

		float laser_theta = atan2(laser_y, laser_x);
		if (laser_theta <= clear_angle_min || laser_theta >= clear_angle_max) {
			// SDL_Log("[%i/%i](discard)sector: %i dcamera_2_laser_xy: (%.3f, %.3f) theta: %.3f laser_theta: %.3f", 
			//	at, dcamera_point_vsize_, MOVEIT_SECTOR_HALF_ANGLE_DEG, dcamera_2_laser_xy_.x, dcamera_2_laser_xy_.y, RAD2DEG(theta), RAD2DEG(laser_theta));
			continue;
		} else {
			// SDL_Log("[%i/%i]sector: %i dcamera_2_laser_xy: (%.3f, %.3f) theta: %.3f laser_theta: %.3f", 
			//	at, dcamera_point_vsize_, MOVEIT_SECTOR_HALF_ANGLE_DEG, dcamera_2_laser_xy_.x, dcamera_2_laser_xy_.y, RAD2DEG(theta), RAD2DEG(laser_theta));
		}

		float angle_positivization = laser_theta - result.angle_min;
		int range_at = angle_positivization / result.angle_increment;
		if (range_at >= node_count) {
			range_at = node_count - 1;
		}

		if (dist >= min_dcamera_distance && dist <= max_dcamera_distance) {
			if (dist < range_data[range_at]) {
				range_data[range_at] = dist;
			}
		}
	}
}

void tros_instance::draw_dcamera_points(gui2::ttrack& track, const SDL_Rect& bg_rect, int ranges, double meter_per_range)
{
	SDL_Renderer* renderer = get_renderer();

	threading::lock lock(dcamera_points_mutex_);
	if (dcamera_point_vsize_ == 0) {
		return;
	}

	SDL_Rect* draw_rects_ = nullptr;
	draw_rects_ = (SDL_Rect*)malloc(dcamera_point_vsize_ * sizeof(SDL_Rect));
    int draw_rects_size_ = dcamera_point_vsize_;

	const int half_thickness = 1;

	int pixels_per_range = bg_rect.w / ranges;
	double radio_x = 1.0 * pixels_per_range / meter_per_range;
	double radio_y = radio_x;
	const int bg_rect_center_y = bg_rect.y + bg_rect.h / 2;
	const int image_h = bg_rect.w;

	int vsize = 0;
	for (int at = 0; at < dcamera_point_vsize_; at ++ ) {
		const SDL_FPoint3& point = dcamera_points_[at];
		if (point.x == 0) {
			VALIDATE(point.y == 0 && point.z == 0, null_str);
			continue;
		}

		const int pixel_x = point.x * radio_x;
		const int pixel_y = point.y * radio_y;
		if (pixel_x >= bg_rect.w || pixel_y >= image_h / 2) {
			continue;
		}

		int center_x = bg_rect.x + pixel_x;
		int center_y = 0;
		if (point.y >= 0) {
			center_y = bg_rect_center_y - pixel_y;
		} else {
			center_y = bg_rect_center_y + (-1 * pixel_y);
		}

		SDL_Rect& to = draw_rects_[vsize ++];
		to.x = center_x - half_thickness;
        to.w = 2 * half_thickness;
        to.y = center_y - half_thickness;
        to.h = 2 * half_thickness;
	}

	if (vsize != 0) {
		render_rects(renderer, 0xffff0000, draw_rects_, vsize);
	}
	free(draw_rects_);


	char buf[128];
	SDL_snprintf(buf, sizeof(buf), "left: %s right: %s", 
		depth_sector_result_.left_no_depth? "true": "false", depth_sector_result_.right_no_depth? "true": "false");

    surface text_surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLEST, font::BLACK_COLOR);

	texture text_tex = SDL_CreateTextureFromSurface2(renderer, text_surf);

	SDL_Rect dstrect;

	dstrect.x = bg_rect.x;
	dstrect.y = bg_rect.y - text_surf->h;
    dstrect.w = text_surf->w;
    dstrect.h = text_surf->h;
	SDL_RenderCopy(renderer, text_tex.get(), nullptr, &dstrect);
}

tros_base_node::tros_base_node(tros_instance& ros_instance)
	: tros_instance::tslot(type_base_node)
	, ros_instance_(ros_instance)
{
}

tros_base_node::~tros_base_node()
{
	// VALIDATE(!started(), null_str);
	VALIDATE(!registered, null_str);
}