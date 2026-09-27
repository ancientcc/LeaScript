#ifndef GUI_DIALOGS_MKSCRIPT_THEME_HPP_INCLUDED
#define GUI_DIALOGS_MKSCRIPT_THEME_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
#include "mkscript_controller.hpp"

// class mkscript_controller;

namespace gui2 {

class tmkscript_scene: public tdialog, public tstatusbar
{
public:
	enum { ZOOM, POSITION, NUM_REPORTS};

	enum {
		HOTKEY_RCLICK = HOTKEY_MIN,

		HOTKEY_BUILD, HOTKEY_SAVE,
		HOTKEY_SETTING, HOTKEY_NEXT_STATE, HOTKEY_CLONE,
		HOTKEY_SWITCH_SUB1, HOTKEY_SWITCH_ADD1, HOTKEY_ERASE,
		HOTKEY_ERASE_ROW, HOTKEY_INSERT_RIGHT, HOTKEY_ADD_TO_WORKING_DIR,
		HOTKEY_INSERT_CHILD, HOTKEY_WORKING_DIR, HOTKEY_SHARE, HOTKEY_UNPACK, HOTKEY_PACK
	};

	tmkscript_scene(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, mkscript_controller& controller);

	enum {MSG_OBJ_CLICKED = POST_MSG_MIN_APP};
	struct tmsg_data_obj_clicked: public rtc::MessageData {
		explicit tmsg_data_obj_clicked(mkscript_controller::tdraw_item_C& _obj)
			: obj_clicked(_obj)
		{
		}

		~tmsg_data_obj_clicked()
		{
		}

		mkscript_controller::tdraw_item_C& obj_clicked;
	};

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	const std::string& window_id() const override;

	void pre_show() override;
	void app_first_drawn() override;
	void app_resize_screen() override;

	void click_file(tbutton& widget);

	void app_OnMessage(rtc::Message* msg) override;

private:
	mkscript_controller& controller_;
};

} //end namespace gui2

#endif
