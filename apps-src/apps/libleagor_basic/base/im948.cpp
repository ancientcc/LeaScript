#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "rose_global.hpp"
#include "im948.hpp"
#include "rose_exception.hpp"

#include <SDL.h>
#include <SDL_log.h>
#include "aplt_clazz.hpp"

using namespace std::placeholders;

//
// tserial
//
tim948serial::tim948serial(const std::string& path, int baudrate, bool use_magnetometer)
	: tserial(path, baudrate)
	, aplt::texternal_imu(use_magnetometer)
	, prefix_(0x49)
	, address_(0x00)
	, postfix_(0x4d)
	, send_mask_ms_(1000) // 3 second
	, prefix4_is_magnetic_(false)
	, last_report_ticks_(SDL_GetTicks())
	, next_send_ticks_(0)
	, status_(status_idle)
	, send_data_(nullptr)
	, send_data_size_(4096)
{
	rx_cmds_.insert((uint8_t)ID_SLEEP_SENSOR);
	rx_cmds_.insert((uint8_t)ID_WAKEUP_SENSOR);
	rx_cmds_.insert((uint8_t)ID_SET_CONFIG);
	rx_cmds_.insert((uint8_t)ID_DISABLE_REPORT);
	rx_cmds_.insert((uint8_t)ID_REPORT_DATA);
	rx_cmds_.insert((uint8_t)ID_ENABLE_REPORT);
	rx_cmds_.insert((uint8_t)ID_START_CALIBRATION);
	rx_cmds_.insert((uint8_t)ID_STOP_CALIBRATION);

	send_data_ = (uint8_t*)malloc(send_data_size_);

	set_did_read(std::bind(&tim948serial::did_read_im948, this, _1, _2));
}

tim948serial::~tim948serial()
{
	SDL_Log("tim948serial::~tim948serial() status_: %i", status_);
	if (status_ != status_idle) {
		// if in status_report, requrie send ID_DISABLE_REPORT, ID_SLEEP_SENSOR, else only the latter.
		bool sleep_sent = false;
		if (status_ == status_report) {
			send_cmd(tim948serial::ID_DISABLE_REPORT, nullptr, 0);
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
				send_cmd(tim948serial::ID_SLEEP_SENSOR, nullptr, 0);
				sleep_sent = true;
			}
			SDL_Delay(10);
		}
	}

	free(send_data_);
}

static uint8_t calculate_sum(const uint8_t* data, int len)
{
	VALIDATE(data && len > 0, null_str);

	uint32_t sum = 0;
	for (int i = 1; i < len; i ++) {
		sum += data[i];
	}
	return sum & 0xff;
}

