/* Require Rose v1.0.19 or above. $ */

#define GETTEXT_DOMAIN "kdesktop-lib"

// #define _CONTAINER_DEBUG_LEVEL
// #define _ITERATOR_DEBUG_LEVEL	1


#include "base_instance.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/dialogs/home.hpp"
#include "gui/dialogs/scan.hpp"
#include "gui/dialogs/desktop.hpp"
#include "gui/dialogs/rexplorer.hpp"
#include "gui/dialogs/rdnn.hpp"
#include "gui/dialogs/rdcamera.hpp"
#include "gui/dialogs/klink.hpp"
#include "gui/widgets/window.hpp"
#include "hotkeys.hpp"
#include "health_controller.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "formula_string_utils.hpp"
#include "help.hpp"
#include "rose_version.hpp"
#include "game_config.hpp"
// #include "net.hpp"
#include "mediapipe/rose/mediapipe_api.hpp"

#include "health.hpp"
#include "drivers_core.hpp"
#include "base_driver_core.hpp"
#include "cfg_cpp_api_core.hpp"

#include "ble2.hpp"

#include "net.hpp"
// #include "mediapipe/rose/mediapipe_api.hpp"

namespace latex {
surface doc_to_surf(const std::string& formula, int max_width) { return nullptr; }
}

surface generate_qr(const std::string& text, int size, int margin, int eccLevel)
{
	VALIDATE(false, "this version doesn's support genrate_qr()");
	return nullptr;
}

namespace aplt {
class tbg_task2: public aplt::tbg_task::tbase_bg_task2
{
public:
	tbg_task2(std::map<aplt::taplt_key, aplt::tapplet>& applets, tdrivers_core& drivers, aplt::tcfg_cpp_api_core& cfg_cpp_api,
		aplt::tbg_task& bg_task, tcamera& camera, const tflite::tscript& def_script, const tflite::ttflite& def_tflite,
		tbase_driver_core& base_driver);
	~tbg_task2() {}

	void slice() {}
};

tbg_task2::tbg_task2(std::map<aplt::taplt_key, aplt::tapplet>& applets, tdrivers_core& drivers, aplt::tcfg_cpp_api_core& cfg_cpp_api,
		aplt::tbg_task& bg_task, tcamera& camera, const tflite::tscript& def_script, const tflite::ttflite& def_tflite,
		tbase_driver_core& base_driver)
	: tbg_task::tbase_bg_task2(bg_task)
{
}

}

class tfull_screen_landscape_lock
{
public:
	tfull_screen_landscape_lock()
	{
		VALIDATE(!game_config::fullscreen, null_str);
		instance->set_fullscreen(true);  // for debug
		if (true) {
			VALIDATE(!gui2::twidget::current_landscape, null_str);
			instance->set_orientation(base_instance::orientation_landscape, true);
		} else {
			// instance->set_mode();
		}
	}

	~tfull_screen_landscape_lock()
	{
		VALIDATE(game_config::fullscreen, null_str);
		instance->set_fullscreen(false);   // for debug
		if (true) {
			VALIDATE(gui2::twidget::current_landscape, null_str);
			instance->set_orientation(base_instance::orientation_portrait, true);
		} else {
			// instance->set_mode();
		}
	}
};

class tstore_slot: public gui2::trstore::tslot
{
public:
	tstore_slot(std::map<aplt::taplt_key, aplt::tapplet>& applets, aplt::tbg_task& bg_task, aplt::tcfg_cpp_api_core& cfg_cpp_api, tdrivers_core& drivers,
		tbase_driver_core& base_driver, const std::map<std::string, aplt::twkocourse_enroll>& wkocourse_enrolls, std::map<std::string, aplt::twkocourse>& wkocourses)
		: gui2::trstore::tslot(applets, bg_task, cfg_cpp_api, drivers, base_driver)
		, wkocourse_enrolls_(wkocourse_enrolls)
		, wkocourses_(wkocourses)
	{
		recommend_aplts_.push_back("aplt.leagor.basiclua");
		recommend_aplts_.push_back("aplt.leagor.khomelua");
		// recommend_aplts_.push_back("aplt.leagor.blesmart");
		// recommend_aplts_.push_back("aplt.leagor.iaccess");
	}

private:
	void rstore_did_applet_installed(const aplt::tapplet& applet) override
	{
		wkocourse_enrolls_to_courses(wkocourse_enrolls_, wkocourses_);
	}
	void rstore_did_applet_uninstalled(int source, const std::string& bundleid) override
	{
		wkocourse_enrolls_to_courses(wkocourse_enrolls_, wkocourses_);
	}

private:
	const std::map<std::string, aplt::twkocourse_enroll>& wkocourse_enrolls_;
	std::map<std::string, aplt::twkocourse>& wkocourses_;
};


class game_instance: public base_instance, /*public gui2::trstore::tslot,*/
	public gui2::trdnn::tslot, public aplt::tb_api, public aplt::tfunction, public aplt::tnlp_4_aiagent
{
public:
	game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv);

	int sdl_field_small_font_size() const { return sdl_field_small_font_size_; }

	tble2& ble() { return ble_; }
	tpbremotes& pbremotes() { return pbremotes_; }

	tdrivers_core& drivers() { return drivers_; }
	tbase_driver_core& base_driver() { return base_driver_; }
	tprivacy& privacy() { return privacy_; }
	aplt::tcfg_cpp_api_core& cfg_cpp_api() { return cfg_cpp_api_; }

	tstore_slot& store_slot() { return store_slot_; }
	aplt::thealth& health() { return health_; }
	tvlog_cfg& vlog_cfg() { return vlog_cfg_; }

	// std::map<std::string, aplt::twkocourse_enroll>& wkocourse_enrolls() { return wkocourse_enrolls_; }
	std::map<std::string, aplt::twkocourse>& wkocourses() { return wkocourses_; }

	void start_health_controller();
	void makesure_deviceid();
	// bool will_enter_landscape_module(const std::string& reason);

