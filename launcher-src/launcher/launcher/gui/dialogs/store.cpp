/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/store.hpp"

#include "gettext.hpp"

// #include "game_config.hpp"
#include "base_instance.hpp"
#include "drivers.hpp"
#include "base_driver.hpp"
#include "cfg_cpp_api.hpp"
#include "game_config.hpp"
#include "ros_instance.hpp"

namespace gui2 {

tstore_slot::tstore_slot(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, std::map<aplt::taplt_key, aplt::tapplet>& applets, aplt::tbg_task& bg_task, aplt::tcfg_cpp_api& cfg_cpp_api, tdrivers& drivers,
	tbase_driver& base_driver, /*const std::function<void (const std::string&)>& did_applet_will_uninstall*/tinstance_slot& instance_slot)
	: trstore::tslot(applets, bg_task, cfg_cpp_api, drivers, base_driver, /*did_applet_will_uninstall,*/ 1000)
	, tstatusbar(rdpd_mgr, pble, privacy)
	// , applets_(applets)
	// , bg_task_(bg_task)
	// , cfg_cpp_api_(cfg_cpp_api)
	// , drivers_(drivers)
	// , base_driver_(base_driver)
	, instance_slot_(instance_slot)
{
	recommend_aplts_.push_back("aplt.leagor.basic");
	recommend_aplts_.push_back("aplt.leagor.khome");
	recommend_aplts_.push_back("aplt.leagor.blesmart");
	recommend_aplts_.push_back("aplt.nlsd.basic");
	// recommend_aplts_.push_back("aplt.leagor.iaccess");

	// did_applet_will_uninstall_ = did_applet_will_uninstall;
}

void tstore_slot::rstore_pre_show(gui2::twindow& window, gui2::tpanel& statusbar_widget)
{
	tstatusbar::pre_show(window, statusbar_widget.grid());
}

void tstore_slot::rstore_post_show(twindow& window)
{
	tstatusbar::clear();
}

void tstore_slot::rstore_timer_handler(twindow& window, uint32_t now)
{
	refresh_statusbar_grid(now);
}

bool tstore_slot::rstore_show_confirm_install(int type, const std::string& bundleid)
{
	if (bg_task_.is_ing()) {
		utils::string_map symbols;
		symbols["aplt"] = bundleid;

		std::string warnning = vgettext2("A bg task is running and it needs to be stopped first. Do you want to continue installing '$aplt'?", symbols);
		std::string log = vgettext2("To install '$aplt', stop the current one", symbols);
		if (!instance->stop_bg_task_if_runing(warnning, log)) {
			return false;
		}
		return true;
	}
	return gui2::trstore::tslot::rstore_show_confirm_install(type, bundleid);
}

bool tstore_slot::rstore_show_confirm_uninstall(const aplt::tapplet& aplt)
{
	if (bg_task_.is_ing()) {
		utils::string_map symbols;
		symbols["aplt"] = aplt.name2();

		std::string warnning = vgettext2("A bg task is running and it needs to be stopped first. Do you want to continue uninstalling '$aplt'?", symbols);
		std::string log = vgettext2("To uninstall '$aplt', stop the current one", symbols);
		if (!instance->stop_bg_task_if_runing(warnning, log)) {
			return false;
		}
		return true;
	}
	return gui2::trstore::tslot::rstore_show_confirm_uninstall(aplt);
}
/*
void tstore_slot::rstore_did_applet_uninstalled(int source, const std::string& bundleid)
{
	did_applet_installed_or_uninstalled(source, bundleid);
}
*/

void tstore_slot::rstore_nullptr_slot_for_aplt_drivers(const aplt::tapplet& aplt)
{
	instance_slot_.nullptr_slot_for_aplt_drivers(aplt);
}

}
