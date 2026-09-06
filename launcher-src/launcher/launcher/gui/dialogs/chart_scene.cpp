#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/chart_scene.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/stack.hpp"

#include "formula_string_utils.hpp"
#include "chart_display.hpp"
#include "chart_controller.hpp"
#include "hotkeys.hpp"

#include "gettext.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(launcher, chart_scene);

tchart_scene::tchart_scene(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, chart_display& disp, chart_controller& controller)
	: tdialog(&controller)
	, tstatusbar(rdpd_mgr, pble, privacy)
	, controller_(controller)
{
}

void tchart_scene::pre_show()
{
	// prepare status report.
	reports_.insert(std::make_pair(ZOOM, "zoom"));
	reports_.insert(std::make_pair(POSITION, "position"));
	reports_.insert(std::make_pair(FORMAT, "format"));
	reports_.insert(std::make_pair(STATUS, "status"));
	reports_.insert(std::make_pair(VOICE_MAYBE_START, "icon_voice_maybe_start"));
	reports_.insert(std::make_pair(ALLOW_SHORT_VOICE, "icon_allow_short_voice"));

	// prepare hotkey
	hotkey::insert_hotkey(HOTKEY_RETURN, "return", null_str);
	hotkey::insert_hotkey(HOTKEY_RECORD, "record", null_str);
	hotkey::insert_hotkey(HOTKEY_PLAY, "play", null_str);

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid(), &reports_);

	std::stringstream err;
	utils::string_map symbols;

	tbutton* widget = dynamic_cast<tbutton*>(get_object("return"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("record"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("play"));
	click_generic_handler(*widget, null_str);

	std::stringstream ss;
	gui2::ttrack* tag = dynamic_cast<gui2::ttrack*>(get_object("tag"));
	tag->set_did_draw(std::bind(&chart_controller::did_draw_tag, &controller_, _1, _2, _3, nposm));

	gui2::ttrack* tag1 = dynamic_cast<gui2::ttrack*>(get_object("tag1"));
	tag1->set_did_draw(std::bind(&chart_controller::did_draw_float_tag1, &controller_, _1, _2, _3, nposm));

	tag1->connect_signal<event::LEFT_BUTTON_DOWN>(
			std::bind(
				&chart_controller::signal_handler_left_button_down
				, &controller_
				, tag1
				, _5)
			, event::tdispatcher::back_child);
	tag1->connect_signal<event::MOUSE_LEAVE>(
			std::bind(
				&chart_controller::callback_control_drag_detect
				, &controller_
				, tag1
				, _5)
			, event::tdispatcher::back_child);
	tag1->connect_signal<event::MOUSE_MOTION>(
			std::bind(
				&chart_controller::callback_set_drag_coordinate
				, &controller_
				, tag1
				, _5)
			, event::tdispatcher::back_child);

	window_->find_float_widget("tag1")->set_visible(false);

	if (controller_.show_mic_wave()) {
		find_widget<tcontrol>(window_, "_mini_map", false, true)->set_visible(twidget::INVISIBLE);
	}
	find_widget<tbutton>(window_, "play", false, true)->set_visible(twidget::INVISIBLE);
}

void tchart_scene::app_first_drawn()
{
	controller_.app_first_drawn();
}

void tchart_scene::app_resize_screen()
{
	controller_.app_resize_screen();
}

void tchart_scene::statusbar_refresh_report(int num, const std::string& label)
{
	controller_.gui().refresh_report(num, reports::report(label, null_str));
}

} //end namespace gui2
