#ifndef GUI_DIALOGS_MOVEIT_HPP_INCLUDED
#define GUI_DIALOGS_MOVEIT_HPP_INCLUDED

#include "gui/dialogs/statusbar.hpp"
#include "gui/dialogs/dialog.hpp"
#include "ros_instance.hpp"

namespace gui2 {

class tbutton;
class tslider;
class tlistbox;
class ttoggle_button;

class tmoveit: public tdialog, public tstatusbar, public tros_instance::tslot
{
public:
	explicit tmoveit(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tmoveit_driver& moveit_driver, tdrivers& drivers, tros_instance& ros_instance, trobot_imu& robot_imu);
	~tmoveit();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void did_include_first_changed(ttoggle_button& widget);
	enum {joint_a, joint_b, joint_c, joint_d};
	void did_slider_value_changed(tlistbox& list, tslider& widget, int value, int at);

	void reload_joint_list(tlistbox& list);
	void copy_to_this_positions();
	void set_this_position(int at, double val);

	void click_group(tbutton& widget);
	bool did_ik_range(gui2::tprogress_& progress, double step, double ik_bound, const std::string& result_png);
	void click_ik_range(tbutton& widget);
	void click_generate_moveit_rsp(tbutton& widget);
	void click_ikfast(tbutton& widget);
	void click_state(tbutton& widget);

	void set_curr_group(const aplt::tjoint_group_model& group);
	void reload_fk_joints();
	void did_joints_value_changed(tlistbox& list, bool wait_for_joint_state_publisher);
	void set_joint_value_label(tlabel& widget, double value);
	void set_status_label(const std::string& msg);
	void set_rpy_label();
	void calculate_fk(const std::vector<double>& angles);

	void app_timer_handler(uint32_t now) override;

public:
	const int SLIDER_TIMES;
	const int SLIDER_GTE0_OFFSET;

private:
	tmoveit_driver& moveit_driver_;
	tdrivers& drivers_;
	tros_instance& ros_instance_;
	trobot_imu& robot_imu_;
	const aplt::trobot_model& robot_model_;
	aplt::tvariable_positions& variable_positions_;
	ros::ttf_calculator& tf_calculator_;
	const int max_display_joints_;
	const std::string release_ik_mid_dat_;

	const aplt::tjoint_group_model* curr_group_;
	double* this_positions_;
	std::vector<ros::tfk_joint> fk_joints_;

	class tdisable_reload_fk_joints_lock
	{
	public:
		tdisable_reload_fk_joints_lock(tmoveit& moveit)
			: moveit_(moveit)
		{
			VALIDATE(!moveit_.disable_reload_fk_joints_, null_str);
			moveit_.disable_reload_fk_joints_ = true;
		}

		~tdisable_reload_fk_joints_lock()
		{
			moveit_.disable_reload_fk_joints_ = false;
		}

	private:
		tmoveit& moveit_;
	};
	bool disable_reload_fk_joints_;

	class tdisable_JointState_lock
	{
	public:
		tdisable_JointState_lock(tmoveit& moveit)
			: moveit_(moveit)
		{
			VALIDATE(!moveit_.disable_JointState_, null_str);
			moveit_.disable_JointState_ = true;
		}

		~tdisable_JointState_lock()
		{
			moveit_.disable_JointState_ = false;
		}

	private:
		tmoveit& moveit_;
	};
	bool disable_JointState_;

	tlabel* status_widget_;
	tlabel* rpy_widget_;
	ttoggle_button* include_first_widget_;
	tlistbox* joint_list_;

};

} // namespace gui2

#endif

