#define GETTEXT_DOMAIN "launcher-lib"

#include "gui/dialogs/moveit.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/menu.hpp"
#include "gui/dialogs/message.hpp"

#include <tf2/utils.h>

#include "gettext.hpp"
#include "font.hpp"

using namespace std::placeholders;

bool did_write_rsp_moveit2(tfile& file, const std::string& bundleid, const version_info& rose_version, 
	const trsp_clawgap* clawgaps, int clawgap_count, const trsp_ikmid* ikmids, int ikmid_count)
{
	VALIDATE(clawgaps != nullptr, null_str);
	VALIDATE(clawgap_count > 0, null_str);
	if (ikmids != nullptr) {
		VALIDATE(ikmid_count > 0, null_str);
	} else {
		VALIDATE(ikmid_count == 0, null_str);
	}
	const int fclawgap_bytes = sizeof(trsp_clawgap) * clawgap_count;
	const int ikmid_bytes = sizeof(trsp_ikmid) * ikmid_count;

	const int one_block = 1024 * 1024;

	trsp_header header;
	memset(&header, 0, sizeof(trsp_header));
	header.fourcc = SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_moveit));
	header.version = SDL_FOURCC(0, 0, 0, RSP_MOVEIT_VER);
	header.build_date = (uint32_t)time(nullptr);

	strcpy(header.bundleid, bundleid.c_str());
	header.rose_version = SDL_FOURCC(0, rose_version.major_version(), rose_version.minor_version(), rose_version.revision_level());
	header.zip_size = sizeof(trsp_moveitheader2) + fclawgap_bytes + ikmid_bytes;
	posix_fwrite(file.fp, &header, sizeof(header));

	double claw_height = 0.017; // 1.7cm
	// xyz="-0.027 -0.0222 0.0432"
	// SDL_DPoint3 joint5_xyz{-0.027, -0.0222, 0.0432};
	// rotate RPY(0, 90, 0) from joint5.xyz
	const SDL_DPoint3 PRP_offset{0.0432, 0, 0.027};
	const SDL_DPoint3 PRP_2offset{0.02, 0, 0.003};

	trsp_moveitheader2 moveit_header;
	memset(&moveit_header, 0, sizeof(moveit_header));
	moveit_header.fourcc = SDL_FOURCC('M', 'I', 'T', RSP_MOVEIT_VER);
	moveit_header.clawgaps = clawgap_count;
	moveit_header.ikmids = ikmid_count;
	moveit_header.claw_height = claw_height;
	moveit_header.PRP_offset = PRP_offset;
	moveit_header.PRP_2offset = PRP_2offset;
	posix_fwrite(file.fp, &moveit_header, sizeof(moveit_header));

	int pos = 0;
	while (pos < fclawgap_bytes) {
		int bytes = one_block;
		if (pos + bytes > fclawgap_bytes) {
			bytes = fclawgap_bytes - pos;
		}
		posix_fwrite(file.fp, (uint8_t*)clawgaps + pos, bytes);

		pos += bytes;
	}

	pos = 0;
	while (pos < ikmid_bytes) {
		int bytes = one_block;
		if (pos + bytes > ikmid_bytes) {
			bytes = ikmid_bytes - pos;
		}
		posix_fwrite(file.fp, (uint8_t*)ikmids + pos, bytes);

		pos += bytes;
	}
	return true;
}

bool load_moveit_from_rsp2(const std::string& path_to_rsp, trsp_moveit2& moveit)
{
	moveit.clear();
	moveit.rspfile = path_to_rsp;

    tsha1reader src(path_to_rsp, false, NULL);
	VALIDATE(src.valid(), null_str);
	int payload_size = src.verify_sha1();
	if (payload_size < sizeof(trsp_header) + sizeof(trsp_moveitheader2)) {
		return false;
	}

	trsp_header header;
	memset(&header, 0, sizeof(header));
	posix_fread(src.fp, &header, sizeof(trsp_header));
	if (header.fourcc != SDL_FOURCC('R', 'S', 'P', posix_mku8(1, zipt_moveit))) {
		return false;
	}
	trsp_moveitheader2 moveitheader;
	memset(&moveitheader, 0, sizeof(moveitheader));
	posix_fread(src.fp, &moveitheader, sizeof(moveitheader));
	if (moveitheader.fourcc != SDL_FOURCC('M', 'I', 'T', RSP_MOVEIT_VER)) {
		return false;
	}
	if (moveitheader.clawgaps <= 0 || moveitheader.ikmids < 0) {
		return false;
	}
	if (header.zip_size != sizeof(moveitheader) + sizeof(trsp_clawgap) * moveitheader.clawgaps + sizeof(trsp_ikmid) * moveitheader.ikmids) {
		const bool always_true = false;
		if (!always_true) {
			return false;

		} else {
			// if you change trsp_moveitheader2, or trsp_clawgap, or trsp_ikmid
			moveit.items = (trsp_clawgap*)malloc(sizeof(trsp_clawgap) * moveitheader.clawgaps);
			moveit.item_count = 1;
			return true;
		}
	}

	if (moveitheader.reserve0 != 0 || moveitheader.reserve1 != 0 || moveitheader.reserve2 != 0) {
		return false;
	}
	moveit.header = moveitheader;

	moveit.item_count = moveitheader.clawgaps;

	moveit.items = (trsp_clawgap*)malloc(sizeof(trsp_clawgap) * moveitheader.clawgaps);
	posix_fread(src.fp, moveit.items, sizeof(trsp_clawgap) * moveitheader.clawgaps);

	if (moveitheader.ikmids > 0) {
		moveit.ikmid_count = moveitheader.ikmids;

		moveit.ikmids = (trsp_ikmid*)malloc(sizeof(trsp_ikmid) * moveitheader.ikmids);
		posix_fread(src.fp, moveit.ikmids, sizeof(trsp_ikmid) * moveitheader.ikmids);
	}

	if (game_config::os == os_windows && moveitheader.ikmids > 0) {
		std::vector<trsp_ikmid> ikmids;
		for (int at = 0; at < moveit.ikmid_count; at ++) {
			ikmids.push_back(moveit.ikmids[at]);
		}
		int ii = 0;
	}

    return true;
}

