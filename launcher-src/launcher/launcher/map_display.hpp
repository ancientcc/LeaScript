#ifndef MAP_DISPLAY_HPP_INCLUDED
#define MAP_DISPLAY_HPP_INCLUDED

#include "display.hpp"
#include "rdp_server_rose.h"
#include "pble2.hpp"

class tros_instance;
class tdcamera_driver;
class map_controller;
class map_unit_map;
class map_unit;
class tcamera;

class map_display: public display
{
public:
	map_display(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tros_instance& ros_instance, tdcamera_driver& dcamera_driver, tcamera& camera, map_controller& controller, map_unit_map& units, CVideo& video, const tmap& map, int initial_zoom);
	~map_display();

	bool in_theme() const override { return true; }
	map_controller& get_controller() { return controller_; }
	
protected:
	void draw_sidebar();
	void app_pre_set_zoom(int new_zoom) override;
	void app_post_set_zoom(int old_zoom) override;
	void app_draw_minimap_units(surface& screen) override;

private:
	gui2::tdialog* app_create_scene_dlg() override;
	void app_post_initialize() override;
	void add_haloes() override;
	void did_post_scroll(int dx, int dy) override;

private:
	net::trdpd_manager& rdpd_mgr_;
	tpble2& pble_;
	tprivacy& privacy_;
	tros_instance& ros_instance_;
	tdcamera_driver& dcamera_driver_;
	tcamera& camera_;
	map_controller& controller_;
	map_unit_map& units_;
};

#endif
