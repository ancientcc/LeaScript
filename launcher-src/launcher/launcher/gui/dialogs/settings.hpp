#ifndef GUI_DIALOGS_SETTINGS_HPP_INCLUDED
#define GUI_DIALOGS_SETTINGS_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
#include "aplt.hpp"
#include "base_driver.hpp"
#include "moveit_driver.hpp"
#include "laser_driver.hpp"
#include "dcamera_driver.hpp"
#include "iot_driver.hpp"
#include "speech_driver.hpp"
#include "ai_driver.hpp"


class tros_map;
class tinstance_slot;
class tros_instance;
class tprivacy;

namespace gui2 {

class tbutton;
class tlistbox;
class ttoggle_panel;
class ttoggle_button;

class tsettings: public tdialog, public tstatusbar
{
public:
	explicit tsettings(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tros_instance& ros_instance, tros_map& curmap, 
		std::map<aplt::taplt_key, aplt::tapplet>& applets, tinstance_slot& slot, aplt::tcfg_cpp_api& cfg_cpp_api, tbase_driver& base_driver, tmoveit_driver& moveit_driver, 
		tlaser_driver& laser_driver, tdcamera_driver& dcamera_driver, tiot_driver& iot_driver, tspeech_driver& speech_driver, tai_driver& ai_driver,
		const trobot_imu& robot_imu, const std::string& saves_map_dir);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void set_login_label();
	void click_login(tbutton& widget);
	void set_privacy_desc_label();
	void click_privacy(tbutton& widget);
	void did_privacy_protection(ttoggle_button& widget);
	void click_driver(tbutton& widget, int type);
	void click_start_imu(tbutton& widget);
	void click_charge_task(tbutton& widget);
	bool verify_charge_width(const std::string& label) const;
	void click_charge_width(tbutton& widget);
	void click_map(tbutton& widget);
	void set_map_position_label();
	void click_map_position(tbutton& widget);
	void click_import_klink_cfgs(tbutton& widget);

	void set_status_label(const std::string& msg);
	void set_rpy_label();

	void app_timer_handler(uint32_t now) override;

public:
	tros_instance& ros_instance_;
	tros_map& curmap_;
	std::map<aplt::taplt_key, aplt::tapplet>& applets_;
	tinstance_slot& slot_;
	aplt::tcfg_cpp_api& cfg_cpp_api_;
	tbase_driver& base_driver_;
	const std::string original_base_aplt_id_;
	tmoveit_driver& moveit_driver_;
	const std::string original_moveit_aplt_id_;
	tlaser_driver& laser_driver_;
	const std::string original_laser_aplt_id_;
	tdcamera_driver& dcamera_driver_;
	const std::string original_dcamera_aplt_id_;
	tiot_driver& iot_driver_;
	const std::string original_iot_aplt_id_;
	tspeech_driver& speech_driver_;
	const std::string original_speech_aplt_id_;
	tai_driver& ai_driver_;
	const std::string original_aiagent_aplt_id_;
	const trobot_imu& robot_imu_;
	const std::string saves_map_dir_;

	aplt::tdisable_new_klink_task_lock disable_new_aplt_lock_;

	std::string last_im948_serial_;

	ttoggle_button* privacy_protection_widget_;
	tlabel* status_widget_;
	tlabel* rpy_widget_;
	tbutton* start_imu_widget_;
};

} // namespace gui2

#endif

