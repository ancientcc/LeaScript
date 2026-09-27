#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/mkcourse.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gettext.hpp"
#include "aplt.hpp"
#include "wkoscript.hpp"
#include "font.hpp"
#include "game_config.hpp"

using namespace std::placeholders;

extern std::string handle_browse_file2(bool wkoscript, bool read_only, const std::string& title, bool set_pref);
extern bool is_valid_wkoscript_or_wkocoruse_path(bool wkoscript, const std::string& path);

namespace gui2 {

REGISTER_DIALOG(launcher, mkcourse)

tmkcourse::tclear_drag_candidate_lock::~tclear_drag_candidate_lock()
{
	cursor::set(cursor::NORMAL);
}

tmkcourse::tmkcourse(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, std::map<aplt::taplt_key, aplt::tapplet>& applets)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, applets_(applets)
	, file_ops_({
		{file_new_from_benchmark, _("file^From benchmark...")},
		{file_new_empty, _("file^Empty script")},
		{file_open, _("file^Open")},
		{file_save_as, _("file^Save as")},
		{file_exit, _("file^Exit")},
	})
	, edit_widget_ids_({
		{edittype_course_id, "course_id", aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_id)},
		{edittype_course_title, "course_title", aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_title)}, 
		{edittype_course_author, "course_author", aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_author)},
		{edittype_course_desc, "course_desc", aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_description)},
		{edittype_course_reference, "course_reference", aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_reference)},
	})
	, btn_widget_ids_({
		{btntype_total_days, "total_days", aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_total_days)},
		{btntype_grace_period_days, "grace_period_days", aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_grace_period_days)},
	})
	, day_title_msgstr_(aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_day, aplt::twkocourse::fid_title))
	, workout_note_msgstr_(aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_workout, aplt::twkocourse::fid_note))
	, max_show_days_non_maximized_(45)
	, target_aplt_(nullptr)
	, title_widget_(nullptr)
	, save_widget_(nullptr)
	, candidate_workout_list_(nullptr)
	, calendar_(nullptr)
	, day_title_widget_(nullptr)
	, day_workout_list_(nullptr)
	, status_widget_(nullptr)
	, day_at_(nposm)
{
	set_timer_interval(500);
}

void tmkcourse::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	label->set_label(aplt::all_fake_applets.find(aplt::builtinid_mkcourse)->second.name);
	title_widget_ = label;

	status_widget_ = find_widget<tlabel>(window_, "status", false, true);

	init_wkocourse();

	tbutton* button = gui2::find_widget<gui2::tbutton>(window_, "file", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tmkcourse::click_file
			, this
			, std::ref(*button)));

	button = find_widget<tbutton>(window_, "save", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmkcourse::click_save
			, this));
	save_widget_ = button;

	button = find_widget<tbutton>(window_, "share", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmkcourse::click_share
			, this));

	for (std::vector<tcode3>::const_iterator it = edit_widget_ids_.begin(); it != edit_widget_ids_.end(); ++ it) {
		const tcode3& type = *it;
		button = find_widget<tbutton>(window_, type.id, false, true);
		connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmkcourse::handle_edit_text
			, this, type.code, nposm));
		button->set_icon("misc/edit.png");
		update_edittype_course_ui(type.code);
	}

	for (std::vector<tcode3>::const_iterator it = btn_widget_ids_.begin(); it != btn_widget_ids_.end(); ++ it) {
		const tcode3& type = *it;
		button = find_widget<tbutton>(window_, type.id, false, true);
		connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmkcourse::click_btn_type
			, this, type.code, std::ref(*button)));
		button->set_icon("misc/edit.png");
		update_btntype_course_ui(type.code);
	}


	button = find_widget<tbutton>(window_, "target_aplt", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmkcourse::click_target_aplt
			, this, std::ref(*button)));
	button->set_label(target_aplt_->name2());

	tlistbox* list = find_widget<tlistbox>(window_, "candidate_workout_list", false, true);
	list->enable_select(false);
	// list->set_did_row_pre_change(std::bind(&tlatex_editor::did_tpl_list_row_pre_change, this, _1, _2));
	list->set_did_row_changed(std::bind(&tmkcourse::did_candidate_workout_list_row_changed, this, _1, _2));
	candidate_workout_list_ = list;

	treport* report = find_widget<treport>(window_, "calendar", false, true);
	report->set_did_item_changed(std::bind(&tmkcourse::did_calendar_changed, this, _1, _2));
	calendar_ = report;

	button = find_widget<tbutton>(window_, "day_title", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
		&tmkcourse::handle_edit_text
		, this, edittype_day_title, nposm));
	// button->set_icon("misc/edit.png");
	day_title_widget_ = button;

	list = find_widget<tlistbox>(window_, "day_workout_list", false, true);
	list->enable_select(false);
	// list->set_did_row_pre_change(std::bind(&tlatex_editor::did_tpl_list_row_pre_change, this, _1, _2));
	list->set_did_row_changed(std::bind(&tmkcourse::did_day_workout_list_row_changed, this, _1, _2));
	day_workout_list_ = list;

	reload_candidate_workout_list(*candidate_workout_list_);

	reload_calendar_report(0);

	refresh_toolbar_ui();
}

void tmkcourse::post_show()
{
}

void tmkcourse::app_first_drawn()
{
	SDL_Rect list_rect = day_workout_list_->get_rect();
	/*
		* width >= n * unit_w + (n - 1) * gap_w;
		* width >= n * unit_w + n * gap_w - gap_w;
		* width >= n * (unit_w + gap_w) - gap_w;
		* n < width + gap_w / (unit_w + gap_w);
	*/
	int gap = calendar_->get_gap();
	int fixed_cols = (list_rect.w + gap) / (calendar_->get_unit_width() + gap);

	calendar_->set_fixed_cols(fixed_cols);
	// calendar_->invalidate_layout(nullptr);
	window_->invalidate_layout(nullptr);

	// calendar_->select_item(0);

	if (preferences::maximized() && calendar_->items() == max_show_days_non_maximized_) {
		reload_calendar_report(day_at_);

	} else {
		VALIDATE((int)calendar_->items() <= max_show_days_non_maximized_, null_str);
	}
}

