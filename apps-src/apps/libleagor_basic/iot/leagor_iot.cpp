/* $Id: dialog.cpp 50956 2011-08-30 19:41:22Z mordante $ */
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

#define GETTEXT_DOMAIN "aplt_leagor_basic-lib"

#include "leagor_iot.hpp"
#include "gettext.hpp"
#include "rose_config_3rdparty.hpp"
#include <SDL_log.h>
#include <SDL_timer.h>

#include "aplt_common.hpp"
#include "rose_ros/utils.hpp"

#include <openssl/sha.h>
#include <inttypes.h>
#include "../common.hpp"

using namespace std::placeholders;


namespace aplt {

tleagor_iot::tleagor_iot(aplt::tslot_subscriber& subscriber)
	: tiot_slot(subscriber)
	, fake_deivceid_("fake")
	, pool_event_interval_s_(10)
	, next_get_access_token_ticks_(0)
	, next_get_doorbell_event_ticks_(0)
	, last_start_time_(nposm)
{
	my_iot_srcs_.insert(std::make_pair(iot_src_doorbell, tiot_src(iot_src_doorbell, "misc/doorbell.png")));
	my_iot_srcs_.insert(std::make_pair(iot_src_doorcontact, tiot_src(iot_src_doorcontact, "misc/doorcontact.png")));
	my_iot_srcs_.insert(std::make_pair(iot_src_ir_motion_sensor, tiot_src(iot_src_ir_motion_sensor, "misc/ir_motion_sensor.png")));
	my_iot_srcs_.insert(std::make_pair(iot_src_smoke_sensor, tiot_src(iot_src_smoke_sensor, "misc/smoke_sensor.png")));

	reload_device_ids();
}

void tleagor_iot::reload_device_ids()
{
	VALIDATE_IN_MAIN_THREAD();
	device_ids_.clear();

	trose_prefs& aplt_prefs = aplt::curr_aplt->prefs;

	client_id_ = aplt_prefs.get_str("tuya_client_id");
	client_secret_ = aplt_prefs.get_str("tuya_client_secret");

	int max_count = 3;

	std::vector<std::string> v_str;
	std::set<std::string> existed_ids;
	char key_buf[32];
	for (int at = 0; at < max_count; at ++) {
		SDL_snprintf(key_buf, sizeof(key_buf), "iot_deviceid%i", at + 1);
		const std::string id2 = aplt_prefs.get_str(key_buf);
		if (id2.empty()) {
			continue;
		}
		v_str = utils::split(id2, ',');
		if (v_str.size() != 2) {
			continue;
		}
		const std::string& id = v_str[1];
		VALIDATE(!id.empty(), null_str);
		if (existed_ids.count(id) != 0) {
			continue;
		}
		existed_ids.insert(id);

		int src = utils::to_int(v_str[0]);
		if (my_iot_srcs_.count(src) == 0) {
			src = my_iot_srcs_.begin()->second.src;
		}
		device_ids_.push_back(tdevice2(id, src));
	}
}

static std::string generic_handle_response(bool success, Json::Value& json_object, bool& handled)
{
	std::stringstream err;
	utils::string_map symbols;
	
	handled = false;

	if (!success) {
		// cmd_get_doorbell_event & error access_token
		//   {"code":1004,"msg":"sign invalid","success":false,"t":1722080070806,"tid":"3074352c4c0c11efb454526982c4e3fa"}
		//   {"code":1010,"msg":"token invalid","success":false,"t":1721545820202,"tid":"4a4599d8473011efb3aa5674a4074f57"}
		//   {"code":28841002,"msg":"No permissions. Your subscription to cloud development plan has expired.","success":false,"t":1722041365459,"tid":"123b1ff74bb211ef8fb702cdc153b4b1"}
		int code = json_object["code"].asInt();
		const std::string msg = json_object["msg"].asString();
		err << "Fail. msg: " << msg << "(" << code << ")";

		handled = true;

	} else {
		Json::Value& result = json_object["result"];
		if (!result.isObject()) {
			err << "Fail. no 'result', or isn't an object";
			handled = true;
		}
	}

	return err.str();
}

std::string calcSign(const std::string& clientId, const std::string& accessToken, int64_t timestamp, const std::string& nonce, 
	const std::string& signStr, const std::string& secret)
{
	// var str = clientId + accessToken + timestamp + nonce + signStr;
	const std::string str = clientId + accessToken + str_cast(timestamp) + nonce + signStr;

	// const std::string str2 = "4wwuer5u3sa4ycsep4a832054bc122f69af3e8a260770989b5461721194427480GET\ne3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n\n/v1.0/devices/70482087ec94cb835c89";
	// VALIDATE(str == str2, null_str);

	// var hash = CryptoJS.HmacSHA256(str, secret);
	uint8_t szDigest[EVP_MAX_MD_SIZE];
	int nLenDigest = utils::hmac_md_sha(EVP_sha256(), (const uint8_t*)secret.c_str(), secret.size(), (const uint8_t*)str.c_str(), str.size(), szDigest);
	VALIDATE(nLenDigest == SHA256_DIGEST_LENGTH, null_str);

	// var hashInBase64 = hash.toString();
	const std::string hashInBase64 = utils::hex_encode_cstyle((const char*)szDigest, nLenDigest, '\0');
	// const std::string desire_HmacSHA256 = "b74c87eea94bdec023f853cfd1df78bf8e794e4a5b2e2cc2eb57ed7a1c310b58";
	// VALIDATE(hashInBase64 == desire_HmacSHA256, null_str);

	// var signUp = hashInBase64.toUpperCase();
	const std::string signUp = utils::uppercase(hashInBase64);
	// VALIDATE(signUp == "F97E797A8C70EFF3C4DAF321D9594185CFEE7F01F38840544A8D04E9093DD848", null_str);

	return signUp;
}

enum {cmd_get_access_token, cmd_get_device_info, cmd_get_doorbell_event};

// bool tleagor_iot::did_pre_tuya(net::HttpRequestHeaders& headers, std::string& body, const std::string& client_id, 
bool tleagor_iot::did_pre_tuya(net::thttp_api& net_api, std::string& body, const std::string& client_id, 
	int64_t t, const std::string& sign, int cmd)
{
	// --header "sign_method: HMAC-SHA256"
	// --header "client_id: 4wwuer5u3sa4ycsep4a8"
	// --header "t: 1720493186611" 
	// --header "mode: cors"
	// --header "Content-Type: application/json"
	// --header "sign: C047019897ABF0BF288172445C3AB6B98FC4F7A34D29FE7D0637C60030807819"
	// --header "access_token: babb8351d90bf909f144ad639da25b34"

	bool get_access_token = false;
	Json::Value json_root;
	
	if (cmd == cmd_get_access_token) {
		net_api.SetHeader("client_id", client_id);
		net_api.SetHeader("t", str_cast(t));
		net_api.SetHeader("sign", sign);
		net_api.SetHeader("sign_method", "HMAC-SHA256");

	} else if (cmd == cmd_get_device_info) {
		net_api.SetHeader("client_id", client_id);
		net_api.SetHeader("t", str_cast(t));
		net_api.SetHeader("sign", sign);
		net_api.SetHeader("access_token", access_token_);
		net_api.SetHeader("sign_method", "HMAC-SHA256");

		net_api.SetHeader("mode", "cors");

		// net_api.SetHeader("nonce", null_str);
		// net_api.SetHeader("stringToSign", null_str);

	} else {
		VALIDATE(cmd == cmd_get_doorbell_event, null_str);
		net_api.SetHeader("client_id", client_id);
		net_api.SetHeader("t", str_cast(t));
		net_api.SetHeader("sign", sign);
		net_api.SetHeader("access_token", access_token_);
		net_api.SetHeader("sign_method", "HMAC-SHA256");

		// net_api.SetHeader("mode", "cors");
	}

	// net_api.SetHeader(net::HttpRequestHeaders::kContentType, "application/json");

	net_api.SetHeader(HttpRequestHeaders_kContentType, "application/json");

	return true;
}

bool tleagor_iot::did_post_tuya(net::thttp_api& net_api, int status, const std::string& data_received, const tdevice2& device, int cmd)
{
	std::stringstream err;

	if (status != net_api.OK) {
		err << net_api.err_2_description(status);
		// gui2::show_message(null_str, err.str());
		return false;
	}

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid json request");
		} else {
			Json::Value& json_success = json_object["success"];
			bool success = json_success.asBool();
			bool handled;
			err << generic_handle_response(success, json_object, handled);
			if (!handled && err.str().empty()) {
				Json::Value& result = json_object["result"];
				if (result.isObject()) {
					if (cmd == cmd_get_access_token) {
						// {"access_token":"3c050913cb8e63cdf42cd407009d26f3",
						// "expire_time":5374,
						// "refresh_token":"1c5bdbeadfc57f611819a5177185bbd8",
						// "uid":"bay1718162227099b70j"}
						access_token_ = result["access_token"].asString();

						int expire_time = result["expire_time"].asInt();
						const std::string refresh_token = result["refresh_token"].asString();
						const std::string uid = result["uid"].asString();

						// SDL_Log("%u tuya's cmd get resp(cmd_get_device_info). access_token: %s", SDL_GetTicks(), access_token_.c_str());
						
					} else if (cmd == cmd_get_device_info) {
						// SDL_Log("%u tuya's cmd get resp(cmd_get_device_info)", SDL_GetTicks());

					} else {
						VALIDATE(cmd == cmd_get_doorbell_event, null_str);
						Json::Value& json_logs = result["logs"];
						std::set<int64_t> logs;
						if (json_logs.isArray()) {
							int log_count = json_logs.size();
							for (int at = 0; at < log_count; at ++) {
								const Json::Value& json_log = json_logs[at];
								logs.insert(json_log["event_time"].asInt64());
							}
						}

						if (!logs.empty()) {
							threading::lock lock(event_result_mutex_);
							event_result_dirty_ = true;
							for (std::set<int64_t>::const_iterator it = logs.begin(); it != logs.end(); ++ it) {
								int64_t t = *it;
								VALIDATE(my_iot_srcs_.count(device.src) != 0, null_str);
								const tiot_src& my_iot_src = my_iot_srcs_.find(device.src)->second;
								int evt = *iot_sources.find(device.src)->second.events.begin();
								event_result_.insert(tiot_event(t, device.src, evt, device.id, my_iot_src.icon));
							}

							// @logs is sorted in ascending order base @t. not event_result_.
							last_start_time_ = *logs.rbegin() + 1;

							// SDL_Log("{dbg-tuya}did_post_tuya(cmd_get_doorbell_event)now: %s, event: %s", 
							//	utils::format_time_ymdhms(time(nullptr)).c_str(), utils::format_time_ymdhms(*logs.rbegin() / 1000).c_str());
						}

						// SDL_Log("%u tuya's cmd get resp(cmd_get_doorbell_event)", SDL_GetTicks());
					}
				}
			}

			// tfile file(game_config::preferences_dir + "/1.dat", GENERIC_WRITE, CREATE_ALWAYS);
			// VALIDATE(file.valid(), null_str);
			// posix_fwrite(file.fp, data_received.c_str(), data_received.size());
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		if (cmd == cmd_get_doorbell_event) {
			access_token_.clear();
		}
	}
	

