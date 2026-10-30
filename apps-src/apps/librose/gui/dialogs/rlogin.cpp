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
#include "gui/dialogs/register.hpp"
#include "help.hpp"
#include "filesystem.hpp"
#include <time.h>
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"
#include "rose_version.hpp"
#include "font.hpp"

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
	, username_type_report_(nullptr)
	, use_text_box2_(false)
	, pd_username_(nullptr)
	, pd_password_(nullptr)
	, ck_username_(nullptr)
	, ck_cookie_(nullptr)
	, username_text_(nullptr)
	, password_text_(nullptr)
	, login_widget_(nullptr)
	, logged_in_username_widget_(nullptr)
	, logged_in_uid_widget_(nullptr)
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

static std::string get_username_placeholder(int username_type)
{
	return username_type == net::cswamp_login_usernametype_mobile? _("Mobile number"): _("Username");
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
	tlabel* label = find_widget<tlabel>(&grid, "login_remark", false, true);
	label->set_label(ss.str());
	if (game_config::app_code == aplt::app_kdesktop) {
		label->set_visible(twidget::INVISIBLE);
	}

	find_widget<tlabel>(&grid, "deviceid", false, true)->set_label(current_user_.deviceid);
	if (game_config::app_code == aplt::app_kdesktop) {
		find_widget<tgrid>(&grid, "deviceid_grid", false, true)->set_visible(twidget::INVISIBLE);
	}

	tbutton* button = find_widget<tbutton>(&grid, "login", false, true);
		connect_signal_mouse_left_click(
					*button
				, std::bind(
				&trcswamp_login::click_login
				, this, std::ref(*button)));
	button->set_border("blue_ellipse");
	button->set_active(false);
	login_widget_ = button;

	button = find_widget<tbutton>(&grid, "register", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&trcswamp_login::click_register
			, this, tregister::type_register));
	button->set_label(ht::generate_format(_("Register now"), color_to_uint32(font::BLUE_COLOR)));

	treport* report = find_widget<treport>(&grid, "username_type_report", false, true);
	for (std::map<int, tcode3>::const_iterator it = net::cswamp_login_usernametypes.begin(); it != net::cswamp_login_usernametypes.end(); ++ it) {
		const tcode3& code3 = it->second;
		report->insert_item(null_str, code3.name);
	}
	report->set_did_item_changed(std::bind(&trcswamp_login::did_username_type_changed, this, _1, _2));
	int curr_username_type = net::cswamp_username_type();
	report->select_item(curr_username_type);
	username_type_report_ = report;

	pre_login_type_password(grid);

	if (use_text_box2_) {
		pd_username_->text_box()->set_label(current_user_.username);

		ck_username_->text_box()->set_label(current_user_.username);
		ck_cookie_->text_box()->set_label(current_user_.pwcookie);

	} else {
		const std::string& username2 = curr_username_type == net::cswamp_login_usernametype_mobile? 
			current_user_.mobile: current_user_.username;
		username_text_->set_label(username2);
	}

	if (use_text_box2_) {
		set_login_active(pd_username_->text_box()->label(), pd_password_->text_box()->label());
	} else {
		set_login_active(username_text_->label(), password_text_->label());
	}
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
	tlabel* label = find_widget<tlabel>(&grid, "logout_remark", false, true);
	label->set_label(ss.str());
	if (game_config::app_code == aplt::app_kdesktop) {
		label->set_visible(twidget::INVISIBLE);
	}

	tbutton* button = find_widget<tbutton>(&grid, "logged_in_username", false, true);
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&trcswamp_login::click_logged_in_username
			, this, std::ref(*button)));
	button->set_icon("misc/edit.png");
	logged_in_username_widget_ = button;

	label = find_widget<tlabel>(&grid, "uid", false, true);
	logged_in_uid_widget_ = label;

	button = find_widget<tbutton>(&grid, "erase_user", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&trcswamp_login::click_register
			, this, tregister::type_eraseuser));
	button->set_label(ht::generate_format(_("login^Erase user"), color_to_uint32(font::BLUE_COLOR)));
	{
		// Wait until the new UI is ready, then enable it.
		button->set_visible(twidget::INVISIBLE);
	}

	find_widget<tlabel>(&grid, "deviceid", false, true)->set_label(current_user_.deviceid);
	if (game_config::app_code == aplt::app_kdesktop) {
		find_widget<tgrid>(&grid, "deviceid_grid", false, true)->set_visible(twidget::INVISIBLE);
	}

	button = find_widget<tbutton>(&grid, "logout", false, true);
	connect_signal_mouse_left_click(
				*button
			, std::bind(
			&trcswamp_login::click_logout
			, this, std::ref(*button)));
	// button->set_border("blue_ellipse");
	button->set_label(ht::generate_format(_("Logout"), color_to_uint32(font::BAD_COLOR)));
}

