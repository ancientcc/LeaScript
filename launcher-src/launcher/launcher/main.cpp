/* Require Rose v1.0.19 or above. $ */

#define GETTEXT_DOMAIN "launcher-lib"

#include "rdp_server_rose.h"
#include "base_instance.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/dialogs/home.hpp"
#include "gui/dialogs/explorer.hpp"
#include "gui/dialogs/store.hpp"
#include "gui/dialogs/settings.hpp"
#include "gui/dialogs/moveit.hpp"
#include "gui/dialogs/dcamera.hpp"
#include "gui/dialogs/klink.hpp"
#include "gui/dialogs/speech2.hpp"
#include "gui/dialogs/task2.hpp"
#include "gui/dialogs/courseware2.hpp"
#include "gui/dialogs/center.hpp"
#include "gui/dialogs/rdnn.hpp"
// #include "gui/dialogs/pose_state2.hpp"
#include "gui/dialogs/var_editor.hpp"
#include "gui/dialogs/mkcourse.hpp"
#include "gui/widgets/window.hpp"
#include "map_controller.hpp"
#include "mkscript_controller.hpp"
#include "health_controller.hpp"
#include "chart_controller.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "formula_string_utils.hpp"
#include "help.hpp"
#include "rose_version.hpp"
#include "hotkeys.hpp"
#include <kosapi/sys.h>
#include <kosapi/net.h>

#include "game_config.hpp"
#include "pble2.hpp"
#include "ResponseCode.h"
#include <ros/common.h>
#include <ros/init.h>
#include "aplt_common.hpp"
#include <rose_ros/utils.hpp>
#include <rose_ros/kidnap.hpp>
// #include "vros.hpp"
#include "protobuf.hpp"
#include "iot_driver.hpp"

#include "chinese.hpp"
#include "bg_task2.hpp"
#include "net.hpp"
#include "mediapipe/rose/mediapipe_api.hpp"

#include "game_latex.hpp"
#include "game_tray.hpp"
#include "health.hpp"


// using namespace std::placeholders;
extern uint32_t ReceivingInterfaceAddr;

extern bool is_valid_wkoscript_or_wkocoruse_path(bool wkoscript, const std::string& path);

std::string label_state_nposm;

class game_instance: public base_instance, public tinstance_slot,
	public gui2::trdnn::tslot, public aplt::tb_api, public aplt::tr_api, 
	public aplt::tslot_subscriber, public aplt::tfunction, public aplt::tnlp_4_aiagent
{
public:
	game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv);
	~game_instance();

	void did_receive_broadcast(int argc, const char** argv);
	void did_net_state_changed(const char* iface, bool connected, uint32_t ipv4);

	net::trdpd_manager& rdpd_mgr() { return rdpd_mgr_; }
	tpble2& pble() { return pble_; }
	tdrivers& drivers() { return drivers_; }
	trobot_imu& robot_imu() { return robot_imu_; }
	tros_instance& ros_instance() { return ros_instance_; }

	tros_map& curmap() { return curmap_; }
	const tros_map& curmap() const override { 
		VALIDATE_IN_MAIN_THREAD();
		return curmap_; 
	}
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
	bool goaling() const override { return ros_instance_.goaling(); }
	const std::map<aplt::taplt_key, aplt::tapplet>& const_applets() const override { return applets_; }

	tbase_driver& base_driver() { return base_driver_; }
	tmoveit_driver& moveit_driver() { return moveit_driver_; }
	tlaser_driver& laser_driver() { return laser_driver_; }
	tdcamera_driver& dcamera_driver() { return dcamera_driver_; }
	tiot_driver& iot_driver() { return iot_driver_; }
	tspeech_driver& speech_driver() { return speech_driver_; }
	tai_driver& ai_driver() { return ai_driver_; }

	tprivacy& privacy() { return privacy_; }
	aplt::tcfg_cpp_api& cfg_cpp_api() { return cfg_cpp_api_; }
	std::unique_ptr<tmoveit_aplt_task>& moveit_aplt_task() { return moveit_aplt_task_; }

	aplt::tbg_task2& temp_task() { return bg_task2_; }
	gui2::tstore_slot& store_slot() { return store_slot_; }

	const std::string& saves_courseware_dir() const { return saves_courseware_dir_; }
	const std::string& saves_map_dir() const { return saves_map_dir_; }
	const std::string& wkoscript_dir() const { return wkoscript_dir_; }
	int sdl_field_small_font_size() { return sdl_field_small_font_size_; }
	const SDL_OsInfo& os_info() const { return os_info_; }
	// ttray2& tray() { return tray_; }

	void set_can_run_var_or_timing_task()
	{
		VALIDATE(!can_run_var_or_timing_task_, null_str);
		can_run_var_or_timing_task_ = true;
	}

	void test_did_receive_broadcast();
	bool app_start_navigation(const aplt::tapplet& aplt, const std::string& position, const fn_navigation_bh& luafunc) override;
	// bool app_did_navigation_bh_moveit(bool result, const aplt::tapplet& aplt, const aplt::taplt_task& aplt_task) override;
	bool app_did_navigation_bh_use_cpp(bool result, const aplt::tapplet& aplt, const aplt::taplt_task& aplt_task, const aplt::tapplet::ttask& cfg_task) override;
	void app_stop_single_task_use_cpp(const aplt::taplt_task& aplt_task, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task) override;
	void app_request_retry_aplt_task() override;
	bool app_can_run_var_or_timing_task() const override;
	const aplt::taplt_task* app_shedule_var_task(aplt::ttask_vars& task_vars_result) override;
	aplt::tbg_task::tbase_bg_task2* app_request_klink_aplt_task(const aplt::taplt_task& aplt_task, const aplt::ttask_vars& task_vars) override;
	void app_clear_taskpoint() override;
	void app_ble_center_will_connect() override;

	void start_map_controller(tros_instance::ttask* task_ptr) override;
	void did_scan_subscribed(const sensor_msgs::LaserScan& msg, SDL_2Point& charging) override;
	bool show_gui_center(const tstart_aiagent* start_aiagent) override;
	void nullptr_slot_for_aplt_drivers(const aplt::tapplet& aplt) override;

	void start_mkscript_controller();
	void start_health_controller();
	void start_chart_controller();
	void did_applet_will_uninstall3(const std::string& res_path);
	// void did_applet_will_uninstall2(const aplt::tapplet& desire_aplt);

	void set_desire_depth_task(int task) override;
	void popup_rcamera(bool pre) override;
	void make_sure_clear_countdown_klink_aplt_task()
	{
		countdown_klink_aplt_task_.make_sure_clear();
	}

	aplt::thealth& health() { return health_; }

	// std::map<std::string, aplt::twkocourse_enroll>& wkocourse_enrolls() { return wkocourse_enrolls_; }
	std::map<std::string, aplt::twkocourse>& wkocourses() { return wkocourses_; }

private:
	void app_load_settings_config(const config& cfg) override;
	void app_pre_setmode(tpre_setmode_settings& settings) override;
	void app_load_pb() override;
	std::pair<std::string, ::google::protobuf::MessageLite*> app_pblite_from_type(int type) override;
	void app_post_lua() override;
	config app_critical_prefs() override;
	void app_handle_clipboard_paste(const std::string& text) override;
	bool app_update_ip(const uint32_t ipv4, const int prefixlen, const uint32_t gateway) override;
	void app_aplt_slice() override;
	void app_ros_slice() override;
	bool app_handle_aplt_msg(const aplt::tapplet* aplt, int msg) override;
	void app_fg_aplt_send_cpp_id(const aplt::tapplet& aplt, const std::string& window_id, int cpp_id) override;
	std::string app_fg_aplt_lua_pre_navigation_start(const aplt::tapplet& aplt) override;
	bool app_in_pure_task_cpp() const override;
	void app_bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task) override;
	void app_bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task) override;
	void app_rose_button_clicked(int retval) override;
	void app_didenterbackground() override;
	void app_didenterforeground() override;
	void did_driver_refreshed(const std::map<int, aplt::tapplet*>& type_applets, bool quiet, bool& require_start_base_node);
	void app_uninitialize() override;

	void handle_tray_event(uint32_t type) override;
	int sdl_GetTtyUSB(SDL_ttyUSB** ppttyUSB) override;

	void validate_driver_slot(int type, bool is_nullptr) const;
	bool refresh_driver_slot(int type, const aplt::tapplet* aplt, bool quiet);
	void apply_new_base_serial();
	int get_serial_path(int driver_type, std::string& path, std::string& model) override;
	void pinyin_did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start) override;

	void clear_save_task_point(const aplt::taplt_task* new_klink_aplt_task);
	void request_klink_aplt_task2(const aplt::taplt_task& new_aplt_task, const aplt::ttask_vars& task_vars);
	// void main_thread_set_req_task(const aplt::treq_task& task, const aplt::ttask_vars& task_vars);

	// aplt::tr_api
	aplt::tnlp_4_aiagent& nlp_4_aiagent() override { return *this; }
	bool add_timed_task(const std::map<int64_t, tadd_timed_task>& tasks) override;
	std::string request_task(const aplt::treq_task& req_task, const aplt::ttask_vars& task_vars,
		const std::function<void (const aplt::ttask_vars& task_vars)>& did_task_finished) override;
	std::string request_single_task(const aplt::treq_task& req_task) override;
	void set_task_finished() override { bg_task_.set_task_finished(); }
	void set_allow_short_voice(bool val) override;
	void set_dcamera_points(const SDL_FPoint3* points, int size, const tdepth_sector_result& result) override;
	void laser_publish_scan(const sensor_msgs::LaserScan& msg) override;
	lua_State* get_lua_State() const override;
	void call_lua_breakpoint() override;
	void aplt_add_msg_log(int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent) override;
	bool navigation_node_started() const override;
	void public_vel(double linear_x, double linear_y, double angular_z) override;
	const trpy& get_imu_rpy() const override { return ros_instance_.get_imu_rpy(); }
	void aplt_set_env_var(int type, const config::attribute_value& val) override;
	const aplt::ttask_var* aplt_get_env_var(int type) override
	{
		return aplt::get_env_var(aplt::builtin_var(type).id);
	}

	void set_privacy_protect(bool enable) override
	{
		if (enable != privacy_.protect()) {
			privacy_.set_protect(enable);
			preferences::set_privacy_protect(privacy_.protect());
		}
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
	bool is_listening() const override { return speech_driver_.is_listening(); }
	bool is_listen_speaking() const override { return speech_driver_.is_listen_speaking(); }
	bool start_listen() override { return speech_driver_.start_listen(nullptr); }
	void stop_listen() override { speech_driver_.stop_listen(); }
	void listen_next_course() override { speech_driver_.listen_next_course(); }

	void push_floating_window_task(const std::string& msg, int duration_ms) override;

	void health_push_n32_event(int type, int ctx) override;
	void health_push_str_event(int type, int ctx, const std::string& str, const std::string& aux_str, const std::string& aux_str2, int aux_int) override;
	void health_push_landmarks(const SDL_U16Point* landmarks, int unsatisfied_reason) override;
	void health_workout_finished(const std::string& aplt, const std::string& id) override;

	bool cswamp_addevent(int64_t ts, const std::string& desc, const std::vector<timage_pair>& images, bool quiet) override;
	bool cswamp_querytablecooking(int table, net::tcswamp_table_result& result, bool quiet) override;

	// tnlp_4_aiagent
	bool is_nlp_questioning() const override;
	void send_nlp_question_4_aiagent(bool new_conversation, const std::string& question, const surface& surf,
		const std::function<void(bool retbool, const std::string& answer, int input_tokens, int output_tokens)>& did_nlp_answer) override;
	void stop_nlp_question() override;

	// aplt::tslot_subscriber
	void iot_did_heartbeats(const std::set<aplt::tiot_heartbeat>& heartbeats) override;
	void iot_did_events(const std::set<aplt::tiot_event>& events) override;

	void speech_did_capture_audio(const uint8_t* stream, int len) override;
	void speech_send_nlp_question(const std::string& question) override;
	void aiagent_did_nlp_answer(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens) override;
	bool speech_did_recognition_result(const std::string& result) override;
	bool speech_can_request_task() override;

	enum {func_distance = func_builtin_count};
	void init_my_functions();
	// aplt::tfunction
	bool calculate2(const aplt::ttask_vars& task_vars, int func_code, const std::vector<std::string>& inputs, tresult& result) override;
	std::string func_2_var_exp(const aplt::tfunction_code& func, const std::vector<std::string>& params, bool browser) const;

private:
	const std::string saves_courseware_dir_;
	const std::string saves_map_dir_;
	std::string wkoscript_dir_;
	int sdl_field_small_font_size_;
	net::trdpd_manager rdpd_mgr_;
	tpble2 pble_;
	tdrivers drivers_;
	tros_map curmap_;
	tros_instance ros_instance_;
	tros_base_node base_node_;

	tbase_driver base_driver_;
	trobot_imu robot_imu_;
	tmoveit_driver moveit_driver_;
	tlaser_driver laser_driver_;
	tdcamera_driver dcamera_driver_;
	tiot_driver iot_driver_;
	tspeech_driver speech_driver_;
	tai_driver ai_driver_;

	tprivacy privacy_;

	aplt::tcfg_cpp_api cfg_cpp_api_;
	bool can_run_var_or_timing_task_;

	threading::mutex req_task_mutex_;
	aplt::treq_task req_task_;
	aplt::ttask_vars req_task_task_vars_;
	bool req_task_dirty_;

	aplt::tbg_task2 bg_task2_;
	std::unique_ptr<tmoveit_aplt_task> moveit_aplt_task_;

	gui2::tstore_slot store_slot_;
	std::unique_ptr<tpreempt_base_subtask_lock> preempt_base_subtask_lock_;

	SDL_OsInfo os_info_;
#ifdef _WIN32
	ttray2 tray_;
#endif

	struct tcountdown_klink_aplt_task
	{
	public:
		tcountdown_klink_aplt_task(game_instance& owner)
			: owner_(owner)
		{
			clear();
		}

		bool valid() const { return desire_request_ticks != 0; }

		void set(int _countdown_s, const aplt::taplt_task& _aplt_task, const aplt::ttask_vars& _task_vars)
		{
			VALIDATE(_countdown_s > 0, null_str);

			VALIDATE(!valid(), null_str);

			desire_request_ticks = SDL_GetTicks() + _countdown_s * 1000;

			// why initial set '_countdown_s + 1'? avoid speak '_countdown_s + 1'.
			// see 'int integer = diff / 1000 + 1' in slice(). 
			last_speak_s = _countdown_s + 1;

			aplt_task = _aplt_task;
			task_vars = _task_vars;
		}

		void clear()
		{
			desire_request_ticks = 0;
			task_vars.clear();
		}

		void make_sure_clear()
		{
			if (valid()) {
				clear();
			}
		}

		void slice()
		{
			if (!valid()) {
				return;
			}

			uint32_t now = SDL_GetTicks();
			if (desire_request_ticks > now) {
				int diff = desire_request_ticks - now;
				int integer = diff / 1000 + 1;

				if (integer != last_speak_s) {
					chinese::curr_pinyin.speak(str_cast(integer));
					last_speak_s = integer;
				}

			} else {
				owner_.request_klink_aplt_task2(aplt_task, task_vars);
				clear();
			}
		}

	public:
		game_instance& owner_;

		uint32_t desire_request_ticks;
		int last_speak_s;

		aplt::taplt_task aplt_task;
		aplt::ttask_vars task_vars;
	};
	tcountdown_klink_aplt_task countdown_klink_aplt_task_;

	aplt::thealth health_;

	// std::map<std::string, aplt::twkocourse_enroll> wkocourse_enrolls_;
	std::map<std::string, aplt::twkocourse> wkocourses_;
};

static void did_net_receive_broadcast(int argc, const char** argv, void* user)
{
	game_instance* instance = reinterpret_cast<game_instance*>(user);
	instance->did_receive_broadcast(argc, argv);
}

game_instance::game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv)
	: base_instance(ss, argc, argv)
	, saves_courseware_dir_(game_config::preferences_dir + "/saves/courseware")
	, saves_map_dir_(game_config::preferences_dir + "/saves/map")
	, wkoscript_dir_(preferences::wkoscript_dir())
	// so far font::SIZE_SMALLER or font::SIZE_SMALLEST isn't ready.
	, sdl_field_small_font_size_(nposm)
	, rdpd_mgr_(privacy_)
	, pble_(this, privacy_)
	, drivers_(*this, applets_, base_driver_, os_info_, std::bind(&game_instance::did_driver_refreshed, this, _1, _2, _3))
	, ros_instance_(drivers_, curmap_, base_driver_, moveit_driver_, laser_driver_, dcamera_driver_, speech_driver_, robot_imu_, *this)
	, base_node_(ros_instance_)
	, base_driver_(applets_, ros_instance_, base_node_, robot_imu_, camera_, bg_task_, cfg_cpp_api_, health_, drivers_, privacy_)
	, moveit_driver_(ros_instance_, applets_)
	, dcamera_driver_(camera_)
	, iot_driver_(*this)
	, speech_driver_(drivers_, cfg_cpp_api_, saves_courseware_dir_)
	, ai_driver_(drivers_, bg_task_)
	, privacy_(base_driver_)
	, cfg_cpp_api_(applets_, curmap_, bg_task_, base_driver_, speech_driver_)
	, can_run_var_or_timing_task_(false)
	, req_task_dirty_(false)
	, bg_task2_(applets_, drivers_, curmap_, cfg_cpp_api_, bg_task_, camera_, def_script_, def_tflite_, ros_instance_, base_driver_, dcamera_driver_, ai_driver_)
	, store_slot_(rdpd_mgr_, pble_, privacy_, applets_, bg_task_, cfg_cpp_api_, drivers_, base_driver_, /*std::bind(&game_instance::did_applet_will_uninstall, this, _1)*/*this)
