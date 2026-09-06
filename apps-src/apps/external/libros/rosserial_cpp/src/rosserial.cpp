#include "rose_global.hpp"
#include "rosserial_cpp/rosserial_cpp.h"
#include <SDL.h>
#include <SDL_peripheral.h>
// #include "rose_config.hpp"
#include "rose_exception.hpp"

using namespace std::placeholders;

namespace ros {

trosserial::trosserial(const std::string& path, int baudrate, void* nh, bool delete_nh)
	: tserial(path, baudrate)
	, header_(0xff)
	, version_(0xfe) // 0xfe(Xiao R)  0xfb??
	, send_data_(nullptr)
	, send_data_size_(4096)
	, nh_(nh)
	, delete_nh_(delete_nh)
{
	VALIDATE(nh != nullptr, null_str);
	send_data_ = (uint8_t*)malloc(send_data_size_);
	set_did_read_topicid(std::bind(&ros::trosserial::did_read_rosserial, this, _1, _2, _3));
}

void trosserial::app_pool_read_bh()
{
	const int min_size = 8; // 1(header) + 1(version) + 3(message length+checksum) + 2(topic_id) + msg_data + 1(checksum)
	const int len_befor_topic = 5;
	const int topic_size = 2;
	const int len_befor_msg_data = len_befor_topic + topic_size;

	while (true) {
		uint16_t topicid = 0;
		int msg_len = 0;
		while (true) {
			// 1. must prefix with: header_ version_
			int skip = 0;
			for (int i = 0; i < recv_data_vsize_ - 1; i ++) {
				if (recv_data_[i] == header_ && recv_data_[i + 1] == version_) {
					break;
				}
				skip ++;
			}
			if (skip && skip != recv_data_vsize_) {
				memcpy(recv_data_, recv_data_ + skip, recv_data_vsize_ - skip);
			}
			recv_data_vsize_ -= skip;
			if (recv_data_vsize_ <= min_size) {
				// need more byte
				return;
			}

			// Read message length, checksum (3 bytes)
			const uint8_t* ptr = recv_data_;
			msg_len = posix_mku16(ptr[2], ptr[3]);
			int checksum = ptr[2];
			checksum += ptr[3];
			checksum += ptr[4];

			if ((checksum % 256) != 255) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}

			// Read topic id (2 bytes)
			topicid = posix_mku16(ptr[5], ptr[6]);

			if (recv_data_vsize_ < len_befor_msg_data + msg_len + 1) {
				// need more byte
				return;
			}
		
			// Reada checksum for topic id and msg
			const int checksum_at = len_befor_topic + 2 + msg_len;
			checksum = 0;
			for (int at = len_befor_topic; at <= checksum_at; at ++) {
				checksum += ptr[at];
			}
			if (checksum % 256 != 255) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}	
			break;
		}

		const int consumed = len_befor_msg_data + msg_len + 1;
		VALIDATE(recv_data_vsize_ >= consumed, null_str);

		if (did_read_topicid_) {
			did_read_topicid_(topicid, recv_data_ + len_befor_msg_data, msg_len);
		}

		if (recv_data_vsize_ > consumed) {
			memcpy(recv_data_, recv_data_ + consumed, recv_data_vsize_ - consumed);
		}
		recv_data_vsize_ -= consumed;

		if (recv_data_vsize_ == 0) {
			// SDL_Log("tmainserial read(1.2): there is no extra data");
		} else {
			// std::string str = rtc::hex_encode((const char*)recv_data_, recv_data_vsize_);
			// SDL_Log("tmainserial read(1.2): extra data: %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);
		}
	}
}

void trosserial::send_topic(uint16_t topic_id, const uint8_t* msg, int msg_len)
{
	// reference to: http://wiki.ros.org/rosserial/Overview/Protocol
	send_data_[0] = header_;
	send_data_[1] = version_;

	send_data_[2] = posix_lo8(msg_len);
	send_data_[3] = posix_hi8(msg_len);

	int sum = send_data_[2];
	sum += send_data_[3];
	send_data_[4] = 255 - (sum % 256);
	send_data_[5] = posix_lo8(topic_id);
	send_data_[6] = posix_hi8(topic_id);
	if (msg_len != 0) {
		memcpy(send_data_ + 7, msg, msg_len);
	}

	// checksum: 2(topic) + msg_len(msg)
	sum = send_data_[5];
	sum += send_data_[6];
	for (int n = 0; n < msg_len; n ++) {
		sum += msg[n];
	}
	send_data_[7 + msg_len] = 255 - (sum % 256);
	send_data(send_data_, 7 + msg_len + 1);
}

}