private:
	void app_load_settings_config(const config& cfg) override;
	void app_pre_setmode(tpre_setmode_settings& settings) override;
	void app_load_pb() override;
	std::pair<std::string, ::google::protobuf::MessageLite*> app_pblite_from_type(int type) override;
	void app_post_lua() override;
	void app_aplt_slice() override;

	void validate_driver_slot(int type, bool is_nullptr) const;
	bool refresh_driver_slot(int type, const aplt::tapplet* aplt, bool quiet);
	void did_driver_refreshed(const std::map<int, aplt::tapplet*>& type_applets, bool quiet, bool& require_start_base_node);
	void app_uninitialize() override;

	int sdl_GetTtyUSB(SDL_ttyUSB** ppttyUSB) override;
	int get_serial_path(int driver_type, std::string& path, std::string& model) override;

	void purchase_wkocourse(const std::string& aplt, const std::string& id) override;

	// aplt::tb_api
	std::string request_task(const aplt::treq_task& req_task, const aplt::ttask_vars& task_vars,
		const std::function<void (const aplt::ttask_vars& task_vars)>& did_task_finished) override
	{
		VALIDATE(false, null_str);
		return null_str;
	}
	std::string request_single_task(const aplt::treq_task& req_task) override
	{
		VALIDATE(false, null_str);
		return null_str;
	}

	void set_task_finished() override { VALIDATE(false, null_str); } 
	void set_allow_short_voice(bool val) override { VALIDATE(false, null_str); } 

	const std::map<aplt::taplt_task_key, aplt::taplt_task>& iot_tasks() const override {
		VALIDATE_IN_MAIN_THREAD();
		return bg_task_.iot_tasks();
	}
	const std::map<aplt::taplt_task_key, aplt::taplt_task>& timed_tasks() const override {
		VALIDATE_IN_MAIN_THREAD();
		return bg_task_.timed_tasks();
	}
	const std::map<aplt::tiot_device_key, aplt::tiot_device>& iot_devices() const override {
		VALIDATE_IN_MAIN_THREAD();
		return bg_task_.iot_devices();
	}
	const std::map<aplt::taplt_key, aplt::tapplet>& const_applets() const override { return applets_; }

	aplt::tnlp_4_aiagent& nlp_4_aiagent() override { return *this; }
	bool add_timed_task(const std::map<int64_t, tadd_timed_task>& tasks)
	{
		VALIDATE_IN_MAIN_THREAD();
		return false;
	}

	// void set_dcamera_points(const SDL_FPoint3* points, int size, const tdepth_sector_result& result) override;
	// void laser_publish_scan(const sensor_msgs::LaserScan& msg) override;
	lua_State* get_lua_State() const override;
	void call_lua_breakpoint() override;
	void aplt_add_msg_log(int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent) override;
	bool navigation_node_started() const override
	{
		VALIDATE_IN_MAIN_THREAD();
		return false;
	}
	// void public_vel(double linear_x, double linear_y, double angular_z) override;
	// const trpy& get_imu_rpy() const override { return ros_instance_.get_imu_rpy(); }
	void aplt_set_env_var(int type, const config::attribute_value& val) override;
	const aplt::ttask_var* aplt_get_env_var(int type) override
	{
		return aplt::get_env_var(aplt::builtin_var(type).id);
	}

	void set_privacy_protect(bool enable) override
	{
		VALIDATE(false, null_str);
/*
		if (enable != privacy_.protect()) {
			privacy_.set_protect(enable);
			preferences::set_privacy_protect(privacy_.protect());
		}
*/
	}
	bool is_privacy_protecting() const override
	{
		return privacy_.protect();
	}

	const std::vector<aplt::tbase_scene>& aplt_base_scenes() const override
	{
		return cfg_cpp_api_.base_scenes();
	}

	std::pair<const aplt::tbase_scene*, int> aplt_curr_base_scene() const override
	{
		VALIDATE_IN_MAIN_THREAD();
		std::string id = base_driver_.scene_id();
		if (id.empty()) {
			return std::make_pair(nullptr, nposm);
		}
		return std::make_pair(cfg_cpp_api_.base_scene_from_id(id, true), base_driver_.subtask_state());
	}

	const aplt::tbase_scene* aplt_set_base_scene(const aplt::tbase_scene& scene, int to_state, std::string& err_msg) override;

	// listen state
	bool is_listening() const override 
	{ 
		VALIDATE_IN_MAIN_THREAD();
		return false;
	}
	bool is_listen_speaking() const override 
	{ 
		VALIDATE_IN_MAIN_THREAD();
		return false;
	}
	bool start_listen() override 
	{
		VALIDATE_IN_MAIN_THREAD();
		return false;
	}
	void stop_listen() override 
	{
		VALIDATE_IN_MAIN_THREAD();
	}
	void listen_next_course() override 
	{ 
		VALIDATE_IN_MAIN_THREAD();
	}

	void push_floating_window_task(const std::string& msg, int duration_ms) override
	{
		VALIDATE_IN_MAIN_THREAD();
	}

	void health_push_n32_event(int type, int ctx) override;
	void health_push_str_event(int type, int ctx, const std::string& str, const std::string& aux_str, const std::string& aux_str2, int aux_int) override;
	void health_push_landmarks(const SDL_U16Point* landmarks, int unsatisfied_reason) override;
	void health_workout_finished(const std::string& aplt, const std::string& id) override;

	bool cswamp_addevent(int64_t ts, const std::string& desc, const std::vector<timage_pair>& images, bool quiet) override
	{
		VALIDATE_IN_MAIN_THREAD();
		return false;
	}
	bool cswamp_querytablecooking(int table, net::tcswamp_table_result& result, bool quiet) override
	{
		VALIDATE_IN_MAIN_THREAD();
		return false;
	}

	// tnlp_4_aiagent
	bool is_nlp_questioning() const override
	{
		VALIDATE_IN_MAIN_THREAD();
		return false;
	}
	void send_nlp_question_4_aiagent(bool new_conversation, const std::string& question, const surface& surf,
		const std::function<void(bool retbool, const std::string& answer, int input_tokens, int output_tokens)>& did_nlp_answer) override
	{
		VALIDATE_IN_MAIN_THREAD();
	}
	void stop_nlp_question() override
	{
		VALIDATE_IN_MAIN_THREAD();
	}

	// aplt::tfunction
	bool calculate2(const aplt::ttask_vars& task_vars, int func_code, const std::vector<std::string>& inputs, tresult& result) override;
	// std::string func_2_var_exp(const aplt::tfunction_code& func, const std::vector<std::string>& params, bool browser) const;


