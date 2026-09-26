#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/dlg_utils.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/edit_box.hpp"
// #include "gui/dialogs/if_block2.hpp"
#include "gui/dialogs/combo_box2.hpp"
#include "gui/dialogs/insert_scene.hpp"
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
#include "wkoscript.hpp"

// using namespace std::placeholders;

namespace aplt {

extern void iot_device_to_cfg(const aplt::tiot_device& device, config& cfg);

}
/*
std::string scene_get_file_val(const aplt::tbase_scene& scene)
{
	const std::string file_key = "file";
	std::string file;

	if (scene.input_vars.count(file_key) != 0) {
		file = scene.input_vars.find(file_key)->second;
	}
	return file;
}
*/
namespace gui2 {

#define MAX_USE_SIMPLE_SCNE_DRAG_MENU_DOTS		450

thelper_klink::thelper_klink(trvar_editor::tslot& var_editor_slot, std::map<aplt::taplt_key, aplt::tapplet>& applets, 
	aplt::tcfg_cpp_api_core& cfg_cpp_api, aplt::tbg_task& bg_task, tbase_driver_core& base_driver)
	: pinyin_(aplt::get_curr_pinyin())
	, var_editor_slot_(var_editor_slot)
	, applets_(applets)
	, cfg_cpp_api_(cfg_cpp_api)
	, bg_task_(bg_task)
	, base_driver_(base_driver)
	// , online_threshold_minute_(1) // 10 minutes
	, title_widget_(nullptr)
	// , report_(nullptr)
	// , stack_(nullptr)
	// , insert_event_widget_(nullptr)
	// , insert_speech_widget_(nullptr)
	// , insert_var_widget_(nullptr)
	// , insert_timed_widget_(nullptr)
	, import_widget_(nullptr)
	, export_widget_(nullptr)
	// , insert_alias_widget_(nullptr)
	, insert_scene_widget_(nullptr)
	, refresh_widget_(nullptr)
	// , clear_widget_(nullptr)
	// , task_list_(nullptr)
	// , device_list_(nullptr)
	, scene_list_(nullptr)
	// , var_sensor_list_(nullptr)
	, env_var_list_(nullptr)
	// , add_timed_task_list_(nullptr)
	// , current_layer_(nposm)
	, auto_edit_workout_id_or_name_(true)
	, simple_scene_drag_menu_(false)
	, window_priv_(nullptr)
{
	int dots = round_double(gui2::settings::screen_width / gui2::twidget::hdpi_scale);
	if (dots < MAX_USE_SIMPLE_SCNE_DRAG_MENU_DOTS) {
		simple_scene_drag_menu_ = true;
	}
	// set_timer_interval(1000);
}

void thelper_klink::pre_show(twindow& window)
{
	window_priv_ = &window;
	// window_->set_label("misc/bg_ffffff.png");

	// tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	tbutton* button = find_widget<tbutton>(&window, "title", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&thelper_klink::click_title
			, this, std::ref(*button)));
	title_widget_ = button;
	set_title_label();

	button = find_widget<tbutton>(&window, "import", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&thelper_klink::click_import
			, this, std::ref(*button)));
	import_widget_ = button;

	button = find_widget<tbutton>(&window, "export", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&thelper_klink::click_export
			, this, std::ref(*button)));
	export_widget_ = button;

	button = find_widget<tbutton>(&window, "insert_scene", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&thelper_klink::click_insert_scene
			, this, std::ref(*button)));
	insert_scene_widget_ = button;

		button = find_widget<tbutton>(&window, "refresh", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&thelper_klink::click_refresh
			, this, std::ref(*button)));
	refresh_widget_ = button;
/*
	//
	//
	// tgrid* grid = find_widget<tgrid>(&window, "main_grid", false, true);
	treport* report = find_widget<treport>(&window, "__timing_report", false, true);
	report_ = report;
	stack_ = find_widget<tstack>(&window, "__timing_stack", false, true);

	pre_scene(*stack_->layer(SCENE_LAYER));
*/
}


