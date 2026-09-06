/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
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
*   * Neither the name of the Willow Garage nor the names of its
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
*********************************************************************/

/* Author: Ioan Sucan */

#include "ompl/geometric/planners/rrt/RRTConnect.h"
#include "ompl/base/goals/GoalSampleableRegion.h"
#include "ompl/tools/config/SelfConfig.h"
#include "ompl/util/String.h"

#include <moveit/ompl_interface/parameterization/model_based_state_space.h>
#include <SDL_log.h>
static char GrowStates[][20] = {"TRAPPED", "ADVANCED", "REACHED"};

ompl::geometric::RRTConnect::RRTConnect(const base::SpaceInformationPtr &si, bool addIntermediateStates)
  : base::Planner(si, addIntermediateStates ? "RRTConnectIntermediate" : "RRTConnect")
{
    specs_.recognizedGoal = base::GOAL_SAMPLEABLE_REGION;
    specs_.directed = true;

    Planner::declareParam<double>("range", this, &RRTConnect::setRange, &RRTConnect::getRange, "0.:1.:10000.");
    Planner::declareParam<bool>("intermediate_states", this, &RRTConnect::setIntermediateStates,
                                &RRTConnect::getIntermediateStates, "0,1");

    connectionPoint_ = std::make_pair<base::State *, base::State *>(nullptr, nullptr);
    distanceBetweenTrees_ = std::numeric_limits<double>::infinity();
    addIntermediateStates_ = addIntermediateStates;
}

ompl::geometric::RRTConnect::~RRTConnect()
{
    freeMemory();
}

void ompl::geometric::RRTConnect::setup()
{
    Planner::setup();
    tools::SelfConfig sc(si_, getName());
    sc.configurePlannerRange(maxDistance_);

    if (!tStart_)
        tStart_.reset(tools::SelfConfig::getDefaultNearestNeighbors<Motion *>(this));
    if (!tGoal_)
        tGoal_.reset(tools::SelfConfig::getDefaultNearestNeighbors<Motion *>(this));
    tStart_->setDistanceFunction([this](const Motion *a, const Motion *b) { return distanceFunction(a, b); });
    tGoal_->setDistanceFunction([this](const Motion *a, const Motion *b) { return distanceFunction(a, b); });
}

void ompl::geometric::RRTConnect::freeMemory()
{
    std::vector<Motion *> motions;

    if (tStart_)
    {
        tStart_->list(motions);
        for (auto &motion : motions)
        {
            if (motion->state != nullptr)
                si_->freeState(motion->state);
            delete motion;
        }
    }

    if (tGoal_)
    {
        tGoal_->list(motions);
        for (auto &motion : motions)
        {
            if (motion->state != nullptr)
                si_->freeState(motion->state);
            delete motion;
        }
    }
}

void ompl::geometric::RRTConnect::clear()
{
    Planner::clear();
    sampler_.reset();
    freeMemory();
    if (tStart_)
        tStart_->clear();
    if (tGoal_)
        tGoal_->clear();
    connectionPoint_ = std::make_pair<base::State *, base::State *>(nullptr, nullptr);
    distanceBetweenTrees_ = std::numeric_limits<double>::infinity();
}

std::string get_state_str(const ompl::base::State* state)
{
    char state_str[64] = "<nil>";
    const ompl_interface::ModelBasedStateSpace::StateType* state2 = nullptr;
    if (state != nullptr) {
        state2 = state->as<ompl_interface::ModelBasedStateSpace::StateType>();
        SDL_snprintf(state_str, sizeof(state_str), "(%.5f, %.5f, %.5f)", 
            state2->values[0], state2->values[1], state2->values[2]);
    }
    return state_str;
}

std::string get_motion_str(const ompl::geometric::RRTConnect::Motion* motion)
{
    if (motion == nullptr) {
        return "<nil>";
    }
    char motion_str[128];
    SDL_snprintf(motion_str, sizeof(motion_str), "{0x%p, root: %s, state: %s, parent: 0x%p", 
        motion,
        get_state_str(motion->root).c_str(),
        get_state_str(motion->state).c_str(),
        motion->parent);
    return motion_str;
}

std::string get_tgi_str(const ompl::geometric::RRTConnect::TreeGrowingInfo& tgi)
{
    char tgi_str[256];
    SDL_snprintf(tgi_str, sizeof(tgi_str), "{xstate: %s, xmotion: %s, start: %s", 
        get_state_str(tgi.xstate).c_str(),
        get_motion_str(tgi.xmotion).c_str(),
        tgi.start? "true": "false");
    return tgi_str;
}

