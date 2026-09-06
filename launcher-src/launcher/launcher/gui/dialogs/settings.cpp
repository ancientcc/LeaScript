#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/settings.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/privacy2.hpp"
#include "gui/dialogs/import2.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/login.hpp"
#include "gettext.hpp"
#include "game_config.hpp"
#include "formula_string_utils.hpp"
#include "ros_instance.hpp"
#include "base_instance.hpp"
#include "net.hpp"
#include "cfg_cpp_api.hpp"

#include <rose_ros/utils.hpp>

std::string im948_label(const tbase_driver& base_driver)
{
    if (!base_driver.use_external_imu()) {
        return "---";
    }
    return base_driver.slot->external_imu() != nullptr? _("im948 running"): _("Open im948");
}

namespace gui2 {

REGISTER_DIALOG(launcher, settings)

tsettings::tsettings(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tros_instance& ros_instance, tros_map& curmap, 
	std::map<aplt::taplt_key, aplt::tapplet>& applets, tinstance_slot& slot, aplt::tcfg_cpp_api& cfg_cpp_api, tbase_driver& base_driver, tmoveit_driver& moveit_driver, 
	tlaser_driver& laser_driver, tdcamera_driver& dcamera_driver, tiot_driver& iot_driver, tspeech_driver& speech_driver, tai_driver& ai_driver,
	const trobot_imu& robot_imu, const std::string& saves_map_dir)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, ros_instance_(ros_instance)
	, curmap_(curmap)
	, applets_(applets)
	, slot_(slot)
	, cfg_cpp_api_(cfg_cpp_api)
	, base_driver_(base_driver)
	, original_base_aplt_id_(preferences::driver(apltsotype_base))
	, moveit_driver_(moveit_driver)
	, original_moveit_aplt_id_(preferences::driver(apltsotype_moveit))
	, laser_driver_(laser_driver)
	, original_laser_aplt_id_(preferences::driver(apltsotype_laser))
	, dcamera_driver_(dcamera_driver)
	, original_dcamera_aplt_id_(preferences::driver(apltsotype_dcamera))
	, iot_driver_(iot_driver)
	, original_iot_aplt_id_(preferences::driver(apltsotype_iot))
	, speech_driver_(speech_driver)
	, original_speech_aplt_id_(preferences::driver(apltsotype_speech))
	, ai_driver_(ai_driver)
	, original_aiagent_aplt_id_(preferences::driver(apltsotype_ai))
	, robot_imu_(robot_imu)
	, saves_map_dir_(saves_map_dir)
	, disable_new_aplt_lock_(aplt::tdisable_new_klink_task_lock::reason_settings)
	, privacy_protection_widget_(nullptr)
	, status_widget_(nullptr)
	, rpy_widget_(nullptr)
	, start_imu_widget_(nullptr)
{
	VALIDATE(!instance->bg_task().is_ing(), null_str);
	set_timer_interval(80);
}

extern void get_task_item3fs_ex(const tmoveit_driver& moveit_driver, std::vector<gui2::tmenu::titem>& items, std::vector<aplt::ttask_item3f>& item3fs, uint32_t allow_type_mask, bool allow_aiagent, bool allow_empty);
extern std::string generate_task2_desc(const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task, const std::string& device_id);

void set_charge_width_label(tbutton& widget, int width_mm)
{
	std::string label;
	if (is_valid_charge_width(width_mm)) {
		utils::string_map symbols;
		symbols["dist"] = str_cast(width_mm);
		label = vgettext2("$dist mm", symbols);
	} else {
		label = _("Unset");
	}
	widget.set_label(label);
}

void tsettings::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	find_widget<tlabel>(window_, "title", false, true)->set_label(aplt::all_fake_applets.find(aplt::builtinid_settings)->second.name);
	find_widget<tlabel>(window_, "warnning", false, true)->set_label(ht::generate_format(disable_timing_warnning(), 0xffff0000));

	status_widget_ = find_widget<tlabel>(window_, "status", false, true);
	rpy_widget_ = find_widget<tlabel>(window_, "rpy", false, true);

	utils::string_map symbols;