namespace gui2 {

REGISTER_DIALOG(launcher, moveit)

tmoveit::tmoveit(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy, tmoveit_driver& moveit_driver, tdrivers& drivers, tros_instance& ros_instance, trobot_imu& robot_imu)
	: tstatusbar(rdpd_mgr, pble, privacy)
	, SLIDER_TIMES(200) // 100
	, SLIDER_GTE0_OFFSET(1000)
	, moveit_driver_(moveit_driver)
	, drivers_(drivers)
	, ros_instance_(ros_instance)
	, robot_imu_(robot_imu)
	, robot_model_(ros_instance.aplt_model_of_moveit_model())
	, variable_positions_(ros_instance.aplt_positions_of_moveit_model())
	, tf_calculator_(ros_instance.tf_calculator_of_moveit_model())
	// , in_moveit_gui_lock_(ros_instance)
	, max_display_joints_(6)
	, release_ik_mid_dat_(game_config::preferences_dir + "/ik_mid.dat")
	, curr_group_(nullptr)
	, this_positions_(nullptr)
	, disable_reload_fk_joints_(false)
	, disable_JointState_(false)
	, status_widget_(nullptr)
	, rpy_widget_(nullptr)
	, include_first_widget_(nullptr)
	, joint_list_(nullptr)
{
	VALIDATE(robot_model_.valid(), null_str);

	set_timer_interval(200);

	this_positions_ = (double*)malloc(variable_positions_.count * sizeof(double));
	copy_to_this_positions();

	ros_instance_.register_slot(*this);
	ros_instance_.start_moveit_node(false);
}

tmoveit::~tmoveit()
{
	// ros_instance_.stop_moveit_node();
	ros_instance_.deregister_slot(*this);

	if (this_positions_ != nullptr) {
		free(this_positions_);
	}
}

void tmoveit::pre_show()
{
	window_->set_label("misc/bg_ffffff.png");
	
	tstatusbar::pre_show(*window_, find_widget<tpanel>(window_, "statusbar", false).grid());

	// status_widget_ and include_first_widget_ must be valid before set_curr_group(...)
	status_widget_ = find_widget<tlabel>(window_, "status", false, true);

	ttoggle_button* toggle = find_widget<ttoggle_button>(window_, "include_first", false, true);
	toggle->set_did_state_changed(std::bind(&tmoveit::did_include_first_changed, this, _1));
	include_first_widget_ = toggle;

	set_curr_group(*robot_model_.joint_model_groups_[0]);

	rpy_widget_ = find_widget<tlabel>(window_, "rpy", false, true);

	find_widget<tlabel>(window_, "title", false, true)->set_label(aplt::all_fake_applets.find(aplt::builtinid_moveit)->second.name);
	find_widget<tlabel>(window_, "warnning", false, true)->set_label(ht::generate_format(disable_timing_warnning(), 0xffff0000));

	tbutton* button = find_widget<tbutton>(window_, "group", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmoveit::click_group
			, this, std::ref(*button)));
	button->set_border("textbox");
	button->set_label(curr_group_->name_);

	button = find_widget<tbutton>(window_, "ik_range", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmoveit::click_ik_range
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "lookup_table", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmoveit::click_generate_moveit_rsp
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "ikfast", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmoveit::click_ikfast
			, this, std::ref(*button)));

	button = find_widget<tbutton>(window_, "state", false, true);
	connect_signal_mouse_left_click(
			  *button
			, std::bind(
			&tmoveit::click_state
			, this, std::ref(*button)));

	tlistbox* listbox = find_widget<tlistbox>(window_, "joints", false, true);
	listbox->enable_select(false);
	joint_list_ = listbox;
	reload_joint_list(*joint_list_);
}

void tmoveit::post_show()
{
}

void tmoveit::set_status_label(const std::string& msg)
{
	status_widget_->set_label(msg);
}

void tmoveit::set_rpy_label()
{
	rpy_widget_->set_label(ros_instance_.get_imu_desc());
}

void tmoveit::reload_fk_joints()
{
	VALIDATE(curr_group_ != nullptr, null_str);

	bool include_first = include_first_widget_->get_value();
	if (curr_group_->joint_model_vector_.size() == 1) {
		VALIDATE(include_first, null_str);
	}

	fk_joints_.clear();

	for (std::vector<aplt::tjoint_model*>::const_iterator it = curr_group_->joint_model_vector_.begin(); it != curr_group_->joint_model_vector_.end(); ++ it) {
		const aplt::tjoint_model* joint_model = *it;

		if (!include_first && it == curr_group_->joint_model_vector_.begin()) {
			continue;
		}

		bool fixed = robot_model_.joint_variables_index_map_.count(joint_model->name_) == 0;
		fk_joints_.push_back(ros::tfk_joint(joint_model->name_, fixed, joint_model->origin_, joint_model->axis_));
	}
}

void tmoveit::set_curr_group(const aplt::tjoint_group_model& group)
{
	VALIDATE(include_first_widget_ != nullptr, null_str);

	curr_group_ = &group;

	const int joint_count = curr_group_->joint_model_vector_.size();
	VALIDATE(joint_count <= max_display_joints_, "Now max support 6 joints");

	include_first_widget_->set_visible(joint_count != 0? twidget::VISIBLE: twidget::INVISIBLE);
	if (joint_count != 0) {
		{
			tdisable_reload_fk_joints_lock lock(*this);
			include_first_widget_->set_value(true);
		}
		include_first_widget_->set_active(joint_count > 1);

		utils::string_map symbols;
		symbols["joint"] = curr_group_->joint_model_vector_[0]->name_;
		include_first_widget_->set_label(vgettext2("fk include '$joint'", symbols));
	}
	reload_fk_joints();
}

