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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/rstore.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"
#include "preferences.hpp"
#include "font.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/messagefs.hpp"
#include "gui/dialogs/rcamera.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/listbox.hpp"

// std::tolower
#include <cctype>

#include "cfg_cpp_api_core.hpp"
#include "drivers_core.hpp"
#include "base_driver_core.hpp"
#include "base_instance.hpp"
#include "chinese.hpp"

namespace aplt {

void tdesktop::rapplets_pre_show(gui2::twindow& window, gui2::treport& report, gui2::tlabel* pinyin_label, gui2::tbutton* upgrade_pinyin)
{
	window2_ = &window;
	report_ = &report;
	pinyin_label_widget_ = pinyin_label;
	upgrade_pinyin_widget_ = upgrade_pinyin;
	report.set_did_item_click(std::bind(&tdesktop::did_applet_click, this, _1, _2));
	report.set_did_item_longpress(std::bind(&tdesktop::did_applet_longpress, this, _1, _2, _3));

	reload_applets(report);

	if (upgrade_pinyin != nullptr) {
		connect_signal_mouse_left_click(
			  *upgrade_pinyin
			, std::bind(
			&tdesktop::click_upgrade_pinyin
			, this, std::ref(*upgrade_pinyin)));
		refresh_pinyin_label();
	}
}

void tdesktop::reload_applets(gui2::treport& report)
{
	report.clear();

	if (buildins_.empty()) {
		buildins_ = rapplets_get_fake_applets();
	}

	for (std::vector<aplt::tbuildin>::const_iterator it = buildins_.begin(); it != buildins_.end(); ++ it) {
		const aplt::tbuildin& applet = *it;

		gui2::tcontrol& widget = report.insert_item(null_str, applet.name);
		widget.set_icon(applet.icon);
	}

	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		const aplt::tapplet& applet = it->second;

		std::string label = applet.name2();
		gui2::tcontrol& widget = report.insert_item(null_str, label);
		widget.set_icon(applet.icon);
	}
}

void tdesktop::did_applet_click(gui2::treport& report, gui2::tbutton& widget)
{
	int at = widget.at();
	if (at < (int)buildins_.size()) {
		const aplt::tbuildin& fake = buildins_[at];
		if (fake.id == aplt::builtinid_store) {
			rapplets_did_click_fake_applet(fake);
		} else {
			rapplets_did_click_fake_applet(fake);
		}
		return;
	}

	at -= buildins_.size();
	VALIDATE(at < (int)applets_.size(), null_str);
	rapplets_did_click_applet(at);
}

void tdesktop::popup_longpress_menu(int at, const tpoint& coordinate)
{
	std::vector<gui2::tmenu::titem> items;

	VALIDATE(at < (int)applets_.size(), null_str);
	const aplt::tapplet& aplt = aplt::aplt_from_at(applets_, at);

	enum {about, delete_applet};
	
	// items.push_back(gui2::tmenu::titem(std::string(_("About")), about));
	items.push_back(gui2::tmenu::titem(_("applet^Delete"), delete_applet));

	int selected;
	{
		gui2::tmenu dlg(items, nposm);
		dlg.show(coordinate.x - 48 * gui2::twidget::hdpi_scale, coordinate.y + 32 * gui2::twidget::hdpi_scale);
		int retval = dlg.get_retval();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}
		// absolute_draw();
		selected = dlg.selected_val();
	}
	if (selected == about) {
		
	} else if (selected == delete_applet) {
		if (!slot_.rstore_show_confirm_uninstall(aplt)) {
			return;
		}
		slot_.uninstall_applet(applets_, aplt);
		reload_applets(*report_);
	}
}

void tdesktop::did_applet_longpress(gui2::treport& report, gui2::tbutton& widget, const tpoint& coordinate)
{
	int at = widget.at();
	if (at < (int)buildins_.size()) {
		return;
	}

	at -= buildins_.size();
	const aplt::tapplet& aplt = aplt::aplt_from_at(applets_, at);
	if (aplt.source == aplt::src_studio) {
		return;
	}

	tmsg_longpress_applet* pdata = new tmsg_longpress_applet(at, coordinate);
	rtc::Thread::Current()->Post(RTC_FROM_HERE, &msghandler_, MSG_LONGPRESS_APPLET, pdata);
}

void tdesktop::refresh_pinyin_label()
{
	gui2::tlabel* label = pinyin_label_widget_;
	gui2::tbutton* button = upgrade_pinyin_widget_;

	std::string label_str;
	std::string button_str;
	if (chinese::curr_pinyin.rsp.valid()) {
		label->set_visible(gui2::twidget::VISIBLE);

		utils::string_map symbols;
		symbols["version"] = chinese::curr_pinyin.rsp.version.str(true);
		label_str = vgettext2("Is using pinyin pack(V$version)", symbols);

		button_str = _("Upgrade");

	} else {
		label_str = _("No pinyin pack");
		button_str = _("Install");
	}
	label->set_label(label_str);
	button->set_label(button_str);
}