private:
	int sdl_field_small_font_size_;
	tble2 ble_;
	tpbremotes pbremotes_;

	tdrivers_core drivers_;
	tros_base_node_core base_node_;

	tbase_driver_core base_driver_;
	tprivacy privacy_;
	aplt::tcfg_cpp_api_core cfg_cpp_api_;

	aplt::tbg_task2 bg_task2_;
	tstore_slot store_slot_;

	std::unique_ptr<tpreempt_base_subtask_lock> preempt_base_subtask_lock_;
	SDL_OsInfo os_info_;

	aplt::thealth health_;
	tvlog_cfg vlog_cfg_;

	// std::map<std::string, aplt::twkocourse_enroll> wkocourse_enrolls_;
	std::map<std::string, aplt::twkocourse> wkocourses_;
};

game_instance::game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv)
	: base_instance(ss, argc, argv)
	, sdl_field_small_font_size_(nposm)
	, ble_(this)
	, drivers_(*this, applets_, base_driver_, os_info_, std::bind(&game_instance::did_driver_refreshed, this, _1, _2, _3))
	, base_driver_(applets_, base_node_, camera_, bg_task_, cfg_cpp_api_, health_, drivers_, privacy_)
	, privacy_(base_driver_)
	, cfg_cpp_api_(applets_, bg_task_, base_driver_)
	, bg_task2_(applets_, drivers_, cfg_cpp_api_, bg_task_, camera_, def_script_, def_tflite_, base_driver_)
	, store_slot_(applets_, bg_task_, cfg_cpp_api_, drivers_, base_driver_, wkocourse_enrolls_, wkocourses_)
{
	// To avoid accidents, always enable them every time.
	preferences::set_sound(true);
	preferences::set_music(true);

	// mediapipe::rose_set_create_pose_tracking_api(mediapipe::create_pose_tracking_api);
/*
	recommend_aplts_.push_back("aplt.leagor.basiclua");
	recommend_aplts_.push_back("aplt.leagor.khomelua");
	recommend_aplts_.push_back("aplt.leagor.blesmart");
	recommend_aplts_.push_back("aplt.leagor.iaccess");
*/
	bg_task_.set_bg_task2(bg_task2_);
	memset(&os_info_, 0, sizeof(os_info_));

	net::rose_set_create_http_api(net::chromium_create_http_api);
	mediapipe::rose_set_create_pose_tracking_api(mediapipe::create_pose_tracking_api);

	rpy_sensor_.init();
}

void game_instance::app_uninitialize()
{
	VALIDATE(preempt_base_subtask_lock_.get() == nullptr, null_str);

	const library& fgaplt_lib = drivers_.find_by_type(apltsotype_fgaplt);
	VALIDATE(fgaplt_lib.get() == nullptr, null_str);
/*
	if (cfg_cpp_api_.has_taskpoint()) {
		cfg_cpp_api_.clear_taskpoint();
	}
*/
	base_driver_.set_slot(null_str, nullptr);
	const library& base2th_lib = drivers_.find_by_type(apltsotype_base2th);
	VALIDATE(base2th_lib.get() == nullptr, null_str);
	const library& aiagent_task_lib = drivers_.find_by_type(apltsotype_aiagent_task);
	VALIDATE(aiagent_task_lib.get() == nullptr, null_str);
/*
	moveit_driver_.set_slot(null_str, nullptr);
	laser_driver_.set_slot(null_str, nullptr);
	dcamera_driver_.set_slot(null_str, nullptr);
	iot_driver_.set_slot(null_str, nullptr);
	// makesure stop speech before free tdrivers.
	speech_driver_.set_slot(null_str, nullptr);
	ai_driver_.set_slot(null_str, nullptr);
*/
	// has 'lib' use lua_, release 'lib' before 'delete lua_ ' in ~base_instance().
	drivers_.libs.clear();
}

void game_instance::app_load_settings_config(const config& cfg)
{
	bool disable_startup_scene = true;
	if (disable_startup_scene) {
		preferences::set_base_scene_id(null_str);
	}

	VALIDATE(screenmode_min != screenmode_ratio, "Switch mode in rdp must not screenmode_ratio be min screen mode");
	game_config::screen_modes.insert(std::make_pair(screenmode_scale, _("Scale to screen")));
	game_config::screen_modes.insert(std::make_pair(screenmode_ratio, _("Keep aspect ratio")));
	game_config::screen_modes.insert(std::make_pair(screenmode_partial, _("Partial")));
	VALIDATE(game_config::screen_modes.size() == screenmode_count, null_str);

	game_config::version = version_info(cfg["version"].str());
	VALIDATE(game_config::version.is_rose_recommended(), null_str);

	vlog_cfg_.read_pref();
}

void game_instance::app_pre_setmode(tpre_setmode_settings& settings)
{	
	settings.pc_default_font_size = 15;
	settings.fullscreen = false;
	// settings.silent_background = false;
	settings.landscape = false;
	if (game_config::os == os_windows) {
		// settings.landscape = true;

		// health_controller require it > 610
		// wko_setting require it > 680
		settings.min_width = 690; 
		settings.min_height = 449; // 450
	}
}

