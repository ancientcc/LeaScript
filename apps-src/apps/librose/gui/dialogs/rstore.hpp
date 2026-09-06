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

#ifndef GUI_DIALOGS_RSOTRE_HPP_INCLUDED
#define GUI_DIALOGS_RSOTRE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "util.hpp"
#include "serialization/string_utils.hpp"
#include <vector>
#include <set>
#include "aplt_net.hpp"


// using namespace std::placeholders;

class tcamera;
class tdrivers_core;
class tbase_driver_core;

namespace aplt {
class tbg_task;
class tcfg_cpp_api_core;
}

namespace gui2 {
class tlabel;
class tbutton;
class treport;
class ttoggle_button;
class tstack;
class ttoggle_panel;
class ttext_box;
class tlistbox;
class tpanel;

class trstore: public tdialog
{
public:
	class tslot
	{
	public:
		tslot(std::map<aplt::taplt_key, aplt::tapplet>& applets, aplt::tbg_task& bg_task, aplt::tcfg_cpp_api_core& cfg_cpp_api, tdrivers_core& drivers,
			tbase_driver_core& base_driver, int interval = 0)
			: applets_(applets)
			, bg_task_(bg_task)
			, cfg_cpp_api_(cfg_cpp_api)
			, drivers_(drivers)
			, base_driver_(base_driver)
			, interval_(interval)
		{}
		virtual ~tslot() {}

		// why not use 'tgrid& statusbar_widget'?
		// --derived class maybe INVISIBLE statusbar_widget. if INVISIBLE tpanel's tgrid, will throw exception.
		virtual void rstore_pre_show(twindow& window, tpanel& statusbar_widget);
		virtual void rstore_post_show(twindow& window) {}
		virtual void rstore_timer_handler(twindow& window, uint32_t now) {}

		virtual bool rstore_show_confirm_install(int type, const std::string& bundleid) { return true; }
		virtual void rstore_did_applet_installed(const aplt::tapplet& aplt) {}
		virtual bool rstore_show_confirm_uninstall(const aplt::tapplet& aplt);
		// virtual void rstore_did_applet_will_uninstall(int source, const std::string& bundleid, const std::string& res_path);
		void rstore_did_applet_will_uninstall(int source, const std::string& bundleid, const std::string& res_path);
		virtual void rstore_did_applet_uninstalled(int source, const std::string& bundleid) {}

		virtual void rstore_nullptr_slot_for_aplt_drivers(const aplt::tapplet& aplt);

		int interval() { return interval_; }
		const std::vector<std::string>& recommend_aplts() const { return recommend_aplts_; }

		bool install_applet(std::map<aplt::taplt_key, aplt::tapplet>& applets, int type, const std::string bundleid);
		void uninstall_applet(std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::tapplet& aplt);

		void did_applet_installed_or_uninstalled(int source, const std::string& bundleid);
		void did_applet_will_uninstall(const std::string& res_path);

	protected:
		std::map<aplt::taplt_key, aplt::tapplet>& applets_;
		aplt::tbg_task& bg_task_;
		aplt::tcfg_cpp_api_core& cfg_cpp_api_;
		tdrivers_core& drivers_;
		tbase_driver_core& base_driver_;

		int interval_;
		std::vector<std::string> recommend_aplts_;

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
	};

	enum {RECOMMEND_PAGE, FIND_PAGE, ME_PAGE};

	trstore(tslot& slot, tcamera& camera, std::map<aplt::taplt_key, aplt::tapplet>& applets)
		: slot_(slot)
		, camera_(camera)
		, applets_(applets)
		, min_title_bytes_(3)
		, current_page_(nposm)
		, bar_(nullptr)
		, body_(nullptr)
		, recommend_applets_list_(nullptr)
		, find_applets_list_(nullptr)
		, find_find_button_(nullptr)
		, me_applets_list_(nullptr)
		, devel_install_button_(nullptr)
	{
		if (slot_.interval() != 0) {
			set_timer_interval(slot_.interval());
		}
	}

protected:
	virtual void rapplets_post_show() {};
	virtual void rapplets_first_drawn() {};
	virtual void rapplets_resize_screen() {};

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;
	void post_show() override;

