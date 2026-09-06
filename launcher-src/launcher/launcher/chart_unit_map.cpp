#define GETTEXT_DOMAIN "launcher-lib"

#include "chart_unit_map.hpp"
#include "chart_display.hpp"
#include "chart_controller.hpp"
#include "gui/dialogs/chart_scene.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/report.hpp"
#include "game_config.hpp"

chart_unit_map::chart_unit_map(chart_controller& controller, const tmap& gmap, bool consistent)
	: base_map(controller, gmap, consistent)
	, controller_(controller)
{
}

void chart_unit_map::create_coor_map(int w, int h)
{
	size_t orignal_map_vsize = map_vsize_;
	base_unit** orignal_map = NULL;
	if (orignal_map_vsize) {
		VALIDATE(w * h >= (int)orignal_map_vsize, "desire map size must not less than orignal nodes!"); 
		orignal_map = (base_unit**)malloc(map_vsize_ * sizeof(base_unit*));
		memcpy(orignal_map, map_, map_vsize_ * sizeof(base_unit*));
	}
	base_map::clear();
	base_map::create_coor_map(w, h);
	if (orignal_map_vsize) {
		map_vsize_ = orignal_map_vsize;
		memcpy(map_, orignal_map, map_vsize_ * sizeof(base_unit*));

		for (int i = 0; i < map_vsize_; i ++) {
			base_unit* n = map_[i];
			const map_location& loc = n->get_location();
			int pitch = loc.y * w;
			if (n->base()) {
				coor_map_[pitch + loc.x].base = n;
			} else {
				coor_map_[pitch + loc.x].overlay = n;
			}
		}
	}
}

void chart_unit_map::add(const map_location& loc, const base_unit* base_u)
{
	const chart_unit* u = dynamic_cast<const chart_unit*>(base_u);
	insert(loc, new chart_unit(*u));
}

chart_unit* chart_unit_map::find_unit(const map_location& loc) const
{
	return dynamic_cast<chart_unit*>(find_base_unit(loc));
}

chart_unit* chart_unit_map::find_unit(const map_location& loc, bool overlay) const
{
	return dynamic_cast<chart_unit*>(find_base_unit(loc, overlay));
}

void chart_unit_map::left_shift_units(int count)
{
	for (int at = count; at < map_vsize_; at ++) {
		chart_unit* from = static_cast<chart_unit*>(map_[at]);

		chart_unit* to = static_cast<chart_unit*>(map_[at - count]);
		to->set_samples(from->samples(), from->pixels_per_sample(), from->chart_data());

		from->clear_samples();
	}

}