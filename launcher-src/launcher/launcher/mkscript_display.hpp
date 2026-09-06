#ifndef MKSCRIPT_DISPLAY_HPP_INCLUDED
#define MKSCRIPT_DISPLAY_HPP_INCLUDED

#include "display.hpp"
#include "rdp_server_rose.h"
#include "pble2.hpp"

class mkscript_controller;
class mkscript_unit_map;
class mkscript_unit;

namespace gui2 {
class treport;
}

class mkscript_display : public display
{
public:
	mkscript_display(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, mkscript_controller& controller, mkscript_unit_map& units, CVideo& video, const tmap& map, int initial_zoom);
	~mkscript_display();

	bool in_theme() const override { return true; }
	mkscript_controller& get_controller() { return controller_; }
	
protected:
	void draw_sidebar();

private:
	gui2::tdialog* app_create_scene_dlg() override;
	void app_post_initialize() override;
	void app_draw_minimap_units(surface& screen) override;

	void reload_widget_palette();
	void scroll_top(gui2::treport& widget);
	void scroll_bottom(gui2::treport& widget);

private:
	net::trdpd_manager& rdpd_mgr_;
	tpble2& pble_;
	tprivacy& privacy_;
	mkscript_controller& controller_;
	mkscript_unit_map& units_;

	gui2::treport* widget_palette_;
};

#endif