void tmkcourse::app_resize_screen()
{
	window_->set_undraw();
	// Set calendar_'s best_size to 0(fixed_cols = 0). 
	// This prevents get_best_size() in twindow::layout() from using the previous fixed_cols_ for calculation, 
	// which would cause it to throw tlayout_exception(*this).
	calendar_->set_fixed_cols(0);

	if (calendar_->items() > max_show_days_non_maximized_) {
		reload_calendar_report(day_at_);
	}
}

std::string tmkcourse::target_wkocourse_dir() const
{
	VALIDATE(target_aplt_ != nullptr, null_str);
	return target_aplt_->preferences_dir + "/wkocourse";
}

std::string tmkcourse::candidate_wkoscript_dir() const
{
	VALIDATE(target_aplt_ != nullptr, null_str);
	return target_aplt_->res_path + "/wkoscript";
}

void tmkcourse::click_target_aplt(tbutton& widget)
{

}

void tmkcourse::init_wkocourse()
{
	VALIDATE(target_aplt_ == nullptr, null_str);

	aplt::tapplet* aplt = aplt::mutable_aplt_from_bundleid(applets_, aplt::get_bundleid(aplt::bundleid_leagor_khomelua));
	VALIDATE(aplt != nullptr, null_str);

	target_aplt_ = aplt;

	wkocourse_dir_ = target_aplt_->preferences_dir + "/wkocourse";
	std::string filename = preferences::last_wkocourse_file();

	std::string must_ext_name = "cfg";
	if (!filename.empty() && utils::file_ext_name(filename) == must_ext_name) {
		aplt::twkocourse course;
		course.from_file(filename);

		std::string err_msg;
		if (course.is_valid2(err_msg, nullptr) == TCOOKIE3F_CHECK_OK) {
			draw_flowchart_from_course(true, filename, course);
		}
	}
	if (!course_.valid()) {
/*
		int w = min_map_size_.w;
		int h = min_map_size_.h;

		if (w != map_.w() || h != map_.h()) {
			const int right_padding_cells = 0; // 2
			reload_map(w + right_padding_cells, h);
		}
*/
		new_empty_flowchart(true);
	}
}

void tmkcourse::update_edittype_course_ui(int type)
{
	std::string widget_id;
	std::string name;
	std::string value;
	int max_chars = 4;
	if (type == edittype_course_id) {
		widget_id = "course_id";
		name = _("ID");
		value = utils::truncate_to_max_chars2(tmp_course_.id, max_chars + 4, true);

	} else if (type == edittype_course_title) {
		widget_id = "course_title";
		name = _("Title");
		value = utils::truncate_to_max_chars2(tmp_course_.title, max_chars, true);

	} else if (type == edittype_course_author) {
		widget_id = "course_author";
		name = _("Author");
		value = utils::truncate_to_max_chars2(tmp_course_.author, max_chars, true);

	} else if (type == edittype_course_desc) {
		widget_id = "course_desc";
		name = _("wkocourse^Description");
		value = utils::truncate_to_max_chars2(tmp_course_.description, max_chars, true);

	} else {
		VALIDATE(type == edittype_course_reference, null_str);
		widget_id = "course_reference";
		name = _("Reference");
		value = utils::truncate_to_max_chars2(tmp_course_.reference, max_chars, true);
	}

	utils::string_map symbols;
	symbols["name"] = ht::generate_format(name, 0xff808080);
	symbols["value"] = value;
	std::string label = vgettext2("$name: $value", symbols);
	find_widget<tbutton>(window_, widget_id, false, true)->set_label(label);
}

void tmkcourse::update_btntype_course_ui(int type)
{
	std::string widget_id;
	std::string name;
	std::string value;
	if (type == btntype_total_days) {
		widget_id = "total_days";
		name = aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_total_days);
		value = str_cast(tmp_course_.total_days);

	} else {
		VALIDATE(type == btntype_grace_period_days, null_str);
		widget_id = "grace_period_days";
		name = aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_grace_period_days);
		value = str_cast(tmp_course_.grace_period_days);
	}

	utils::string_map symbols;
	symbols["name"] = ht::generate_format(name, 0xff808080);
	symbols["value"] = value;
	std::string label = vgettext2("$name: $value", symbols);
	find_widget<tbutton>(window_, widget_id, false, true)->set_label(label);
}

void tmkcourse::reload_candidate_workout_list(tlistbox& list)
{
	list.clear();

	VALIDATE(target_aplt_ != nullptr, null_str);
	candidate_scripts_.clear();

	std::set<std::string> cfgfiles;
	const std::string wkoscript_dir = candidate_wkoscript_dir();
	aplt::list_wkoscript_files_by_type(wkoscript_dir, aplt::type_wkoscript_cfgfiles, cfgfiles);

	std::map<std::string, std::string> data;
	for (std::set<std::string>::const_iterator it = cfgfiles.begin(); it != cfgfiles.end(); ++ it) {
		const std::string& file = *it;

		candidate_scripts_.push_back(aplt::twkoscript());
		aplt::twkoscript& script = candidate_scripts_.back();

		script.from_file(wkoscript_dir + "/" + file);
		VALIDATE(script.valid(), null_str);

		data["title"] = script.title;
		data["desc"] = file;

		ttoggle_panel& row = list.insert_row(data);
		row.connect_signal<gui2::event::LONGPRESS>(
			std::bind(
				&tmkcourse::longpress_widget, this,
				_4, _5, std::ref(row)), gui2::event::tdispatcher::back_child);

		row.connect_signal<gui2::event::LONGPRESS>(
			std::bind(
				&tmkcourse::longpress_widget, this,
				_4, _5, std::ref(row)), gui2::event::tdispatcher::back_post_child);

		find_widget<tpanel>(&row, "bg_panel", false, true)->set_border("label12_f2");
		// find_widget<tpanel>(&row, "bg_panel", false, true)->set_border("blue_ellipse");
	}
}

void tmkcourse::did_candidate_workout_list_row_changed(tlistbox& list, ttoggle_panel& row)
{
/*
	int at = row.at();
	const tformula& formula = formulas_[at];

	pdf_surf_ = latex::doc_to_surf(formula.tex, 384);
	pdf_surf_widget_->immediate_draw();
*/
}