#ifdef _WIN32
	, tray_(video())
#endif
	, countdown_klink_aplt_task_(*this)
{
	// To avoid accidents, always enable them every time.
	preferences::set_sound(true);
	preferences::set_music(true);

	create_directory_if_missing(saves_courseware_dir_ + "/download");
	create_directory_if_missing(saves_courseware_dir_ + "/upload");
	create_directory_if_missing(saves_map_dir_);

	create_directory_if_missing(game_config::preferences_dir + "/miktex/sandbox");

	bg_task_.set_bg_task2(bg_task2_);

	memset(&os_info_, 0, sizeof(os_info_));

	net::rose_set_create_http_api(net::chromium_create_http_api);
	mediapipe::rose_set_create_pose_tracking_api(mediapipe::create_pose_tracking_api);

	SDL_SetBgAudioOutput(SDL_TRUE);
	kosNetSetReceiveBroadcast(::did_net_receive_broadcast, this);

	dnn_warnning_ = disable_timing_warnning();

	init_my_functions();
}

game_instance::~game_instance()
{
	VALIDATE(moveit_aplt_task_.get() == nullptr, null_str);
}

void game_instance::app_uninitialize()
{
	VALIDATE(preempt_base_subtask_lock_.get() == nullptr, null_str);

	const library& fgaplt_lib = drivers_.find_by_type(apltsotype_fgaplt);
	VALIDATE(fgaplt_lib.get() == nullptr, null_str);

	if (cfg_cpp_api_.has_taskpoint()) {
		cfg_cpp_api_.clear_taskpoint();
	}

	base_driver_.set_slot(null_str, nullptr);
	const library& base2th_lib = drivers_.find_by_type(apltsotype_base2th);
	VALIDATE(base2th_lib.get() == nullptr, null_str);
	const library& aiagent_task_lib = drivers_.find_by_type(apltsotype_aiagent_task);
	VALIDATE(aiagent_task_lib.get() == nullptr, null_str);

	moveit_driver_.set_slot(null_str, nullptr);
	laser_driver_.set_slot(null_str, nullptr);
	dcamera_driver_.set_slot(null_str, nullptr);
	iot_driver_.set_slot(null_str, nullptr);
	// makesure stop speech before free tdrivers.
	speech_driver_.set_slot(null_str, nullptr);
	ai_driver_.set_slot(null_str, nullptr);

	// has 'lib' use lua_, release 'lib' before 'delete lua_ ' in ~base_instance().
	drivers_.libs.clear();
}

void game_instance::handle_tray_event(uint32_t type)
{
#ifdef _WIN32
	tray_.handle_tray_event(type);
#endif
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
		SDL_strlcpy(fix[0], "/dev/ttyACM0", sizeof(fix[0]));
	
		if (true || game_config::board_model == board_lubancat3) {
			SDL_strlcpy(fix[1], "/dev/ttyS10", sizeof(fix[1]));

		} else if (game_config::board_model == board_lubancat4) {
			SDL_strlcpy(fix[1], "/dev/ttyS0", sizeof(fix[1]));

		} else if (game_config::board_model == board_roc_rk3588s_pc) {
			SDL_strlcpy(fix[1], "/dev/ttyS7", sizeof(fix[1]));
		}
		fix_count = 2;
	}

	return SDL_GetTtyUSB(fix_dev_nodes, fix_count, ppttyUSB);
}

void game_instance::did_receive_broadcast(int argc, const char** argv)
{
	if (argc <= 1) {
		return;
	}
	int code = utils::to_int(argv[0]);
	if (code == ResponseCode::InterfaceAddressChange) {
		// 614    Address updated         192.168.1.108/24             wlan0   128   0
		// 614    Address removed         192.168.1.108/24             wlan0   128   0
		// 614    Address updated/removed 192.168.1.109/24             eth0    128    0
		// 614    Address updated         fe80::ce4b:73ff:fe1d:7f1c/64 wlan0   196    253
		// 614    Address removed         fe80::ce4b:73ff:fe1d:7f1c/64 wlan0   128    253
        // [code] [name]                  [address]                    [iface] [flag] [scope]
		const char *kUpdated = "updated";
		const char *kRemoved = "removed";
        if (argc != 7) {
            return;
        }
        if (strcmp(argv[1], "Address")) {
            return;
        }
        bool updated = true;

        if (!strcmp(argv[2], kUpdated)) {
            // Address updated
            updated = true;

        } else if (!strcmp(argv[2], kRemoved)) {
            // Address removed
            updated = false;
        } else {
            return;
        }

		net::IPAddress ip_address;
		size_t prefix_length_in_bits;
		bool ok = net::ParseCIDRBlock(argv[3], &ip_address, &prefix_length_in_bits);
		if (ok) {
			if (ip_address.IsIPv4()) {
				const uint8_t* data = ip_address.bytes().data();
				uint32_t ipv4;
				memcpy(&ipv4, data, 4);
				did_net_state_changed(argv[4], updated, ipv4);
			} else if (ip_address.IsIPv6()) {
			}
		}
	}
}

void game_instance::did_net_state_changed(const char* iface, bool connected, uint32_t ipv4)
{
	SDL_Log("did_net_state_changed(iface: %s, connected: %s, ipv4: 0x%08x) ip: 0x%08x",
		iface, connected? "true": "false", ipv4, ReceivingInterfaceAddr);

	if (!connected && ReceivingInterfaceAddr != INADDR_ANY && ipv4 == ReceivingInterfaceAddr) {
		// run not in main thread.
		make_ip_invalid();
	}
}

void game_instance::test_did_receive_broadcast()
{
	VALIDATE(game_config::os == os_windows, null_str);
	std::vector<std::string> argvs;
	argvs.push_back("612, Address, updated, 192.168.1.100/24, wlan0, 128, 0");
	argvs.push_back("614, Address, updated, 192.168.1.100/24, wlan0, 128");
	argvs.push_back("614, Address, updated, 192.168.1.100/24, wlan0, 128, 0");
	argvs.push_back("614, Address, removed, 192.168.1.100/24, wlan0, 128, 0");
	argvs.push_back("614, Address, updated/removed, 192.168.1.109/24, eth0, 128, 0");
	argvs.push_back("614, Address, updated, fe80::ce4b:73ff:fe1d:7f1c/64, wlan0, 196, 253");

	char* argv[24];
	for (std::vector<std::string>::const_iterator it = argvs.begin(); it != argvs.end(); it ++) {
		const std::string& msg = *it;
		std::vector<std::string> vstr = utils::split(msg);
		int n = 0;
		for (std::vector<std::string>::const_iterator it2 = vstr.begin(); it2 != vstr.end(); ++ it2, n ++) {
			const std::string& field = *it2;
			argv[n] = (char*)malloc(field.size() + 1);
			memcpy(argv[n], field.c_str(), field.size());
			argv[n][field.size()] = '\0';
		}
		int argc = n;
		did_net_receive_broadcast(argc, (const char**)argv, this);
		for (n = 0; n < argc; n ++) {
			free(argv[n]);
		}
	}
	SDL_Log("game_instance::test_did_receive_broadcast X");
}

void game_instance::app_load_settings_config(const config& cfg)
{

	game_config::version = version_info(cfg["version"].str());
	VALIDATE(game_config::version.is_rose_recommended(), null_str);

	char libkosapi_ver[36];
	kosGetVersion(libkosapi_ver, sizeof(libkosapi_ver));
	game_config::kosapi_ver = version_info(libkosapi_ver);
	VALIDATE(game_config::kosapi_ver.is_rose_recommended(), std::string("Error version: ") + game_config::kosapi_ver.str(true));
	const version_info min_libkosapi_ver("1.0.3-20211230");
	if (game_config::kosapi_ver < min_libkosapi_ver) {
		std::stringstream err;
		err << "libkospai's version(" << game_config::kosapi_ver.str(true) << ") must >= " << min_libkosapi_ver.str(true);
		VALIDATE(false, err.str());
	}

	SDL_GetOsInfo(&os_info_);
	const std::string display_str = utils::lowercase(os_info_.display);
	// if (display_str.find("eng.leagor") != std::string::npos) {
		game_config::board_model = board_model_str_2_int(os_info_.model);
	// }

	game_config::max_fps_to_encoder = cfg["max_fps_to_encoder"].to_int();
	VALIDATE(game_config::max_fps_to_encoder == 0 || (game_config::max_fps_to_encoder >= 20 && game_config::max_fps_to_encoder <= 60), null_str);

	// if any key in critical prefs isn't in preferences, try read it from critical_prefs
	{
		const config& critical_prefs = preferences::get_critical_prefs();

		const std::string sn = critical_prefs["sn"].str();
		// default startup ble broadcast.
		const bool bleperipheral = critical_prefs["bleperipheral"].to_bool(true);
		std::string blepassword = critical_prefs["blepassword"].str();
		if (blepassword.empty()) {
			blepassword = sha1_blepassword(DEFAULT_BLEPASSWORD);
		}
		
		// SDL_Log("app_load_settings_config, dump critical_prefs");
		// SDL_Log("    sn: %s", sn.c_str());
		// SDL_Log("    bleperipheral: %s", bleperipheral? "true": "false");

		if (preferences::bleperipheral() != bleperipheral) {
			preferences::set_bleperipheral(bleperipheral);
		}

		if (preferences::blepassword() != blepassword) {
			preferences::set_blepassword(blepassword, true);
		}

		if (preferences::sn().empty() && !sn.empty()) {
			preferences::set_sn(sn);
		}
	}
}

void game_instance::app_pre_setmode(tpre_setmode_settings& settings)
{	
	settings.pc_default_font_size = 16;
	settings.mobile_default_font_size = 16;
	settings.fullscreen = false;
	// settings.silent_background = false;
	if (game_config::os == os_windows) {
		settings.min_width = 880;
		// settings.min_height = 498; // 478 is tool small
		settings.min_height = 508;
	}
	// settings.startup_servers = server_httpd;
}

void game_instance::app_load_pb()
{
	load_action_tpl2s_cfg();
	sdl_field_small_font_size_ = game_config::os == os_windows? font::SIZE_SMALLER: font::SIZE_SMALLEST;
	wkocourse_enrolls_from_pref(wkocourse_enrolls_, &wkocourses_);

	load_logs_pb(LOGS_PB, LOGS_PB_MAX_DAYS, LOGS_PB_MAX_LOGS);
	bg_task_.app_load_pb(KLINK_PB);
	cfg_cpp_api_.load();

	label_state_nposm = _("_Idle");

	game_config::board_models.insert(std::make_pair(board_lubancat3, _("name^lubancat3")));
	game_config::board_models.insert(std::make_pair(board_lubancat4, _("name^lubancat4")));
	game_config::board_models.insert(std::make_pair(board_roc_rk3588s_pc, _("name^roc_rk3588s_pc")));
	VALIDATE((int)game_config::board_models.size() == board_count, null_str);

	utils::string_map symbols;
	symbols["count"] = "1";
	game_config::suppress_thresholds.insert(std::make_pair(1 * 60, vgettext2("$count minutes", symbols)));
	symbols["count"] = "15";
	game_config::suppress_thresholds.insert(std::make_pair(15 * 60, vgettext2("$count minutes", symbols)));
	symbols["count"] = "30";
	game_config::suppress_thresholds.insert(std::make_pair(30 * 60, vgettext2("$count minutes", symbols)));
	symbols["count"] = "1";
	game_config::suppress_thresholds.insert(std::make_pair(1 * 3600, vgettext2("$count hours", symbols)));
	game_config::suppress_thresholds.insert(std::make_pair(nposm, _("No restrictions")));
	VALIDATE(game_config::suppress_thresholds.count(DEFAULT_SUPRESS_THRESHOLD) != 0, null_str);

	game_config::driver_names.insert(std::make_pair(apltsotype_base, _("Base driver")));
	game_config::driver_names.insert(std::make_pair(apltsotype_laser, _("Laser driver")));
	game_config::driver_names.insert(std::make_pair(apltsotype_moveit, _("Moveit driver")));
	game_config::driver_names.insert(std::make_pair(apltsotype_dcamera, _("DCamera driver")));
	game_config::driver_names.insert(std::make_pair(apltsotype_iot, _("IoT driver")));
	game_config::driver_names.insert(std::make_pair(apltsotype_speech, _("Speech driver")));
	game_config::driver_names.insert(std::make_pair(apltsotype_ai, _("AI driver")));
	game_config::driver_names.insert(std::make_pair(apltsotype_fgaplt, _("Foreground applet")));
	game_config::driver_names.insert(std::make_pair(apltsotype_base2th, _("Base2th applet")));
	game_config::driver_names.insert(std::make_pair(apltsotype_aiagent_task, _("Aiagent task applet")));
	VALIDATE((int)game_config::driver_names.size() == apltsotype_count, null_str);

	game_config::driver_keys.insert(std::make_pair(apltsotype_base, "base_driver"));
	game_config::driver_keys.insert(std::make_pair(apltsotype_laser, "laser_driver"));
	game_config::driver_keys.insert(std::make_pair(apltsotype_moveit, "moveit_driver"));
	game_config::driver_keys.insert(std::make_pair(apltsotype_dcamera, "dcamera_driver"));
	game_config::driver_keys.insert(std::make_pair(apltsotype_iot, "iot_driver"));
	game_config::driver_keys.insert(std::make_pair(apltsotype_speech, "speech_driver"));
	game_config::driver_keys.insert(std::make_pair(apltsotype_ai, "ai_driver"));
	game_config::driver_keys.insert(std::make_pair(apltsotype_fgaplt, "fg_applet"));
	game_config::driver_keys.insert(std::make_pair(apltsotype_base2th, "base2th_applet"));
	game_config::driver_keys.insert(std::make_pair(apltsotype_aiagent_task, "aiagent_task_applet"));
	VALIDATE((int)game_config::driver_keys.size() == apltsotype_count, null_str);

	game_config::velocities.insert(std::make_pair(velocity_slower, _("velocity^slower")));
	game_config::velocities.insert(std::make_pair(velocity_slow, _("velocity^slow")));
	game_config::velocities.insert(std::make_pair(velocity_normal, _("velocity^normal")));
	game_config::velocities.insert(std::make_pair(velocity_fast, _("velocity^fast")));
	game_config::velocities.insert(std::make_pair(velocity_custom, _("velocity^custom")));
	VALIDATE((int)game_config::velocities.size() == velocity_count, null_str);

	game_config::markers.insert(std::make_pair(rspmapmarkertype_virtual, _("marker^virtual")));
	game_config::markers.insert(std::make_pair(rspmapmarkertype_wall, _("marker^wall")));
	VALIDATE((int)game_config::markers.size() == rspmapmarkertype_count, null_str);

	std::set<aplt::per_t> permissions;
	int type = task_type_moveto;
	permissions.insert(aplt::per_navigation);
	permissions.insert(aplt::per_ble);
	aplt::fill_per_if_necessary(permissions);
	game_config::temp_task_types.insert(std::make_pair(type, ttemp_task_type(type, "task_moveto", _("task^Move"), permissions)));

	type = task_type_aplt_task_no_position;
	// this type's permissions is determined by the specific aplt_task.
	permissions.clear();
	aplt::fill_per_if_necessary(permissions);
	game_config::temp_task_types.insert(std::make_pair(type, ttemp_task_type(type, "task_aplt_task_no_pos", _("task^task"), permissions)));

	type = task_type_aplt_task;
	// this type's permissions is determined by the specific aplt_task.
	permissions.clear();
	aplt::fill_per_if_necessary(permissions);
	game_config::temp_task_types.insert(std::make_pair(type, ttemp_task_type(type, "task_aplt_task", _("task^Move+task"), permissions)));

	type = task_type_aplt_task_2position;
	// this type's permissions is determined by the specific aplt_task.
	permissions.clear();
	aplt::fill_per_if_necessary(permissions);
	game_config::temp_task_types.insert(std::make_pair(type, ttemp_task_type(type, "task_aplt_task_2pos", _("task^Move2+task"), permissions)));

	type = task_type_charge2;
	permissions.clear();
	permissions.insert(aplt::per_navigation);
	permissions.insert(aplt::per_ble);
	permissions.insert(aplt::per_camera);
	aplt::fill_per_if_necessary(permissions);
	game_config::temp_task_types.insert(std::make_pair(type, ttemp_task_type(type, "task_charge", _("task^Charge"), permissions)));

	type = task_type_chat;
	permissions.clear();
	aplt::fill_per_if_necessary(permissions);
	game_config::temp_task_types.insert(std::make_pair(type, ttemp_task_type(type, "chat", _("Chat"), permissions)));
	VALIDATE((int)game_config::temp_task_types.size() == task_type_count2, null_str);

	// gui_modes
	std::vector<tgui_mode>& gui_modes = game_config::gui_modes;
	gui_modes.push_back(tgui_mode(mode_buildmap, _("Buildmap"), nposm));
    if (support_rosbag(mode_buildmap)) {
        gui_modes.push_back(tgui_mode(mode_buildmap, _("Buildmap(rosbag record)"), rosbag_record));
		gui_modes.push_back(tgui_mode(mode_buildmap, _("Buildmap(rosbag play)"), rosbag_play));
    }
    gui_modes.push_back(tgui_mode(mode_navigation, _("Navigation"), nposm));
    gui_modes.push_back(tgui_mode(mode_position, _("Position"), nposm));
    VALIDATE(gui_modes.size() >= mode_count, null_str);

	klink_cfgs.find(cfgtype_klink)->second.name = _("klink_cfg");
	klink_cfgs.find(cfgtype_task_cpp)->second.name = _("task_cpp_cfg");
	klink_cfgs.find(cfgtype_speech)->second.name = _("speech_cfg");

	const std::string short_rspfile = preferences::curmap(saves_map_dir_);
    if (!short_rspfile.empty()) {
		std::string rspfile = rspfile_short_2_full(saves_map_dir_, short_rspfile);
		ros::load_map_from_rsp(rspfile, curmap_);
    }

	VALIDATE(privacy_.is_nposm(), null_str);
	if (preferences::privacy_protect()) {
		privacy_.set_protect(true);
	}
	if (preferences::privacy_auto_protect_threshold() != nposm) {
		privacy_.set_auto_protect_threshold_s(preferences::privacy_auto_protect_threshold());
	}

#ifdef _WIN32
	surface icon = image::get_image("misc/lq48.png");
	tray_.create(icon, "Launcher");
	tray_.create_main_menu();
#endif
	SDL_RaiseWindow2();
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
	lua::register_ros_metatable(lua_->get_state(), *this);

	drivers_.init(applets_);
}

