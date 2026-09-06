#include "ros/ros.h"
#include "std_msgs/String.h"
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include "stdint.h"
#include "rrt_exploration/functions.h"
#include "mtrand.h"


#include "nav_msgs/OccupancyGrid.h"
#include "geometry_msgs/PointStamped.h"
#include "std_msgs/Header.h"
#include "nav_msgs/MapMetaData.h"
#include "geometry_msgs/Point.h"
#include "visualization_msgs/Marker.h"
#include <tf/transform_listener.h>
#include <ros/callback_queue.h>

#include <SDL_log.h>
#include <SDL_timer.h>

// global variables
static nav_msgs::OccupancyGrid mapData;
static geometry_msgs::PointStamped clickedpoint;
static geometry_msgs::PointStamped exploration_goal;
static visualization_msgs::Marker points,line;
// static float xdim,ydim,resolution,Xstartx,Xstarty,init_map_x,init_map_y;
static float xdim,ydim,resolution,init_map_x,init_map_y;

static rdm r; // for genrating random numbers



//Subscribers callback functions---------------------------------------
void mapCallBack(const nav_msgs::OccupancyGrid::ConstPtr& msg)
{
	mapData=*msg;
}
 
void rvizCallBack(const geometry_msgs::PointStamped::ConstPtr& msg)
{ 
	geometry_msgs::Point p;  
	p.x=msg->point.x;
	p.y=msg->point.y;
	p.z=msg->point.z;

	points.points.push_back(p);
}


static std::string log_vvf(const std::vector<std::vector<float> >& V)
{
	std::stringstream ss;
	ss << "(" << V.size() << ")";
	for (std::vector<std::vector<float> >::const_iterator it = V.begin(); it != V.end(); ++ it) {
		const std::vector<float>& p = *it;
		if (!ss.str().empty()) {
			ss << " ";
		}
		ss << "(" << p[0] << ", " << p[1] << ")";
	}
	return ss.str();
}

static void test()
{
	std::vector<std::vector<float> > rands;
	std::vector<std::vector<float> > nearest;

	std::vector<float> point;
	for (int at = 0; at < 200; at ++) {
		int r = rand() % 100000 + 1;
		point.push_back(((r & 1)? -1.0: 1.0) * r / 100);
		r = rand() % 100000 + 1;
		point.push_back(((r & 1)? -1.0: 1.0) * r / 100);
		rands.push_back(point);
		point.clear();

		r = rand() % 100000 + 1;
		point.push_back(((r & 1)? -1.0: 1.0) * r / 100);
		r = rand() % 100000 + 1;
		point.push_back(((r & 1)? -1.0: 1.0) * r / 100);
		nearest.push_back(point);
		point.clear();
	}

	float eta = 10.5; // 0.5
	std::vector<float> x_new;
	for (int at = 0; at < (int)rands.size(); at ++) {
		const std::vector<float>& x_rand = rands[at];
		const std::vector<float>& x_nearest = nearest[at];

		x_new.clear();
		if (Norm(x_nearest,x_rand) > eta){
			float m=(x_rand[1]-x_nearest[1])/(x_rand[0]-x_nearest[0]);
			x_new.push_back(  (sign(x_rand[0]-x_nearest[0]))* (   sqrt( (pow(eta,2)) / ((pow(m,2))+1) )   )+x_nearest[0] );
			x_new.push_back(  m*(x_new[0]-x_nearest[0])+x_nearest[1] );

			SDL_Log("#%i n(%.5f, %.5f) -- r(%.5f, %.5f) ==> (%.5f, %.5f) distance: %.5f",
				at, x_nearest[0], x_nearest[1], x_rand[0], x_rand[1], x_new[0], x_new[1], Norm(x_new, x_nearest));

			float lambda = eta / (Norm(x_nearest,x_rand) - eta);
			std::vector<float> x_new2;
			x_new2.push_back(x_nearest[0] + lambda * x_rand[0] / (1 + lambda));
			x_new2.push_back(x_nearest[1] + lambda * x_rand[1] / (1 + lambda));
			SDL_Log("#%i n(%.5f, %.5f) -- r(%.5f, %.5f) ==> (%.5f, %.5f) distance: %.5f",
				at, x_nearest[0], x_nearest[1], x_rand[0], x_rand[1], x_new2[0], x_new2[1], Norm(x_new2, x_nearest));

		} else {
			SDL_Log("#%i n(%.5f, %.5f) -- r(%.5f, %.5f) distance: %.5f <= eta",
				at, x_nearest[0], x_nearest[1], x_rand[0], x_rand[1], Norm(x_new, x_nearest));
		}
	}
}

