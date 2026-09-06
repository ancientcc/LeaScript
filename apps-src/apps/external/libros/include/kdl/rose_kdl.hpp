// Copyright  (C)  2007  Ruben Smits <ruben dot smits at mech dot kuleuven dot be>

// Version: 1.0
// Author: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// Maintainer: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// URL: http://www.orocos.org/kdl

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#ifndef KDL_ROSE_KDL_HPP
#define KDL_ROSE_KDL_HPP

#include "jacobian.hpp"
#include <Eigen/Dense>
#include <SDL_log.h>

namespace KDL
{
void eigen_svd(const Jacobian& _jac);
void eigen_verify(const Jacobian& _jac, const Twist& _v_in);
void rotation_matrix_2_rpy(const Eigen::Matrix3d& rotation, double* roll, double* pitch, double* yaw);
std::string verbose_Isometry3d(const Eigen::Isometry3d& tf);
Eigen::Isometry3d frame_2_Isometry3d(const Frame& f);
Frame Isometry3d_2_frame(const Eigen::Isometry3d& src);

std::string Vector_DebugString(const KDL::Vector& v);
std::string Rotation_DebugString(const KDL::Rotation& M);
std::string Frame_DebugString(const KDL::Frame& f);

}
#endif