void tmoveit::reload_joint_list(tlistbox& list)
{
	list.clear();
	tdisable_JointState_lock lock(*this);

	std::map<std::string, std::string> data;

	std::stringstream ss;

	int at = 0;
	for (std::vector<std::string>::const_iterator it = curr_group_->variable_names_.begin(); it != curr_group_->variable_names_.end(); ++ it, at ++) {
		if (at >= max_display_joints_) {
			break;
		}
		const std::string& joint_name = *it;
		VALIDATE(robot_model_.joint_model_map_.count(joint_name) != 0, null_str);

		const aplt::tjoint_model* joint = robot_model_.joint_model_map_.find(joint_name)->second;

		VALIDATE(robot_model_.joint_variables_index_map_.count(joint_name), null_str);

		data["name"] = joint->name_;
		ttoggle_panel& row = list.insert_row(data);

		tslider* slider = find_widget<tslider>(&row, "slider", false, true);
		slider->set_did_value_changed(std::bind(&tmoveit::did_slider_value_changed, this, std::ref(list), _1, _2, at));
		slider->set_maximum_value(SLIDER_GTE0_OFFSET + joint->max_position_ * SLIDER_TIMES);
		slider->set_minimum_value(SLIDER_GTE0_OFFSET + joint->min_position_ * SLIDER_TIMES);
		slider->set_value(SLIDER_GTE0_OFFSET + this_positions_[joint->variable_index_] * SLIDER_TIMES);
	}
}

void tmoveit::did_joints_value_changed(tlistbox& list, bool wait_for_joint_state_publisher)
{
	SDL_Log("%u did_joints_value_changed called", SDL_GetTicks());
	if (wait_for_joint_state_publisher) {
		// joint_state_publisher receive last JointState and save to RobotState require some time.
		SDL_Delay(ros_instance_.light_moveit()? 200: 500);
	}

	copy_to_this_positions();

	tdisable_JointState_lock lock(*this);

	std::vector<double> angles;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = curr_group_->variable_names_.begin(); it != curr_group_->variable_names_.end(); ++ it, at ++) {
		if (at >= max_display_joints_) {
			break;
		}
		const std::string& joint_name = *it;
		VALIDATE(robot_model_.joint_model_map_.count(joint_name) != 0, null_str);

		const aplt::tjoint_model* joint = robot_model_.joint_model_map_.find(joint_name)->second;

		VALIDATE(robot_model_.joint_variables_index_map_.count(joint_name), null_str);

		ttoggle_panel& row = list.row_panel(at);

		tslider* slider = find_widget<tslider>(&row, "slider", false, true);
		slider->set_value(SLIDER_GTE0_OFFSET + this_positions_[joint->variable_index_] * SLIDER_TIMES);

		double value = this_positions_[joint->variable_index_];

		if (include_first_widget_->get_value() || it != curr_group_->variable_names_.begin()) {
			angles.push_back(value);
		}
	}
/*
	std::vector<double> angles;
	for (std::vector<std::string>::const_iterator it = curr_group_->variable_names_.begin(); it != curr_group_->variable_names_.end(); ++ it) {
		const aplt::tjoint_model* joint = robot_model_.joint_model_map_.find(*it)->second;
		double value = this_positions_[joint->variable_index_];

		if (include_first_widget_->get_value() || it != curr_group_->variable_names_.begin()) {
			angles.push_back(value);
		}
	}
*/
	calculate_fk(angles);
}

double calculate_yaw(const geometry_msgs::Pose& result)
{
/*
	geometry_msgs::Pose robot_pose;
    tf2::toMsg(tf2::Transform::getIdentity(), robot_pose);

    geometry_msgs::Pose t_out;
    tf2::doTransform(robot_pose, t_out, transform);

    double x = t_out.position.x;
    double y = t_out.position.y;
    double yaw = tf2::getYaw(t_out.orientation);
*/
	double yaw = tf2::getYaw(result.orientation);
	SDL_Log("calculate_yaw: %.3f", RAD2DEG(yaw));
	return yaw;
}

void tmoveit::calculate_fk(const std::vector<double>& angles)
{
	geometry_msgs::Pose result = tf_calculator_.calculate_tf(fk_joints_, angles);

	double roll, pitch, yaw;
	tf2::getEulerYPR(result.orientation, yaw, pitch, roll);

	double yaw2 = calculate_yaw(result);

	char buf[256];
	SDL_snprintf(buf, sizeof(buf), "fk {t:(%.6f, %.6f, %.6f) q:(%.6f(%.2f), %.6f(%.2f), %.6f(%.2f))} %.2f", 
		result.position.x, result.position.y, result.position.z,
		roll, RAD2DEG(roll), pitch, RAD2DEG(pitch), yaw, RAD2DEG(yaw),
		RAD2DEG(yaw2));
	set_status_label(buf);
}

void tmoveit::did_include_first_changed(ttoggle_button& widget)
{
	if (!disable_reload_fk_joints_) {
		reload_fk_joints();

		std::vector<double> angles;
		for (std::vector<std::string>::const_iterator it = curr_group_->variable_names_.begin(); it != curr_group_->variable_names_.end(); ++ it) {
			const aplt::tjoint_model* joint = robot_model_.joint_model_map_.find(*it)->second;
			double value = this_positions_[joint->variable_index_];

			if (include_first_widget_->get_value() || it != curr_group_->variable_names_.begin()) {
				angles.push_back(value);
			}
		}
		calculate_fk(angles);

	}
}

void tmoveit::copy_to_this_positions()
{
	memcpy(this_positions_, variable_positions_.positions, variable_positions_.count * sizeof(double));
}

void tmoveit::set_this_position(int at, double val)
{
	VALIDATE(at >= 0 && at < variable_positions_.count, null_str);
	this_positions_[at] = val;
}

void tmoveit::did_slider_value_changed(tlistbox& list, tslider& widget, int value, int at)
{
	aplt::tjoint_model* cur_joint = robot_model_.joint_model_map_.find(curr_group_->variable_names_[at])->second;
	set_this_position(cur_joint->variable_index_, 1.0 * (value - SLIDER_GTE0_OFFSET) / SLIDER_TIMES);

	std::map<std::string, double> values;
	std::vector<double> angles;
	for (std::vector<std::string>::const_iterator it = curr_group_->variable_names_.begin(); it != curr_group_->variable_names_.end(); ++ it) {
		const aplt::tjoint_model* joint = robot_model_.joint_model_map_.find(*it)->second;
		double value = this_positions_[joint->variable_index_];

		values.insert(std::make_pair(joint->name_, value));
		if (include_first_widget_->get_value() || it != curr_group_->variable_names_.begin()) {
			angles.push_back(value);
		}
	}

	if (!disable_JointState_) {
		ros_instance_.do_JointState_action(values);
		calculate_fk(angles);
	}

	ttoggle_panel& row = list.row_panel(at);
	tlabel* label = find_widget<tlabel>(&row, "value", false, true);
	set_joint_value_label(*label, this_positions_[cur_joint->variable_index_]);
}

