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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/rlogin.hpp"

#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/panel.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/menu.hpp"
#include "help.hpp"
#include "filesystem.hpp"
#include <time.h>
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "rose_version.hpp"

using namespace std::placeholders;


namespace gui2 {

void trlogin::tslot::rlogin_click_login(const std::string& username, const std::string& password)
{
	rlogin->get_window()->set_retval(twindow::OK);
}

REGISTER_DIALOG(rose, rlogin)

trlogin::trlogin(tslot& slot, const std::string& title, const std::string& login)
	: slot_(slot)
	, title_(title)
	, login_label_(login)
	, min_username_chars_(1)
	, max_username_chars_(16)
	, min_password_chars_(1)
	, max_password_chars_(16)
	, title_widget_(nullptr)
	, login_widget_(nullptr)
	, username_(nullptr)
	, password_(nullptr)
{
	slot_.rlogin = this;
	set_timer_interval(10000);
}

void trlogin::pre_show()
{
	window_->set_escape_disabled(true);
	// window_->set_label("misc/bg_ffffff.png");

	title_widget_ = find_widget<tlabel>(window_, "title", false, true);
	title_widget_->set_label(title_);

	// username
	username_ = new ttext_box2(*window_, *find_widget<tcontrol>(window_, "username", false, true), null_str, "misc/device.png");
	username_->text_box()->set_maximum_chars(max_username_chars_);
	username_->text_box()->set_placeholder(_("Username"));
	username_->set_did_text_changed(std::bind(&trlogin::did_text_box_changed, this, _1));

	password_ = new ttext_box2(*window_, *find_widget<tcontrol>(window_, "password", false, true), null_str, "misc/password.png", true, "misc/eye.png", ttext_box2::button_auto_visible);
	connect_signal_mouse_left_click(
		*password_->button()
		, std::bind(
			&trlogin::click_password
			, this));

	// password
	password_->text_box()->set_maximum_chars(max_password_chars_);
	password_->text_box()->set_placeholder(_("Password"));
	password_->set_did_text_changed(std::bind(&trlogin::did_text_box_changed, this, _1));

	login_widget_ = find_widget<tbutton>(window_, "login", false, true);
	login_widget_->set_label(login_label_);
	login_widget_->set_canvas_variable("border", variant("login2_border"));
	login_widget_->set_active(slot_.rlogin_can_login(username_->text_box()->label(), password_->text_box()->label()));
	connect_signal_mouse_left_click(
		*login_widget_
		, std::bind(
			&trlogin::click_login
			, this
			, std::ref(*window_)));
}

void trlogin::post_show()
{
	slot_.rlogin_username = username_->text_box()->label();
	slot_.rlogin_password = password_->text_box()->label();
	slot_.rlogin = nullptr;
}

void trlogin::did_text_box_changed(ttext_box& widget)
{
	const std::string& username = username_->text_box()->label();
	const std::string& password = password_->text_box()->label();

	slot_.rlogin_did_text_box_changed(username, password);
	login_widget_->set_active(slot_.rlogin_can_login(username, password));
}

void trlogin::click_password()
{
	password_->text_box()->set_cipher(!password_->text_box()->cipher());
}

void trlogin::click_login(twindow& window)
{
	const std::string& username = username_->text_box()->label();
	const std::string& password = password_->text_box()->label();

	VALIDATE(slot_.rlogin_can_login(username, password), null_str);
	slot_.rlogin_click_login(username, password);
}


//
// trcswamp_login
//
trcswamp_login::trcswamp_login(net::truser& current_user)
	: current_user_(current_user)
	, login_stack_(nullptr)
	, login_type_report_(nullptr)
	, login_type_stack_(nullptr)
	, current_login_layer_(nposm)
	, use_text_box2_(false)
	, pd_username_(nullptr)
	, pd_password_(nullptr)
	, ck_username_(nullptr)
	, ck_cookie_(nullptr)
	, username_text_(nullptr)
	, password_text_(nullptr)
	, login_widget_(nullptr)
	, window_priv_(nullptr)
{}

void trcswamp_login::pre_show(twindow& window, tgrid& grid)
{
	VALIDATE(window_priv_ == nullptr && login_stack_ == nullptr, null_str);
	window_priv_ = &window;

	tstack* stack = find_widget<tstack>(&grid, "login_stack", false, true);
	pre_login(*stack->layer(LOGIN_LAYER));
	pre_logout(*stack->layer(LOGOUT_LAYER));
	login_stack_ = stack;

	set_login_layer();
}

void trcswamp_login::pre_login(tgrid& grid)
{
	std::stringstream ss;

	utils::string_map symbols;
	symbols["cswamp"] = "www.cswamp.com";
	if (game_config::app == "launcher") {
		ss << vgettext2("login remark, $cswamp, launcher", symbols);
	} else {
		ss << vgettext2("login remark, $cswamp, kdesktop", symbols);
	}
	find_widget<tlabel>(&grid, "login_remark", false, true)->set_label(ss.str());

	find_widget<tlabel>(&grid, "deviceid", false, true)->set_label(current_user_.deviceid);

	tbutton* button = find_widget<tbutton>(&grid, "login", false, true);
		connect_signal_mouse_left_click(
					*button
				, std::bind(
				&trcswamp_login::click_login
				, this, std::ref(*button)));
	button->set_active(false);
	login_widget_ = button;

	treport* report = find_widget<treport>(&grid, "login_type_report", false, true);
	gui2::tcontrol* item = &report->insert_item(null_str, _("Password"));
	item = &report->insert_item(null_str, _("Cookie"));
	report->set_did_item_changed(std::bind(&trcswamp_login::did_login_type_changed, this, _1, _2));
	login_type_report_ = report;
	login_type_report_->set_visible(twidget::INVISIBLE);

	tstack* stack = find_widget<tstack>(&grid, "login_type_stack", false, true);
	pre_login_type_password(*stack->layer(LOGIN_TYPE_PASSWORD_LAYER));
	pre_login_type_cookie(*stack->layer(LOGIN_TYPE_COOKIE_LAYER));
	login_type_stack_ = stack;

	if (use_text_box2_) {
		pd_username_->text_box()->set_label(current_user_.username);

		ck_username_->text_box()->set_label(current_user_.username);
		ck_cookie_->text_box()->set_label(current_user_.pwcookie);

	} else {
		username_text_->set_label(current_user_.username);
	}

	report->select_item(LOGIN_TYPE_PASSWORD_LAYER);
}

void trcswamp_login::pre_logout(tgrid& grid)
{
	std::stringstream ss;

	utils::string_map symbols;
	symbols["cswamp"] = "www.cswamp.com";
	if (game_config::app == "launcher") {
		ss << vgettext2("logout remark, $cswamp, launcher", symbols);
	} else {
		ss << vgettext2("logout remark, $cswamp, kdesktop", symbols);
	}
	find_widget<tlabel>(&grid, "logout_remark", false, true)->set_label(ss.str());

	find_widget<tlabel>(&grid, "deviceid", false, true)->set_label(current_user_.deviceid);

	tbutton* button = find_widget<tbutton>(&grid, "logout", false, true);
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&trcswamp_login::click_logout
			, this, std::ref(*button)));
}

