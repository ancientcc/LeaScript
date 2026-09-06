

// #include <nav_msgs/MapMetaData.h>
// #include <nav_msgs/LoadMap.h>

#include <rose_ros/utils.hpp>
#include "rose_exception.hpp"
#include <tf2/LinearMath/Quaternion.h>

// #include <costmap_2d/costmap_2d.h>
#include <costmap_2d/footprint.h>
#include <base_local_planner/line_iterator.h>
#include <rose_ros/pathfind.hpp>
#include <geometry_msgs/PoseStamped.h>
#include <angles/angles.h>
#include <SDL_image.h>
#include "rose_sdl_utils.hpp"
#include <tf2/utils.h>

namespace dwa_local_planner {
extern int distinguishable_obs_cells(const costmap_2d::Costmap2D& costmap, const SDL_Point& robot_cell, const std::set<tpoint>& _obs_cells, bool verbose, int& cell_dist, std::vector<SDL_Point>* ret_ptr);
extern bool get_footprint_obs_cells(costmap_2d::Costmap2D& costmap, const std::vector<geometry_msgs::Point>& footprint, const tpose2d& robot_pose, double padding_x, double padding_y, double yaw_increment, std::vector<SDL_Point>& obs_cells);
}

namespace ros {
/*
tbase_cfg::tbase_cfg()
    : min_moveable_vel_x(ROS_DEF_MIN_MOVEABLE_VEL_X)
    , min_moveable_vel_theta(ROS_DEF_MIN_MOVEABLE_VEL_THETA)
    , max_buildmap_vel_x(ROS_DEF_MAX_BUILDMAP_VEL_X)
    , max_buildmap_vel_theta(ROS_DEF_MAX_BUILDMAP_VEL_THETA)
    , max_navigation_vel_x(ROS_DEF_MAX_NAVIGATION_VEL_X)
    , max_navigation_vel_theta(ROS_DEF_MAX_NAVIGATION_VEL_THETA)
    , move_base_controller_freq(ROS_DEF_MOVE_BASE_CONTROLLER_FREQ)
    , robot_length(ROS_DEF_ROBOT_LENGTH)
    , robot_width(ROS_DEF_ROBOT_WIDTH)
{}
tbase_cfg base_cfg;

std::string footprint_string = ROS_DEF_FOOTPRINT_STRING;

bool buildmap = false;
*/
double xy_goal_tolerance = ROS_DEF_XY_GOAL_TOLERANCE;
double yaw_goal_tolerance = ROS_DEF_YAW_GOAL_TOLERANCE; // M_PI/DEG2RAD(20)
bool rviz_breakpoint = false;
bool use_follow = false;

bool dcamera_installed = true;
double dcamera_height_from_ground = 0.5f; // 50cm, let moveit enter state_navigation, can estimate it. 
double safe_obstacle_height = 0.18f; // 18cm

bool dwa_straight_ward = false;

double get_yaw_goal_tolerance()
{
    return yaw_goal_tolerance;
}

bool get_use_follow()
{
    return use_follow;
}

double shortest_angular_distance2(double from, double to_x1, double to_y1, double to_x2, double to_y2, double* angle_ptr)
{
    double deltax = to_x2 - to_x1;
    double deltay = to_y2 - to_y1;
    // (-pi, pi]
    double angle = atan2(deltay, deltax);

    double ang_diff = angles::shortest_angular_distance(from, angle);
    if (angle_ptr != nullptr) {
        *angle_ptr = angle;
    }
    return ang_diff;
}

std::vector<geometry_msgs::Point> getNarrowFootprint(double robot_width)
{
    // double robot_width = 0.3;

    double resolution = 0.05;
    double min_narrow_width = robot_width + resolution * 2; // more 0.1cm, tow Resolution
    double x_radius = 0.075;

    double y_radius = min_narrow_width - robot_width / 2 + resolution; // resolution / 2

    std::vector<geometry_msgs::Point> footprint;
    geometry_msgs::Point point;
    // [[-0.075, -0.275], [-0.075, 0.275], [0.075, 0.275], [0.075, -0.275]]
    point.x = -1 * x_radius;
    point.y = -1 * y_radius;
    footprint.push_back(point);

    point.x = -1 * x_radius;
    point.y = y_radius;
    footprint.push_back(point);

    point.x = x_radius;
    point.y = y_radius;
    footprint.push_back(point);

    point.x = x_radius;
    point.y = -1 * y_radius;
    footprint.push_back(point);
    return footprint;
}

#pragma pack()

tmap_position get_special_position(const std::string& uuid)
{
    double initial_x = float_nposm;
    double initial_y = float_nposm;
    double initial_theta = float_nposm;
    std::string name;

    if (uuid == charge_pos_uuid) {
        VALIDATE(!charge_pos_name.empty(), null_str);
        name = charge_pos_name;

    } else {
        VALIDATE(false, "Unknown special position uuid");
    }

    return tmap_position(uuid, name, initial_x, initial_y, initial_theta);
}

void insert_special_position(const std::string& uuid, tros_map& ros_map)
{
    VALIDATE(ros_map.positions.count(uuid) == 0, null_str);

    ros_map.positions.insert(std::make_pair(uuid, get_special_position(uuid)));
}

bool load_map_from_rsp(const std::string& path_to_rsp, tros_map& ros_map)
{
	ros_map.clear();

    tsha1reader src(path_to_rsp, false, NULL);
	VALIDATE(src.valid(), null_str);
	int payload_size = src.verify_sha1();
	if (payload_size < sizeof(trsp_header) + sizeof(trsp_rosmap80bytes)) {
		return false;
	}

	trsp_header header;
	memset(&header, 0, sizeof(header));
	posix_fread(src.fp, &header, sizeof(trsp_header));
	if (header.fourcc != SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_rosmap))) {
		return false;
	}
    if (header.version != SDL_FOURCC(0, 0, 0, RSP_MAP_VER)) {
        return false;
    }

	trsp_rosmap80bytes rosmap;
	memset(&rosmap, 0, sizeof(rosmap));
	posix_fread(src.fp, &rosmap, sizeof(rosmap));

	if (rosmap.desc[RSP_MAXDESCBYTES] != '\0') {
		return false;
	}
	if (rosmap.map == 0 || rosmap.reserve0 != 0) {
		return false;
	}
	if (payload_size != sizeof(trsp_header) + sizeof(trsp_rosmap80bytes) + rosmap.map + rosmap.positions + rosmap.markers) {
		return false;
	}
	if ((rosmap.positions % sizeof(trsp_rosmapposition)) != 0) {
		return false;
	}
	if ((rosmap.markers % sizeof(trsp_rosmapmarker)) != 0) {
		return false;
	}

	ros_map.desc = rosmap.desc;
    // ros_map.charge = tpose2d(rosmap.charge_x, rosmap.charge_y, rosmap.charge_theta);

    uint8_t* p = new uint8_t[SDL_max(rosmap.map, rosmap.positions + rosmap.markers)];
	posix_fread(src.fp, p, rosmap.map);
	boost::shared_array<uint8_t> buf(p);
	ros::SerializedMessage serialized(buf, rosmap.map);

	ros::serialization::deserializeMessage<nav_msgs::OccupancyGrid>(serialized, ros_map.map);
    int width = ros_map.map.info.width;
    int height = ros_map.map.info.height;
    if (width <= 0 || height <= 0) {
        return false;
    }
    const int8_t* map_data = &ros_map.map.data[0];
    int cells = width * height;
    for (int at = 0; at < cells; at ++) {
        const int8_t raw_i8 = map_data[at];
        if (raw_i8 < -1 || raw_i8 > MAX_CARTOGRAPHER_CELL_VAL) {
            return false;
        }
    }

	if (rosmap.positions != 0) {
		posix_fread(src.fp, p, rosmap.positions);
		trsp_rosmapposition* pos_base = (trsp_rosmapposition*)p;
		const int position_count = rosmap.positions / sizeof(trsp_rosmapposition);

		for (int at = 0; at < position_count; at ++) {
			const trsp_rosmapposition& pos = pos_base[at];
            if (!utils::is_uuid(pos.uuid, true)) {
                continue;
            }
			std::pair<std::map<std::string, tmap_position>::iterator, bool> ins = 
				ros_map.positions.insert(std::make_pair(pos.uuid, tmap_position(pos.uuid, pos.name, pos.x, pos.y, pos.theta)));

			tmap_position& position = ins.first->second;
		}
	}

    if (rosmap.markers != 0) {
        posix_fread(src.fp, p, rosmap.markers);
		trsp_rosmapmarker* marker_base = (trsp_rosmapmarker*)p;
		const int marker_count = rosmap.markers / sizeof(trsp_rosmapmarker);

		for (int at = 0; at < marker_count; at ++) {
			const trsp_rosmapmarker& rsp_marker = marker_base[at];
            if (!utils::is_uuid(rsp_marker.uuid, true)) {
                continue;
            }
            if (rsp_marker.type < 0 || rsp_marker.type >= rspmapmarkertype_count) {
                continue;
            }
            if (rsp_marker.width < RSP_MIN_WALL_WIDTH || rsp_marker.width > RSP_MAX_WALL_WIDTH) {
                continue;
            }
            if (rsp_marker.height != 0) {
                continue;
            }

            ros_map.markers.insert(std::make_pair(rsp_marker.uuid, tmap_marker(rsp_marker)));
		}
    }

    if (ros_map.positions.count(charge_pos_uuid) == 0) {
        insert_special_position(charge_pos_uuid, ros_map);

    } else {
        // this time maybe change charge_name.
        tmap_position& position = ros_map.positions.find(charge_pos_uuid)->second;
        position.name = charge_pos_name;
    }

    ros_map.rspfile = path_to_rsp;
    return true;
}

