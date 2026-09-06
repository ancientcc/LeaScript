
#include "rose_ros/moveit_light.hpp"
#include <angles/angles.h>

#include "rose_exception.hpp"
#include <SDL_timer.h>
#include <SDL_log.h>
#include <rose_ros/utils.hpp>

#include <moveit/ompl_interface/parameterization/joint_space/joint_model_state_space_factory.h>
#include <moveit/ompl_interface/parameterization/joint_space/joint_model_state_space.h>
#include <moveit/ompl_interface/parameterization/work_space/pose_model_state_space_factory.h>

#include <moveit/ompl_interface/detail/state_validity_checker.h>
#include <ompl/geometric/PathGeometric.h>

#include <moveit/trajectory_processing/iterative_spline_parameterization.h>

#include <tf_conversions/tf_kdl.h>
#include <kdl/tree.hpp>
#include <kdl_parser/kdl_parser.hpp>
#include <trac_ik/trac_ik.hpp>
#include <kdl/chain.hpp>

#include <tf2/LinearMath/Quaternion.h>
#include <nlopt.h>

extern bool trac_ik_sdl_log;

namespace moveit {

tlight_interface::tlight_interface(moveit::core::RobotModelConstPtr& robot_model, robot_state::RobotState& robot_state, const ros::Publisher& pub)
	: robot_model_(robot_model)
	, robot_state_(robot_state)
	, pub_(pub)
	, cancel_(false)
{
	ompl_interface::ModelBasedStateSpaceFactoryPtr factory;
	state_space_factory_.reset(new ompl_interface::JointModelStateSpaceFactory());
}

tlight_interface::~tlight_interface()
{
	
}

void tlight_interface::joint_value_target(const std::string& group_name, const std::vector<double>& joint_values)
{
	const moveit::core::RobotModel& robot_model = *robot_model_.get();
	const moveit::core::JointModelGroup* joint_model_group = robot_model.getJointModelGroup(group_name);
	VALIDATE(joint_model_group != nullptr, null_str);

	const std::vector<int>& variable_index_list = joint_model_group->getVariableIndexList();
	VALIDATE(variable_index_list.size() == joint_values.size(), null_str);

	ompl_interface::ModelBasedStateSpaceSpecification space_spec(robot_model_, group_name);
	ompl_interface::ModelBasedStateSpacePtr state_space_ = state_space_factory_->getNewStateSpace(space_spec);

	si_ = std::make_shared<ompl::base::SpaceInformation>(state_space_);
	// si_->setStateValidityChecker(ob::StateValidityCheckerPtr(new ompl_interface::StateValidityChecker(this)));
	// ompl_interface::ModelBasedPlanningContext::useConfig()
	si_->setup();
	std::map<std::string, std::string> cfg;
	cfg.insert(std::make_pair("longest_valid_segment_fraction", "0.005"));
	si_->params().setParams(cfg, true);
	si_->setup();

	ompl::base::State* state1_0 = si_->allocState();
	ompl_interface::ModelBasedStateSpace::StateType* state1 = state1_0->as<ompl_interface::ModelBasedStateSpace::StateType>();

	// stat1: from
	for (int at = 0; at < (int)variable_index_list.size(); at ++) {
		state1->values[at] = robot_state_.getVariablePositions()[variable_index_list[at]];
	}

	// for (int at = 0; at < joint_model_group->getVariableCount(); at ++) {
	// }
	// state1->values[0] = 0.0;
	// state1->values[1] = 0.1;
	// state1->values[2] = 0;
/*
	state1->values[0] = robot_state_.getVariablePositions()[0];
	state1->values[1] = robot_state_.getVariablePositions()[1];
	state1->values[2] = robot_state_.getVariablePositions()[2];
*/
	ompl::base::State* state2_0 = si_->allocState();
	ompl_interface::ModelBasedStateSpace::StateType* state2 = state2_0->as<ompl_interface::ModelBasedStateSpace::StateType>();
	// state2->values[0] = 0.5;
	// state2->values[1] = 0.6;
	// state2->values[2] = -0.7;

	// stat1: from
	for (int at = 0; at < (int)joint_values.size(); at ++) {
		double value = joint_values[at];
		state2->values[at] = value;
	}
/*
	VALIDATE(joint_values.size() == 3, null_str);
	state2->values[0] = joint_values[0];
	state2->values[1] = joint_values[1];
	state2->values[2] = joint_values[2];
*/
	ompl::geometric::PathGeometricPtr path(std::make_shared<ompl::geometric::PathGeometric>(si_));
	path->getStates().push_back(state1);
	path->getStates().push_back(state2);

	// below is copy from ompl_interface::ModelBasedPlanningContext::interpolateSolution()
	ompl::geometric::PathGeometric& pg = *path.get();

    // Find the number of states that will be in the interpolated solution.
    // This is what interpolate() does internally.
    unsigned int eventual_states = 1;
    std::vector<ompl::base::State*>& states = pg.getStates();
    for (size_t i = 0; i < states.size() - 1; i++) {
      eventual_states += si_->getStateSpace()->validSegmentCount(states[i], states[i + 1]);
    }
    if (eventual_states >= 200) {
        // ik_arm, joint_values(-1.2248325209190654, 1.5700000524520874, -2578.3107910156250)
        //  ===> eventual_states(88738)
        char err[128];
        SDL_snprintf(err, sizeof(err), "states.size: %i, eventual_states: %i", (int)states.size(), (int)eventual_states);
        VALIDATE(false, err);
    }

	unsigned int minimum_waypoint_count_ = 2;
	if (eventual_states < minimum_waypoint_count_)
    {
      // If that's not enough states, use the minimum amount instead.
      pg.interpolate(minimum_waypoint_count_);
    } else {
      // Interpolate the path to have as the exact states that are checked when validating motions.
      pg.interpolate();
    }

	const bool verbose = false;
	int at = 0;
	if (verbose) {
		ROS_INFO("---{light moveit}post pg.interpolate(), size: %i---", (int)states.size());
		for (std::vector<ompl::base::State*>::const_iterator it = states.begin(); it != states.end(); ++ it, at ++) {
			ompl_interface::ModelBasedStateSpace::StateType* state = (*it)->as<ompl_interface::ModelBasedStateSpace::StateType>();
			SDL_Log("[%i/%i]{1/3} (%.6f, %.6f %.6f)", at, (int)states.size(), state->values[0], state->values[1], state->values[2]);
		}
	}
/*
void ompl_interface::ModelBasedPlanningContext::convertPath(const ompl::geometric::PathGeometric& pg,
                                                            robot_trajectory::RobotTrajectory& traj) const
{
  moveit::core::RobotState ks = complete_initial_robot_state_;
  for (std::size_t i = 0; i < pg.getStateCount(); ++i)
  {
    spec_.state_space_->copyToRobotState(ks, pg.getState(i));
    traj.addSuffixWayPoint(ks, 0.0);
  }
}
*/

	robot_trajectory::RobotTrajectory traj(robot_model_, group_name);

	moveit::core::RobotState complete_initial_robot_state_ = state_space_->getRobotModel();
	moveit::core::RobotState ks = complete_initial_robot_state_;
	for (std::size_t i = 0; i < pg.getStateCount(); ++i)
	{
		state_space_->copyToRobotState(ks, pg.getState(i));
		traj.addSuffixWayPoint(ks, 0.0);
	}

	if (verbose) {
		for (at = 0; at < (int)traj.getWayPointCount(); at ++) {
			const moveit::core::RobotState& way_point = traj.getWayPoint(at);
			const double* positions = way_point.getVariablePositions();
			SDL_Log("[%i/%i]{2/3} (%.6f, %.6f %.6f)", at, (int)traj.getWayPointCount(), positions[0], positions[1], positions[2]);
		}
	}

	robot_trajectory::RobotTrajectory& trajectory_ = traj;
	double max_velocity_scaling_factor = 0;
	double max_acceleration_scaling_factor = 0;

	// copy from 
	trajectory_processing::IterativeSplineParameterization time_param_;
	if (!time_param_.computeTimeStamps(trajectory_, max_velocity_scaling_factor, max_acceleration_scaling_factor)) {
        ROS_ERROR("Time parametrization for the solution path failed.");
    }

	if (verbose) {
		for (at = 0; at < (int)traj.getWayPointCount(); at ++) {
			const moveit::core::RobotState& way_point = traj.getWayPoint(at);
			const double* positions = way_point.getVariablePositions();
			SDL_Log("[%i/%i]{3/3} (%.6f, %.6f %.6f)", at, (int)traj.getWayPointCount(), positions[0], positions[1], positions[2]);
		}
	}

	moveit_msgs::RobotTrajectory trajectory_msg;
	traj.getRobotTrajectoryMsg(trajectory_msg);

	execTrajectory(trajectory_msg);
}

void tlight_interface::execTrajectory(const moveit_msgs::RobotTrajectory& t)
{
  // copy from: void ViaPointController::execTrajectory(const moveit_msgs::RobotTrajectory& t)
  ROS_INFO("[ViaPoint]Fake execution of trajectory");
  sensor_msgs::JointState js;
  js.header = t.joint_trajectory.header;
  js.name = t.joint_trajectory.joint_names;

  // publish joint states for all intermediate via points of the trajectory
  // no further interpolation
  ros::Time start_time = ros::Time::now();
  for (std::vector<trajectory_msgs::JointTrajectoryPoint>::const_iterator via = t.joint_trajectory.points.begin(),
                                                                          end = t.joint_trajectory.points.end();
       !cancelled() && via != end; ++via)
  {
    js.position = via->positions;
    js.velocity = via->velocities;
    js.effort = via->effort;

    ros::Duration wait_time = via->time_from_start - (ros::Time::now() - start_time);
    // ROS_INFO("via->time_from_start: %.3f", via->time_from_start.toSec());
    if (wait_time.toSec() > std::numeric_limits<float>::epsilon())
    {
      // ROS_INFO("Fake execution: waiting %0.1fs for next via point, %ld remaining", wait_time.toSec(), end - via);
      wait_time.sleep();
    }
    js.header.stamp = ros::Time::now();
    pub_.publish(js);
  }
  ROS_INFO("[ViaPoint]Fake execution of trajectory: done");
}

void tlight_interface::qrcode_did_corners(const SDL_Point& size, const std::vector<cv::Point>& corners)
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
}

} // namespace moveit

