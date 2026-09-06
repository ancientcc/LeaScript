#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/center.hpp"

#include "gui/widgets/settings.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/text_box2.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "game_config.hpp"
#include "chinese.hpp"

#include "dcamera_driver.hpp"
#include "ai_driver.hpp"
#include "bg_task2.hpp"

#include "serialization/parser.hpp"
#include "minizip/minizip.hpp"

using namespace std::placeholders;

namespace utils {
// for utf8 continuation bytes must be rule: b7 must 1, b6 must 0.
// caller must define some variable:
// const uint8_t* data_ptr = (const uint8_t*)utf8str.c_str();
// int pos; when call, pos is offset utf8-char's first byte. after macro, pos will be offset first byte after uftf8-char.
// int continuation_pos;
#define check_utf8_continuation_byte(c_bytes, fail_res)	\
	for (++ pos, continuation_pos = 0; continuation_pos < c_bytes; continuation_pos ++, pos ++) { \
		if (((uint8_t)(data_ptr[pos]) & 0xc0) != 0x80) { \
			return fail_res; \
		} \
	}

}

namespace gui2 {

REGISTER_DIALOG(launcher, center)

tcenter::tcenter(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tros_instance& ros_instance, aplt::tbg_task2& temp_task, const std::map<aplt::taplt_key, aplt::tapplet>& applets, aplt::tb_api& b_api, 
	tros_map& curmap, aplt::tbg_task& bg_task, tbase_driver& base_driver, tspeech_driver& speech_driver, tdcamera_driver& dcamera_driver, tai_driver& ai_driver, const std::string& saves_courseware_dir,
	const tstart_aiagent* start_aiagent)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, tbase_courseware(saves_courseware_dir)
	// , tcamera::tslot()
	, ros_instance_(ros_instance)
	, drivers_(ros_instance.drivers())
	, camera_(instance->camera())
	, temp_task_(temp_task)
	, applets_(applets)
	, b_api_(b_api)
	, curmap_(curmap)
	, bg_task_(bg_task)
	, base_driver_(base_driver)
	, speech_driver_(speech_driver)
	, dcamera_driver_(dcamera_driver)
	, ai_driver_(ai_driver)
	, ai_start_aiagent_()
	, ai_slot_(ai_driver.slot)
	, pinyin_(chinese::curr_pinyin)
	, ai_disable_find_user_(true)
	, def_item_list_layer_(POSITIONS_LAYER)
	, curr_layer_(nposm)
	, ai2_report_(nullptr)
	, camera_widget_(nullptr)
	, task_report_(nullptr)
	, body_stack_(nullptr)
	, status_widget_(nullptr)
	, item_list_stack_(nullptr)
	, position_list_(nullptr)
	, timing_task_list_(nullptr)
	, history_(nullptr)
	, vrenderer_widget_(nullptr)
	, ai_left_top_grid_(nullptr)
	, ai_task_type_widget_(nullptr)
	, ai_tokens_widget_(nullptr)
	, ai_task_widget_(nullptr)
	, ai_new_conversation_widget_(nullptr)
	, ai_find_user_widget_(nullptr)
	, ai_input_grid_(nullptr)
	, ai_input_(nullptr)
	, ai_input_tb_(nullptr)
	, ai_input_scale_(nullptr)
	, ai_keyboard_spacer_(nullptr)
	, ai_task_toolbar_stack_(nullptr)
	, ai_courseware_main_cfg_listen_widget_(nullptr)
	, ai_send_widget_(nullptr)
	, ai_main_cfg_list_(nullptr)
	// ai layer
	, ai_task_types_({
		{ai_tasktype_chat, tai_task_type(ai_tasktype_chat, "temp_chat", _("Temporary Chat"), _("Within this task, new conversations are allowed"))},
		{ai_tasktype_followup, tai_task_type(ai_tasktype_followup, "followup", _("aiagent^Followup"), _("Please choose"))},
		{ai_tasktype_courseware, tai_task_type(ai_tasktype_courseware, "courseware", _("aiagent^Courseware"), _("Please choose"))},
		{ai_tasktype_aiagent, tai_task_type(ai_tasktype_aiagent, "aiagent", _("aiagent^aiagent"), _("Please choose"))}
	})
	, ai_tasktype_aiagent_at_(nposm)
	, ai_simulate_mobile_(false) // only valid on os_windows
	, ai_no_keyboard_((game_config::os == os_windows && !ai_simulate_mobile_) || preferences::keyboard_style() == keyboard_style_disable)
	, def_input_scale_height_(160 * twidget::hdpi_scale)
	, ai_new_conversation_(true)
	, ai_input_tokens_(0)
	, ai_output_tokens_(0)
	, ai_inputing_(false)
	, ai_curr_task_type_(nposm)
	, ai_curr_followup_(nullptr)
	, ai_list_state_(nposm)
	, ai_curr_task3_(nullptr)
	, ai_curr_aiagent_task_api_(nullptr)
	// non ai layer
	, nonai_curr_layer_(NONAI_HOME_LAYER)
	, nonai_curr_task_type_(nposm)
	, nonai_curr_position_(nullptr)
	, nonai_curr_timing_(nullptr)
	, nonai_task_clearing_(false)
{
	if (start_aiagent != nullptr) {
		VALIDATE(start_aiagent->pair.aplt != nullptr && start_aiagent->pair.task != nullptr, null_str);
		// ai_start_aiagent_ = tstart_aiagent(*start_aiagent->pair.aplt, *start_aiagent->pair.task, 
		//	start_aiagent->subject, start_aiagent->header, start_aiagent->did_handle_text);
		ai_start_aiagent_ = *start_aiagent;
	}

	set_timer_interval(1000);
	instance->adjust_logs_pb();
}

tcenter::~tcenter()
{
	VALIDATE(base_driver_.camera_viewer() == nullptr, null_str);
	VALIDATE(temp_task_.camera_viewer() == nullptr, null_str);
}

void tcenter::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	tbutton* button = find_widget<tbutton>(window_, "back", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcenter::click_back
			, this, std::ref(*button)));

	ai_left_top_grid_ = find_widget<tgrid>(window_, "ai_left_top_grid", false, true);
	ai_task_type_widget_ = find_widget<tlabel>(window_, "task_type", false, true);
	ai_tokens_widget_ = find_widget<tlabel>(window_, "tokens", false, true);
	ai_task_widget_ = find_widget<tlabel>(window_, "task", false, true);

	tstack* stack = find_widget<tstack>(window_, "body_stack", false, true);
	pre_home(*stack->layer(NONAI_HOME_LAYER));
	pre_camera(*stack->layer(NONAI_CAMERA_LAYER));
	body_stack_ = stack;

	ai_set_task_type(ai_tasktype_chat);

	if (ai_slot_ != nullptr) {
		ai_set_send_label(!ai_slot_->is_nlp_questioning());
	}

	if (ai_start_aiagent_.pair.aplt != nullptr) {
		VALIDATE(!ai_slot_->is_nlp_questioning(), null_str);

		ai_did_tasks_item_click(*static_cast<tbutton*>(&task_report_->item(ai_tasktype_aiagent_at_)));

		int row_at = nposm;
		for (std::vector<tcand_task3>::iterator it = ai_curr_timings_.begin(); it != ai_curr_timings_.end(); ++ it) {
			const tcand_task3& task3 = *it;
			if (&task3.cfg_task == ai_start_aiagent_.pair.task) {
				row_at = std::distance(ai_curr_timings_.begin(), it);
			}
		}
		VALIDATE(row_at != nposm, null_str);
		position_list_->select_row(row_at);
	}
}

void tcenter::post_show()
{
	VALIDATE(base_driver_.camera_viewer() == nullptr, null_str);
	VALIDATE(temp_task_.camera_viewer() == nullptr, null_str);

	if (ai_slot_ != nullptr) {
		if (ai_driver_.is_aiagent_tasking()) {
			ai_driver_.stop_aiagent_task();

		} else if (ai_slot_->is_nlp_questioning()) {
			ai_slot_->stop_nlp_question();

		}

		VALIDATE(!ai_driver_.is_aiagent_tasking(), null_str);
		VALIDATE(!ai_driver_.is_nlp_questioning(), null_str);
	}

	if (ai_curr_task_type_ != ai_tasktype_chat) {
		// because curr_aiagent_task_api_ maybe not nullptr. 
		// it must call ai_clear_task. and must behind stop_aiagent_task().
		ai_clear_task();
	}
}

void tcenter::did_tasks_item_click(tbutton& widget)
{
	if (curr_layer_ == AI_LAYER) {
		ai_did_tasks_item_click(widget);

	} else {
		nonai_did_tasks_item_click(widget);
	}
}

void tcenter::did_list_row_changed(tlistbox& list, ttoggle_panel& row, int layer)
{
	if (curr_layer_ == AI_LAYER) {
		ai_did_list_row_changed(list, row, layer);

	} else {
		nonai_did_list_row_changed(list, row, layer);
	}
}

