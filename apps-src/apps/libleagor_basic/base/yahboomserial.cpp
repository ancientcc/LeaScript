#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "rose_global.hpp"
#include "yahboomserial.hpp"
#include "rose_exception.hpp"
#include "aplt_clazz.hpp"

#include <SDL.h>
#include <SDL_log.h>
#include "rose_string_utils.hpp"

using namespace std::placeholders;

//
// tserial
//
tyahboomserial::tyahboomserial(const std::string& path, int baudrate, uint8_t car_type)
	: tserial(path, baudrate)
	, car_type_(car_type)
	, prefix_(0xff)
	, prefix_req_(0xfc)
	, prefix_resp_(0xfb)
	, postfix_(0x4d)
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

	set_did_read(std::bind(&tyahboomserial::did_read_im948, this, _1, _2));
}

tyahboomserial::~tyahboomserial()
{
	CAR_RUN_stop();
/*
	if (status_ != status_idle) {
		// if in status_report, requrie send ID_DISABLE_REPORT, ID_SLEEP_SENSOR, else only the latter.
		bool sleep_sent = false;
		if (status_ == status_report) {
			send_cmd(tyahboomserial::ID_DISABLE_REPORT, nullptr, 0);
		}

		const int timeout = 5000;
		const uint32_t start_ticks = SDL_GetTicks();
		while (status_ != status_idle) {
			if ((int)(SDL_GetTicks() - start_ticks) >= timeout) {
				break;;
			}
			pool_read();
			if (!sleep_sent && status_ != status_report) {
				// Once out of status_report, send ID_SLEEP_SENSOR. Of course, only send it once.
				send_cmd(tyahboomserial::ID_SLEEP_SENSOR, nullptr, 0);
				sleep_sent = true;
			}
			SDL_Delay(10);
		}
	}
*/
	free(send_data_);
}

static uint8_t calculate_sum(const uint8_t* data, int len)
{
	VALIDATE(data && len > 0, null_str);

	uint32_t sum = 0;
	for (int i = 2; i < len; i ++) {
		sum += data[i];
	}
	return sum & 0xff;
}

void tyahboomserial::app_pool_read_bh()
{
	// std::string str = rtc::hex_encode((const char*)recv_data_, recv_data_vsize_);
	// SDL_Log("serial read(1.1): %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);

	// ff([prefix) fc(prefix_resp) 13(length) 11(cmd_code) 48 00 68 49 22 00 A4 00 C3 00 CE FF 00 FE 2D F9 B5 1D 69(checksum) 4D(postfix)
	const int size_before_len = 2; // 1(header) + 1(address)
	const int min_size = size_before_len + 3; // 1(length) + 1(cmd_code) + 1(checksum)
	const int min_size_mimus1 = min_size - 1; // min_size include one length.

	while (true) {
		uint16_t topicid = 0;
		int msg_len = 0;
		while (true) {
			// 1. must prefix with: header_ address_
			int skip = 0;
			for (int i = 0; i < recv_data_vsize_ - 1; i ++) {
				if (recv_data_[i] == prefix_ && recv_data_[i + 1] == prefix_resp_) {
					break;
				}
				skip ++;
			}
			if (skip && skip != recv_data_vsize_) {
				memcpy(recv_data_, recv_data_ + skip, recv_data_vsize_ - skip);
			}
			recv_data_vsize_ -= skip;
			if (recv_data_vsize_ < min_size) {
				// need more byte
				return;
			}

			// 2. len
			const int len = recv_data_[2];
			if (len < 2 || rx_cmds_.count(recv_data_[3]) == 0) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
			if (recv_data_vsize_ < size_before_len + len + 1) {
				return;
			}
			const uint8_t sum = calculate_sum(recv_data_, size_before_len + len - 1);
			if (sum != recv_data_[size_before_len + len - 1]) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
			break;
		}

		const int len = recv_data_[2];
		const int len2 = size_before_len + len;
		VALIDATE(recv_data_vsize_ >= len2, null_str);

		const uint8_t sum = calculate_sum(recv_data_, len2 - 1);
		VALIDATE(sum == recv_data_[len2 - 1], null_str);

		if (did_read_) {
			did_read_(recv_data_, len2);
		}

		if (recv_data_vsize_ > len2) {
			memcpy(recv_data_, recv_data_ + len2, recv_data_vsize_ - len2);
		}
		recv_data_vsize_ -= len2;

		if (recv_data_vsize_ == 0) {
			// SDL_Log("tyahboomserial read(1.2): there is no extra data");
		} else {
			// std::string str = rtc::hex_encode((const char*)recv_data_, recv_data_vsize_);
			// SDL_Log("tyahboomserial read(1.2): extra data: %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);
		}
	}
}

