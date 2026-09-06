#define GETTEXT_DOMAIN "rose-lib"

#include "health_unit_map.hpp"
#include "health_display.hpp"
#include "health_controller.hpp"
#include "gui/dialogs/health_scene.hpp"

health_unit_map::health_unit_map(health_controller& controller, const tmap& gmap, bool consistent)
	: base_map(controller, gmap, consistent)
	, controller_(controller)
{
}

void health_unit_map::add(const map_location& loc, const base_unit* base_u)
{
	const health_unit* u = dynamic_cast<const health_unit*>(base_u);
	insert(loc, new health_unit(*u));
}

health_unit* health_unit_map::find_unit(const map_location& loc) const
{
	return dynamic_cast<health_unit*>(find_base_unit(loc));
}

health_unit* health_unit_map::find_unit(const map_location& loc, bool overlay) const
{
	return dynamic_cast<health_unit*>(find_base_unit(loc, overlay));
}

void health_unit_map::create_coor_map(int w, int h)
{
	VALIDATE(controller_.initialized(), null_str);
	VALIDATE((w % HEAL_UNIT_LOCS) == 0 && (h % HEAL_UNIT_LOCS) == 0, null_str);
	const int desire_map_vsize = (w / HEAL_UNIT_LOCS) * (h / HEAL_UNIT_LOCS);

	const int original_map_vsize = map_vsize_;
	base_unit** original_map = nullptr;
	if (original_map_vsize != 0) {
		// VALIDATE(desire_map_vsize >= original_map_vsize, "desire map size must not less than orignal nodes!"); 
		original_map = (base_unit**)malloc(map_vsize_ * sizeof(base_unit*));
		memcpy(original_map, map_, map_vsize_ * sizeof(base_unit*));
	}

	if (map_ != nullptr) {
		memset(map_, 0, map_size_ * sizeof(base_unit*));
		map_vsize_ = 0;
	}
	if (coor_map_ != nullptr) {
		memset(coor_map_, 0, w_ * h_ * sizeof(loc_cookie));
	}

	base_map::clear();
	base_map::create_coor_map(w, h);
	// if (original_map_vsize != 0) {
		// map_vsize_ = orignal_map_vsize;
		// memcpy(map_, orignal_map, map_vsize_ * sizeof(base_unit*));

		health_display& disp = controller_.get_display();
		int at = 0;
		const int zoom = disp.zoom();
		const int unit_size = zoom * HEAL_UNIT_LOCS;
		for (int row = 0; row < h; row += HEAL_UNIT_LOCS) {
			for (int col = 0; col < w; col += HEAL_UNIT_LOCS) {
				SDL_Rect rect{col * zoom, row * zoom, unit_size, unit_size};
				base_unit* u = nullptr;
				if (at < original_map_vsize) {
					u = original_map[at];
					u->clear_rect();
				} else {
					u = new health_unit(controller_, disp, *this);
				}
				u->set_rect(rect);
				insert2(disp, u);

				const map_location& loc = u->get_location();
				VALIDATE(loc.x == col && loc.y == row, null_str);
				at ++;
			}
		}

		VALIDATE(at == map_vsize_, null_str);
		VALIDATE(w * h == map_vsize_ * HEAL_UNIT_LOCS * HEAL_UNIT_LOCS, null_str);
	// }
	if (original_map != nullptr) {
		free(original_map);
	}
}