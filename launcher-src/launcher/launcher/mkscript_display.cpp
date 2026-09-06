#define GETTEXT_DOMAIN "launcher-lib"

#include "mkscript_display.hpp"
#include "mkscript_controller.hpp"
#include "mkscript_unit_map.hpp"
#include "gui/dialogs/mkscript_scene.hpp"
#include "gui/widgets/report.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "halo.hpp"
#include "formula_string_utils.hpp"

using namespace std::placeholders;

mkscript_display::mkscript_display(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, mkscript_controller& controller, mkscript_unit_map& units, CVideo& video, const tmap& map, int initial_zoom)
	: display(game_config::tile_square, controller, video, &map, gui2::tmkscript_scene::NUM_REPORTS, initial_zoom)
	, rdpd_mgr_(rdpd_mgr)
	, pble_(pble)
	, privacy_(privacy)
	, controller_(controller)
	, units_(units)
	, widget_palette_(nullptr)
{
	min_zoom_ = 64;
	max_zoom_ = 1024;

	show_hover_over_ = false;
	// set_grid_style(display::grid_frame);
}

mkscript_display::~mkscript_display()
{
}

gui2::tdialog* mkscript_display::app_create_scene_dlg()
{
	return new gui2::tmkscript_scene(rdpd_mgr_, pble_, privacy_, controller_);
}

void mkscript_display::app_post_initialize()
{
	widget_palette_ = dynamic_cast<gui2::treport*>(get_theme_object("widget_palette"));
	reload_widget_palette();
}

void mkscript_display::app_draw_minimap_units(surface& screen)
{
	// double xscaling = 1.0 * minimap_location_.w / map_->w();
	// double yscaling = 1.0 * minimap_location_.h / map_->h();

	const SDL_Rect map_rect = main_map_rect();
	VALIDATE(map_rect.x == 0 && map_rect.y == 0 && map_rect.w == map_->w() * hex_width() && map_rect.h == map_->h() * hex_width(), null_str);

	double xscaling = 1.0 * minimap_location_.w / (map_->w() * hex_width());
	double yscaling = 1.0 * minimap_location_.h / (map_->h() * hex_width());

	surface label_surf;
	int item_count = nposm;
	const mkscript_controller::tdraw_item_C* items = controller_.draw_items(item_count);
	SDL_Color col = font::BLACK_COLOR;
	const Uint32 edge_mapped_col = SDL_MapRGB(screen->format, col.r, col.g, col.b);
	for (int at = 0; at < item_count; at ++) {
		const mkscript_controller::tdraw_item_C& item = items[at];
		
		if (item.obj->shape.type == visio::shape_edge) {
			mkscript_controller::tedge* edge = static_cast<mkscript_controller::tedge*>(item.obj);

			SDL_Point start;
			SDL_Point end;
			edge->start_end_SDL_Points(start, end);

			start.x = minimap_location_.x + round_double(start.x * xscaling);
			start.y = minimap_location_.y + round_double(start.y * yscaling);
			end.x = minimap_location_.x + round_double(end.x * xscaling);
			end.y = minimap_location_.y + round_double(end.y * yscaling);

			if (start.x < end.x) {
				draw_line(screen, edge_mapped_col, start.x, start.y, end.x, end.y);
			} else {
				draw_line(screen, edge_mapped_col, end.x, end.y, start.x, start.y);
			}
			continue;

		} else if (!shape_type_is_state(item.obj->shape.type)) {
			continue;
		}

		SDL_Size item_size{item.rect.w, item.rect.h};
		VALIDATE(item_size.w > 0 && item_size.h > 0, null_str);

		const SDL_Rect& rect = item.rect;
		double u_x = rect.x * xscaling;
		double u_y = rect.y * yscaling;
		double u_w = rect.w * xscaling;
		double u_h = rect.h * yscaling;

		SDL_Color col = item.obj->shape.type == visio::shape_voice_state? font::GRAY_COLOR: font::BAD_COLOR;
/*
		if (u->cell().id != "_main_map") {
			int level = 1;	// level = 1 is red.
			for (std::vector<SDL_Rect>::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
				const SDL_Rect& that = *it;
				if (rects_overlap(that, rect)) {
					level ++;
				}
			}
			col = candidates[level % candidates.size()];;
		}
		rects.push_back(rect);
*/
		const Uint32 mapped_col = SDL_MapRGB(screen->format, col.r, col.g, col.b);

		int minimap_u_x = minimap_location_.x + round_double(u_x);
		int minimap_u_y = minimap_location_.y + round_double(u_y);

		SDL_Rect r{minimap_u_x, minimap_u_y, round_double(u_w), round_double(u_h)};
		bool use_frame = false;
		if (use_frame) {
			draw_rectangle(r.x , r.y, r.w, r.w, mapped_col, screen);

		} else {
			sdl_fill_rect(screen, &r, mapped_col);
		}

		label_surf = font::get_rendered_text(str_cast(item.obj->obj_at + 1), INT_MAX, font::SIZE_SMALLEST, font::BLACK_COLOR);
		SDL_Rect dst_rect{minimap_u_x, minimap_u_y, label_surf->w, label_surf->h};
		sdl_blit(label_surf, nullptr, screen, &dst_rect);
	}
}

