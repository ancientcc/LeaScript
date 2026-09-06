#ifndef LIBROS_YAHBOOM_HPP_INCLUDED
#define LIBROS_YAHBOOM_HPP_INCLUDED

#include "yahboomserial.hpp"
#include <SDL_thread.h>

#include <ros/ros.h>
#include <geometry_msgs/Twist.h>

class tyahboom: public tyahboomserial
{
public:
	tyahboom(ros::NodeHandle& nh, const std::string& path, int baudrate);
	~tyahboom();

private:
	void send_cmd_timer_handler(const ::ros::TimerEvent& timer_event);
	void did_serial_geometry_msgs_Twist(const geometry_msgs::Twist& msg);

private:
	ros::NodeHandle& nh_;
	ros::Subscriber twist_sub_;
	::ros::Timer send_cmd_timer_;
	SDL_threadID tid_;

	int CAR_RUN_stop_threshold_;
	uint32_t next_REPORT_SPEED_ticks_;
	uint32_t next_CAR_RUN_stop_ticks_;
};

#endif // LIBROS_YAHBOOM_HPP_INCLUDED
