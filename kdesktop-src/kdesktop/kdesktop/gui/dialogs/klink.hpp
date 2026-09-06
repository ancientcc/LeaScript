#ifndef GUI_DIALOGS_KLINK_HPP
#define GUI_DIALOGS_KLINK_HPP

// #include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/dlg_utils.hpp"
#include "aplt_net.hpp"
#include "base_instance.hpp"
// #include <rose_ros/utils.hpp>
#include "cfg_cpp_api_core.hpp"
#include "base_driver_core.hpp"
// #include "moveit_driver.hpp"

namespace gui2 {

class tbutton;
class treport;
class tstack;
class tlistbox;
class tlabel;
class ttoggle_panel;

class tklink: public tdialog, public thelper_klink
{
public:
	enum {SCENE_LAYER, ENV_VAR_LAYER};

	tklink(trvar_editor::tslot& var_editor_slot,
		std::map<aplt::taplt_key, aplt::tapplet>& applets, aplt::tcfg_cpp_api_core& cfg_cpp_api,
		aplt::tbg_task& bg_task, tbase_driver_core& base_driver);
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

	// layer: env_var
	// void click_refresh(tbutton& widget);
	// void reload_env_var_list(tlistbox& list);

	//
	// thelper_klink
	//
	void app_get_task_item3fs(std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, uint32_t allow_type_mask, uint32_t deny_type_mask, bool allow_empty) override;
	void app_post_click_import() override;
	void app_pre_click_export() override;

	void app_timer_handler(uint32_t now) override;

private:
	treport* report_;
	tstack* stack_;
	// tbutton* insert_event_widget_;
	// tbutton* insert_speech_widget_;
	// tbutton* insert_var_widget_;
	// tbutton* insert_timed_widget_;
	// tbutton* insert_alias_widget_;
	// tbutton* refresh_widget_;
	// tbutton* clear_widget_;
	// tlistbox* task_list_;
	// tlistbox* device_list_;
	// tlistbox* var_sensor_list_;
	// tlistbox* env_var_list_;
	// tlistbox* add_timed_task_list_;

	int current_layer_;
};

} // namespace gui2

#endif

