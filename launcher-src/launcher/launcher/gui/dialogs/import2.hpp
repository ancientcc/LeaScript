#ifndef GUI_DIALOGS_IMPORT2_HPP_INCLUDED
#define GUI_DIALOGS_IMPORT2_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"

namespace aplt {

class tcfg_cpp_api;

}

namespace gui2 {

class tbutton;
class tlistbox;
class ttoggle_panel;

class timport2: public tdialog, public tstatusbar
{
public:
	timport2(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, aplt::tcfg_cpp_api& cfg_cpp_api);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	std::string load_product_def_cfg();
	void reload_product_list();
	void did_product_changed(tlistbox& list, ttoggle_panel& row);
	void click_import(tbutton& widget);

	void app_timer_handler(uint32_t now) override;

private:
	aplt::tcfg_cpp_api& cfg_cpp_api_;

	struct tproduct
	{
		tproduct(const std::string& dir, const config& cfg)
		{
			name = cfg["name"].str();
			klink_cfg = dir + "/" + cfg["klink_cfg"].str();
			task_cpp_cfg = dir + "/" + cfg["task_cpp_cfg"].str();
			speech_cfg = dir + "/" + cfg["speech_cfg"].str();
		}

		std::string name;
		std::string klink_cfg;
		std::string task_cpp_cfg;
		std::string speech_cfg;
	};

	std::vector<tproduct> products_;

	tlistbox* product_list_;
	tbutton* import_widget_;
};

} // namespace gui2

#endif

