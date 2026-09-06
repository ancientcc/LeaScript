// Efficient alternative to joint_state_publisher + robot_state_publisher
// Author: Max Schwarz <max.schwarz@ais.uni-bonn.de>

#include <ros/ros.h>

#include <sensor_msgs/JointState.h>

#include <moveit/robot_model_loader/robot_model_loader.h>
#include <moveit/robot_state/robot_state.h>

#include <tf2_ros/static_transform_broadcaster.h>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2_eigen/tf2_eigen.h>
#include <ros/callback_queue.h>

#include <rose_ros/utils.hpp>
#include <rose_thread.hpp>
#include <rose_exception.hpp>


class tstate_publisher
{
public:
    tstate_publisher(ros::CallbackQueue& cbqueue, robot_state::RobotState& state, ros::Publisher& pub_js, 
        const std::string& topic, const ros::Duration& minPeriod)
     : JS_HZ(10) // default 10 hz
     , joint_values_dirty(false)
     , cbqueue_(cbqueue)
     , state_{state}
     , pub_js_(pub_js)
     , topic_(topic)
     , min_period_(minPeriod)
     , pub_JSs_(0)
    {
        ros::NodeHandle nh("~");
        nh.setCallbackQueue(&cbqueue_);
        subscriber_ = nh.subscribe(topic_, 1, &tstate_publisher::did_JointState_subscribed, this);
    }

    void publish_js(ros::Publisher& pub_js);

private:
    void did_JointState_subscribed(const sensor_msgs::JointState& js);

public:
    const int JS_HZ;
    bool joint_values_dirty;

private:
    ros::CallbackQueue& cbqueue_;
    robot_state::RobotState& state_;
    ros::Publisher& pub_js_;
    const std::string topic_;
    const ros::Duration min_period_;

    ros::Subscriber subscriber_;
    ros::Time last_stamp_;
    int pub_JSs_; // published JointState count
};


void tstate_publisher::did_JointState_subscribed(const sensor_msgs::JointState& js)
{
    if (js.header.stamp - last_stamp_ < min_period_) {
        return;
    }

    ros::Time startTime = ros::Time::now();

    if (js.name.size() != js.position.size()) {
        ROS_ERROR_THROTTLE(1.0, "Ignoring invalid joint_state msg");
        return;
    }

    // SDL_Log("{smart_state_publisher}%u, receive JointState: %s", SDL_GetTicks(), JointState_to_string(js).c_str());

    state_.setVariableValues(js);
    joint_values_dirty = true;

    ros::Time sendTime = ros::Time::now();
    // broadcaster.sendTransform(transforms);

    // publish_js(pub_js_);

    last_stamp_ = js.header.stamp;
}

void tstate_publisher::publish_js(ros::Publisher& pub_js)
{
    sensor_msgs::JointState msg;

	std_msgs::Header& header = msg.header;
	header.seq = 0;
	// header.frame_id = "map";
	header.stamp = ros::Time::now();

    const moveit::core::RobotModel& model = *state_.getRobotModel().get();
    int count = model.getVariableCount();
    const std::vector<std::string>& names = model.getVariableNames();
    VALIDATE(count == (int)names.size(), null_str);

    // positions
    const double* position = state_.getVariablePositions();

    msg.name = names;
    for (int at = 0; at < count; at ++) {
		msg.position.push_back(position[at]);
	}

    if ((pub_JSs_ % (10 * JS_HZ)) == 0) { // (10hz) 10 == 1 second
        SDL_Log("{smart_state_publisher}#%i, %u, send JointState: %s", pub_JSs_, SDL_GetTicks(), JointState_to_string(msg).c_str());
    }

    pub_JSs_ ++;

	pub_js.publish(msg);
}

ROSTIME_DECL int joint_state_publisher__joint_state_publisher(bool& exit, void* void_ptr_RobotState, trose_event& e)
{
    int argc = 0;
    char** argv = nullptr;
    ros::init(argc, argv, "smart_state_publisher");

    // ros::NodeHandle nh{"~"};
    ros::NodeHandle nh;
    ros::CallbackQueue cbqueue;
    nh.setCallbackQueue(&cbqueue);

    // robot_state::RobotState state(model);
    robot_state::RobotState& state = *reinterpret_cast<robot_state::RobotState*>(void_ptr_RobotState);
    // keep original value, don't call state.setToDefaultValues();

    // Publish all received (rate-limited) messages on the agg topic
    ros::Publisher pub_js = nh.advertise<sensor_msgs::JointState>("joint_states", 10);

    // Dynamic transforms
    tstate_publisher publisher(cbqueue, state, pub_js, "fake_controller_joint_states", ros::Duration{0.0});

    // ros::spin();
    ros::Rate r(publisher.JS_HZ);
    ros::WallDuration timeout(0.1f);

    e.Set();
    while (!exit & ros::ok()) {
        publisher.publish_js(pub_js);
	    cbqueue.callAvailable(timeout);

        if (!publisher.joint_values_dirty) {
            r.sleep();

        } else {
            publisher.joint_values_dirty = false;
        }
    }

    return 0;
}