void tmkcourse::longpress_widget(bool& halt, const tpoint& coordinate, ttoggle_panel& row)
{
	halt = true;

	const aplt::twkoscript& script = candidate_scripts_[row.at()];

	surface surf = font::get_rendered_text(script.title, INT_MAX, font::SIZE_DEFAULT, font::BLACK_COLOR);

	twindow& window = *window_;
	window.set_drag_surface(surf, false);

	// longpress_shape_ = &shape;

	tpoint new_coordinate(coordinate.x - surf->w / 2, coordinate.y - surf->h / 2);

	SDL_Point custom_xy_formula{- surf->w / 2, - surf->h / 2};
	// longpress_custom_xy_formula_ = custom_xy_formula;
	window.start_drag(&custom_xy_formula, coordinate, std::bind(&tmkcourse::did_drag_mouse_motion, this, _1, _2, std::ref(window), row.at()),
		std::bind(&tmkcourse::did_drag_mouse_leave, this, _1, _2, _3, row.at()));
}

bool tmkcourse::did_drag_mouse_motion(const int x, const int y, gui2::twindow& window, int row_at)
{
	SDL_Rect list_rect = day_workout_list_->get_rect();
	const bool allow = point_in_rect(x, y, list_rect);

	cursor::set(allow? cursor::NORMAL: cursor::ILLEGAL_DRAG);

	return true;
}

void tmkcourse::did_drag_mouse_leave(const int x, const int y, bool up_result, int row_at)
{
	tclear_drag_candidate_lock lock(*this);

	if (up_result) {
		SDL_Rect list_rect = day_workout_list_->get_rect();
		const bool allow = point_in_rect(x, y, list_rect);

		const aplt::twkoscript& script = candidate_scripts_[row_at];
		const std::string candidate_id2 = utils::join_app_prefix_id(target_aplt_->bundleid, script.id);
		if (allow) {
			aplt::twkocourse::tday& day = tmp_course_.days[day_at_];

			bool found = false;
			bool dirty = false;
			for (std::vector<aplt::twkocourse::tworkout>::iterator it = day.workouts.begin(); it != day.workouts.end(); ++ it) {
				aplt::twkocourse::tworkout& workout = *it;
				if (workout.get_id2(target_aplt_->bundleid) != candidate_id2) {
					continue;
				}
				found = true;
				if (workout.rounds >= WKO_MAX_ROUNDS_PER_WORKOUT) {
					continue;
				}
				workout.rounds ++;

				dirty = true;
			}
			if (!found) {
				day.workouts.push_back(aplt::twkocourse::tworkout());
				aplt::twkocourse::tworkout& workout = day.workouts.back();
				workout.id = script.id;
				workout.rounds = 1;

				dirty = true;
			}

			if (dirty) {
				update_calendar_day(day_at_);
				reload_day_workout_list(*day_workout_list_);

				refresh_toolbar_ui();
			}
		}
	}
}

void tmkcourse::reload_calendar_report(int sel_at)
{
	VALIDATE(sel_at >= 0 && sel_at < tmp_course_.total_days, null_str);
	aplt::twkocourse& course = tmp_course_;
	VALIDATE(course.total_days == (int)course.days.size(), null_str);

	treport& report = *calendar_;
	report.clear();

	const tpoint& unit_size = report.get_unit_size();
	surface bg_surf = create_neutral_surface(unit_size.x, unit_size.y);
	std::stringstream ss;

	int show_days = course.total_days;
	if (show_days > max_show_days_non_maximized_) {
		if (!preferences::maximized()) {
			show_days = max_show_days_non_maximized_;
		}
	}
	if (sel_at >= show_days) {
		sel_at = show_days - 1;
	}

	for (int day_at = 0; day_at < show_days; day_at ++) {
		aplt::twkocourse::tday& day = course.days[day_at];
		ss.str("");
		for (int workout_at = 0; workout_at < (int)day.workouts.size(); workout_at ++) {
			aplt::twkocourse::tworkout& workout = day.workouts[workout_at];
			if (workout_at != 0) {
				ss << " + ";
			}
			ss << workout.rounds;
		}
		tcontrol& item = report.insert_item(null_str, ss.str());
		item.set_best_size_1th(bg_surf->w, true, nposm, item.get_height_is_max());
		item.set_text_font_size(font::SIZE_SMALL);
		item.set_border("label12_f2");

		fill_surface(bg_surf, 0x0);
		blit_integer_surface(day_at + 1, bg_surf, 0, 0);


		// surface day_surf = font::get_rendered_text(str_cast(day_at + 1), INT_MAX, font::SIZE_DEFAULT, font::BLACK_COLOR);
		// blit_integer_blits(
		// SDL_Rect dst_rect{0, 0, day_surf->w, day_surf->h};
		// sdl_blit(day_surf, nullptr, bg_surf, &dst_rect);
		// surface extra_surf = font::get_rendered_text("1111", INT_MAX, font::SIZE_DEFAULT, font::NORMAL_COLOR);
		item.set_blits(gui2::tformula_blit(clone_surface(bg_surf) , null_str, null_str, "(width)", "(height)"));
	}
	report.select_item(sel_at);
}

void tmkcourse::update_calendar_day(int day_at)
{
	VALIDATE(day_at >= 0 && day_at < calendar_->items(), null_str);
	treport& report = *calendar_;

	const tpoint& unit_size = report.get_unit_size();
	surface bg_surf = create_neutral_surface(unit_size.x, unit_size.y);
	std::stringstream ss;

	aplt::twkocourse::tday& day = tmp_course_.days[day_at];
	tcontrol& item = report.item(day_at);
	ss.str("");
	for (int workout_at = 0; workout_at < (int)day.workouts.size(); workout_at ++) {
		aplt::twkocourse::tworkout& workout = day.workouts[workout_at];
		if (workout_at != 0) {
			ss << "+";
		}
		ss << workout.rounds;
	}
	item.set_label(ss.str());
	// item.set_best_size_1th(bg_surf->w, true, nposm, item.get_height_is_max());
	// item.set_text_font_size(font::SIZE_SMALL);
	// item.set_border("label12_f2");

	fill_surface(bg_surf, 0x0);
	blit_integer_surface(day_at + 1, bg_surf, 0, 0);
}

void tmkcourse::did_calendar_changed(treport& report, ttoggle_button& row)
{
	day_at_ = row.at();
	reload_day_workout_list(*day_workout_list_);
}

std::string get_rounds_msgstr(int rounds)
{
	utils::string_map symbols;
	symbols["count"] = str_cast(rounds);
	return vgettext2("$count rounds", symbols);
}

