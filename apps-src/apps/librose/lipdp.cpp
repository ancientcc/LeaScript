/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Clipboard Redirection
 *
 * Copyright 2012 Jason Champion
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define GETTEXT_DOMAIN "rose-lib"

#include "lipdp.hpp"
#include "wml_exception.hpp"
#include <SDL_log.h>


tlipdp_items_lock::tlipdp_items_lock(int count, tlipdp_item** ppitems)
	: items_(nullptr)
{
	VALIDATE(count > 0 && ppitems != nullptr, null_str);
	int size = sizeof(tlipdp_item) * count;
	items_ = (tlipdp_item*)malloc(size);
	memset(items_, 0, size);
	*ppitems = items_;
}

tlipdp_packer::~tlipdp_packer()
{
	if (packet_data_ != nullptr) {
		free(packet_data_);
		packet_data_ = nullptr;
	}
}

void tlipdp_packer::resize_packet_data(int size)
{
	const int min_size = posix_align_ceil(size, 4096);

	if (min_size > packet_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(min_size);
		if (packet_data_) {
			free(packet_data_);
		}
		packet_data_ = tmp;
		packet_data_size_ = min_size;
	}
}

uint8_t* tlipdp_packer::fill_mtu_4bytes(uint8_t* data, int cmd, int payload_len)
{
	data[0] = LEAGOR_BLE_PREFIX_BYTE;
	data[1] = cmd;
	data[2] = posix_lo8(posix_lo16(payload_len));
	data[3] = posix_hi8(posix_lo16(payload_len));

	return data + 4;
}

int tlipdp_packer::items_2_data(int cmd, int payload_len, const tlipdp_item* items, int count, uint8_t* caller_packet_data)
{
	const int packet_len = LEAGOR_BLE_MTU_HEADER_SIZE + payload_len;

	uint8_t* packet_data = nullptr;
	if (caller_packet_data == nullptr) {
		// use tlipdp_pakcer's packet data buffer
		resize_packet_data(packet_len);
		packet_data = packet_data_;
	} else {
		// use caller packet data buffer
		packet_data = caller_packet_data;
	}

	uint8_t* wt_ptr = fill_mtu_4bytes(packet_data, cmd, payload_len);
	for (int at = 0; at < count; at ++) {
		const tlipdp_item& item = items[at];
		if (item.type != lipdp_thexstring) {
			wt_ptr[0] = item.type;
		} else {
			// save to memory: lipdp_thexstring => lipdp_tbinary
			wt_ptr[0] = lipdp_tbinary;
		}
		wt_ptr ++;
		if (item.type == lipdp_tn8) {
			wt_ptr[0] = item.u8;
			wt_ptr ++;

		} else if (item.type == lipdp_tn32) {
			memcpy(wt_ptr, &item.int32, 4);
			wt_ptr += 4;

		} else if (item.type == lipdp_tn64) {
			memcpy(wt_ptr, &item.int64, 8);
			wt_ptr += 8;

		} else if (item.type == lipdp_tstring) {
			if (item.int32 != 0) {
				memcpy(wt_ptr, item.data, item.int32);
				wt_ptr += item.int32;
			}
			wt_ptr[0] = '\0';
			wt_ptr ++;

		} else if (item.type == lipdp_thexstring) {
			uint16_t hsize = 0;
			if (item.int32 != 0) {
				hsize = utils::hex_decode_prealloc((const char*)item.data, item.int32, wt_ptr + 2);
			}
			wt_ptr[0] = posix_lo8(hsize);
			wt_ptr[1] = posix_hi8(hsize);
			wt_ptr += 2 + hsize; // 2 + item.int32 / 2;

		} else if (item.type == lipdp_tbinary) {
			wt_ptr[0] = posix_lo8(posix_lo16(item.int32));
			wt_ptr[1] = posix_hi8(posix_lo16(item.int32));
			wt_ptr += 2;
			if (item.int32 != 0) {
				memcpy(wt_ptr, item.data, item.int32);
				wt_ptr += item.int32;
			}

		} else if (item.type == lipdp_tip) {
			wt_ptr[0] = item.u8;
			wt_ptr ++;
			if (item.u8 == LEAGOR_BLE_AF_INET) {
				// ipv4
				memcpy(wt_ptr, &item.int32, 4);
				wt_ptr += 4;

			} else if (item.u8 == LEAGOR_BLE_AF_INET6) {
				// ipv6
				memcpy(wt_ptr, item.data, 16);
				wt_ptr += 16;

			} else if (item.u8 == LEAGOR_BLE_AF_ERR_VER || item.u8 == LEAGOR_BLE_AF_ERR_PRIVACY) {
				// unspec

			} else {
				VALIDATE(false, null_str);
			}
		}
	}
	VALIDATE((int)(wt_ptr - packet_data) == packet_len, null_str);
	return packet_len;
}