void thelper_klink::pre_scene(tgrid& grid)
{
	utils::string_map symbols;
	// symbols["online_minute"] = str_cast(online_threshold_minute_);
	// find_widget<tlabel>(&grid, "remark", false, true)->set_label(vgettext2("klink's scene layer remark, $online_minute", symbols));

	ttoggle_button* toggle = find_widget<ttoggle_button>(&grid, "auto_edit_workout_id_or_name", false, true);
	toggle->set_value(auto_edit_workout_id_or_name_);
	toggle->set_did_state_changed(std::bind(&thelper_klink::did_auto_edit_workout_id_or_name_changed, this, _1));

	// why don't use toggle's label?
	// --this msg is too long, for short-width, can't fit in one line.
	tlabel* label = find_widget<tlabel>(&grid, "auto_edit_workout_label", false, true);
	symbols["workout"] = _("task^Workout");
	std::string msg = vgettext2("For '$workout' task, use auto-modify when editing ID and name.", symbols);
	label->set_label(msg);

	scene_list_ = find_widget<tlistbox>(&grid, "scene_list", false, true);
	tlistbox& list = *scene_list_;
	list.enable_select(false);
	list.set_did_can_drag(std::bind(&thelper_klink::did_scene_can_drag, this, _1, _2));


	tbutton* button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("edit_name", true));
	button->set_icon("misc/bg_ff0000.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&thelper_klink::click_edit_scene_id_or_name
			, this
			, std::ref(list), etype_scene_name));

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("edit_amp", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&thelper_klink::click_edit_amp
			, this
			, std::ref(list), std::ref(*button)));

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("edit_input_vars", true));
	button->set_icon("misc/bg_ff0000.png");
	if (!simple_scene_drag_menu_) {
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&thelper_klink::click_edit_scene_input_vars
				, this
				, std::ref(list)));
	} else {
		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&thelper_klink::click_edit_scene_4item
				, this
				, std::ref(list), std::ref(*button)));
		button->set_label(_("Edit"));
	}

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("erase", true));
	button->set_icon("misc/bg_f3f3f3.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&thelper_klink::click_erase_scene
			, this
			, std::ref(list)));

	button = dynamic_cast<tbutton*>(list.left_drag_grid()->find("start", true));
	button->set_icon("misc/bg_ff0000.png");
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&thelper_klink::click_start_scene
			, this
			, std::ref(list)));
}

void thelper_klink::pre_env_var(tgrid& grid)
{
	utils::string_map symbols;
	symbols["prefix"] = aplt::fake_aplt.bundleid + "__";
	std::string msg = vgettext2("For variable name, the prefix '$prefix' is omitted when displayed here", symbols);
	find_widget<tlabel>(&grid, "remark", false, true)->set_label(msg);

	env_var_list_ = find_widget<tlistbox>(&grid, "var_list", false, true);
	tlistbox& list = *env_var_list_;
	list.enable_select(false);
	// list.set_did_can_drag(std::bind(&tklink::did_var_sensors_can_drag, this, _1, _2));
}

void thelper_klink::set_title_label()
{
	const std::string& aplt_name = aplt::all_fake_applets.find(aplt::builtinid_klink)->second.name;

	char label[80];
	SDL_snprintf(label, sizeof(label), "%s(%s)", aplt_name.c_str(), bg_task_.scene_name().c_str());

	title_widget_->set_label(label);
}

bool thelper_klink::verify_klink_scene_name(const std::string& label, const std::string& initial) const
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

void thelper_klink::click_title(tbutton& widget)
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
		param.did_text_changed = std::bind(&thelper_klink::verify_klink_scene_name, this, _1, std::ref(initial));
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_priv_->get_height() / 5);
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
/*
std::string task_name2_from_3id(std::map<aplt::taplt_key, aplt::tapplet>& applets, 
	const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, bool aplt_id_is_bundleid)
{
	const aplt::tapplet* hit_aplt = aplt_id_is_bundleid? aplt_from_bundleid_ex(applets, aplt_id): aplt_from_id_ex(applets, aplt_id);

	const aplt::tapplet::ttask* cfg_task = nullptr;
	std::string task_name = task_id;
	if (hit_aplt != nullptr && hit_aplt->tasks.count(task_id) != 0) {
		cfg_task = &hit_aplt->tasks.find(task_id)->second;
		task_name = cfg_task->name;
	}

	std::string aplt_name;
	if (hit_aplt != nullptr) {
		if (cfg_task != nullptr) {
			aplt_name = hit_aplt->name2();
		} else {
			aplt_name = hit_aplt->name2() + _("[Task absent]");
		}

	} else {
		aplt_name = aplt_id + _("[Unloaded]");
	}


	utils::string_map symbols;
	std::stringstream ss;
	ss << aplt_name<< "(" << task_name << ")";
	if (!ble_device_id.empty()) {
		ss << "-" << ble_device_id;
	}
	return ss.str();
}

static std::string task_name2_from_aplt_task(std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::taplt_task& task)
{
	return task_name2_from_3id(applets, task.aplt_id, task.task_id, task.ble_device_id, false);
}
*/
std::string thelper_klink::msg_running_not_modify(const aplt::tbg_task& bg_task, bool dlg) const
{
	VALIDATE(bg_task.is_ing(), null_str);

	std::string msg = _("Background task is running, and cannot be modified.");
	if (dlg) {
		gui2::show_message(null_str, msg);
	}
	return msg;
}

//
// scene_layer 
//
#define MAX_KLINK_BASE_SCENE		20

std::string unique_scene_id(const std::set<std::string>& ids)
{
	const std::string uuid = utils::create_uuid(false);
	const std::string prefix = uuid.substr(0, 7);
	int number = 1;
	std::stringstream ss;

	while (true) {
		ss.str("");
		ss << prefix << number;

		if (ids.count(ss.str()) == 0) {
			break;
		}
		number ++;
	}
	return ss.str();
}