void tdesktop::click_upgrade_pinyin(gui2::tbutton& widget)
{
	tdisable_idle_lock lock;
	version_info curr_version;
	if (chinese::curr_pinyin.rsp.valid()) {
		curr_version = chinese::curr_pinyin.rsp.version;
	}
	const version_info new_version;
	bool result = net::download_pinyin_or_latexrsp(zipt_pinyin, curr_version, new_version);
	if (result) {
		chinese::load_pinyin_rsp();
		// speech_driver may be some class-member variables that require pinyin library to generate, for example: moveto_prefix_
		// since pinyin library has changed, those variables must also be recalculated.
		// speech_driver_.restart();
		refresh_pinyin_label();

		rapplets_did_pinyin_upgraded();
	}
}

void tdesktop::rapplets_OnMessage(rtc::Message* msg)
{
	const uint32_t now = SDL_GetTicks();

	if (msg->message_id == MSG_LONGPRESS_APPLET) {
		tmsg_longpress_applet* pdata = static_cast<tmsg_longpress_applet*>(msg->pdata);
		popup_longpress_menu(pdata->at, pdata->coordinate);
	}
}

}



namespace gui2 {

REGISTER_DIALOG(rose, rstore)

void trstore::pre_show()
{
	window_->set_escape_disabled(true);
	window_->set_label("misc/bg_ffffff.png");

	slot_.rstore_pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false));

	find_widget<tlabel>(window_, "title", false, true)->set_label(_("Store"));

	tbutton* button = find_widget<tbutton>(window_, "qr_scan", false, true);
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&trstore::click_qr_scan
			, this));
	const bool ios_disable_camera = true;
	if (ios_disable_camera && game_config::os == os_ios) {
		button->set_visible(twidget::INVISIBLE);
	}

	// prepare navigate bar.
	std::vector<std::string> labels;
	labels.push_back(_("Recommend"));
	labels.push_back(_("Search"));
	labels.push_back(_("Me"));

	bar_ = find_widget<treport>(window_, "bar", false, true);
	bar_->set_did_item_pre_change(std::bind(&trstore::did_bar_item_pre_change, this, _1, _2, _3));
	bar_->set_did_item_changed(std::bind(&trstore::did_bar_item_changed, this, _1, _2));
	bar_->set_boddy(find_widget<twidget>(window_, "bar_panel", false, true));
	int at = 0;
	for (std::vector<std::string>::const_iterator it = labels.begin(); it != labels.end(); ++ it) {
		tcontrol& widget = bar_->insert_item(null_str, *it);
		widget.set_cookie(at ++);
	}

	body_ = find_widget<tstack>(window_, "body", false, true);
	body_->set_radio_layer(RECOMMEND_PAGE);

	pre_recommend(*body_->layer(RECOMMEND_PAGE));
	pre_find(*body_->layer(FIND_PAGE));
	pre_me(*body_->layer(ME_PAGE));

	// until
	bar_->select_item(RECOMMEND_PAGE);
}

void trstore::post_show()
{
	slot_.rstore_post_show(*window_);
}

void trstore::pre_recommend(tgrid& grid)
{
	tlistbox* list = find_widget<tlistbox>(&grid, "applets", false, true);
	recommend_applets_list_ = list;
	list->enable_select(false);

}

void trstore::pre_find(tgrid& grid)
{
	tlistbox* list = find_widget<tlistbox>(&grid, "applets", false, true);
	find_applets_list_ = list;
	list->enable_select(false);

	tbutton* button = find_widget<tbutton>(&grid, "find", false, true);
	find_find_button_ = button;
	button->set_active(false);
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&trstore::click_find_find
			, this, std::ref(grid)));

	ttext_box* text_box = find_widget<ttext_box>(&grid, "title", false, true);
	text_box->set_maximum_chars(20);
	text_box->set_placeholder(_("object^Name"));
	text_box->set_did_text_changed(std::bind(&trstore::did_bundleid_changed, this, _1, true));
	// text_box->set_label("aplt.leagor.iaccess");
}

void trstore::pre_me(tgrid& grid)
{
	tlistbox* list = find_widget<tlistbox>(&grid, "applets", false, true);
	me_applets_list_ = list;
	list->enable_select(false);

	// development section
	tbutton* button = find_widget<tbutton>(&grid, "devel_install", false, true);
	devel_install_button_ = button;
	button->set_label(_("Install devel version applet"));
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&trstore::click_me_devel_install
			, this, std::ref(grid)));
}

