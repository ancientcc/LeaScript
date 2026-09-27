#define GETTEXT_DOMAIN "launcher-lib"

/*
 * How to use...
 * display_lock lock(game.disp());
 * hotkey::scope_changer changer(game.app_cfg(), "hotkey_ocr");
 * mkscript_controller controller(game.app_cfg(), game.video());
 * controller.initialize(display::ZOOM_72);
 * int ret = controller.main_loop();
 */

#include "mkscript_controller.hpp"
#include "mkscript_display.hpp"
#include "gui/dialogs/mkscript_scene.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/browse.hpp"
#include "gui/dialogs/pose_state2.hpp"
#include "gui/dialogs/speak_state2.hpp"
#include "gui/dialogs/new_wkoscript.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/window.hpp"
#include "hotkeys.hpp"
#include "gettext.hpp"
#include "cairo2.hpp"
#include "angles/angles.h"
#include "aplt.hpp"
#include "game_config.hpp"
#include <opencv2/imgproc.hpp>

using namespace std::placeholders;


#define WKO_LMK33_PNG_WIDTH		1280
#define WKO_LMK33_PNG_HEIGHT	720

surface save_lmk33_png_from_surf(const surface& src_surf, aplt::twkoscript::tstate2& state2, const std::string& phase_surf_dir, int phase_at)
{
	surface surf = src_surf;
	int restrict_width = WKO_LMK33_PNG_WIDTH;
	int restrict_height = WKO_LMK33_PNG_HEIGHT;
	if (surf->w != restrict_width || surf->h != restrict_height) {
		surface bg = create_neutral_surface(restrict_width, restrict_height);
		fill_surface(bg, 0xffffffff);
		tpoint ratio_size = calculate_adaption_ratio_size(restrict_width, restrict_height, surf->w, surf->h);
		surf = scale_surface(surf, ratio_size.x, ratio_size.y);
		SDL_Rect dst{(restrict_width - ratio_size.x) / 2, (restrict_height - ratio_size.y) / 2, ratio_size.x, ratio_size.y};
		sdl_blit(surf, nullptr, bg, &dst);
		surf = bg;
	}
	if (!SDL_IsDirectory(phase_surf_dir.c_str())) {
		SDL_MakeDirectory(phase_surf_dir.c_str());
	}
	std::string filename2 = state2.build_lmk33_png_filename(phase_surf_dir, phase_at);
	imwrite(surf, filename2);

	return surf;
}

extern aplt::twkoscript::tpose& insert_preset_pose(const aplt::tpreset_pose& preset,
	const SDL_FPoint* landmarks, bool landmarks_valid, aplt::twkoscript::tstate2& state2);

bool calc_surf_landmarks(mediapipe::tpose_tracking_api& api, const surface& surf, SDL_FPoint* landmarks)
{
	VALIDATE(surf.get() != nullptr, null_str);

	tsurface_2_mat_lock lock(surf);
	bool flip_h_ = false;

	bool is_less_than_min_interval = false;
	cv::Mat output_frame_mat = api.next_image(lock.mat, flip_h_, landmarks, is_less_than_min_interval);
	VALIDATE(!is_less_than_min_interval, null_str);
	bool landmarks_valid = !output_frame_mat.empty();
	for (int at = 0; at < mediapipe::kNumPoseLandmarks && landmarks_valid; at ++) {
		if (std::isnan(landmarks[at].x)) {
			landmarks_valid = false;
		}
	}
	return landmarks_valid;
}

std::string handle_browse_file(bool read_only, const std::string& _title, const std::string& ext_name)
{
	std::string title = _title;
	std::string open;
	if (read_only) {
		if (title.empty()) {
			title = i18n::freq_msgstr(i18n::msgid_open_file);
		}
	} else {
		if (title.empty()) {
			title = i18n::freq_msgstr(i18n::msgid_save_file_as);
		}
		open = _("Save");
	}
	VALIDATE(!title.empty(), null_str);

	const std::string khome_wokoscript = game_config::preferences_dir + "/aplt_leagor_khome__documents/wkoscript";
	// std::string initial = utils::extract_directory(preferences::last_wkoscript_file());
	std::string initial = preferences::last_browse_file_path();
	if (initial.empty() || !SDL_IsDirectory(initial.c_str())) {
		initial = khome_wokoscript;
	}
	if (!SDL_IsDirectory(initial.c_str())) {
		// initial = upload_path_;
		initial = game_config::preferences_dir;
	}

	gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_FILE, read_only, initial, title, open);
	param.extra = gui2::tbrowse::tentry(khome_wokoscript, _("kHome"), "misc/dir_res.png");

	{
		gui2::tbrowse dlg(param, false);
		dlg.show();
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return null_str;
		}
	}

	if (!ext_name.empty() && utils::file_ext_name(param.result) != ext_name) {
		utils::string_map symbols;
		symbols["ext_name"] = ext_name;
		std::string err = _("The file extension must be '.$ext_name'.");
		gui2::show_message("", err);
		return null_str;
	}
	preferences::set_last_browse_file_path(utils::extract_directory(param.result));

	return param.result;
}

std::string handle_browse_file2(bool wkoscript, bool read_only, const std::string& title, bool set_pref)
{
	std::string filename = handle_browse_file(read_only, title, "cfg");
	if (!filename.empty() && set_pref) {
		if (wkoscript) {
			preferences::set_last_wkoscript_file(filename);

		} else {
			preferences::set_last_wkocourse_file(filename);
		}
	}
	return filename;
}

bool is_valid_wkoscript_or_wkocoruse_path(bool wkoscript, const std::string& path)
{
	std::string path2 = utils::normalize_path(path);

	size_t pos = path2.find(game_config::preferences_dir);
	if (pos != 0) {
		return false;
	}
	std::vector<std::string> v_str = utils::split(path2.substr(game_config::preferences_dir.size() + 1), '/');
	if (v_str.size() != 2) {
		return false;
	}

	const std::string dir_name = wkoscript? "wkoscript": "wkocourse";
	if (v_str[1] != dir_name) {
		return false;
	}

	// aplt_leagor_khome__documents/wkoscript
	// aplt_leagor_khomelua__documents/wkocourse
	int type = wkoscript? aplt::bundleid_leagor_khome: aplt::bundleid_leagor_khomelua;

	std::string aplt_documents = utils::join_app_prefix_id(bundleid_2_lua_bundleid(aplt::get_bundleid(type)), "documents");
	if (v_str[0] != aplt_documents) {
		return false;
	}
	return true;
}


#define MKSCRIPT_DEFAULT_VERT_LOCS	6	// 16

static std::string generate_map_data2(int width, int height, bool colorful)
{
    VALIDATE((width % MKSCRIPT_UNIT_LOCS) == 0 && (height % MKSCRIPT_UNIT_LOCS) == 0, null_str);
    return generate_map_data(width, height, colorful, square_terrain_almost_white);
	// return generate_map_data(width, height, colorful, square_terrain_red);
}

void mkscript_controller::tobject::create_surf_for_state(int _width, int _height, bool sel)
{
	VALIDATE(shape_type_is_state(shape.type), null_str);

	const surface& shape_surf = sel? shape.get_sel_surf(_width, _height): shape.get_surf(_width, _height);
	surface& bg = sel? sel_surf: surf;
	bg = clone_surface(shape_surf);

	int width = bg->w;
	int height = bg->h;
	const int label_margin = 8;
	const int margin = visio::SHAPE_MARGIN + label_margin;

	aplt::twkoscript& script = controller.tmp_script_;
	const aplt::twkoscript::tstate2& state2 = script.states.find(obj_at)->second;
	// 
	std::stringstream ss;
	if (state2.debug_skip) {
		std::string extra_flag("[");
		extra_flag.append(_("SKIP")).append("]");
		ss << ht::generate_format(extra_flag, 0xffff0000);
	}
	if (state2.is_setup) {
		std::string extra_flag("(");
		extra_flag.append(_("wko^Setup")).append(")");
		ss << ht::generate_format(extra_flag, 0xff0000ff);
	}
	ss << (obj_at + 1) << " " << controller.tmp_script_.state_names[obj_at];
	const SDL_Size max_size{width - 2 * margin, height - 2 * margin};
	SDL_Color color = font::BLACK_COLOR;
	surface name_surf = font::get_rendered_text(ss.str(), max_size.w, font::SIZE_DEFAULT, color);

	surface app_bg_surf;
	app_create_bg_surf(max_size, app_bg_surf);

	surface label_surf;
	app_create_label_surf(max_size.w, label_surf);

	// blit app_bg_surface
	SDL_Rect dst_rect = create_rect(margin, margin, max_size.w, max_size.h);
	if (app_bg_surf.get() != nullptr) {
		sdl_blit(app_bg_surf, nullptr, bg, &dst_rect); 
	}

	const int gap_y = 4;
	SDL_Size content_size{name_surf->w, name_surf->h};
	if (label_surf.get() != nullptr) {
		// if (label_surf->w > content_size.w) {
		//	content_size.w = label_surf->w;
		// }
		content_size.h += gap_y + label_surf->h;
	}

	// blit name_surface
	dst_rect = create_rect(margin, (height - content_size.h) / 2, name_surf->w, name_surf->h); 
	sdl_blit(name_surf, nullptr, bg, &dst_rect);

	// blit label_surface
	int y_offset = dst_rect.y + dst_rect.h + gap_y;
	if (label_surf.get() != nullptr) {
		dst_rect = create_rect(margin, y_offset, label_surf->w, label_surf->h); 
		sdl_blit(label_surf, nullptr, bg, &dst_rect);
	}
}

void mkscript_controller::tobject::switch_surf(bool _sel)
{
	VALIDATE(surf.get() != nullptr, null_str);
	if (_sel) {
		if (sel_surf.get() == nullptr) {
			create_surf_for_state(surf->w, surf->h, true);
		}
	}
	sel = _sel;
}

void mkscript_controller::tvoice_state::app_create_label_surf(int max_width, surface& label_surf)
{
	aplt::twkoscript& script = controller.tmp_script_;
	const aplt::twkoscript::tstate2& state2 = script.states.find(obj_at)->second;

	std::stringstream ss;
	utils::string_map symbols;
	VALIDATE(state2.task->type == aplt::twkoscript::tasktype_speak, null_str);

	int max_chars = 48;
	aplt::twkoscript::ttask_speak* speak = static_cast<aplt::twkoscript::ttask_speak*>(state2.task);
	ss << utils::truncate_to_max_chars2(speak->msgstr, max_chars, true);
	symbols["msgstr"] = ss.str();
	std::string msg;
	msg = vgettext2("voice_state's label surf msg, $msgstr", symbols);
	if (speak->min_state_duration_s != nposm) {
		symbols["min_duration"] = utils::format_elapse_hms(speak->min_state_duration_s);
		std::string msg2 = vgettext2("The state lasts at least $min_duration.", symbols);
		msg.append("\n" + ht::generate_format(msg2, 0xff0000ff));
	}
	if (speak->repeat_s != nposm) {
		symbols["repeat_s"] = str_cast(speak->repeat_s);
		std::string msg2 = vgettext2("Repeat the voice every $repeat_s seconds.", symbols);
		msg.append("\n" + ht::generate_format(msg2, 0xff0000ff));
	}

	SDL_Color color = font::GRAY_COLOR;
	label_surf = font::get_rendered_text(msg, max_width, font::SIZE_SMALL, color);
}

void mkscript_controller::tpose_state::app_create_bg_surf(const SDL_Size& max_size, surface& bg_surf)
{
	aplt::twkoscript& script = controller.tmp_script_;
	const aplt::twkoscript::tstate2& state2 = script.states.find(obj_at)->second;

	const int phase_count = wko_phase_count_from_task_type(state2.task->type);
	surface surf0;
	std::string phase_surf_dir = controller.editing_phase_surf_dir();
	surf0 = image::get_image_withoutcache(state2.build_lmk33_png_filename(phase_surf_dir, 0));
	if (surf0.get() != nullptr && (surf0->w != WKO_LMK33_PNG_WIDTH || surf0->h != WKO_LMK33_PNG_HEIGHT)) {
		surf0 = nullptr;
	}

	surface surf1;
	if (phase_count > 1) {
		surf1 = image::get_image_withoutcache(state2.build_lmk33_png_filename(phase_surf_dir, 1));
		if (surf1.get() != nullptr && (surf1->w != WKO_LMK33_PNG_WIDTH || surf1->h != WKO_LMK33_PNG_HEIGHT)) {
			surf1 = nullptr;
		}
	}

	surface bg = create_neutral_surface(max_size.w, max_size.h);
	SDL_Size adaption_size = calculate_max_size_with_ratio(WKO_LMK33_PNG_WIDTH, WKO_LMK33_PNG_HEIGHT * phase_count, max_size.w, max_size.h);
	if (surf0.get() == nullptr && surf1.get() == nullptr) {
		fill_surface(bg, 0x807b7b7b);
		bg_surf = bg;
		return;
	}

	// SDL_Size adaption_size = calculate_max_size_with_ratio(WKO_LMK33_PNG_WIDTH, WKO_LMK33_PNG_HEIGHT * phase_count, max_size.w, max_size.h);
	surf0 = scale_surface(surf0, adaption_size.w, adaption_size.h / phase_count);
	if (phase_count > 1) {
		surf1 = scale_surface(surf1, adaption_size.w, adaption_size.h / 2);
	}

	SDL_Point offset{(max_size.w - adaption_size.w) / 2, (max_size.h - adaption_size.h) / 2};
	SDL_Rect dst_rect{offset.x, offset.y, adaption_size.w, adaption_size.h / phase_count};
	sdl_blit(surf0, nullptr, bg, &dst_rect);
	if (phase_count > 1) {
		dst_rect.y += dst_rect.h;
		sdl_blit(surf1, nullptr, bg, &dst_rect);
	}

	bg_surf = bg;

	add_white_overlay_surface(bg_surf, 0.5);
}

