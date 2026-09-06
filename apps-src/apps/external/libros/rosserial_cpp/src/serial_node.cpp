#include <ros/ros.h>
#include <ros/callback_queue.h>
#include <ros/serialization.h>
#include <geometry_msgs/Twist.h>
#include <std_msgs/String.h>
#include <riki_msgs/Battery.h>
#include <riki_msgs/Imu.h>
#include <riki_msgs/PID.h>
#include <riki_msgs/Velocities.h>
#include <riki_msgs/Speed_compensation.h>
#include <sensor_msgs/JointState.h>

#include "rose_global.hpp"
#include "rosserial_cpp/rosserial_cpp.h"
#include <SDL.h>
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"

// #include "rose_config.hpp"

// #include <functional>
// using namespace std::placeholders;

// #include <boost/bind/bind.hpp>
// using namespace boost::placeholders;

namespace ros {

struct ttopic_info
{
	ttopic_info()
		: topic_id(0)
		, buffer_size(0)
	{}

	bool deserialize(const uint8_t* msg, int msg_len);

	uint16_t topic_id;
	std::string topic_name;
	std::string message_type;
	std::string md5sum;
	uint32_t buffer_size;
};

bool ttopic_info::deserialize(const uint8_t* msg, int msg_len)
{
	int pos = 0;
	const int min_size = 2 + 4 + 4; // 2(0x007d) + (4)topic_name_size + topic_name + 4(type_size) + type
	if (msg_len < min_size) {
		return false;
	}
	topic_id = posix_mku16(msg[0], msg[1]);
	pos += 2;
	// topic name: mbed_odom
	const int topic_name_size = posix_mku32(posix_mku16(msg[2], msg[3]), posix_mku16(msg[4], msg[5]));
	pos += 4;
	if (msg_len < pos + topic_name_size) {
		return false;
	}
	topic_name.assign((const char*)msg + pos, topic_name_size);
	pos += topic_name_size;

	// type: std_msgs/String
	const int type_name_size = posix_mku32(posix_mku16(msg[pos], msg[pos + 1]), posix_mku16(msg[pos + 2], msg[pos + 3]));
	pos += 4;
	if (msg_len < pos + type_name_size) {
		return false;
	}
	message_type.assign((const char*)msg + pos, type_name_size);
	pos += type_name_size;

	// md5: 992ce8a1687cec8c8bd883ec73ca41d1
	const int md5_size = posix_mku32(posix_mku16(msg[pos], msg[pos + 1]), posix_mku16(msg[pos + 2], msg[pos + 3]));
	pos += 4;
	if (msg_len < pos + md5_size) {
		return false;
	}
	md5sum.assign((const char*)msg + pos, md5_size);
	pos += md5_size;

	// buffer_size
	if (msg_len != pos + 4) {
		return false;
	}
	buffer_size = posix_mku32(posix_mku16(msg[pos], msg[pos + 1]), posix_mku16(msg[pos + 2], msg[pos + 3]));
	return true;
}

struct tpublisher
{
	tpublisher(const ttopic_info& ti, ros::NodeHandle& nh)
		: ti(ti)
	{
		if (ti.message_type == "std_msgs/String") {
			pub = nh.advertise<std_msgs::String>(ti.topic_name, 10);
		} else if (ti.message_type =="riki_msgs/Battery") {
			pub = nh.advertise<riki_msgs::Battery>(ti.topic_name, 10);
		} else if (ti.message_type =="riki_msgs/Imu") {
			pub = nh.advertise<riki_msgs::Imu>(ti.topic_name, 10);
		} else if (ti.message_type =="riki_msgs/Velocities") {
			pub = nh.advertise<riki_msgs::Velocities>(ti.topic_name, 10);
		} else if (ti.message_type =="riki_msgs/Speed_compensation") {
			// xrrobot spcial
			pub = nh.advertise<riki_msgs::Speed_compensation>(ti.topic_name, 10);
		} else {
			std::stringstream err;
			err << "publisher, Unsupport topic-message: " << ti.topic_name << "-" << ti.message_type;
			VALIDATE(false, err.str());
		}
	}

