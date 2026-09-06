/*********************************************************************
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2017, Open Source Robotics Foundation, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Morgan Quigley
 *********************************************************************/

#ifndef ROSE_COST_FUNCTION_H
#define ROSE_COST_FUNCTION_H

#include <base_local_planner/trajectory_cost_function.h>
#include <base_local_planner/local_planner_limits.h>
#include <costmap_2d/costmap_2d.h>
#include <geometry_msgs/PoseStamped.h>

#include "rose_util.hpp"

namespace base_local_planner {

/**
 * This class provides a cost based on how much a robot "twirls" on its
 * way to the goal. With differential-drive robots, there isn't a choice,
 * but with holonomic or near-holonomic robots, sometimes a robot spins
 * more than you'd like on its way to a goal. This class provides a way
 * to assign a penalty purely to rotational velocities.
 */
class RoseCostFunction: public base_local_planner::TrajectoryCostFunction
{
public:

	RoseCostFunction();
	~RoseCostFunction() {}

	void setOneMisc(const geometry_msgs::PoseStamped& global_pose, const std::vector<geometry_msgs::PoseStamped>& global_plan,
		const base_local_planner::LocalPlannerLimits& limits, costmap_2d::Costmap2D& costmap);

	double scoreTrajectory(Trajectory &traj) override;
	bool prepare() override {return true;};

private:
	void adjust_plan_resolution(const std::vector<tpose2d_C>& global_plan_in, std::vector<tpose2d_C>& global_plan_out, double resolution);
	void adjust_plan_cell(const std::vector<tpose2d_C>& global_plan_in, std::vector<tpose2d_C>& global_plan_out, const costmap_2d::Costmap2D& costmap);

private:
	tpose2d robot_pose2d_;
	std::vector<geometry_msgs::PoseStamped> global_plan_;
	costmap_2d::Costmap2D* costmap_;
	base_local_planner::LocalPlannerLimits limits_;

	std::vector<tpose2d_C> global_plan_2d_;
	std::vector<tpose2d_C> global_plan_cell_;
};

} /* namespace base_local_planner */
#endif /* TWIRLING_COST_FUNCTION_H_ */
