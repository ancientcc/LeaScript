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

#ifdef _WIN32
#if !defined(UNICODE)
#error "On windows, this file must complete Use Unicode Character Set"
#endif
#endif

#if defined(_WIN32) || defined(ANDROID) || defined(__APPLE__)
#include "freerdp_config.h"
#endif

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/channels/leagor_ble.hpp>

#include "wml_exception.hpp"
#include <SDL_log.h>
/*
tblebuf::~tblebuf()
{
	if (recv_data_) {
		free(recv_data_);
		recv_data_ = nullptr;
	}

	if (packet_data_) {
		free(packet_data_);
		packet_data_ = nullptr;
	}
}

void tblebuf::enqueue(const uint8_t* data, int len)
{
	VALIDATE(data != nullptr && len > 0, null_str);

	// SDL_Log("tblebuf::enqueue---len: %i, recv_data_vsize_: %i", len, recv_data_vsize_);

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
		SDL_Log("tblebuf::enqueue, cmd: %i, len: %i", cmd, len);

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
			SDL_Log("tblebuf::enqueue(1.2): there is no extra data");
		} else {
			std::string str = rtc::hex_encode((const char*)recv_data_, recv_data_vsize_);
			SDL_Log("tblebuf::enqueue(1.2): extra data: %s, recv_data_vsize_: %i", str.c_str(), recv_data_vsize_);
		}
	}
}
*/
void tblebuf::form_queryip_req(tuint8data_C& result)
{
	tlipdp_item* items = nullptr;
	int item_count = 1;
	tlipdp_items_lock lock(item_count, &items);
	int index = 0;
	int payload_len = 0;

	LIPDP_PUSH_ITEM_n8(LEAGOR_BLE_VERSION);

	VALIDATE(index == item_count, null_str);
	int packet_len = items_2_data(msg_queryip_req, payload_len, items, item_count);

	result.ptr = packet_data_;
	result.len = packet_len;
}

int tblebuf::parse_queryip_req(const uint8_t* data, int len)
{
	parser_.handle(data, len);
	if (parser_.count == 1 && parser_.items[0].type == lipdp_tn8) {
		return parser_.items[0].u8;
	}
	return nposm;
}

void tblebuf::form_queryip_resp(const tqueryip_resp& src, tuint8data_C& result)
{
	tlipdp_item* items = nullptr;
	int item_count = 3;
	tlipdp_items_lock lock(item_count, &items);
	int index = 0;
	int payload_len = 0;

	LIPDP_PUSH_ITEM_n8(LEAGOR_BLE_VERSION);
	if (src.af == LEAGOR_BLE_AF_INET) {
		LIPDP_PUSH_ITEM_ipv4(src.a.ipv4);

	} else if (src.af == LEAGOR_BLE_AF_ERR_VER || src.af == LEAGOR_BLE_AF_ERR_PRIVACY) {
		LIPDP_PUSH_ITEM_ipunspec(src.af);
	} else {
		VALIDATE(false, null_str);
	}
	LIPDP_PUSH_ITEM_n32(src.flags);

	VALIDATE(index == item_count, null_str);
	int packet_len = items_2_data(msg_queryip_resp, payload_len, items, item_count);

	result.ptr = packet_data_;
	result.len = packet_len;
}

tblebuf::tqueryip_resp tblebuf::parse_queryip_resp(const uint8_t* payload, int len)
{
	tqueryip_resp result;
	memset(&result, 0, sizeof(result));

	parser_.handle(payload, len);
	if (parser_.count == 3 && parser_.items[0].type == lipdp_tn8 && parser_.items[1].type == lipdp_tip &&
		parser_.items[2].type == lipdp_tn32) {
		result.version = parser_.items[0].u8;
		result.af = parser_.items[1].u8;
		if (result.af == LEAGOR_BLE_AF_INET) {
			result.a.ipv4 = parser_.items[1].int32;
		}
		result.flags = parser_.items[2].int32;
	}
	return result;
}

void tblebuf::form_updateip_req(const std::string& password, tuint8data_C& result)
{
	VALIDATE(!password.empty(), null_str);
	const int password_size = password.size();

	tlipdp_item* items = nullptr;
	int item_count = 1;
	tlipdp_items_lock lock(item_count, &items);
	int index = 0;
	int payload_len = 0;

	LIPDP_PUSH_ITEM_string(password.c_str(), password_size);

	VALIDATE(index == item_count, null_str);
	int packet_len = items_2_data(msg_updateip_req, payload_len, items, item_count);

	result.ptr = packet_data_;
	result.len = packet_len;
}

std::string tblebuf::parse_updateip_req(const uint8_t* data, int len)
{
	std::string password;
	parser_.handle(data, len);
	if (parser_.count == 1 && parser_.items[0].type == lipdp_tstring) {
		password.assign((const char*)parser_.items[0].data, parser_.items[0].int32);
	}

	return password;
}