void tcenter::pre_home(tgrid& grid)
{
	find_widget<tlabel>(&grid, "title", false, true)->set_label(aplt::all_fake_applets.find(aplt::builtinid_center)->second.name);

	treport* report = find_widget<treport>(window_, "ai2_report", false, true);
	report->insert_item(null_str, _("center^ai layer"));
	if (ai_start_aiagent_.pair.aplt == nullptr) {
		report->insert_item(null_str, _("center^non-ai layer"));
	}
	report->set_did_item_pre_change(std::bind(&tcenter::did_ai2_report_item_pre_change, this, _1, _2, _3));
	report->set_did_item_changed(std::bind(&tcenter::did_ai2_report_item_changed, this, _2));
	ai2_report_ = report;

	tbutton* button = find_widget<tbutton>(&grid, "new_conversation", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcenter::ai_click_new_conversation
			, this, std::ref(*button)));
	ai_new_conversation_widget_ = button;

	button = find_widget<tbutton>(&grid, "camera", false, true);
	if (!camera_is_using()) {
		button->set_visible(twidget::HIDDEN);
	}
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcenter::click_camera
			, this, std::ref(*button)));
	camera_widget_ = button;
	if (base_driver_.scene_id().empty()) {
		camera_widget_->set_visible(twidget::HIDDEN);
	}

	report = find_widget<treport>(window_, "task_report", false, true);
	if (ai_start_aiagent_.pair.aplt == nullptr) {
		report->set_did_item_click(std::bind(&tcenter::did_tasks_item_click, this, _2));
	}
	task_report_ = report;

	history_ = find_widget<tlistbox>(&grid, "history", false, true);
	history_->enable_select(false);

	ai_input_grid_ = find_widget<tgrid>(&grid, "input_grid", false, true);
	ai_input_ = find_widget<tscroll_text_box>(&grid, "input", false, true);
	ai_input_scale_ = find_widget<tspacer>(&grid, "input_scale", false, true);
	ai_input_scale_->set_best_size_1th(nposm, ai_input_scale_->get_width_is_max(), def_input_scale_height_, ai_input_scale_->get_height_is_max());

	ai_keyboard_spacer_ = find_widget<tspacer>(&grid, "keyboard_spacer", false, true);

	button = find_widget<tbutton>(&grid, "send", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcenter::ai_click_send
			, this, std::ref(*button)));

	ai_send_widget_ = button;

	ai_pre_input(*ai_input_);

	// device code
	ttext_box2* text_box2 = new ttext_box2(*window_, *find_widget<tcontrol>(&grid, "find_user", false, true), "textbox", null_str, false, "misc/find.png", ttext_box2::button_always_visible);
	const int max_chars = 16;
	text_box2->text_box()->set_maximum_chars(max_chars);
	text_box2->text_box()->set_placeholder(_("Username to search for"));
	text_box2->set_did_text_changed(std::bind(&tcenter::ai_did_find_user_text_box_changed, this, _1));
	connect_signal_mouse_left_click(
			*text_box2->button()
		, std::bind(
			&tcenter::ai_click_find_user
			, this, std::ref(*text_box2->button())));
	text_box2->set_visible(twidget::INVISIBLE);
	ai_find_user_widget_ = text_box2;

	status_widget_ = find_widget<tlabel>(&grid, "status", false, true);
	reset_status();

	tstack* stack = find_widget<tstack>(&grid, "item_list_stack", false, true);
	item_list_stack_ = stack;
	pre_position_list(*stack->layer(POSITIONS_LAYER));
	pre_timing_task_list(*stack->layer(TIMING_TASKS_LAYER));

	// focus back to input.
	// window_->keyboard_capture(ai_input_tb_);

	//
	// task toolbar stack
	//
	stack = find_widget<tstack>(window_, "task_toolbar_stack", false, true);
	ai_pre_task_toolbar_empty(*stack->layer(TASK_TOOLBAR_EMPTY_LAYER));
	ai_pre_task_toolbar_chat(*stack->layer(TASK_TOOLBAR_CHAT_LAYER));
	ai_pre_task_toolbar_followup(*stack->layer(TASK_TOOLBAR_FOLLOWUP_LAYER));
	ai_pre_task_toolbar_courseware_main_cfg(*stack->layer(TASK_TOOLBAR_COURSEWARE_MAIN_CFG_LAYER));
	ai_task_toolbar_stack_ = stack;

	if (ai_slot_ == nullptr) {
		ai2_report_->set_item_visible(AI_LAYER, false);
		// ai2_report_->item(AI_LAYER).set
	}
	ai2_report_->select_item(ai_slot_ != nullptr? AI_LAYER: NONAI_LAYER);

	// BUG: no effect. use MSG_LOGS_SCROLL_TO_BUTTOM
	// if (history_->rows() > 0) {
	//	history_->scroll_to_row2(history_->rows() - 1);
	// }
}

void tcenter::pre_camera(tgrid& grid)
{
	ttrack* track = find_widget<ttrack>(&grid, "vrenderer", false, true);
	track->set_did_draw(std::bind(&tcenter::did_draw_paper, this, _1, _2, _3));
	track->set_did_left_button_down(std::bind(&tcamera::did_left_button_down_paper, &camera_, _1, _2));
	track->set_did_mouse_motion(std::bind(&tcamera::did_mouse_motion_paper, &camera_, _1, _2, _3));
	track->set_did_mouse_leave(std::bind(&tcenter::did_mouse_leave_paper, this, _1, _2, _3));
	vrenderer_widget_ = track;
}

void tcenter::click_back(tbutton& widget)
{
	if (!ai_can_switch(_("Exit"))) {
		return;
	}

	window_->set_retval(twindow::CANCEL);
}

void tcenter::click_camera(tbutton& widget)
{
	// tcamera::tslot& slot = get_camera_slot();
	// slot.set_center_dlg(this);
	base_driver_.set_camera_viewer(this);
	temp_task_.set_camera_viewer(this);

	camera_post_enter_task_c();

	nonai_curr_layer_ = NONAI_CAMERA_LAYER;
	body_stack_->set_radio_layer(nonai_curr_layer_);
}

void tcenter::app_first_drawn()
{
	if (history_->rows() > 0) {
		tmsg_data_listbox_scroll_to_bottom* pdata = new tmsg_data_listbox_scroll_to_bottom(*window_, *history_);
		rtc::Thread::Current()->Post(RTC_FROM_HERE, this, POST_MSG_LOGS_SCROLL_TO_BOTTOM, pdata);
	}

	// if (history_->rows() > 0) {
		// history_->scroll_to_row2(history_->rows() - 1);
		// history_->scroll_to_row(history_->rows() - 1);
	// }
}

bool tcenter::did_ai2_report_item_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to)
{
	if (!ai_can_switch(_("Switching"))) {
		return false;
	}
	if (from.at() == AI_LAYER && ai_curr_task_type_ != ai_tasktype_chat) {
		ai_clear_task();

	} else if (from.at() == NONAI_LAYER && nonai_curr_task_type_ != nposm) {
		nonai_clear_task();
	}

	return true;
}

void tcenter::did_ai2_report_item_changed(ttoggle_button& widget)
{
	int desire_layer = widget.at();

	curr_layer_ = desire_layer;
	if (desire_layer == AI_LAYER) {
		ai_reload_tasks();

		ai_left_top_grid_->set_visible(twidget::VISIBLE);
		ai_input_grid_->set_visible(twidget::VISIBLE);

		camera_widget_->set_visible(twidget::INVISIBLE);
		
	} else {
		VALIDATE(desire_layer == NONAI_LAYER, null_str);
		nonai_reload_tasks();

		ai_left_top_grid_->set_visible(twidget::INVISIBLE);
		ai_input_grid_->set_visible(twidget::INVISIBLE);

		camera_widget_->set_visible(twidget::VISIBLE);
	}

	reload_log_list(*history_);
}

void tcenter::pre_position_list(tgrid& grid)
{
	tlistbox* list = find_widget<tlistbox>(&grid, "positions", false, true);
	list->enable_select(false);
	list->set_did_row_changed(std::bind(&tcenter::did_list_row_changed, this, _1, _2, POSITIONS_LAYER));
	position_list_ = list;

	ai_main_cfg_list_ = list;
}

void tcenter::pre_timing_task_list(tgrid& grid)
{
	tlistbox* list = find_widget<tlistbox>(&grid, "timing_tasks", false, true);
	list->enable_select(false);
	list->set_did_row_changed(std::bind(&tcenter::did_list_row_changed, this, _1, _2, TIMING_TASKS_LAYER));
	timing_task_list_ = list;
}

#define MAX_ITEM_MAJOR_LABEL_CHARS	400
#define MAX_ITEM_MINOR_LABEL_CHARS	1000
static std::string truncate_for_item_label(bool major, const std::string& label)
{
	if (label.empty()) {
		return label;
	}

	int max_chars = major? MAX_ITEM_MAJOR_LABEL_CHARS: MAX_ITEM_MINOR_LABEL_CHARS;

	bool ellipsis = true;
	return utils::truncate_to_max_chars2(label, max_chars, ellipsis);
}

void tcenter::reload_items_list(tlistbox& list, const std::vector<titem3>& items, bool truncate_major)
{
	list.clear();

	std::map<std::string, std::string> data;

	std::stringstream ss;
	
	for (std::vector<titem3>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem3& item = *it;
		
		data["major_label"] = truncate_major? truncate_for_item_label(true, item.id): item.id;
		data["major_label"] = item.id;
		data["minor_label"] = truncate_for_item_label(false, item.label);

		ttoggle_panel& panel = list.insert_row(data);
		panel.set_cookie(item.val);
	}
}

void tcenter::get_task_items(bool task_cpp, bool task_aiagent, uint32_t type_mask, std::vector<tcand_task3>& timings, std::vector<titem3>& items) const
{
	VALIDATE(timings.empty(), null_str);
	items.clear();

	std::stringstream ss;

	int at = 0;
	if (task_cpp) {
		const aplt::tapplet& bonus_aplt = aplt::fake_aplt;
		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it = bonus_aplt.tasks.begin(); it != bonus_aplt.tasks.end(); ++ it) {
			const aplt::tapplet::ttask& task = it->second;
			if (task.type != aplt::task_cpp) {
				continue;
			}
			if (type_mask != nposm && !(BIT_IDX_MASK(task.type) & type_mask)) {
				continue;
			}

			ss.str("");
			ss << bonus_aplt.name2() << "-" << task.name;

			items.push_back(titem3(at, task.name, bonus_aplt.name2()));
			at ++;
			timings.push_back(tcand_task3(bonus_aplt, task, null_str));
		}
	}

	std::vector<const aplt::tiot_device*> src_aliases;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it2 = aplt.tasks.begin(); it2 != aplt.tasks.end(); ++ it2) {
			const aplt::tapplet::ttask& task = it2->second;
			if (task.type == aplt::task_nonblock && task.subtype == aplt::tnonblock_api::subtype_charge) {
				continue;
			}
			if (task.type == aplt::task_cpp && !task_cpp) {
				continue;
			}

			if (!task_aiagent && task.type == aplt::task_aiagent) {
				continue;
			}
			if (type_mask != nposm && !(BIT_IDX_MASK(task.type) & type_mask)) {
				continue;
			}

			{
				ss.str("");
				ss << task.name;

				items.push_back(titem3(at, ss.str(), aplt.name2()));
				at ++;
				timings.push_back(tcand_task3(aplt, task, null_str));

			}

			if (aplt::iot_sources.count(task.iot_src) == 0) {
				continue;
			}
			const aplt::tiot_src2& iot_src2 = aplt::iot_sources.find(task.iot_src)->second;
			src_aliases = bg_task_.iot_devices_from_src(iot_src2.code);

			{
				for (std::vector<const aplt::tiot_device*>::const_iterator it2 = src_aliases.begin(); it2 != src_aliases.end(); ++ it2) {
					const aplt::tiot_device& alias = **it2;

					ss.str("");
					ss << task.name << "[" << alias.device_id << "]";
					items.push_back(titem3(at, ss.str(), aplt.name2()));
					at ++;
					timings.emplace_back(tcand_task3(aplt, task, alias.device_id));
				}
			}
		}
	}
	VALIDATE(items.size() == timings.size(), null_str);
	VALIDATE(at == (int)timings.size(), null_str);
}