void thelper_klink::click_insert_scene(tbutton& widget)
{
	if (is_wkocourse_scene_ing(wkocourse_blocktype_insert_scene, true)) {
		return;
	}

	std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.mutable_base_scenes();
	if (scenes.size() >= MAX_KLINK_BASE_SCENE) {
		utils::string_map symbols;
		symbols["max"] = str_cast(MAX_KLINK_BASE_SCENE);
		std::string err = vgettext2("Supports up to $max base scenes", symbols);
		gui2::show_message(null_str, err);
		return;
	}

	aplt::tbase_scene scene;
	if (game_config::app_code == aplt::app_kdesktop) {
		gui2::tinsert_scene dlg(var_editor_slot_, applets_, cfg_cpp_api_);
		dlg.show(nposm, window_priv_->get_height() / 5);
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
		scene = dlg.get_scene();

		int scene_count = scenes.size();
		for (int at = 0; at < scene_count; at ++) {
			const aplt::tbase_scene& that = scenes[at];
			VALIDATE(that.get_id() != scene.get_id(), null_str);
			VALIDATE(that.name() != scene.name(), null_str);
		}

	} else {
		scene.set_name(_("Untitle"));
		if (game_config::app_code == aplt::app_launcher) {
			scene.aplt = aplt::get_bundleid(aplt::bundleid_leagor_khome);
		} else {
			scene.aplt = aplt::get_bundleid(aplt::bundleid_leagor_khomelua);
		}
		scene.task = aplt::reserved_tasks.find(aplt::taskid_workout)->second.id;

		if (cfg_cpp_api_.base_scene_from_id(scene.get_id(), false) != nullptr) {
			return;
		}
	}

	scenes.push_back(scene);
	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_base_scene);

	reload_scene_list(*scene_list_);
}

