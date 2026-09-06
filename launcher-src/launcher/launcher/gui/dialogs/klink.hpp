#ifndef GUI_DIALOGS_KLINK_HPP_INCLUDED
#define GUI_DIALOGS_KLINK_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/dlg_utils.hpp"
#include "aplt_net.hpp"
#include "base_instance.hpp"
#include <rose_ros/utils.hpp>
#include "cfg_cpp_api.hpp"
#include "base_driver.hpp"
#include "moveit_driver.hpp"

namespace gui2 {

class tbutton;
class treport;
class tstack;
class tlistbox;
class tlabel;
class ttoggle_panel;

class tklink: public tdialog, public tstatusbar, public tbase_msg_subscriber, public thelper_klink
{
public:
	enum {TASK_LAYER, SCENE_LAYER, DEVICE_LAYER, ENV_VAR_LAYER, ADD_TIMED_TASK_LAYER};
	enum {KLINK_IOT_LAYER, KLINK_SPEECH_LAYER, KLINK_VAR_LAYER, KLINK_TIMED_LAYER};

	tklink(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, trvar_editor::tslot& var_editor_slot,
		std::map<aplt::taplt_key, aplt::tapplet>& applets, const tros_map& curmap, aplt::tcfg_cpp_api& cfg_cpp_api,
		aplt::tbg_task& bg_task, tbase_driver& base_driver, tmoveit_driver& moveit_driver);
	posix_noncopyable(tklink);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void pre_task(tgrid& grid);
	void pre_device(tgrid& grid);
	// void pre_scene(tgrid& grid);
	// void pre_env_var(tgrid& grid);
	void pre_add_timed_task(tgrid& grid);
	void did_item_changed(treport& report, ttoggle_button& widget);
/*
	void set_title_label();
	bool verify_klink_scene_name(const std::string& label, const std::string& initial) const;
	void click_title(tbutton& widget);
*/
	// layer: task
	void click_insert_iot_task(tbutton& widget);
	void click_insert_speech_sensor_task(tbutton& widget);
	void click_insert_timed_task(tbutton& widget);
	void click_insert_var_task(tbutton& widget);
	void insert_row(tlistbox& list, const aplt::taplt_task& task, const std::map<std::string, const aplt::tapplet*>& id_aplt_map, std::map<std::string, std::string>& data);
	void reload_task_list(tlistbox& list);
	bool did_tasks_can_drag(tlistbox& list, ttoggle_panel& row);
	void click_edit_name_or_sync_vars(tlistbox& list, bool is_name);
	void click_edit_input_vars(tlistbox& list);
	void click_edit_task(tlistbox& list);
	void click_stop_task(tlistbox& list);
	void click_erase_task(tlistbox& list);
	void click_task_task2(tlistbox& list, tbutton& widget, int at);
	void click_task_priority(tlistbox& list, tbutton& widget, int at);
	void click_task_position(tlistbox& list, tbutton& widget, tbutton& other, int at, bool p1);
	void refresh_2position(tbutton& position1_widget, tbutton& position2_widget, const aplt::taplt_task& task, const aplt::tapplet* aplt, const aplt::tapplet::ttask* cfg_task);

	bool show_if_block2_dlg(const std::string& title, const aplt::tapplet::ttask& cfg_task, aplt::tif_block& if_block, bool& dirty);
/*
	// layer: scene
	void click_insert_scene(tbutton& widget);
	void reload_scene_list(tlistbox& list);
	void click_scene_task(tlistbox& list, tbutton& widget, int at);
	bool did_scene_can_drag(tlistbox& list, ttoggle_panel& row);
	void click_edit_scene_id_or_name(tlistbox& list, int type);
	void click_edit_scene_input_vars(tlistbox& list);
	void click_start_scene(tlistbox& list);
	void click_erase_scene(tlistbox& list);
*/
	// layer: device
	void click_insert_device(tbutton& widget);
	void reload_device_list(tlistbox& list);
	bool did_pins_can_drag(tlistbox& list, ttoggle_panel& row);
	void click_edit_alias_name(tlistbox& list);
	void click_erase_device(tlistbox& list);
	
	bool verify_edit_device_id(const std::string& label, const std::string& initial, int min_chars) const;
/*
	enum {etype_var_name, etype_iot_alias, etype_scene_id, etype_scene_name, edit_count};
	bool verify_edit_alias_name(const std::string& label, const std::string& initial, int etype, const std::set<std::string>& excludes) const;
*/
	std::string edit_device_id(const std::string& src, bool& cancel) const;
/*
	// layer: var_sensor
	void click_import(tbutton& widget);
	void click_export(tbutton& widget);

	// layer: env_var
	void click_refresh(tbutton& widget);
	void reload_env_var_list(tlistbox& list);
*/
	// layer: add_timed_task
	void click_clear(tbutton& widget);
	void reload_add_timed_task_list(tlistbox& list);