	//
	// cswamp login
	//
	tbutton* button = find_widget<tbutton>(window_, "login", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tsettings::click_login
			, this, std::ref(*button)));
	set_login_label();

	//
	// privacy_protect
	//
	button = find_widget<tbutton>(window_, "privacy", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tsettings::click_privacy
			, this, std::ref(*button)));

	ttoggle_button* toggle = find_widget<ttoggle_button>(window_, "privacy_protection", false, true);
	toggle->set_did_state_changed(std::bind(&tsettings::did_privacy_protection, this, _1));
	toggle->set_value(privacy_.protect());
	privacy_protection_widget_ = toggle;
	if (!privacy_.protect()) {
		set_privacy_desc_label();
	}

	//
	// drivers
	//
	gui2::tlabel* label = find_widget<tlabel>(window_, "dcamera_moveit_driver_help", false, true);
	symbols["dcamera_driver"] = _("DCamera driver");
	symbols["moveit_driver"] = _("Moveit driver");
	std::string msg = vgettext2("dcamera_moveit_driver_help $dcamera_driver $moveit_driver", symbols);
	label->set_label(msg);

	label = find_widget<tlabel>(window_, "iot_speech_driver_help", false, true);
	symbols["iot_driver"] = _("IoT driver");
	symbols["speech_driver"] = _("Speech driver");
	msg = vgettext2("iot_speech_driver_help $iot_driver $speech_driver", symbols);
	label->set_label(msg);

	std::map<std::string, std::string> id_2_title2s;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		const aplt::tapplet& applet = it->second;
		id_2_title2s.insert(std::make_pair(applet.id, applet.name2()));
	}

	for (std::map<int, std::string>::const_iterator it = game_config::driver_keys.begin(); it != game_config::driver_keys.end(); ++ it) {
		int type = it->first;
		if (type > apltsotype_maxdriver) {
			continue;
		}
		const std::string& id = it->second;
		button = find_widget<tbutton>(window_, id, false, true);
		// button->set_border("textbox");
		connect_signal_mouse_left_click(
				*button
			, std::bind(
				&tsettings::click_driver
				, this, std::ref(*button), type));
		std::string applet_id = preferences::driver(type);
		if (id_2_title2s.count(applet_id) != 0) {
			button->set_label(id_2_title2s.find(applet_id)->second);
		}
	}

	//
	// imu grid
	//
	if (base_driver_.installed() && base_driver_.use_external_imu()) {
		button = find_widget<tbutton>(window_, "start_imu", false, true);
		button->set_label(im948_label(base_driver_));
		connect_signal_mouse_left_click(
				*button
			, std::bind(
				&tsettings::click_start_imu
				, this, std::ref(*button)));
		start_imu_widget_ = button;

	} else {
		find_widget<tgrid>(window_, "imu_grid", false, true)->set_visible(twidget::INVISIBLE);
	}

	//
	// charge grid
	//
	button = find_widget<tbutton>(window_, "charge_task", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tsettings::click_charge_task
			, this, std::ref(*button)));
	const std::string task_id2 = preferences::charge_task_id2();
	aplt::ttask_pair pair = aplt::split_aplt_task_id2(applets_, task_id2, false);
	if (pair.aplt != nullptr && pair.task != nullptr) {
		const aplt::tapplet& aplt = *pair.aplt;
		const aplt::tapplet::ttask& task = *pair.task;
		button->set_label(generate_task2_desc(*pair.aplt, *pair.task, null_str));
	}

	button = find_widget<tbutton>(window_, "charge_width", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tsettings::click_charge_width
			, this, std::ref(*button)));
	set_charge_width_label(*button, preferences::charge_width_mm());

	button = find_widget<tbutton>(window_, "map", false, true);
	// button->set_border("textbox");
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tsettings::click_map
			, this, std::ref(*button)));
	if (curmap_.valid()) {
		button->set_label(utils::extract_file(curmap_.rspfile));
	}

	button = find_widget<tbutton>(window_, "map_position", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tsettings::click_map_position
			, this, std::ref(*button)));
	set_map_position_label();

	button = find_widget<tbutton>(window_, "import_klink_cfgs", false, true);
	connect_signal_mouse_left_click(
			*button
		, std::bind(
			&tsettings::click_import_klink_cfgs
			, this, std::ref(*button)));
}

