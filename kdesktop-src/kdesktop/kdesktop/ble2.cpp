/* $Id: title_screen.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "kdesktop-lib"

#include "ble2.hpp"

#include <time.h>
#include "gettext.hpp"
#include "help.hpp"
#include "filesystem.hpp"
#include "sound.hpp"
#include "wml_exception.hpp"

#include <iomanip>
using namespace std::placeholders;

#include <algorithm>

#include "base_instance.hpp"

#define MAX_RESERVE_TEMPS		6
#define RESERVE_FILE_DATA_LEN   (sizeof(ttemperature) * MAX_RESERVE_TEMPS)

#define THRESHOLD_HDERR_REF		5
#define RESISTANCE_OUTRANDE		UINT16_MAX

const char* tble2::uuid_rdpd_service = "5356";
const char* tble2::uuid_write_characteristic = "fd01";
const char* tble2::uuid_notify_characteristic = "fd03";

bool tble2::tether3elem::valid() const 
{
	return ipv4 != 0 && prefixlen >= 1 && prefixlen <= 31 && gateway != 0 && utils::is_same_net(ipv4, gateway, prefixlen);
}

bool tble2::is_discovery_name(const SDL_BlePeripheral& peripheral)
{
	if (peripheral.manufacturer_data_len < 2) {
		return false;
	}
	const uint16_t launcher_manufacturer_id_ = 65520; // 0xfff0 ==> (xmit)f0 ff
	if (peripheral.manufacturer_data[0] != posix_lo8(launcher_manufacturer_id_) || peripheral.manufacturer_data[1] != posix_hi8(launcher_manufacturer_id_)) {
		return false;
	}

	if (peripheral.uuid == nullptr) {
		return false;
	}

	SDL_Log("is_discovery_name, peripheral.uuid: %s uuid_rdpd_service: %s", peripheral.uuid, uuid_rdpd_service);
	if (!SDL_BleUuidEqual(peripheral.uuid, uuid_rdpd_service)) {
		return false;
	}
	return true;
/*
	const std::string lower_name = utils::lowercase(peripheral.name);
	if (lower_name.empty()) {
		return false;
	}

	// peripehral maybe change name fail.
	std::set<std::string> maybe_names;
	maybe_names.insert("rdpd");
	maybe_names.insert("rk3399"); // aio3399j orignal name
	maybe_names.insert("rk3588"); // roc-rk3588s-pc orignal name
	return maybe_names.count(lower_name) != 0;
*/
}

tble2::tble2(base_instance* instance)
	: tble(instance, connector_, true)
	, status_(nposm)
	, plugin_(NULL)
	, buf_(true)
{
	buf_.set_did_read(std::bind(&tble2::did_read_ble, this, _1, _2, _3));

	{
		ttask& task = insert_task(taskid_postready);
		// step0: notify read
		task.insert(nposm, uuid_rdpd_service, uuid_notify_characteristic, operator_notify);
	}

	{
		ttask& task = insert_task(taskid_queryip);
		// step0: set time
		task.insert(nposm, uuid_rdpd_service, uuid_write_characteristic, operator_write);
	}

	{
		ttask& task = insert_task(taskid_updateip);
		// step0: set time
		task.insert(nposm, uuid_rdpd_service, uuid_write_characteristic, operator_write);
	}

	{
		ttask& task = insert_task(taskid_connectwifi);
		// step0: set time
		task.insert(nposm, uuid_rdpd_service, uuid_write_characteristic, operator_write);
	}

	{
		ttask& task = insert_task(taskid_removewifi);
		// step0: set time
		task.insert(nposm, uuid_rdpd_service, uuid_write_characteristic, operator_write);
	}

	{
		ttask& task = insert_task(taskid_refreshwifilist);
		// step0: set time
		task.insert(nposm, uuid_rdpd_service, uuid_write_characteristic, operator_write);
	}
}

tble2::~tble2()
{
}

void tble2::set_blepassword(const std::string& password)
{
	VALIDATE(!password.empty(), null_str);
	blepassword_ = password;
}

void tble2::disconnect_with_disable_reconnect()
{
	bg_scan_uuid_.clear();
	connector_.clear();
	disconnect_peripheral();
}

void tble2::connect_wifi(const std::string& ssid, const std::string& password)
{
	VALIDATE(!ssid.empty() && !password.empty(), null_str);
	VALIDATE(status_ == status_updateip, null_str);

	tuint8data_C data;
	buf_.form_connectwifi_req(ssid, password, data);
	if (game_config::os == os_windows) {
		std::string password2;
		const std::string ssid2 = buf_.parse_connectwifi_req(data.ptr + LEAGOR_BLE_MTU_HEADER_SIZE, data.len - LEAGOR_BLE_MTU_HEADER_SIZE, password2);
		VALIDATE(ssid == ssid2 && password == password2, null_str);
	}

	tble::ttask& task = get_task(taskid_connectwifi);
	tstep& step = task.get_step(0);
	step.set_data(data.ptr, data.len);
	task.execute(*this);
}