void game_instance::app_load_pb()
{
	load_action_tpl2s_cfg();
	sdl_field_small_font_size_ = game_config::os == os_windows? font::SIZE_SMALLER: font::SIZE_SMALLEST;
	wkocourse_enrolls_from_pref(wkocourse_enrolls_, &wkocourses_);

	load_logs_pb(LOGS_PB, LOGS_PB_MAX_DAYS, LOGS_PB_MAX_LOGS);
	bg_task_.app_load_pb(KLINK_PB);
	cfg_cpp_api_.load();

	pbremotes_.version = 1;
	pbremotes_.timestamp = time(nullptr);

	std::set<tpbgroup>& groups = pbremotes_.groups;

	std::set<tpbdevice> devices;
	devices.insert(tpbdevice(SDL_CreateGUID(SDL_TRUE), "first_a", 0, 0));
	devices.insert(tpbdevice(SDL_CreateGUID(SDL_TRUE), "second_b", 0, 0));
	std::pair<std::set<tpbgroup>::iterator, bool> ins = groups.insert(tpbgroup(SDL_CreateGUID(SDL_TRUE), "first", devices));
	VALIDATE(ins.second, null_str);

	devices.clear();
	devices.insert(tpbdevice(SDL_CreateGUID(SDL_TRUE), "first_1", 0, 0));
	devices.insert(tpbdevice(SDL_CreateGUID(SDL_TRUE), "second_2", 0, 0));
	ins = groups.insert(tpbgroup(SDL_CreateGUID(SDL_TRUE), "second", devices));
	VALIDATE(ins.second, null_str);

	devices.clear();
	devices.insert(tpbdevice(SDL_CreateGUID(SDL_TRUE), "first_A", 0, 0));
	devices.insert(tpbdevice(SDL_CreateGUID(SDL_TRUE), "second_B", 0, 0));
	ins = groups.insert(tpbgroup(SDL_CreateGUID(SDL_TRUE), "third", devices));
	VALIDATE(ins.second, null_str);
	
		
/*
	const std::string src = game_config::app_dir_root + "/tflites";
	faceprint::copy_model_2_preferences(src);

	protobuf::load_sha1pb(EVENTS_PB, true);
	const int events_pb_version = 1;
	if (pb_events_.version() != events_pb_version || pb_events_.events_size() == 0) {
		pb_events_.set_version(events_pb_version);
		pb_events_.set_next_image_index(0);
	}

	protobuf::load_sha1pb(GAUTH_ACTIONS_PB, true);
	const int gauth_actions_pb_version = 1;
	if (pb_gauth_actions_.version() != gauth_actions_pb_version || pb_gauth_actions_.actions_size() == 0) {
		pb_gauth_actions_.set_version(gauth_actions_pb_version);
	}
*/

}

std::pair<std::string, ::google::protobuf::MessageLite*> game_instance::app_pblite_from_type(int type)
{
	std::string file_name;
	::google::protobuf::MessageLite* lite = nullptr;
	if (type == LOGS_PB) {
		lite = &logs_pb_;
		file_name = "logs";

	} else if (type == KLINK_PB) {
		lite = &bg_task_.pb_klink();
		file_name = "klink"; // 

	} else {
		VALIDATE(false, null_str);
	}
	return std::make_pair(game_config::preferences_dir + "/" + file_name + ".pb", lite);

}

void game_instance::app_post_lua()
{
	// lua::register_ros_metatable(lua_->get_state(), *this);

	drivers_.init(applets_);
}

void game_instance::app_aplt_slice()
{
	base_driver_.slice();
/*
	if (!kidnap.has_imu()) {
		if (base_driver_.imu_can_read()) {
			kidnap.set_has_imu(true);
		}

	} else if (!base_driver_.imu_can_read()) {
		kidnap.set_has_imu(false);
	}

	moveit_driver_.slice();

	speech_driver_.slice();

	iot_driver_.slice();

	ai_driver_.slice();

	if (req_task_dirty_) {
		// see request_task()
		VALIDATE(false, null_str);
		// main_thread_set_req_task(req_task_, req_task_task_vars_);
	}

	countdown_klink_aplt_task_.slice();
*/
	bg_task2_.slice();
/*
	cfg_cpp_api_.add_timed_task_sliced();

	if (moveit_aplt_task_.get() != nullptr) {
		moveit_aplt_task_->slice();
	}

	if (fg_aplt() == nullptr && !bg_task_.is_ing() && cfg_cpp_api_.has_taskpoint() && !aplt::new_klink_task_disabled2(nullptr)) {
		const aplt::ttaskpoint& taskpoint = cfg_cpp_api_.taskpoint();
		const aplt::taplt_task& klink_aplt_task = bg_task_.task_from_pb_at(taskpoint.task_cpp_type, taskpoint.task_cpp_pb_at);
		bg_task_.request_klink_aplt_task(klink_aplt_task, taskpoint.task_vars);
	}
*/
	health_.slice();
/*
#ifdef _WIN32
	tray_.slice();
#endif
*/
}

void game_instance::start_health_controller()
{
	tfull_screen_landscape_lock lock;

	int initial_zoom = 64; // 64

	trhealth_scene_slot scene_slot;

	hotkey::scope_changer changer(core_cfg(), "hotkey_health");
	health_controller chart(scene_slot, core_cfg(), video_, health_, wkocourse_enrolls_, 
		wkocourses_, sdl_field_small_font_size_);
	chart.initialize(initial_zoom);
	chart.main_loop();
}

void game_instance::makesure_deviceid()
{
	current_user.deviceid = preferences::login_deviceid();
	if (!utils::is_uuid(current_user.deviceid, false)) {
		current_user.deviceid = utils::create_uuid(false);
		preferences::set_login_deviceid(current_user.deviceid);
	}

	current_user.username = preferences::login_username();
	current_user.pwcookie = preferences::login_pwcookie();
}

bool will_enter_landscape_module(tbase_driver_core& base_driver, const std::string& module)
{
	VALIDATE(!module.empty(), null_str);
	if (base_driver.subtask_state() != aplt::sts_nposm) {
		utils::string_map symbols;
		symbols["module"] = module;
		std::string warnning;
		// {
			warnning = vgettext2("A base task is running. Do you want to stop the task and enter '$module'?", symbols);
		// }
		const std::string log = vgettext2("To enter $module, stop the base task", symbols);
		if (!base_driver.stop_subtask_if_runing(warnning, log)) {
			return false;
		}
	}

	// driver_base2th maybe use camera.
	// VALIDATE(!camera_.tasking(), null_str);

	return true;
}

