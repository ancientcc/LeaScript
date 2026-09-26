#ifndef GUI_DIALOGS_INSERT_SCENE_HPP
#define GUI_DIALOGS_INSERT_SCENE_HPP

#include "gui/dialogs/dialog.hpp"
#include "aplt2.hpp"
#include "gui/dialogs/rvar_editor.hpp"

namespace aplt {
class tcfg_cpp_api_core;
}

namespace gui2 {

class tbutton;
class tlabel;

class tinsert_scene: public tdialog
{
public:
	tinsert_scene(trvar_editor::tslot& var_editor_slot, const std::map<aplt::taplt_key, aplt::tapplet>& applets, const aplt::tcfg_cpp_api_core& cfg_cpp_api);

	const aplt::tbase_scene& get_scene() const { return scene_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void click_scene_task(tbutton& widget);
	void click_edit_scene_input_vars(tbutton& widget);
	void did_input_vars_changed();

private:
	trvar_editor::tslot& var_editor_slot_;
	const std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	const aplt::tcfg_cpp_api_core& cfg_cpp_api_;
	const std::string workout_task_id_;
	aplt::tbase_scene scene_;

	tbutton* task_widget_;
	tlabel* input_vars_widget_;
	tlabel* name_widget_;
	tbutton* ok_widget_;
};

} // namespace gui2

#endif

