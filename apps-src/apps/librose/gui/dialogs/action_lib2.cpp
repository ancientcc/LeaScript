#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/action_lib2.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "wkoscript.hpp"
#include "font.hpp"
#include "rose_config.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, action_lib2)

taction_lib2::taction_lib2(int scene, const std::string& caller_id)
	: scene_(scene)
	, caller_id_(caller_id)
	, action_list_(nullptr)
	, ok_widget_(nullptr)
{
	VALIDATE(scene_ >= 0 && scene_ < scene_count, null_str);
	if (scene_ == scene_paste) {
		curr_sel_id_ = caller_id;

	} else {
		VALIDATE(!caller_id.empty(), null_str);
	}
}

void taction_lib2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	find_widget<tlabel>(window_, "title", false).set_label(_("Action library"));

	tbutton* button = find_widget<tbutton>(window_, "share", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&taction_lib2::click_share
			, this
			, std::ref(*button)));

	tlistbox* list = find_widget<tlistbox>(window_, "action_list", false, true);
	// list->enable_select(false);
	// list->set_did_row_pre_change(std::bind(&tlatex_editor::did_tpl_list_row_pre_change, this, _1, _2));
	list->set_did_row_changed(std::bind(&taction_lib2::did_action_list_row_changed, this, _1, _2));
	action_list_ = list;

	button = find_widget<tbutton>(window_, "ok2", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&taction_lib2::click_ok
			, this
			, std::ref(*button)));
	if (scene_ == scene_paste) {
		button->set_label(_("Paste"));

	} else {
		button->set_visible(twidget::INVISIBLE);
	}
	ok_widget_ = button;

	ok_widget_->set_active(is_ok_active());

	reload_action_list(*action_list_);
}

void taction_lib2::post_show()
{
}

std::string action_tpl2_task_str(const aplt::taction_tpl2& tpl2)
{
	VALIDATE(wko_is_pose_state2_from_task_type(tpl2.task->type), null_str);

	std::string result;
	utils::string_map symbols;
	if (tpl2.task->type == aplt::twkoscript::tasktype_time_counter) {
		const aplt::twkoscript::ttime_counter* task2 = static_cast<const aplt::twkoscript::ttime_counter*>(tpl2.task);
		symbols["max_count"] = str_cast(task2->max_count);
		result = vgettext2("action_tpl2_task_str, time_counter, $max_count", symbols);

	} else {
		const aplt::twkoscript::trep_counter* task2 = static_cast<const aplt::twkoscript::trep_counter*>(tpl2.task);
		symbols["max_count"] = str_cast(task2->max_count);
		std::string phase_str;
		for (std::vector<aplt::twkoscript::tphase>::const_iterator it = task2->phases.begin(); it != task2->phases.end(); ++ it) {
			const aplt::twkoscript::tphase& phase = *it;
			if (!phase_str.empty()) {
				phase_str.append("/");
			}
			phase_str.append(phase.action_msg);
		}
		symbols["phase"] = phase_str;
		result = vgettext2("action_tpl2_task_str, rep_counter, $max_count, $phase", symbols);
	}
	return result;
}

static void increment_s(int this_w, int this_h, SDL_Size& s)
{
	if (this_w > s.w) {
		s.w = this_w;
	}
	s.h += this_h;
}

static void draw_line_surf(bool with_save, const surface& line_surf, int indentation_w, SDL_Size& s, surface& bg_surf)
{
	VALIDATE(line_surf.get() != nullptr, null_str);
	VALIDATE(indentation_w >= 0, null_str);

	if (with_save) {
		SDL_Rect dst_rect = ::create_rect(indentation_w, s.h, line_surf->w, line_surf->h);
		sdl_blit(line_surf, nullptr, bg_surf, &dst_rect);
	}
	increment_s(indentation_w + line_surf->w, line_surf->h, s);
}

void draw_line_surf2(const surface& line_surf, int offset_x, int offset_y, const SDL_Size& cell_size, surface& bg_surf)
{
	VALIDATE(line_surf.get() != nullptr, null_str);
	VALIDATE(offset_x >= 0, null_str);
	VALIDATE(offset_y >= 0, null_str);
	VALIDATE(cell_size.w > 0 && cell_size.h > 0, null_str);

	SDL_Rect dst_rect = ::create_rect(offset_x, offset_y + (cell_size.h - line_surf->h) / 2, line_surf->w, line_surf->h);
	sdl_blit(line_surf, nullptr, bg_surf, &dst_rect);
}

enum {col_index, col_name, col_task, col_msg, col_count};

