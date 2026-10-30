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

#include "gui/dialogs/register.hpp"

// #include "game_config.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/text_box2.hpp"
#include "gui/dialogs/message.hpp"
#include "formula_string_utils.hpp"
#include "help.hpp"
#include "filesystem.hpp"
#include <time.h>
#include "wml_exception.hpp"
#include "language.hpp"
// #include "net.hpp"
// #include "hero.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, register)

tregister::tregister(net::truser& current_user, int type, const std::string& caller_mobile)
	: current_user_(current_user)
	, type_(type)
	, caller_mobile_(caller_mobile)
	, register_(nullptr)
	, next_sms_ticks_(0)
{
	VALIDATE(type_ >= 0 && type_ < type_count, null_str);
}

void tregister::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	vericode_timer_.reset(400, *window_, std::bind(&tregister::vericode_timer_handler, this));

	std::stringstream ss;
	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	if (type_ == type_resetpwd) {
		label->set_label(_("Forgot password"));

	} else if (type_ == type_eraseuser) {
		label->set_label(_("login^Erase user"));
	}

	tbutton* button = find_widget<tbutton>(window_, "cancel", false, true);
	button->set_icon("misc/back.png");

	get_vericode_ = find_widget<tbutton>(window_, "get_vericode", false, true);
	get_vericode_->set_label(_("Get"));
	get_vericode_->set_canvas_variable("border", variant("login2_border"));
	get_vericode_->set_active(false);
	connect_signal_mouse_left_click(
		*get_vericode_
		, std::bind(
			&tregister::click_get_vericode
			, this));

	register_ = find_widget<tbutton>(window_, "register", false, true);
	if (type_ == type_register) {
		register_->set_label(_("Register"));

	} else if (type_ == type_resetpwd) {
		register_->set_label(_("Reset password"));

	} else {
		register_->set_label(_("login^Erase user"));
	}
	register_->set_canvas_variable("border", variant("login2_border"));
	register_->set_active(false);
	connect_signal_mouse_left_click(
		*register_
		, std::bind(
			&tregister::click_register
			, this));

	button = find_widget<tbutton>(window_, "agreement", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tregister::user_agreement
			, this
			, std::ref(*window_)));

	utils::string_map symbols;
	symbols["agreement"] = "Sesame User Agreement";
	std::string agreement = vgettext2("I have read and agreed to $agreement", symbols);
	button->set_label(agreement);

	// mobile
	// mobile_ = new ttext_box2(*window_, *find_widget<twidget>(window_, "mobile", false, true), null_str, "misc/mobile.png");
	mobile_ = new ttext_box2(*window_, *find_widget<twidget>(window_, "mobile", false, true), null_str, null_str);
	mobile_->set_did_text_changed(std::bind(&tregister::did_field_changed, this));
	mobile_->text_box()->set_maximum_chars(22);
	mobile_->text_box()->set_placeholder(_("Mobile number"));

	// verification code
	// vericode_ = new ttext_box2(*window_, *find_widget<twidget>(window_, "vericode", false, true), null_str, "misc/email.png", false, null_str, false);
	vericode_ = new ttext_box2(*window_, *find_widget<twidget>(window_, "vericode", false, true), null_str, null_str, false, null_str, false);
	vericode_->set_did_text_changed(std::bind(&tregister::did_field_changed, this));
	vericode_->text_box()->set_maximum_chars(net::max_vericode_size);
	ss.str("");
	ss << _("Verification code");
	vericode_->text_box()->set_placeholder(ss.str());

	// password
	// password_ = new ttext_box2(*window_, *find_widget<tcontrol>(window_, "password", false, true), null_str, "misc/password.png", true, "misc/eye.png", ttext_box2::button_auto_visible);	
	password_ = new ttext_box2(*window_, *find_widget<tcontrol>(window_, "password", false, true), null_str, null_str, true, "misc/eye.png", ttext_box2::button_auto_visible);
	connect_signal_mouse_left_click(
		*password_->button()
		, std::bind(
			&tregister::clear_password
			, this
			, std::ref(*window_)));

	password_->text_box()->set_maximum_chars(22);
	password_->text_box()->set_placeholder(_("Password"));
	password_->set_did_text_changed(std::bind(&tregister::did_field_changed, this));
	// connect_signal_pre_key_press(password_.text_box(), std::bind(&tregister::signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));

	tgrid& bonus_grid = find_widget<tgrid>(window_, "bonus_grid", false);
	bonus_grid.set_visible(twidget::INVISIBLE);

	// When calling, must wait until the relevant 'xxxx_widget_' pointer is valid.
	mobile_->text_box()->set_label(caller_mobile_);
}

