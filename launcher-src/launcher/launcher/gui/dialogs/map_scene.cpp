#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/map_scene.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/edit_position.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/label.hpp"

#include "map_controller.hpp"
#include "hotkeys.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "game_config.hpp"
#include "ros_instance.hpp"

using namespace std::placeholders;


namespace gui2 {

REGISTER_DIALOG(launcher, map_scene);

tmap_scene::tmap_scene(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tros_instance& ros_instance, tdcamera_driver& dcamera_driver, tcamera& camera, map_controller& _controller)
	: tdialog(&_controller)
	, tstatusbar(rdpd_mgr, pble, privacy)
	, ros_instance_(ros_instance)
	, controller_(_controller)
	, saves_map_dir_(_controller.saves_map_dir())
	, files_(_controller.files())
	, positions_(controller_.curmap().positions)
	, mode_stack_(nullptr)
	, mapfile_stack_(nullptr)
	, files_widget_(nullptr)
	, delete_widget_(nullptr)
	, denoise_map_widget_(nullptr)
	, positions_widget_(nullptr)
	, position_edit_widget_(nullptr)
	, position_erase_widget_(nullptr)
	, teleop_grid_(nullptr)
	, velocity_stack_(nullptr)
	, custom_move_widget_(nullptr)
{
	// secne doesn't support 'set_timer_interval+app_timer_handler'.
	// set_timer_interval(1000);

	for (int at = 0; at < sizeof(curr_velocity_) / sizeof(curr_velocity_[0]); at ++) {
		curr_velocity_[at] = float_nposm;
	}
}

void tmap_scene::pre_show()
{
	// prepare status report.
	reports_.insert(std::make_pair(ZOOM, "zoom"));
	reports_.insert(std::make_pair(POSITION, "position"));
	reports_.insert(std::make_pair(STATUS, "status"));
	reports_.insert(std::make_pair(EULER_Z, "euler_z"));
	reports_.insert(std::make_pair(BATTERY, "battery"));
	reports_.insert(std::make_pair(DRIVER, "driver"));

	// prepare hotkey
	hotkey::insert_hotkey(HOTKEY_RETURN, "return", null_str);
	hotkey::insert_hotkey(HOTKEY_START, "start", null_str);
	hotkey::insert_hotkey(HOTKEY_SAVE_MAP, "save_map", null_str);
	hotkey::insert_hotkey(HOTKEY_AUTO_BUILDMAP, "auto_buildmap", null_str);
	hotkey::insert_hotkey(HOTKEY_CANCEL_GOAL, "cancel_goal", null_str);
	hotkey::insert_hotkey(HOTKEY_SWITCH_SLAM, "switch_slam", null_str);
	hotkey::insert_hotkey(HOTKEY_DENOISE_MAP, "denoise_map", null_str);
	hotkey::insert_hotkey(HOTKEY_REPOSITION_ROBOT, "reposition_robot", null_str);
	hotkey::insert_hotkey(HOTKEY_CONFIRM_OK, "confirm_ok", null_str);
	hotkey::insert_hotkey(HOTKEY_CONFIRM_CANCEL, "confirm_cancel", null_str);

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid(), &reports_);
	
	tbutton* widget = dynamic_cast<tbutton*>(get_object("return"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("start"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("zoomin"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("zoomout"));
	click_generic_handler(*widget, null_str);

	find_widget<tlabel>(window_, "warnning", false, true)->set_label(ht::generate_format(disable_timing_warnning(), 0xffff0000));

	tbutton* button = find_widget<tbutton>(window_, "mode", false, true);
	// button->set_border("textbox");
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tmap_scene::click_mode
			, this, std::ref(*button)));
    button->set_label(controller_.cur_mode().name);

	tstack* stack = find_widget<tstack>(window_, "mode_stack", false, true);;
	pre_mode_navigation_stopped(*stack->layer(NAVIGATION_STOPPED_LAYER));
	pre_mode_position(*stack->layer(POSITION_LAYER));
	pre_mode_slam(*stack->layer(SLAM_LAYER));
	pre_mode_confirm(*stack->layer(CONFIRM_LAYER));
	stack->set_radio_layer(preferences::mapop_mode());
	mode_stack_ = stack;

	widget = dynamic_cast<tbutton*>(get_object("save_map"));
	widget->set_label("misc/save.png");
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("auto_buildmap"));
	widget->set_label("misc/auto_buildmap.png");
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("cancel_goal"));
	widget->set_label("misc/cancel_goal.png");
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("switch_slam"));
	widget->set_label("misc/switch.png");
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("denoise_map"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("reposition_robot"));
	widget->set_label("misc/reposition_robot.png");
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("confirm_ok"));
	click_generic_handler(*widget, null_str);

	widget = dynamic_cast<tbutton*>(get_object("confirm_cancel"));
	click_generic_handler(*widget, null_str);

	teleop_grid_ = find_widget<tgrid>(window_, "teleop_grid", false, true);
	pre_teleop_grid(*teleop_grid_);
}