bool trstore::did_bar_item_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to)
{
	twindow& window = *to.get_window();
	bool ret = true;
	int previous_page = (int)from.cookie();
/*
	if (previous_page == BASE_PAGE) {
		ret = save_base(window);
	} else if (previous_page == SIZE_PAGE) {
		ret = save_size(window);
	} else if (previous_page == ADVANCED_PAGE) {
		ret = save_advanced(window);
	}
*/
	return ret;
}

void trstore::update_recommend_grid()
{
	tlistbox* list = recommend_applets_list_;
	if (list->rows() != 0) {
		return;
	}

	const std::vector<std::string>& recommend_aplts = slot_.recommend_aplts();
	std::vector<std::string> bundleids;
	for (std::vector<std::string>::const_iterator it = recommend_aplts.begin(); it != recommend_aplts.end(); ++ it) {
		const std::string& recommend_bundleid = *it;
		bool found = false;
		for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it2 = applets_.begin(); it2 != applets_.end(); ++ it2) {
			const aplt::tapplet& aplt = it2->second;
			if ((aplt.source == aplt::src_distribution || aplt.source == aplt::src_development) && aplt.bundleid == recommend_bundleid) {
				found = true;
				break;
			}
		}
		if (got_recommend_bundles_.count(recommend_bundleid) != 0) {
			found = true;
		}
		if (!found) {
			bundleids.push_back(recommend_bundleid);
		}
	}

	std::vector<aplt::tapplet> result;
	if (!bundleids.empty()) {
		// [1/3]download
		gui2::tprogress_default_slot slot(std::bind(&net::cswamp_findapplet, _1, null_str, aplt::app_code_from_str(game_config::app), nposm, 
			utils::join(bundleids), null_str, false, std::ref(result)));
		bool ret = gui2::run_with_progress(slot, null_str, _("Find applet"), 1000);
		if (!result.empty()) {
			VALIDATE(ret, null_str);
			for (std::vector<aplt::tapplet>::const_iterator it = result.begin(); it != result.end(); ++ it) {
				const aplt::tapplet& aplt = *it;
				got_recommend_applets_.push_back(aplt);
				got_recommend_bundles_.insert(aplt.bundleid);
			}
		}
	}
/*
	for (std::vector<std::string>::const_iterator it = bundleids.begin(); it != bundleids.end(); ++ it) {
		const std::string& bundleid = *it;
		if (got_recommend_bundles_.count(bundleid) != 0) {
			continue;
		}
		// [1/3]download
		gui2::tprogress_default_slot slot(std::bind(&net::cswamp_findapplet, _1, null_str, nposm, nposm, bundleid, null_str, false, std::ref(result)));
		bool ret = gui2::run_with_progress(slot, null_str, _("Find applet"), 1000);
		if (!ret) {
			continue;
		}
		if (!result.empty()) {
			got_recommend_applets_.push_back(result.front());
			got_recommend_bundles_.insert(got_recommend_applets_.back().bundleid);
		}
	}
*/
	VALIDATE(got_recommend_applets_.size() == got_recommend_bundles_.size(), null_str);

	reload_applets(got_recommend_applets_, *list, list_recommend);
}

void trstore::update_find_grid()
{
	tlistbox* list = find_applets_list_;
	list->clear();

	find_applets_.clear();
}

void trstore::update_me_grid()
{
	tlistbox* list = me_applets_list_;
	if (list->rows() != 0) {
		return;
	}

	std::vector<aplt::tapplet> vec_applets;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		if (aplt.source == aplt::src_studio) {
			continue;
		}
		vec_applets.push_back(aplt);
	}

	reload_applets(vec_applets, *list, list_me);
}

void trstore::did_bar_item_changed(treport& report, ttoggle_button& widget)
{
	int page = (int)widget.cookie();
	body_->set_radio_layer(page);
	current_page_ = widget.cookie();

	if (page == RECOMMEND_PAGE) {
		update_recommend_grid();

	} else if (page == FIND_PAGE) {
		update_find_grid();

	} else if (page == ME_PAGE) {
		update_me_grid();
	}
}

std::string extract_bundileid_from_qrcode_str(const std::string& qrcode)
{
	const std::string prefix = "distribution/";
	size_t pos = qrcode.find(prefix);
	if (pos != 0) {
		return null_str;
	}
	const std::string result = qrcode.substr(prefix.size());
	if (!utils::is_rose_bundleid(result, '.')) {
		return null_str;
	}
	return result;
}

//
// trcamera::treceiver
//
class trstore_receiver: public trcamera::treceiver
{
public:
	trstore_receiver(trstore& owner)
		: owner_(owner)
	{}

private:
	bool rcamera_verify_new_qrcode(const std::string& qrcode) override
	{
		{
			// return false;
		}

		// distribution/aplt.leagor.iaccess
		return !extract_bundileid_from_qrcode_str(qrcode).empty();
	}

