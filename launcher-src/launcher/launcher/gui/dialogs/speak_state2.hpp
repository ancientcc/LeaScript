#ifndef GUI_DIALOGS_SPEAK_STATE2_HPP
#define GUI_DIALOGS_SPEAK_STATE2_HPP

#include "gui/dialogs/statusbar.hpp"
#include "wkoscript.hpp"

namespace gui2 {

class tbutton;
class ttext_box;
class tscroll_text_box;
class ttoggle_button;

class tspeak_state2: public tdialog, public tstatusbar, public twko_state2
{
public:
	tspeak_state2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, aplt::twkoscript::tstate2& state2, std::vector<std::string>& state_names, int sdl_field_small_font_size);

	const aplt::twkoscript::tstate2& get_new_state2() const { return state2_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void click_back(tbutton& widget);

	void set_did_text_changed2(ttext_box& widget, int type, int fid, tscroll_text_box* scroll_text_box = nullptr);
	void did_fid_text_changed(ttext_box& widget, int fid);
	void did_state2_bool_field_changed(ttoggle_button& widget, int fid);

	//
	// task
	//
	void update_task_remark_label() const;

	void app_timer_handler(uint32_t now) override;

private:
	const aplt::twkoscript::tstate2& original_state2_;
	aplt::twkoscript::tstate2 state2_;
	// const std::string state_name_;
	const int sdl_field_small_font_size_;

	tlabel* task_remark_widget_;
};

} // namespace gui2

#endif