SDL_Size do_save_image_action_tpl2s(bool todo, int header_tail_truncate_days, int* max_col_widths, int* max_line_heights, surface* p_surf)
{
	bool with_save = false;
	surface bg_surf;
	if (p_surf != nullptr) {
		bg_surf = *p_surf;
		with_save = true;
	}

	std::string line_str;
	SDL_Size s = {0};
	surface tmp_surf;
	utils::string_map symbols;
	// SDL_Rect dst_rect;

	// surface split_line_surf = image::get_image("misc/split_line.png");
	// VALIDATE(split_line_surf.get() != nullptr, null_str);

	const int msg_font_size = font::SIZE_SMALL;
	tpoint one_chinese_size = font::get_rendered_text_size(dgettext("rose-lib", "time^s"), INT_MAX, msg_font_size);
	int max_one_line_msg_chars = 16;
	int max_msg_width = one_chinese_size.x * max_one_line_msg_chars;


	const int split_line_h = 1;
	int gap_y_line = 2;
	//
	// workouts
	//
	line_str = _("Action library");
	if (todo) {
		std::string todo = _("todo^action library");
		line_str.append(ht::generate_format("(" + todo + ")", 0xffff0000, font::SIZE_SMALL)); 
	}
	tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_LARGE, font::GRAY_COLOR);
	draw_line_surf(with_save, tmp_surf, 0, s, bg_surf);

	s.h += gap_y_line;
	if (with_save) {
		draw_rectangle(0, s.h, bg_surf->w, split_line_h, 0x80808080, bg_surf);
	}
	s.h += split_line_h;
	s.h += gap_y_line;

	int line_at = 0;
	for (std::map<std::string, aplt::taction_tpl2>::const_iterator it = aplt::action_tpl2s.begin(); it != aplt::action_tpl2s.end(); ++ it) {
		const aplt::taction_tpl2& tpl2 = it->second;
		if (tpl2.is_setup) {
			continue;
		}

		int offset_x = 0;

		SDL_Size cell_size;

		for (int col_at = 0; col_at < col_count; col_at ++) {
			int maximum_width = INT_MAX;
			int font_size = font::SIZE_DEFAULT;
			SDL_Color color = font::BLACK_COLOR;
			if (col_at == col_index) {
				line_str = str_cast(line_at + 1);
				color = font::GRAY_COLOR;

			} else if (col_at == col_name) {
				// col: name/id
				line_str = tpl2.name;
				line_str.append("\n");
				line_str.append(ht::generate_format(tpl2.id, color_to_uint32(font::GRAY_COLOR), font_size)); 

			} else if (col_at == col_task)  {
				line_str = action_tpl2_task_str(tpl2);
				int font_size = font::SIZE_SMALL;
				color = font::GRAY_COLOR;

			} else if (col_at == col_msg) {
				line_str = tpl2.msg;
				maximum_width = max_msg_width;
				font_size = msg_font_size;
				color = font::GRAY_COLOR;

			} else {
				VALIDATE(false, null_str);
			}

			tmp_surf = font::get_rendered_text(line_str, maximum_width, font_size, color);

			if (with_save) {
				cell_size = SDL_Size{max_col_widths[col_at], max_line_heights[line_at]};
				draw_line_surf2(tmp_surf, offset_x, s.h, cell_size, bg_surf);
				offset_x += cell_size.w;

			} else {
				int gap_width = 4;
				int cell_wdith = tmp_surf->w + gap_width;
				if (cell_wdith > max_col_widths[col_at]) {
					max_col_widths[col_at] = cell_wdith;
				}
				if (tmp_surf->h > max_line_heights[line_at]) {
					max_line_heights[line_at] = tmp_surf->h;
				}
			}
		}

		if (with_save) {
			if (offset_x > s.w) {
				s.w = offset_x;
			} else {
				VALIDATE(offset_x <= s.w, null_str);
			}
		}

		const int gap_y_between_actions = 3;

		s.h += max_line_heights[line_at];

		s.h += gap_y_between_actions;
		if (with_save) {
			draw_rectangle(0, s.h, bg_surf->w, split_line_h, 0x80808080, bg_surf);
		}
		s.h += split_line_h;
		s.h += gap_y_between_actions;

		line_at ++;
	}

	if (with_save) {
		VALIDATE(bg_surf->h == s.h, null_str);
		return s;
	}

	// VALIDATE(s.w == 0, null_str);
	int max_width_actions = 0; 
	for (int col_at = 0; col_at < col_count; col_at ++) {
		max_width_actions += max_col_widths[col_at];
	}
	if (max_width_actions > s.w) {
		s.w = max_width_actions;
	}

	return s;
}

