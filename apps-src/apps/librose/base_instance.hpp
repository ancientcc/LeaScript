/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_BASE_INSTANCE_HPP_INCLUDED
#define LIBROSE_BASE_INSTANCE_HPP_INCLUDED

#include "rose_config.hpp"
#include "font.hpp"
#include "area_anim.hpp"
#include "hero.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "filesystem.hpp"
#include "serialization/preprocessor.hpp"
#include "config_cache.hpp"
#include "cursor.hpp"
#include "loadscreen.hpp"
#include "lobby.hpp"
#include "live555d.hpp"
#include "aplt.hpp"
#include "camera.hpp"
#include "logs.pb.h"

// protobuf
#include <google/protobuf/message_lite.h>

// webrtc
#include <rtc_base/thread_checker.h>
#include <rtc_base/thread.h>
#include <rtc_base/physicalsocketserver.h>

// chromium
#include <base/rose/task_environment.h>
#include <net/server/http_server_rose.hpp>

// 
#include "scripts/rose_lua_kernel.hpp"

#define INVALID_UINT32_ID		0

class animation;
class trtc_client;
class tble;

namespace rtc {
// SDLThread. Automatically pumps wakeup and IO messages.

class SDLThread : public Thread
{
public:
	SDLThread(PhysicalSocketServer* ss)
		: Thread(ss)
	{
		rtc::ThreadManager::Instance()->SetCurrentThread(this);
	}
	virtual ~SDLThread()
	{
		// Stop();
		SDL_Log("~---SDLThread()--- X");
	}
	void pump()
	{
		Message msg;
		size_t max_msgs = std::max<size_t>(1, size());
		// require detect io signal, so io_process of Get use true.
		for (; max_msgs > 0 && Get(&msg, 0, true); --max_msgs) {
			Dispatch(&msg);
		}
	}
	void clear_msg(MessageHandler* phandler, uint32_t id = MQID_ANY)
	{
		Clear(phandler, id);
	}
};

}

struct tfarthest_filter
{
public:
	static constexpr int MAX_SAMPLES_FOR_FILTER = 10;
	static constexpr int MIN_SAMPLES_FOR_FILTER = 3;
	tfarthest_filter(int max_samples = nposm)
		: reset_samples_threshold_ms_(5000) // 5s
		, max_samples_(max_samples == nposm? 4: max_samples)
		, sync_val_(0.0)
	{
		VALIDATE(max_samples_ >= MIN_SAMPLES_FOR_FILTER && max_samples_ <= MAX_SAMPLES_FOR_FILTER, null_str);
		reset_anti_shike_samples();
	}

	void reset_anti_shike_samples();
	void nonmain_did_received(double newly);
	double average_sample(double newly);

	double sync_val() const { return sync_val_.load(); }

private:
	const int reset_samples_threshold_ms_;
	const int max_samples_;
	int next_sample_index_;
	double anti_shake_samples_[MAX_SAMPLES_FOR_FILTER];
	double temp_4_average_[MAX_SAMPLES_FOR_FILTER];
	uint32_t last_recv_sample_ticks_;
	int recv_samples_;

	std::atomic<double> sync_val_;
};

/*
template<typename T = float>
class tlow_pass_filter
{
public:
    tlow_pass_filter(T alpha = 0.3f, uint32_t reset_threshold_ms = 500)
        : filtered_(0)
		, initialized_(false)
		, alpha_(alpha)
        , reset_threshold_ms_(reset_threshold_ms)
		, last_recv_sample_ticks_(0)
	{
		VALIDATE(alpha > 0.05f, null_str);
	}

    // Update the filtered value.
    T update(T raw)
	{
        const uint32_t now = SDL_GetTicks();
        
        if (!initialized_) {
            filtered_ = raw;
            initialized_ = true;
            last_recv_sample_ticks_ = now;
            return filtered_;
        }
        
        // Check whether a reset is needed.
        uint32_t delta = now - last_recv_sample_ticks_;
        if (delta > reset_threshold_ms_) {
            reset(raw);
            return filtered_;
        }
        
        // Normal filtering.
        filtered_ = alpha_ * raw + (static_cast<T>(1.0) - alpha_) * filtered_;
        last_recv_sample_ticks_ = now;
        return filtered_;
    }

    // Reset the filter.
    void reset(T value = static_cast<T>(0.0))
	{
        filtered_ = value;
        last_recv_sample_ticks_ = SDL_GetTicks();
    }

    // Get the current filtered value.
    T get_filtered() const { return filtered_; }

    // Set parameters.
    void set_alpha(T alpha) { alpha_ = alpha; }

private:
    T filtered_;
    bool initialized_;
    T alpha_;

	const uint32_t reset_threshold_ms_;
    uint32_t last_recv_sample_ticks_;
};

class trpy_sensor
{
public:
    trpy_sensor();
    ~trpy_sensor();
    
	// Initialize the sensor.
    bool init();
    
    // Update sensor data (call in the main loop).
    void update();
    
    // Determine whether it is perpendicular to the ground.
    bool is_vertical(float threshold = 70.0f) const;
    
    // Get the angle.
    float get_pitch() const { return pitch_; }
    float get_roll() const { return roll_; }
    bool has_data() const { return has_data_; }
    
    // Clean up resources.
    void quit();
    
private:
    SDL_Sensor* accel_sensor_;
    float pitch_;
    float roll_;
    bool has_data_;
    bool initialized_;

	tlow_pass_filter<float> pitch_filter_;
};
*/

