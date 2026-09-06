#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/health_scene.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/toggle_button.hpp"

#include "health_controller.hpp"
#include "hotkeys.hpp"
#include "gettext.hpp"
#include "preferences.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, health_scene);

thealth_scene::thealth_scene(trhealth_scene_slot& scene_slot, health_controller& controller)
	: tdialog(&controller)
	, scene_slot_(scene_slot)
	, controller_(controller)
	, curr_toolbar_layer_(TB_NORMAL_LAYER)
	, toolbar_stack_(nullptr)
	, watermark_widget_(nullptr)
	, main_report_(nullptr)
	, status_widget_(nullptr)
	, flt_erase_widget_(nullptr)
{
}

thealth_scene::~thealth_scene()
{
}

void thealth_scene::pre_show()
{
	// prepare status report.
	reports_.insert(std::make_pair(ZOOM, "zoom"));
	reports_.insert(std::make_pair(POSITION, "position"));

	// prepare hotkey
	hotkey::insert_hotkey(HOTKEY_RETURN, "return", null_str);
	hotkey::insert_hotkey(HOTKEY_SHARE, "share", null_str);

	// tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid(), &reports_);
	scene_slot_.pre_show(*window_);

	timage* image = find_widget<timage>(window_, "left_spacer", false, true);
	int left_spacer_w = 0;
	if (game_config::os == os_ios) {
		left_spacer_w = game_config::statusbar_height * 3.0 / 4;
	}
	image->set_best_size_1th(left_spacer_w, image->get_width_is_max(),
		 nposm, image->get_height_is_max());

	tstack* stack = find_widget<tstack>(window_, "toolbar_stack", false, true);
	pre_toolbar_normal(*stack->layer(TB_NORMAL_LAYER));
	pre_toolbar_share(*stack->layer(TB_SHARE_LAYER));
	toolbar_stack_ = stack;

	// utils::string_map symbols;
	// symbols["min_duration"] = utils::format_elapse_hms(WKO_MIN_REPORT_GENERATION_DURATION_S);

	status_widget_ = find_widget<tlabel>(window_, "status", false, true);
	set_status_label(controller_.default_status_msg());

	main_report_->select_item(0);
}

void thealth_scene::app_first_drawn()
{
	controller_.app_first_drawn();
}

void thealth_scene::app_resize_screen()
{
	controller_.app_resize_screen();
}

void thealth_scene::pre_toolbar_normal(tgrid& grid)
{
	utils::string_map symbols;

	tbutton* widget = dynamic_cast<tbutton*>(get_object("return"));
	click_generic_handler(*widget, null_str);

	treport* report = find_widget<treport>(&grid, "navigation_report", false, true);
	report->insert_item(null_str, _("Today")).set_cookie(health_controller::chartsel_today);
	report->insert_item(null_str, _("Yesterday")).set_cookie(health_controller::chartsel_yesterday);
	report->insert_item(null_str, _("The day before yesterday")).set_cookie(health_controller::chartsel_2daysago);
	symbols["count"] = str_cast(15);
	report->insert_item(null_str, vgettext2("$count days", symbols)).set_cookie(health_controller::chartsel_15days);
	symbols["count"] = str_cast(30);
	report->insert_item(null_str, vgettext2("$count days", symbols)).set_cookie(health_controller::chartsel_30days);

	std::vector<health_controller::tdyn_chartsel*>& dyn_chartsels = controller_.dyn_chartsels();
	int dyn_chartsel_count = dyn_chartsels.size();
	for (int at = 0; at < dyn_chartsel_count; at ++) {
		const health_controller::tdyn_chartsel& chartsel = *dyn_chartsels[at];
		// report->insert_item(null_str, chartsel.title()).set_cookie(health_controller::chartsel_dyn_min + at);
		report->insert_item(null_str, chartsel.title());
	}
	report->set_did_item_pre_change(std::bind(&thealth_scene::did_navigation_report_item_pre_change, this, _1, _2, _3));
	report->set_did_item_changed(std::bind(&thealth_scene::did_navigation_report_item_changed, this, _2));
	main_report_ = report;

	tbutton* button = find_widget<tbutton>(&grid, "insert", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&health_controller::click_insert_dyn_chart
			, &controller_, std::ref(*button)));

	button = find_widget<tbutton>(&grid, "share", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&thealth_scene::click_share
			, this, std::ref(*button)));

	button = find_widget<tbutton>(&grid, "test_lru", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&health_controller::click_test_lrn_cache
			, &controller_));
	button->set_visible(twidget::INVISIBLE);

	gui2::tfloat_widget* flt_widget = window_->find_float_widget("flt_to_top");
	button = dynamic_cast<tbutton*>(flt_widget->widget.get());
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&health_controller::click_flt_to_top
			, &controller_, std::ref(*button)));

	// flt_widget->set_visible(true);

	flt_widget = window_->find_float_widget("flt_erase");
	button = dynamic_cast<tbutton*>(flt_widget->widget.get());
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&health_controller::click_flt_erase
			, &controller_, std::ref(*button)));
	// flt_widget->set_visible(true);

	click_generic_handler(*widget, null_str);

	find_widget<tcontrol>(window_, "_mini_map", false, true)->set_visible(twidget::INVISIBLE);
}