#pragma pack(1)
struct tlaser_msg_header {
	uint32_t fourcc;
	int msg_len;
	int data_len;
};
#pragma pack()


bool LaserScan_write(const sensor_msgs::LaserScan& scan_msg, const tvoidcdata_C* data, const std::string& path)
{
	if (data != nullptr) {
		VALIDATE(data->ptr == nullptr && data->len == 0 || data->ptr != nullptr && data->len > 0, null_str);
	}
	VALIDATE(!path.empty(), null_str);

	ros::SerializedMessage serialized = ros::serialization::serializeMessage<sensor_msgs::LaserScan>(scan_msg);
	int msg_len = serialized.num_bytes - (serialized.message_start - serialized.buf.get());
	const uint8_t* serialized_msg_data = serialized.message_start;

	std::string filename = path;
	if (!SDL_IsFromRootPath(path.c_str())) {
		filename = game_config::preferences_dir + "/" + path;
	}

	tfile file(filename, GENERIC_WRITE, CREATE_ALWAYS);
	if (!file.valid()) {
		return false;
	}
	tlaser_msg_header header;
	header.fourcc = SDL_FOURCC('L', 'A', 'S', 0x1);
	header.msg_len = msg_len;
	header.data_len = data != nullptr? data->len: 0;

	posix_fwrite(file.fp, &header, sizeof(tlaser_msg_header));
	posix_fwrite(file.fp, serialized_msg_data, msg_len);
	if (header.data_len != 0) {
		posix_fwrite(file.fp, data->ptr, header.data_len);
	}
	return true;
}

bool LaserScan_read(const std::string& path, sensor_msgs::LaserScan& scan_msg, const tvoiddata_C* data)
{
	if (data != nullptr) {
		VALIDATE(data->ptr == nullptr && data->len == 0 || data->ptr != nullptr && data->len > 0, null_str);
	}

	std::string filename = path;
	if (!SDL_IsFromRootPath(path.c_str())) {
		filename = game_config::preferences_dir + "/" + path;
	}
	scan_msg.header.frame_id.clear();

	tfile file(filename, GENERIC_READ, OPEN_EXISTING);
	if (!file.valid()) {
		return false;
	}
	int fsize = posix_fsize(file.fp);
	if (fsize < sizeof(tlaser_msg_header)) {
		return false;
	}

	posix_fseek(file.fp, 0);

	tlaser_msg_header header;
	posix_fread(file.fp, &header, sizeof(tlaser_msg_header));

	const int require_data_len = data != nullptr? data->len: 0;
	if (header.data_len != require_data_len) {
		return false;
	}

	if (fsize != sizeof(tlaser_msg_header) + header.msg_len + header.data_len) {
		return false;
	}
	
	uint8_t* p = new uint8_t[header.msg_len];
	posix_fread(file.fp, p, header.msg_len);
	boost::shared_array<uint8_t> buf(p);
	ros::SerializedMessage serialized(buf, header.msg_len);

	ros::serialization::deserializeMessage<sensor_msgs::LaserScan>(serialized, scan_msg);

	if (header.data_len != 0) {
		posix_fread(file.fp, data->ptr, header.data_len);
	}
	return true;
}

static void lineCost(int x0, int x1, int y0, int y1, const costmap_2d::Costmap2D& costmap, uint32_t* pixels, int size_x, int size_y, int margin_x, int margin_y, int cell_size)
{
    const int pitch = margin_x + size_x * cell_size + margin_x;
    for (base_local_planner::LineIterator line( x0, y0, x1, y1 ); line.isValid(); line.advance()) {
        u8_data_fill_cell_val(margin_x, margin_y, line.getX(), size_y - 1 - line.getY(), pitch, cell_size, pixels, 0xffffff00, false);
    }
}

