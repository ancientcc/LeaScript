#ifndef GUI_DIALOGS_HEALTH_THEME_HPP_INCLUDED
#define GUI_DIALOGS_HEALTH_THEME_HPP_INCLUDED

// #include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/text_box2.hpp"

class health_controller;
class trhealth_scene_slot;

namespace gui2 {
class tlabel;
class tbutton;
class tstack;
class treport;
class ttoggle_button;

class thealth_scene: public tdialog
{
public:
	enum { ZOOM, POSITION, NUM_REPORTS};

	enum {
		HOTKEY_RETURN = HOTKEY_MIN,
		HOTKEY_SHARE,
	};

	enum {TB_NORMAL_LAYER, TB_SHARE_LAYER};

	thealth_scene(trhealth_scene_slot& scene_slot, health_controller& controller);
	~thealth_scene();

	treport& main_report() { return *main_report_; }
	ttext_box2& watermark_widget() { return *watermark_widget_; }

	tbutton& ok2_widget() { return *ok2_widget_; }
	void set_status_label(const std::string& msg);

	tfloat_widget& flt_erase_widget() { return *flt_erase_widget_; }

	enum {MSG_SHOW_MESSAGE = POST_MSG_MIN_APP};
	struct tmsg_data_show_message: public rtc::MessageData {
		explicit tmsg_data_show_message(const std::string& msg)
			: msg(msg)
		{
		}

		~tmsg_data_show_message()
		{
		}

		const std::string msg;
	};

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	const std::string& window_id() const override;

	void pre_show() override;
	void app_first_drawn() override;
	void app_resize_screen() override;

	void pre_toolbar_normal(tgrid& grid);
	void pre_toolbar_share(tgrid& grid);

	bool did_navigation_report_item_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to);
	void did_navigation_report_item_changed(ttoggle_button& widget);

	void click_share(tbutton& widget);
	void click_share_result(tbutton& widget, bool finish);

	// void statusbar_refresh_report(int num, const std::string& label) override;

	void app_OnMessage(rtc::Message* msg) override;

private:
	trhealth_scene_slot& scene_slot_;
	health_controller& controller_;

	int curr_toolbar_layer_;

	tstack* toolbar_stack_;
	ttext_box2* watermark_widget_;
	treport* main_report_;
	tbutton* ok2_widget_;
	tlabel* status_widget_;
	tfloat_widget* flt_erase_widget_;
};

} //end namespace gui2

#endif