/*
 * @enum DiscretizationMethods
 *
 * @brief Flags for choosing the type discretization method applied on the redundant joints during an ik query
 */
namespace DiscretizationMethods
{
enum DiscretizationMethod
{
  NO_DISCRETIZATION = 1, /**< The redundant joints will be fixed at their current value. */
  ALL_DISCRETIZED,       /**< All redundant joints will be discretized uniformly */
  SOME_DISCRETIZED, /**< Some redundant joints will be discretized uniformly. The unused redundant joints will be fixed
                       at their
                         current value */
  ALL_RANDOM_SAMPLED, /**< the discretization for each redundant joint will be randomly generated.*/
  SOME_RANDOM_SAMPLED /**< the discretization for some redundant joint will be randomly generated.
                           The unused redundant joints will be fixed at their current value. */
};
}  // namespace DiscretizationMethods
using DiscretizationMethod = DiscretizationMethods::DiscretizationMethod;

/*
 * @enum KinematicErrors
 * @brief Kinematic error codes that occur in a ik query
 */
namespace KinematicErrors
{
enum KinematicError
{
  OK = 1,                              /**< No errors*/
  UNSUPORTED_DISCRETIZATION_REQUESTED, /**< Discretization method isn't supported by this implementation */
  DISCRETIZATION_NOT_INITIALIZED,      /**< Discretization values for the redundancy has not been set. See
                                            setSearchDiscretization(...) method*/
  MULTIPLE_TIPS_NOT_SUPPORTED,         /**< Only single tip link support is allowed */
  EMPTY_TIP_POSES,                     /**< Empty ik_poses array passed */
  IK_SEED_OUTSIDE_LIMITS,              /**< Ik seed is out of bounds*/
  SOLVER_NOT_ACTIVE,                   /**< Solver isn't active */
  NO_SOLUTION                          /**< A valid joint solution that can reach this pose(s) could not be found */

};
}  // namespace KinematicErrors
using KinematicError = KinematicErrors::KinematicError;