//
// ai layer
// 
void tcenter::ai_reload_tasks()
{
	std::vector<int> types;
	if (ai_start_aiagent_.pair.aplt == nullptr) {
		types.push_back(ai_tasktype_chat);
		types.push_back(ai_tasktype_followup);
		types.push_back(ai_tasktype_courseware);
	}
	types.push_back(ai_tasktype_aiagent);

	treport& report = *task_report_;
	report.clear();
	for (std::vector<int>::const_iterator it = types.begin(); it != types.end(); ++ it) {
		int type = *it;
		const tai_task_type& task = ai_task_types_.find(type)->second;

		tcontrol& widget = report.insert_item(null_str, task.name);

		widget.set_icon(std::string("misc/") + task.id + ".png");
		widget.set_cookie(task.type);

		if (task.type == ai_tasktype_aiagent) {
			ai_tasktype_aiagent_at_ = report.items() - 1;
		}
	}
	VALIDATE(ai_tasktype_aiagent_at_ != nposm, null_str);
}

void tcenter::ai_do_new_conversation()
{
	ai_new_conversation_ = true;

	ai_input_tokens_ = 0;
	ai_output_tokens_ = 0;
	ai_set_tokens_label();
}

bool tcenter::ai_can_switch(const std::string& action)
{
	if (curr_layer_ != AI_LAYER) {
		return true;
	}

	std::string reason;
	if (ai_driver_.is_aiagent_tasking()) {
		reason = _("Executing AI agent task");

	} else if (ai_slot_->is_nlp_questioning()) {
		reason = _("The current task is waiting for a answer from the large module");

	} else {
		return true;
	}

	utils::string_map symbols;
	symbols["reason"] = reason;
	symbols["action"] = action;
	std::string msg = vgettext2("$reason. $action will end the waiting process. Do you want to continue?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return false;
	}

	if (ai_driver_.is_aiagent_tasking()) {
		ai_driver_.stop_aiagent_task();

	} else if (ai_slot_->is_nlp_questioning()) {
		ai_slot_->stop_nlp_question();

	} else {
		VALIDATE(false, null_str);
	}
	return true;
}

void tcenter::ai_did_tasks_item_click(tbutton& widget)
{
	utils::string_map symbols;
	int task_type = uint64_2_int(widget.cookie());

	if (ai_curr_task_type_ == ai_tasktype_chat && task_type == ai_tasktype_chat) {
		return;
	}

	if (!ai_can_switch(_("Switching"))) {
		return;
	}

	if (ai_curr_task_type_ != ai_tasktype_chat) {
		ai_clear_task();
	}

	VALIDATE(ai_curr_task_type_ == ai_tasktype_chat, null_str);
	ai_set_task_type(task_type);

	if (task_type == ai_tasktype_chat) {

	} else if (task_type == ai_tasktype_followup) {
		std::vector<titem3> items;
		ai_get_scene_items(nullptr, items);

		reload_items_list(*position_list_, items, true);
		set_status_task_label(task_type, _("Choose which followup to execute"));
		
	} else if (task_type == ai_tasktype_courseware) {
		VALIDATE(ai_list_state_ == nposm, null_str);
		VALIDATE(ai_find_user_widget_->get_visible() == twidget::INVISIBLE, null_str);

		if (!ai_disable_find_user_) {
			ai_find_user_widget_->set_visible(twidget::VISIBLE);
		}

		std::vector<titem3> items;
		ai_get_download_courseware_items(items);

		reload_items_list(*timing_task_list_, items, true);
		item_list_stack_->set_radio_layer(TIMING_TASKS_LAYER);
		ai_list_state_ = ai_list_state_local_coursewares;

		set_status_task_label(task_type, _("Select the task you want to execute"));
		
	} else if (task_type == ai_tasktype_aiagent) {
		VALIDATE(ai_curr_timings_.empty(), null_str);
		VALIDATE(ai_curr_aiagent_prompts_.empty(), null_str);

		std::vector<titem3> items;
		get_task_items(false, true, BIT_IDX_MASK(aplt::task_aiagent), ai_curr_timings_, items);

		reload_items_list(*position_list_, items, true);
		set_status_task_label(task_type, _("Choose where to go"));

	} else {
		VALIDATE(false, null_str);
	}
}

void tcenter::ai_get_scene_items(const aplt::tai_slot::tscene* exclude, std::vector<titem3>& items) const
{
	items.clear();

	int at = 0;
	const std::map<std::string, aplt::tai_slot::tscene>& scenes = ai_slot_->scenes();
	for (std::map<std::string, aplt::tai_slot::tscene>::const_iterator it = scenes.begin(); it != scenes.end(); ++ it, at ++) {
		const aplt::tai_slot::tscene& scene = it->second;
		if (exclude != nullptr && scene.id == exclude->id) {
			continue;
		}
		items.push_back(titem3(at, scene.id, scene.name));
	}
}

void tcenter::ai_get_followup_prompt_items(aplt::tai_slot::tfollowup& followup, std::vector<aplt::tai_slot::tprompt>& prompts, std::vector<titem3>& items) const
{
	items.clear();

	prompts = followup.all_prompts();
	for (std::vector<aplt::tai_slot::tprompt>::const_iterator it = prompts.begin(); it != prompts.end(); ++ it) {
		const aplt::tai_slot::tprompt& prompt = *it;
		items.push_back(titem3(items.size(), prompt.question, prompt.tip));
	}
}

void tcenter::ai_get_download_courseware_items(std::vector<titem3>& items)
{
	items.clear();

	std::map<std::string, tcourseware_file>& files = courseware_files_;
	collect_courseware(true, courseware_load_path(), files);

	std::stringstream minor_label_ss;
	for (std::map<std::string, tcourseware_file>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const tcourseware_file& file = it->second;

		minor_label_ss.str("");
		tcourseware_distribution_vals vals = get_distribution_vals(join_courseware_dir(file.dir_name));
		if (vals.valid()) {
			minor_label_ss << vals.username << "  " << utils::format_time_ymdhms(vals.ts); 
		} else {
			minor_label_ss << _("Unknown");
		}

		std::pair<std::string, std::string> pair = utils::split_app_prefix_id(file.dir_name);
		items.push_back(titem3(items.size(), pair.second, minor_label_ss.str()));
	}
}

void tcenter::ai_get_finduser_result_items(const std::vector<net::tcswamp_finduser_result>& result, std::vector<titem3>& items)
{
	items.clear();

	utils::string_map symbols;
	for (std::vector<net::tcswamp_finduser_result>::const_iterator it = result.begin(); it != result.end(); ++ it) {
		const net::tcswamp_finduser_result& user = *it;
		symbols["count"] = str_cast(user.materialcount);
		items.push_back(titem3(items.size(), user.username, vgettext2("Courseware: $count", symbols)));
	}
}

void tcenter::ai_get_cswamp_user_items(const aplt::tcswamp_user& user, std::vector<titem3>& items)
{
	items.clear();

	utils::string_map symbols;
	for (std::vector<aplt::tcswamp_material>::const_iterator it = user.materials.begin(); it != user.materials.end(); ++ it) {
		const aplt::tcswamp_material& material = *it;
		items.push_back(titem3(items.size(), utils::file_stem_name(material.file), utils::format_time_ymdhms(material.time)));
	}
}

void tcenter::ai_get_aiagent_prompt_items(std::vector<titem3>& items)
{
	items.clear();

	const int max_chars = MAX_ITEM_MAJOR_LABEL_CHARS;
	for (std::vector<aplt::taiagent_api::tprompt>::const_iterator it = ai_curr_aiagent_prompts_.begin(); it != ai_curr_aiagent_prompts_.end(); ++ it) {
		const aplt::taiagent_api::tprompt& prompt = *it;
		std::string prompt2 = prompt.ellipsis_at_start? utils::truncate_to_max_chars2_at_start(prompt.prompt, max_chars):
			utils::truncate_to_max_chars2(prompt.prompt, max_chars, true);
		items.push_back(titem3(items.size(), prompt2, prompt.note));;
	}
}