SDL_Surface* costmap2d_2_SDL_Surface(const costmap_2d::Costmap2D& costmap, const std::string& file_name, 
    double robot_x, double robot_y, double robot_yaw, const std::vector<geometry_msgs::PoseStamped>& transformed_plan, const std::vector<geometry_msgs::Point>& _footprint)
{
    uint32_t start = SDL_GetTicks();

    const uint8_t* raw_costmap_data = costmap.getCharMap();
    const int size_x = costmap.getSizeInCellsX();
    const int size_y = costmap.getSizeInCellsY();

    unsigned int origin_map_x;
    unsigned int origin_map_y;
    bool valid = costmap.worldToMap(robot_x, robot_y, origin_map_x, origin_map_y);
    if (!valid) {
        // if trigger breakpoint during visual studio debugging, may enter here.
        VALIDATE(false, null_str);
        return nullptr;
    }

    const int margin_x = COSTMAP2D_SURF_MARGIN_X;
    const int margin_y = COSTMAP2D_SURF_MARGIN_Y;
    const int size_x2 = margin_x + size_x + margin_x;
    const int size_y2 = margin_y + size_y + margin_y;
    int cell_size = COSTMAP2D_SURF_CELL_SIZE;

    SDL_Surface* photo = u8_data_2_cell_surf(0xffffffff, raw_costmap_data, margin_x, margin_y, 
        size_x, size_y, cell_size, 0xff788b8a, 0, maptype_costmap, nullptr);
    uint32_t* pixels = (uint32_t*)photo->pixels;

    int at2 = 0;
    for (std::vector<geometry_msgs::PoseStamped>::const_iterator it = transformed_plan.begin(); 
        it < transformed_plan.end(); ++ it, at2 ++) {

        const geometry_msgs::Pose& pose = it->pose;
        double g_x = pose.position.x;
        double g_y = pose.position.y;
        unsigned int map_x, map_y;
        // if (costmap.worldToMap(g_x, g_y, map_x, map_y) && costmap.getCost(map_x, map_y) != costmap_2d::NO_INFORMATION) {
        if (costmap.worldToMap(g_x, g_y, map_x, map_y)) {
            // last point maybe out of costmap.
            // int index = margin_x + map_x + (margin_y + size_y - map_y - 1) * size_x2;
            // pixels[index] = 0xff0000ff;
            // SDL_Log("[%i/%i](%.3f, %.3f) --> (%i, %i)", at2, (int)transformed_plan.size(), g_x, g_y, map_x, map_y);
            u8_data_fill_cell_val(margin_x, margin_y, map_x, size_y - 1 - map_y, photo->w, cell_size, pixels, 0xff0000ff, false);
        }
    }

    std::vector<geometry_msgs::Point> transformed_footprint_;

    if (!_footprint.empty()) {
        costmap_2d::transformFootprint(robot_x, robot_y, robot_yaw, _footprint, transformed_footprint_);

        // unsigned int map_x, map_y;
        // we need to rasterize each line in the footprint
        const std::vector<geometry_msgs::Point>& footprint = transformed_footprint_;
        unsigned int x0, x1, y0, y1;
        for (unsigned int i = 0; i < footprint.size() - 1; ++i){
            //get the cell coord of the first point
            if (!costmap.worldToMap(footprint[i].x, footprint[i].y, x0, y0)) {
                continue;
                // return -3.0;
            }

            //get the cell coord of the second point
            if (!costmap.worldToMap(footprint[i + 1].x, footprint[i + 1].y, x1, y1)) {
                continue;
                // return -3.0;
            }

            lineCost(x0, x1, y0, y1, costmap, pixels, size_x, size_y, margin_x, margin_y, cell_size);
        }

        //we also need to connect the first point in the footprint to the last point
        //get the cell coord of the last point
        valid = costmap.worldToMap(footprint.back().x, footprint.back().y, x0, y0);
            // SDL_Log("(CostmapModel::footprintCost)(%i, %i) -3.0", (int)x0, (int)y0);
            // return -3.0;

        //get the cell coord of the first point
        if (valid && costmap.worldToMap(footprint.front().x, footprint.front().y, x1, y1)) {
            // SDL_Log("(CostmapModel::footprintCost)(%i, %i) -3.0", (int)x1, (int)y1);
            lineCost(x0, x1, y0, y1, costmap, pixels, size_x, size_y, margin_x, margin_y, cell_size);
        }
    }

    // pixels[margin_x + origin_map_x + (margin_y + size_y - origin_map_y - 1) * size_x2] = 0xff00ff00;
    u8_data_fill_cell_val(margin_x, margin_y, origin_map_x, size_y - 1 - origin_map_y, photo->w, cell_size, pixels, 0xff00ff00, false);

    uint32_t stop = SDL_GetTicks();
    SDL_Log("costmap2d_2_SDL_Surface cost: %u ms", stop - start);
    if (!file_name.empty()) {
        IMG_SavePNG(photo, file_name.c_str());
        SDL_FreeSurface(photo);
        return nullptr;
    }
    return photo;
}

tpose2d calcuate_target_pose2d(const tpose2d& source_pose, const tpose2d& source_2_taget_pose, bool verbose)
{
    geometry_msgs::TransformStamped transform;
    transform.transform.translation.x = source_2_taget_pose.x;
    transform.transform.translation.y = source_2_taget_pose.y;
    transform.transform.translation.z = 0;

    tf2::Quaternion q;
    q.setRPY(0, 0, source_2_taget_pose.yaw);

    transform.transform.rotation.x = q.x();
    transform.transform.rotation.y = q.y();
    transform.transform.rotation.z = q.z();
    transform.transform.rotation.w = q.w();

    geometry_msgs::Pose t_out;

    geometry_msgs::Pose t_in;
    tf2::toMsg(tf2::Transform::getIdentity(), t_in);
    t_in.position.x = source_pose.x;
    t_in.position.y = source_pose.y;
    t_in.position.z = 0;

    tf2::Quaternion r;
    r.setRPY(0, 0, source_pose.yaw);
    tf2::convert(r, t_in.orientation);

    tf2::doTransform(t_in, t_out, transform);
    if (verbose) {
        SDL_Log("source: (%.3f, %.3f, %.3f) --> target: (%.3f, %.3f, %.3f)", 
            t_in.position.x, t_in.position.y, RAD2DEG(tf2::getYaw(t_in.orientation)), 
            t_out.position.x, t_out.position.y, RAD2DEG(tf2::getYaw(t_out.orientation)));
    }
    return tpose2d(t_out.position.x, t_out.position.y, tf2::getYaw(t_out.orientation));
}

#define FIRST_DIST_THRESHOLD        0.4 // 0.4m

bool transformed_plan_2_plan_points(const tpose2d& robot_pose, const std::vector<geometry_msgs::PoseStamped>& transformed_plan, const costmap_2d::Costmap2D& costmap, 
    std::vector<SDL_Point>& plan_points, int& diff_points, int& min_x, int& min_y, int& max_x, int& max_y)
{
    VALIDATE(!transformed_plan.empty(), null_str);
    plan_points.clear();
    diff_points = 0;
    min_x = INT32_MAX;
    min_y = INT32_MAX;
    max_x = INT32_MIN;
    max_y = INT32_MIN;

    const uint8_t* raw_costmap_data = costmap.getCharMap();

    struct ttmp_point {
        SDL_Point map_xy;
        uint8_t cost;
        int followup_not_NO_INFORMATIONs;
        uint64_t key;

        double sq_dist;
        SDL_DPoint g_xy;
    };

    std::vector<ttmp_point> tmp_points;
    tmp_points.resize(transformed_plan.size());
    ttmp_point* tmp_points_ptr = &tmp_points[0];

    double x_diff;
    double y_diff;
    int last_NO_INFORMATION_at = nposm;
    int at = 0;
    for (std::vector<geometry_msgs::PoseStamped>::const_iterator it = transformed_plan.begin(); 
        it < transformed_plan.end(); ++ it, at ++) {

        const geometry_msgs::Pose& pose = it->pose;
        double g_x = pose.position.x;
        double g_y = pose.position.y;
        unsigned int map_x, map_y;
        // last point maybe out of costmap. or after breakpoint in visual studio.
        if (costmap.worldToMap(g_x, g_y, map_x, map_y)) {
            int cell_index = costmap.getIndex(map_x, map_y);
            ttmp_point& tmp_point = tmp_points_ptr[at];

            tmp_point.g_xy.x = g_x;
            tmp_point.g_xy.y = g_y;
            tmp_point.map_xy.x = map_x;
            tmp_point.map_xy.y = map_y;
            tmp_point.cost = raw_costmap_data[cell_index];
            tmp_point.key = posix_mku64(map_x, map_y);

            tmp_point.followup_not_NO_INFORMATIONs = 0;
            if (tmp_point.cost != costmap_2d::NO_INFORMATION) {
                if (last_NO_INFORMATION_at != nposm) {
                    tmp_points_ptr[last_NO_INFORMATION_at].followup_not_NO_INFORMATIONs ++;
                }
            } else {
                last_NO_INFORMATION_at = at;
            }

            x_diff = robot_pose.x - g_x;
            y_diff = robot_pose.y - g_y;
            tmp_point.sq_dist = x_diff * x_diff + y_diff * y_diff;

            posix_touch_i32(map_x, map_y, &min_x, &min_y, &max_x, &max_y);

        } else {
            break;
        }
    }
    const int valid_size = at;

    std::set<uint64_t> sorted_keys;

    int last_INSCRIBED_INFLATED_OBSTACLE_at = nposm;
    bool see_NO_INFORMATION = false;
    int plan_points_start_at = nposm;
    const int at_least_reserve_size = 8 * 2; // about 8 cells, think 2 points/cell.
    double first_dist_threshold = FIRST_DIST_THRESHOLD;
    double sq_first_dist_threshold = first_dist_threshold * first_dist_threshold;
    const int min_points = 3;
    SDL_DPoint last_transformed_plan_pose{float_nposm, float_nposm};

    for (at = 0; at < valid_size; at ++) {
        const ttmp_point& tmp_point = tmp_points_ptr[at];
        
        // int cell_index = costmap.getIndex(map_x, map_y);
        if (!see_NO_INFORMATION) {
            if (plan_points.empty()) {
                plan_points_start_at = at;
            }
            plan_points.push_back(tmp_point.map_xy);
            last_transformed_plan_pose = tmp_point.g_xy;

            if (tmp_point.cost == costmap_2d::INSCRIBED_INFLATED_OBSTACLE) {
                last_INSCRIBED_INFLATED_OBSTACLE_at = at;
            }

            if (sorted_keys.count(tmp_point.key) == 0) {
                sorted_keys.insert(tmp_point.key);
            }
        }
        
        if (tmp_point.cost == costmap_2d::NO_INFORMATION) {
            // when you encounter NO_INFORMATION, terminate
            if ((int)plan_points.size() >= min_points && (valid_size - at <= at_least_reserve_size || tmp_point.sq_dist > sq_first_dist_threshold)) {
                see_NO_INFORMATION = true;
            } else {
                // reset
                plan_points.clear();
                diff_points = 0;
                last_INSCRIBED_INFLATED_OBSTACLE_at = nposm;
                // now, see_NO_INFORMATION maybe is true.
                see_NO_INFORMATION = false;
                plan_points_start_at = nposm;

                sorted_keys.clear();
            }
        }
    }

    if (!plan_points.empty()) {
        VALIDATE(!is_float_nposm(last_transformed_plan_pose.x), null_str);
        VALIDATE(plan_points_start_at != nposm, null_str);
    }

    diff_points = sorted_keys.size();

    bool maybe_unreachable = false;
    if (see_NO_INFORMATION && last_INSCRIBED_INFLATED_OBSTACLE_at != nposm) {
        const int s = plan_points.size();
        const SDL_Point& cell = plan_points[s - 1];

        double x_diff = robot_pose.x - last_transformed_plan_pose.x;
        double y_diff = robot_pose.y - last_transformed_plan_pose.y;
        double sq_dist = x_diff * x_diff + y_diff * y_diff;
        double dist = sqrt(sq_dist);

        // plan_points.size() - 1 must be NO_INFORMATION
        double unreachable_dist_threshold = FIRST_DIST_THRESHOLD + 0.1;
        const int check_cells = 2;
        maybe_unreachable = dist < unreachable_dist_threshold && last_INSCRIBED_INFLATED_OBSTACLE_at - plan_points_start_at >= s - 1 - check_cells;            
        if (maybe_unreachable) {
            maybe_unreachable = costmap.thisCost8Cells(cell.x, cell.y, costmap_2d::NO_INFORMATION) >= 3;
        }
    }

    return maybe_unreachable;
}