	bool rcamera_did_valid_qrcode(const std::string& qrcode) override
	{
		const std::string bundleid = extract_bundileid_from_qrcode_str(qrcode);
		VALIDATE(is_bundleid(bundleid), null_str);

		std::vector<aplt::tapplet> result;
		gui2::tprogress_default_slot slot(std::bind(&net::cswamp_findapplet, _1, null_str, nposm, nposm, bundleid, null_str, false, std::ref(result)));
		// gui2::tprogress_default_slot slot(std::bind(&net::cswamp_findapplet, _1, null_str, aplt::app_code_from_str(game_config::app), nposm, bundleid, null_str, false, std::ref(result)));
		// gui2::tprogress_default_slot slot(std::bind(&net::cswamp_findapplet, _1, null_str, aplt::app_launcher, nposm, bundleid, null_str, false, std::ref(result)));
		bool ret = gui2::run_with_progress(slot, null_str, _("Find applet"), 500);
		if (!ret) {
			return false;
		}

		utils::string_map symbols;
		symbols["bundleid"] = bundleid;
		if (result.empty()) {
			const std::string err = vgettext2("The store does not have a applet bundleid is '$bundleid'.", symbols);
			gui2::show_message(null_str, err);
			return false;
		}

		const aplt::tapplet& aplt = result[0];

		// check whether can run this launcher/kdesktop
		bool can_run_on = false;
		const std::vector<std::string> apps = utils::split(aplt.app);
		for (std::vector<std::string>::const_iterator it = apps.begin(); it != apps.end(); ++ it) {
			const std::string& app = *it;
			if (app == game_config::app) {
				can_run_on = true;
				break;
			}
		}

		symbols["aplt"] = aplt.name + "(" + bundleid + ")";
		if (!can_run_on) {
			const std::string err = vgettext2("$aplt cannot be run in this app.", symbols);
			gui2::show_message(null_str, err);
			return false;
		}

		const std::string msg = vgettext2("Do you want to install applet: $aplt?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			// continue running trcamera, let trcamera can scan new qrcode again.
			return false;
		}
		return true;
	}

private:
	trstore& owner_;
};

void trstore::click_qr_scan()
{
	utils::string_map symbols;
	symbols["website"] = _("cswamp(cswmap.com)"); 

	const std::string remark = vgettext2("Visit the '$website' website, switch to the 'Applet' page, enter the applet to be downloaded, move the mouse to the 'Download' button, and the download QR code will be displayed.", symbols);

	std::string result;
	{
		trstore_receiver receiver(*this);

		int retval = gui2::show_rcamera(camera_, receiver, remark, result);
		if (retval != gui2::twindow::OK) {
			return;
		}

		VALIDATE(!result.empty(), null_str);
	}
/*	
	symbols["aplt"] = result;
	const std::string msg = vgettext2("Do you want to install applet: $aplt?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}
*/
	const std::string bundileid = extract_bundileid_from_qrcode_str(result);
	bool ret = slot_.install_applet(applets_, aplt::src_distribution, bundileid);
	if (ret) {
		did_applets_changed();
	}
}

void trstore::reload_applets(const std::vector<aplt::tapplet>& applets, gui2::tlistbox& list, int type)
{
	list.clear();

	std::set<std::string> installed_bundleid;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		if (aplt.source == aplt::src_studio) {
			continue;
		}
		VALIDATE(installed_bundleid.count(aplt.bundleid) == 0, null_str);
		installed_bundleid.insert(aplt.bundleid);
	}

	std::map<std::string, std::string> data;
	for (std::vector<aplt::tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const aplt::tapplet& applet = *it;
		if (type == list_me && applet.source == aplt::src_studio) {
			continue;
		}

		data["icon"] = applet.icon.empty()? "misc/applet.png": applet.icon;
		data["title"] = applet.name2(); // applet.name;
		data["subtitle"] = applet.subtitle + "(" + applet.bundleid + ")";

		gui2::ttoggle_panel& row = list.insert_row(data);
		// widget.set_icon(applet.icon);
		if (type == list_recommend) {
			tbutton* button = find_widget<tbutton>(&row, "install", false, true);
			connect_signal_mouse_left_click(
					  *button
					, std::bind(
					&trstore::click_recommend_install
					, this, std::ref(row), std::ref(*button)));
			button->set_label("misc/download.png");

		} else if (type == list_find) {
			tbutton* button = find_widget<tbutton>(&row, "install", false, true);
			if (installed_bundleid.count(applet.bundleid) == 0) {
				connect_signal_mouse_left_click(
						  *button
						, std::bind(
						&trstore::click_find_install
						, this, std::ref(row)));
				button->set_label("misc/download.png");
			} else {
				button->set_visible(twidget::INVISIBLE);
			}

		} else if (type == list_me) {
			tbutton* button = find_widget<tbutton>(&row, "upgrade", false, true);
			connect_signal_mouse_left_click(
					  *button
					, std::bind(
					&trstore::click_me_upgrade
					, this, std::ref(row), std::ref(*button)));
		}
	}
}