void tble2::remove_wifi(const std::string& ssid)
{
	VALIDATE(!ssid.empty(), null_str);
	VALIDATE(status_ == status_updateip, null_str);

	tuint8data_C data;
	buf_.form_removewifi_req(ssid, data);
	if (game_config::os == os_windows) {
		const std::string ssid2 = buf_.parse_removewifi_req(data.ptr + LEAGOR_BLE_MTU_HEADER_SIZE, data.len - LEAGOR_BLE_MTU_HEADER_SIZE);
		VALIDATE(ssid == ssid2, null_str);
	}

	tble::ttask& task = get_task(taskid_removewifi);
	tstep& step = task.get_step(0);
	step.set_data(data.ptr, data.len);
	task.execute(*this);
}

void tble2::refresh_wifi()
{
	VALIDATE(status_ == status_updateip, null_str);

	tuint8data_C data;
	buf_.form_wifilist_req(data);

	tble::ttask& task = get_task(taskid_refreshwifilist);
	tstep& step = task.get_step(0);
	step.set_data(data.ptr, data.len);
	task.execute(*this);
}

#include <openssl/sha.h>
#include <openssl/mem.h>

std::string sha1_data(const uint8_t* data, int len)
{
	VALIDATE(data != nullptr, null_str);
	VALIDATE(len > 0, null_str);

	uint8_t md[SHA_DIGEST_LENGTH];
	memset(md, 0, sizeof(md));
	SHA_CTX ctx;
	SHA1_Init(&ctx);

	SHA1_Update(&ctx, data, len);

	SHA1_Final(md, &ctx);
	OPENSSL_cleanse(&ctx, sizeof(ctx));

	return std::string((const char*)md, SHA_DIGEST_LENGTH);
}

void tble2::app_calculate_mac_addr(SDL_BlePeripheral& peripheral)
{
	uint64_t plaintext = (uint64_t)&peripheral;

	const std::string sha1text = sha1_data((const uint8_t*)&plaintext, sizeof(plaintext));
    const char* c_str = sha1text.c_str();

	for (int at = 0; at < 6; at ++) {
		peripheral.mac_addr[at] = c_str[at];
	}

/*
    if (peripheral.manufacturer_data != nullptr) {
		VALIDATE(peripheral.manufacturer_data_len > 0, null_str);

		const std::string sha1text = sha1_data(peripheral.manufacturer_data, peripheral.manufacturer_data_len);
        const char* c_str = sha1text.c_str();

		for (int at = 0; at < 6; at ++) {
			peripheral.mac_addr[at] = c_str[at];
		}
        
    } else {
        peripheral.mac_addr[0] = '\0';
    }
*/
}

void tble2::app_discover_peripheral(SDL_BlePeripheral& peripheral)
{
	if (plugin_) {
		plugin_->did_discover_peripheral(peripheral);
	}
}

void tble2::app_release_peripheral(SDL_BlePeripheral& peripheral)
{
	if (plugin_) {
		plugin_->did_release_peripheral(peripheral);
	}
}

bool tble2::app_is_right_services()
{
	// even thouth this peripheral has 3 service, but for ios, only can discover 1 service.
	// if (peripheral_->valid_services != 3) {
	if (peripheral_->valid_services == 0) {
		return false;
	}
	const SDL_BleService* my_service = nullptr;
	for (int n = 0; n < peripheral_->valid_services; n ++) {
		const SDL_BleService& service = peripheral_->services[n];
		if (SDL_BleUuidEqual(service.uuid, uuid_rdpd_service)) {
			my_service = &service;
		}
	}
	if (my_service == nullptr) {
		return false;
	}
	if (my_service->valid_characteristics != 3) {
		return false;
	}

	std::vector<std::string> chars;
	chars.push_back(uuid_write_characteristic);
	chars.push_back(uuid_notify_characteristic);
	for (std::vector<std::string>::const_iterator it = chars.begin(); it != chars.end(); ++ it) {
		int n = 0;
		for (; n < my_service->valid_characteristics; n ++) {
			const SDL_BleCharacteristic& char2 = my_service->characteristics[n];
			if (SDL_BleUuidEqual(char2.uuid, it->c_str())) {
				break;
			}
		}
		if (n == my_service->valid_characteristics) {
			return false;
		}
	}

	return true;
}

void tble2::app_connect_peripheral(SDL_BlePeripheral& peripheral, const int error)
{
    if (!error) {
		SDL_Log("tble2::app_connect_peripheral, will execute task#%i", taskid_postready);
		tble::ttask& task = get_task(taskid_postready);
		task.execute(*this);
    }

	if (plugin_) {
		plugin_->did_connect_peripheral(peripheral, error);
	}
}

void tble2::app_disconnect_peripheral(SDL_BlePeripheral& peripheral, const int error)
{
	if (plugin_) {
		plugin_->did_disconnect_peripheral(peripheral, error);
	}
}

