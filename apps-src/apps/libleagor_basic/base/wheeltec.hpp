#ifndef LIBROS_WHEELTEC_HPP_INCLUDED
#define LIBROS_WHEELTEC_HPP_INCLUDED

#include "wheeltecserial.hpp"
#include <SDL_thread.h>

#include <ros/ros.h>

class twheeltec: public twheeltecserial
{
public:
	twheeltec(ros::NodeHandle& nh, const std::string& path, int baudrate);
	~twheeltec();

private:
	void send_cmd_timer_handler(const ::ros::TimerEvent& timer_event);
	void did_serial_geometry_msgs_Twist(const geometry_msgs::Twist& msg);
	void did_serial_sensor_msgs_JointState(const sensor_msgs::JointState& msg);

private:
	ros::NodeHandle& nh_;
	ros::Subscriber twist_sub_;
	ros::Subscriber joint_states_sub_;
	::ros::Timer send_cmd_timer_;
	SDL_threadID tid_;
};

#endif // LIBROS_WHEELTEC_HPP_INCLUDED