SDL_Surface* costmap2d_file_2_SDL_Surface(const std::string& filename, costmap_2d::tcostmap_header& header)
{
    VALIDATE(!filename.empty(), null_str);
    costmap_2d::Costmap2D costmap;

    memset(&header, 0, sizeof(header));
    std::vector<geometry_msgs::PoseStamped> transformed_plan;
    std::vector<geometry_msgs::Point> footprint;
    bool valid = costmap.loadMap(filename, header, transformed_plan, footprint);
    if (!valid) {
        return nullptr;
    }

    if (game_config::os == os_windows) {
        std::vector<geometry_msgs::Point> footprint = getNarrowFootprint(ros::base_cfg.robot_width);

        std::vector<SDL_Point> obs_cells2;
        // tpose2d pose2d(header.robot_x, header.robot_y, header.robot_yaw);
        tpose2d pose2d(header.robot_x, header.robot_y, DEG2RAD(54.590));
        dwa_local_planner::get_footprint_obs_cells(costmap, footprint, pose2d, 0.0, 0.0, -1 * header.odom_2_map_pose.yaw, obs_cells2);


        SDL_Point robot_cell{29, 30};
        std::set<tpoint> obs_cells;

        obs_cells.insert(tpoint(24,25));
        obs_cells.insert(tpoint(25,24));
        obs_cells.insert(tpoint(25,25));
        obs_cells.insert(tpoint(33,25));

        int front_cell_dist;
        std::vector<SDL_Point> front_cells;
        int front_cell_size = dwa_local_planner::distinguishable_obs_cells(costmap, robot_cell, obs_cells, true, front_cell_dist, &front_cells);

        obs_cells.clear();

        obs_cells.insert(tpoint(23,35));
        obs_cells.insert(tpoint(23,36));
        obs_cells.insert(tpoint(24,36));
        obs_cells.insert(tpoint(25,35));
        obs_cells.insert(tpoint(25,36));
        obs_cells.insert(tpoint(26,35));
        obs_cells.insert(tpoint(26,36));
        obs_cells.insert(tpoint(27,35));
        obs_cells.insert(tpoint(27,36));
        obs_cells.insert(tpoint(28,35));
        obs_cells.insert(tpoint(28,36));
        obs_cells.insert(tpoint(29,35));
        obs_cells.insert(tpoint(29,36));
        obs_cells.insert(tpoint(30,36));

        int back_cell_dist;
        std::vector<SDL_Point> back_cells;
        int back_cell_size = dwa_local_planner::distinguishable_obs_cells(costmap, robot_cell, obs_cells, true, back_cell_dist, &back_cells);
        int ii = 0;
    }

    const uint8_t* raw_costmap_data = costmap.getCharMap();
    const tpose2d robot_pose(header.robot_x, header.robot_y, header.robot_yaw);

    int min_x = INT32_MAX;
    int min_y = INT32_MAX;
    int max_x = INT32_MIN;
    int max_y = INT32_MIN;

    std::vector<SDL_Point> plan_points;
    int diff_points;
    if (!transformed_plan.empty()) {
        transformed_plan_2_plan_points(robot_pose, transformed_plan, costmap, plan_points, diff_points, min_x, min_y, max_x, max_y);
    }

    int at = 0;
    const int points = plan_points.size();
    for (std::vector<geometry_msgs::PoseStamped>::const_iterator it = transformed_plan.begin(); 
        it < transformed_plan.end() && at < points; ++ it, at ++) {

        const geometry_msgs::Pose& pose = it->pose;
        double g_x = pose.position.x;
        double g_y = pose.position.y;

        calcuate_target_pose2d(tpose2d(g_x, g_y, tf2::getYaw(pose.orientation)), header.odom_2_map_pose, true);

    }
    if (!plan_points.empty()) {
        VALIDATE(plan_points.back().x == header.dst.x && plan_points.back().y == header.dst.y, null_str);
    } else {
        VALIDATE(header.dst.x == nposm && header.dst.y == nposm, null_str);
    }

    const int size_x = costmap.getSizeInCellsX();
    const int size_y = costmap.getSizeInCellsY();

    double inscribed_radius = header.inscribed_radius;
    int cell_inscribed_radius = costmap.cellDistance(inscribed_radius);
    const pathfind::shortest_path_calculator calc(costmap, cell_inscribed_radius);

    std::vector<geometry_msgs::PoseStamped> transformed_plan2;
    if (!plan_points.empty()) {
        SDL_Point dst = plan_points.back();
    
    
        SDL_Rect exclusion_rect{header.exclusion_x, header.exclusion_y, header.exclusion_w, header.exclusion_h};
        pathfind::plain_route route = pathfind::a_star_search(pathfind::search_global_plan, header.src, 
            dst, 0.0, calc, costmap, cell_inscribed_radius, exclusion_rect, true);
        if (route.steps.empty()) {
            // adjust path fail.
            int ii = 0;
            // return;
        }

        geometry_msgs::PoseStamped pose;
        ros::Time plan_time(0, 0);
        for (std::vector<SDL_Rect>::const_iterator it = route.steps.begin(); it != route.steps.end(); ++ it) {
            const SDL_Rect& p = *it;
            // convert the plan to world coordinates
            double world_x, world_y;
            costmap.mapToWorld(p.x, p.y, world_x, world_y);

            // geometry_msgs::PoseStamped& pose = goal_copy;
            pose.header.stamp = plan_time;
            pose.header.frame_id = "odom";
            pose.pose.position.x = world_x;
            pose.pose.position.y = world_y;
            pose.pose.position.z = 0.0;
            pose.pose.orientation.x = 0.0;
            pose.pose.orientation.y = 0.0;
            pose.pose.orientation.z = 0.0;
            pose.pose.orientation.w = 1.0;

            transformed_plan2.push_back(pose);
        }
    }

    SDL_Surface* surf = costmap2d_2_SDL_Surface(costmap, null_str, header.robot_x, header.robot_y, header.robot_yaw, 
        transformed_plan2, footprint);
    return surf;
}

}

