#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/klink.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/time_setter.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/if_block2.hpp"
#include "gui/dialogs/combo_box2.hpp"
#include "gui/dialogs/var_editor.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/listbox.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "base_instance.hpp"
#include "serialization/parser.hpp"
#include "game_config.hpp"

// using namespace std::placeholders;

namespace aplt {

extern void iot_device_to_cfg(const aplt::tiot_device& device, config& cfg);

}

namespace gui2 {

REGISTER_DIALOG(launcher, klink)

tklink::tklink(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, trvar_editor::tslot& var_editor_slot,
	std::map<aplt::taplt_key, aplt::tapplet>& applets, const tros_map& curmap, aplt::tcfg_cpp_api& cfg_cpp_api,
	aplt::tbg_task& bg_task, tbase_driver& base_driver, tmoveit_driver& moveit_driver)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, thelper_klink(var_editor_slot, applets, cfg_cpp_api, bg_task, base_driver)
	// , applets_(applets)
	, curmap_(curmap)
	// , cfg_cpp_api_(cfg_cpp_api)
	// , bg_task_(bg_task)
	// , base_driver_(base_driver)
	, moveit_driver_(moveit_driver)
	, online_threshold_minute_(1) // 10 minutes
	// , title_widget_(nullptr)
	, report_(nullptr)
	, stack_(nullptr)
	, insert_event_widget_(nullptr)
	, insert_speech_widget_(nullptr)
	, insert_var_widget_(nullptr)
	, insert_timed_widget_(nullptr)
	// , import_widget_(nullptr)
	// , export_widget_(nullptr)
	, insert_alias_widget_(nullptr)
	// , insert_scene_widget_(nullptr)
	// , refresh_widget_(nullptr)
	, clear_widget_(nullptr)
	, task_list_(nullptr)
	, device_list_(nullptr)
	// , scene_list_(nullptr)
	, var_sensor_list_(nullptr)
	// , env_var_list_(nullptr)
	, add_timed_task_list_(nullptr)
	, current_layer_(nposm)
{
	set_timer_interval(1000);
}

void tklink::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());
	thelper_klink::pre_show(*window_);
/*
	tbutton* button = find_widget<tbutton>(window_, "title", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tklink::click_title
			, this, std::ref(*button)));
	title_widget_ = button;
	set_title_label();
*/
	tbutton* button = find_widget<tbutton>(window_, "insert_event", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tklink::click_insert_iot_task
			, this, std::ref(*button)));
	insert_event_widget_ = button;

	button = find_widget<tbutton>(window_, "insert_speech_sensor", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tklink::click_insert_speech_sensor_task
			, this, std::ref(*button)));
	insert_speech_widget_ = button;

	button = find_widget<tbutton>(window_, "insert_timed", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tklink::click_insert_timed_task
			, this, std::ref(*button)));
	insert_timed_widget_ = button;

	button = find_widget<tbutton>(window_, "insert_var", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tklink::click_insert_var_task
			, this, std::ref(*button)));
	insert_var_widget_ = button;

	// button = find_widget<tbutton>(window_, "import", false, true);
	// button = find_widget<tbutton>(window_, "export", false, true);

	button = find_widget<tbutton>(window_, "insert_device", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tklink::click_insert_device
			, this, std::ref(*button)));
	insert_alias_widget_ = button;

	// button = find_widget<tbutton>(window_, "insert_scene", false, true);
	
	button = find_widget<tbutton>(window_, "refresh", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tklink::click_refresh
			, this, std::ref(*button)));
	refresh_widget_ = button;

	button = find_widget<tbutton>(window_, "clear", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tklink::click_clear
			, this, std::ref(*button)));
	clear_widget_ = button;

	//
	//
	// tgrid* grid = find_widget<tgrid>(window_, "main_grid", false, true);
	treport* report = find_widget<treport>(window_, "main_report", false, true);
	report_ = report;
	stack_ = find_widget<tstack>(window_, "main_stack", false, true);

	pre_task(*stack_->layer(TASK_LAYER));
	pre_scene(*stack_->layer(SCENE_LAYER));
	pre_device(*stack_->layer(DEVICE_LAYER));
	pre_env_var(*stack_->layer(ENV_VAR_LAYER));
	pre_add_timed_task(*stack_->layer(ADD_TIMED_TASK_LAYER));

	tcontrol* item = &report->insert_item(null_str, _("Task"));
	// item->set_icon("misc/task.png");

	item = &report->insert_item(null_str, _("Base scene"));

	item = &report->insert_item(null_str, _("IoT device"));
	// item->set_icon("misc/log.png");

	item = &report->insert_item(null_str, _("Env var"));

	item = &report->insert_item(null_str, _("Add timed task"));

	report->set_did_item_changed(std::bind(&tklink::did_item_changed, this, _1, _2));
	report->select_item(TASK_LAYER);
}

void tklink::post_show()
{
}

void tklink::pre_task(tgrid& grid)
{
	utils::string_map symbols;
	symbols["priority_preempt_aiagent"] = str_cast(aplt::priority_preempt_aiagent);
	find_widget<tlabel>(&grid, "task_layer_remark", false, true)->set_label(vgettext2("task_layer remark, $priority_preempt_aiagent", symbols));

	task_list_ = find_widget<tlistbox>(&grid, "task_list", false, true);
	tlistbox& list = *task_list_;
	list.enable_select(false);
	list.set_did_can_drag(std::bind(&tklink::did_tasks_can_drag, this, _1, _2));

	tbutton* button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("edit_name", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tklink::click_edit_name_or_sync_vars
			, this
			, std::ref(list), true));

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("edit", true));
	button->set_icon("misc/bg_ff0000.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tklink::click_edit_task
			, this
			, std::ref(list)));

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("edit_sync_vars", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tklink::click_edit_name_or_sync_vars
			, this
			, std::ref(list), false));

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("edit_input_vars", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tklink::click_edit_input_vars
			, this
			, std::ref(list)));

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("stop", true));
	button->set_icon("misc/bg_ff0000.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tklink::click_stop_task
			, this
			, std::ref(list)));

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("erase", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tklink::click_erase_task
			, this
			, std::ref(list)));
}

void tklink::pre_device(tgrid& grid)
{
	utils::string_map symbols;
	symbols["online_minute"] = str_cast(online_threshold_minute_);
	find_widget<tlabel>(&grid, "remark", false, true)->set_label(vgettext2("klink's device layer remark, $online_minute", symbols));

	device_list_ = find_widget<tlistbox>(&grid, "device_list", false, true);
	tlistbox& list = *device_list_;
	list.enable_select(false);
	list.set_did_can_drag(std::bind(&tklink::did_pins_can_drag, this, _1, _2));

	tbutton* button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("edit", true));
	button->set_icon("misc/bg_ff0000.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tklink::click_edit_alias_name
			, this
			, std::ref(list)));

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("erase", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tklink::click_erase_device
			, this
			, std::ref(list)));
}

void tklink::pre_add_timed_task(tgrid& grid)
{
	find_widget<tlabel>(&grid, "remark", false, true)->set_label(_("klink's add_timed_task layer remark"));

	add_timed_task_list_ = find_widget<tlistbox>(&grid, "add_timed_task_list", false, true);
	tlistbox& list = *add_timed_task_list_;
	list.enable_select(false);
	// list.set_did_can_drag(std::bind(&tklink::did_var_sensors_can_drag, this, _1, _2));
}
/*
void tklink::set_title_label()
{
	const std::string& aplt_name = aplt::all_fake_applets.find(aplt::builtinid_klink)->second.name;

	char label[80];
	SDL_snprintf(label, sizeof(label), "%s(%s)", aplt_name.c_str(), bg_task_.scene_name().c_str());

	title_widget_->set_label(label);
}

bool tklink::verify_klink_scene_name(const std::string& label, const std::string& initial) const
{
	if (label.empty()) {
		return false;
	}
	if (label == initial) {
		return false;
	}

	int s = label.size();
	if (s > MAX_KLINK_SCENE_NAME_BYTES) {
		return false;
	}
	return true;
}

std::string scene_name_2_klink_cfg_name(const std::string& scene_name)
{
	VALIDATE(!scene_name.empty(), null_str);
	char buf[36];
	SDL_snprintf(buf, sizeof(buf), "klink_%s.cfg", scene_name.c_str());
	return buf;
}

void tklink::click_title(tbutton& widget)
{
	std::string title = _("Scene name");
	std::string prefix;
    std::string placeholder;
    const std::string initial = bg_task_.scene_name();

	utils::string_map symbols;
	int max_chars = MAX_KLINK_SCENE_NAME_BYTES;
	const std::string example_scene = _("Lamp");
	symbols["scene_name"] = example_scene;
	symbols["filename"] = scene_name_2_klink_cfg_name(example_scene);
    std::string remark = vgettext2("scene_name remark, $scene_name, $filename", symbols);

	std::string new_name;
	{
		gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), max_chars, gui2::tedit_box_param::show_cancel);
		param.did_text_changed = std::bind(&tklink::verify_klink_scene_name, this, _1, std::ref(initial));
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_->get_height() / 5);
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
		}
		new_name = param.result;
	}

	VALIDATE(new_name != initial, null_str);
	bg_task_.set_scene_name(new_name, true);
	set_title_label();
}
*/
void tklink::did_item_changed(treport& report, ttoggle_button& widget)
{
	tgrid* current_layer = stack_->layer(widget.at());
	stack_->set_radio_layer(widget.at());

	current_layer_ = widget.at();

	if (widget.at() == TASK_LAYER) {
		insert_event_widget_->set_visible(twidget::VISIBLE);
		insert_speech_widget_->set_visible(twidget::VISIBLE);
		insert_var_widget_->set_visible(twidget::VISIBLE);
		insert_timed_widget_->set_visible(twidget::VISIBLE);
		import_widget_->set_visible(twidget::VISIBLE);
		export_widget_->set_visible(twidget::VISIBLE);
		insert_alias_widget_->set_visible(twidget::INVISIBLE);
		insert_scene_widget_->set_visible(twidget::INVISIBLE);
		refresh_widget_->set_visible(twidget::INVISIBLE);
		clear_widget_->set_visible(twidget::INVISIBLE);

		reload_task_list(*task_list_);

	} else if (widget.at() == SCENE_LAYER) {
		insert_event_widget_->set_visible(twidget::INVISIBLE);
		insert_speech_widget_->set_visible(twidget::INVISIBLE);
		insert_var_widget_->set_visible(twidget::INVISIBLE);
		insert_timed_widget_->set_visible(twidget::INVISIBLE);
		import_widget_->set_visible(twidget::INVISIBLE);
		export_widget_->set_visible(twidget::INVISIBLE);
		insert_alias_widget_->set_visible(twidget::INVISIBLE);
		insert_scene_widget_->set_visible(twidget::VISIBLE);
		refresh_widget_->set_visible(twidget::INVISIBLE);
		clear_widget_->set_visible(twidget::INVISIBLE);

		reload_scene_list(*scene_list_);

	} else if (widget.at() == DEVICE_LAYER) {
		insert_event_widget_->set_visible(twidget::INVISIBLE);
		insert_speech_widget_->set_visible(twidget::INVISIBLE);
		insert_var_widget_->set_visible(twidget::INVISIBLE);
		insert_timed_widget_->set_visible(twidget::INVISIBLE);
		import_widget_->set_visible(twidget::INVISIBLE);
		export_widget_->set_visible(twidget::INVISIBLE);
		insert_alias_widget_->set_visible(twidget::VISIBLE);
		insert_scene_widget_->set_visible(twidget::INVISIBLE);
		refresh_widget_->set_visible(twidget::INVISIBLE);
		clear_widget_->set_visible(twidget::INVISIBLE);

		reload_device_list(*device_list_);

	} else if (widget.at() == ENV_VAR_LAYER) {
		insert_event_widget_->set_visible(twidget::INVISIBLE);
		insert_speech_widget_->set_visible(twidget::INVISIBLE);
		insert_var_widget_->set_visible(twidget::INVISIBLE);
		insert_timed_widget_->set_visible(twidget::INVISIBLE);
		import_widget_->set_visible(twidget::INVISIBLE);
		export_widget_->set_visible(twidget::INVISIBLE);
		insert_alias_widget_->set_visible(twidget::INVISIBLE);
		insert_scene_widget_->set_visible(twidget::INVISIBLE);
		refresh_widget_->set_visible(twidget::VISIBLE);
		clear_widget_->set_visible(twidget::INVISIBLE);

		reload_env_var_list(*env_var_list_);

	} else {
		VALIDATE(widget.at() == ADD_TIMED_TASK_LAYER, null_str);
		insert_event_widget_->set_visible(twidget::INVISIBLE);
		insert_speech_widget_->set_visible(twidget::INVISIBLE);
		insert_var_widget_->set_visible(twidget::INVISIBLE);
		insert_timed_widget_->set_visible(twidget::INVISIBLE);
		import_widget_->set_visible(twidget::INVISIBLE);
		export_widget_->set_visible(twidget::INVISIBLE);
		insert_alias_widget_->set_visible(twidget::INVISIBLE);
		insert_scene_widget_->set_visible(twidget::INVISIBLE);
		refresh_widget_->set_visible(twidget::INVISIBLE);
		clear_widget_->set_visible(twidget::VISIBLE);

		clear_widget_->set_active(!cfg_cpp_api_.add_timed_tasks().empty());
		reload_add_timed_task_list(*add_timed_task_list_);
	}
}