/**
 * @struct KinematicsQueryOptions
 * @brief A set of options for the kinematics solver
 */
struct KinematicsQueryOptions
{
  KinematicsQueryOptions()
    : lock_redundant_joints(false)
    , return_approximate_solution(false)
    , discretization_method(DiscretizationMethods::NO_DISCRETIZATION)
  {
  }

  bool lock_redundant_joints;                 /**<  KinematicsQueryOptions#lock_redundant_joints. */
  bool return_approximate_solution;           /**<  KinematicsQueryOptions#return_approximate_solution. */
  DiscretizationMethod discretization_method; /**<  Enumeration value that indicates the method for discretizing the
                                                    redundant. joints KinematicsQueryOptions#discretization_method. */
};

/*
 * @struct KinematicsResult
 * @brief Reports result details of an ik query
 *
 * This struct is used as an output argument of the getPositionIK(...) method that returns multiple joint solutions.
 * It contains the type of error that led to a failure or KinematicErrors::OK when a set of joint solutions is found.
 * The solution percentage shall provide a ratio of solutions found over solutions searched.
 *
 */
struct KinematicsResult
{
  KinematicError kinematic_error; /**< Error code that indicates the type of failure */
  double solution_percentage;     /**< The percentage of solutions achieved over the total number
                                       of solutions explored. */
};