enum {APLT_MSG_WILLRUN, APLT_MSG_DIDDLGCLOSE, APLT_MSG_DIDTERMINATE};
enum {logtype_aplt_task, logtype_iot_event, logtype_nlp_resp, logtype_center_chat, logtype_speech_recognition, logtype_warn, logtype_count};

// tbase_msg_subscriber support callbacks in main-thread. 
// if not-main-thread, use tnot_main_base_msg_subscriber.
class tbase_msg_subscriber
{
public:
	tbase_msg_subscriber();
	virtual ~tbase_msg_subscriber();

	// timing
	virtual void bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task) {}
	virtual void bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task) {}
	virtual void logs_pb_log_added(int count, const pb2::tlog& log) {}

	// The '2' is added to distinguish it from the tslot_subscriber
	virtual void iot_did_heartbeats2(const std::set<aplt::tiot_heartbeat>& heartbeats) {}
	virtual void iot_did_events2(const std::set<aplt::tiot_event>& events) {}

	virtual std::string can_send_nlp_question2(int src) const { return null_str; }
	virtual void aiagent_did_nlp_answer2(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens) {}
	virtual void speech_did_recognition_result2(const std::string& result) {}

	// base scene subtask
	virtual void base_scene_state_changed(const aplt::tbase_scene& scene, int to_state) {}

	virtual void pinyin_did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start) {}

	virtual void klink_tasks_changed2() {}

protected:
};

class tnot_main_base_msg_subscriber
{
public:
	tnot_main_base_msg_subscriber()
		// : subscriber_ready_(false)
	{}

	virtual ~tnot_main_base_msg_subscriber()
	{
		// VALIDATE(!subscriber_ready_, null_str);
	}

	virtual void speech_did_capture_audio2(const uint8_t* stream, int len) {}

	// bool subscriber_ready() const
	// {
	//	VALIDATE_NOT_MAIN_THREAD();
	//	return subscriber_ready_; 
	// }

	threading::mutex& subscriber_mutex() { return subscriber_mutex_; }

protected:
	// It is only used when the callback is on a non-main thread. for example speech_did_capture_audio2().
	// bool subscriber_ready_;
	threading::mutex subscriber_mutex_;
};

class base_instance
{
public:
	struct tpre_setmode_settings {
		tpre_setmode_settings(bool& landscape, bool& silent_background, bool& fullscreen, uint32_t& startup_servers,
			int& pc_default_font_size, int& mobile_default_font_size, int& longpress_time, int& min_width, int& min_height, uint16_t& server_check_port)
			: landscape(landscape)
			, silent_background(silent_background)
			, fullscreen(fullscreen)
			, startup_servers(startup_servers)
			, pc_default_font_size(pc_default_font_size)
			, mobile_default_font_size(mobile_default_font_size)
			, longpress_time(longpress_time)
			, min_width(min_width)
			, min_height(min_height)
			, server_check_port(server_check_port)
		{}

		bool& landscape;
		bool& silent_background;
		bool& fullscreen;
		uint32_t& startup_servers;
		int& pc_default_font_size;
		int& mobile_default_font_size;
		int& longpress_time;
		int& min_width;
		int& min_height;
		uint16_t& server_check_port;
	};