void mkscript_controller::tpose_state::app_create_label_surf(int max_width, surface& label_surf)
{
	aplt::twkoscript& script = controller.tmp_script_;
	const aplt::twkoscript::tstate2& state2 = script.states.find(obj_at)->second;

	utils::string_map symbols;
	// std::string task_type_str = aplt::wko_task_types.find(state2.task->type)->second.name;
	std::stringstream special_ss;
	if (state2.task->type == aplt::twkoscript::tasktype_time_counter) {
		aplt::twkoscript::ttime_counter* time_counter = static_cast<aplt::twkoscript::ttime_counter*>(state2.task);
		// task_type_str.append("(").append(aplt::wko_time_rules.find(time_counter->rule)->second.name).append(")");
		special_ss << state2.get_field_str(script.typeid_task, script.fid_time_counter_rule);
		special_ss << ": " << aplt::wko_time_rules.find(time_counter->rule)->second.name << "\n";

		special_ss << state2.get_field_str(script.typeid_task, script.fid_time_counter_tone);
		special_ss << ": " << aplt::wko_time_tones.find(time_counter->tone)->second.name << "\n";

		special_ss << state2.get_field_str(script.typeid_task, script.fid_time_counter_max_count);
		special_ss << ": " << time_counter->max_count;
			
	} else if (state2.task->type == aplt::twkoscript::tasktype_rep_counter) {
		aplt::twkoscript::trep_counter* rep_counter = static_cast<aplt::twkoscript::trep_counter*>(state2.task);
		for (int phase_at = 0; phase_at < (int)rep_counter->phases.size(); phase_at ++) {
			const aplt::twkoscript::tphase& phase = rep_counter->phases[phase_at];
			if (phase_at != 0) {
				special_ss << "\n";
			}
			symbols["number"] = str_cast(phase_at + 1);
			symbols["action_msg"] = phase.action_msg;
			symbols["min_duration_ms"] = str_cast(phase.min_duration_ms);
			symbols["cooldowned_ms"] = str_cast(phase.cooldowned_ms);
			special_ss << vgettext2("tasktype_rep_counter, line desc, $number, $action_msg, $min_duration_ms, $cooldowned_ms", symbols);
		}
		special_ss << "\n";

		special_ss << state2.get_field_str(script.typeid_task, script.fid_rep_counter_max_count);
		special_ss << ": " << rep_counter->max_count;
	}

	symbols["task_type"] = aplt::wko_task_types.find(state2.task->type)->second.name;
	symbols["pose_count"] = str_cast(state2.track_pose.poses.size());
	symbols["special"] = special_ss.str();
	std::string msg = vgettext2("pose_state's label surf msg, $tast_type, $pose_count, $special", symbols);

	SDL_Color color = font::BLACK_COLOR; // font::GRAY_COLOR;
	label_surf = font::get_rendered_text(msg, max_width, font::SIZE_SMALL, color);
}

// Calculate angle (radians to degrees)
double calculate_angle_0_360(const SDL_Point& center1, const SDL_Point& center2)
{
	double rad_pi_div2 = utils::imgcoor_calculate_angle_2SDL_Point(center1, center2);
	double rad_0_2pi = angles::normalize_angle_positive(rad_pi_div2);
	return RAD2DEG(rad_0_2pi);
}

// Select the connecting edge based on the angle
void get_edges_by_angle_and_position(double angle, 
                                      const SDL_Rect& start_rect, 
                                      const SDL_Rect& end_rect,
                                      int* start_edge, 
                                      int* end_edge)
{
    // Check if end_rect is entirely above start_rect
    bool is_entirely_above = (end_rect.y + end_rect.h <= start_rect.y);
    // Check if end_rect is entirely below start_rect
    bool is_entirely_below = (end_rect.y >= start_rect.y + start_rect.h);
    
    // Priority: relative position first, then angle
    if (is_entirely_above) {
        *start_edge = rect8p_mtop;
        *end_edge = rect8p_mbottom;
        return;
    }
    
    if (is_entirely_below) {
        *start_edge = rect8p_mbottom;
        *end_edge = rect8p_mtop;
        return;
    }

    if (angle >= 315 || angle < 45) {
		// [315, 360) or [0, 45)
        *start_edge = rect8p_mright;
        *end_edge = rect8p_mleft;

    } else if (angle >= 45 && angle < 135) {
		// [45, 135)
        *start_edge = rect8p_mtop;
        *end_edge = rect8p_mbottom;

    } else if (angle >= 135 && angle < 225) {
		// [135, 225)
        *start_edge = rect8p_mleft;
        *end_edge = rect8p_mright;

    } else {
		// [225, 315)
        *start_edge = rect8p_mbottom;
        *end_edge = rect8p_mtop;
    }
}

// Main function: only returns which edge should be used
void calculate_optimal_connection_by_angle(const SDL_Rect& start_rect, const SDL_Rect& end_rect,
	int* start_edge, int* end_edge)
{
    // calculate center point
    SDL_Point start_center = {
        start_rect.x + start_rect.w / 2,
        start_rect.y + start_rect.h / 2
    };
    SDL_Point end_center = {
        end_rect.x + end_rect.w / 2,
        end_rect.y + end_rect.h / 2
    };
    
    // calculate angle
    double angle = calculate_angle_0_360(start_center, end_center);
    
    // Get the edge that should be used (with position priority)
    get_edges_by_angle_and_position(angle, start_rect, end_rect, start_edge, end_edge);
}

SDL_Rect mkscript_controller::tedge::set_endpoints2(tdraw_item_C& start_obj, tdraw_item_C& end_obj)
{
	int start_conn;
	int end_conn;
	calculate_optimal_connection_by_angle(start_obj.rect, end_obj.rect, &start_conn, &end_conn);

	int start_at = start_conn;
	int end_at = end_conn;
	return set_endpoints(start_obj, start_at, end_obj, end_at);
}

SDL_Rect mkscript_controller::tedge::set_endpoints(tdraw_item_C& start_obj, int start_at, tdraw_item_C& end_obj, int end_at)
{
	int edge_at = nposm;
	for (int at = 0; at < start_obj.obj->edge_count; at ++) {
		if (start_obj.obj->edges[at] == this) {
			edge_at = at;
			break;
		}
	}
	if (edge_at == nposm) {
		if (start_obj.obj->edge_count + 1 == MAX_EDGES_PER_OBJ) {
			return empty_rect;
		}
	}

	start = tedge_endpoint{start_obj.obj->obj_at, start_at};
	end = tedge_endpoint{end_obj.obj->obj_at, end_at};
	SDL_Rect enclose_rect = create_surf_for_edge();

	if (edge_at == nposm) {
		start_obj.obj->edges[start_obj.obj->edge_count ++] = this;
	}

	return enclose_rect;
}

void mkscript_controller::tedge::start_end_SDL_Points(SDL_Point& _start, SDL_Point& _end) const
{
	tdraw_item_C* start_obj = controller.find_draw_item_for_state(start.obj_at);
	tdraw_item_C* end_obj = controller.find_draw_item_for_state(end.obj_at);
	_start = start_obj->obj->stretch_points[start.rect8p_at];
	_end = end_obj->obj->stretch_points[end.rect8p_at];
}

SDL_Rect mkscript_controller::tedge::create_surf_for_edge()
{
	VALIDATE(shape.type == visio::shape_edge, null_str);

	// SDL_DColor color{0.9, 0.2, 0.2, 1.0};
	SDL_DColor color{0.5, 0.5, 0.5, 1.0};
	double arrow_size = 15.0;
	double line_width = 2.0;
	double arrow_width_ratio = 0.45;

	SDL_Point start_point;
	SDL_Point end_point;
	start_end_SDL_Points(start_point, end_point);

	SDL_Rect enclose_rect;
	surf = cairo::draw_line_width_arrow_for_shape(start_point, end_point, color, arrow_size, line_width, arrow_width_ratio, &enclose_rect);
	return enclose_rect;
}
/*
SDL_Rect mkscript_controller::ttext::set_label(const std::string& new_label)
{
	if (new_label != label || surf.get() == nullptr) {
		label = new_label;
		return create_surf_for_text();
	}
	return controller.find_draw_item_for_text(obj_at, nullptr)->rect;
}
*/
SDL_Rect mkscript_controller::ttext::create_surf_for_text()
{
	VALIDATE(shape.type == visio::shape_text, null_str);

	// SDL_Color color{128, 128, 128, 255};
	SDL_Color color = font::BLACK_COLOR;
	int font_size = controller.shape_text_font_size_;

	std::string text = label;
	if (text.empty()) {
		text = "<unset>";
	}

	tdraw_item_C* item = controller.find_draw_item_for_text(obj_at, nullptr);
	surf = font::get_rendered_text(text, INT_MAX, font_size, color);

	return SDL_Rect{item->rect.x, item->rect.y, surf->w, surf->h};
}

mkscript_controller::tclear_drag_obj_lock::~tclear_drag_obj_lock()
{
	controller_.longpress_shape_ = nullptr;
	controller_.downing_obj_ = nullptr;

	cursor::set(cursor::NORMAL);
}

#define stretch_x_to_left()	do { \
	x_diff = first_mouse_point.x - map_xy.x; \
	now_width = orignal_rect.w + x_diff; \
	if (now_width < MIN_SHAPE_WIDTH) { \
		now_width = MIN_SHAPE_WIDTH; \
		x_diff = now_width - orignal_rect.w; \
	} \
	sel_obj.rect.x = orignal_rect.x - x_diff; \
	sel_obj.rect.w = now_width; \
} while (false)

#define stretch_y_to_up()	do { \
	y_diff = first_mouse_point.y - map_xy.y; \
	now_height = orignal_rect.h + y_diff; \
	if (now_height < MIN_SHAPE_HEIGHT) { \
		now_height = MIN_SHAPE_HEIGHT; \
		y_diff = now_height - orignal_rect.h; \
	} \
	sel_obj.rect.y = orignal_rect.y - y_diff; \
	sel_obj.rect.h = now_height; \
} while (false)

#define stretch_x_to_right()	do { \
	x_diff = map_xy.x - first_mouse_point.x; \
	now_width = orignal_rect.w + x_diff; \
	if (now_width < MIN_SHAPE_WIDTH) { \
		now_width = MIN_SHAPE_WIDTH; \
		x_diff = now_width - orignal_rect.w; \
	} \
	sel_obj.rect.w = now_width; \
} while (false)

#define stretch_y_to_down()		do { \
	y_diff = map_xy.y - first_mouse_point.y; \
	now_height = orignal_rect.h + y_diff; \
	if (now_height < MIN_SHAPE_HEIGHT) { \
		now_height = MIN_SHAPE_HEIGHT; \
		y_diff = now_height - orignal_rect.h; \
	} \
	sel_obj.rect.h = now_height; \
} while (false)

void mkscript_controller::tstretch_helper::stretch(tdraw_item_C& sel_obj, const SDL_Point& map_xy)
{
	VALIDATE(at >= 0 && at < SHAPE_STRETCH_RECT_COUNT, null_str);

	int x_diff = 0;
	int y_diff = 0;
	int now_width = 0;
	int now_height = 0;
	if (at == rect8p_ltop) {
		stretch_x_to_left();
		stretch_y_to_up();

	} else if (at == rect8p_rtop) {
		stretch_x_to_right();
		stretch_y_to_up();

	} else if (at == rect8p_rbottom) {
		stretch_x_to_right();
		stretch_y_to_down();

	} else if (at == rect8p_lbottom) {
		stretch_x_to_left();
		stretch_y_to_down();

	} else if (at == rect8p_mtop) {
		stretch_y_to_up();

	} else if (at == rect8p_mright) {
		stretch_x_to_right();

	} else if (at == rect8p_mbottom) {
		stretch_y_to_down();

	} else if (at == rect8p_mleft) {
		stretch_x_to_left();
	}

	if (sel_obj.rect.w != sel_obj.obj->sel_surf->w || sel_obj.rect.h != sel_obj.obj->sel_surf->h) {
		sel_obj.obj->create_surf_for_state(sel_obj.rect.w, sel_obj.rect.h, true);
		sel_obj.obj->calculate_stretch_rects(sel_obj.rect);
	}
}

mkscript_controller::mkscript_controller(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const config& app_cfg, CVideo& video,
	int sdl_field_small_font_size, const std::string& saves_courseware_dir, std::string& wkoscript_dir)
	: base_controller(SDL_GetTicks(), app_cfg, video)
	, rdpd_mgr_(rdpd_mgr)
	, pble_(pble)
	, privacy_(privacy)
	, sdl_field_small_font_size_(sdl_field_small_font_size)
	, upload_path_(saves_courseware_dir + "/upload")
	, download_path_(saves_courseware_dir + "/download")
	, wkoscript_dir_(wkoscript_dir)
	, shape_types_({
		{visio::shape_voice_state, visio::tshape(visio::shape_voice_state, "misc/shape_voice_state.png", _("shape^voice state"))},
		{visio::shape_pose_state, visio::tshape(visio::shape_pose_state, "misc/shape_pose_state_time.png", _("shape^pose state"))},
		{visio::shape_pose_state_rep, visio::tshape(visio::shape_pose_state_rep, "misc/shape_pose_state_rep.png", _("shape^pose state"))},
		{visio::shape_edge, visio::tshape(visio::shape_edge, "misc/shape_pose_state.png", _("shape^edge"))},
		{visio::shape_text, visio::tshape(visio::shape_text, "misc/shape_text.png", _("shape^text"))},
	})
	, file_ops_({
		{file_new_from_benchmark, _("file^From benchmark...")},
		{file_new_empty, _("file^Empty script")},
		{file_open, _("file^Open")},
		{file_save_as, _("file^Save as")},
		{file_exit, _("file^Exit")},
	})
	, max_draw_items_(512)
	, min_map_size_{posix_align_ceil2(26, MKSCRIPT_UNIT_LOCS), posix_align_ceil2(14, MKSCRIPT_UNIT_LOCS)}
	// state_margin.x/y do not contain text, so there is no need to use hdpi_scale.
	, flowchart_margin_{32, 32}
	, shape_text_font_size_(font::SIZE_SMALL) // font::SIZE_DEFAULT
	, add_to_working_dir_msgstr_(_("Add to working"))
	, gui_(nullptr)
	, dlg_(nullptr)
	, window_(nullptr)
	, title_widget_(nullptr)
	, status_widget_(nullptr)
	, map_(null_str)
	, units_(*this, map_, false)
	, next_1second_ticks_(0)
	, allow_draw_(false)
	, min_vsize_(0)
	, next_lmk33_png_at_(nposm)
	, draw_items_(sizeof(tdraw_item_C)) // max_draw_items_
	, disable_validate_items_states_(false)
	, longpress_shape_(nullptr)
	, downing_obj_(nullptr)
	, sel_obj_(nullptr)
	, obj_may_click_(nullptr)
	, nposm_when_down_(false)
	, action_tpl2s_(aplt::action_tpl2s)
{
	VALIDATE(!action_tpl2s_.empty(), null_str);
	VALIDATE(shape_types_.size() == visio::shape_count, null_str);
	visio::SHAPE_MARGIN = 6.4 * gui2::twidget::hdpi_scale;

	map_ = tmap(generate_map_data2(2, 4, false));
	// units_.create_coor_map(map_.w(), map_.h());
}