void trcswamp_login::did_login_type_changed(treport& report, ttoggle_button& row)
{
	tgrid* current_layer = login_type_stack_->layer(row.at());
	login_type_stack_->set_radio_layer(row.at());

	current_login_layer_ = row.at();
	if (current_login_layer_ == LOGIN_TYPE_PASSWORD_LAYER) {
		if (use_text_box2_) {
			set_login_active(pd_username_->text_box()->label(), pd_password_->text_box()->label());
		} else {
			set_login_active(username_text_->label(), password_text_->label());
		}

	} else if (current_login_layer_ == LOGIN_TYPE_COOKIE_LAYER) {
		ck_username_->text_box()->set_label(current_user_.username);
		ck_cookie_->text_box()->set_label(current_user_.pwcookie);

		set_login_active(ck_username_->text_box()->label(), ck_cookie_->text_box()->label());
	}
}

void trcswamp_login::pre_login_type_password(tgrid& grid)
{
	if (use_text_box2_) {
		ttext_box2* text_box2 = new ttext_box2(*window_priv_, *find_widget<tcontrol>(&grid, "username", false, true), null_str, null_str);
		text_box2->set_did_text_changed(std::bind(&trcswamp_login::did_username_changed, this, _1, LOGIN_TYPE_PASSWORD_LAYER));
		text_box2->text_box()->set_maximum_chars(22);
		text_box2->text_box()->set_placeholder(_("Username"));
		pd_username_ = text_box2;

		text_box2 = new ttext_box2(*window_priv_, *find_widget<tcontrol>(&grid, "password", false, true), null_str, null_str, true, "misc/eye.png", ttext_box2::button_auto_visible);
		connect_signal_mouse_left_click(
			*text_box2->button()
			, std::bind(
				&trcswamp_login::click_password
				, this, LOGIN_TYPE_PASSWORD_LAYER));
		text_box2->text_box()->set_maximum_chars(22);
		text_box2->text_box()->set_placeholder(_("Password"));
		text_box2->set_did_text_changed(std::bind(&trcswamp_login::did_password_changed, this, _1, LOGIN_TYPE_PASSWORD_LAYER));
		pd_password_ = text_box2;

	} else {
		tpanel* panel = find_widget<tpanel>(&grid, "username", false, true);
		panel->set_border(null_str);
		panel->set_margin(0, 0, 0, 0);

		ttext_box* text_box = find_widget<ttext_box>(&grid, "username_text", false, true);
		text_box->set_did_text_changed(std::bind(&trcswamp_login::did_username_changed, this, _1, LOGIN_TYPE_PASSWORD_LAYER));
		text_box->set_maximum_chars(22);
		text_box->set_placeholder(_("Username"));
		text_box->set_border(null_str);
		username_text_ = text_box;

		// password
		panel = find_widget<tpanel>(&grid, "password", false, true);
		panel->set_border(null_str);
		panel->set_margin(0, 0, 0, 0);

		text_box = find_widget<ttext_box>(&grid, "password_text", false, true);
		text_box->set_maximum_chars(22);
		text_box->set_placeholder(_("Password"));
		text_box->set_did_text_changed(std::bind(&trcswamp_login::did_password_changed, this, _1, LOGIN_TYPE_PASSWORD_LAYER));
		text_box->set_desensitize();
		text_box->set_border(null_str);
		password_text_ = text_box;

		tbutton* button = find_widget<tbutton>(&grid, "password_button", false, true);
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&trcswamp_login::click_password
				, this, LOGIN_TYPE_PASSWORD_LAYER));
	}
}

