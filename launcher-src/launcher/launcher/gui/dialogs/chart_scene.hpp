/* $Id: lobby_player_info.hpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Tomasz Sniatowski <kailoran@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_CHART_SCENE_HPP_INCLUDED
#define GUI_DIALOGS_CHART_SCENE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "config.hpp"
#include "statusbar.hpp"

class chart_display;
class chart_controller;
class unit_map;

namespace gui2 {

class tchart_scene: public tdialog, public tstatusbar
{
public:
	enum {ZOOM = DERIVED_REP_MIN, POSITION, FORMAT, STATUS, VOICE_MAYBE_START, ALLOW_SHORT_VOICE, NUM_REPORTS};

	enum {
		HOTKEY_RETURN = HOTKEY_MIN,
		HOTKEY_RECORD,
		HOTKEY_PLAY,
	};

	tchart_scene(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, chart_display& disp, chart_controller& controller);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	const std::string& window_id() const override;

	void pre_show() override;
	void app_first_drawn() override;
	void app_resize_screen() override;

	void statusbar_refresh_report(int num, const std::string& label) override;

private:
	chart_controller& controller_;
};

} //end namespace gui2

#endif