void thelper_klink::click_scene_task(tlistbox& list, tbutton& widget, int at)
{
	std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.mutable_base_scenes();
	aplt::tbase_scene& scene = scenes[at];

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::vector<aplt::ttask_item3f> item3fs;

	app_get_task_item3fs(items, item3fs, BIT_IDX_MASK(aplt::task_camera), nposm, false);

	int at2 = 0;
	for (std::vector<aplt::ttask_item3f>::const_iterator it = item3fs.begin(); it != item3fs.end(); ++ it, at2 ++) {
		const aplt::ttask_item3f& item3f = *it;
		if (item3f.aplt->bundleid == scene.aplt && item3f.task->id == scene.task) {
			initial_sel = at2;
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

	const aplt::tapplet* cur_aplt = nullptr;
	const aplt::tapplet::ttask* cur_task = nullptr;
	std::string cur_ble_device_id;

	// if (cursel > 0) {
		// const aplt::ttask_item3f& cur_item = item3fs[cursel - 1];
		const aplt::ttask_item3f& cur_item = item3fs[cursel];
		cur_aplt = cur_item.aplt;
		cur_task = cur_item.task;
		cur_ble_device_id = cur_item.device_id;
	// }

	bool is_me = scene.get_id() == preferences::base_scene_id();
	const bool restart = is_me;
	if (restart && bg_task_.is_ing()) {
		msg_running_not_modify(bg_task_, true);
		return;
	}

	ttoggle_panel& row_panel = list.row_panel(at);

	scene.aplt = cur_aplt->bundleid;
	scene.task = cur_task->id;
	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_base_scene);

	if (restart) {
		base_driver_.restart_subtask();

		std::string state;
		if (base_driver_.subtask_state() != aplt::sts_nposm) {
			state = aplt::base_subtask_states[base_driver_.subtask_state()];

		} else {
			state = _("Start fail");
		}
		
		row_panel.set_child_label("state", state);
	}

	std::string label;
	if (cur_task != nullptr) {
		label = generate_task2_desc2(nposm, *cur_aplt, *cur_task, cur_ble_device_id);
	}
	widget.set_label(label);
}

void thelper_klink::did_auto_edit_workout_id_or_name_changed(ttoggle_button& widget)
{
	auto_edit_workout_id_or_name_ = widget.get_value();
}

void thelper_klink::reload_scene_list(tlistbox& list)
{
	// Unlike event_tasks/timed_tasks, all devices in the device list must be valid, 
	// that is, they have a valid device.src and a valid device.device_id.
	list.clear();

	const std::string pref_scene_id = preferences::base_scene_id();
	const std::string& driver_scene_id = base_driver_.scene_id();

	const std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.base_scenes();
	std::stringstream ss;
	std::map<std::string, std::string> data;
	for (std::vector<aplt::tbase_scene>::const_iterator it = scenes.begin(); it != scenes.end(); ++ it) {
		// data.clear();
		const aplt::tbase_scene& scene = *it;

		data["name"] = scene.name_for_gui(); // scene.name();
		std::string label = task_name2_from_3id(applets_, scene.aplt, scene.task, null_str, true);
		// when pc, at lest show: aplt.leagor.khomelua(studio)(x
		int max_chars = game_config::mobile? 27: 30; 
		data["task"] = utils::truncate_to_max_chars2(label, max_chars, false);
		VALIDATE(scene.amp >= 0 && scene.amp < ampmode_count, null_str);
		data["amp"] = aplt::amp_modes.find(scene.amp)->second.id;
		data["input_vars"] = scene.join_input_vars();

		std::string png = "misc/running_gray.png";
		bool is_me = scene.get_id() == driver_scene_id;
		if (is_me) {
			png = sts_is_idle2(base_driver_.subtask_state())? "misc/running_green.png": "misc/running_red.png";
		}
		data["icon"] = png;

		std::string state;
		if (!base_driver_.installed()) {

		} else if (is_me) {
			state = aplt::base_subtask_states[base_driver_.subtask_state()];

		} else if (scene.get_id() == pref_scene_id && driver_scene_id.empty()) {
			if (!base_driver_.slot->moveable()) {
				state = _("Start fail");
			}

		}
		data["state"] = state;
		
		ttoggle_panel& row = list.insert_row(data);

		tbutton* button = find_widget<tbutton>(&row, "task", false, true);
		// if (cfg_task != nullptr) {
		//	task2_widget->set_label(generate_task2_desc2(task.type, *hit_aplt, *cfg_task, task.ble_device_id));
		// }

		connect_signal_mouse_left_click(
			*button
			, std::bind(
				&thelper_klink::click_scene_task
				, this
				, std::ref(list), std::ref(*button), row.at()));
		if (game_config::app_code == aplt::app_kdesktop) {
			// button->set_text_font_size(font::SIZE_SMALL);
			button->set_text_font_size(font::SIZE_SMALLER);

			find_widget<tlabel>(&row, "name", false, true)->set_text_font_size(font::SIZE_SMALL);
		}
	}
}

bool thelper_klink::did_scene_can_drag(tlistbox& list, ttoggle_panel& row)
{
	const int at = row.at();

	const std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.base_scenes();
	const aplt::tbase_scene& scene = scenes[at];

	std::map<std::string, twidget::tvisible> visibles;

	tgrid& drag_grid = *list.left_drag_grid();
	if (simple_scene_drag_menu_) {
		visibles.insert(std::make_pair("edit_id", twidget::INVISIBLE));
		visibles.insert(std::make_pair("edit_name", twidget::INVISIBLE));
		visibles.insert(std::make_pair("edit_amp", twidget::INVISIBLE));
		// visibles.insert(std::make_pair("edit_input_vars", twidget::INVISIBLE));
	}

	bool is_me = scene.get_id() == base_driver_.scene_id();

	std::string start_label = _("Start");
	if (is_me) {
		if (base_driver_.subtask_state() == aplt::sts_ing) {
			start_label = _("sts^Idle");

		} else {
			start_label = _("Resume");
		}
	}
	dynamic_cast<tbutton*>(drag_grid.find("start", true))->set_label(start_label);

	bool show_start = base_driver_.node_started() && !bg_task_.is_ing() && !base_driver_.slot->moveable();
	visibles.insert(std::make_pair("start", show_start? twidget::VISIBLE: twidget::INVISIBLE));
	list.left_drag_grid_set_widget_visible(visibles);

	return true;
}

void thelper_klink::click_edit_scene_4item(tlistbox& list, tbutton& widget)
{
	const int drag_at = list.drag_at();

	SDL_Rect widget_rect = widget.get_rect(); // widget.get_x(), widget.get_y() + widget.get_height()

	// first, require cancel left_drag grid.
	list.cancel_drag();

	enum {item_name, item_input_vals, item_amp_min = 50};
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	items.push_back(gui2::tmenu::titem(_("Edit name"), item_name));
	items.push_back(gui2::tmenu::titem(_("Edit input vars"), item_input_vals));
	items.back().separator = true;

	std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.mutable_base_scenes();
	aplt::tbase_scene& scene = scenes[drag_at];
	{
		std::vector<gui2::tmenu::titem> amp_items;

		for (std::map<int, tcode3>::const_iterator it = aplt::amp_modes.begin(); it != aplt::amp_modes.end(); ++ it) {
			const tcode3& mode = it->second;

			if (mode.code == scene.amp) {
				initial_sel = item_amp_min + mode.code;
			}
			items.push_back(tmenu::titem(mode.name, item_amp_min + mode.code));
		}
	}

	if (items.empty()) {
		return;
	}

	int sel_at = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		// dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		dlg.show(widget_rect.x, widget_rect.y + widget_rect.h + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		sel_at = dlg.selected_val();
	}

	if (sel_at == item_name) {
		click_edit_scene_id_or_name_internal(list, drag_at, etype_scene_name);

	} else if (sel_at == item_input_vals) {
		VALIDATE(sel_at == item_input_vals, null_str);

		aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, scene.aplt, scene.task, false, true);
		if (pair.task == nullptr) {
			return;
		}

		click_edit_scene_input_vars_internal(list, drag_at);

	} else {
		VALIDATE(sel_at >= item_amp_min && sel_at < item_amp_min + ampmode_count, null_str);
		ttoggle_panel& row = list.row_panel(drag_at);
		sel_new_amp_mode_bh(scene, row, sel_at - item_amp_min);
	}
}

