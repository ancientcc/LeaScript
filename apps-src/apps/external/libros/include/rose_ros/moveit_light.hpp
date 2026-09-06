#ifndef LIBROS_MOVEIT_LIGHT_HPP
#define LIBROS_MOVEIT_LIGHT_HPP

#include <ros/common.h>

#include "rose_util.hpp"
#include "rose_thread.hpp"

#include <opencv2/opencv.hpp>

#include "moveit/moveit_decl.h"
#include <moveit/ompl_interface/parameterization/model_based_state_space_factory.h>

#include "ompl/base/SpaceInformation.h"

namespace moveit {

class MOVEIT_DECL tlight_interface
{
public:
	tlight_interface(moveit::core::RobotModelConstPtr& robot_model, robot_state::RobotState& robot_state, const ros::Publisher& pub);

	~tlight_interface();

	void joint_value_target(const std::string& group_name, const std::vector<double>& joint_values);
	void execTrajectory(const moveit_msgs::RobotTrajectory& t);

	std::vector<double> joint_pose_target(const std::string& group_name, const tpose3d& target_pose, const tpose3d& bounds, bool use_as_seed);

	// qrcode
	void qrcode_did_corners(const SDL_Point& size, const std::vector<cv::Point>& corners);

private:
	bool cancelled() const { return cancel_; }

private:
	moveit::core::RobotModelConstPtr robot_model_;
	robot_state::RobotState& robot_state_;
	const ros::Publisher& pub_;

	ompl_interface::ModelBasedStateSpaceFactoryPtr state_space_factory_;
	ompl::base::SpaceInformationPtr si_;

	bool cancel_;
	threading::mutex mutex_;
};

} // namespace moveit



#endif // LIBROS_MOVEIT_LIGHT_HPP