mkscript_controller::~mkscript_controller()
{
	// api_ptr_.reset();

	clear_draw_items();

	if (gui_) {
		delete gui_;
		gui_ = nullptr;
	}
}

void mkscript_controller::app_create_display(int initial_zoom)
{
	gui_ = new mkscript_display(rdpd_mgr_, pble_, privacy_, *this, units_, video_, map_, initial_zoom);
}

void mkscript_controller::app_post_initialize()
{
	dlg_ = static_cast<gui2::tmkscript_scene*>(gui_->get_theme());
	window_ = dlg_->get_window();

	title_widget_ = gui2::find_widget<gui2::tlabel>(window_, "title", false, true);
	status_widget_ = gui2::find_widget<gui2::tlabel>(window_, "status", false, true);

	load_preset_poses_cfg();

/*
	api_ptr_.reset(mediapipe::rose_create_pose_tracking_api());
	bool retbool = api_ptr_->graph_initialized();
	VALIDATE(retbool, null_str);
*/
}

void mkscript_controller::app_play_slice()
{
	uint32_t now = SDL_GetTicks();
    if (now > next_1second_ticks_) {
        dlg_->refresh_statusbar_grid(now);

        const int threshold_1s = 1000;
        next_1second_ticks_ = now + threshold_1s;
    }
}

extern void win_ShellExecuteW_open(const std::string& url);
/*
void win_ShellExecuteW_open(const std::string& url)
{
#ifdef _WIN32
	VALIDATE(game_config::os == os_windows, null_str);
	VALIDATE(!url.empty(), null_str);

	wchar_t* urlw = (wchar_t *)SDL_iconv_string("UTF-16LE", "UTF-8", (char *)(url.c_str()), url.size()+1);
	ShellExecuteW(NULL, L"open", urlw, NULL, NULL, SW_SHOWNORMAL);
	SDL_free(urlw);
#endif
}
*/
void mkscript_controller::app_execute_command(int command, const std::string& sparam)
{
	using namespace gui2;

	switch (command) {
	case HOTKEY_ZOOM_IN:
		gui_->set_zoom(ZOOM_INCREMENT);
		break;
	case HOTKEY_ZOOM_OUT:
		gui_->set_zoom(-ZOOM_INCREMENT);
		break;

	case HOTKEY_SYSTEM:
		click_system();
		break;

	case tmkscript_scene::HOTKEY_SAVE:
		click_save();
		break;

	case tmkscript_scene::HOTKEY_SETTING:
		click_setting();
		break;

	case tmkscript_scene::HOTKEY_NEXT_STATE:
		click_next_state();
		break;

	case HOTKEY_COPY:
		click_copy_action_tpl();
		break;

	case HOTKEY_PASTE:
		click_paste_action_tpl();
		break;

	case tmkscript_scene::HOTKEY_CLONE:
		click_clone();
		break;

	case gui2::tmkscript_scene::HOTKEY_SWITCH_SUB1:
	case gui2::tmkscript_scene::HOTKEY_SWITCH_ADD1:
		click_switch_state(command == gui2::tmkscript_scene::HOTKEY_SWITCH_ADD1);
		break;

	case gui2::tmkscript_scene::HOTKEY_ERASE: // erase
		click_erase();
		break;

	case tmkscript_scene::HOTKEY_ADD_TO_WORKING_DIR:
		click_add_to_working_dir();
		break;

	case tmkscript_scene::HOTKEY_WORKING_DIR:
		win_ShellExecuteW_open(wkoscript_dir_);
		// click_edit_state_name();
		break;

	case gui2::tmkscript_scene::HOTKEY_SHARE:
		click_share();
		break;

	default:
		base_controller::app_execute_command(command, sparam);
	}
}

bool mkscript_controller::app_in_context_menu(const std::string& id) const
{
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	bool is_wkoscript_path = filename_.empty() || filename_.find(wkoscript_dir_) == 0;

	// const unit* u = units_.find_unit(selected_hex_, true);
	switch(command) {
	// idle section
/*
	case gui2::tmkscript_scene::HOTKEY_BUILD:
	{
		gui2::tmkwin_scene* scene = dynamic_cast<gui2::tmkwin_scene*>(gui_->get_theme());
		return !selected_hex_.valid() && scene->require_build();
	}
*/
	case gui2::tmkscript_scene::HOTKEY_SAVE:
		return is_wkoscript_path;

	case HOTKEY_ZOOM_IN:
	case HOTKEY_ZOOM_OUT:
		return false;
	case HOTKEY_SYSTEM:
		return false; // true

	case gui2::tmkscript_scene::HOTKEY_SETTING: // setting
	case gui2::tmkscript_scene::HOTKEY_NEXT_STATE:
		return is_wkoscript_path && sel_obj_ != nullptr;

	case HOTKEY_COPY:
		if (is_wkoscript_path && sel_obj_ != nullptr) {
			const aplt::twkoscript::tstate2& state2 = state2_from_sel_obj_with_validate();
			return wko_is_pose_state2_from_task_type(state2.task->type);
		}
		return false;

	// case HOTKEY_CUT:
	case HOTKEY_PASTE:
		return is_wkoscript_path && sel_obj_ != nullptr;

	case gui2::tmkscript_scene::HOTKEY_CLONE:
		return is_wkoscript_path && sel_obj_ != nullptr;

	case gui2::tmkscript_scene::HOTKEY_SWITCH_SUB1:
		return is_wkoscript_path && sel_obj_ != nullptr && sel_obj_->obj->obj_at != 0;
	case gui2::tmkscript_scene::HOTKEY_SWITCH_ADD1:
		return is_wkoscript_path && sel_obj_ != nullptr && sel_obj_->obj->obj_at != (int)tmp_script_.states.size() - 1;
	// unit
	case gui2::tmkscript_scene::HOTKEY_ERASE: // erase
		return is_wkoscript_path && sel_obj_ != nullptr && calculate_edge_count(*sel_obj_, nullptr, nullptr) == 0;

	case gui2::tmkscript_scene::HOTKEY_ADD_TO_WORKING_DIR:
		return !is_wkoscript_path;

	case gui2::tmkscript_scene::HOTKEY_WORKING_DIR:
		return game_config::os == os_windows;

	case gui2::tmkscript_scene::HOTKEY_SHARE:
		return !tmp_script_.states.empty();

/*
	case gui2::tmkscript_scene::HOTKEY_ERASE_ROW:
		return !preview() && u && u->type() == unit::ROW && can_adjust_row(u);

	// column
	case gui2::tmkscript_scene::HOTKEY_INSERT_RIGHT:
		return !preview() && u && u->type() == unit::COLUMN && can_adjust_column(u);

	case gui2::tmkscript_scene::HOTKEY_INSERT_CHILD: // add a page
		return u && u->is_stack();

	case gui2::tmkscript_scene::HOTKEY_PACK:
		return !preview_ && u && u->type() == unit::WIDGET && !u->is_tpl() && !u->is_spacer() && !u->is_grid();

	case gui2::tmkscript_scene::HOTKEY_UNPACK:
		return !preview_ && u && u->is_tpl();
*/
	default:
		return false;
	}

	return false;
}

bool mkscript_controller::actived_context_menu(const std::string& id) const
{
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	switch(command) {
	case gui2::tmkscript_scene::HOTKEY_SAVE:
		return wkoscript_dirty();

	default:
		return true;
	}

	return true;
}

void mkscript_controller::script_clear_and_set_valid_id()
{
	// Why set the id even when 'filename_' is empty? heare 'script_' is 'empty'.
	// --The user might upload 'lmk33.png', and those files need a storable directory with a fixed location.  
	// This has a side effect: if the user edits for a while but does not save, that directory becomes an orphan directory.
	VALIDATE(filename_.empty(), null_str);

	script_.clear();

	std::set<std::string> existed;
	aplt::list_wkoscript_files_by_type(wkoscript_dir_, aplt::type_wkoscript_ids, existed);
	script_.id = utils::unique_untitle_id(existed, "workout", null_str, 1);

	SDL_DeleteFiles(script_.build_phase_surf_dir(wkoscript_dir_).c_str());
}

void mkscript_controller::app_first_drawn()
{
	VALIDATE(filename_.empty(), null_str);

	// std::string filename = game_config::preferences_dir + "/aplt_leagor_khome__documents/saves/workout_shoulder_neck.cfg";
	// std::string filename = game_config::preferences_dir + "/aplt_leagor_khome__documents/saves/workout_pushup.cfg";
	std::string filename = preferences::last_wkoscript_file();

	std::string must_ext_name = "cfg";
	if (!filename.empty() && utils::file_ext_name(filename) == must_ext_name) {
		aplt::twkoscript script;
		script.from_file(filename);

		std::string err_msg;
		if (script.is_valid2(err_msg, nullptr) == TCOOKIE3F_CHECK_OK) {
			draw_flowchart_from_script(filename, script);
		}
	}
	if (!script_.valid()) {
		int w = min_map_size_.w;
		int h = min_map_size_.h;

		if (w != map_.w() || h != map_.h()) {
			const int right_padding_cells = 0; // 2
			reload_map(w + right_padding_cells, h);
		}
		new_empty_flowchart();
	}

	gui_->show_context_menu();

	VALIDATE(!allow_draw_, null_str);
	allow_draw_ = true;
}

void mkscript_controller::app_resize_screen()
{
	gui_->app_resize_screen();
}

void mkscript_controller::update_title_label()
{
	std::stringstream ss;
	ss << utils::extract_file(filename_);
	ss << " - ";
	ss << aplt::all_fake_applets.find(aplt::builtinid_mkscript)->second.name;
	title_widget_->set_label(ss.str());
}

void mkscript_controller::update_status_label(bool valid)
{
	std::string msg;
	if (valid) {
		msg.append(_("Working directory"));
		msg.append(": ");
		msg.append(os_normalize_path(wkoscript_dir_));

	} else {
		utils::string_map symbols;
		symbols["add_to_working_dir"] = add_to_working_dir_msgstr_;
		msg = vgettext2("invalid wkoscript_dir remark, $add_to_working_dir", symbols);
	}
	status_widget_->set_label(msg);
}