	return true;
}

bool tleagor_iot::xmit_tuya(const std::string& clientId, const std::string& secret, const tdevice2& device_id, int cmd)
{
	VALIDATE(!clientId.empty(), null_str);
	VALIDATE(!secret.empty(), null_str);

	VALIDATE(!device_id.id.empty(), null_str);
	if (cmd == cmd_get_access_token) {
		VALIDATE(device_id.id == fake_deivceid_, null_str);
	} else {
		VALIDATE(device_id.id != fake_deivceid_, null_str);
	}
	VALIDATE(last_start_time_ != nposm, null_str);
	
	// std::string accessToken = "c4226de561c1ed6a49bab5713775a83f";
	// const std::string accessToken;
	// const int64_t timestamp = INT64_C(1721205070653);
	const int64_t timestamp = INT64_C(1000) * time(nullptr);
	const std::string nonce;
	const std::string method = "GET";

	std::string bodyStr;
	std::unique_ptr<uint8_t[]> md = utils::sha256((const uint8_t*)bodyStr.c_str(), bodyStr.size());
	const std::string sha256 = utils::hex_encode_cstyle((const char*)md.get(), SHA256_DIGEST_LENGTH, '\0');

	const std::string headersStr;
	std::string url = "/v1.0/token?grant_type=1";
	char buf[256];

	// SDL_Log("%u xmit_tuya(device_id: %s, cmd: %i, accessToken: %s)", SDL_GetTicks(), device_id.c_str(), cmd, access_token_.c_str());
	if (cmd == cmd_get_access_token) {
		VALIDATE(access_token_.empty(), null_str);

	} else if (cmd == cmd_get_device_info) {
		VALIDATE(!access_token_.empty(), null_str);
		// url = "/v1.0/devices/70482087ec94cb835c89";
		SDL_snprintf(buf, sizeof(buf), "/v1.0/devices/%s", device_id.id.c_str());
		url = buf;

		// const std::string url2 = "/v1.0/devices/70482087ec94cb835c89";
		// VALIDATE(url2 == url, null_str);

	} else {
		VALIDATE(cmd == cmd_get_doorbell_event, null_str);
		VALIDATE(!access_token_.empty(), null_str);
		// url = "/v2.0/cloud/thing/70482087ec94cb835c89/report-logs?codes=alarm_msg&end_time=1721146197336&size=20&start_time=1721090943613";
		// int64_t start_time = 1721114597790;
		// int64_t end_time = 1721246197336;
		int64_t start_time = last_start_time_;
		int64_t end_time = INT64_C(1000) * (time(nullptr) + INT64_C(10));
		// SDL_snprintf(buf, sizeof(buf), "/v2.0/cloud/thing/%s/report-logs?codes=alarm_msg&end_time=%lld&size=20&start_time=%lld",
		//	device_id.c_str(), end_time, start_time);

		SDL_snprintf(buf, sizeof(buf), "/v2.0/cloud/thing/%s/report-logs?codes=alarm_msg&end_time=%" PRIu64 "&size=20&start_time=%" PRIu64 "",
			device_id.id.c_str(), end_time, start_time);

		url = buf;

		// SDL_Log("{dbg-tuya}xit_tuya(cmd_get_doorbell_event)now: %s, strat_time: %s, end_time: %s", 
		//	utils::format_time_ymdhms(time(nullptr)).c_str(), utils::format_time_ymdhms(start_time / 1000).c_str(), utils::format_time_ymdhms(end_time / 1000).c_str());
		// const std::string url2 = "/v2.0/cloud/thing/70482087ec94cb835c89/report-logs?codes=alarm_msg&end_time=1721220859998&size=20&start_time=1721090943613";
		// VALIDATE(url2 == url, null_str);
	}

	const std::string signUrl = method + "\n" + sha256 + "\n" + headersStr + "\n" + url;
	const std::string& signStr = signUrl;
	const std::string sign = calcSign(clientId, access_token_, timestamp, nonce, signStr, secret); 

	// const std::string url2 = "https://openapi.tuyacn.com/v2.0/cloud/thing/70482087ec94cb835c89/report-logs?codes=alarm_msg&start_time=1721090943613&end_time=1721146197336&size=20";
	char url2[256];
	SDL_snprintf(url2, sizeof(url2), "https://openapi.tuyacn.com%s", url.c_str());

	bool cancel = false;
	net::thttp_api* net_api = net::create_http_api(url2, "GET", 5000);

	net_api->did_pre = std::bind(&tleagor_iot::did_pre_tuya, this, _1, _2, clientId, timestamp, sign, cmd);
	net_api->did_post = std::bind(&tleagor_iot::did_post_tuya, this, _1, _2, _3, device_id, cmd);
	net_api->handle_http_request(cancel);

	delete net_api;
	return true;
}

