#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "rose_global.hpp"
#include "wheeltecserial.hpp"
#include "rose_exception.hpp"
#include "rose_filesystem.hpp"
#include "aplt_clazz.hpp"

#include <SDL.h>
#include <SDL_log.h>
#include "rose_string_utils.hpp"

using namespace std::placeholders;

//
// tserial
//
twheeltecserial::twheeltecserial(const std::string& path, int baudrate, uint8_t car_type)
	: tserial(path, baudrate)
	, car_type_(car_type)
	, prefix_(0x7b)
	, prefix_req_(0xfc)
	, prefix_resp_(0xfb)
	, postfix_(0x7d)
	, moveit_prefix_(0xaa)
	, moveit_postfix_(0xbb)
	, voltage_(float_nposm)
	, send_mask_ms_(1000) // 3 second
	, last_report_ticks_(SDL_GetTicks())
	, next_send_ticks_(0)
	, next_pub_voltage_ticks_(0)
	, status_(status_idle)
	, send_data_(nullptr)
	, send_data_size_(4096)
{
	VALIDATE(car_type >= CARTYPE_MIN && car_type <= CARTYPE_MAX, null_str);

	rx_cmds_.insert((uint8_t)FUNC_AUTO_REPORT);
    rx_cmds_.insert((uint8_t)FUNC_BEEP);
    rx_cmds_.insert((uint8_t)FUNC_PWM_SERVO);
    rx_cmds_.insert((uint8_t)FUNC_PWM_SERVO_ALL);
    rx_cmds_.insert((uint8_t)FUNC_RGB);
    rx_cmds_.insert((uint8_t)FUNC_RGB_EFFECT);

    rx_cmds_.insert((uint8_t)FUNC_REPORT_SPEED);
    rx_cmds_.insert((uint8_t)FUNC_REPORT_IMU_RAW);
    rx_cmds_.insert((uint8_t)FUNC_REPORT_IMU_ATT);
    rx_cmds_.insert((uint8_t)FUNC_REPORT_ENCODER);
        
    rx_cmds_.insert((uint8_t)FUNC_RESET_STATE);

    rx_cmds_.insert((uint8_t)FUNC_MOTOR);
    rx_cmds_.insert((uint8_t)FUNC_CAR_RUN);
    rx_cmds_.insert((uint8_t)FUNC_MOTION);
    rx_cmds_.insert((uint8_t)FUNC_SET_MOTOR_PID);
    rx_cmds_.insert((uint8_t)FUNC_SET_YAW_PID);
    rx_cmds_.insert((uint8_t)FUNC_SET_CAR_TYPE);

    rx_cmds_.insert((uint8_t)FUNC_UART_SERVO);
    rx_cmds_.insert((uint8_t)FUNC_UART_SERVO_ID);
    rx_cmds_.insert((uint8_t)FUNC_UART_SERVO_TORQUE);
    rx_cmds_.insert((uint8_t)FUNC_ARM_CTRL);
    rx_cmds_.insert((uint8_t)FUNC_ARM_OFFSET);

    rx_cmds_.insert((uint8_t)FUNC_AKM_DEF_ANGLE);
    rx_cmds_.insert((uint8_t)FUNC_AKM_STEER_ANGLE);


    rx_cmds_.insert((uint8_t)FUNC_REQUEST_DATA);
    rx_cmds_.insert((uint8_t)FUNC_VERSION);

    rx_cmds_.insert((uint8_t)FUNC_RESET_FLASH);

	send_data_ = (uint8_t*)malloc(send_data_size_);
}

twheeltecserial::~twheeltecserial()
{
	free(send_data_);
}

static uint8_t calculate_sum(const uint8_t* data, int len)
{
	VALIDATE(data && len > 0, null_str);

	uint8_t sum = 0;
	for (int i = 0; i < len; i ++) {
		sum ^= data[i];
	}
	return sum;
}

void twheeltecserial::app_pool_read_bh()
{
	// std::string str = utils::hex_encode_cstyle((const char*)recv_data_, recv_data_vsize_, ' ');
	// SDL_Log("%u {twheeltecserial}app_pool_read_bh: %s, recv_data_vsize_: %i", SDL_GetTicks(), str.c_str(), recv_data_vsize_);

	// 7b([prefix) 00(flag_stop) 00 00 00 00 00 00 ff e0 00 0a 3f d8 ff fc ff ff ff f7 30 78 ca(checksum) 7d (postfix)
	const int pack_len = 24;

	while (true) {
		while (true) {
			// 1. must prefix with 0x7b
			int skip = 0;
			for (int i = 0; i < recv_data_vsize_; i ++) {
				if (recv_data_[i] == prefix_) {
					break;
				}
				skip ++;
			}
			if (skip && skip != recv_data_vsize_) {
				memcpy(recv_data_, recv_data_ + skip, recv_data_vsize_ - skip);
			}
			recv_data_vsize_ -= skip;
			if (recv_data_vsize_ < pack_len) {
				return;
			}
			// 2. postfix
			if (recv_data_[pack_len - 1] != postfix_) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
	
			const uint8_t sum = calculate_sum(recv_data_, pack_len - 2);
			if (sum != recv_data_[pack_len - 2]) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
			break;
		}

		const int len2 = pack_len;
		VALIDATE(recv_data_vsize_ >= len2, null_str);

		const uint8_t sum = calculate_sum(recv_data_, len2 - 2);
		VALIDATE(sum == recv_data_[len2 - 2], null_str);

		did_read_wheeltec(recv_data_, len2);

		if (recv_data_vsize_ > len2) {
			memcpy(recv_data_, recv_data_ + len2, recv_data_vsize_ - len2);
		}
		recv_data_vsize_ -= len2;

		if (recv_data_vsize_ == 0) {
			// SDL_Log("twheeltecserial read(1.2): there is no extra data");
		} else {
			// std::string str = utils::hex_encode_cstyle((const char*)recv_data_, recv_data_vsize_, ' ');
			// SDL_Log("twheeltecserial read(1.2): extra data: %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);
		}
	}
}