void tyahboomserial::send_cmd(uint8_t cmd_id, const uint8_t* msg, uint8_t msg_len)
{
	// SDL_Log("%u tyahboomserial::send_cmd send(cmd_id:0x%x, msg_len:%i)", SDL_GetTicks(), cmd_id, msg_len);

	// reference to: https://www.yuque.com/cxqwork/lkw3sg/yqa3e0?
	memset(send_data_, 0, msg_len + 10);

	const uint8_t broadcast_address = 0xff;
	send_data_[0] = prefix_;
	send_data_[1] = prefix_req_;

	send_data_[2] = 1 + 1 + msg_len + 1;
	send_data_[3] = cmd_id;

	if (msg_len != 0) {
		memcpy(send_data_ + 4, msg, msg_len);
	}
	int sum_index = 4 + msg_len;

	uint8_t sum = calculate_sum(send_data_, 1 + 1 + 1 + 1 + msg_len);
	send_data_[sum_index] = sum;
	send_data(send_data_, sum_index + 1);

	int ii = 0;

	// write_file("c:/ddksample/1.dat", (const char*)send_data_, sum_index + 1 + 1);
	// write_file("c:/ddksample/2.dat", (const char*)to, sizeof(to));
}

void tyahboomserial::did_read_im948(const uint8_t* data_ptr, int len2)
{
	// VALIDATE(status_ >= 0 && status_ < status_count, null_str);

	// const uint32_t now = SDL_GetTicks();
	// std::string str = utils::hex_encode_cstyle((const char*)data_ptr, len2, ' ');
	// SDL_Log("%u serial read2: %s", now, str.c_str());

	const int msg_len = data_ptr[2] - 1 - 1;
	const int cmd_id = data_ptr[3];
	const uint8_t* msg = data_ptr + 4;

	if (cmd_id == FUNC_REPORT_SPEED) {
		double voltage = msg[6] / 10.0;
		voltage_ = voltage;
		// SDL_Log("%u, voltage: %.2f", SDL_GetTicks());

		const uint32_t now = SDL_GetTicks();
		if (now >= next_pub_voltage_ticks_) {
			const int pub_voltage_period_sec = 2000;
			next_pub_voltage_ticks_ = now + pub_voltage_period_sec;
			aplt::valuex.set_NMTHREAD_battery_level(voltage_);

			SDL_Log("%u, {yahboom}public voltage: %.4f", SDL_GetTicks(), voltage_);
		}

	} else if (cmd_id == FUNC_REPORT_IMU_RAW) {
		int16_t MAG_X = posix_mki16(msg[12], msg[13]);
		int16_t MAG_Y = posix_mki16(msg[14], msg[15]);
		int16_t MAG_Z = posix_mki16(msg[16], msg[17]);
		// SDL_Log("%u, MAX_X:%i MAX_Y:%i, MAX_Z: %i", SDL_GetTicks(), (int)MAG_X, (int)MAG_Y, (int)MAG_Z);
		
	} else if (cmd_id == FUNC_REPORT_IMU_ATT) {


	} else if (cmd_id == FUNC_REPORT_ENCODER) {


	}
}

void tyahboomserial::CAR_RUN_stop()
{
	uint8_t status = 0; // don't output motor, and result to stop
	uint8_t speed = 0;
	uint8_t data[] = {car_type_, status, speed};
	send_cmd(FUNC_CAR_RUN, data, sizeof(data) / sizeof(data[0]));
	SDL_Log("%u CAR_RUN_stop...", SDL_GetTicks());
}

void tyahboomserial::slice()
{
/*
	pool_read();
	if (status_ != tyahboomserial::status_report) {
		state_slice();

	}
*/
}


void tyahboomserial::state_slice()
{
/*
	VALIDATE(status_ != status_report, null_str);
	VALIDATE(status_ >= 0 && status_ < status_count, null_str);

	const uint32_t now = SDL_GetTicks();
	if (status_ == status_idle) {
		if (now >= next_send_ticks_) {
			// uint8_t msg[] = {0x05, 0xFF, 0x00, 0x05, 0x1E, 0x01, 0x03, 0x05, 0x48, 0x00}; // 30hz
			uint8_t msg[] = {0x05, 0xFF, 0x00, 0x05, 10, 0x01, 0x03, 0x05, 0x48, 0x00}; // 10hz
			// uint8_t msg[] = {0x05, 0xFF, 0x00, 0x05, 50, 0x01, 0x03, 0x05, 0x48, 0x00}; // 50hz
			send_cmd(tyahboomserial::ID_SET_CONFIG, msg, sizeof(msg));
			next_send_ticks_ = now + send_mask_ms_;
		}

	} else if (status_ == wait_wakeup) {
		if (now >= next_send_ticks_) {
			send_cmd(ID_WAKEUP_SENSOR, nullptr, 0);
			next_send_ticks_ = now + send_mask_ms_;
		}

	} else if (status_ == wait_enable_report) {
		if (now >= next_send_ticks_) {
			send_cmd(ID_ENABLE_REPORT, nullptr, 0);
			next_send_ticks_ = now + send_mask_ms_;
		}
	}
*/
}