// std::string thelper_klink::auto_edit_scene_id_or_name(const aplt::ttask_pair& pair, const aplt::tbase_scene& scene, int scene_at, int type) const
std::string auto_edit_scene_id_or_name(const aplt::ttask_pair& pair, const aplt::tbase_scene& scene, int scene_at, bool type_is_id)
{
	VALIDATE(pair.task->id == aplt::reserved_tasks.find(aplt::taskid_workout)->second.id, null_str);

	// const std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.base_scenes();
	aplt::tb_api& b_api = aplt::get_b_api();
	const std::vector<aplt::tbase_scene>& scenes = b_api.aplt_base_scenes();
	int scene_count = scenes.size();

	std::set<std::string> excludes;
	for (int at = 0; at < scene_count; at ++) {
		if (at == scene_at) {
			continue;
		}
		const aplt::tbase_scene& scene = scenes[at];
		if (type_is_id) {
			excludes.insert(scene.get_id());
		} else {
			excludes.insert(scene.name());
		}
	}

	// const std::string file_key = "file";
	// std::string file;
	aplt::twkoscript script;
	std::string err_msg;

	utils::string_map symbols;
	symbols["key"] = scene.file_key;
	err_msg = vgettext2("Edit failed. Please set the input variable: 'file' first.", symbols);
	const std::string file = scene.file_var_val();
	if (!file.empty()) {
		err_msg = script.from_aplt_file(*pair.aplt, file);
	}

	if (!script.valid()) {
		gui2::show_message(null_str, err_msg);
		return null_str;
	}
	std::string result;

	// -2 is used to ensure uniqueness. This also causes the resulting id to appear 2 bytes shorter than the actual id.
	const int bytes_makesure_unique = 2;
	if (type_is_id) {
		int max_chars = MAX_NORMAL_ID_OR_VAR_NAME_BYTES - bytes_makesure_unique;
		const std::string id2 = utils::truncate_to_max_chars(script.id.c_str(), script.id.size(), max_chars);
		result = utils::unique_untitle_id(excludes, id2, null_str, 0);

	} else {
		int max_chars = MAX_NORMAL_UTF8_NAME_CHARS - bytes_makesure_unique;
		const std::string name2 = utils::truncate_to_max_chars(script.title.c_str(), script.title.size(), max_chars);
		result = utils::unique_untitle_name(excludes, name2, 0);
	}
	return result;
}

void thelper_klink::click_edit_scene_id_or_name_internal(tlistbox& list, int drag_at, int type)
{
	VALIDATE(type == etype_scene_name, null_str);

	// const int drag_at = list.drag_at();

	std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.mutable_base_scenes();
	aplt::tbase_scene& scene = scenes[drag_at];

	// first, require cancel left_drag grid.
	// list.cancel_drag();

	std::string new_str;

	if (auto_edit_workout_id_or_name_) {
		aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, scene.aplt, scene.task, false, true);
		if (pair.task != nullptr && pair.task->id == aplt::reserved_tasks.find(aplt::taskid_workout)->second.id) {
			new_str = auto_edit_scene_id_or_name(pair, scene, drag_at, false);
			if (new_str.empty()) {
				return;
			}
			if (new_str == scene.name()) {
				return;
			}
		}
	}
	
	if (new_str.empty()) {
		utils::string_map symbols;

        std::string title = _("Edit scene ID");
	    std::string prefix;
        std::string placeholder;
        const std::string initial = scene.name();

		int max_chars = MAX_NORMAL_UTF8_NAME_CHARS;
		symbols["max_chars"] = str_cast(max_chars);
        std::string remark = vgettext2("Scene ID. Must be unique. Up to $max_chars characters, cannot contain Chinese.", symbols);

		if (type == etype_scene_name) {
			title = _("Edit scene name");
			remark = vgettext2("Scene name. Must be unique. Up to $max_chars characters, including Chinese.", symbols);
		}

		std::set<std::string> excludes;
		for (std::vector<aplt::tbase_scene>::const_iterator it = scenes.begin(); it != scenes.end(); ++ it) {
			const aplt::tbase_scene& scene = *it;
			VALIDATE(type == etype_scene_name, null_str);
			excludes.insert(scene.name());
		}

	    {
		    gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), max_chars, gui2::tedit_box_param::show_cancel);
		    param.did_text_changed = std::bind(&thelper_klink::verify_edit_alias_name, this, _1, std::ref(initial), type, std::ref(excludes));
		    {
			    gui2::tedit_box dlg(param);
			    dlg.show(nposm, window_priv_->get_height() / 5);
			    if (dlg.get_retval() != gui2::twindow::OK) {
				    return;
			    }
		    }
		    new_str = param.result;
	    }
	}

	VALIDATE(new_str != scene.name(), null_str);
	scene.set_name(new_str);

	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_base_scene);

	// if want to execute below, require cancel left_drag grid.
	ttoggle_panel& row = list.row_panel(drag_at);
	std::string label;
	label = scene.name_for_gui();

	row.set_child_label("name", label);
}

