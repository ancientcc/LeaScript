#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/edit_position.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "game_config.hpp"

using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(launcher, edit_position)

tedit_position::tedit_position(bool edit, const tmap_position* position)
	: edit_(edit)
	, position_(position)
	, def_angle_(DEG2RAD(0))
	, is_charge_position_(false)
	, original_restrict_angle_(false)
	, name_txt_(nullptr)
	, restrict_angle_widget_(nullptr)
	, angle_widget_(nullptr)
	, ok_(nullptr)
	, new_theta_(0)
{
	if (!edit_) {
		VALIDATE(position_ == nullptr, null_str);

	} else {
		VALIDATE(position_ != nullptr, null_str);
		is_charge_position_ = position_->uuid == charge_pos_uuid;
	}
}

void tedit_position::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	std::string title;
	std::string def_name;
	int def_angle = def_angle_;
	bool restrict_angle = false;
	if (!edit_) {
		title = _("New position");
		def_name = _("Untitle");

	} else {
		title = _("Edit position");
		def_name = position_->name;

		if (!is_charge_position_) {
			restrict_angle = !is_float_nposm(position_->theta);
			def_angle = restrict_angle? round(RAD2DEG(position_->theta)): def_angle_;

		} else {
			restrict_angle = true;
			if (!is_float_nposm(position_->theta)) {
				def_angle = round(RAD2DEG(position_->theta));
			} else {
				// charge_position must be angle-restriced.
				// I don't know who modified charge_position's yaw, corrected it back
				def_angle = def_angle_;
			}
		}
	}
	original_restrict_angle_ = restrict_angle;

	find_widget<tlabel>(window_, "title", false).set_label(title);

	ok_ = find_widget<tbutton>(window_, "ok", false, true);
	// ok_->set_label(param_.ok);

	name_txt_ = new ttext_box2(*window_, *find_widget<twidget>(window_, "txt", false, true));
	name_txt_->text_box()->set_border("textbox");
	window_->keyboard_capture(name_txt_->text_box());
	// user_widget->text_box().goto_end_of_data();  now not support, should fixed in future.

	name_txt_->text_box()->set_placeholder(null_str);
	name_txt_->text_box()->set_maximum_chars(20);

	name_txt_->set_did_text_changed(std::bind(&tedit_position::did_text_changed, this, _1));

	ttoggle_button* toggle = find_widget<ttoggle_button>(window_, "restrict_angle", false, true);
	toggle->set_value(restrict_angle);
	toggle->set_did_state_changed(std::bind(&tedit_position::did_restrict_angle_state_changed, this, _1));
	restrict_angle_widget_ = toggle;

	ttext_box* text_box = find_widget<ttext_box>(window_, "angle", false, true);
	text_box->set_placeholder("[0, 360)");
	text_box->set_maximum_chars(3);
	text_box->set_did_text_changed(std::bind(&tedit_position::did_text_changed, this, _1));
	if (!restrict_angle) {
		text_box->set_visible(twidget::INVISIBLE);
	}
	angle_widget_ = text_box;

	if (is_charge_position_) {
		name_txt_->set_active(false);
		restrict_angle_widget_->set_active(false);
	}

	name_txt_->text_box()->set_label(def_name);
	text_box->set_label(str_cast(def_angle));
}

void tedit_position::post_show()
{
	new_name_ = name_txt_->text_box()->label();

	const int angle = utils::to_int(angle_widget_->label());
	if (restrict_angle_widget_->get_value()) {
		new_theta_ = DEG2RAD(angle);
	} else {
		VALIDATE(angle == def_angle_, null_str);
		new_theta_ = float_nposm;
	}
}

bool tedit_position::verify_name(const std::string& label, bool& equal) const
{
	equal = false;
	if (label.empty()) {
		return false;
	}
	if (label.size() >= RSP_MAP_MAXPOSNAMEBYTES) {
		return false;
	}
	if (edit_) {
		equal = label == position_->name;
	}

	return true;
}

bool tedit_position::verify_angle(const std::string& label, bool& equal) const
{
	equal = false;
	if (label.empty()) {
		return false;
	}
	if (!utils::isinteger(label)) {
		return false;
	}

	int angle = utils::to_int(label);
	if (angle < 0 || angle >= 360) {
		return false;
	}

	if (edit_) {
		equal = angle == round(RAD2DEG(position_->theta));
	}
	return true;
}

void tedit_position::did_text_changed(ttext_box& widget)
{
	bool name_equal;
	bool angle_equal;
	bool valid = verify_name(name_txt_->text_box()->label(), name_equal);
	if (valid) {
		valid = verify_angle(angle_widget_->label(), angle_equal);
	}
	ok_->set_active(valid && (!name_equal || !angle_equal || original_restrict_angle_ != restrict_angle_widget_->get_value()));
}

void tedit_position::did_restrict_angle_state_changed(ttoggle_button& widget)
{
	VALIDATE(!is_charge_position_, null_str);

	if (widget.get_value()) {
		angle_widget_->set_visible(twidget::VISIBLE);

	} else {
		angle_widget_->set_visible(twidget::INVISIBLE);

		std::string new_label = str_cast(def_angle_);
		if (new_label != angle_widget_->label()) {
			// below set_label will call 'did_text_changed'
			angle_widget_->set_label(str_cast(def_angle_));
		} else {
			did_text_changed(*angle_widget_);
		}
	}
}

} // namespace gui2