void tmkcourse::reload_day_workout_list(tlistbox& list)
{
	list.clear();

	VALIDATE(day_at_ >= 0 && day_at_ < tmp_course_.total_days, null_str);

	const aplt::twkocourse::tday& day = tmp_course_.days[day_at_];

	update_day_title_label(day.title);

	utils::string_map symbols;
	aplt::twkoscript script;
	std::map<std::string, std::string> data;
	for (std::vector<aplt::twkocourse::tworkout>::const_iterator it = day.workouts.begin(); it != day.workouts.end(); ++ it) {
		const aplt::twkocourse::tworkout& workout = *it;

		const aplt::tapplet* aplt = aplt::aplt_from_bundleid(applets_, workout.aplt(target_aplt_->bundleid));
		script.from_aplt_file(*aplt, workout.id + ".cfg");
		VALIDATE(script.valid(), null_str);

		data["name"] = script.title;
		data["note"] = workout.note;
		data["rounds"] = get_rounds_msgstr(workout.rounds);

		ttoggle_panel& row = list.insert_row(data);
		find_widget<tpanel>(&row, "bg_panel", false, true)->set_border("label12_f2"); // blue_ellipse
		// find_widget<tpanel>(&row, "bg_panel", false, true)->set_border("blue_ellipse");

		tbutton* button = find_widget<tbutton>(&row, "note", false, true);
		button->set_icon("misc/edit.png");
		connect_signal_mouse_left_click(
				  *button
				, std::bind(
				&tmkcourse::handle_edit_text
					, this, edittype_workout_note, row.at()));

		button = find_widget<tbutton>(&row, "rounds", false, true);
		button->set_icon("misc/edit.png");
		connect_signal_mouse_left_click(
				  *button
				, std::bind(
				&tmkcourse::click_rounds
				, this, std::ref(row), std::ref(*button)));

		button = find_widget<tbutton>(&row, "down", false, true);
		// if (day.workouts.size() > 1) {
			connect_signal_mouse_left_click(
					  *button
					, std::bind(
					&tmkcourse::click_down_day_workout
					, this, std::ref(list), std::ref(row)));
/*
		} else {
			button->set_visible(twidget::INVISIBLE);
		}
*/
		button->set_active(day.workouts.size() > 1);

		button = find_widget<tbutton>(&row, "erase", false, true);
		connect_signal_mouse_left_click(
				  *button
				, std::bind(
				&tmkcourse::click_erase_day_workout
				, this, std::ref(list), std::ref(row)));
	}
}

void tmkcourse::did_day_workout_list_row_changed(tlistbox& list, ttoggle_panel& row)
{
}

void tmkcourse::click_rounds(ttoggle_panel& row, tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
    
	aplt::twkocourse::tday& day = tmp_course_.days[day_at_];
	aplt::twkocourse::tworkout& workout = day.workouts[row.at()];
	for (int at = WKO_MIN_ROUNDS_PER_WORKOUT; at <= WKO_MAX_ROUNDS_PER_WORKOUT; at ++) {
		items.push_back(gui2::tmenu::titem(str_cast(at), at));
		if (at == workout.rounds) {
			initial_sel = at;
		}
	}

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();
	workout.rounds = cursel;

	row.set_child_label("rounds", get_rounds_msgstr(workout.rounds));

	update_calendar_day(day_at_);
	refresh_toolbar_ui();
}

void tmkcourse::click_down_day_workout(tlistbox& list, ttoggle_panel& row)
{
	aplt::twkocourse::tday& day = tmp_course_.days[day_at_];
	aplt::twkocourse::tworkout& workout = day.workouts[row.at()];

	int s1 = row.at();
	int s2 = (s1 + 1) % day.workouts.size();

	std::iter_swap(day.workouts.begin() + s1, day.workouts.begin() + s2);

	reload_day_workout_list(*day_workout_list_);

	update_calendar_day(day_at_);
	refresh_toolbar_ui();
}

void tmkcourse::click_erase_day_workout(tlistbox& list, ttoggle_panel& row)
{
	aplt::twkocourse::tday& day = tmp_course_.days[day_at_];
	aplt::twkocourse::tworkout& workout = day.workouts[row.at()];

	std::string title = workout.id;
	const std::string msg = i18n::freq_msgstr_2str(i18n::msgid_confirm_delete_2str, _("Workout"), title);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	int row_at = row.at();
	std::vector<aplt::twkocourse::tworkout>::iterator erase_it = day.workouts.begin();
	if (row_at > 0) {
		std::advance(erase_it, row_at);
	}
	day.workouts.erase(erase_it);

	list.erase_row(row.at());

	reload_day_workout_list(*day_workout_list_);

	update_calendar_day(day_at_);
	refresh_toolbar_ui();
}

void tmkcourse::update_title_label()
{
	std::stringstream ss;
	ss << utils::extract_file(filename_);
	ss << " - ";
	ss << aplt::all_fake_applets.find(aplt::builtinid_mkcourse)->second.name;
	title_widget_->set_label(ss.str());
}

void tmkcourse::update_day_title_label(const std::string& msg)
{
	utils::string_map symbols;
	symbols["name"] = ht::generate_format(day_title_msgstr_, 0xff808080);
	symbols["value"] = msg;
	std::string label = vgettext2("$name: $value", symbols);
	day_title_widget_->set_label(label);
}

void tmkcourse::update_status_label(bool valid)
{
	std::string msg;
	if (valid) {
		msg.append(_("Working directory"));
		msg.append(": ");
		msg.append(os_normalize_path(wkocourse_dir_));

	} else {
		utils::string_map symbols;
		// symbols["add_to_working_dir"] = add_to_working_dir_msgstr_;
		symbols["add_to_working_dir"] = "add_to_working_dir_msgstr_";
		msg = vgettext2("invalid wkoscript_dir remark, $add_to_working_dir", symbols);
	}
	status_widget_->set_label(msg);
}

bool tmkcourse::wkocourse_dirty() const
{
	return !tmp_course_.equal(course_);
}

void tmkcourse::refresh_toolbar_ui()
{
	bool dirty = wkocourse_dirty();
	save_widget_->set_active(dirty);
}