static std::string join2_to_event_src2(const std::string& src, const std::string& evt)
{
	utils::string_map symbols;

	symbols["src"] = src;
	symbols["evt"] = evt;
	return vgettext2("$src$evt", symbols);
}

static std::string join2_to_event_name(const std::string& src2, const std::string& alias)
{
	utils::string_map symbols;

	symbols["src2"] = src2;
	symbols["alias"]= alias;
	return vgettext2("$src2|($alias)", symbols);
}

static std::string join3_to_event_name(const std::string& src, const std::string& evt, const std::string& alias)
{
	utils::string_map symbols;

	symbols["src"] = src;
	symbols["evt"] = evt;
	symbols["alias"]= alias;
	return vgettext2("$src$evt|($alias)", symbols);
}

static std::pair<std::string, std::string> may_nposm_event_name(aplt::tbg_task& bg_task, const aplt::taplt_task& task)
{
	utils::string_map symbols;
	if (aplt::iot_sources.count(task.iot_src) != 0) {
		const aplt::tiot_src2& iot_src2 = aplt::iot_sources.find(task.iot_src)->second;
		symbols["src"] = iot_src2.name;

		aplt::tiot_device_key alias_key(task.iot_src, task.src_device_id);
		if (aplt::iot_events.count(task.src_evt) != 0) {
			const std::string evt_name = aplt::iot_events.find(task.src_evt)->second.name;
			symbols["evt"] = evt_name;

			if (bg_task.iot_devices().count(alias_key) != 0) {
				const aplt::tiot_device& device = bg_task.iot_devices().find(alias_key)->second;
				return std::make_pair(join2_to_event_src2(iot_src2.name, evt_name), device.alias2());

			} else {
				// src_device_id is nposm
				// std::string new_device_id = task.src_device_id;
				// utils::ellipsis_truncate(new_device_id, 4);
				std::string new_device_id = utils::truncate_to_max_chars2(task.src_device_id, 4/*MAX_NORMAL_UTF8_NAME_CHARS - 1*/, true);

				symbols["device_id"]= new_device_id;
				return std::make_pair(vgettext2("$src$evt|[iot_device_id absent: $device_id]", symbols), null_str);
			}
		} else {
			// src_evt is nposm
			symbols["src"] = iot_src2.name;
			symbols["evt"] = str_cast(task.src_evt);
			return std::make_pair(vgettext2("$src|[iot_evt absent. evt: $evt]", symbols), null_str);
		}

	} else {
		// iot_src is nposm
		symbols["src"] = str_cast(task.iot_src);
		return std::make_pair(vgettext2("[iot_src absent. src: $src]", symbols), null_str);
	}
}

static std::string may_nposm_speech_name(const std::map<std::string, aplt::tspeech_sensor>& speech_sensors, aplt::tbg_task& bg_task, const aplt::taplt_task& task)
{
	utils::string_map symbols;
	if (speech_sensors.count(task.speech_id) != 0) {
		std::map<std::string, aplt::tspeech_sensor>::const_iterator it = speech_sensors.find(task.speech_id);

		const aplt::tspeech_sensor& sensor = it->second;
		return sensor.name;

	}

	// speech_id is absent
	symbols["id"] = task.speech_id;
	return vgettext2("[speech_id absent. id: $id]", symbols);
}

static bool gui_has_task(int type)
{
	return type == aplt::taplt_task::type_iot || type == aplt::taplt_task::type_speech || type == aplt::taplt_task::type_var;
}

static bool gui_has_priority(int type)
{
	return type == aplt::taplt_task::type_iot || type == aplt::taplt_task::type_speech;
}

static bool gui_has_2position(int type)
{
	return type == aplt::taplt_task::type_iot || type == aplt::taplt_task::type_timed;
}