void trcswamp_login::pre_login_type_cookie(tgrid& grid)
{
	if (!use_text_box2_) {
		return;
	}
	ttext_box2* text_box2 = new ttext_box2(*window_priv_, *find_widget<tcontrol>(&grid, "username", false, true), null_str, null_str, false, null_str, ttext_box2::button_auto_visible);
	// text_box2->set_did_text_changed(std::bind(&thome::did_username_changed, this, _1, LOGIN_COOKIE_LAYER));
	// text_box2->text_box()->set_maximum_chars(22);
	text_box2->text_box()->set_placeholder(_("Username"));
	text_box2->text_box()->set_active(false);
	ck_username_ = text_box2;

	text_box2 = new ttext_box2(*window_priv_, *find_widget<tcontrol>(&grid, "password", false, true), null_str, null_str, true, "misc/eye.png", ttext_box2::button_auto_visible);
	connect_signal_mouse_left_click(
		*text_box2->button()
		, std::bind(
			&trcswamp_login::click_password
			, this, LOGIN_TYPE_COOKIE_LAYER));
	// text_box2->text_box()->set_maximum_chars(22);
	// text_box2->text_box()->set_placeholder(_("Password"));
	// text_box2->set_did_text_changed(std::bind(&thome::did_password_changed, this, _1, LOGIN_COOKIE_LAYER));
	text_box2->text_box()->set_active(false);
	ck_cookie_ = text_box2;
}

void trcswamp_login::did_username_changed(ttext_box& widget, int layer)
{
	if (layer == LOGIN_TYPE_PASSWORD_LAYER) {
		if (use_text_box2_) {
			set_login_active(widget.label(), pd_password_->text_box()->label());

		} else {
			set_login_active(widget.label(), password_text_->label());
		}
	}
}

void trcswamp_login::did_password_changed(ttext_box& widget, int layer)
{
	if (layer == LOGIN_TYPE_PASSWORD_LAYER) {
		if (use_text_box2_) {
			set_login_active(pd_username_->text_box()->label(), widget.label());
		} else {
			set_login_active(username_text_->label(), widget.label());
		}
	}
}

void trcswamp_login::click_password(int layer)
{
	ttext_box* text_box = nullptr;
	if (layer == LOGIN_TYPE_PASSWORD_LAYER) {
		if (use_text_box2_) {
			text_box = pd_password_->text_box();
		} else {
			text_box = password_text_;
		}

	} else {
		VALIDATE(layer == LOGIN_TYPE_COOKIE_LAYER, null_str);
		text_box = ck_cookie_->text_box();
	}
	text_box->set_cipher(!text_box->cipher());
}

void trcswamp_login::set_login_active(const std::string& username, const std::string& password)
{
	bool active = !username.empty() && !password.empty();
	// if (active) {
	//	active = current_user.valid();
	// }
	login_widget_->set_active(active);
}

void trcswamp_login::set_login_layer()
{
	int layer = nposm;
	if (current_user_.valid()) {
		// already logged in
		layer = LOGOUT_LAYER;

	} else {
		// not Logged In
		layer = LOGIN_LAYER;
	}
	login_stack_->set_radio_layer(layer);
}

void trcswamp_login::click_login(tbutton& widget)
{
	const std::string& username = use_text_box2_? pd_username_->text_box()->label(): username_text_->label();
	VALIDATE(!username.empty(), null_str);
	const std::string& password = use_text_box2_? pd_password_->text_box()->label(): password_text_->label();
	VALIDATE(!password.empty(), null_str);

	bool ret = net::cswamp_login2(current_user_, net::cswamp_login_type_password, username, password, 10000, false);
	if (ret) {
		ttext_box* password_widget = use_text_box2_? pd_password_->text_box(): password_text_;
		password_widget->set_label(null_str);

		set_login_layer();
		app_did_login_status_changed(true);

	} else if (!current_user_.pwcookie.empty()) {
		current_user_.pwcookie.clear();
		preferences::set_login_pwcookie(current_user_.pwcookie);
	}
}

void trcswamp_login::click_logout(tbutton& widget)
{
	VALIDATE(current_user_.valid(), null_str);

	gui2::tprogress_default_slot slot(std::bind(&net::cswamp_logout, _1, current_user_.sessionid, false));
	gui2::run_with_progress(slot, null_str, _("Logout"), 1500);
	current_user_.did_logout();

	set_login_layer();
	app_did_login_status_changed(false);
}

} // namespace gui2

