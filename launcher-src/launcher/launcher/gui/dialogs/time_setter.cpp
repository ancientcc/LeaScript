#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/time_setter.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
#include "formula_string_utils.hpp"

namespace gui2 {

REGISTER_DIALOG(launcher, time_setter)

ttime_setter::ttime_setter(const aplt::tapplet& applet, aplt::tbg_task& timing, aplt::taplt_task& task)
	: applet_(applet)
	, timing_(timing)
	, task_(task)
	, current_time_widget_(nullptr)
	, state_widget_(nullptr)
	, ok_widget_(nullptr)
	, state_strategy_(strategy_original)
{
	set_timer_interval(400);

	utils::string_map symbols;
	strategies_.insert(std::make_pair(strategy_original, _("strategy^strategy_original")));
	symbols["state"] = timing_.task_state_desc(aplt::taplt_task::state_fresh);
	strategies_.insert(std::make_pair(strategy_fresh, vgettext2("strategy^strategy_fresh $state", symbols)));
	symbols["state"] = timing_.task_state_desc(aplt::taplt_task::state_finished_expired);
	strategies_.insert(std::make_pair(strategy_finished, vgettext2("strategy^strategy_finished $state", symbols)));
	VALIDATE(strategies_.size() == strategy_count, null_str);

	const std::map<aplt::taplt_task_key, aplt::taplt_task>& tasks = timing_.timed_tasks();
	for (std::map<aplt::taplt_task_key, aplt::taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const aplt::taplt_task& task = it->second;
		if (task.aplt_id != task_.aplt_id || task.task_id != task_.task_id || task.zerotz_t != task_.zerotz_t) {
			others_zerotz_t_.insert(task.zerotz_t);
		}
	}
}

static void get_3fields(int elapse, int& hour, int& min, int &sec)
{
	int one_day_seconds = 24 * 3600;
	elapse = elapse % one_day_seconds;

	sec = elapse % 60;
	min = (elapse / 60) % 60;
	hour = (elapse / 3600) % 24;
}

void ttime_setter::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	std::stringstream ss;

	ss.str("");
	VALIDATE(applet_.tasks.count(task_.task_id) != 0, null_str);
	ss << applet_.tasks.find(task_.task_id)->second.name << "(" << task_.aplt_id << ")";
	find_widget<tlabel>(window_, "title", false).set_label(ss.str());

	tlabel* label = find_widget<tlabel>(window_, "current_time", false, true);
	label->set_label(utils::format_time_hms(time(nullptr)));
	current_time_widget_ = label;

	std::map<std::string, int> types;
	types.insert(std::make_pair("hour", type_hour));
	types.insert(std::make_pair("minute", type_minute));
	types.insert(std::make_pair("second", type_second));

	tbutton* button = find_widget<tbutton>(window_, "state", true, false);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&ttime_setter::click_state
			, this
			, std::ref(*button)));
	button->set_border("textbox");
	VALIDATE(state_strategy_ >= 0 && state_strategy_ < (int)strategies_.size(), null_str);
	button->set_label(strategies_.find(state_strategy_)->second);
	state_widget_ = button;

	int hour;
	int min;
	int sec;
	get_3fields(task_.zerotz_t, hour, min, sec);
	for (std::map<std::string, int>::const_iterator it = types.begin(); it != types.end(); ++ it) {
		int type = it->second;
		button = find_widget<tbutton>(window_, it->first, true, false);
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&ttime_setter::click_clock
				, this
				, std::ref(*button)
				, it->second));
		button->set_border("textbox");

		std::string label;
		if (type == type_hour) {
			label = str_cast(hour);
		} else if (type == type_minute) {
			label = str_cast(min);
		} else if (type == type_second) {
			label = str_cast(sec);
		}
		button->set_label(label);
	}

	ok_widget_ = find_widget<tbutton>(window_, "ok", true, false);
}

void ttime_setter::post_show()
{
	int strategy = state_strategy_;
	const int elapse_today = (time(nullptr) + game_config::equation_of_time) % (24 * 3600);
	if (strategy == strategy_fresh) {
		if (task_.zerotz_t > elapse_today) {
			task_.state = aplt::taplt_task::state_fresh;
		}

	} else if (strategy == strategy_finished) {
		if (task_.zerotz_t > elapse_today) {
			task_.state = aplt::taplt_task::state_finished_expired;
		}

	}
}

void ttime_setter::click_clock(tbutton& widget, int type)
{
	int hour;
	int min;
	int sec;
	get_3fields(task_.zerotz_t, hour, min, sec);

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	if (type == type_hour) {
		for (int at = 0; at < 24; at ++) {
			items.push_back(gui2::tmenu::titem(str_cast(at), at));
		}
		initial_sel = hour;

	} else if (type == type_minute) {
		for (int at = 0; at < 60; at ++) {
			items.push_back(gui2::tmenu::titem(str_cast(at), at));
		}
		initial_sel = min;

	} else if (type == type_second) {
		for (int at = 0; at < 60; at ++) {
			items.push_back(gui2::tmenu::titem(str_cast(at), at));
		}
		initial_sel = sec;

	} else {
		VALIDATE(false, null_str);
	}
	
	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int new_value = dlg.selected_val();
	if (type == type_hour) {
		hour = new_value;

	} else if (type == type_minute) {
		min = new_value;

	} else if (type == type_second) {
		sec = new_value;

	}
	task_.zerotz_t = hour * 3600 + min * 60 + sec;
	widget.set_label(str_cast(new_value));

	did_task_zerotz_t_changed();
}

void ttime_setter::click_state(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	for (std::map<int, std::string>::const_iterator it = strategies_.begin(); it != strategies_.end(); ++ it) {
		int strategy = it->first;
		items.push_back(gui2::tmenu::titem(it->second, strategy));
	}
	
	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();
	state_strategy_ = cursel;
	widget.set_label(strategies_.find(cursel)->second);
}

void ttime_setter::did_task_zerotz_t_changed()
{
	bool active = others_zerotz_t_.count(task_.zerotz_t) == 0;
	ok_widget_->set_active(active);
}

void ttime_setter::app_timer_handler(uint32_t now)
{
	current_time_widget_->set_label(utils::format_time_hms(time(nullptr)));
}

} // namespace gui2