void tklink::click_insert_iot_task(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	struct titem2f {
		titem2f(int _src, int _evt, const std::string& _device_id)
			: src(_src)
			, evt(_evt)
			, device_id(_device_id)
		{}

		const int src;
		const int evt;
		const std::string device_id;
	};
	std::vector<titem2f> item3fs;

	const std::map<aplt::tiot_device_key, aplt::tiot_device>& aliases = bg_task_.iot_devices();
	for (std::map<aplt::tiot_device_key, aplt::tiot_device>::const_iterator it = aliases.begin(); it != aliases.end(); ++ it) {
		const aplt::tiot_device& device = it->second;

		VALIDATE(aplt::iot_sources.count(device.src) != 0, null_str);
		const aplt::tiot_src2& iot_src2 = aplt::iot_sources.find(device.src)->second;
		for (std::set<int>::const_iterator it3 = iot_src2.events.begin(); it3 != iot_src2.events.end(); ++ it3) {
				const int evt = *it3;
				VALIDATE(aplt::iot_events.count(evt) != 0, null_str);

				// aplt::taplt_task this_task_key = get_event_aplt_task_key(device.src, evt, device.device_id);
				// if (bg_task_.event_tasks().count(this_task_key) != 0) {
				if (bg_task_.iot_task_by_iot_evt(device.src, evt, device.device_id) != nullptr) {
					continue;
				}

				ss.str("");
				ss << join3_to_event_name(iot_src2.name, aplt::iot_events.find(evt)->second.name, device.alias2());

				items.push_back(gui2::tmenu::titem(ss.str(), items.size()));
				item3fs.emplace_back(titem2f(device.src, evt, device.device_id));
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

	const titem2f& cur_item = item3fs[cursel];

	bg_task_.insert_iot_task(aplt::priority_nontimed_def, aplt::taplt_task::state_finished_expired, cur_item.src, cur_item.evt, cur_item.device_id);
	reload_task_list(*task_list_);
}

void tklink::click_insert_speech_sensor_task(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	std::vector<const aplt::tspeech_sensor*> sensors;

	const std::map<std::string, aplt::tspeech_sensor>& speech_sensors = cfg_cpp_api_.speech_sensors();
	for (std::map<std::string, aplt::tspeech_sensor>::const_iterator it = speech_sensors.begin(); it != speech_sensors.end(); ++ it) {
		const aplt::tspeech_sensor& sensor = it->second;

		// aplt::taplt_task this_task_key = get_speech_aplt_task_key(sensor.id);
		// if (bg_task_.speech_tasks().count(this_task_key) != 0) {
		if (bg_task_.speech_task_by_speech_id(sensor.id) != nullptr) {
			continue;
		}

		items.push_back(gui2::tmenu::titem(sensor.name, items.size()));
		sensors.push_back(&sensor);
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
	const aplt::tspeech_sensor& speech_sensor = *sensors[cursel];

	bg_task_.insert_speech_task(aplt::priority_nontimed_def, aplt::taplt_task::state_finished_expired, speech_sensor.id);
	reload_task_list(*task_list_);
}
/*
void get_task_item3fs(const tmoveit_driver& moveit_driver, std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, bool task_aiagent, uint32_t allow_type_mask, bool allow_empty)
{
	items.clear();
	item3fs.clear();

	const std::map<aplt::taplt_key, aplt::tapplet>& applets = instance->applets();
	const aplt::tbg_task& bg_task = instance->bg_task();

	std::stringstream ss;

	if (allow_empty) {
		// 'Empty' --> val(0)
		items.push_back(gui2::tmenu::titem(_("Empty"), 0));
	}

	const aplt::tapplet& bonus_aplt = aplt::fake_aplt;
	for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it = bonus_aplt.tasks.begin(); it != bonus_aplt.tasks.end(); ++ it) {
		const aplt::tapplet::ttask& task = it->second;
		if (task.type != aplt::task_cpp) {
			// non-task_cpp in fake_aplt are not selectable always.
			continue;
		}
		if (allow_type_mask != nposm && !(BIT_IDX_MASK(task.type) & allow_type_mask)) {
			continue;
		}

		ss.str("");
		ss << bonus_aplt.name2() << "-" << task.name2();

		items.push_back(gui2::tmenu::titem(ss.str(), items.size()));
		item3fs.emplace_back(aplt::ttask_item3f(bonus_aplt, task, null_str));
	}

	std::vector<const aplt::tiot_device*> src_devices;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const aplt::tapplet& applet = it->second;
		const std::string id = applet.id;

		if (applet.tasks.empty()) {
			continue;
		}

		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it = applet.tasks.begin(); it != applet.tasks.end(); ++ it) {
			const aplt::tapplet::ttask& task = it->second;
			if (!moveit_driver.installed() && task.type == aplt::task_moveit) {
				continue;
			}

			if (!task_aiagent && task.type == aplt::task_aiagent) {
				continue;
			}
			if (allow_type_mask != nposm && !(BIT_IDX_MASK(task.type) & allow_type_mask)) {
				continue;
			}
			{
				ss.str("");
				ss << applet.name2() << "-" << task.name2();

				items.push_back(gui2::tmenu::titem(ss.str(), items.size()));
				item3fs.emplace_back(aplt::ttask_item3f(applet, task, null_str));

			}

			if (aplt::iot_sources.count(task.iot_src) == 0) {
				continue;
			}
			const aplt::tiot_src2& iot_src2 = aplt::iot_sources.find(task.iot_src)->second;
			src_devices = bg_task.iot_devices_from_src(iot_src2.code);

			{
				for (std::vector<const aplt::tiot_device*>::const_iterator it2 = src_devices.begin(); it2 != src_devices.end(); ++ it2) {
					const aplt::tiot_device& device = **it2;

					ss.str("");
					ss << applet.name2() << "-" << task.name2() << "[" << device.alias2() << "]";
					items.push_back(gui2::tmenu::titem(ss.str(), items.size()));
					item3fs.emplace_back(aplt::ttask_item3f(applet, task, device.device_id));
				}
			}
		}
	}
}
*/
void get_task_item3fs_ex(const tmoveit_driver& moveit_driver, std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, uint32_t allow_type_mask, bool allow_aiagent, bool allow_empty)
{
	uint32_t deny_type_mask = 0;

	if (!moveit_driver.installed()) {
		deny_type_mask |= BIT_IDX_MASK(aplt::task_moveit);
	}
	if (!allow_aiagent) {
		deny_type_mask |= BIT_IDX_MASK(aplt::task_aiagent);
	}
	get_task_item3fs(items, item3fs, allow_type_mask, deny_type_mask, allow_empty);
}

/*
std::string generate_task2_desc(const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task, const std::string& device_id)
{
	std::stringstream ss;
	ss << aplt.name2() << "-" << cfg_task.name2();
	if (!device_id.empty()) {
		ss << "[" << device_id << "]";
	}
	return ss.str();
}

std::string generate_task2_desc2(int type, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task, const std::string& device_id)
{
	std::string result = generate_task2_desc(aplt, cfg_task, device_id);
	if (type == aplt::taplt_task::type_var) {
		result = utils::truncate_to_max_chars2(result, 34, false);
	}
	return result;
}
*/
void tklink::click_insert_timed_task(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::vector<aplt::ttask_item3f> item3fs;

	get_task_item3fs_ex(moveit_driver_, items, item3fs, nposm, false, false);

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();

	const aplt::ttask_item3f& cur_item = item3fs[cursel];
	const aplt::tapplet* cur_aplt = item3fs[cursel].aplt;
	const aplt::tapplet::ttask* cur_cfg_task = item3fs[cursel].task;

	int new_timed_at = bg_task_.timed_tasks().size();
	const aplt::taplt_task* new_task = bg_task_.insert_timed_task(cur_aplt->id, cur_cfg_task->id, cur_item.device_id, aplt::taplt_task::state_finished_expired, 0);
	if (new_task == nullptr) {
		return;
	}

	cfg_cpp_api_.insert_timed_sensor(new_timed_at, nposm);

	reload_task_list(*task_list_);
}

void tklink::click_insert_var_task(tbutton& widget)
{
	std::vector<aplt::tvar_sensor>& var_sensors = cfg_cpp_api_.var_sensors();
	if (var_sensors.size() >= MAX_KLINK_VARS) {
		utils::string_map symbols;
		symbols["max"] = str_cast(MAX_KLINK_VARS);
		std::string err = vgettext2("Supports up to $max variable tasks", symbols);
		gui2::show_message(null_str, err);
		return;
	}

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	const std::vector<aplt::tvar_sensor>& buildin_sensors = cfg_cpp_api_.buildin_var_sensors();

	items.push_back(gui2::tmenu::titem(_("New var sensor"), buildin_sensors.size()));
	if (!buildin_sensors.empty()) {
		int at = 0;
		for (std::vector<aplt::tvar_sensor>::const_iterator it = buildin_sensors.begin(); it != buildin_sensors.end(); ++ it, at ++) {
			const aplt::tvar_sensor& sensor = *it;
			items.push_back(gui2::tmenu::titem(sensor.name, at));
		}
	}

	int cursel;
	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		cursel = dlg.selected_val();
	}

	int new_var_at = bg_task_.var_tasks().size();
	const aplt::taplt_task* new_task = bg_task_.insert_var_task(aplt::taplt_task::state_finished_expired);
	if (new_task == nullptr) {
		return;
	}

	int builtin_at = cursel == (int)buildin_sensors.size()? nposm: cursel;
	std::string name;
	if (builtin_at == nposm) {
		std::set<std::string> existed_names;
		for (std::vector<aplt::tvar_sensor>::const_iterator it = var_sensors.begin(); it != var_sensors.end(); ++ it) {
			const aplt::tvar_sensor& sensor = *it;
			existed_names.insert(sensor.name);
		}
		name = utils::unique_untitle_name(existed_names, null_str, 1);
	}
	cfg_cpp_api_.insert_var_sensor(new_var_at, builtin_at, name);

	reload_task_list(*task_list_);
}

#define MAX_POSITION_NAME_CHARS		10

void tklink::refresh_2position(tbutton& position1_widget, tbutton& position2_widget, const aplt::taplt_task& task, const aplt::tapplet* aplt, const aplt::tapplet::ttask* cfg_task)
{
	if (cfg_task != nullptr) {
		VALIDATE(aplt != nullptr, null_str);
	}

	if (cfg_task == nullptr || cfg_task->type == aplt::task_cpp) {
		// this aplt is unloaded or this aplt hasn't this task.id
		position1_widget.set_visible(twidget::INVISIBLE);
		position2_widget.set_visible(twidget::INVISIBLE);
		return;
	}

	position1_widget.set_visible(twidget::VISIBLE);
	position2_widget.set_visible(twidget::VISIBLE);

	std::string position1_name;
	if (curmap_.positions.count(task.position1) != 0) {
		position1_name = curmap_.positions.find(task.position1)->second.name;
	}

	std::string position2_name;
	if (curmap_.positions.count(task.position2) != 0) {
		position2_name = curmap_.positions.find(task.position2)->second.name;
	}

	if (!position1_name.empty()) {
		position1_widget.set_label(position1_name);

		if (!position2_name.empty()) {
			position2_widget.set_label(position2_name);

		} else if (task.position2.empty()) {
			position2_widget.set_label(null_str);
		}

	} else {
		if (task.position1.empty()) {
			position1_widget.set_label(null_str);
		}

		if (task.position2.empty()) {
			VALIDATE(position2_name.empty(), null_str);
			position2_widget.set_visible(twidget::INVISIBLE);

		} else if (!position2_name.empty()) {
			// !postion1_name.empty() && !position2_name.empty()
			position2_widget.set_label(position2_name);
		}
	}

	// both task.postion1 and task.position2 are uuid-format, too long. for display require to truncate.
	if (position1_name.empty() && !task.position1.empty()) {
		std::string truncated = utils::truncate_to_max_chars(task.position1.c_str(), task.position1.size(), MAX_POSITION_NAME_CHARS);
		position1_widget.set_label(ht::generate_format(truncated, 0xffff0000));
	}

	if (position2_name.empty() && !task.position2.empty()) {
		std::string truncated = utils::truncate_to_max_chars(task.position2.c_str(), task.position2.size(), MAX_POSITION_NAME_CHARS);
		position2_widget.set_label(ht::generate_format(truncated, 0xffff0000));
	}
}

static std::string truncate_for_var_name_label(const std::string& label)
{
	VALIDATE(!label.empty(), null_str);
	const int max_bytes = 20;
	return utils::truncate_to_max_bytes(label.c_str(), label.size(), 20);
}

#define MAX_VAR_TASK_LABEL_CHARS		256
static std::string truncate_for_var_task_label(const std::string& label, int max_chars = MAX_VAR_TASK_LABEL_CHARS, bool ellipsis = true)
{
	VALIDATE(max_chars >= 64, null_str);
	if (label.empty()) {
		return label;
	}
	return utils::truncate_to_max_chars2(label, max_chars, ellipsis);
}

std::string sync_vars_label(const aplt::tvar_sensor& sensor)
{
	std::stringstream ss;
	if (!sensor.sync_vars.empty()) {
		ss << "(" << sensor.sync_vars.size() << ")";
	}
	for (std::set<std::string>::const_iterator it = sensor.sync_vars.begin(); it != sensor.sync_vars.end(); ++ it) {
		std::pair<std::string, std::string> pair = utils::split_app_prefix_id(*it);
		VALIDATE(pair.first == aplt::fake_aplt.bundleid, null_str);
		if (it != sensor.sync_vars.begin()) {
			ss << ", ";
		}
		ss << pair.second;
	}
	if (ss.str().empty()) {
		ss << _("Env_vars to be synchronized");
	}
	return utils::truncate_to_max_chars2(ss.str(), 36, false);
}

std::string timed_task_name_label(const std::string& task_name)
{
	return utils::truncate_to_max_chars2(task_name, 8, true);
}

std::string input_vars_label(const std::map<std::string, std::string>& input_vars, bool show_empty_tip)
{
	std::stringstream ss;
	if (!input_vars.empty()) {
		ss << "(" << input_vars.size() << ")";
	}
	for (std::map<std::string, std::string>::const_iterator it = input_vars.begin(); it != input_vars.end(); ++ it) {
		const std::string& name = it->first;
		const std::string& val = it->second;
		if (it != input_vars.begin()) {
			ss << ", ";
		}
		ss << name << "=" << val;
	}
	if (ss.str().empty() && show_empty_tip) {
		ss << "<" << _("Pre-assigned input variable") << ">";
	}
	return utils::truncate_to_max_chars2(ss.str(), 36, true);
}

void tklink::insert_row(tlistbox& list, const aplt::taplt_task& task, const std::map<std::string, const aplt::tapplet*>& id_aplt_map, std::map<std::string, std::string>& data)
{
	std::stringstream ss;
	utils::string_map symbols;

	std::string aplt_name;
	data.clear();

	const aplt::tapplet* hit_aplt = aplt::aplt_from_id_ex(applets_, task.aplt_id);

	const aplt::tapplet::ttask* cfg_task = nullptr;
	std::string task_name = task.task_id;
	if (hit_aplt != nullptr && hit_aplt->tasks.count(task.task_id) != 0) {
		cfg_task = &hit_aplt->tasks.find(task.task_id)->second;
		// task_name = cfg_task->name;
		task_name = cfg_task->name2();
	}

	if (hit_aplt != nullptr) {
		if (cfg_task != nullptr) {
			aplt_name = hit_aplt->name2();
		} else {
			aplt_name = hit_aplt->name2() + _("[Task absent]");
		}

	} else {
		aplt_name = task.aplt_id + _("[Unloaded]");
	}

	if (task.type == aplt::taplt_task::type_iot) {
		std::pair<std::string, std::string> pair = may_nposm_event_name(bg_task_, task);
		data.insert(std::make_pair("event_src", pair.first));
		data.insert(std::make_pair("event_alias", pair.second));
		data.insert(std::make_pair("event_icon", get_clock_png(task)));

	} else if (task.type == aplt::taplt_task::type_speech) {
		const std::map<std::string, aplt::tspeech_sensor>& speech_sensors = cfg_cpp_api_.speech_sensors();
		std::string speech_name = may_nposm_speech_name(speech_sensors, bg_task_, task);
		data.insert(std::make_pair("speech_sensor", speech_name));
		data.insert(std::make_pair("speech_icon", get_clock_png(task)));

	} else if (task.type == aplt::taplt_task::type_var) {
		// const std::map<std::string, aplt::tspeech_sensor>& speech_sensors = cfg_cpp_api_.speech_sensors();
		// std::string speech_name = may_nposm_speech_name(speech_sensors, bg_task_, task);
		const std::vector<aplt::tvar_sensor>& var_sensors = cfg_cpp_api_.var_sensors();
		VALIDATE((int)var_sensors.size() > task.var_at, null_str);
		const aplt::tvar_sensor& sensor = var_sensors[task.var_at];

		std::string name = sensor.name;
		if (name.empty()) {
			symbols["number"] = str_cast(task.var_at + 1);
			name = vgettext2("Var $number", symbols);
		}
		name = truncate_for_var_name_label(name);

		data.insert(std::make_pair("var_sensor", name));

		aplt::ttask_cpp_pair pair;
		ss.str("");
		std::string str = tif_block_to_string(sensor.if_block, if_block_var_task, pair, curmap_);
		ss << truncate_for_var_task_label(str);
		data.insert(std::make_pair("var_if_block", ss.str()));

		data.insert(std::make_pair("var_icon", get_clock_png(task)));

		data.insert(std::make_pair("sync_vars", sync_vars_label(sensor)));

	} else {
		VALIDATE(task.type == aplt::taplt_task::type_timed, null_str);
		const std::vector<aplt::ttimed_sensor>& timed_sensors = cfg_cpp_api_.timed_sensors();
		VALIDATE((int)timed_sensors.size() > task.timed_at, null_str);
		const aplt::ttimed_sensor& sensor = timed_sensors[task.timed_at];

		// task.state == aplt::taplt_task::state_fresh? task_name_value: ht::generate_format(task_name_value, 0xff808080);
		data.insert(std::make_pair("timed_task", timed_task_name_label(task_name)));

		ss.str("");
		ss << aplt_name;
		if (!task.ble_device_id.empty()) {
			ss << "-" << task.ble_device_id;
		}
		data.insert(std::make_pair("timed_aplt", ss.str()));
		data.insert(std::make_pair("timed_input_vars", input_vars_label(sensor.input_vars, true)));
		data.insert(std::make_pair("timed_time", utils::format_second_24hoursys(task.zerotz_t)));

		data.insert(std::make_pair("timed_icon", get_clock_png(task)));
		data.insert(std::make_pair("timed_state", bg_task_.task_state_desc(task.state)));

	}

	ttoggle_panel& row = list.insert_row(data);
	tstack* stack_widget = find_widget<tstack>(&row, "row_stack", false, true);
	int layer = KLINK_IOT_LAYER;
	if (task.type == aplt::taplt_task::type_speech) {
		layer = KLINK_SPEECH_LAYER;
	} else if (task.type == aplt::taplt_task::type_var) {
		layer = KLINK_VAR_LAYER;
	} else if (task.type == aplt::taplt_task::type_timed) {
		layer = KLINK_TIMED_LAYER;
	} 
	stack_widget->set_radio_layer(layer);
	tgrid& layer_grid = *stack_widget->layer(layer);
	// below 'find_widget<txxx>' require to use @&layer_grid, must not use @&row.

	if (gui_has_task(task.type)) {
		tbutton* task2_widget = find_widget<tbutton>(&layer_grid, "event_task2", false, true);
		if (cfg_task != nullptr) {
			task2_widget->set_label(generate_task2_desc2(task.type, *hit_aplt, *cfg_task, task.ble_device_id));
		}

		connect_signal_mouse_left_click(
			*task2_widget
			, std::bind(
				&tklink::click_task_task2
				, this
				, std::ref(list), std::ref(*task2_widget), row.at()));
	}

	if (gui_has_priority(task.type)) {
		tbutton* priority_widget = find_widget<tbutton>(&layer_grid, "priority", false, true);
		priority_widget->set_label(aplt::nontimed_priority_name(task.priority));

		connect_signal_mouse_left_click(
			*priority_widget
			, std::bind(
				&tklink::click_task_priority
				, this
				, std::ref(list), std::ref(*priority_widget), row.at()));
	}

	if (gui_has_2position(task.type)) {
		const std::string position1_key = "position1";
		const std::string position2_key = "position2";
	
		tbutton* position1_widget = find_widget<tbutton>(&layer_grid, position1_key, false, true);
		tbutton* position2_widget = find_widget<tbutton>(&layer_grid, position2_key, false, true);

		connect_signal_mouse_left_click(
			*position1_widget
			, std::bind(
				&tklink::click_task_position
				, this
				, std::ref(list), std::ref(*position1_widget), std::ref(*position2_widget), row.at(), true));

		connect_signal_mouse_left_click(
			*position2_widget
			, std::bind(
				&tklink::click_task_position
				, this
				, std::ref(list), std::ref(*position2_widget), std::ref(*position1_widget), row.at(), false));

		// +++++++++++++++++
		refresh_2position(*position1_widget, *position2_widget, task, hit_aplt, cfg_task);
	}
}

void tklink::reload_task_list(tlistbox& list)
{
	int prev_sel_at = list.cursel() != nullptr? list.cursel()->at(): nposm;

	VALIDATE(list.rows() == (int)task_keys_.size(), null_str);
	list.clear();
	task_keys_.clear();

	std::set<ttask_key> set_task_keys;
	// const std::set<aplt::taplt_task>& event_tasks = bg_task_.iot_tasks();
	const std::map<aplt::tiot_device_key, aplt::tiot_device>& iot_devices = bg_task_.iot_devices();

	int klink_types[] = {aplt::taplt_task::type_iot, aplt::taplt_task::type_speech, aplt::taplt_task::type_timed, aplt::taplt_task::type_var};
	int type_count = sizeof(klink_types) / sizeof(klink_types[0]);
	for (int type_at = 0; type_at < type_count; type_at ++) {
		int type = klink_types[type_at];
		const std::map<aplt::taplt_task_key, aplt::taplt_task>& tasks = bg_task_.get_tasks(type);
		int at = 0;
		for (std::map<aplt::taplt_task_key, aplt::taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it, at ++) {
			const aplt::taplt_task& task = it->second;
			std::string alias;
			if (type == aplt::taplt_task::type_iot) {
				aplt::tiot_device_key key(task.iot_src, task.src_device_id);
				if (iot_devices.count(key) != 0) {
					alias = iot_devices.find(key)->second.alias;
				}
			}
			set_task_keys.insert(ttask_key(task.type, task.priority, task.var_at, task.zerotz_t, at, alias));
		}
	}

	std::map<std::string, const aplt::tapplet*> id_aplt_map;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		const aplt::tapplet& applet = it->second;
		const std::string id = applet.id;
		id_aplt_map.insert(std::make_pair(id, &applet));
	}

	std::map<std::string, std::string> data;
	for (std::set<ttask_key>::const_iterator it = set_task_keys.begin(); it != set_task_keys.end(); ++ it) {
		const ttask_key& key = *it;
		task_keys_.push_back(key);

		const aplt::taplt_task& task = bg_task_.task_from_at(key.type, key.at);
		insert_row(list, task, id_aplt_map, data);
	}

	VALIDATE(list.rows() == (int)task_keys_.size(), null_str);
}

bool tklink::did_tasks_can_drag(tlistbox& list, ttoggle_panel& row)
{
	const int at = row.at();
	const ttask_key& key = task_keys_[at];
	const aplt::taplt_task& task = bg_task_.task_from_at(key.type, key.at);

	std::map<std::string, twidget::tvisible> visibles;

	bool running = task.state == aplt::taplt_task::state_running;

	const aplt::tapplet* aplt = aplt::aplt_from_id_ex(applets_, task.aplt_id);
	const aplt::tapplet::ttask* cfg_task = nullptr;
	if (aplt != nullptr && aplt->tasks.count(task.task_id) != 0) {
		cfg_task = &aplt->tasks.find(task.task_id)->second;
	}

	bool show_edit_name = task.type == aplt::taplt_task::type_var;
	bool show_edit = task.type == aplt::taplt_task::type_var || task.type == aplt::taplt_task::type_timed;
	bool show_edit_sync_vars = task.type == aplt::taplt_task::type_var;
	bool show_edit_input_vars = task.type == aplt::taplt_task::type_timed;

	if (running) {
		show_edit_name = false;
		show_edit = false;
		show_edit_sync_vars = false;
	}

	visibles.insert(std::make_pair("edit_name", show_edit_name? twidget::VISIBLE: twidget::INVISIBLE));
	visibles.insert(std::make_pair("edit", show_edit? twidget::VISIBLE: twidget::INVISIBLE));
	visibles.insert(std::make_pair("edit_sync_vars", show_edit_sync_vars? twidget::VISIBLE: twidget::INVISIBLE));
	visibles.insert(std::make_pair("edit_input_vars", show_edit_input_vars? twidget::VISIBLE: twidget::INVISIBLE));
	visibles.insert(std::make_pair("erase", running? twidget::INVISIBLE: twidget::VISIBLE));

	visibles.insert(std::make_pair("stop", running? twidget::VISIBLE: twidget::INVISIBLE));

	list.left_drag_grid_set_widget_visible(visibles);
	return true;
}

static std::set<aplt::taplt_var_pair> collect_var_names2()
{
	std::set<aplt::taplt_var_pair> result;

	for (std::map<int, tcode3>::const_iterator it = aplt::BI_env_vars.begin(); it != aplt::BI_env_vars.end(); ++ it) {
		const std::string short_var_name = utils::split_app_prefix_id(it->second.id).second;
		result.insert(aplt::taplt_var_pair(aplt::fake_aplt, null_str, short_var_name, null_str));
	}

	return result;
}

bool tklink::show_if_block2_dlg(const std::string& title, const aplt::tapplet::ttask& cfg_task, aplt::tif_block& if_block, bool& dirty)
{
	std::set<aplt::taplt_var_pair> vars = collect_var_names2();
	const aplt::ttask_cpp_pair cpp_pair;
	gui2::tif_block2 dlg(rdpd_mgr_, pble_, privacy_, vars, cpp_pair, curmap_, title, &cfg_task, if_block, if_block_var_task);
	dlg.show();
	if (dlg.get_retval() != twindow::OK) {
		return false;
	}
	dirty = dlg.get_dirty();

	return true;
}

void tklink::click_edit_name_or_sync_vars(tlistbox& list, bool is_name)
{
	const int drag_at = list.drag_at();
	const ttask_key& key = task_keys_[drag_at];
	const aplt::taplt_task* task = &bg_task_.task_from_at(key.type, key.at);

	// first, require cancel left_drag grid.
	list.cancel_drag();
	const aplt::taplt_task* running_klink_aplt_task = bg_task_.klink_task_if_ing();
	// must not use if (running_klink_aplt_task == task), 
	// once task running, task's filed will modify, result 'task''s memory be free.
	if (running_klink_aplt_task != nullptr && 
			running_klink_aplt_task->type == key.type && running_klink_aplt_task->pb_at == key.at) {
		return;
	}

	std::vector<aplt::tvar_sensor>& sensors = cfg_cpp_api_.var_sensors();
	VALIDATE(key.at < (int)sensors.size(), null_str);
	aplt::tvar_sensor& sensor = sensors[key.at];
	VALIDATE(key.at == sensor.at, null_str);

	std::string new_name;

	std::set<int> cursels;
	std::vector<tcode2> items;
	if (is_name) {
		utils::string_map symbols;
		symbols["number"] = str_cast(key.at + 1);

		std::string title = vgettext2("Edit var $number's name", symbols);
		title = title + "(" + sensor.name + ")";
		std::string prefix;
		std::string placeholder;
		const std::string initial = sensor.name;

		int max_chars = MAX_NORMAL_UTF8_NAME_CHARS;
		symbols["max_chars"] = str_cast(max_chars);
		std::string remark = vgettext2("Easy name, can be Chinese, and maximum $max_chars characters", symbols);

		std::set<std::string> excludes;
		{
			gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), max_chars, gui2::tedit_box_param::show_cancel);
			param.did_text_changed = std::bind(&tklink::verify_edit_alias_name, this, _1, std::ref(initial), etype_var_name, std::ref(excludes));
			{
				gui2::tedit_box dlg(param);
				dlg.show(nposm, window_->get_height() / 5);
				if (dlg.get_retval() != gui2::twindow::OK) {
					return;
				}
			}
			new_name = param.result;
		}
		VALIDATE(new_name != sensor.name, null_str);

	} else {
		std::stringstream ss;

		std::string title = _("Env_vars to be synchronized after the task is finished");
		std::set<int> initials;
		for (std::map<int, tcode3>::const_iterator it = aplt::BI_env_vars.begin(); it != aplt::BI_env_vars.end(); ++ it) {
			const tcode3& code3 = it->second;
			if (BI_var_is_auto_update(code3.code)) {
				continue;
			}

			if (sensor.sync_vars.count(code3.id) != 0) {
				// std::set<std::string>::const_iterator found_it = var_sensor.sync_vars.find(code3.id);
				// initials.insert(std::distance(var_sensor.sync_vars.begin(), found_it));
				initials.insert(items.size());
			}

			items.push_back(tcode2(items.size(), code3.id));
		}

		{
			const std::string remark = _("select sync_vars in var trigger remark");
			gui2::tcombo_box2 dlg(title, remark, items, nposm, false, &initials);
			dlg.show();
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
			cursels = dlg.cursels();
		}
	}

	running_klink_aplt_task = bg_task_.klink_task_if_ing();
	// must not use if (running_klink_aplt_task == task), 
	// once task running, task's filed will modify, result 'task''s memory be free.
	if (running_klink_aplt_task != nullptr && 
		running_klink_aplt_task->type == aplt::taplt_task::type_var && running_klink_aplt_task->var_at == key.at) {
		return;
	}

	if (is_name) {
		sensor.name = new_name;

	} else {
		sensor.sync_vars.clear();
		for (std::set<int>::const_iterator it = cursels.begin(); it != cursels.end(); ++ it) {
			int sel = *it;
			sensor.sync_vars.insert(items[sel].id);
		}
	}

	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_var_sensor);

	// if want to execute below, require cancel left_drag grid.
	ttoggle_panel& row = list.row_panel(drag_at);
	if (is_name) {
		row.set_child_label("var_sensor", truncate_for_var_name_label(sensor.name));
	} else {
		row.set_child_label("sync_vars", sync_vars_label(sensor));
	}
}