config game_instance::app_critical_prefs()
{
	config cfg;

	const std::string sn = preferences::sn();
	if (sn.empty()) {
		return cfg;
	}
	cfg["sn"] = sn;
	cfg["bleperipheral"].from_bool(preferences::bleperipheral());
	cfg["blepassword"] = preferences::blepassword();

	return cfg;
}

void game_instance::app_handle_clipboard_paste(const std::string& text)
{
	rdpd_mgr_.clipboard_updated(text);
}

uint32_t calculate_widecard_ip(uint32_t ipv4, int prefixlen)
{
	VALIDATE(prefixlen >= 1 && prefixlen < 32, null_str);
	uint32_t ret = SDL_Swap32(ipv4) & ~((1u << (32 - prefixlen)) - 1);
	return SDL_Swap32(ret);
}

bool game_instance::app_update_ip(const uint32_t ipv4, const int prefixlen, const uint32_t _gateway)
{
	VALIDATE(false, "now must not call kosNetSendMsg.");
	VALIDATE(ipv4 != 0, null_str);
	VALIDATE(prefixlen >= 1 && prefixlen < 32, null_str);
	VALIDATE(_gateway != 0, null_str);

	const std::string iface = "eth0";

	net::IPAddress ipaddr((const uint8_t*)&ipv4, 4);
	net::IPAddress gateway((const uint8_t*)&_gateway, 4);
    char result[128];
    const int maxBytes = sizeof(result);
	std::unique_ptr<char> msg(new char[1024]);
    char* msg_ptr = msg.get();

	sprintf(msg_ptr, "launcher netid %s", iface.c_str());
    int code = kosNetSendMsg(msg_ptr, result, maxBytes);
	if (game_config::os == os_windows) {
		SDL_snprintf(result, maxBytes, "%s 100", iface.c_str());
	}
    std::vector<std::string> vstr = utils::split(result, ' ');
    int netId = nposm;
    if (vstr.size() == 2 && vstr[0] == iface) {
         netId = utils::to_int(vstr[1]);
    }
    if (netId < 0) {
        SDL_Log("app_update_ip, result: %s, len: %i, netId(%i) fail", result, (int)strlen(result), netId);
        return false;
    }

	// interface getcfg eth0
	sprintf(msg_ptr, "interface getcfg %s", iface.c_str());
    kosNetSendMsg(msg_ptr, result, maxBytes);
	if (game_config::os == os_windows) {
		// 36:56:fb:9f:f8:a2 192.168.1.108 24 up broadcast running multicast
		// 00:00:00:00:00:00 0.0.0.0 0 down
		strcpy(result, "36:56:fb:9f:f8:a2 192.168.1.108 24 up broadcast running multicast");
	}
    vstr = utils::split(result, ' ');
    uint32_t originalIpv4 = 0;
	// net::IPAddress
    if (vstr.size() >= 4) {
        originalIpv4 = utils::to_ipv4(vstr[1]);
    }

    kosNetSendMsg("launcher broadcast disable", result, maxBytes);
	// interface clearaddrs eth0
	sprintf(msg_ptr, "interface clearaddrs %s", iface.c_str());
    kosNetSendMsg(msg_ptr, result, maxBytes);

	did_net_state_changed(nullptr, false, originalIpv4);

	// interface setcfg eth0 192.168.1.116 24 multicast up broadcast running
	sprintf(msg_ptr, "interface setcfg %s %s %i multicast up broadcast running", iface.c_str(), ipaddr.ToString().c_str(), prefixlen);
    kosNetSendMsg(msg_ptr, result, maxBytes);

	// network route add 100 eth0 192.168.1.0/24
	uint32_t widecard_gateway = calculate_widecard_ip(_gateway, prefixlen);
	net::IPAddress widecard_gateway2((const uint8_t*)&widecard_gateway, 4);
    sprintf(msg_ptr, "network route add %i %s %s/%i", netId, iface.c_str(), widecard_gateway2.ToString().c_str(), prefixlen);
    kosNetSendMsg(msg_ptr, result, maxBytes);

	// network route add 100 eth0 0.0.0.0/0 192.168.1.1
    sprintf(msg_ptr, "network route add %i %s 0.0.0.0/0 %s", netId, iface.c_str(), gateway.ToString().c_str());
    kosNetSendMsg(msg_ptr, result, maxBytes);

	// network default set 100
    kosNetSendMsg("launcher broadcast enable", result, maxBytes);

	return true;
}

void game_instance::app_aplt_slice()
{
/*
	// {leagor-stop_timing_lock}
	// if (bg_task_.disable_stop_timing()) {
	//	return;
	// }
*/
	if (!privacy_.disable_slice() && !privacy_.protect() && privacy_.auto_protect_threshold_s() != nposm) {
		uint32_t next_protect_ticks = privacy_.next_protect_ticks();

		bool has_rdp_client = rdpd_mgr_.started() && rdpd_mgr_.rdp_server().connection_count() != 0;
		if (has_rdp_client) {
			if (next_protect_ticks != 0) {
				SDL_Log("{dbg_privacy}%u reset_next_protect_ticks", SDL_GetTicks());
				privacy_.reset_next_protect_ticks();
			}

		} else if (next_protect_ticks == 0) {
			SDL_Log("{dbg_privacy}%u set_next_protect_ticks", SDL_GetTicks());
			privacy_.set_next_protect_ticks();

		} else if (SDL_GetTicks() >= next_protect_ticks) {
			SDL_Log("{dbg_privacy}%u set_protect(true)", SDL_GetTicks());
			privacy_.set_protect(true);
		}
	}

	base_driver_.slice();

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

	bg_task2_.slice();

	cfg_cpp_api_.add_timed_task_sliced();

	if (moveit_aplt_task_.get() != nullptr) {
		moveit_aplt_task_->slice();
	}

	if (fg_aplt() == nullptr && !bg_task_.is_ing() && cfg_cpp_api_.has_taskpoint() && !aplt::new_klink_task_disabled2(nullptr)) {
		const aplt::ttaskpoint& taskpoint = cfg_cpp_api_.taskpoint();
		const aplt::taplt_task& klink_aplt_task = bg_task_.task_from_pb_at(taskpoint.task_cpp_type, taskpoint.task_cpp_pb_at);
		bg_task_.request_klink_aplt_task(klink_aplt_task, taskpoint.task_vars);
	}

	health_.slice();

#ifdef _WIN32
	tray_.slice();
#endif
}

void game_instance::app_ros_slice()
{
	if (ros_instance_.initialized()) {
		gui2::twidget::tdisable_popup_window_lock lock("Must not popup new window during ros_instance_.slice().");
		ros_instance_.slice();
	}
}

void game_instance::app_rose_button_clicked(int retval)
{
	if (retval == gui2::twindow::MAP_VIEWER) {
		tspecial_mapop_mode_lock lock(mode_navigation);
		start_map_controller(nullptr);
	}
}

bool game_instance::app_start_navigation(const aplt::tapplet& aplt, const std::string& position, const fn_navigation_bh& luafunc)
{
	tros_instance::ttask* task_ptr = ros_instance_.moveto_guid(aplt, position, luafunc);
	if (task_ptr == nullptr) {
		return false;
	}
	if (bg_task2_.in_task_cpp()) {
		if (speech_driver_.installed()) {
			speech_driver_.set_allow_short_voice(false);
		}
	}
	return true;
}
/*
bool game_instance::app_did_navigation_bh_moveit(bool result, const aplt::tapplet& aplt, const aplt::taplt_task& aplt_task)
{
	// Why not make moveit_aplt_task_ as a object variable? 
	// - Some parameters are required to construct tmoveit_aplt_task, like ros_instance_.rsp_of_moveit_model(). 
	// At game_instance is constructed, these variables are unknown.
	VALIDATE(moveit_aplt_task_.get() == nullptr, null_str);

	moveit_aplt_task_.reset(new tmoveit_aplt_task(applets_, dcamera_driver_, ros_instance_, camera_));
	moveit_aplt_task_->did_navigation_bh_moveit(result, aplt, aplt_task);
	return true;
}
*/
bool game_instance::app_did_navigation_bh_use_cpp(bool result, const aplt::tapplet& _aplt, const aplt::taplt_task& aplt_task, const aplt::tapplet::ttask& cfg_task)
{
	aplt::tapplet& aplt = *const_cast<aplt::tapplet*>(&_aplt);

	bool keep_ros_task = false;
	utils::string_map symbols;
	std::string msg;

	const tmap_position* position1 = nullptr;
	if (!aplt_task.position1.empty()) {
		VALIDATE(curmap_.positions.count(aplt_task.position1) != 0, null_str);
		// although aplt_task.position1 isn't empty, it maybe not valid. 
		// for example: position in other map. but user don't revise in task2-gui.
		position1 = &curmap_.positions.find(aplt_task.position1)->second;
	}

	if (cfg_task.type == aplt::task_moveit) {
		VALIDATE(!aplt.fake, null_str);
		keep_ros_task = true;

		// Why not make moveit_aplt_task_ as a object variable? 
		// - Some parameters are required to construct tmoveit_aplt_task, like ros_instance_.rsp_of_moveit_model(). 
		// At game_instance is constructed, these variables are unknown.
		VALIDATE(moveit_aplt_task_.get() == nullptr, null_str);

		moveit_aplt_task_.reset(new tmoveit_aplt_task(applets_, dcamera_driver_, ros_instance_, camera_));
		moveit_aplt_task_->did_navigation_bh_moveit(result, aplt, cfg_task, aplt_task);

	} else if (cfg_task.type == aplt::task_camera) {
		VALIDATE(!aplt.fake, null_str);
		aplt::ttask_api* task_api = drivers_.create_task_api(aplt);
		if (task_api != nullptr && task_api->camera != nullptr) {
			int subtask_state = base_driver_.subtask_state();
			if (subtask_state == aplt::sts_ing) {
				// why not place it before 'instance->set_desire_depth_task(dctask_color)'?
				// -- subtask and this task maybe in a same aplt. 
				//    rose given such a rule: at any given moment, at most one task is running.
				//    camera->start_task() is already in aplt, so base_subtask must be terminated before it.
				base_driver_.idle_or_preempt_subtask(false);
			}
			msg = task_api->camera->start_task(aplt, cfg_task, bg_task2_.mutable_task_vars());
			// if tcpp_api/tcamera_api.start_task() fail, don't let tcpp_api/tcamera_api.task_finished() be called.
			if (msg.empty()) {
				instance->set_desire_depth_task(dctask_color);

				bg_task2_.set_camera_api(task_api->camera);

				camera_.set_slot(&bg_task2_);
				camera_.enter_task(tcamera::taskid_bgtask, tcamera::FLAG_SHOW_CANCEL, false);
			}

		} else {
			msg = _("Not implete 'aplt_create_task_api' method, or aplt::tcamera_api object.");
		}
		if (!msg.empty()) {
			bg_task_.add_log2(time(nullptr), msg, 0, false);
			bg_task_.set_luafunc_finished();
		}

	} else if (cfg_task.type == aplt::task_nonblock) {
		VALIDATE(!aplt.fake, null_str);
		bool subtype_is_charge = cfg_task.subtype == aplt::tnonblock_api::subtype_charge;
		if (subtype_is_charge) {
			VALIDATE(position1 != nullptr, null_str);
		}
		
		if (!result) {
			VALIDATE(subtype_is_charge, null_str);
			symbols["position"] = position1->name;
			msg = vgettext2("Cannot reach the '$position' position. Not charging", symbols);

		} else {
			aplt::ttask_api* task_api = drivers_.create_task_api(aplt);
			if (task_api != nullptr && task_api->nonblock != nullptr) {
				int width_mm = preferences::charge_width_mm();
				if (subtype_is_charge) {
					if (is_valid_charge_width(width_mm)) {
						msg = _("Start charge fail. Please set a valid charging pile width first.");
					}
				}
				if (msg.empty()) {
					msg = task_api->nonblock->start_task(aplt, cfg_task, bg_task2_.mutable_task_vars());
					if (task_api->nonblock->subtype() == aplt::tnonblock_api::subtype_charge) {
						task_api->nonblock->set_charge_width(width_mm / 1000.0);
					}
				}
				// if tcpp_api/tnonblock_api.start_task() fail, here don't call tcpp_api/tnonblock_api.task_finished().
				// it will called during 'app_stop_single_task_use_cpp'.
				if (msg.empty()) {
					// if (game_config::is_dbg_charge()) {
					if (subtype_is_charge) {
						ros_instance_.stop_dwa_local_planner_node();
						keep_ros_task = true;
					}
					bg_task2_.set_nonblock_api(task_api->nonblock);
				}

			} else {
				msg = _("Not implete 'aplt_create_task_api' method, or aplt::tnonblock_api object.");
			}
		}
		if (!msg.empty()) {
			bg_task_.add_log2(time(nullptr), msg, 0, false);
			bg_task_.set_luafunc_finished();
		}

	} else {
		VALIDATE(cfg_task.type == aplt::task_block, null_str);
		if (!aplt.fake) {
			aplt::ttask_api* task_api = drivers_.create_task_api(aplt);
			if (task_api !=nullptr && task_api->block != nullptr) {
				// aplt::ttask_pair pair = aplt::task_pair_from_task_id(aplt, aplt_task.task_id, true);
				msg = task_api->block->start_task(aplt, cfg_task, bg_task2_.mutable_task_vars());

				if (msg.empty()) {
					msg = _("block task finished");
				}

			} else {
				msg = _("Not implete 'aplt_create_task_api' method, or aplt::tblock_api object.");
			}
			bg_task_.add_log2(time(nullptr), msg, 0, false);
		}

		bg_task_.set_luafunc_finished();
	}

	return keep_ros_task;
}

void game_instance::app_stop_single_task_use_cpp(const aplt::taplt_task& aplt_task, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task)
{
	if (cfg_task.type == aplt::task_moveit) {
		// noral flow: navigation + bh_moveit
		// if terminate when navigation, for example bg_aplt running, it result bh_moveit don't executed.
		// in this scenario, moveit_aplt_task_ is nullptr.

		if (moveit_aplt_task_.get() != nullptr) {
			moveit_aplt_task_->stop_navigation_aplt_task(aplt_task);

			moveit_aplt_task_.reset();
		}

	} else if (cfg_task.type == aplt::task_camera) {
		// if task_api->camera->start_task() fail, that can only finished here.
		bg_task2_.finish_task_camera_or_charge(true, aplt, cfg_task, aplt::task_camera);

	} else if (cfg_task.type == aplt::task_nonblock) {
		// if task_api->charge->start_task() fail, that can only finished here.
		bg_task2_.finish_task_camera_or_charge(true, aplt, cfg_task, aplt::task_nonblock);

	} else {
		VALIDATE(cfg_task.type == aplt::task_block, null_str);

	} 
}

void game_instance::app_request_retry_aplt_task()
{
	bg_task2_.request_retry_aplt_task();
}

bool game_instance::app_can_run_var_or_timing_task() const
{
	VALIDATE(fg_aplt_ == nullptr && !bg_task_.is_ing(), null_str);

	if (!can_run_var_or_timing_task_) {
		return false;
	}

	// speech_sensor has a higher priority than timing_task.
	if (countdown_klink_aplt_task_.valid()) {
		return false;
	}

	if (cfg_cpp_api_.has_taskpoint()) {
		return false;
	}

	return true;
}