	// friend void VALIDATE_IN_MAIN_THREAD();
	// friend void VALIDATE_NOT_MAIN_THREAD();

	static void prefix_create(const std::string& _app, const std::string& channel);

	base_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv, int sample_rate = 44100, size_t sound_buffer_size = 4096);
	virtual ~base_instance();

	void initialize();
	void uninitialize();
	virtual tlobby* create_lobby();

	loadscreen::global_loadscreen_manager& loadscreen_manager() const { return *loadscreen_manager_; }

	bool init_language();
	bool init_video();
	bool load_data_bin();

	virtual void app_load_pb() {}
	virtual std::pair<std::string, ::google::protobuf::MessageLite*> app_pblite_from_type(int type) { return std::make_pair(null_str, nullptr); }

	// if app is ready to output's cfg, return non-empty cfg, else return empty cfg.
	virtual config app_critical_prefs() { return config::empty_cfg; }

	virtual std::string app_defaut_title() const { return null_str; }

	virtual void app_handle_clipboard_paste(const std::string& text) {}

	// Gets the underlying screen object.
	CVideo& video() { return video_; }

	const config& app_cfg() const { return app_cfg_; }
	const config& core_cfg() const { return core_cfg_; }

	// it is called by studio, other app don't use it.
	void reload_data_bin(const config& data_cfg);
	void set_data_bin_cfg(const config& cfg);

	bool is_loading() { return false; }

	bool change_language();
	virtual int show_preferences_dialog();

	virtual void regenerate_heros(hero_map& heros, bool allow_empty);
	hero_map& heros() { return heros_; }

	rose_lua_kernel& lua() { return *lua_; }
	std::map<aplt::taplt_key, aplt::tapplet>& applets() { return applets_; }
	const aplt::tapplet* fg_aplt() const { return fg_aplt_; }
	void set_fg_aplt(const aplt::tapplet* aplt)  { fg_aplt_ = aplt; }
	aplt::tbg_task& bg_task() { return bg_task_; }
	void stop_bg_task(bool fail_retry);
	void stop_bg_task2();
	bool stop_bg_task_if_runing(const std::string& warnning, const std::string& log);
	bool lua_did_navigation_start(const aplt::tapplet& aplt, const std::string& position_uuid, const fn_navigation_bh& luafunc);

	void bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task);
	void bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task, bool log);
	void logs_pb_log_added(int count, const pb2::tlog& log);
	void iot_did_heartbeats2(const std::set<aplt::tiot_heartbeat>& heartbeats);
	void iot_did_events2(const std::set<aplt::tiot_event>& events);
	std::string can_send_nlp_question2(int src) const;
	void aiagent_did_nlp_answer2(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens);
	void speech_did_recognition_result2(const std::string& result);
	void base_scene_state_changed(const aplt::tbase_scene& scene, int to_state);
	virtual void pinyin_did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start);
	void klink_tasks_changed2();

	pb2::tlogs& logs_pb() { return logs_pb_; }
	void load_logs_pb(int type, int max_days, int max_logs);
	void adjust_logs_pb();
	void add_aplt_task_log(const std::string& aplt_id, const std::string& task_id, const std::string& device_id, int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent);
	void add_msg_only_log(int type, const std::string& msg, uint64_t tokens, bool aiagent);
	void add_aplt_ts_msg_log(int type, const std::string& aplt_id, int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent);
	void undelivered_logs_pump();

	// logs for listbox
	void join_msg_tokens(const std::string& msg, uint64_t tokens, std::string& msg2) const;
	std::string get_log_msg2(const pb2::tlog& log) const;
	void logs_reload_log_list(gui2::tlistbox& list, bool aiagent_dlg);
	void logs_validate_log_list(gui2::tlistbox& list, bool aiagent_dlg);
	void logs_pb_log_added2(gui2::tlistbox& list, int count, const pb2::tlog& log, bool aiagent_dlg);

	virtual void app_fill_anim_tags(std::map<const std::string, int>& tags) {};
	virtual void fill_anim(int at, const std::string& id, bool area, bool tpl, const config& cfg);

	void pre_create_renderer();
	void post_create_renderer();

	void clear_anims();
	const std::multimap<std::string, const config>& utype_anim_tpls() const { return utype_anim_tpls_; }
	const animation* anim(int at) const;

	void init_locale();
	void handle_app_event(Uint32 type);
	void handle_window_event(SDL_Window* window, Uint32 type);
	void handle_resize_screen(const int width, const int height);

	bool foreground() const { return foreground_; }
	bool terminating() const { return terminating_; }
	bool minimized() const { return minimized_; }
	bool silent_background() const { return silent_background_; }

	void theme_switch_to(const std::string& app, const std::string& id);
	void will_longblock();

	uint32_t get_callback_id() const;
	uint32_t background_connect(const std::function<bool (uint32_t ticks, bool screen_on)>& callback);
	void handle_background(bool screen_on);
	virtual void handle_tray_event(uint32_t type) {};
	// virtual void handle_rpy_received(double roll, double pitch, double yaw);

	void set_rtc_client(trtc_client* chat) { rtc_client_ = chat; }
	trtc_client* rtc_client() const { return rtc_client_; }

	void set_current_ble(tble* ble);
	tble* current_ble() const { return current_ble_; }

	rtc::SDLThread& sdl_thread() { return sdl_thread_; }
	void block_slice();
	void nonblock_slice();

	live555d::tmanager& live555d_mgr() { return live555d_mgr_; }
	net::thttpd_manager& httpd_mgr() { return httpd_mgr_; }
	void handle_http_request(const net::HttpServerRequestInfo& info, net::tresponse_data& resp);

	// for ddn_camera
	struct tmsg_data_other_id: public rtc::MessageData {
		explicit tmsg_data_other_id(const std::function<void (int id)>& _func)
			: func(_func)
		{
			VALIDATE(_func != NULL, null_str);
		}

		std::function<void (int id)> func;
	};

	class tmessage_handler: public rtc::MessageHandler
	{
	public:
		tmessage_handler(base_instance& instance)
			: send_helper(this)
			, instance_(instance)
		{}

		void OnMessage(rtc::Message* msg) override
		{
			switch (msg->message_id) {
			case gui2::tdialog::POST_MSG_SWITCH_CAMERA:
			case gui2::tdialog::POST_MSG_CANCEL_CAMERA:
			case gui2::tdialog::POST_MSG_SNAPSHOT_CAMERA:
				instance_.camera_.camera_OnMessage(msg);
				break;

			default:
				{
					tmsg_data_other_id* pdata = static_cast<tmsg_data_other_id*>(msg->pdata);
					pdata->func(msg->message_id);
				}
				break;
			}

			if (msg->pdata) {
				delete msg->pdata;
				msg->pdata = nullptr;
			}
		}

	public:
		twebrtc_send_helper send_helper;

	private:
		base_instance& instance_;
	};
	tmessage_handler& msg_handler() { return msg_handler_; }
	tcamera& camera() { return camera_; }

	void register_server(int flag, tserver_* server);
	void unregister_server(int flag);
	bool server_registered(int flag) const;
	bool servers_ready() const;
	uint32_t next_check_ip_ticks() const { return next_check_ip_ticks_; }
	void check_ip_slice();
	void make_ip_invalid();
	uint32_t current_ip() const;

	void register_base_msg_subscriber(tbase_msg_subscriber* subscriber);
	void unregister_base_msg_subscriber(tbase_msg_subscriber* subscriber);

	bool will_enter_sys_module(const std::string& reason);
	bool will_enter_sys_module(int fakeid);
	virtual void set_desire_depth_task(int task) {}
	virtual void popup_rcamera(bool pre) {}

	virtual bool app_update_ip(const uint32_t ipv4, const int prefixlen, const uint32_t gateway) { return false; }
	virtual void app_rose_button_clicked(int retval) {}
	virtual bool app_start_navigation(const aplt::tapplet& aplt, const std::string& position, const fn_navigation_bh& luafunc) { return true; }
	// virtual bool app_did_navigation_bh_moveit(bool result, const aplt::tapplet& aplt, const aplt::taplt_task& aplt_task) { return false; }
	virtual bool app_did_navigation_bh_use_cpp(bool result, const aplt::tapplet& aplt, const aplt::taplt_task& aplt_task, const aplt::tapplet::ttask& cfg_task) { return false; }
	virtual void app_stop_single_task_use_cpp(const aplt::taplt_task& aplt_task, const aplt::tapplet& aplt, const aplt::tapplet::ttask& cfg_task) {}
	virtual void app_ble_center_will_connect() {}
	virtual bool app_handle_aplt_msg(const aplt::tapplet* aplt, int msg) { return true; }
	// applet-gui use to notify preferences changed.
	virtual void app_fg_aplt_send_cpp_id(const aplt::tapplet& aplt, const std::string& window_id, int cpp_id) {}
	virtual std::string app_fg_aplt_lua_pre_navigation_start(const aplt::tapplet& aplt) { return null_str; }
	virtual bool app_in_pure_task_cpp() const { return false; }
	virtual bool app_can_run_var_or_timing_task() const { return true; }
	virtual const aplt::taplt_task* app_shedule_var_task(aplt::ttask_vars& task_vars_result) { return nullptr; }
	virtual aplt::tbg_task::tbase_bg_task2* app_request_klink_aplt_task(const aplt::taplt_task& aplt_task, const aplt::ttask_vars& task_vars) { return nullptr; }
	virtual void app_clear_taskpoint() {}
	virtual void app_request_retry_aplt_task() {}

	virtual int get_serial_path(int driver_type, std::string& path, std::string& model) { return nposm;}
	virtual int sdl_GetTtyUSB(SDL_ttyUSB** ppttyUSB)
	{
		*ppttyUSB = nullptr;
		return 0;
	}

	void set_sdl_reboothandler(SDL_RebootHandler handler, void* param);
	void backup_did_file_write(const std::string& file, int backup_type);
	void backup_pump();

	bool invalidate_layout(bool clear);

	tpoint get_landscape_size() const;

	void set_fullscreen(bool fullscreen);
	enum {orientation_portrait, orientation_landscape};
	bool set_orientation(int orientation, bool with_mode);
	bool set_mode();

	// void increment_textdomain_usage(const std::string& aplt_id);
	void increment_textdomain_usage(const aplt::tapplet& aplt);
	// void decrement_textdomain_usage(const std::string& aplt_id);
	void decrement_textdomain_usage(const aplt::tapplet& aplt);

	// Other thread how to use get_invoke_user_lock()?
	// ---------------------------
	// std::unique_ptr<base_instance::tinvoke_user_lock> lock = instance->get_invoke_user_lock();
	// if (lock.get() == nullptr) {
	//    base_instance has been in deconstructing, must not call instance->sdl_thread().Invoke<...>(...).
	//	  return;
	// }
	// ....
	// instance->sdl_thread().Invoke<void>(RTC_FROM_HERE, rtc::Bind(&base_instance::handle_http_request, instance, ...));
	// ---------------------------
	twebrtc_send_helper& webrtc_send_helper() { return send_helper_; }

	aplt::trpy_sensor& rpy_sensor() { return rpy_sensor_; }
	// tfarthest_filter& pitch_filter() { return pitch_filter_; }

