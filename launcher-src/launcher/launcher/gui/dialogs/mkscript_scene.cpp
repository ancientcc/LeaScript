#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/mkscript_scene.hpp"
#include "gui/dialogs/menu.hpp"
#include "mkscript_controller.hpp"
#include "hotkeys.hpp"
#include "gettext.hpp"

namespace gui2 {

REGISTER_DIALOG(launcher, mkscript_scene);

tmkscript_scene::tmkscript_scene(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, mkscript_controller& controller)
	: tdialog(&controller)
	, tstatusbar(rdpd_mgr, pble, privacy)
	, controller_(controller)
{
}

void tmkscript_scene::pre_show()
{
	// prepare status report.
	reports_.insert(std::make_pair(ZOOM, "zoom"));
	reports_.insert(std::make_pair(POSITION, "position"));

	// prepare hotkey
	hotkey::insert_hotkey(HOTKEY_BUILD, "build", _("Build gui.bin"));
	hotkey::insert_hotkey(HOTKEY_SAVE, "save", _("Save"));
	hotkey::insert_hotkey(HOTKEY_SETTING, "settings", _("Settings"));
	hotkey::insert_hotkey(HOTKEY_NEXT_STATE, "next_state", _("Next state"));
	hotkey::insert_hotkey(HOTKEY_CLONE, "clone", _("Clone"));
	hotkey::insert_hotkey(HOTKEY_SWITCH_SUB1, "switch_sub1", _("Switch sub1"));
	hotkey::insert_hotkey(HOTKEY_SWITCH_ADD1, "switch_add1", _("Switch add1"));
	hotkey::insert_hotkey(HOTKEY_ERASE, "erase", _("Erase"));
	hotkey::insert_hotkey(HOTKEY_ERASE_ROW, "erase_row", _("Erase row"));
	hotkey::insert_hotkey(HOTKEY_INSERT_RIGHT, "insert_right", _("Insert right"));
	hotkey::insert_hotkey(HOTKEY_ADD_TO_WORKING_DIR, "add_to_working_dir", controller_.add_to_working_dir_msgstr());

	hotkey::insert_hotkey(HOTKEY_INSERT_CHILD, "insert_child", _("Insert child"));
	hotkey::insert_hotkey(HOTKEY_WORKING_DIR, "working_dir", _("Working directory"));
	hotkey::insert_hotkey(HOTKEY_SHARE, "share", _("Share"));

	hotkey::insert_hotkey(HOTKEY_UNPACK, "unpack", _("Unpack"));
	hotkey::insert_hotkey(HOTKEY_PACK, "pack", _("Pack"));

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid(), &reports_);

	tbutton* button = gui2::find_widget<gui2::tbutton>(window_, "file", false, true);
	connect_signal_mouse_left_click(
		*button
		, std::bind(
			&tmkscript_scene::click_file
			, this
			, std::ref(*button)));
}

void tmkscript_scene::app_first_drawn()
{
	controller_.app_first_drawn();
}

void tmkscript_scene::app_resize_screen()
{
	controller_.app_resize_screen();
}

void tmkscript_scene::click_file(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
    
	const std::map<int, std::string>& ops = controller_.file_ops();

	std::vector<gui2::tmenu::titem> new_items;
	std::set<int> new_codes{mkscript_controller::file_new_empty, mkscript_controller::file_new_from_benchmark};
	for (std::set<int>::const_iterator it = new_codes.begin(); it != new_codes.end(); ++ it) {
		int code = *it;
		const std::string& name = ops.find(code)->second;
		new_items.push_back(gui2::tmenu::titem(name, code));
	}
	items.push_back(gui2::tmenu::titem(_("file^New"), new_items));
	for (std::map<int, std::string>::const_iterator it = ops.begin(); it != ops.end(); ++ it) {
		int code = it->first;
		if (new_codes.count(code) != 0) {
			continue;
		}
		const std::string& name = it->second;
		items.push_back(gui2::tmenu::titem(name, code));
		if (code == mkscript_controller::file_exit - 1) {
			items.back().separator = true;
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
	controller_.handle_file_menu(cursel);
}

void tmkscript_scene::app_OnMessage(rtc::Message* msg)
{
	switch (msg->message_id) {
	case MSG_OBJ_CLICKED:
		{
			tmsg_data_obj_clicked* pdata = static_cast<tmsg_data_obj_clicked*>(msg->pdata);
			controller_.click_object(pdata->obj_clicked);
		}
		break;

	default:
		VALIDATE(false, null_str);
	}

	if (msg->pdata != nullptr) {
		delete msg->pdata;
	}
}

} //end namespace gui2
