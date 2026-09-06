#define GETTEXT_DOMAIN "studio-lib"

#include "simple_unit.hpp"
#include "gettext.hpp"
#include "simple_display.hpp"
#include "simple_controller.hpp"
#include "gui/dialogs/simple_scene.hpp"

simple_unit::simple_unit(simple_controller& controller, simple_display& disp, simple_unit_map& units)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
{
}

void simple_unit::app_draw_unit(const int xsrc, const int ysrc)
{
}