	ttopic_info ti;
	ros::Publisher pub;
};
static std::map<int, tpublisher*> publishers;


trosserial* serial_ptr = nullptr;
uint16_t subcriber_id_Twist = 0;
uint16_t subcriber_id_js = 0;
void did_serial_geometry_msgs_Twist(const geometry_msgs::Twist& msg)
{
	SDL_Log("%u did_serial_geometry_msgs_Twist Twist msg: linear(%.5f, %.5f, %.5f), angular(%.5f, %.5f, %.5f[deg:%.5f])", 
		SDL_GetTicks(), msg.linear.x, msg.linear.y, msg.linear.z,
		msg.angular.x, msg.angular.y, msg.angular.z, RAD2DEG(msg.angular.z));

	SerializedMessage serialized = ros::serialization::serializeMessage<geometry_msgs::Twist>(msg);
/*
	geometry_msgs::Twist recover;
	ros::serialization::deserializeMessage<geometry_msgs::Twist>(serialized, recover);

	SDL_Log("recover Twist msg: linear(%.2f, %.2f, %.2f), angular(%.2f, %.2f, %.2f)", 
		recover.linear.x, recover.linear.y, recover.linear.z,
		recover.angular.x, recover.angular.y, recover.angular.z);
*/
	// if (game_config::os != os_windows) {
		int msg_len = serialized.num_bytes - (serialized.message_start - serialized.buf.get());
		serial_ptr->send_topic(subcriber_id_Twist, serialized.message_start, msg_len);
	// }
}

void trosserial::send_Twist(const geometry_msgs::Twist& msg)
{
	SDL_Log("%u trosserial::send_Twist, msg: linear(%.5f, %.5f, %.5f), angular(%.5f, %.5f, %.5f[deg:%.5f])",
		SDL_GetTicks(), msg.linear.x, msg.linear.y, msg.linear.z,
		msg.angular.x, msg.angular.y, msg.angular.z, RAD2DEG(msg.angular.z));

	SerializedMessage serialized = ros::serialization::serializeMessage<geometry_msgs::Twist>(msg);
/*
	geometry_msgs::Twist recover;
	ros::serialization::deserializeMessage<geometry_msgs::Twist>(serialized, recover);

	SDL_Log("recover Twist msg: linear(%.2f, %.2f, %.2f), angular(%.2f, %.2f, %.2f)", 
		recover.linear.x, recover.linear.y, recover.linear.z,
		recover.angular.x, recover.angular.y, recover.angular.z);
*/
	// if (game_config::os != os_windows) {
		int msg_len = serialized.num_bytes - (serialized.message_start - serialized.buf.get());
		send_topic(subcriber_id_Twist, serialized.message_start, msg_len);
	// }
}

void did_serial_std_msgs_String(const std_msgs::String& msg)
{
	int ii = 0;
}

void did_serial_riki_msgs_Battery(const riki_msgs::Battery& msg)
{
	int ii = 0;
}

void did_serial_riki_msgs_Imu(const riki_msgs::Imu& msg)
{
	int ii = 0;
}

void did_serial_riki_msgs_PID(const riki_msgs::PID& msg)
{
	int ii = 0;
}

void did_serial_riki_msgs_Velocities(const riki_msgs::Velocities& msg)
{
	int ii = 0;
}

void did_serial_riki_msgs_Speed_compensation(const riki_msgs::Speed_compensation& msg)
{
	int ii = 0;
}

void did_serial_sensor_msgs_JointState(const sensor_msgs::JointState& msg)
{
	SerializedMessage serialized = ros::serialization::serializeMessage<sensor_msgs::JointState>(msg);
/*
	geometry_msgs::Twist recover;
	ros::serialization::deserializeMessage<geometry_msgs::Twist>(serialized, recover);

	SDL_Log("recover Twist msg: linear(%.2f, %.2f, %.2f), angular(%.2f, %.2f, %.2f)", 
		recover.linear.x, recover.linear.y, recover.linear.z,
		recover.angular.x, recover.angular.y, recover.angular.z);
*/
	// if (game_config::os != os_windows) {
		int msg_len = serialized.num_bytes - (serialized.message_start - serialized.buf.get());
		serial_ptr->send_topic(subcriber_id_js, serialized.message_start, msg_len);
	// }
}

void trosserial::send_JointState(const sensor_msgs::JointState& msg)
{
	SerializedMessage serialized = ros::serialization::serializeMessage<sensor_msgs::JointState>(msg);
/*
	geometry_msgs::Twist recover;
	ros::serialization::deserializeMessage<geometry_msgs::Twist>(serialized, recover);

	SDL_Log("recover Twist msg: linear(%.2f, %.2f, %.2f), angular(%.2f, %.2f, %.2f)", 
		recover.linear.x, recover.linear.y, recover.linear.z,
		recover.angular.x, recover.angular.y, recover.angular.z);
*/
	// if (game_config::os != os_windows) {
		int msg_len = serialized.num_bytes - (serialized.message_start - serialized.buf.get());
		send_topic(subcriber_id_js, serialized.message_start, msg_len);
	// }
}

struct tsubcriber
{
	tsubcriber(const ttopic_info& ti, ros::NodeHandle& nh)
		: ti(ti)
	{
		if (ti.message_type == "geometry_msgs/Twist") {
			subcriber_id_Twist = ti.topic_id;
			sub = nh.subscribe(ti.topic_name, 100, did_serial_geometry_msgs_Twist);

		} else if (ti.message_type == "std_msgs/String") {
			sub = nh.subscribe(ti.topic_name, 100, did_serial_std_msgs_String);

		} else if (ti.message_type =="riki_msgs/Battery") {
			sub = nh.subscribe(ti.topic_name, 100, did_serial_riki_msgs_Battery);

		} else if (ti.message_type =="riki_msgs/Imu") {
			sub = nh.subscribe(ti.topic_name, 100, did_serial_riki_msgs_Imu);

		} else if (ti.message_type =="riki_msgs/PID") {
			sub = nh.subscribe(ti.topic_name, 100, did_serial_riki_msgs_PID);

		} else if (ti.message_type =="riki_msgs/Velocities") {
			sub = nh.subscribe(ti.topic_name, 100, did_serial_riki_msgs_Velocities);

		} else if (ti.message_type =="riki_msgs/Speed_compensation") {
			// xrrobot spcial
			sub = nh.subscribe(ti.topic_name, 100, did_serial_riki_msgs_Speed_compensation);

		} else if (ti.message_type == "sensor_msgs/JointState") {
			subcriber_id_js = ti.topic_id;
			sub = nh.subscribe(ti.topic_name, 100, did_serial_sensor_msgs_JointState);

		} else {
			std::stringstream err;
			err << "subcriber, Unsupport topic-message: " << ti.topic_name << "-" << ti.message_type;
			VALIDATE(false, err.str());
		}
	}