void tble2::app_discover_characteristics(SDL_BlePeripheral& peripheral, SDL_BleService& service, const int error)
{
	if (plugin_) {
		plugin_->did_discover_characteristics(peripheral, service, error);
	}
}

void tble2::app_read_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const unsigned char* data, int len)
{
	VALIDATE(peripheral_ == &peripheral, null_str);
	if (SDL_BleUuidEqual(characteristic.uuid, uuid_notify_characteristic) && plugin_ != nullptr) {
		buf_.enqueue(data, len);
	}
}

void tble2::did_read_ble(int cmd, const uint8_t* data, int len)
{
	// std::string str = rtc::hex_encode((const char*)data, len);
	// SDL_Log("%u tble2::did_read_ble, %s", SDL_GetTicks(), str.c_str());

	SDL_BlePeripheral& peripheral = *peripheral_;
	if (cmd == msg_queryip_resp) {
		tblebuf::tqueryip_resp resp = buf_.parse_queryip_resp(data, len);
		plugin_->did_query_ip(peripheral, resp);

	} else if (cmd == msg_wifilist_resp) {
		if (status_ == status_updateip) {
			plugin_->did_recv_wifilist(peripheral, data, len);
		}

	} else if (cmd == msg_error_resp) {
		if (status_ == status_updateip) {
			tblebuf::terror_resp resp = buf_.parse_error_resp(data, len);
			plugin_->did_recv_error(peripheral, resp);
		}

	} 
}

bool tble2::app_task_callback(ttask& task, int step_at, bool start)
{
	SDL_Log("tble2::app_task_callback--- task#%i, step_at: %i, %s, current_step_: %i", task.id(), step_at, start? "prefix": "postfix", current_step_);

	if (start) {
		if (task.id() == taskid_queryip) {
			if (step_at == 0) {
				tstep& step = task.get_step(0);
				tuint8data_C data;
				buf_.form_queryip_req(data);
				step.set_data(data.ptr, data.len);
				if (game_config::os == os_windows) {
					int client_version = buf_.parse_queryip_req(data.ptr + LEAGOR_BLE_MTU_HEADER_SIZE, data.len - LEAGOR_BLE_MTU_HEADER_SIZE);
					VALIDATE(client_version == LEAGOR_BLE_VERSION, null_str);

					tblebuf::tqueryip_resp src;
					memset(&src, 0, sizeof(src));
					src.version = client_version;
					src.af = LEAGOR_BLE_AF_INET;
					if (src.af == LEAGOR_BLE_AF_INET) {
						src.a.ipv4 = 0x7301a8c0; // 0x7301a8c0 => 192.168.1.115;
					} else if (src.af == LEAGOR_BLE_AF_ERR_VER || src.af == LEAGOR_BLE_AF_ERR_PRIVACY) {
					}
					src.flags = LEAGOR_BLE_FLAG_WIFI_ENABLED;

					tuint8data_C resp;
					buf_.form_queryip_resp(src, resp);

					std::string str = rtc::hex_encode((const char*)resp.ptr, resp.len);
					SDL_Log("for debug msg_queryip_req/msg_queryip_resp:str(%i): %s", resp.len, str.c_str());
					tblebuf::tqueryip_resp src2 = buf_.parse_queryip_resp(resp.ptr + LEAGOR_BLE_MTU_HEADER_SIZE, resp.len - LEAGOR_BLE_MTU_HEADER_SIZE);
					VALIDATE(memcmp(&src, &src2, sizeof(src)) == 0, null_str);

					buf_.form_error_resp(msg_queryip_req, leagorerr_versiondismatch, resp);
					tblebuf::terror_resp error_resp = buf_.parse_error_resp(resp.ptr + LEAGOR_BLE_MTU_HEADER_SIZE, resp.len - LEAGOR_BLE_MTU_HEADER_SIZE);
					VALIDATE(error_resp.cmd == msg_queryip_req && error_resp.code == leagorerr_versiondismatch, null_str);
				}
			}
		} else if (task.id() == taskid_updateip) {
			if (step_at == 0) {
				tstep& step = task.get_step(0);

				tuint8data_C data;
				buf_.form_updateip_req(blepassword_, data);
				step.set_data(data.ptr, data.len);

				if (game_config::os == os_windows) {
					const std::string password2 = buf_.parse_updateip_req(data.ptr + LEAGOR_BLE_MTU_HEADER_SIZE, data.len - LEAGOR_BLE_MTU_HEADER_SIZE);
					VALIDATE(blepassword_ == password2, null_str);
				}
			}
		}
	} else {
		if (task.id() == taskid_postready) {
			if (step_at == (int)(task.steps().size() - 1)) {
				if (plugin_ != nullptr) {
					plugin_->did_start_queryip(*peripheral_);
				}

				int taskid = nposm;
				if (status_ == status_queryip) {
					taskid = taskid_queryip;
				} else if (status_ == status_updateip) {
					taskid = taskid_updateip;
				} else {
					VALIDATE(false, null_str);
				}
				
				tble::ttask& task = get_task(taskid);
				task.execute(*this);
			}
		}
	}
	return true;
}