void thelper_klink::click_edit_scene_id_or_name(tlistbox& list, int type)
{
	VALIDATE(type == etype_scene_name, null_str);

	const int drag_at = list.drag_at();

	// first, require cancel left_drag grid.
	list.cancel_drag();

	click_edit_scene_id_or_name_internal(list, drag_at, type);
}

void thelper_klink::sel_new_amp_mode_bh(aplt::tbase_scene& scene, ttoggle_panel& row, int new_mode)
{
	bool is_me = scene.get_id() == base_driver_.scene_id();
	if (is_me) {
		int curr_mode = pinyin_.get_amp_mode();
		if (base_driver_.subtask_state() == aplt::sts_ing) {
			VALIDATE(curr_mode == scene.amp, null_str);
			pinyin_.set_amp_mode(new_mode);

		} else {
			VALIDATE(curr_mode == ampmode_1x, null_str);
		}
	}

	scene.amp = new_mode;
	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_base_scene);

	// ttoggle_panel& row = list.row_panel(drag_at);
	row.set_child_label("amp", aplt::amp_modes.find(new_mode)->second.id);
}

void thelper_klink::click_edit_amp(tlistbox& list, tbutton& widget)
{
	const int drag_at = list.drag_at();

	SDL_Rect widget_rect = widget.get_rect(); // widget.get_x(), widget.get_y() + widget.get_height()

	// first, require cancel left_drag grid.
	list.cancel_drag();

	enum {item_id, item_name, item_input_vals, item_count};
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.mutable_base_scenes();
	aplt::tbase_scene& scene = scenes[drag_at];

	// std::vector<std::string> task_id2s;
	for (std::map<int, tcode3>::const_iterator it = aplt::amp_modes.begin(); it != aplt::amp_modes.end(); ++ it) {
		const tcode3& mode = it->second;

		if (mode.code == scene.amp) {
			initial_sel = mode.code;
		}
		items.push_back(tmenu::titem(mode.name, mode.code));
	}


	if (items.empty()) {
		return;
	}

	int new_mode = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		// dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		dlg.show(widget_rect.x, widget_rect.y + widget_rect.h + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		new_mode = dlg.selected_val();
		VALIDATE(new_mode >= 0 && new_mode < ampmode_count, null_str);
	}

	ttoggle_panel& row = list.row_panel(drag_at);
	sel_new_amp_mode_bh(scene, row, new_mode);
/*
	bool is_me = scene.id == base_driver_.scene_id();
	if (is_me) {
		int curr_mode = pinyin.get_amp_mode();
		if (base_driver_.subtask_state() == aplt::sts_ing) {
			VALIDATE(curr_mode == scene.amp, null_str);
			pinyin.set_amp_mode(new_mode);

		} else {
			VALIDATE(curr_mode == aplt::ampmode_1x, null_str);
		}
	}

	scene.amp = new_mode;
	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_base_scene);

	ttoggle_panel& row = list.row_panel(drag_at);
	row.set_child_label("amp", aplt::amp_modes.find(new_mode)->second.id);
*/
}

// void thelper_klink::list_wkoscript_files_to_freq_vals(const aplt::tapplet& aplt, std::vector<std::string>& result) const
void list_wkoscript_files_to_freq_vals(const aplt::tapplet& aplt, std::vector<std::string>& result)
{
	result.clear();

	std::vector<std::string> dirs;
	dirs.push_back(aplt.res_path + "/wkoscript");
	dirs.push_back(aplt.preferences_dir + "/wkoscript");

	std::set<std::string> existed2;
	for (std::vector<std::string>::const_iterator it = dirs.begin(); it != dirs.end(); ++ it) {
		const std::string& wkoscript_dir = *it;
		std::set<std::string> existed;
		aplt::list_wkoscript_files_by_type(wkoscript_dir, aplt::type_wkoscript_cfgfiles, existed);
		existed2.insert(existed.begin(), existed.end());
	}
	
	for (std::set<std::string>::const_iterator it = existed2.begin(); it != existed2.end(); ++ it) {
		const std::string& file = *it;
		result.push_back(file);
	}
}