void tsettings::post_show()
{
	if (original_base_aplt_id_ != preferences::driver(apltsotype_base)) {
		// applet of original_base_aplt_id_ will be unload, destroy this base_slot. 
		base_driver_.set_slot(null_str, nullptr);
	}

	if (original_moveit_aplt_id_ != preferences::driver(apltsotype_moveit)) {
		// applet of original_moveit_aplt_id_ will be unload, destroy this moveit_slot. 
		moveit_driver_.set_slot(null_str, nullptr);
	}

	if (original_laser_aplt_id_ != preferences::driver(apltsotype_laser)) {
		// applet of original_laser_aplt_id_ will be unload, destroy this laser_slot. 
		laser_driver_.set_slot(null_str, nullptr);
	}

	if (original_dcamera_aplt_id_ != preferences::driver(apltsotype_dcamera)) {
		// applet of original_dcamera_aplt_id_ will be unload, destroy this dcamera_slot. 
		dcamera_driver_.set_slot(null_str, nullptr);
	}

	if (original_iot_aplt_id_ != preferences::driver(apltsotype_iot)) {
		// applet of original_speech_aplt_id_ will be unload, destroy this speech_slot. 
		iot_driver_.set_slot(null_str, nullptr);
	}

	if (original_speech_aplt_id_ != preferences::driver(apltsotype_speech)) {
		// applet of original_speech_aplt_id_ will be unload, destroy this speech_slot. 
		speech_driver_.set_slot(null_str, nullptr);
	}

	if (original_aiagent_aplt_id_ != preferences::driver(apltsotype_ai)) {
		// applet of original_aiagent_aplt_id_ will be unload, destroy this aiagent_slot. 
		ai_driver_.set_slot(null_str, nullptr);
	}
}

void tsettings::set_login_label()
{
	tlabel* label = find_widget<tlabel>(window_, "login_label", false, true);

	std::stringstream ss;
	if (current_user.valid()) {
		ss << current_user.username;
	} else {
		ss << _("Not logged in");
	}
	label->set_label(ss.str());
}

void tsettings::click_login(tbutton& widget)
{
	gui2::tlogin dlg(current_user);
	dlg.show();
/*
	if (!curmap_.valid()) {
		return;
	}
	// instance->stop_bg_task_if_runing(_("A timing task is using navigation. Do you want to stop the task and thus enter the editing position?"), null_str);

	{
		tspecial_mapop_mode_lock lock(mode_position);
		slot_.start_map_controller(nullptr);
	}
*/
	set_login_label();
}

void tsettings::set_privacy_desc_label()
{
	tlabel* label = find_widget<tlabel>(window_, "privacy_desc", false, true);
	label->set_label(privacy_.desc());
}

void tsettings::click_privacy(tbutton& widget)
{
	tprivacy::tdisable_slice_lock lock(privacy_);

	{
		gui2::tprivacy2 dlg(rdpd_mgr_, pble_, privacy_);
		dlg.show();
	}
	privacy_.validate();

	set_privacy_desc_label();
}

void tsettings::did_privacy_protection(ttoggle_button& widget)
{
	if (widget.get_value()) {
		// disable --> enable
		if (!privacy_.protect()) {
			privacy_.set_protect(true);
		}

	} else {
		// enable --> disable
		if (privacy_.protect()) {
			privacy_.set_protect(false);
		}
	}

	set_privacy_desc_label();
	preferences::set_privacy_protect(widget.get_value());
}