/**
 * @class KinematicsBase
 * @brief Provides an interface for kinematics solvers.
 */
class KinematicsBase
{
public:
  static MOVEIT_KINEMATICS_BASE_EXPORT const double DEFAULT_SEARCH_DISCRETIZATION; /* = 0.1 */
  static MOVEIT_KINEMATICS_BASE_EXPORT const double DEFAULT_TIMEOUT;               /* = 1.0 */

  /**
   * @brief Set the parameters for the solver, for use with kinematic chain IK solvers
   * @param robot_description This parameter can be used as an identifier for the robot kinematics it is computed for;
   * For example, the name of the ROS parameter that contains the robot description;
   * @param group_name The group for which this solver is being configured
   * @param base_frame The base frame in which all input poses are expected.
   * This may (or may not) be the root frame of the chain that the solver operates on
   * @param tip_frame The tip of the chain
   * @param search_discretization The discretization of the search when the solver steps through the redundancy
   */
  /* Replace by tip_frames-based method! */
  [[deprecated]] virtual void setValues(const std::string& group_name,
                                        const std::string& base_frame, const std::string& tip_frame,
                                        double search_discretization);

  /**
   * @brief Set the parameters for the solver, for use with non-chain IK solvers
   * @param robot_description This parameter can be used as an identifier for the robot kinematics it is computed for;
   * For example, the name of the ROS parameter that contains the robot description;
   * @param group_name The group for which this solver is being configured
   * @param base_frame The base frame in which all input poses are expected.
   * This may (or may not) be the root frame of the chain that the solver operates on
   * @param tip_frames A vector of tips of the kinematic tree
   * @param search_discretization The discretization of the search when the solver steps through the redundancy
   */
  virtual void setValues(const std::string& group_name,
                         const std::string& base_frame, const std::vector<std::string>& tip_frames,
                         double search_discretization);

  /**
   * @brief Set a set of redundant joints for the kinematics solver to use.
   * This can fail, depending on the IK solver and choice of redundant joints!. Also, it sets
   * the discretization values for each redundant joint to a default value.
   * @param redundant_joint_indices The set of redundant joint indices
   *        (corresponding to the list of joints you get from getJointNames()).
   * @return False if any of the input joint indices are invalid (exceed number of joints)
   */
  virtual bool setRedundantJoints(const std::vector<unsigned int>& redundant_joint_indices);

  /**
   * @brief Set a set of redundant joints for the kinematics solver to use.
   * This function is just a convenience function that calls the previous definition of setRedundantJoints()
   * @param redundant_joint_names The set of redundant joint names.
   * @return False if any of the input joint indices are invalid (exceed number of joints)
   */
  bool setRedundantJoints(const std::vector<std::string>& redundant_joint_names);

  /**
   * @brief Get the set of redundant joints
   */
  virtual void getRedundantJoints(std::vector<unsigned int>& redundant_joint_indices) const
  {
    redundant_joint_indices = redundant_joint_indices_;
  }

  /**
   * @brief  Return all the joint names in the order they are used internally
   */
  virtual const std::vector<std::string>& getJointNames() const = 0;

  /**
   * @brief  Return all the link names in the order they are represented internally
   */
  virtual const std::vector<std::string>& getLinkNames() const = 0;

  /**
   * \brief Check if this solver supports a given JointModelGroup.
   *
   * Override this function to check if your kinematics solver
   * implementation supports the given group.
   *
   * The default implementation just returns jmg->isChain(), since
   * solvers written before this function was added all supported only
   * chain groups.
   *
   * \param jmg the planning group being proposed to be solved by this IK solver
   * \param error_text_out If this pointer is non-null and the group is
   *          not supported, this is filled with a description of why it's not
   *          supported.
   * \return True if the group is supported, false if not.
   */
  virtual bool supportsGroup(const moveit::core::JointModelGroup* jmg, std::string* error_text_out = nullptr) const;

  /**
   * @brief  Set the search discretization value for all the redundant joints
   */
  void setSearchDiscretization(double sd)
  {
    redundant_joint_discretization_.clear();
    for (unsigned int index : redundant_joint_indices_)
      redundant_joint_discretization_[index] = sd;
  }