	std::string get_clock_png(const aplt::taplt_task& task);
	int row_from_key(int type, int priority, int at) const;
	void aplt_task_state_changed(bool is_task_cpp, const aplt::taplt_task& task, bool started, int stopped_state = nposm);

	//
	// tbase_msg_subscriber
	//
	void bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task) override;
	void bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task) override;
	void iot_did_heartbeats2(const std::set<aplt::tiot_heartbeat>& heartbeats) override;
	void iot_did_events2(const std::set<aplt::tiot_event>& events) override;
	void klink_tasks_changed2() override;

	//
	// thelper_klink
	//
	void app_get_task_item3fs(std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, uint32_t allow_type_mask, uint32_t deny_type_mask, bool allow_empty) override;
	void app_post_click_import() override;
	void app_pre_click_export() override;

	void app_timer_handler(uint32_t now) override;

private:
	// std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	const tros_map& curmap_;
	// aplt::tcfg_cpp_api& cfg_cpp_api_;
	// aplt::tbg_task& bg_task_;
	// tbase_driver& base_driver_;
	tmoveit_driver& moveit_driver_;
	const int online_threshold_minute_;

	// tbutton* title_widget_;
	treport* report_;
	tstack* stack_;
	tbutton* insert_event_widget_;
	tbutton* insert_speech_widget_;
	tbutton* insert_var_widget_;
	tbutton* insert_timed_widget_;
	// tbutton* import_widget_;
	// tbutton* export_widget_;
	tbutton* insert_alias_widget_;
	// tbutton* insert_scene_widget_;
	// tbutton* refresh_widget_;
	tbutton* clear_widget_;
	tlistbox* task_list_;
	tlistbox* device_list_;
	// tlistbox* scene_list_;
	tlistbox* var_sensor_list_;
	// tlistbox* env_var_list_;
	tlistbox* add_timed_task_list_;
	int current_layer_;

	struct ttask_key
	{
		ttask_key(int type, int priority, int var_at, int zerotz_t, int at, const std::string& alias)
			: type(type)
			, priority(priority)
			, var_at(var_at)
			, zerotz_t(zerotz_t)
			, at(at)
			, alias(alias)
		{
			if (type == aplt::taplt_task::type_speech || type == aplt::taplt_task::type_timed) {
				VALIDATE(alias.empty(), null_str);
			}
		}

		bool operator<(const ttask_key& that) const noexcept
		{
			VALIDATE(IS_VALID_APLT_TASK_PRIORITY(priority), null_str);
			VALIDATE(IS_VALID_APLT_TASK_PRIORITY(that.priority), null_str);
			if (priority != that.priority) {
				// if priority isn't, more priority is before.
				return priority > that.priority;
			}
			if (type != that.type) {
				// sore: type_iot, type_speech, type_var, type_timed
				if (type == aplt::taplt_task::type_iot) {
					return true;

				} else if (type == aplt::taplt_task::type_speech) {
					return that.type != aplt::taplt_task::type_iot;

				} else if (type == aplt::taplt_task::type_var) {
					return that.type == aplt::taplt_task::type_timed;
				}

				VALIDATE(type == aplt::taplt_task::type_timed, null_str);
				return false;
			}

			// if both priority and type are same, smaller @at is before.
			VALIDATE(at != that.at, null_str);
			if (!alias.empty()) {
				if (!that.alias.empty()) {
					int cmp = SDL_strcmp(alias.c_str(), that.alias.c_str());
					return cmp < 0;
				} else {
					return true;
				}
			} else if (!that.alias.empty()) {
				return false;
			}

			if (type == aplt::taplt_task::type_var) {
				if (var_at != that.var_at) {
					return var_at < that.var_at;
				}
				return false;
			}

			if (type == aplt::taplt_task::type_timed) {
				if (zerotz_t != that.zerotz_t) {
					return zerotz_t < that.zerotz_t;
				}
				return false;
			}

			return at < that.at;
		}

		const int type;
		const int priority;
		const int var_at;
		const int zerotz_t;
		const int at;
		const std::string alias;
	};
	std::vector<ttask_key> task_keys_;
};

} // namespace gui2

#endif