void tmkcourse::draw_flowchart_from_course(bool is_initial, const std::string& filename, const aplt::twkocourse& course)
{
	VALIDATE(!filename.empty(), null_str);
	std::string err_msg;
	// When generating a script file from a benchmark image, the given image may not have detectable 33landmarks. 
	// In this case, 'pose.range' can only be '{nposm, nposm}'.
	VALIDATE(course.is_valid2(err_msg, nullptr) == TCOOKIE3F_CHECK_OK, null_str);

	filename_ = filename;
	course_.assign(course);

	// script_.set_lmk33_png_at_equal_to_state_at();
	// next_lmk33_png_at_ = script_.states.size();
	tmp_course_.assign(course_);

	VALIDATE(tmp_course_.equal(course_), null_str);

	//
	// draw flowchart
	//
	if (!is_initial) {
		draw_flowchart();
	}

	update_title_label();

	const std::string path = utils::extract_directory(filename);

	bool valid = is_valid_wkoscript_or_wkocoruse_path(false, path);
	if (valid) {
		wkocourse_dir_ = path;
		preferences::set_wkocourse_dir(path);
	}
	update_status_label(valid);
}

void tmkcourse::new_empty_flowchart(bool is_initial)
{
/*
	select_object(nullptr);

	clear_draw_items();
*/
	filename_.clear();

	course_clear_and_set_valid_id();
	tmp_course_.assign(course_);

	if (!is_initial) {
		draw_flowchart();
	}
/*
	draw_fix_text_shapes();
*/
	update_title_label();
}

void tmkcourse::course_clear_and_set_valid_id()
{
	// Why set the id even when 'filename_' is empty? heare 'script_' is 'empty'.
	// --The user might upload 'lmk33.png', and those files need a storable directory with a fixed location.  
	// This has a side effect: if the user edits for a while but does not save, that directory becomes an orphan directory.
	VALIDATE(filename_.empty(), null_str);

	course_.clear();

	std::set<std::string> existed;
	aplt::list_wkocourse_files_by_type(wkocourse_dir_, aplt::type_wkocourse_ids, existed);
	course_.id = utils::unique_untitle_id(existed, "course", null_str, 1);

	course_.total_days = WKO_DFLT_TOTAL_DAYS;
	course_.grace_period_days = WKO_DFLT_GRACE_PERIOD_DAYS;
	course_.resize_days_by_total_days();

	// SDL_DeleteFiles(script_.build_phase_surf_dir(wkoscript_dir_).c_str());
}

void tmkcourse::draw_flowchart()
{
	for (std::vector<tcode3>::const_iterator it = edit_widget_ids_.begin(); it != edit_widget_ids_.end(); ++ it) {
		const tcode3& type = *it;
		update_edittype_course_ui(type.code);
	}

	for (std::vector<tcode3>::const_iterator it = btn_widget_ids_.begin(); it != btn_widget_ids_.end(); ++ it) {
		const tcode3& type = *it;
		update_btntype_course_ui(type.code);
	}

	reload_calendar_report(0);
}

void tmkcourse::click_file(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
    
	const std::map<int, std::string>& ops = file_ops_;

	std::vector<gui2::tmenu::titem> new_items;
	std::set<int> new_codes{file_new_empty, file_new_from_benchmark};
	for (std::set<int>::const_iterator it = new_codes.begin(); it != new_codes.end(); ++ it) {
		int code = *it;
		if (code == file_new_from_benchmark) {
			continue;
		}
		const std::string& name = ops.find(code)->second;
		new_items.push_back(gui2::tmenu::titem(name, code));
	}
	items.push_back(gui2::tmenu::titem(_("file^New"), new_items));
	for (std::map<int, std::string>::const_iterator it = ops.begin(); it != ops.end(); ++ it) {
		int code = it->first;
		if (new_codes.count(code) != 0) {
			continue;
		}
		const std::string& name = it->second;
		items.push_back(gui2::tmenu::titem(name, code));
		if (code == file_exit - 1) {
			items.back().separator = true;
		}
	}

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();
	handle_file_menu(cursel);
}

// true: ok, user don't cancel.
// fasle: user cancel it.
bool tmkcourse::confirm_file_op(int sel)
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
	bool dirty = wkocourse_dirty();
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

void tmkcourse::open_cfg_file_bh(const std::string& filename)
{
	aplt::twkocourse course;
	course.from_file(filename, false);
	std::string err_msg;
	if (course.is_valid2(err_msg, nullptr) != TCOOKIE3F_CHECK_OK || !course.is_id_same_filename(filename)) {
		std::string reason;
		if (course.valid()) {
			reason = _("The filename and ID are different.");
		}
		const std::string msg = i18n::freq_msgstr_3str(i18n::msgid_load_file_fail, _("Course script"), filename, reason);
		gui2::show_message(null_str, msg);
		return;
	}
	if (!confirm_file_op(file_open)) {
		return;
	}
	preferences::set_last_wkocourse_file(filename);

	// select_object(nullptr);
	draw_flowchart_from_course(false, filename, course);

	// gui_->show_context_menu();
	refresh_toolbar_ui();
}

void tmkcourse::handle_file_op(int sel)
{

	if (sel == file_new_empty) {
		if (!confirm_file_op(sel)) {
			return;
		}

		new_empty_flowchart(false);
		// don't update last_wkocourse_file, it will change browse wkocourse directory.
		// preferences::set_last_wkocourse_file(null_str);

		// gui_->show_context_menu();
		refresh_toolbar_ui();

	} else if (sel == file_new_from_benchmark) {
		if (!confirm_file_op(sel)) {
			return;
		}
/*
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

		preferences::set_last_wkocourse_file(filename_);
		gui_->redraw_minimap();

		std::string err_msg;
		VALIDATE(tmp_script_.is_valid2(err_msg, nullptr) == TCOOKIE3F_CHECK_OK, null_str);
		if (tmp_script_.is_valid2(err_msg, nullptr) != TCOOKIE3F_CHECK_OK) {
			script_.clear();
			script_.id = tmp_script_.id;
		}
		gui_->show_context_menu();
*/
	} else if (sel == file_open) {
		const std::string filename = handle_browse_file2(false, true, null_str, false);
		if (filename.empty()) {
			return;
		}

		open_cfg_file_bh(filename);

	} else if (sel == file_save_as) {
		if (wkocourse_dirty() && !handle_pre_save()) {
			return;
		}
		std::string filename = handle_browse_file2(false, false, null_str, true);
		if (filename.empty()) {
			return;
		}
		tmp_course_.to_file(filename);

	} else if (sel == file_exit) {
		if (!confirm_file_op(sel)) {
			return;
		}
		window_->set_retval(twindow::OK);
	}
}

