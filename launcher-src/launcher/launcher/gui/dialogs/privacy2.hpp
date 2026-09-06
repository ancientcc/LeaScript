#ifndef GUI_DIALOGS_PRIVACY2_HPP_INCLUDED
#define GUI_DIALOGS_PRIVACY2_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include <rose_ros/utils.hpp>

class tprivacy;

namespace gui2 {

class tlistbox;
class tbutton;
class ttoggle_panel;

class tprivacy2: public tdialog, public tstatusbar
{
public:
	explicit tprivacy2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void did_next_protect_changed(ttoggle_button& widget);

	void app_timer_handler(uint32_t now) override;

private:
};

} // namespace gui2

#endif

