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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the driver.	If not, see <http://www.gnu.org/licenses/>.
 */

#include <ros/ros.h>
#include <lslidar_driver/lslidar_driver.h>
#include <ros/callback_queue.h>

#include <SDL_log.h>
// volatile sig_atomic_t flag = 1;

// static void my_handler(int sig)
// {
//	flag = 0;
// }


int lslidar_driver__lslidar_driver_node(bool& exit, const std::string& serial_path, int baudrate, const std::string& lidar_name)
{
	int argc = 0;
	ros::init(argc, nullptr, "lslidar_driver_node");
	ros::CallbackQueue cbqueue;

	// ros::init(argc, argv, "lslidar_driver_node");
	ros::NodeHandle node;
	node.setCallbackQueue(&cbqueue);

	ros::NodeHandle private_nh("~");
	private_nh.setCallbackQueue(&cbqueue);

	// start the driver
	lslidar_driver::LslidarDriver driver(cbqueue, node, private_nh, serial_path, baudrate, lidar_name);
	if (!driver.valid()) {
		SDL_Log("{lslidar_driver__lslidar_driver_node}cannot open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
		return -1;
	}
	if (!driver.initialize()) {
		SDL_Log("{lslidar_driver__lslidar_driver_node}Cannot initialize lslidar driver...");
		return -1;
	}

	// loop until shut down or end of file
	// while(ros::ok() && driver.polling()) {
	//	ros::spinOnce();
	// }

	ros::WallDuration timeout(0.003f); // 3ms
	// while (!exit & ros::ok() && driver.polling()) {
	while (!exit & ros::ok()) {
		driver.pool_read();
		cbqueue.callAvailable(timeout);
		// cbqueue.callAvailable(ros::WallDuration());
	}

	return 0;
}
