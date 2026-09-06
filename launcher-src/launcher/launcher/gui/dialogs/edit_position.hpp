#ifndef GUI_DIALOGS_EDIT_POSITION_HPP_INCLUDED
#define GUI_DIALOGS_EDIT_POSITION_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/text_box2.hpp"
#include "rose_filesystem.hpp"

namespace gui2 {

class ttext_box;
class tbutton;
class ttoggle_button;

class tedit_position: public tdialog
{
public:
	explicit tedit_position(bool edit, const tmap_position* position);

	std::string get_name() const { return new_name_; }
	double get_theta() const { return new_theta_; }

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void did_text_changed(ttext_box& widget);
	bool verify_name(const std::string& label, bool& equal) const;
	bool verify_angle(const std::string& label, bool& equal) const;
	void did_restrict_angle_state_changed(ttoggle_button& widget);

private:
	const bool edit_;
	const tmap_position* position_;
	const double def_angle_;
	bool is_charge_position_;
	bool original_restrict_angle_;
	ttext_box2* name_txt_;
	ttoggle_button* restrict_angle_widget_;
	ttext_box* angle_widget_;
	tbutton* ok_;

	std::string new_name_;
	double new_theta_;
};

} // namespace gui2

#endif