void tklink::click_edit_input_vars(tlistbox& list)
{
	const int drag_at = list.drag_at();
	const ttask_key& key = task_keys_[drag_at];
	const aplt::taplt_task* task = &bg_task_.task_from_at(key.type, key.at);

	// first, require cancel left_drag grid.
	list.cancel_drag();
	const aplt::taplt_task* running_klink_aplt_task = bg_task_.klink_task_if_ing();
	// must not use if (running_klink_aplt_task == task), 
	// once task running, task's filed will modify, result 'task''s memory be free.
	if (running_klink_aplt_task != nullptr && 
			running_klink_aplt_task->type == key.type && running_klink_aplt_task->pb_at == key.at) {
		return;
	}

	aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, task->aplt_id, task->task_id, true, false);
	VALIDATE(pair.task != nullptr, null_str);

	std::vector<aplt::ttimed_sensor>& sensors = cfg_cpp_api_.timed_sensors();
	VALIDATE(key.at < (int)sensors.size(), null_str);
	aplt::ttimed_sensor& sensor = sensors[key.at];
	VALIDATE(key.at == sensor.at, null_str);


	std::map<std::string, std::string> map_vals = sensor.input_vars;
	{
		utils::string_map symbols;

		std::string title = _("Variable");
		const std::string name_prefix = pair.aplt->bundleid;
		symbols["prefix"] = name_prefix + "__";
		std::string remark = vgettext2("remark^edit scene's input vars, $prefix", symbols);

		std::vector<std::string> freq_vals_;

		tvar_editor_slot slot(rdpd_mgr_, pble_, privacy_, curmap_);
		gui2::trvar_editor dlg(slot, title, remark, pair.task, freq_vals_, name_prefix, map_vals);
		dlg.show();
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}
	VALIDATE(sensor.input_vars != map_vals, null_str);

	sensor.input_vars = map_vals;
	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_timed_sensor);

	// if want to execute below, require cancel left_drag grid.
	ttoggle_panel& row = list.row_panel(drag_at);
	row.set_child_label("timed_input_vars", input_vars_label(sensor.input_vars, true));
}