ompl::geometric::RRTConnect::GrowState ompl::geometric::RRTConnect::growTree(TreeData &tree, TreeGrowingInfo &tgi,
                                                                             Motion *rmotion)
{
    SDL_Log("{growTree}---tree size: %i, rmotion: %s, tgi: %s", (int)tree->size(), get_motion_str(rmotion).c_str(), get_tgi_str(tgi).c_str());

    /* find closest state in the tree */
    Motion *nmotion = tree->nearest(rmotion);
    SDL_Log("{growTree}nmotion: %s", get_motion_str(nmotion).c_str());

    /* assume we can reach the state we go towards */
    bool reach = true;

    /* find state to add */
    base::State *dstate = rmotion->state;
    double d = si_->distance(nmotion->state, rmotion->state);
    if (d > maxDistance_)
    {
        SDL_Log("{growTree}d(%.5f) > maxDistance_(%.5f)", d, maxDistance_);
        si_->getStateSpace()->interpolate(nmotion->state, rmotion->state, maxDistance_ / d, tgi.xstate);

        /* Check if we have moved at all. Due to some stranger state spaces (e.g., the constrained state spaces),
         * interpolate can fail and no progress is made. Without this check, the algorithm gets stuck in a loop as it
         * thinks it is making progress, when none is actually occurring. */
        SDL_Log("{growTree}post interpolate, nmotion->state(%s) <==> tgi.xstate(%s)", 
            get_state_str(nmotion->state).c_str(), get_state_str(tgi.xstate).c_str());
        if (si_->equalStates(nmotion->state, tgi.xstate)) {
            SDL_Log("{growTree}X[1/3], return %s", GrowStates[TRAPPED]);
            return TRAPPED;
        }

        dstate = tgi.xstate;
        reach = false;
    } else {
        SDL_Log("{growTree}d(%.5f) <= maxDistance_(%.5f)", d, maxDistance_);
    }

    bool validMotion = tgi.start ? si_->checkMotion(nmotion->state, dstate) :
                                   si_->isValid(dstate) && si_->checkMotion(dstate, nmotion->state);
    // bool validMotion = true;
    SDL_Log("{growTree}validMotion: %s, tgi.start: %s", validMotion? "true": "false", tgi.start? "true": "false");
    
    if (!validMotion) {
        SDL_Log("{growTree}X[2/3], return %s", GrowStates[TRAPPED]);
        return TRAPPED;
    }

    if (addIntermediateStates_)
    {
        SDL_Log("{growTree}addIntermediateStates_: true section");

        const base::State *astate = tgi.start ? nmotion->state : dstate;
        const base::State *bstate = tgi.start ? dstate : nmotion->state;

        std::vector<base::State *> states;
        const unsigned int count = si_->getStateSpace()->validSegmentCount(astate, bstate);

        if (si_->getMotionStates(astate, bstate, states, count, true, true))
            si_->freeState(states[0]);

        for (std::size_t i = 1; i < states.size(); ++i)
        {
            auto *motion = new Motion;
            motion->state = states[i];
            motion->parent = nmotion;
            motion->root = nmotion->root;
            tree->add(motion);

            nmotion = motion;
        }

        tgi.xmotion = nmotion;
    }
    else
    {
        SDL_Log("{growTree}addIntermediateStates_: false section, dstate: %s", get_state_str(dstate).c_str());
        auto *motion = new Motion(si_);
        si_->copyState(motion->state, dstate);
        motion->parent = nmotion;
        motion->root = nmotion->root;
        tree->add(motion);
        SDL_Log("{growTree}post tree->add(motion), tree size: %i, motion: %s", (int)tree->size(), get_motion_str(motion).c_str());

        tgi.xmotion = motion;
    }

    SDL_Log("{growTree}X[3/3], tgi: %s, return %s", get_tgi_str(tgi).c_str(), GrowStates[reach ? REACHED : ADVANCED]);
    return reach ? REACHED : ADVANCED;
}