void tmap_scene::post_show()
{
}

void tmap_scene::pre_mode_slam(tgrid& grid)
{
}

void tmap_scene::pre_mode_navigation_stopped(tgrid& grid)
{
	tlistbox* list = find_widget<tlistbox>(&grid, "files", false, true);
	list->set_did_row_changed(std::bind(&tmap_scene::did_file_changed, this, _1, _2));

	files_widget_ = list;

	tstack* stack = find_widget<tstack>(window_, "mapfile_stack", false, true);;
	pre_mapfile_unselect(*stack->layer(MAPFILE_UNSELECT_LAYER));
	pre_mapfile_selected(*stack->layer(MAPFILE_SELECTED_LAYER));
	// stack->set_radio_layer(preferences::mapop_mode());
	mapfile_stack_ = stack;
}

void tmap_scene::pre_mode_position(tgrid& grid)
{
	tlistbox* list = find_widget<tlistbox>(&grid, "positions", false, true);
	list->set_did_row_changed(std::bind(&tmap_scene::did_position_changed, this, _1, _2));
	positions_widget_ = list;

	tbutton* button = find_widget<tbutton>(&grid, "insert", false, true);
    connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tmap_scene::click_position_insert
			, this, std::ref(*button), std::ref(*list)));

	button = find_widget<tbutton>(&grid, "edit", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tmap_scene::click_position_edit
			, this, std::ref(*button), std::ref(*list)));
	position_edit_widget_ = button;

	button = find_widget<tbutton>(&grid, "erase", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tmap_scene::click_position_erase
			, this, std::ref(*button), std::ref(*list)));
	position_erase_widget_ = button;

	button = find_widget<tbutton>(&grid, "insert_wall", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tmap_scene::click_insert_wall
			, this, std::ref(*button)));
	utils::string_map symbols;
	symbols["marker"] = game_config::markers.find(rspmapmarkertype_wall)->second;
	button->set_label(vgettext2("Insert $marker", symbols));

}

void tmap_scene::pre_mode_confirm(tgrid& grid)
{
}

void tmap_scene::pre_teleop_grid(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "velocity", false, true);
    connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tmap_scene::click_velocity
			, this, std::ref(*button)));
	const int curr_velocity = preferences::velocity();
	button->set_label(game_config::velocities[curr_velocity]);
	
	tstack* stack = find_widget<tstack>(window_, "velocity_stack", false, true);;
	pre_teleop_preset(*stack->layer(TELEOP_PRESET_LAYER));
	pre_teleop_custom(*stack->layer(TELEOP_CUSTOM_LAYER));
	stack->set_radio_layer(curr_velocity != velocity_custom? TELEOP_PRESET_LAYER: TELEOP_CUSTOM_LAYER);
	velocity_stack_ = stack;
}

void tmap_scene::pre_mapfile_unselect(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "debug", false, true);
	// button->set_label("misc/debug.png");
    connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tmap_scene::click_debug
			, this, std::ref(*button)));
}

void tmap_scene::pre_mapfile_selected(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "delete", false, true);
        connect_signal_mouse_left_click(
			    *button
		    , std::bind(
			    &tmap_scene::click_delete_rspfile
			    , this, std::ref(*button)));
	delete_widget_ = button;

	denoise_map_widget_ = find_widget<tbutton>(&grid, "denoise_map", false, true);
}