void mkscript_controller::load_preset_poses_cfg()
{
	std::string stream;
	std::string filename = game_config::app_dir_root + "/cert/preset_poses.cfg";
	{
		const int max_cw_default_cfg_size = 256 * 1024; // 256K bytes
		tfile file(filename, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		VALIDATE(fsize > 0 && fsize <= max_cw_default_cfg_size, null_str);

		bool all_is_utf8 = utils::is_utf8str(file.data, fsize);
		VALIDATE(all_is_utf8, null_str);
		stream.assign(file.data, fsize);
	}

	config top_cfg;
	aplt::read_config_ex(stream, true, top_cfg);

	BOOST_FOREACH (const config &pose_cfg, top_cfg.child_range("preset_pose")) {
		aplt::tpreset_pose preset_pose;
		bool retbool = preset_pose.from_cfg(pose_cfg);
		if (!retbool) {
			continue;
		}
		VALIDATE(preset_pose.phase_mask == 1, null_str);
		preset_poses_.insert(std::make_pair(preset_pose.metric, preset_pose));
	}
}

void mkscript_controller::reload_map(int w, int h)
{
	// VALIDATE(filled_units_ == 0, null_str);

    const int original_w = map_.w();
    const int original_h = map_.h();

	map_ = tmap(generate_map_data2(w, h, false));
	gui_->reload_map();
	units_.create_coor_map(map_.w(), map_.h());

    VALIDATE(w * h == units_.size() * MKSCRIPT_UNIT_LOCS * MKSCRIPT_UNIT_LOCS, null_str);

    std::stringstream ss;
    ss << "reload_map(" << w << ", " << h << ")";
    // units_.dump(ss.str());
}

std::string generate_textid_label(int id, const std::string& label)
{
	std::string prefix;
	if (id == visio::textid_id) {
		prefix = "ID";

	} else if (id == visio::textid_name) {
		prefix = _("object^Name");

	} else if (id == visio::textid_author) {
		prefix = _("Author");

	} else if (id == visio::textid_reference) {
		prefix = _("Reference");

	} else {
		VALIDATE(false, null_str);
	}

	utils::string_map symbols;
	symbols["key"] = prefix;
	symbols["value"] = label;
	return vgettext2("$key: $value", symbols);
}

void mkscript_controller::draw_fix_text_shapes()
{
	VALIDATE(draw_items_.vsize == 0, null_str);
	VALIDATE(min_vsize_ == 0, null_str);

	int zoom = gui_->zoom();

	// SDL_Point offset{zoom, zoom / 2};
	SDL_Point offset{flowchart_margin_.w, flowchart_margin_.h};
	tdraw_item_C* text_obj = insert_text(visio::textid_id, offset, script_.id);
	// text_obj->rect.y = text_obj->rect.y - text_obj->rect.h / 2;

	// name
	const int max_textid_id_width = 208 * gui2::twidget::hdpi_scale;
	offset.x += max_textid_id_width;
	text_obj = insert_text(visio::textid_name, offset, script_.title);
	// text_obj->rect.y = text_obj->rect.y - text_obj->rect.h / 2;

	// author
	const int max_textid_name_width = 288 * gui2::twidget::hdpi_scale;
	offset.x += max_textid_name_width;
	text_obj = insert_text(visio::textid_author, offset, script_.author);
	// text_obj->rect.y = text_obj->rect.y - text_obj->rect.h / 2;

	// reference
	offset.x += max_textid_name_width;
	text_obj = insert_text(visio::textid_reference, offset, script_.reference);


	min_vsize_ = draw_items_.vsize;
}

void mkscript_controller::draw_flowchart_from_script(const std::string& filename, const aplt::twkoscript& script)
{
	VALIDATE(!filename.empty(), null_str);
	std::string err_msg;
	// When generating a script file from a benchmark image, the given image may not have detectable 33landmarks. 
	// In this case, 'pose.range' can only be '{nposm, nposm}'.
	VALIDATE(script.is_valid2(err_msg, nullptr) == TCOOKIE3F_CHECK_OK, null_str);

	VALIDATE(sel_obj_ == nullptr, null_str);

	tdisable_validate_items_states_lock lock(*this);

	filename_ = filename;
	script_.assign(script);

	script_.set_lmk33_png_at_equal_to_state_at();
	next_lmk33_png_at_ = script_.states.size();
	tmp_script_.assign(script_);

	clear_draw_items();

	int zoom = gui_->zoom();
	int states_per_line = 5;
	// state_margin.x do not contain text, so there is no need to use hdpi_scale.
	const SDL_Size& flowchart_margin = flowchart_margin_;
	const int fix_text_shapes_height = font::get_rendered_text_size(_("object^Name"), INT_MAX, shape_text_font_size_).y + 24;

	const SDL_Size state_size{(int)(64 * 3.5 * gui2::twidget::hdpi_scale), (int)(64 * 2.5 * gui2::twidget::hdpi_scale)};
	const SDL_Size state_margin{(int)(36 * gui2::twidget::hdpi_scale), (int)(72 * gui2::twidget::hdpi_scale)};
/*	
	{
		state_size = SDL_Size{64 * 4, 64 * 3};
		state_margin = SDL_Size{64, 64};
	}
*/
	const int state_count = script_.states.size();
	SDL_Size desire_size{0, 0};
	for (int n = 0; n < state_count; n += states_per_line) {
		int width = state_size.w * states_per_line + state_margin.w * (states_per_line - 1);
		if (width > desire_size.w) {
			desire_size.w = width;
		}

		if (n != 0) {
			desire_size.h += state_margin.h;
		}
		desire_size.h += state_size.h;
	}
	desire_size.w += flowchart_margin.w * 2;
	desire_size.h += flowchart_margin.h * 2 + fix_text_shapes_height;

	// SDL_Rect widget_rect = gui_->main_map_widget_rect();
	const SDL_Size min_size{min_map_size_.w * zoom, min_map_size_.h * zoom};
	if (desire_size.w < min_size.w) {
		desire_size.w = min_size.w;
	}
	if (desire_size.h < min_size.h) {
		desire_size.h = min_size.h;
	}

	desire_size.w = posix_align_ceil2(desire_size.w, zoom * MKSCRIPT_UNIT_LOCS);
	desire_size.h = posix_align_ceil2(desire_size.h, zoom * MKSCRIPT_UNIT_LOCS);

    int cols = desire_size.w / zoom;
	int rows = desire_size.h / zoom;

    int map_w = map_.w();
    int map_h = map_.h();

    if (cols != map_w || rows != map_h) {
		reload_map(cols, rows);
    }
    // 
	const std::map<int, aplt::twkoscript::tstate2>& states = script_.states;

	draw_fix_text_shapes();

	int at = 0;
	for (std::map<int, aplt::twkoscript::tstate2>::const_iterator it = states.begin(); it != states.end(); ++ it, at ++) {
		const aplt::twkoscript::tstate2& state2 = it->second;
		VALIDATE(state2.task != nullptr, null_str);
		int task_type = state2.task->type;
		const bool is_pose_state = wko_is_pose_state2_from_task_type(task_type);
		int row = at / states_per_line;
		int col = at % states_per_line;
		// if (row & 1) {
		//	col = states_per_line - col - 1; 
		// }
		int offset_x = flowchart_margin.w + state_size.w * col + state_margin.w * col;
		int offset_y = flowchart_margin.h + fix_text_shapes_height + state_margin.h * row + state_size.h * row;

		int type = nposm;
		if (task_type == aplt::twkoscript::tasktype_speak) {
			type = visio::shape_voice_state;

		} else if (task_type == aplt::twkoscript::tasktype_time_counter) {
			type = visio::shape_pose_state;

		} else if (task_type == aplt::twkoscript::tasktype_rep_counter) {
			type = visio::shape_pose_state_rep;
		} else {

			VALIDATE(false, null_str);
		}
		
		const visio::tshape& shape = shape_types_.find(type)->second;
		tdraw_item_C* item = insert_draw_item(shape, SDL_Rect{offset_x, offset_y, state_size.w, state_size.h},
			false);
		VALIDATE(!item->obj->sel && item->obj->sel_surf.get() == nullptr, null_str);

		modify_item_rect(*item, SDL_Rect{nposm, nposm, state_size.w, state_size.h});
	}

	// connect all edge
	for (std::map<int, aplt::twkoscript::tstate2>::const_iterator it = states.begin(); it != states.end(); ++ it, at ++) {
		const aplt::twkoscript::tstate2& state2 = it->second;
		if (state2.state == states.size() - 1) {
			continue;
		}

		tdraw_item_C* start = find_draw_item_for_state(state2.state);
		VALIDATE(start != nullptr, null_str);

		const std::vector<aplt::tif_branch>& branches = state2.next.branches;
		for (std::vector<aplt::tif_branch>::const_iterator it2 = branches.begin(); it2 != branches.end(); ++ it2) {
			const aplt::tif_branch& branch = *it2;
			if (branch.do_to_state == nposm) {
				continue;
			}
			tdraw_item_C* end = find_draw_item_for_state(branch.do_to_state);
			if (end != nullptr) {
				insert_edge(*start, *end);
			}
		}

		// tdraw_item_C* end = find_draw_item_for_state(state2.state + 1);
		// VALIDATE(end != nullptr, null_str);
		// insert_edge(*start, *end);
	}
	
	update_title_label();

	const std::string path = utils::extract_directory(filename);
	bool valid = is_valid_wkoscript_or_wkocoruse_path(true, path);
	if (valid) {
		wkoscript_dir_ = path;
		preferences::set_wkoscript_dir(path);
	}
	update_status_label(valid);
}

void mkscript_controller::new_empty_flowchart()
{
	select_object(nullptr);

	clear_draw_items();
	filename_.clear();

	script_clear_and_set_valid_id();
	tmp_script_.assign(script_);

	draw_fix_text_shapes();

	update_title_label();
}

void mkscript_controller::modify_item_rect(tdraw_item_C& item, const SDL_Rect& _new_rect)
{
	bool dirty = false;
	bool size_dirty = false;
	SDL_Rect rect = item.rect;
	if (_new_rect.x != nposm && _new_rect.x != rect.x) {
		rect.x = _new_rect.x;
		dirty = true;
	}
	if (_new_rect.y != nposm && _new_rect.y != rect.y) {
		rect.y = _new_rect.y;
		dirty = true;
		size_dirty = true;
	}
	if (_new_rect.w != nposm && _new_rect.w != rect.w) {
		rect.w = _new_rect.w;
		dirty = true;
	}
	if (_new_rect.h != nposm && _new_rect.h != rect.h) {
		rect.h = _new_rect.h;
		dirty = true;
		size_dirty = true;
	}
	if (dirty) {
		item.rect = rect;
		if (size_dirty) {
			item.obj->create_surf_for_state(rect.w, rect.h, false);
			if (item.obj->sel_surf.get() != nullptr) {
				item.obj->create_surf_for_state(rect.w, rect.h, true);
			}
		}
		reconnect_edges(item);
	}
}

void mkscript_controller::clear_draw_items()
{
	tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
	for (int at = 0; at < draw_items_.vsize; at ++) {
		tdraw_item_C& item = items[at];
		delete item.obj;
		item.obj = nullptr;
	}
	draw_items_.clear();
	min_vsize_ = 0;
}

mkscript_controller::tobject* mkscript_controller::create_derived_obj(const visio::tshape& shape, int obj_at, const SDL_Rect& item_rect)
{
	VALIDATE(obj_at >= 0, null_str);

	tobject* obj = nullptr;
	if (shape.type == visio::shape_voice_state) {
		obj = new tvoice_state(*this, shape, obj_at);

	} else if (shape.type == visio::shape_pose_state) {
		obj = new tpose_state(*this, shape, obj_at);

	} else if (shape.type == visio::shape_pose_state_rep) {
		obj = new tpose_state(*this, shape, obj_at);

	} else {
		VALIDATE(false, null_str);
	}
	obj->create_surf_for_state(item_rect.w, item_rect.h, false);
	VALIDATE(obj->surf->w == item_rect.w && obj->surf->h == item_rect.h, null_str);
	obj->calculate_stretch_rects(item_rect);
	return obj;
}

int def_task_type_from_shape_type(const visio::tshape& shape)
{
	if (shape.type == visio::shape_voice_state) {
		return aplt::twkoscript::tasktype_speak;

	} else if (shape.type == visio::shape_pose_state) {
		return aplt::twkoscript::tasktype_time_counter;

	} else if (shape.type == visio::shape_pose_state_rep) {
		return aplt::twkoscript::tasktype_rep_counter;
	}

	VALIDATE(false, null_str);
	return nposm;
}

mkscript_controller::tdraw_item_C* mkscript_controller::insert_draw_item(const visio::tshape& shape, const SDL_Rect& rect, 
	bool new_state2)
{
	if (new_state2) {
		tmp_script_.insert_state(nposm, null_str, def_task_type_from_shape_type(shape), next_lmk33_png_at_ ++);
	}

	surface surf = shape.get_surf(rect.w, rect.h);

	SDL_Rect item_rect{rect.x, rect.y, surf->w, surf->h};
	const SDL_Rect map_rect = gui_->main_map_rect();

	VALIDATE(SDL_HasIntersection(&item_rect, &map_rect), null_str);

	int obj_at = -1;
	std::set<std::string> existed_names;
	const tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
	for (int at = 0; at < draw_items_.vsize; at ++) {
		const tdraw_item_C& item = items[at];
		if (!shape_type_is_state(item.obj->shape.type)) {
			continue;
		}
		obj_at = item.obj->obj_at;
		// existed_names.insert(item.obj->obj_name);
	}

	obj_at ++;
	tdraw_item_C* item = (tdraw_item_C*)draw_items_.append_1();
	item->rect = item_rect;
	item->obj = create_derived_obj(shape, obj_at, item_rect);


	validate_draw_items();

	return item;
}

void mkscript_controller::erase_draw_item(int obj_at)
{
	VALIDATE(obj_at >= 0 && obj_at < (int)tmp_script_.states.size(), null_str);

	validate_draw_items();
}

void mkscript_controller::insert_edge(tdraw_item_C& start, tdraw_item_C& end)
{
	tdraw_item_C* item = (tdraw_item_C*)draw_items_.append_1();
	const visio::tshape& shape = shape_types_.find(visio::shape_edge)->second;
	tedge* obj = new tedge(*this, shape);
	item->rect = obj->set_endpoints2(start, end);
	item->obj = obj;
}

void mkscript_controller::erase_edge_from_draw_items(int item_at)
{
	VALIDATE(item_at >= 0 && item_at < draw_items_.vsize, null_str);
	const tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;

	VALIDATE(items[item_at].obj->shape.type == visio::shape_edge, null_str);
	tedge* desire_edge = static_cast<tedge*>(items[item_at].obj);
	bool found = false;
	for (int at = 0; at < draw_items_.vsize && !found; at ++) {
		const tdraw_item_C& item = items[at];
		if (!shape_type_is_state(item.obj->shape.type)) {
			continue;
		}
		// Search for the tobject containing this edge, find it, and set edges[edge_at] = nullptr.
		for (int edge_at = 0; edge_at < item.obj->edge_count; edge_at ++) {
			tedge* edge = item.obj->edges[edge_at];
			if (edge == desire_edge) {
				item.obj->edges[edge_at] = nullptr;
				if (edge_at < item.obj->edge_count - 1) {
					int elem_sz = sizeof(tedge*);
					int vsize = item.obj->edge_count;
					memcpy(item.obj->edges + edge_at, item.obj->edges + (edge_at + 1), 
						(vsize - edge_at - 1) * elem_sz);
				}
				item.obj->edge_count --;
				found = true;
				break;
			}
		}
	}
	// free edge
	delete desire_edge;
	draw_items_.drop_at(item_at);
}

mkscript_controller::tdraw_item_C* mkscript_controller::insert_text(int obj_at, const SDL_Point& offset, const std::string& label)
{
	VALIDATE(obj_at != nposm, null_str);
	tdraw_item_C* exited = find_draw_item_for_text(obj_at, nullptr);
	VALIDATE(exited == nullptr, null_str);

	tdraw_item_C* item = (tdraw_item_C*)draw_items_.append_1();
	const visio::tshape& shape = shape_types_.find(visio::shape_text)->second;
	ttext* obj = new ttext(*this, shape, obj_at);
	item->rect.x = offset.x;
	item->rect.y = offset.y;
	item->obj = obj;

	item->rect = obj->set_label(generate_textid_label(obj_at, label));

	return item;
}

mkscript_controller::tdraw_item_C* mkscript_controller::in_which_obj(int screen_x, int screen_y) const
{
	SDL_Point map_xy{screen_x, screen_y};
	gui_->screen_2_map(map_xy.x, map_xy.y);
	
	tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
	for (int at = draw_items_.vsize - 1; at >= 0; at --) {
		tdraw_item_C& item = items[at];
		// if (!shape_type_is_state(item.obj->shape.type)) {
		if (!shape_type_is_can_click(item.obj->shape.type)) {
			continue;
		}
		if (SDL_PointInRect(&map_xy, &item.rect)) {
			return &item;
		}
	}
	return nullptr;
}

int mkscript_controller::in_which_stretch_rect(const SDL_Point& map_xy) const
{
	if (sel_obj_ == nullptr) {
		return nposm;
	}

	const SDL_Rect* rects = get_stretch_rects(*sel_obj_);
	for (int at = 0; at < SHAPE_STRETCH_RECT_COUNT; at ++) {
		const SDL_Rect& rect = rects[at];
		if (SDL_PointInRect(&map_xy, &rect)) {
			return at;
		}
	}
	return nposm;
}

void mkscript_controller::select_object(tdraw_item_C* new_obj)
{
	// tauto_destruct_executor destruct_executor(std::bind(&display::show_context_menu, gui_, null_str, null_str));

	if (sel_obj_ == new_obj) {
		return;
	}

	if (sel_obj_ != nullptr) {
		sel_obj_->obj->switch_surf(false);
	}

	if (new_obj != nullptr) {
		VALIDATE(sel_obj_ != new_obj, nullptr);

		new_obj->obj->switch_surf(true);
		sel_obj_ = new_obj;

	} else if (sel_obj_ != nullptr) {
		sel_obj_ = nullptr;
	}

	validate_draw_items();
	gui_->show_context_menu();
}

void mkscript_controller::scroll_to_object(tdraw_item_C& item)
{
	const SDL_Rect& rect = item.rect;
	SDL_Point point{rect.x + rect.w / 2, rect.y + rect.h / 2};
	map_location loc = gui_->map_2_loc(point.x, point.y);
	loc.x = posix_align_floor(loc.x, MKSCRIPT_UNIT_LOCS);
	loc.y = posix_align_floor(loc.y, MKSCRIPT_UNIT_LOCS);
	const base_unit* u = units_.find_base_unit(loc);
	VALIDATE(u != nullptr, null_str);
	gui_->scroll_to_tile(*u->get_draw_locations().begin(), display::ONSCREEN, true, true);

	select_object(&item);
}

void mkscript_controller::reconnect_edges(const tdraw_item_C& reason_obj)
{
	tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
	for (int at = 0; at < draw_items_.vsize; at ++) {
		tdraw_item_C& item = items[at];
		if (item.obj->shape.type == visio::shape_edge) {
			tedge* edge = static_cast<tedge*>(item.obj);
			if (edge->start.obj_at == reason_obj.obj->obj_at || edge->end.obj_at == reason_obj.obj->obj_at) {
				tdraw_item_C* start_obj = find_draw_item_for_state(edge->start.obj_at);
				tdraw_item_C* end_obj = find_draw_item_for_state(edge->end.obj_at);
				item.rect = edge->set_endpoints2(*start_obj, *end_obj);
			}
		}
	}
}

void mkscript_controller::switch_item(int obj_at1, int obj_at2)
{
	VALIDATE(obj_at1 != obj_at2, null_str);

	tdraw_item_C* item1 = find_draw_item_for_state(obj_at1);
	VALIDATE(item1 != nullptr, null_str);

	tdraw_item_C* item2 = find_draw_item_for_state(obj_at2);
	VALIDATE(item2 != nullptr, null_str);

	item1->obj->obj_at = obj_at2;
	item2->obj->obj_at = obj_at1;

	tdraw_item_C* tmp_item = (tdraw_item_C*)malloc(sizeof(tdraw_item_C));
	memcpy(tmp_item, item1, sizeof(tdraw_item_C));
	memcpy(item1, item2, sizeof(tdraw_item_C));
	memcpy(item2, tmp_item, sizeof(tdraw_item_C));

	tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
	for (int at = 0; at < draw_items_.vsize; at ++) {
		const tdraw_item_C& item = items[at];
		if (item.obj->shape.type == visio::shape_edge) {
			tedge* edge = static_cast<tedge*>(item.obj);
			if (edge->start.obj_at == obj_at1) {
				edge->start.obj_at = obj_at2;

			} else if (edge->start.obj_at == obj_at2) {
				edge->start.obj_at = obj_at1;
			}

			if (edge->end.obj_at == obj_at1) {
				edge->end.obj_at = obj_at2;

			} else if (edge->end.obj_at == obj_at2) {
				edge->end.obj_at = obj_at1;
			}
		}
	}

}

int mkscript_controller::calculate_edge_count(const tdraw_item_C& desire_item, int* _start, int* _end) const
{
	int start_count = 0;
	int end_count = 0;
	const tdraw_item_C* items = (const tdraw_item_C*)draw_items_.data;
	for (int at = 0; at < draw_items_.vsize; at ++) {
		const tdraw_item_C& item = items[at];
		if (item.obj->shape.type == visio::shape_edge) {
			tedge* edge = static_cast<tedge*>(item.obj);
			if (edge->start.obj_at == desire_item.obj->obj_at) {
				start_count ++;

			}
			if (edge->end.obj_at == desire_item.obj->obj_at) {
				end_count ++;
			}
		}
	}
	VALIDATE(start_count == desire_item.obj->edge_count, null_str);
	if (_start != nullptr) {
		*_start = start_count;
	}
	if (_end != nullptr) {
		*_end = end_count;
	}

	return start_count + end_count;
}

void mkscript_controller::validate_draw_items() const
{
	const tdraw_item_C* cur_sel_item = sel_obj_;

	int sel_items = 0;
	const tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;

	struct tedge2
	{
		const tobject* edge;
		bool existed;
	};

	tedge2* edge2s = nullptr;
	if (draw_items_.vsize > 0) {
		edge2s = (tedge2*)malloc(draw_items_.vsize * sizeof(tedge2));
		memset(edge2s, 0, draw_items_.vsize * sizeof(tedge2));
	}

	int last_obj_at = -1;
	int edges_in_obj = 0;
	int edges_in_item = 0;
	for (int at = 0; at < draw_items_.vsize; at ++) {
		const tdraw_item_C& item = items[at];
		VALIDATE(item.obj != nullptr, null_str);
		if (shape_type_is_state(item.obj->shape.type)) {
			VALIDATE(item.obj->obj_at == last_obj_at + 1, null_str);
			last_obj_at ++;
			edges_in_obj += item.obj->edge_count;
		} else if (item.obj->shape.type == visio::shape_edge) {
			VALIDATE(item.obj->obj_at == nposm, null_str);
			// edge can exist in only one state.
			edge2s[edges_in_item ++].edge = item.obj;
		}
		if (item.obj->sel) {
			VALIDATE(&item == cur_sel_item, null_str);
			sel_items ++;
		}
	}
	VALIDATE(edges_in_obj == edges_in_item, null_str);
	if (cur_sel_item == nullptr) {
		VALIDATE(sel_items == 0, null_str);
	} else {
		VALIDATE(sel_items == 1, null_str);
	}
	if (!disable_validate_items_states_) {
		VALIDATE(last_obj_at + 1 == (int)tmp_script_.states.size(), null_str);
	}

	const bool validate_edge_unique = true;
	if (validate_edge_unique) {
		// one edge must exist and at max in only one state.
		for (int at = 0; at < draw_items_.vsize; at ++) {
			const tdraw_item_C& item = items[at];
			const tobject& obj = *item.obj;
			if (!shape_type_is_state(obj.shape.type)) {
				continue;
			}
			int edge_at2;
			for (int edge_at = 0; edge_at < obj.edge_count; edge_at ++) {
				const tedge* that = obj.edges[edge_at];
				for (edge_at2 = 0; edge_at2 < edges_in_item; edge_at2 ++) {
					tedge2& edge2 = edge2s[edge_at2];
					if (that == edge2.edge) {
						VALIDATE(!edge2.existed, null_str);
						edge2.existed = true;
						break;
					}
				}
				VALIDATE(edge_at2 < edges_in_item, null_str);
			}
		}
	}

	if (edge2s != nullptr) {
		free(edge2s);
	}
}

const SDL_Rect* mkscript_controller::get_stretch_rects(const tdraw_item_C& item) const
{
	VALIDATE(item.obj->sel, null_str);
	// visio::calculate_stretch_rects(item.rect, item.obj->stretch_rects);

	const bool overlay_frame = false;
	if (overlay_frame) {
		// only for debug 'calculate_stretch_rects' is ok.
		surface bg = sel_obj_->obj->sel_surf;
		for (int at = 0; at < SHAPE_STRETCH_RECT_COUNT; at ++) {
			const SDL_Rect& rect = item.obj->stretch_rects[at];
			draw_rectangle(rect.x - sel_obj_->rect.x, rect.y - sel_obj_->rect.y, rect.w, rect.h, 0xffff0000, bg);

			const SDL_Point& point = item.obj->stretch_points[at];
			draw_circle(bg, 0xff00ff00, point.x - sel_obj_->rect.x, point.y - sel_obj_->rect.y, 2, false);
		}
	}
	return item.obj->stretch_rects;
}

std::string mkscript_controller::editing_phase_surf_dir() const
{
	return script_.build_phase_surf_dir(wkoscript_dir_);
/*
	const std::string ext_name = utils::file_ext_name(filename_);
	VALIDATE(ext_name == "cfg", null_str);
	return filename_.substr(0, filename_.size() - 4);
*/
}

void mkscript_controller::click_object(tdraw_item_C& item)
{
	VALIDATE(item.obj->shape.type == visio::shape_text, null_str);

	int edittype = nposm;
	if (item.obj->obj_at == visio::textid_id) {
		edittype = edittype_id;

	} else if (item.obj->obj_at == visio::textid_name) {
		edittype = edittype_name;

	} else if (item.obj->obj_at == visio::textid_author) {
		edittype = edittype_author;

	} else if (item.obj->obj_at == visio::textid_reference) {
		edittype = edittype_reference;
	}

	VALIDATE(edittype != nposm, null_str);

	handle_edit_text(edittype);
}

void mkscript_controller::handle_file_menu(int sel)
{
	if (sel == file_new_empty || sel == file_new_from_benchmark || sel == file_open || sel == file_save_as || sel == file_exit) {
		handle_file_op(sel);

	} else {
		VALIDATE(false, null_str);
	}
}

bool mkscript_controller::app_mouse_motion(const int x, const int y, const bool minimap)
{
	if (minimap) {
		return true;
	}

	const SDL_Rect& _map_area = gui_->main_map_view_rect();
	if (!point_in_rect(x, y, _map_area)) {
		if (stretch_helper_.valid()) {
			stretch_helper_.halt = true;
		}
		if (obj_may_click_ != nullptr) {
			obj_may_click_ = nullptr;
		}
	}

	if ((!stretch_helper_.valid() || stretch_helper_.halt) && obj_may_click_ == nullptr) {
		return true;
	}

	if (obj_may_click_ != nullptr) {
		VALIDATE(!stretch_helper_.valid(), null_str);
		const tdraw_item_C* now_hiting = in_which_obj(x, y);
		if (now_hiting != obj_may_click_) {
			obj_may_click_ = nullptr;
		}
		return obj_may_click_ == nullptr;
	}

	SDL_Point map_xy{x, y};
	gui_->screen_2_map(map_xy.x, map_xy.y);

	if (stretch_helper_.valid()) {
		stretch_helper_.stretch(*sel_obj_, map_xy);
		return false;
	}

	return !stretch_helper_.valid();
}

void mkscript_controller::app_left_mouse_down(const int x, const int y, const bool minimap)
{
	if (minimap) {
		return;
	}

	VALIDATE(downing_obj_ == nullptr, null_str);
	VALIDATE(!stretch_helper_.valid(), null_str);
	VALIDATE(obj_may_click_ == nullptr, null_str);
	VALIDATE(!nposm_when_down_, null_str);

	SDL_Point map_xy{x, y};
	gui_->screen_2_map(map_xy.x, map_xy.y);

	int stretch_at = in_which_stretch_rect(map_xy);
	if (stretch_at != nposm) {
		stretch_helper_.set(stretch_at, sel_obj_->rect, map_xy);
		return;
	}

	tdraw_item_C* now_downing = in_which_obj(x, y);
	if (now_downing != nullptr) {
		if (shape_type_is_state(now_downing->obj->shape.type)) {
			surface surf = now_downing->obj->surf;

			gui2::twindow& window = *window_;
			window.set_drag_surface(surf, false);

			gui2::twidget* widget = window_->find("_main_map", false);
			// SDL_Point custom_xy_formula{- surf->w / 2, - surf->h / 2};
			SDL_Point custom_xy_formula{- (map_xy.x - now_downing->rect.x), - (map_xy.y - now_downing->rect.y)};
			longpress_custom_xy_formula_ = custom_xy_formula;
			window.start_drag(&custom_xy_formula, tpoint(x, y), std::bind(&mkscript_controller::did_drag_mouse_motion, this, dragtype_move, _1, _2, std::ref(window)),
				std::bind(&mkscript_controller::did_drag_mouse_leave, this, dragtype_move, _1, _2, _3), widget);
			downing_obj_ = now_downing;

		} else {
			VALIDATE(now_downing->obj->shape.type == visio::shape_text, null_str);
			obj_may_click_ = now_downing;
		}

	} else {
		nposm_when_down_ = true;
	}
}

void mkscript_controller::app_left_mouse_up(const int x, const int y, const bool click)
{
	tclear_left_mouse_down_lock lock(*this);

	if (stretch_helper_.valid()) {
		if (sel_obj_->rect.w != sel_obj_->obj->surf->w || sel_obj_->rect.h != sel_obj_->obj->surf->h) {
			sel_obj_->obj->create_surf_for_state(sel_obj_->rect.w, sel_obj_->rect.h, false);
			reconnect_edges(*sel_obj_);

			gui_->redraw_minimap();
		}
	}

	if (!click) {
		return;
	}

	if (!point_in_rect(x, y, gui_->main_map_widget_rect())) {
		return;
	}

	if (gui_->point_in_volatiles(x, y)) {
		return;
	}

	if (obj_may_click_ != nullptr) {
		gui2::tmkscript_scene::tmsg_data_obj_clicked* pdata = 
			new gui2::tmkscript_scene::tmsg_data_obj_clicked(*obj_may_click_);
		rtc::Thread::Current()->Post(RTC_FROM_HERE, dlg_, gui2::tmkscript_scene::MSG_OBJ_CLICKED, pdata);

	} else if (nposm_when_down_) {
		select_object(nullptr);
	}
	// cursor::set(cursor::ILLEGAL_DRAG);
}

void mkscript_controller::app_right_mouse_down(const int x, const int y)
{
	do_right_click();
}

void mkscript_controller::longpress_widget(bool& halt, const tpoint& coordinate, gui2::twindow& window, const visio::tshape& shape)
{
	VALIDATE(longpress_shape_ == nullptr, null_str);

	halt = true;

	const std::string filename = shape.stem_png;
	surface surf(image::get_image(filename));

	// surface surf = shape.get_surf(nposm, nposm);

	window.set_drag_surface(surf, false);

	longpress_shape_ = &shape;
	// gui_->click_widget(type, definition);

	tpoint new_coordinate(coordinate.x - surf->w / 2, coordinate.y - surf->h / 2);

	SDL_Point custom_xy_formula{- surf->w / 2, - surf->h / 2};
	longpress_custom_xy_formula_ = custom_xy_formula;
	window.start_drag(&custom_xy_formula, coordinate, std::bind(&mkscript_controller::did_drag_mouse_motion, this, dragtype_place, _1, _2, std::ref(window)),
		std::bind(&mkscript_controller::did_drag_mouse_leave, this, dragtype_place, _1, _2, _3));
}

bool mkscript_controller::did_drag_mouse_motion(int type, const int x, const int y, gui2::twindow& window)
{
	int xmap = x, ymap = y;
	gui_->screen_2_map(xmap, ymap);
	const SDL_Rect map_rect = gui_->main_map_rect();

	surface surf;

	const bool allow = point_in_rect(xmap, ymap, map_rect);
	if (type == dragtype_place) {
		VALIDATE(longpress_shape_ != nullptr, null_str);
		const visio::tshape& shape = *longpress_shape_;
		if (allow) {
			surface surf2 = shape.get_surf(nposm, nposm);

			surf = create_neutral_surface(surf2->w, surf2->h);
			sdl_blit(surf2, nullptr, surf, nullptr);

			uint32_t color = 0xff0000ff;
			draw_rectangle(visio::SHAPE_MARGIN, visio::SHAPE_MARGIN, surf->w - 2 * visio::SHAPE_MARGIN, surf->h - 2 * visio::SHAPE_MARGIN, color, surf);

		} else {
			const std::string filename = shape.stem_png;
			surf = image::get_image(filename);
		}

	} else if (type == dragtype_move) {
		VALIDATE(downing_obj_ != nullptr, null_str);

		gui2::tfloat_widget& float_drag = window_->float_drag();
		float_drag.set_visible(allow);
		surf = downing_obj_->obj->surf;

		select_object(downing_obj_);

	} else {
		VALIDATE(false, null_str);
	}
	cursor::set(allow? cursor::NORMAL: cursor::ILLEGAL_DRAG);

    window.set_drag_surface(surf, false);
	return true;
}

void mkscript_controller::did_drag_mouse_leave(int type, const int x, const int y, bool up_result)
{
	tclear_drag_obj_lock lock(*this, type);
/*
	if (type == dragtype_place) {
		VALIDATE(longpress_shape_ != nullptr, null_str);

	} else if (type == dragtype_move) {
		VALIDATE(downing_obj_ != nullptr, null_str);

	} else {
		VALIDATE(false, null_str);
	}
*/
	if (up_result) {
		int xmap = x, ymap = y;
		gui_->screen_2_map(xmap, ymap);
		const SDL_Rect map_rect = gui_->main_map_rect();

		if (point_in_rect(xmap, ymap, map_rect)) {
			if (type == dragtype_place) {
				// int stem_png_size = float_drag.custom_xy_formula.x;
				// surface surf = longpress_shape_->get_surf(nposm, nposm);
				tdraw_item_C* item = insert_draw_item(*longpress_shape_, SDL_Rect{xmap + longpress_custom_xy_formula_.x, 
					ymap + longpress_custom_xy_formula_.y, nposm, nposm}, true);
				select_object(item);

			} else if (type == dragtype_move) {
				downing_obj_->rect.x = xmap + longpress_custom_xy_formula_.x;
				downing_obj_->rect.y = ymap + longpress_custom_xy_formula_.y;
				downing_obj_->obj->calculate_stretch_rects(downing_obj_->rect);

				reconnect_edges(*downing_obj_);

				select_object(downing_obj_);
			}
			gui_->redraw_minimap();
		}
	}
}

void mkscript_controller::do_right_click()
{
	if (draw_items_.vsize != min_vsize_) {
		select_object(nullptr);

	} else {
		gui_->show_context_menu();
	}
}

bool mkscript_controller::wkoscript_dirty() const
{
	return !tmp_script_.equal(script_);
}

// true: ok, user don't cancel.
// fasle: user cancel it.
bool mkscript_controller::confirm_file_op(int sel)
{
	VALIDATE(file_ops_.count(sel) != 0, null_str);

	bool always_show = sel == file_exit;
	std::string action = file_ops_.find(sel)->second;
	if (sel == file_new_from_benchmark) {
		action = _("file^New from benchmark");

	} else if (sel == file_new_empty) {
		action = _("file^New empty script");
	}

	utils::string_map symbols;
	symbols["action"] = action;
	std::string message = vgettext2("Are you sure you want to $action?", symbols);
	bool dirty = wkoscript_dirty();
	if (dirty || always_show) {
		if (dirty) {
			std::string str = vgettext2("The file has been modified. Do you want to $action without saving?", symbols);
			message += "\n\n" + ht::generate_format(str, color_to_uint32(font::BAD_COLOR));
		}
		const int res = gui2::show_message2(action, message, gui2::tmessage::yes_no_buttons);
		if (res == gui2::twindow::CANCEL) {
			return false;
		}
	}

	return true;
}

std::string gettext_load_file_fail(const std::string& file_type, const std::string& filename)
{
	// std::string full_filename = utils::os_normalize_path(filename);
	std::string full_filename = filename;
	std::string short_filename;

	if (SDL_IsFromRootPath(full_filename.c_str())) {
		short_filename = utils::extract_file(filename);

	} else {
		short_filename = full_filename;
		full_filename.clear();
	}

	std::string msgstr;
	utils::string_map symbols;
	if (full_filename.empty()) {
		symbols["type"] = file_type;
		symbols["filename"] = filename;

		msgstr = vgettext2("load file fail. $type, $filename", symbols);

	} else {
		symbols["type"] = file_type;
		symbols["short_filename"] = short_filename;
		symbols["full_filename"] = full_filename;
		msgstr = vgettext2("load file fail. $type, $short_filename, $full_filename", symbols);
	}
	return msgstr;
}

void mkscript_controller::open_cfg_file_bh(const std::string& filename)
{
	aplt::twkoscript script;
	script.from_file(filename, false);
	std::string err_msg;
	if (script.is_valid2(err_msg, nullptr) != TCOOKIE3F_CHECK_OK || !script.is_id_same_filename(filename)) {
		std::string reason;
		if (script.valid()) {
			reason = _("The filename and ID are different.");
		}
		const std::string msg = i18n::freq_msgstr_3str(i18n::msgid_load_file_fail, _("Action script"), filename, reason);
		gui2::show_message(null_str, msg);
		return;
	}
	if (!confirm_file_op(file_open)) {
		return;
	}
	preferences::set_last_wkoscript_file(filename);

	select_object(nullptr);
	draw_flowchart_from_script(filename, script);

	gui_->show_context_menu();
}

void mkscript_controller::handle_file_op(int sel)
{
	if (sel == file_new_empty) {
		if (!confirm_file_op(sel)) {
			return;
		}
		new_empty_flowchart();
		// don't update last_wkoscript_file, it will change browse wkoscript directory.
		// preferences::set_last_wkoscript_file(null_str);

		gui_->show_context_menu();

	} else if (sel == file_new_from_benchmark) {
		if (!confirm_file_op(sel)) {
			return;
		}

		aplt::twkoscript script;
		{
			gui2::tnew_wkoscript dlg(rdpd_mgr_, pble_, privacy_, script, preset_poses_, wkoscript_dir_);
			dlg.show();
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
		}
		select_object(nullptr);

		std::string filename = script.build_script_filename(wkoscript_dir_);
		script.to_file(filename);

		draw_flowchart_from_script(filename, script);

		preferences::set_last_wkoscript_file(filename_);
		gui_->redraw_minimap();

		std::string err_msg;
		VALIDATE(tmp_script_.is_valid2(err_msg, nullptr) == TCOOKIE3F_CHECK_OK, null_str);
		if (tmp_script_.is_valid2(err_msg, nullptr) != TCOOKIE3F_CHECK_OK) {
			script_.clear();
			script_.id = tmp_script_.id;
		}
		gui_->show_context_menu();

	} else if (sel == file_open) {
		const std::string filename = handle_browse_file2(true, true, null_str, false);
		if (filename.empty()) {
			return;
		}

		open_cfg_file_bh(filename);

	} else if (sel == file_save_as) {
		if (wkoscript_dirty() && !handle_pre_save()) {
			return;
		}
		std::string filename = handle_browse_file2(true, false, null_str, true);
		if (filename.empty()) {
			return;
		}
		tmp_script_.to_file(filename);

	} else if (sel == file_exit) {
		if (!confirm_file_op(sel)) {
			return;
		}
		int mode = gui2::twindow::OK;
		do_quit_ = true;
		quit_mode_ = mode;
	}
}

void mkscript_controller::click_system()
{
	enum {_LOAD, _SAVE, _SAVE_AS, _PREFERENCES, _QUIT};
/*
	int retval;
	std::vector<gui2::tsystem::titem> items;
	std::vector<int> rets;

	const config top = generate_window_cfg().child("window");
	const unit* window = get_window();
	bool window_dirty = original_.first != window->cell().window.app || original_.second != top;

	{
		std::string str = _("Save");

		items.push_back(gui2::tsystem::titem(_("Load")));
		rets.push_back(_LOAD);

		items.push_back(gui2::tsystem::titem(str, window_is_valid(false) && window_dirty && !file_.empty()));
		rets.push_back(_SAVE);

		items.push_back(gui2::tsystem::titem(_("Save As"), window_is_valid(false)));
		rets.push_back(_SAVE_AS);

		items.push_back(gui2::tsystem::titem(_("Preferences")));
		rets.push_back(_PREFERENCES);

		items.push_back(gui2::tsystem::titem(_("Quit")));
		rets.push_back(_QUIT);

		gui2::tsystem dlg(items);
		try {
			dlg.show();
			retval = dlg.get_retval();
		} catch(twml_exception& e) {
			e.show();
			return;
		}
		if (retval == gui2::twindow::OK) {
			return;
		}
	}
	bool require_show_context_menu = false;
	if (rets[retval] == _LOAD) {
		load_window();	

	} else if (rets[retval] == _SAVE) {
		save_window(false);
		require_show_context_menu = true;

	} else if (rets[retval] == _SAVE_AS) {
		save_window(true);
		require_show_context_menu = true;

	} else if (rets[retval] == _PREFERENCES) {
		preferences::show_preferences_dialog(gui_->video());
		// gui_->redraw_everything();

	} else if (rets[retval] == _QUIT) {
		quit_confirm(gui2::twindow::OK, window_dirty);
	}

	if (require_show_context_menu) {
		gui_->show_context_menu();
	}
*/
	bool window_dirty = true;
	// quit_confirm(gui2::twindow::OK, window_dirty);
}

// true: no error, can save.
// false: has error, cannot save.
bool mkscript_controller::handle_pre_save()
{
	VALIDATE(wkoscript_dirty(), null_str);

	const aplt::twkoscript& script = tmp_script_;

	tdraw_item_C* err_item = nullptr;
	std::string err_msg;
	const aplt::twkoscript::tstate2* err_state2 = nullptr;
	uint64_t res = script.is_valid2(err_msg, &err_state2);
	if (res != TCOOKIE3F_CHECK_OK) {
		err_msg = aplt::twkoscript::fomrat_is_valid2_result(res, err_msg);

		if (err_state2 != nullptr) {
			err_item = find_draw_item_for_state(err_state2->state);
			VALIDATE(err_item != nullptr, null_str);
		}

	} else {
		VALIDATE(err_msg.empty(), null_str);
	}

	if (err_msg.empty()) {
		std::set<int> existed_ends;
		tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
		for (int at = 0; at < draw_items_.vsize; at ++) {
			tdraw_item_C* item = items + at;
			if (item->obj->shape.type == visio::shape_edge) {
				const tedge* edge = static_cast<tedge*>(item->obj);
				if (existed_ends.count(edge->end.obj_at) != 0) {
					err_item = find_draw_item_for_state(edge->end.obj_at);
					break;
				}
				existed_ends.insert(edge->end.obj_at);
			}
		}
		if (err_item != nullptr) {
			err_msg = _("Any state appears at most once as the next state of another state.");
		}
	}

	if (!err_msg.empty()) {
		std::stringstream err;
		err << err_msg;

		err << "\n\n";
		err << _("There is a data error, and cannot save.");
		gui2::show_message(null_str, err.str());

		if (err_item != nullptr) {
			scroll_to_object(*err_item);
		}

		return false;
	}
	return true;
}

void rename_image_pair(const std::string& phase_surf_dir, int old_at, int new_at)
{
	for (int at = 0; at < WKO_MAX_PHASE_COUNT; at ++) {
		const std::string old_path = aplt::twkoscript::build_lmk33_png_filename3(phase_surf_dir, old_at, at, false);
		const std::string new_path = aplt::twkoscript::build_lmk33_png_filename3(phase_surf_dir, new_at, at, false);
		const std::string new_basename = aplt::twkoscript::build_lmk33_png_basename(new_at, at);

		SDL_DeleteFiles(new_path.c_str());
		if (SDL_IsFile(old_path.c_str())) {
			SDL_RenameFile(old_path.c_str(), new_basename.c_str());
		}
	}
}

// Temporary renaming (used to handle circular dependencies)
void rename_to_tmp(const std::string& phase_surf_dir, int old_at, int new_at)
{
	for (int at = 0; at < WKO_MAX_PHASE_COUNT; at ++) {
		const std::string old_path = aplt::twkoscript::build_lmk33_png_filename3(phase_surf_dir, old_at, at, false);
		const std::string new_tmp_path = aplt::twkoscript::build_lmk33_png_filename3(phase_surf_dir, new_at, at, true);
		const std::string new_tmp_basename = utils::extract_file(new_tmp_path);

		SDL_DeleteFiles(new_tmp_path.c_str());
		if (SDL_IsFile(old_path.c_str())) {
			SDL_RenameFile(old_path.c_str(), new_tmp_basename.c_str());
		}
	}
}

// Restore from temporary file.
void rename_from_tmp(const std::string& phase_surf_dir, int tmp_at, int final_at)
{
	for (int at = 0; at < WKO_MAX_PHASE_COUNT; at ++) {
		const std::string tmp_path = aplt::twkoscript::build_lmk33_png_filename3(phase_surf_dir, tmp_at, at, true);
		const std::string final_path = aplt::twkoscript::build_lmk33_png_filename3(phase_surf_dir, final_at, at, false);
		const std::string final_basename = utils::extract_file(final_path);

		SDL_DeleteFiles(final_path.c_str());
		if (SDL_IsFile(tmp_path.c_str())) {
			SDL_RenameFile(tmp_path.c_str(), final_basename.c_str());
		}
	}

}

static void apply_rename_lmk33_png(aplt::twkoscript& tmp_script_, const std::string& wkoscript_dir_, int next_lmk33_png_at_)
{
	// Step 1: Build a renaming plan, old_lmk33_at -> new_lmk33_at
	const std::string phase_surf_dir = tmp_script_.build_phase_surf_dir(wkoscript_dir_);

	int old_to_new_map[aplt::workoutn32_max_states];
    for (int i = 0; i < aplt::workoutn32_max_states; i++) {
		old_to_new_map[i] = nposm;
	}

	for (std::map<int, aplt::twkoscript::tstate2>::iterator it = tmp_script_.states.begin(); it != tmp_script_.states.end(); ++ it) {
		aplt::twkoscript::tstate2& state2 = it->second;

		if (state2.lmk33_png_at != state2.state) {
			VALIDATE(old_to_new_map[state2.lmk33_png_at] == nposm, null_str);
			VALIDATE(state2.lmk33_png_at < next_lmk33_png_at_, null_str);
			old_to_new_map[state2.lmk33_png_at] = state2.state;
		}
	}

	// Step 2: Detect and handle circular dependencies.
	bool visited[aplt::workoutn32_max_states] = {false};

	for (int old_lmk33_at = 0; old_lmk33_at < next_lmk33_png_at_; old_lmk33_at ++) {
		if (old_to_new_map[old_lmk33_at] == nposm) {
			continue;
		}
		if (visited[old_lmk33_at]) {
			continue;
		}

        int cycle[aplt::workoutn32_max_states];
        int cycle_len = 0;
        int cur = old_lmk33_at;

		while (old_to_new_map[cur] != nposm && !visited[cur]) {
			cycle[cycle_len++] = cur;
            visited[cur] = true;
            cur = old_to_new_map[cur];
		}

		// Check if a cycle is formed (cur == cycle[0]).
        bool is_cycle = false;
        for (int i = 0; i < cycle_len; i++) {
            if (cycle[i] == cur) {
                is_cycle = true;
                break;
            }
        }

		if (is_cycle && cycle_len > 1) {
            // Handle cycles: first rename all to .tmp
            for (int i = 0; i < cycle_len; i++) {
                int cid = cycle[i];
				VALIDATE(old_to_new_map[cid] != nposm, null_str);
				int target = old_to_new_map[cid];

                rename_to_tmp(phase_surf_dir, cid, target);
            }
            // Then rename from .tmp back to the final name.
            for (int i = 0; i < cycle_len; i++) {
                int cid = cycle[i];
				VALIDATE(old_to_new_map[cid] != nposm, null_str);
                int target = old_to_new_map[cid];

                rename_from_tmp(phase_surf_dir, target, target);
            }
        } else {
            // No cycles, rename directly.
            for (int i = 0; i < cycle_len; i++) {
                int cid = cycle[i];
				VALIDATE(old_to_new_map[cid] != nposm, null_str);
                int target = old_to_new_map[cid];

                rename_image_pair(phase_surf_dir, cid, target);
            }
        }
	}

	// Step 3: Delete images corresponding to unreferenced real IDs.
	for (int id = tmp_script_.states.size(); id < next_lmk33_png_at_; id ++) {
		for (int phase_at = 0; phase_at < WKO_MAX_PHASE_COUNT; phase_at ++) {
			const std::string path = aplt::twkoscript::build_lmk33_png_filename3(phase_surf_dir, id, phase_at, false);
			if (SDL_IsFile(path.c_str())) {
				SDL_DeleteFiles(path.c_str());
			}
		}
	}
}

void mkscript_controller::click_save()
{
	bool no_error = handle_pre_save();
	if (!no_error) {
		return;
	}

	bool filename_is_changed = false;
	if (filename_.empty() || tmp_script_.id != script_.id) {
		VALIDATE(!script_.id.empty(), null_str);
		filename_is_changed = true;

		// filename
		const std::string desire_filename = tmp_script_.build_script_filename(wkoscript_dir_);
		const std::string desire_short_file = utils::extract_file(desire_filename);
		// 1/2: Delete the files and directories that will be generated this time.
		if (SDL_IsFile(desire_filename.c_str())) {
			utils::string_map symbols;
			symbols["file"] = desire_short_file;
			std::string msg = vgettext2("The target location already contains '$file'. Do you want to replace this file?", symbols);
			if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
				return;
			}
			SDL_DeleteFiles(desire_filename.c_str());
		}
		if (!filename_.empty()) {
			VALIDATE(filename_ == script_.build_script_filename(wkoscript_dir_), null_str);

			// 2/2: rename old to new.
			VALIDATE(tmp_script_.id != script_.id, null_str);
			// const std::string src_file = script_.build_script_filename(wkoscript_dir_);
			// SDL_RenameFile(src_file.c_str(), desire_short_file.c_str());
			SDL_RenameFile(filename_.c_str(), desire_short_file.c_str());
		}

		// 
		if (tmp_script_.id != script_.id) {
			const std::string desire_dir = tmp_script_.build_phase_surf_dir(wkoscript_dir_);
			SDL_DeleteFiles(desire_dir.c_str());

			const std::string src_dir = script_.build_phase_surf_dir(wkoscript_dir_);
			SDL_RenameFile(src_dir.c_str(), tmp_script_.id.c_str());
		}

		filename_ = desire_filename;
	}
	VALIDATE(!filename_.empty(), null_str);

	// handle lmk33_<state>_[0|1].png
	// The phase_surf directory has been renamed to tmp_script_.id, so use tmp_script_.
	// const std::string phase_surf_dir = tmp_script_.build_phase_surf_dir(wkoscript_dir_);
	apply_rename_lmk33_png(tmp_script_, wkoscript_dir_, next_lmk33_png_at_);

	tmp_script_.set_lmk33_png_at_equal_to_state_at();
	next_lmk33_png_at_ = tmp_script_.states.size();

	script_.assign(tmp_script_);
	script_.to_file(filename_);

	bool dbg_handle_pngs = true;
	if (dbg_handle_pngs) {
		tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
		for (int at = 0; at < draw_items_.vsize; at ++) {
			tdraw_item_C& item = items[at];
			if (shape_type_is_state(item.obj->shape.type)) {
				item.obj->fresh_2surf(item.rect.w, item.rect.h);
			}
		}
	}

	if (filename_is_changed) {
		preferences::set_last_wkoscript_file(filename_);
		update_title_label();
	}

	gui_->show_context_menu();
}