#include <dynamic_reconfigure/server.h>

namespace dynamic_reconfigure
{

static std::set<const ros::NodeHandle*> servers;

void register_server(const ros::NodeHandle& nh)
{
    VALIDATE(servers.count(&nh) == 0, null_str);
    servers.insert(&nh);
}

void deregister_server(const ros::NodeHandle& nh)
{
    std::set<const ros::NodeHandle*>::iterator it = servers.find(&nh);
    VALIDATE(it != servers.end(), null_str);
    servers.erase(it);
}

}

namespace ros {

const std::set<const ros::NodeHandle*>& dynamic_reconfigure_servers()
{
    return dynamic_reconfigure::servers;
}

}



//
// test function that must use ros, and must run in main thread.
//
#include <ros/node_handle.h>
#include <urdf_model/model.h>
#include "kdl_parser/kdl_parser.hpp"
#include <trac_ik/trac_ik.hpp>

#include <kdl/tree.hpp>
#include <kdl/chainiksolverpos_nr_jl.hpp>
#include <kdl/chain.hpp>
#include <kdl/chainfksolver.hpp>
#include <kdl/chainfksolverpos_recursive.hpp>
#include <kdl/frames_io.hpp>

#include <tf2_kdl/tf2_kdl.h>

#include <moveit/rdf_loader/rdf_loader.h>



namespace ros {

bool load_urdf(ros::NodeHandle& nh, const std::string& urdf_file_path)
{
    std::string urdf_string_;
	const std::string xacro_args;
	bool ret = rdf_loader::RDFLoader::loadXmlFileToString(urdf_string_, urdf_file_path, { xacro_args });
    if (ret) {
        nh.setParam("/robot_description", urdf_string_);
    }

    return ret;
}

bool load_srdf(ros::NodeHandle& nh, const std::string& urdf_file_path)
{
    std::string urdf_string_;
	const std::string xacro_args;
	bool ret = rdf_loader::RDFLoader::loadXmlFileToString(urdf_string_, urdf_file_path, { xacro_args });
    if (ret) {
        nh.setParam("/robot_description_semantic", urdf_string_);
    }

    return ret;
}

void isometry3d_from_xyz_rpy(const Eigen::Vector3d& xyz, const Eigen::Vector3d& rpy, Eigen::Isometry3d& result)
{
	std::stringstream ss;

	Eigen::AngleAxisd rollAngle(Eigen::AngleAxisd(rpy(0), Eigen::Vector3d::UnitX()));
	Eigen::AngleAxisd pitchAngle(Eigen::AngleAxisd(rpy(1), Eigen::Vector3d::UnitY()));
	Eigen::AngleAxisd yawAngle(Eigen::AngleAxisd(rpy(2), Eigen::Vector3d::UnitZ()));

	result = Eigen::Isometry3d::Identity();
	result.rotate(yawAngle * pitchAngle * rollAngle);
	result.pretranslate(xyz);
}

void rpy_from_isometry3d(const Eigen::Isometry3d& src, double* roll, double* pitch, double* yaw)
{
    Eigen::Vector3d eulerAngle = src.linear().eulerAngles(2, 1, 0);
    if (roll != nullptr) {
        *roll = eulerAngle(2);
    }
    if (pitch != nullptr) {
        *pitch = eulerAngle(1);
    }
    if (yaw != nullptr) {
        *yaw = eulerAngle(0);
    }
    // M.GetRPY(roll,pitch,yaw);
}

Eigen::Isometry3d p_M_2_Isometry3d(const Eigen::Vector3d& p, const Eigen::Matrix3d& M)
{
    // ----eigen::matrix4d----
    Eigen::Matrix4d T;
    T.setIdentity();
    T.block<3, 3>(0, 0) = M;
    T.topRightCorner(3, 1) = p;

    return Eigen::Isometry3d(T);
}

// @duble xyz[3]
// @duble rpy[3]
// @duble axis_tag_xyz[3]
// @angle: if this joint is fixed, angle is 0.
Eigen::Isometry3d chain_joint_pose_KDL(const double* xyz, const double* rpy, const double* axis_tag_xyz, double angle)
{
	std::stringstream ss;

	Eigen::AngleAxisd rollAngle(Eigen::AngleAxisd(rpy[0], Eigen::Vector3d::UnitX()));
	Eigen::AngleAxisd pitchAngle(Eigen::AngleAxisd(rpy[1], Eigen::Vector3d::UnitY()));
	Eigen::AngleAxisd yawAngle(Eigen::AngleAxisd(rpy[2], Eigen::Vector3d::UnitZ()));

	Eigen::Isometry3d F_parent_jnt = Eigen::Isometry3d::Identity();
	F_parent_jnt.rotate(yawAngle * pitchAngle * rollAngle);

	Eigen::Vector3d _axis = F_parent_jnt.rotation() * Eigen::Vector3d(axis_tag_xyz[0], axis_tag_xyz[1], axis_tag_xyz[2]);
	Eigen::Vector3d axis = _axis / _axis.norm();

	if (false) {
		ss << "\n_axis = " << _axis.transpose();
	}

	Eigen::Isometry3d joint_pose = Eigen::Isometry3d::Identity();
	joint_pose.rotate(Eigen::AngleAxisd(angle, axis));

	if (false) {
		ss << "\njoint_pose_M = \n";
		ss << joint_pose.rotation().matrix();
	}

	joint_pose.pretranslate(Eigen::Vector3d(xyz[0], xyz[1], xyz[2]));

	Eigen::Isometry3d f_tip = Eigen::Isometry3d::Identity();
	f_tip.rotate(F_parent_jnt.rotation());


	Eigen::Isometry3d result = joint_pose * f_tip;
	ss << "\nresult's matrix = \n";
	ss << result.matrix();
	if (false) {
		ss << result.rotation().eulerAngles(2, 1, 0).transpose();
	}


	// SDL_Log("%s", ss.str().c_str());
	return result;
}

Eigen::Isometry3d chain_joint_pose(const double* xyz, const double* rpy, const double* axis_tag_xyz, double angle)
{
    bool use_chain_joint_pose_KDL = false;
    if (use_chain_joint_pose_KDL) {
        return chain_joint_pose_KDL(xyz, rpy, axis_tag_xyz, angle);
    }

/*
	Eigen::AngleAxisd rollAngle(Eigen::AngleAxisd(rpy[0], Eigen::Vector3d::UnitX()));
	Eigen::AngleAxisd pitchAngle(Eigen::AngleAxisd(rpy[1], Eigen::Vector3d::UnitY()));
	Eigen::AngleAxisd yawAngle(Eigen::AngleAxisd(rpy[2], Eigen::Vector3d::UnitZ()));

    Eigen::Matrix3d M(yawAngle * pitchAngle * rollAngle);
*/
    tf2::Quaternion q;
    q.setRPY(rpy[0], rpy[1], rpy[2]);
    Eigen::Quaterniond eigen_q(q.w(), q.x(), q.y(), q.z());
    Eigen::Matrix3d M = eigen_q.matrix();


	Eigen::Vector3d _axis = M * Eigen::Vector3d(axis_tag_xyz[0], axis_tag_xyz[1], axis_tag_xyz[2]);
	Eigen::Vector3d axis = _axis / _axis.norm();

	Eigen::Isometry3d joint_pose = p_M_2_Isometry3d(Eigen::Vector3d(xyz[0], xyz[1], xyz[2]), Eigen::AngleAxisd(angle, axis).matrix());
	Eigen::Isometry3d f_tip = p_M_2_Isometry3d(Eigen::Vector3d(0, 0, 0), M);

	return joint_pose * f_tip;

}

// @angles. if joint is RotAxis, dof = 1. so only angle value. angles.size() is count of variablable joint.
Eigen::Isometry3d forward_kinematic(const std::vector<tfk_joint>& joints, const std::vector<double>& angles)
{
	VALIDATE(joints.size() >= angles.size(), null_str);
	Eigen::Isometry3d result = Eigen::Isometry3d::Identity();
	int RotAxis_at = 0;
	for (std::vector<tfk_joint>::const_iterator it = joints.begin(); it != joints.end(); ++ it) {
		const tfk_joint& joint = *it;
		// if joint is Fixed, angle set to 0.
		double angle = joint.fixed? 0: angles[RotAxis_at ++];
		result = result * chain_joint_pose(joint.xyz, joint.rpy, joint.axis_tag_xyz, angle);
	}

	return result;
}

class tkdl_tf_calculator: public ttf_calculator
{
    // tkdl_tf_calculator is 'copy' from 'class RobotStatePublisher'
public:
    class SegmentPair
    {
    public:
      SegmentPair(const KDL::Segment& p_segment, const std::string& p_root, const std::string& p_tip):
        segment(p_segment), root(p_root), tip(p_tip){}