private:
	virtual void app_load_settings_config(const config& cfg) {}
	virtual void app_pre_setmode(tpre_setmode_settings& settings) {}

	virtual void app_init_locale(const std::string& intl_dir) {}
	virtual void app_load_data_bin() {}
	virtual void app_setup_user_data_dir() {}

	virtual void app_post_lua() {}
	virtual void app_aplt_slice() {}
	virtual void app_ros_slice() {}

	virtual void app_terminating() {}
	virtual void app_willenterbackground() {}
	virtual void app_didenterbackground() {}
	virtual void app_willenterforeground() {}
	virtual void app_didenterforeground() {}
	virtual void app_lowmemory() {}

	virtual void app_pre_create_renderer() {}
	virtual void app_post_create_renderer() {}
	
	virtual void app_handle_http_request(const net::HttpServerRequestInfo& info, net::tresponse_data& resp) {}
	virtual void app_bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task) {}
	virtual void app_bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task) {}

	virtual void app_uninitialize() {}

	void did_enter_background();
	void did_enter_foreground();

	void background_disconnect(const uint32_t id);
	void background_disconnect_th(const uint32_t id);

	void add_logs_pb_log(int type, const std::string& aplt_id, const std::string& task_id, const std::string& device_id, 
		int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent);

	class tdisable_check_ip_lock
	{
	public:
		tdisable_check_ip_lock(base_instance& _instance)
			: instance_(_instance)
		{
			VALIDATE(!instance_.disable_check_ip_, null_str);
			instance_.disable_check_ip_ = true;
		}
		~tdisable_check_ip_lock()
		{
			VALIDATE(instance_.disable_check_ip_, null_str);
			instance_.disable_check_ip_ = false;
		}

	private:
		base_instance& instance_;
	};
	bool start_servers_internal(uint32_t ipaddr);
	void stop_servers_internal();

	// it will execute long block task
	virtual void app_will_longblock() {}