void game_instance::validate_driver_slot(int type, bool is_nullptr) const
{
	if (type == apltsotype_base) {
		if (is_nullptr) {
			VALIDATE(base_driver_.slot == nullptr, null_str);
		} else {
			VALIDATE(base_driver_.slot != nullptr, null_str);
		}

	} /* else if (type == apltsotype_moveit) {
		if (is_nullptr) {
			VALIDATE(moveit_driver_.slot == nullptr, null_str);
		} else {
			VALIDATE(moveit_driver_.slot != nullptr, null_str);
		}

	} else if (type == apltsotype_laser) {
		if (is_nullptr) {
			VALIDATE(laser_driver_.slot == nullptr, null_str);
		} else {
			VALIDATE(laser_driver_.slot != nullptr, null_str);
		}

	} else if (type == apltsotype_dcamera) {
		if (is_nullptr) {
			VALIDATE(dcamera_driver_.slot == nullptr, null_str);
		} else {
			VALIDATE(dcamera_driver_.slot != nullptr, null_str);
		}

	} else if (type == apltsotype_iot) {
		if (is_nullptr) {
			VALIDATE(iot_driver_.slot == nullptr, null_str);
		} else {
			VALIDATE(iot_driver_.slot != nullptr, null_str);
		}

	} else if (type == apltsotype_speech) {
		if (is_nullptr) {
			VALIDATE(speech_driver_.slot == nullptr, null_str);
		} else {
			VALIDATE(speech_driver_.slot != nullptr, null_str);
		}

	} else if (type == apltsotype_ai) {
		if (is_nullptr) {
			VALIDATE(ai_driver_.slot == nullptr, null_str);
		} else {
			VALIDATE(ai_driver_.slot != nullptr, null_str);
		}

	} */ else {
		VALIDATE(false, null_str);
	}
}

bool game_instance::refresh_driver_slot(int type, const aplt::tapplet* aplt, bool quiet)
{
	bool require_start_base_node = false;

	std::string curr_aplt_id;
	if (type == apltsotype_base) {
		curr_aplt_id = base_driver_.aplt_id();

	} /* else if (type == apltsotype_moveit) {
		curr_aplt_id = moveit_driver_.aplt_id();

	} else if (type == apltsotype_laser) {
		curr_aplt_id = laser_driver_.aplt_id();

	} else if (type == apltsotype_dcamera) {
		curr_aplt_id = dcamera_driver_.aplt_id();

	} else if (type == apltsotype_iot) {
		curr_aplt_id = iot_driver_.aplt_id();

	} else if (type == apltsotype_speech) {
		curr_aplt_id = speech_driver_.aplt_id();

	} else if (type == apltsotype_ai) {
		curr_aplt_id = ai_driver_.aplt_id();

	} */ else {
		VALIDATE(false, null_str);
	}

	if (aplt == nullptr) {
		if (type == apltsotype_base) {
			base_driver_.set_slot(null_str, nullptr);

		} /* else if (type == apltsotype_moveit) {
			moveit_driver_.set_slot(null_str, nullptr);

		} else if (type == apltsotype_laser) {
			laser_driver_.set_slot(null_str, nullptr);

		} else if (type == apltsotype_dcamera) {
			dcamera_driver_.set_slot(null_str, nullptr);

		} else if (type == apltsotype_iot) {
			iot_driver_.set_slot(null_str, nullptr);

		} else if (type == apltsotype_speech) {
			speech_driver_.set_slot(null_str, nullptr);

		} else if (type == apltsotype_ai) {
			ai_driver_.set_slot(null_str, nullptr);
		} */

	} else if (curr_aplt_id != aplt->id) {
		void* slot = nullptr;
		if (type == apltsotype_base) {
			slot = drivers_.create_base_slot();
			SDL_Log("{dbg-kdesktop}refresh_driver_slot, curr_aplt_id != aplt->id, slot: %p", slot);

		} /* else if (type == apltsotype_moveit) {
			slot = drivers_.create_moveit_slot();

		} else if (type == apltsotype_laser) {
			slot = drivers_.create_laser_slot();

		} else if (type == apltsotype_dcamera) {
			slot = drivers_.create_dcamera_slot();

		} else if (type == apltsotype_iot) {
			aplt::tslot_subscriber* subscriber = this;
			slot = drivers_.create_iot_slot(*subscriber);

		} else if (type == apltsotype_speech) {
			aplt::tslot_subscriber* subscriber = this;
			slot = drivers_.create_speech_slot(*subscriber);

		} else if (type == apltsotype_ai) {
			aplt::tslot_subscriber* subscriber = this;
			slot = drivers_.create_ai_slot(*subscriber);
		} */

		if (slot != nullptr) {
			if (type == apltsotype_base) {
				base_driver_.set_slot(aplt->id, reinterpret_cast<aplt::tbase_slot*>(slot));

				require_start_base_node = true;
				// aplt::tbase_slot* slot2 = aplt::get_base_slot();
				// SDL_Log("refresh_driver_slot, slot: %p =><= slot2: %p", slot, slot2);
				// base_driver_.set_slot(aplt->id, slot2);
/*
				tdrivers::tvars vars = drivers_.curvars(true);
				if (vars.base.valid(true)) {
					base_driver_.start_node(vars.base.dev, vars.base.baudrate, vars.base.model);
				}
*/

			} /* else if (type == apltsotype_moveit) {
				moveit_driver_.set_slot(aplt->id, reinterpret_cast<aplt::tmoveit_slot*>(slot));

			} else if (type == apltsotype_laser) {
				laser_driver_.set_slot(aplt->id, reinterpret_cast<aplt::tlaser_slot*>(slot));

			} else if (type == apltsotype_dcamera) {
				dcamera_driver_.set_slot(aplt->id, reinterpret_cast<aplt::tdcamera_slot*>(slot));

			} else if (type == apltsotype_iot) {
				iot_driver_.set_slot(aplt->id, reinterpret_cast<aplt::tiot_slot*>(slot));

			} else if (type == apltsotype_speech) {
				speech_driver_.set_slot(aplt->id, reinterpret_cast<aplt::tspeech_slot*>(slot));

			} else if (type == apltsotype_ai) {
				ai_driver_.set_slot(aplt->id, reinterpret_cast<aplt::tai_slot*>(slot));
			} */
		} else {
			if (!quiet) {
				utils::string_map symbols;
				symbols["driver"] = aplt::aplt_drivers.find(type)->second.name;
				symbols["center"] = aplt::all_fake_applets.find(aplt::builtinid_center)->second.name;
				const std::string err = vgettext2("$driver failed to create slot. Navigate to '$center' to check the cause.", symbols);
				gui2::show_message(null_str, err);
			}	
			drivers_.clear_by_type(type);
			validate_driver_slot(type, true);
			// drivers_.refresh(quiet);
		}
	}

	return require_start_base_node;
}

