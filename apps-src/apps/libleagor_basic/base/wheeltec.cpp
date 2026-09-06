#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "rose_global.hpp"
#include "wheeltec.hpp"
#include "rose_exception.hpp"

#include <SDL.h>
#include <SDL_log.h>
#include "aplt_common.hpp"

#include <ros/callback_queue.h>
#include <ros/serialization.h>
#include <std_msgs/String.h>
#include "aplt2.hpp"
#include "rose_ros/utils.hpp"

//
// tserial
//
twheeltec::twheeltec(ros::NodeHandle& nh, const std::string& path, int baudrate)
	: twheeltecserial(path, baudrate, CARTYPE_X3)
	, nh_(nh)
	, tid_(SDL_ThreadID())
{
	// const config& cfg = aplt::curr_aplt->prefs.cfg();
	double publish_period_sec = 0.1; // 100 ms

	send_cmd_timer_ = nh.createTimer(
        ::ros::Duration(publish_period_sec),
        &twheeltec::send_cmd_timer_handler, this);

	twist_sub_ = nh.subscribe("cmd_vel", 1, &twheeltec::did_serial_geometry_msgs_Twist, this);
	joint_states_sub_ = nh.subscribe("joint_states", 1, &twheeltec::did_serial_sensor_msgs_JointState, this);
}

twheeltec::~twheeltec()
{
}

void twheeltec::send_cmd_timer_handler(const ::ros::TimerEvent& timer_event)
{
	VALIDATE_IN_THIS_THREAD(tid_);
}

void twheeltec::did_serial_geometry_msgs_Twist(const geometry_msgs::Twist& msg)
{
	SDL_Log("%u did_serial_geometry_msgs_Twist Twist msg: linear(%.5f, %.5f, %.5f), angular(%.5f, %.5f, %.5f[deg:%.5f])", 
		SDL_GetTicks(), msg.linear.x, msg.linear.y, msg.linear.z,
		msg.angular.x, msg.angular.y, msg.angular.z, RAD2DEG(msg.angular.z));

	send_Twist(msg);
}

void twheeltec::did_serial_sensor_msgs_JointState(const sensor_msgs::JointState& msg)
{
	// SDL_Log("%u, {twheeltec}receive JointState: %s", SDL_GetTicks(), JointState_to_string(msg).c_str());
	send_JointState(msg);
}

int wheeltec_base__node(bool& exit, const std::string& serial_path, int baudrate)
{
	VALIDATE(!serial_path.empty(), null_str);
	VALIDATE(baudrate > 0, null_str);
    int argc = 0;
	ros::init(argc, nullptr, "node");

	ros::NodeHandle n;

	ros::CallbackQueue cbqueue;
	n.setCallbackQueue(&cbqueue);

	SDL_Log("{wheeltec_base__node}want open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
	twheeltec serial(n, serial_path, baudrate);
	if (!serial.valid()) {
		SDL_Log("{wheeltec_base__node}cannot open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
		return -1;
	}

	ros::WallDuration timeout(0.1f);
	while (!exit && ros::ok()) {
		serial.pool_read();
		cbqueue.callAvailable(timeout);
	}

	// rose_ros::sub_scan.shutdown();

    return 0;
}