void thelper_klink::click_edit_scene_input_vars_internal(tlistbox& list, int drag_at)
{
	// const int drag_at = list.drag_at();

	std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.mutable_base_scenes();
	aplt::tbase_scene& scene = scenes[drag_at];

	aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, scene.aplt, scene.task, false, true);
	VALIDATE(pair.task != nullptr, null_str);
	// if (pair.task == nullptr) {
	//	return;
	// }

	// first, require cancel left_drag grid.
	// list.cancel_drag();

	std::map<std::string, std::string> map_vals = scene.input_vars;
	{
		utils::string_map symbols;

		std::string title = _("Variable");
		const std::string name_prefix = pair.aplt->bundleid;
		symbols["prefix"] = name_prefix + "__";
		std::string remark = vgettext2("remark^edit scene's input vars, $prefix", symbols);

		std::vector<std::string> freq_vals;
		if (pair.task->id == aplt::reserved_tasks.find(aplt::taskid_workout)->second.id) {
			list_wkoscript_files_to_freq_vals(*pair.aplt, freq_vals);
		}

		gui2::trvar_editor dlg(var_editor_slot_, title, remark, pair.task, freq_vals, name_prefix, map_vals);
		dlg.show();
		var_editor_slot_.rvar_did_dlg_close();
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
	}
	VALIDATE(scene.input_vars != map_vals, null_str);

	const int original_sts = base_driver_.subtask_state();
	bool is_me = scene.get_id() == preferences::base_scene_id();
	if (is_me && bg_task_.is_ing()) {
		msg_running_not_modify(bg_task_, true);
		return;
	}
	const bool restart = is_me && (original_sts == aplt::sts_ing || original_sts == aplt::sts_nposm);

	scene.input_vars = map_vals;
	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_base_scene);

	if (restart) {
		if (original_sts == aplt::sts_ing) {
			base_driver_.idle_or_preempt_subtask(true);
			base_driver_.resume_subtask();

		} else {
			VALIDATE(original_sts == aplt::sts_nposm, null_str);
			VALIDATE(!preferences::base_scene_id().empty(), null_str);
			base_driver_.start_subtask_from_preferences();
		}
	}

	ttoggle_panel& row = list.row_panel(drag_at);
	if (restart) {
		reload_scene_list(list);

	} else {
		row.set_child_label("input_vars", scene.join_input_vars());
		if (base_driver_.subtask_state() == aplt::sts_nposm && scene.get_id() == preferences::base_scene_id()) {
			row.set_child_label("state", null_str);
		}
	}
}

void thelper_klink::click_edit_scene_input_vars(tlistbox& list)
{
	const int drag_at = list.drag_at();

	std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.mutable_base_scenes();
	aplt::tbase_scene& scene = scenes[drag_at];

	aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, scene.aplt, scene.task, false, true);
	if (pair.task == nullptr) {
		return;
	}

	// first, require cancel left_drag grid.
	list.cancel_drag();

	click_edit_scene_input_vars_internal(list, drag_at);
}

void thelper_klink::click_start_scene(tlistbox& list)
{
	const int drag_at = list.drag_at();

	const std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.base_scenes();
	VALIDATE(drag_at < (int)scenes.size(), null_str);
	const aplt::tbase_scene& scene = scenes[drag_at];

	// first, require cancel left_drag grid.
	list.cancel_drag();

	if (bg_task_.is_ing()) {
		msg_running_not_modify(bg_task_, true);
		return;
	}

	bool is_me = scene.get_id() == base_driver_.scene_id();

	utils::string_map symbols;
	symbols["name"] = scene.name();
	std::string msg;
	if (is_me) {
		if (base_driver_.subtask_state() == aplt::sts_ing) {
			msg = vgettext2("Do you want scene '$name' to be idle?", symbols);

		} else {
			msg = vgettext2("Do you want to restore scene '$name' to working state?", symbols);
		}
	} else {
		msg = vgettext2("Do you want to switch to scene '$name'?", symbols);
	}
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	base_driver_.start_or_stop_subtask(scene, true);
/*
	if (is_me) {
		if (base_driver_.subtask_state() == aplt::sts_ing) {
			base_driver_.idle_or_preempt_subtask(true);

		} else {
			base_driver_.resume_subtask();
		}

	} else {
		preferences::set_base_scene_id(scene.id);
		base_driver_.restart_subtask();
	}
*/
	reload_scene_list(list);
}