void tklink::click_edit_task(tlistbox& list)
{
	const int drag_at = list.drag_at();
	const ttask_key& key = task_keys_[drag_at];
	const aplt::taplt_task* task = &bg_task_.task_from_at(key.type, key.at);

	// first, require cancel left_drag grid.
	list.cancel_drag();
	const aplt::taplt_task* running_klink_aplt_task = bg_task_.klink_task_if_ing();
	// must not use if (running_klink_aplt_task == task), 
	// once task running, task's filed will modify, result 'task''s memory be free.
	if (running_klink_aplt_task != nullptr && 
			running_klink_aplt_task->type == key.type && running_klink_aplt_task->pb_at == key.at) {
		return;
	}

	const aplt::tapplet* hit_aplt = aplt::aplt_from_id_ex(applets_, task->aplt_id);
	if (hit_aplt == nullptr) {
		return;
	}
	if (hit_aplt->tasks.count(task->task_id) == 0) {
		return;
	}

	const aplt::tapplet::ttask& cfg_task = hit_aplt->tasks.find(task->task_id)->second;

	aplt::taplt_task val_task;
	uint32_t flags = 0;
	if (task->type == aplt::taplt_task::type_var) {
		utils::string_map symbols;
		symbols["number"] = str_cast(task->var_at + 1);
		std::string title = vgettext2("Var $number", symbols);

		std::vector<aplt::tvar_sensor>& var_sensors = cfg_cpp_api_.var_sensors();
		VALIDATE(bg_task_.var_tasks().size() == var_sensors.size(), null_str);
		aplt::tvar_sensor cloned_var_sensor = var_sensors[key.at];
		VALIDATE(cloned_var_sensor.at == key.at, null_str);

		bool dirty;
		if (!show_if_block2_dlg(title, cfg_task, cloned_var_sensor.if_block, dirty)) {
			return;
		}
		// save new if_block to 
		if (!dirty) {
			return;
		}

		running_klink_aplt_task = bg_task_.klink_task_if_ing();
		if (running_klink_aplt_task != nullptr && 
			running_klink_aplt_task->type == key.type && running_klink_aplt_task->pb_at == key.at) {
			gui2::show_message(null_str, _("The task associated with this trigger is currently running and cannot be modified."));
			return;
		}
		var_sensors[key.at] = cloned_var_sensor;
		cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_var_sensor);

		ttoggle_panel& row = list.row_panel(drag_at);
		std::string str = tif_block_to_string(cloned_var_sensor.if_block, if_block_var_task, aplt::ttask_cpp_pair(), curmap_);
		row.set_child_label("var_if_block", truncate_for_var_task_label(str));
		return;

	} else {
		const int original_state = task->state;
		aplt::taplt_task new_task = *task;
		{
			gui2::ttime_setter dlg(*hit_aplt, bg_task_, new_task);
			dlg.show();
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}

		}

		// task maybe change to state, and old task is destroy, new task created.
		task = &bg_task_.task_from_at(task->type, key.at);
		if (task->state != original_state) {
			SDL_Log("click_edit_task, during ttime_setting, state chagned, this time do nothing");
			return;
		}

		if (new_task.zerotz_t == task->zerotz_t && new_task.state == task->state) {
			return;
		}

		val_task.state = new_task.state;
		val_task.zerotz_t = new_task.zerotz_t;

		flags = aplt::taplt_task::FLAG_STATE | aplt::taplt_task::FLAG_ZEROTZ_T;
	}
	VALIDATE(flags != 0, null_str);
	bg_task_.modify_task(*task, val_task, flags, true);

	// Although it modifies a task, it may cause the list sort to change
	reload_task_list(list);
}

