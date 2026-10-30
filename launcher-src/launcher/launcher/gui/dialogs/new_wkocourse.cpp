#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/new_wkocourse.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "game_config.hpp"
#include "font.hpp"

using namespace std::placeholders;

extern void win_ShellExecuteW_open(const std::string& url);

namespace gui2 {

REGISTER_DIALOG(launcher, new_wkocourse)

tnew_wkocourse::tnew_wkocourse(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const std::string& csv_file,
		std::vector<aplt::twkoscript>& scripts, aplt::twkocourse& course, const aplt::tapplet& aplt)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, csv_file_(csv_file)
	, scripts_(scripts)
	, course_(course)
	, aplt_(aplt)
	, flags_{
		{flag_new, {flag_new, "misc/circle_green.png", _("csv^flag_new msgstr"), _("csv^flag_new method")}},
		{flag_same, {flag_same, "misc/circle_blue.png", _("csv^flag_same msgstr"), _("csv^flag_same method")}},
		{flag_vermismatch, {flag_vermismatch, "misc/circle_gray.png", _("csv^flag_vermismatch msgstr"), _("csv^flag_vermismatch method")}},
		{flag_diff, {flag_diff, "misc/circle_red.png", _("csv^flag_diff msgstr"), _("csv^flag_diff method")}},
	}
	, back_widget_(nullptr)
	// , legend_list_(nullptr)
	, file_list_(nullptr)
	// , script_flags_(scripts_.size(), nposm)
	, course_flag_(nposm)
{
	VALIDATE((int)flags_.size() == flag_count, null_str);
	set_timer_interval(1000);

	std::string err_msg;
	int script_count = scripts_.size();
	for (int at = 0; at < script_count; at ++) {
		const aplt::twkoscript& script = scripts_[at];
		VALIDATE(script.is_valid2(err_msg, nullptr) == TCOOKIE3F_CHECK_OK, null_str);
	}

	VALIDATE(course.is_valid2(err_msg, nullptr) == TCOOKIE3F_CHECK_OK, null_str);
}

void tnew_wkocourse::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	find_widget<tlabel>(window_, "title", false).set_label(_("wko^New course from csv"));

	tbutton* button = find_widget<tbutton>(window_, "back", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tnew_wkocourse::click_back
			, this
			, std::ref(*button)));
	button->set_icon("misc/back.png");
	back_widget_ = button;

	tlabel* label = find_widget<tlabel>(window_, "remark", false, true);
	utils::string_map symbols;
	// symbols["working_dir"] = _("Working directory");
	// label->set_label(vgettext2("generate_wkoscript_from_benchmarks remark, $working_dir", symbols));

	symbols["csv_file"] = csv_file_;
	symbols["working_dir"] = aplt_.preferences_dir;
	label->set_label(vgettext2("new_wkocourse remark, $csv_file, $working_dir", symbols));

	button = find_widget<tbutton>(window_, "working_dir", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tnew_wkocourse::click_working_dir
			, this
			, std::ref(*button)));

	// tlistbox* list = find_widget<tlistbox>(window_, "legend_list", false, true);
	// legend_list_ = list;

	tlistbox* list = find_widget<tlistbox>(window_, "file_list", false, true);
	list->enable_select(false);
	// list->set_did_row_pre_change(std::bind(&tcourseware2::did_file_list_row_pre_change, this, _1, _2));
	// list->set_did_row_changed(std::bind(&tcourseware2::did_file_list_row_changed, this, _1, _2));
	// list->set_did_can_drag(std::bind(&tcourseware2::did_file_list_can_drag, this, _1, _2));
	file_list_ = list;

	reload_legend_grid();

	reload_action_list(*file_list_);
	std::string status_msg;
	if (!can_next()) {
		back_widget_->set_visible(twidget::HIDDEN);
		symbols["new"] = _("file^From csv");
		status_msg = vgettext2("Files requiring manual handling have been detected. After resolving the issues, please select \"$new\" again.", symbols);

	} else {
		symbols["next"] = back_widget_->label();
		status_msg = vgettext2("Click \"$next\" to generate the script file associated with this course.", symbols);
	}
	find_widget<tlabel>(window_, "next_action", false, true)->set_label(status_msg);

	// find_widget<tlabel>(window_, "status", false, true)->set_label(status_msg);
}

void tnew_wkocourse::post_show()
{
}

void tnew_wkocourse::reload_legend_grid()
{
	char key_buf[256];
	VALIDATE(flags_.size() == 4, null_str);

	for (std::map<int, tflag4>::const_iterator it = flags_.begin(); it != flags_.end(); ++ it) {
		const tflag4& flag4 = it->second;

		SDL_snprintf(key_buf, sizeof(key_buf), "legend_png%i", flag4.code);
		find_widget<tcontrol>(window_, key_buf, false, true)->set_label(flag4.png);

		SDL_snprintf(key_buf, sizeof(key_buf), "legend_msgstr%i", flag4.code);
		find_widget<tlabel>(window_, key_buf, false, true)->set_label(flag4.msgstr);
	}
}

bool tnew_wkocourse::can_next() const
{
	VALIDATE(script_flags_.size() == scripts_.size(), null_str);
	VALIDATE(course_flag_ != nposm, null_str);

	int script_count = script_flags_.size();
	for (int at = 0; at < script_count; at ++) {
		if (script_flags_[at] == flag_diff) {
			return false;
		}
	}

	return course_flag_ != flag_diff;
}