void trstore::tslot::rstore_pre_show(twindow& window, tpanel& statusbar_widget)
{
	statusbar_widget.set_visible(twidget::INVISIBLE);
}

// Why the parameter @bundleid using 'std::string' instead of 'std::string&' ? 
// -- The caller may use the @applets[at].bundlid as @bundleid, and tslot::install_applet will modify the applets, 
//    it will result in unpredictable problems if use @bundlid.
bool trstore::tslot::install_applet(std::map<aplt::taplt_key, aplt::tapplet>& applets, int type, const std::string bundleid)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(type == aplt::src_distribution || type == aplt::src_development, null_str);

	if (!rstore_show_confirm_install(type, bundleid)) {
		return false;
	}

	// [1/3]download
	aplt::tapplet net_applet;
	std::function<void (int, const std::string&, const std::string&)> did_applet_will_uninstall = std::bind(&trstore::tslot::rstore_did_applet_will_uninstall, this, _1, _2, _3);
	// gui2::tprogress_default_slot slot(std::bind(&net::cswamp_getapplet, _1, type, bundleid, std::ref(did_applet_will_uninstall_), std::ref(net_applet), false), "misc/remove.png");
	gui2::tprogress_default_slot slot(std::bind(&net::cswamp_getapplet, _1, type, bundleid, 
		std::ref(did_applet_will_uninstall), std::ref(net_applet), false), "misc/remove.png");
	bool ret = gui2::run_with_progress(slot, null_str, _("Get applet"), 1);

	if (!ret) {
		return false;
	}

	// [2/3]synchronous applets
	// The order of requirements must be maintained: #1 src_distribution, #2 src_development, #3 src_studio
	bool equal = false;
	bool insert = false;

	std::map<aplt::taplt_key, aplt::tapplet>::iterator it;
	// first for: Find the applet that same bundleid but different source, erase it.
	for (it = applets.begin(); it != applets.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		if ((aplt.source == aplt::src_distribution || aplt.source == aplt::src_development) && aplt.bundleid == bundleid) {
			// if (aplt.source != net_applet.source) {
				applets.erase(it);
				break;
			// }
		}
	}

	applets.insert(std::make_pair(aplt::taplt_key(net_applet.source, net_applet.bundleid), net_applet));
/*
	// second for
	for (it = applets.begin(); it != applets.end(); ++ it) {
		aplt::tapplet& aplt = *it;
		if ((aplt.source == aplt::src_distribution || aplt.source == aplt::src_development) && aplt.bundleid == bundleid) {
			VALIDATE(aplt.source == net_applet.source, null_str);
			equal = true;
			// aplt = net_applet;
			break;

		} else if (aplt.source == aplt::src_development && net_applet.source == aplt::src_distribution) {
			insert = true;
			// applets_.insert(it, net_applet);
			break;

		} else if (aplt.source == aplt::src_studio) {
			insert = true;
			// applets_.insert(it, net_applet);
			break;
		}
	}

	if (equal) {
		aplt::tapplet& aplt = *it;
		aplt = net_applet;

	} else if (insert) {
		applets.insert(it, net_applet);

	} else {
		applets.push_back(net_applet);
	}
*/	
	// [3/3]notify installed
	did_applet_installed_or_uninstalled(net_applet.source, net_applet.bundleid);

	rstore_did_applet_installed(net_applet);
	return true;
}

bool trstore::tslot::rstore_show_confirm_uninstall(const aplt::tapplet& aplt)
{
	utils::string_map symbols;
	symbols["aplt"] = aplt.name2();
	return gui2::show_messagefs(vgettext2("Delete $aplt?", symbols), _("Delete")) == gui2::twindow::OK;
}