void trcswamp_login::did_username_type_changed(treport& report, ttoggle_button& row)
{
	int at = row.at();
	preferences::set_login_username_type(net::cswamp_login_usernametypes.find(at)->second.id);

	int username_type = net::cswamp_username_type();
	if (username_text_ != nullptr) {
		username_text_->set_placeholder(get_username_placeholder(username_type));
		// username_text_->set_dirty();
	}

	if (username_text_ != nullptr && password_text_ != nullptr) {
		if (use_text_box2_) {
			set_login_active(pd_username_->text_box()->label(), pd_password_->text_box()->label());
		} else {
			set_login_active(username_text_->label(), password_text_->label());
		}
	}
}

void trcswamp_login::pre_login_type_password(tgrid& grid)
{
	if (use_text_box2_) {
		ttext_box2* text_box2 = new ttext_box2(*window_priv_, *find_widget<tcontrol>(&grid, "username", false, true), null_str, null_str);
		text_box2->set_did_text_changed(std::bind(&trcswamp_login::did_username_changed, this, _1));
		text_box2->text_box()->set_maximum_chars(22);
		text_box2->text_box()->set_placeholder(_("Username"));
		pd_username_ = text_box2;

		text_box2 = new ttext_box2(*window_priv_, *find_widget<tcontrol>(&grid, "password", false, true), null_str, null_str, true, "misc/eye.png", ttext_box2::button_auto_visible);
		connect_signal_mouse_left_click(
			*text_box2->button()
			, std::bind(
				&trcswamp_login::click_password
				, this));
		text_box2->text_box()->set_maximum_chars(22);
		text_box2->text_box()->set_placeholder(_("Password"));
		text_box2->set_did_text_changed(std::bind(&trcswamp_login::did_password_changed, this, _1));
		pd_password_ = text_box2;

	} else {
		int username_type = net::cswamp_username_type();

		tpanel* panel = find_widget<tpanel>(&grid, "username", false, true);
		panel->set_border(null_str);
		panel->set_margin(0, 0, 0, 0);

		ttext_box* text_box = find_widget<ttext_box>(&grid, "username_text", false, true);
		text_box->set_did_text_changed(std::bind(&trcswamp_login::did_username_changed, this, _1));
		text_box->set_maximum_chars(22);
		text_box->set_placeholder(get_username_placeholder(username_type));
		text_box->set_border(null_str);
		username_text_ = text_box;

		// password
		panel = find_widget<tpanel>(&grid, "password", false, true);
		panel->set_border(null_str);
		panel->set_margin(0, 0, 0, 0);

		text_box = find_widget<ttext_box>(&grid, "password_text", false, true);
		text_box->set_maximum_chars(22);
		text_box->set_placeholder(_("Password"));
		text_box->set_did_text_changed(std::bind(&trcswamp_login::did_password_changed, this, _1));
		text_box->set_desensitize();
		text_box->set_border(null_str);
		password_text_ = text_box;

		tbutton* button = find_widget<tbutton>(&grid, "password_button", false, true);
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&trcswamp_login::click_password
				, this));
	}

	tbutton* button = find_widget<tbutton>(&grid, "resetpwd", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&trcswamp_login::click_register
			, this, tregister::type_resetpwd));
	button->set_label(ht::generate_format(_("Forgot password?"), color_to_uint32(font::BLUE_COLOR)));
}