void tmkcourse::handle_file_menu(int sel)
{
	if (sel == file_new_empty || sel == file_new_from_benchmark || sel == file_open || sel == file_save_as || sel == file_exit) {
		handle_file_op(sel);

	} else {
		VALIDATE(false, null_str);
	}
}

// true: no error, can save.
// false: has error, cannot save.
bool tmkcourse::handle_pre_save()
{
	VALIDATE(wkocourse_dirty(), null_str);

	const aplt::twkocourse& script = tmp_course_;

	// tdraw_item_C* err_item = nullptr;
	std::string err_msg;

	const aplt::twkocourse::tday* err_day = nullptr;
	uint64_t res = script.is_valid2(err_msg, &err_day);
	if (res != TCOOKIE3F_CHECK_OK) {
		err_msg = aplt::twkocourse::fomrat_is_valid2_result(res, err_msg);

	} else {
		VALIDATE(err_msg.empty(), null_str);
	}

	if (!err_msg.empty()) {
		std::stringstream err;
		err << err_msg;

		err << "\n\n";
		err << _("There is a data error, and cannot save.");
		gui2::show_message(null_str, err.str());

		return false;
	}
	return true;
}

void tmkcourse::click_save()
{
	bool no_error = handle_pre_save();
	if (!no_error) {
		return;
	}

	bool filename_is_changed = false;
	if (filename_.empty() || tmp_course_.id != course_.id) {
		VALIDATE(!course_.id.empty(), null_str);
		filename_is_changed = true;

		// filename
		const std::string desire_filename = tmp_course_.build_course_filename(wkocourse_dir_);
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
			VALIDATE(filename_ == course_.build_course_filename(wkocourse_dir_), null_str);

			// 2/2: rename old to new.
			VALIDATE(tmp_course_.id != course_.id, null_str);
			// const std::string src_file = script_.build_script_filename(wkoscript_dir_);
			// SDL_RenameFile(src_file.c_str(), desire_short_file.c_str());
			SDL_RenameFile(filename_.c_str(), desire_short_file.c_str());
		}
/*
		// 
		if (tmp_course_.id != course_.id) {
			const std::string desire_dir = tmp_course_.build_phase_surf_dir(wkocourse_dir_);
			SDL_DeleteFiles(desire_dir.c_str());

			const std::string src_dir = course_.build_phase_surf_dir(wkoscript_dir_);
			SDL_RenameFile(src_dir.c_str(), tmp_course_.id.c_str());
		}
*/
		filename_ = desire_filename;
	}
	VALIDATE(!filename_.empty(), null_str);

	// handle lmk33_<state>_[0|1].png
	// The phase_surf directory has been renamed to tmp_script_.id, so use tmp_script_.
/*
	apply_rename_lmk33_png(tmp_script_, wkoscript_dir_, next_lmk33_png_at_);

	tmp_script_.set_lmk33_png_at_equal_to_state_at();
	next_lmk33_png_at_ = tmp_script_.states.size();
*/
	course_.assign(tmp_course_);
	course_.to_file(filename_);
/*
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
*/
	if (filename_is_changed) {
		preferences::set_last_wkocourse_file(filename_);
		update_title_label();
	}

	refresh_toolbar_ui();
}

void increment_s(int this_w, int this_h, SDL_Size& s)
{
	if (this_w > s.w) {
		s.w = this_w;
	}
	s.h += this_h;
}

void draw_line_surf(bool with_save, const surface& line_surf, int indentation_w, SDL_Size& s, surface& bg_surf)
{
	VALIDATE(line_surf.get() != nullptr, null_str);
	VALIDATE(indentation_w >= 0, null_str);

	if (with_save) {
		SDL_Rect dst_rect = ::create_rect(indentation_w, s.h, line_surf->w, line_surf->h);
		sdl_blit(line_surf, nullptr, bg_surf, &dst_rect);
	}
	increment_s(indentation_w + line_surf->w, line_surf->h, s);
}