void trstore::tslot::uninstall_applet(std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::tapplet& desire)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(desire.source == aplt::src_distribution || desire.source == aplt::src_development, null_str);
	bool found_desire = false;
	bool has_studio = false;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		if (aplt.bundleid == desire.bundleid) {
			if (aplt.source == desire.source) {
				found_desire = true;
			} else if (aplt.source == aplt::src_studio) {
				has_studio = true; 
			}
		}
	}
	VALIDATE(found_desire, null_str);

	// @desire is most likely from @applets, below will modify @applets, so save source and bundlid first.
	const int desire_source = desire.source;
	const std::string desire_bundleid = desire.bundleid;
	const std::string lua_bundleid = utils::replace_all(desire_bundleid, ".", "_");
	const std::string preferences_aplt_path = game_config::preferences_dir + "/" + lua_bundleid;

	// [1/4]
	rstore_did_applet_will_uninstall(desire_source, desire_bundleid, preferences_aplt_path);
	// if (did_applet_will_uninstall_ != NULL) {
	//	did_applet_will_uninstall_(preferences_aplt_path);
	// }

	// [2/4] delete relative files
	SDL_DeleteFiles(preferences_aplt_path.c_str());
	// gui2::delete_file(progress, preferences_aplt_path);

	if (!has_studio) {
		SDL_DeleteFiles(get_aplt_user_data_dir(lua_bundleid).c_str());
	}

	// [3/4] synchronous applets
	for (std::map<aplt::taplt_key, aplt::tapplet>::iterator it = applets.begin(); it != applets.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		if (aplt.source == desire_source && aplt.bundleid == desire_bundleid) {
			applets.erase(it);
			break;
		}
	}

	// [4/4] notify unstalled
	did_applet_installed_or_uninstalled(desire_source, desire_bundleid);

	rstore_did_applet_uninstalled(desire_source, desire_bundleid);
}

void trstore::tslot::rstore_did_applet_will_uninstall(int source, const std::string& bundleid, const std::string& res_path)
{
	// Whether install or uninstall, it will be called.
	// Uninstall is easy to understand, why do install need to be called? 
	//  --I'm afraid there are already applet that I want to install. 
	//  For safety reasons, I need to uninstall them first.

	uninstall_point_.validate_nposm();
	// @scene_id use value, not reference. if ref, below base_driver_.stop_subtask() will clear it.
	const std::string scene_id = base_driver_.scene_id();
	int subtask_state = nposm;
	if (!scene_id.empty()) {
		// Why dneed to enter here even when 'subtask_state == sts_idle'?
		// -- when sts_idle, apltsotype_base2th's ref_count is 1. 
		//    However, during uninstall, apltsotype_base2th maybe to uninstalled.
		subtask_state = base_driver_.subtask_state();
		VALIDATE(subtask_state == aplt::sts_idle || subtask_state == aplt::sts_ing, null_str);
		base_driver_.stop_subtask(true);

		uninstall_point_.set(scene_id, subtask_state);
	}

	did_applet_will_uninstall(res_path);
}

void trstore::tslot::did_applet_installed_or_uninstalled(int source, const std::string& bundleid)
{
	{
		tbase_driver_core::tdisable_start_subtask_lock lock(base_driver_);
		// install applet[2/2]. open new libroseaplt.so
		drivers_.refresh();
	}

	bg_task_.apltnotfound_or_verdismatch_to_fresh(false);

	VALIDATE(base_driver_.subtask_state() == aplt::sts_nposm, str_cast(base_driver_.subtask_state()));
	VALIDATE(base_driver_.scene_id().empty(), base_driver_.scene_id());

	int to_subtask_state = nposm;
	const aplt::tbase_scene* scene = nullptr;
	if (!uninstall_point_.scene_id.empty()) {
		if (base_driver_.installed()) {
			const std::string& scene_id = uninstall_point_.scene_id;
			scene = cfg_cpp_api_.base_scene_from_id(uninstall_point_.scene_id, true);
			to_subtask_state = uninstall_point_.subtask_state;
		}
		uninstall_point_.clear();

	} else if (base_driver_.installed()) {
		const std::string scene_id = preferences::base_scene_id();
		if (!scene_id.empty()) {
			scene = cfg_cpp_api_.base_scene_from_id(scene_id, false);
			if (scene != nullptr) {
				to_subtask_state = aplt::sts_ing;
			}
		}
	}

	if (scene != nullptr) {
		if (to_subtask_state == aplt::sts_ing) {
			base_driver_.start_subtask_from_nposm(*scene);

		} else {
			VALIDATE(to_subtask_state == aplt::sts_idle, null_str);
			base_driver_.idle_subtask_from_empty(*scene);
		}
	}
}

