/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Server Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_LEAGOR_BLE_H
#define FREERDP_CHANNEL_LEAGOR_BLE_H

#include <freerdp/channels/leagor_common_context.h>
#include <string>
#include <set>

#include <SDL_peripheral.h>

#include "lipdp.hpp"

enum {msg_queryip_req = 1, 
	msg_updateip_req,
	msg_connectwifi_req,
	msg_removewifi_req,
	msg_wifilist_req,

	msg_queryip_resp = 50,
	msg_wifilist_resp,
	msg_error_resp
};

enum {
	leagorerr_versiondismatch = -1,
	leagorerr_blepassword = -2,
	leagorerr_privacyprotect = -3
};

enum {
	LEAGOR_BLE_FLAG_WIFI_ENABLED = 0x1,
};

class tblebuf: public tlipdp_packer, public tlipdp_receiver
{
public:
	tblebuf(bool center)
		// : recv_data_(nullptr)
		// , recv_data_size_(0)
		// , recv_data_vsize_(0)
	{
		if (center) {
			// use as ble center
			recv_cmds_.insert(msg_queryip_resp);
			recv_cmds_.insert(msg_wifilist_resp);
			recv_cmds_.insert(msg_error_resp);

		} else {
			// use as ble peripheral
			recv_cmds_.insert(msg_queryip_req);
			recv_cmds_.insert(msg_updateip_req);
			recv_cmds_.insert(msg_connectwifi_req);
			recv_cmds_.insert(msg_removewifi_req);
			recv_cmds_.insert(msg_wifilist_req);
		}
	}
	// ~tblebuf();
/*
	void set_did_read(const std::function<void (int cmd, const uint8_t* data, int len)>& did)
	{
		did_read_ = did;
	}
	void enqueue(const uint8_t* data, int len);
*/
	void form_queryip_req(tuint8data_C& result);
	int parse_queryip_req(const uint8_t* data, int len);

	struct tqueryip_resp {
		int version;
		int af;
		union {
			uint32_t ipv4;
			uint8_t ipv6[16];
		} a;
		uint32_t flags;
	};
	void form_queryip_resp(const tqueryip_resp& src, tuint8data_C& result);
	tqueryip_resp parse_queryip_resp(const uint8_t* payload, int len);

	void form_updateip_req(const std::string& password, tuint8data_C& result);
	std::string parse_updateip_req(const uint8_t* data, int len);

	void form_wifilist_req(tuint8data_C& result);
	void form_wifilist_resp_ipv4(uint32_t ip, uint32_t flags, SDL_WifiScanResult* results, int count, tuint8data_C& out);

	void form_connectwifi_req(const std::string& ssid, const std::string& password, tuint8data_C& result);
	std::string parse_connectwifi_req(const uint8_t* data, int len, std::string& password);

	void form_removewifi_req(const std::string& ssid, tuint8data_C& result);
	std::string parse_removewifi_req(const uint8_t* data, int len);

	void form_error_resp(int cmd, int errcode, tuint8data_C& result);
	struct terror_resp {
		int cmd;
		int code;
	};
	terror_resp parse_error_resp(const uint8_t* payload, int len);

private:
	// void resize_recv_data(int size);

protected:
	// uint8_t* recv_data_;
	// int recv_data_size_;
	// int recv_data_vsize_;
	// std::function<void (int cmd, const uint8_t* data, int len)> did_read_;

	// std::set<int> recv_cmds_;
	// std::set<int> send_cmds_;

	tlipdp_parser parser_;
};

#endif /* FREERDP_CHANNEL_LEAGOR_BLE_H */
