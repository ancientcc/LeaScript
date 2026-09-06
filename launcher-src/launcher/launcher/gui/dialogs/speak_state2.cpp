#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/speak_state2.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"

using namespace std::placeholders;

namespace gui2 {


REGISTER_DIALOG(launcher, speak_state2)

tspeak_state2::tspeak_state2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, aplt::twkoscript::tstate2& state2, std::vector<std::string>& state_names, int sdl_field_small_font_size)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, twko_state2(state2.state, state_names)
	, original_state2_(state2)
	, state2_(state2)
	// , state_name_(state_name)
	, sdl_field_small_font_size_(sdl_field_small_font_size)
	, task_remark_widget_(nullptr)
{
	set_timer_interval(1000);
}

void tspeak_state2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());
	twko_state2::pre_show(*window_, find_widget<ttext_box>(window_, "state_name", false));

	// find_widget<tlabel>(window_, "title", false).set_label(state_name_);

	tbutton* button = find_widget<tbutton>(window_, "back", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tspeak_state2::click_back
			, this
			, std::ref(*button)));

	ttoggle_button* toggle = find_widget<ttoggle_button>(window_, "debug_skip", false, true);
	toggle->set_label(dgettext("rose-lib", "wko^debug_skip label"));
	toggle->set_value(state2_.debug_skip);
	toggle->set_did_state_changed(std::bind(&tspeak_state2::did_state2_bool_field_changed, this, _1,
		aplt::twkoscript::fid_debug_skip));

	//
	// task
	//
	const aplt::twkoscript::ttask_speak* task = static_cast<const aplt::twkoscript::ttask_speak*>(state2_.task);

	tscroll_text_box* scroll_text_box = find_widget<tscroll_text_box>(window_, "msgstr", false, true);
	// ttext_box* text_box = find_widget<ttext_box>(window_, "msgstr", false, true);
	ttext_box* text_box = scroll_text_box->tb();
	text_box->set_label(task->msgstr);
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_task_msgstr, scroll_text_box);

	text_box = find_widget<ttext_box>(window_, "repeat_s", false, true);
	if (task->repeat_s != nposm) {
		text_box->set_label(str_cast(task->repeat_s));
	}
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_task_repeat_s);

	text_box = find_widget<ttext_box>(window_, "min_state_duration_s", false, true);
	if (task->min_state_duration_s != nposm) {
		text_box->set_label(str_cast(task->min_state_duration_s));
	}
	set_did_text_changed2(*text_box, aplt::twkoscript::typeid_task, aplt::twkoscript::fid_task_min_state_duration_s);

	tlabel* label = find_widget<tlabel>(window_, "task_remark", false, true);
	task_remark_widget_ = label;

	//
	update_task_remark_label();
}

void tspeak_state2::post_show()
{
}

extern void xxx_state2_click_back(twko_state2& wko_state2, aplt::twkoscript::tstate2& state2, gui2::twindow& window);

void tspeak_state2::click_back(tbutton& widget)
{
	xxx_state2_click_back(*this, state2_, *window_);
}

void tspeak_state2::set_did_text_changed2(ttext_box& widget, int type, int fid, tscroll_text_box* scroll_text_box)
{
	std::string placeholder = state2_.get_placeholder_msg(type, fid);

	widget.set_placeholder(placeholder);
	widget.set_maximum_chars(72);
	if (scroll_text_box == nullptr) {
		widget.set_did_text_changed(std::bind(&tspeak_state2::did_fid_text_changed, this, _1, fid));
	} else {
		scroll_text_box->set_did_text_changed(std::bind(&tspeak_state2::did_fid_text_changed, this, _1, fid));
	}
}

void tspeak_state2::did_fid_text_changed(ttext_box& widget, int fid)
{
	const std::string& label = widget.label();

	VALIDATE(state2_.task->type == aplt::twkoscript::tasktype_speak, null_str);

	aplt::twkoscript::ttask_speak* task2 = static_cast<aplt::twkoscript::ttask_speak*>(state2_.task);

	bool remark_dirty = false;
	if (fid == aplt::twkoscript::fid_task_msgstr) {
		if (label != task2->msgstr) {
			task2->msgstr = label;
			remark_dirty = true;
		}

	} else if (fid == aplt::twkoscript::fid_task_repeat_s) {
		int new_repeat_s = nposm;
		if (!label.empty()) {
			new_repeat_s = utils::to_int(label);
		}
		if (new_repeat_s != task2->repeat_s) {
			task2->repeat_s = new_repeat_s;
			remark_dirty = true;
		}

	} else if (fid == aplt::twkoscript::fid_task_min_state_duration_s) {
		int new_min_state_duration_s = nposm;
		if (!label.empty()) {
			new_min_state_duration_s = utils::to_int(label);
		}
		if (new_min_state_duration_s != task2->min_state_duration_s) {
			task2->min_state_duration_s = new_min_state_duration_s;
			remark_dirty = true;
		}

	} else {
		VALIDATE(false, null_str);
	}

	if (remark_dirty) {
		update_task_remark_label();
	}
}

void tspeak_state2::did_state2_bool_field_changed(ttoggle_button& widget, int fid)
{
	/* if (fid == aplt::twkoscript::fid_is_setup) {
		state2_.is_setup = widget.get_value();

	} else */ if (fid == aplt::twkoscript::fid_debug_skip) {
		state2_.debug_skip = widget.get_value();

	} else {
		VALIDATE(false, null_str);
	}
}

void tspeak_state2::update_task_remark_label() const
{
	aplt::twkoscript::ttask_speak* task2 = static_cast<aplt::twkoscript::ttask_speak*>(state2_.task);

	utils::string_map symbols;
	symbols["msgstr"] = task2->msgstr;

	std::string msg;
	if (task2->repeat_s == nposm) {
		msg = vgettext2("speak_state2 remark, $msgstr", symbols);

	} else {
		symbols["repeat_s"] = str_cast(task2->repeat_s);
		msg = vgettext2("speak_state2 remark, $msgstr, $repeat_s", symbols);
	}

	task_remark_widget_->set_label(msg);
}

void tspeak_state2::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

