#ifndef GUI_DIALOGS_KEEPALIVE_HPP_INCLUDED
#define GUI_DIALOGS_KEEPALIVE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/timer.hpp"
#include "game_config.hpp"

namespace gui2 {

class tlabel;
class tgrid;

class tkeepalive
{
public:
	enum {MSG_UPDATE_APP = tdialog::POST_MSG_MIN_APP, POST_MSG_BASECLASS};

	explicit tkeepalive();

	void pre_show(twindow& window, tlabel& status_widget);

protected:
	void xmit_keepalive(const SDL_Rect& rect);
	void did_login_status_changed(bool login);

private:
	void refresh_remark_label(int unget_events);
	void time_timer_handler(const SDL_Rect& rect);

	virtual void app_did_network_changed(bool connected) = 0;
	virtual void app_did_new_events(const std::set<trobot_event>& events) = 0;
	virtual bool app_can_refersh_remark_label() const { return true; }
	virtual bool app_disable_slice() const { return false; }
	virtual void app_did_timer_handler() {}

protected:
	tlabel* status_widget_;
	ttimer time_timer_;
	const int relogin_interval_;
	uint32_t next_keepalive_ticks_;
	uint32_t next_relogin_ticks_;
	bool status_only_elapse_;
	bool auto_relogin_;
	int unget_events_;

private:
	twindow* window_priv_;
};

} // namespace gui2

#endif