ompl::base::PlannerStatus ompl::geometric::RRTConnect::solve(const base::PlannerTerminationCondition &ptc)
{
    OMPL_INFORM("RRTConnect::solve---%s", getName().c_str());

    checkValidity();
    auto *goal = dynamic_cast<base::GoalSampleableRegion *>(pdef_->getGoal().get());

    if (goal == nullptr)
    {
        OMPL_ERROR("%s: Unknown type of goal", getName().c_str());
        return base::PlannerStatus::UNRECOGNIZED_GOAL_TYPE;
    }

    while (const base::State *st = pis_.nextStart())
    {
        auto *motion = new Motion(si_);
        si_->copyState(motion->state, st);
        motion->root = motion->state;
        tStart_->add(motion);
        SDL_Log("{solve}post tStart_->add(motion), motion: %s, tStart_.size: %i", get_motion_str(motion).c_str(), (int)tStart_->size());
    }

    if (tStart_->size() == 0)
    {
        OMPL_ERROR("%s: Motion planning start tree could not be initialized!", getName().c_str());
        return base::PlannerStatus::INVALID_START;
    }

    if (!goal->couldSample())
    {
        OMPL_ERROR("%s: Insufficient states in sampleable goal region", getName().c_str());
        return base::PlannerStatus::INVALID_GOAL;
    }

    if (!sampler_)
        sampler_ = si_->allocStateSampler();

    OMPL_INFORM("%s: Starting planning with %d states already in datastructure", getName().c_str(),
                (int)(tStart_->size() + tGoal_->size()));

    TreeGrowingInfo tgi;
    {
        tgi.start = false;
        tgi.xstate = nullptr;
        tgi.xmotion = nullptr;
    }
    tgi.xstate = si_->allocState();

    Motion *approxsol = nullptr;
    double approxdif = std::numeric_limits<double>::infinity();
    auto *rmotion = new Motion(si_);
    base::State *rstate = rmotion->state;
    SDL_Log("{solve}initial rmotion: %s", get_motion_str(rmotion).c_str());
    bool solved = false;

    int while_n = 0;
    while (!ptc)
    {
        TreeData &tree = startTree_ ? tStart_ : tGoal_;
        tgi.start = startTree_;
        startTree_ = !startTree_;
        TreeData &otherTree = startTree_ ? tStart_ : tGoal_;

        OMPL_INFORM("#%i, while (!ptr)---", while_n ++);

        if (tGoal_->size() == 0 || pis_.getSampledGoalsCount() < tGoal_->size() / 2)
        {
            const base::State *st = tGoal_->size() == 0 ? pis_.nextGoal(ptc) : pis_.nextGoal();
            if (st != nullptr)
            {
                auto *motion = new Motion(si_);
                si_->copyState(motion->state, st);
                motion->root = motion->state;
                tGoal_->add(motion);
                SDL_Log("{solve}post tGoal_->add(motion), motion: %s, tGoal_.size: %i", get_motion_str(motion).c_str(), (int)tGoal_->size());
            }

            if (tGoal_->size() == 0)
            {
                OMPL_ERROR("%s: Unable to sample any valid states for goal tree", getName().c_str());
                break;
            }
        }

        /* sample random state */
        sampler_->sampleUniform(rstate);
        SDL_Log("[rstate]post sampler_->sampleUniform(rstate): %s", get_state_str(rstate).c_str());

        GrowState gs = growTree(tree, tgi, rmotion);

        if (gs != TRAPPED)
        {
            /* remember which motion was just added */
            Motion *addedMotion = tgi.xmotion;

            /* attempt to connect trees */

            /* if reached, it means we used rstate directly, no need to copy again */
            if (gs != REACHED)
                si_->copyState(rstate, tgi.xstate);

            tgi.start = startTree_;

            /* if initial progress cannot be done from the otherTree, restore tgi.start */
            SDL_Log("{solve}call growTree[2/3]");
            GrowState gsc = growTree(otherTree, tgi, rmotion);
            if (gsc == TRAPPED)
                tgi.start = !tgi.start;

            SDL_Log("{solve}pre while (gsc == ADVANCED), gsc: %s", GrowStates[gsc]);
            while (gsc == ADVANCED) {
                SDL_Log("{solve}call growTree[3/3]");
                gsc = growTree(otherTree, tgi, rmotion);
            }

            /* update distance between trees */
            const double newDist = tree->getDistanceFunction()(addedMotion, otherTree->nearest(addedMotion));
            if (newDist < distanceBetweenTrees_)
            {
                distanceBetweenTrees_ = newDist;
                // OMPL_INFORM("Estimated distance to go: %f", distanceBetweenTrees_);
            }

            Motion *startMotion = tgi.start ? tgi.xmotion : addedMotion;
            Motion *goalMotion = tgi.start ? addedMotion : tgi.xmotion;
            
            SDL_Log("{solve}distanceBetweenTrees_: %.5f, startMotion: %s, goalMotion: %s", distanceBetweenTrees_, get_motion_str(startMotion).c_str(), get_motion_str(goalMotion).c_str());
            /* if we connected the trees in a valid way (start and goal pair is valid)*/
            if (gsc == REACHED && goal->isStartGoalPairValid(startMotion->root, goalMotion->root))
            {
                // it must be the case that either the start tree or the goal tree has made some progress
                // so one of the parents is not nullptr. We go one step 'back' to avoid having a duplicate state
                // on the solution path
                if (startMotion->parent != nullptr)
                    startMotion = startMotion->parent;
                else
                    goalMotion = goalMotion->parent;

                SDL_Log("{solve}if (gsc == REACHED &&...), startMotion: %s, goalMotion: %s", get_motion_str(startMotion).c_str(), get_motion_str(goalMotion).c_str());
                connectionPoint_ = std::make_pair(startMotion->state, goalMotion->state);

                /* construct the solution path */
                Motion *solution = startMotion;
                std::vector<Motion *> mpath1;
                while (solution != nullptr)
                {
                    mpath1.push_back(solution);
                    SDL_Log("{solve}mpath1.push_back(solution: %s), post size: %i", get_motion_str(solution).c_str(), (int)mpath1.size());
                    solution = solution->parent;
                }

                solution = goalMotion;
                std::vector<Motion *> mpath2;
                while (solution != nullptr)
                {
                    mpath2.push_back(solution);
                    SDL_Log("{solve}mpath2.push_back(solution: %s), post size: %i", get_motion_str(solution).c_str(), (int)mpath2.size());
                    solution = solution->parent;
                }

                auto path(std::make_shared<PathGeometric>(si_));
                path->getStates().reserve(mpath1.size() + mpath2.size());
                for (int i = mpath1.size() - 1; i >= 0; --i)
                    path->append(mpath1[i]->state);
                for (auto &i : mpath2)
                    path->append(i->state);
                SDL_Log("{solve}post 2for, path size: %i, length: %.5f", (int)path->getStates().size(), path->length());

                OMPL_INFORM("pdef_->addSolutionPath");
                pdef_->addSolutionPath(path, false, 0.0, getName());
                solved = true;
                break;
            }
            else
            {
                // We didn't reach the goal, but if we were extending the start
                // tree, then we can mark/improve the approximate path so far.
                if (tgi.start)
                {
                    // We were working from the startTree.
                    double dist = 0.0;
                    goal->isSatisfied(tgi.xmotion->state, &dist);
                    if (dist < approxdif)
                    {
                        approxdif = dist;
                        approxsol = tgi.xmotion;
                    }
                }
            }
        }
    }

    si_->freeState(tgi.xstate);
    si_->freeState(rstate);
    delete rmotion;

    OMPL_INFORM("%s: Created %u states (%u start + %u goal)", getName().c_str(), tStart_->size() + tGoal_->size(),
                tStart_->size(), tGoal_->size());

    if (approxsol && !solved)
    {
        /* construct the solution path */
        std::vector<Motion *> mpath;
        while (approxsol != nullptr)
        {
            mpath.push_back(approxsol);
            approxsol = approxsol->parent;
        }

        auto path(std::make_shared<PathGeometric>(si_));
        for (int i = mpath.size() - 1; i >= 0; --i)
            path->append(mpath[i]->state);
        pdef_->addSolutionPath(path, true, approxdif, getName());
        return base::PlannerStatus::APPROXIMATE_SOLUTION;
    }

    OMPL_INFORM("---RRTConnect::solve, X, %s", solved? "true": "false");
    return solved ? base::PlannerStatus::EXACT_SOLUTION : base::PlannerStatus::TIMEOUT;
}

void ompl::geometric::RRTConnect::getPlannerData(base::PlannerData &data) const
{
    Planner::getPlannerData(data);

    std::vector<Motion *> motions;
    if (tStart_)
        tStart_->list(motions);

    for (auto &motion : motions)
    {
        if (motion->parent == nullptr)
            data.addStartVertex(base::PlannerDataVertex(motion->state, 1));
        else
        {
            data.addEdge(base::PlannerDataVertex(motion->parent->state, 1), base::PlannerDataVertex(motion->state, 1));
        }
    }

    motions.clear();
    if (tGoal_)
        tGoal_->list(motions);

    for (auto &motion : motions)
    {
        if (motion->parent == nullptr)
            data.addGoalVertex(base::PlannerDataVertex(motion->state, 2));
        else
        {
            // The edges in the goal tree are reversed to be consistent with start tree
            data.addEdge(base::PlannerDataVertex(motion->state, 2), base::PlannerDataVertex(motion->parent->state, 2));
        }
    }

    // Add the edge connecting the two trees
    data.addEdge(data.vertexIndex(connectionPoint_.first), data.vertexIndex(connectionPoint_.second));

    // Add some info.
    data.properties["approx goal distance REAL"] = ompl::toString(distanceBetweenTrees_);
}