void tmoveit::set_joint_value_label(tlabel& widget, double value)
{
	// char buf[32];
	// SDL_snprintf(buf, sizeof(buf), "%+.04f(deg:%+.04f)", value, RAD2DEG(value)); 
	widget.set_label(utils::from_double(value));
}

void tmoveit::click_group(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;

	for (std::vector<aplt::tjoint_group_model*>::const_iterator it = robot_model_.joint_model_groups_.begin(); it != robot_model_.joint_model_groups_.end(); ++ it) {
		const aplt::tjoint_group_model* group = *it;
		items.push_back(gui2::tmenu::titem(group->name_, items.size()));

		if (curr_group_ == group) {
			initial_sel = items.size() - 1;
		}
	}
	
	gui2::tmenu dlg(items, initial_sel);
	dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
	if (dlg.get_retval() != gui2::twindow::OK) {
		return;
	}

	const int cursel = dlg.selected_val();

	set_curr_group(*robot_model_.joint_model_groups_[cursel]);

	widget.set_label(curr_group_->name_);

	reload_joint_list(*joint_list_);
}

void push_clawgap(trsp_clawgap* items, int index, double value, double gap, double dist)
{
	VALIDATE(index >= 0, null_str);
	const double min_gap = 0.00; // 0mm
	const double max_gap = 0.12; // 12cm
	VALIDATE(gap >= min_gap && gap <= max_gap, null_str);

	const double min_dist = 0.06; // 6cm
	const double max_dist = 0.15; // 15cm
	VALIDATE(dist >= min_dist && dist <= max_dist, null_str);

	if (index > 0) {
		// halfgap must be ascending order
		const trsp_clawgap& prev_item = items[index - 1];
		VALIDATE(prev_item.halfgap <= gap / 2, null_str);
	}

	items[index].value = value;
	items[index].halfgap = gap / 2;
	items[index].dist = dist;
}

struct tik_yitem_C
{
	int y;
	double z;
	double mid;
	double range;
};

std::string parse_ik_mid_dat(const std::string& ik_mid_dat, std::vector<tik_yitem_C>& yitems)
{
	yitems.clear();

	char buf[128];
	tfile file(ik_mid_dat, GENERIC_READ, OPEN_EXISTING);
	int fsize = file.read_2_data(0, 1);
	if (fsize == 0) {
		SDL_snprintf(buf, sizeof(buf), "%s, cannot open or is empty", ik_mid_dat.c_str());
		return buf;
	}
	file.data[fsize ++] = '\n'; // allow last line no '\n'
	file.data[fsize] = '\0';

	std::set<std::string> idtype_numbers;
	std::set<std::string> house_names;
	int start = fsize > 3 && (uint8_t)(file.data[0]) == 0xef && (uint8_t)(file.data[1]) == 0xbb && (uint8_t)(file.data[2]) == 0xbf? 3: 0;
	int pos = start;
	const char* tmp_data = file.data;
	int line = 0;
	std::string err_msg;
	if (!utils::is_utf8str(tmp_data + start, fsize - start - 1)) {
		SDL_snprintf(buf, sizeof(buf), "%s, isn't utf8 file", ik_mid_dat.c_str());
		return buf;
	}
	while (pos < fsize && err_msg.empty()) {
		uint8_t ch = tmp_data[pos];
		if (ch == '\n') {
			std::string span(tmp_data + start, pos - start);
			std::vector<std::string> vstr = utils::split(span, ',', utils::STRIP_SPACES);
			if (vstr.size() != 4) {
				break;
			}

			yitems.push_back(tik_yitem_C());
			tik_yitem_C& yitem = yitems.back();

			// strip ' at left/right edge.
			yitem.y = utils::to_int(vstr[0]);
			yitem.z = SDL_atof(vstr[1].c_str());
			yitem.mid = SDL_atof(vstr[2].c_str());
			yitem.range = SDL_atof(vstr[3].c_str());

			line ++;
			start = pos + 1;
		}
		pos ++;
	}

	return null_str;
}

int calculate_clawgap_data(trsp_clawgap* clawgap_data, double joint5_connector_x, int max_clawgaps)
{
	// radian(0.8) == degree(45.85987261146497)

	const SDL_DRange joint6_range{-0.8, 0.8};
	const double y_shoulder = 0.025;
	const double y_val0 = 0.061; // require measure
	const double y_glove = 0.011; // 0.01 + 0.001 ---bonuse 0.001, iet it be clamped tighter
	const double x_wrist = 0.025; // 0.0305
	const double x_hypotenuse = 0.035;
	const double x_finger = 0.0685;

	double K;
	double gap;
	double L;
	double L2;
	double dist;

	int index = 0;
	SDL_Log("---calculate_clawgap_data---");
	double theta_val0 = asin((y_val0 / 2 - y_shoulder/ 2) / 0.035);
	SDL_Log("min_theta: %.3f / 0.035 --> %.3f(rad) %.3f(deg)", y_val0 / 2 - y_shoulder/ 2, theta_val0, RAD2DEG(theta_val0));

	for (double value = joint6_range.min; value < joint6_range.max + 0.01; value += 0.05) {
		VALIDATE(index < max_clawgaps, null_str);
		double theta = value + theta_val0;

		// gap
		if (theta >= 0) {
			K = y_shoulder + 2 * x_hypotenuse * sin(theta);
		} else {
			K = y_shoulder - 2 * x_hypotenuse * sin(-theta);
		}
		gap = K - y_glove;
		if (gap < 0) {
			gap = 0;
		}

		// dist
		if (theta >= 0) {
			L = x_hypotenuse * cos(theta) + x_finger;
		} else {
			L = x_hypotenuse * cos(-theta) + x_finger;
		}
		L2 = L + x_wrist;
		dist = L2 + joint5_connector_x;

		SDL_Log("[%02i]%.4f(theta: %.2f) --> gap: %.4f(K: %.4f), dist: %.4f(L2: %.4f)", index, value, RAD2DEG(theta), gap, K, dist, L2);
		push_clawgap(clawgap_data, index ++, value, gap, dist);
	}

	SDL_Log("------------");
	return index;
}

