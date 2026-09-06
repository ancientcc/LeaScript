#ifndef GUI_DIALOGS_WKO_SETTINGS_HPP
#define GUI_DIALOGS_WKO_SETTINGS_HPP

#include "gui/dialogs/dialog.hpp"
#include "game_config.hpp"

namespace gui2 {

class ttoggle_button;
class tbutton;
class treport;
class tstack;
class tgrid;
class tslider;

class twko_settings: public tdialog
{
public:
	enum {VLOG_LAYER, MISC_LAYER};
	explicit twko_settings(tvlog_cfg& vlog_cfg);

	const tvlog_cfg& get_new_vlog_cfg() const { return vlog_cfg_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void pre_vlog(tgrid& grid);
	void pre_misc(tgrid& grid);
	void did_main_report_changed(treport& report, ttoggle_button& row);

	void did_hide_changed(ttoggle_button& widget, int hid);
	enum {toggleid_video, toggleid_poses, toggleid_rep_sfx, toggleid_count};
	void did_toggle_id_on_top_changed(ttoggle_button& widget, int toggle_id);

	void update_xxx_captuion_msg_label(int hid);
	void click_xxx_caption_msg(tbutton& widget, int hid);
	bool verify_xxx_caption_msg(const std::string& label, int hid, const std::string& initial, int max_chars) const;

	void sound_changed(tslider& widget, int value);

private:
	const tvlog_cfg& original_vlog_cfg_;
	tvlog_cfg vlog_cfg_;
	int current_layer_;

	treport* main_report_;
	tstack* main_stack_;
	tbutton* start_caption_msg_widget_;
	tbutton* finish_caption_msg_widget_;
};

} // namespace gui2

#endif