void game_instance::did_driver_refreshed(const std::map<int, aplt::tapplet*>& type_applets, bool quiet, bool& require_start_base_node)
{
	aplt::tapplet* base_aplt = nullptr;
/*
	const aplt::tapplet* moveit_aplt = nullptr;
	const aplt::tapplet* laser_aplt = nullptr;
	const aplt::tapplet* dcamera_aplt = nullptr;
	const aplt::tapplet* iot_aplt = nullptr;
	const aplt::tapplet* speech_aplt = nullptr;
	const aplt::tapplet* aiagent_aplt = nullptr;
*/
	for (std::map<int, aplt::tapplet*>::const_iterator it = type_applets.begin(); it != type_applets.end(); ++ it) {
		int type = it->first;
		aplt::tapplet& aplt = *it->second;
		if (type == apltsotype_base) {
			base_aplt = &aplt;

		} /* else if (type == apltsotype_moveit) {
			moveit_aplt = &aplt;

		} else if (type == apltsotype_laser) {
			laser_aplt = &aplt;

		} else if (type == apltsotype_dcamera) {
			dcamera_aplt = &aplt;

		} else if (type == apltsotype_iot) {
			iot_aplt = &aplt;

		} else if (type == apltsotype_speech) {
			speech_aplt = &aplt;

		} else if (type == apltsotype_ai) {
			aiagent_aplt = &aplt;
		} */
	}

	require_start_base_node = refresh_driver_slot(apltsotype_base, base_aplt, quiet);
/*
	refresh_driver_slot(apltsotype_moveit, moveit_aplt, quiet);
	refresh_driver_slot(apltsotype_laser, laser_aplt, quiet);
	refresh_driver_slot(apltsotype_dcamera, dcamera_aplt, quiet);
	refresh_driver_slot(apltsotype_iot, iot_aplt, quiet);
	refresh_driver_slot(apltsotype_speech, speech_aplt, quiet);
	refresh_driver_slot(apltsotype_ai, aiagent_aplt, quiet);
*/
}

int game_instance::sdl_GetTtyUSB(SDL_ttyUSB** ppttyUSB)
{
	int fix_count = 0;
	const char (*fix_dev_nodes)[48] = nullptr;

	char fix[40][48];
	fix_dev_nodes = fix;

	if (game_config::os == os_windows) {
		fix_count = sizeof(fix) / sizeof(fix[0]);
		for (int n = 0; n < fix_count; n ++) {
			SDL_snprintf(fix[n], sizeof(fix[n]), "COM%i", n + 1);
		}

	} else if (game_config::os == os_android) {
		SDL_strlcpy(fix[0], "/dev/ttyS0", sizeof(fix[0]));
		fix_count = 1;

	} else {
		VALIDATE(game_config::os == os_ios, null_str);
		SDL_strlcpy(fix[0], "/dev/ttyS0", sizeof(fix[0]));

		fix_count = 1;
	}

	return SDL_GetTtyUSB(fix_dev_nodes, fix_count, ppttyUSB);
}

int game_instance::get_serial_path(int driver_type, std::string& path, std::string& model)
{
	SDL_Log("{dbg-kdesktop}get_serial_path, driver_type: %i, path: %s, model: %s, base_driver_.slot: %p", driver_type, path.c_str(), model.c_str(), base_driver_.slot);
    if (driver_type == apltsotype_base) {
		if (base_driver_.slot != nullptr) {
			return base_driver_.slot->get_serial_path(path, model);
		}

    } /* else if (driver_type == apltsotype_laser) {
        if (laser_driver_.slot != nullptr) {
			return laser_driver_.slot->get_serial_path(path);
		}

    } else if (driver_type == apltsotype_moveit) {
        if (moveit_driver_.slot != nullptr) {
			return moveit_driver_.slot->get_serial_path(path);
		}

    } */ else {
        VALIDATE(false, null_str);
    }

	path.clear();
    return nposm;
}

void game_instance::purchase_wkocourse(const std::string& aplt, const std::string& id)
{
	std::string id2 = utils::join_app_prefix_id(aplt, id);
	// std::map<std::string, aplt::twkocourse_enroll>& enrolls = instance->wkocourse_enrolls();
	std::map<std::string, aplt::twkocourse_enroll>& enrolls = wkocourse_enrolls_;
	VALIDATE(enrolls.count(id2) == 0, null_str);

	// 1/3: update enroll
	std::pair<std::map<std::string, aplt::twkocourse_enroll>::iterator, bool> ins = enrolls.insert(std::make_pair(id2, aplt::twkocourse_enroll()));
	aplt::twkocourse_enroll& enroll = ins.first->second;
	enroll.do_purchase(aplt, id);

	// 2/3: update wkocourses in pref.
	wkocourse_enrolls_to_pref(enrolls);

	// 3/3: update wkocourses
	VALIDATE(wkocourses_.count(id2) == 0, null_str);

	const aplt::tapplet* aplt2 = aplt::aplt_from_bundleid(applets_, enroll.aplt);
	VALIDATE(aplt2 != nullptr, null_str);

	std::pair<std::map<std::string, aplt::twkocourse>::iterator, bool> ins2 =
		wkocourses_.insert(std::make_pair(enroll.id2, aplt::twkocourse()));

	aplt::twkocourse& wkocourse = ins2.first->second;
	std::string err_msg = wkocourse.from_aplt_file(*aplt2, enroll.id_to_filename());
	VALIDATE(wkocourse.valid(), null_str);
/*
	if (!wkocourse.valid()) {
		wkocourses_.erase(ins.first);
	}
*/
}

