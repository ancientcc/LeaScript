#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/statusbar.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "game_config.hpp"
#include "filesystem.hpp"
#include "SDL_power.h"
#include "rose_version.hpp"
#include "base_instance.hpp"
#include "bg_task2.hpp"

#include <net/server/rdp_connection.h>



namespace gui2 {

tstatusbar::tstatusbar(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy)
	: rdpd_mgr_(rdpd_mgr)
	, pble_(pble)
	, privacy_(privacy)
	, base_scene_msg_str_(_("Base scene"))
	, interrupted_msg_str_(_("Interrupted"))
	// during display::refresh_report(), because widget->get_rect()'s rect is invaid, will display error.
	// will consider the 'rect' problem when optimizing the scene in the future.
	, use_scene_method_(false)
{
	clear();
}

void tstatusbar::pre_show(twindow& window, tgrid& statusbar_widget, std::map<int, const std::string>* reports)
{
	VALIDATE(window_priv_ == nullptr && statusbar_widget_ == nullptr, null_str);
	window_priv_ = &window;
	statusbar_widget_ = &statusbar_widget;

	privacy_icon_widget_ = find_widget<timage>(&window, "_statusbar_privacy_icon", false, true);
	ble_icon_widget_ = find_widget<timage>(&window, "_statusbar_bluetooth_icon", false, true);
	client_icon_widget_ = find_widget<timage>(&window, "_statusbar_client_icon", false, true);
	client_ip_widget_ = find_widget<tlabel>(&window, "_statusbar_client_ip", false, true);
	if (use_scene_method_ && window.is_scene()) {
		VALIDATE(reports != nullptr, null_str);
		reports->insert(std::make_pair(STATUSBAR_BG_DESC, "_statusbar_bg_desc"));
	} else {
		// VALIDATE(reports == nullptr, null_str);
		bg_desc_widget_ = find_widget<tlabel>(&window, "_statusbar_bg_desc", false, true);
	}
	server_ip_widget_ = find_widget<tlabel>(&window, "_statusbar_server_ip", false, true);

	refresh_statusbar_grid(SDL_GetTicks());
}

void tstatusbar::clear()
{
	window_priv_ = nullptr;

	statusbar_widget_ = nullptr;
	privacy_icon_widget_ = nullptr;
	ble_icon_widget_ = nullptr;
	client_icon_widget_ = nullptr;
	client_ip_widget_ = nullptr;
	bg_desc_widget_ = nullptr;
	server_ip_widget_ = nullptr;
	next_ble_ticks_ = 0;
}

std::string format_bg_desc(const std::string& base_scene_msg_str, const std::string& interrupted_msg_str)
{
	aplt::tbg_task& bg_task = instance->bg_task();
	const char* ai_agent_msg_str = "AI agent";
	char ai_agent[64] = {'\0'};

	const aplt::tapplet::ttask* aiagent_task = bg_task.bg_task2_unsafe().get_aiagent_cfg_task();
	if (aiagent_task != nullptr) {
		SDL_snprintf(ai_agent, sizeof(ai_agent), "[%s: %s]", ai_agent_msg_str, aiagent_task->name.c_str());
	}

	if (!bg_task.is_ing()) {
		const aplt::tbg_task2& bg_task2 = *static_cast<const aplt::tbg_task2*>(&bg_task.bg_task2_unsafe());
		int subtask_state = nposm;
		const std::string scene = bg_task2.curr_base_scene_name(subtask_state);
		if (scene.empty()) {
			return null_str;
		}

		char result[256];
		if (subtask_state == aplt::sts_idle || subtask_state == aplt::sts_preempted) {
			SDL_snprintf(result, sizeof(result), "%s%s: %s[%s]", ai_agent, base_scene_msg_str.c_str(), scene.c_str(),
				aplt::base_subtask_states[subtask_state]);

		} else {
			SDL_snprintf(result, sizeof(result), "%s%s: %s", ai_agent, base_scene_msg_str.c_str(), scene.c_str());
		}
		return result;
	}

	char interrupted[64] = {'\0'};

	const aplt::tbg_task::tbase_bg_task2& sys_task = bg_task.bg_task2();
	const aplt::ttaskpoint* taskpoint = sys_task.get_taskpoint();
	if (taskpoint != nullptr) {
		SDL_snprintf(interrupted, sizeof(interrupted), "[%s: %s]", interrupted_msg_str.c_str(), taskpoint->task_name.c_str());

	}

	char result[256];

	VALIDATE(sys_task.in_task_cpp() || sys_task.aplt_task != nullptr, null_str);
	aplt::ttask_pair single_task_pair;
	if (sys_task.aplt_task != nullptr) {
		single_task_pair = aplt::task_pair_from_task_id(*sys_task.curr_aplt, sys_task.aplt_task->task_id, true);
	}

	if (sys_task.in_task_cpp()) {
		aplt::ttask_pair task_cpp_pair = sys_task.task_cpp_pair();
		if (sys_task.aplt_task != nullptr) {
			SDL_snprintf(result, sizeof(result), "%s%s{%s}-[%s]%s", interrupted, ai_agent, task_cpp_pair.task->name.c_str(), 
				single_task_pair.aplt->name2().c_str(), single_task_pair.task->name.c_str());
		} else {
			SDL_snprintf(result, sizeof(result), "%s%s{%s}", interrupted, ai_agent, task_cpp_pair.task->name.c_str());
		}
	} else {
		SDL_snprintf(result, sizeof(result), "%s%s[%s]%s", interrupted, ai_agent, single_task_pair.aplt->name2().c_str(), single_task_pair.task->name.c_str());
	}

	
	return result;
}

void tstatusbar::refresh_statusbar_grid(uint32_t now)
{
	pble_.timer_handler(now);

	std::string privacy_icon;
	if (privacy_.protect()) {
		privacy_icon = "misc/privacy.png";
	}
	privacy_icon_widget_->set_label(privacy_icon);

	// ble section
	std::string ble_icon = "misc/bluetooth.png";
	if (pble_.is_connected()) {
		ble_icon = "misc/bluetooth-connected.png";
	} else if (pble_.is_advertising()) {
		if (now >= next_ble_ticks_) {
			if (!current_ble_icon_.empty()) {
				ble_icon.clear();
			}
			const int blink_threshold = 100;
			next_ble_ticks_ = now + blink_threshold;
		} else {
			ble_icon = current_ble_icon_;
		}
	}
	ble_icon_widget_->set_label(ble_icon);
	current_ble_icon_ = ble_icon;

	// client section
	std::string client_icon = "misc/no_mobile.png";
	std::stringstream client_ip;
	if (rdpd_mgr_.started()) {
		net::RdpServer& server = rdpd_mgr_.rdp_server();
		if (server.normal_connection_count() != 0) {
			client_icon = "misc/mobile.png";
			net::RdpConnection* connection = server.FindFirstNormalConnection();
			VALIDATE(connection != nullptr, null_str);
			const net::IPEndPoint& peer = connection->peer_ip();
			client_ip << peer.ToStringWithoutPort();
			if (!connection->handshaked()) {
				client_ip << "(" << _("Unhandshakse") << ")";
			}
		}
	}
	client_icon_widget_->set_label(client_icon);
	client_ip_widget_->set_label(client_ip.str());

	std::string msg = format_bg_desc(base_scene_msg_str_, interrupted_msg_str_);
	if (use_scene_method_ && window_priv_->is_scene()) {
		statusbar_refresh_report(STATUSBAR_BG_DESC, msg);

	} else {
		bg_desc_widget_->set_label(msg);
	}

	server_ip_widget_->set_label(rdpd_mgr_.url());
}

twko_state2::twko_state2(int state_at, std::vector<std::string>& existed_state_names)
	: state_at_(state_at)
	, existed_state_names_(existed_state_names)
	, state_name_widget_(nullptr)
	, window_priv_(nullptr)
{
	VALIDATE(state_at >= 0 && state_at < (int)existed_state_names.size(), null_str);
	state_name_ = existed_state_names_[state_at];
}

void twko_state2::pre_show(twindow& window, ttext_box& state_name_widget)
{
	VALIDATE(window_priv_ == nullptr && state_name_widget_ == nullptr, null_str);
	window_priv_ = &window;
	state_name_widget_ = &state_name_widget;

	ttext_box* text_box = state_name_widget_;
	text_box->set_label(state_name_);

	std::string placeholder = i18n::freq_msgstr(i18n::msgid_notempty_and_utf8str);

	text_box->set_placeholder(placeholder);
	text_box->set_maximum_chars(MAX_NORMAL_UTF8_NAME_CHARS);
	text_box->set_did_text_changed(std::bind(&twko_state2::did_state_name_text_changed, this, _1));
}

void twko_state2::did_state_name_text_changed(ttext_box& widget)
{
	state_name_ = widget.label();
}

std::string twko_state2::can_update() const
{
	const std::string& label = state_name_widget_->label();

	std::string err_msg;
	if (!isvalid_normal_utf8_name224(label)) {
		const std::string val = _("State name's characters");
		SDL_Range r{MIN_NORMAL_UTF8_NAME_CHARS, MAX_NORMAL_UTF8_NAME_CHARS};
		err_msg = i18n::freq_msgstr_1str_2int(i18n::msgid_value_range, val, r.min, r.max);
		return err_msg;
	}

	utils::string_map symbols;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = existed_state_names_.begin(); it != existed_state_names_.begin(); ++ it, at ++) {
		if (at == state_at_) {
			continue;
		}
		const std::string& name = *it;
		if (label == name) {
			symbols["name"] = _("State name");
			return vgettext2("The $name already exists. Please use a different name.", symbols);
		}
	}

	return null_str;
}

void twko_state2::do_update()
{
	existed_state_names_[state_at_] = state_name_;
}

} // namespace gui2

