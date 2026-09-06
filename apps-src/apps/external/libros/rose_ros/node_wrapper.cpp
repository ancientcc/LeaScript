
#include "rose_ros/node_wrapper.hpp"
#include <costmap_2d/costmap_2d_ros.h>
#include <tf2_ros/buffer.h>
#include <angles/angles.h>

#include "rose_exception.hpp"
#include <SDL_timer.h>
#include <SDL_log.h>
#include <base_local_planner/line_iterator.h>
#include <rose_ros/utils.hpp>

#include <opencv2/imgproc.hpp>

#include <SDL_filesystem.h>
#include <SDL_image.h>

namespace ros {

tnode_wrapper::tnode_wrapper()
	: check_movement_ticks_(0)
{
	// check_movement_ticks_ = SDL_GetTicks();
}

tnode_wrapper::~tnode_wrapper()
{
}

// Norm function 
float SDL_PointNorm(const SDL_Point& x1, const SDL_Point& x2)
{
	return hypot(x2.x - x1.x, x2.y - x1.y);
}

float SDL_FPointNorm(const SDL_FPoint& x1, const SDL_FPoint& x2)
{
	return hypot(x2.x - x1.x, x2.y - x1.y);
}

// Nearest function
SDL_Point Nearest(const std::vector<SDL_Point>& V, const SDL_Point&  x)
{
	float min = SDL_PointNorm(V[0], x);
	int min_index;
	float temp;

	for (int j = 0; j < (int)V.size(); j ++) {
		temp = SDL_PointNorm(V[j], x);
		if (temp <= min) {
			min = temp;
			min_index = j;
		}
	}

	return V[min_index];
}

// Starting from {from}, move forward {adv} along line{from-->target},
// and find the coordinate at {adv}.
void coordinate_advance(double from_x, double from_y, double target_x, double target_y, double adv, double& result_x, double& result_y)
{
	// adv > 0
	double m = (target_y - from_y) / (target_x - from_x);

	int s = 1;
	if (target_x - from_x < 0.0) {
		s = -1.0;
	}

	result_x = s * (sqrt( (adv * adv) / (m * m + 1) ) ) + from_x;
	result_y = (m * (result_x - from_x) + from_y );
}

//Steer function
SDL_Point Steer(const SDL_Point& x_nearest, const SDL_Point& x_rand, int eta)
{
    SDL_Point x_new;

    if (SDL_PointNorm(x_nearest, x_rand) <= eta) {
        x_new = x_rand;
    } else {
        double x_new_x;
		double x_new_y;
		coordinate_advance(x_nearest.x, x_nearest.y, x_rand.x, x_rand.y, eta, x_new_x, x_new_y);

		// Because forced rounding to int, there will be some errors
		x_new.x = x_new_x;
		x_new.y = x_new_y;

        if (x_rand.x == x_nearest.x) {
            x_new.x = x_nearest.x;
            x_new.y = x_nearest.y + eta;
        }
    }
    return x_new;
}


// ObstacleFree function-------------------------------------

enum {ObstacleFree_result_unk = -1, ObstacleFree_result_obs = 0, ObstacleFree_result_other = 1};

char ObstacleFree(const costmap_2d::Costmap2D& costmap, const SDL_Point& xnear, SDL_Point& xnew)
{
    SDL_Point xi = xnear;
    bool obs = false;
    bool unk = false;
 
	int times = 0;
	// SDL_Log("---ObstacleFree, (%i x %i)", costmap.getSizeInCellsX(), costmap.getSizeInCellsY());
	const int cells = costmap.getSizeInCellsX() * costmap.getSizeInCellsY();
	const unsigned char* raw_costmap_data = costmap.getCharMap();
	for (base_local_planner::LineIterator line(xnear.x, xnear.y, xnew.x, xnew.y); line.isValid(); line.advance()) {
		xi.x = line.getX();
		xi.y = line.getY();
		int cell_index = costmap.getIndex(line.getX(), line.getY());
		VALIDATE(cell_index >= 0 && cell_index < cells, null_str);
		int cost = raw_costmap_data[cell_index];

		// SDL_Log("[#%i], from xi(%i, %i) to xnew(%i, %i), over (%i, %i), cost: %i", 
		//	times ++, xnear.x, xnear.y, xnew.x, xnew.y, xi.x, xi.y, cost);

		const int obs_threshold = 75; // 57

        if (cost == costmap_2d::NO_INFORMATION) {
            SDL_Log("ObstacleFree, (%i, %i)unknown cell, break, obs(%s)", xi.x, xi.y, obs? "true": "false"); 
            unk = true;
            break;

        } else if (cost >= obs_threshold) { // cost == 100
            obs = true;
        }
    }
    // SDL_Log("---------xnew(%i, %i), xi(%i, %i)", xnew.x, xnew.y, xi.x, xi.y);

    char out = 0;
    xnew = xi;
    
    if (obs) {
        return ObstacleFree_result_obs;

    } else if (unk) {
        return ObstacleFree_result_unk;
    }
	return ObstacleFree_result_other;
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

static std::string log_vPoint(const std::vector<SDL_Point>& V)
{
	std::stringstream ss;
	ss << "(" << V.size() << ")";
	for (std::vector<SDL_Point>::const_iterator it = V.begin(); it != V.end(); ++ it) {
		const SDL_Point& p = *it;
		if (!ss.str().empty()) {
			ss << " ";
		}
		ss << "(" << p.x << ", " << p.y << ")";
	}
	return ss.str();
}

static std::string log_vFPoint(const std::vector<SDL_FPoint>& V)
{
	std::stringstream ss;
	ss << "(" << V.size() << ")";
	for (std::vector<SDL_FPoint>::const_iterator it = V.begin(); it != V.end(); ++ it) {
		const SDL_FPoint& p = *it;
		if (!ss.str().empty()) {
			ss << " ";
		}
		ss << "(" << p.x << ", " << p.y << ")";
	}
	return ss.str();
}

static void test()
{
	std::vector<SDL_Point> rands;
	std::vector<SDL_Point> nearest;

	SDL_Point point;
	for (int at = 0; at < 200; at ++) {
		int r = rand() % 100000 + 1;
		point.x = (((r & 1)? -1.0: 1.0) * r / 100);
		r = rand() % 100000 + 1;
		point.y = (((r & 1)? -1.0: 1.0) * r / 100);
		rands.push_back(point);

		r = rand() % 100000 + 1;
		point.x = (((r & 1)? -1.0: 1.0) * r / 100);
		r = rand() % 100000 + 1;
		point.y = (((r & 1)? -1.0: 1.0) * r / 100);
		nearest.push_back(point);
	}

	double eta = 10.79; // 0.5
	SDL_Point x_new;
	for (int at = 0; at < (int)rands.size(); at ++) {
		const SDL_Point& x_rand = rands[at];
		const SDL_Point& x_nearest = nearest[at];

		if (SDL_PointNorm(x_nearest, x_rand) > eta) {
			double x_new_x;
			double x_new_y;
			coordinate_advance(x_nearest.x, x_nearest.y, x_rand.x, x_rand.y, eta, x_new_x, x_new_y);

			x_new.x = x_new_x;
			x_new.y = x_new_y;

			SDL_Log("#%i(1) n(%i, %i) -- r(%i, %i) ==> (%i, %i) distance: %.5f",
				at, x_nearest.x, x_nearest.y, x_rand.x, x_rand.y, x_new.x, x_new.y, SDL_PointNorm(x_new, x_nearest));

			SDL_Log("#%i(2) n(%i, %i) -- r(%i, %i) ==> (%.5f, %.5f) distance: %.5f",
				at, x_nearest.x, x_nearest.y, x_rand.x, x_rand.y, x_new_x, x_new_y, 
					SDL_FPointNorm(SDL_FPoint{(float)x_new_x, (float)x_new_y}, SDL_FPoint{1.0f * x_nearest.x, 1.0f * x_nearest.y}));

		} else {
			SDL_Log("#%i n(%i, %i) -- r(%i, %i) distance: %.5f <= eta",
				at, x_nearest.x, x_nearest.y, x_rand.x, x_rand.y, SDL_PointNorm(x_rand, x_nearest));
		}
	}
}

static void clip(SDL_Point& cell, int width, int height)
{
	if (cell.x < 0) {
		cell.x = 0;
	}
	if (cell.y < 0) {
		cell.y = 0;
	}
	if (cell.x >= width) {
		cell.x = width - 1;
	}
	if (cell.y >= height) {
		cell.y = height - 1;
	}
}

bool tnode_wrapper::exploration_detect_rrt(const costmap_2d::Costmap2D& costmap, const tpose2d& robot_pose, geometry_msgs::Point& unk)
{
	// test();
    const uint8_t* raw_costmap_data = costmap.getCharMap();
    const int size_x = costmap.getSizeInCellsX();
    const int size_y = costmap.getSizeInCellsY();
	VALIDATE(costmap.getResolution() > 0 && size_x > 0 && size_y > 0, null_str);

    unsigned int origin_map_x;
    unsigned int origin_map_y;
    bool valid = costmap.worldToMap(robot_pose.x, robot_pose.y, origin_map_x, origin_map_y);
    if (!valid) {
        // if trigger breakpoint during visual studio debugging, may enter here.
        return false;
    }

	int eta = 4.0 / costmap.getResolution();
	SDL_Point tl{0, size_y - 1};
	SDL_Point br{size_x - 1, 0};
	SDL_Point tr{br.x, tl.y};
	SDL_Point bl{tl.x, br.y};

	const float Xstartx = (tl.x + br.x) / 2;
	const float Xstarty = (tl.y + br.y) / 2;

	// float init_map_x = SDL_PointNorm(tr, tl);
	// float init_map_y = SDL_PointNorm(tl, bl);
	int init_map_x = SDL_PointNorm(tr, tl);
	int init_map_y = SDL_PointNorm(tl, bl);

	SDL_Point x_rand, x_nearest, x_new;


	srand((unsigned)(time(nullptr)));

	double world_x, world_y;
	std::vector<SDL_Point> V;

	for (std::vector<SDL_FPoint>::const_iterator it = V_.begin(); it != V_.end(); ++ it) {
		const SDL_FPoint& src = *it;
		int map_x;
		int map_y;
		costmap.worldToMapEnforceBounds(src.x, src.y, map_x, map_y);
		V.push_back(SDL_Point{map_x, map_y});
	}

	if (V.empty()) {
		SDL_Point xnew{init_map_x / 2, init_map_y / 2};
		// xnew.x = origin_map_x + (init_map_x - origin_map_x) / 2;
		// xnew.y = origin_map_y + (init_map_y - origin_map_y) / 2;  
		V.push_back(xnew);
		costmap.mapToWorld(xnew.x, xnew.y, world_x, world_y);
		V_.push_back(SDL_FPoint{(float)world_x, (float)world_y});
	}

	SDL_Log("exploration_detect, eta: %i, init_map(%i x %i), V_: %s, V: %s", 
		eta, init_map_x, init_map_y, log_vFPoint(V_).c_str(), log_vPoint(V).c_str());

	bool result = false;
	int rest_try = 100;
	while (rest_try >= 0) {
		// Sample free
		// x_rand.x = (drand_() * init_map_x) - (init_map_x * 0.5) + Xstartx; 
		// x_rand.y = (drand_() * init_map_y) - (init_map_y * 0.5) + Xstarty;

		x_rand.x = (rand() % (init_map_x - 2)) + 1;
		x_rand.y = (rand() % (init_map_y - 2)) + 1; 
		clip(x_rand, init_map_x, init_map_y);

		// Nearest
		x_nearest = Nearest(V, x_rand);
		clip(x_nearest, init_map_x, init_map_y);

		// Steer
		x_new = Steer(x_nearest, x_rand, eta);

		SDL_Log("#%i, %u, x_rand(%i, %i), x_nearest(%i, %i), x_new(%i, %i)", 
			rest_try, SDL_GetTicks(), x_rand.x, x_rand.y, x_nearest.x, x_nearest.y, x_new.x, x_new.y);
		rest_try --;
		// ObstacleFree    1:free     -1:unkown (frontier region)      0:obstacle
		signed char  checking = ObstacleFree(costmap, x_nearest, x_new);

		if (checking == ObstacleFree_result_unk) {
			costmap.mapToWorld(x_new.x, x_new.y, world_x, world_y);
			SDL_Log("x_new(%i, %i) should send goal(%.5f, %.5f)", 
				x_new.x, x_new.y, world_x, world_y);
			unk.x = world_x;
			unk.y = world_y;
			unk.z = 0.0;
			result = true;
			break;
/*
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
*/
        	
        } else if (checking == ObstacleFree_result_other) {
	 		V.push_back(x_new);

			costmap.mapToWorld(x_new.x, x_new.y, world_x, world_y);
			V_.push_back(SDL_FPoint{(float)world_x, (float)world_y});
/*	 	
	 		p.x=x_new[0]; 
			p.y=x_new[1]; 
			p.z=0.0;
	 		line.points.push_back(p);
	 		p.x=x_nearest[0]; 
			p.y=x_nearest[1]; 
			p.z=0.0;
	 		line.points.push_back(p);
*/
		}

		// pub.publish(line);   
	}
	return result;
}

void save_mat_8UC1(const cv::Mat& src, bool vflip, const std::string& filename, const std::set<tpoint>* specials)
{
	const uint8_t* src_data = src.ptr<uint8_t>(0);

	int pos = 0;
    SDL_Surface* photo = SDL_CreateRGBSurface(0, src.cols, src.rows, 4 * 8,
			0xFF0000, 0xFF00, 0xFF, 0xFF000000); // SDL_PIXELFORMAT_ARGB8888
	uint32_t* pixels = (uint32_t*)photo->pixels;
    memset(pixels, 0, src.cols * src.rows);
    for (int y = 0; y < src.rows; y ++) {
		for(int x = 0; x < src.cols; x ++) {
			const int y2 = vflip? src.rows - y - 1: y;
			int index = x + y2 * src.cols;
            const uint8_t u8 = src_data[index];
            uint32_t val = posix_mku32(posix_mku16(u8, u8), posix_mku16(u8, 0xff));
			if (specials != nullptr && specials->count(tpoint(x, y2))) {
				val = 0xffff0000;
			}
            pixels[pos ++] = val;
		}
    }
    VALIDATE(pos == src.cols * src.rows, null_str);

	IMG_SavePNG(photo, filename.c_str());
    SDL_FreeSurface(photo);
}

bool tnode_wrapper::exploration_detect(const tpose2d& robot_pose, geometry_msgs::Point& unk)
{
	const bool use_rrt = true;

	VALIDATE(robot_pose.valid, null_str);
	if (costmap_2d::global_costmap_ros == nullptr) {
		return false;
	}
	// unsigned int init[4] = {0x123, 0x234, 0x345, 0x456}, length = 7;
	// MTRand_int32 irand(init, length); // 32-bit int generator

	boost::unique_lock<costmap_2d::Costmap2D::mutex_t> lock(*(costmap_2d::global_costmap_ros->getCostmap()->getMutex()));
	costmap_2d::Costmap2DROS& costmap_ros = *costmap_2d::global_costmap_ros;
	
    const costmap_2d::Costmap2D& costmap = *costmap_ros.getCostmap();
	if (use_rrt) {
		return exploration_detect_rrt(costmap, robot_pose, unk);
	}

    const uint8_t* raw_costmap_data = costmap.getCharMap();
    const int size_x = costmap.getSizeInCellsX();
    const int size_y = costmap.getSizeInCellsY();
	VALIDATE(costmap.getResolution() > 0 && size_x > 0 && size_y > 0, null_str);

    unsigned int origin_map_x;
    unsigned int origin_map_y;
    bool valid = costmap.worldToMap(robot_pose.x, robot_pose.y, origin_map_x, origin_map_y);
    if (!valid) {
        // if trigger breakpoint during visual studio debugging, may enter here.
        return false;
    }

	// <rrt_exploration>/scripts/getfrontier.py
	// data = mapData.data
	int w = size_x;
	int h = size_y;

	double resolution = costmap.getResolution();
	// double Xstartx = robot_pose.x; // costmap.getOriginX();
	// double Xstarty = robot_pose.y; // costmap.getOriginY();
	
	const int cells = size_x * size_y;
	const uint8_t* data = raw_costmap_data;
	cv::Mat img = cv::Mat(size_y, size_x, CV_8UC1);
    uint8_t* img_data = img.ptr<uint8_t>(0);
	memset(img_data, 0, cells);
	
	cv::Mat o;
	int index = 0;
	for (int i = 0; i < h; i ++) {
		for (int j = 0; j < w; j ++) {
			int src_index = i * w + j;
			if (data[src_index] == 100) {
			// if (data[src_index] >= 57) {
				img_data[index] = 0;

			} else if (data[src_index] == 0) {
				img_data[index] = 255;

			} else if (data[src_index] == 255) {
				img_data[index] = 205;
			}
			index ++;
		}
       	cv::inRange(img, 0, 1, o);
	}
	VALIDATE(index == size_x * size_y, null_str);

	cv::Mat edges;
	// edges = cv::Canny(img, 0, 255)
	cv::Canny(img, edges, 0, 255);

	// im2, contours, hierarchy = cv2.findContours(o, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
	std::vector<std::vector<cv::Point> > contours;
	cv::findContours(o, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

	// cv2.drawContours(o, contours, -1, (255,255,255), 5)
	cv::drawContours(o, contours, -1, (255,255,255), 5);

	// o=cv2.bitwise_not(o);
	cv::bitwise_not(o, o);

	// res = cv2.bitwise_and(o,edges)
	cv::Mat res;
	cv::bitwise_and(o, edges, res);
	// #------------------------------

	cv::Mat frontier;
	// frontier=copy(res)
	res.copyTo(frontier);
	// im2, contours, hierarchy = cv2.findContours(frontier,cv2.RETR_TREE,cv2.CHAIN_APPROX_SIMPLE)
	cv::findContours(frontier, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

	// cv2.drawContours(frontier, contours, -1, (255,255,255), 2)
	cv::drawContours(frontier, contours, -1, (255,255,255), 2);

	// im2, contours, hierarchy = cv2.findContours(frontier,cv2.RETR_TREE,cv2.CHAIN_APPROX_SIMPLE)
	cv::findContours(frontier, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

	save_mat_8UC1(img, true, game_config::preferences_dir + "/1_img.png", nullptr);
	save_mat_8UC1(edges, true, game_config::preferences_dir + "/1_edges.png", nullptr);
	save_mat_8UC1(o, true, game_config::preferences_dir + "/1_o.png", nullptr);
	save_mat_8UC1(res, true, game_config::preferences_dir + "/1_res.png", nullptr);
	save_mat_8UC1(frontier, true, game_config::preferences_dir + "/1_frontier.png", nullptr);

	std::vector<cv::Point2d> all_pts;
	std::set<tpoint> specials;
	// std::vector<std::vector<cv::Point> > contours;
	// all_pts=[]
	// if len(contours)>0:
	if (!contours.empty()) {
		// upto=len(contours)-1
		// i=0
		// maxx=0
		// maxind=0
		
		double world_x;
		double world_y;
		// for i in range(0,len(contours)):
		for (std::vector<std::vector<cv::Point> >::const_iterator it = contours.begin(); it != contours.end(); ++ it) {
			// cnt = contours[i]
			const std::vector<cv::Point>& cnt = *it;
			// M = cv2.moments(cnt)
			cv::Moments M = cv::moments(cnt);
			// cx = int(M['m10']/M['m00'])
			int cx = int(M.m10 / M.m00);
			// cy = int(M['m01']/M['m00'])
			int cy = int(M.m01 / M.m00);
			costmap.mapToWorld(cx, cy, world_x, world_y);
			// pt=[np.array([xr,yr])]
			// if len(all_pts)>0:
			//	all_pts=np.vstack([all_pts,pt])
			// else:
			//	all_pts=pt
			all_pts.push_back(cv::Point2d(world_x, world_y));
			specials.insert(tpoint(cx, cy));
		}

		unk.x = all_pts.front().x;
		unk.y = all_pts.front().y;
		unk.z = 0.0;

		save_mat_8UC1(frontier, true, game_config::preferences_dir + "/1_frontier2.png", &specials);
		return true;
	}

	return false;

}

trose_slot::trose_slot()
	: tid(0)
	, normal_page_dirty_(false)
	, computeVelocityCommands_id_(0)
	, narrow_(false)
	, top_is_narrow_(false)
	, straight_ward_(false)
	, use_negative_vel_(false)
	, xy_tolerance_latch_(false)
	, yaw_goal_tolerance_(0)
	, front_pressure_(false)
	, backward_ang_diff_(0)
{}

void trose_slot::did_post_adjust_global_plan(uint32_t id, bool narrow, bool top_is_narrow, bool straight_ward)
{
	threading::lock lock(mutex_);

	computeVelocityCommands_id_ = id;
	narrow_ = narrow;
	top_is_narrow_ = top_is_narrow;
	straight_ward_ = straight_ward;
}

void trose_slot::did_adjust_traj_backward(bool front_pressure, double ang_diff, const std::string& desc)
{
	threading::lock lock(mutex_);

	front_pressure_ = front_pressure;
	backward_ang_diff_ =  ang_diff;
	adjust_traj_backward_desc_ = desc;
}

void trose_slot::set_cmd_vel(const geometry_msgs::Twist& cmd_vel, bool use_negative_vel, bool xy_tolerance_latch, double yaw_goal_tolerance)
{
	threading::lock lock(mutex_);

	cmd_vel_ = cmd_vel;
	use_negative_vel_ = use_negative_vel;
	xy_tolerance_latch_ = xy_tolerance_latch;
	yaw_goal_tolerance_ = yaw_goal_tolerance;
	normal_page_dirty_ = true;
}

std::string trose_slot::to_string(uint32_t navigation_start_ticks)
{
	threading::lock lock(mutex_);
	normal_page_dirty_ = false;

	char laser_tf_buf[64] = {"---"};
	if (ros::base_cfg.laser_to_base_footprint_tf.valid) {
		SDL_snprintf(laser_tf_buf, sizeof(laser_tf_buf), "%.4f, %.4f, yaw: %.1f", ros::base_cfg.laser_to_base_footprint_tf.x, ros::base_cfg.laser_to_base_footprint_tf.y, RAD2DEG(ros::base_cfg.laser_to_base_footprint_tf.yaw));
	}
	char dcamera_tf_buf[64] = {"---"};
	if (ros::base_cfg.dcamera_to_base_footprint_tf.valid) {
		SDL_snprintf(dcamera_tf_buf, sizeof(dcamera_tf_buf), "%.4f, %.4f, yaw: %.1f", ros::base_cfg.dcamera_to_base_footprint_tf.x, ros::base_cfg.dcamera_to_base_footprint_tf.y, RAD2DEG(ros::base_cfg.dcamera_to_base_footprint_tf.yaw));
	}

	char params_buf[160];
	SDL_snprintf(params_buf, sizeof(params_buf), "min_vel_x: %.3f\nmin_vel_theta: %.3f\ncontroller_freq: %.2f\nlaser_tf: %s\ndcamera_tf: %s",
		ros::base_cfg.min_moveable_vel_x, RAD2DEG(ros::base_cfg.min_moveable_vel_theta), ros::base_cfg.move_base_controller_freq,
		laser_tf_buf, dcamera_tf_buf);

	char backward_buf[128];
	if (!adjust_traj_backward_desc_.empty()) {
		SDL_snprintf(backward_buf, sizeof(backward_buf), "(%.3f)%s", RAD2DEG(backward_ang_diff_), adjust_traj_backward_desc_.c_str());
	} else {
		SDL_strlcpy(backward_buf, "---", sizeof(backward_buf));
	}

	// path_normal, path_only_backward
	char path_cases[][20] = { "normal", "only_backward"};
	// VALIDATE(sizeof(path_cases) / sizeof(path_cases[0]) == path_count, null_str);

	char buf[512];

	SDL_snprintf(buf, sizeof(buf), "%s\n\n---#%u---\nnarrow: %s\ntop_is_narrow_: %s\nstraight_ward: %s\nuse_negative_vel: %s\nfront_pressure: %s\nbackward: %s\nxy_tolerance_latch: %s\nyaw_goal_tolerance: %.3f\nvel:(%.3f, %.3f, %.3f)", 
		params_buf, computeVelocityCommands_id_, narrow_? "true": "false", top_is_narrow_? "true": "false", straight_ward_? "true": "false", use_negative_vel_? "true": "false",
		front_pressure_? "yes": "no", backward_buf, xy_tolerance_latch_? "true": "false", RAD2DEG(yaw_goal_tolerance),
		cmd_vel_.linear.x, cmd_vel_.linear.y, RAD2DEG(cmd_vel_.angular.z));

	front_pressure_ = false;
	backward_ang_diff_ = 0;
	adjust_traj_backward_desc_.clear();
	return buf;
}

trose_slot rose_slot;

}