lua_State* game_instance::get_lua_State() const
{
	return lua_->get_state();
}

void game_instance::call_lua_breakpoint()
{
	return lua_->call_lua_breakpoint();
}

void game_instance::aplt_add_msg_log(int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent)
{
	// must is called by one applet.
	if (fg_aplt_ != nullptr) {
		add_aplt_ts_msg_log(logtype_warn, null_str, ts, msg, tokens, aiagent);

	} else if (bg_task_.is_ing()) {
		bg_task_.add_log2(ts, msg, tokens, aiagent);

	} else {
		// for example: this aplt is used for base-subtask.
		add_aplt_ts_msg_log(logtype_warn, null_str, ts, msg, tokens, aiagent);
	}
}

void game_instance::aplt_set_env_var(int type, const config::attribute_value& val)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(var_type_is_BI_env(type), null_str);
	bg_task2_.set_task_var(aplt::builtin_var(type).id, false, val);
}

const aplt::tbase_scene* game_instance::aplt_set_base_scene(const aplt::tbase_scene& scene, int to_state, std::string& err_msg)
{
	VALIDATE(to_state == aplt::sts_ing || to_state == aplt::sts_idle, null_str);

	err_msg.clear();
	if (!base_driver_.node_started()) {
		err_msg = _("Base node not started");
		return nullptr;
	}
	if (base_driver_.slot->moveable()) {
		err_msg = _("Base is in moveable mode");
		return nullptr;
	}

	if (bg_task_.is_ing() && bg_task_.in_which_single_task() == aplt::task_camera) {
		err_msg = _("A background task using the camera is currently running");
		return nullptr;
	}
	// above 'if()' block can validate below statement.
	VALIDATE(base_driver_.subtask_state() != aplt::sts_preempted, null_str);

	const std::string desire_scene_id = scene.get_id();
	const aplt::tbase_scene* found_scene = cfg_cpp_api_.base_scene_from_id(desire_scene_id, true);
	VALIDATE(found_scene == &scene, null_str);

	tbase_driver_core::tallow_restart_subtask_when_bg_ing_lock lock(base_driver_);

	bool is_me = scene.get_id() == base_driver_.scene_id();
	if (is_me) {
		if (base_driver_.subtask_state() == aplt::sts_ing) {
			// this scene is ing
			if (to_state == aplt::sts_idle) {
				base_driver_.idle_or_preempt_subtask(true);
			}

		} else if (to_state == aplt::sts_ing) {
			base_driver_.resume_subtask();
		}

	} else {
		if (to_state == aplt::sts_idle) {
			err_msg = _("No scenes available to suspend");
			return nullptr;
		}
		preferences::set_base_scene_id(scene.get_id());
		base_driver_.restart_subtask();
	}
	
	const aplt::tbase_scene* result = nullptr;
	if (!base_driver_.scene_id().empty()) {
		VALIDATE(base_driver_.scene_id() == desire_scene_id, null_str);
		result = cfg_cpp_api_.base_scene_from_id(desire_scene_id, true);

	} else {
		err_msg = _("Unable to start scene");
	}
	return result;
}

void game_instance::health_push_n32_event(int type, int ctx)
{
	health_.health_push_n32_event(type, ctx);
}

void game_instance::health_push_str_event(int type, int ctx, const std::string& str, const std::string& aux_str, const std::string& aux_str2, int aux_int)
{
	health_.health_push_str_event(type, ctx, str, aux_str, aux_str2, aux_int);
}

void game_instance::health_push_landmarks(const SDL_U16Point* landmarks, int unsatisfied_reason)
{
	health_.health_push_landmarks(landmarks, unsatisfied_reason);
}

void game_instance::health_workout_finished(const std::string& aplt, const std::string& id)
{
	VALIDATE(is_bundleid(aplt), null_str);
	VALIDATE(!id.empty(), null_str);
	const std::string id2 = utils::join_app_prefix_id(aplt, id);

	// std::vector<aplt::twkocourse_enroll>* desire_enrolls;
	bool dirty = false;
	for (std::map<std::string, aplt::twkocourse_enroll>::iterator it = wkocourse_enrolls_.begin(); it != wkocourse_enrolls_.end(); ++ it) {
		aplt::twkocourse_enroll& enroll = it->second;
		if (enroll.active != nposm) {
			continue;
		}
		if (wkocourses_.count(enroll.id2) == 0) {
			continue;
		}
		const aplt::twkocourse& course = wkocourses_.find(enroll.id2)->second;

		std::map<std::string, int> workout_id2s;
		course.get_workout_id2s(aplt, workout_id2s);
		if (workout_id2s.count(id2) != 0) {
			enroll.do_active();
			dirty = true;
		}
	}

	if (dirty) {
		wkocourse_enrolls_to_pref(wkocourse_enrolls_);
	}

	// health_.health_workout_finished(aplt, id);
}

bool game_instance::calculate2(const aplt::ttask_vars& task_vars, int func_code, const std::vector<std::string>& params, tresult& result)
{
/*
	if (func_code == func_distance) {
		const std::string& uuid = params[0];

		double dist = float_nposm;
		result.val.from_double(dist);
		if (curmap_.positions.count(uuid) == 0) {
			return true;
		}
		if (!kidnap.last_pose2d.valid) {
			return true;
		}

		const tmap_position& position = curmap_.positions.find(uuid)->second;
		dist = hypot(kidnap.last_pose2d.x - position.x, kidnap.last_pose2d.y - position.y);

        result.val.from_double(dist);

	} else */ if (func_code == func_is_wko_task_finished) {
		bool retbool = false;
		if (base_driver_.subtask_state() == aplt::sts_ing) {
			retbool = base_driver_.camera_api().is_wko_task_finished();
		}
		result.val.from_bool(retbool);

	} else {
		return false;
	}

	return true;
}