std::string tcenter::ai_get_label_from_list_row_cookie(const aplt::tcourseware& courseware, const tcookie3f& cookie3f) const
{
	const int index = cookie3f.index;
	const int type = cookie3f.type;
	const int field = cookie3f.field;

	if (type == type_global) {
		if (field == field_uuid) {
			return courseware.uuid;

		} else if (field == field_type) {
			return aplt::tcourseware::types[courseware.type].name;

		} else if (field == field_tex_header) {
			return courseware.tex_header;

		} else if (field == field_tex_tail) {
			return courseware.tex_tail;

		} else if (field == field_title) {
			return courseware.title;

		} else if (field == field_content) {
			return courseware.content;

		} else if (field == field_annotation) {
			return courseware.annotation;

		} else if (field == field_analysis) {
			return courseware.analysis;

		} else if (field == field_reference) {
			return courseware.reference;

		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == type_keypoint) {
		VALIDATE(index >= 0 && index < (int)courseware.keypoints.size(), null_str);
		const aplt::tcourseware::tkeypoint& keypoint = courseware.keypoints[index];
		if (field == field_section) {
			return keypoint.section;

		} else if (field == field_name) {
			return keypoint.name;

		} else if (field == field_annotation) {
			return keypoint.annotation;

		} else if (field == field_analysis) {
			return keypoint.analysis;

		} else {
			VALIDATE(false, null_str);
		}

	} else if (type == type_exercise) {
		VALIDATE(index >= 0 && index < (int)courseware.exercises.size(), null_str);
		const aplt::tcourseware::texercise& exercise = courseware.exercises[index];
		if (field == field_section) {
			return exercise.section;

		} else if (field == field_question) {
			return exercise.question;

		} else if (field == field_analysis) {
			return exercise.analysis;

		} else if (field == field_answer) {
			return exercise.answer;

		} else {
			VALIDATE(false, null_str);
		}
	} else {
		VALIDATE(false, null_str);
	}

	VALIDATE(false, null_str);
	return null_str;
}

void tcenter::ai_keynote_update_to_list(tlistbox& list, int index, const aplt::tcourseware::tkeypoint& keypoint)
{
	insert_list_line_keypoint(list, keypoint, index, field_section);
	insert_list_line_keypoint(list, keypoint, index, field_name);
	insert_list_line_keypoint(list, keypoint, index, field_annotation);
	insert_list_line_keypoint(list, keypoint, index, field_analysis);
}

void tcenter::ai_exercise_update_to_list(tlistbox& list, int index, const aplt::tcourseware::texercise& exercise)
{
	std::map<std::string, std::string> data;

	insert_list_line_exercise(list, exercise, index, field_section);
	insert_list_line_exercise(list, exercise, index, field_question);
	insert_list_line_exercise(list, exercise, index, field_analysis);
	insert_list_line_exercise(list, exercise, index, field_answer);
}

void tcenter::ai_courseware_update_to_list(const aplt::tcourseware& courseware)
{
	tlistbox& list = *ai_main_cfg_list_;
	list.clear();
	
	insert_list_line_global(list, courseware, field_uuid);
	insert_list_line_global(list, courseware, field_type);
	insert_list_line_global(list, courseware, field_tex_header);
	insert_list_line_global(list, courseware, field_tex_tail);
	insert_list_line_global(list, courseware, field_title);
	insert_list_line_global(list, courseware, field_content);
	insert_list_line_global(list, courseware, field_annotation);
	insert_list_line_global(list, courseware, field_analysis);
	insert_list_line_global(list, courseware, field_reference);

	int index = 0;
	for (std::vector<aplt::tcourseware::tkeypoint>::const_iterator it = courseware.keypoints.begin(); it != courseware.keypoints.end(); ++ it, index ++) {
		const aplt::tcourseware::tkeypoint& keypoint = *it;
		ai_keynote_update_to_list(list, index, keypoint);
	}

	index = 0;
	for (std::vector<aplt::tcourseware::texercise>::const_iterator it = courseware.exercises.begin(); it != courseware.exercises.end(); ++ it, index ++) {
		const aplt::tcourseware::texercise& exercise = *it;
		ai_exercise_update_to_list(list, index, exercise);
	}

	ai_validate_main_cfg_list_cookie();
}

int tcenter::ai_courseware_2_main_cfg_list_rows(const aplt::tcourseware& courseware) const
{
	const int keypoint_atts = 4;
	const int exercise_atts = 4;
	return global_atts_ + courseware.keypoints.size() * keypoint_atts + courseware.exercises.size() * exercise_atts;
}

void tcenter::ai_validate_main_cfg_list_cookie() const
{
	const aplt::tcourseware& courseware = ai_curr_courseware_;
	tlistbox& list = *ai_main_cfg_list_;

	const int list_rows = list.rows();
	VALIDATE(list_rows == ai_courseware_2_main_cfg_list_rows(courseware), null_str);
	int child_at = global_atts_;

	// startup next states
	int index = 0;
	for (std::vector<aplt::tcourseware::tkeypoint>::const_iterator it = courseware.keypoints.begin(); it != courseware.keypoints.end(); ++ it, index ++) {
		const aplt::tcourseware::tkeypoint& keypoint = *it;

		VALIDATE(list.row_panel(child_at ++).cookie() == tcookie3f(index, type_keypoint, field_section).u64, null_str);
		VALIDATE(list.row_panel(child_at ++).cookie() == tcookie3f(index, type_keypoint, field_name).u64, null_str);
		VALIDATE(list.row_panel(child_at ++).cookie() == tcookie3f(index, type_keypoint, field_annotation).u64, null_str);
		VALIDATE(list.row_panel(child_at ++).cookie() == tcookie3f(index, type_keypoint, field_analysis).u64, null_str);
	}

	index = 0;
	for (std::vector<aplt::tcourseware::texercise>::const_iterator it = courseware.exercises.begin(); it != courseware.exercises.end(); ++ it, index ++) {
		const aplt::tcourseware::texercise& exercise = *it;

		VALIDATE(list.row_panel(child_at ++).cookie() == tcookie3f(index, type_exercise, field_section).u64, null_str);
		VALIDATE(list.row_panel(child_at ++).cookie() == tcookie3f(index, type_exercise, field_question).u64, null_str);
		VALIDATE(list.row_panel(child_at ++).cookie() == tcookie3f(index, type_exercise, field_analysis).u64, null_str);
		VALIDATE(list.row_panel(child_at ++).cookie() == tcookie3f(index, type_exercise, field_answer).u64, null_str);
	}
	VALIDATE(child_at == list_rows, null_str);
}

void tcenter::ai_did_load_task_api_quited(const std::string& err_msg)
{
	// VALIDATE(subtask_state_ == aplt::sts_nposm || subtask_state_ == aplt::sts_idle || subtask_state_ == aplt::sts_preempted, null_str);
	// VALIDATE(!scene_id_.empty(), null_str);
	// const aplt::tbase_scene* curr_scene = cfg_cpp_api_.base_scene_from_id(scene_id_, true);
	// int original_subtask_state = subtask_state_;

	if (!err_msg.empty()) {
		VALIDATE(ai_curr_task3_ == nullptr, null_str);
		VALIDATE(ai_curr_aiagent_task_api_ == nullptr, null_str);

		const library& lib = drivers_.find_by_type(apltsotype_aiagent_task);
		if (lib.get() != nullptr) {
			drivers_.set_special_applet(apltsotype_aiagent_task, nullptr);
		}

		set_status_task_label(ai_curr_task_type_, err_msg);
	}
}

void tcenter::ai_did_list_row_changed(tlistbox& list, ttoggle_panel& row, int layer)
{
	VALIDATE(ai_curr_task_type_ != nposm && ai_curr_task_type_ != ai_tasktype_chat, null_str);

	const int cookie = uint64_2_int(row.cookie());

	utils::string_map symbols;
	std::stringstream ss;
	if (ai_curr_task_type_ == ai_tasktype_followup) {
		if (ai_curr_followup_ == nullptr) {
			VALIDATE(ai_curr_followup_prompts_.empty(), null_str);

			VALIDATE(layer == POSITIONS_LAYER, null_str);

			const std::map<std::string, aplt::tai_slot::tscene>& scenes = ai_slot_->scenes();
			VALIDATE(list.rows() == (int)scenes.size(), null_str);

			std::map<std::string, aplt::tai_slot::tscene>::const_iterator hit_it = scenes.begin();
			if (cookie != 0) {
				std::advance(hit_it, cookie);
			}
			ai_slot_->select_scene(hit_it->second.id);
			ai_set_task_label(hit_it->second.name);

			std::vector<titem3> items;
			ai_curr_followup_ = ai_slot_->curr_followup();
			ai_get_followup_prompt_items(*ai_curr_followup_, ai_curr_followup_prompts_, items);

			reload_items_list(*timing_task_list_, items, true);
			item_list_stack_->set_radio_layer(TIMING_TASKS_LAYER);
			set_status_task_label(ai_curr_task_type_, _("Choose which prompt to send"));

			ai_task_toolbar_stack_->set_radio_layer(TASK_TOOLBAR_FOLLOWUP_LAYER);

		} else {
			VALIDATE(cookie < ai_curr_followup_prompts_.size(), null_str);
			ai_input_->set_label(ai_curr_followup_prompts_[cookie].question);

			// task_toolbar_stack_->set_radio_layer(TASK_TOOLBAR_FOLLOWUP_LAYER);
		}

	} else if (ai_curr_task_type_ == ai_tasktype_courseware) {
		if (ai_list_state_ == ai_list_state_local_coursewares) {
			VALIDATE(layer == TIMING_TASKS_LAYER, null_str);
			VALIDATE(list.rows() == (int)courseware_files_.size(), null_str);

			const tcourseware_file& file = courseware_file_from_at(cookie);
			const std::string cfgfile = join_main_cfg_filename(file.dir_name);
			load_courseware_cfg2(cfgfile, ai_curr_courseware_);

			ai_courseware_update_to_list(ai_curr_courseware_);
			item_list_stack_->set_radio_layer(POSITIONS_LAYER);

			ai_did_into_list_state_main_cfg();

		} else if (ai_list_state_ == ai_list_state_main_cfg) {
			VALIDATE(layer == POSITIONS_LAYER, null_str);
			VALIDATE(list.rows() == ai_courseware_2_main_cfg_list_rows(ai_curr_courseware_), null_str);

			tcookie3f cookie3f(row.cookie());
			const std::string label = ai_get_label_from_list_row_cookie(ai_curr_courseware_, cookie3f);
			ai_input_->set_label(label);

		} else if (ai_list_state_ == ai_list_state_users) {
			VALIDATE(layer == POSITIONS_LAYER, null_str);
			VALIDATE(list.rows() == (int)ai_curr_finduser_result_.size(), null_str);

			const net::tcswamp_finduser_result& user = ai_curr_finduser_result_[cookie];
			if (user.materialcount == 0) {
				return;
			}

			ai_handle_list_state_users_bh(user);

		} else if (ai_list_state_ == ai_list_state_cswamp_coursewares) {
			VALIDATE(layer == TIMING_TASKS_LAYER, null_str);
			VALIDATE(list.rows() == (int)ai_curr_cswamp_user_.materials.size(), null_str);

			const aplt::tcswamp_material& material = ai_curr_cswamp_user_.materials[cookie];
			ai_handle_list_state_cswamp_coursewares_bh(material);
		}

	} else {
		VALIDATE(ai_curr_task_type_ == ai_tasktype_aiagent, null_str);
		if (ai_curr_aiagent_prompts_.empty()) {
			VALIDATE(layer == POSITIONS_LAYER, null_str);

			VALIDATE(list.rows() == (int)ai_curr_timings_.size(), null_str);
			const tcand_task3& task3 = ai_curr_timings_[cookie];

			const library& lib = drivers_.find_by_type(apltsotype_aiagent_task);
			VALIDATE(lib.get() == nullptr, null_str);


			aplt::ttask_api* task_api = nullptr;
			std::string err_msg;
			tauto_destruct_executor destruct_executor(std::bind(&tcenter::ai_did_load_task_api_quited, this, std::ref(err_msg)));
			{
				drivers_.set_special_applet(apltsotype_aiagent_task, &task3.aplt);
				if (lib.get() == nullptr) {
					err_msg = vgettext2("Cannot load applet '$aplt_task' located in *. so.", symbols);
					// set_status_task_label(ai_curr_task_type_, err_msg);
					return;
				}
				trose_library* rose_lib = lib.get();
				void* v_task = nullptr;
				if (rose_lib->create_task_api != nullptr) {
					v_task = rose_lib->create_task_api(const_cast<aplt::tapplet*>(&task3.aplt));
				}
				if (v_task == nullptr) {
					err_msg = _("Not implete 'aplt_create_task_api' method.");
					// set_status_task_label(ai_curr_task_type_, err_msg);
					return;
				}
				task_api = reinterpret_cast<aplt::ttask_api*>(v_task);
				if (task_api->aiagent == nullptr) {
					delete task_api;
					err_msg = _("Not implete aplt::taiagent_api object.");
					return;
				}
			}
			ai_curr_task3_ = &task3;
			ai_curr_aiagent_task_api_ = task_api;
			int subject = ai_start_aiagent_.pair.aplt != nullptr? ai_start_aiagent_.subject: nposm;
			const std::string& header = ai_start_aiagent_.pair.aplt != nullptr? ai_start_aiagent_.header: null_str;
			const std::vector<aplt::taiagent_api::tprompt>& prompts = task_api->aiagent->prompts(task3.cfg_task.id, subject, header);
			// ai_curr_aiagent_prompts_ = prompts;

			for (std::vector<aplt::taiagent_api::tprompt>::const_iterator it = prompts.begin(); it != prompts.end(); ++ it) {
				const aplt::taiagent_api::tprompt& prompt = *it;
				ai_curr_aiagent_prompts_.push_back(aplt::taiagent_api::tprompt(prompt.prompt, prompt.note, prompt.ellipsis_at_start));
			}

			std::vector<titem3> items;
			ai_get_aiagent_prompt_items(items);

			reload_items_list(*timing_task_list_, items, false);
			item_list_stack_->set_radio_layer(TIMING_TASKS_LAYER);

		} else {
			VALIDATE(cookie < ai_curr_aiagent_prompts_.size(), null_str);
			ai_input_->set_label(ai_curr_aiagent_prompts_[cookie].prompt);
		}
	}

}

void tcenter::ai_set_task_type(int type)
{
	VALIDATE(ai_task_types_.count(type) != 0, null_str);
	const tai_task_type& task_type = ai_task_types_.find(type)->second;

	ai_curr_task_type_ = type;
	ai_task_type_widget_->set_label(task_type.name);

	ai_do_new_conversation();
	ai_set_task_label(task_type.note);

	// new_conversation_widget_->set_visible(type == ai_tasktype_chat? twidget::VISIBLE: twidget::HIDDEN);
	ai_new_conversation_widget_->set_active(type == ai_tasktype_chat);

	int toolbar_layer = type == ai_tasktype_chat? TASK_TOOLBAR_CHAT_LAYER: TASK_TOOLBAR_EMPTY_LAYER;
	ai_task_toolbar_stack_->set_radio_layer(toolbar_layer);
}

void tcenter::ai_set_tokens_label()
{
	int tokens = ai_input_tokens_ + ai_output_tokens_;
	char buf[64];
	SDL_snprintf(buf, sizeof(buf), "(%i + %i)", ai_input_tokens_, ai_output_tokens_);
	ai_tokens_widget_->set_label(buf);
}

void tcenter::ai_set_task_label(const std::string& label)
{
	ai_task_widget_->set_label(label);
}

void tcenter::ai_set_send_label(bool show_send)
{
	ai_send_widget_->set_label(show_send? "misc/send.png": "misc/stop.png");
}

bool tcenter::ai_send_label_is_send_png() const
{
	return ai_send_widget_->label() == "misc/send.png";
}

void tcenter::ai_clear_task()
{
	VALIDATE(ai_curr_task_type_ != nposm && ai_curr_task_type_ != ai_tasktype_chat, null_str);
	// curr_task_type_ = nposm;
	ai_curr_task_type_ = ai_tasktype_chat;

	ai_curr_followup_ = nullptr;
	ai_curr_followup_prompts_.clear();
	ai_curr_courseware_.clear();
	ai_curr_finduser_result_.clear();
	ai_curr_cswamp_user_.clear();
	ai_list_state_ = nposm;

	ai_curr_timings_.clear();
	ai_curr_task3_ = nullptr;
	if (ai_curr_aiagent_task_api_ != nullptr) {
		delete ai_curr_aiagent_task_api_;
		ai_curr_aiagent_task_api_ = nullptr;

		const library& lib = drivers_.find_by_type(apltsotype_aiagent_task);
		VALIDATE(lib.get() != nullptr, null_str);
		drivers_.set_special_applet(apltsotype_aiagent_task, nullptr);
	}
	ai_curr_aiagent_prompts_.clear();

	if (pinyin_.is_speaking()) {
		pinyin_.stop_speak2();
	}

	ai_find_user_widget_->set_visible(twidget::INVISIBLE);
	position_list_->clear();
	timing_task_list_->clear();
	item_list_stack_->set_radio_layer(def_item_list_layer_);
	ai_input_->set_label(null_str);
	reset_status();
}

void tcenter::ai_click_new_conversation(tbutton& widget)
{
	ai_do_new_conversation();
}

void tcenter::ai_pre_input(tscroll_text_box& input)
{
	ttext_box& tb = find_widget<ttext_box>(input.content_grid(), "_text_box", false);
	tb.set_SDLK_RETURN_as_text_input(true);
	ai_input_tb_ = &tb;
	window_->keyboard_capture(&tb);

	connect_signal_pre_key_press(tb, std::bind(&tcenter::ai_signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));

	tb.connect_signal<event::SDL_TEXT_INPUT>(
		std::bind(
			&tcenter::ai_enter_inputing
			, this
			, (int)true)
			, event::tdispatcher::front_child);

	tb.connect_signal<event::LEFT_BUTTON_DOWN>(
		std::bind(
			&tcenter::ai_enter_inputing
			, this
			, (int)true)
			, event::tdispatcher::front_child);

	history_->connect_signal<event::LEFT_BUTTON_UP>(
		std::bind(
			&tcenter::ai_enter_inputing
			, this
			, (int)false)
			, event::tdispatcher::front_post_child);

	history_->connect_signal<event::WHEEL_DOWN>(
		std::bind(
			&tcenter::ai_enter_inputing
			, this
			, (int)false)
			, event::tdispatcher::front_post_child);

	history_->connect_signal<event::WHEEL_UP>(
		std::bind(
			&tcenter::ai_enter_inputing
			, this
			, (int)false)
			, event::tdispatcher::front_post_child);
}

void tcenter::ai_signal_handler_sdl_key_down(bool& handled
		, bool& halt
		, const SDL_Keycode key
		, SDL_Keymod modifier
		, const Uint16 unicode)
{
	bool ctrl_keyboard = false;
	if (!ai_no_keyboard_) {
		if (game_config::os == os_ios || game_config::os == os_android) {
			ctrl_keyboard = key == SDLK_NUMLOCKCLEAR;

		} else if (ai_simulate_mobile_ && game_config::os == os_windows) {
			if (ai_inputing_) {
				ctrl_keyboard = key == SDLK_F8;
			}
		}
	}

	if (ctrl_keyboard) {
		int keyboard_height = 0;
		if (game_config::os == os_ios || game_config::os == os_android) {
			keyboard_height = SDL_GetScreenKeyboardHeight();
			if (keyboard_height != 0) {
				keyboard_height -= status_widget_->get_height();
				keyboard_height += 16; // 2
			}

		} else if (key == SDLK_F8) {
			keyboard_height = 101;
		}

		int height2 = keyboard_height;
		if (keyboard_height != 0) {
			
		} else {
			VALIDATE(game_config::os != os_windows, null_str);
		}
		// ai_keyboard_spacer_->set_best_size_1th(nposm, ai_keyboard_spacer_->get_width_is_max(), height2, ai_keyboard_spacer_->get_height_is_max());
	}

	SDL_Log("%u signal_handler_sdl_key_down, key: 0x%x <=>SDLK_RETURN: 0x%x", SDL_GetTicks(), key, SDLK_RETURN);
/*
	if (ai_no_keyboard_) {
		if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && !(modifier & KMOD_SHIFT)) {
			ai_do_send();

			handled = true;
			halt = true;
		}

	} else {
		if (key == SDLK_RETURN) {
			ai_do_send();

			handled = true;
			halt = true;
		}
	}
*/
	if (key == SDLK_ESCAPE) {
		ai_enter_inputing(false);

	} else if (key >= SDLK_SPACE && key < SDL_SCANCODE_TO_KEYCODE(1)) {
		ai_enter_inputing(true);
	}
}

void tcenter::ai_enter_inputing(bool enter)
{
	if ((enter && ai_inputing_) || (!enter && !ai_inputing_)) {
		return;
	}

	const bool input_height_const = true;
	int input_scale_height = nposm;
	int kb_spacer_height = nposm;
	if (enter) {
		if (ai_no_keyboard_) {
			// input_scale_height = 108 * twidget::hdpi_scale;
			
		} else {
			if (!input_height_const) {
				input_scale_height = 30 * twidget::hdpi_scale;
			}
			if (game_config::os == os_windows) {
				// kb_spacer_height = 81;
				kb_spacer_height = 591;
			}
		}
		if (game_config::mobile) {
			// SDL_StartTextInput();
		}

	} else {
		input_scale_height = def_input_scale_height_;
		if (ai_no_keyboard_) {
			
		} else {
			if (game_config::os == os_windows) {
				kb_spacer_height = 0;
			}
		}

		if (game_config::mobile) {
			SDL_StopTextInput();
		}
	}

	ai_inputing_ = enter;

	if (input_scale_height != nposm) {
		ai_input_scale_->set_best_size_1th(nposm, ai_input_scale_->get_width_is_max(), input_scale_height, ai_input_scale_->get_height_is_max());
	}

	if (kb_spacer_height != nposm) {
		// ai_keyboard_spacer_->set_best_size_1th(nposm, ai_keyboard_spacer_->get_width_is_max(), kb_spacer_height, ai_keyboard_spacer_->get_height_is_max());
	}
	// history_->invalidate_layout(nullptr);
}

static bool is_blank_str(const std::string& str)
{
	const char* blank_char = " \n";
	size_t pos = str.find_first_not_of(blank_char);
	return pos == std::string::npos;
}

void tcenter::ai_do_send()
{
	if (ai_driver_.slot == nullptr) {
		gui2::show_message(null_str, _("Can't chat, need to set up the aiagent driver first"));
		return;
	}

	twindow& window = *window_;

	// if (!slot_.chat_can_send()) {
	//	return;
	// }


	std::string input_str = ai_input_->label();
	if (is_blank_str(input_str)) {
		return;
	}

	if (!ai_no_keyboard_) {
		ai_enter_inputing(false);
	}

	surface surf;
	// surface surf = image::get_image("c:/ddksample/images/3-15.jpg");
	// VALIDATE(surf.get() != nullptr, null_str);

	const bool new_conversation_always = ai_curr_task_type_ == ai_tasktype_chat && preferences::new_conversation_always();
	if (new_conversation_always) {
		ai_new_conversation_ = true;
	}

	if (ai_curr_task_type_ == ai_tasktype_aiagent) {
		VALIDATE(ai_curr_task3_ != nullptr, null_str);
		ai_driver_.start_aiagent_task(ai_curr_aiagent_task_api_, ai_curr_task3_->aplt, ai_curr_task3_->cfg_task, input_str, nullptr);

	} else {
		ai_driver_.send_nlp_question(aplt::chatsrc_ai_chat, ai_new_conversation_, input_str, surf);
	}

	ai_input_->set_label(null_str);

	if (ai_curr_task_type_ != ai_tasktype_aiagent) {
		uint64_t tokens = aplt::join_log_tokens(0, 0, ai_new_conversation_? aplt::tokensflag_new_conversation: 0);
		instance->add_msg_only_log(logtype_center_chat, input_str, tokens, true);
	}

	if (!new_conversation_always) {
		ai_new_conversation_ = false;
	}

	ai_set_send_label(false);
}

void tcenter::ai_click_send(tbutton& widget)
{
	if (ai_start_aiagent_.pair.aplt != nullptr) {
		gui2::show_message(null_str, _("center don't support send msg"));
		return;
	}

	bool is_ing = false;
	if (ai_curr_task_type_ == ai_tasktype_aiagent) {
		if (ai_driver_.is_aiagent_tasking()) {
			is_ing = true;
			ai_driver_.stop_aiagent_task();
		}

	} else {
		if (ai_driver_.is_nlp_questioning()) {
			is_ing = true;
			ai_driver_.stop_nlp_question();
		}
	}

	if (is_ing) {
		ai_set_send_label(true);
	} else {
		ai_do_send();
	}
}

void tcenter::ai_pre_task_toolbar_empty(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "help", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcenter::ai_click_help
			, this, std::ref(*button), help_clipboard_to_exam));

	button = find_widget<tbutton>(&grid, "clipboard_to_exam", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcenter::ai_click_task_toolbar_clipboard_to_exam
			, this, std::ref(*button)));

	if (ai_start_aiagent_.pair.aplt == nullptr) {
		find_widget<tgrid>(&grid, "start_aiagent_grid", false, true)->set_visible(twidget::INVISIBLE);
	}
}

