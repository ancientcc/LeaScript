// Copyright  (C)  2007  Ruben Smits <ruben dot smits at intermodalics dot eu>

// Version: 1.0
// Author: Ruben Smits <ruben dot smits at intermodalics dot eu>
// Maintainer: Ruben Smits <ruben dot smits at intermodalics dot eu>
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

#include "chainjnttojacsolver.hpp"
#include <kdl/frames_io.hpp>
#include <kdl/kinfam_io.hpp>

namespace KDL
{
    ChainJntToJacSolver::ChainJntToJacSolver(const Chain& _chain):
        chain(_chain),locked_joints_(chain.getNrOfJoints(),false)
    {
    }

    void ChainJntToJacSolver::updateInternalDataStructures() {
        locked_joints_.resize(chain.getNrOfJoints(),false);
    }
    ChainJntToJacSolver::~ChainJntToJacSolver()
    {
    }

    int ChainJntToJacSolver::setLockedJoints(const std::vector<bool> locked_joints)
    {
        if(locked_joints_.size() != chain.getNrOfJoints())
            return (error = E_NOT_UP_TO_DATE);
        if(locked_joints.size()!=locked_joints_.size())
            return (error = E_SIZE_MISMATCH);
        locked_joints_=locked_joints;
        return (error = E_NOERROR);
    }

    int ChainJntToJacSolver::JntToJac(const JntArray& q_in, Jacobian& jac, int seg_nr)
    {
        bool verbose = KDL::kdl_sdl_log;
        if (verbose) {
            std::stringstream ss;
            ss << q_in;
            SDL_Log("{ChainJntToJacSolver::JntToJac}------q_in: %s, jac: %s", ss.str().c_str(), jac.DebugString().c_str());
        }

        if(locked_joints_.size() != chain.getNrOfJoints())
            return (error = E_NOT_UP_TO_DATE);
        unsigned int segmentNr;
        if(seg_nr<0)
            segmentNr=chain.getNrOfSegments();
        else
            segmentNr = seg_nr;

        //Initialize Jacobian to zero since only segmentNr columns are computed
        SetToZero(jac) ;

        if( q_in.rows()!=chain.getNrOfJoints() || jac.columns() != chain.getNrOfJoints())
            return (error = E_SIZE_MISMATCH);
        else if(segmentNr>chain.getNrOfSegments())
            return (error = E_OUT_OF_RANGE);

        T_tmp = Frame::Identity();
        SetToZero(t_tmp);
        int j=0;
        int k=0;
        Frame total;
        for (unsigned int i=0;i<segmentNr;i++) {
            //Calculate new Frame_base_ee
            if(chain.getSegment(i).getJoint().getType()!=Joint::Fixed) {
            	//pose of the new end-point expressed in the base
                total = T_tmp*chain.getSegment(i).pose(q_in(j));
                //changing base of new segment's twist to base frame if it is not locked
                //t_tmp = T_tmp.M*chain.getSegment(i).twist(1.0);
                const Segment& segment = chain.getSegment(i);
                const Joint& joint = segment.getJoint();
                if (verbose) {
                    SDL_Log("{JntToCart}%i/%i, segment: %s, joint: %s type:%i(Fixed: %i)", (int)i, (int)segmentNr, 
                        segment.DebugString().c_str(), joint.getName().c_str(), joint.getType(), Joint::Fixed);
                }

                if(!locked_joints_[j]) {
                    if (verbose) {
                        SDL_Log("[%i/%i]section 2.1, locked_joints_: true", (int)i, (int)segmentNr);
                    }
                    t_tmp = T_tmp.M*chain.getSegment(i).twist(q_in(j),1.0);
                } else {
                    if (verbose) {
                        SDL_Log("[%i/%i]section 2.2, locked_joints_: false", (int)i, (int)segmentNr);
                    }
                }
            }else{
                if (verbose) {
                    SDL_Log("[%i/%i]section 2.3, Fixed", (int)i, (int)segmentNr);
                }
                total = T_tmp*chain.getSegment(i).pose(0.0);
            }

            if (verbose) {
                SDL_Log("[%i/%i]pre changeRefPoint(jac,total.p-T_tmp.p,jac), t_tmp: %s", (int)i, (int)segmentNr, t_tmp.DebugString().c_str());
            }
            //Changing Refpoint of all columns to new ee
            changeRefPoint(jac,total.p-T_tmp.p,jac);

            if (verbose) {
                SDL_Log("[%i/%i]post changeRefPoint(jac,total.p-T_tmp.p,jac), jac: %s", (int)i, (int)segmentNr, jac.DebugString().c_str());
            }
            //Only increase jointnr if the segment has a joint
            if(chain.getSegment(i).getJoint().getType()!=Joint::Fixed) {
                //Only put the twist inside if it is not locked
                if(!locked_joints_[j]) {
                    jac.setColumn(k++,t_tmp);
                    if (verbose) {
                        SDL_Log("[%i/%i]jac.setColumn(k++,t_tmp), k: %i, t_tmp: %s, jac: %s", (int)i, (int)segmentNr, (int)k,
                            t_tmp.DebugString().c_str(), jac.DebugString().c_str());
                    }
                }
                j++;
            }

            T_tmp = total;
        }

        if (verbose) {
            SDL_Log("raw jac: %s", jac.DebugString().c_str());
        }

        ZeroJacBaseBounds(jac);

        if (verbose) {
            SDL_Log("------{ChainJntToJacSolver::JntToJac}X, total: %s, jac: %s", total.DebugString().c_str(), jac.DebugString().c_str());
        }

        return (error = E_NOERROR);
    }

    void ChainJntToJacSolver::ZeroJacBaseBounds(Jacobian& jac)
    {
        double ik_ignore_threshold = 100000.0;

        int cols = jac.data.cols();
        // for (int row = 3; row < 6; row ++) {
        for (int row = 0; row < 6; row ++) {
            if (bounds_(row) >= ik_ignore_threshold) {
                // set jac's line to zero if ignore this component.
                for (int col = 0; col < cols; col ++) {
                    jac.data(row, col) = 0;
                }
            }
        }
    }
}