const aplt::taplt_task* game_instance::app_shedule_var_task(aplt::ttask_vars& task_vars_result)
{
	VALIDATE(fg_aplt() == nullptr && !bg_task_.is_ing(), null_str);

	aplt::ttask_vars task_vars1 = aplt::clone_env_vars(false);
	const std::map<aplt::taplt_task_key, aplt::taplt_task>& var_tasks = bg_task_.var_tasks();

	const std::vector<aplt::tvar_sensor>& var_sensors = cfg_cpp_api_.var_sensors();
	VALIDATE(bg_task_.var_tasks().size() == var_sensors.size(), null_str);
	aplt::tif_block::tresult result;
	const aplt::taplt_task* hit_var_task = nullptr;
	const aplt::tvar_sensor* hit_var_sensor = nullptr;

	const int var_mask_threshold = 5000; // 5 second
	for (std::map<aplt::taplt_task_key, aplt::taplt_task>::const_iterator it = var_tasks.begin(); it != var_tasks.end(); ++it) {
		const aplt::taplt_task& var_task = it->second;
		if (var_task.aplt_id.empty()) {
			continue;
		}
		if (var_task.state == aplt::taplt_task::state_apltnotfound || var_task.state == aplt::taplt_task::state_verdismatch) {
			continue;
		}
		const aplt::tvar_sensor& var_sensor = var_sensors[var_task.var_at];
		VALIDATE(var_task.var_at == var_sensor.at, null_str);

		uint32_t now = SDL_GetTicks();
		if (SDL_GetTicks() - var_task.last_shedule_ticks < var_mask_threshold) {
			// Users may encounter writing var_ensor errors, causing this task to be continuously triggered. 
			// Give a mask time so that users have time to take action after discovering errors.
			continue;
		}

		result = var_sensor.if_block.calculate(task_vars1);
		if (result.do_bool_set == bool_set_true) {
			const aplt::tapplet* var_aplt = aplt::aplt_from_id_ex(applets_, var_task.aplt_id);
			if (var_aplt == nullptr || var_aplt->tasks.count(var_task.task_id) == 0) {
				// Although 'state' will be modified in tbg_task2::shedule, 
				// but before that, 'tbg_task2::request_klink_aplt_task' will validate 'task_id'. 
				// Here, it needs to modify 'state' first.
				bg_task_.modify_task_state(var_task, aplt::taplt_task::state_apltnotfound, true);
				continue;
			}
			hit_var_task = &var_task;
			hit_var_sensor = &var_sensor;
			break;
		}
	}
	if (hit_var_sensor == nullptr) {
		return nullptr;
	}

	task_vars_result = aplt::clone_env_vars(true);
	for (std::map<std::string, std::string>::const_iterator it = result.do_map_vals.begin(); it != result.do_map_vals.end(); ++ it) {
		const std::string var_name = utils::join_app_prefix_id(aplt::fake_aplt.bundleid, it->first);
		const std::string& var_val = it->second;

		const config::attribute_value* exp_val = nullptr;
		aplt::tfunction::tresult exp_result;
		std::string no_symboled_str;
		int type = aplt::var_exp_which_type(var_val, &no_symboled_str);
		if (type != nposm) {
			exp_val = aplt::var_val_from_no_symboled_str(task_vars_result, type, no_symboled_str, exp_result);
		}
		if (exp_val != nullptr) {
			task_vars_result.insert_attribute(var_name, false, *exp_val);
		} else {
			task_vars_result.insert_string(var_name, false, it->second);
		}
	}

	return hit_var_task;
}

aplt::tbg_task::tbase_bg_task2* game_instance::app_request_klink_aplt_task(const aplt::taplt_task& aplt_task, const aplt::ttask_vars& task_vars)
{
	return bg_task2_.request_klink_aplt_task(aplt_task, task_vars);
}

void game_instance::app_clear_taskpoint()
{
	VALIDATE(false, null_str);
	if (bg_task2_.get_taskpoint() != nullptr) {
		cfg_cpp_api_.clear_taskpoint();
	}
}

void game_instance::app_ble_center_will_connect()
{
	// launcher acting as a peripheral, with other centers being connected, should disconnect
	if (!pble_.is_connected()) {
		return;
	}

	// bluetoothGattServer.cancelConnection(mPBleDevice) fail
	//   [ERROR:bta_gatts_act.cc(474)] bta_gatts_cancel_open: failed for open request
	SDL_Log("bluetoothGattServer.cancelConnection(mPBleDevice) fail, fix it in the future");
	return;

	bool use_stop_advertising = true;
	if (use_stop_advertising) {
		SDL_Log("%u app_ble_center_will_connect pre, pble_.stop_advertising()", SDL_GetTicks());
		pble_.stop_advertising();
	} else {
		SDL_Log("%u app_ble_center_will_connect pre, pble_.cancel_connection()", SDL_GetTicks());
		pble_.cancel_connection();
	}

	// int threshold = 3000; // 3 second
	int threshold = 2000; // 500ms
	uint32_t end_ticks = SDL_GetTicks() + threshold;
	do {
		events::pump();
		SDL_Delay(50);
	} while (SDL_GetTicks() < end_ticks);
	SDL_Log("%u app_ble_center_will_connect post", SDL_GetTicks());
}

void game_instance::app_bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
}

void game_instance::app_bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	const aplt::taplt_task* aplt_task = sys_task.aplt_task;

	const_cast<aplt::tbg_task::tbase_bg_task2*>(&sys_task)->task_finished(aplt_task);

	if (ros_instance_.has_task()) {
        // there maybe has task_, destroy it.
        // ros_instance_.makesure_cancel_goal(true);
        // VALIDATE(!ros_instance_.has_task(), null_str);
		// when 'keep_task == true'
		ros_instance_.erase_task();
    }
	VALIDATE(!ros_instance_.has_task(), null_str);
}

bool game_instance::app_handle_aplt_msg(const aplt::tapplet* aplt, int msg)
{
	bool caller_is_fg_aplt = true;
	int apltsotype = apltsotype_fgaplt;

	bool retval = true;
	if (msg == APLT_MSG_WILLRUN) {
		VALIDATE(caller_is_fg_aplt, null_str);
		VALIDATE(aplt != nullptr, null_str);
		drivers_.set_special_applet(apltsotype, aplt);
		const std::string libroseaplt_so_path = get_libroseaplt_so_path(aplt->res_path);
		if (SDL_IsFile(libroseaplt_so_path.c_str())) {
			// this applet exist libroseaplt.so
			const library& fgaplt_lib = drivers_.find_by_type(apltsotype);
			if (fgaplt_lib.get() == nullptr) {
				retval = false;
			}
		}

	} else if (msg == APLT_MSG_DIDDLGCLOSE) {
		VALIDATE(caller_is_fg_aplt, null_str);
		VALIDATE(aplt != nullptr, null_str);
		if (ros_instance_.has_task() && aplt == &ros_instance_.task()->aplt) {
			// closing dialog exist ros task(navigation/moveit)
			ros_instance_.erase_task();
		}

	} else if (msg == APLT_MSG_DIDTERMINATE) {
		if (caller_is_fg_aplt) {
			VALIDATE(!ros_instance_.has_task() || aplt != &ros_instance_.task()->aplt, null_str);

		} else {
			VALIDATE(!ros_instance_.has_task(), null_str);
		}
		drivers_.set_special_applet(apltsotype, nullptr);

		if (caller_is_fg_aplt) {
			if (aplt->id == base_driver_.aplt_id()) {
				apply_new_base_serial();
			}
		}
	}
	return retval;
}

void game_instance::app_fg_aplt_send_cpp_id(const aplt::tapplet& aplt, const std::string& window_id, int cpp_id)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE((cpp_id >= 0 && cpp_id < aplt::cpp_id_sys_count) || cpp_id >= aplt::cpp_id_aplt_min, null_str);

	for (int type = 0; type <= apltsotype_maxdriver; type ++) {
		if (type == apltsotype_base) {
			if (base_driver_.aplt_id() == aplt.id) {
				// Only send to the aplt that generated the cpp_id.
				base_driver_.slot->fg_aplt_send_cpp_id(window_id, cpp_id);
			}
			if (base_driver_.subtask_state() == aplt::sts_ing && base_driver_.subtask_pair().aplt->id == aplt.id) {
				base_driver_.camera_api().fg_aplt_send_cpp_id(window_id, cpp_id);
			}

		} else if (type == apltsotype_moveit) { 
			if (moveit_driver_.aplt_id() == aplt.id) {
				moveit_driver_.slot->fg_aplt_send_cpp_id(window_id, cpp_id);
			}

		} else if (type == apltsotype_laser) {
			if (laser_driver_.aplt_id() == aplt.id) {
				laser_driver_.slot->fg_aplt_send_cpp_id(window_id, cpp_id);
			}

		} else if (type == apltsotype_dcamera) {
			if (dcamera_driver_.aplt_id() == aplt.id) {
				dcamera_driver_.slot->fg_aplt_send_cpp_id(window_id, cpp_id);
			}

		} else if (type == apltsotype_iot) {
			if (iot_driver_.aplt_id() == aplt.id) {
				iot_driver_.slot->fg_aplt_send_cpp_id(window_id, cpp_id);
			}

		} else if (type == apltsotype_speech) {
			if (speech_driver_.aplt_id() == aplt.id) {
				speech_driver_.slot->fg_aplt_send_cpp_id(window_id, cpp_id);
			}

		} else if (type == apltsotype_ai) {
			if (ai_driver_.aplt_id() == aplt.id) {
				ai_driver_.slot->fg_aplt_send_cpp_id(window_id, cpp_id);
			}

		} else {
			VALIDATE(false, null_str);
		}
	}
}

void game_instance::apply_new_base_serial()
{
	tdrivers::tvars vars = drivers_.curvars(true);
	std::string serial_dev = vars.base.dev;
	int serial_baudrate = vars.base.baudrate;
	std::string serial_model = vars.base.model;

	if (!vars.base.valid(true)) {
		serial_dev.clear();
		serial_baudrate = nposm;
		serial_model.clear();
	}

	base_driver_.apply_new_serial(serial_dev, serial_baudrate, serial_model);
}

std::string game_instance::app_fg_aplt_lua_pre_navigation_start(const aplt::tapplet& aplt)
{
	VALIDATE(&aplt == instance->fg_aplt(), null_str);

	if (aplt.id == base_driver_.aplt_id()) {
		apply_new_base_serial();
	}

	std::string err_msg;
	if (!base_driver_.node_started()) {
		err_msg = base_node_error_str(base_driver_, _("Navigation"));
	}
	return err_msg;
}

bool game_instance::app_in_pure_task_cpp() const
{
	return bg_task2_.in_pure_task_cpp();
}

void game_instance::app_didenterbackground()
{
	if (game_config::explorer_singleton != nullptr) {
		gui2::texplorer_slot* slot = reinterpret_cast<gui2::texplorer_slot*>(game_config::explorer_singleton);
		slot->send_LG_EXPLORER_CODE_HIDDEN();
	}
}

void game_instance::app_didenterforeground()
{
	if (game_config::explorer_singleton != nullptr) {
		gui2::texplorer_slot* slot = reinterpret_cast<gui2::texplorer_slot*>(game_config::explorer_singleton);
		slot->send_LG_EXPLORER_CODE_SHOWN();
	}
}

void game_instance::start_map_controller(tros_instance::ttask* /*task_ptr*/)
{
	if (!base_driver().node_started()) {
		const std::string msg = base_node_error_str(base_driver(), aplt::all_fake_applets.find(aplt::builtinid_map)->second.name);
		gui2::show_message(null_str, msg);
		return;
	}

	hotkey::scope_changer changer(app_cfg(), "hotkey_map");
	map_controller controller(rdpd_mgr(), pble(), privacy(), drivers(), ros_instance(), dcamera_driver(), camera(), curmap(), base_driver_, 
		moveit_driver_, robot_imu_, app_cfg(), video(), moveit_aplt_task_, saves_map_dir_);
	controller.initialize(display::ZOOM_72);
	controller.main_loop();
}

void game_instance::did_scan_subscribed(const sensor_msgs::LaserScan& msg, SDL_2Point& charging)
{
	bg_task2_.did_scan_subscribed(msg, charging);
}

bool game_instance::show_gui_center(const tstart_aiagent* start_aiagent)
{
	if (!ai_driver().installed()) {
		utils::string_map symbols;
		symbols["target"] = aplt::all_fake_applets.find(aplt::builtinid_center)->second.name;
		symbols["driver"] = _("AI driver");
		symbols["settings"] = aplt::all_fake_applets.find(aplt::builtinid_settings)->second.name;
		gui2::show_message(null_str, vgettext2("To enter '$target', first select a valid $driver in '$settings'.", symbols));
		return false;
	}

	gui2::tcenter dlg(rdpd_mgr_, pble_, privacy_, ros_instance_, bg_task2_, applets_, *this, curmap_, 
		bg_task_, base_driver_, speech_driver_, dcamera_driver_, ai_driver_, saves_courseware_dir_, start_aiagent);
	dlg.show();
	return true;
}

void game_instance::start_mkscript_controller()
{
	if (!is_valid_wkoscript_or_wkocoruse_path(true, wkoscript_dir_)) {
		const aplt::tapplet* aplt = aplt::aplt_from_bundleid(applets_, aplt::get_bundleid(aplt::bundleid_leagor_khome));
		VALIDATE(aplt != nullptr, null_str);
		wkoscript_dir_ = aplt->preferences_dir + "/wkoscript";
		preferences::set_wkoscript_dir(wkoscript_dir_);
	}

	hotkey::scope_changer changer(core_cfg(), "hotkey_mkscript");
	mkscript_controller mkscript(rdpd_mgr(), pble(), privacy(), core_cfg(), video_, sdl_field_small_font_size_, saves_courseware_dir_, wkoscript_dir_);
	mkscript.initialize(64);
	mkscript.main_loop();
}

void game_instance::start_health_controller()
{
	class thealth_scene_slot: public trhealth_scene_slot, public gui2::tstatusbar
	{
	public:
		thealth_scene_slot(net::trdpd_manager& rdpd_mgr, tpble2& pble, tprivacy& privacy)
			: gui2::tstatusbar(rdpd_mgr, pble, privacy)
		{}

		void pre_show(gui2::twindow& window) override
		{
			tstatusbar::pre_show(window, gui2::find_widget<gui2::tpanel>(&window, "statusbar", false).grid());
		}

		void timer_handler(uint32_t now) override
		{
			refresh_statusbar_grid(now);
		}
	};
	thealth_scene_slot scene_slot(rdpd_mgr(), pble(), privacy());

	hotkey::scope_changer changer(core_cfg(), "hotkey_health");
	health_controller chart(scene_slot, core_cfg(), video_, health_, wkocourse_enrolls_, 
		wkocourses_, sdl_field_small_font_size_);
	chart.initialize(chart_unit::initial_zoom);
	chart.main_loop();
}

void game_instance::start_chart_controller()
{
	hotkey::scope_changer changer(core_cfg(), "hotkey_chart");
	chart_controller chart(rdpd_mgr(), pble(), privacy(), speech_driver_, core_cfg(), video_);
	chart.initialize(chart_unit::initial_zoom);
	chart.main_loop();
}

void game_instance::did_applet_will_uninstall3(const std::string& res_path)
{
	VALIDATE(!bg_task_.is_ing(), null_str);

	// 1) New installation. res_path's applet does not exist.
	// 2) Upgrade. res_path's applet is existed.
	// 3) Uninstall. res_path's applet is existed.
	// The res_path's applet(desire_aplt) may or may not exist.
	const aplt::tapplet* desire_aplt = nullptr;
	for (std::map<aplt::taplt_key, aplt::tapplet>::const_iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		const aplt::tapplet& aplt = it->second;
		if (aplt.res_path == res_path) {
			desire_aplt = &aplt;
			break;
		}
	}
	if (desire_aplt == nullptr) {
		VALIDATE(!drivers_.has_lib_using(res_path), null_str);
		return;
	}

	// if need, stop timing
	std::string stop_reason;
	if (drivers_.has_lib_using(res_path)) {
		stop_reason = _("One driver to be uninstalled");

	}
	if (!stop_reason.empty()) {
		instance->stop_bg_task_if_runing(null_str, stop_reason);
	}

	// did_applet_will_uninstall2(*desire_aplt);
	nullptr_slot_for_aplt_drivers(*desire_aplt);
/*
	for (int type = 0; type <= apltsotype_maxdriver; type ++) {
		if (type == apltsotype_base) {
			// if hit, stop base. 
			if (base_driver_.aplt_id() == desire_aplt->id) {
				base_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_moveit) { 
			// if hit, stop moveit. 
			if (moveit_driver_.aplt_id() == desire_aplt->id) {
				moveit_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_laser) {
			// if hit, stop laser. 
			if (laser_driver_.aplt_id() == desire_aplt->id) {
				laser_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_dcamera) {
			// if hit, stop dcamera. 
			if (dcamera_driver_.aplt_id() == desire_aplt->id) {
				dcamera_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_iot) {
			// if hit, stop iot. 
			if (iot_driver_.aplt_id() == desire_aplt->id) {
				iot_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_speech) {
			// if hit, stop speech. 
			if (speech_driver_.aplt_id() == desire_aplt->id) {
				speech_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_ai) {
			// if hit, stop aiagent. 
			if (ai_driver_.aplt_id() == desire_aplt->id) {
				ai_driver_.set_slot(null_str, nullptr);
			}

		} else {
			VALIDATE(false, null_str);
		}
	}
*/
	drivers_.did_uninstall(res_path);
}

