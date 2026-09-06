#define GETTEXT_DOMAIN "aplt_nlsd_basic-lib"

#include "rose_global.hpp"
#include "llampcp.hpp"
#include "rose_exception.hpp"

#include <SDL.h>
#include <SDL_log.h>
#include "aplt_clazz.hpp"

using namespace std::placeholders;

//
// tl07serial
//
tl07serial::tl07serial(const std::string& path, int baudrate)
	: tserial(path, baudrate)
	, prefix_(0x07)
	, fixed_4bytes_(4) // prefix_, len, cmd, ..., sum
	, send_data_(nullptr)
	, send_data_size_(0)
{
	resize_send_data(1024);
}

tl07serial::~tl07serial()
{
	if (send_data_ != nullptr) {
		free(send_data_);
		send_data_ = nullptr;
		send_data_size_ = 0;
	}
}

void tl07serial::resize_send_data(int size)
{
	size = posix_align_ceil(size, 1024);
	VALIDATE(size > 0, null_str);

	if (size > send_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (send_data_ != nullptr) {
			free(send_data_);
		}
		send_data_ = tmp;
		send_data_size_ = size;
	}
}

uint8_t tl07serial::calculate_sum(const uint8_t* data, int len) const
{
	VALIDATE(data && len > 0, null_str);

	uint32_t sum = 0;
	for (int i = 1; i < len; i ++) {
	// for (int i = 0; i < len; i ++) {
		sum += data[i];
	}
	return sum & 0xff;
}