void tklink::click_stop_task(tlistbox& list)
{
	const int drag_at = list.drag_at();
	const ttask_key& key = task_keys_[drag_at];
	const aplt::taplt_task* task = &bg_task_.task_from_at(key.type, key.at);

	// first, require cancel left_drag grid.
	list.cancel_drag();
	if (task->state != aplt::taplt_task::state_running) {
		return;
	}

	utils::string_map symbols;
	std::stringstream ss;
	ss << task->task_id << "(" << task->aplt_id << ")";
	if (!task->ble_device_id.empty()) {
		ss << "-" << task->ble_device_id;
	}
	symbols["task"] = ss.str();
	const std::string msg = vgettext2("Do you want to stop task: $task?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	// task maybe change to state, and old task is destroy, new task created.
	task = &bg_task_.task_from_at(task->type, key.at);
	if (task->state != aplt::taplt_task::state_running) {
		SDL_Log("click_stop_task, yesno-dialog, state change to running, this time do nothing");
		return;
	}

	instance->stop_bg_task2();
	reload_task_list(list);
}
/*
static std::string task_name2_from_aplt_task(std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::taplt_task& task)
{
	return task_name2_from_3id(applets, task.aplt_id, task.task_id, task.ble_device_id, false);
}
*/
std::string msg_running_not_modify2(std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::taplt_task& task)
{
	utils::string_map symbols;
	symbols["task"] = task_name2_from_aplt_task(applets, task);
	return vgettext2("Task '$task' is running and cannot be modified", symbols);
}

void tklink::click_erase_task(tlistbox& list)
{
	const int drag_at = list.drag_at();
	const ttask_key& key = task_keys_[drag_at];
	const aplt::taplt_task* task = &bg_task_.task_from_at(key.type, key.at);

	// first, require cancel left_drag grid.
	list.cancel_drag();
	if (task->state == aplt::taplt_task::state_running) {
		return;
	}

	utils::string_map symbols;
	if (task->type == aplt::taplt_task::type_iot) {
		symbols["type"] = _("tasks^event");
		std::pair<std::string, std::string> pair = may_nposm_event_name(bg_task_, *task);
		std::string task_name = pair.second.empty()? pair.first: join2_to_event_name(pair.first, pair.second);
		symbols["task"] = task_name;

	} else if (task->type == aplt::taplt_task::type_speech) {
		symbols["type"] = _("tasks^speech");
		symbols["task"] = may_nposm_speech_name(cfg_cpp_api_.speech_sensors(), bg_task_, *task);

	} else if (task->type == aplt::taplt_task::type_var) {
		symbols["type"] = _("tasks^var");
		symbols["task"] = task_name2_from_aplt_task(applets_, *task);

	} else {
		symbols["type"] = _("tasks^Timed");
		symbols["task"] = task_name2_from_aplt_task(applets_, *task);
	}
	const std::string msg = vgettext2("Do you want to delete $type task: $task?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	// during gui2::show_message2, task maybe change to state, and old task is destroy, new task created.
	task = &bg_task_.task_from_at(task->type, key.at);
	if (task->state == aplt::taplt_task::state_running) {
		SDL_Log("click_erase_task, during yesno-dialog, state change to running, this time do nothing");
		return;
	}

	int var_at = nposm;
	int timed_at = nposm;
	if (task->type == aplt::taplt_task::type_var) {
		var_at = task->var_at;

	} else if (task->type == aplt::taplt_task::type_timed) {
		timed_at = task->timed_at;
	}


	bg_task_.erase_task2(task->type, *task);

	if (var_at != nposm) {
		cfg_cpp_api_.erase_var_sensor(var_at);
	}
	if (timed_at != nposm) {
		cfg_cpp_api_.erase_timed_sensor(timed_at);
	}

	reload_task_list(list);
}

void tklink::click_task_task2(tlistbox& list, tbutton& widget, int at)
{
	const ttask_key& key = task_keys_[at];
	const aplt::taplt_task& task = bg_task_.task_from_at(key.type, key.at);
	VALIDATE(gui_has_task(task.type), null_str);

	if (task.state == aplt::taplt_task::state_running) {
		gui2::show_message(null_str, msg_running_not_modify2(applets_, task));
		return;
	}


	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::vector<aplt::ttask_item3f> item3fs;

	get_task_item3fs_ex(moveit_driver_, items, item3fs, nposm, false, true);

	if (items.size() < 1) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const aplt::taplt_task* running_klink_aplt_task = bg_task_.klink_task_if_ing();
	if (running_klink_aplt_task != nullptr && 
		running_klink_aplt_task->type == key.type && running_klink_aplt_task->pb_at == key.at) {
		gui2::show_message(null_str, msg_running_not_modify2(applets_, *running_klink_aplt_task));
		return;
	}

	const int cursel = dlg.selected_val();

	const aplt::tapplet* cur_aplt = nullptr;
	const aplt::tapplet::ttask* cur_task = nullptr;
	std::string cur_pin;

	if (cursel > 0) {
		const aplt::ttask_item3f& cur_item = item3fs[cursel - 1];
		cur_aplt = cur_item.aplt;
		cur_task = cur_item.task;
		cur_pin = cur_item.device_id;
	}

	ttoggle_panel& row_panel = list.row_panel(at);
	if (gui_has_2position(task.type)) {
		const std::string position1_key = "position1";
		const std::string position2_key = "position2";

		tbutton* position1_widget = find_widget<tbutton>(&row_panel, position1_key, false, true);
		tbutton* position2_widget = find_widget<tbutton>(&row_panel, position2_key, false, true);
		refresh_2position(*position1_widget, *position2_widget, task, cur_aplt, cur_task);
	}

	aplt::taplt_task val_task;
	val_task.aplt_id = cur_aplt != nullptr? cur_aplt->id: null_str;
	val_task.task_id = cur_task != nullptr? cur_task->id: null_str;
	val_task.ble_device_id = cur_pin;
	val_task.state = aplt::taplt_task::state_fresh;
	uint32_t flags = aplt::taplt_task::FLAG_APLT_ID | aplt::taplt_task::FLAG_TASK_ID | 
		aplt::taplt_task::FLAG_BLE_DEVICE_ID | aplt::taplt_task::FLAG_STATE;
	bg_task_.modify_task(task, val_task, flags, true);

	std::string label;
	if (cur_task != nullptr) {
		label = generate_task2_desc2(task.type, *cur_aplt, *cur_task, cur_pin);
	}
	widget.set_label(label);

	std::string icon_key;
	if (task.type == aplt::taplt_task::type_iot) {
		icon_key = "event_icon";

	} else if (task.type == aplt::taplt_task::type_speech) {
		icon_key = "speech_icon";

	} else {
		VALIDATE(task.type == aplt::taplt_task::type_var, null_str);
		icon_key = "var_icon";
	}
	row_panel.set_child_label(icon_key, get_clock_png(task));

	// row_panel.set_child_label("event_state", timing.task_state_desc(task.state));
}

void tklink::click_task_priority(tlistbox& list, tbutton& widget, int at)
{
	const ttask_key& key = task_keys_[at];
	const aplt::taplt_task& task = bg_task_.task_from_at(key.type, key.at);

	VALIDATE(task.type == aplt::taplt_task::type_iot || task.type == aplt::taplt_task::type_speech, null_str);

	if (task.state == aplt::taplt_task::state_running) {
		gui2::show_message(null_str, msg_running_not_modify2(applets_, task));
		return;
	}

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	for (std::map<int, std::string>::const_iterator it = aplt::nontimed_priorities.begin(); it != aplt::nontimed_priorities.end(); ++ it) {
		int priority = it->first;
		const std::string& name = it->second;
		if (priority == task.priority) {
			initial_sel = priority;
		}
		items.push_back(gui2::tmenu::titem(name, priority));
	}

	if (items.empty()) {
		return;
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int new_priority = dlg.selected_val();
	aplt::taplt_task val_task;
	val_task.priority = new_priority;
	uint32_t flags = aplt::taplt_task::FLAG_PRIORITY;
	bg_task_.modify_task(task, val_task, flags, true);

	reload_task_list(list);
}

void tklink::click_task_position(tlistbox& list, tbutton& widget, tbutton& other, int at, bool p1)
{
	if (!curmap_.valid()) {
		return;
	}

	const ttask_key& key = task_keys_[at];
	const aplt::taplt_task& task = bg_task_.task_from_at(key.type, key.at);

	if (task.state == aplt::taplt_task::state_running) {
		gui2::show_message(null_str, msg_running_not_modify2(applets_, task));
		return;
	}

	std::vector<const tmap_position*> vec_positions;
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	const std::string& task_this_position = p1? task.position1: task.position2;
	const std::string& task_other_position = p1? task.position2: task.position1;

	if (!p1 || task_other_position.empty()) {
		items.push_back(gui2::tmenu::titem(_("Empty"), curmap_.positions.size()));
		initial_sel = task_this_position.empty()? items.back().val: nposm;
	}
	for (std::map<std::string, tmap_position>::const_iterator it = curmap_.positions.begin(); it != curmap_.positions.end(); ++ it) {
		const tmap_position& position = it->second;
		if (position.uuid == task_other_position) {
			continue;
		}
		
		items.push_back(gui2::tmenu::titem(position.name, vec_positions.size()));
		vec_positions.push_back(&position);

		if (position.uuid == task_this_position) {
			VALIDATE(initial_sel == nposm, null_str);
			initial_sel = vec_positions.size() - 1;
		}
	}

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();

	std::string new_position_uuid;
	if (cursel != curmap_.positions.size()) {
		const tmap_position& new_position = *vec_positions[cursel];
		new_position_uuid = new_position.uuid;
	}

	// const aplt::taplt_task* new_task = nullptr;

	aplt::taplt_task val_task;
	uint32_t flags = 0;
	if (p1) {
		val_task.position1 = new_position_uuid;
		flags = aplt::taplt_task::FLAG_POSITION1;

	} else {
		val_task.position2 = new_position_uuid;
		flags = aplt::taplt_task::FLAG_POSITION2;
	}
	bg_task_.modify_task(task, val_task, flags, true);

	const aplt::tapplet* aplt = aplt::aplt_from_id(applets_, task.aplt_id);
	VALIDATE(aplt != nullptr && aplt->tasks.count(task.task_id) != 0, null_str);
	const aplt::tapplet::ttask& cfg_task = aplt->tasks.find(task.task_id)->second;

	refresh_2position(p1? widget: other, p1? other: widget, task, aplt, &cfg_task);
}

//
// scene_layer 
//

//
// device layer
//
void tklink::click_insert_device(tbutton& widget)
{
	VALIDATE(current_layer_ == DEVICE_LAYER, null_str);

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;
    
	for (std::map<int, aplt::tiot_src2>::const_iterator it = aplt::iot_sources.begin(); it != aplt::iot_sources.end(); ++ it) {
		int src = it->first;
		const std::string& name = it->second.name;

		items.push_back(gui2::tmenu::titem(name, src));
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
	std::map<int, aplt::tiot_src2>::const_iterator cur_it = aplt::iot_sources.find(cursel);
	VALIDATE(cur_it != aplt::iot_sources.end(), null_str);

	bool cancel = false;
	const std::string device_id = edit_device_id(null_str, cancel);
	if (cancel) {
		return;
	}

	bg_task_.insert_iot_device(cur_it->first, device_id);

	reload_device_list(*device_list_);
}

static std::string device_row_icon_value(const aplt::tiot_device& device, int64_t now_ms, int online_threshold_minute)
{
	std::string icon_png = device.icon;
	const int online_threshold_ms = online_threshold_minute * 60 * 1000;
	if (device.ts == nposm || (now_ms - device.ts) > online_threshold_ms) {
		icon_png += "~GS()";
	}

	return icon_png;
}

static std::string device_row_desc_value(const aplt::tiot_device& device, int64_t now_ms)
{
	const int one_day_mseconds = ONE_DAY_SECONDS * 1000;
	if (device.ts != nposm || (now_ms - device.ts) < one_day_mseconds) {
		return utils::format_time_date(device.ts / 1000);
	}
	return null_str;
}

void tklink::reload_device_list(tlistbox& list)
{
	// Unlike event_tasks/timed_tasks, all devices in the device list must be valid, 
	// that is, they have a valid device.src and a valid device.device_id.
	list.clear();
	const std::map<aplt::tiot_device_key, aplt::tiot_device>& aliases = bg_task_.iot_devices();

	int64_t now_ms = INT64_C(1000) * time(nullptr);
	std::stringstream ss;
	std::map<std::string, std::string> data;
	for (std::map<aplt::tiot_device_key, aplt::tiot_device>::const_iterator it = aliases.begin(); it != aliases.end(); ++ it) {
		data.clear();
		const aplt::tiot_device& device = it->second;

		data.insert(std::make_pair("src", aplt::iot_sources.find(device.src)->second.name));
		data.insert(std::make_pair("device_id", device.device_id));
		data.insert(std::make_pair("alias", device.alias));
/*
		std::string icon_png = device.icon;
		const int online_threshold_ms = online_threshold_minute_ * 60 * 1000;
		if (device.ts == nposm || (now - device.ts) > online_threshold_ms) {
			icon_png += "~GS()";
		}
		data.insert(std::make_pair("icon", icon_png));

		const int one_day_mseconds = ONE_DAY_SECONDS * 1000;
		if (device.ts != nposm || (now - device.ts) < one_day_mseconds) {
			std::string desc = format_time_date(device.ts / 1000);
			data.insert(std::make_pair("desc", desc));
		}
*/
		data.insert(std::make_pair("icon", device_row_icon_value(device, now_ms, online_threshold_minute_)));
		data.insert(std::make_pair("desc", device_row_desc_value(device, now_ms)));

		list.insert_row(data);
	}
}

bool tklink::did_pins_can_drag(tlistbox& list, ttoggle_panel& row)
{
	const int at = row.at();

	const std::map<aplt::tiot_device_key, aplt::tiot_device>& aliases = bg_task_.iot_devices();
	VALIDATE(at < (int)aliases.size(), null_str);
	std::map<aplt::tiot_device_key, aplt::tiot_device>::const_iterator it = aliases.begin();
	if (at != 0) {
		std::advance(it, at);
	}
	const aplt::tiot_device& alias = it->second;

	std::map<std::string, twidget::tvisible> visibles;

	// bool running = task.state == aplt::tklink::state_running;
	bool running = false;

	visibles.insert(std::make_pair("edit", running? twidget::INVISIBLE: twidget::VISIBLE));
	visibles.insert(std::make_pair("erase", running? twidget::INVISIBLE: twidget::VISIBLE));

	list.left_drag_grid_set_widget_visible(visibles);

	return true;
}

bool tklink::verify_edit_device_id(const std::string& label, const std::string& initial, int min_chars) const
{
	VALIDATE(min_chars > 0, null_str);

	if (label == initial) {
		return false;
	}

	int size = label.size();
	if (size < min_chars) {
		return false;
	}

	const char* c_str = label.c_str();
	for (int at = 0; at < size; at ++) {
		if (c_str[at] & 0x80) {
			return false;
		}
	}

	return true;
}

std::string tklink::edit_device_id(const std::string& initial, bool& cancel) const
{
	cancel = false;
    std::string title = _("Input device ID");
	std::string prefix;

    std::string placeholder = _("Device ID");

	const SDL_Range char_range{IOT_DEVICE_ID_CHARS_MIN, IOT_DEVICE_ID_CHARS_MAX};
	utils::string_map symbols;
	symbols["min"] = str_cast(char_range.min);
	symbols["max"] = str_cast(char_range.max);
    std::string remark = vgettext2("A device ID is used to uniquely identify the device, and provided by the device manufacturer. It is generally a string of letters and numbers, characters is in [$min, $max].", symbols);

	{
		gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), char_range.max, gui2::tedit_box_param::show_cancel);
		param.did_text_changed = std::bind(&tklink::verify_edit_device_id, this, _1, std::ref(initial), char_range.min);
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_->get_height() / 5);
			if (dlg.get_retval() != gui2::twindow::OK) {
				cancel = true;
				return initial;
			}
		}
		return param.result;
	}
}