void mkscript_controller::list_wkoscript_files2(int type, std::set<std::string>& result_set)
{
	if (!filename_.empty()) {
		VALIDATE(wkoscript_dir_ == utils::extract_directory(filename_), null_str);
	}
	aplt::list_wkoscript_files_by_type(wkoscript_dir_, aplt::type_wkoscript_ids, result_set);
}

bool mkscript_controller::did_verify_text_changed(const std::string& label, const std::string& initial, int type, const std::set<std::string>& xcludes) const
{
	if (label == initial) {
		return false;
	}

	if (label.empty()) {
		return type == edittype_author || type == edittype_reference;
	}

	if (!xcludes.empty() && xcludes.count(label) != 0) {
		return false;
	}

	if (type == edittype_id) {
		return isvalid_normal_id_or_var_name224(label);

	} else if (type == edittype_name || type == edittype_author) {
		return isvalid_short_utf8_name216(label);

	} else if (type == edittype_reference) {
		return label.size() <= WKO_MAX_REFERENCE_BYTES;
	}

	VALIDATE(type == edittype_state_name, null_str);
	return isvalid_normal_utf8_name224(label);
}

void mkscript_controller::handle_edit_text(int type)
{
	aplt::twkoscript& script = tmp_script_;
	std::string* target = nullptr;

	std::set<std::string> xcludes;
	std::string type_name;
	int max_chars = MAX_NORMAL_UTF8_NAME_CHARS;
	std::string initial;
	std::string remark = i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);
	bool scroll = false;
	if (type == edittype_id) {
		type_name = "ID";
		max_chars = MAX_NORMAL_ID_OR_VAR_NAME_BYTES;
		remark = _("It will be used as the stem name of the script file.");
		remark.append(i18n::freq_msgstr(i18n::msgid_isvalid_normal_id_or_var_name));
		target = &script.id;

		list_wkoscript_files2(aplt::type_wkoscript_ids, xcludes);
		if (!filename_.empty()) {
			std::set<std::string>::iterator it = xcludes.find(script_.id);
			VALIDATE(it != xcludes.end(), null_str);
			xcludes.erase(script_.id);
		}

	} else if (type == edittype_name) {
		type_name = _("Title");
		max_chars = MAX_SHORT_UTF8_NAME_CHARS;
		target = &script.title;

	} else if (type == edittype_author) {
		type_name = _("Author");
		max_chars = MAX_SHORT_UTF8_NAME_CHARS;
		target = &script.author;

	} else if (type == edittype_reference) {
		type_name = _("Reference");
		max_chars = WKO_MAX_REFERENCE_BYTES;
		target = &script.reference;
		scroll = true;

	} else if (type == edittype_state_name) {
		type_name = _("State name");
		aplt::twkoscript::tstate2& state2 = mutable_state2_from_sel_obj_with_validate();
		target = &tmp_script_.state_names[state2.state];
		for (std::vector<std::string>::const_iterator it = tmp_script_.state_names.begin(); it != tmp_script_.state_names.end(); ++ it) {
			const std::string& name = *it;
			xcludes.insert(name);
		}

	} else {
		VALIDATE(false, null_str);
	}

	initial = *target;

	utils::string_map symbols;
	symbols["type"] = type_name;
	std::string title = vgettext2("Edit $type", symbols);

	std::string prefix;
    std::string placeholder;

	std::string new_name;
	{
		gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), max_chars, gui2::tedit_box_param::show_cancel, scroll);
		param.did_text_changed = std::bind(&mkscript_controller::did_verify_text_changed, this, _1, 
			std::ref(initial), type, std::ref(xcludes));
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_->get_height() / 5);
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}

			if (type == edittype_id) {
				// The 'id' is part of the filename. On Windows, filenames are case-insensitive, 
				// so the 'id' must also be case-insensitive. It is assumed to be lowercase.
				utils::lowercase2(param.result);
				if (param.result == initial) {
					return;
				}
			}
		}
		new_name = param.result;
	}

	VALIDATE(new_name != initial, null_str);
	*target = new_name;

	if (type == edittype_id || type == edittype_name || type == edittype_author || type == edittype_reference) {
		int textid = nposm;
		if (type == edittype_id) {
			textid = visio::textid_id;

		} else if (type == edittype_name) {
			textid = visio::textid_name;

		} else if (type == edittype_author) {
			textid = visio::textid_author;

		} else if (type == edittype_reference) {
			textid = visio::textid_reference;
		}
		VALIDATE(textid >= 0 && textid < visio::textid_sys_count, null_str);

		tdraw_item_C* item = find_draw_item_for_text(textid, nullptr);

		ttext* text = static_cast<ttext*>(item->obj);
		item->rect = text->set_label(generate_textid_label(textid, *target));

	} else if (type == edittype_state_name) {
		sel_obj_->obj->fresh_2surf(sel_obj_->rect.w, sel_obj_->rect.h);
	}

	gui_->show_context_menu();
}