void tl07serial::app_pool_read_bh()
{
	while (true) {
		while (true) {
			// 1. must prefix with 0x7
			int skip = 0;
			for (int i = 0; i < recv_data_vsize_; i ++) {
				if (recv_data_[i] == 0x7) {
					break;
				}
				skip ++;
			}
			if (skip && skip != recv_data_vsize_) {
				memcpy(recv_data_, recv_data_ + skip, recv_data_vsize_ - skip);
			}
			recv_data_vsize_ -= skip;
			if (recv_data_vsize_ <= 3) { // 1 + 1 + 1 + x
				return;
			}
	
			// 2. len
			const int len = recv_data_[1];
			if (len_cmds_.count(posix_mku16(len, recv_data_[2])) == 0) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
			if (recv_data_vsize_ < 1 + 1 + len + 1) {
				return;
			}
			const uint8_t sum = calculate_sum(recv_data_, 1 + 1 + len);
			if (sum != recv_data_[1 + 1 + len]) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
			break;
		}

		const int len = recv_data_[1];
		const int len2 = 1 + 1 + len + 1;
		VALIDATE(recv_data_vsize_ >= len2, null_str);

		const uint8_t sum = calculate_sum(recv_data_, 1 + 1 + len);
		VALIDATE(sum == recv_data_[1 + 1 + len], null_str);

		did_read_cmd(recv_data_, len2);

		if (recv_data_vsize_ > len2) {
			memcpy(recv_data_, recv_data_ + len2, recv_data_vsize_ - len2);
		}
		recv_data_vsize_ -= len2;

		if (recv_data_vsize_ == 0) {
			// SDL_Log("tmainserial read(1.2): there is no extra data");
		} else {
			std::string str = utils::hex_encode_cstyle((const char*)recv_data_, recv_data_vsize_, ' ');
			SDL_Log("tmainserial read(1.2): extra data: %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);
		}
	}
}

void tl07serial::send_cmd(uint8_t cmd_id, const uint8_t* msg, uint8_t msg_len)
{
	// memset(send_data_, 0, msg_len + prefix_3bytes_);

	send_data_[0] = prefix_;
	send_data_[1] = 1 + msg_len;
	send_data_[2] = cmd_id;

	if (msg_len != 0) {
		memcpy(send_data_ + 3, msg, msg_len);
	}
	int sum_index = 3 + msg_len;

	uint8_t sum = calculate_sum(send_data_, 3 + msg_len);
	send_data_[sum_index] = sum;
	send_data(send_data_, sum_index + 1);
}

//
// tserial
//
tllampcpserial::tllampcpserial(aplt::tbase_ext_lamp& ext_lamp, const std::string& path, int baudrate)
	: tl07serial(path, baudrate)
	, ext_lamp_(ext_lamp)
	, initialized_(false)
	, new_fw_(false)
{
	// len_cmds_.insert(posix_mku16(7, 0x10));
	len_cmds_.insert(posix_mku16(11, 0x10));
}

tllampcpserial::~tllampcpserial()
{
	SDL_Log("tllampcpserial::~tllampcpserial()");
}

void tllampcpserial::app_pool_read_bh()
{
	tl07serial::app_pool_read_bh();
}

void tllampcpserial::did_read_cmd(const uint8_t* data_ptr, int len2)
{
	const int len = data_ptr[1];
	const int msg = data_ptr[2];

	if (msg == 0x10) {
		if (len != 11) {
			SDL_Log("tllampcpserial::did_read_cmd, data error, cmd = 0x10, but len != 11");
			return;
		}

		new_fw_ = len == 11;

		const uint8_t CB_ver = data_ptr[3];
		const int manufacturer = posix_mku16(data_ptr[5], data_ptr[4]);
		uint8_t product = data_ptr[6];
		uint8_t temperature = data_ptr[7];
		uint8_t reserved = data_ptr[8];
		uint8_t custom0 = data_ptr[9];
		uint8_t custom1 = data_ptr[10];

		uint8_t scene_value = data_ptr[11];
		uint8_t privacy_value = data_ptr[12];

		SDL_Log("%u tllampcpserial::did_read_cmd, receive cmd: 0x10, initialized_: %s", SDL_GetTicks(), initialized_? "true": "false");
		const bool first = !initialized_;
		if (!initialized_) {
/*
			if (scene_value != 0 || privacy_value != 0) {
				SDL_Log("%u tllampcpserial::did_read_cmd, 'initialized_ == false', but either scene_value(%i) or privary_val(%i) isn't 0, discard", 
					(int)scene_value, (int)privacy_value);
				return;
			}
*/
			SDL_Log("%u tllampcpserial::did_read_cmd, 'initialized_ == false', send global setting", SDL_GetTicks());
			ext_lamp_.lamp_set_global_settings(ext_lamp_.lamp_longpress_threshold());
			initialized_ = true;
			// return;
		}

		ext_lamp_.lamp_did_status_report(first, CB_ver, manufacturer, product, 
			temperature, reserved, custom0, custom1);

		if (scene_value != 0) {
			ext_lamp_.lamp_did_button_pressed(aplt::tbase_ext_lamp::btntype_scene, scene_value);

		} else if (privacy_value != 0) {
			ext_lamp_.lamp_did_button_pressed(aplt::tbase_ext_lamp::btntype_privacy, privacy_value);
		}
	}

}

void tllampcpserial::set_global_settings(int longpress_mul10)
{
	uint8_t data[] = {(uint8_t)longpress_mul10};
	send_cmd(SET_GLOBAL_SETTINGS, data, sizeof(data) / sizeof(data[0]));
}

void tllampcpserial::set_brightness(int value)
{
	uint8_t data[] = {(uint8_t)value};
	send_cmd(SET_BRIGHTNESS, data, sizeof(data) / sizeof(data[0]));
}

void tllampcpserial::set_color_temperature(int index)
{
	uint8_t data[] = {(uint8_t)index};
	send_cmd(SET_COLOR_TEMPERATURE, data, sizeof(data) / sizeof(data[0]));
}

void tllampcpserial::ctrl_led(int type, uint8_t value)
{
	VALIDATE(type >= 0 && type < aplt::tbase_ext_lamp::ledtype_count, null_str);
	VALIDATE(value == aplt::tbase_ext_lamp::ledact_on || value == aplt::tbase_ext_lamp::ledact_off, null_str);

	bool is_lamp = false;

	if (is_lamp) {
		uint8_t data[] = {type == aplt::tbase_ext_lamp::ledtype_scene? value: (uint8_t)aplt::tbase_ext_lamp::ledact_nothing, 
			type == aplt::tbase_ext_lamp::ledtype_privacy? value: (uint8_t)aplt::tbase_ext_lamp::ledact_nothing};
		send_cmd(CTRL_LED, data, sizeof(data) / sizeof(data[0]));

	} else {
		uint8_t data[] = {type == aplt::tbase_ext_lamp::ledtype_scene? value: (uint8_t)aplt::tbase_ext_lamp::ledact_nothing, 
			type == aplt::tbase_ext_lamp::ledtype_privacy? value: (uint8_t)aplt::tbase_ext_lamp::ledact_nothing,
			(uint8_t)aplt::tbase_ext_lamp::ledact_nothing, (uint8_t)aplt::tbase_ext_lamp::ledact_nothing};
		send_cmd(CTRL_LED, data, sizeof(data) / sizeof(data[0]));
	}
}

void tllampcpserial::set_button_threshold(int power, int non_power)
{
	uint8_t data[] = {(uint8_t)power, (uint8_t)non_power};
	send_cmd(SET_BUTTON_THRESHOLD, data, sizeof(data) / sizeof(data[0]));
}

tllampcpserial* llampcpserial_open(aplt::tbase_ext_lamp& ext_lamp, const std::string& serial_path, int baudrate)
{
	VALIDATE(!serial_path.empty(), null_str);
	VALIDATE(baudrate > 0, null_str);

	SDL_Log("{llampcpserial_start}want open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
	tllampcpserial* serial = new tllampcpserial(ext_lamp, serial_path, baudrate);
	if (!serial->valid()) {
		SDL_Log("{llampcpserial_start}cannot open serial, node: %s, baudrate: %i", serial_path.c_str(), baudrate);
		delete serial;
		return nullptr;
	}

    return serial;
}