void tcenter::ai_pre_task_toolbar_chat(tgrid& grid)
{
	ttoggle_button* toggle = find_widget<ttoggle_button>(&grid, "new_conversation_always", false, true);
	toggle->set_did_state_changed(std::bind(&tcenter::ai_did_new_conversation_always_changed, this, _1));
	toggle->set_value(preferences::new_conversation_always());
}

void tcenter::ai_pre_task_toolbar_followup(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "reference", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcenter::ai_click_task_toolbar_reference
			, this, std::ref(*button), TASK_TOOLBAR_FOLLOWUP_LAYER));
}

void tcenter::ai_pre_task_toolbar_courseware_main_cfg(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "reference", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcenter::ai_click_task_toolbar_reference
			, this, std::ref(*button), TASK_TOOLBAR_COURSEWARE_MAIN_CFG_LAYER));

	button = find_widget<tbutton>(&grid, "listen", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tcenter::ai_click_task_toolbar_listen
			, this, std::ref(*button), TASK_TOOLBAR_COURSEWARE_MAIN_CFG_LAYER));
	ai_courseware_main_cfg_listen_widget_ = button;
}

void tcenter::ai_click_help(tbutton& widget, int type)
{
	VALIDATE(type == help_clipboard_to_exam, null_str);

	utils::string_map symbols;
	std::string title;
	std::string msg;
	if (type == help_clipboard_to_exam) {
		title = _("How to Add Questions via DeepSeek Web Version");
		symbols["add_from_clipboard"] = _("Add from clipboard");
		msg = vgettext2("clipboard to exam help, $add_from_clipboard", symbols);
	}
	gui2::show_message(title, msg);
}