int calculate_clawgap_data2(trsp_clawgap* clawgap_data, double joint5_connector_x, int max_clawgaps)
{
	int index = 0;
	push_clawgap(clawgap_data, index ++, -0.8, 0.021, 0.136);
	push_clawgap(clawgap_data, index ++, -0.7, 0.022, 0.137);
	push_clawgap(clawgap_data, index ++, -0.6, 0.029, 0.136);
	push_clawgap(clawgap_data, index ++, -0.5, 0.035, 0.135);
	push_clawgap(clawgap_data, index ++, -0.45, 0.038, 0.134);
	push_clawgap(clawgap_data, index ++, -0.4, 0.041, 0.133);
	push_clawgap(clawgap_data, index ++, -0.3, 0.048, 0.131);
	push_clawgap(clawgap_data, index ++, -0.2, 0.054, 0.129);
	push_clawgap(clawgap_data, index ++, -0.1, 0.060, 0.127);
	push_clawgap(clawgap_data, index ++, 0.0, 0.066, 0.124);
	push_clawgap(clawgap_data, index ++, 0.1, 0.070, 0.120);
	push_clawgap(clawgap_data, index ++, 0.2, 0.073, 0.118);
	push_clawgap(clawgap_data, index ++, 0.3, 0.076, 0.115);
	push_clawgap(clawgap_data, index ++, 0.4, 0.079, 0.112);
	push_clawgap(clawgap_data, index ++, 0.45, 0.080, 0.110);
	push_clawgap(clawgap_data, index ++, 0.5, 0.082, 0.108);
	push_clawgap(clawgap_data, index ++, 0.6, 0.083, 0.105);
	push_clawgap(clawgap_data, index ++, 0.7, 0.083, 0.102);
	push_clawgap(clawgap_data, index ++, 0.8, 0.084, 0.101);
	
	// 8 * 2 + 3
	VALIDATE(index == 8 * 2 + 3, null_str);

	return index;
}

void tmoveit::click_generate_moveit_rsp(tbutton& widget)
{
	std::vector<tik_yitem_C> yitems;
	std::string err = parse_ik_mid_dat(release_ik_mid_dat_, yitems);
	if (!err.empty()) {
		gui2::show_message(null_str, err);
		return;
	}

	if (yitems.empty()) {
		return;
	}

	int yitems_bytes = sizeof(trsp_ikmid) * yitems.size();
	trsp_ikmid* ikmids_data = (trsp_ikmid*)malloc(yitems_bytes);
	memset(ikmids_data, 0, yitems_bytes);

	int index = 0;
	for (std::vector<tik_yitem_C>::const_reverse_iterator rit = yitems.rbegin(); rit != yitems.rend(); ++ rit, index ++) {
		const tik_yitem_C& src = *rit;
		trsp_ikmid& dst = ikmids_data[index];
		dst.z = src.z;
		dst.mid = src.mid;
		dst.range = src.range;
	}

	const aplt::tjoint_group_model* group_model = robot_model_.joint_model_group_map_.find(group_name_claw)->second;
	VALIDATE(group_model->joint_model_vector_.size() == 1, null_str);

	const aplt::tjoint_model* joint_model = group_model->joint_model_vector_[0];
	const double max_value = joint_model->max_position_;
	const double min_value = joint_model->min_position_;

	const int max_clawgaps = 100;
	int data_len2 = sizeof(trsp_clawgap) * max_clawgaps;
	trsp_clawgap* clawgap_data = (trsp_clawgap*)malloc(data_len2);
	memset(clawgap_data, 0, data_len2);

	const double joint5_connector_x = 0.0487 - 0.0432;

	const int clawgaps = calculate_clawgap_data(clawgap_data, joint5_connector_x, max_clawgaps);
	VALIDATE(clawgaps <= max_clawgaps, null_str);


	const std::string rspfile = game_config::preferences_dir + "/moveit.rsp";
	const std::string bundleid = "com.kos.launcher";
	tsha1writer sha1file(rspfile, nposm, std::bind(&did_write_rsp_moveit2, _1, bundleid, std::ref(game_config::rose_version), clawgap_data, clawgaps, ikmids_data, yitems.size()));
	sha1file.write();

	free(clawgap_data);
	free(ikmids_data);

	utils::string_map symbols;
	symbols["rspfile"] = rspfile;
	gui2::show_message(null_str, vgettext2("Generate moveit.rsp finished. file: $rspfile", symbols));
}

static std::string vector_double_DebugString(const std::vector<double>& values)
{
    std::stringstream res;
    res << "(";
    for (std::vector<double>::const_iterator it = values.begin(); it != values.end(); ++ it) {
        const double& value = *it;
        if (it != values.begin()) {
            res << ", ";
        }
        res << value;
    }
    res << ")";
    return res.str();
}

