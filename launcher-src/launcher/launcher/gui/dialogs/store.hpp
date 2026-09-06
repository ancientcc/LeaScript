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

class tdrivers;
class tinstance_slot;

namespace aplt {
class tcfg_cpp_api;
}

namespace gui2 {
class tpanel;

class tstore_slot: public trstore::tslot, public tstatusbar
{
public:
	tstore_slot(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, std::map<aplt::taplt_key, aplt::tapplet>& applets, aplt::tbg_task& bg_task, aplt::tcfg_cpp_api& cfg_cpp_api, tdrivers& drivers,
		tbase_driver& base_driver, /*const std::function<void (const std::string&)>& did_applet_will_uninstall*/tinstance_slot& instance_slot);

private:
	void rstore_pre_show(twindow& window, tpanel& statusbar_widget) override;
	void rstore_post_show(twindow& window) override;
	void rstore_timer_handler(twindow& window, uint32_t now) override;
	bool rstore_show_confirm_install(int type, const std::string& bundleid) override;
	// void rstore_did_applet_installed(const aplt::tapplet& applet) override;
	bool rstore_show_confirm_uninstall(const aplt::tapplet& aplt) override;
	// void rstore_did_applet_will_uninstall(int source, const std::string& bundleid, const std::string& res_path) override;
	// void rstore_did_applet_uninstalled(int source, const std::string& bundleid) override;

	// void did_applet_installed_or_uninstalled(int source, const std::string& bundleid);
	void rstore_nullptr_slot_for_aplt_drivers(const aplt::tapplet& aplt) override;

private:
	// std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	// aplt::tbg_task& bg_task_;
	// aplt::tcfg_cpp_api& cfg_cpp_api_;
	// tbase_driver& base_driver_;
	// tdrivers& drivers_;
	// std::function<void (const std::string&)> did_applet_will_uninstall_;
	tinstance_slot& instance_slot_;
/*
	struct tuninstall_point
	{
		tuninstall_point()
			: subtask_state(nposm)
		{}

		void set(const std::string& _scene_id, int _subtask_state)
		{
			VALIDATE(!_scene_id.empty() && _subtask_state == aplt::sts_idle || _subtask_state == aplt::sts_ing, null_str);
			scene_id = _scene_id;
			subtask_state = _subtask_state;
		}

		void clear()
		{
			scene_id.clear();
			subtask_state = nposm;
		}

		void validate_nposm() const
		{
			VALIDATE(scene_id.empty(), null_str);
			VALIDATE(subtask_state == nposm, null_str);
		}

		std::string scene_id;
		int subtask_state;
	};
	tuninstall_point uninstall_point_;
*/
};


}

#endif /* ! GUI_DIALOGS_EXPLORER_HPP_INCLUDED */
