#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/var_editor.hpp"

#include "gui/widgets/panel.hpp"
#include "gui/widgets/window.hpp"

namespace gui2 {

tvar_editor_slot::tvar_editor_slot(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const tros_map& curmap)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, curmap_(curmap)
{}

void tvar_editor_slot::rvar_editor_pre_show(twindow& window)
{
	tstatusbar::pre_show(window, find_widget<tpanel>(&window, "statusbar", false).grid());
}

void tvar_editor_slot::rvar_did_dlg_close()
{
	tstatusbar::clear();
}

void tvar_editor_slot::rvar_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
}
/*
extern bool select_freq_val(const tros_map& curmap, const twidget& widget, const std::vector<std::string>& freq_vals, bool allow_last, const std::string& curr_val, std::string& result_val, std::string& result_alias);

bool tvar_editor_slot::select_freq_val(const twidget& widget, const std::vector<std::string>& freq_vals, bool allow_last, const std::string& curr_val, std::string& result_val, std::string& result_alias)
{
	return gui2::select_freq_val(curmap_, widget, freq_vals, allow_last, curr_val, result_val, result_alias);
}
*/
extern std::string get_freq_val_alias(const tros_map& curmap, const std::string& curr_val);

std::string tvar_editor_slot::get_freq_val_alias(const std::string& curr_val) const
{
	return gui2::get_freq_val_alias(curmap_, curr_val);
}

} // namespace gui2