void thelper_klink::click_erase_scene(tlistbox& list)
{
	const int drag_at = list.drag_at();

	std::vector<aplt::tbase_scene>& scenes = cfg_cpp_api_.mutable_base_scenes();
	VALIDATE(drag_at < (int)scenes.size(), null_str);
	const aplt::tbase_scene& scene = scenes[drag_at];

	// first, require cancel left_drag grid.
	list.cancel_drag();

	bool is_me = base_driver_.scene_id() == scene.get_id();
	if (is_me && bg_task_.is_ing()) {
		return;
	}

	utils::string_map symbols;
	symbols["name"] = scene.name();
	const std::string msg = vgettext2("Do you want to delete scene '$name'?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}

	tbase_driver_core::tdisable_earase_wkocourse_scene_lock lock(base_driver_);

	if (is_me) {
		// preferences::set_base_scene_id(null_str);
		base_driver_.stop_subtask(true);
	}

	cfg_cpp_api_.erase_scene(scene.get_id());
/*
	std::vector<aplt::tbase_scene>::iterator it = scenes.begin();
	if (drag_at != 0) {
		std::advance(it, drag_at);
	}
	scenes.erase(it);

	cfg_cpp_api_.save_klink_pb(aplt::tbg_task::misc_cfg_base_scene);
*/
	if (is_me) {
		if (!scenes.empty()) {
			int at = drag_at % scenes.size();
			const aplt::tbase_scene& scene2 = scenes[at];
			base_driver_.start_subtask_from_nposm(scene2);

		} else {
			preferences::set_base_scene_id(null_str);
		}
	}

	reload_scene_list(list);
}


bool thelper_klink::verify_edit_alias_name(const std::string& label, const std::string& initial, int etype, const std::set<std::string>& excludes) const
{
	if (label == initial) {
		return false;
	}

	if (label.empty()) {
		if (etype != etype_iot_alias) {
			return false;
		}
		return true;
	}
	if (utils::has_portable_space_2end(label)) {
		return false;
	}

	if (excludes.count(label) != 0) {
		return false;
	}

	bool valid = false;
	if (etype == etype_var_name || etype == etype_scene_name || etype == etype_iot_alias) {
		valid = isvalid_normal_utf8_name224(label);	

	} else {
		VALIDATE(false, null_str);
	}

	return valid;
}

bool thelper_klink::is_wkocourse_scene_ing(int type, bool show_dlg) const
{
	VALIDATE(type >= 0 && type < wkocourse_blocktype_count, null_str);
	if (!base_driver_.scene_id().empty()) {
		const aplt::tbase_scene* scene = cfg_cpp_api_.base_scene_from_id(base_driver_.scene_id(), true);
		if (!scene->wkocourse_id2.empty()) {
			utils::string_map symbols;
			if (type == wkocourse_blocktype_insert_scene) {
				// To avoid the case where scene(wkoscript_id2 isn't empty) is no longer the last one, 
				// which could later cause an "unexpected" invalid memory access(scene) due to deletion.
				symbols["action"] = _("Insert base scene");

			} else if (type == wkocourse_blocktype_import) {
				// Course tasks cannot be exported. Once imported, the current course tasks will definitely be lost.
				symbols["action"] = _("Import");
			}
			if (show_dlg) {
				std::string msg = vgettext2("A task is currently running in the wkocourse, cannot $action.", symbols);
				gui2::show_message(null_str, msg);
			}
			return true;
		}
	}
	return false;
}

//
// var_sensor layer
//
void thelper_klink::click_import(tbutton& widget)
{
	if (is_wkocourse_scene_ing(wkocourse_blocktype_import, true)) {
		return;
	}

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
	app_post_click_import();
	// reload_task_list(*task_list_);
}

void thelper_klink::click_export(tbutton& widget)
{
	// VALIDATE(current_layer_ == TASK_LAYER, null_str);
	app_pre_click_export();

	utils::string_map symbols;
	std::stringstream out;
	std::string cfg_filename = "var_sensor_export.cfg";
	const std::vector<aplt::tvar_sensor>& var_sensors = cfg_cpp_api_.var_sensors();
	const std::vector<aplt::ttimed_sensor>& timed_sensors = cfg_cpp_api_.timed_sensors();

	const std::string scene_name = bg_task_.scene_name();
	cfg_filename = scene_name_2_klink_cfg_name(scene_name);

	const std::string filename = game_config::preferences_dir + "/saves/" + cfg_filename;
	if (SDL_IsFile(filename.c_str())) {
		symbols["filename"] = cfg_filename;
		const std::string msg = vgettext2("$filename, do you want to overwrite a file with the same name?", symbols);
		if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return;
		}
	}

	// {
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

	// }

	if (!out.str().empty()) {
		write_file(filename, out.str().c_str(), out.str().size());

		symbols["file"] = filename;
		gui2::show_message(null_str, vgettext2("Export finished. file: $file", symbols));
	}
}

//
// env_var
//
void thelper_klink::click_refresh(tbutton& widget)
{
	reload_env_var_list(*env_var_list_);
}

void thelper_klink::reload_env_var_list(tlistbox& list)
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

void get_task_item3fs(std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, uint32_t allow_type_mask, uint32_t deny_type_mask, bool allow_empty)
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
		uint32_t bit_mask = BIT_IDX_MASK(task.type);
		if (allow_type_mask != nposm && !(bit_mask & allow_type_mask)) {
			continue;
		}
		if (deny_type_mask != nposm && (bit_mask & deny_type_mask)) {
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
			uint32_t bit_mask = BIT_IDX_MASK(task.type);
			if (allow_type_mask != nposm && !(bit_mask & allow_type_mask)) {
				continue;
			}
			if (deny_type_mask != nposm && (bit_mask & deny_type_mask)) {
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

} // namespace gui2

