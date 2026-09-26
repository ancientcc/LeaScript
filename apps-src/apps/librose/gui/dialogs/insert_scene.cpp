#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/insert_scene.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
#include "cfg_cpp_api_core.hpp"


using namespace std::placeholders;

namespace gui2 {

extern void list_wkoscript_files_to_freq_vals(const aplt::tapplet& aplt, std::vector<std::string>& result);
extern std::string auto_edit_scene_id_or_name(const aplt::ttask_pair& pair, const aplt::tbase_scene& scene, int scene_at, bool type_is_id);

REGISTER_DIALOG(rose, insert_scene)

tinsert_scene::tinsert_scene(trvar_editor::tslot& var_editor_slot, const std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::tcfg_cpp_api_core& cfg_cpp_api)
	: var_editor_slot_(var_editor_slot)
	, applets_(applets)
	, cfg_cpp_api_(cfg_cpp_api)
	, workout_task_id_(aplt::reserved_tasks.find(aplt::taskid_workout)->second.id)
	, task_widget_(nullptr)
	, input_vars_widget_(nullptr)
	, name_widget_(nullptr)
	, ok_widget_(nullptr)
{
}

void tinsert_scene::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	find_widget<tlabel>(window_, "title", false).set_label(_("Insert base scene"));

	tbutton* button = find_widget<tbutton>(window_, "task", false, true);
	connect_signal_mouse_left_click(
			*button
			, std::bind(
				&tinsert_scene::click_scene_task
				, this
				, std::ref(*button)));
	task_widget_ = button;

	button = find_widget<tbutton>(window_, "edit_input_vars", false, true);
	connect_signal_mouse_left_click(
			*button
			, std::bind(
				&tinsert_scene::click_edit_scene_input_vars
				, this
				, std::ref(*button)));

	aplt::tbase_scene& scene = scene_;
	if (game_config::app_code == aplt::app_launcher) {
		scene.aplt = aplt::get_bundleid(aplt::bundleid_leagor_khome);
	} else {
		scene.aplt = aplt::get_bundleid(aplt::bundleid_leagor_khomelua);
	}
	scene.task = workout_task_id_;

	std::string label = aplt::task_name2_from_3id(applets_, scene.aplt, scene.task, null_str, true);
	task_widget_->set_label(label);

	input_vars_widget_ = find_widget<tlabel>(window_, "input_vars", false, true);
	// input_vars_widget_->set_border("label12_f2");

	name_widget_ = find_widget<tlabel>(window_, "name", false, true);

	ok_widget_ = find_widget<tbutton>(window_, "ok", false, true);

	did_input_vars_changed();
}

void tinsert_scene::post_show()
{
}

void tinsert_scene::click_scene_task(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::vector<aplt::ttask_item3f> item3fs;

	items.clear();
	item3fs.clear();

	const std::map<aplt::taplt_key, aplt::tapplet>& applets = applets_;

	std::stringstream ss;

	std::vector<const aplt::tiot_device*> src_devices;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets.begin(); it != applets.end(); ++ it) {
		const aplt::tapplet& applet = it->second;
		const std::string id = applet.id;

		if (applet.tasks.empty()) {
			continue;
		}

		for (std::map<std::string, aplt::tapplet::ttask>::const_iterator it = applet.tasks.begin(); it != applet.tasks.end(); ++ it) {
			const aplt::tapplet::ttask& task = it->second;
			if (task.type != aplt::task_camera) {
				continue;
			}
			if (task.id != workout_task_id_) {
				continue;
			}
			{
				ss.str("");
				ss << applet.name2() << "-" << task.name2();

				items.push_back(gui2::tmenu::titem(ss.str(), items.size()));
				item3fs.emplace_back(aplt::ttask_item3f(applet, task, null_str));

			}
		}
	}

	//////////////////////////////////////
	aplt::tbase_scene& scene = scene_;

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

	scene.aplt = cur_aplt->bundleid;
	scene.task = cur_task->id;

	scene.input_vars.clear();

	did_input_vars_changed();

	std::string label = aplt::task_name2_from_3id(applets_, scene.aplt, scene.task, null_str, true);
	widget.set_label(label);
}

void tinsert_scene::click_edit_scene_input_vars(tbutton& widget)
{
	aplt::tbase_scene& scene = scene_;

	aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, scene.aplt, scene.task, false, true);
	VALIDATE(pair.task != nullptr, null_str);
	VALIDATE(pair.task->id == workout_task_id_, null_str);

	std::map<std::string, std::string> map_vals = scene.input_vars;
	{
		utils::string_map symbols;

		std::string title = _("Variable");
		const std::string name_prefix = pair.aplt->bundleid;
		symbols["prefix"] = name_prefix + "__";
		std::string remark = vgettext2("remark^edit scene's input vars, $prefix", symbols);

		std::vector<std::string> freq_vals;
		if (pair.task->id == workout_task_id_) {
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

	scene.input_vars = map_vals;

	did_input_vars_changed();
}

void tinsert_scene::did_input_vars_changed()
{
	aplt::tbase_scene& scene = scene_;

	std::string new_name;
	bool valid = false;
	aplt::ttask_pair pair = aplt::task_pair_from_2_id(applets_, scene.aplt, scene.task, false, true);
	if (pair.task != nullptr && pair.task->id == workout_task_id_ && !scene.file_var_val().empty()) {
		new_name = auto_edit_scene_id_or_name(pair, scene, nposm, false);
		valid = !new_name.empty();
	}

	// if (valid && scene.valid()) {
	if (valid) {
		std::string scene_id = scene.get_id();
		if (!scene_id.empty() && cfg_cpp_api_.base_scene_from_id(scene_id, false) != nullptr) {
			valid = false;
		}
	}

	if (!new_name.empty()) {
		scene.set_name(new_name, true);
	}

	input_vars_widget_->set_label(scene.join_input_vars());

	name_widget_->set_label(scene.name());

	ok_widget_->set_active(valid);
}

} // namespace gui2

