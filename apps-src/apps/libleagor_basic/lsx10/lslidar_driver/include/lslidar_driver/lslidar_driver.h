/*
 * This file is part of lslidar driver.
 *
 * The driver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the driver.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LSLIDAR_DRIVER_H
#define LSLIDAR_DRIVER_H

// #include <unistd.h>
#include <stdio.h>
// #include <netinet/in.h>
#include <string>
// #include "input.h"

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
// #include <boost/thread.hpp>
#include <ros/ros.h>
#include <diagnostic_updater/diagnostic_updater.h>
#include <diagnostic_updater/publisher.h>
// #include "lsiosr.h"
#include <sensor_msgs/LaserScan.h>

// #include <pcl_conversions/pcl_conversions.h>
// #include <pcl_ros/point_cloud.h>
// #include <pcl/point_types.h>

// #include <lslidar_msgs/LslidarPacket.h>
#include <std_msgs/Byte.h>
#include <std_msgs/Int8.h>
#include "rose_peripheral.hpp"

namespace lslidar_driver {

typedef struct {
    double degree;
    double range;
    double intensity;
} ScanPoint;

// uint16_t PACKET_SIZE ;

class LslidarDriver: public tserial {
public:

    LslidarDriver(ros::CallbackQueueInterface& cbqueue, ros::NodeHandle& n, ros::NodeHandle& pn, const std::string& serial_path, int baud_rate, const std::string& _lidar_name);
    ~LslidarDriver();

    bool initialize();
    int getScan(std::vector<ScanPoint> &points, ros::Time &scan_time, float &scan_duration);
    void data_processing(const uint8_t* packet_bytes,int len);
    void data_processing_2(const unsigned char *packet_bytes,int len);
    void difop_processing(const unsigned char *packet_bytes);

private:
    void app_pool_read_bh() override;

    bool createRosIO();
    void pubScanThread();
    void lidar_difop();
    void lidar_order(const std_msgs::Int8 msg);
    void did_read_one_packet(const uint8_t* packet_bytes, int len);
    void initParam();
    uint8_t N10_CalCRC8(unsigned char * p, int len);
    void ctrl_motor(bool start);

private:
    const uint8_t prefix0_;
	const uint8_t prefix1_;
    SDL_threadID tid_;
    // Ethernet relate variables
    int UDP_PORT_NUMBER;
	bool is_start;
    // ROS related variables
    // LSIOSR * serial_;
    // std::string serial_port_;
    ros::NodeHandle nh;
    ros::NodeHandle pnh;
    std::string  interface_selection;
    // boost::shared_ptr<Input> msop_input_;
    ros::Publisher packet_pub;
	ros::Publisher pointcloud_pub;

	ros::Subscriber difop_switch;
    // Diagnostics updater
    diagnostic_updater::Updater diagnostics;
    boost::shared_ptr<diagnostic_updater::TopicDiagnostic> diag_topic;
    double diag_min_freq;
    double diag_max_freq;

    std::vector<ScanPoint> scan_points_;
    std::vector<ScanPoint> scan_points_bak_;
    std::string frame_id;
    std::string lidar_name;
    std::string scan_topic;
    std::string dump_file;
    std::string pointcloud_topic;
    std::string in_file_name;
    double min_range;
    double max_range;
    double angle_disable_min;
    double angle_disable_max;
    double angle_able_min;
    double angle_able_max;
    double degree_compensation = 0.0;
    bool use_gps_ts;
    bool high_reflection;
    bool n10p_double_echo;
    bool compensation;
    bool first_compensation = true;
    bool pubScan;
    // bool pubPointCloud2;
    int count_num;
    int package_points;
    int data_bits_start;
    int degree_bits_start;
    int end_degree_bits_start;
    int rpm_bits_start;
	int baud_rate_;
    int points_size_;
    ros::Time pre_time_;
    ros::Time time_;
    ros::Publisher pub_;
    tm pTime;    
    uint64_t sub_second;
    uint64_t get_gps_stamp(tm t);
    uint64_t sweep_end_time_gps;
    uint64_t sweep_end_time_hardware;
    int idx = 0;
    int link_time = 0;
    double last_degree = 0.0;	

    int truncated_mode_;
    double min_distance,max_distance;
    std::vector<int> disable_angle_min_range, disable_angle_max_range, disable_angle_range_default;

    int pub_scan_times_;
    const int verbose_interval_ms_;
    uint32_t app_pool_read_bh_last_verbose_ms_;
    uint32_t did_read_one_packet_last_verbose_ms_;
    uint32_t data_processing_last_verbose_ms_;
};

} // namespace lslidar_driver

#endif // _LSLIDAR__DRIVER_H_
