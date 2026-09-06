#ifndef HEALTH_DISPLAY_HPP_INCLUDED
#define HEALTH_DISPLAY_HPP_INCLUDED

#include "display.hpp"
// #include "rdp_server_rose.h"
// #include "pble2.hpp"
#include "halo.hpp"

class health_controller;
class health_unit_map;
class health_unit;

class trhealth_scene_slot
{
public:
	virtual void pre_show(gui2::twindow& window) {}
	virtual void timer_handler(uint32_t now) {}
};

class health_display : public display
{
public:
	health_display(trhealth_scene_slot& scene_slot, health_controller& controller, health_unit_map& units, CVideo& video, const tmap& map, int initial_zoom);
	~health_display();

	bool in_theme() const override { return true; }
	health_controller& get_controller() { return controller_; }

protected:
	void draw_sidebar();

private:
	gui2::tdialog* app_create_scene_dlg() override;
	void app_post_initialize() override;

	void add_haloes() override;
	void clear_haloes();


private:
	trhealth_scene_slot& scene_slot_;
	health_controller& controller_;
	health_unit_map& units_;

	struct tbase_halos
	{
	public:
		tbase_halos()
			: title(halo::NO_HALO)
			, x_axis(halo::NO_HALO)
			, y_axis(halo::NO_HALO)
		{}

		~tbase_halos()
		{
			// VALIDATE(x_axis == halo::NO_HALO, null_str);
			// VALIDATE(y_axis == halo::NO_HALO, null_str);
		}

		virtual void clear()
		{
			if (title != halo::NO_HALO) {
				halo::remove(title);
				title = halo::NO_HALO;
			}
			if (x_axis != halo::NO_HALO) {
				halo::remove(x_axis);
				x_axis = halo::NO_HALO;
			}
			if (y_axis != halo::NO_HALO) {
				halo::remove(y_axis);
				y_axis = halo::NO_HALO;
			}
		}

	public:
		int title;
		int x_axis;
		int y_axis;
	};

	struct tposture_halos: public tbase_halos
	{
		tposture_halos()
		{}

		void clear() override
		{
			tbase_halos::clear();
		}
	};
	tposture_halos posture_halos_;

	struct troutine_halos: public tbase_halos
	{
		troutine_halos()
		{}

		void clear() override
		{
			tbase_halos::clear();
		}
	};
	troutine_halos routine_halos_;

	struct tworkout_halos: public tbase_halos
	{
		tworkout_halos()
		{}

		void clear() override
		{
			tbase_halos::clear();
		}
	};
	tworkout_halos workout_halos_;
};

#endif