      KDL::Segment segment;
      std::string root, tip;
    };

    tkdl_tf_calculator(const urdf::ModelInterface& model);

    geometry_msgs::Pose calculate_tf(const std::vector<tfk_joint>& joints, const std::vector<double>& angles);

private:
    void addChildren(const urdf::ModelInterface& model, const KDL::SegmentMap::const_iterator segment);

private:
    std::map<std::string, SegmentPair> segments_;
    std::map<std::string, SegmentPair> segments_fixed_;
    std::unique_ptr<KDL::Tree> tree;
};

tkdl_tf_calculator::tkdl_tf_calculator(const urdf::ModelInterface& model)
{
    // bool ret = model_.initParam("robot_description");
	// VALIDATE(ret, null_str);

    tree.reset(new KDL::Tree);
	bool ret = kdl_parser::treeFromUrdfModel(model, *tree.get());
	VALIDATE(ret, null_str);

    // walk the tree and add segments to segments_
    addChildren(model, tree->getRootSegment());
}

// add children to correct maps
void tkdl_tf_calculator::addChildren(const urdf::ModelInterface& model, const KDL::SegmentMap::const_iterator segment)
{
  const std::string& root = GetTreeElementSegment(segment->second).getName();

  const std::vector<KDL::SegmentMap::const_iterator>& children = GetTreeElementChildren(segment->second);
  for (size_t i = 0; i < children.size(); ++i) {
    const KDL::Segment& child = GetTreeElementSegment(children[i]->second);
    SegmentPair s(GetTreeElementSegment(children[i]->second), root, child.getName());
    if (child.getJoint().getType() == KDL::Joint::None) {
      if (model.getJoint(child.getJoint().getName()) && model.getJoint(child.getJoint().getName())->type == urdf::Joint::FLOATING) {
        ROS_INFO("Floating joint. Not adding segment from %s to %s. This TF can not be published based on joint_states info", root.c_str(), child.getName().c_str());
      }
      else {
        segments_fixed_.insert(make_pair(child.getJoint().getName(), s));
        ROS_DEBUG("Adding fixed segment from %s to %s", root.c_str(), child.getName().c_str());
      }
    }
    else {
      segments_.insert(make_pair(child.getJoint().getName(), s));
      ROS_DEBUG("Adding moving segment from %s to %s", root.c_str(), child.getName().c_str());
    }
    addChildren(model, children[i]);
  }
}

geometry_msgs::Pose tkdl_tf_calculator::calculate_tf(const std::vector<tfk_joint>& joints, const std::vector<double>& angles)
{
    geometry_msgs::Pose t_in;
    geometry_msgs::Pose result;
    tf2::toMsg(tf2::Transform::getIdentity(), result);

    bool use_tf2_doTransform = false;

    if (!use_tf2_doTransform) {
        // reference to:
        //   {orocos_kdl/src/chainfksolverpos_recursive.cpp}ChainFkSolverPos_recursive::JntToCart(const JntArray& q_in, Frame& p_out, int seg_nr)

        KDL::Frame p_out = KDL::Frame::Identity();
        int RotAxis_at = 0;

        int at = 0;
        for (std::vector<tfk_joint>::const_iterator it = joints.begin(); it != joints.end(); ++ it, at ++) {
            const tfk_joint& joint = *it;

            if (segments_.count(joint.name) != 0) {
                std::map<std::string, SegmentPair>::const_iterator seg = segments_.find(joint.name);
                double angle = angles[RotAxis_at ++];
                p_out = p_out * seg->second.segment.pose(angle);

                // SDL_Log("[%i/%i]p_out: (%.6f, %.6f, %.6f)", at, (int)joints.size(), p_out.p.data[0], p_out.p.data[1], p_out.p.data[2]);

            } else {
                std::map<std::string, SegmentPair>::const_iterator seg = segments_fixed_.find(joint.name);
                VALIDATE(seg != segments_fixed_.end(), null_str);
                p_out = p_out * seg->second.segment.pose(0.0);
            }
        }

        VALIDATE(RotAxis_at == angles.size(), null_str);

        geometry_msgs::TransformStamped tf_transform = tf2::kdlToTransform(p_out);

        result.position.x = tf_transform.transform.translation.x;
        result.position.y = tf_transform.transform.translation.y;
        result.position.z = tf_transform.transform.translation.z;
        result.orientation = tf_transform.transform.rotation;

    } else {
        geometry_msgs::TransformStamped tf_transform;
        int RotAxis_at = angles.size() - 1;
        for (std::vector<tfk_joint>::const_reverse_iterator rit = joints.rbegin(); rit != joints.rend(); ++ rit) {
            const tfk_joint& joint = *rit;
            t_in = result;

            if (segments_.count(joint.name) != 0) {
                std::map<std::string, SegmentPair>::const_iterator seg = segments_.find(joint.name);
                double angle = angles[RotAxis_at --];
                tf_transform = tf2::kdlToTransform(seg->second.segment.pose(angle));
            } else {
                std::map<std::string, SegmentPair>::const_iterator seg = segments_fixed_.find(joint.name);
                VALIDATE(seg != segments_fixed_.end(), null_str);
                tf_transform = tf2::kdlToTransform(seg->second.segment.pose(0));
            }
            tf2::doTransform(t_in, result, tf_transform);

        }
        VALIDATE(RotAxis_at == -1, null_str);
    }

    return result; 
}

/*
geometry_msgs::Pose tkdl_tf_calculator::calculate_tf(const std::vector<tfk_joint>& joints, const std::vector<double>& angles)
{
    geometry_msgs::Pose t_in;
    geometry_msgs::Pose result;
    tf2::toMsg(tf2::Transform::getIdentity(), result);

    bool use_tf2_doTransform = true;

    if (!use_tf2_doTransform) {
        // reference to:
        //   {orocos_kdl/src/chainfksolverpos_recursive.cpp}ChainFkSolverPos_recursive::JntToCart(const JntArray& q_in, Frame& p_out, int seg_nr)

        KDL::Frame p_out = KDL::Frame::Identity();
        int RotAxis_at = 0;

        for (std::vector<tfk_joint>::const_iterator it = joints.begin(); it != joints.end(); ++ it) {
            const tfk_joint& joint = *it;

            if (segments_.count(joint.name) != 0) {
                std::map<std::string, SegmentPair>::const_iterator seg = segments_.find(joint.name);
                double angle = angles[RotAxis_at ++];
                p_out = p_out * seg->second.segment.pose(angle);

            } else {
                std::map<std::string, SegmentPair>::const_iterator seg = segments_fixed_.find(joint.name);
                VALIDATE(seg != segments_fixed_.end(), null_str);
                p_out = p_out * seg->second.segment.pose(0.0);
            }
        }

        VALIDATE(RotAxis_at == angles.size(), null_str);

        geometry_msgs::TransformStamped tf_transform = tf2::kdlToTransform(p_out);

        result.position.x = tf_transform.transform.translation.x;
        result.position.y = tf_transform.transform.translation.y;
        result.position.z = tf_transform.transform.translation.z;
        result.orientation = tf_transform.transform.rotation;

    } else {
        geometry_msgs::TransformStamped tf_transform;
        int RotAxis_at = 0;
        for (std::vector<tfk_joint>::const_iterator it = joints.begin(); it != joints.end(); ++ it) {
            const tfk_joint& joint = *it;
            t_in = result;

            if (segments_.count(joint.name) != 0) {
                std::map<std::string, SegmentPair>::const_iterator seg = segments_.find(joint.name);
                double angle = angles[RotAxis_at ++];
                tf_transform = tf2::kdlToTransform(seg->second.segment.pose(angle));
            } else {
                std::map<std::string, SegmentPair>::const_iterator seg = segments_fixed_.find(joint.name);
                VALIDATE(seg != segments_fixed_.end(), null_str);
                tf_transform = tf2::kdlToTransform(seg->second.segment.pose(0));
            }
            tf2::doTransform(t_in, result, tf_transform);

        }
        VALIDATE(RotAxis_at == (int)angles.size(), null_str);
    }

    return result; 
}
*/
ttf_calculator* create_kdl_tf_calculator(const urdf::ModelInterface& model)
{
    return new tkdl_tf_calculator(model);
}

ttf_calculator* create_fk_tf_calculator(const urdf::ModelInterface& model)
{
    VALIDATE(false, "Not impletement");
    return nullptr;
}

bool getKDLChain(const urdf::ModelInterface& model, const std::string& root_name, const std::string& tip_name,
                 KDL::Chain& kdl_chain)
{
  // create robot chain from root to tip
  KDL::Tree tree;
  if (!kdl_parser::treeFromUrdfModel(model, tree))
  {
    // ROS_ERROR("Could not initialize tree object");
    return false;
  }
  if (!tree.getChain(root_name, tip_name, kdl_chain))
  {
    // ROS_ERROR_STREAM("Could not initialize chain object for base " << root_name << " tip " << tip_name);
    return false;
  }
  return true;
}

turdf::turdf(const std::string& urdf_file, const std::string& chain_start, const std::string& chain_end, 
    int _nj, int _q_type, const double* _q_custom)
    : urdf_file(urdf_file)
    , chain_start(chain_start)
    , chain_end(chain_end)
    , nj(_nj)
    , valid_jss(0)
    , valid_poses(0)
    , q_type(_q_type)
{
    VALIDATE(nj > 0 && nj <= MAX_NROFJOINTS, null_str);
    memset(jss, 0, sizeof(jss));
    memset(poses, 0, sizeof(poses));

    VALIDATE(q_type >= 0 && q_type < q_count, null_str);
    memset(custom_q, 0, sizeof(custom_q));
    if (q_type == q_custom) {
        memcpy(custom_q, _q_custom, sizeof(double) * nj);
    }
}

void turdf::push_js(const double* _js)
{
    memcpy(jss[valid_jss], _js, sizeof(double) * nj);
    valid_jss ++;
}

void turdf::push_pose(const double* pose)
{
    memcpy(poses[valid_poses], pose, sizeof(double) * 6);
    valid_poses ++;
}

void ikfast_test(ros::NodeHandle& nh, const turdf& urdf)
{
    std::stringstream ss;
    std::string chain_start, chain_end, urdf_param;
    double timeout;
    const double error = 1e-5;
/*
    double js[3] = {0.5, 0.6, -0.7};
    double q_custom[3] = {0.1, 0.1, -0.1};
    turdf urdf("C:/ddksample/moveit_ws/src/tank_arm/robots/tank_arm.urdf", "arm_Link", "grasping_frame", 
        js, sizeof(js) / sizeof(js[0]), turdf::q_custom, q_custom);
*/
 
    // double js[3] = {0, 1.5708, 0};
    // turdf urdf("C:/ddksample/moveit2_ws/src/trac_ik_examples/robots/robot.urdf", "link0", "link3", js, sizeof(js) / sizeof(js[0]));
/*
    double js[7] = {0.0180582, -0.537152, -0.0168831, -2.60095, 0.0297504, 2.03967, 0.753258};
    turdf urdf("C:/ddksample/moveit_ws/src/franka_description/robots/panda_arm.urdf", "panda_link0", "panda_link8",
        js, sizeof(js) / sizeof(js[0]), turdf::q_middle);
*/

    if (!load_urdf(nh, urdf.urdf_file)) {
        return;
    }

    nh.param("chain_start", chain_start, urdf.chain_start);
    nh.param("chain_end", chain_end, urdf.chain_end);

    if (chain_start == "" || chain_end == "") {
        // ROS_FATAL("Missing chain info in launch file");
        return;
    }

	// nh.param("timeout", timeout, 0.005);
    nh.param("timeout", timeout, 1.0);
    nh.param("urdf_param", urdf_param, std::string("/robot_description"));


    TRAC_IK::TRAC_IK ik_solver(chain_start, chain_end, urdf_param, timeout, error, TRAC_IK::Speed);  

    KDL::Chain chain;
    bool valid = ik_solver.getKDLChain(chain);

    if (!valid) {
        ROS_ERROR("There was no valid KDL chain found");
        return;
    }

    KDL::JntArray ll, ul; //lower joint limits, upper joint limits
    valid = ik_solver.getKDLLimits(ll, ul);

    if (!valid) {
        ROS_INFO("There were no valid KDL joint limits found");
    }

    SDL_Log("ll: (%.5f, %.5f, %.5f), ul: (%.5f, %.5f, %.5f)", ll(0), ll(1), ll(2), ul(0), ul(1), ul(2));

    // Set up KDL IK
    KDL::ChainFkSolverPos_recursive fk_solver(chain); // Forward kin. solver based on kinematic chain

    unsigned int nj = chain.getNrOfJoints();
    ROS_INFO ("Using %d joints", nj);
    // Assign some values to the joint positions
    VALIDATE(nj == urdf.nj, null_str);

    int valid_poses = 0;
    double poses[MAX_ONEIK_POSES][6];
    memset(poses, 0, sizeof(poses));
    if (urdf.valid_jss == 0 && urdf.valid_poses > 0) {
        memcpy(poses, urdf.poses, sizeof(double) * 6 * urdf.valid_poses);
        valid_poses = urdf.valid_poses;
    }
    for (int js_at = 0; js_at < urdf.valid_jss; js_at ++) {
        // Create joint array
        KDL::JntArray jointpositions = KDL::JntArray(nj);
        ss.str("");
        for (int at = 0; at < urdf.nj; at ++) {
            jointpositions(at) = urdf.jss[js_at][at];
            ss << jointpositions(at) << " ";
        }
        SDL_Log("---[%i/%i]jointpositions: %s---", js_at, urdf.valid_jss, ss.str().c_str());

        // Create the frame that will contain the results
        KDL::Frame cartpos;    

        // Calculate forward position kinematics
        int kinematics_status;
        kinematics_status = fk_solver.JntToCart(jointpositions, cartpos);

        KDL::Vector p = cartpos.p;   // Origin of the Frame
        KDL::Rotation M = cartpos.M; // Orientation of the Frame

        if (kinematics_status>=0){
            double roll, pitch, yaw;
            M.GetRPY(roll, pitch, yaw);
            double pose[6] = {p.data[0], p.data[1], p.data[2], roll, pitch, yaw};
            memcpy(poses[valid_poses], pose, sizeof(double) * 6);
            valid_poses ++;
            // urdf.push_
            // SDL_Log("%s","KDL FK Succes");
/*        
            ss.str("");
            ss << "Origin: " << p(0) << "," << p(1) << "," << p(2) << "\n";
            ss << "RPY: " << roll << "," << pitch << "," << yaw;
            SDL_Log("%s", ss.str().c_str());

            char buf[320];
            SDL_snprintf(buf, sizeof(buf), "%.15f  %.15f  %.15f  %.15f\n%.15f  %.15f  %.15f  %.15f\n%.15f  %.15f  %.15f  %.15f",
                M.data[0], M.data[1], M.data[2], p.data[0],
                M.data[3], M.data[4], M.data[5], p.data[1],
                M.data[6], M.data[7], M.data[8], p.data[2]);
            SDL_Log("Frame:\n%s", buf);
*/        
        } else{
            SDL_Log("%s","Error: could not calculate forward kinematics :(");
        }
    }

    tf2::Quaternion q;
    for (int pose_at = 0; pose_at < valid_poses; pose_at ++) {
        const double* pose = poses[pose_at];
        q.setRPY(pose[3], pose[4], pose[5]);
        Eigen::Quaterniond eigen_q(q.w(), q.x(), q.y(), q.z());
        Eigen::Matrix3d rotation = eigen_q.matrix();
        const double* r_data = rotation.data();

        KDL::Frame cartpos(KDL::Rotation(r_data[0], r_data[3], r_data[6], 
            r_data[1], r_data[4], r_data[7], 
            r_data[2], r_data[5], r_data[8]),
            KDL::Vector(pose[0], pose[1], pose[2]));

        KDL::JntArray joint_seed(nj);
        if (urdf.q_type == turdf::q_middle) {
            for (int j = 0; j < joint_seed.data.size(); j++) {
                joint_seed(j) = (ll(j) + ul(j)) / 2.0;
            }
        } else if (urdf.q_type == turdf::q_custom) {
            for (int j = 0; j < joint_seed.data.size(); j++) {
                joint_seed(j) = urdf.custom_q[j];
            }
        } else {
            // urdf.q_type == turdf::q_zero
            KDL::SetToZero(joint_seed);
        }

        KDL::JntArray result(joint_seed);
   
        int rc=ik_solver.CartToJnt(joint_seed, cartpos, result);
        if (rc < 0) {
            SDL_Log("[%i/%i]{p(%.6f, %.6f, %.6f) M(%.6f, %.6f, %.6f)} TRAC IK=> fail",
                pose_at, valid_poses, pose[0], pose[1], pose[2], pose[3], pose[4], pose[5]);
        } else {
            ss.str("");
            for (unsigned int i = 0; i < nj; i++) {
                if (i > 0) {
                    ss << ", ";
                }
                ss << result(i);
            }
            SDL_Log("[%i/%i]{p(%.6f, %.6f, %.6f) M(%.6f, %.6f, %.6f)} TRAC IK=> (%s)", 
                pose_at, valid_poses, pose[0], pose[1], pose[2], pose[3], pose[4], pose[5], ss.str().c_str());
        }
    }
}

}
/*
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

#include <urdf_parser/urdf_parser.h>
#include <moveit/robot_state/robot_state.h>
// #include <moveit/collision_detection/collision_env.h>
#include <moveit/collision_detection_fcl/collision_env_fcl.h>

#include <geometric_shapes/shape_operations.h>

namespace ros {
const std::string LOGNAME = "robot_model_builder";

urdf::ModelInterfaceSharedPtr loadModelInterface()
{
  std::string urdf_path;
  // urdf_path = "C:/ddksample/moveit_ws/src/tank_arm/robots/tank_arm.urdf";
  urdf_path = "C:/ddksample/moveit_ws/src/franka_description/robots/panda_arm.urdf";

  urdf::ModelInterfaceSharedPtr urdf_model = urdf::parseURDFFile(urdf_path);
  if (urdf_model == nullptr)
  {
    ROS_ERROR_NAMED(LOGNAME, "Cannot find URDF: %s. Make sure robot description is installed", urdf_path.c_str());
  }
  return urdf_model;
}

srdf::ModelSharedPtr loadSRDFModel()
{
  urdf::ModelInterfaceSharedPtr urdf_model = loadModelInterface();
  srdf::ModelSharedPtr srdf_model(new srdf::Model());
  std::string srdf_path;
  // srdf_path = "C:/ddksample/moveit_ws/src/moveit_arm/config/tank_arm.srdf";
  srdf_path = "C:/ddksample/moveit_ws/src/franka_description12/config/panda.srdf";
  
  srdf_model->initFile(*urdf_model, srdf_path);
  return srdf_model;
}

}
*/