  /**
   * @brief Sets individual discretization values for each redundant joint.
   *
   * Calling this method replaces previous discretization settings.
   *
   * @param discretization a map of joint indices and discretization value pairs.
   */
  void setSearchDiscretization(const std::map<int, double>& discretization)
  {
    redundant_joint_discretization_.clear();
    redundant_joint_indices_.clear();
    for (const auto& pair : discretization)
    {
      redundant_joint_discretization_.insert(pair);
      redundant_joint_indices_.push_back(pair.first);
    }
  }

  /**
   * @brief  Get the value of the search discretization
   */
  double getSearchDiscretization(int joint_index = 0) const
  {
    if (redundant_joint_discretization_.count(joint_index) > 0)
    {
      return redundant_joint_discretization_.at(joint_index);
    }
    else
    {
      return 0.0;  // returned when there aren't any redundant joints
    }
  }

  /**
   * @brief Returns the set of supported kinematics discretization search types.  This implementation only supports
   * the DiscretizationMethods::ONE search.
   */
  std::vector<DiscretizationMethod> getSupportedDiscretizationMethods() const
  {
    return supported_methods_;
  }

  /** @brief For functions that require a timeout specified but one is not specified using arguments,
      a default timeout is used, as set by this function (and initialized to KinematicsBase::DEFAULT_TIMEOUT) */
  void setDefaultTimeout(double timeout)
  {
    default_timeout_ = timeout;
  }

  /** @brief For functions that require a timeout specified but one is not specified using arguments,
      this default timeout is used */
  double getDefaultTimeout() const
  {
    return default_timeout_;
  }

  /**
   * @brief  Virtual destructor for the interface
   */
  virtual ~KinematicsBase();

  KinematicsBase();

protected:
  std::string group_name_;
  std::string base_frame_;
  std::vector<std::string> tip_frames_;

  // The next two variables still exists for backwards compatibility
  // with previously generated custom ik solvers like IKFast
  // Replace tip_frame_ -> tip_frames_[0], search_discretization_ -> redundant_joint_discretization_
  [[deprecated]] std::string tip_frame_;
  [[deprecated]] double search_discretization_;

  double default_timeout_;
  std::vector<unsigned int> redundant_joint_indices_;
  std::map<int, double> redundant_joint_discretization_;
  std::vector<DiscretizationMethod> supported_methods_;

private:
  std::string removeSlash(const std::string& str) const;
};

static const std::string LOGNAME1 = "kinematics_base";

const double KinematicsBase::DEFAULT_SEARCH_DISCRETIZATION = 0.1;
const double KinematicsBase::DEFAULT_TIMEOUT = 1.0;

void KinematicsBase::setValues(const std::string& group_name,
                               const std::string& base_frame, const std::vector<std::string>& tip_frames,
                               double search_discretization)
{
  group_name_ = group_name;
  base_frame_ = removeSlash(base_frame);
  tip_frames_.clear();
  for (const std::string& name : tip_frames)
    tip_frames_.push_back(removeSlash(name));
  setSearchDiscretization(search_discretization);

  // store deprecated values for backwards compatibility
  search_discretization_ = search_discretization;
  if (tip_frames_.size() == 1)
    tip_frame_ = tip_frames_[0];
  else
    tip_frame_.clear();
}

void KinematicsBase::setValues(const std::string& group_name,
                               const std::string& base_frame, const std::string& tip_frame,
                               double search_discretization)
{
  setValues(group_name, base_frame, std::vector<std::string>({ tip_frame }), search_discretization);
}

bool KinematicsBase::setRedundantJoints(const std::vector<unsigned int>& redundant_joint_indices)
{
  for (const unsigned int& redundant_joint_index : redundant_joint_indices)
  {
    if (redundant_joint_index >= getJointNames().size())
    {
      return false;
    }
  }
  redundant_joint_indices_ = redundant_joint_indices;
  setSearchDiscretization(DEFAULT_SEARCH_DISCRETIZATION);

  return true;
}

bool KinematicsBase::setRedundantJoints(const std::vector<std::string>& redundant_joint_names)
{
  const std::vector<std::string>& jnames = getJointNames();
  std::vector<unsigned int> redundant_joint_indices;
  for (const std::string& redundant_joint_name : redundant_joint_names)
    for (std::size_t j = 0; j < jnames.size(); ++j)
      if (jnames[j] == redundant_joint_name)
      {
        redundant_joint_indices.push_back(j);
        break;
      }
  return redundant_joint_indices.size() == redundant_joint_names.size() ? setRedundantJoints(redundant_joint_indices) :
                                                                          false;
}