void mkscript_controller::click_edit_state_name()
{
	handle_edit_text(edittype_state_name);
}

void mkscript_controller::click_setting()
{
	aplt::twkoscript::tstate2& state2 = mutable_state2_from_sel_obj_with_validate();
	const std::string state_name = tmp_script_.state_names[state2.state];
	if (state2.task->type == aplt::twkoscript::tasktype_speak) {
		VALIDATE(!state2.track_pose.valid(), null_str);
		{
			gui2::tspeak_state2 dlg(rdpd_mgr_, pble_, privacy_, state2, tmp_script_.state_names, sdl_field_small_font_size_);
			dlg.show();
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			bool state2_dirty = state2 != dlg.get_new_state2();
			if (!state2_dirty && state_name == tmp_script_.state_names[state2.state]) {
				return;
			}
			if (state2_dirty) {
				state2 = dlg.get_new_state2();
			}
		}

	} else {
		VALIDATE(state2.track_pose.valid(), null_str);
		{
			gui2::tpose_state2 dlg(rdpd_mgr_, pble_, privacy_, state2, tmp_script_.state_names, preset_poses_,
				sdl_field_small_font_size_, script_.build_phase_surf_dir(wkoscript_dir_));
			dlg.show();
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			bool state2_dirty = state2 != dlg.get_new_state2();
			if (!state2_dirty && !dlg.phase_surf_updated() && state_name == tmp_script_.state_names[state2.state]) {
				return;
			}
			if (state2_dirty) {
				state2 = dlg.get_new_state2();
			}
		}
	}
	sel_obj_->obj->fresh_2surf(sel_obj_->rect.w, sel_obj_->rect.h);

	gui_->show_context_menu();
}

