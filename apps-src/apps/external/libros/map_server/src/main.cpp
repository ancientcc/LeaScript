/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Author: Brian Gerkey */

#define USAGE "\nUSAGE: map_server <map.yaml>\n" \
              "  map.yaml: map description file\n" \
              "DEPRECATED USAGE: map_server <map> <resolution>\n" \
              "  map: image file to load\n"\
              "  resolution: map resolution [meters/pixel]"

#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <boost/filesystem.hpp>

#include "ros/ros.h"
#include "ros/console.h"
#include "map_server/image_loader.h"
#include "nav_msgs/MapMetaData.h"
#include "nav_msgs/LoadMap.h"
// #include "yaml-cpp/yaml.h"
#include <ros/callback_queue.h>

#include <rose_ros/utils.hpp>
#include "rose_filesystem.hpp"
#include "rose_exception.hpp"
#include <SDL_timer.h>

class MapServer
{
public:
    /** Trivial constructor */
    MapServer(ros::CallbackQueue& cbqueue, const nav_msgs::OccupancyGrid& curr_map)
    {
        std::string mapfname = "";
        // double origin[3];
        // int negate;
        // double occ_th, free_th;
        MapMode mode = TRINARY;
        ros::NodeHandle private_nh("~");
        private_nh.setCallbackQueue(&cbqueue);
        {
            // private_nh.setParam("frame_id", "map");
        }

        // private_nh.param("frame_id", frame_id_, std::string("map"));
        frame_id_ = "map";
        
        nh_.setCallbackQueue(&cbqueue);
        //When called this service returns a copy of the current map
        get_map_service_ = nh_.advertiseService("static_map", &MapServer::mapCallback, this);

        // Change the currently published map
        // change_map_srv_ = nh_.advertiseService("change_map", &MapServer::changeMapCallback, this);

        // Latched publisher for metadata
        metadata_pub_ = nh_.advertise<nav_msgs::MapMetaData>("map_metadata", 1, true);

        // Latched publisher for data
        map_pub_ = nh_.advertise<nav_msgs::OccupancyGrid>("map", 1, true);

        // sub
        occupancy_grid_publisher_timer_ = nh_.createWallTimer(::ros::WallDuration(1.0), &MapServer::publishMap, this);

        loadMapFromRsp(curr_map);
    }

  private:
    ros::NodeHandle nh_;
    ros::Publisher map_pub_;
    ros::Publisher metadata_pub_;
    ros::ServiceServer get_map_service_;
    // ros::ServiceServer change_map_srv_;
    std::string frame_id_;
    ::ros::WallTimer occupancy_grid_publisher_timer_;

    /** Callback invoked when someone requests our service */
    bool mapCallback(nav_msgs::GetMap::Request  &req,
                     nav_msgs::GetMap::Response &res )
    {
      // request is empty; we ignore it

      // = operator is overloaded to make deep copy (tricky!)
      res = map_resp_;
      ROS_INFO("Sending map");

      return true;
    }

    // Callback invoked when someone requests to change the map
/*
    bool changeMapCallback(nav_msgs::LoadMap::Request  &request,
                           nav_msgs::LoadMap::Response &response )
    {
      VALIDATE(false, null_str);
      if (loadMapFromRsp(request.map_url))
      {
        response.result = response.RESULT_SUCCESS;
        ROS_INFO("Changed map to %s", request.map_url.c_str());
      }
      else
      {
        response.result = response.RESULT_UNDEFINED_FAILURE;
      }
      return true;
    }
*/
    /** Load a map given a path to a yaml file
     */
    bool loadMapFromRsp(const nav_msgs::OccupancyGrid& curr_map)
    {
        map_resp_.map = curr_map;

        // To make sure get a consistent time in simulation
        ros::Time::waitForValid();
        map_resp_.map.info.map_load_time = ros::Time::now();
        map_resp_.map.header.frame_id = frame_id_;
        map_resp_.map.header.stamp = ros::Time::now();
        ROS_INFO("Read a %d X %d map @ %.3lf m/cell",
               map_resp_.map.info.width,
               map_resp_.map.info.height,
               map_resp_.map.info.resolution);
        meta_data_message_ = map_resp_.map.info;

        //Publish latched topics
        metadata_pub_.publish( meta_data_message_ );
        map_pub_.publish( map_resp_.map );
        return true;
    }

    void publishMap(const ::ros::WallTimerEvent& unused_timer_event)
    {
        if (map_resp_.map.info.width <= 0 || map_resp_.map.info.height <= 0 || map_resp_.map.info.resolution <= 0) {
            return;
        }
        if (map_pub_.getNumSubscribers() == 0) {
            return;
        }
        // metadata_pub_.publish( meta_data_message_ );
        map_pub_.publish( map_resp_.map );
    }

    /** The map data is cached here, to be sent out to service callers
     */
    nav_msgs::MapMetaData meta_data_message_;
    nav_msgs::GetMap::Response map_resp_;

    /*
    void metadataSubscriptionCallback(const ros::SingleSubscriberPublisher& pub)
    {
      pub.publish( meta_data_message_ );
    }
    */

};

ROSTIME_DECL int map_server__map_server(bool& exit, const nav_msgs::OccupancyGrid& curr_map)
{
  // VALIDATE(SDL_IsFile(rspfile.c_str()), null_str);

  int argc = 0;
  ros::init(argc, nullptr, "map_server", ros::init_options::AnonymousName);
  // ros::NodeHandle nh("~");

  try
  {
    ros::CallbackQueue cbqueue;
    MapServer ms(cbqueue, curr_map);
    ros::WallDuration timeout(0.1f);
    while (!exit && ros::ok()) {
      cbqueue.callAvailable(timeout);
      // ros::spinOnce();
    }
    // ros::spin();
  }
  catch(std::runtime_error& e)
  {
    ROS_ERROR("map_server exception: %s", e.what());
    return -1;
  }

  return 0;
}