void tsettings::click_driver(tbutton& widget, int type)
{
	VALIDATE(game_config::driver_keys.count(type) != 0, null_str);
	VALIDATE(type <= apltsotype_maxdriver, null_str);
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;
    
	// initial_sel = applets_.size();
	items.push_back(gui2::tmenu::titem(_("Empty"), applets_.size()));

	int at = 0;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it, at ++) {
		const aplt::tapplet& applet = it->second;
		const std::string id = applet.id;

		if (type == apltsotype_base) {
			if (!applet.base_driver) {
				continue;
			}
		} else if (type == apltsotype_laser) {
			if (!applet.laser_driver) {
				continue;
			}
		} else if (type == apltsotype_dcamera) {
			if (!applet.dcamera_driver) {
				continue;
			}
		} else if (type == apltsotype_moveit) {
			if (!applet.moveit_driver) {
				continue;
			}
		} else if (type == apltsotype_iot) {
			if (!applet.iot_driver) {
				continue;
			}
		} else if (type == apltsotype_speech) {
			if (!applet.speech_driver) {
				continue;
			}
		} else if (type == apltsotype_ai) {
			if (!applet.ai_driver) {
				continue;
			}
		}

		items.push_back(gui2::tmenu::titem(applet.name2(), at));
		if (id == preferences::driver(type)) {
			initial_sel = at;
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

	std::string id;
	std::string title2;
	if (cursel != applets_.size()) {
		const aplt::tapplet& applet = aplt::aplt_from_at(applets_, cursel);
		id = applet.id;
		title2 = applet.name2();
	}
	preferences::set_driver(type, id);
	widget.set_label(title2);
}

void tsettings::click_start_imu(tbutton& widget)
{
    // if (mode_ == mode_position) {
    //    return;
    // }

    VALIDATE(base_driver_.use_external_imu(), null_str);

    utils::string_map symbols;
    bool open_or_close = false;

    std::string serial_path;
    if (base_driver_.slot->external_imu() == nullptr) {
        int im948_baudrate = 115200;
        std::string title = im948_label(base_driver_);
	    std::string prefix;
        std::string placeholder = game_config::os == os_windows ? "COM4": "/dev/ttyS7";
        std::string initial = preferences::im948serial();
        if (initial.empty()) {
            initial = last_im948_serial_;
        }
        if (initial.empty()) {
	        initial = game_config::os == os_windows? "COM4": "/dev/ttyS7";
        }

		symbols["baudrate"] = str_cast(im948_baudrate);
        std::string remark = vgettext2("im948 serial remark $baudrate", symbols);

	    {
		    gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), 20, gui2::tedit_box_param::show_cancel);
		    // param.did_text_changed = std::bind(&tmap_scene::verify_edit, this, _1, type, position);
		    {
			    gui2::tedit_box dlg(param);
			    dlg.show(nposm, window_->get_height() / 4);
			    if (dlg.get_retval() != gui2::twindow::OK) {
				    return;
			    }
		    }
		    serial_path = param.result;
	    }
        base_driver_.slot->open_external_imu(serial_path, im948_baudrate, robot_imu_.has_magnetometer);
        open_or_close = true;

    } else {
        enum {start_calibration, close};

        std::vector<gui2::tmenu::titem> items;
        // items.push_back(gui2::tmenu::titem(_("Calibrate magnetometer"), start_calibration, null_str));
        items.push_back(gui2::tmenu::titem(_("Close im948"), close, null_str));

        int selected;
	    {
		    gui2::tmenu dlg(items, nposm);
		    dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * gui2::twidget::hdpi_scale);
		    int retval = dlg.get_retval();
		    if (dlg.get_retval() != gui2::twindow::OK) {
			    return;
		    }
		    // absolute_draw();
		    selected = dlg.selected_val();
	    }

        if (selected == start_calibration) {
            symbols["ok"] = dgettext("rose-lib", "Yes");
            std::string msg = vgettext2("1/2: step1 of Calibrate magnetometer '$ok'", symbols);
            if (gui2::show_message2(_("Calibrate magnetometer"), msg, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
		        return;
	        }
            // im948_->send_cmd(tim948serial::ID_START_CALIBRATION, nullptr, 0);

            msg = _("2/2: step1 of Calibrate magnetometer");
            gui2::show_message(vgettext2("Calibrate magnetometer", symbols), msg);

            // im948_->send_cmd(tim948serial::ID_STOP_CALIBRATION, nullptr, 0);

        } else if (selected == close) {
            last_im948_serial_ = preferences::im948serial();
            base_driver_.slot->close_external_imu();
            // im948_.reset();
            open_or_close = true;
        }
    }

    if (open_or_close) {
        preferences::set_im948serial(serial_path);
        widget.set_label(im948_label(base_driver_));
    }
}

void tsettings::click_charge_task(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::vector<aplt::ttask_item3f> item3fs;

	get_task_item3fs_ex(moveit_driver_, items, item3fs, BIT_IDX_MASK(aplt::task_nonblock), false, true);

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
	std::string cur_device_id;

	if (cursel > 0) {
		const aplt::ttask_item3f& cur_item = item3fs[cursel - 1];
		cur_aplt = cur_item.aplt;
		cur_task = cur_item.task;
		cur_device_id = cur_item.device_id;
	}

	std::string label;
	std::string new_task_id2;
	if (cur_task != nullptr) {
		label = generate_task2_desc(*cur_aplt, *cur_task, cur_device_id);
		new_task_id2 = utils::join_app_prefix_id(cur_aplt->id, cur_task->id);
	}
	widget.set_label(label);
	preferences::set_charge_task_id2(new_task_id2);
}