void game_instance::nullptr_slot_for_aplt_drivers(const aplt::tapplet& aplt)
{
	for (int type = 0; type <= apltsotype_maxdriver; type ++) {
		if (type == apltsotype_base) {
			// if hit, stop base. 
			if (base_driver_.aplt_id() == aplt.id) {
				base_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_moveit) { 
			// if hit, stop moveit. 
			if (moveit_driver_.aplt_id() == aplt.id) {
				moveit_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_laser) {
			// if hit, stop laser. 
			if (laser_driver_.aplt_id() == aplt.id) {
				laser_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_dcamera) {
			// if hit, stop dcamera. 
			if (dcamera_driver_.aplt_id() == aplt.id) {
				dcamera_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_iot) {
			// if hit, stop iot. 
			if (iot_driver_.aplt_id() == aplt.id) {
				iot_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_speech) {
			// if hit, stop speech. 
			if (speech_driver_.aplt_id() == aplt.id) {
				speech_driver_.set_slot(null_str, nullptr);
			}

		} else if (type == apltsotype_ai) {
			// if hit, stop aiagent. 
			if (ai_driver_.aplt_id() == aplt.id) {
				ai_driver_.set_slot(null_str, nullptr);
			}

		} else {
			VALIDATE(false, null_str);
		}
	}
}

void game_instance::validate_driver_slot(int type, bool is_nullptr) const
{
	if (type == apltsotype_base) {
		if (is_nullptr) {
			VALIDATE(base_driver_.slot == nullptr, null_str);
		} else {
			VALIDATE(base_driver_.slot != nullptr, null_str);
		}

	} else if (type == apltsotype_moveit) {
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

	} else {
		VALIDATE(false, null_str);
	}
}

bool game_instance::refresh_driver_slot(int type, const aplt::tapplet* aplt, bool quiet)
{
	bool require_start_base_node = false;

	std::string curr_aplt_id;
	if (type == apltsotype_base) {
		curr_aplt_id = base_driver_.aplt_id();

	} else if (type == apltsotype_moveit) {
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

	} else {
		VALIDATE(false, null_str);
	}

	if (aplt == nullptr) {
		if (type == apltsotype_base) {
			base_driver_.set_slot(null_str, nullptr);

		} else if (type == apltsotype_moveit) {
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
		}

	} else if (curr_aplt_id != aplt->id) {
		void* slot = nullptr;
		if (type == apltsotype_base) {
			slot = drivers_.create_base_slot();

		} else if (type == apltsotype_moveit) {
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
		}

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

			} else if (type == apltsotype_moveit) {
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
			}
		} else {
			if (!quiet) {
				utils::string_map symbols;
				symbols["driver"] = game_config::driver_names.find(type)->second;
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
	const aplt::tapplet* moveit_aplt = nullptr;
	const aplt::tapplet* laser_aplt = nullptr;
	const aplt::tapplet* dcamera_aplt = nullptr;
	const aplt::tapplet* iot_aplt = nullptr;
	const aplt::tapplet* speech_aplt = nullptr;
	const aplt::tapplet* aiagent_aplt = nullptr;
	for (std::map<int, aplt::tapplet*>::const_iterator it = type_applets.begin(); it != type_applets.end(); ++ it) {
		int type = it->first;
		aplt::tapplet& aplt = *it->second;
		if (type == apltsotype_base) {
			base_aplt = &aplt;

		} else if (type == apltsotype_moveit) {
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
		}
	}

	require_start_base_node = refresh_driver_slot(apltsotype_base, base_aplt, quiet);
	refresh_driver_slot(apltsotype_moveit, moveit_aplt, quiet);
	refresh_driver_slot(apltsotype_laser, laser_aplt, quiet);
	refresh_driver_slot(apltsotype_dcamera, dcamera_aplt, quiet);
	refresh_driver_slot(apltsotype_iot, iot_aplt, quiet);
	refresh_driver_slot(apltsotype_speech, speech_aplt, quiet);
	refresh_driver_slot(apltsotype_ai, aiagent_aplt, quiet);
}

int game_instance::get_serial_path(int driver_type, std::string& path, std::string& model)
{
    if (driver_type == apltsotype_base) {
		if (base_driver_.slot != nullptr) {
			return base_driver_.slot->get_serial_path(path, model);
		}

    } else if (driver_type == apltsotype_laser) {
        if (laser_driver_.slot != nullptr) {
			return laser_driver_.slot->get_serial_path(path);
		}

    } else if (driver_type == apltsotype_moveit) {
        if (moveit_driver_.slot != nullptr) {
			return moveit_driver_.slot->get_serial_path(path);
		}

    } else {
        VALIDATE(false, null_str);
    }

	path.clear();
    return nposm;
}

void game_instance::pinyin_did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start)
{
	base_instance::pinyin_did_speak_stopped(id, text, unplayed_start);
	speech_driver_.pinyin_did_speak_stopped(id, text, unplayed_start);
}

void game_instance::set_desire_depth_task(int task)
{
	if (dcamera_driver_.installed()) {
		dcamera_driver_.set_desire_depth_task(dctask_color);
	}
}

void game_instance::popup_rcamera(bool pre)
{
	if (!base_driver_.installed()) {
		VALIDATE(preempt_base_subtask_lock_.get() == nullptr, null_str);
		return;
	}
	if (pre) {
		VALIDATE(preempt_base_subtask_lock_.get() == nullptr, null_str);
		preempt_base_subtask_lock_.reset(new tpreempt_base_subtask_lock(base_driver_));

	} else {
		VALIDATE(preempt_base_subtask_lock_.get() != nullptr, null_str);
		preempt_base_subtask_lock_.reset();
	}
}

void game_instance::clear_save_task_point(const aplt::taplt_task* new_klink_aplt_task)
{
	if (new_klink_aplt_task != nullptr) {
		// if @new_klink_aplt_task isn't nullptr, it must be klink aplt_task.
		VALIDATE(bg_task_.in_4tasks(*new_klink_aplt_task), null_str);
	}
	VALIDATE(bg_task_.is_ing(), null_str);

	if (bg_task2_.can_save_taskpoint(new_klink_aplt_task)) {
		// both task-a and task-b are recoverable.
		// 1)task-a. (after)taskpoint: nullptr
		// 2)task-b. (after)taskpoint: task-a
		// 3)tasb-b. (after)taskpoint: nullptr
		if (cfg_cpp_api_.has_taskpoint()) {
			aplt::ttaskpoint& taskpoint = cfg_cpp_api_.taskpoint();
			cfg_cpp_api_.clear_taskpoint();
		}
		if (bg_task2_.klink_cpp_aplt_task2 != new_klink_aplt_task) {
			bg_task2_.save_taskpoint();
		}

	} else if (cfg_cpp_api_.has_taskpoint()) {
		if (new_klink_aplt_task != nullptr) {
			aplt::ttaskpoint& taskpoint = cfg_cpp_api_.taskpoint();
			if (taskpoint.task_cpp_type == new_klink_aplt_task->type && taskpoint.task_cpp_pb_at == new_klink_aplt_task->pb_at) {
				VALIDATE(false, "program error #5a");
				cfg_cpp_api_.clear_taskpoint();
			}
		}
	}
}

void game_instance::request_klink_aplt_task2(const aplt::taplt_task& new_aplt_task, const aplt::ttask_vars& task_vars)
{
	VALIDATE(bg_task_.in_4tasks(new_aplt_task), null_str);

	if (bg_task_.is_ing()) {
		clear_save_task_point(&new_aplt_task);
/*
		if (bg_task2_.can_save_taskpoint(new_aplt_task)) {
			// both task-a and task-b are recoverable.
			// 1)task-a. (after)taskpoint: nullptr
			// 2)task-b. (after)taskpoint: task-a
			// 3)tasb-b. (after)taskpoint: nullptr
			if (cfg_cpp_api_.has_taskpoint()) {
				aplt::ttaskpoint& taskpoint = cfg_cpp_api_.taskpoint();
				cfg_cpp_api_.clear_taskpoint();
			}
			if (bg_task2_.klink_cpp_aplt_task2 != &new_aplt_task) {
				bg_task2_.save_taskpoint();
			}

		} else if (cfg_cpp_api_.has_taskpoint()) {
			aplt::ttaskpoint& taskpoint = cfg_cpp_api_.taskpoint();
			if (taskpoint.task_cpp_type == new_aplt_task.type && taskpoint.task_cpp_pb_at == new_aplt_task.pb_at) {
				cfg_cpp_api_.clear_taskpoint();
			}
		}
*/
	}
	bg_task_.request_klink_aplt_task(new_aplt_task, task_vars);
}

bool game_instance::add_timed_task(const std::map<int64_t, tadd_timed_task>& tasks)
{
	cfg_cpp_api_.set_add_timed_tasks(tasks);
	return true;
}

std::string game_instance::request_task(const aplt::treq_task& req_task, const aplt::ttask_vars& task_vars,
	const std::function<void (const aplt::ttask_vars& task_vars)>& did_task_finished)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(req_task.valid(), null_str);
	// bool now_is_task_cpp = req_task.task->type == aplt::task_cpp;

	if (ai_driver_.aiagent_task_is_terminating()) {
		return _("The AI agent task is terminating and cannot run new applet task.");
	}

	// gui-press has a higher priority than speech_sensor.
	countdown_klink_aplt_task_.make_sure_clear();

	if (bg_task_.is_ing()) {
		clear_save_task_point(nullptr);
		stop_bg_task_if_runing(null_str, _("To run a temporary task, stop the current one"));
	}

	if (!aplt::get_b_api().is_listen_speaking() && chinese::curr_pinyin.rsp.valid()) {
		chinese::curr_pinyin.speak(null_str);
	}

	VALIDATE(!ros_instance_.has_task(), null_str);
	{
		threading::lock lock(req_task_mutex_);
		req_task_dirty_ = false;

		// Indeed, just in the 'center', that's fine with 'use_post = false'. 
		// It is not possible to determine which function the 3rd-applet will call, for security, use 'use_post = true'.
		return bg_task2_.request_gui_aplt_task(req_task, task_vars, did_task_finished);
	}
}

std::string game_instance::request_single_task(const aplt::treq_task& req_task)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(bg_task2_.in_pure_task_cpp(), null_str);

	// req_task_validate2(req_task);

	std::string err_reason;
	utils::string_map symbols;

	if (!req_task.position1_uuid.empty() && curmap_.positions.count(req_task.position1_uuid) == 0) {
		symbols["position"] = req_task.position1_uuid;
		err_reason = vgettext2("There is no position: $position", symbols);

	} else if (!req_task.position2_uuid.empty() && curmap_.positions.count(req_task.position2_uuid) == 0) {
		symbols["position"] = req_task.position2_uuid;
		err_reason = vgettext2("There is no position: $position", symbols);

	} else {
		VALIDATE(req_task.valid(), null_str);
		if (req_task.position1_uuid.empty() && !req_task.position2_uuid.empty()) {
			err_reason = vgettext2("If poisition1 is empty, position2 must be empty", symbols);
		}
	}
	//
	if (!err_reason.empty()) {
		symbols["task"] = req_task.task->name;
		symbols["reason"] = err_reason;
		const std::string err_msg = vgettext2("Cannot execute $task. $reason", symbols);
		add_msg_only_log(logtype_warn, err_msg, 0, false);
		return err_msg;
	}

	req_task.validate();

	if (chinese::curr_pinyin.rsp.valid()) {
		// ???---
		// chinese::curr_pinyin.speak(null_str);
	}

	// below will result reenter, and to dead-lock.
	bg_task2_.request_single_aplt_task(req_task);

	return null_str;
}

void game_instance::set_allow_short_voice(bool val)
{
	if (speech_driver_.installed()) {
		speech_driver_.set_allow_short_voice(val);
	}
}

void game_instance::set_dcamera_points(const SDL_FPoint3* points, int size, const tdepth_sector_result& result)
{
	ros_instance_.set_dcamera_points(points, size, result);
}

void game_instance::laser_publish_scan(const sensor_msgs::LaserScan& msg)
{
	// run in thread: "laser_driver_node"
	VALIDATE_NOT_MAIN_THREAD();

	sensor_msgs::LaserScan result;
	ros_instance_.integrate_dcamera_LaserScan(msg, result);
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

bool game_instance::navigation_node_started() const
{
	return ros_instance_.navigation_node_started();
}

void game_instance::public_vel(double linear_x, double linear_y, double angular_z)
{
	ros_instance_.public_vel(linear_x, linear_y, angular_z);
}

void user_not_valid_try_again()
{
	if (!current_user.valid()) {
		if (!preferences::login_username().empty() && !preferences::login_pwcookie().empty()) {
			const std::string username = preferences::login_username();
			const std::string cookie = preferences::login_pwcookie();

			net::cswamp_login2(current_user, net::cswamp_login_type_cookie, username, cookie, nposm, true);
		}
	}
}

void game_instance::aplt_set_env_var(int type, const config::attribute_value& val)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(var_type_is_BI_env(type), null_str);
	bg_task2_.set_task_var(aplt::builtin_var(type).id, false, val);
}

void game_instance::push_floating_window_task(const std::string& msg, int duration_ms)
{
	VALIDATE_IN_MAIN_THREAD();
#ifdef _WIN32
	if (tray_.valid() && !msg.empty()) {
		tray_.push_task(msg, duration_ms);
	}
#endif
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
	health_.health_workout_finished(aplt, id);
}

bool game_instance::cswamp_addevent(int64_t ts, const std::string& desc, const std::vector<timage_pair>& images, bool quiet)
{
	if (bg_task_.is_ing()) {
		VALIDATE(quiet, null_str);
	}

	user_not_valid_try_again();
	if (!current_user.valid()) {
		return false;
	}

	const std::string devicename = preferences::sn();
	int image_format = img_png;
	gui2::tprogress_default_slot slot(std::bind(&net::cswamp_addevent, _1, current_user.sessionid, 
			ts, devicename, desc, image_format, std::ref(images), quiet));
	bool ret = gui2::run_with_progress(slot, null_str, null_str, 1);
	return ret;
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

	tbase_driver::tallow_restart_subtask_when_bg_ing_lock lock(base_driver_);

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

bool game_instance::cswamp_querytablecooking(int table, net::tcswamp_table_result& result, bool quiet)
{
	if (bg_task_.is_ing()) {
		VALIDATE(quiet, null_str);
	}

	user_not_valid_try_again();
	if (!current_user.valid()) {
		return false;
	}
	gui2::tprogress_default_slot slot(std::bind(&net::cswamp_querytablecooking, _1, current_user.sessionid, 
			table, std::ref(result), quiet));
	bool ret = gui2::run_with_progress(slot, null_str, null_str, 1);
	return ret;
}

bool game_instance::is_nlp_questioning() const
{
	VALIDATE_IN_MAIN_THREAD();
	if (!ai_driver_.installed()) {
		return false;
	}
	return ai_driver_.slot->is_nlp_questioning();
}

void game_instance::send_nlp_question_4_aiagent(bool new_conversation, const std::string& question, const surface& surf,
	const std::function<void(bool retbool, const std::string& answer, int input_tokens, int output_tokens)>& did_nlp_answer)
{
	VALIDATE_IN_MAIN_THREAD();
	if (game_config::os == os_windows) {
		// AI driver should support multi-turn conversations. 
		// Multi-turn conversations may result to the consumption of more tokens, 
		// but currently, there is no necessity for multi-turn conversations.
		// To avoid incorrect parameter transmission, 'new_conversation == true' checks are enforced here.
		VALIDATE(new_conversation, "'new_conversation == true' checks are enforced here.");
	}

	std::string err_msg;
	if (!ai_driver_.installed()) {
		err_msg = _("The AI driver is not installed.");
	}

	if (err_msg.empty() && ai_driver_.aiagent_task_is_terminating()) {
		err_msg = _("The AI agent task is terminating and cannot send new NLP request.");
	}

	if (!err_msg.empty()) {
		if (did_nlp_answer != NULL) {
			did_nlp_answer(false, err_msg, 0, 0);
		}
		return;
	}

	ai_driver_.slot->send_nlp_question_4_aiagent(new_conversation, question, surf, did_nlp_answer);
}

void game_instance::stop_nlp_question()
{
	VALIDATE_IN_MAIN_THREAD();
	if (!ai_driver_.installed()) {
		return;
	}
	return ai_driver_.slot->stop_nlp_question();
}


void game_instance::iot_did_heartbeats(const std::set<aplt::tiot_heartbeat>& heartbeats)
{
	VALIDATE_IN_MAIN_THREAD();
}

void game_instance::iot_did_events(const std::set<aplt::tiot_event>& events)
{	
	VALIDATE_IN_MAIN_THREAD();

	const library& iot_driver = drivers_.find_by_type(apltsotype_iot);
	VALIDATE(iot_driver.get() != nullptr, null_str);
	const std::string iot_lib_images_path = iot_driver.get()->res_path + "/images";

	const std::map<aplt::tiot_device_key, aplt::tiot_device>& iot_devices = bg_task_.iot_devices();

	utils::string_map symbols;
	std::string src_name;
	std::string evt_name;
	std::string device_name;
	char msg[128];
	for (std::set<aplt::tiot_event>::const_reverse_iterator it = events.rbegin(); it != events.rend(); ++ it) {
		const aplt::tiot_event& evt = *it;

		std::string alias = evt.device_id;
		aplt::tiot_device_key key(evt.src, evt.device_id);
		if (iot_devices.count(key) == 0) {
			bg_task_.insert_iot_device(evt.src, evt.device_id);
		} else {
			alias = iot_devices.find(key)->second.alias2();
		}
		std::string new_icon = iot_lib_images_path + "/" + evt.icon;
		bg_task_.modify_iot_device(evt.src, evt.device_id, null_str, new_icon, evt.t, aplt::tiot_device::FLAG_ICON | aplt::tiot_device::FLAG_TS);

		symbols["device"] = alias;
		device_name = vgettext2("device: $device", symbols);

		src_name = aplt::iot_sources.count(evt.src) != 0? aplt::iot_sources.find(evt.src)->second.name: str_cast(evt.src);
		evt_name = aplt::iot_events.count(evt.evt) != 0? aplt::iot_events.find(evt.evt)->second.name: str_cast(evt.evt);
		SDL_snprintf(msg, sizeof(msg), "%s %s %s %s", 
			utils::format_time_hms(evt.t / 1000).c_str(), src_name.c_str(), evt_name.c_str(), device_name.c_str());
		add_msg_only_log(logtype_iot_event, msg, 0, false);
	}

	iot_did_events2(events);

	if (instance->fg_aplt() == nullptr && !aplt::new_klink_task_disabled2(nullptr)) {
		for (std::set<aplt::tiot_event>::const_iterator it = events.begin(); it != events.end(); ++ it) {
			const aplt::tiot_event& evt = *it;

			const aplt::taplt_task* iot_task_ptr = bg_task_.iot_task_by_iot_evt(evt.src, evt.evt, evt.device_id);
			if (iot_task_ptr != nullptr) {
				const aplt::taplt_task& hit_task = *iot_task_ptr;
				if (bg_task_.is_ing() && !bg_task_.priority_can_run(hit_task, nullptr)) {
					continue;
				}
				const aplt::tapplet* aplt = aplt::aplt_from_id_ex(applets_, hit_task.aplt_id);
				if (aplt != nullptr && aplt->tasks.count(hit_task.task_id) != 0) {
					// iot_sensor has a higher priority than speech_sensor.
					countdown_klink_aplt_task_.make_sure_clear();

					aplt::ttask_vars task_vars = aplt::clone_env_vars(true);
					task_vars.insert_string(aplt::builtin_var(aplt::var_iot_device_id).id, false, hit_task.src_device_id);

					const std::map<aplt::tiot_device_key, aplt::tiot_device>& iot_devices = bg_task_.iot_devices();
					aplt::tiot_device_key key(hit_task.iot_src, hit_task.src_device_id);
					const std::string alias = iot_devices.count(key) != 0? iot_devices.find(key)->second.alias: null_str;
					task_vars.insert_string(aplt::builtin_var(aplt::var_iot_alias).id, false, alias);

					request_klink_aplt_task2(hit_task, task_vars);
				}
				break;
			}
		}
	} else {
		const std::string warn_msg = _("Receive a IoT event, but timing is not right, it cannot be executed");
		add_msg_only_log(logtype_warn, warn_msg, 0, false);
	}
}

void game_instance::speech_did_capture_audio(const uint8_t* stream, int len)
{
	VALIDATE_NOT_MAIN_THREAD();

	tnot_main_base_msg_subscriber* singleton = capture_audio2_singleton;

	if (capture_audio2_singleton != nullptr) {
		// why use singleton->subscriber_mutex()?
		//  -- between this tow statement, capture_audio2_singleton maybe set to nullptr.
		//     But in such a short period of time, the 'tnot_main_base_msg_subscriber' object still existed.
		//     This requires ~chart_controller to execute 'capture_audio2_singleton = nullptr' as soon as enter.
		threading::lock lock(singleton->subscriber_mutex());
		if (capture_audio2_singleton == nullptr) {
			return;
		}
		capture_audio2_singleton->speech_did_capture_audio2(stream, len);
	}
}

void game_instance::speech_send_nlp_question(const std::string& question)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(!question.empty(), null_str);

	std::string err_msg;
	if (!ai_driver_.installed()) {
		utils::string_map symbols;
		symbols["settings"] = aplt::all_fake_applets.find(aplt::builtinid_settings)->second.name;
		err_msg = vgettext2("For voice chat, first select a aiagent drive in '$settings'", symbols);
	}

	if (err_msg.empty() && ai_driver_.is_nlp_questioning()) {
		err_msg = _("The large language model is answering the question. Please wait to ask your question by voice.");
	}

	if (err_msg.empty()) {
		err_msg = can_send_nlp_question2(aplt::chatsrc_speech);
	}

	if (!err_msg.empty()) {
		chinese::curr_pinyin.speak(err_msg);
		return;
	}

	utils::string_map symbols;
	symbols["question"] = question;
	symbols["max"] = str_cast(200);
	const std::string question2 = vgettext2("$question. Keep your response under $max words.", symbols);

	ai_driver_.send_nlp_question(aplt::chatsrc_speech, true, question2, nullptr);
}

void game_instance::aiagent_did_nlp_answer(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens)
{
	VALIDATE_IN_MAIN_THREAD();

	// add_msg_only_log(logtype_nlp_resp, resp, 0, false);
	aiagent_did_nlp_answer2(src, retbool, answer, input_tokens, output_tokens);
}

// if don't deliver @result to NLP(for example spark) after it, let speech_did_recognition_result() return true.
bool game_instance::speech_did_recognition_result(const std::string& result)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(!result.empty(), null_str);
	add_msg_only_log(logtype_speech_recognition, result, 0, false);

	speech_did_recognition_result2(result);

	if (countdown_klink_aplt_task_.valid()) {
		// is being countdown for klink_aplt_task, this speech will do nothing.
		return true;
	}

	utils::string_map symbols;

	std::map<std::string, std::string> matched_vals;
	const aplt::tspeech_sensor* matched_sensor = nullptr;
	bool nick_found = false;
	const aplt::taplt_task* matched_task = cfg_cpp_api_.find_matched_speech_sensor(result, matched_vals, &matched_sensor, nick_found);
	if (matched_task != nullptr) {
		VALIDATE(matched_sensor != nullptr && matched_sensor->countdown_s >= 0, null_str);
		const aplt::taplt_task& hit_task = *matched_task;
		const aplt::tapplet* aplt = aplt::aplt_from_id_ex(applets_, hit_task.aplt_id);

		if (aplt != nullptr && aplt->tasks.count(hit_task.task_id) != 0) {
			if (instance->fg_aplt() == nullptr && !aplt::new_klink_task_disabled2(nullptr)) {
				aplt::ttask_vars task_vars = aplt::clone_env_vars(true);
				for (std::map<std::string, std::string>::const_iterator it = matched_vals.begin(); it != matched_vals.end(); ++ it) {
					const std::string& name = it->first;
					const std::string& val = it->second;
					task_vars.insert_string(name, false, val);
				}

				// iot_sensor has a higher priority than speech_sensor.
				// countdown_klink_aplt_task_.make_sure_clear();
				VALIDATE(!countdown_klink_aplt_task_.valid(), null_str);

				std::string priority_msg;
				if (bg_task_.is_ing() && !bg_task_.priority_can_run(hit_task, &priority_msg)) {
					chinese::curr_pinyin.speak(priority_msg);
					
				} else {
					if (matched_sensor->countdown_s > 0) {
						countdown_klink_aplt_task_.set(matched_sensor->countdown_s, hit_task, task_vars);
					} else {
						request_klink_aplt_task2(hit_task, task_vars);
					}
				}

			} else {
				chinese::curr_pinyin.speak(_("The timing is not right, and speech commands cannot be executed at this time"));
			}

		} else {
			symbols["speech"] = matched_sensor->name;
			std::string msg = vgettext2("What you say satisfies speech command: $speech. But the applet or task associated with this command does not exist", symbols);
			chinese::curr_pinyin.speak(msg);
		}
		return true;

	} else if (nick_found) {
		chinese::curr_pinyin.speak(_("Unsupported speech commands"));
		return true;
	}

	if (bg_task_.is_ing()) {
		VALIDATE(!countdown_klink_aplt_task_.valid(), null_str);
		const aplt::tbg_task::tbase_bg_task2& sys_task = bg_task_.bg_task2();
		if (sys_task.in_task_cpp()) {
			// during a task, and thinks it's a speech ask/question.
			if (bg_task2_.speech_did_recognition_result(result)) {
				return true;
			}
		}
	}

	// deliver @result to NLP.
	return false;
}

bool game_instance::speech_can_request_task()
{
	if (instance->fg_aplt() != nullptr) {
		return false;
	}
	if (aplt::new_klink_task_disabled2(nullptr)) {
		return false;
	}

	if (bg_task_.is_ing()) {
		const aplt::tbg_task::tbase_bg_task2& sys_task = bg_task_.bg_task2();
		if (sys_task.in_task_cpp()) {
			return false;
		}
	}
	return true;
}

void game_instance::init_my_functions()
{
	std::pair<std::map<int, aplt::tfunction_code>::iterator, bool> ins = aplt::functions.insert(std::make_pair(func_distance,
		aplt::tfunction_code(func_distance, "distance", {"position"}, _("function^distance desc"))));
	VALIDATE(ins.second, null_str);
	const aplt::tfunction_code* new_func = &ins.first->second;
	aplt::function_keys.insert(std::make_pair(new_func->id, new_func->code));
}

bool game_instance::calculate2(const aplt::ttask_vars& task_vars, int func_code, const std::vector<std::string>& params, tresult& result)
{
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

	} else if (func_code == func_is_wko_task_finished) {
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

std::string game_instance::func_2_var_exp(const aplt::tfunction_code& func, const std::vector<std::string>& params, bool browser) const
{
	const std::vector<std::string>* params_ptr = &params;
	std::vector<std::string> new_params;
	if (browser) {
		if (func.code == func_distance) {
			const std::string& p0_val = params[0];
			if (curmap_.positions.count(p0_val) != 0) {
				new_params = params;
				new_params[0] = curmap_.positions.find(p0_val)->second.name;
				params_ptr = &new_params;
			}
		}
	}
	return aplt::tfunction::func_2_var_exp(func, *params_ptr, browser);
}

std::string base_node_error_str(const tbase_driver& base_driver, const std::string& task)
{
	VALIDATE(!base_driver.node_started(), null_str);

	std::string result;
	
	utils::string_map symbols;
	symbols["task"] = task;
	if (!base_driver.installed()) {
		symbols["settings"] = _("icon^Settings");
		result = vgettext2("To perform '$task', first select a base drive in '$settings'", symbols);
	} else {
		result = vgettext2("To perform '$task', select valid serial and baudrate for base drive", symbols);
	}
	return result;
}


#include <tf2_ros/transform_listener.h>

#include <opencv2/opencv.hpp>


std::string join_unordered_set_string(const std::unordered_set<std::string>& v, const std::string& s = ",")
{
	std::stringstream ss;
    for (std::unordered_set<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		if (it != v.begin()) {
			ss << s;
		}
        ss << *it;
    }

    return ss.str();
}

#include "minizip/minizip.hpp"

static std::string is_subdirname_exist_pkgname(const std::string& path, const std::string& pkgname)
{
	if (pkgname.empty() || pkgname.find(".") == '/') {
		return "";
	}

	SDL_DIR* dir = SDL_OpenDir(path.c_str());
	if (!dir) {
		return "";
	}

	std::string result;
	SDL_dirent2* dirent;
	while ((dirent = SDL_ReadDir(dir))) {
		if (SDL_DIRENT_DIR(dirent->mode)) {
			if (SDL_strcmp(dirent->name, ".") == 0 || SDL_strcmp(dirent->name, "..") == 0) {
				continue;
			}
			std::string name = dirent->name;
			if (name.find(pkgname) == 0) {
				result = name;
				break;
			}
		}
	}
	SDL_CloseDir(dir);
	return result;
}

static void correct_so_zero_if_need(const std::string& app_dir, const std::string& pkgname, const std::string& unzip_dir)
{
	if (app_dir.empty() || app_dir[app_dir.size() - 1] == '/') {
		SDL_Log("[correct_so_zero_if_need]invalid app_dir: %s", app_dir.c_str());
		return;
	}
	if (pkgname.empty() || pkgname.find(".") == '/') {
		SDL_Log("[correct_so_zero_if_need]invalid pkgname: %s", pkgname.c_str());
		return;
	}
	if (unzip_dir.empty() || unzip_dir[unzip_dir.size() - 1] == '/') {
		SDL_Log("[correct_so_zero_if_need]invalid unzip_dir: %s", unzip_dir.c_str());
		return;
	}
	if (!SDL_IsDirectory(app_dir.c_str())) {
		SDL_Log("[correct_so_zero_if_need]app_dir(%s) isn't directory", app_dir.c_str());
		return;
	}
	if (!SDL_IsDirectory(unzip_dir.c_str())) {
		SDL_Log("[correct_so_zero_if_need]unzip_dir(%s) isn't directory", unzip_dir.c_str());
		return;
	}

	// target_basedir: /data/app/~~07kR77IGzQfrbWwdbtkBJQ==/com.kos.launcher-6kugzuV2AqFjUibzOJm1eQ==
	std::string target_basedir;
	SDL_DIR* dir = SDL_OpenDir(app_dir.c_str());
	if (!dir) {
		SDL_Log("[correct_so_zero_if_need]SDL_OpenDir app_dir(%s) fail", app_dir.c_str());
		return;
	}

	SDL_dirent2* dirent;
	while ((dirent = SDL_ReadDir(dir))) {
		if (SDL_DIRENT_DIR(dirent->mode)) {
			// directory
			if (SDL_strcmp(dirent->name, ".") == 0 || SDL_strcmp(dirent->name, "..") == 0) {
				continue;
			}
			const std::string app_sub_dir = app_dir + "/" + dirent->name;
			const std::string sub_sub_dir = is_subdirname_exist_pkgname(app_sub_dir, pkgname);
			if (!sub_sub_dir.empty()) {
				target_basedir = app_sub_dir + "/" + sub_sub_dir;
				break;
			}
		}
	}
	SDL_CloseDir(dir);
	if (target_basedir.empty()) {
		SDL_Log("[correct_so_zero_if_need]cannot find sub_sub_dir that begin with pkgname(%s) in app_dir(%s)", pkgname.c_str(), app_dir.c_str());
		return;
	}

	const std::string base_apk = target_basedir + "/base.apk";
	if (!SDL_IsFile(base_apk.c_str())) {
		SDL_Log("[correct_so_zero_if_need]base_apk(%s) isn't file", base_apk.c_str());
		return;
	}

	const bool arm64 = true;
	// target_libdir: /data/app/~~07kR77IGzQfrbWwdbtkBJQ==/com.kos.launcher-6kugzuV2AqFjUibzOJm1eQ==/lib/arm64
	const std::string target_libdir = target_basedir + (arm64? "/lib/arm64": "/lib/arm");
	if (!SDL_IsDirectory(target_libdir.c_str())) {
		SDL_Log("[correct_so_zero_if_need]target_libdir(%s) isn't directory", target_libdir.c_str());
		return;
	}
	{
		const std::string probe_file = target_libdir + "/libmain.so";
		tfile file(probe_file, GENERIC_READ, OPEN_EXISTING);
		size_t bytes = 0;
		uint32_t magic = 0;
		if (file.valid()) {
			bytes = posix_fread(file.fp, &magic, sizeof(uint32_t));
			if (bytes == 4 && magic == 0x464c457f) {
				// has right magic, needn't correct.
                SDL_Log("[correct_so_zero_if_need]needn't correct. libmain.so: %s", probe_file.c_str());
				return;
			}
		}
		SDL_Log("[correct_so_zero_if_need]require correct, target_libdir: %s, unzip_dir: %s, %s: %s, bytes: %i, magic: 0x%08x", 
			target_libdir.c_str(), unzip_dir.c_str(), probe_file.c_str(), file.valid()? "is existed": "isn't existed",
			(int)bytes, magic);
	}

	const std::string leagorlibdir = unzip_dir + "/leagorlib";
	SDL_bool ret2 = SDL_MakeDirectory(leagorlibdir.c_str());
    // int32_t uid = multiuser_get_uid(userId, AID_SYSTEM);
    // const std::string leagorlibdir = _pkgdir + "/leagorlib";
    // int ret = prepare_app_dir(leagorlibdir, 0700, uid);
	if (!ret2) {
		SDL_Log("[correct_so_zero_if_need]create directory(%s) fail", leagorlibdir.c_str());
		return;
	}
    bool ret = minizip::unzip_file(base_apk, leagorlibdir, null_str, null_str);
	if (!ret) {
		SDL_DeleteFiles(leagorlibdir.c_str());
		SDL_Log("[correct_so_zero_if_need]unzip_file from %s to %s fail", base_apk.c_str(), leagorlibdir.c_str());
		return;
	}

	const std::string from_libdir = leagorlibdir + (arm64? "/lib/arm64-v8a": "/lib/armeabi-v7a");
	dir = SDL_OpenDir(from_libdir.c_str());
	if (!dir) {
		SDL_DeleteFiles(leagorlibdir.c_str());
		SDL_Log("[correct_so_zero_if_need]SDL_OpenDir %s fail", from_libdir.c_str());
		return;
	}

	while ((dirent = SDL_ReadDir(dir))) {
		if (SDL_DIRENT_DIR(dirent->mode)) {
			// should no directory. maybe has "." and ".."
			// if (SDL_strcmp(dirent->name, ".") && SDL_strcmp(dirent->name, "..")) {
			// }
		} else {
			// file
			const std::string from = from_libdir + "/" + dirent->name;
			const std::string to = target_libdir + "/" + dirent->name;
            bool ret = SDL_CopyFiles(from.c_str(), to.c_str());
            SDL_Log("copy %s to %s, %s", from.c_str(), to.c_str(), ret? "true": "false");
		}
	}
	SDL_CloseDir(dir);

	SDL_DeleteFiles(leagorlibdir.c_str());
}

#include <angles/angles.h>

unsigned char computeCost(double distance)
{
	double weight_ = 5;
	double resolution_ = 0.05;
	double inscribed_radius_ = sqrt(2) * resolution_;

	const unsigned char NO_INFORMATION = 255;
	const unsigned char LETHAL_OBSTACLE = 254;
	const unsigned char INSCRIBED_INFLATED_OBSTACLE = 253;

	unsigned char cost = 0;
	if (distance == 0)
		cost = LETHAL_OBSTACLE;
	else if (distance * resolution_ <= inscribed_radius_)
		cost = INSCRIBED_INFLATED_OBSTACLE;
	else
	{
		// make sure cost falls off by Euclidean distance
		double euclidean_distance = distance * resolution_;
		double factor = exp(-1.0 * weight_ * (euclidean_distance - inscribed_radius_));
		cost = (unsigned char)((INSCRIBED_INFLATED_OBSTACLE - 1) * factor);
	}
	return cost;
}

/*
uint8_t calculate_sum(const uint8_t* data, int len)
{
	VALIDATE(data && len > 0, null_str);

	uint32_t sum = 0;
	for (int i = 1; i < len; i ++) {
		sum += data[i];
	}
	return sum & 0xff;
}
*/


#include <tf2/utils.h>
#include "qr_code.hpp"

#define MAX_IMU_SAMPLES 5

// extern int miktex_pdftex_main(const std::vector<std::string>& args);
extern int miktex_makefmt_main(const std::vector<std::string>& args);
// extern int miktex_makepk_main(const std::vector<std::string>& args);
// extern int miktex_makemf_main(const std::vector<std::string>& args);
extern int miktex_miktex_main(const std::vector<std::string>& args);
// extern int miktex_xetex_main(const std::vector<std::string>& args);
// extern int miktex_dvipdfmx_main(const std::vector<std::string>& args);

void test_new_sdl_api()
{
	const std::string file = game_config::preferences_dir + "/test_SDL_SetTimes.png";

	time_t now = time(nullptr);
	bool ret = SDL_SetTimes(file.c_str(), now - 2, now - 1, now);

	const std::string dir = game_config::preferences_dir + "/test_SDL_SetTimers";
	ret = SDL_SetTimes(dir.c_str(), now - 2, now - 1, now);

	time_t creation_time = 0;
	time_t last_access_time = 0;
	time_t last_write_time = file_3times(dir, &creation_time, &last_access_time);

	const std::string pref_file = game_config::preferences_dir + "/bookmarks1.html";
	const std::string d_file = "d:/backup20251001/bookmarks2.html";
	ret = SDL_MoveFile(d_file.c_str(), pref_file.c_str());
	SDL_Log("SDL_MoveFile: %s", ret? "true": "false");

	unsigned long attributes = SDL_GetAttributes(file.c_str());
	// attributes |= FILE_ATTRIBUTE_READONLY;
	attributes &= ~FILE_ATTRIBUTE_READONLY;
	SDL_SetAttributes(file.c_str(), attributes);

	attributes = SDL_GetAttributes(dir.c_str());
	// attributes |= FILE_ATTRIBUTE_READONLY;
	attributes &= ~FILE_ATTRIBUTE_READONLY;
	SDL_SetAttributes(dir.c_str(), attributes);
}

const FILE* logfile_fp = nullptr;

void test_miktex()
{
	bool pdflatex = false;
	if (false) {
		std::vector<std::string> makefmt_args;
		makefmt_args.push_back("C:/ddksample/apps-src/apps/projectfiles/vc/Release/launcher.exe");
/*
		if (pdflatex) {
			makefmt_args.push_back("--engine=pdftex");
			makefmt_args.push_back("--dest-name=pdflatex");
			makefmt_args.push_back("--no-dump");
			makefmt_args.push_back("pdflatex.ini");
			makefmt_args.push_back("--engine-option=-tcx=cp227.tcx");

		} else {
			makefmt_args.push_back("--engine=xetex");
			makefmt_args.push_back("--dest-name=xelatex");
			makefmt_args.push_back("--no-dump");
			makefmt_args.push_back("xelatex.ini");
		}

		makefmt_args.push_back("--miktex-disable-maintenance");
		makefmt_args.push_back("--miktex-disable-diagnose");
*/
		// --engine=luahbtex --dest-name=lualatex --no-dump lualatex.ini --miktex-disable-maintenance --miktex-disable-diagnose
		makefmt_args.push_back("--engine=luahbtex");
		makefmt_args.push_back("--dest-name=lualatex");
		makefmt_args.push_back("--no-dump");
		makefmt_args.push_back("lualatex.ini");
		makefmt_args.push_back("--miktex-disable-maintenance");
		makefmt_args.push_back("--miktex-disable-diagnose");

		miktex_makefmt_main(makefmt_args);
	}

	if (true) {
		std::vector<std::string> miktex_args;
		miktex_args.push_back("C:/ddksample/apps-src/apps/projectfiles/vc/Release/launcher.exe");

		// formats build lualatex --engine luahbtex
		miktex_args.push_back("formats");
		miktex_args.push_back("build");
/*
		if (pdflatex) {
			miktex_args.push_back("pdflatex");
			miktex_args.push_back("--engine");
			miktex_args.push_back("pdftex");

		} else {
			miktex_args.push_back("xelatex");
			miktex_args.push_back("--engine");
			miktex_args.push_back("xetex");
		}
*/
		// lualatex --engine luahbtex
		miktex_args.push_back("lualatex");
		miktex_args.push_back("--engine");
		miktex_args.push_back("luahbtex");

		miktex_miktex_main(miktex_args);
	}

	if (false && pdflatex) {
		std::vector<std::string> pdftex_args;
		pdftex_args.push_back("C:/ddksample/apps-src/apps/projectfiles/vc/Release/launcher.exe");

		bool top = true;
		if (top) {
			// -synctex=1 -undump=pdflatex c:/ddksample/test2.tex
			pdftex_args.push_back("-synctex=-1"); // -synctex=1
			pdftex_args.push_back("-undump=pdflatex");
			pdftex_args.push_back("-verbose");
#ifdef _WIN32
			pdftex_args.push_back("c:/ddksample/test2.tex");
#else
			pdftex_args.push_back("/sdcard/apk/test2.tex");
#endif

		} else {
			// --miktex-disable-maintenance --miktex-disable-diagnose --interaction=nonstopmode --initialize --halt-on-error --alias=pdflatex --job-name=pdflatex -tcx=cp227.tcx --enable-etex pdflatex.ini
			pdftex_args.push_back("--miktex-disable-maintenance");
			pdftex_args.push_back("--miktex-disable-diagnose");
			pdftex_args.push_back("--interaction=nonstopmode");
			pdftex_args.push_back("--initialize");
			pdftex_args.push_back("--halt-on-error");
			pdftex_args.push_back("--alias=pdflatex");
			pdftex_args.push_back("--job-name=pdflatex");
			pdftex_args.push_back("-tcx=cp227.tcx");
			pdftex_args.push_back("--enable-etex");
			pdftex_args.push_back("pdflatex.ini");
		}

#ifdef pdfTeX
		// miktex_pdftex_main(pdftex_args);
#else
		// miktex_xetex_main(pdftex_args);
#endif

	}

	std::vector<std::string> texs;
	if (game_config::os == os_windows) {
		// texs.push_back("c:/ddksample/test2.tex");
		texs.push_back("c:/ddksample/test3.tex");		
		// texs.push_back("c:/ddksample/test0.tex");
		// texs.push_back("c:/ddksample/test2.tex");
	} else {
		texs.push_back("/sdcard/apk/test2.tex");
		texs.push_back("/sdcard/apk/test3.tex");
		texs.push_back("/sdcard/apk/test0.tex");
		texs.push_back("/sdcard/apk/test2.tex");
	}
	for (std::vector<std::string>::const_iterator it = texs.begin(); it != texs.end(); ++ it) {
		const std::string& tex = *it;
		SDL_Log("convert tex(%s) to pdf...", tex.c_str());
		if (false && !pdflatex) {
			std::vector<std::string> pdftex_args;
			pdftex_args.push_back("C:/ddksample/apps-src/apps/projectfiles/vc/Release/launcher.exe");

			bool top = true;
			if (top) {
	/*
				// -synctex=-1 -undump=xelatex c:/ddksample/test2.tex
				// -synctex=1 --fmt=lualatex c:/ddksample/test3.tex
				// pdftex_args.push_back("-synctex=-1"); // -synctex=1
				pdftex_args.push_back("-undump=xelatex");
				pdftex_args.push_back("-no-pdf");
				// pdftex_args.push_back("-verbose");
	#ifdef _WIN32
				pdftex_args.push_back("c:/ddksample/test3.tex");
	#else
				pdftex_args.push_back("/sdcard/apk/test2.tex");
	#endif
	*/
				// -synctex=-1 --fmt=lualatex c:\ddksample\test3.tex
				pdftex_args.push_back("-synctex=-1");
				pdftex_args.push_back("--fmt=lualatex");
				pdftex_args.push_back(tex);
	
			} else {
	/*
				// --miktex-disable-maintenance --miktex-disable-diagnose --interaction=nonstopmode --initialize --halt-on-error --alias=xelatex --job-name=xelatex --enable-etex xelatex.ini
				pdftex_args.push_back("--miktex-disable-maintenance");
				pdftex_args.push_back("--miktex-disable-diagnose");
				pdftex_args.push_back("--interaction=nonstopmode");
				pdftex_args.push_back("--initialize");
				pdftex_args.push_back("--halt-on-error");
				pdftex_args.push_back("--alias=xelatex");
				pdftex_args.push_back("--job-name=xelatex");
				pdftex_args.push_back("--enable-etex");
				pdftex_args.push_back("xelatex.ini");
	*/
				// --miktex-disable-maintenance --miktex-disable-diagnose --interaction=nonstopmode --initialize --halt-on-error --alias=lualatex --job-name=lualatex lualatex.ini
				pdftex_args.push_back("--miktex-disable-maintenance");
				pdftex_args.push_back("--miktex-disable-diagnose");
				pdftex_args.push_back("--interaction=nonstopmode");
				pdftex_args.push_back("--initialize");
				pdftex_args.push_back("--halt-on-error");
				pdftex_args.push_back("--alias=lualatex");
				pdftex_args.push_back("--job-name=lualatex");
				pdftex_args.push_back("lualatex.ini");

			}

			// miktex_xetex_main(pdftex_args);
			miktex_luahbtex_main(pdftex_args, nullptr);
			if (!top) {
				break;
			}
		}
	}

	if (false) {
		std::vector<std::string> dvipdfmx_args;

		dvipdfmx_args.push_back("dvipdfmx.exe");
		dvipdfmx_args.push_back("c:/ddksample/test3.xdv");
		// miktex_dvipdfmx_main(dvipdfmx_args);
	}
		
}

#include <mupdf/fitz.h>

int test_mudpf()
{
	// c:/ddksample/test3.pdf 1 400 0 > c:/ddksample/page1.ppm
	fz_context *ctx;
	fz_document *doc;
	fz_pixmap *pix;
	fz_matrix ctm;
	int x, y;
	int page_count;

	// std::string input = game_config::os == os_windows? "c:/ddksample/test3.pdf": "/sdcard/apk/test3.pdf";
	std::string input = game_config::os == os_windows? "c:/ddksample/zzzzzz.pdf": "/sdcard/apk/zzzzzz.pdf";
	int page_number = 1;
	float zoom = 200;
	float rotate = 0;

	SDL_Log("{mupdf}input: %s, page_number: %i, zoom: %.3f, rotate: %.3f", input.c_str(), page_number, zoom, rotate);

	/* Create a context to hold the exception stack and various caches. */
	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot create mupdf context\n");
		return EXIT_FAILURE;
	}

	/* Register the default file types to handle. */
	fz_try(ctx)
		fz_register_document_handlers(ctx);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot register document handlers\n");
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	/* Open the document. */
	fz_try(ctx)
		doc = fz_open_document(ctx, input.c_str());
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot open document\n");
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	/* Count the number of pages. */
	fz_try(ctx)
		page_count = fz_count_pages(ctx, doc);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot count number of pages\n");
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	if (page_number < 0 || page_number >= page_count)
	{
		fprintf(stderr, "page number out of range: %d (page count %d)\n", page_number + 1, page_count);
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	/* Compute a transformation matrix for the zoom and rotation desired. */
	/* The default resolution without scaling is 72 dpi. */
	ctm = fz_scale(zoom / 100, zoom / 100);
	ctm = fz_pre_rotate(ctm, rotate);

	/* Render page to an RGB pixmap. */
	fz_try(ctx)
		pix = fz_new_pixmap_from_page_number(ctx, doc, page_number, ctm, fz_device_rgb(ctx), 0);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot render page\n");
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	const std::string outfile = game_config::os == os_windows? "c:/ddksample/page1.ppm": "/sdcard/apk/page1.ppm";
	SDL_Log("{mupdf}output: %s, page_count: %i, pix(x: %i, y: %i, w: %i, h: %i, n: %i)",
		outfile.c_str(), page_count, pix->x, pix->y, pix->w, pix->h, pix->n);

	tfile file(outfile, GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(file.valid(), null_str);

	char szbuf[1024];

	/* Print image data in ascii PPM format. */
	// printf("P3\n");
	int len = SDL_snprintf(szbuf, sizeof(szbuf), "P3\n");
	posix_fwrite(file.fp, szbuf, len);

	// printf("%d %d\n", pix->w, pix->h);
	len = SDL_snprintf(szbuf, sizeof(szbuf), "%d %d\n", pix->w, pix->h);
	posix_fwrite(file.fp, szbuf, len);

	// printf("255\n");
	len = SDL_snprintf(szbuf, sizeof(szbuf), "255\n");
	posix_fwrite(file.fp, szbuf, len);

	for (y = 0; y < pix->h; ++y)
	{
		unsigned char *p = &pix->samples[y * pix->stride];
		for (x = 0; x < pix->w; ++x)
		{
			if (x > 0) {
				// printf("  ");
				szbuf[0] = ' ';
				szbuf[1] = ' ';
				posix_fwrite(file.fp, szbuf, 2);
			}
			// printf("%3d %3d %3d", p[0], p[1], p[2]);
			len = SDL_snprintf(szbuf, sizeof(szbuf), "%3d %3d %3d", p[0], p[1], p[2]);
			posix_fwrite(file.fp, szbuf, len);
			p += pix->n;

			// SDL_Delay(1);
		}
		// printf("\n");
		len = SDL_snprintf(szbuf, sizeof(szbuf), "\n");
		posix_fwrite(file.fp, szbuf, len);
	}

	/* Clean up. */
	fz_drop_pixmap(ctx, pix);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);

	SDL_Log("{mupdf}---test finished ok---");
	return EXIT_SUCCESS;
}

unsigned hash_function(const char *s) {
    unsigned h = 0;
	unsigned hash_size = 211;
    while (*s) {
        h = (h + h + *s++) % hash_size;
    }
    return h;
}

extern int draw_bar_chart();
extern int draw_stacked_bar_chart();
// extern int draw_line_chart();
extern int draw_complete_curved_chart();
extern int draw_monitoring_chart();
extern int test_draw_rounded_rectangle();
extern int load_and_draw_png();
extern int draw_rounded_canvas();
extern int test_cairo();


void test()
{
	// float_2_u16::test();

	// const std::string filename = "C:/ddksample/apps-src/apps/launcher/test_cairo2.cpp";
	// file_is_all_ascii(filename, true);
/*
	draw_bar_chart();
	draw_stacked_bar_chart();
	// draw_line_chart();
	draw_complete_curved_chart();
	draw_monitoring_chart();
	test_draw_rounded_rectangle();
	load_and_draw_png();
	draw_rounded_canvas();
*/
	// test_cairo();

	// surface qrcode_surf = generate_qr("https://u.wechat.com/MAIbg1qhzTsDNMQ152CVRvI?s=2", 300);
	// imwrite(qrcode_surf, "qrcode.png");

	// surface barcode_surf = image::get_image("c:/ddksample/images/barcode_book.jpg");
	// surface barcode_surf = image::get_image("c:/ddksample/images/barcode-EAN_EXTENSION_d5.png");
	// surface barcode_surf = image::get_image("c:/ddksample/images/barcode-EAN_13.png");
	// surface barcode_surf = image::get_image("c:/ddksample/images/barcode-EAN_8.png");
	// surface barcode_surf = image::get_image("c:/ddksample/images/barcode-EAN_8-2.jpg");
	// surface barcode_surf = image::get_image("c:/ddksample/images/barcode-UPC_E.png");
	// VALIDATE(barcode_surf.get() != nullptr, null_str);
	// tsurface_2_mat_lock mat_lock(barcode_surf);
	// cv::Mat rgb;
	// cv::cvtColor(mat_lock.mat, rgb, cv::COLOR_BGRA2BGR);

	// std::vector<cv::Point> corners;
	// std::string barcode = find_barcode(rgb, &corners);

	{
		std::vector<std::string> images;
/*
		images.push_back("c:/ddksample/images/qr-input.jpg");
		images.push_back("c:/ddksample/images/qr-input-1.jpg");
		images.push_back("c:/ddksample/images/qr-input-2.jpg");
		images.push_back("c:/ddksample/images/qr-input-2-small.jpg");
		images.push_back("c:/ddksample/images/IMG_1495-1512x2016.jpg");
		images.push_back("c:/ddksample/images/IMG_1495.JPG");
		images.push_back("c:/ddksample/images/IMG_1496-1512x2016.jpg");
		images.push_back("c:/ddksample/images/IMG_1496.JPG");
		images.push_back("c:/ddksample/images/IMG_1497.JPG");
		images.push_back("c:/ddksample/images/IMG_1498.JPG");
		images.push_back("c:/ddksample/images/IMG_1499.JPG");
		images.push_back("c:/ddksample/images/IMG_1500.JPG");
		images.push_back("c:/ddksample/images/IMG_1517.JPG");
		images.push_back("c:/ddksample/images/IMG_1518.JPG");
		images.push_back("c:/ddksample/images/1.png");
		images.push_back("c:/ddksample/images/IMG_3321.png");
*/
/*
		images.push_back(game_config::preferences_dir + "/qrcode.png");
		for (std::vector<std::string>::const_iterator it = images.begin(); it != images.end(); ++ it) {
			const std::string& image = *it;
			surface surf = image::get_image(image);
			VALIDATE(surf.get() != nullptr, null_str);
			tsurface_2_mat_lock mat_lock(surf);
			cv::Mat rgb;
			cv::cvtColor(mat_lock.mat, rgb, cv::COLOR_BGRA2BGR);
			std::string result = find_qr(rgb);
			SDL_Log("find_qr(%s), result: %s", image.c_str(), result.empty()? "null": result.c_str());
		}
*/
	}

	// const std::string filename = "C:/ddksample/apps-src/apps/launcher/test_cairo2.cpp";
	// file_is_all_ascii(filename, true);
}


// extern int test_slam();
extern void test_ceres_curve_fitting();

void makesure_deviceid()
{
	current_user.deviceid = preferences::login_deviceid();
	if (!utils::is_uuid(current_user.deviceid, false)) {
		current_user.deviceid = utils::create_uuid(false);
		preferences::set_login_deviceid(current_user.deviceid);
	}

	current_user.username = preferences::login_username();
	current_user.pwcookie = preferences::login_pwcookie();
}

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	SDL_SetHint(SDL_HINT_BLE, "1");

	rtc::PhysicalSocketServer ss;
	instance_manager<game_instance> manager(ss, argc, argv, "launcher", "#rose"); // #rose, #kdesktop
	game_instance& game = manager.get();

	latex::enable_latex();

	makesure_deviceid();

	SDL_Log("do_gameloop, main_tid: %" PRIu64 "", (uint64_t)main_tid);
	// VALIDATE_IN_MAIN_THREAD();
	// VALIDATE_NOT_MAIN_THREAD();

	// test_new_sdl_api();
	// test_miktex();
	// test_mudpf();
/*
	std::vector<std::string> files;
	files.push_back("health20260314.dat");
	files.push_back("health20260314.dat.bak");
	files.push_back("health20260315.dat");
	files.push_back("health20260315.dat.bak");
	// files.push_back("health20260316.dat");
	// files.push_back("health20260316.dat.bak");
	files.push_back("health20260317.dat");
	files.push_back("health20260317.dat.bak");
	files.push_back("health20260318.dat");
	files.push_back("health20260318.dat.bak");
	files.push_back("health20260319.dat");
	files.push_back("health20260319.dat.bak");
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string file = game_config::preferences_dir + "/saves/health/" + *it;
		SDL_DeleteFiles(file.c_str());
	}
*/

	if (game_config::os == os_windows) {
		const size_t kMaxBodySize = 100 << 20;
		int ii = 0;
		// test_slam();
		// test_ceres_curve_fitting();
		test();
		// SDL_assert(false);
		// preferences::set_keyboard_style(keyboard_style_rose);
	}

	try {
		if (!preferences::login_username().empty() && !preferences::login_pwcookie().empty()) {
			const std::string username = preferences::login_username();
			const std::string cookie = preferences::login_pwcookie();

			net::cswamp_login2(current_user, net::cswamp_login_type_cookie, username, cookie, nposm, true);
		}

		const bool test_httpd = false;
		if (test_httpd) {
			instance->register_server(server_httpd, nullptr);
		}

		game.register_server(server_rdpd, &game.rdpd_mgr());

		game.set_can_run_var_or_timing_task();

		bool startup_enable_wifi = true;
		if (startup_enable_wifi) {
			SDL_WifiSetEnable(SDL_TRUE);
		}

		for (; ;) {
			game.loadscreen_manager().reset();
			const font::floating_label_context label_manager;
			cursor::set(cursor::NORMAL);

			int res;
			{
				gui2::thome dlg(game.store_slot(), game.rdpd_mgr(), game.pble(), game.privacy(), game.applets(), game.drivers(), game.base_driver(), game.speech_driver(), game.iot_driver(), game.os_info(), 
					game.health());
				dlg.show();
				res = static_cast<gui2::thome::tresult>(dlg.get_retval());
			}

			if (res < gui2::thome::APPLET0) {
				continue;
			}
			const int at = res - gui2::thome::APPLET0;
			if (at < MAX_APPLETS) {
				game.make_sure_clear_countdown_klink_aplt_task();

				const aplt::tapplet& applet = aplt::aplt_from_at(game.applets(), at);
				aplt::texecutor executor(game.lua(), applet);
				executor.run();

			} else if (at == aplt::builtinid_store) {
				gui2::trstore dlg(game.store_slot(), game.camera(), game.applets());
				dlg.show();

			} else if (at == aplt::builtinid_dnn) {
				if (!game.will_enter_sys_module(aplt::builtinid_dnn)) {
					continue;
				}
				game.set_desire_depth_task(dctask_color);

				aplt::tdisable_new_klink_task_lock lock(aplt::tdisable_new_klink_task_lock::reason_dnn);
				gui2::trdnn dlg(game, game.camera(), game.app_cfg());
				dlg.show();
				// res = static_cast<gui2::thome::tresult>(dlg.get_retval());

			} else if (at == aplt::builtinid_explorer) {
				if (!game.will_enter_sys_module(aplt::builtinid_explorer)) {
					continue;
				}
				aplt::tdisable_new_klink_task_lock lock(aplt::tdisable_new_klink_task_lock::reason_explorer);

				if (game_config::os == os_windows) {
					// game.test_did_receive_broadcast();
				}

				SDL_Rect win_rect;
				SDL_Window* sdl_window = get_sdl_window();

				if (game_config::os == os_windows) {
					SDL_GetWindowPosition(sdl_window, &win_rect.x, &win_rect.y);
					SDL_GetWindowSize(sdl_window, &win_rect.w, &win_rect.h);
					// SDL_SetWindowResizable(sdl_window, SDL_FALSE);
					// SDL_SetWindowBordered(get_sdl_window(), SDL_FALSE);
					SDL_MaximizeWindow(sdl_window);
				}

				gui2::trexplorer::tentry extra(null_str, null_str, null_str);
				if (game_config::os == os_windows) {
					extra = gui2::trexplorer::tentry(game_config::path + "/data/gui/default/scene", "gui/scene", "misc/dir_res.png");
				} else if (game_config::os == os_android) {
					extra = gui2::trexplorer::tentry("/sdcard", "/sdcard", "misc/dir_res.png");
				}

				gui2::texplorer_slot slot(game.rdpd_mgr(), game.pble(), game.privacy());
				bool click_open_dir = game.rdpd_mgr().started()? game.rdpd_mgr().client_is_mobile(): game_config::mobile;
				// When ios client connect to the server, server may be running trexplorer, 
				// and this is opened by the previous windows client, which will cause ios confusion. 
				// It's better to use unified rules for the all os.
				click_open_dir = true;
				gui2::trexplorer dlg(slot, null_str, extra, false, click_open_dir);
				dlg.show();
				int res = dlg.get_retval();

				if (game_config::os == os_windows) {
					// above SDL_MaximizeWindow myabe result recreate SDL_window, so get again.
					// sdl_window = get_sdl_window();

					// SDL_SetWindowPosition(sdl_window, win_rect.x, win_rect.y);
					// SDL_SetWindowSize(sdl_window, win_rect.w, win_rect.h);

					// if use SDL_SetWindowPosition+SDL_SetWindowSize, maybe cann't clear SDL_WINDOW_MAXIMIZED,
					// it will result next SDL_MaximizeWindow do nothing.
					SDL_RestoreWindow(sdl_window);
				}

				if (res != gui2::twindow::OK) {
					continue;
				}

			} else if (at == aplt::builtinid_map) {
				utils::string_map symbols;
				symbols["map"] = aplt::all_fake_applets.find(aplt::builtinid_map)->second.name;

				std::unique_ptr<tspecial_mapop_mode_lock> lock;
				if (game.base_driver().installed() && !game.base_driver().slot->moveable()) {
					gui2::show_message(null_str, vgettext2("Base works in unmovable mode and cannot enter '$map'.", symbols));
					continue;
				}

				if (game.ros_instance().in_map_viewer()) {
					if (game.moveit_aplt_task().get() != nullptr) {
						symbols["moveit"] = aplt::all_fake_applets.find(aplt::builtinid_moveit)->second.name;
						symbols["dcamera"] = aplt::all_fake_applets.find(aplt::builtinid_dcamera)->second.name;

						gui2::show_message(null_str, vgettext2("It is operating '$moveit' and cannot enter '$map'. To view the camera video, go to '$dcamera'", symbols));
						continue;
					}
					lock.reset(new tspecial_mapop_mode_lock(mode_navigation));

				} else if (!game.will_enter_sys_module(aplt::builtinid_map)) {
					// if has bg_task, and !has_task(), require stop. ex. image_recognize
					continue;
				}

				game.start_map_controller(nullptr);

			} else if (at == aplt::builtinid_moveit) {
				if (!game.moveit_driver().installed()) {
					utils::string_map symbols;
					symbols["target"] = aplt::all_fake_applets.find(aplt::builtinid_moveit)->second.name;
					symbols["driver"] = _("Moveit driver");
					symbols["settings"] = aplt::all_fake_applets.find(aplt::builtinid_settings)->second.name;
					gui2::show_message(null_str, vgettext2("To enter '$target', first select a valid $driver in '$settings'.", symbols));
					continue;
				}
				if (!game.base_driver().node_started()) {
					const std::string msg = base_node_error_str(game.base_driver(), aplt::all_fake_applets.find(aplt::builtinid_moveit)->second.name);
					gui2::show_message(null_str, msg);
					continue;
				}
				if (!game.will_enter_sys_module(aplt::builtinid_moveit)) {
					continue;
				}
				gui2::tmoveit dlg(game.rdpd_mgr(), game.pble(), game.privacy(), game.moveit_driver(), game.drivers(), game.ros_instance(), game.robot_imu());
				dlg.show();

			} else if (at == aplt::builtinid_dcamera) {
				bool in_dcamera_viewer = game.moveit_aplt_task().get() != nullptr || game.bg_task().in_task_cpp2();
				if (!in_dcamera_viewer) {
					if (!game.will_enter_sys_module(aplt::builtinid_dcamera)) {
						continue;
					}
				}
				if (game.dcamera_driver().installed()) {
					if (!game.base_driver().node_started()) {
						const std::string msg = base_node_error_str(game.base_driver(), aplt::all_fake_applets.find(aplt::builtinid_dcamera)->second.name);
						gui2::show_message(null_str, msg);
						continue;
					}
				}
				// ----??
/*
				if (!game.moveit_driver().installed()) {
					utils::string_map symbols;
					symbols["moveit"] = aplt::all_fake_applets.find(aplt::builtinid_dcamera)->second.name;
					symbols["settings"] = aplt::all_fake_applets.find(aplt::builtinid_settings)->second.name;
					gui2::show_message(null_str, vgettext2("To operate '$moveit', first select a moveit drive in '$settings'", symbols));
					continue;
					continue;
				}
*/
				// ---
				gui2::tdcamera dlg(game.applets(), game.rdpd_mgr(), game.pble(), game.privacy(), game.dcamera_driver(), game.drivers(), 
					game.temp_task(), game.ros_instance(), game.camera(), game.moveit_aplt_task());
				dlg.show();

			} else if (at == aplt::builtinid_center) {
				// gui2::tcenter dlg(game.rdpd_mgr(), game.pble(), game.privacy(), game.ros_instance(), game.temp_task(), game.applets(), game, game.curmap(), game.bg_task(), game.base_driver(), game.speech_driver(), game.dcamera_driver(), game.ai_driver(), game.saves_courseware_dir(), nullptr);
				// dlg.show();
				game.show_gui_center(nullptr);


			} else if (at == aplt::builtinid_mic) {
				if (!game.speech_driver().installed()) {
					utils::string_map symbols;
					symbols["target"] = aplt::all_fake_applets.find(aplt::builtinid_mic)->second.name;
					symbols["driver"] = _("Speech driver");
					symbols["settings"] = aplt::all_fake_applets.find(aplt::builtinid_settings)->second.name;
					gui2::show_message(null_str, vgettext2("To enter '$target', first select a valid $driver in '$settings'.", symbols));
					continue;
				}
				game.start_chart_controller();

			} else if (at == aplt::builtinid_settings) {
				if (!game.will_enter_sys_module(aplt::builtinid_settings)) {
					continue;
				}

				// tros_instance::tmoveit_model_lock lock(game.drivers(), game.ros_instance());
				gui2::tsettings dlg(game.rdpd_mgr(), game.pble(), game.privacy(), game.ros_instance(), game.curmap(), 
					game.applets(), game, game.cfg_cpp_api(), game.base_driver(), game.moveit_driver(), game.laser_driver(), 
					game.dcamera_driver(), game.iot_driver(), game.speech_driver(), game.ai_driver(), game.robot_imu(), game.saves_map_dir());
				dlg.show();
				game.drivers().refresh();

			} else if (at == aplt::builtinid_klink) {
				gui2::tvar_editor_slot slot(game.rdpd_mgr(), game.pble(), game.privacy(), game.curmap());
				gui2::tklink dlg(game.rdpd_mgr(), game.pble(), game.privacy(), slot, game.applets(), game.curmap(), game.cfg_cpp_api(), game.bg_task(), game.base_driver(), game.moveit_driver());
				dlg.show();

			} else if (at == aplt::builtinid_speech) {
				if (game.bg_task().task_cpp_aplt(nullptr) == &aplt::fake_aplt) {
					if (!game.will_enter_sys_module(aplt::builtinid_speech)) {
						continue;
					}
				}
				gui2::tspeech2 dlg(game.rdpd_mgr(), game.pble(), game.privacy(), game.applets(), game.curmap(), game.cfg_cpp_api(), game.bg_task());
				dlg.show();

			} else if (at == aplt::builtinid_task) {
				if (game.bg_task().task_cpp_aplt(nullptr) == &aplt::fake_aplt) {
					if (!game.will_enter_sys_module(aplt::builtinid_task)) {
						continue;
					}
				}
				gui2::ttask2 dlg(game.rdpd_mgr(), game.pble(), game.privacy(), game.applets(), game.curmap(), game.cfg_cpp_api(), game.bg_task());
				dlg.show();

			} else if (at == aplt::builtinid_artifact) {
				gui2::tcourseware2 dlg(game.rdpd_mgr(), game.pble(), game.privacy(), game, game.applets(), game.cfg_cpp_api(), game.speech_driver(), game.saves_courseware_dir());
				dlg.show();

			} else if (at == aplt::builtinid_mkscript || at == aplt::builtinid_mkcourse) {
				const aplt::tapplet* desire_aplt = aplt::aplt_from_bundleid(game.applets(), aplt::get_bundleid(aplt::bundleid_leagor_khome));
				if (desire_aplt == nullptr) {
					utils::string_map symbols;
					symbols["target"] = aplt::all_fake_applets.find(aplt::builtinid_mkscript)->second.name;
					symbols["aplt"] = _("kHome");
					symbols["store"] = aplt::all_fake_applets.find(aplt::builtinid_store)->second.name;
					gui2::show_message(null_str, vgettext2("Before using the '$target', please install the '$aplt' first. You can download and install it from the $store.", symbols));
					continue;
				}

				if (at == aplt::builtinid_mkscript) {
					game.start_mkscript_controller();

				} else {
					gui2::tmkcourse dlg(game.rdpd_mgr(), game.pble(), game.privacy(), game.applets());
					dlg.show();
				}

			} else if (at == aplt::builtinid_health) {
				game.start_health_controller();
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

	ros::shutdownTimerManagers();
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