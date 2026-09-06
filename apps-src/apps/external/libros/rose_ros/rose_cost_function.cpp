/*
 * twirling_cost_function.cpp
 *
 *  Created on: Apr 20, 2016
 *      Author: Morgan Quigley
 */

#include <rose_ros/rose_cost_function.h>
#include "rose_exception.hpp"

#include <tf2/utils.h>
#include <angles/angles.h>
#include <math.h>

namespace base_local_planner {

RoseCostFunction::RoseCostFunction()
    : TrajectoryCostFunction(1.0, "RoseCost")
    , costmap_(nullptr)
{}

void RoseCostFunction::setOneMisc(const geometry_msgs::PoseStamped& global_pose, const std::vector<geometry_msgs::PoseStamped>& global_plan,
    const base_local_planner::LocalPlannerLimits& limits, costmap_2d::Costmap2D& costmap)
{
    VALIDATE(!global_plan.empty(), null_str);

    robot_pose2d_.x = global_pose.pose.position.x;
    robot_pose2d_.y = global_pose.pose.position.y;
    robot_pose2d_.yaw = tf2::getYaw(global_pose.pose.orientation);

    std::vector<tpose2d_C> in;
    in.resize(global_plan.size());
    for (std::vector<geometry_msgs::PoseStamped>::const_iterator it = global_plan.begin(); it != global_plan.end(); ++ it) {
        const geometry_msgs::PoseStamped& pose = *it;
        in.push_back(tpose2d_C{pose.pose.position.x, pose.pose.position.y, tf2::getYaw(pose.pose.orientation)});
    }
    adjust_plan_resolution(in, global_plan_2d_, costmap.getResolution());
    adjust_plan_cell(global_plan_2d_, global_plan_cell_, costmap);

    global_plan_ = global_plan;


    limits_ = limits;
    costmap_ = &costmap;
}

// copy from MapGrid::adjustPlanResolution
void RoseCostFunction::adjust_plan_resolution(const std::vector<tpose2d_C>& global_plan_in, std::vector<tpose2d_C>& global_plan_out, double resolution)
{
    global_plan_out.clear();
    if (global_plan_in.size() == 0) {
        return;
    }
    double last_x = global_plan_in[0].x;
    double last_y = global_plan_in[0].y;
    global_plan_out.push_back(global_plan_in[0]);

    double min_sq_resolution = resolution * resolution;

    for (unsigned int i = 1; i < global_plan_in.size(); ++i) {
        double loop_x = global_plan_in[i].x;
        double loop_y = global_plan_in[i].y;
        double sqdist = (loop_x - last_x) * (loop_x - last_x) + (loop_y - last_y) * (loop_y - last_y);
        if (sqdist > min_sq_resolution) {
            int steps = ceil((sqrt(sqdist)) / resolution);
            // add a points in-between
            double deltax = (loop_x - last_x) / steps;
            double deltay = (loop_y - last_y) / steps;
            // TODO: Interpolate orientation
            for (int j = 1; j < steps; ++j) {
                tpose2d_C pose;
                pose.x = last_x + j * deltax;
                pose.y = last_y + j * deltay;
                pose.yaw = global_plan_in[i].yaw;
                global_plan_out.push_back(pose);
            }
        }
        global_plan_out.push_back(global_plan_in[i]);
        last_x = loop_x;
        last_y = loop_y;
    }
}

void RoseCostFunction::adjust_plan_cell(const std::vector<tpose2d_C>& global_plan_in, std::vector<tpose2d_C>& global_plan_out, const costmap_2d::Costmap2D& costmap)
{
    global_plan_out.clear();
    if (global_plan_in.size() == 0) {
        return;
    }

    std::set<uint64_t> hit_cells;
    int map_x, map_y;
    int cell_index = 0;
    SDL_Point cell;
    for (unsigned int i = 0; i < global_plan_in.size(); ) {
        double world_x = global_plan_in[i].x;
        double world_y = global_plan_in[i].y;

        // of couse, canuse costmap_->worldToMap, it should be true always.
        costmap_->worldToMapNoBounds(world_x, world_y, map_x, map_y);
        cell.x = map_x;
        cell.y = map_y;
        uint64_t key = posix_mku64(cell.x, cell.y);
        if (hit_cells.count(key) != 0) {
            ++ i;
            continue;
        }

        double yaw = global_plan_in[i].yaw;
        bool found_other = false;
        while (i < global_plan_in.size()) {
            ++ i;
            costmap_->worldToMapNoBounds(global_plan_in[i].x, global_plan_in[i].y, map_x, map_y);
            if (map_x != cell.x || map_y != cell.y) {
                found_other = true;
                break;
            }
            yaw = global_plan_in[i].yaw;
        }

        global_plan_out.push_back(tpose2d_C{1.0 * cell.x, 1.0 * cell.y, yaw});
        hit_cells.insert(key);
    }
}

double RoseCostFunction::scoreTrajectory(Trajectory &traj)
{
    double cost = 0.0;
    double px, py, pth;

    std::vector<tpose2d_C> in;
    in.resize(traj.getPointsSize());
    for (unsigned int i = 0; i < traj.getPointsSize(); ++i) {
        traj.getPoint(i, px, py, pth);
        in.push_back(tpose2d_C{px, py, pth});
    }
    std::vector<tpose2d_C> out, traj_cells;
    adjust_plan_resolution(in, out, costmap_->getResolution());
    in = out;
    adjust_plan_cell(out, traj_cells, *costmap_);

    const tpose2d_C* cells = &traj_cells[0];
    const int traj_points = traj_cells.size();

    const int plan_points = global_plan_cell_.size();

    VALIDATE(cells[0].x == global_plan_cell_[0].x && cells[0].y == global_plan_cell_[0].y, null_str);
    double yaw_diff = 0;
    for (int at = 1; at < traj_points; at ++) {
        if (at == plan_points) {
            break;
        }
        const tpose2d_C& traj_point = traj_cells[at];
        const tpose2d_C& plan_point = global_plan_cell_[at];
        double sqdist = (traj_point.x - plan_point.x) * (traj_point.x - plan_point.x) + (traj_point.y - plan_point.y) * (traj_point.y - plan_point.y);
        cost += sqrt(sqdist);

        // double sqdist = (traj_point.x - plan_point.x) * (traj_point.x - plan_point.x) + (traj_point.y - plan_point.y) * (traj_point.y - plan_point.y);

    }
    return cost;
}

} /* namespace base_local_planner */
