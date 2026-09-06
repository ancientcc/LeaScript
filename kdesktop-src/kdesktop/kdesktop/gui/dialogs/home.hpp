#ifndef GUI_DIALOGS_HOME_HPP_INCLUDED
#define GUI_DIALOGS_HOME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/dialogs/scan.hpp"
#include "gui/dialogs/rstore.hpp"
#include "gui/dialogs/keepalive.hpp"
#include "gui/dialogs/rlogin.hpp"
#include "game_config.hpp"
#include "base_instance.hpp"

#include "scripts/rose_lua_kernel.hpp"
#include "logs.pb.h"

class tbase_driver_core;
class tdrivers_core;

namespace aplt {
class tcfg_cpp_api_core;
}

namespace gui2 {

class tbutton;
class ttuggle_button;
class ttext_box;
class tgrid;
class treport;
class tstack;
class ttext_box2;

class thome: public tdialog, public tscan, public aplt::tdesktop, public tkeepalive, public trcswamp_login,
	public tbase_msg_subscriber
{
public:
	enum tresult {DESKTOP = 1, APPLET0 = 100};
	enum {RDP_LAYER, SCAN_LAYER, APPLET_LAYER, EVENT_LAYER, MORE_LAYER};
	// enum {LOGIN_LAYER, LOGOUT_LAYER};
	// enum {LOGIN_TYPE_PASSWORD_LAYER, LOGIN_TYPE_COOKIE_LAYER};
	thome(gui2::trstore::tslot& slot, tpbremotes& pbremotes, tble2& ble, std::map<aplt::taplt_key, aplt::tapplet>& applets, 
		tbase_driver_core& base_driver, tdrivers_core& drivers, aplt::tcfg_cpp_api_core& cfg_cpp_api, int startup_layer);
	~thome();

	const trdpcookie& rdpcookie() const { return rdpcookie_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void app_first_drawn() override;

	void pre_rdp(tgrid& grid);
	void pre_applet(tgrid& grid);
	void pre_event(tgrid& grid);
	void pre_more(tgrid& grid);
	bool did_navigation_pre_change(treport& report, ttoggle_button& from, ttoggle_button& to);
	void did_navigation_changed(treport& report, ttoggle_button& row);
	// rdp layer
	void click_orientation(tbutton& widget);
	void click_rdp(tbutton& widget);

	void click_open_url(tbutton& widget, bool vlog_example);
	void did_ratio_switchable_changed(ttoggle_button& widget);
	void did_text_box_changed(tgrid& grid, ttext_box& widget);

	void did_auto_enter_dcamera_changed(ttoggle_button& widget);
	void reload_scene_list(tlistbox& list);
	void click_scene_start(tlistbox& list, tbutton& row, int at);
	bool can_rdp() const;

	// event layer
	void did_keepalive_pullrefresh_refresh(ttrack& widget, const SDL_Rect& rect, tgrid& layer);
	void reload_log_list(tlistbox& list);

	//
	// tbase_msg_subscriber
	//
	// void logs_pb_log_added(int count, const pb2::tlog& log) override;
	void logs_pb_log_added(int count, const pb2::tlog& log);

	// more layer
	void app_did_login_status_changed(bool login) override { did_login_status_changed(login); }
	void click_syncapplets(tbutton& widget);
	void click_me_upgrade(tbutton& widget);

	//
	// applet layer
	//
	void refresh_battery_label();
	void click_install_base_driver(tbutton& widget);

	// override rapplets
	std::vector<aplt::tbuildin> rapplets_get_fake_applets() override;
	void rapplets_did_click_fake_applet(const aplt::tbuildin& applet) override;
	
	// override tkeepalive
	void app_did_network_changed(bool connected) override;
	void app_did_new_events(const std::set<trobot_event>& events) override;
	bool app_can_refersh_remark_label() const override { return current_layer_ == EVENT_LAYER; }
	bool app_disable_slice() const override { return is_busy(); }
	// void app_did_timer_handler() override;

	void app_timer_handler(uint32_t now) override;
	void app_OnMessage(rtc::Message* msg) override;

private:
	// tpbremotes& pbremotes_;
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	tbase_driver_core& base_driver_;
	tdrivers_core& drivers_;
	aplt::tcfg_cpp_api_core& cfg_cpp_api_;
	const int startup_layer_;
	int current_layer_;
	tstack* body_widget_;
	treport* navigation_report_;

	ttext_box* ipaddr_;
	trdpcookie rdpcookie_;
	tlistbox* scene_list_;

	// applet layer
	tlabel* battery_widget_;
	tbutton* start_base_node_widget_;

	// event layer
	bool scroll_logs_to_bottom_;
	tlistbox* event_list_;

	// more layer
/*
	tstack* login_stack_;
	treport* login_type_report_;
	tstack* login_type_stack_;
	int current_login_layer_;
	ttext_box2* pd_username_;
	ttext_box2* pd_password_;

	ttext_box2* ck_username_;
	ttext_box2* ck_cookie_;

	tbutton* login_widget_;
*/
};

} // namespace gui2

#endif

