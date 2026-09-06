#define GETTEXT_DOMAIN "kdesktop-lib"

#include "gui/dialogs/wko_settings.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gettext.hpp"
#include "game_config.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(kdesktop, wko_settings)

twko_settings::twko_settings(tvlog_cfg& vlog_cfg)
	: original_vlog_cfg_(vlog_cfg)
	, vlog_cfg_(vlog_cfg)
	, current_layer_(nposm)
	, main_report_(nullptr)
	, main_stack_(nullptr)
	, start_caption_msg_widget_(nullptr)
	, finish_caption_msg_widget_(nullptr)
{
}

void twko_settings::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	//
	// main report
	//
	treport* report = find_widget<treport>(window_, "main_report", false, true);
	report->insert_item(null_str, _("Vlog"));
	report->insert_item(null_str, _("wko_settings^Misc"));

	// report->set_did_item_pre_change(std::bind(&twko_settings::did_main_report_pre_change, this, _1, _2, _3));
	report->set_did_item_changed(std::bind(&twko_settings::did_main_report_changed, this, _1, _2));
	main_report_ = report;

	//
	// main stack
	//
	main_stack_ = find_widget<tstack>(window_, "main_stack", false, true);
	pre_vlog(*main_stack_->layer(VLOG_LAYER));
	pre_misc(*main_stack_->layer(MISC_LAYER));

	report->select_item(VLOG_LAYER);
}

void twko_settings::post_show()
{
}

void twko_settings::pre_vlog(tgrid& grid)
{
	find_widget<tlabel>(&grid, "remark", false, true)->set_label(_("vlog_settings^remark"));

	ttoggle_button* toggle = nullptr;
	for (std::map<int, std::string>::const_iterator it = vlog_cfg_.hide_id_2_names_.begin(); it != vlog_cfg_.hide_id_2_names_.end(); ++ it) {
		int hid = it->first;
		const std::string& name = it->second;

		toggle = find_widget<ttoggle_button>(&grid, name, false, true);
		toggle->set_value(!vlog_cfg_.hides[hid]);
		toggle->set_did_state_changed(std::bind(&twko_settings::did_hide_changed, this, _1, hid));
	}

	toggle = find_widget<ttoggle_button>(&grid, "video_on_center", false, true);
	toggle->set_value(vlog_cfg_.video_on_center);
	toggle->set_did_state_changed(std::bind(&twko_settings::did_toggle_id_on_top_changed, this, _1, toggleid_video));

	tbutton* button = find_widget<tbutton>(&grid, "start_caption_msg", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&twko_settings::click_xxx_caption_msg
			, this, std::ref(*button), tvlog_cfg::hid_start_caption));
	button->set_icon("misc/edit.png");
	start_caption_msg_widget_ = button;
	update_xxx_captuion_msg_label(tvlog_cfg::hid_start_caption);

	button = find_widget<tbutton>(&grid, "finish_caption_msg", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&twko_settings::click_xxx_caption_msg
			, this, std::ref(*button), tvlog_cfg::hid_finish_caption));
	button->set_icon("misc/edit.png");
	finish_caption_msg_widget_ = button;
	update_xxx_captuion_msg_label(tvlog_cfg::hid_finish_caption);

	toggle = find_widget<ttoggle_button>(&grid, "poses_on_top", false, true);
	toggle->set_value(vlog_cfg_.poses_on_top);
	toggle->set_did_state_changed(std::bind(&twko_settings::did_toggle_id_on_top_changed, this, _1, toggleid_poses));
}

void twko_settings::pre_misc(tgrid& grid)
{
	ttoggle_button* toggle = find_widget<ttoggle_button>(&grid, "rep_sfx", false, true);
	toggle->set_value(!vlog_cfg_.misc_sfx_disabled);
	toggle->set_did_state_changed(std::bind(&twko_settings::did_toggle_id_on_top_changed, this, _1, toggleid_rep_sfx));

	tslider* slider = find_widget<tslider>(&grid, "sound_slider", false, true);
	slider->set_did_value_changed(std::bind(&twko_settings::sound_changed, this, _1, _2));
	slider->set_value(preferences::sound_volume());
}

void twko_settings::did_main_report_changed(treport& report, ttoggle_button& row)
{
	tgrid* current_layer = main_stack_->layer(row.at());
	main_stack_->set_radio_layer(row.at());

	current_layer_ = row.at();

	if (row.at() == VLOG_LAYER) {

	} else {
		VALIDATE(row.at() == MISC_LAYER, null_str);
	}
}