void tlipdp_parser::expand_items(int vcount)
{
	const int desire_count = item_count_ + items_per_alloc_;

	tlipdp_item* tmp = (tlipdp_item*)malloc(sizeof(tlipdp_item) * desire_count);
	if (items != nullptr) {
		if (vcount != 0) {
			memcpy(tmp, items, sizeof(tlipdp_item) * vcount);
		}
		free(items);
	}
	items = tmp;
	item_count_ = desire_count;
}

int tlipdp_parser::handle(const uint8_t* data, int len)
{
	VALIDATE(len >= 0, null_str);
	// allow len is 0. if len == 0:
	//   items = nullptr;
	//   count = 0;
	//   fail = false;
	
	// reset
	fail = false;
	count = 0;

	// it is payload only, so data doesn't contain LEAGOR_BLE_MTU_HEADER.
	int32_t n32;
	int64_t n64;

	int rd_idx = 0;
	int index = 0;
	int payload_len = 0;
	while (rd_idx < len) {
		if (item_count_ == index) {
			expand_items(index);
		}
		int type = data[rd_idx];
		rd_idx ++; // [0]: type
		if (type == lipdp_tn8) {
			rd_idx ++;
			if (rd_idx > len) {
				fail = true;
				break;
			}
			LIPDP_PUSH_ITEM_n8(data[payload_len + 1]);

		} else if (type == lipdp_tn32) {
			rd_idx += 4;
			if (rd_idx > len) {
				fail = true;
				break;
			}
			memcpy(&n32, data + payload_len + 1, 4);
			LIPDP_PUSH_ITEM_n32(n32);

		} else if (type == lipdp_tn64) {
			rd_idx += 8;
			if (rd_idx > len) {
				fail = true;
				break;
			}
			memcpy(&n64, data + payload_len + 1, 8);
			LIPDP_PUSH_ITEM_n64(n64);

		} else if (type == lipdp_tstring) {
			for (int at = 0; rd_idx < len; rd_idx ++, at ++) {
				if (data[rd_idx] == '\0') {
					const uint8_t* c_str = at != 0? data + payload_len + 1: nullptr;
					LIPDP_PUSH_ITEM_string(c_str, at);
					break;
				}
			}
			if (rd_idx == len) {
				fail = true;
				break;
			}
			rd_idx ++;

		} else if (type == lipdp_tbinary) {
			if (rd_idx + 2 > len) {
				fail = true;
				break;
			}
			const int binary_len = posix_mku16(data[rd_idx], data[rd_idx + 1]);
			rd_idx += 2 + binary_len;
			if (rd_idx > len) {
				fail = true;
				break;
			}
			// 3: type, binary_len(2)
			const uint8_t* c_str = binary_len != 0? data + payload_len + 3: nullptr;
			LIPDP_PUSH_ITEM_binary(c_str, binary_len);

		} else if (type == lipdp_tip) {
			if (rd_idx + 1 > len) {
				fail = true;
				break;
			}
			int af = data[rd_idx];
			rd_idx ++; // [0]: type
			if (af == LEAGOR_BLE_AF_INET) {
				// ipv4
				rd_idx += 4;
				if (rd_idx > len) {
					fail = true;
					break;
				}
				// 2: type, af
				memcpy(&n32, data + payload_len + 2, 4);
				LIPDP_PUSH_ITEM_ipv4(n32);

			} else if (af == LEAGOR_BLE_AF_INET6) {
				// ipv6
				rd_idx += 16;
				if (rd_idx > len) {
					fail = true;
					break;
				}
				// 2: type, af
				LIPDP_PUSH_ITEM_ipv6(data + payload_len + 2);

			} else if (af == LEAGOR_BLE_AF_ERR_VER || af == LEAGOR_BLE_AF_ERR_PRIVACY) {
				// unspec
				LIPDP_PUSH_ITEM_ipunspec(af);

			} else {
				fail = true;
				break;
			}
		} else {
			// lipdp_thexstring or unknown type
			fail = true;
			break;
		}
	}

	if (!fail) {
		VALIDATE(payload_len == rd_idx, null_str);
		VALIDATE(rd_idx == len, null_str);
		count = index;
	}
	return count;
}