trosverbose::trosverbose(const std::string& scene, int& times, double& total_cost)
    : scene(scene)
    , times(times)
    , total_cost(total_cost)
    , enable_logout(false)
    , start(ros::Time::now().toSec() * 1000)
{}

trosverbose::~trosverbose()
{
    double cost = ros::Time::now().toSec() * 1000 - start;

    times ++;
    total_cost += cost;
    double avg = total_cost / times;
    if (enable_logout && ((times % 10) == 0)) {
        ROS_INFO("T%.3f %s#%i, avg cost: %.3f", start, scene.c_str(), times, avg);
    }
}

double calculate_yaw_internal(double x, double y, double z, double w)
{
    tf2::Matrix3x3 mat(tf2::Quaternion(x, y, z, w));
    double yaw, pitch, roll;
    mat.getEulerYPR(yaw, pitch, roll);
    return RAD2DEG(yaw);
}

std::string JointState_to_string(const sensor_msgs::JointState& msg)
{
    std::stringstream ss;

    int count = (int)msg.name.size();
    VALIDATE(count == (int)msg.position.size(), null_str);
    int at = 0;
    for (std::vector<std::string>::const_iterator it = msg.name.begin(); it != msg.name.end(); ++ it, at ++) {
        const std::string& name = *it;
        if (at != 0) {
            ss << ", ";
        }
        ss << name << ":" << msg.position[at];
    }

    return ss.str();
}

