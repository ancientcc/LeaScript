#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "rose_global.hpp"
#include "yahboom.hpp"
#include "rose_exception.hpp"
// #include "aplt_clazz.hpp"

#include <SDL.h>
#include <SDL_log.h>
#include "aplt_common.hpp"
#include "aplt2.hpp"

#include <ros/callback_queue.h>
#include <ros/serialization.h>
#include <std_msgs/String.h>

//
// tserial
//
tyahboom::tyahboom(ros::NodeHandle& nh, const std::string& path, int baudrate)
	: tyahboomserial(path, baudrate, CARTYPE_X3)
	, nh_(nh)
	, tid_(SDL_ThreadID())
	, CAR_RUN_stop_threshold_(400) // 400, 300short?
	, next_REPORT_SPEED_ticks_(0)
	, next_CAR_RUN_stop_ticks_(0)
{
	const config& cfg = aplt::curr_aplt->prefs.cfg();
    CAR_RUN_stop_threshold_ = cfg["yahboom_motor_threshold"].to_int(400);
	SDL_Log("tyahboom::tyahboom, CAR_RUN_stop_threshold_: %i", CAR_RUN_stop_threshold_);

	double publish_period_sec = 0.1; // 100 ms

	send_cmd_timer_ = nh.createTimer(
        ::ros::Duration(publish_period_sec),
        &tyahboom::send_cmd_timer_handler, this);

	twist_sub_= nh.subscribe("cmd_vel", 1, &tyahboom::did_serial_geometry_msgs_Twist, this);
}

tyahboom::~tyahboom()
{
}

void tyahboom::send_cmd_timer_handler(const ::ros::TimerEvent& timer_event)
{
	VALIDATE_IN_THIS_THREAD(tid_);

	const uint32_t now = SDL_GetTicks();

	if (now >= next_REPORT_SPEED_ticks_) {
		uint8_t data[] = {0x0a, 0x00};
		send_cmd(FUNC_REQUEST_DATA, data, sizeof(data) / sizeof(data[0]));
		// SDL_Log("%u timer_handler, it is timer to send FUNC_REQUEST_DATA", SDL_GetTicks());

		data[0] = 0x0b;
		// send_cmd(FUNC_REQUEST_DATA, data, sizeof(data) / sizeof(data[0]));

		data[0] = 0x0c;
		// send_cmd(FUNC_REQUEST_DATA, data, sizeof(data) / sizeof(data[0]));

		// data[0] = 0x51;
		// send_cmd(FUNC_REQUEST_DATA, data, sizeof(data) / sizeof(data[0]));

		const int report_speed_period_sec = 1000;
		next_REPORT_SPEED_ticks_ = now + report_speed_period_sec;

	} /* else if (now >= next_pub_voltage_ticks_) {
		const int pub_voltage_period_sec = 2000;
		next_pub_voltage_ticks_ = now + pub_voltage_period_sec;
		// aplt::valuex.battery_level = voltage_;
		aplt::valuex.set_NMTHREAD_battery_level(voltage_);
		SDL_Log("%u, {yahboom}public voltage: %.4f", SDL_GetTicks(), voltage_);

	} */ else if (next_CAR_RUN_stop_ticks_ != 0 && now >= next_CAR_RUN_stop_ticks_) {
		CAR_RUN_stop();
		next_CAR_RUN_stop_ticks_ = 0;
	}
}

void tyahboom::did_serial_geometry_msgs_Twist(const geometry_msgs::Twist& msg)
{
	SDL_Log("%u did_serial_geometry_msgs_Twist Twist msg: linear(%.5f, %.5f, %.5f), angular(%.5f, %.5f, %.5f[deg:%.5f])", 
		SDL_GetTicks(), msg.linear.x, msg.linear.y, msg.linear.z,
		msg.angular.x, msg.angular.y, msg.angular.z, RAD2DEG(msg.angular.z));

	double adj_linear_x = msg.linear.x;
	// double linear_y = msg.linear.y;
	double adj_angular_z = msg.angular.z;
/*
	double min_moveable_vel_x = 0.08;
	double min_moveable_vel_theta = DEG2RAD(20);

	if (fabs(msg.linear.x) >= min_moveable_vel_x) {
		// Mainly linear movement
		if (msg.linear.x > 0) {
			adj_linear_x += 0.06;
		} else {
			adj_linear_x += -0.06;
		}
		if (msg.angular.z > 0) {
			adj_angular_z += DEG2RAD(15);

		} else if (msg.angular.z < 0) {
			adj_angular_z -= DEG2RAD(15);
		}

	} else {
		// Mainly is rotation
		adj_linear_x += 0.02;

		if (msg.angular.z > 0) {
			adj_angular_z += DEG2RAD(30);

		} else if (msg.angular.z < 0) {
			adj_angular_z -= DEG2RAD(30);
		}
	}
*/
	// int16_t linear_x = msg.linear.x * 1000.0;
	int16_t linear_x = adj_linear_x * 1000.0;
	int16_t linear_y = msg.linear.y * 1000.0;
	// int16_t angular_z = msg.angular.z * 1000.0;
	int16_t angular_z = adj_angular_z * 1000.0;
	uint8_t data[] = {car_type_, posix_lo8(linear_x), posix_hi8(linear_x), 
		posix_lo8(linear_y), posix_hi8(linear_y), posix_lo8(angular_z), posix_hi8(angular_z)};

	// if (game_config::os != os_windows) {
		send_cmd(FUNC_MOTION, data, sizeof(data) / sizeof(data[0]));

		next_CAR_RUN_stop_ticks_ = SDL_GetTicks() + CAR_RUN_stop_threshold_;
	// }
}

int yahboom_base__node(bool& exit, const std::string& serial_path, int baudrate)
{
	VALIDATE(!serial_path.empty(), null_str);
	VALIDATE(baudrate > 0, null_str);
    int argc = 0;
	ros::init(argc, nullptr, "node");

	ros::NodeHandle n;

	ros::CallbackQueue cbqueue;
	n.setCallbackQueue(&cbqueue);

	SDL_Log("{yahboom_base__node}want open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
	tyahboom serial(n, serial_path, baudrate);
	if (!serial.valid()) {
		SDL_Log("{yahboom_base__node}cannot open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
		return -1;
	}

	// 0x0(disable) 0x00(??don't save to flash, maybe 0x5f)
	// uint8_t auto_report_msg[] = {0x00, 0x00};
	uint8_t auto_report_msg[] = {0x01, 0x00};
	serial.send_cmd(tyahboomserial::FUNC_AUTO_REPORT, auto_report_msg, sizeof(auto_report_msg) / sizeof(auto_report_msg[0]));

	uint8_t beep_msg[] = {posix_lo8(500), posix_hi8(500)};
	serial.send_cmd(tyahboomserial::FUNC_BEEP, beep_msg, sizeof(beep_msg) / sizeof(beep_msg[0]));

	ros::WallDuration timeout(0.1f);
	while (!exit && ros::ok()) {
		serial.pool_read();
		cbqueue.callAvailable(timeout);
	}

	// rose_ros::sub_scan.shutdown();

    return 0;
}