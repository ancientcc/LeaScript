#ifndef GUI_DIALOGS_STATUSBAR_HPP_INCLUDED
#define GUI_DIALOGS_STATUSBAR_HPP_INCLUDED

#include "rdp_server_rose.h"
#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/timer.hpp"
#include "pble2.hpp"

class tprivacy;

namespace gui2 {

class tlabel;
class timage;
class tgrid;
class ttext_box;

class tstatusbar
{
public:
	enum {STATUSBAR_BG_DESC, DERIVED_REP_MIN};

	tstatusbar(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy);

	void pre_show(twindow& window, tgrid& statusbar_widget, std::map<int, const std::string>* reports = nullptr);

	void refresh_statusbar_grid(uint32_t now);

	// if is_scene(), require override it.
	virtual void statusbar_refresh_report(int num, const std::string& label) {}

protected:
	void clear();

protected:
	net::trdpd_manager& rdpd_mgr_;
	tpble2& pble_;
	tprivacy& privacy_;
	const std::string base_scene_msg_str_;
	const std::string interrupted_msg_str_;
	const bool use_scene_method_;

	tgrid* statusbar_widget_;
	timage* privacy_icon_widget_;
	timage* ble_icon_widget_;
	timage* client_icon_widget_;
	tlabel* client_ip_widget_;
	tlabel* bg_desc_widget_;
	tlabel* server_ip_widget_;

	std::string current_ble_icon_;
	uint32_t next_ble_ticks_;

private:
	twindow* window_priv_;
};

class twko_state2
{
public:
	twko_state2(int state_at, std::vector<std::string>& existed_state_names);

	void pre_show(twindow& window, ttext_box& state_name_widget);
	std::string get_new_state_name() const { return state_name_; }

	std::string can_update() const;
	void do_update();

private:
	void did_state_name_text_changed(ttext_box& widget);

protected:
	const int state_at_;
	std::vector<std::string>& existed_state_names_;
	std::string state_name_;

	ttext_box* state_name_widget_;

private:
	twindow* window_priv_;
};

} // namespace gui2

#endif

