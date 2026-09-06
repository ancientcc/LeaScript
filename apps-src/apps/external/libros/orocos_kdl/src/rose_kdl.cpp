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

#include "kdl/rose_kdl.hpp"

#include <iostream>  
#include <Eigen/SVD>  
#include <Eigen/Dense>
// #include <SDL_log.h>
#include "rose_global.hpp"

namespace KDL
{
    void eigen_svd(const Jacobian& _jac)
    {
// using Eigen::MatrixXf;
// using namespace Eigen::internal;    
// using namespace Eigen::Architecture;    
        SDL_Log("---eigen_svd---");
        Eigen::Matrix<double, 6, 3> jac;
        for (int row = 0; row < (int)_jac.rows(); row ++) {
            for (int col = 0; col < (int)_jac.columns(); col ++) {
                jac(row, col) = _jac(row, col);
            }
        }
        const Eigen::Matrix<double, 6, 3>& A = jac;
/*
        Eigen::Matrix3f A;
        A(0,0)=1, A(0,1)=0, A(0,2)=1;  
        A(1,0)=0, A(1,1)=1, A(1,2)=1;  
        A(2,0)=0, A(2,1)=0, A(2,2)=0;  
        Eigen::JacobiSVD<Eigen::MatrixXf> svd(A, Eigen::ComputeThinU | Eigen::ComputeThinV );  
        Eigen::Matrix3f V = svd.matrixV(), U = svd.matrixU();  
        Eigen::Matrix3f S = U.inverse() * A * V.transpose().inverse(); // S = U^-1 * A * VT * -1  
*/    
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinU | Eigen::ComputeThinV );  
        Eigen::Matrix<double, 6, 6> U = svd.matrixU();  
        Eigen::Matrix<double, 3, 3> V = svd.matrixV();
        Eigen::Matrix<double, 6, 3> S = U.inverse() * A * V.transpose().inverse(); // S = U^-1 * A * VT * -1  