void twheeltecserial::did_read_wheeltec(const uint8_t* data_ptr, int len2)
{
	// VALIDATE(status_ >= 0 && status_ < status_count, null_str);

	// const uint32_t now = SDL_GetTicks();
	// std::string str = utils::hex_encode_cstyle((const char*)data_ptr, len2, ' ');
	// SDL_Log("%u serial read2: %s", now, str.c_str());

	uint8_t flag_stop = data_ptr[1];
	double linear_x = posix_mki16(data_ptr[3], data_ptr[2]) / 1000.0;
	double linear_y = posix_mki16(data_ptr[5], data_ptr[4]) / 1000.0;
	double angluar_z = posix_mki16(data_ptr[7], data_ptr[6]) / 1000.0;

	double voltage = posix_mki16(data_ptr[21], data_ptr[20]) / 1000.0;
	voltage_ = voltage;

	const uint32_t now = SDL_GetTicks();
	if (now >= next_pub_voltage_ticks_) {
		const int pub_voltage_period_sec = 2000;
		next_pub_voltage_ticks_ = now + pub_voltage_period_sec;
		aplt::valuex.set_NMTHREAD_battery_level(voltage_);

		// SDL_Log("%u, {wheeltec}public voltage: %.4f", SDL_GetTicks(), voltage_);
	}

	// SDL_Log("%u, flag_stop: %i, vel(%.3f, %.3f, %.3f(%.2f)) voltage: %.3f", 
	//	SDL_GetTicks(), flag_stop, linear_x, linear_y, angluar_z, RAD2DEG(angluar_z), voltage);
}

void twheeltecserial::send_Twist(const geometry_msgs::Twist& msg)
{
	// SDL_Log("%u twheeltecserial::send_cmd send(cmd_id:0x%x, msg_len:%i)", SDL_GetTicks(), cmd_id, msg_len);

	const int pack_len = 11;
	memset(send_data_, 0, pack_len);

	const uint8_t broadcast_address = 0xff;
	send_data_[0] = prefix_;
	send_data_[1] = 0; // set aside
	send_data_[2] = 0; // set aside

	// int16_t linear_x = msg.linear.x * 1000.0;
	int16_t linear_x = msg.linear.x * 1000.0;
	int16_t linear_y = msg.linear.y * 1000.0;
	int16_t angular_z = msg.angular.z * 1000.0;

	send_data_[3] = posix_hi8(linear_x);
	send_data_[4] = posix_lo8(linear_x);


	send_data_[5] = posix_hi8(linear_y);
	send_data_[6] = posix_lo8(linear_y);

	send_data_[7] = posix_hi8(angular_z);
	send_data_[8] = posix_lo8(angular_z);

	uint8_t sum = calculate_sum(send_data_, 9);
	send_data_[9] = sum;
	send_data_[10] = postfix_;
	send_data(send_data_, pack_len);

	// write_file("c:/ddksample/1.dat", (const char*)send_data_, sum_index + 1 + 1);
}

void twheeltecserial::send_JointState(const sensor_msgs::JointState& msg)
{
	VALIDATE(msg.name.size() == msg.position.size(), null_str);

	const int joints = 6;
	if (msg.position.size() != joints) {
		return;
	}

	// range: rad[-1.57, 1.57]/deg[-90, 90], To avoid reaching MAX limit, take a slightly smaller value
	// const int16_t min_value = -1565; // -1.565 * 1000
	// const int16_t max_value = 1565; // 1.565 * 1000

	const int16_t min_value = -1570; // -1.565 * 1000
	const int16_t max_value = 1570; // 1.565 * 1000

	const int pack_len = 16;
	memset(send_data_, 0, pack_len);

	int index = 0;
	send_data_[0] = moveit_prefix_;
	index ++;

	// double joint_values[6] = {0x00, DEG2RAD(-45), DEG2RAD(45), DEG2RAD(-45), DEG2RAD(90), DEG2RAD(-90)};
	const double* joint_values = &msg.position[0];
	for (int at = 0; at < joints; at ++) {
		int16_t value = joint_values[at] * 1000;

		if (value < min_value) {
			value = min_value;
		} else if (value > max_value) {
			value = max_value;
		}

		send_data_[index ++] = posix_hi8(value);
		send_data_[index ++] = posix_lo8(value);
	}

	send_data_[index ++] = 0x01;

	uint8_t sum = calculate_sum(send_data_, index);
	send_data_[index ++] = sum;

	send_data_[index ++] = moveit_postfix_;
	send_data(send_data_, pack_len);
	VALIDATE(index == pack_len, null_str);
}