void tmap_scene::reload_file_list(tlistbox& list)
{
	list.clear();
	std::map<std::string, std::string> data;
	for (std::set<std::string>::const_reverse_iterator rit = files_.rbegin(); rit != files_.rend(); ++ rit) {
		const std::string& file = *rit;

		bool is_curmap = controller_.curmap().rspfile == file;
		std::string label = utils::extract_file(file);
		if (is_curmap) {
			label = ht::generate_format(label, color_to_uint32(font::BLUE_COLOR)); // GOOD_COLOR
		}
		data["label"] = label;

		ttoggle_panel& row = list.insert_row(data);
		// row.set_child_icon("label", item.icon);
		// row.set_cookie(item.cookie);
	}
	delete_widget_->set_active(false);
	denoise_map_widget_->set_active(false);
}

void tmap_scene::did_file_changed(tlistbox& list, ttoggle_panel& row)
{
	tgrid* layer_grid = mode_stack_->layer(NAVIGATION_STOPPED_LAYER);

	const int at = list.rows() - 1 - row.at();
	std::set<std::string>::const_iterator it = files_.begin();
	std::advance(it, at);

	const std::string file = *it;
	bool result = controller_.set_navigation_rspfile(file);
	delete_widget_->set_active(true);
	denoise_map_widget_->set_active(result);

	
	mapfile_stack_->set_radio_layer(MAPFILE_SELECTED_LAYER);
}

void tmap_scene::click_delete_rspfile(gui2::tbutton& widget)
{
	tlistbox& list = *files_widget_;
	ttoggle_panel* cursel = list.cursel();
	if (cursel == nullptr) {
		return;
	}

	const int at = list.rows() - 1 - cursel->at();
	std::set<std::string>::const_iterator it = files_.begin();
	std::advance(it, at);

	const std::string rspfile = *it;
	utils::string_map symbols;
	symbols["file"] = utils::extract_file(rspfile);
	int res = gui2::show_message2("", vgettext2("Do you want to delete $file?", symbols), gui2::tmessage::yes_no_buttons);
	if (res != gui2::twindow::OK) {
		return;
	}

	SDL_DeleteFiles(rspfile.c_str());
	controller_.did_rspfile_deleted(rspfile);
	reload_file_list(list);
}

void tmap_scene::click_debug(tbutton& widget)
{
	controller_.click_debug(widget);
}

void tmap_scene::did_denoise_map(bool start)
{
	find_widget<tbutton>(window_, "mode", false, true)->set_active(!start);
	find_widget<tbutton>(window_, "start", false, true)->set_active(!start);

	if (start) {
		mode_stack_->set_radio_layer(CONFIRM_LAYER);
	} else {
		mode_stack_->set_radio_layer(NAVIGATION_STOPPED_LAYER);
	}
}

static bool did_compare_position(const tmap_position* a, const tmap_position* b)
{
	// it is mathematical rectangular coordinate system.
	// The higher, the greater y.
	return a->y > b->y || (a->y == b->y && a->x < b->x);
}

void tmap_scene::reload_position_list(tlistbox& list)
{
	list.clear();
	uuids_in_list_.clear();

	std::vector<const tmap_position*> positions;
	for (std::map<std::string, tmap_position>::const_iterator it = positions_.begin(); it != positions_.end(); ++ it) {
		const tmap_position& position = it->second;
		positions.push_back(&position);
	}
	std::stable_sort(positions.begin(), positions.end(), did_compare_position);

	char buf[256];
	std::map<std::string, std::string> data;
	for (std::vector<const tmap_position*>::const_iterator it = positions.begin(); it != positions.end(); ++ it) {
		const tmap_position& position = **it;
		uuids_in_list_.push_back(position.uuid);

		data["name"] = position.name;
		buf[0] = '\0';
		if (!is_float_nposm(position.x)) {
			if (!is_float_nposm(position.theta)) {
				SDL_snprintf(buf, sizeof(buf), "(%.2f, %.2f, %.3f)", position.x, position.y, round(RAD2DEG(position.theta)));
			} else {
				std::string unrestricted_msg = _("unrestricted");
				SDL_snprintf(buf, sizeof(buf), "(%.2f, %.2f, %s)", position.x, position.y, unrestricted_msg.c_str());
			}
		} else {
			SDL_strlcpy(buf, "(--, --, --)", sizeof(buf));
		}
		data["xy"] = buf;

		ttoggle_panel& row = list.insert_row(data);
		// row.set_child_icon("label", item.icon);
		// row.set_cookie(item.cookie);

		row.connect_signal<event::LONGPRESS>(
			std::bind(
				&map_controller::longpress_position
				, &controller_
				, _4, _5, std::ref(*window_), std::ref(row))
			, event::tdispatcher::back_child);

		row.connect_signal<event::LONGPRESS>(
			std::bind(
				&map_controller::longpress_position
				, &controller_
				, _4, _5, std::ref(*window_),std::ref(row))
			, event::tdispatcher::back_post_child);
	}
	position_edit_widget_->set_active(false);
	position_erase_widget_->set_active(false);
}