void tcenter::ai_click_task_toolbar_clipboard_to_exam(tbutton& widget)
{
	VALIDATE(ai_start_aiagent_.pair.aplt != nullptr, null_str);
	VALIDATE(ai_start_aiagent_.did_handle_text != NULL, null_str);

	char* text = SDL_GetClipboardText();
	if (text == nullptr) {
		const std::string msg = _("The clipboard is empty.\n\n"
			"Step 1: Click the 'Copy' button in the DeepSeek answer code block.\n"
			"Step 2: Return here to proceed.");
		gui2::show_message(null_str, msg);
		return;
	}

	SDL_Point3 added = ai_start_aiagent_.did_handle_text(text, SDL_strlen(text));
	// write_file(game_config::preferences_dir + "/saves/clipboard.data", text, SDL_strlen(text));
	SDL_free(text);

	if (added.x != 0 || added.y != 0 || added.z != 0) {
		utils::string_map symbols;
		symbols["choices"] = str_cast(added.x);
		symbols["fillins"] = str_cast(added.y);
		symbols["solvings"] = str_cast(added.z);
		const std::string msg = vgettext2("$choices choice, $fillins fillin, and $solvings solving questions have been appended to the exam.", symbols);

		const std::string ok_str = _("Stay here");
		const std::string cancel_str = _("Return to courseware");

		const int res = gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons, null_str, null_str, ok_str, cancel_str);
		if (res != gui2::twindow::OK) {
			window_->set_retval(twindow::CANCEL);
			return;
		}
	} else {
		gui2::show_message(null_str, _("Unable to add questions from clipboard: invalid data format."));
	}
}

void tcenter::ai_did_new_conversation_always_changed(ttoggle_button& widget)
{
	if (widget.get_value()) {
		// disable --> enable

	} else {
		// enable --> disable
	}
	preferences::set_new_conversation_always(widget.get_value());
}

void tcenter::ai_click_task_toolbar_reference(tbutton& widget, int layer)
{
	std::string msg;
	if (layer == TASK_TOOLBAR_FOLLOWUP_LAYER) {
		VALIDATE(ai_curr_task_type_ == ai_tasktype_followup, null_str);
		VALIDATE(ai_curr_followup_ != nullptr, null_str);

		msg = ai_curr_followup_->reference();

	} else if (layer == TASK_TOOLBAR_COURSEWARE_MAIN_CFG_LAYER) {
		VALIDATE(ai_curr_task_type_ == ai_tasktype_courseware, null_str);
		msg = ai_curr_courseware_.reference;

	} else {
		VALIDATE(false, null_str);
	}

	gui2::show_message(null_str, msg);
}

void tcenter::ai_click_task_toolbar_listen(tbutton& widget, int layer)
{
	if (layer == TASK_TOOLBAR_COURSEWARE_MAIN_CFG_LAYER) {
		VALIDATE(ai_curr_task_type_ == ai_tasktype_courseware, null_str);

		std::string listen_png;
		if (!pinyin_.is_speaking()) {
			const std::string text = ai_curr_courseware_.text_for_listen();
			chinese::curr_pinyin.speak(text);

			listen_png = "misc/stop_listen.png";

		} else {
			pinyin_.stop_speak2();
			listen_png = "misc/listen.png";
		}
		ai_courseware_main_cfg_listen_widget_->set_label(listen_png);

	} else {
		VALIDATE(false, null_str);
	}
}

void tcenter::ai_did_find_user_text_box_changed(ttext_box& widget)
{
	VALIDATE(ai_curr_task_type_ == ai_tasktype_courseware, null_str);
}

void tcenter::ai_click_find_user(tbutton& widget)
{
	VALIDATE(ai_curr_task_type_ == ai_tasktype_courseware, null_str);
	const std::string& name = ai_find_user_widget_->text_box()->label();

	std::vector<net::tcswamp_finduser_result>& result = ai_curr_finduser_result_;
	{
		gui2::tprogress_default_slot slot(std::bind(&net::cswamp_finduser, _1, nposm, name,
			false, std::ref(result)));
		bool ret = gui2::run_with_progress(slot, null_str, _("Find user"), 1000);
		if (!ret) {
			return;
		}
	}

	std::vector<titem3> items;
	ai_get_finduser_result_items(result, items);

	reload_items_list(*position_list_, items, true);
	item_list_stack_->set_radio_layer(POSITIONS_LAYER);

	ai_list_state_ = ai_list_state_users;
	ai_task_toolbar_stack_->set_radio_layer(TASK_TOOLBAR_EMPTY_LAYER);

	if (pinyin_.is_speaking()) {
		pinyin_.stop_speak2();
	}
}

void tcenter::ai_handle_list_state_users_bh(const net::tcswamp_finduser_result& user)
{
	VALIDATE(ai_curr_task_type_ == ai_tasktype_courseware, null_str);
	VALIDATE(ai_list_state_ == ai_list_state_users, null_str);
	VALIDATE(user.materialcount > 0, null_str);

	aplt::tcswamp_user& cswamp_user = ai_curr_cswamp_user_;
	{
		gui2::tprogress_default_slot slot(std::bind(&net::cswamp_getuserinfo, _1, user.uid,
			false, std::ref(cswamp_user)));
		bool ret = gui2::run_with_progress(slot, null_str, _("Get user info"), 1000);
		if (!ret) {
			return;
		}
	}

	std::vector<titem3> items;
	ai_get_cswamp_user_items(cswamp_user, items);
	reload_items_list(*timing_task_list_, items, true);
	item_list_stack_->set_radio_layer(TIMING_TASKS_LAYER);
	ai_list_state_ = ai_list_state_cswamp_coursewares;
}

void tcenter::ai_write_distribution_cfg(const std::string& path, const aplt::tcswamp_user& user, const aplt::tcswamp_material& material) const
{
	VALIDATE(user.valid(), null_str);
	VALIDATE(material.valid(), null_str);
	
	config cfg;
	cfg["uid"].from_int64(user.uid);
	cfg["username"].from_string(user.username, true);
	cfg["ts"].from_int64(material.time);

	std::stringstream out;
	if (!cfg.empty()) {
		write(out, cfg);
	}
	VALIDATE(!out.str().empty(), null_str);

	write_file(path + "/" + APLT_DISTRIBUTION_CFG, out.str().c_str(), out.str().size());
}

