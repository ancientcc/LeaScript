#ifndef GUI_DIALOGS_HOME_HPP_INCLUDED
#define GUI_DIALOGS_HOME_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/rstore.hpp"
#include "gui/dialogs/dialog.hpp"
#include "game_config.hpp"
#include "drivers.hpp"
#include "base_driver.hpp"
#include "speech_driver.hpp"
#include "iot_driver.hpp"

namespace aplt {
class thealth;
}

namespace gui2 {

class tbutton;
class ttoggle_button;
class ttext_box;

class thome: public tdialog, public tstatusbar, public aplt::tdesktop
{
public:
	enum tresult {EXPLORER = 1, MAP, APPLET0 = 100};

	thome(gui2::trstore::tslot& slot, net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, 
		std::map<aplt::taplt_key, aplt::tapplet>& applets, tdrivers& drivers, tbase_driver& base_driver, 
		tspeech_driver& speech_driver, tiot_driver& iot_driver, const SDL_OsInfo& os_info, aplt::thealth& health);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void did_bleperipheral_changed(ttoggle_button& widget);
	void did_disable_keyboard_changed(ttoggle_button& widget);
	bool did_verify_sn(const std::string& label);
	void click_update_sn(tbutton& widget);
	void click_update_blepassword(tbutton& widget);
	void click_upgrade_app(tbutton& widget);
	
	void refresh_pinyin_label();
	void click_upgrade_pinyin(tbutton& widget);
	void click_stop_speak(tbutton& widget);

	void write_distribution_cfg(const std::string& path, trsp_header& header, const trsp_latex80bytes& latex80bytes) const;
	bool load_latex_rsp() const;
	void refresh_latex_label();
	void click_upgrade_latex(tbutton& widget);

	void start_advertising();

	void refresh_battery_label();
	void click_start_base_node(tbutton& widget);

	// override rapplets
	std::vector<aplt::tbuildin> rapplets_get_fake_applets() override;
	void rapplets_did_pinyin_upgraded() override;

	void app_timer_handler(uint32_t now) override;
	void app_OnMessage(rtc::Message* msg) override;

private:
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	tdrivers& drivers_;
	tbase_driver& base_driver_;
	tspeech_driver& speech_driver_;
	tiot_driver& iot_driver_;
	const SDL_OsInfo& os_info_;
	aplt::thealth& health_;
	const std::string peripheral_name_;
	const int manufacturer_id_;
	uint32_t start_ticks_;

	tlabel* battery_widget_;
	tbutton* start_base_node_widget_;
	tlabel* status_widget_;

	const std::string battery_max_str_;
	const std::string battery_charge_str_;
	const std::string battery_cutoff_str_;
	const std::string battery_curr_str_;
};

} // namespace gui2

#endif

