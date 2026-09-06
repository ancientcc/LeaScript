#ifndef SIMPLE_CONTROLLER_HPP
#define SIMPLE_CONTROLLER_HPP

#include "base_controller.hpp"
#include "mouse_handler_base.hpp"
#include "simple_display.hpp"
#include "simple_unit_map.hpp"
#include "map.hpp"

namespace gui2 {
class tsimple_scene;
}

class simple_controller : public base_controller, public events::mouse_handler_base
{
public:
	simple_controller(const config &app_cfg, CVideo& video);
	~simple_controller();

	simple_display& gui() { return *gui_; }
	const simple_display& gui() const { return *gui_; }
	events::mouse_handler_base& get_mouse_handler_base() override { return *this; }
	simple_display& get_display() override { return *gui_; }
	const simple_display& get_display() const override { return *gui_; }

	simple_unit_map& get_units() override { return units_; }
	const simple_unit_map& get_units() const override { return units_; }

	void app_first_drawn();
	void app_resize_screen();

private:
	void app_create_display(int initial_zoom) override;
	void app_post_initialize() override;

	void app_execute_command(int command, const std::string& sparam) override;

private:
	simple_unit_map units_;
	tmap map_;
	simple_display* gui_;
	gui2::tsimple_scene* dlg_;
	gui2::twindow* window_;
};

#endif
