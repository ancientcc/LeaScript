
#include "rose_ros/follow.hpp"
#include <angles/angles.h>

#include "rose_exception.hpp"
#include <SDL_timer.h>
#include <SDL_log.h>
#include <rose_ros/utils.hpp>

namespace ros {

tfollow::tfollow()
	: goal_(SDL_DPoint{float_nposm, float_nposm})
	, goal_dirty_(false)
{
}

tfollow::~tfollow()
{
	
}

void tfollow::set_new_goal(const SDL_DPoint& goal)
{
	VALIDATE(goal.x <= fabs(1.5) && goal.y <= fabs(1.5), null_str);

	threading::lock lock(mutex_);

	goal_ = goal_;
	goal_dirty_ = true;
}

SDL_DPoint tfollow::get_new_goal()
{
	SDL_DPoint result{float_nposm, float_nposm};

	threading::lock lock(mutex_);
	if (goal_dirty_) {
		goal_dirty_ = false;
		result = goal_;
	}
	return result;
}

void tfollow::qrcode_did_corners(const SDL_Point& size, const std::vector<cv::Point>& corners)
{
	VALIDATE(corners.size() >= 4, null_str);

	int min_x = INT32_MAX;
    int min_y = INT32_MAX;
    int max_x = INT32_MIN;
    int max_y = INT32_MIN;
	for (std::vector<cv::Point>::const_iterator it = corners.begin(); it != corners.end(); ++ it) {
		const cv::Point& point = *it;
		posix_touch_i32(point.x, point.y, &min_x, &min_y, &max_x, &max_y);

	}

	cv::Rect rect = cv::boundingRect(corners);
	SDL_DPoint center{rect.x + rect.width / 2.0, rect.y + rect.height / 2.0};

	SDL_DPoint center2{min_x + (max_x - min_x) / 2.0, min_y + (max_y - min_y) / 2.0};

	double reality_size = 30; // mm
	double photo_size = (rect.width + rect.height) / 2;

	const double x_dist = 0.45; // 45cm
	const double fov = DEG2RAD(64);

	const double full_y_dist_half = hypot(x_dist, x_dist) / 2;

	int half_photo_width = size.x / 2;
	// Xr / Xp = Yr / Yp
	double ratio = center.x / half_photo_width;
	double y_dist = full_y_dist_half * ratio;
	if (center.x <= half_photo_width) {
		// left
		y_dist *= -1;
	}
	set_new_goal(SDL_DPoint{x_dist, y_dist});
}

tfollow follow;

} // namespace ros