void thealth_scene::pre_toolbar_share(tgrid& grid)
{
	// watermark
	ttext_box2* text_box2 = new ttext_box2(*window_, *find_widget<twidget>(&grid, "watermark", false, true));
	text_box2->text_box()->set_border("textbox");
	// window_->keyboard_capture(name_txt_->text_box());
	// user_widget->text_box().goto_end_of_data();  now not support, should fixed in future.

	text_box2->text_box()->set_placeholder(_("Watermark added to the bottom right corner of image"));
	text_box2->text_box()->set_maximum_chars(48);

	// make this set_label don't trigger 'did_watermark_text_changed'.
	text_box2->text_box()->set_label(preferences::share_watermark());
	text_box2->set_did_text_changed(std::bind(&health_controller::did_watermark_text_changed, &controller_, _1));
	watermark_widget_ = text_box2;

	tbutton* button = find_widget<tbutton>(&grid, "cancel2", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&thealth_scene::click_share_result
			, this, std::ref(*button), false));

	button = find_widget<tbutton>(&grid, "ok2", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&thealth_scene::click_share_result
			, this, std::ref(*button), true));
	ok2_widget_ = button;

	flt_erase_widget_ = window_->find_float_widget("flt_erase");
}

bool thealth_scene::did_navigation_report_item_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to)
{
/*
	if (!ai_can_switch(_("Switching"))) {
		return false;
	}
	if (from.at() == AI_LAYER && ai_curr_task_type_ != ai_tasktype_chat) {
		ai_clear_task();

	} else if (from.at() == NONAI_LAYER && nonai_curr_task_type_ != nposm) {
		nonai_clear_task();
	}
*/
	return true;
}

void thealth_scene::did_navigation_report_item_changed(ttoggle_button& widget)
{
	int desire_sel = widget.at();

	// curr_layer_ = desire_sel;
	controller_.clear_tip_mat2();
	// MUST not call 'controller_.clear_workout_mat2s()' here.
	// controller_.clear_workout_mat2s();
	controller_.refresh_chart_by_sel(desire_sel);
}

void thealth_scene::click_share(tbutton& widget)
{
	VALIDATE(curr_toolbar_layer_ == TB_NORMAL_LAYER, null_str);
	toolbar_stack_->set_radio_layer(TB_SHARE_LAYER);
	curr_toolbar_layer_ = TB_SHARE_LAYER;

	VALIDATE(ok2_widget_->get_active(), null_str);

	controller_.enter_share();
}

void thealth_scene::click_share_result(tbutton& widget, bool finish)
{
	VALIDATE(curr_toolbar_layer_ == TB_SHARE_LAYER, null_str);
	toolbar_stack_->set_radio_layer(TB_NORMAL_LAYER);
	curr_toolbar_layer_ = TB_NORMAL_LAYER;

	controller_.exit_share(finish);

	ok2_widget_->set_active(true);
}
/*
void thealth_scene::statusbar_refresh_report(int num, const std::string& label)
{
	controller_.gui().refresh_report(num, reports::report(label, null_str));
}
*/
void thealth_scene::set_status_label(const std::string& msg)
{
	VALIDATE(!msg.empty(), null_str);

	std::string msg2 = msg;
	msg2.append(" | " + controller_.consecutive_checkin_rule_msg());
	status_widget_->set_label(msg2);
}

void thealth_scene::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case MSG_SHOW_MESSAGE:
		{
			tmsg_data_show_message* pdata = static_cast<tmsg_data_show_message*>(msg->pdata);
			gui2::show_message(null_str, pdata->msg);
		}
		break;

	default:
		VALIDATE(false, null_str);
	}

	if (msg->pdata != nullptr) {
		delete msg->pdata;
	}
}

} //end namespace gui2
