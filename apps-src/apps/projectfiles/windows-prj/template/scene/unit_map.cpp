#define GETTEXT_DOMAIN "studio-lib"

#include "simple_unit_map.hpp"
#include "simple_display.hpp"
#include "simple_controller.hpp"
#include "gui/dialogs/simple_scene.hpp"

simple_unit_map::simple_unit_map(simple_controller& controller, const tmap& gmap, bool consistent)
	: base_map(controller, gmap, consistent)
{
}

void simple_unit_map::add(const map_location& loc, const base_unit* base_u)
{
	const simple_unit* u = dynamic_cast<const simple_unit*>(base_u);
	insert(loc, new simple_unit(*u));
}

simple_unit* simple_unit_map::find_unit(const map_location& loc) const
{
	return dynamic_cast<simple_unit*>(find_base_unit(loc));
}

simple_unit* simple_unit_map::find_unit(const map_location& loc, bool overlay) const
{
	return dynamic_cast<simple_unit*>(find_base_unit(loc, overlay));
}