const std::string& tmap_scene::row_uuid(int at) const 
{
	VALIDATE(at >= 0 && at < (int)uuids_in_list_.size(), null_str);
	return uuids_in_list_[at];
}

bool is_fixed_uuid(const std::string& uuid)
{
	return uuid == charge_pos_uuid;
}

void tmap_scene::did_position_changed(tlistbox& list, ttoggle_panel& row)
{
	tgrid* layer_grid = mode_stack_->layer(POSITION_LAYER);
	const int at = row.at();

	position_edit_widget_->set_active(true);

	bool can_erase = !is_fixed_uuid(row_uuid(at));
	position_erase_widget_->set_active(can_erase);
}

void tmap_scene::click_position_insert(tbutton& widget, tlistbox& list)
{
	double theta;
	std::string ret = handle_edit_box(edittype_positionnew, nullptr, theta);
	if (!ret.empty()) {
		controller_.did_position_insert(ret, theta);
		reload_position_list(*positions_widget_);
	}
}

void tmap_scene::click_position_edit(tbutton& widget, tlistbox& list)
{
	ttoggle_panel* cursel = list.cursel();
	VALIDATE(cursel != nullptr, null_str);

	const std::string& uuid = uuids_in_list_[cursel->at()];
	const tmap_position* position = controller_.position_from_uuid(uuid);
	double theta;
	std::string ret = handle_edit_box(edittype_positionedit, position, theta);
	if (!ret.empty()) {
		controller_.did_position_edit(uuid, ret, theta);
		reload_position_list(*positions_widget_);
	}
}

void tmap_scene::click_position_erase(tbutton& widget, tlistbox& list)
{
	ttoggle_panel* cursel = list.cursel();
	VALIDATE(cursel != nullptr, null_str);

	const std::string& uuid = uuids_in_list_[cursel->at()];
	const tmap_position& position = positions_.find(uuid)->second;

	VALIDATE(!is_fixed_uuid(uuid), null_str);

	utils::string_map symbols;
	symbols["name"] = position.name;
	std::string msg = vgettext2("Do you want to delete '$name'?", symbols);
	if (gui2::show_message2(null_str, msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		return;
	}
	controller_.did_position_erase(uuid);
	reload_position_list(*positions_widget_);
}

void tmap_scene::click_insert_wall(tbutton& widget)
{
	controller_.insert_map_marker(rspmapmarkertype_wall);
}

bool tmap_scene::verify_edit(const std::string& label, int type, const tmap_position* position) const
{
	VALIDATE(type >= 0 && type < edittype_count, null_str);
	if (label.empty()) {
		return false;
	}
	if (label.size() >= RSP_MAP_MAXPOSNAMEBYTES) {
		return false;
	}
	if (type == edittype_positionedit) {
		if (label == position->name) {
			return false;
		}
	}
	return true;
}

std::string tmap_scene::handle_edit_box(int type, const tmap_position* position, double& theta)
{
	VALIDATE(type >= 0 && type < edittype_count, null_str);

	gui2::tedit_position dlg(type == edittype_positionedit, position);
	dlg.show(nposm, window_->get_height() / 8);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return null_str;
	}
	theta = dlg.get_theta();
	return dlg.get_name();
}

