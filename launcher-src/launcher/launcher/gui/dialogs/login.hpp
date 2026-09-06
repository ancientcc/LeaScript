#ifndef GUI_DIALOGS_LOGIN_HPP_INCLUDED
#define GUI_DIALOGS_LOGIN_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/dialogs/rlogin.hpp"


namespace gui2 {

class tlogin: public tdialog, public trcswamp_login
{
public:
	explicit tlogin(net::truser& current_user);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	// void app_did_login_status_changed(bool login) override { did_login_status_changed(login); }
	void app_did_login_status_changed(bool login) override {}
};

} // namespace gui2

#endif

