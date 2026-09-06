#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/login.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"

namespace gui2 {

REGISTER_DIALOG(launcher, login)

tlogin::tlogin(net::truser& current_user)
	: trcswamp_login(current_user)
{
}

void tlogin::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	find_widget<tlabel>(window_, "title", false).set_label(_("CSwamp account"));

	tgrid& grid = *find_widget<tgrid>(window_, "cswamp_login_grid", false, true);
	trcswamp_login::pre_show(*window_, grid);
}

void tlogin::post_show()
{
}

} // namespace gui2