	void pre_recommend(tgrid& grid);
	void pre_find(tgrid& grid);
	void pre_me(tgrid& grid);

	bool did_bar_item_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to);
	void did_bar_item_changed(treport& report, ttoggle_button& widget);

	void click_qr_scan();

	enum {list_recommend, list_find, list_me}; // list_me_distribution, list_me_development
	void reload_applets(const std::vector<aplt::tapplet>& applets, gui2::tlistbox& list, int type);

	void click_recommend_install(ttoggle_panel& row, tbutton& widget);
	void update_recommend_grid();
	void did_applets_changed();

	void update_find_grid();
	void did_bundleid_changed(ttext_box& widget, bool find_page);
	void click_find_find(tgrid& grid);
	void click_find_install(ttoggle_panel& row);

	void update_me_grid();
	void click_me_upgrade(ttoggle_panel& row, tbutton& widget);
	void click_me_devel_install(tgrid& grid);

	void app_timer_handler(uint32_t now) override { slot_.rstore_timer_handler(*window_, now); }

private:
	tslot& slot_;
	tcamera& camera_;
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	const int min_title_bytes_;
	int current_page_;
	treport* bar_;
	tstack* body_;

	tlistbox* recommend_applets_list_;
	tlistbox* find_applets_list_;
	tbutton* find_find_button_;
	tlistbox* me_applets_list_;
	tbutton* devel_install_button_;

	std::vector<aplt::tapplet> got_recommend_applets_;
	std::set<std::string> got_recommend_bundles_;

	std::vector<aplt::tapplet> find_applets_;
};

}

namespace aplt {
class tdesktop
{
public:
	tdesktop(rtc::MessageHandler& msghandler, gui2::trstore::tslot& slot, std::map<aplt::taplt_key, aplt::tapplet>& applets, int base_retval)
		: msghandler_(msghandler)
		, slot_(slot)
		, report_(nullptr)
		, pinyin_label_widget_(nullptr)
		, upgrade_pinyin_widget_(nullptr)
		, base_retval_(base_retval)
		, applets_(applets)
	{}

protected:
	void rapplets_pre_show(gui2::twindow& window, gui2::treport& report, gui2::tlabel* pinyin_label, gui2::tbutton* upgrade_pinyin);
	// virtual void rapplets_post_show() {};
	// virtual void rapplets_first_drawn() {};
	// virtual void rapplets_resize_screen() {};

	virtual std::vector<aplt::tbuildin> rapplets_get_fake_applets() = 0;
	virtual void rapplets_did_click_fake_applet(const aplt::tbuildin& applet)
	{
		window2_->set_retval(base_retval_ + applet.id);
	}

	virtual void rapplets_did_click_applet(int at)
	{
		window2_->set_retval(base_retval_ + at);
	}

	void refresh_pinyin_label();
	void click_upgrade_pinyin(gui2::tbutton& widget);

	virtual void rapplets_did_pinyin_upgraded() {}

	struct tmsg_longpress_applet: public rtc::MessageData {
		explicit tmsg_longpress_applet(int at, const tpoint& coordinate)
			: at(at)
			, coordinate(coordinate)
		{}

		const int at;
		const tpoint coordinate;
	};
	enum {MSG_LONGPRESS_APPLET = gui2::tdialog::POST_MSG_MIN_APP + 100};
	void rapplets_OnMessage(rtc::Message* msg);

private:
	void reload_applets(gui2::treport& report);
	void did_applet_click(gui2::treport& report, gui2::tbutton& widget);
	void did_applet_longpress(gui2::treport& report, gui2::tbutton& widget, const tpoint& coordinate);
	void popup_longpress_menu(int at, const tpoint& coordinate);

private:
	rtc::MessageHandler& msghandler_;
	gui2::trstore::tslot& slot_;
	gui2::twindow* window2_;
	gui2::treport* report_;
	gui2::tlabel* pinyin_label_widget_;
	gui2::tbutton* upgrade_pinyin_widget_;
	const int base_retval_;

	std::vector<aplt::tbuildin> buildins_;
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
};

}



#endif /* ! GUI_DIALOGS_RSOTRE_HPP_INCLUDED */