void tim948serial::app_pool_read_bh()
{
	// std::string str = rtc::hex_encode((const char*)recv_data_, recv_data_vsize_);
	// SDL_Log("serial read(1.1): %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);

	// 49(heador) 00(address) 13(length) 11(cmd_code) 48 00 68 49 22 00 A4 00 C3 00 CE FF 00 FE 2D F9 B5 1D 69(checksum) 4D(postfix)
	const int size_before_len = 3; // 1(header) + 1(address) + 1(length)
	const int min_size = size_before_len + 3; // 1(cmd_code) + 1(checksum) + 1(postfix)
	const int min_size_mimus1 = min_size - 1; // min_size include one length.

	while (true) {
		uint16_t topicid = 0;
		int msg_len = 0;
		while (true) {
			// 1. must prefix with: header_ address_
			int skip = 0;
			for (int i = 0; i < recv_data_vsize_ - 1; i ++) {
				if (recv_data_[i] == prefix_ && recv_data_[i + 1] == address_) {
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
			if (len == 0 || rx_cmds_.count(recv_data_[3]) == 0) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
			if (recv_data_vsize_ < min_size_mimus1 + len) {
				return;
			}
			const uint8_t sum = calculate_sum(recv_data_, size_before_len + len);
			if (sum != recv_data_[size_before_len + len] || recv_data_[size_before_len + len + 1] != postfix_) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
			break;
		}

		const int len = recv_data_[2];
		const int len2 = min_size_mimus1 + len;
		VALIDATE(recv_data_vsize_ >= len2, null_str);

		const uint8_t sum = calculate_sum(recv_data_, size_before_len + len);
		VALIDATE(sum == recv_data_[size_before_len + len], null_str);

		if (did_read_) {
			did_read_(recv_data_, len2);
		}

		if (recv_data_vsize_ > len2) {
			memcpy(recv_data_, recv_data_ + len2, recv_data_vsize_ - len2);
		}
		recv_data_vsize_ -= len2;

		if (recv_data_vsize_ == 0) {
			// SDL_Log("tim948serial read(1.2): there is no extra data");
		} else {
			// std::string str = rtc::hex_encode((const char*)recv_data_, recv_data_vsize_);
			// SDL_Log("tim948serial read(1.2): extra data: %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);
		}
	}
}

// #include "filesystem.hpp"

void tim948serial::send_cmd(uint8_t cmd_id, const uint8_t* msg, uint8_t msg_len)
{
	SDL_Log("%u tim948serial::send_cmd send(cmd_id:0x%x, msg_len:%i)", SDL_GetTicks(), cmd_id, msg_len);
/*
	uint8_t to[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 
		0x00, 0xFF, 0x49, 0xFF, 0x0B, 0x12, 0x05, 0xFF, 0x00, 0x05, 0x1E, 0x01, 0x03, 0x05, 0x48, 0x00, 0x94, 0x4D};
*/
/*
	uint8_t to[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 
		0x00, 0xFF, 0x49, 0xFF, 0x01, 0x03, 0x03, 0x4D};
*/
	// reference to: https://www.yuque.com/cxqwork/lkw3sg/yqa3e0?
	memset(send_data_, 0, 46);
	send_data_[46] = 0x00;
	send_data_[47] = 0xff;
	send_data_[48] = 0x00;
	send_data_[49] = 0xff;

	const uint8_t broadcast_address = 0xff;
	send_data_[50] = prefix_;
	send_data_[51] = broadcast_address;

	send_data_[52] = msg_len + 1;
	send_data_[53] = cmd_id;

	if (msg_len != 0) {
		memcpy(send_data_ + 54, msg, msg_len);
	}
	int sum_index = 54 + msg_len;

	uint8_t sum = calculate_sum(send_data_ + 50, 1 + 1 + 1 + 1 + msg_len);
	send_data_[sum_index] = sum;
	send_data_[sum_index + 1] = postfix_;
	send_data(send_data_, sum_index + 1 + 1);

	// write_file("c:/ddksample/1.dat", (const char*)send_data_, sum_index + 1 + 1);
	// write_file("c:/ddksample/2.dat", (const char*)to, sizeof(to));
}

void tim948serial::did_read_im948(const uint8_t* data_ptr, int len2)
{
	VALIDATE(status_ >= 0 && status_ < status_count, null_str);

	// const uint32_t now = SDL_GetTicks();
	// std::string str = rtc::hex_encode((const char*)data_ptr, len2);
	// SDL_Log("%u serial read2: %s", now, str.c_str());

	const int msg_len = data_ptr[2] - 1;
	const int cmd_id = data_ptr[3];
	if (cmd_id == ID_SET_CONFIG) {
		// user swing card
		if (status_ == status_idle && msg_len == 0) {
			SDL_Log("rx [ID_SET_CONFIG], status_idle --> wait_wakeup");
			status_ = wait_wakeup;
		} else {
			SDL_Log("rx [ID_SET_CONFIG], ignore. len-of-msg: %i", msg_len);
		}

	} else if (cmd_id == ID_WAKEUP_SENSOR) {
		if (status_ == wait_wakeup && msg_len == 0) {
			SDL_Log("rx [ID_WAKEUP_SENSOR], wait_wakeup --> wait_enable_report");
			status_ = wait_enable_report;
		} else {
			SDL_Log("rx [ID_WAKEUP_SENSOR], ignore. len-of-msg: %i", msg_len);
		}

	} else if (cmd_id == ID_ENABLE_REPORT) {
		// user swing card
		if (status_ == wait_enable_report && msg_len == 0) {
			SDL_Log("rx [ID_ENABLE_REPORT], wait_enable_report --> status_report");
			status_ = status_report;
		} else {
			SDL_Log("rx [ID_ENABLE_REPORT], ignore. len-of-msg: %i", msg_len);
		}

	} else if (cmd_id == ID_REPORT_DATA) {
		const uint8_t* magnetic_ptr = data_ptr + (4 + 2 + 4);
		if (prefix4_is_magnetic_) {
			double magnetic_ratio = 0.15106201171875;
			int16_t i16_magnetic[3] = {posix_mki16(magnetic_ptr[0], magnetic_ptr[1]), 
				posix_mki16(magnetic_ptr[2], magnetic_ptr[3]), posix_mki16(magnetic_ptr[4], magnetic_ptr[5])};
			for (int at = 0; at < 3; at ++) {
				magnetic[at] = i16_magnetic[at] * magnetic_ratio;
			}
			magnetic[3] = sqrt(magnetic[0] * magnetic[0] + magnetic[1] * magnetic[1] + magnetic[2] * magnetic[2]);
		} else {
			double angular_velocity_ratio = 0.06103515625;
			int16_t i16_magnetic[3] = {posix_mki16(magnetic_ptr[0], magnetic_ptr[1]), 
				posix_mki16(magnetic_ptr[2], magnetic_ptr[3]), posix_mki16(magnetic_ptr[4], magnetic_ptr[5])};
			for (int at = 0; at < 3; at ++) {
				angular_velocity[at] = i16_magnetic[at] * angular_velocity_ratio;
			}
			angular_velocity[3] = sqrt(angular_velocity[0] * angular_velocity[0] + angular_velocity[1] * angular_velocity[1] + angular_velocity[2] * angular_velocity[2]);

			aplt::valuex.angular_vel[0] = DEG2RAD(angular_velocity[0]);
			aplt::valuex.angular_vel[1] = DEG2RAD(angular_velocity[1]);
			aplt::valuex.angular_vel[2] = DEG2RAD(angular_velocity[2]);
		}

		const uint8_t* euler_ptr = magnetic_ptr + 6;
		double euler_ratio = 0.0054931640625;
		int16_t i16_euler[3] = {posix_mki16(euler_ptr[0], euler_ptr[1]), 
			posix_mki16(euler_ptr[2], euler_ptr[3]), posix_mki16(euler_ptr[4], euler_ptr[5])};
		for (int at = 0; at < 3; at ++) {
			euler[at] = i16_euler[at] * euler_ratio;
		}
		aplt::valuex.euler[0] = DEG2RAD(euler[0]);
		aplt::valuex.euler[1] = DEG2RAD(euler[1]);
		aplt::valuex.euler[2] = DEG2RAD(euler[2]);

		// SDL_Log("%u(intval:%u), [ID_REPORT_DATA] angular_velocity(%.5f, %.5f, %.5f, %.5f) euler(%.5f, %.5f, %.5f)", 
		//	SDL_GetTicks(), SDL_GetTicks() - last_report_ticks_, angular_velocity[0], angular_velocity[1], angular_velocity[2], magnetic[3],
		//	euler[0], euler[1], euler[2]);

		last_report_ticks_ = SDL_GetTicks();

	} else if (cmd_id == ID_DISABLE_REPORT) {
		if (status_ == status_report && msg_len == 0) {
			SDL_Log("rx [ID_DISABLE_REPORT], status_report --> wait_enable_report");
			status_ = wait_enable_report;
		} else {
			SDL_Log("rx [ID_DISABLE_REPORT], ignore. len-of-msg: %i", msg_len);
		}

	} else if (cmd_id == ID_SLEEP_SENSOR) {
		SDL_Log("rx [ID_SLEEP_SENSOR], %i --> status_idle", status_);
		status_ = status_idle;

	} else if (cmd_id == ID_START_CALIBRATION) {
		SDL_Log("rx [ID_START_CALIBRATION], status: %i len-of-msg: %i", status_, msg_len);

	} else if (cmd_id == ID_STOP_CALIBRATION) {
		SDL_Log("rx [ID_STOP_CALIBRATION], status: %i len-of-msg: %i", status_, msg_len);
	}
}

void tim948serial::slice()
{
	pool_read();
	if (status_ != tim948serial::status_report) {
		state_slice();

	}
}

bool tim948serial::can_read() const
{
	return status_ == tim948serial::status_report;
}

void tim948serial::state_slice()
{
	VALIDATE(status_ != status_report, null_str);
	VALIDATE(status_ >= 0 && status_ < status_count, null_str);

	const uint32_t now = SDL_GetTicks();
	if (status_ == status_idle) {
		if (now >= next_send_ticks_) {
			int MAGNETIC_FLAG = 0x1;
			uint8_t byte4 = 0x4 | (use_magnetometer_? MAGNETIC_FLAG: 0x0);
			uint8_t byte5 = 25; // Reporting frequency, default is 10
			uint8_t byte9 = prefix4_is_magnetic_? 0x48: 0x44;

			uint8_t msg[] = {0x05, 0xFF, 0x00, byte4, byte5, 0x01, 0x03, 0x05, byte9, 0x00}; // 10hz
			send_cmd(tim948serial::ID_SET_CONFIG, msg, sizeof(msg));
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
}

tim948serial* im948serial_open(const std::string& serial_path, int baudrate, bool use_magnetometer)
{
	VALIDATE(!serial_path.empty(), null_str);
	VALIDATE(baudrate > 0, null_str);

	SDL_Log("{im948serial_start}want open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
	tim948serial* serial = new tim948serial(serial_path, baudrate, use_magnetometer);
	if (!serial->valid()) {
		SDL_Log("{im948serial_start}cannot open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
		delete serial;
		return nullptr;
	}

    return serial;
}