void trcswamp_login::did_username_changed(ttext_box& widget)
{
	if (use_text_box2_) {
		set_login_active(widget.label(), pd_password_->text_box()->label());

	} else {
		set_login_active(widget.label(), password_text_->label());
	}
}

void trcswamp_login::did_password_changed(ttext_box& widget)
{
	if (use_text_box2_) {
		set_login_active(pd_username_->text_box()->label(), widget.label());
	} else {
		set_login_active(username_text_->label(), widget.label());
	}
}

void trcswamp_login::click_password()
{
	ttext_box* text_box = nullptr;
	if (use_text_box2_) {
		text_box = pd_password_->text_box();
	} else {
		text_box = password_text_;
	}

	text_box->set_cipher(!text_box->cipher());
}

void trcswamp_login::set_login_active(const std::string& username, const std::string& password)
{
	bool username_valid = !username.empty();
	if (username_valid) {
		int username_type = net::cswamp_username_type();
		if (username_type == net::cswamp_login_usernametype_mobile) {
			username_valid = utils::is_valid_mobile(username);	
		}
	}

	bool active = username_valid && !password.empty();
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
		logged_in_username_widget_->set_label(current_user_.username);
		logged_in_uid_widget_->set_label(str_cast(current_user_.uid));

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

	int username_type = net::cswamp_username_type();
	bool ret = net::cswamp_login2(current_user_, username_type, net::cswamp_login_passwordtype_password, username, password, 10000, false);
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

bool trcswamp_login::did_verify_username(const std::string& _label) const
{
	if (_label.empty()) {
		return false;
	}

	std::string label = _label;
	utils::strip(label);
	if (label.empty()) {
		return false;
	}

	return true;
}

void trcswamp_login::click_logged_in_username(tbutton& widget)
{
	VALIDATE(current_user_.valid(), null_str);

	utils::string_map symbols;
	std::string title = _("Update username");
	std::string initial = current_user_.username;

	gui2::tedit_box_param param(title, _("Username"), null_str, initial, null_str, null_str, _("OK"), 16, gui2::tedit_box_param::show_cancel);
	param.did_text_changed = std::bind(&trcswamp_login::did_verify_username, this, _1);

	std::string new_username;
	{
		gui2::tedit_box dlg(param);
		dlg.show(nposm, window_priv_->get_height() / 5);
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
		new_username = param.result;
	}
	VALIDATE(current_user_.username != new_username, null_str);
	bool ret = net::cswamp_updateusername2(current_user_, new_username, nposm, false);

	if (ret) {
		VALIDATE(current_user_.username == preferences::login_username(), null_str);
		logged_in_username_widget_->set_label(current_user_.username);
	}
}

void trcswamp_login::click_register(int type)
{
	std::string caller_mobile;
	int username_type = net::cswamp_username_type();
	// if (username_type == net::cswamp_login_usernametype_mobile) {
		caller_mobile = use_text_box2_? pd_username_->text_box()->label(): username_text_->label();
	// }
	if (!caller_mobile.empty() && !utils::is_valid_mobile(caller_mobile)) {
		caller_mobile.clear();
	}

	gui2::tregister dlg(current_user_, type, caller_mobile);
	dlg.show();

	if (type == tregister::type_register) {
		if (current_user_.valid()) {
			set_login_layer();
			app_did_login_status_changed(true);
		}

	} else if (type == tregister::type_eraseuser) {
		if (!current_user_.valid()) {
			set_login_layer();
			app_did_login_status_changed(false);
		}
	}
}

} // namespace gui2