void mkscript_display::draw_sidebar()
{
	// Fill in the terrain report
	if (map_->on_board_with_border(mouseoverHex_)) {
		refresh_report(gui2::tmkscript_scene::POSITION, reports::report(lexical_cast<std::string>(mouseoverHex_), null_str));
	}
	std::stringstream ss;
	ss << zoom_ << "(" << int(get_zoom_factor() * 100) << "%)";
	refresh_report(gui2::tmkscript_scene::ZOOM, reports::report(ss.str(), null_str));
}

void post_widget_button(gui2::twindow& window, gui2::tcontrol& widget, mkscript_controller& controller, const visio::tshape& shape)
{
	widget.set_tooltip(shape.name);

	widget.connect_signal<gui2::event::LONGPRESS>(
		std::bind(
			&mkscript_controller::longpress_widget, &controller,
			_4, _5, std::ref(window), std::ref(shape)), gui2::event::tdispatcher::back_child);
}

void mkscript_display::reload_widget_palette()
{
	widget_palette_->clear();

	gui2::tdialog* dlg = get_theme();
	gui2::twindow& window = *dlg->get_window();

	const std::map<int, visio::tshape>& shape_types = controller_.shape_types();
	for (std::map<int, visio::tshape>::const_iterator it = shape_types.begin(); it != shape_types.end(); ++ it) {
		const visio::tshape& shape = it->second;
		if (!shape_type_is_state(shape.type)) {
			continue;
		}
		gui2::tcontrol& widget = widget_palette_->insert_item(null_str, shape.name);
		widget.set_icon(shape.stem_png);
		widget.set_cookie(shape.type);
		post_widget_button(window, widget, controller_, shape);
	}

	scroll_top(*widget_palette_);
}
/*
void mkscript_display::reload_scroll_header()
{
	std::vector<std::string> images;
	images.push_back("misc/widget_palette.png");
	images.push_back("misc/object_list.png");

	scroll_header_->clear();
	
	int index = 0;
	for (std::vector<std::string>::const_iterator it = images.begin(); it != images.end(); ++ it, index ++) {
		gui2::tcontrol& widget = scroll_header_->insert_item(null_str, null_str);
		widget.set_icon(*it);
		widget.set_cookie(index);
	}
	scroll_header_->select_item(0);
}
*/
void mkscript_display::scroll_top(gui2::treport& widget)
{
	widget.scroll_vertical_scrollbar(gui2::tscrollbar_::BEGIN);
}

void mkscript_display::scroll_bottom(gui2::treport& widget)
{
	widget.scroll_vertical_scrollbar(gui2::tscrollbar_::END);
}