void share_action_tpl2s()
{
	bool todo = true; // true
	int header_tail_truncate_days = nposm; // nposm, 4

#define MAX_TPL2S		300
	VALIDATE(aplt::action_tpl2s.size() < MAX_TPL2S, null_str);
	int max_col_widths[col_count];
	memset(max_col_widths, 0, sizeof(max_col_widths));

	int max_line_heights[MAX_TPL2S];
	memset(max_line_heights, 0, sizeof(max_line_heights));

	SDL_Size size = do_save_image_action_tpl2s(todo, header_tail_truncate_days, max_col_widths, max_line_heights, nullptr);

	surface content_surf = create_neutral_surface(size.w, size.h);
	fill_surface(content_surf, 0xffffffff);

	SDL_Size size2 = do_save_image_action_tpl2s(todo, header_tail_truncate_days, max_col_widths, max_line_heights, &content_surf);
	VALIDATE(size.w == size2.w && size.h == size2.h, null_str);

	std::string filename_prfix = "share_action_tpl2s";
	const std::string filename = filename_prfix + "-" + utils::format_time_ymdhms2(time(nullptr)) + ".png";
	const std::string full_filename = game_config::preferences_dir + "/saves/" + filename;

	SDL_Size margin{12, 12};
	surface result = create_neutral_surface(size.w + margin.w * 2, size.h + margin.h * 2);
	fill_surface(result, 0xffffffff);
	SDL_Rect dst_rect{margin.w, margin.h, content_surf->w, content_surf->h};
	sdl_blit(content_surf, nullptr, result, &dst_rect);

	imwrite(result, full_filename);

	utils::string_map symbols;
	symbols["type"] = _("Action library");
	symbols["file"] = full_filename;
	const std::string msg = vgettext2("$type file has been generated.\nPath: $file", symbols);
	gui2::show_message(null_str, msg);
}

void taction_lib2::click_share(tbutton& widget)
{
	share_action_tpl2s();
}

void taction_lib2::reload_action_list(tlistbox& list)
{
	list.clear();

	int sel_at = nposm;

	std::map<std::string, std::string> data;
	int line_at = 0;
	std::string label;
	for (std::map<std::string, aplt::taction_tpl2>::const_iterator it = aplt::action_tpl2s.begin(); it != aplt::action_tpl2s.end(); ++ it) {
		const aplt::taction_tpl2& tpl2 = it->second;

		if (tpl2.id == curr_sel_id_) {
			sel_at = line_at;
		}

		data["index"] = str_cast(line_at + 1);

		label = tpl2.name;
		label.append("\n");
		label.append(ht::generate_format(tpl2.id, color_to_uint32(font::GRAY_COLOR))); 
		data["name"] = label;
		data["task"] = action_tpl2_task_str(tpl2);
		data["msg"] = tpl2.msg;

		ttoggle_panel& row = list.insert_row(data);
		row.set_cookie(line_at);
/*
		row.connect_signal<gui2::event::LONGPRESS>(
			std::bind(
				&tmkcourse::longpress_widget, this,
				_4, _5, std::ref(row)), gui2::event::tdispatcher::back_child);

		row.connect_signal<gui2::event::LONGPRESS>(
			std::bind(
				&tmkcourse::longpress_widget, this,
				_4, _5, std::ref(row)), gui2::event::tdispatcher::back_post_child);

		find_widget<tpanel>(&row, "bg_panel", false, true)->set_border("label12_f2");
*/
		line_at ++;
	}

	if (sel_at != nposm) {
		list.select_row(sel_at);
	}
}

void taction_lib2::did_action_list_row_changed(tlistbox& list, ttoggle_panel& row)
{
	int action_at = uint64_2_int(row.cookie());

	std::map<std::string, aplt::taction_tpl2>::const_iterator sel_it = aplt::action_tpl2s.begin();
	if (action_at != 0) {
		std::advance(sel_it, action_at);
	}
	curr_sel_id_ = sel_it->second.id;

	ok_widget_->set_active(is_ok_active());
}

bool taction_lib2::is_ok_active() const
{
	if (scene_ == scene_paste) {
		return curr_sel_id_ != caller_id_;
	}
	return true;
}

void taction_lib2::click_ok(tbutton& widget)
{
	VALIDATE(is_ok_active(), null_str);
	window_->set_retval(twindow::OK);
}

} // namespace gui2