bool tmoveit::did_ik_range(gui2::tprogress_& progress, double step, double ik_bound, const std::string& result_png)
{
	VALIDATE(ik_bound > 0, null_str);
	VALIDATE(!result_png.empty(), null_str);

	const std::string ik_arm_name = group_name_ik_arm;

	std::vector<double> angles;
	angles.push_back(-1.57);
	angles.push_back(0);
	angles.push_back(0);
	geometry_msgs::Pose xmax_z0_fk = ros_instance_.calculate_group_fk_tf(ik_arm_name, &angles);

	angles.clear();
	angles.push_back(-1.57);
	angles.push_back(0);
	geometry_msgs::Pose a_fk = ros_instance_.calculate_group_fk_tf("ik_arm_sub1", &angles);

	double outer_safe = 0.0; // 0.01
	double inner_safe = 0.0; // 0.01
	double a = a_fk.position.x - outer_safe; // 0.0955(0.1055 - 0.01)
	double b = xmax_z0_fk.position.x - a_fk.position.x - outer_safe; // 0.0875(0.0975 - 0.01)

	double bonus_x = 0.01;
	double max_x = a + b + bonus_x;
	// double min_x = max_x - 0.1; // - 0.1
	double min_x = 0.0;
	double step_x = step;

	double bonus_z = 0.0; // 0.01
	double max_z = a + b + bonus_z; // 0
	double min_z = -b;
	double step_z = step;

	bool use_as_seed = false;
	double nolimit = std::numeric_limits<float>::max();

	char buf[256];
	int fails = 0;
	const tpose3d bounds(ik_bound, nolimit, ik_bound, nolimit, nolimit, nolimit);
	tpose3d pose3d(0, 0, 0, 0, 0, 0);
	std::vector<double> result;

	// {t:(0.162326, 0.017199, 0.091761)}
	// pose3d.x = 0.162326;
	// pose3d.z = 0.091761;
	// result = ros_instance_.do_light_joint_pose_target(ik_arm_name, pose3d, bounds, use_as_seed);

	enum {u8_ikfail_ge0 = 0, u8_ikfail_less0 = 0x40, u8_ikok = 255, u8_radius_fail = 0x50, u8_radius_ok = 0xe0};

	int cells = 0;
	int width = nposm;
	double first_less0_z = float_nposm;
	int estimate_width = (max_x - min_x) / step_x;
	int estimate_height = (max_z - min_z) / step_z;
	uint8_t* flags = (uint8_t*)malloc((estimate_width + 2) * (estimate_height + 2));
	SDL_DPoint* xz = (SDL_DPoint*)malloc((estimate_width + 2) * (estimate_height + 2) * sizeof(SDL_DPoint));

	uint32_t start_ticks = SDL_GetTicks();
	for (double z = max_z; z >= min_z; z -= step_z) {
		if (width != nposm) {
			SDL_snprintf(buf, sizeof(buf), "%.6f - z(%.3f) -> %.6f", z, step_z, min_z);
			progress.set_message(buf);
			progress.set_percentage(100 * cells / (width * estimate_height));
		}

		int cols = 0;
		for (double x = min_x; x <= max_x; x += step_x, cols ++) {
			// delta_twist: [   0.0505859,       0.002,   0.0466934, 5.17015e-17,    -2.61759, 2.96563e-16] q_out: [   -0.489953    0.248543    0.487098]
			pose3d.x = x;
			pose3d.z = z;
			result = ros_instance_.do_light_joint_pose_target(ik_arm_name, pose3d, bounds, use_as_seed, false);
			if (!result.empty()) {
				geometry_msgs::Pose curr_fk = ros_instance_.calculate_group_fk_tf(ik_arm_name, &result);
				SDL_Log("[%i]ik_query:(%.6f, %.6f) result: %s =>diff: (%.6f, %.6f, %.6f)",
					cells, pose3d.x, pose3d.z, vector_double_DebugString(result).c_str(),
					pose3d.x - curr_fk.position.x, pose3d.y - curr_fk.position.y, pose3d.z - curr_fk.position.z);
				flags[cells] = u8_ikok;
			} else {
				SDL_Log("[%i]ik_query:(%.6f, %.6f) fail", cells, pose3d.x, pose3d.z);
				fails ++;
				flags[cells] = z >= 0.0? u8_ikfail_ge0: u8_ikfail_less0;
			}
			
			if (z < 0.0 && is_float_nposm(first_less0_z)) {
				first_less0_z = z;
			}
			xz[cells].x = x;
			xz[cells].y = z;

			cells ++;
		}
		if (width == nposm) {
			width = cols;
		} else {
			VALIDATE(width == cols, null_str);
		}
	}

	uint32_t elapse = SDL_GetTicks() - start_ticks;
	const int height = cells / width;

	struct tyitem_C
	{
		double mid;
		double range;
	};

	std::map<int, tyitem_C> yitems;
	std::vector<SDL_DPoint> xy_data;
	SDL_DPoint3 abc;
	for (int y = 0; y < height; y ++) {
		const int y_start = y * width;
		const double point_z = xz[y_start + 0].y;
		double first_can = float_nposm;
		double last_can = float_nposm;
		for (int x = 0; x < width; x ++) {
			int index = y_start + x;
			const SDL_DPoint& point = xz[index];
			VALIDATE(KDL::Equal(point_z, point.y), null_str);

			// SDL_Log("(%i, %i)point: (%.3f, %.3f) flag: %s", x, y, point.x, point.y, flags[index] == u8_ikok? "true": "false");

			if (is_float_nposm(first_can)) {
				if (flags[index] == u8_ikok) {
					first_can = point.x;
				}

			} else if (flags[index] == u8_ikok) {
				last_can = point.x;
			}
		}
		if (!is_float_nposm(first_can) && !is_float_nposm(last_can)) {
			const double range = last_can - first_can + step_x;
			double mid = first_can + range / 2;
			xy_data.push_back(SDL_DPoint{point_z, mid});
			yitems.insert(std::make_pair(y, tyitem_C{mid, range}));
			SDL_Log("y: %i point_z: %.6f range: %.6f(first_can: %.6f last_can: %.6f), mid: %.6f", 
				y, point_z, range, first_can, last_can, mid);
		} else {
			// SDL_Log("y: %i point_z: %.6f first_can: %.6f last_can: %.6f, ignore ", y, point_z, first_can, last_can);
		}
	}

	std::stringstream ik_mid_ss;
	const int max_xys = 10;
	double max_abs_diff = nposm;
	if (xy_data.size() >= max_xys) { 
		abc = ceres_curve_fitting(xy_data);
		SDL_Log("estimate[a: %.8f b: %.8f c: %.8f]", abc.x, abc.y, abc.z);

		for (int y = 0; y < height; y ++) {
			if (yitems.count(y) == 0) {
				continue;
			}

			const tyitem_C& yitem = yitems.find(y)->second;

			const int y_start = y * width;
			const double z = xz[y_start + 0].y;
			double estimated_mid = exp(abc.x * z * z + abc.y * z + abc.z);
			double abs_diff = fabs(estimated_mid - yitem.mid);
			if (is_float_nposm(max_abs_diff) || abs_diff > max_abs_diff) {
				max_abs_diff = abs_diff;
			}
			SDL_Log("y: %i z: %.6f => estimated: %.6f desire: %.6f abs_diff: %.6f", y, z, estimated_mid, yitem.mid, abs_diff);
			if (!ik_mid_ss.str().empty()) {
				ik_mid_ss << "\n";
			}
			// ik_mid_ss << y << ", " << std::setprecision(6) << z << ", " << yitem.mid << ", " << yitem.range;
			ik_mid_ss << y << ", " << z << ", " << yitem.mid << ", " << yitem.range;

			bool found = false;
			for (int x = 0; x < width; x ++) {
				int index = y_start + x;
				const SDL_DPoint& point = xz[index];
				
				if (!found && (point.x >= estimated_mid || point.x + step_x > estimated_mid)) {
					VALIDATE(flags[index] == u8_ikok, null_str);
					flags[index] = u8_radius_ok;
					found = true;
				}
			}
		}
	}

	std::map<uint8_t, uint32_t> palette;
	palette.insert(std::make_pair(u8_radius_ok, 0xff00ff00)); // green
	palette.insert(std::make_pair(u8_radius_fail, 0xffffff00)); // yellow

	const int margin_x = 4;
	const int cell_size = 8; // 16
    surface surf = u8_data_2_cell_surf(0xffffffff, flags, margin_x, 4, width, height, cell_size, 0xffff0000, 0xff788b8a, nposm, &palette);
    // imwrite(surf, "1-cell_value.png");

	free(flags);

	// surface surf = image::get_image(game_config::preferences_dir + "/1-cell_value1.png");
	// VALIDATE(surf.get() != nullptr, null_str);

	SDL_snprintf(buf, sizeof(buf), "ik_bound(%.4f) fails(%i/%i) cell(%.3fx%.3f) cells(%ix%i) use %s\nabc:(%.8f, %.8f, %.8f)\na(%.6f) + b(%.6f) = %.6f z0(%.6f) first_less0_z(%.6f)\n(x:%.6f, z:%.6f)",
		ik_bound, fails, cells, step_x, step_z, width, height, utils::format_elapse_hms(elapse / 1000).c_str(),
		abc.x, abc.y, abc.z,
		a, b, a + b, xmax_z0_fk.position.z, first_less0_z,
		min_x, max_z);
	surface top_text_surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLEST);

	SDL_snprintf(buf, sizeof(buf), "(x:%.6f, z:%.6f)", max_x, min_z);
	surface bottom_text_surf = font::get_rendered_text(buf, 0, font::SIZE_SMALLEST);

	int bg_width = SDL_max(margin_x + top_text_surf->w, surf->w);
	int bg_height = top_text_surf->h + surf->h + bottom_text_surf->h;
	surface bg_surf = create_neutral_surface(bg_width, bg_height);

	// blit top text
	int y_offset = 0;
	SDL_Rect dstrect{margin_x, 0, top_text_surf->w, top_text_surf->h};
    sdl_blit(top_text_surf, nullptr, bg_surf, &dstrect);
	y_offset += top_text_surf->h;

	// blit ik-range
	dstrect = ::create_rect(0, y_offset, surf->w, surf->h);
    sdl_blit(surf, nullptr, bg_surf, &dstrect);
	y_offset += surf->h;

	// blit bottom text
	int bottom_text_x = surf->w - margin_x;
	if (bottom_text_x + bottom_text_surf->w > bg_surf->w) {
		bottom_text_x = bg_surf->w - bottom_text_surf->w;
	}
	dstrect = ::create_rect(bottom_text_x, y_offset, bottom_text_surf->w, bottom_text_surf->h);
    sdl_blit(bottom_text_surf, nullptr, bg_surf, &dstrect);

	imwrite(bg_surf, result_png);

	{
		tfile file(game_config::preferences_dir + "/ik_mid.dat", GENERIC_WRITE, CREATE_ALWAYS);
		VALIDATE(file.valid(), null_str);
		if (!ik_mid_ss.str().empty()) {
			posix_fwrite(file.fp, ik_mid_ss.str().c_str(), ik_mid_ss.str().size());
		}
	}

	return true;
}