void tklink::click_edit_alias_name(tlistbox& list)
{
	const int drag_at = list.drag_at();

	const std::map<aplt::tiot_device_key, aplt::tiot_device>& aliases = bg_task_.iot_devices();
	VALIDATE(drag_at < (int)aliases.size(), null_str);
	std::map<aplt::tiot_device_key, aplt::tiot_device>::const_iterator it = aliases.begin();
	if (drag_at != 0) {
		std::advance(it, drag_at);
	}
	const aplt::tiot_device& alias = it->second;

	// first, require cancel left_drag grid.
	list.cancel_drag();

	std::string new_name;
	{
		utils::string_map symbols;
		symbols["src"] = aplt::iot_sources.find(alias.src)->second.name;
		symbols["device_id"] = alias.device_id;

        std::string title = vgettext2("Edit $src($device_id)'s name", symbols);
	    std::string prefix;
        std::string placeholder;
        const std::string initial = alias.alias;

		int max_chars = MAX_NORMAL_UTF8_NAME_CHARS;
		symbols["max_chars"] = str_cast(max_chars);
        std::string remark = vgettext2("Easy name, can be Chinese, and maximum $max_chars characters", symbols);

		std::set<std::string> excludes;

	    {
		    gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), max_chars, gui2::tedit_box_param::show_cancel);
		    param.did_text_changed = std::bind(&tklink::verify_edit_alias_name, this, _1, std::ref(initial), etype_iot_alias, std::ref(excludes));
		    {
			    gui2::tedit_box dlg(param);
			    dlg.show(nposm, window_->get_height() / 5);
			    if (dlg.get_retval() != gui2::twindow::OK) {
				    return;
			    }
		    }
		    new_name = param.result;
	    }
	}

	VALIDATE(new_name != alias.alias, null_str);

	bg_task_.modify_iot_device(alias.src, alias.device_id, new_name, null_str, nposm, aplt::tiot_device::FLAG_ALIAS);
	it = aliases.begin();
	if (drag_at != 0) {
		std::advance(it, drag_at);
	}
	const aplt::tiot_device& new_alias = it->second;

	// Although it modifies a task, it may cause the list sort to change
	// reload_device_list(list);

	// if want to execute below, require cancel left_drag grid.
	ttoggle_panel& row = list.row_panel(drag_at);
	row.set_child_label("alias", new_alias.alias);
}