protected:
	rtc::SDLThread sdl_thread_;
	rtc::ThreadChecker thread_checker_;
	surface icon_;
	base::test::TaskEnvironment chromium_env_;
	// base::rose::ScopedTaskEnvironment chromium_env_;
	CVideo video_;
	hero_map heros_;
	rose_lua_kernel* lua_;
	std::map<aplt::taplt_key, aplt::tapplet> applets_;
	const aplt::tapplet* fg_aplt_;
	aplt::tbg_task bg_task_;

	const font::manager font_manager_;
	const preferences::base_manager prefs_manager_;
	const image::manager image_manager_;
	sound::music_thinker music_thinker_;
	binary_paths_manager paths_manager_;
	anim2::manager anim2_manager_;
	const cursor::manager* cursor_manager_; // it should create after video-subsystem.
	loadscreen::global_loadscreen_manager* loadscreen_manager_; // it should create after video-subsystem.
	const gui2::event::tmanager* gui2_event_manager_; // it should create after gui2-subsystem.

	std::string tokens_new_msgstr_;

	config app_cfg_;
	config core_cfg_; // remove [terrain_type] children's data.bin.

	preproc_map old_defines_map_;
	config_cache& cache_;

	SDL_AppHandlers sdl_apphandlers_;
	bool foreground_;
	bool terminating_;
	bool minimized_;
	bool silent_background_;
	std::map<int, animation*> anims_;
	std::multimap<std::string, const config> utype_anim_tpls_;

	twebrtc_send_helper send_helper_;

	// boost::scoped_ptr<sound::tpersist_xmit_audio_lock> background_persist_xmit_audio_;
	int background_callback_id_;
	std::map<uint32_t, std::function<bool (uint32_t ticks, bool screen_on)> > background_callbacks_;

	trtc_client* rtc_client_;
	std::map<int, tserver_*> servers_;
	live555d::tmanager live555d_mgr_;
	net::thttpd_manager httpd_mgr_;
	const int check_ip_threshold_;
	uint32_t next_check_ip_ticks_;
	bool disable_check_ip_;
	bool ip_require_invalid_;
	uint16_t server_check_port_;

	std::set<tbase_msg_subscriber*> base_msg_subscribers_;

	bool invalidate_layout_;
	std::map<std::string, uint32_t> backup_files_;

	// (key)textdomain --> use_count
	std::map<std::string, int> aplt_textdomain_usage_;

	tmessage_handler msg_handler_;
	tflite::tscript def_script_;
	tflite::ttflite def_tflite_;
	tcamera camera_;
	uint32_t last_report_camera_nosignal_ticks_;

	tflite::tslot_impl tflite_slot_;

	// logs.pb
	int logs_pb_type_;
	const int logs_pb_backup_type_;
	int logs_pb_max_days_;
	int logs_pb_max_logs_;
	pb2::tlogs logs_pb_;
	threading::mutex undelivered_logs_mutex_;
	std::vector<pb2::tlog> undelivered_logs_;

	aplt::trpy_sensor rpy_sensor_;
	// tfarthest_filter pitch_filter_;

private:
	tble* current_ble_;
	tble* app_ble_;
};

extern base_instance* instance;


// C++ Don't call virtual function during class construct function.
template<class T>
class instance_manager
{
public:
	// @default_size: if nposm, decided by rose.
	instance_manager(rtc::PhysicalSocketServer& ss, int argc, char** argv, const std::string& app, const std::string& channel)
	{
		// if exception generated when construct, system don't call destructor.
		try {
			base_instance::prefix_create(app, channel);
			instance = new T(ss, argc, argv);
			instance->initialize();

		} catch(...) {
			if (instance) {
				instance = nullptr;
				delete instance;
			}
			throw;
		}
	}
	
	~instance_manager()
	{
		if (instance) {
			// some moudle require instance is not-nullptr, instance->uninitialize() terminate them.
			// during instance->uninitialize(), instance is not-nullptr
			instance->uninitialize();
			::backup_did_file_write = backup_did_file_write_simple;
			base_instance* instance2 = instance;
			instance = nullptr; // when delete instance, some function's operator is depend on instance.
			delete instance2;
		}
	}
	T& get() { return *(dynamic_cast<T*>(instance)); }
};

#endif