tlipdp_receiver::~tlipdp_receiver()
{
	if (recv_data_ != nullptr) {
		free(recv_data_);
		recv_data_ = nullptr;
	}
}

void tlipdp_receiver::enqueue(const uint8_t* data, int len)
{
	VALIDATE(data != nullptr && len > 0, null_str);

	// SDL_Log("tlipdp_receiver::enqueue---len: %i, recv_data_vsize_: %i", len, recv_data_vsize_);

	const int min_size = posix_align_ceil(recv_data_vsize_ + len, 4096);

	if (min_size > recv_data_size_) {
		uint8_t* tmp = (uint8_t*)malloc(min_size);
		if (recv_data_) {
			if (recv_data_vsize_) {
				memcpy(tmp, recv_data_, recv_data_vsize_);
			}
			free(recv_data_);
		}
		recv_data_ = tmp;
		recv_data_size_ = min_size;
	}

	memcpy(recv_data_ + recv_data_vsize_, data, len);
	recv_data_vsize_ += len;

	while (true) {
		while (true) {
			// 1. must prefix with LEAGOR_BLE_PREFIX_BYTE
			int skip = 0;
			for (int i = 0; i < recv_data_vsize_; i ++) {
				if (recv_data_[i] == LEAGOR_BLE_PREFIX_BYTE) {
					break;
				}
				skip ++;
			}
			if (skip && skip != recv_data_vsize_) {
				memcpy(recv_data_, recv_data_ + skip, recv_data_vsize_ - skip);
			}
			recv_data_vsize_ -= skip;
			if (recv_data_vsize_ < LEAGOR_BLE_MTU_HEADER_SIZE) { // payload len maybe is 0.
				return;
			}
	
			// 2. len
			const int cmd = recv_data_[1];
			if (recv_cmds_.count(cmd) == 0) {
				// skip first. then again.
				memcpy(recv_data_, recv_data_ + 1, recv_data_vsize_ - 1);
				recv_data_vsize_ --;
				continue;
			}
			const int len = posix_mku16(recv_data_[2], recv_data_[3]);
			if (recv_data_vsize_ < LEAGOR_BLE_MTU_HEADER_SIZE + len) {
				return;
			}
			break;
		}

		const int cmd = recv_data_[1];
		const int len = posix_mku16(recv_data_[2], recv_data_[3]);
		SDL_Log("tlipdp_receiver::enqueue, cmd: %i, len: %i", cmd, len);

		const int len2 = LEAGOR_BLE_MTU_HEADER_SIZE + len;
		VALIDATE(recv_data_vsize_ >= len2, null_str);

		if (did_read_) {
			did_read_(cmd, recv_data_ + LEAGOR_BLE_MTU_HEADER_SIZE, len);
		}

		if (recv_data_vsize_ > len2) {
			memcpy(recv_data_, recv_data_ + len2, recv_data_vsize_ - len2);
		}
		recv_data_vsize_ -= len2;

		if (recv_data_vsize_ == 0) {
			SDL_Log("tlipdp_receiver::enqueue(1.2): there is no extra data");
		} else {
			std::string str = rtc::hex_encode((const char*)recv_data_, recv_data_vsize_);
			SDL_Log("tlipdp_receiver::enqueue(1.2): extra data: %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);
		}
	}
}

bool leagor_verify_blepassword(const std::string& label)
{
	int size = label.size();
	if (size < MIN_BLEPASSWORD_SIZE || size > MAX_BLEPASSWORD_SIZE) {
		return false;
	}
	const char* c_str = label.c_str();

	for (int at = 0; at < size; at ++) {
		char ch = c_str[at];
		if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
		} else {
			return false;
		}
	}
	return true;
}