        std::stringstream ss;
        ss << "jac :\n" << jac << "\n";  
        ss << "A :\n" << A << "\n";  
        ss << "U :\n" << U <<  "\n";  
        ss <<"S :\n"  << S <<  "\n";  
        ss <<"V :\n" << V <<  "\n";  
        ss <<"U * S * VT :\n" << U * S * V.transpose();
        SDL_Log("%s", ss.str().c_str());
        SDL_Log("-----------------");
    }

    void eigen_verify(const Jacobian& _jac, const Twist& _v_in)
    {
        SDL_Log("---eigen_verify---");
        Eigen::Matrix<double, 6, 3> jac;
        for (int row = 0; row < (int)_jac.rows(); row ++) {
            for (int col = 0; col < (int)_jac.columns(); col ++) {
                jac(row, col) = _jac(row, col);
            }
        }
        const Eigen::Matrix<double, 6, 3>& A = jac;

        Eigen::Matrix<double, 6, 1> v_in;
        v_in(0, 0) = _v_in.vel.x();
        v_in(1, 0) = _v_in.vel.y();
        v_in(2, 0) = _v_in.vel.z();
        v_in(3, 0) = _v_in.rot.x();
        v_in(4, 0) = _v_in.rot.y();
        v_in(5, 0) = _v_in.rot.z();

        const Eigen::Matrix<double, 6, 6> I = Eigen::Matrix<double, 6, 6>::Identity(6, 6);
        const Eigen::Matrix<double, 3, 6> jac_inv = jac.colPivHouseholderQr().solve(I);
        // jac * jac_inv = I, solve for jac_inv.
        const Eigen::Matrix<double, 3, 1> qdot_out = jac_inv * v_in;

        const Eigen::Matrix<double, 6, 6> I2 = jac * jac_inv;
        
        // Eigen::Matrix<double, 3, 1> qdot_out = A.inverse() * v_in;
        
        std::stringstream ss;
        ss << "jac :\n" << jac << "\n";  
        // ss << "jac.inverse() :\n" << jac.inverse().matrix() << "\n";  
        ss << "jac_inv :\n" << jac_inv << "\n";
        ss << "I :\n" << I << "\n";
        ss << "I2 :\n" << I2 << "\n";
        ss << "v_in :\n" << v_in << "\n";  
        ss << "qdot_out :\n" << qdot_out;  
        SDL_Log("%s", ss.str().c_str());
        SDL_Log("-----------------");
    }

    void rotation_matrix_2_rpy(const Eigen::Matrix3d& rotation, double* roll, double* pitch, double* yaw)
    {
        KDL::Rotation M(rotation(0, 0), rotation(0, 1), rotation(0, 2),
            rotation(1, 0), rotation(1, 1), rotation(1, 2),
            rotation(2, 0), rotation(2, 1), rotation(2, 2));

        double roll1, pitch1, yaw1;    
        M.GetRPY(roll1, pitch1, yaw1);

        if (roll) {
            *roll = roll1;
        }
        if (roll) {
            *pitch = pitch1;
        }
        if (roll) {
            *yaw = yaw1;
        }
    }

    std::string verbose_Isometry3d(const Eigen::Isometry3d& tf)
    {
        char buf[512];
        std::stringstream ss;

        const Eigen::Vector3d& trans = tf.translation();
        const Eigen::Quaterniond quat(tf.rotation());
        ss << tf.rotation();
        SDL_snprintf(buf, sizeof(buf), "{p(%.6f, %.6f, %.6f) quat(%.6fi+%.6fj+%.6fk+%.6f)}\n%s", trans.x(), trans.y(), trans.z(),
            quat.x(), quat.y(), quat.z(), quat.w(), ss.str().c_str());
        // SDL_snprintf(buf, sizeof(buf), "{p(%.6f, %.6f, %.6f) quat(%.6fi+%.6fj+%.6fk+%.6f)}", trans.x(), trans.y(), trans.z(),
        //    quat.x(), quat.y(), quat.z(), quat.w());
        return buf;
    }

    Eigen::Isometry3d frame_2_Isometry3d(const Frame& f)
    {
        // ----3.eigen::matrix4d----
        Eigen::Matrix4d T2;
        T2.setIdentity();
        T2.block<3, 3>(0, 0) = Eigen::Matrix3d(f.M.data).transpose();
        T2.topRightCorner(3, 1) = Eigen::Vector3d(f.p.data);
        // T2.topRightCorner<3, 1>() = t1;

        Eigen::Isometry3d result(T2);
/*
        std::stringstream ss;
        ss.str("");
        ss << result.matrix();
        SDL_Log("result:\n %s", ss.str().c_str());

        double roll;
        double pitch;
        double yaw;
        rotation_matrix_2_rpy(result.rotation(), &roll, &pitch, &yaw);
        SDL_Log("roll: %.5f, pitch: %.5f, yaw: %.5f", roll, pitch, yaw);


        double roll1, pitch1, yaw1;    
        f.M.GetRPY(roll1, pitch1, yaw1);
        SDL_Log("roll1: %.5f, pitch1: %.5f, yaw1: %.5f", roll1, pitch1, yaw1);
*/
        return result;
    }

    Frame Isometry3d_2_frame(const Eigen::Isometry3d& src)
    {
        Eigen::Vector3d p = src.translation();
        Eigen::Matrix3d rotation = src.rotation();
        KDL::Rotation M(rotation(0, 0), rotation(0, 1), rotation(0, 2),
            rotation(1, 0), rotation(1, 1), rotation(1, 2),
            rotation(2, 0), rotation(2, 1), rotation(2, 2));
        Frame result(M, KDL::Vector(p(0), p(1), p(2)));

/*        
        std::stringstream ss;
        ss.str("");
        ss << src.matrix();
        SDL_Log("src:\n %s", ss.str().c_str());

        double roll;
        double pitch;
        double yaw;
        rotation_matrix_2_rpy(src.rotation(), &roll, &pitch, &yaw);
        SDL_Log("roll: %.5f, pitch: %.5f, yaw: %.5f", roll, pitch, yaw);


        double roll1, pitch1, yaw1;    
        result.M.GetRPY(roll1, pitch1, yaw1);
        SDL_Log("roll1: %.5f, pitch1: %.5f, yaw1: %.5f", roll1, pitch1, yaw1);
*/
        return result;
    }


    std::string Vector_DebugString(const KDL::Vector& v)
    {
        char buf[64];
        SDL_snprintf(buf, sizeof(buf), "(%.6f, %.6f, %.6f)", v.data[0], v.data[1], v.data[2]);
        return buf;
    }

    std::string Rotation_DebugString(const KDL::Rotation& M)
    {
        char buf[128];
/*
        SDL_snprintf(buf, sizeof(buf), "(%.4f, %.4f, %.4f; %.4f, %.4f, %.4f; %.4f, %.4f, %.4f)",
            data[0], data[1], data[2],
            data[3], data[4], data[5],
            data[6], data[7], data[8]);
*/
        double roll;
        double pitch;
        double yaw;
        M.GetRPY(roll, pitch, yaw);
        SDL_snprintf(buf, sizeof(buf), "(%.6f, %.6f(%.4f), %.6f)", roll, pitch, RAD2DEG(pitch), yaw);
        return buf;
    }

    std::string Frame_DebugString(const KDL::Frame& f)
    {
        char buf[128];
        SDL_snprintf(buf, sizeof(buf), "{p:%s, M:%s}", Vector_DebugString(f.p).c_str(), Rotation_DebugString(f.M).c_str());
        return buf;
    }
}