void tcenter::ai_handle_list_state_cswamp_coursewares_bh(const aplt::tcswamp_material& material)
{
	VALIDATE(ai_curr_task_type_ == ai_tasktype_courseware, null_str);
	VALIDATE(ai_list_state_ == ai_list_state_cswamp_coursewares, null_str);
	VALIDATE(ai_curr_cswamp_user_.valid(), null_str);

	const aplt::tcswamp_user& user = ai_curr_cswamp_user_;

	const std::string courseware_name = utils::file_stem_name(material.file);
	const std::string courseware_dir_name = utils::join_app_prefix_id(str_cast(user.uid), courseware_name);
	const std::string courseware_dir = join_courseware_dir(courseware_dir_name);

	tcourseware_distribution_vals local = get_distribution_vals(courseware_dir);

	const std::string cfgfile = join_main_cfg_filename(courseware_dir_name);

	bool require_donwload = local.uid != user.uid || local.ts != material.time;
	if (!require_donwload) {
		load_courseware_cfg2(cfgfile, ai_curr_courseware_);
		require_donwload = !ai_curr_courseware_.valid();
	}

	if (require_donwload) {
		const std::string remote_src = material.file;
		const std::string courseware_rsp = download_path_ + "/" + material.file;
		bool ret = net::download_materialrsp(remote_src, null_str, user.uid, courseware_rsp);
		if (!ret) {
			return;
		}

		// 1/5: delete local dir (<uid>__<courseware_name>)
		SDL_DeleteFiles(courseware_dir.c_str());

		// 2/5: unzip *.rsp to <courseware_name>
		minizip::unzip_file(courseware_rsp, download_path_.c_str(), null_str, null_str);

		// 3/5: delete *.rsp
		SDL_DeleteFiles(courseware_rsp.c_str());
 
		// 4/5: raname <courseware_name> to (<uid>__<courseware_name>)
		SDL_RenameFile(join_courseware_dir(courseware_name).c_str(), courseware_dir_name.c_str());

		// 5/5: generate <courseware_dir>/distribution.cfg
		ai_write_distribution_cfg(courseware_dir, user, material);

		load_courseware_cfg2(cfgfile, ai_curr_courseware_);

	} else if (local.username != user.username) {
		ai_write_distribution_cfg(courseware_dir, user, material);
	}

	ai_courseware_update_to_list(ai_curr_courseware_);
	item_list_stack_->set_radio_layer(POSITIONS_LAYER);

	ai_did_into_list_state_main_cfg();
}

void tcenter::ai_did_into_list_state_main_cfg()
{
	ai_list_state_ = ai_list_state_main_cfg;
	ai_task_toolbar_stack_->set_radio_layer(TASK_TOOLBAR_COURSEWARE_MAIN_CFG_LAYER);

	ai_courseware_main_cfg_listen_widget_->set_label("misc/listen.png");
}

const tcenter::tai_task_type& tcenter::ai_task_from_type(int type) const
{
	VALIDATE(ai_task_types_.count(type) != 0, null_str);
	return ai_task_types_.find(type)->second;
}

//
// non-ai layer
//
void tcenter::nonai_reload_tasks()
{
	std::vector<int> types;
	types.push_back(task_type_moveto);
	types.push_back(task_type_aplt_task_no_position);
	types.push_back(task_type_aplt_task);
	types.push_back(task_type_aplt_task_2position);
	types.push_back(task_type_charge2);

	treport* report = find_widget<treport>(window_, "task_report", false, true);
	report->clear();
	for (std::vector<int>::const_iterator it = types.begin(); it != types.end(); ++ it) {
		int type = *it;
		const ttemp_task_type& task = game_config::temp_task_types.find(type)->second;

		tcontrol& widget = report->insert_item(null_str, task.name);

		widget.set_icon(std::string("misc/") + task.id + ".png");
		widget.set_cookie(task.type);
	}
}

static std::string position_xy_theta_str(const tmap_position& position)
{
	char buf[256];
	if (!is_float_nposm(position.theta)) {
		SDL_snprintf(buf, sizeof(buf), "(%.2f, %.2f, %.3f)", position.x, position.y, round(RAD2DEG(position.theta)));
	} else {
		std::string unrestricted_msg = _("unrestricted");
		SDL_snprintf(buf, sizeof(buf), "(%.2f, %.2f, %s)", position.x, position.y, unrestricted_msg.c_str());
	}
	return buf;
}

static bool req_task_require_position(int type)
{
	return type == task_type_moveto || 
		type == task_type_aplt_task || 
		type == task_type_aplt_task_2position;
}

static bool req_task_require_move(int type)
{ 
	return req_task_require_position(type) || type == task_type_charge2; 
}

void tcenter::nonai_did_tasks_item_click(tbutton& widget)
{
	if (nonai_task_clearing_) {
		return;
	}

	if (nonai_curr_task_type_ != nposm) {
		nonai_clear_task();
	}

	int task_type = uint64_2_int(widget.cookie());

	if (req_task_require_move(task_type)) {
		if (!base_driver_.node_started()) {
			const std::string msg = base_node_error_str(base_driver_, game_config::temp_task_types.find(task_type)->second.name);
			gui2::show_message(null_str, msg);
			return;
		}
	}

	utils::string_map symbols;

	if (task_type == task_type_moveto) {
		std::vector<titem3> items;
		nonai_get_position_items(nullptr, items);

		reload_items_list(*position_list_, items, true);
		nonai_curr_task_type_ = task_type;
		set_status_task_label(task_type, _("Choose where to go"));
		
	} else if (task_type == task_type_aplt_task_no_position) {
		VALIDATE(nonai_curr_timings_.empty(), null_str);

		std::vector<titem3> items;
		get_task_items(true, false, nposm, nonai_curr_timings_, items);

		reload_items_list(*timing_task_list_, items, true);
		item_list_stack_->set_radio_layer(TIMING_TASKS_LAYER);

		nonai_curr_task_type_ = task_type;
		set_status_task_label(task_type, _("Select the task you want to execute"));
		
	} else if (task_type == task_type_aplt_task || task_type == task_type_aplt_task_2position) {
		std::vector<titem3> items;
		nonai_get_position_items(nullptr, items);

		reload_items_list(*position_list_, items, true);
		nonai_curr_task_type_ = task_type;
		set_status_task_label(task_type, _("Choose where to go"));

	} else if (task_type == task_type_charge2) {

		if (!base_driver_.installed()) {
			return;
		}

		symbols["action"] = _("Charge");
		if (!base_driver_.slot->moveable()) {
			gui2::show_message(null_str, vgettext2("Base works in unmovable mode, cannot $action.", symbols));
			return;
		}

		const std::string msg = vgettext2("Do you want robot to perform $action?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}

		
		// ros_.request_task(req_task_);

		aplt::ttask_pair pair;
		if (!ros_instance_.check_charge_env(true, &pair, nullptr)) {
			return;
		}
		const std::string device_id;
		nonai_req_task_.set_aplt_task(*pair.aplt, *pair.task, device_id, charge_pos_uuid, null_str);
		b_api_.request_task(nonai_req_task_);

	} else {
		VALIDATE(false, null_str);
	}
}

void tcenter::nonai_get_position_items(const tmap_position* exclude, std::vector<titem3>& items) const
{
	items.clear();

	int at = 0;
	for (std::map<std::string, tmap_position>::const_iterator it = curmap_.positions.begin(); it != curmap_.positions.end(); ++ it, at ++) {
		const tmap_position& position = it->second;
		if (exclude != nullptr && position.uuid == exclude->uuid) {
			continue;
		}
		items.push_back(titem3(at, position.name, position_xy_theta_str(position)));
	}
}

bool tcenter::nonai_can_clear_task() const
{
	VALIDATE(nonai_curr_task_type_ != nposm, null_str);
	if (nonai_curr_task_type_ == task_type_aplt_task) {
		return nonai_curr_position_ == nullptr;

	} else if (nonai_curr_task_type_ == task_type_aplt_task_2position) {
		return nonai_curr_position_ == nullptr && nonai_curr_timing_ == nullptr;
	}

	return true;
}

void tcenter::nonai_did_list_row_changed(tlistbox& list, ttoggle_panel& row, int layer)
{
	if (nonai_task_clearing_) {
		return;
	}

	VALIDATE(nonai_curr_task_type_ != nposm, null_str);

	const int cookie = uint64_2_int(row.cookie());

	utils::string_map symbols;
	std::stringstream ss;
	if (req_task_require_position(nonai_curr_task_type_) && nonai_curr_position_ == nullptr) {
		VALIDATE(layer == POSITIONS_LAYER, null_str);
		std::map<std::string, tmap_position>::const_iterator hit_it = curmap_.positions.begin();
		if (cookie != 0) {
			std::advance(hit_it, cookie);
		}
		const tmap_position& position = hit_it->second;

		if (nonai_curr_task_type_ == task_type_moveto) {
			aplt::ttask_pair pair = aplt::task_pair_for_move(applets_);
			nonai_req_task_.set_aplt_task(*pair.aplt, *pair.task, null_str, position.uuid, null_str);
			b_api_.request_task(nonai_req_task_);

		} else if (nonai_curr_task_type_ == task_type_aplt_task || nonai_curr_task_type_ == task_type_aplt_task_2position) {
			nonai_curr_position_ = &position;
			VALIDATE(nonai_curr_timings_.empty(), null_str);

			std::vector<titem3> items;
			get_task_items(false, false, nposm, nonai_curr_timings_, items);

			symbols["position"] = nonai_curr_position_->name;
			set_status_task_label(nonai_curr_task_type_, vgettext2("Move to $position. Select the task you want to execute", symbols));

			reload_items_list(*timing_task_list_, items, true);
			item_list_stack_->set_radio_layer(TIMING_TASKS_LAYER);

		} else {
			VALIDATE(false, null_str);
		}

	} else if (nonai_curr_task_type_ == task_type_aplt_task_no_position) {
		VALIDATE(nonai_curr_position_ == nullptr, null_str);
		VALIDATE(nonai_curr_timing_ == nullptr, null_str);

		VALIDATE(layer == TIMING_TASKS_LAYER, null_str);
		VALIDATE(list.rows() == (int)nonai_curr_timings_.size(), null_str);

		const tcand_task3& timing = nonai_curr_timings_[cookie];
		nonai_req_task_.set_aplt_task(timing.aplt, timing.cfg_task, timing.ble_device_id, null_str, null_str);
		b_api_.request_task(nonai_req_task_);

	} else if (nonai_curr_task_type_ == task_type_aplt_task) {
		VALIDATE(nonai_curr_position_ != nullptr, null_str);
		VALIDATE(nonai_curr_timing_ == nullptr, null_str);

		VALIDATE(layer == TIMING_TASKS_LAYER, null_str);
		VALIDATE(list.rows() == (int)nonai_curr_timings_.size(), null_str);

		const tcand_task3& timing = nonai_curr_timings_[cookie];
		nonai_req_task_.set_aplt_task(timing.aplt, timing.cfg_task, timing.ble_device_id, nonai_curr_position_->uuid, null_str);
		b_api_.request_task(nonai_req_task_);
		nonai_curr_position_ = nullptr;

	} else {
		VALIDATE(nonai_curr_task_type_ == task_type_aplt_task_2position, null_str);
		VALIDATE(nonai_curr_position_ != nullptr, null_str);

		if (nonai_curr_timing_ == nullptr) {
			// [2/4]selected task
			nonai_curr_timing_ = &nonai_curr_timings_[cookie];

			std::vector<titem3> items;
			nonai_get_position_items(nonai_curr_position_, items);

			reload_items_list(*position_list_, items, true);
			item_list_stack_->set_radio_layer(POSITIONS_LAYER);
			set_status_task_label(nonai_curr_task_type_, _("Choose where to be back"));

		} else {
			// [3/4]selected posisition2
			VALIDATE(layer == POSITIONS_LAYER, null_str);
			std::map<std::string, tmap_position>::const_iterator hit_it = curmap_.positions.begin();
			if (cookie != 0) {
				std::advance(hit_it, cookie);
			}
			const tmap_position& position2 = hit_it->second;
			VALIDATE(position2.uuid != nonai_curr_position_->uuid, null_str);

			// [4/4]request task
			nonai_req_task_.set_aplt_task(nonai_curr_timing_->aplt, nonai_curr_timing_->cfg_task, nonai_curr_timing_->ble_device_id, 
				nonai_curr_position_->uuid, position2.uuid);
			b_api_.request_task(nonai_req_task_);
			nonai_curr_position_ = nullptr;
			nonai_curr_timing_ = nullptr;
		}

	}

	if (nonai_can_clear_task()) {
		// tlistbox: Must not insert/erase row when did_row_changed_
		tmsg_data_nonai_clear_task* pdata = new tmsg_data_nonai_clear_task(*this);
		rtc::Thread::Current()->Post(RTC_FROM_HERE, this, MSG_NONAI_CLEAR_TASK, pdata);
	}
}

void tcenter::nonai_clear_task()
{
	VALIDATE(nonai_curr_task_type_ != nposm, null_str);
	nonai_curr_task_type_ = nposm;
	nonai_curr_position_ = nullptr;
	nonai_curr_timing_ = nullptr;
	nonai_curr_timings_.clear();

	position_list_->clear();
	timing_task_list_->clear();
	item_list_stack_->set_radio_layer(def_item_list_layer_);
	reset_status();
}

void tcenter::reload_log_list(tlistbox& list)
{
	bool aiagent_dlg = curr_layer_ == AI_LAYER;
	aiagent_dlg = false;
	instance->logs_reload_log_list(list, aiagent_dlg);
}

bool can_switch_to_camera_layer(const std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	return sys_task.in_which_single_task(applets) == aplt::task_camera;
}

bool tcenter::camera_is_using() const
{
	if (bg_task_.is_ing()) {
		return can_switch_to_camera_layer(applets_, bg_task_.bg_task2());

	}
	return base_driver_.subtask_state() == aplt::sts_ing;
}

tcamera::tslot& tcenter::get_camera_slot()
{
	tcamera::tslot* slot = &temp_task_;
	if (base_driver_.subtask_state() == aplt::sts_ing) {
		slot = &base_driver_;
	}
	return *slot;
}

void tcenter::camera_layer_back_home()
{
	VALIDATE(!vrenderer_widget_->is_timer_enable(), null_str);

	did_camera_layer_back_home();
}

void tcenter::bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	if (nonai_curr_layer_ == NONAI_CAMERA_LAYER) {
		if (base_driver_.subtask_state() == aplt::sts_ing) {
			camera_pre_exit_task_c();
		}
		camera_layer_back_home();
	}

	if (can_switch_to_camera_layer(applets_, sys_task)) {
		if (curr_layer_ == NONAI_LAYER) {
			camera_widget_->set_visible(twidget::VISIBLE);
		}
	}
}

