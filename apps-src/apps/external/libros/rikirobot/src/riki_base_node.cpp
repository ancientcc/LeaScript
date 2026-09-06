#include <ros/ros.h>
#include "../include/riki_base.h"

// int main(int argc, char** argv )
int rikirobot__riki_base_node(bool& exit)
{
    int argc = 0;
    ros::init(argc, nullptr, "riki_base_node");
    ros::CallbackQueue cbqueue;
    RikiBase riki(cbqueue);

    // ros::spin();
    ros::WallDuration timeout(0.1f);
    while (!exit & ros::ok()) {
	    cbqueue.callAvailable(timeout);
    }
    return 0;
}
