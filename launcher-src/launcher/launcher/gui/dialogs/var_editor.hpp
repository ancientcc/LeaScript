#ifndef GUI_DIALOGS_VAR_EDITOR_HPP
#define GUI_DIALOGS_VAR_EDITOR_HPP

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/rvar_editor.hpp"

class tros_map;

namespace gui2 {

class tvar_editor_slot: public trvar_editor::tslot, public tstatusbar
{
public:
	tvar_editor_slot(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, const tros_map& curmap);

private:
	void rvar_editor_pre_show(twindow& window) override;
	void rvar_did_dlg_close() override;
	void rvar_timer_handler(uint32_t now) override;

	// bool select_freq_val(const twidget& widget, const std::vector<std::string>& freq_vals, bool allow_last, const std::string& curr_val, std::string& result_val, std::string& result_alias) override;
	std::string get_freq_val_alias(const std::string& curr_val) const override;

private:
	const tros_map& curmap_;
};

} // namespace gui2

#endif