void trstore::tslot::rstore_nullptr_slot_for_aplt_drivers(const aplt::tapplet& aplt)
{
	// example: kdesktop, only base_driver
	
	for (int type = 0; type <= apltsotype_maxdriver; type ++) {
		if (type == apltsotype_base) {
			// if hit, stop base. 
			if (base_driver_.aplt_id() == aplt.id) {
				base_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_moveit) { 
			// if hit, stop moveit. 
			// if (moveit_driver_.aplt_id() == aplt.id) {
			//	moveit_driver_.set_slot(null_str, nullptr);
			// }

		} else if (type == apltsotype_laser) {
			// if hit, stop laser. 
			// if (laser_driver_.aplt_id() == aplt.id) {
			//	laser_driver_.set_slot(null_str, nullptr);
			// }

		} else if (type == apltsotype_dcamera) {
			// if hit, stop dcamera. 
			// if (dcamera_driver_.aplt_id() == aplt.id) {
			//	dcamera_driver_.set_slot(null_str, nullptr);
			// }

		} else if (type == apltsotype_iot) {
			// if hit, stop iot. 
			// if (iot_driver_.aplt_id() == aplt.id) {
			//	iot_driver_.set_slot(null_str, nullptr);
			// }

		} else if (type == apltsotype_speech) {
			// if hit, stop speech. 
			// if (speech_driver_.aplt_id() == aplt.id) {
			//	speech_driver_.set_slot(null_str, nullptr);
			// }

		} else if (type == apltsotype_ai) {
			// if hit, stop aiagent. 
			// if (ai_driver_.aplt_id() == aplt.id) {
			//	ai_driver_.set_slot(null_str, nullptr);
			// }

		} else {
			VALIDATE(false, null_str);
		}
	}
}

void trstore::tslot::did_applet_will_uninstall(const std::string& res_path)
{
	VALIDATE(!bg_task_.is_ing(), null_str);

	// 1) New installation. res_path's applet does not exist.
	// 2) Upgrade. res_path's applet is existed.
	// 3) Uninstall. res_path's applet is existed.
	// The res_path's applet(desire_aplt) may or may not exist.
	const aplt::tapplet* desire_aplt = nullptr;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		if (aplt.res_path == res_path) {
			desire_aplt = &aplt;
			break;
		}
	}
	if (desire_aplt == nullptr) {
		VALIDATE(!drivers_.has_lib_using(res_path), null_str);
		return;
	}

	// if need, stop timing
	std::string stop_reason;
	if (drivers_.has_lib_using(res_path)) {
		stop_reason = _("One driver to be uninstalled");

	}
	if (!stop_reason.empty()) {
		instance->stop_bg_task_if_runing(null_str, stop_reason);
	}

	// did_applet_will_uninstall2(*desire_aplt);
	rstore_nullptr_slot_for_aplt_drivers(*desire_aplt);
/*
	for (int type = 0; type <= apltsotype_maxdriver; type ++) {
		if (type == apltsotype_base) {
			// if hit, stop base. 
			if (base_driver_.aplt_id() == desire_aplt->id) {
				base_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_moveit) { 
			// if hit, stop moveit. 
			if (moveit_driver_.aplt_id() == desire_aplt->id) {
				moveit_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_laser) {
			// if hit, stop laser. 
			if (laser_driver_.aplt_id() == desire_aplt->id) {
				laser_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_dcamera) {
			// if hit, stop dcamera. 
			if (dcamera_driver_.aplt_id() == desire_aplt->id) {
				dcamera_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_iot) {
			// if hit, stop iot. 
			if (iot_driver_.aplt_id() == desire_aplt->id) {
				iot_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_speech) {
			// if hit, stop speech. 
			if (speech_driver_.aplt_id() == desire_aplt->id) {
				speech_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_ai) {
			// if hit, stop aiagent. 
			if (ai_driver_.aplt_id() == desire_aplt->id) {
				ai_driver_.set_slot(null_str, nullptr);
			}

		} else {
			VALIDATE(false, null_str);
		}
	}
*/
	drivers_.did_uninstall(res_path);
}

void trstore::click_recommend_install(ttoggle_panel& row, tbutton& widget)
{
	int at = row.at();
	VALIDATE(at <= (int)got_recommend_applets_.size(), null_str);
	// std::string bundleid = "aplt.leagor.basic";
	// std::string bundleid = "aplt.leagor.iaccess";
	// std::string bundleid = "aplt.leagor.key";
	std::string bundleid = got_recommend_applets_[at].bundleid;
	bool ret = slot_.install_applet(applets_, aplt::src_distribution, bundleid);
	if (ret) {
		did_applets_changed();
	}
}