void tblebuf::form_wifilist_req(tuint8data_C& result)
{
	int packet_len = items_2_data(msg_wifilist_req, 0, nullptr, 0);
	result.ptr = packet_data_;
	result.len = packet_len;
}

void tblebuf::form_connectwifi_req(const std::string& ssid, const std::string& password, tuint8data_C& result)
{
	VALIDATE(!ssid.empty() && !password.empty(), null_str);
	const int ssid_size = ssid.size();
	const int password_size = password.size();

	tlipdp_item* items = nullptr;
	int item_count = 2;
	tlipdp_items_lock lock(item_count, &items);
	int index = 0;
	int payload_len = 0;

	LIPDP_PUSH_ITEM_string(ssid.c_str(), ssid_size);
	LIPDP_PUSH_ITEM_string(password.c_str(), password_size);

	VALIDATE(index == item_count, null_str);
	int packet_len = items_2_data(msg_connectwifi_req, payload_len, items, item_count);

	result.ptr = packet_data_;
	result.len = packet_len;
}

std::string tblebuf::parse_connectwifi_req(const uint8_t* data, int len, std::string& password)
{
	std::string ssid;
	password.clear();

	parser_.handle(data, len);
	if (parser_.count == 2 && parser_.items[0].type == lipdp_tstring && parser_.items[1].type == lipdp_tstring) {
		ssid.assign((const char*)parser_.items[0].data, parser_.items[0].int32);
		password.assign((const char*)parser_.items[1].data, parser_.items[1].int32);
	}

	return ssid;
}

void tblebuf::form_removewifi_req(const std::string& ssid, tuint8data_C& result)
{
	VALIDATE(!ssid.empty(), null_str);
	const int ssid_size = ssid.size();

	tlipdp_item* items = nullptr;
	int item_count = 1;
	tlipdp_items_lock lock(item_count, &items);
	int index = 0;
	int payload_len = 0;

	LIPDP_PUSH_ITEM_string(ssid.c_str(), ssid_size);

	VALIDATE(index == item_count, null_str);
	int packet_len = items_2_data(msg_removewifi_req, payload_len, items, item_count);

	result.ptr = packet_data_;
	result.len = packet_len;
}

std::string tblebuf::parse_removewifi_req(const uint8_t* data, int len)
{
	std::string ssid;

	parser_.handle(data, len);
	if (parser_.count == 1 && parser_.items[0].type == lipdp_tstring) {
		ssid.assign((const char*)parser_.items[0].data, parser_.items[0].int32);
	}
	return ssid;
}

void tblebuf::form_error_resp(int cmd, int errcode, tuint8data_C& result)
{
	tlipdp_item* items = nullptr;
	int item_count = 2;
	tlipdp_items_lock lock(item_count, &items);
	int index = 0;
	int payload_len = 0;

	LIPDP_PUSH_ITEM_n32(cmd);
	LIPDP_PUSH_ITEM_n32(errcode);

	VALIDATE(index == item_count, null_str);
	int packet_len = items_2_data(msg_error_resp, payload_len, items, item_count);

	result.ptr = packet_data_;
	result.len = packet_len;
}

tblebuf::terror_resp tblebuf::parse_error_resp(const uint8_t* payload, int len)
{
	terror_resp result;
	memset(&result, 0, sizeof(result));

	result.cmd = nposm;

	parser_.handle(payload, len);
	if (parser_.count == 2 && parser_.items[0].type == lipdp_tn32 && parser_.items[1].type == lipdp_tn32) {
		result.cmd = parser_.items[0].int32;
		result.code = parser_.items[1].int32;
	}

	return result;
}

void tblebuf::form_wifilist_resp_ipv4(uint32_t ip, uint32_t flags, SDL_WifiScanResult* results, int count, tuint8data_C& out)
{
	// caller must sort results by rssi.
	const int max_wifiaps = 24;
	count = SDL_min(max_wifiaps, count);
	int item_count = 3 + 3 * count;

	tlipdp_item* items = nullptr;
	tlipdp_items_lock lock(item_count, &items);
	int index = 0;
	int payload_len = 0;
	LIPDP_PUSH_ITEM_ipv4(ip);
	LIPDP_PUSH_ITEM_n32(flags);
	LIPDP_PUSH_ITEM_n8(count);

	if (count > 0) {
		VALIDATE(results != nullptr, null_str);
		for (int at = 0; at < count; at ++) {
			const SDL_WifiScanResult& result = results[at];
			// 4(rssi) + 4(flags)
			LIPDP_PUSH_ITEM_n32(result.rssi);
			LIPDP_PUSH_ITEM_n32(result.flags);
			int ssid_len = SDL_strlen(result.ssid);
			LIPDP_PUSH_ITEM_string(result.ssid, ssid_len);
		}
	}
	VALIDATE(index == item_count, null_str);
	int packet_len = items_2_data(msg_wifilist_resp, payload_len, items, item_count);

	if (results != nullptr) {
		SDL_free(results);
	}

	out.ptr = packet_data_;
	out.len = packet_len;
}