std::string PoseStamped_DebugString(const geometry_msgs::PoseStamped& pose)
{
    char buf[256];
    SDL_snprintf(buf, sizeof(buf), "{ t: [%.5f, %.5f, %.5f], q: [%.5f] }",
        pose.pose.position.x, pose.pose.position.y, pose.pose.position.z,
        calculate_yaw_internal(pose.pose.orientation.x, pose.pose.orientation.y, 
        pose.pose.orientation.z, pose.pose.orientation.w));
    return buf;
}

std::string Vector3f_DebugString(const Eigen::Vector3f& v3)
{
    char buf[256];
    float v0 = v3[0];
    float v1 = v3[1];
    float v2 = v3[2];
    SDL_snprintf(buf, sizeof(buf), "{(%.5f, %.5f, %.5f), R: %.5f theta: %.5f}",
        v0, v1, v2, sqrt(v0 * v0 + v1 * v1), RAD2DEG(atan2(v1, v0)));
    return buf;
}

#include <ompl/geometric/PathGeometric.h>

extern std::string get_state_str(const ompl::base::State* state);
std::string PathGeometric_DebugString(const ompl::geometric::PathGeometric& path)
{
    std::stringstream res;
    char buf[128];
    const int count = path.getStateCount();
    for (int at = 0; at < count; at ++) {
        if (at > 0) {
            res << ",";
        }
        const ompl::base::State* state = path.getState(at);
        SDL_snprintf(buf, sizeof(buf), "[%i/%i]%s", at, count, get_state_str(state).c_str()); 
        res << buf;
    }
    return res.str();
}


std::string vector_double_DebugString(const std::vector<double>& values)
{
    std::stringstream res;
    res << "(";
    for (std::vector<double>::const_iterator it = values.begin(); it != values.end(); ++ it) {
        const double& value = *it;
        if (it != values.begin()) {
            res << ", ";
        }
        res << value;
    }
    res << ")";
    return res.str();
}