/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
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

#ifndef GUI_DIALOGS_STORE_HPP_INCLUDED
#define GUI_DIALOGS_STORE_HPP_INCLUDED

#include "gui/dialogs/rstore.hpp"
#include "gui/dialogs/statusbar.hpp"
#include "base_driver.hpp"

#include "wkocourse.hpp"

class tdrivers;
class tinstance_slot;

namespace aplt {
class tcfg_cpp_api;
class twkocourse_enroll;
class twkocourse;
}

void wkocourse_enrolls_to_courses(const std::map<std::string, aplt::twkocourse_enroll>& enrolls, std::map<std::string, aplt::twkocourse>& wkocourses);

namespace gui2 {
class tpanel;

class tstore_slot: public trstore::tslot, public tstatusbar
{
public:
	tstore_slot(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, std::map<aplt::taplt_key, aplt::tapplet>& applets, aplt::tbg_task& bg_task, aplt::tcfg_cpp_api& cfg_cpp_api, tdrivers& drivers,
		tbase_driver& base_driver, tinstance_slot& instance_slot,
		const std::map<std::string, aplt::twkocourse_enroll>& wkocourse_enrolls, std::map<std::string, aplt::twkocourse>& wkocourses);

private:
	void rstore_pre_show(twindow& window, tpanel& statusbar_widget) override;
	void rstore_post_show(twindow& window) override;
	void rstore_timer_handler(twindow& window, uint32_t now) override;
	bool rstore_show_confirm_install(int type, const std::string& bundleid) override;
	void rstore_did_applet_installed(const aplt::tapplet& applet) override
	{
		wkocourse_enrolls_to_courses(wkocourse_enrolls_, wkocourses_);
	}

	bool rstore_show_confirm_uninstall(const aplt::tapplet& aplt) override;
	// void rstore_did_applet_will_uninstall(int source, const std::string& bundleid, const std::string& res_path) override;
	void rstore_did_applet_uninstalled(int source, const std::string& bundleid) override
	{
		wkocourse_enrolls_to_courses(wkocourse_enrolls_, wkocourses_);
	}

	// void did_applet_installed_or_uninstalled(int source, const std::string& bundleid);
	void rstore_nullptr_slot_for_aplt_drivers(const aplt::tapplet& aplt) override;

private:
	tinstance_slot& instance_slot_;

	const std::map<std::string, aplt::twkocourse_enroll>& wkocourse_enrolls_;
	std::map<std::string, aplt::twkocourse>& wkocourses_;
};


}

#endif /* ! GUI_DIALOGS_EXPLORER_HPP_INCLUDED */