std::string KinematicsBase::removeSlash(const std::string& str) const
{
  return (!str.empty() && str[0] == '/') ? removeSlash(str.substr(1)) : str;
}

bool KinematicsBase::supportsGroup(const moveit::core::JointModelGroup* jmg, std::string* error_text_out) const
{
  // Default implementation for legacy solvers:
  if (!jmg->isChain())
  {
    if (error_text_out)
    {
      *error_text_out = "This plugin only supports joint groups which are chains";
    }
    return false;
  }

  return true;
}

KinematicsBase::KinematicsBase()
  : tip_frame_("DEPRECATED")
  // help users understand why this variable might not be set
  // (if multiple tip frames provided, this variable will be unset)
  , search_discretization_(DEFAULT_SEARCH_DISCRETIZATION)
  , default_timeout_(DEFAULT_TIMEOUT)
{
  supported_methods_.push_back(DiscretizationMethods::NO_DISCRETIZATION);
}

KinematicsBase::~KinematicsBase() = default;

// class TRAC_IKKinematicsPlugin : public kinematics::KinematicsBase
class TRAC_IKKinematicsPlugin : public KinematicsBase
{
  std::vector<std::string> joint_names_;
  std::vector<std::string> link_names_;

  unsigned int num_joints_;
  bool active_; // Internal variable that indicates whether solvers are configured and ready

  KDL::Chain chain;

  KDL::JntArray joint_min, joint_max;

  std::string solve_type;

public:
  const std::vector<std::string>& getJointNames() const
  {
    return joint_names_;
  }
  const std::vector<std::string>& getLinkNames() const
  {
    return link_names_;
  }


  /** @class
   *  @brief Interface for an TRAC-IK kinematics plugin
   */
  TRAC_IKKinematicsPlugin(): active_(false) {}

  ~TRAC_IKKinematicsPlugin()
  {
  }



  /**
   * @brief Given a desired pose of the end-effector, search for the joint angles required to reach it.
   * This particular method is intended for "searching" for a solutions by stepping through the redundancy
   * (or other numerical routines).
   * @param ik_pose the desired pose of the link
   * @param ik_seed_state an initial guess solution for the inverse kinematics
   * @return True if a valid solution was found, false otherwise
   */

  bool searchPositionIK(const geometry_msgs::Pose &ik_pose,
                        const std::vector<double> &ik_seed_state,
                        double timeout,
                        std::vector<double> &solution,
                        moveit_msgs::MoveItErrorCodes &error_code,
                        const tpose3d& bounds,
                        const KinematicsQueryOptions &options) const;


  bool initialize(const urdf::ModelInterface & robot_model,
                  const std::string& group_name,
                  const std::string& base_name,
                  const std::string& tip_name,
                  double search_discretization);

}; // end class


// see: https://blog.csdn.net/qq_32761549/article/details/119911096
double myfunc(unsigned n, const double *x, double *grad, void *my_func_data)
{
    if (grad) {
        grad[0] = 0.0;
        grad[1] = 0.5 / sqrt(x[1]);
    }
    return sqrt(x[1]);
}

typedef struct {
    double a, b;
} my_constraint_data;

double myconstraint(unsigned n, const double *x, double *grad, void *data)
{
    my_constraint_data *d = (my_constraint_data *) data;
    double a = d->a, b = d->b;
    if (grad) {
        grad[0] = 3 * a * (a*x[0] + b) * (a*x[0] + b);
        grad[1] = -1.0;
    }
    return ((a*x[0] + b) * (a*x[0] + b) * (a*x[0] + b) - x[1]);
}

void test_nlopt()
{
    nlopt_opt opt;
    opt = nlopt_create(NLOPT_LD_MMA, 2);

    double lb[2] = { -HUGE_VAL, 0 };
    nlopt_set_lower_bounds(opt, lb);

    double ub[2] = { 5, 10 };
    nlopt_set_upper_bounds(opt, ub );

    nlopt_set_min_objective(opt, myfunc, NULL);

    my_constraint_data data[2] = { {2,0}, {-1,1} };
    nlopt_add_inequality_constraint(opt, myconstraint, &data[0], 1e-8);
    nlopt_add_inequality_constraint(opt, myconstraint, &data[1], 1e-8);

    nlopt_set_xtol_rel(opt, 1e-4);

    double x[2] = { 1.234, 5.678 };  // x's initial value
    double minf; // calculated min value save to it
    if (nlopt_optimize(opt, x, &minf) < 0) {
        SDL_Log("{test_nlopt}nlopt failed!");
    } else {
        SDL_Log("{test_nlopt}found minimum at f(%.5f, %.5f) = %0.10f", x[0], x[1], minf);
    }
    nlopt_destroy(opt);
}