void tleagor_iot::pre_start_iot()
{
	VALIDATE(next_get_access_token_ticks_ == 0, null_str);
	next_get_access_token_ticks_ = SDL_GetTicks();

	VALIDATE(next_get_doorbell_event_ticks_ == 0, null_str);
	next_get_doorbell_event_ticks_ = SDL_GetTicks();

	last_start_time_ = INT64_C(1000) * time(nullptr);
}

void tleagor_iot::post_stop_iot()
{
	next_get_access_token_ticks_ = 0;
	next_get_doorbell_event_ticks_ = 0;
	last_start_time_ = nposm;
}

void tleagor_iot::start_iot(bool& exit)
{
	std::string last_client_id;
	std::string last_client_secret;

	while (!exit) {
		std::string client_id;
		std::string client_secret;

		// enum {cmd_get_access_token, cmd_get_device_info, cmd_get_doorbell_event};
		if (access_token_.empty() && SDL_GetTicks() >= next_get_access_token_ticks_) {
			{
				threading::lock lock(cpp_id_mutex_);
				client_id = client_id_;
				client_secret = client_secret_;
			}

			if (client_id != last_client_id || client_secret != last_client_secret) {
				last_client_id = client_id;
				last_client_secret = client_secret;
			}

			if (!client_id.empty() && !client_secret.empty()) {
				const tdevice2 fake_device(fake_deivceid_, iot_src_doorbell);
				xmit_tuya(client_id, client_secret, fake_device, cmd_get_access_token);
				next_get_access_token_ticks_ = SDL_GetTicks() + 5 * 1000;
			}
		}

		if (!access_token_.empty() && SDL_GetTicks() >= next_get_doorbell_event_ticks_) {
			std::vector<tdevice2> device_ids;
			{
				threading::lock lock(cpp_id_mutex_);
				client_id = client_id_;
				client_secret = client_secret_;
				device_ids = device_ids_;
			}
			if (client_id != last_client_id || client_secret != last_client_secret) {
				// xmit cmd_get_access_token again.
				access_token_.clear();

				last_client_id = client_id;
				last_client_secret = client_secret;
			}

			// xmit_tuya(device_id2, cmd_get_device_info);
			for (std::vector<tdevice2>::const_iterator it = device_ids.begin(); it != device_ids.end(); ++ it) {
				const tdevice2& device = *it;
				if (!client_id.empty() && !client_secret.empty() && !access_token_.empty()) {
					// below cmd_get_doorbell_event maybe fail. 
					// for example: Your subscription to cloud development plan has expired.
					xmit_tuya(client_id, client_secret, device, cmd_get_doorbell_event);
				}
			}
			next_get_doorbell_event_ticks_ = SDL_GetTicks() + pool_event_interval_s_ * 1000;
		}
	}
}

void tleagor_iot::fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id)
{
	if (cpp_id == cpp_id_sys_dlg_closed) {

	} else if (cpp_id == cpp_id_save_iot_5fields) {
		threading::lock lock(cpp_id_mutex_);
		reload_device_ids();
	}
}

}

void* aplt_create_iot_slot(void* _subscriber)
{
	aplt::tslot_subscriber* subscriber = reinterpret_cast<aplt::tslot_subscriber*>(_subscriber);
	aplt::tleagor_iot* leagor = new aplt::tleagor_iot(*subscriber);
	// if (!leagor->xfyun().libmsc_loaded()) {
	//	delete leagor;
	//	return nullptr;
	// }

	aplt::tiot_slot* result = leagor;
	return result;
}