int tmap_scene::mode_2_mode_layer(int mode)
{
	int desire_mode_layer = nposm;
	if (mode == mode_buildmap) {
		desire_mode_layer = BUILDMAP_STOPPED_LAYER;

	} else if (mode == mode_navigation) {
		desire_mode_layer = NAVIGATION_STOPPED_LAYER;

	} else {
		VALIDATE(mode == mode_position, null_str);
		desire_mode_layer = POSITION_LAYER;
	}
	return desire_mode_layer;
}

void tmap_scene::click_mode(gui2::tbutton& widget)
{
	const std::vector<tgui_mode>& gui_modes = game_config::gui_modes;
	const tgui_mode& cur_mode = controller_.cur_mode();
	const bool use_rosbag = true;

    std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::vector<const tgui_mode*> candicates;
    
	for (std::vector<tgui_mode>::const_iterator it = gui_modes.begin(); it != gui_modes.end(); ++ it) {
		const tgui_mode& gui_mode = *it;
		if (!controller_.can_swtich_to_mode(gui_mode.mode)) {
			continue;
		}
		if (!use_rosbag && gui_mode.rosbag != nposm) {
			continue;
		}

		items.push_back(gui2::tmenu::titem(gui_mode.name, (int)candicates.size()));
		if (gui_mode.mode == cur_mode.mode && gui_mode.rosbag == cur_mode.rosbag) {
			initial_sel = (int)candicates.size();
		}
		candicates.push_back(&gui_mode);
	}
	VALIDATE(!items.empty(), null_str);
	
	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();
	const tgui_mode& new_mode = *candicates[cursel];

	mode_stack_->set_radio_layer(mode_2_mode_layer(new_mode.mode));

	controller_.set_cur_mode(new_mode);
    preferences::set_mapop_mode(new_mode.mode);
	widget.set_label(new_mode.name);
}

// is called by map_controller only.
void tmap_scene::did_run_state_changed(int mode, bool start)
{
	tgrid& grid = *mode_stack_->layer(mode);

	find_widget<tbutton>(window_, "mode", false, true)->set_active(!start && tspecial_mapop_mode_lock::mode == nposm);
	find_widget<tgrid>(window_, "teleop_grid", false, true)->set_visible(start? twidget::VISIBLE: twidget::INVISIBLE);

	if (controller_.mapviewer()) {
		VALIDATE(start, null_str);
		VALIDATE(mode == NAVIGATION_STOPPED_LAYER, null_str);

		bool enable_teleop_when_map_viewer = game_config::is_dbg_charge();
		if (!enable_teleop_when_map_viewer) {
			find_widget<tgrid>(window_, "teleop_grid", false, true)->set_visible(twidget::INVISIBLE);
		} else {
			find_widget<tgrid>(window_, "teleop_grid", false, true)->set_visible(twidget::VISIBLE);
		}
	}
	if (start) {
		VALIDATE(mode >= 0 && mode <= mode_maxros, null_str);
		// stopped --> started
		files_widget_->select_row(nposm);

		mode_stack_->set_radio_layer(SLAM_LAYER);

	} else {
		// started --> stopped
		VALIDATE(!controller_.mapviewer(), null_str);
		mode_stack_->set_radio_layer(mode_2_mode_layer(mode));

		if (mode == mode_buildmap) {

		} else if (mode == mode_navigation) {
			mapfile_stack_->set_radio_layer(MAPFILE_UNSELECT_LAYER);
			reload_file_list(*files_widget_);

		} else if (mode == mode_position) {
			controller_.did_position_entered();
			reload_position_list(*positions_widget_);
		}
	}
}


//
// teleop grid
//
void tmap_scene::pre_teleop_preset(tgrid& grid)
{
	std::map<std::string, int> teleops;
    teleops.insert(std::make_pair("forward", teleop_forward));
    teleops.insert(std::make_pair("forwardleft", teleop_forwardleft));
    teleops.insert(std::make_pair("forwardright", teleop_forwardright));
    teleops.insert(std::make_pair("backward", teleop_backward));
    teleops.insert(std::make_pair("backwardleft", teleop_backwardleft));
    teleops.insert(std::make_pair("backwardright", teleop_backwardright));

    for (std::map<std::string, int>::const_iterator it = teleops.begin(); it != teleops.end(); ++ it) {
        tbutton* button = find_widget<tbutton>(window_, it->first, false, true);
        connect_signal_mouse_left_click(
			    *button
		    , std::bind(
			    &tmap_scene::click_teleop
			    , this, it->second));
    }
}