	ttopic_info ti;
	ros::Subscriber sub;
};
static std::map<int, tsubcriber*> subcribers;

void trosserial::did_read_rosserial(uint16_t topicid, const uint8_t* data, int len)
{
	VALIDATE(len >= 0, null_str);
	// std::string str = rtc::hex_encode((const char*)data, len);
	// SDL_Log("did_read_4gserial, len: %i, data: %s", str.length(), str.c_str());

	// uint16_t topic_id_header = posix_mku16(data[0], data[1]);
	const uint8_t* msg = data;
	int msg_len = len;

	VALIDATE(nh_ != nullptr, null_str);
	ros::NodeHandle& nh = *(reinterpret_cast<ros::NodeHandle*>(nh_));
	if (topicid == ID_PUBLISHER) {
		ttopic_info ti;
		if (!ti.deserialize(msg, msg_len)) {
			return;
		}
		
		VALIDATE(publishers.count(ti.topic_id) == 0, null_str);
		publishers.insert(std::make_pair(ti.topic_id, new tpublisher(ti, nh)));

	} else if (topicid == ID_SUBSCRIBER) {
		ttopic_info ti;
		if (!ti.deserialize(msg, msg_len)) {
			return;
		}

		VALIDATE(subcribers.count(ti.topic_id) == 0, null_str);
		subcribers.insert(std::make_pair(ti.topic_id, new tsubcriber(ti, nh)));

	} else if (topicid == ID_SERVICE_SERVER) {
		int ii = 0;
	} else if (topicid == ID_SERVICE_CLIENT) {
		int ii = 0;
	} else if (topicid == ID_PARAMETER_REQUEST) {
		int ii = 0;
	} else {
		if (publishers.count(topicid)) {
			tpublisher& pub = *publishers.find(topicid)->second;

			uint8_t* p = new uint8_t[msg_len];
			memcpy(p, msg, msg_len);
			boost::shared_array<uint8_t> buf(p);
			ros::SerializedMessage serialized(buf, msg_len);

			// IStream s(msg, msg_len);
			// ros::serialization::deserialize(s, message);
			// SDL_Log("%u [serial_node]msg: %s", SDL_GetTicks(), pub.ti.message_type.c_str());

			if (pub.ti.message_type == "std_msgs::String") {
				std_msgs::String msg;
				ros::serialization::deserializeMessage<std_msgs::String>(serialized, msg);
				pub.pub.publish(msg);

			} else if (pub.ti.message_type =="riki_msgs/Battery") {
				riki_msgs::Battery msg;
				ros::serialization::deserializeMessage<riki_msgs::Battery>(serialized, msg);
				// pub.pub.publish(msg);
				// aplt::valuex.battery_level = msg.battery;
				aplt::valuex.set_NMTHREAD_battery_level(msg.battery);

			} else if (pub.ti.message_type =="riki_msgs/Imu") {
				riki_msgs::Imu msg;
				ros::serialization::deserializeMessage<riki_msgs::Imu>(serialized, msg);
				pub.pub.publish(msg);
				// SDL_Log("%u [serial_node]msg: %s, (%.2f, %.2f, %.2f)", 
				// 	SDL_GetTicks(), pub.ti.message_type.c_str(), msg., msg.linear_y, msg.angular_z);

			} else if (pub.ti.message_type =="riki_msgs/Velocities") {
				riki_msgs::Velocities msg;
				ros::serialization::deserializeMessage<riki_msgs::Velocities>(serialized, msg);
				pub.pub.publish(msg);

			} else if (pub.ti.message_type =="riki_msgs/Speed_compensation") {
				riki_msgs::Speed_compensation msg;
				ros::serialization::deserializeMessage<riki_msgs::Speed_compensation>(serialized, msg);
				pub.pub.publish(msg);
				// SDL_Log("%u [serial_node]msg: %s, k: %i, scc: %i", 
				//	SDL_GetTicks(), pub.ti.message_type.c_str(), msg.k, msg.scc);
			}

		} else if (subcribers.count(topicid)) {
			int ii = 0;
		}
	}
}

trosserial::~trosserial()
{
	free(send_data_);

	for (std::map<int, ros::tpublisher*>::iterator it = ros::publishers.begin(); it != ros::publishers.end(); ++ it) {
		delete it->second;
	}
	ros::publishers.clear();
	for (std::map<int, ros::tsubcriber*>::iterator it = ros::subcribers.begin(); it != ros::subcribers.end(); ++ it) {
		delete it->second;
	}
	ros::subcribers.clear();

	VALIDATE(nh_ != nullptr, null_str);
	if (delete_nh_) {
		delete reinterpret_cast<ros::NodeHandle*>(nh_);
	}
}

}

ROSCPP_DECL int rosserial_cpp__node(bool& exit, const std::string& serial_path, int baudrate)
{
	VALIDATE(!serial_path.empty(), null_str);
	VALIDATE(baudrate > 0, null_str);
    int argc = 0;
	ros::init(argc, nullptr, "node");

	ros::NodeHandle n;

	ros::CallbackQueue cbqueue;
	n.setCallbackQueue(&cbqueue);

	SDL_Log("{rosserial_cpp__node}want open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
	ros::trosserial serial(serial_path, baudrate, &n, false);
	if (!serial.valid()) {
		SDL_Log("{rosserial_cpp__node}cannot open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
		return -1;
	}
	serial.send_topic(ros::trosserial::ID_PUBLISHER, nullptr, 0);

	ros::serial_ptr = &serial;

	ros::WallDuration timeout(0.1f);
	while (!exit && ros::ok()) {
		serial.pool_read();
		cbqueue.callAvailable(timeout);
	}

    return 0;
}
