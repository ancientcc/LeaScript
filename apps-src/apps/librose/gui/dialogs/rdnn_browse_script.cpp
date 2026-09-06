#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/rdnn_browse_script.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"

#include "filesystem.hpp"
using namespace std::placeholders;

namespace gui2 {

REGISTER_DIALOG(rose, rdnn_browse_script)

trdnn_browse_script::trdnn_browse_script(const std::string& title, const std::string& label, const std::string& cancel_label, const std::string& ok_label)
	: title_(title)
	, label_(label)
	, cancel_label_(cancel_label)
	, ok_label_(ok_label)
{
}

void trdnn_browse_script::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");

	find_widget<tbutton>(window_, "cancel", false).set_label(cancel_label_);
	find_widget<tbutton>(window_, "cancel", false).set_icon("misc/back.png");

	find_widget<tlabel>(window_, "title", false).set_label(title_);

	tlabel& label = find_widget<tlabel>(window_, "label", false);
	// label.set_label(label_);

	find_widget<tbutton>(window_, "ok", false).set_visible(twidget::HIDDEN);
	if (!ok_label_.empty()) {
		find_widget<tbutton>(window_, "ok", false).set_label(ok_label_);
		ttoggle_button& toggle = find_widget<ttoggle_button>(window_, "agree", false);
		toggle.set_did_state_changed(std::bind(&trdnn_browse_script::did_agree_changed, this, _1));
	} else {
		find_widget<ttoggle_button>(window_, "agree", false).set_visible(twidget::INVISIBLE);
	}
}

void trdnn_browse_script::post_show()
{
}

void trdnn_browse_script::did_agree_changed(ttoggle_button& widget)
{
	find_widget<tbutton>(window_, "ok", false).set_visible(widget.get_value()? twidget::VISIBLE: twidget::HIDDEN);
}

} // namespace gui2

