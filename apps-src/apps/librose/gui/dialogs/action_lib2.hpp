#ifndef GUI_DIALOGS_ACTION_LIB2_HPP
#define GUI_DIALOGS_ACTION_LIB2_HPP

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tbutton;
class tlistbox;
class ttoggle_panel;

class taction_lib2: public tdialog
{
public:
	enum {scene_view, scene_paste, scene_count};
	taction_lib2(int scene, const std::string& caller_id);

	const std::string& get_curr_sel_id() const { return curr_sel_id_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void click_share(tbutton& widget);
	void reload_action_list(tlistbox& list);
	void did_action_list_row_changed(tlistbox& list, ttoggle_panel& row);

	bool is_ok_active() const;
	void click_ok(tbutton& widget);

private:
	int scene_;
	const std::string caller_id_;

	tlistbox* action_list_;
	tbutton* ok_widget_;

	std::string curr_sel_id_;
};

} // namespace gui2

#endif