SDL_Size tmkcourse::do_save_image(bool todo, std::map<std::string, aplt::twkoscript>& scripts, surface* p_surf) const
{
	const aplt::twkocourse& course = tmp_course_;

	bool with_save = false;
	surface bg_surf;
	if (p_surf != nullptr) {
		bg_surf = *p_surf;
		with_save = true;
	}

	std::string err_msg;
	uint64_t res = tmp_course_.is_valid2(err_msg, nullptr);
	VALIDATE(res == TCOOKIE3F_CHECK_OK, null_str);

	const std::string aplt = "aplt.leagor.khomelua";
	std::map<std::string, int> id2s;
	// std::map<std::string, aplt::twkoscript> scripts;
	course.get_workout_id2s(aplt, id2s);
	if (scripts.empty()) {
		for (std::map<std::string, int>::const_iterator it = id2s.begin(); it != id2s.end(); ++ it) {
			const std::string& id2 = it->first;
			std::pair<std::string, std::string> pair = utils::split_app_prefix_id(id2);
			std::pair<std::map<std::string, aplt::twkoscript>::iterator, bool> ins = scripts.insert(std::make_pair(id2, aplt::twkoscript()));
			VALIDATE(ins.second, null_str);

			aplt::twkoscript& script = ins.first->second;
			const aplt::tapplet* aplt = aplt::aplt_from_bundleid(applets_, pair.first);
			VALIDATE(aplt != nullptr, null_str);
			script.from_aplt_file(*aplt, aplt::twkoscript::id_to_filename(pair.second));
		}
	} else {
		VALIDATE(scripts.size() == id2s.size(), null_str);
	}

	std::string line_str;
	SDL_Size s = {0};
	surface tmp_surf;
	utils::string_map symbols;
	SDL_Rect dst_rect;

	// surface split_line_surf = image::get_image("misc/split_line.png");
	// VALIDATE(split_line_surf.get() != nullptr, null_str);

	const int split_line_h = 1;
	int gap_y_line = 2;
	//
	// workouts
	//
	line_str = _("noun^Workout");
	if (todo) {
		std::string todo = _("todo^workout");
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


	for (std::map<std::string, aplt::twkoscript>::const_iterator it = scripts.begin(); it != scripts.end(); ++ it) {
		const aplt::twkoscript& script = it->second;

		// line1: title(id)
		line_str = script.title;
		line_str.append("(" + script.id + ")");
		tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_DEFAULT, font::BLACK_COLOR);
		draw_line_surf(with_save, tmp_surf, 0, s, bg_surf);

		const int gap_y_between_workouts = 4;
		const int offset_x_actions = 8;

		line_str.clear();
		// line2: action1(max_count) - action2(max_count)...
		for (std::map<int, aplt::twkoscript::tstate2>::const_iterator it2 = script.states.begin(); it2 != script.states.end(); ++ it2) {
			const aplt::twkoscript::tstate2& state = it2->second;
			if (state.track_pose.poses.empty() || state.is_setup) {
				continue;
			}
			if (!line_str.empty()) {
				line_str.append("  ");
			}
			VALIDATE(!state.action_tpl2_id.empty(), null_str);
			VALIDATE(aplt::action_tpl2s.count(state.action_tpl2_id) != 0, null_str);
			line_str.append(aplt::action_tpl2s.find(state.action_tpl2_id)->second.name);
			std::string count_str;
			if (state.task->type == aplt::twkoscript::tasktype_time_counter) {
				const aplt::twkoscript::ttime_counter* task2 = static_cast<aplt::twkoscript::ttime_counter*>(state.task);
				symbols["count"] = str_cast(task2->max_count);
				count_str = vgettext2("$count seconds", symbols);

			} else {
				VALIDATE(state.task->type == aplt::twkoscript::tasktype_rep_counter, null_str);
				const aplt::twkoscript::trep_counter* task2 = static_cast<aplt::twkoscript::trep_counter*>(state.task);
				symbols["count"] = str_cast(task2->max_count);
				count_str = vgettext2("$count reps", symbols);
			}
			line_str.append("(" + count_str + ")");
		}
		tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_SMALL, font::GRAY_COLOR);
		draw_line_surf(with_save, tmp_surf, offset_x_actions, s, bg_surf);

		s.h += gap_y_between_workouts;
	}

	if (todo) {
		line_str = _("todo^miss action");
		tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_SMALL, font::GOOD_COLOR);
		draw_line_surf(with_save, tmp_surf, 0, s, bg_surf);
	}

	//
	// course
	//
	int gap_y_2sections = 16;
	s.h += gap_y_2sections;

	line_str = _("Course");
	if (todo) {
		std::string todo = _("todo^course");
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

	// title(id)
	line_str = course.title;
	line_str.append("(" + course.id + ")");
	tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_DEFAULT, font::BLACK_COLOR);
	draw_line_surf(with_save, tmp_surf, 0, s, bg_surf);

	// description
	line_str = aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_description);
	line_str.append(": " + course.description);
	tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_SMALL, font::GRAY_COLOR);
	draw_line_surf(with_save, tmp_surf, 0, s, bg_surf);

	// total_days: <total_days>, grace_period_days: <grace_period_days>
	line_str = aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_total_days);
	line_str.append(": " + str_cast(course.total_days) + "    ");

	line_str.append(aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_grace_period_days));
	line_str.append(": " + str_cast(course.grace_period_days));
	tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_SMALL, font::GRAY_COLOR);
	draw_line_surf(with_save, tmp_surf, 0, s, bg_surf);

	// author: <author> reference: <reference>
	line_str = aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_author);
	line_str.append(": " + course.author + "    ");
	line_str.append(aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_reference));
	line_str.append(": " + course.reference);

	tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_SMALL, font::GRAY_COLOR);
	draw_line_surf(with_save, tmp_surf, 0, s, bg_surf);

	// price, currency

	const int gap_y_course_days = 8;
	s.h += gap_y_course_days;

	// days
	VALIDATE(course.total_days == (int)course.days.size(), null_str);
	for (int day_at = 0; day_at < course.total_days; day_at ++) {
		const aplt::twkocourse::tday& day = course.days[day_at];

		if (day_at != 0) {
			const int gap_y_between_days = 5;
			s.h += gap_y_between_days;
		}

		// Day<1> <title>
		line_str = "Day ";
		line_str.append(str_cast(day.day_at + 1));
		line_str.append(" " + ht::generate_format(day.title, 0xff808080, font::SIZE_SMALL));

		tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_DEFAULT, font::BLACK_COLOR);
		draw_line_surf(with_save, tmp_surf, 0, s, bg_surf);

		for (int workout_at = 0; workout_at < (int)day.workouts.size(); workout_at ++) {
			const aplt::twkocourse::tworkout& workout = day.workouts[workout_at];
			// (<rounds>)<title>  <note>
			const std::string id2 = workout.get_id2(aplt);
			VALIDATE(scripts.count(id2) != 0, null_str);

			line_str = "(";
			line_str.append(str_cast(workout.rounds) + ")" + scripts.find(id2)->second.title);

			if (!workout.note.empty()) {
				line_str.append("    ");
				line_str.append(workout.note);
			}

			tmp_surf = font::get_rendered_text(line_str, INT_MAX, font::SIZE_SMALL, font::GRAY_COLOR);
			draw_line_surf(with_save, tmp_surf, 0, s, bg_surf);
		}
	}


	return s;
}

void tmkcourse::click_share()
{
	if (wkocourse_dirty()) {
		bool no_error = handle_pre_save();
		if (!no_error) {
			return;
		}
	}
	const aplt::twkocourse& course = tmp_course_;
	bool todo = true;

	std::map<std::string, aplt::twkoscript> scripts;
	SDL_Size size = do_save_image(todo, scripts, nullptr);

	surface content_surf = create_neutral_surface(size.w, size.h);
	fill_surface(content_surf, 0xffffffff);

	SDL_Size size2 = do_save_image(todo, scripts, &content_surf);
	VALIDATE(size.w == size2.w && size.h == size2.h, null_str);

	std::string filename_prfix = "share_course-";
	const std::string filename = filename_prfix + course.title + "-" + utils::format_time_ymdhms2(time(nullptr)) + ".png";
	const std::string full_filename = game_config::preferences_dir + "/saves/" + filename;

	SDL_Size margin{12, 12};
	surface result = create_neutral_surface(size.w + margin.w * 2, size.h + margin.h * 2);
	fill_surface(result, 0xffffffff);
	SDL_Rect dst_rect{margin.w, margin.h, content_surf->w, content_surf->h};
	sdl_blit(content_surf, nullptr, result, &dst_rect);

	imwrite(result, full_filename);

	utils::string_map symbols;
	symbols["type"] = _("Task to-do items");
	symbols["file"] = full_filename;
	const std::string msg = vgettext2("$type file has been generated.\nPath: $file", symbols);
	gui2::show_message(null_str, msg);
}