void mkscript_controller::click_next_state()
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;
    
	// gui2::tcontext_menu& menu = dlg_->context_menus().front();
	// gui2::tcontrol* widget = menu.report->item(

	aplt::twkoscript& script = tmp_script_;
	aplt::twkoscript::tstate2& state2 = mutable_state2_from_sel_obj_with_validate();
	int original_to_state = nposm;
	if (!state2.next.branches.empty()) {
		original_to_state = state2.next.branches[0].do_to_state;
	}

	std::set<int> xclude;
	xclude.insert(state2.state); // 1/2)myself
	const tdraw_item_C* items2 = (const tdraw_item_C*)draw_items_.data;
	for (int at = 0; at < draw_items_.vsize; at ++) {
		const tdraw_item_C& item = items2[at];
		// 1/2)end is myself
		if (item.obj->shape.type == visio::shape_edge) {
			tedge* edge = static_cast<tedge*>(item.obj);
			if (edge->end.obj_at == state2.state) {
				xclude.insert(edge->start.obj_at);
			}
		}
	}

	const int state_count = script.states.size();
	items.push_back(gui2::tmenu::titem(_("Empty"), script.state_names.size()));
	if (original_to_state == nposm) {
		initial_sel = items.back().val;
	}
	for (int at = 0; at < state_count; at ++) {
		if (xclude.count(at) != 0) {
			// xclude myself
			continue;
		}
		ss.str("");
		ss << at + 1 << " " << script.state_names[at];
		items.push_back(gui2::tmenu::titem(ss.str(), at));

		if (at == original_to_state) {
			initial_sel = at;
		}
	}

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	// dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	dlg.show();
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	int to_state = nposm;
	const int cursel = dlg.selected_val();
	// 1/2: update twkoscript
	if (cursel < script.states.size()) {
		to_state = cursel;
	}
	state2.set_next_to_state(to_state);

	// 2/2: update draw_items
	if (to_state == nposm) {
		// clear original edge
		while (sel_obj_->obj->edge_count != 0) {
			int item_at = nposm;
			tdraw_item_C* item = find_draw_item_for_edge(sel_obj_->obj->edges[0]->start.obj_at, &item_at);
			VALIDATE(item != nullptr, null_str);
			erase_edge_from_draw_items(item_at);
		}
	} else {
		tdraw_item_C* start = find_draw_item_for_state(state2.state);
		tdraw_item_C* end = find_draw_item_for_state(to_state);
		if (sel_obj_->obj->edge_count == 0) {
			insert_edge(*start, *end);

		} else {
			VALIDATE(sel_obj_->obj->edge_count == 1, null_str);
			int item_at = nposm;
			tdraw_item_C* item = find_draw_item_for_edge(sel_obj_->obj->edges[0]->start.obj_at, &item_at);
			VALIDATE(item != nullptr, null_str);
			tedge* edge = static_cast<tedge*>(item->obj);
			item->rect = edge->set_endpoints2(*start, *end);
		}
	}

	validate_draw_items();

	gui_->redraw_minimap();
	gui_->show_context_menu();
}