bool TRAC_IKKinematicsPlugin::initialize(const urdf::ModelInterface & robot_model,
    const std::string& group_name,
    const std::string& base_name,
    const std::string& tip_name,
    double search_discretization)
{
    if (game_config::os == os_windows) {
        // test_nlopt();
    }

  std::vector<std::string> tip_names = {tip_name};
  setValues(group_name, base_name, tip_names, search_discretization);

  ROS_DEBUG_STREAM_NAMED("trac_ik", "Reading joints and links from URDF");

  KDL::Tree tree;

  if (!kdl_parser::treeFromUrdfModel(robot_model, tree))
  {
    ROS_FATAL("Failed to extract kdl tree from xml robot description");
    return false;
  }

  if (!tree.getChain(base_name, tip_name, chain))
  {
    ROS_FATAL("Couldn't find chain %s to %s", base_name.c_str(), tip_name.c_str());
    return false;
  }

  num_joints_ = chain.getNrOfJoints();

  std::vector<KDL::Segment> chain_segs = chain.segments;

  urdf::JointConstSharedPtr joint;

  std::vector<double> l_bounds, u_bounds;

  joint_min.resize(num_joints_);
  joint_max.resize(num_joints_);

  uint joint_num = 0;
  for (unsigned int i = 0; i < chain_segs.size(); ++i)
  {

    link_names_.push_back(chain_segs[i].getName());
    joint = robot_model.getJoint(chain_segs[i].getJoint().getName());
    if (joint->type != urdf::Joint::UNKNOWN && joint->type != urdf::Joint::FIXED)
    {
      joint_num++;
      assert(joint_num <= num_joints_);
      float lower, upper;
      int hasLimits;
      joint_names_.push_back(joint->name);
      if (joint->type != urdf::Joint::CONTINUOUS)
      {
        if (joint->safety)
        {
          lower = std::max(joint->limits->lower, joint->safety->soft_lower_limit);
          upper = std::min(joint->limits->upper, joint->safety->soft_upper_limit);
        }
        else
        {
          lower = joint->limits->lower;
          upper = joint->limits->upper;
        }
        hasLimits = 1;
      }
      else
      {
        hasLimits = 0;
      }
      if (hasLimits)
      {
        joint_min(joint_num - 1) = lower;
        joint_max(joint_num - 1) = upper;
      }
      else
      {
        joint_min(joint_num - 1) = std::numeric_limits<float>::lowest();
        joint_max(joint_num - 1) = std::numeric_limits<float>::max();
      }
      if (trac_ik_sdl_log) {
        ROS_INFO_STREAM("IK Using joint " << chain_segs[i].getName() << " " << joint_min(joint_num - 1) << " " << joint_max(joint_num - 1));
      }
    }
  }

  solve_type = "Speed";

  active_ = true;
  return true;
}