void tmap_scene::pre_teleop_custom(tgrid& grid)
{
	tbutton* button = find_widget<tbutton>(&grid, "move", false, true);
	button->set_label(_("Move"));
	connect_signal_mouse_left_click(
			    *button
		    , std::bind(
			    &tmap_scene::click_custom_move
			    , this));
	custom_move_widget_ = button;

	double twist[3];
	preferences::custom_vel(twist);

	std::map<int, std::string> ids;
	ids.insert(std::make_pair(fid_velocity_x, "x"));
	ids.insert(std::make_pair(fid_velocity_y, "y"));
	ids.insert(std::make_pair(fid_velocity_theta, "theta"));

	for (std::map<int, std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it) {
		int fid = it->first;
		const std::string& id = it->second;
		ttext_box* text_box = find_widget<ttext_box>(&grid, id, false, true);
		text_box->set_placeholder(id);
		text_box->set_maximum_chars(5);
		text_box->set_did_text_changed(std::bind(&tmap_scene::did_custom_velocity_changed, this, _1, fid));
		text_box->set_label(utils::from_double(twist[fid]));
	}
}

void tmap_scene::click_velocity(tbutton& widget)
{
	VALIDATE(!game_config::velocities.empty(), null_str);

    std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;
    
	for (std::map<int, std::string>::const_iterator it = game_config::velocities.begin(); it != game_config::velocities.end(); ++ it) {
		items.push_back(gui2::tmenu::titem(it->second, it->first));
		if (it->first == preferences::velocity()) {
			initial_sel = it->first;
		}
	}
	
	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int new_velocity = dlg.selected_val();
    preferences::set_velocity(new_velocity);
	widget.set_label(game_config::velocities[new_velocity]);
	velocity_stack_->set_radio_layer(new_velocity != velocity_custom? TELEOP_PRESET_LAYER: TELEOP_CUSTOM_LAYER);
}

double to_double(const std::string& str)
{
	const char* c_str = str.c_str();
	int size = str.size();
	
	double result = float_nposm;
	if (size != 0) {
		char* endp = nullptr;
		result = SDL_strtod(c_str, &endp);
		if (endp[0] != '\0') {
			result = float_nposm;
		}
	}
	return result;
}

static std::string format_pub_twist(double* twist)
{
	char buf[128];
	SDL_snprintf(buf, sizeof(buf), "%s pub twist: %.3f, %.3f, %.3f(deg:%.1f)",
	utils::format_time_hms(time(nullptr)).c_str(), twist[0], twist[1], twist[2], RAD2DEG(twist[2]));

	return buf;
}

void tmap_scene::click_teleop(int op)
{
	tros_instance& ros_instance = controller_.get_ros_instance();
	double twist[3];
    ros_instance.do_riki_action(op, twist);
	controller_.set_status_report(format_pub_twist(twist));
}

bool tmap_scene::curr_velocity_valid() const
{
	return !is_float_nposm(curr_velocity_[0]) && !is_float_nposm(curr_velocity_[1]) && !is_float_nposm(curr_velocity_[2]);
}

void tmap_scene::did_custom_velocity_changed(ttext_box& widget, int fid)
{
	VALIDATE(fid >= 0 && fid < (int)(sizeof(curr_velocity_) / sizeof(curr_velocity_[0])), null_str);

	const std::string& label = widget.label();	
	curr_velocity_[fid] = to_double(label);

	custom_move_widget_->set_active(curr_velocity_valid());
}

void tmap_scene::click_custom_move()
{
	VALIDATE(curr_velocity_valid(), null_str);

	double twist[3] = {curr_velocity_[0], curr_velocity_[1], DEG2RAD(curr_velocity_[2])};
	ros_instance_.public_vel(twist[0], twist[1], twist[2]);
	preferences::set_custom_vel(curr_velocity_);

	controller_.set_status_report(format_pub_twist(twist));
}

void tmap_scene::statusbar_refresh_report(int num, const std::string& label)
{
	controller_.gui().refresh_report(num, reports::report(label, null_str));
}

} //end namespace gui2
