#define GETTEXT_DOMAIN "launcher-lib"

#include "mkscript_unit_map.hpp"
#include "mkscript_display.hpp"
#include "mkscript_controller.hpp"
#include "gui/dialogs/mkscript_scene.hpp"

mkscript_unit_map::mkscript_unit_map(mkscript_controller& controller, const tmap& gmap, bool consistent)
	: base_map(controller, gmap, consistent)
	, controller_(controller)
{
}

void mkscript_unit_map::add(const map_location& loc, const base_unit* base_u)
{
	const mkscript_unit* u = dynamic_cast<const mkscript_unit*>(base_u);
	insert(loc, new mkscript_unit(*u));
}

mkscript_unit* mkscript_unit_map::find_unit(const map_location& loc) const
{
	return dynamic_cast<mkscript_unit*>(find_base_unit(loc));
}

mkscript_unit* mkscript_unit_map::find_unit(const map_location& loc, bool overlay) const
{
	return dynamic_cast<mkscript_unit*>(find_base_unit(loc, overlay));
}

void mkscript_unit_map::create_coor_map(int w, int h)
{
	VALIDATE(controller_.initialized(), null_str);
	VALIDATE((w % MKSCRIPT_UNIT_LOCS) == 0 && (h % MKSCRIPT_UNIT_LOCS) == 0, null_str);
	const int desire_map_vsize = (w / MKSCRIPT_UNIT_LOCS) * (h / MKSCRIPT_UNIT_LOCS);

	const int original_map_vsize = map_vsize_;
	base_unit** original_map = NULL;
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

		mkscript_display& disp = controller_.get_display();
		int at = 0;
		const int zoom = disp.zoom();
		const int unit_size = zoom * MKSCRIPT_UNIT_LOCS;
		for (int row = 0; row < h; row += MKSCRIPT_UNIT_LOCS) {
			for (int col = 0; col < w; col += MKSCRIPT_UNIT_LOCS) {
				SDL_Rect rect{col * zoom, row * zoom, unit_size, unit_size};
				base_unit* u = nullptr;
				if (at < original_map_vsize) {
					u = original_map[at];
					u->clear_rect();
				} else {
					u = new mkscript_unit(controller_, disp, *this);
				}
				u->set_rect(rect);
				insert2(disp, u);

				const map_location& loc = u->get_location();
				VALIDATE(loc.x == col && loc.y == row, null_str);
				at ++;
			}
		}

		VALIDATE(at == map_vsize_, null_str);
		VALIDATE(w * h == map_vsize_ * MKSCRIPT_UNIT_LOCS * MKSCRIPT_UNIT_LOCS, null_str);
	// }
	if (original_map != nullptr) {
		free(original_map);
	}
}