void tmoveit::click_ik_range(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	std::vector<std::pair<double, std::string> > steps;
	steps.push_back(std::make_pair(0.002, "for release(2mm)"));
	steps.push_back(std::make_pair(0.02, "for debug(2cm)"));

	for (std::vector<std::pair<double, std::string> >::const_iterator it = steps.begin(); it != steps.end(); ++ it) {
		const std::pair<double, std::string>& p = *it;
		items.push_back(gui2::tmenu::titem(p.second, items.size()));
	}

	if (items.empty()) {
		return;
	}

	double step = float_nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		step = steps[dlg.selected_val()].first;
	}

	const double ik_bound = 0.005; // 0.005

	char result_png[32];
	SDL_snprintf(result_png, sizeof(result_png), "ik_range-s%imm-b%imm.png", (int)ceil(step * 1000), (int)ceil(ik_bound * 1000));

	gui2::tprogress_default_slot slot(std::bind(&tmoveit::did_ik_range, this, _1, step, ik_bound, result_png));
	gui2::run_with_progress(slot, null_str, null_str, 0);

	utils::string_map symbols;
	symbols["file"] = result_png;
	std::string msg = vgettext2("ik range finished, result is saved at $file", symbols);
	gui2::show_message(null_str, msg);
}

void tmoveit::click_ikfast(tbutton& widget)
{
	std::string msg = "IK fail";
	if (ros_instance_.light_moveit()) {
		std::stringstream ss;

		std::vector<double> angles;
		int at = 0;
		for (std::vector<std::string>::const_iterator it = curr_group_->variable_names_.begin(); it != curr_group_->variable_names_.end(); ++ it, at ++) {
			const aplt::tjoint_model* joint = robot_model_.joint_model_map_.find(*it)->second;

			double value = this_positions_[joint->variable_index_];
			angles.push_back(value);

			if (at != 0) {
				ss << ", ";
			}
			ss << joint->name_ << ":" << std::setprecision(5) << value;
		}

		SDL_Log("{dbg_ik}(light_moveit)values: %s", ss.str().c_str());

		geometry_msgs::Pose result = tf_calculator_.calculate_tf(fk_joints_, angles);
		double roll, pitch, yaw;
		tf2::getEulerYPR(result.orientation, yaw, pitch, roll);

		char buf[256];
		SDL_snprintf(buf, sizeof(buf), "{t:(%.6f, %.6f, %.6f) q:(%.6f(deg:%.5f), %.6f(deg:%.5f), %.6f(deg:%.5f))}", 
			result.position.x, result.position.y, result.position.z,
			roll, RAD2DEG(roll), pitch, RAD2DEG(pitch), yaw, RAD2DEG(yaw));
		SDL_Log("{dbg_ik}(light_moveit)ik_query: %s", buf);

		{
			roll *= -1;
			// pitch *= -1;
			// pitch = DEG2RAD(20);
			pitch = pitch + DEG2RAD(10);
			yaw *= -1;
		}

		tpose3d ik_query(result.position.x, result.position.y, result.position.z, roll, pitch, yaw);
		double ik_bound = 0.005;
		tpose3d bounds(ik_bound, std::numeric_limits<float>::max(), ik_bound, std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		bool use_as_seed = false;
		std::vector<double> joint_values = ros_instance_.do_light_joint_pose_target(curr_group_->name_, ik_query, bounds, use_as_seed);

		if (!joint_values.empty()) {
			ros_instance_.do_light_joint_value_target(curr_group_->name_, joint_values);

			VALIDATE(curr_group_->variable_names_.size() == joint_values.size(), null_str);
			ss.str("");
			ss << "IK successful. ";
			at = 0;
			for (std::vector<std::string>::const_iterator it = curr_group_->variable_names_.begin(); it != curr_group_->variable_names_.end(); ++ it, at ++) {
				const aplt::tjoint_model* joint = robot_model_.joint_model_map_.find(*it)->second;

				double value = joint_values[at];

				if (at != 0) {
					ss << ", ";
				}
				ss << joint->name_ << ":" << std::setprecision(5) << value;
			}
			msg = ss.str();
		}

	} else {
		ros_instance_.do_moveit_temporary();
	}

	did_joints_value_changed(*joint_list_, true);

	if (ros_instance_.light_moveit()) {
		gui2::show_message(null_str, msg);
	}

	// ros_instance_.do_ik();
	// ros_instance_.do_pickplace();
}

void tmoveit::click_state(tbutton& widget)
{
	std::vector<gui2::tmenu::titem> items;
	int initial_sel = nposm;
	std::stringstream ss;

	items.push_back(gui2::tmenu::titem("navigation", aplt::tmoveit_slot::state_navigation));
	items.push_back(gui2::tmenu::titem("recognize", aplt::tmoveit_slot::state_recognize));
	items.push_back(gui2::tmenu::titem("recognize_near", aplt::tmoveit_slot::state_recognize_near));
	items.push_back(gui2::tmenu::titem("overlook", aplt::tmoveit_slot::state_overlook));
	items.push_back(gui2::tmenu::titem("place", aplt::tmoveit_slot::state_place));
	items.push_back(gui2::tmenu::titem("place_right", aplt::tmoveit_slot::state_place_right));

	if (items.empty()) {
		return;
	}

	int action = nposm;

	{
		gui2::tmenu dlg(items, initial_sel);
		dlg.show(widget.get_x(), widget.get_y() + widget.get_height() + 16 * twidget::hdpi_scale);
		if (dlg.get_retval() != gui2::twindow::OK) {
			return;
		}

		action = dlg.selected_val();
	}

	const std::vector<aplt::tgroup_state>& states = moveit_driver_.get_common_state(action);

	for (std::vector<aplt::tgroup_state>::const_iterator it = states.begin(); it != states.end(); ++ it) {
		const aplt::tgroup_state& group_state = *it;
		const bool name_arm_require_0 = false;
		if (name_arm_require_0 && group_state.name == group_name_arm) {
			VALIDATE(group_state.values.size() == 5, null_str);

			std::vector<double> values = group_state.values;
			double joint2 = values[1];

			double first_joint2 = joint2 > 0? 0.0: joint2;
			values[1] = first_joint2;
			ros_instance_.do_light_joint_value_target(group_state.name, values);

			if (joint2 > 0) {
				values[1] = joint2;
				ros_instance_.do_light_joint_value_target(group_state.name, values);
			}

		} else {
			ros_instance_.do_light_joint_value_target(group_state.name, group_state.values);
		}
	}

	did_joints_value_changed(*joint_list_, true);


	// ros_instance_.do_soymilk();
/*
	{
		geometry_msgs::Pose t_in;
        tf2::toMsg(tf2::Transform::getIdentity(), t_in);

		geometry_msgs::Pose t_out;


		const std::string target_frame = "link1"; // base_link, arm_Link
		const std::string source_frame = "link5"; // link5
		geometry_msgs::TransformStamped transform51;
		bool ret = ros_instance_.get_source_2_target_tf(source_frame, target_frame, transform51);

		double yaw, pitch, roll;

        tf2::doTransform(t_in, t_out, transform51);
		tf2::getEulerYPR(t_out.orientation, yaw, pitch, roll);

		double roll1, pitch1, yaw1;
		tf2::getEulerYPR(transform51.transform.rotation, yaw1, pitch1, roll1);

		SDL_Log("%s transform51: {p:(%.6f, %.6f, %.6f) M:(%.6f(%.5f), %.6f(%.5f), %.6f(%.5f))} {M1:(%.6f(%.5f), %.6f(%.5f), %.6f(%.5f))}",
			ret? "success": "fail", transform51.transform.translation.x, transform51.transform.translation.y, transform51.transform.translation.z,
			roll, RAD2DEG(roll), pitch, RAD2DEG(pitch), yaw, RAD2DEG(yaw),
			roll1, RAD2DEG(roll1), pitch1, RAD2DEG(pitch1), yaw1, RAD2DEG(yaw1));

		return;
	}
*/

}

void tmoveit::app_timer_handler(uint32_t now)
{
	refresh_statusbar_grid(now);
	set_rpy_label();
}

} // namespace gui2