bool TRAC_IKKinematicsPlugin::searchPositionIK(const geometry_msgs::Pose &ik_pose,
    const std::vector<double> &ik_seed_state,
    double timeout,
    std::vector<double> &solution,
    moveit_msgs::MoveItErrorCodes &error_code,
    const tpose3d& _bounds,
    const KinematicsQueryOptions &options) const
{
    VALIDATE(active_, null_str);

    if (ik_seed_state.size() != num_joints_) {
        ROS_ERROR_STREAM_NAMED("trac_ik", "Seed state must have size " << num_joints_ << " instead of size " << ik_seed_state.size());
        error_code.val = error_code.NO_IK_SOLUTION;
        return false;
    }

    KDL::Frame frame;
    tf::poseMsgToKDL(ik_pose, frame);

    KDL::JntArray in(num_joints_), out(num_joints_);

    for (uint z = 0; z < num_joints_; z++) {
        in(z) = ik_seed_state[z];
    }

    // delta_twist: [   0.0163263,      -0.023,    0.016839,   -0.618564,    -1.25479,    0.767303]
    // KDL::Twist bounds2 = KDL::Twist::Zero();
    KDL::Twist bounds(KDL::Vector(_bounds.x, _bounds.y, _bounds.z), KDL::Vector(_bounds.roll, _bounds.pitch, _bounds.yaw));

    if (trac_ik_sdl_log) {
        ROS_INFO("bounds: [%.6f %.6f %.6f, %.6f %.6f(deg:%.5f) %.6f]", 
            bounds.vel.x(), bounds.vel.y(), bounds.vel.z(), bounds.rot.x(), bounds.rot.y(), RAD2DEG(bounds.rot.y()), bounds.rot.z());
    }

    double epsilon = 1e-5;  //Same as MoveIt's KDL plugin
    TRAC_IK::SolveType solvetype;

    if (solve_type == "Manipulation1") {
        solvetype = TRAC_IK::Manip1;
    } else if (solve_type == "Manipulation2") {
        solvetype = TRAC_IK::Manip2;
    } else if (solve_type == "Distance") {
        solvetype = TRAC_IK::Distance;
    } else {
        if (solve_type != "Speed") {
            ROS_WARN_STREAM_NAMED("trac_ik", solve_type << " is not a valid solve_type; setting to default: Speed");
        }
        solvetype = TRAC_IK::Speed;
    }

    TRAC_IK::TRAC_IK ik_solver(chain, joint_min, joint_max, timeout, epsilon, solvetype);

    int rc = ik_solver.CartToJnt(in, frame, out, bounds);
    VALIDATE(rc != 0, null_str);

    solution.resize(num_joints_);

    if (rc >= 0) {
        for (uint z = 0; z < num_joints_; z++) {
            solution[z] = out(z);
        }
        return true; // no collision check callback provided
    }

    error_code.val = moveit_msgs::MoveItErrorCodes::NO_IK_SOLUTION;
    return false;
}


namespace moveit {

std::vector<double> tlight_interface::joint_pose_target(const std::string& group_name, const tpose3d& target_pose, const tpose3d& bounds, bool use_as_seed)
{
    const moveit::core::JointModelGroup* const jmg = robot_model_->getJointModelGroup(group_name);

    // how to get base_link, tip_link, see:
    // {moveit_ros/planning/kinematics_plugin_loader/src/kinematics_plugin_loader.cpp}
    // kinematics::KinematicsBasePtr allocKinematicsSolver(const moveit::core::JointModelGroup* jmg)
    const std::vector<const moveit::core::LinkModel*>& links = jmg->getLinkModels();
    VALIDATE(!links.empty(), null_str);

    const std::string& base = links.front()->getParentJointModel()->getParentLinkModel() ?
                                  links.front()->getParentJointModel()->getParentLinkModel()->getName() :
                                  jmg->getParentModel().getModelFrame();
    const std::string base_link = (base.empty() || base[0] != '/') ? base : base.substr(1);

    const std::string tip_link = jmg->getLinkModels().back()->getName();


    TRAC_IKKinematicsPlugin plugin;

    double search_discretization = 0.005;
    plugin.initialize(*robot_model_->getURDF().get(), group_name, base_link, tip_link, search_discretization);

    geometry_msgs::Pose ik_query;
    ik_query.position.x = target_pose.x;
    ik_query.position.y = target_pose.y;
    ik_query.position.z = target_pose.z;

    tf2::Quaternion q;
    q.setRPY(target_pose.roll, target_pose.pitch, target_pose.yaw);
    ik_query.orientation.x = q.getX();
    ik_query.orientation.y = q.getY();
    ik_query.orientation.z = q.getZ();
    ik_query.orientation.w = q.getW();

    std::vector<double> seed;
    if (use_as_seed) {
        robot_state_.copyJointGroupPositions(jmg, seed);
    } else {
        // sample a seed value
        random_numbers::RandomNumberGenerator random_number_generator_;
        jmg->getVariableRandomPositions(random_number_generator_, seed);
    }
    // VALIDATE(seed.size() == jmp_-> .size(), null_str);

    // for (std::size_t i = 0; i < ik_joint_bijection.size(); ++i) {
        // seed[i] = vals[ik_joint_bijection[i]];
    // }

    double timeout = 0.2;
    std::vector<double> ik_sol;
    moveit_msgs::MoveItErrorCodes error;

    // kb_
    bool ret = plugin.searchPositionIK(ik_query, seed, timeout, ik_sol, error, bounds, KinematicsQueryOptions());
    if (!ret) {
        ik_sol.clear();
    }

    return ik_sol;
}

} // namespace moveit