void tmkcourse::list_wkocourse_files2(int type, std::set<std::string>& result_set)
{
	if (!filename_.empty()) {
		VALIDATE(wkocourse_dir_ == utils::extract_directory(filename_), null_str);
	}
	aplt::list_wkoscript_files_by_type(wkocourse_dir_, aplt::type_wkocourse_ids, result_set);
}

#define WKO_MAX_DAY_TITLE_OR_WORKOUT_NOTE_CHARS		32

bool tmkcourse::did_verify_text_changed(const std::string& label, const std::string& initial, int type, const std::set<std::string>& xcludes) const
{
	if (label == initial) {
		return false;
	}

	if (label.empty()) {
		return type == edittype_course_author || type == edittype_course_reference ||
			edittype_workout_note;
	}

	if (!xcludes.empty() && xcludes.count(label) != 0) {
		return false;
	}

	if (type == edittype_course_id) {
		return isvalid_normal_id_or_var_name224(label);

	} else if (type == edittype_course_title || type == edittype_course_author) {
		return isvalid_short_utf8_name216(label);

	} else if (type == edittype_course_desc) {
		const int max_course_desc_chars = 48;
		return utils::isvalid_utf8_name(label, MIN_NORMAL_UTF8_NAME_CHARS, max_course_desc_chars);

	} else if (type == edittype_course_reference) {
		return label.size() <= WKO_MAX_REFERENCE_BYTES;
	}

	VALIDATE(type == edittype_day_title || type == edittype_workout_note, null_str);
	return isvalid_normal_utf8_name224(label);
}

void tmkcourse::handle_edit_text(int type, int row_at)
{
	aplt::twkocourse& course = tmp_course_;
	aplt::twkocourse::tday& wkoday = course.days[day_at_];

	std::string* target = nullptr;

	std::set<std::string> xcludes;
	std::string type_name;
	int max_chars = MAX_NORMAL_UTF8_NAME_CHARS;
	std::string initial;
	std::string remark = i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);
	bool scroll = false;
	if (type == edittype_course_id) {
		type_name = "ID";
		max_chars = MAX_NORMAL_ID_OR_VAR_NAME_BYTES;
		remark = _("It will be used as the stem name of the script file.");
		remark.append(i18n::freq_msgstr(i18n::msgid_isvalid_normal_id_or_var_name));
		target = &course.id;

		list_wkocourse_files2(aplt::type_wkocourse_ids, xcludes);
		if (!filename_.empty()) {
			std::set<std::string>::iterator it = xcludes.find(course_.id);
			VALIDATE(it != xcludes.end(), null_str);
			xcludes.erase(course_.id);
		}

	} else if (type == edittype_course_title) {
		type_name = _("Title");
		max_chars = MAX_SHORT_UTF8_NAME_CHARS;
		target = &course.title;

	} else if (type == edittype_course_author) {
		type_name = _("Author");
		max_chars = MAX_SHORT_UTF8_NAME_CHARS;
		target = &course.author;

	} else if (type == edittype_course_desc) {
		type_name = aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_description);
		max_chars = WKO_MAX_REFERENCE_BYTES;
		target = &course.description;
		scroll = true;

	} else if (type == edittype_course_reference) {
		type_name = aplt::twkocourse::get_field_str(aplt::twkocourse::typeid_course, aplt::twkocourse::fid_reference);
		max_chars = WKO_MAX_REFERENCE_BYTES;
		target = &course.reference;
		scroll = true;

	} else if (type == edittype_day_title) {
		type_name = day_title_msgstr_;
		max_chars = WKO_MAX_REFERENCE_BYTES;
		target = &wkoday.title;
		scroll = true;

	} else if (type == edittype_workout_note) {
		type_name = workout_note_msgstr_;
		max_chars = MAX_SHORT_UTF8_NAME_CHARS;

		VALIDATE(!wkoday.workouts.empty(), null_str);
		VALIDATE(row_at >= 0 && row_at < (int)wkoday.workouts.size(), null_str);
		target = &wkoday.workouts[row_at].note;
		scroll = true;

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
		param.did_text_changed = std::bind(&tmkcourse::did_verify_text_changed, this, _1, 
			std::ref(initial), type, std::ref(xcludes));
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_->get_height() / 5);
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}

			if (type == edittype_course_id) {
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

	if (type == edittype_course_id || type == edittype_course_title || type == edittype_course_author || type == edittype_course_desc || type == edittype_course_reference) {
		update_edittype_course_ui(type);
		// find_widget<tlabel>(window_, "widget_id", false, true)->set_label(gui_label);
		
	} else if (type == edittype_day_title) {
		update_day_title_label(wkoday.title);
		
	} else if (type == edittype_workout_note) {
		ttoggle_panel& row_panel = day_workout_list_->row_panel(row_at);
		row_panel.set_child_label("note", wkoday.workouts[row_at].note);
	}

	refresh_toolbar_ui();
}

void tmkcourse::click_btn_type(int type, tbutton& widget)
{
	VALIDATE(type >= 0 && type < btntype_count, null_str);
	aplt::twkocourse& course = tmp_course_;

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	if (type == btntype_total_days) {
		for (int at = WKO_MIN_TOTAL_DAYS; at <= WKO_MAX_TOTAL_DAYS; at ++) {
			items.push_back(gui2::tmenu::titem(str_cast(at), at));
			if (at == tmp_course_.total_days) {
				initial_sel = at;
			}
		}

	} else if (type == btntype_grace_period_days) {
		for (int at = WKO_MIN_GRACE_PERIOD_DAYS; at <= WKO_MAX_GRACE_PERIOD_DAYS; at ++) {
			items.push_back(gui2::tmenu::titem(str_cast(at), at));
			if (at == tmp_course_.grace_period_days) {
				initial_sel = at;
			}
		}
	} else {
		VALIDATE(false, null_str);
	}

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel, nullptr, true);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();

	if (type == btntype_total_days) {
		course.total_days = cursel;
		course.resize_days_by_total_days();
		if (day_at_ >= course.total_days) {
			day_at_ = course.total_days - 1;
		}
		reload_calendar_report(day_at_);

	} else if (type == btntype_grace_period_days) {
		course.grace_period_days = cursel;
	}

	update_btntype_course_ui(type);
	refresh_toolbar_ui();
}

void tmkcourse::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