void tregister::post_show()
{
	vericode_timer_.reset();
}

void tregister::set_register_active(const std::string& mobile, const std::string& vericode, const std::string& password)
{
	bool active = utils::is_valid_mobile(mobile);
	if (active) {
		active = !password.empty();
	}
	if (active) {
		active = net::check_vericode_size(vericode.size());
	}

	register_->set_active(active);
}

void tregister::set_vericode_active(const std::string& mobile)
{
	get_vericode_->set_active(!next_sms_ticks_ && utils::is_valid_mobile(mobile));
}

void tregister::did_field_changed()
{
	set_register_active(mobile_->text_box()->label(), vericode_->text_box()->label(), password_->text_box()->label());
	set_vericode_active(mobile_->text_box()->label());
}

void tregister::clear_password(twindow& window)
{
	password_->text_box()->set_cipher(!password_->text_box()->cipher());
}

void tregister::set_retval(twindow& window, int retval)
{
	window.set_retval(retval);
}

void tregister::user_agreement(twindow& window)
{
}

void tregister::click_get_vericode()
{
	int vericode_type = net::cswamp_vericodetype_register;
	if (type_ == type_resetpwd) {
		vericode_type = net::cswamp_vericodetype_resetpwd;

	} else if (type_ == type_eraseuser) {
		vericode_type = net::cswamp_vericodetype_eraseuser;
	}

	gui2::tprogress_default_slot slot(std::bind(&net::cswamp_vericode, _1, mobile_->text_box()->label(), vericode_type, 
		nposm, false));
	bool ret = run_with_progress(slot, null_str, null_str, 2000);
	if (ret) {
		get_vericode_->set_active(false);
		next_sms_ticks_ = SDL_GetTicks() + net::vericode_reserve_secs * 1000;
	}
}

void tregister::click_register()
{
	std::string msg;
	if (type_ == type_eraseuser) {
		msg = _("login^erase user warning");
	}
	if (!msg.empty()) {
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons, 
			null_str, null_str, _("login^Erase user")) != gui2::twindow::OK) {
			return;
		}
	}

	std::string mobile = mobile_->text_box()->label();
	std::string password = password_->text_box()->label();

	bool back_login = false;
	if (type_ == type_register) {
		bool ret = net::cswamp_register2(current_user_, mobile, password, vericode_->text_box()->label(), 10000, false);
		// bool ret = net::cswamp_login2(current_user_, net::cswamp_login_usernametype_mobile, 
		//	net::cswamp_login_passwordtype_password, mobile, password, nposm, false);
		if (ret) {

		} else if (!current_user_.pwcookie.empty()) {
			current_user_.pwcookie.clear();
			preferences::set_login_pwcookie(current_user_.pwcookie);
		}
		if (current_user_.valid()) {
			back_login = true;
		}

	} else if (type_ == type_resetpwd) {
		bool ret = net::cswamp_resetpwd2(current_user_, mobile, vericode_->text_box()->label(), password, 6000, false);
		if (ret) {
			back_login = true;
		} else {

		}

	} else {
		VALIDATE(type_ == type_eraseuser, null_str);
		bool ret = net::cswamp_eraseuser2(current_user_, vericode_->text_box()->label(), nposm, false);
		if (ret) {
			back_login = true;
		}
	}

	if (back_login) {
		window_->set_retval(twindow::OK);
	}
}

void tregister::vericode_timer_handler()
{
	if (next_sms_ticks_) {
		Uint32 now = SDL_GetTicks();
		std::stringstream ss;

		if (now < next_sms_ticks_) {
			int remainder = (next_sms_ticks_ - now) / 1000 + 1;
			ss << remainder << dsgettext("rose-lib", "time^s");
		} else {
			next_sms_ticks_ = 0;
			ss << _("Get");
		}
		get_vericode_->set_label(ss.str());
		set_vericode_active(mobile_->text_box()->label());
	}
}

void tregister::signal_handler_sdl_key_down(bool& handled
		, bool& halt
		, const SDL_Keycode key
		, SDL_Keymod modifier
		, const Uint16 unicode)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	if (key == SDLK_PRINTSCREEN) {
		tcontrol* ceil = find_widget<tcontrol>(window_, "login_ceil", false, true);
		ceil->set_visible(twidget::INVISIBLE);
	}
#endif

#ifdef _WIN32
	if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && !(modifier & KMOD_SHIFT)) {
		handled = true;
		halt = true;
	}
#else
	if (key == SDLK_RETURN) {
		tcontrol* ceil = find_widget<tcontrol>(window_, "login_ceil", false, true);
		ceil->set_visible(twidget::VISIBLE);

		handled = true;
		halt = true;
	}
#endif
}

} // namespace gui2