bool tsettings::verify_charge_width(const std::string& label) const
{
	if (label.empty()) {
		return false;
	}

	int width = utils::to_int(label);

	return is_valid_charge_width(width);
}

void tsettings::click_charge_width(tbutton& widget)
{
	utils::string_map symbols;

    std::string title = _("Set charge pile width");
	std::string prefix;
    std::string placeholder;

	int charge_width = preferences::charge_width_mm();
	std::string initial = str_cast(charge_width);
	if (charge_width == nposm) {
		initial.clear();
	}

	symbols["min"] = str_cast(charge_width_range.min);
	symbols["max"] = str_cast(charge_width_range.max);
    const std::string remark = vgettext2("Charging pile width, in mm. Range: [$min, $max]", symbols);

	int new_charge_width = nposm;
	{
		gui2::tedit_box_param param(title, prefix, placeholder, initial, remark, null_str, _("OK"), 20, gui2::tedit_box_param::show_cancel);
		param.did_text_changed = std::bind(&tsettings::verify_charge_width, this, _1);
		{
			gui2::tedit_box dlg(param);
			dlg.show(nposm, window_->get_height() / 4);
			if (dlg.get_retval() != gui2::twindow::OK) {
				return;
			}
		}
		new_charge_width = utils::to_int(param.result);
		VALIDATE(is_valid_charge_width(new_charge_width), null_str);
	}

    if (new_charge_width != charge_width) {
		set_charge_width_label(widget, new_charge_width);
		preferences::set_charge_width_mm(new_charge_width);
	}
}

void tsettings::click_map(tbutton& widget)
{
	std::set<std::string> files;
	collect_rsp_files(saves_map_dir_, files);
	if (files.empty()) {
		return;
	}

	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;
    
	std::string curmap;
	if (curmap_.valid()) {
		curmap = utils::extract_file(curmap_.rspfile);
	}
	int at = 0;
	for (std::set<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it, at ++) {
		const std::string file = utils::extract_file(*it);

		items.push_back(gui2::tmenu::titem(file, at));
		if (file == curmap) {
			initial_sel = at;
		}
	}

	if (items.empty()) {
		return;
	}
	// instance->stop_bg_task_if_runing(_("A timing task is using navigation. Do you want to stop the task and thus enter the editing position?"), null_str);

	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();

	std::set<std::string>::const_iterator hit = files.begin();
	std::advance(hit, cursel);

	const std::string& new_rspfile = *hit;
	ros::load_map_from_rsp(new_rspfile, curmap_);
	std::string new_short_rspfile = utils::extract_file(new_rspfile);
	if (!curmap_.valid()) {
		utils::string_map symbols;
		symbols["file"] = new_short_rspfile;
		gui2::show_message(null_str, vgettext2("$file isn't a valid map file", symbols));
		new_short_rspfile.clear();
	}
	preferences::set_curmap(new_short_rspfile);
	widget.set_label(new_short_rspfile);
	set_map_position_label();
}

void tsettings::set_map_position_label()
{
	tlabel* label = find_widget<tlabel>(window_, "map_position_label", false, true);

	std::stringstream ss;
	if (curmap_.valid()) {
		ss << curmap_.positions.size();
	}
	label->set_label(ss.str());
}

void tsettings::click_map_position(tbutton& widget)
{
	if (!curmap_.valid()) {
		return;
	}
	// instance->stop_bg_task_if_runing(_("A timing task is using navigation. Do you want to stop the task and thus enter the editing position?"), null_str);

	{
		tspecial_mapop_mode_lock lock(mode_position);
		slot_.start_map_controller(nullptr);
	}
	set_map_position_label();
}

void tsettings::click_import_klink_cfgs(tbutton& widget)
{
	gui2::timport2 dlg(rdpd_mgr_, pble_, privacy_, cfg_cpp_api_);
	dlg.show();
}

void tsettings::set_status_label(const std::string& msg)
{
	status_widget_->set_label(msg);
}

void tsettings::set_rpy_label()
{
	rpy_widget_->set_label(ros_instance_.get_imu_desc());
}

void tsettings::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
	set_rpy_label();

	if (privacy_.protect() != privacy_protection_widget_->get_value()) {
		privacy_protection_widget_->set_value(privacy_.protect());
	}
}

} // namespace gui2

