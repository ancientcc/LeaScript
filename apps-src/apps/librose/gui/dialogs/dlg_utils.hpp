#ifndef GUI_DIALOGS_DLG_UTILS_HPP
#define GUI_DIALOGS_DLG_UTILS_HPP

#include "gui/dialogs/dialog.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/rvar_editor.hpp"
#include "aplt_net.hpp"
#include "base_instance.hpp"
#include "cfg_cpp_api_core.hpp"
#include "base_driver_core.hpp"

namespace gui2 {

std::string generate_task2_desc(const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task, const std::string& device_id);
std::string generate_task2_desc2(int type, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task, const std::string& device_id);
// std::string task_name2_from_3id(std::map<aplt::taplt_key, aplt::tapplet>& applets, 
//	const std::string& aplt_id, const std::string& task_id, const std::string& ble_device_id, bool aplt_id_is_bundleid);

class tbutton;
class treport;
class tstack;
class tlistbox;
class tlabel;
class ttoggle_panel;

class thelper_klink
{
public:
	thelper_klink(gui2::trvar_editor::tslot& var_editor_slot, std::map<aplt::taplt_key, aplt::tapplet>& applets, 
		aplt::tcfg_cpp_api_core& cfg_cpp_api, aplt::tbg_task& pb_task, tbase_driver_core& base_driver);
	posix_noncopyable(thelper_klink);

	virtual ~thelper_klink() {}

	void pre_show(twindow& window);

protected:
	void pre_scene(tgrid& grid);
	void pre_env_var(tgrid& grid);

	void set_title_label();
	bool verify_klink_scene_name(const std::string& label, const std::string& initial) const;
	void click_title(tbutton& widget);
/*
	bool show_if_block2_dlg(const std::string& title, const aplt::tapplet::ttask& cfg_task, aplt::tif_block& if_block, bool& dirty);
*/
	// layer: scene
	void click_insert_scene(tbutton& widget);
	void did_auto_edit_workout_id_or_name_changed(ttoggle_button& widget);
	void reload_scene_list(tlistbox& list);
	void click_scene_task(tlistbox& list, tbutton& widget, int at);
	bool did_scene_can_drag(tlistbox& list, ttoggle_panel& row);
	void click_edit_scene_4item(tlistbox& list, tbutton& widget);
	std::string auto_edit_scene_id_or_name(const aplt::ttask_pair& pair, const aplt::tbase_scene& scene, int scene_at, int type) const;
	void click_edit_scene_id_or_name_internal(tlistbox& list, int drag_at, int type);
	void click_edit_scene_id_or_name(tlistbox& list, int type);
	void sel_new_amp_mode_bh(aplt::tbase_scene& scene, ttoggle_panel& row, int new_mode);
	void click_edit_amp(tlistbox& list, tbutton& widget);
	void list_wkoscript_files_to_freq_vals(const aplt::tapplet& aplt, std::vector<std::string>& result) const;
	void click_edit_scene_input_vars_internal(tlistbox& list, int drag_at);
	void click_edit_scene_input_vars(tlistbox& list);
	void click_start_scene(tlistbox& list);
	void click_erase_scene(tlistbox& list);

/*	
	bool verify_edit_device_id(const std::string& label, const std::string& initial, int min_chars) const;
*/
	enum {etype_var_name, etype_iot_alias, etype_scene_id, etype_scene_name, edit_count};
	bool verify_edit_alias_name(const std::string& label, const std::string& initial, int etype, const std::set<std::string>& excludes) const;
/*
	std::string edit_device_id(const std::string& src, bool& cancel) const;
*/
	// layer: var_sensor
	void click_import(tbutton& widget);
	void click_export(tbutton& widget);

	// layer: env_var
	void click_refresh(tbutton& widget);
	void reload_env_var_list(tlistbox& list);
/*
	// layer: add_timed_task
	void click_clear(tbutton& widget);
	void reload_add_timed_task_list(tlistbox& list);

	std::string get_clock_png(const aplt::taplt_task& task);
	int row_from_key(int type, int priority, int at) const;
	void aplt_task_state_changed(bool is_task_cpp, const aplt::taplt_task& task, bool started, int stopped_state = nposm);
*/
	std::string msg_running_not_modify(const aplt::tbg_task& bg_task, bool dlg) const;

private:
	virtual void app_get_task_item3fs(std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, uint32_t allow_type_mask, uint32_t deny_type_mask, bool allow_empty) = 0;

	virtual void app_post_click_import() {}
	virtual void app_pre_click_export() {}

protected:
	aplt::tpinyin& pinyin_;
	gui2::trvar_editor::tslot& var_editor_slot_;
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	aplt::tcfg_cpp_api_core& cfg_cpp_api_;
	aplt::tbg_task& bg_task_;
	tbase_driver_core& base_driver_;
	// const int online_threshold_minute_;

	tbutton* title_widget_;
	// treport* report_;
	// tstack* stack_;
	// tbutton* insert_event_widget_;
	// tbutton* insert_speech_widget_;
	// tbutton* insert_var_widget_;
	// tbutton* insert_timed_widget_;
	tbutton* import_widget_;
	tbutton* export_widget_;
	// tbutton* insert_alias_widget_;
	tbutton* insert_scene_widget_;
	tbutton* refresh_widget_;
	// tbutton* clear_widget_;
	// tlistbox* task_list_;
	// tlistbox* device_list_;
	tlistbox* scene_list_;
	// tlistbox* var_sensor_list_;
	tlistbox* env_var_list_;
	// tlistbox* add_timed_task_list_;
	// int current_layer_;

	bool auto_edit_workout_id_or_name_;
	bool simple_scene_drag_menu_;

private:
	twindow* window_priv_;
};

void get_task_item3fs(std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, uint32_t allow_type_mask, uint32_t deny_type_mask, bool allow_empty);
} // namespace gui2

#endif