void tklink::click_erase_device(tlistbox& list)
{
	const int drag_at = list.drag_at();

	const std::map<aplt::tiot_device_key, aplt::tiot_device>& aliases = bg_task_.iot_devices();
	VALIDATE(drag_at < (int)aliases.size(), null_str);
	std::map<aplt::tiot_device_key, aplt::tiot_device>::const_iterator it = aliases.begin();
	if (drag_at != 0) {
		std::advance(it, drag_at);
	}
	const aplt::tiot_device& alias = it->second;

	// first, require cancel left_drag grid.
	list.cancel_drag();

	utils::string_map symbols;
	symbols["src"] = aplt::iot_sources.find(alias.src)->second.name;
	symbols["device_id"] = alias.device_id;
	const std::string msg = vgettext2("Do you want to delete device '$src($device_id)'?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	bg_task_.erase_iot_device(alias.src, alias.device_id);
	reload_device_list(list);
}

//
// var_sensor layer
//
/*
void tklink::click_import(tbutton& widget)
{
	std::set<std::string> files;
	collect_klink_cfg_files(cfgtype_klink, files);

	std::string errmsg;
	tauto_destruct_executor destruct_executor(std::bind(&did_post_show_err_message, std::ref(errmsg)));

	const tcfg_4field& cfg_4field = klink_cfgs.find(cfgtype_klink)->second;

	utils::string_map symbols;
	symbols["type_cfg"] = cfg_4field.name;
	if (files.empty()) {
		symbols["dir"] = get_saves_dir();
		errmsg = vgettext2("In $dir, there are no $klink_cfg files that conform to the file name format", symbols);
		return;
	}

	std::string filename;
	{
		std::vector<gui2::tmenu::titem> items;
		int initial_sel = nposm;

		for (std::set<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
			const std::string& file = *it;
			std::stringstream name;
			if (file.find(game_config::path) == 0) {
				name << "(" << _("Buildin") << ")";
			}
			name << utils::extract_file(file);
			items.push_back(gui2::tmenu::titem(name.str(), items.size()));
		}

		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		int new_val = dlg.selected_val();
		std::set<std::string>::const_iterator hit_it = files.begin();
		if (new_val != 0) {
			std::advance(hit_it, new_val);
		}
		filename = *hit_it;
	}

	const std::string action = _("Import");
	symbols["action"] = action;

	if (bg_task_.is_ing()) {
		std::string warnning = vgettext2("A bg task is running and it needs to be stopped first. Do you want to continue $action?", symbols);
		const std::string log = vgettext2("To $action, stop the task", symbols);
		if (!instance->stop_bg_task_if_runing(warnning, log)) {
			return;
		}
	}

	errmsg = cfg_cpp_api_.import_klink_cfg(filename);
	if (!errmsg.empty()) {
		return;
	}

	set_title_label();
	reload_task_list(*task_list_);
}

void tklink::click_export(tbutton& widget)
{
	VALIDATE(current_layer_ == TASK_LAYER, null_str);

	utils::string_map symbols;
	std::stringstream out;
	std::string cfg_filename = "var_sensor_export.cfg";
	const std::vector<aplt::tvar_sensor>& var_sensors = cfg_cpp_api_.var_sensors();
	const std::vector<aplt::ttimed_sensor>& timed_sensors = cfg_cpp_api_.timed_sensors();

	const std::string scene_name = bg_task_.scene_name();
	if (current_layer_ == TASK_LAYER) {
		cfg_filename = scene_name_2_klink_cfg_name(scene_name);
	}

	const std::string filename = game_config::preferences_dir + "/saves/" + cfg_filename;
	if (SDL_IsFile(filename.c_str())) {
		symbols["filename"] = cfg_filename;
		const std::string msg = vgettext2("$filename, do you want to overwrite a file with the same name?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}
	}

	if (current_layer_ == TASK_LAYER) {
		const tcfg_4field& cfg_4field = klink_cfgs.find(cfgtype_klink)->second;

		config top_cfg;
		top_cfg["type"] = cfg_4field.id;
		top_cfg["scene_name"] = scene_name;

		// 1/4: 4 tasks --> cfg
		const std::map<aplt::tiot_device_key, aplt::tiot_device>& iot_devices = bg_task_.iot_devices();

		std::map<aplt::tiot_device_key, std::string> aliases;

		// When there are add_timed_tasks, some of the timed_tasks may belong to the add_timed_tasks. 
		// During export, these tasks must be excluded. The @timed_ats stores these timed_tasks.
		// https://www.cswamp.com/post/345
		std::set<int> timed_ats;

		int klink_types[] = {aplt::taplt_task::type_iot, aplt::taplt_task::type_speech, aplt::taplt_task::type_var, aplt::taplt_task::type_timed};
		int type_count = sizeof(klink_types) / sizeof(klink_types[0]);
		for (int type_at = 0; type_at < type_count; type_at ++) {
			int type = klink_types[type_at];
			const std::map<aplt::taplt_task_key, aplt::taplt_task>& tasks = bg_task_.get_tasks(type);
			int at = 0;
			for (std::map<aplt::taplt_task_key, aplt::taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it, at ++) {
				const aplt::taplt_task& task = it->second;
				std::string alias;
				if (type == aplt::taplt_task::type_iot) {
					aplt::tiot_device_key key(task.iot_src, task.src_device_id);
					if (iot_devices.count(key) != 0) {
						alias = iot_devices.find(key)->second.alias;
						aliases.insert(std::make_pair(key, alias));
					}
				}
				if (task.type != aplt::taplt_task::type_timed) {
					task.to_cfg(top_cfg);

				} else if (!cfg_cpp_api_.aplt_task_is_from_add_timed_tasks(task)) {
					// Extract timed_tasks that are not add_timed_tasks.
					aplt::taplt_task task2 = task;
					task2.timed_at = timed_ats.size();
					task2.to_cfg(top_cfg);

					timed_ats.insert(task.timed_at);
				}
			}
		}

		// 2/4: aliases --> cfg
		for (std::map<aplt::tiot_device_key, aplt::tiot_device>::const_iterator it = iot_devices.begin(); it != iot_devices.end(); ++ it) {
			const aplt::tiot_device& iot_device = it->second;
			config& iot_device_cfg = top_cfg.add_child("iot_device");
			iot_device_to_cfg(iot_device, iot_device_cfg);
		}
		
		// 3/4: var sensors --> cfg
		cfg_cpp_api_.var_sensors_to_cfg(var_sensors, top_cfg);

		// 4/4: timed sensors --> cfg
		cfg_cpp_api_.timed_sensors_to_cfg(timed_sensors, timed_ats, top_cfg);

		const std::vector<aplt::tbase_scene>& base_scenes = cfg_cpp_api_.base_scenes();
		cfg_cpp_api_.base_scenes_to_cfg(base_scenes, top_cfg);

		aplt::add_timed_tasks_to_cfg(cfg_cpp_api_.add_timed_tasks(), top_cfg);

		cfg_cpp_api_.courselist().to_cfg(top_cfg);

		write(out, top_cfg);

	} else {
		VALIDATE(false, null_str);
		// VALIDATE(current_layer_ == VAR_SENSOR_LAYER, null_str);
		cfg_cpp_api_.var_sensors_to_stringstream(var_sensors, out);
	}

	if (!out.str().empty()) {
		write_file(filename, out.str().c_str(), out.str().size());

		symbols["file"] = filename;
		gui2::show_message(null_str, vgettext2("Export finished. file: $file", symbols));
	}
}

//
// env_var
//
void tklink::click_refresh(tbutton& widget)
{
	reload_env_var_list(*env_var_list_);
}

void tklink::reload_env_var_list(tlistbox& list)
{
	list.clear();

	const std::vector<aplt::tvar_sensor>& sensors = cfg_cpp_api_.var_sensors();
	std::stringstream ss;
	std::map<std::string, std::string> data;
	const aplt::ttask_cpp_pair pair;
	int at = 0;
	for (std::map<int, tcode3>::const_iterator it = aplt::BI_env_vars.begin(); it != aplt::BI_env_vars.end(); ++ it) {
		const tcode3& code3 = it->second;

		data["type"] = str_cast(code3.code);

		std::pair<std::string, std::string> pair = utils::split_app_prefix_id(code3.id);
		VALIDATE(pair.first == aplt::fake_aplt.bundleid, null_str);
		data["name"] = pair.second;

		const aplt::ttask_var* var = aplt::get_env_var(code3.id);
		ss.str("");
		if (var != nullptr) {
			ss << var->val.str();
		} else {
			ss << _("Not created");
		}
		data["val"] = ss.str();

		data["desc"] = code3.name;

		list.insert_row(data);
	}
}
*/

//
// add_timed_task
//
void tklink::click_clear(tbutton& widget)
{
	VALIDATE(current_layer_ == ADD_TIMED_TASK_LAYER, null_str);
	VALIDATE(!cfg_cpp_api_.add_timed_tasks().empty(), null_str);

	const std::string action = _("clear add timed task list");
	utils::string_map symbols;
	symbols["action"] = action;

	if (bg_task_.is_ing()) {
		std::string warnning = vgettext2("A bg task is running and it needs to be stopped first. Do you want to continue $action?", symbols);
		const std::string log = vgettext2("To $action, stop the task", symbols);
		if (!instance->stop_bg_task_if_runing(warnning, log)) {
			return;
		}

	} else {
		const std::string msg = vgettext2("Do you want to $action?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}
	}

	cfg_cpp_api_.clear_add_timed_tasks();
	reload_add_timed_task_list(*add_timed_task_list_);
	clear_widget_->set_active(false);
}

void tklink::reload_add_timed_task_list(tlistbox& list)
{
	list.clear();

	const std::map<int64_t, aplt::tb_api::tadd_timed_task>& tasks = cfg_cpp_api_.add_timed_tasks();
	std::stringstream ss;
	std::map<std::string, std::string> data;
	const aplt::ttask_cpp_pair pair;
	int at = 0;
	for (std::map<int64_t, aplt::tb_api::tadd_timed_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it) {
		const aplt::tb_api::tadd_timed_task& task = it->second;

		data.clear();

		std::string aplt_name = task.aplt_id;
		std::string task_name = task.task_id;
		aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, task.aplt_id, task.task_id, true, false);
		if (pair.aplt != nullptr) {
			aplt_name = pair.aplt->id;
		}
		if (pair.task != nullptr) {
			task_name = pair.task->name;
		}

		data.insert(std::make_pair("timed_task", timed_task_name_label(task_name)));

		ss.str("");
		ss << aplt_name;
		if (!task.ble_device_id.empty()) {
			ss << "-" << task.ble_device_id;
		}
		data.insert(std::make_pair("timed_aplt", ss.str()));
		data.insert(std::make_pair("timed_input_vars", input_vars_label(task.input_vars, false)));
		data.insert(std::make_pair("timed_time", utils::format_time_ymdhms(task.time)));


		ss.str("");
		if (!task.input_vars.empty()) {
			ss << "(" << task.input_vars.size() << ")";
		}
		for (std::map<std::string, std::string>::const_iterator it = task.input_vars.begin(); it != task.input_vars.end(); ++ it) {
			const std::string& name = it->first;
			const std::string& val = it->second;
			if (it != task.input_vars.begin()) {
				ss << ", ";
			}
			ss << name << "=" << val;
		}
		data["input_vars"] = ss.str();

		ss.str("");
		ss << (task.added? _("Added"): "--") << "/" << (task.deleted? _("Deleted"): "--");
		data["add_delete"] = ss.str();

		list.insert_row(data);
	}
}

std::string tklink::get_clock_png(const aplt::taplt_task& task)
{
	std::string clock_png;

	if (task.type == aplt::taplt_task::type_iot) {
		clock_png = "misc/event_green_trigger.png";
		if (task.state == aplt::taplt_task::state_running) {
			clock_png = "misc/event_red_trigger.png";
		} else if (task.aplt_id.empty()) {
			clock_png = "misc/event_gray_trigger.png";
		}

	} else if (task.type == aplt::taplt_task::type_speech) {
		clock_png = "misc/speech_green_trigger.png";
		if (task.state == aplt::taplt_task::state_running) {
			clock_png = "misc/speech_red_trigger.png";
		} else if (task.aplt_id.empty()) {
			clock_png = "misc/speech_gray_trigger.png";
		}

	} else if (task.type == aplt::taplt_task::type_timed) {
		clock_png = "misc/time_green.png";
		if (bg_task_.state_can_fresh(task.state)) {
			clock_png = "misc/time_gray.png";

		} else if (bg_task_.state_is_closed(task.state)) {
			clock_png = "misc/time_red.png";
		}

	} else {
		clock_png = "misc/var_green_trigger.png";
		if (task.state == aplt::taplt_task::state_running) {
			clock_png = "misc/var_red_trigger.png";
		} else if (task.aplt_id.empty()) {
			clock_png = "misc/var_gray_trigger.png";
		}
	}

	return clock_png;
}

int tklink::row_from_key(int type, int priority, int at) const
{
	// Although there are 'alias' fields in the ttask_key, these three fields alone can be uniquely located, 
	// and 'alia's only affect the sorting.
	// Don't even need 'priority'.
	int row_at = 0;
	for (std::vector<ttask_key>::const_iterator it = task_keys_.begin(); it != task_keys_.end(); ++ it, row_at ++) {
		const ttask_key& key = *it;
		if (key.type == type && key.priority == priority && key.at == at) {
			return row_at;
		}
	}

	VALIDATE(false, null_str);
	return nposm;
}

void tklink::aplt_task_state_changed(bool is_task_cpp, const aplt::taplt_task& task, bool started, int stopped_state)
{
	VALIDATE(task.is_klink(), null_str);
	int hit_at = nposm;
	int at = 0;
	const std::map<aplt::taplt_task_key, aplt::taplt_task>& tasks = bg_task_.get_tasks(task.type);
	for (std::map<aplt::taplt_task_key, aplt::taplt_task>::const_iterator it = tasks.begin(); it != tasks.end(); ++ it, at ++) {
		const aplt::taplt_task& that = it->second;
		if (&task == &that) {
			hit_at = at;
			break;
		}
	}
	VALIDATE(hit_at != nposm && hit_at < (int)tasks.size(), null_str);

	// now maybe in draging, first cancel drag.
	task_list_->cancel_drag();

	// if want to execute below, require cancel left_drag grid.
	int row_at = row_from_key(task.type, task.priority, hit_at);
	VALIDATE(row_at >= 0 && row_at < task_list_->rows(), null_str);

	ttoggle_panel& row = task_list_->row_panel(row_at);

	std::string icon_key = "event_icon";
	std::string state_key = "event_state";
	if (task.type == aplt::taplt_task::type_speech) {
		icon_key = "speech_icon";
		state_key = "speech_state";

	} else if (task.type == aplt::taplt_task::type_var) {
		icon_key = "var_icon";
		state_key = "var_state";

	} else if (task.type == aplt::taplt_task::type_timed) {
		icon_key = "timed_icon";
		state_key = "timed_state";
	}

	aplt::taplt_task task2 = task;
	// if (!started && is_task_cpp) {
	if (!started) {
		// here is execute before 'tbg_task2::task_finished', that will set task.state.
		// It's a bit difficult to switch the order, so let's just give a 'state' value here.
		// task2.state = aplt::taplt_task::state_finished_ok;

		VALIDATE(stopped_state != nposm, null_str);
		task2.state = stopped_state;
	} else {
		VALIDATE(stopped_state == nposm, null_str);
	}

	row.set_child_label(icon_key, get_clock_png(task2));

	row.set_child_label(state_key, bg_task_.task_state_desc(task2.state));
}

void tklink::bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	const aplt::taplt_task* aplt_task = nullptr;
	bool is_task_cpp = true;
	if (sys_task.klink_cpp_aplt_task2 != nullptr) {
		if (sys_task.in_pure_task_cpp()) {
			aplt_task = sys_task.klink_cpp_aplt_task2;
		}

	} else if (sys_task.klink_single_aplt_task2 != nullptr) {
		aplt_task = sys_task.klink_single_aplt_task2;
		is_task_cpp = false;
	}

	if (aplt_task == nullptr) {
		return;
	}
	aplt_task_state_changed(is_task_cpp, *aplt_task, true);
}

void tklink::bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	const aplt::taplt_task* aplt_task = nullptr;
	bool is_task_cpp = true;
	int stopped_state = aplt::taplt_task::state_finished_ok;
	if (sys_task.klink_cpp_aplt_task2 != nullptr) {
		if (sys_task.in_pure_task_cpp()) {
			aplt_task = sys_task.klink_cpp_aplt_task2;
		}

	} else if (sys_task.klink_single_aplt_task2 != nullptr) {
		aplt_task = sys_task.klink_single_aplt_task2;
		is_task_cpp = false;

		VALIDATE(sys_task.aplt_task != nullptr, null_str);
		stopped_state = sys_task.aplt_task->state;
	}

	if (aplt_task == nullptr) {
		return;
	}
	aplt_task_state_changed(is_task_cpp, *aplt_task, false, stopped_state);
}

void tklink::iot_did_heartbeats2(const std::set<aplt::tiot_heartbeat>& heartbeats)
{
}

void tklink::iot_did_events2(const std::set<aplt::tiot_event>& events)
{
	if (current_layer_ != DEVICE_LAYER) {
		return;
	}

	const std::map<aplt::tiot_device_key, aplt::tiot_device>& iot_devices = bg_task_.iot_devices();
	tlistbox& list = *device_list_;
	if (iot_devices.size() != list.rows()) {
		VALIDATE((int)iot_devices.size() > list.rows(), null_str);
		reload_device_list(list);
		return;
	}

	const std::string icon_key = "icon";
	const std::string desc_key = "desc";
	int64_t now_ms = INT64_C(1000) * time(nullptr);

	std::map<aplt::tiot_device_key, aplt::tiot_device>::const_iterator device_it;
	for (std::set<aplt::tiot_event>::const_reverse_iterator it = events.rbegin(); it != events.rend(); ++ it) {
		const aplt::tiot_event& evt = *it;

		aplt::tiot_device_key key(evt.src, evt.device_id);
		VALIDATE(iot_devices.count(key) != 0, nullptr);

		device_it = iot_devices.find(key);
		int at = std::distance(iot_devices.begin(), device_it);
		if (at == list.drag_at()) {
			// require cancel left_drag grid.
			list.cancel_drag();
		}
		const aplt::tiot_device& device = device_it->second;

		ttoggle_panel& row = list.row_panel(at);

		row.set_child_label(icon_key, device_row_icon_value(device, now_ms, online_threshold_minute_));
		row.set_child_label(desc_key, device_row_desc_value(device, now_ms));
	}
}

void tklink::klink_tasks_changed2()
{
	if (current_layer_ == TASK_LAYER) {
		reload_task_list(*task_list_);
	}
}

void tklink::app_get_task_item3fs(std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, uint32_t allow_type_mask, uint32_t deny_type_mask, bool allow_empty)
{
	uint32_t deny_type_mask2 = deny_type_mask == nposm? 0: deny_type_mask;
	if (!moveit_driver_.installed()) {
		deny_type_mask2 |= BIT_IDX_MASK(aplt::task_moveit);
	}
	get_task_item3fs(items, item3fs, allow_type_mask, deny_type_mask2, allow_empty);
}

void tklink::app_post_click_import()
{
	reload_task_list(*task_list_);
}

void tklink::app_pre_click_export()
{
	VALIDATE(current_layer_ == TASK_LAYER, null_str);
}

void tklink::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

