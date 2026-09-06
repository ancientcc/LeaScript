#ifndef LIBROS_ROS_FOLLOW_HPP
#define LIBROS_ROS_FOLLOW_HPP

#include <ros/common.h>

#include "rose_util.hpp"
#include "rose_thread.hpp"

#include <opencv2/opencv.hpp>

namespace ros {

class ROSCPP_DECL tfollow
{
public:
	tfollow();

	~tfollow();

	void set_new_goal(const SDL_DPoint& goal);
	SDL_DPoint get_new_goal();

	// qrcode
	void qrcode_did_corners(const SDL_Point& size, const std::vector<cv::Point>& corners);

private:
	SDL_DPoint goal_;
	bool goal_dirty_;

	threading::mutex mutex_;
};

extern ROSVARIABLE_DECL tfollow follow;

} // namespace ros

#endif // LIBROS_ROS_FOLLOW_HPP
