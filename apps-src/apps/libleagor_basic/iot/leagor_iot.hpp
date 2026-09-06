/* $Id: dialog.hpp 50956 2011-08-30 19:41:22Z mordante $ */
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

#ifndef LIBROSE_LEAGOR_IOT_HPP_INCLUDED
#define LIBROSE_LEAGOR_IOT_HPP_INCLUDED

#include "base_slot.hpp"
#include <memory>

#include "rose_net_api.hpp"

namespace aplt {

class tleagor_iot: public tiot_slot
{
public:
	tleagor_iot(aplt::tslot_subscriber& subscriber);
	~tleagor_iot() {}

private:
	// tiot_slot
	void pre_start_iot() override;
	void post_stop_iot() override;
	void start_iot(bool& exit) override;
	void fg_aplt_send_cpp_id(const std::string& window_id, int cpp_id) override;

	void reload_device_ids();

	struct tiot_src
	{
		tiot_src(int src, const std::string& icon)
			: src(src)
			, icon(icon)
		{}

		int src;
		std::string icon;
	};

	struct tdevice2
	{
		tdevice2(const std::string& _id, int _src)
			: id(_id)
			, src(_src)
		{
			VALIDATE(!id.empty(), null_str);
			if (src < aplt::iot_src_sensor_min || src > aplt::iot_src_sensor_max) {
				src = aplt::iot_src_doorbell;
			}
		}
		std::string id;
		int src;
	};
	bool did_pre_tuya(net::thttp_api& net_api, std::string& body, const std::string& client_id, 
		int64_t t, const std::string& sign, int cmd);
	bool did_post_tuya(net::thttp_api& net_api, int status, const std::string& data_received, const tdevice2& device_id, int cmd);
	bool xmit_tuya(const std::string& client_id, const std::string& client_secret, const tdevice2& device, int cmd);

private:
	const std::string fake_deivceid_;
	threading::mutex cpp_id_mutex_;

	std::map<int, tiot_src> my_iot_srcs_;

	int pool_event_interval_s_;
	uint32_t next_get_access_token_ticks_;
	uint32_t next_get_doorbell_event_ticks_;

	std::string client_id_;
	std::string client_secret_;
	std::vector<tdevice2> device_ids_;
	std::string access_token_;
	int64_t last_start_time_;
};

}

#endif