// int main(int argc, char **argv)
ROSTIME_DECL int rrt_exploration__global_rrt_detector(bool& exit)
{
	test();

	unsigned int init[4] = {0x123, 0x234, 0x345, 0x456}, length = 7;
	MTRand_int32 irand(init, length); // 32-bit int generator
	// this is an example of initializing by an array
	// you may use MTRand(seed) with any 32bit integer
	// as a seed for a simpler initialization
	MTRand drand; // double in [0, 1) generator, already init

	// generate the same numbers as in the original C test program
	// ros::init(argc, argv, "global_rrt_frontier_detector");
	int argc = 1;
	char* argv[1] = {"global_rrt_frontier_detector"};
	ros::init(argc, argv, "global_rrt_frontier_detector");
	ros::NodeHandle nh;
  
	ros::CallbackQueue cbqueue;
	nh.setCallbackQueue(&cbqueue);

	// fetching all parameters
	float eta,init_map_x,init_map_y,range;
	std::string map_topic,base_frame_topic;
  
	std::string ns;
	ns=ros::this_node::getName();

	ros::param::param<float>(ns+"/eta", eta, 0.5);
	ros::param::param<std::string>(ns+"/map_topic", map_topic, "/robot_1/map"); 
	//---------------------------------------------------------------
	ros::Subscriber sub = nh.subscribe("/map", 100, mapCallBack);
	ros::Subscriber rviz_sub= nh.subscribe("/clicked_point", 100 ,rvizCallBack);	

	ros::Publisher targetspub = nh.advertise<geometry_msgs::PointStamped>("/detected_points", 10);
	ros::Publisher pub = nh.advertise<visualization_msgs::Marker>(ns+"_shapes", 10);

	ros::Rate rate(100); 
 
	// wait until map is received, when a map is received, mapData.header.seq will not be < 1  
	// while (mapData.header.seq<1 or mapData.data.size()<1)  {  ros::spinOnce();  ros::Duration(0.1).sleep();}
	// while (mapData.header.seq < 1 || mapData.data.size() < 1)  {
	// 	ros::spinOnce();  ros::Duration(0.1).sleep();
	// }
	ros::Rate r(10);
    while (!exit && mapData.header.seq < 1 || mapData.data.size() < 1)
    {
      // ros::spinOnce();
      cbqueue.callAvailable(ros::WallDuration());
      r.sleep();
    }


	// visualizations  points and lines..
	points.header.frame_id=mapData.header.frame_id;
	line.header.frame_id=mapData.header.frame_id;
	points.header.stamp=ros::Time(0);
	line.header.stamp=ros::Time(0);
	
	points.ns=line.ns = "markers";
	points.id = 0;
	line.id =1;


	points.type = points.POINTS;
	line.type=line.LINE_LIST;

	//Set the marker action.  Options are ADD, DELETE, and new in ROS Indigo: 3 (DELETEALL)
	points.action =points.ADD;
	line.action = line.ADD;
	points.pose.orientation.w =1.0;
	line.pose.orientation.w = 1.0;
	line.scale.x =  0.03;
	line.scale.y= 0.03;
	points.scale.x=0.3; 
	points.scale.y=0.3; 

	line.color.r =9.0/255.0;
	line.color.g= 91.0/255.0;
	line.color.b =236.0/255.0;
	points.color.r = 255.0/255.0;
	points.color.g = 0.0/255.0;
	points.color.b = 0.0/255.0;
	points.color.a=1.0;
	line.color.a = 1.0;
	points.lifetime = ros::Duration();
	line.lifetime = ros::Duration();

	geometry_msgs::Point p;  


/*
	ros::WallDuration timeout(0.1f);
	while (!exit && points.points.size()<5) {
		cbqueue.callAvailable(timeout);
		// pub.publish(points);
	}
	if (exit) {
		return 0;
	}
*/

	{
		geometry_msgs::Point p;  
		p.x=-1;
		p.y=0;
		p.z=0;
		points.points.push_back(p);

		p.x = -0.5;
		points.points.push_back(p);

		p.x = 2;
		p.y = -3;
		points.points.push_back(p);

		p.x = 0.5;
		p.y = 0;
		points.points.push_back(p);

		p.x = 1;
		points.points.push_back(p);
	}

	std::vector<float> temp1;
	temp1.push_back(points.points[0].x);
	temp1.push_back(points.points[0].y);
	
	std::vector<float> temp2; 
	temp2.push_back(points.points[2].x);
	temp2.push_back(points.points[0].y);


	init_map_x=Norm(temp1,temp2);
	temp1.clear();		
	temp2.clear();

	temp1.push_back(points.points[0].x);
	temp1.push_back(points.points[0].y);

	temp2.push_back(points.points[0].x);
	temp2.push_back(points.points[2].y);

	init_map_y=Norm(temp1,temp2);
	temp1.clear();		
	temp2.clear();

	const float Xstartx=(points.points[0].x+points.points[2].x)*.5;
	const float Xstarty=(points.points[0].y+points.points[2].y)*.5;


	geometry_msgs::Point trans;
	trans=points.points[4];
	std::vector< std::vector<float>  > V; 
	std::vector<float> xnew; 
	xnew.push_back( trans.x);xnew.push_back( trans.y);  
	V.push_back(xnew);

	points.points.clear();
	pub.publish(points) ;


	std::vector<float> frontiers;
	int i=0;
	float xr,yr;
	std::vector<float> x_rand,x_nearest,x_new;


	// Main loop
	int times = 0;
	while (!exit && ros::ok()){
		SDL_Log("#%i V: %s", times ++, log_vvf(V).c_str());

		// Sample free
		x_rand.clear();
		xr=(drand()*init_map_x)-(init_map_x*0.5)+Xstartx;
		yr=(drand()*init_map_y)-(init_map_y*0.5)+Xstarty;


		x_rand.push_back( xr ); x_rand.push_back( yr );


		// Nearest
		x_nearest=Nearest(V,x_rand);

		// Steer

		x_new=Steer(x_nearest,x_rand,eta);


		SDL_Log("%u, x_rand(%.5f, %.5f), x_nearest(%.5f, %.5f), x_new(%.5f, %.5f)", 
			SDL_GetTicks(), x_rand[0], x_rand[1], x_nearest[0], x_nearest[1], x_new[0], x_new[1]);
		// ObstacleFree    1:free     -1:unkown (frontier region)      0:obstacle
		char   checking=ObstacleFree(x_nearest,x_new,mapData);

		if (checking==-1){
          	exploration_goal.header.stamp=ros::Time(0);
          	exploration_goal.header.frame_id=mapData.header.frame_id;
          	exploration_goal.point.x=x_new[0];
          	exploration_goal.point.y=x_new[1];
          	exploration_goal.point.z=0.0;
          	p.x=x_new[0]; 
			p.y=x_new[1]; 
			p.z=0.0;
          	points.points.push_back(p);
          	pub.publish(points) ;
          	targetspub.publish(exploration_goal);
		  	points.points.clear();
        	
        } else if (checking==1) {
	 		V.push_back(x_new);
	 	
	 		p.x=x_new[0]; 
			p.y=x_new[1]; 
			p.z=0.0;
	 		line.points.push_back(p);
	 		p.x=x_nearest[0]; 
			p.y=x_nearest[1]; 
			p.z=0.0;
	 		line.points.push_back(p);
		}

		pub.publish(line);
   
		cbqueue.callAvailable(ros::WallDuration());
		// ros::spinOnce();
		rate.sleep();
	}
	return 0;
}