void tcenter::bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	if (can_switch_to_camera_layer(applets_, sys_task)) {
		if (nonai_curr_layer_ == NONAI_CAMERA_LAYER) {
			// timer is diable during camera_.exit_task(..)
			if (base_driver_.subtask_state() == aplt::sts_ing) {
				camera_pre_exit_task_c();

			} else {
				VALIDATE(!vrenderer_widget_->is_timer_enable(), null_str);
			}

			// tcamera::tslot& slot = get_camera_slot();
			// during both bg_task_will_start and bg_task_stopped, 
			// calling get_camera_slot() always returns base_driver_'s slot.
			
			// tcamera::tslot* slot = &temp_task_;
			// slot->set_center_dlg(nullptr);

			// VALIDATE(base_driver_.center_dlg() == nullptr, null_str);

			did_camera_layer_back_home();
		}
		if (base_driver_.scene_id().empty()) {
			if (curr_layer_ == NONAI_LAYER) {
				camera_widget_->set_visible(twidget::HIDDEN);
			}
		}
	}
}

void tcenter::logs_pb_log_added(int count, const pb2::tlog& log)
{
	bool aiagent_dlg = curr_layer_ == AI_LAYER;
	aiagent_dlg = false;
	instance->logs_pb_log_added2(*history_, count, log, aiagent_dlg);
}

std::string tcenter::can_send_nlp_question2(int src) const
{
	VALIDATE(ai_curr_task_type_ != nposm, null_str);

	if (ai_curr_task_type_ != ai_tasktype_chat) {
		utils::string_map symbols;
		symbols["aiagent"] = aplt::all_fake_applets.find(aplt::builtinid_center)->second.name;
		symbols["task_type"] = ai_task_types_.find(ai_tasktype_chat)->second.name;
		std::string err_msg = vgettext2("During the $aiagent window, voice chat is only available in the '$task_type'.", symbols);
		SDL_Log("err_msg: %s", err_msg.c_str());
		return err_msg;
	}
	return null_str;
}

void tcenter::aiagent_did_nlp_answer2(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens)
{
	if (ai_new_conversation_) {
		ai_input_tokens_ = 0;
		ai_output_tokens_ = 0;

	} else {
		ai_input_tokens_ += input_tokens;
		ai_output_tokens_ += output_tokens;
	}

	ai_set_tokens_label();
}

const ttemp_task_type& tcenter::nonai_task_from_type(int type) const
{
	VALIDATE(game_config::temp_task_types.count(type) != 0, null_str);
	return game_config::temp_task_types.find(type)->second;
}

void tcenter::reset_status()
{
	std::string msg;
	if (speech_driver_.slot == nullptr) {
		msg = _("Speech drive hasn't been set up yet");

	} else if (!chinese::curr_pinyin.rsp.valid()) {
		msg = _("The pinyin package has not been installed yet");

	} else {
		utils::string_map symbols;
		symbols["days"] = str_cast(LOGS_PB_MAX_DAYS);
		symbols["max_logs"] = str_cast(LOGS_PB_MAX_LOGS);
		msg = vgettext2("A maximum of $days days of logs can be stored, and no more than $max_logs logs can be stored", symbols);
	}

	status_widget_->set_label(msg);
}

void tcenter::set_status_task_label(int type, const std::string& desc)
{
	char buf[1024];
	if (curr_layer_ == AI_LAYER) {
		const tai_task_type& task = ai_task_from_type(type);
		SDL_snprintf(buf, sizeof(buf), "{%s}%s", task.name.c_str(), desc.c_str());

	} else {
		const ttemp_task_type& task = nonai_task_from_type(type);
		SDL_snprintf(buf, sizeof(buf), "{%s}%s", task.name.c_str(), desc.c_str());
	}
	status_widget_->set_label(buf);
}

void tcenter::did_camera_layer_back_home()
{
	VALIDATE(nonai_curr_layer_ == NONAI_CAMERA_LAYER, null_str);

	nonai_curr_layer_ = NONAI_HOME_LAYER;
	body_stack_->set_radio_layer(nonai_curr_layer_);

	base_driver_.set_camera_viewer(nullptr);
	temp_task_.set_camera_viewer(nullptr);
}

void tcenter::did_draw_paper(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();
	
	if (camera_.is_avcapture_started()) {
		camera_.slice(widget_rect, true);
	}
}

void tcenter::did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(last)) {
		return;
	}

	camera_.did_mouse_leave_paper(widget, first, last);
}

void tcenter::camera_post_enter_task_c()
{
	vrenderer_widget_->set_timer_interval(30);
}

void tcenter::camera_pre_exit_task_c()
{
	VALIDATE(vrenderer_widget_->is_timer_enable(), null_str);
	vrenderer_widget_->set_timer_interval(0);
}

void tcenter::camera_did_draw_slice_c(int id, SDL_Renderer* renderer, trtc_client::VideoRenderer** locals, int locals_count, trtc_client::VideoRenderer** remotes, int remotes_count, const SDL_Rect& draw_rect)
{
	camera_.camera_did_draw_slice_use_cv_frame(id, renderer, locals, locals_count, remotes, remotes_count, draw_rect);
}

void tcenter::camera_did_button_clicked_c(int msgid)
{
	if (msgid == POST_MSG_CANCEL_CAMERA) {
		camera_pre_exit_task_c();
		camera_layer_back_home();
	}
}

void tcenter::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);

	if (ai_slot_ != nullptr) {
		// if switch, it maybe in non-ai layer.
		bool is_ing = ai_driver_.is_aiagent_tasking();
		if (!is_ing) {
			is_ing = ai_slot_->is_nlp_questioning();
		}
		if (is_ing) {
			if (ai_send_label_is_send_png()) {
				ai_set_send_label(false);
			}
		} else {
			if (!ai_send_label_is_send_png()) {
				ai_set_send_label(true);
			}
		}
	}

	if (ai_list_state_ == ai_list_state_main_cfg && !pinyin_.is_speaking()) {
		const std::string listen_png = "misc/listen.png";
		if (ai_courseware_main_cfg_listen_widget_->label() != listen_png) {
			ai_courseware_main_cfg_listen_widget_->set_label(listen_png);
		}
	}
}

void tcenter::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case POST_MSG_LOGS_SCROLL_TO_BOTTOM:
		{
			VALIDATE(false, null_str);
			tmsg_data_listbox_scroll_to_bottom* pdata = static_cast<tmsg_data_listbox_scroll_to_bottom*>(msg->pdata);
			if (pdata->listbox.rows() > 0) {
				pdata->listbox.scroll_to_row2(pdata->listbox.rows() - 1);
			}
		}
		break;

	case MSG_NONAI_CLEAR_TASK:
		{
			tmsg_data_nonai_clear_task* pdata = static_cast<tmsg_data_nonai_clear_task*>(msg->pdata);
			nonai_clear_task();
		}
		break;

	default:
		VALIDATE(false, null_str);
	}

	if (msg->pdata != nullptr) {
		delete msg->pdata;
	}
}

} // namespace gui2