void mkscript_controller::click_clone()
{
	aplt::twkoscript& script = tmp_script_;
	aplt::twkoscript::tstate2& state2 = mutable_state2_from_sel_obj_with_validate();

	script.clone_state(state2.state);
	aplt::twkoscript::tstate2& new_state2 = script.states.find(script.states.size() - 1)->second;
	VALIDATE(new_state2.lmk33_png_at == new_state2.state, null_str);
	VALIDATE(new_state2.lmk33_png_at == next_lmk33_png_at_, null_str);
	// linke insert_state, next_lmk33_png_at require '+1'.
	next_lmk33_png_at_ ++;

	const int zoom = gui_->zoom();
	SDL_Rect new_rect = sel_obj_->rect;
	new_rect.x += zoom;
	new_rect.y += zoom;

	const SDL_Rect map_rect = gui_->main_map_rect();
	if (new_rect.x >= map_rect.w - zoom / 2) {
		new_rect.x = map_rect.w - zoom;
	}
	if (new_rect.y >= map_rect.h - zoom / 2) {
		new_rect.y = map_rect.h - zoom;
	}

	tdraw_item_C* new_draw_item = insert_draw_item(sel_obj_->obj->shape, new_rect, false);
	select_object(new_draw_item);

	// validate_draw_items(); // insert_draw_item has called it.
	gui_->redraw_minimap();
	gui_->show_context_menu();
}

void mkscript_controller::click_switch_state(bool add1)
{
	aplt::twkoscript& script = tmp_script_;
	aplt::twkoscript::tstate2& state2 = mutable_state2_from_sel_obj_with_validate();

	select_object(nullptr);

	int s1 = state2.state;
	int s2;
	if (add1) {
		s2 = s1 + 1;
		VALIDATE(s2 < (int)script.states.size(), null_str);

	} else {
		s2 = s1 - 1;
		VALIDATE(s2 >= 0, null_str);
	}
	script.state_swap(s1, s2);

	switch_item(s1, s2);

	tdraw_item_C* s1_item = find_draw_item_for_state(s1);
	s1_item->obj->fresh_2surf(s1_item->rect.w, s1_item->rect.h);
	bool dbg_edge = true;
	if (dbg_edge) {
		reconnect_edges(*s1_item);
	}

	tdraw_item_C* s2_item = find_draw_item_for_state(s2);
	s2_item->obj->fresh_2surf(s2_item->rect.w, s2_item->rect.h);
	if (dbg_edge) {
		reconnect_edges(*s2_item);
	}

	select_object(s2_item);

	validate_draw_items();

	gui_->redraw_minimap();
	gui_->show_context_menu();
}

void mkscript_controller::click_erase()
{
	VALIDATE(downing_obj_ == nullptr, null_str);
	VALIDATE(calculate_edge_count(*sel_obj_, nullptr, nullptr) == 0, null_str);

	aplt::twkoscript::tstate2& state2 = mutable_state2_from_sel_obj_with_validate();

	const std::string& state_name = tmp_script_.state_names[state2.state];
	const std::string msg = i18n::freq_msgstr_2str(i18n::msgid_confirm_delete_2str, _("State"), state_name);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	select_object(nullptr);
	VALIDATE(sel_obj_ == nullptr, null_str);

	tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
	for (int at = 0; at < draw_items_.vsize; at ++) {
		tdraw_item_C& item = items[at];
		if (!shape_type_is_state(item.obj->shape.type)) {
			continue;
		}
		if (item.obj->obj_at == state2.state) {
			delete item.obj;
			item.obj = nullptr;
			draw_items_.drop_at(at);
			break;
		}
	}
	for (int at = 0; at < draw_items_.vsize; at ++) {
		tdraw_item_C& item = items[at];
		if (shape_type_is_state(item.obj->shape.type)) {
			if (item.obj->obj_at > state2.state) {
				item.obj->obj_at --;
			}
			
		} else if (item.obj->shape.type == visio::shape_edge) {
			tedge* edge = static_cast<tedge*>(item.obj);
			if (edge->start.obj_at > state2.state) {
				edge->start.obj_at --;
			}
			if (edge->end.obj_at > state2.state) {
				edge->end.obj_at --;
			}
		}
	}

	const int erase_state_at = state2.state;
	tmp_script_.erase_state(state2.state);
	// 'state2' has been deleted, and subsequent code cannot use 'state2'.

	// update surf/sel_surf. it requrie both tdraw_item and tstate2 are ok.
	for (int at = 0; at < draw_items_.vsize; at ++) {
		tdraw_item_C& item = items[at];
		if (shape_type_is_state(item.obj->shape.type)) {
			if (item.obj->obj_at >= erase_state_at) {
				item.obj->fresh_2surf(item.rect.w, item.rect.h);
			}
		}
	}

	validate_draw_items();

	gui_->redraw_minimap();
	gui_->show_context_menu();
}

void mkscript_controller::click_copy_action_tpl()
{
	aplt::twkoscript& script = tmp_script_;
	const aplt::twkoscript::tstate2& state2 = state2_from_sel_obj_with_validate();

	VALIDATE(state2.task != nullptr, null_str);
	VALIDATE(wko_is_pose_state2_from_task_type(state2.task->type), null_str);

	aplt::taction_tpl2& action_tpl2 = clipboard_action_tpl2_;
	script.copy_action_tpl2(state2, action_tpl2);

	utils::string_map symbols;
	symbols["name"] = action_tpl2.name;
	gui2::show_message(null_str, vgettext2("Action copied to clipboard: $name", symbols));
}

void mkscript_controller::click_paste_action_tpl()
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	// std::stringstream ss;

	aplt::twkoscript& script = tmp_script_;
	aplt::twkoscript::tstate2& state2 = mutable_state2_from_sel_obj_with_validate();

	if (clipboard_action_tpl2_.valid() && wko_is_pose_state2_from_task_type(state2.task->type)) {
		// why not use to non-pose_state2?
		// --clipboard save taction_tpl only, no msg.
		std::string name(_("Clipboard"));
		name.append(": ");
		name.append(clipboard_action_tpl2_.name);

		items.push_back(gui2::tmenu::titem(name, action_tpl2s_.size()));
		items.back().separator = true;
	}

	int action_tlp2_at = 0;
	for (std::map<std::string, aplt::taction_tpl2>::const_iterator it = action_tpl2s_.begin(); it != action_tpl2s_.end(); ++ it, action_tlp2_at ++) {
		const aplt::taction_tpl2& action_tpl = it->second;
		items.push_back(gui2::tmenu::titem(action_tpl.name2(), action_tlp2_at));
	}

	if (items.empty()) {
		return;
	}

	int new_sel = nposm;
	{
		gui2::tmenu dlg(items, initial_sel);
		// dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		new_sel = dlg.selected_val();
	}

	const aplt::taction_tpl2* p_action_tpl2 = nullptr;
	if (new_sel < (int)action_tpl2s_.size()) {
		std::map<std::string, aplt::taction_tpl2>::const_iterator sel_it = action_tpl2s_.begin();
		if (new_sel != 0) {
			std::advance(sel_it, new_sel);
		}
		p_action_tpl2 = &sel_it->second;

	} else {
		p_action_tpl2 = &clipboard_action_tpl2_;
	}
	script.apply_action_tpl2(state2, *p_action_tpl2);

	sel_obj_->obj->fresh_2surf(sel_obj_->rect.w, sel_obj_->rect.h);

	// validate_draw_items();

	gui_->redraw_minimap();
	gui_->show_context_menu();
}

void mkscript_controller::click_add_to_working_dir()
{
	const std::string short_filename = utils::extract_file(filename_);

	std::string new_filename = wkoscript_dir_ + "/" + short_filename;
	if (SDL_IsFile(new_filename.c_str())) {
		std::string msg = _("A file with the same name exists in the working directory. Do you want to overwrite it?");
		const int res = gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons);
		if (res == gui2::twindow::CANCEL) {
			return;
		}
	}
	SDL_CopyFiles(filename_.c_str(), new_filename.c_str());

	VALIDATE(!wkoscript_dirty(), null_str);
	open_cfg_file_bh(new_filename);

	preferences::set_last_browse_file_path(utils::extract_directory(new_filename));
}

void mkscript_controller::click_share()
{
	std::string filename_prfix = "share_action_flowchart-";
	const std::string filename = filename_prfix + utils::format_time_ymdhms2(time(nullptr)) + ".png";
	const std::string full_filename = game_config::preferences_dir + "/saves/" + filename;

	surface surf = gui_->screenshot(true);
	VALIDATE(surf.get() != nullptr, null_str);

	SDL_Point* points = (SDL_Point*)malloc(draw_items_.vsize * 2 * sizeof(SDL_Point));
	int point_count = 0;

	tdraw_item_C* items = (tdraw_item_C*)draw_items_.data;
	points[point_count ++] = SDL_Point{0, 0};
	for (int at = 0; at < draw_items_.vsize; at ++) {
		tdraw_item_C& item = items[at];

		SDL_Point& lbottom = points[point_count];
		lbottom.x = item.rect.x + item.rect.w;
		lbottom.y = item.rect.y + item.rect.h;

		if (shape_type_is_state(item.obj->shape.type)) {
			point_count ++;

		} else if (item.obj->shape.type == visio::shape_text) {
			point_count ++;
		}
	}

	SDL_Rect enclose_rect;
    SDL_EnclosePoints(points, point_count, nullptr, &enclose_rect);

	VALIDATE(enclose_rect.x == 0 && enclose_rect.y == 0, null_str);
	SDL_Rect result_rect = enclose_rect;
	const SDL_Size& margin = flowchart_margin_;
	result_rect.w += margin.w;
	if (result_rect.w > surf->w) {
		result_rect.w = surf->w;
	}
	result_rect.h += margin.h;
	if (result_rect.h > surf->h) {
		result_rect.h = surf->h;
	}
	if (result_rect.w != surf->w || result_rect.h != surf->h) {
		surf = cut_surface(surf, result_rect);
	}
	free(points);


	imwrite(surf, full_filename);

	utils::string_map symbols;
	symbols["type"] = _("Action flowchart");
	symbols["file"] = full_filename;
	const std::string msg = vgettext2("$type file has been generated.\nPath: $file", symbols);
	gui2::show_message(null_str, msg);
}