void test()
{
	// const std::string filename = "C:/ddksample/apps-src/apps/external/boringssl/ssl/internal.h1";
	// file_is_all_ascii(filename, true);

	std::vector<lua_Integer> n8vals;
	n8vals.push_back(10);
	n8vals.push_back(-10);

	std::vector<lua_Integer> n32vals;
	n32vals.push_back(10);
	n32vals.push_back(-10);
	n32vals.push_back(655367);
	n32vals.push_back(-655367);
	n32vals.push_back(INT_MAX);
	n32vals.push_back(-INT_MAX);
	n32vals.push_back(INT64_C(2147483647) + 100);
	n32vals.push_back(-(INT64_C(2147483647) + 100));
	
	for (std::vector<lua_Integer>::const_iterator it = n32vals.begin(); it != n32vals.end(); ++ it) {
		lua_Integer n64 = (*it);
		int n32 = (int32_t)n64;
		SDL_Log("n64(%lld) --(int32_t)-->%d", n64, n32);

		int un32 = (uint32_t)n64;
		SDL_Log("n64(%lld) --(uint32_t)-->%d", n64, n32);
	}
}

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	SDL_SetHint(SDL_HINT_BLE, "1");

	rtc::PhysicalSocketServer ss;
	instance_manager<game_instance> manager(ss, argc, argv, "kdesktop", "#rose");
	game_instance& game = manager.get();

	game.makesure_deviceid();

	test();

	try {
		preferences::set_keyboard_style(keyboard_style_default);

		if (!preferences::login_username().empty() && !preferences::login_pwcookie().empty()) {
			const std::string username = preferences::login_username();
			const std::string cookie = preferences::login_pwcookie();

			net::cswamp_login2(current_user, net::cswamp_login_type_cookie, username, cookie, nposm, true);
		}

		int startup_layer = gui2::thome::RDP_LAYER;
		for (; ;) {
			game.loadscreen_manager().reset();
			const font::floating_label_context label_manager;
			cursor::set(cursor::NORMAL);

			int res;
			trdpcookie rdpcookie;
			{
				gui2::thome dlg(game.store_slot(), game.pbremotes(), game.ble(), game.applets(), game.base_driver(), game.drivers(), game.cfg_cpp_api(), 
					game.wkocourse_enrolls(), game.wkocourses(), game.health(), startup_layer);
				dlg.show();
				res = static_cast<gui2::thome::tresult>(dlg.get_retval());
				rdpcookie = dlg.rdpcookie();
				startup_layer = gui2::thome::RDP_LAYER;
			}

			if (res == gui2::thome::DESKTOP) {
				gui2::trexplorer::tentry extra(null_str, null_str, null_str);
				if (game_config::os == os_windows) {
					// extra = gui2::trexplorer::tentry(game_config::path + "/data/gui/default/scene", _("gui/scene"), "misc/dir_res.png");
				} else if (game_config::os == os_android) {
					extra = gui2::trexplorer::tentry("/sdcard", "/sdcard", "misc/dir_res.png");
				}

				VALIDATE(!game_config::fullscreen, null_str);
				instance->set_fullscreen(true);
				if (rdpcookie.landscape) {
					instance->set_orientation(base_instance::orientation_landscape, true);
				} else {
					instance->set_mode();
				}
				{
					// require first swip-up not result to enter background.
					thome_indicator_lock indicator_lock(2);
					gui2::tdesktop slot(rdpcookie);
					// since launcher has used unified rules for the all os, here according to it.
					gui2::trexplorer dlg(slot, null_str, extra, false, true); // game_config::mobile
					dlg.show();
					int res = dlg.get_retval();
					if (res != gui2::twindow::OK) {
						// continue;
					}
				}
				VALIDATE(game_config::fullscreen, null_str);
				instance->set_fullscreen(false);
				if (rdpcookie.landscape) {
					instance->set_orientation(base_instance::orientation_portrait, true);
				} else {
					instance->set_mode();
				}
			} else if (res >= gui2::thome::APPLET0) {
				const int at = res - gui2::thome::APPLET0;
				if (at < MAX_APPLETS) {
					const aplt::tapplet& applet = aplt::aplt_from_at(game.applets(), at);
					aplt::texecutor executor(game.lua(), applet);
					executor.run();

				} else if (at == aplt::builtinid_store) {
					gui2::trstore dlg(game.store_slot(), game.camera(), game.applets());
					dlg.show();

				} else if (at == aplt::builtinid_center) {
					gui2::tchat_slot slot;
					gui2::tchat2 dlg(slot, "chat_module");
					dlg.show();

				} else if (at == aplt::builtinid_klink) {
					gui2::tsimple_var_editor_slot slot;
					gui2::tklink dlg(slot, game.applets(), game.cfg_cpp_api(), game.bg_task(), game.base_driver());
					dlg.show();

				} else if (at == aplt::builtinid_health) {
					bool allow_start = true;
/*
					if (!game.will_enter_landscape_module(aplt::all_fake_applets.find(aplt::builtinid_health)->second.name))  {
						allow_start = false;
					}
*/
					if (allow_start) {
						game.start_health_controller();
					}

				} else if (at == aplt::builtinid_dcamera) {
					gui2::trdcamera::tslot slot;
					gui2::trdcamera dlg(slot, game.health(), game.applets(), game.base_driver(), game.wkocourse_enrolls(), game.wkocourses(), game.camera(), game.vlog_cfg(), game.sdl_field_small_font_size());
					dlg.show();

				} else if (at == aplt::builtinid_dnn) {
					gui2::trdnn dlg(game, game.camera(), game.app_cfg());
					dlg.show();
				}

				startup_layer = gui2::thome::APPLET_LAYER;

			}
		}

	} catch (twml_exception& e) {
		e.show();

	} catch (CVideo::quit&) {
		//just means the game should quit
		SDL_Log("SDL_main, catched CVideo::quit");

	} catch (game_logic::formula_error& e) {
		gui2::show_error_message(e.what());
	} 

	return 0;
}

int main(int argc, char** argv)
{
	try {
		do_gameloop(argc, argv);
	} catch (twml_exception& e) {
		// this exception is generated when create instance.
		e.show();
	}

	return 0;
}