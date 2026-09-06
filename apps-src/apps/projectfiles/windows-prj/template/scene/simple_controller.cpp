#define GETTEXT_DOMAIN "studio-lib"

/*
 * How to use...
 * display_lock lock(game.disp());
 * hotkey::scope_changer changer(game.app_cfg(), "hotkey_ocr");
 * simple_controller controller(game.app_cfg(), game.video());
 * controller.initialize(display::ZOOM_72);
 * int ret = controller.main_loop();
 */

#include "simple_controller.hpp"
#include "simple_display.hpp"
#include "gui/dialogs/simple_scene.hpp"

simple_controller::simple_controller(const config& app_cfg, CVideo& video)
	: base_controller(SDL_GetTicks(), app_cfg, video)
	, gui_(nullptr)
	, dlg_(nullptr)
	, window_(nullptr)
	, map_(null_str)
	, units_(*this, map_, false)
{
	map_ = tmap(generate_map_data(12, 8, true, square_terrain_blue));
	units_.create_coor_map(map_.w(), map_.h());
}

simple_controller::~simple_controller()
{
	if (gui_) {
		delete gui_;
		gui_ = nullptr;
	}
}

void simple_controller::app_create_display(int initial_zoom)
{
	gui_ = new simple_display(*this, units_, video_, map_, initial_zoom);
}

void simple_controller::app_post_initialize()
{
	dlg_ = static_cast<gui2::tsimple_scene*>(gui_->get_theme());
	window_ = dlg_->get_window();
}

void simple_controller::app_execute_command(int command, const std::string& sparam)
{
	using namespace gui2;

	switch (command) {
		case tsimple_scene::HOTKEY_RETURN:
			do_quit_ = true;
			break;

		case tsimple_scene::HOTKEY_SHARE:
			// share();
			break;

		case HOTKEY_ZOOM_IN:
			gui_->set_zoom(ZOOM_INCREMENT);
			break;
		case HOTKEY_ZOOM_OUT:
			gui_->set_zoom(-ZOOM_INCREMENT);
			break;

		case HOTKEY_SYSTEM:
			break;

		default:
			base_controller::app_execute_command(command, sparam);
	}
}

void simple_controller::app_first_drawn()
{
}

void simple_controller::app_resize_screen()
{
	gui_->app_resize_screen();
}