void tnew_wkocourse::click_back(tbutton& widget)
{
	VALIDATE(can_next(), null_str);

	const std::string wkoscript_dir = aplt_.wkoscript_dir(false);
	int script_count = scripts_.size();
	for (int at = 0; at < script_count; at ++) {
		const aplt::twkoscript& script = scripts_[at];

		if (is_flag_require_write(script_flags_[at])) {
			const std::string filename = script.build_script_filename(wkoscript_dir);
			script.to_file(filename);
		}
	}

	if (is_flag_require_write(course_flag_)) {
		const std::string wkocourse_dir = aplt_.wkocourse_dir(false);
		std::string course_filename = course_.build_course_filename(wkocourse_dir);
		course_.to_file(course_filename);
	}

	int retval = twindow::OK;
	window_->set_retval(retval);
}

void tnew_wkocourse::click_working_dir(tbutton& widget)
{
	win_ShellExecuteW_open(aplt_.preferences_dir);
}

void tnew_wkocourse::click_new_dirs(tbutton& widget)
{
/*
	std::set<std::string> new_dirs;
	aplt::list_wkoscript_files_by_type(wkoscript_dir_, aplt::type_wkoscript_new_dirs, new_dirs);

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	for (std::set<std::string>::const_iterator it = new_dirs.begin(); it != new_dirs.end(); ++ it) {
		const std::string& dir = *it;
		items.push_back(gui2::tmenu::titem(dir, items.size()));
		if (dir == curr_new_dir_) {
			initial_sel = items.back().val;
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
	std::set<std::string>::const_iterator curr_it = new_dirs.begin();
	if (cursel != 0) {
		std::advance(curr_it, cursel);
	}

	curr_new_dir_ = *curr_it;
	widget.set_label(curr_new_dir_);

	generate_widget_->set_active(true);
*/
}

void set_ratio_grid(ttoggle_panel& row, bool is_type)
{
	tgrid* type_grid = find_widget<tgrid>(&row, "type_grid", false, true);
	tgrid* file_grid = find_widget<tgrid>(&row, "file_grid", false, true);

	if (is_type) {
		file_grid->set_visible(twidget::INVISIBLE);

	} else {
		type_grid->set_visible(twidget::INVISIBLE);
	}
}

int tnew_wkocourse::get_wkoscript_file_flag(const std::string& wkoscript_dir, const aplt::twkoscript& script) const
{
	const std::string filename = script.build_script_filename(wkoscript_dir);
	if (!SDL_IsFile(filename.c_str())) {
		return flag_new;
	}

	aplt::twkoscript script2;
	script2.from_file(filename);
	if (!script2.valid()) {
		return flag_diff;
	}

	if (!script.equal(script2)) {
		return flag_diff;
	}

	if (script2.version != game_config::rose_version) {
		return flag_vermismatch;
	}

	return flag_same;
}

int tnew_wkocourse::get_wkocourse_file_flag(const std::string& wkocourse_dir, const aplt::twkocourse& course) const
{
	const std::string filename = course.build_course_filename(wkocourse_dir);
	if (!SDL_IsFile(filename.c_str())) {
		return flag_new;
	}

	aplt::twkocourse course2;
	course2.from_file(filename);
	if (!course2.valid()) {
		return flag_diff;
	}

	if (!course.equal(course2)) {
		return flag_diff;
	}

	if (course2.version != game_config::rose_version) {
		return flag_vermismatch;
	}
	return flag_same;
}

std::string tnew_wkocourse::method_str(const std::string& str, int flag_code) const
{
	if (flag_code != flag_diff) {
		return str;
	}
	return ht::generate_format(str, color_to_uint32(font::BAD_COLOR));
}

void tnew_wkocourse::reload_action_list(tlistbox& list)
{
	list.clear();

	script_flags_.clear();
	course_flag_ = nposm;

	std::map<std::string, std::string> data;

	data["type"] = std::string(_("noun^Workout")) + "(wkoscript)";
	set_ratio_grid(list.insert_row(data), true);

	const std::string wkoscript_dir = aplt_.wkoscript_dir(false);
	int script_count = scripts_.size();
	for (int at = 0; at < script_count; at ++) {
		const aplt::twkoscript& script = scripts_[at];

		data["number"] = str_cast(at + 1);

		int flag = get_wkoscript_file_flag(wkoscript_dir, script);
		script_flags_.push_back(flag);
		const tflag4& flag4 = flags_.find(flag)->second;
		data["flag"] = flag4.png;

		data["title"] = script.title;
		data["file"] = script.id_to_filename(script.id);
		data["method"] = method_str(flag4.method, flag4.code);

		ttoggle_panel& row = list.insert_row(data);
		set_ratio_grid(row, false);
	}
	VALIDATE(script_flags_.size() == scripts_.size(), null_str);

	data["type"] = std::string(_("Course")) + "(wkocourse)";
	set_ratio_grid(list.insert_row(data), true);

	const std::string wkocourse_dir = aplt_.wkocourse_dir(false);
	{
		data["number"] = "1";

		int flag = get_wkocourse_file_flag(wkocourse_dir, course_);
		course_flag_ = flag;
		const tflag4& flag4 = flags_.find(flag)->second;
		data["flag"] = flag4.png;

		data["title"] = course_.title;
		data["file"] = course_.id_to_filename(course_.id);
		data["method"] = method_str(flag4.method, flag4.code);

		ttoggle_panel& row = list.insert_row(data);
		set_ratio_grid(row, false);
	}
}

void tnew_wkocourse::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

