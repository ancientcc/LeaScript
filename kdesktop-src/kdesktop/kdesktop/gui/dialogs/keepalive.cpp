#define GETTEXT_DOMAIN "kdesktop-lib"

#include "gui/dialogs/keepalive.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "game_config.hpp"
#include "net.hpp"
#include "rose_version.hpp"



namespace gui2 {

tkeepalive::tkeepalive()
	: status_widget_(nullptr)
	, relogin_interval_(10)
	, next_keepalive_ticks_(0)
	, next_relogin_ticks_(0)
	, status_only_elapse_(false)
	, auto_relogin_(true)
	, unget_events_(0)
	, window_priv_(nullptr)
{
	if (!current_user.valid()) {
		next_relogin_ticks_ = SDL_GetTicks() + relogin_interval_ * 1000;
	}
}

void tkeepalive::pre_show(twindow& window, tlabel& status_widget)
{
	VALIDATE(!window_priv_ && !status_widget_, null_str);
	window_priv_ = &window;
	status_widget_ = &status_widget;

	time_timer_.reset(200, *window_priv_, std::bind(&tkeepalive::time_timer_handler, this, SDL_Rect{nposm, nposm, nposm, nposm}));
	time_timer_handler(SDL_Rect{nposm, nposm, nposm, nposm});
}

void tkeepalive::refresh_remark_label(int unget_events)
{
	std::stringstream ss;
	utils::string_map symbols;

	uint32_t now = SDL_GetTicks();

	if (unget_events != 0) {
		symbols["count"] = str_cast(unget_events);
		ss << vgettext2("At least $count more.", symbols);
	}
	if (current_user.valid()) {
		VALIDATE(next_keepalive_ticks_ > 0, null_str);
		int elapse = next_keepalive_ticks_ >= now? next_keepalive_ticks_ - now: 0;
		symbols["elapse"] = utils::format_elapse_hms(elapse / 1000);
		ss << vgettext2("Will keepalive automatically after $elapse", symbols);

	} else {
		VALIDATE(next_relogin_ticks_ > 0, null_str);
		VALIDATE(unget_events == 0, null_str);
		int elapse = next_relogin_ticks_ >= now? next_relogin_ticks_ - now: 0;
		symbols["elapse"] = utils::format_elapse_hms(elapse / 1000);
		if (auto_relogin_) {
			ss << vgettext2("Device is not logged on to the server, will login automatically after $elapse", symbols);
		} else {
			ss << _("Device is not logged on to the server");
		}
	}
/*
	{
		symbols["count"] = str_cast(faceprint::list_cs_vsize);
		ss << "  |" << vgettext2("Local facestore has $count faces", symbols);
	}

	if (faceprint::facestore_ts) {
		symbols["time"] = format_time_ymdhms(faceprint::facestore_ts / 1000);
		ss << "  |" << vgettext2("Facestore time: $time", symbols);
	}
*/
	status_widget_->set_label(ss.str());
}

void tkeepalive::xmit_keepalive(const SDL_Rect& rect)
{
	if (current_user.valid()) {
		VALIDATE(next_keepalive_ticks_ >= 0, null_str);
		next_keepalive_ticks_ = SDL_GetTicks();

	} else {
		VALIDATE(next_relogin_ticks_ > 0, null_str);
		next_relogin_ticks_ = SDL_GetTicks();
	}
	time_timer_handler(rect);
}

void tkeepalive::did_login_status_changed(bool login)
{
	unget_events_ = 0;
	if (login) {
		VALIDATE(current_user.valid(), null_str);
		next_relogin_ticks_ = 0;
		next_keepalive_ticks_ = SDL_GetTicks() + current_user.keepalive * 1000;

	} else {
		VALIDATE(!current_user.valid(), null_str);
		next_keepalive_ticks_ = 0;
		next_relogin_ticks_ = SDL_GetTicks() + relogin_interval_ * 1000;
	}
/*
	VALIDATE(!auto_relogin_, null_str);

	VALIDATE(current_user.valid(), null_str);
	next_relogin_ticks_ = 0;
	next_keepalive_ticks_ = SDL_GetTicks() + current_user.keepalive * 1000;
*/
}

void tkeepalive::time_timer_handler(const SDL_Rect& rect)
{
	if (tprogress_::top_instance) {
		// avoie nestcall run_with_progress. you mamybe in debug.
		return;
	}

	if (app_disable_slice()) {
		// SDL_Log("%u tkeepalive::time_timer_handler, disable slice", SDL_GetTicks());
		return;
	}

	bool use_speical_rect = rect.x != nposm && rect.y != nposm && rect.w != nposm && rect.h != nposm;
	const int hidden_ms = use_speical_rect? 0: 2000;

	const uint32_t now = SDL_GetTicks();
	if (current_user.valid()) {
		if (next_keepalive_ticks_ && now >= next_keepalive_ticks_) {
			bool ret = false;
			int64_t server_event_time;
			{
				gui2::tprogress_default_slot slot(std::bind(&net::cswamp_keepalive, _1, current_user.sessionid, 
					true, std::ref(server_event_time)));
				ret = gui2::run_with_progress_quiet(slot, null_str, _("Keepalive"), hidden_ms, rect);
			}

			if (ret) {
				const int64_t local_event_time = preferences::event_time();
				if (server_event_time > local_event_time) {
					std::set<trobot_event> events;
					int count = nposm;
					gui2::tprogress_default_slot slot(std::bind(&net::cswamp_getevent, _1, current_user.sessionid, 
						local_event_time, std::ref(events), std::ref(count), false));
					ret = gui2::run_with_progress(slot, null_str, _("Get event"), 1);
					if (ret) {
						if (!events.empty()) {
							app_did_new_events(events);
							int64_t largest_event_time = events.begin()->ts;
							VALIDATE(largest_event_time > local_event_time, null_str);
							if (largest_event_time > local_event_time) {
								preferences::set_event_time(largest_event_time);
							} else {
								// Normally, it shouldn't be here
								VALIDATE(game_config::os == os_windows, null_str);
							}
						}
						unget_events_ = count - events.size();
					}
				}

				next_keepalive_ticks_ = SDL_GetTicks() + current_user.keepalive * 1000;

			} else {
				current_user.clear();
				next_keepalive_ticks_ = 0;

				next_relogin_ticks_ = SDL_GetTicks() + relogin_interval_ * 1000;

				app_did_network_changed(false);
			}

		} else if (!next_keepalive_ticks_) {
			next_keepalive_ticks_ = SDL_GetTicks() + current_user.keepalive * 1000;
		}
	} else if (auto_relogin_) {
		if (next_relogin_ticks_ && now >= next_relogin_ticks_) {
			std::string username = preferences::login_username();
			std::string cookie = preferences::login_pwcookie();
			if (!username.empty() && !cookie.empty()) {
				net::cswamp_login2(current_user, net::cswamp_login_type_cookie, username, cookie, nposm, true);
			}

			if (current_user.valid()) {
				next_relogin_ticks_ = 0;
				next_keepalive_ticks_ = now + current_user.keepalive * 1000;

				app_did_network_changed(true);
			} else {
				next_relogin_ticks_ = now + relogin_interval_ * 1000;
			}
		}
	}
	if (current_user.valid()) {
		VALIDATE(next_keepalive_ticks_ && !next_relogin_ticks_, null_str);
	} else {
		VALIDATE(!next_keepalive_ticks_, null_str);
	}
	if (app_can_refersh_remark_label()) {
		refresh_remark_label(unget_events_);
	}

	app_did_timer_handler();
}

} // namespace gui2