void trstore::did_applets_changed()
{
	// erase applets from got_recommend_applets_ that exists in applets_.
	// how insert to got_recommend_applets_? see update_recommend_grid.
	bool recommend_list_dirty = false;
	for (std::vector<aplt::tapplet>::iterator it = got_recommend_applets_.begin(); it != got_recommend_applets_.end(); ) {
		aplt::tapplet& recommend = *it;
		bool found = false;
		for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it2 = applets_.begin(); it2 != applets_.end(); ++ it2) {
			const aplt::tapplet& existed = it2->second;
			if (existed.source != aplt::src_studio && recommend.bundleid == existed.bundleid) {
				found = true;
				break;
			}
		}
		if (found) {
			recommend_list_dirty = true;
			VALIDATE(got_recommend_bundles_.count(recommend.bundleid) > 0, null_str);
			got_recommend_bundles_.erase(got_recommend_bundles_.find(recommend.bundleid));
			it = got_recommend_applets_.erase(it);

		} else {
			++ it;
		}
	}
	VALIDATE(got_recommend_bundles_.size() == got_recommend_bundles_.size(), null_str);
	if (recommend_list_dirty) {
		recommend_applets_list_->clear();
	}

	// since applets_ was changed, me_list awlays is true.
	bool me_list_dirty = true;
	if (me_list_dirty) {
		me_applets_list_->clear();
	}

	if (current_page_ == RECOMMEND_PAGE) {
		if (recommend_list_dirty) {
			update_recommend_grid();
		}

	} else if (current_page_ == ME_PAGE) {
		if (me_list_dirty) {
			update_me_grid();
		}
	}
}

void trstore::did_bundleid_changed(ttext_box& widget, bool find_page)
{
	const std::string& label = widget.label();
	if (find_page) {
		std::string title = label;
		utils::strip(title);
		find_find_button_->set_active((int)title.size() >= min_title_bytes_);
		// find_find_button_->set_active(utils::is_rose_bundleid(label, '.'));
	} else {
		devel_install_button_->set_active(utils::is_rose_bundleid(label, '.'));
	}
}

void trstore::click_find_find(tgrid& grid)
{
	ttext_box* text_box = find_widget<ttext_box>(&grid, "title", false, true);
	std::string title = text_box->label();
	utils::strip(title);
	VALIDATE((int)title.size() >= min_title_bytes_, null_str);

	// const std::string& bundleid = text_box->label();
	// VALIDATE(utils::is_rose_bundleid(bundleid, '.'), null_str);

	std::vector<aplt::tapplet>& result = find_applets_;
	gui2::tprogress_default_slot slot(std::bind(&net::cswamp_findapplet, _1, null_str, aplt::app_code_from_str(game_config::app), nposm, null_str, title, false, std::ref(result)));
	bool ret = gui2::run_with_progress(slot, null_str, _("Find applet"), 500);
	if (!ret) {
		return;
	}

	reload_applets(result, *find_applets_list_, list_find);
}

void trstore::click_find_install(ttoggle_panel& row)
{
	int at = row.at();
	VALIDATE(at <= (int)find_applets_.size(), null_str);
	std::string bundleid = find_applets_[at].bundleid;
	bool ret = slot_.install_applet(applets_, aplt::src_distribution, bundleid);
	if (ret) {
		did_applets_changed();
		find_widget<tbutton>(&row, "install", false, true)->set_visible(twidget::INVISIBLE);
	}
}

void trstore::click_me_upgrade(ttoggle_panel& row, tbutton& widget)
{
	int at = row.at();
	VALIDATE(at < (int)applets_.size(), null_str);
	const aplt::tapplet& aplt = aplt::aplt_from_at(applets_, at);
	if (aplt.source == aplt::src_development) {
		utils::string_map symbols;
		symbols["aplt"] = aplt.name;
		const std::string msg = vgettext2("Installed is devel version, and once upgraded, it will become a distribution. Want to upgrade?", symbols);
		int res = gui2::show_message2(aplt.name, msg, gui2::tmessage::yes_no_buttons);
		if (res == gui2::twindow::CANCEL) {
			return;
		}
	}

	bool ret = slot_.install_applet(applets_, aplt::src_distribution, aplt.bundleid);
	if (ret) {
		did_applets_changed();
	}
}

void trstore::click_me_devel_install(tgrid& grid)
{
	// ttext_box* text_box = find_widget<ttext_box>(&grid, "devel_bundleid", false, true);
	// const std::string& bundleid = text_box->label();
	// VALIDATE(utils::is_rose_bundleid(bundleid, '.'), null_str);

	std::string bundleid;
	{
		std::string title = _("Install devel version applet");
		// const std::string initial = "aplt.leagor.blesmart";
		const std::string initial;
		gui2::tedit_box_param param(title, null_str, "xxx.xxx.xxx", initial, null_str, null_str, dgettext("rose-lib", "OK"), 20, gui2::tedit_box_param::show_cancel);
		param.did_text_changed = std::bind(&utils::is_rose_bundleid, _1, '.');
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_->get_height() / 4);
			if (dlg.get_retval() != twindow::OK) {
				return;
			}
		}
		bundleid = param.result;
	}

	bool ret = slot_.install_applet(applets_, aplt::src_development, bundleid);
	if (ret) {
		did_applets_changed();
	}
}

}