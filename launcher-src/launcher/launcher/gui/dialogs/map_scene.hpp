#ifndef GUI_DIALOGS_MAP_THEME_HPP_INCLUDED
#define GUI_DIALOGS_MAP_THEME_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
#include "game_config.hpp"
#include "ros_instance.hpp"

class map_controller;

namespace gui2 {

class tstack;
class tlistbox;
class ttoggle_panel;

class tmap_scene: public tdialog, public tstatusbar
{
public:
	enum {ZOOM = DERIVED_REP_MIN, POSITION, STATUS, EULER_Z, BATTERY, DRIVER, NUM_REPORTS};

	enum {
		HOTKEY_RETURN = HOTKEY_MIN,
		HOTKEY_START, HOTKEY_SAVE_MAP, HOTKEY_AUTO_BUILDMAP, HOTKEY_CANCEL_GOAL, 
		HOTKEY_SWITCH_SLAM, HOTKEY_DENOISE_MAP, HOTKEY_REPOSITION_ROBOT,
		HOTKEY_CONFIRM_OK, HOTKEY_CONFIRM_CANCEL
	};

	// must be same as mode_buildmap/mode_navigation/mode_position
	enum {BUILDMAP_STOPPED_LAYER, NAVIGATION_STOPPED_LAYER, POSITION_LAYER, SLAM_LAYER, CONFIRM_LAYER};
	enum {MAPFILE_UNSELECT_LAYER, MAPFILE_SELECTED_LAYER};
	enum {TELEOP_PRESET_LAYER, TELEOP_CUSTOM_LAYER};

	tmap_scene(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tros_instance& ros_instance, tdcamera_driver& dcamera_driver, tcamera& camera, map_controller& controller);

	void did_run_state_changed(int mode, bool start);

	tlistbox& positions_widget() { return *positions_widget_; }

	const std::string& row_uuid(int at) const;
	void reload_position_list(tlistbox& list);

	void did_denoise_map(bool start);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;
	void post_show() override;

	void pre_mode_navigation_stopped(tgrid& grid);
	void pre_mode_position(tgrid& grid);
	void pre_mode_slam(tgrid& grid);
	void pre_mode_confirm(tgrid& grid);
	void pre_teleop_grid(tgrid& grid);

	enum {edittype_positionnew, edittype_positionedit, edittype_count};
	bool verify_edit(const std::string& label, int type, const tmap_position* position) const;
	std::string handle_edit_box(int type, const tmap_position* position, double& theta);

	// navigation layer
	void pre_mapfile_unselect(tgrid& grid);
	void pre_mapfile_selected(tgrid& grid);
	void reload_file_list(tlistbox& list);
	void did_file_changed(tlistbox& list, ttoggle_panel& row);
	void click_delete_rspfile(tbutton& widget);
	void click_debug(tbutton& widget);

	// position layer
	void did_position_changed(tlistbox& list, ttoggle_panel& row);
	void click_position_insert(tbutton& widget, tlistbox& list);
	void click_position_edit(tbutton& widget, tlistbox& list);
	void click_position_erase(tbutton& widget, tlistbox& list);
	void click_insert_wall(tbutton& widget);

	void click_mode(tbutton& widget);
	int mode_2_mode_layer(int mode);

	// teloop grid
	void pre_teleop_preset(tgrid& grid);
	void pre_teleop_custom(tgrid& grid);
	void click_velocity(tbutton& widget);
	void click_teleop(int op);
	enum {fid_velocity_x, fid_velocity_y, fid_velocity_theta};
	bool curr_velocity_valid() const;
	void did_custom_velocity_changed(ttext_box& widget, int fid);
	void click_custom_move();

	void statusbar_refresh_report(int num, const std::string& label) override;

private:
	tros_instance& ros_instance_;
	map_controller& controller_;
	const std::string& saves_map_dir_;
	const std::set<std::string>& files_;
	const std::map<std::string, tmap_position>& positions_;
	std::vector<std::string> uuids_in_list_;
	tstack* mode_stack_;
	tstack* mapfile_stack_;
	tlistbox* files_widget_;
	tbutton* delete_widget_;
	tbutton* denoise_map_widget_;

	tlistbox* positions_widget_;
	tbutton* position_edit_widget_;
	tbutton* position_erase_widget_;

	tgrid* teleop_grid_;
	tstack* velocity_stack_;
	tbutton* custom_move_widget_;
	double curr_velocity_[3];
};

} //end namespace gui2

#endif