void twko_settings::did_hide_changed(ttoggle_button& widget, int hid)
{
	VALIDATE(hid >= 0 && hid < vlog_cfg_.hid_count, null_str);
	vlog_cfg_.hides[hid] = !widget.get_value();
}

void twko_settings::did_toggle_id_on_top_changed(ttoggle_button& widget, int toggle_id)
{
	if (toggle_id == toggleid_video) {
		vlog_cfg_.video_on_center = widget.get_value();

	} else if (toggle_id == toggleid_poses) {
		vlog_cfg_.poses_on_top = widget.get_value();

	} else if (toggle_id ==toggleid_rep_sfx) {
		vlog_cfg_.misc_sfx_disabled = !widget.get_value();

		// sound::rose_play_sound_simple("rep_phase1.wav");

	} else {
		VALIDATE(false, null_str);
	}
}

void twko_settings::update_xxx_captuion_msg_label(int hid)
{
	const std::string* msg = nullptr;
	tbutton* widget = nullptr;
	if (hid == vlog_cfg_.hid_start_caption) {
		msg = &vlog_cfg_.start_caption_msg;
		widget = start_caption_msg_widget_;

	} else if (hid == vlog_cfg_.hid_finish_caption) {
		msg = &vlog_cfg_.finish_caption_msg;
		widget = finish_caption_msg_widget_;

	} else {
		VALIDATE(false, null_str);
	}
	VALIDATE(msg != nullptr, null_str);
	VALIDATE(widget != nullptr, null_str);

	const int max_gui_chars = 12;
	std::string label = msg->empty()? _("<Not set>"): utils::truncate_to_max_chars2(*msg, max_gui_chars, true);

	widget->set_label(label);
}

bool twko_settings::verify_xxx_caption_msg(const std::string& label, int hid, const std::string& initial, int max_chars) const
{
	if (label == initial) {
		return false;
	}
	return utils::isvalid_utf8_name(label, 0, max_chars);
}

void twko_settings::click_xxx_caption_msg(tbutton& widget, int hid)
{
	utils::string_map symbols;
	std::string prefix;
	std::string placeholder;
	std::string remark;
	std::string* result_to = nullptr;

	// it is
	int max_chars = 48; // MAX_NORMAL_UTF8_NAME_CHARS;

	if (hid == vlog_cfg_.hid_start_caption) {
		result_to = &vlog_cfg_.start_caption_msg;

	} else if (hid == vlog_cfg_.hid_finish_caption) {
		result_to = &vlog_cfg_.finish_caption_msg;

	} else {
		VALIDATE(false, null_str);
	}

	tgrid& grid = *main_stack_->layer(VLOG_LAYER);

	VALIDATE(vlog_cfg_.hide_id_2_names_.count(hid) != 0, null_str);
	const std::string& widget_id = vlog_cfg_.hide_id_2_names_.find(hid)->second;
	symbols["name"] = find_widget<ttoggle_button>(&grid, widget_id, false, true)->label();
	std::string title = vgettext2("Edit $name text", symbols);
	std::string initial = *result_to;

	std::vector<std::pair<std::string, std::string> > freq_vals;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = vlog_cfg_.freq_caption_msgs.begin(); it != vlog_cfg_.freq_caption_msgs.end(); ++ it) {
		const std::string* msg = nullptr;
		if (hid == vlog_cfg_.hid_start_caption) {
			msg = &it->first;
		} else if (hid == vlog_cfg_.hid_finish_caption) {
			msg = &it->second;
		} else {
			VALIDATE(false, null_str);
		}
		freq_vals.push_back(std::make_pair(*msg, null_str));
	}

	std::string msg;
	{
		gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), max_chars, gui2::tedit_box_param::show_cancel, true);
		param.freq_vals = freq_vals;
		param.did_text_changed = std::bind(&twko_settings::verify_xxx_caption_msg, this, _1, hid, std::ref(initial), max_chars);
		{
			gui2::tedit_box dlg(param);
			// it is in landscape, on android/ios, soft-keyboard is almost height.
			dlg.show(nposm, window_->get_height() / 10); // / 5
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
		}
		msg = param.result;
	}

	*result_to = msg;
	update_xxx_captuion_msg_label(hid);
}

void twko_settings::sound_changed(tslider& widget, int value)
{
	preferences::set_sound_volume(value);

	tgrid& grid = *main_stack_->layer(MISC_LAYER);
	find_widget<tlabel>(&grid, "volume", false, true)->set_label(str_cast(value));
}

} // namespace gui2

