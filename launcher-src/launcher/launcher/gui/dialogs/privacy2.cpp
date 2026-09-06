#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/privacy2.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "game_config.hpp"
#include "base_driver_core.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(launcher, privacy2)

tprivacy2::tprivacy2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy)
	: tstatusbar(rdpd_mgr, pble, privacy)
{
	set_timer_interval(1000);
}

void tprivacy2::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());
	
	// find_widget<tlabel>(window_, "title", false).set_label(aplt_.name);

	find_widget<tlabel>(window_, "remark", false).set_label(_("privacy^settings remark"));

	ttoggle_button* toggle = find_widget<ttoggle_button>(window_, "next_protect", false, true);
	toggle->set_did_state_changed(std::bind(&tprivacy2::did_next_protect_changed, this, _1));
	toggle->set_value(privacy_.auto_protect_threshold_s() != nposm);

	toggle->set_label(privacy_.auto_protect_msgstr(PRIVACY_AUTO_PROTECT_THRESHOLD_S));
}

void tprivacy2::post_show()
{
	if (privacy_.auto_protect_threshold_s() == nposm) {
		if (privacy_.next_protect_ticks() != 0) {
			privacy_.reset_next_protect_ticks();
		}
	}
}

void tprivacy2::did_next_protect_changed(ttoggle_button& widget)
{
	if (widget.get_value()) {
		// disable --> enable
		if (privacy_.auto_protect_threshold_s() == nposm) {
			privacy_.set_auto_protect_threshold_s(PRIVACY_AUTO_PROTECT_THRESHOLD_S);
		}

	} else {
		// enable --> disable
		if (privacy_.auto_protect_threshold_s() != nposm) {
			privacy_.set_auto_protect_threshold_s(nposm);
		}
	}

	// set_privacy_desc_label();
	preferences::set_privacy_auto_protect_threshold(privacy_.auto_protect_threshold_s());
}

void tprivacy2::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}

} // namespace gui2

