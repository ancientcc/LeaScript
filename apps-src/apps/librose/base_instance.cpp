/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
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
#define GETTEXT_DOMAIN "rose-lib"

#include "base_instance.hpp"
#include "gettext.hpp"
#include "builder.hpp"
#include "language.hpp"
#include "preferences_display.hpp"
#include "xwml.hpp"
#include "cursor.hpp"
#include "map.hpp"
#include "rose_version.hpp"
#include "formula_string_utils.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/combo_box2.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/label.hpp"
#include "ble.hpp"
#include "theme.hpp"
#include "chinese.hpp"
#include "protobuf.hpp"

#ifdef WEBRTC_ANDROID
#include "modules/utility/include/jvm_android.h"
#include <base/android/jni_android.h>
#include <base/android/base_jni_onload.h>
#endif

// chromium
#include <base/command_line.h>
#include <base/logging.h>

// live555
#include <groupsock/include/GroupsockHelper.hh>

#include <iostream>
#include <clocale>
using namespace std::placeholders;

#include <opencv2/core.hpp>
#include "qr_code.hpp"

#include "aplt_common.hpp"
#include "serialization/parser.hpp"

double remove_two_farthest_iterative(const double* samples, int max_samples, double* temp)
{   
	VALIDATE(max_samples > 2, null_str);

    memcpy(temp, samples, sizeof(double) * max_samples);
    int size = max_samples;
    
	double sum = 0.0f;
    // Remove the two farthest points
    for (int remove = 0; remove < 2; remove ++) {
        // Calculate the average
        sum = 0.0f;
        for (int i = 0; i < size; i++) {
			sum += temp[i];
		}
        double mean = sum / size;
        
        // Find the farthest
        int farthest_idx = 0;
        double max_dist = fabs(temp[0] - mean);
        for (int i = 1; i < size; i++) {
            double dist = fabs(temp[i] - mean);
            if (dist > max_dist) {
                max_dist = dist;
                farthest_idx = i;
            }
        }
        
        // Delete this element
        for (int i = farthest_idx; i < size - 1; i++) {
            temp[i] = temp[i + 1];
        }
        size --;
    }
    
    // copy result
	sum = 0.0;
	for (int i = 0; i < size; i++) {
		sum += temp[i];
	}

    double mean = sum / size;
    return mean;
}

void tfarthest_filter::reset_anti_shike_samples()
{
	// SDL_Log("%u ---reset_anti_shike_samples---", SDL_GetTicks());

	next_sample_index_ = 0;
	memset(anti_shake_samples_, 0, sizeof(anti_shake_samples_));
	memset(temp_4_average_, 0, sizeof(temp_4_average_));

	last_recv_sample_ticks_ = 0;
	recv_samples_ = 0;

	sync_val_.store(float_nposm);
}

void tfarthest_filter::nonmain_did_received(double newly)
{
	// VALIDATE_NOT_MAIN_THREAD();

	// Ensure that only this one thread is writing.
	double filtered = average_sample(newly);
	sync_val_.store(filtered);
}

double tfarthest_filter::average_sample(double newly)
{
	//
	// step1: add newly_sample to samples.
	//
	if (last_recv_sample_ticks_ != 0 && (int)(SDL_GetTicks() - last_recv_sample_ticks_) > reset_samples_threshold_ms_) {
		reset_anti_shike_samples();
	}

	const bool verbose = false;

	double* samples = anti_shake_samples_;
	const int max_samples = max_samples_;
	int& next_sample_index = next_sample_index_;

	samples[next_sample_index_] = newly;

	if (verbose) {
		SDL_Log("-----{%s} next_sample_index: %i---", 
			"vel", next_sample_index);
		for (int at = 0; at < max_samples; at ++) {
			const double& line = samples[at];
			// SDL_Log("[%i/%i]nose_x: %.3f, shoulder_mid: %.3f, shoulder_width: %.3f, ear_diff: %.3f, head_forward: %.3f", at, max_line_samples, 
			//	line.d[fidx_nose_x], line.d[fidx_shoulder_mid_x], line.d[fidx_shoulder_width], line.d[fidx_ear_diff_y], line.d[fidx_head_forward]);
		}
		SDL_Log("--------");
	}

	next_sample_index ++;
	next_sample_index %= max_samples;

	last_recv_sample_ticks_ = SDL_GetTicks();
	recv_samples_ ++;

	//
	// step2: calculate average sample.
	//
	if (recv_samples_ < max_samples) {
		if (recv_samples_ < MIN_SAMPLES_FOR_FILTER) {
			// With only 1-2 samples, filtering is not possible; return the average.
			double sum = 0.0;
			for (int i = 0; i < recv_samples_; i++) {
				sum += anti_shake_samples_[i];
			}
			return sum / recv_samples_;
		} else {
			// With more than 3 samples, one farthest point can be removed.
			return remove_two_farthest_iterative(anti_shake_samples_, recv_samples_, temp_4_average_);
		}
	}

	return remove_two_farthest_iterative(samples, max_samples, temp_4_average_);
}
/*
trpy_sensor::trpy_sensor()
    : accel_sensor_(nullptr)
    , pitch_(0.0f)
    , roll_(0.0f)
    , has_data_(false)
    , initialized_(false)
{
}

trpy_sensor::~trpy_sensor()
{
    quit();
}

bool trpy_sensor::init()
{
    // If already initialized, clean up first.
    if (initialized_) {
        quit();
    }
    
    // Find and open the accelerometer.
    int sensorCount = SDL_NumSensors();
    if (sensorCount <= 0) {
        SDL_Log("No sensors found.");
        return false;
    }
    
    for (int i = 0; i < sensorCount; i++) {
        SDL_SensorType type = SDL_SensorGetDeviceType(i);
        if (type == SDL_SENSOR_ACCEL) {
            accel_sensor_ = SDL_SensorOpen(i);
            if (accel_sensor_) {
                SDL_Log("Accelerometer opened: %s", SDL_SensorGetDeviceName(i));
                initialized_ = true;
                has_data_ = false;
                pitch_ = 0.0f;
                roll_ = 0.0f;
                return true;
            }
        }
    }
    
    SDL_Log("Accelerometer not found.");
    return false;
}

void trpy_sensor::update()
{
    if (!initialized_ || accel_sensor_ == nullptr) {
        return;
    }
    
    float data[3];
    int result = SDL_SensorGetData(accel_sensor_, data, 3);
    if (result == 0) {
        float ax = data[0];
        float ay = data[1];
        float az = data[2];
        
        // Calculate Pitch (tilt angle): rotation around the X-axis.
        // pitch_ = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
		float pitch = atan2f(-ax, sqrtf(ay * ay + az * az));
		pitch_ = pitch_filter_.update(pitch);
        
        // Calculate Roll (bank angle): rotation around the Y-axis.
        // roll_ = atan2f(ay, az) * 180.0f / M_PI;
		roll_ = atan2f(ay, az);

		if (game_config::os == os_ios) {
			pitch_ = -pitch_;
		}
        
		// SDL_Log("trpy_sensor(num == 3)roll: %.3f, pitch: %.3f", RAD2DEG(roll_), RAD2DEG(pitch_));

        has_data_ = true;

    } else {
		// SDL_Log("trpy_sensor(num(%i) != 3)", num);
	}
}

bool trpy_sensor::is_vertical(float threshold) const
{
    if (!has_data_) return false;
    
    float abs_pitch = fabsf(pitch_);
    return (abs_pitch > threshold && abs_pitch < (180.0f - threshold));
}

void trpy_sensor::quit()
{
    if (accel_sensor_ != nullptr) {
        SDL_SensorClose(accel_sensor_);
        accel_sensor_ = nullptr;
    }
    
    initialized_ = false;
    has_data_ = false;
    pitch_ = 0.0f;
    roll_ = 0.0f;
}
*/
base_instance* instance = nullptr;
/*
void VALIDATE_IN_MAIN_THREAD()
{
	SDL_Log("{VALIDATE_IN_MAIN_THREAD}main_tid: %llu, tid: %llu", (uint64_t)main_tid, (uint64_t)SDL_ThreadID());
	VALIDATE(instance == nullptr || instance->thread_checker_.CalledOnValidThread(), null_str);
}

void VALIDATE_NOT_MAIN_THREAD()
{
	DCHECK(!instance->thread_checker_.CalledOnValidThread());
}

bool base_instance::in_main_thread() const
{
	SDL_Log("{in_main_thread}main_tid: %llu, tid: %llu", (uint64_t)main_tid, (uint64_t)SDL_ThreadID());
	return thread_checker_.CalledOnValidThread();
}

bool base_instance::called_on_this_thread(unsigned long tid) const
{
	return tid == SDL_ThreadID();
}
*/

void base_instance::regenerate_heros(hero_map& heros, bool allow_empty)
{
	const std::string hero_data_path = game_config::path + "/xwml/" + "hero.dat";
	heros.map_from_file(hero_data_path);
	if (!heros.map_from_file(hero_data_path)) {
		if (allow_empty) {
			// allow no hero.dat
			heros.realloc_hero_map(HEROS_MAX_HEROS);
		} else {
			std::stringstream err;
			err << _("Can not find valid hero.dat in <data>/xwml");
			throw game::error(err.str());
		}
	}
	hero_map::map_size_from_dat = heros.size();
	hero player_hero((uint16_t)hero_map::map_size_from_dat);
	if (!preferences::get_hero(player_hero, heros.size())) {
		// write [hero] to preferences
		preferences::set_hero(heros, player_hero);
	}

	group.reset();
	heros.add(player_hero);
	hero& leader = heros[(uint16_t)(heros.size() - 1)];
	group.set_leader(leader);
	leader.set_uid(preferences::uid());
	group.set_noble(preferences::noble());
	group.set_coin(preferences::coin());
	group.set_score(preferences::score());
	group.interior_from_str(preferences::interior());
	group.signin_from_str(preferences::signin());
	group.reload_heros_from_string(heros, preferences::member(), preferences::exile());
	group.associate_from_str(preferences::associate());
	// group.set_layout(preferences::layout()); ?????
	group.set_map(preferences::map());

	group.set_city(heros[hero::number_local_player_city]);
	group.city().set_name(preferences::city());

	other_group.clear();
}

extern void preprocess_res_explorer();
static void handle_app_event(Uint32 type, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_app_event(type);
}

static void handle_window_event(SDL_Window* window, Uint32 type, void* param)
{
	if (SDL_GetWindowID(window) == tray::window_id) {
		return;
	}
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_window_event(window, type);
}

static void handle_background(SDL_bool screen_on, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_background(screen_on? true: false);
}

static void handle_destroy_texture(const void* _tex, void* param)
{
	const SDL_Texture* tex = static_cast<const SDL_Texture*>(_tex);
	if (game_config::os == os_windows) {
#ifdef ENABLE_ROSE_DBG_TEXTURE
		rose_dbg_texture.did_DestroyTexture(tex);
#endif
	}
}

static void handle_tray_event(Uint32 type, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_tray_event(type);
}
/*
static void handle_rpy_received(double roll, double pitch, double yaw, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_rpy_received(roll, pitch, yaw);
}
*/
bool is_all_ascii(const std::string& str)
{
	// str maybe ansi, unicode, utf-8.
	const char* c_str = str.c_str();
	int size = str.size();
	VALIDATE(size, null_str);

	for (int at = 0; at < size; at ++) {
		if (c_str[at] & 0x80) {
			return false;
		}
	}
	return true;
}

void path_must_all_ascii(const std::string& path)
{
	// path is utf-8 format.
	VALIDATE(!path.empty(), null_str);

	bool ret = is_all_ascii(path);
	if (!ret) {
		std::string path2 = utils::normalize_path(path);
		utils::string_map symbols;
		symbols["path"] = path2;
		const std::string err = vgettext2("$path contains illegal characters. Please use a pure english path.", symbols);
		VALIDATE(false, err); 
	}
}

void imwrite_mat(const cv::Mat& src, const std::string& path)
{
	VALIDATE(src.rows > 0 && src.cols > 0, null_str);
	if (src.channels() == 1) {
		imwrite_gray(src, path);
	} else {
		imwrite(src, path);
	}
}

static void opencv_verbose()
{
	bool support_avx2 = cv::checkHardwareSupport(CV_CPU_AVX2);
    bool support_neon = cv::checkHardwareSupport(CV_CPU_NEON);
	SDL_Log("opencv, avx2: %s, neon: %s", support_avx2? "true": "false", support_neon? "true": "false");
    cv::Mat mat1(cv::Size(4, 1), CV_32FC1), mat2(cv::Size(4, 1), CV_32FC1), mat3;
    mat1.at<float>(0, 0) = 1;  mat2.at<float>(0, 0) = 1;
    mat1.at<float>(0, 1) = 1;  mat2.at<float>(0, 1) = -1;
    mat1.at<float>(0, 2) = -1; mat2.at<float>(0, 2) = 1;
    mat1.at<float>(0, 3) = -1; mat2.at<float>(0, 3) = -1;
    cv::phase(mat1, mat2, mat3, true);

	const bool extra = false;
	if (extra) {
/*
		std::vector<std::string> images;
		images.push_back(game_config::path + "/app-kdesktop/images/qr-input.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/qr-input-1.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/qr-input-2.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/qr-input-2-small.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1495-1512x2016.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1495.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1496-1512x2016.jpg");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1496.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1497.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1498.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1499.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1500.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1517.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/IMG_1518.JPG");
		images.push_back(game_config::path + "/app-kdesktop/images/1.png");
		images.push_back(game_config::path + "/app-kdesktop/images/2.png");

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
		surface surf = image::get_image("misc/save.png");
		VALIDATE(surf.get() != nullptr, null_str);
		SDL_Point src_size{3024, 4032};
		surf = scale_surface(surf, src_size.x, src_size.y);
		VALIDATE(surf->w == src_size.x && surf->h == src_size.y, null_str);

		std::vector<SDL_Point> scales;
		scales.push_back(SDL_Point{surf->w / 2, surf->h / 2});
		scales.push_back(SDL_Point{surf->w / 3, surf->h / 3});
		scales.push_back(SDL_Point{surf->w / 4, surf->h / 4});
		scales.push_back(SDL_Point{1511, 1311});
		scales.push_back(SDL_Point{2345, 1789});
		scales.push_back(SDL_Point{4123, 3123});
		scales.push_back(SDL_Point{5123, 4123});
		for (std::vector<SDL_Point>::const_iterator it = scales.begin(); it != scales.end(); ++ it) {
			const SDL_Point& size = *it;
			uint32_t start_ticks = SDL_GetTicks();
			surface scaled = scale_surface(surf, size.x, size.y);
			uint32_t stop_ticks = SDL_GetTicks();
			VALIDATE(scaled->w == size.x && scaled->h == size.y, null_str);
			SDL_Log("opencv_verbose, scale_surface from (%ix%i) == > (%ix%i), used: %u ms",
				surf->w, surf->h, scaled->w, scaled->h, stop_ticks - start_ticks);
		}
	}
}

static trose_thread* this_create_thread(const std::function<void (bool& exit)>& DoWork, 
		const std::function<void ()>& OnWorkStart, const std::function<void ()>& DoWorkDone, 
		const std::function<void ()>& OnTriggerExit, const std::string& name)
{
	return new net::tworker(DoWork, OnWorkStart, DoWorkDone, OnTriggerExit, name);
}

static trose_event* this_create_event(bool manual_reset, bool initially_signaled)
{
	return new rtc::Event(manual_reset, initially_signaled);
}

static void backup_did_file_write_instance(const std::string& file, int backup_type)
{
	instance->backup_did_file_write(file, backup_type);
}

static void this_get_config(const std::string& path, config& cfg, bool no_macro)
{
	VALIDATE(!path.empty(), null_str);

	if (no_macro) {
		tfile file(path, GENERIC_READ, OPEN_EXISTING);
		int32_t fsize = file.read_2_data();
		if (fsize > 0) {
			std::string stream(file.data, fsize);
			try {
				read(cfg, stream);
			} catch (twml_exception& ) {
				cfg.clear();
			}
		}

	} else {
		config_cache_transaction transaction;
		config_cache& cache = config_cache::instance();
		cache.clear_defines();

		cache.get_config(path, cfg);
	}
}

static void this_write_config(std::ostream& out, const config& cfg)
{
	write(out, cfg);
}

static void this_read_config(const std::string& in, config& cfg)
{
	read(cfg, in);
}

tbase_msg_subscriber::tbase_msg_subscriber()
{
	VALIDATE(instance != nullptr, null_str);
	instance->register_base_msg_subscriber(this);
}

tbase_msg_subscriber::~tbase_msg_subscriber()
{
	VALIDATE(instance != nullptr, null_str);

	instance->unregister_base_msg_subscriber(this);
}

base_instance::base_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv, int sample_rate, size_t sound_buffer_size)
	: sdl_thread_(&ss)
	, icon_()
	, chromium_env_(base::test::TaskEnvironment::TimeSource::SYSTEM_TIME, base::test::TaskEnvironment::MainThreadType::IO)
	, video_()
	, heros_(game_config::path)
	, lua_(nullptr)
	, fg_aplt_(nullptr)
	, bg_task_(*this, applets_)
	, font_manager_() 
	, prefs_manager_()
	, image_manager_()
	, music_thinker_()
	, paths_manager_()
	, anim2_manager_(video_)
	, cursor_manager_(NULL)
	, loadscreen_manager_(NULL)
	, gui2_event_manager_(NULL)
	, app_cfg_()
	, old_defines_map_()
	, cache_(config_cache::instance())
	, foreground_(true) // normally app cannnot receive first DIDFOREGROUND.
	, terminating_(false)
	, minimized_(false)
	, silent_background_(true)
	, send_helper_(nullptr)
	// , background_persist_xmit_audio_(NULL)
	, background_callback_id_(INVALID_UINT32_ID)
	, rtc_client_(nullptr)
	, check_ip_threshold_(10000)
	, next_check_ip_ticks_(0)
	, disable_check_ip_(false)
	, ip_require_invalid_(false)
	, server_check_port_(8081)
	, invalidate_layout_(false)
	, msg_handler_(*this)
	, def_script_() // evaluate after get valid game_config::path
	, def_tflite_(null_str, false) // evaluate after get valid def_script_
	, camera_(tcamera::cameraid_base, msg_handler_, nposm) // nposm
	, last_report_camera_nosignal_ticks_(0)
	, logs_pb_type_(nposm)
	, logs_pb_backup_type_(backup_on_fixed_delay)
	, logs_pb_max_days_(2)
	, logs_pb_max_logs_(INT32_MAX)
	, current_ble_(nullptr)
	, app_ble_(nullptr)
{
	// VALIDATE_IN_THIS_THREAD require it.
	VALIDATE(sizeof(unsigned long) == sizeof(SDL_threadID), null_str);
	// rose_set_wml_exception(wml_exception);
	rose_set_create_thread(this_create_thread);
	rose_set_create_event(this_create_event);
	aplt::set_get_config(this_get_config);
	aplt::set_read_config(this_read_config);
	aplt::set_write_config(this_write_config);

	// base::CommandLine::InitUsingArgvForTesting(argc, argv);
	base::CommandLine::Init(argc, argv);

	void* v1 = NULL;
	void* v2 = NULL;
	SDL_GetPlatformVariable(&v1, &v2, NULL);
#ifdef WEBRTC_ANDROID
#ifndef _KOS
	webrtc::JVM::Initialize(reinterpret_cast<JavaVM*>(v1), reinterpret_cast<jobject>(v2));
	base::android::InitVM(reinterpret_cast<JavaVM*>(v1));
	base::android::OnJNIOnLoadInit();
#endif
#endif
	logging::LoggingSettings settings;
	settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG; // [chromium]default to file on windows.
	logging::InitLogging(settings);

	// rtc::ThreadManager::Instance()->SetCurrentThread(&sdl_thread_);

	VALIDATE(game_config::path.empty(), null_str);
#ifdef _WIN32
	std::string exe_dir = get_exe_dir();
	if (!exe_dir.empty() && file_exists(exe_dir + "/data/_main.cfg")) {
		game_config::path = exe_dir;
		path_must_all_ascii(game_config::path);
	}

	// windows is debug os. allow pass command line.
	for (int arg_ = 1; arg_ != argc; ++ arg_) {
		const std::string option(argv[arg_]);
		if (option.empty()) {
			continue;
		}

		if (option == "--res-dir") {
			const std::string val = argv[++ arg_];

			// Overriding data directory
			if (val.c_str()[1] == ':') {
				game_config::path = val;
			} else {
				game_config::path = get_cwd() + '/' + val;
			}
			path_must_all_ascii(game_config::path);

			if (!is_directory(game_config::path)) {
				std::stringstream err;
				err << "Use --res-dir set " << utils::normalize_path(game_config::path) << " to game_config::path, but it isn't valid directory.";
				VALIDATE(false, err.str());
			}
		}
	}

#elif defined(__APPLE__)
	// ios, macosx
	game_config::path = get_cwd();

#elif defined(ANDROID)
#ifndef _KOS	
	// on android, use asset.
	const std::string internal_storage_path = SDL_AndroidGetInternalStoragePath();
	SDL_Log("internal_storage_path: %s", internal_storage_path.c_str());
	game_config::path = "res";
#else
	game_config::path = SDL_AndroidGetInternalStoragePath();
#endif
#else
	// linux, etc
	game_config::path = get_cwd();
#endif

	game_config::path = utils::normalize_path(game_config::path);
/*
	std::string path1 = utils::normalize_path(game_config::path + "//.");
	std::string path2 = utils::normalize_path(game_config::path + "//.g");
	std::string path3 = utils::normalize_path(game_config::path + "//.g/h./..");
	std::string path4 = utils::normalize_path(game_config::path + "//.g/.");
	std::string path5 = utils::normalize_path(game_config::path + "//.g/./");
	std::string path6 = utils::normalize_path(game_config::path + "//.g/.h");
*/
	VALIDATE(game_config::path.find('\\') == std::string::npos, null_str);
	VALIDATE(file_exists(game_config::path + "/data/_main.cfg"), "game_config::path(" + game_config::path + ") must be valid!");

	SDL_Log("Data directory: %s, sizeof(long): %i", game_config::path.c_str(), (int)sizeof(long));
	game_config::app_dir_root = game_config::path + "/" + game_config::app_dir;

	if (game_config::app == "launcher" || game_config::app == "kdesktop") {
		def_script_ = tflite::tscript(game_config::app_dir_root + "/tflites/classifier_camera.cfg", true, nullptr);
		VALIDATE(def_script_.valid(), null_str);
		def_tflite_ = tflite::ttflite(def_script_.tflite, def_script_.res);
	}

	preprocess_res_explorer();

	sound::set_play_params(sample_rate, sound_buffer_size);

	bool no_music = false;
	bool no_sound = false;

	// if allocate static, iOS may be not align 4! 
	// it necessary to ensure align great equal than 4.
	game_config::savegame_cache = (unsigned char*)malloc(game_config::savegame_cache_size);

	font_manager_.update_font_path();
	heros_.set_path(game_config::path);

#if defined(_WIN32) && defined(_DEBUG)
	// sound::init_sound make no memory leak output.
	// By this time, I doesn't find what result it, to easy, don't call sound::init_sound. 
	no_sound = true;
#endif

	// disable sound in nosound mode, or when sound engine failed to initialize
	if (no_sound || ((preferences::sound_on() || preferences::music_on() ||
	                  preferences::turn_bell() || preferences::UI_sound_on()) &&
	                 !sound::init_sound())) {

		preferences::set_sound(false);
		preferences::set_music(false);
		preferences::set_turn_bell(false);
		preferences::set_UI_sound(false);
	} else if (no_music) { // else disable the music in nomusic mode
		preferences::set_music(false);
	}

	//
	// initialize player group
	//
	upgrade::fill_require();
	regenerate_heros(heros_, true);

	memset(&sdl_apphandlers_, 0, sizeof(sdl_apphandlers_));
	sdl_apphandlers_.app_event_handler = ::handle_app_event;
	sdl_apphandlers_.app_event_param = this;
	sdl_apphandlers_.window_event_handler = ::handle_window_event;
	sdl_apphandlers_.window_event_param = this;
	sdl_apphandlers_.background_handler = ::handle_background;
	sdl_apphandlers_.background_param = this;
	sdl_apphandlers_.destroy_texture_handler = ::handle_destroy_texture;
	sdl_apphandlers_.destroy_texture_param = this;
	sdl_apphandlers_.tray_event_handler = ::handle_tray_event;
	sdl_apphandlers_.tray_event_param = this;
	// sdl_apphandlers_.did_rpy_handler = ::handle_rpy_received;
	// sdl_apphandlers_.did_rpy_param = this;
	SDL_SetAppHandlers(&sdl_apphandlers_);

	// Followed operation maybe poupup messagebox, it require locale language message. so init internation first.
	init_locale();
	// in order to validate gettext, android requrie below.
	bool res = init_language();
	if (!res) {
		throw twml_exception("could not initialize the language");
	}

	tokens_new_msgstr_ = _("tokens^New");

	aplt::initial();

	opencv_verbose();
}

base_instance::~base_instance()
{
	SDL_Log("base_instance::~base_instance()---");
	// enter deconstruct, instance require nullptr.
	VALIDATE(instance == nullptr, null_str);
	VALIDATE(aplt_textdomain_usage_.empty(), null_str);

	if (icon_) {
		icon_.get()->refcount --;
	}
	if (game_config::savegame_cache) {
		free(game_config::savegame_cache);
		game_config::savegame_cache = NULL;
	}
	terrain_builder::release_heap();
	sound::close_sound();

	clear_anims();

	game_config::path.clear();

	base::CommandLine::Reset();
	SDL_Log("---base_instance::~base_instance() X");
}

/**
 * I would prefer to setup locale first so that early error
 * messages can get localized, but we need the game_controller
 * initialized to have get_intl_dir() to work.  Note: setlocale()
 * does not take GUI language setting into account.
 */
void base_instance::init_locale() 
{
#ifdef _WIN32
    std::setlocale(LC_ALL, "English");
#else
	std::setlocale(LC_ALL, "C");
	std::setlocale(LC_MESSAGES, "");
#endif
	const std::string& intl_dir = get_intl_dir();
	bindtextdomain("rose-lib", intl_dir.c_str());
	bind_textdomain_codeset ("rose-lib", "UTF-8");
	

	const std::string path = game_config::app_dir_root + "/translations"; 
	bindtextdomain(def_textdomain, path.c_str());
	bind_textdomain_codeset(def_textdomain, "UTF-8");
	textdomain(def_textdomain);
}

bool base_instance::init_language()
{
	if (!::load_language_list()) {
		return false;
	}

	if (!::set_language(get_locale())) {
		return false;
	}

	// hotkey::load_descriptions();

	return true;
}

uint32_t base_instance::get_callback_id() const
{
	uint32_t id = background_callback_id_ + 1;
	while (true) {
		if (id == INVALID_UINT32_ID) {
			// turn aournd.
			id ++;
			continue;
		}
		if (background_callbacks_.count(id)) {
			id ++;
			continue;
		}
		break;
	}
	return id;
}

uint32_t base_instance::background_connect(const std::function<bool (uint32_t ticks, bool screen_on)>& callback)
{
	SDL_Log("base_instance::background_connect------, callbacks_: %i", (int)background_callbacks_.size());
	if (!foreground_) {
		if (background_callbacks_.empty()) {
			// VALIDATE(!background_persist_xmit_audio_.get(), null_str);
			// background_persist_xmit_audio_.reset(new sound::tpersist_xmit_audio_lock);
		}
        // VALIDATE(background_persist_xmit_audio_.get(), null_str);
	}

	background_callback_id_ = get_callback_id();
	background_callbacks_.insert(std::make_pair(background_callback_id_, callback));

	SDL_Log("------base_instance::background_connect, callbacks_: %i", (int)background_callbacks_.size());
	return background_callback_id_;
}

void base_instance::background_disconnect_th(const uint32_t id)
{
	if (!foreground_) {
		// VALIDATE(background_persist_xmit_audio_.get(), null_str);
		if (background_callbacks_.size() == 1) {
			// background_persist_xmit_audio_.reset(NULL);
		}
	}
}

void base_instance::background_disconnect(const uint32_t id)
{
	SDL_Log("base_instance::background_disconnect------, callbacks_: %i", (int)background_callbacks_.size());

	background_disconnect_th(id);

	VALIDATE(background_callbacks_.count(id), null_str);
	std::map<uint32_t, std::function<bool (uint32_t, bool)> >::iterator it = background_callbacks_.find(id);
	background_callbacks_.erase(it);

	SDL_Log("------base_instance::background_disconnect, callbacks_: %i", (int)background_callbacks_.size());
}

void base_instance::handle_background(bool screen_on)
{
	uint32_t ticks = SDL_GetTicks();
	for (std::map<uint32_t, std::function<bool (uint32_t ticks, bool screen_on)> >::iterator it = background_callbacks_.begin(); it != background_callbacks_.end(); ) {
		// must not app call background_disconnect! once enable, this for will result confuse!
		bool erase = it->second(ticks, screen_on);
		if (erase) {
			background_disconnect_th(it->first);
			background_callbacks_.erase(it ++);
		} else {
			++ it;
		}
	}
}
/*
void base_instance::handle_rpy_received(double roll, double pitch, double yaw)
{
	// SDL_Log("roll: %.3f, pitch: %.3f", RAD2DEG(roll), RAD2DEG(pitch));
	if (!is_float_nposm(pitch)) {
		pitch_filter_.nonmain_did_received(pitch);
	}
}
*/
void base_instance::set_current_ble(tble* ble)
{
	if (ble != nullptr) {
		if (ble->is_app()) {
			// create by app. for example: kdesktop
			VALIDATE(current_ble_ == nullptr, null_str);

		} else {
			// create by applet
			if (current_ble_ != nullptr) {
				VALIDATE(current_ble_->is_app(), null_str);
				app_ble_ = current_ble_;
			}
		}
		current_ble_ = ble;

	} else {
		current_ble_ = app_ble_;
		app_ble_ = nullptr;
	}
}

void base_instance::did_enter_background()
{
	app_didenterbackground();

	foreground_ = false;
		
	// app require insert callback in app_didenterbackground.
	// VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
	if (!background_callbacks_.empty()) {
		// background_persist_xmit_audio_.reset(new sound::tpersist_xmit_audio_lock);
	}
	if (current_ble_ != nullptr) {
		current_ble_->handle_app_event(true);
	}
}

void base_instance::did_enter_foreground()
{
	// force clear all backgroud callback. app requrie clear callback_id in app_didenterforeground.
	if (!background_callbacks_.empty()) {
		// VALIDATE(background_persist_xmit_audio_.get() != NULL, null_str);
		// background_persist_xmit_audio_.reset(NULL);
		background_callbacks_.clear();
	} else {
		// VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
	}
	app_didenterforeground();
	
	foreground_ = true;
	invalidate_layout_ = true;
	if (current_ble_ != nullptr) {
		current_ble_->handle_app_event(false);
	}
}

void base_instance::handle_app_event(Uint32 type)
{
	// Notice! it isn't in main thread, except SDL_APP_LOWMEMORY.
	if (type == SDL_APP_TERMINATING || type == SDL_QUIT) {
		SDL_Log("handle_app_event, %s", type == SDL_APP_TERMINATING? "SDL_APP_TERMINATING": "SDL_QUIT");
		if (!terminating_) {
			SDL_Log("handle_app_event, handle terminating task");
			SDL_EnableStatusBar(SDL_TRUE);
			app_terminating();

			terminating_ = true;

			if (game_config::os == os_ios) {
				// now iOS is default!
				// if call CVideo::quit when SDL_APP_WILLENTERBACKGROUND, it will success. Of couse, no help.
				throw CVideo::quit();
			}
		}

	} else if (type == SDL_APP_WILLENTERBACKGROUND) {
		SDL_Log("handle_app_event, SDL_APP_WILLENTERBACKGROUND");
		app_willenterbackground();
		// FIX SDL BUG! normally DIDENTERBACKGROUND should be called after WILLENTERBACKGROUND.
		// but on iOS, because SDL event queue, SDL-DIDENTERBACKGROUND is called, but app-DIDENTERBACKGROUND not!
		// app-DIDENTERBACKGROUND is call when WILLENTERFOREGROUND.

	} else if (type == SDL_APP_DIDENTERBACKGROUND) {
		SDL_Log("handle_app_event, SDL_APP_DIDENTERBACKGROUND");
		did_enter_background();
/*
		app_didenterbackground();

		foreground_ = false;
		
		// app require insert callback in app_didenterbackground.
		// VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
		if (!background_callbacks_.empty()) {
			// background_persist_xmit_audio_.reset(new sound::tpersist_xmit_audio_lock);
		}
		if (current_ble_) {
			current_ble_->handle_app_event(true);
		}
*/
	} else if (type == SDL_APP_WILLENTERFOREGROUND) {
		SDL_Log("handle_app_event, SDL_APP_WILLENTERFOREGROUND");
		app_willenterforeground();

	} else if (type == SDL_APP_DIDENTERFOREGROUND) {
		SDL_Log("handle_app_event, SDL_APP_DIDENTERFOREGROUND");
		did_enter_foreground();
/*
		// force clear all backgroud callback. app requrie clear callback_id in app_didenterforeground.
		if (!background_callbacks_.empty()) {
			// VALIDATE(background_persist_xmit_audio_.get() != NULL, null_str);
            // background_persist_xmit_audio_.reset(NULL);
			background_callbacks_.clear();
		} else {
			// VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
		}
		app_didenterforeground();
	
		foreground_ = true;
		invalidate_layout_ = true;
		if (current_ble_ != nullptr) {
			current_ble_->handle_app_event(false);
		}
*/

	} else if (type == SDL_APP_LOWMEMORY) {
		SDL_Log("handle_app_event, SDL_APP_LOWMEMORY");
		app_lowmemory();

	} else if (type == SDL_CLIPBOARDUPDATE) {
		SDL_Log("handle_app_event, SDL_CLIPBOARDUPDATE");
		std::string val;
		if (SDL_HasClipboardText()) {
			char* text = SDL_GetClipboardText();
			val = text;
			SDL_free(text);
		}
		// if "copy" non-text content, it will enter it also.
		// If it's empty, it means copying non-text content, like files.
		instance->app_handle_clipboard_paste(val);
	}
	video_.set_force_render_present();
}

#include <SDL_syswm.h>
void force_window_on_top_native(SDL_Window* window)
{
#ifdef _WIN32
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;
        // Force topmost using Windows API
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
#endif
}

void base_instance::handle_window_event(SDL_Window* window, Uint32 type)
{
	if (get_sdl_window() != nullptr && window != get_sdl_window()) {
		int ii = 0;
	}
	if (type == SDL_WINDOWEVENT_MINIMIZED) {
		minimized_ = true;
		SDL_Log("handle_window_event, MINIMIZED");
		if (game_config::os == os_windows) {
			// foreground_ = false;
			did_enter_background();
		}
		if (tray::window->valid()) {
			force_window_on_top_native(tray::window->window);

			// First try SDL_RaiseWindow; if it fails, then use force_window_on_top_native.
			// SDL_RaiseWindow(tray::window->window);
		}

	} else if (type == SDL_WINDOWEVENT_MAXIMIZED || type == SDL_WINDOWEVENT_RESTORED) {
		minimized_ = false;
		SDL_Log("handle_window_event, %s", type == SDL_WINDOWEVENT_MAXIMIZED? "MAXIMIZED": "RESTORED");
		if (tray::window->valid()) {
			// 1/2)Move the tray::window->window to the topmost position.
			force_window_on_top_native(tray::window->window);
		}
		if (game_config::os == os_windows) {
			// 2/2)because '1/2)force_window_on_top_native', input focuse is tray-window,
			//     now raise sdl_window as input focus.
			SDL_RaiseWindow2();
			preferences::_set_maximized(type == SDL_WINDOWEVENT_MAXIMIZED);
			if (type == SDL_WINDOWEVENT_RESTORED) {
				// invalidate_layout_ = true;
				// foreground_ = true;
				did_enter_foreground();
			}
		}

	}

	video_.set_force_render_present();
}

void base_instance::handle_resize_screen(const int sdl_width, const int sdl_height)
{
	// CVideo::setMode will result handle_resize_screen. 
	// in that case, (width,height) equals to (gui2::settings::screen_width, gui2::settings::screen_width).
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(game_config::os == os_windows, null_str);

	int actual_w = 0;
	int actual_h = 0;
	SDL_GetWindowSize(get_sdl_window(), &actual_w, & actual_h);
	if (sdl_width != actual_w || sdl_height != actual_h) {
		// SDL_WINDOWEVENT_SIZE_CHANGED maybe delay received,
		SDL_Log("base_instance::handle_resize_screen X, sdl size(%ix%i) doesn't equal to actual size(%ix%i), think delay result, do nothing", sdl_width, sdl_height, actual_w, actual_h);
		return;
	}

	int width = posix_align_floor(sdl_width, gui2::hdpi_scale_4_align_floor(gui2::twidget::hdpi_scale));
	int height = posix_align_floor(sdl_height, gui2::hdpi_scale_4_align_floor(gui2::twidget::hdpi_scale));
	const bool size_not_equal = width != sdl_width || height != sdl_height;

	tpoint min_size = gui2::twidget::orientation_swap_size(game_config::min_allowed_width(), game_config::min_allowed_height());
	bool enlarge_size = false;
	if (width < min_size.x) {
		enlarge_size = true;
		width = min_size.x;
	}
	if (height < min_size.y) {
		enlarge_size = true;
		height = min_size.y;
	}
	// if (enlarge_size || size_not_equal) {
	if (enlarge_size) {
		// if SDL_WINDOW_MAXIMIZED is set, SDL_SetWindowSize cannot change size.
		// if cannnot change size, will result handle_resize_screen dead loop. so don't consider size_not_equal(true).
		video_.sdl_set_window_size(width, height);

	}
	if (width == gui2::settings::screen_width && height == gui2::settings::screen_height) {
		if (enlarge_size) {
			// if enlarge, SDL_SetWindowSize will result "black edge" in enlarged area.
			// relayout window to erase "black edge".
			if (gui2::ttrack::global_did_drawing || events::progress_running) {
				gui2::invalidate_layout_all();
			} else {
				gui2::absolute_draw_all(true);
			}
		}
		return;
	}

	keyboard::set_visible(false);
	preferences::set_resolution(video_, width, height, true);

	gui2::settings::screen_width = width;
	gui2::settings::screen_height = height;

	std::vector<gui2::twindow*> windows = gui2::connectd_window();
	for (std::vector<gui2::twindow*>::const_reverse_iterator rit = windows.rbegin(); rit != windows.rend(); ++ rit) {
		gui2::twindow* window = *rit;
		window->dialog().resize_screen();
	}

	if (gui2::ttrack::global_did_drawing || events::progress_running) {
		gui2::invalidate_layout_all();

	} else {
		gui2::absolute_draw_all(true);
	}
}

surface icon2;
bool base_instance::init_video()
{
	{
		cache_.clear_defines();
		config cfg;
		cache_.get_config(game_config::path + "/data/settings.cfg", cfg);
		const config& rose_settings_cfg = cfg.child("settings");
		VALIDATE(!rose_settings_cfg.empty(), null_str);
		game_config::load_config(&rose_settings_cfg);

		// At least until now, the app and applet versions must be the same
		VALIDATE(game_config::rose_version == aplt::min_aplt_rose_ver, null_str);

		// require game_config::rose_version valid.
		aplt::load_applets_from_disk(applets_);

		cache_.clear_defines();
		cache_.get_config(game_config::app_dir_root + "/settings.cfg", cfg);
		const config& app_settings_cfg = cfg.child("settings");
		app_load_settings_config(app_settings_cfg? app_settings_cfg: config());
	}

	// when startup, rose must enable statusbar first. app require disable later by itself.
	SDL_EnableStatusBar(SDL_TRUE);

	uint32_t _startup_servers = 0;
	int pc_default_font_size = nposm;
	int mobile_default_font_size = nposm;
	VALIDATE(game_config::longpress_time == nposm, null_str);
	bool fullscreen = false;
	tpre_setmode_settings pre_settings(gui2::twidget::current_landscape, silent_background_, fullscreen, _startup_servers,
		pc_default_font_size, mobile_default_font_size, game_config::longpress_time, game_config::min_width, game_config::min_height, server_check_port_);
	app_pre_setmode(pre_settings);
	VALIDATE(game_config::longpress_time == nposm || game_config::longpress_time > 0, null_str);
	VALIDATE(game_config::min_width >= MIN_WIN_WIDTH || game_config::min_height >= MIN_WIN_HEIGHT, null_str);
	font::update_7size(game_config::mobile? mobile_default_font_size: pc_default_font_size);

	if (_startup_servers & server_httpd) {
		register_server(server_httpd, nullptr);
	}
	if (_startup_servers & server_rtspd) {
		register_server(server_rtspd, nullptr);
	}

	// So far not initialize font, don't call function related to the font. 
	// for example get_rendered_text/get_rendered_text_size.
	if (fullscreen) {
		set_fullscreen(fullscreen);
	}
	if (!set_orientation(nposm, true)) {
		return false;
	}
	
	{
		const std::string default_bg_image = "assets/LaunchImage-1242x2208.png";

		std::string bg_image = game_config::preferences_dir + "/images/misc/oem-logo.png";
		// render startup image. 
		// normally, app use this png only once, and it' size is large. to save memory, don't add to cache.
		surface surf = image::locator(bg_image).load_from_disk();
		if (!surf) {
			surf = image::locator(default_bg_image).load_from_disk();
			if (!surf) {
				return false;
			}
		}
		surf = makesure_neutral_surface(surf);
		const uint32_t fill_color = surf_calculate_most_color(surf);

		tpoint ratio_size = calculate_adaption_ratio_size(video_.getx(), video_.gety(), surf->w, surf->h);
		surface bg = create_neutral_surface(video_.getx(), video_.gety());
		if (ratio_size.x != video_.getx() || ratio_size.y != video_.gety()) {
			fill_surface(bg, fill_color);
		}
		surf = scale_surface(surf, ratio_size.x, ratio_size.y);
		SDL_Rect dstrect {(video_.getx() - ratio_size.x) / 2, (video_.gety() - ratio_size.y) / 2, ratio_size.x, ratio_size.y};
		sdl_blit(surf, nullptr, bg, &dstrect);
		render_surface(get_renderer(), bg, nullptr, nullptr);
		video_.flip();
	}

	std::string wm_title_string = game_config::get_app_msgstr(null_str) + " - " + game_config::version.str(true);
	SDL_SetWindowTitle(video_.getWindow(), wm_title_string.c_str());

#ifdef _WIN32
	icon2 = image::get_image("game_icon.png");
	if (icon2) {
		SDL_SetWindowIcon(video_.getWindow(), icon2);
	}
#endif

	return true;
}

// #define BASENAME_DATA		"data.bin"

bool base_instance::load_data_bin()
{
	VALIDATE(!game_config::app.empty(), _("Must set game_config::app"));
	VALIDATE(!game_config::app_channel.empty(), _("Must set game_config::app_channel"));
	VALIDATE(core_cfg_.empty(), null_str);

	cache_.clear_defines();

	loadscreen::global_loadscreen_manager loadscreen_manager(video_);
	cursor::setter cur(cursor::WAIT);

	try {		
		wml_config_from_file(game_config::path + "/xwml/" + BASENAME_DATA, app_cfg_);
	
		// [terrain_type]
		const config::const_child_itors& terrains = app_cfg_.child_range("terrain_type");
		BOOST_FOREACH (const config &t, terrains) {
			tmap::terrain_types.add_child("terrain_type", t);
		}
		app_cfg_.clear_children("terrain_type");

		// animation
		anim2::fill_anims(app_cfg_.child("units"));

		app_load_data_bin();

		// save this to core_cfg_
		core_cfg_ = app_cfg_;
		paths_manager_.set_paths(core_cfg_);

	} catch (game::error& e) {
		// ERR_CONFIG << "Error loading game configuration files";
		gui2::show_error_message(_("Error loading game configuration files: '") +
			e.message + _("' (The game will now exit)"));
		return false;
	}
	return true;
}

void base_instance::reload_data_bin(const config& data_cfg)
{
	// place execute to main thread.
	sdl_thread_.Invoke<void>(RTC_FROM_HERE, rtc::Bind(&base_instance::set_data_bin_cfg, this, data_cfg));
}

void base_instance::set_data_bin_cfg(const config& cfg)
{
	app_cfg_ = cfg;
	// [terrain_type]
	const config::const_child_itors& terrains = app_cfg_.child_range("terrain_type");
	BOOST_FOREACH (const config &t, terrains) {
		tmap::terrain_types.add_child("terrain_type", t);
	}
	app_cfg_.clear_children("terrain_type");

	// save this to core_cfg_
	core_cfg_ = app_cfg_;

	// conitnue to [rose_config].
	const config& core_cfg = instance->core_cfg();
	const config& game_cfg = core_cfg.child("rose_config");
	game_config::load_config(game_cfg? &game_cfg : NULL);
}

bool base_instance::change_language()
{
	std::vector<std::string> items;
	std::vector<tcode2> item2s;

	const std::vector<language_def>& languages = get_languages();
	const language_def& current_language = get_language();
	
	int initial_sel = 0;
	BOOST_FOREACH (const language_def& lang, languages) {
		items.push_back(lang.language);
		item2s.push_back(tcode2(item2s.size(), lang.language));
		if (lang == current_language) {
			initial_sel = items.size() - 1;
		}
	}

	int cursel = nposm;
	if (game_config::svga) {
		gui2::tcombo_box dlg(items, initial_sel, _("Language"), true);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK || !dlg.dirty()) {
			return false;
		}

		cursel = dlg.cursel();

	} else {
		gui2::tcombo_box2 dlg(_("Language"), null_str, item2s, initial_sel, true);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return false;
		}

		cursel = dlg.cursel();
	}

	::set_language(languages[cursel]);
	preferences::set_language(languages[cursel].localename);

	std::string wm_title_string = game_config::get_app_msgstr(null_str) + " - " + game_config::version.str(true);
	SDL_SetWindowTitle(video_.getWindow(), wm_title_string.c_str());
	return true;
}

int base_instance::show_preferences_dialog()
{
	if (game_config::mobile) {
		return gui2::twindow::OK;
	}

	std::vector<std::string> items;

	std::vector<int> values;
	int fullwindowed = preferences::fullscreen()? preferences::MAKE_WINDOWED: preferences::MAKE_FULLSCREEN;
	std::string str = preferences::fullscreen()? _("Exit fullscreen") : _("Enter fullscreen");
	items.push_back(str);
	values.push_back(fullwindowed);
	if (!preferences::fullscreen()) {
		items.push_back(_("Change Resolution"));
		values.push_back(preferences::CHANGE_RESOLUTION);
	}
	items.push_back(_("Close"));
	values.push_back(gui2::twindow::OK);

	gui2::tcombo_box dlg(items, nposm);
	dlg.show();
	if (dlg.cursel() == nposm) {
		return gui2::twindow::OK;
	}

	return values[dlg.cursel()];
}

void base_instance::pre_create_renderer()
{
	app_pre_create_renderer();
}

void base_instance::post_create_renderer()
{
	app_post_create_renderer();
}

void base_instance::fill_anim(int at, const std::string& id, bool area, bool tpl, const config& cfg)
{
	if (area) {
		anims_.insert(std::make_pair(at, new animation(cfg)));
	} else {
		if (tpl) {
			utype_anim_tpls_.insert(std::make_pair(id, cfg));
		} else {
			anims_.insert(std::make_pair(at, new animation(cfg)));
		}
	}
}

void base_instance::clear_anims()
{
	utype_anim_tpls_.clear();

	for (std::map<int, animation*>::const_iterator it = anims_.begin(); it != anims_.end(); ++ it) {
		animation* anim = it->second;
		delete anim;
	}
	anims_.clear();
}

const animation* base_instance::anim(int at) const
{
	std::map<int, animation*>::const_iterator i = anims_.find(at);
	if (i != anims_.end()) {
		return i->second;
	}
	return NULL;
}

void base_instance::prefix_create(const std::string& app, const std::string& channel)
{
	rose_set_wml_exception(wml_exception);
	// on android, main-thread is "SDLThread", not "SDLActivity", onCreate is runing on "SDLActivity".
	main_tid = SDL_ThreadID();

	utils::rose_interpolate_variables_into_string = utils::interpolate_variables_into_string;
	font::rose_get_rendered_text = font::get_rendered_text2;
	image::rose_get_image = image::get_image_4_rose_get_image;
	sound::rose_play_sound = sound::play_sound;
	rose_get_sdl_window = get_sdl_window;

	std::stringstream err;
	// if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0) {
	if (SDL_Init(SDL_INIT_TIMER) < 0) {
		err << "Couldn't initialize SDL: " << SDL_GetError();
		throw twml_exception(err.str());
	}

	srand((unsigned int)time(NULL));
	game_config::init(app, channel);
}

void base_instance::initialize()
{
	::backup_did_file_write = backup_did_file_write_instance;

	// why not call app_setup_user_data_dir from setup_user_data_dir?
	// --instance is null when setup_user_data_dir.
	app_setup_user_data_dir();

	// init_locale is called during base_instance's contructor,
	// must not call app_init_locale, so call here.
	app_init_locale(get_intl_dir());

	game_config::reserve_players.insert("");
	game_config::reserve_players.insert("kingdom");
	game_config::reserve_players.insert("Player");
	game_config::reserve_players.insert(_("Player"));
/*
	bool res = init_language();
	if (!res) {
		throw twml_exception("could not initialize the language");
	}
*/
	// In order to reduce 'black screen' time, so as soon as possible to call init_video.
	bool res = init_video();
	if (!res) {
		throw twml_exception("could not initialize display");
	}

	lobby = create_lobby();
	if (!lobby) {
		throw twml_exception("could not create lobby");
	}

	// do initialize fonts before reading the game config, to have game
	// config error messages displayed. fonts will be re-initialized later
	// when the language is read from the game config.
	res = font::load_font_config();
	if (!res) {
		throw twml_exception("could not initialize fonts");
	}

	cursor_manager_ = new cursor::manager;
	cursor::set(cursor::WAIT);

	// in order to 
	loadscreen_manager_ = new loadscreen::global_loadscreen_manager(video_);
	loadscreen::start_stage("titlescreen");

	res = gui2::init();
	if (!res) {
		throw twml_exception("could not initialize gui2-subsystem");
	}
	gui2_event_manager_ = new gui2::event::tmanager;

	// load core config require some time, so execute after gui2::init.
	res = load_data_bin();
	if (!res) {
		throw twml_exception("could not initialize game config");
	}

	// if you modified chinesepinyin.txt, want to generate chinesepinyin.code, enalbe it.
	// chinese::chinesepinyin_raw_2_code();

	chinese::load_pinyin_code_4sort();
	chinese::load_pinyin_rsp();
	for (std::map<aplt::taplt_key, aplt::tapplet>::iterator it = applets_.begin(); it != applets_.end(); ++ it) {
		aplt::tapplet& aplt = it->second;
		aplt_set_pinyin(aplt);
	}

	app_load_pb();

	lua_ = new rose_lua_kernel;
	bg_task_.set_lua(*lua_);
	app_post_lua();

	std::pair<std::string, std::string> desire_theme = utils::split_app_prefix_id(preferences::theme());
	theme_switch_to(desire_theme.first, desire_theme.second);
}

void base_instance::uninitialize()
{
	SDL_Log("base_instance::uninitialize()---");
	VALIDATE(instance != nullptr, null_str);

	if (bg_task_.is_ing()) {
		stop_bg_task2();
	}

	// base_driver maybe use camera.
	app_uninitialize();

	VALIDATE(!camera_.tasking(), null_str);
	camera_.stop_avcapture();

	std::set<int> flags;
	for (std::map<int, tserver_*>::const_iterator it = servers_.begin(); it != servers_.end(); ++ it) {
		flags.insert(it->first);
	}
	for (std::set<int>::const_reverse_iterator it = flags.rbegin(); it != flags.rend(); ++ it) {
		unregister_server(*it);
	}
	VALIDATE(servers_.empty(), null_str);

	// 1. info all invoker, I will uninitialize.
	send_helper_.clear_msg();

	VALIDATE(base_msg_subscribers_.empty(), null_str);

	// in reverse order compaire to initialize 
	if (gui2_event_manager_) {
		delete gui2_event_manager_;
		gui2_event_manager_ = NULL;
	}

	if (loadscreen_manager_) {
		delete loadscreen_manager_;
		loadscreen_manager_ = NULL;
	}

	if (cursor_manager_) {
		delete cursor_manager_;
		cursor_manager_ = NULL;
	}

	if (lobby) {
		delete lobby;
		lobby = nullptr;
	}

	if (lua_ != nullptr) {
		delete lua_;
		lua_ = nullptr;
	}

	gui2::release();
	SDL_Log("---base_instance::uninitialize() X");
}

void base_instance::theme_switch_to(const std::string& app, const std::string& id)
{
	if (!theme::current_id.empty() && theme::current_id == utils::join_app_prefix_id(app, id)) {
		return;
	}

	const config* default_theme_cfg = nullptr;
	const config* theme_cfg = nullptr;
	BOOST_FOREACH(const config& cfg, core_cfg_.child_range("theme")) {
		if (cfg["app"].str() == app && cfg["id"].str() == id) {
			theme_cfg = &cfg;
			break;
		} else if (cfg["app"].str() == "rose" && cfg["id"].str() == "default") {
			default_theme_cfg = &cfg;
		}
	}
	VALIDATE(theme_cfg != nullptr || default_theme_cfg != nullptr, null_str);
	theme::switch_to(theme_cfg? *theme_cfg: *default_theme_cfg);

	preferences::set_theme(theme::current_id);
}

void base_instance::will_longblock()
{
	// app require can block >= game_config::min_longblock_threshold_s second.
	// if cannot, modify game_config::min_longblock_threshold_s to your capability.
	VALIDATE_IN_MAIN_THREAD();
	app_will_longblock();
}

void base_instance::handle_http_request(const net::HttpServerRequestInfo& info, net::tresponse_data& resp)
{
	app_handle_http_request(info, resp);
}

void base_instance::register_server(int flag, tserver_* _server)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(flag != 0 && servers_.count(flag) == 0, std::string("flag: ") + str_cast(flag));

	tserver_* server = _server;
	if (_server == nullptr) {
		if (flag == server_httpd) {
			server = &httpd_mgr_;
		} else if (flag == server_rtspd) {
			server = &live555d_mgr_;
		}
	}
	VALIDATE(server != nullptr, null_str);
	servers_.insert(std::make_pair(flag, server));

	VALIDATE(!server->started(), null_str);

	// all start during check_ip, immediate start.
	next_check_ip_ticks_ = SDL_GetTicks();
}

void base_instance::unregister_server(int flag)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(flag != 0, null_str);
	std::map<int, tserver_*>::iterator it = servers_.find(flag);
	VALIDATE(it != servers_.end(), null_str);

	tserver_* server = it->second;
	servers_.erase(it);

	if (server->started()) {
		tdisable_check_ip_lock lock(*this);
		server->stop();
	}
}

bool base_instance::start_servers_internal(uint32_t ipaddr)
{
	VALIDATE(!servers_.empty(), null_str);
	VALIDATE(ipaddr == ReceivingInterfaceAddr && ipaddr != INADDR_ANY, null_str);

	// start httpd before live555d, because live55d require httpd.
	for (std::map<int, tserver_*>::const_iterator it = servers_.begin(); it != servers_.end(); ++ it) {
		tserver_* server = it->second;
		if (!server->started()) {
			server->start(ipaddr);
			if (!server->started()) {
				server->stop();
			}
		}
	}
	return true;
}

void base_instance::stop_servers_internal()
{
	VALIDATE(!servers_.empty(), null_str);

	tdisable_check_ip_lock lock(*this);
	for (std::map<int, tserver_*>::const_reverse_iterator it = servers_.rbegin(); it != servers_.rend(); ++ it) {
		tserver_* server = it->second;
		if (server->started()) {
			server->stop();
		}
	}
	ReceivingInterfaceAddr = INADDR_ANY;
}

bool base_instance::server_registered(int flag) const
{
	VALIDATE(flag != 0, null_str);
	return servers_.count(flag) != 0;
}

bool base_instance::servers_ready() const
{
	return ReceivingInterfaceAddr != INADDR_ANY;
}

void base_instance::register_base_msg_subscriber(tbase_msg_subscriber* subscriber)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(base_msg_subscribers_.count(subscriber) == 0, null_str);

	base_msg_subscribers_.insert(subscriber);
}

void base_instance::unregister_base_msg_subscriber(tbase_msg_subscriber* subscriber)
{
	VALIDATE_IN_MAIN_THREAD();

	std::set<tbase_msg_subscriber*>::iterator it = base_msg_subscribers_.find(subscriber);
	VALIDATE(it != base_msg_subscribers_.end(), null_str);
	base_msg_subscribers_.erase(it);
}

bool base_instance::will_enter_sys_module(const std::string& reason)
{
	VALIDATE(!reason.empty(), null_str);
	if (bg_task().is_ing()) {
		utils::string_map symbols;
		symbols["aplt"] = reason;
		std::string warnning;
		// {
			warnning = vgettext2("A timing task is running. Do you want to stop the task and enter '$aplt'?", symbols);
		// }
		const std::string log = vgettext2("To enter $aplt, stop the task", symbols);
		if (!stop_bg_task_if_runing(warnning, log)) {
			return false;
		}
	}

	// driver_base2th maybe use camera.
	// VALIDATE(!camera_.tasking(), null_str);

	return true;
}

bool base_instance::will_enter_sys_module(int fakeid)
{
	VALIDATE(aplt::all_fake_applets.count(fakeid) != 0, null_str);

	return will_enter_sys_module(aplt::all_fake_applets.find(fakeid)->second.name);
}

void base_instance::set_sdl_reboothandler(SDL_RebootHandler handler, void* param)
{
	sdl_apphandlers_.reboot_handler = handler;
	sdl_apphandlers_.reboot_param = param;
	SDL_SetAppHandlers(&sdl_apphandlers_);
}

void base_instance::backup_did_file_write(const std::string& file, int backup_type)
{
	VALIDATE_IN_MAIN_THREAD();

	VALIDATE(!file.empty(), null_str);
	std::map<std::string, uint32_t>::iterator it;
	bool frist = true;
	if (backup_files_.count(file) != 0) {
		it = backup_files_.find(file);
		frist = false;
	} else {
		std::pair<std::map<std::string, uint32_t>::iterator, bool> ins = backup_files_.insert(std::make_pair(file, 0));
		it = ins.first;
	}
	bool update_ticks = true;
	if (backup_type == backup_on_fixed_delay) {
		update_ticks = frist;
	}
	if (update_ticks) {
		it->second = SDL_GetTicks();
		// SDL_Log("%u[backup_did_file_write](1/2)backup_files_(%i) %s, update ticks.", it->second, (int)backup_files_.size(), file.c_str());

	} else {
		// SDL_Log("%u[backup_did_file_write](1/2)backup_files_(%i) %s, don't update ticks.", it->second, (int)backup_files_.size(), file.c_str());
	}
}

void base_instance::backup_pump()
{
	// roc-rk3588s-pc: make sure 18 second is too small. 30 second fail occasionally.
	const int delay_ms = 40000;
	const uint32_t now = SDL_GetTicks();

	for (std::map<std::string, uint32_t>::iterator it = backup_files_.begin(); it != backup_files_.end(); ) {
		const std::string& src_name = it->first;
		const uint32_t wrote_ticks = it->second;
		if ((int)(now - wrote_ticks) < delay_ms) {
			++ it;
			continue;
		}
		// SDL_Log("%u[backup_did_file_write](2/2)backup_files_(%i) %s", now, (int)backup_files_.size(), src_name.c_str());
		const int backup = backup_on_idle;
		backup_did_file_write_simple(src_name, backup);
		backup_files_.erase(it ++);
	}
}

bool base_instance::invalidate_layout(bool clear)
{
	bool result = invalidate_layout_;
	if (result && clear) {
		invalidate_layout_ = false;
	}
	return result;
}

tpoint base_instance::get_landscape_size() const
{
	tpoint result(0, 0);
	if (game_config::os == os_ios || game_config::os == os_android) {
		// width/height from preferences.cfg must be landscape.
		SDL_Rect rc = video_.bound();

		SDL_Log("init_video, video_.bound(): (%i, %i, %i, %i), landscape: %s", rc.x, rc.y, rc.w, rc.h, gui2::twidget::current_landscape? "true": "false");

		if (rc.w < rc.h) {
			int tmp = rc.w;
			rc.w = rc.h;
			rc.h = tmp;
		}

		if (game_config::os == os_ios) {
			rc.w *= gui2::twidget::hdpi_scale;
			rc.h *= gui2::twidget::hdpi_scale;
		}

		if (!gui2::twidget::should_conside_orientation(rc.w, rc.h)) {
			// for iOS/Android, it is get screen's width/height first.
			// rule: if should_conside_orientation() is false, current_landscape always must be true.
			// gui2::twidget::current_landscape = true;
		}
		// tpoint normal_size = gui2::twidget::orientation_swap_size(rc.w, rc.h);
		result.x = rc.w;
		result.y = rc.h;

	} else {
		result = preferences::landscape_size(video_);
	}

	return result;
}

void base_instance::set_fullscreen(bool fullscreen)
{
	VALIDATE(game_config::fullscreen != fullscreen, null_str);
	SDL_SetFullscreen(fullscreen? SDL_TRUE: SDL_FALSE);
	game_config::fullscreen = fullscreen;

	// game_config::statusbar_height = game_config::fullscreen? 0: SDL_GetStatusBarHeight();
}

bool base_instance::set_orientation(int orientation, bool with_mode)
{
	VALIDATE(gui2::connectd_window().empty(), null_str);
	bool landscape = gui2::twidget::current_landscape;
	if (orientation != nposm) {
		VALIDATE(orientation == orientation_portrait || orientation == orientation_landscape, null_str);
		landscape = orientation == orientation_landscape;

		VALIDATE(landscape != gui2::twidget::current_landscape, null_str);
		gui2::twidget::current_landscape = landscape;
	}
	SDL_SetOrientation(landscape? SDL_TRUE: SDL_FALSE);

	SDL_SetStatusBarColor(SDL_FALSE);

	bool result = true;
	if (with_mode) {
		result = set_mode();
	}
	return result;
}

bool base_instance::set_mode()
{
	int video_flags = 0;
	if (game_config::os == os_windows) {
		if (preferences::fullscreen()) {
			video_flags |= SDL_WINDOW_FULLSCREEN;
		} else {
			const tpoint landscape_size = get_landscape_size();

			if (preferences::maximized()) {
				video_flags |= SDL_WINDOW_MAXIMIZED;
			}
		}

	} else if (game_config::os == os_ios) {
		// on ios, use SDL_WINDOW_FULLSCREEN, and must not set SDL_WINDOW_BORDERLESS.
		video_flags = game_config::fullscreen? SDL_WINDOW_FULLSCREEN: 0;

	} else if (game_config::os == os_android) {
		// base_instance::set_fullscreen is responsible for managing the full screen, where must not set SDL_WINDOW_FULLSCREEN flag.
	}

	const tpoint landscape_size = get_landscape_size();
	const tpoint resolution = gui2::twidget::orientation_swap_size(landscape_size.x, landscape_size.y);

	SDL_Log("set_mode, setting mode to %ix%ix32, landscape: %s", 
		resolution.x, resolution.y, gui2::twidget::current_landscape? "true": "false");
	if (!video_.setMode(resolution.x, resolution.y, video_flags)) {
		SDL_Log("set_mode, required video mode, %i x %i x32 is not supported", resolution.x, resolution.y);
		return false;
	}
	SDL_Log("set_mode, using mode %ix%ix32", video_.getx(), video_.gety());
	return true;
}

/*
void base_instance::increment_textdomain_usage(const std::string& aplt_id)
{
	const aplt::tapplet* aplt_ptr = aplt::aplt_from_id(applets_, aplt_id);

	// key use res_path.
	// src_development and src_distrubite have same res_path
	VALIDATE(aplt_ptr != nullptr, null_str);

	if (!aplt_ptr->fake) {
		increment_textdomain_usage(*aplt_ptr);
	}
}
*/
void base_instance::increment_textdomain_usage(const aplt::tapplet& aplt)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(!aplt.fake, null_str);

	const std::string textdomain = bundleid_2_lua_bundleid(aplt.bundleid) + "-lib";

	// key use textdomain.
	// src_development and src_distrubite have same res_path
	if (aplt_textdomain_usage_.count(textdomain) == 0) {
		const std::string path_without_translations = aplt.res_path;

		VALIDATE(!textdomain.empty(), null_str);
		VALIDATE(!path_without_translations.empty(), null_str);

		// bindtextdomain must be before load gui/*.cfg
		const std::string path = path_without_translations + "/translations";
		bindtextdomain(textdomain.c_str(), path.c_str());
		bind_textdomain_codeset(textdomain.c_str(), "UTF-8");
	}
	++ (aplt_textdomain_usage_[textdomain]);
}

void base_instance::decrement_textdomain_usage(const aplt::tapplet& aplt)
{
	VALIDATE_IN_MAIN_THREAD();
	VALIDATE(!aplt.fake, null_str);

	const std::string textdomain = bundleid_2_lua_bundleid(aplt.bundleid) + "-lib";

	std::map<std::string, int>::iterator this_usage = aplt_textdomain_usage_.find(textdomain);
	VALIDATE(this_usage != aplt_textdomain_usage_.end(), null_str);
	if (-- (this_usage->second) == 0) {
		VALIDATE(!textdomain.empty(), null_str);
		unbindtextdomain(textdomain.c_str());

		aplt_textdomain_usage_.erase(this_usage);
	}
}

tlobby* base_instance::create_lobby()
{
	return new tlobby(new tlobby::tchat_sock());
}

void base_instance::stop_bg_task(bool fail_retry) 
{
	bg_task_.stop2(fail_retry);
}

void base_instance::stop_bg_task2()
{
	bg_task_.stop2(false);
	if (app_in_pure_task_cpp()) {
		bg_task_.stop2(false);
	}
}

// return value: (true)This time, a timed task was stopped
bool base_instance::stop_bg_task_if_runing(const std::string& warnning, const std::string& log)
{
	if (!bg_task_.is_ing()) {
		return false;
	}
	if (!warnning.empty()) {
		aplt::tbg_task::tdisable_stop_timing_lock lock(bg_task_);

		if (gui2::show_message2(null_str, warnning, gui2::tmessage::yes_no_buttons) != gui2::twindow::OK) {
			return false;
		}
		// During show_message2, the task may have stopeed.
		if (!bg_task_.is_ing()) {
			return true;
		}
	}
	if (!log.empty()) {
		bg_task_.add_log2(time(nullptr), log, 0, false);
	}

	stop_bg_task2();
	// app_clear_taskpoint();
	return true;
}

bool base_instance::lua_did_navigation_start(const aplt::tapplet& aplt, const std::string& position_uuid, const fn_navigation_bh& luafunc)
{
	VALIDATE(luafunc != NULL, null_str);

	return app_start_navigation(aplt, position_uuid, luafunc);
}

void base_instance::bg_task_will_start(const aplt::tbg_task::tbase_bg_task2& sys_task)
{
	std::stringstream msg_ss;
	const aplt::taplt_task* aplt_task = sys_task.aplt_task;

	if (aplt_task == nullptr) {
		if (sys_task.klink_cpp_aplt_task2 == nullptr) {
			// it is trigger by gui-center. now do nothing.
			VALIDATE(sys_task.in_task_cpp(), null_str);
			return;
		}
		aplt_task = sys_task.klink_cpp_aplt_task2;
	}

	// add log
	msg_ss << _("timing^It is time to execute");
	add_aplt_task_log(aplt_task->aplt_id, aplt_task->task_id, aplt_task->ble_device_id, time(nullptr), msg_ss.str(), 0, false);

	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->bg_task_will_start(sys_task);
	}

	app_bg_task_will_start(sys_task);
}

void base_instance::bg_task_stopped(const aplt::tbg_task::tbase_bg_task2& sys_task, bool log)
{
	if (sys_task.aplt_task != nullptr) {
		const aplt::taplt_task& aplt_task = *sys_task.aplt_task;

		// add log
		if (log) {
			add_aplt_task_log(aplt_task.aplt_id, aplt_task.task_id, aplt_task.ble_device_id, time(nullptr), bg_task_.task_state_desc(aplt_task.state), 0, false);
		}

	} else {
		// add log
		if (log) {
			VALIDATE(sys_task.in_pure_task_cpp(), null_str);
			aplt::ttask_pair task_cpp_pair = sys_task.task_cpp_pair();

			// for task_cpp finished, ok always.
			// const std::string msg = bg_task_.task_state_desc(aplt::taplt_task::state_finished_ok);
			const std::string msg = _("cpp task is finished");
			add_aplt_task_log(task_cpp_pair.aplt->id, task_cpp_pair.task->id, null_str, time(nullptr), msg, 0, false);
		}
	}

	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->bg_task_stopped(sys_task);
	}

	app_bg_task_stopped(sys_task);
}

void base_instance::logs_pb_log_added(int count, const pb2::tlog& log)
{
	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->logs_pb_log_added(count, log);
	}
}

void base_instance::iot_did_heartbeats2(const std::set<aplt::tiot_heartbeat>& heartbeats)
{
	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->iot_did_heartbeats2(heartbeats);
	}
}

void base_instance::iot_did_events2(const std::set<aplt::tiot_event>& events)
{
	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->iot_did_events2(events);
	}
}

std::string base_instance::can_send_nlp_question2(int src) const
{
	VALIDATE(src == aplt::chatsrc_speech, null_str);
	std::string err_msg;
	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		err_msg = subscriber->can_send_nlp_question2(src);
		if (!err_msg.empty()) {
			return err_msg;
		}
	}
	return err_msg;
}

void base_instance::aiagent_did_nlp_answer2(int src, bool retbool, const std::string& answer, int input_tokens, int output_tokens)
{
	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->aiagent_did_nlp_answer2(src, retbool, answer, input_tokens, output_tokens);
	}
}

void base_instance::speech_did_recognition_result2(const std::string& result)
{
	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->speech_did_recognition_result2(result);
	}
}

void base_instance::base_scene_state_changed(const aplt::tbase_scene& scene, int to_state)
{
	// VALIDATE(to_state == aplt::sts_idle, null_str);
	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->base_scene_state_changed(scene, to_state);
	}
}

void base_instance::pinyin_did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start)
{
	// VALIDATE(to_state == aplt::sts_idle, null_str);
	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->pinyin_did_speak_stopped(id, text, unplayed_start);
	}
}

void base_instance::klink_tasks_changed2()
{
	// VALIDATE(to_state == aplt::sts_idle, null_str);
	for (std::set<tbase_msg_subscriber*>::const_iterator it = base_msg_subscribers_.begin(); it != base_msg_subscribers_.end(); ++ it) {
		tbase_msg_subscriber* subscriber = *it;
		subscriber->klink_tasks_changed2();
	}
}

void base_instance::load_logs_pb(int type, int max_days, int max_logs)
{
	VALIDATE(max_days > 0 && max_logs > 0, null_str);
	VALIDATE(logs_pb_type_ == nposm, null_str);

	logs_pb_type_ = type;
	logs_pb_max_days_ = max_days;
	logs_pb_max_logs_ = max_logs;

	protobuf::load_sha1pb(type, true);
	const int logs_pb_version = 1;
	if (logs_pb_.version() != logs_pb_version) {
		logs_pb_.set_version(logs_pb_version);
		// pb_klink_.set_timing_ts(0);
		if (logs_pb_.logs_size() > 0) {
			logs_pb_.mutable_logs()->DeleteSubrange(0, logs_pb_.logs_size());
		}
	}

	adjust_logs_pb();
}

static bool did_walk_adjust_log_images(const std::string& logs_path, const SDL_dirent2* dirent, int& had_deletes, int64_t at_least_time)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	if (!isdir) {
		// img_xxx_20241128140345_0.png
		int min_filename_size = LOG_IMG_PREFIX.size() + 1 + 20;
		if ((int)SDL_strlen(dirent->name) < min_filename_size) {
			return true;
		}

		const std::string name = utils::lowercase(dirent->name);
		if (name.find(LOG_IMG_PREFIX) != 0) {
			return true;
		}

		const int size = name.size();
		const char* c_str = name.c_str();

		bool to_ts_fail;
		bool hit = false;
		for (int at = 0; at < nb_rose_image_exts; at ++) {
			const tcharcdata_C& extname = rose_image_exts[at];
			int dot_should_pos = size - extname.len - 1;
			if (c_str[dot_should_pos] != '.') {
				continue;
			}
			if (SDL_strncmp(c_str + size - extname.len, extname.ptr, extname.len) != 0) {
				continue;
			}
			size_t r1th = name.rfind('_', dot_should_pos);
			if (r1th == std::string::npos) {
				continue;
			}
			size_t r2th = name.rfind('_', r1th - 1);
			if (r2th == std::string::npos) {
				continue;
			}
			
			// now: [r2th: reverse second '_', r1th: reverse first '_']
			r1th -= 1;
			r2th += 1;
			// now: [r2th: first digit, r1th: last digit]

			// 20241128140345
			const int digits = 14;
			if (r1th - r2th + 1 != digits) {
				continue;
			}
			std::string sub = name.substr(r2th, digits);
			int ts = utils::yyyymmddhhmmss_2_ts(sub, &to_ts_fail);

			if (game_config::os == os_windows) {
				const std::string sub2 = utils::format_time_ymdhms2(ts);
				if (sub2 != sub) {
					std::stringstream err_msg;
					err_msg << "sub: " << sub << " != " << "sub2: " << sub2;
					VALIDATE(false, err_msg.str());
				}
			}
			if (to_ts_fail || ts < at_least_time) {
				hit = true;
				break;
			}
		}
		if (hit) {
			const std::string filename = logs_path + "/" + dirent->name;
			SDL_DeleteFiles(filename.c_str());
			had_deletes ++;
		}
	}
	int max_deltes_per_time = 50;
	return had_deletes < max_deltes_per_time;
}

void base_instance::adjust_logs_pb()
{
	VALIDATE_IN_MAIN_THREAD();

	bool logs_pb_dirty = false;
	const int at_most_save_seconds = logs_pb_max_days_ * ONE_DAY_SECONDS; // 2 days
	const int64_t at_least_time = time(nullptr) - at_most_save_seconds;
	int delete_from = nposm;
	int at = 0;

	// const int max_logs = 300; // 300/INT_MAX
	if (logs_pb_.logs_size() > logs_pb_max_logs_) {
		logs_pb_.mutable_logs()->DeleteSubrange(0, logs_pb_.logs_size() - logs_pb_max_logs_);
		logs_pb_dirty = true;
	}

	while (at < logs_pb_.logs_size()) {
		const pb2::tlog& log = logs_pb_.logs(at);
		if (log.timestamp() < at_least_time) {
			if (delete_from == nposm) {
				delete_from = at;
			}
		} else if (delete_from != nposm) {
			const int num = at - delete_from;
			VALIDATE(delete_from + num < logs_pb_.logs_size(), null_str);
			logs_pb_.mutable_logs()->DeleteSubrange(delete_from, num);
			delete_from = nposm;
			at -= num;
			logs_pb_dirty = true;
		}
		at ++;
	}

	if (delete_from != nposm) {
		const int num = at - delete_from;
		VALIDATE(delete_from + num == logs_pb_.logs_size(), null_str);
		logs_pb_.mutable_logs()->DeleteSubrange(delete_from, num);
		logs_pb_dirty = true;
	}

	if (logs_pb_dirty) {
		protobuf::write_sha1pb(logs_pb_type_, logs_pb_backup_type_);
	}

	// delete image files
	const std::string logs_path = get_saves_logs_path();
	int had_deletes = 0;
	walk_dir(logs_path, true, std::bind(&did_walk_adjust_log_images, _1, _2, std::ref(had_deletes), at_least_time));
}

void base_instance::add_aplt_task_log(const std::string& aplt_id, const std::string& task_id, const std::string& device_id, int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent)
{
	add_logs_pb_log(logtype_aplt_task, aplt_id, task_id, device_id, ts, msg, tokens, aiagent);
}

void base_instance::add_msg_only_log(int type, const std::string& msg, uint64_t tokens, bool aiagent)
{
	add_logs_pb_log(type, null_str, null_str, null_str, time(nullptr), msg, tokens, aiagent);
}

void base_instance::add_aplt_ts_msg_log(int type, const std::string& aplt_id, int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent)
{
	VALIDATE(type == logtype_iot_event || type == logtype_nlp_resp || type == logtype_center_chat || type == logtype_speech_recognition || type == logtype_warn, null_str);
	if (type == logtype_iot_event || type == logtype_center_chat || type == logtype_speech_recognition) {
		VALIDATE_IN_MAIN_THREAD();
	}

	add_logs_pb_log(type, aplt_id, null_str, null_str, ts, msg, tokens, aiagent);
}

void base_instance::add_logs_pb_log(int type, const std::string& aplt_id, const std::string& task_id, const std::string& device_id, 
	int64_t ts, const std::string& msg, uint64_t tokens, bool aiagent)
{
	VALIDATE(type >= 0 && type < logtype_count, null_str);
	if (type == logtype_iot_event || type == logtype_center_chat || type == logtype_speech_recognition) {
		VALIDATE_IN_MAIN_THREAD();
	}

	std::unique_ptr<threading::lock> lock;

	pb2::tlog* new_log = nullptr;
	if (IN_MAIN_THREAD()) {
		new_log = logs_pb_.add_logs();

	} else {
		lock.reset(new threading::lock(undelivered_logs_mutex_));
		undelivered_logs_.push_back(pb2::tlog());
		new_log = &undelivered_logs_.back();
	}

	new_log->set_type(type);
	new_log->set_timestamp(ts == nposm? time(nullptr): ts);
	new_log->set_aplt_id(aplt_id);
	new_log->set_task_id(task_id);
	new_log->set_device_id(device_id);
	new_log->set_msg(msg);
	new_log->set_tokens(tokens);
	new_log->set_aiagent(aiagent);

	if (IN_MAIN_THREAD()) {
		logs_pb_log_added(1, *new_log);

		protobuf::write_sha1pb(logs_pb_type_, logs_pb_backup_type_);
	}
}

void base_instance::undelivered_logs_pump()
{
	if (!undelivered_logs_.empty()) {
		const pb2::tlog* last_new_log = nullptr;
		const int count = undelivered_logs_.size();
		{
			threading::lock lock(undelivered_logs_mutex_);
			for (std::vector<pb2::tlog>::const_iterator it = undelivered_logs_.begin(); it != undelivered_logs_.end(); ++ it) {
				const pb2::tlog& log = *it;
				pb2::tlog* new_log = logs_pb_.add_logs();

				new_log->set_type(log.type());
				new_log->set_timestamp(log.timestamp());
				new_log->set_aplt_id(log.aplt_id());
				new_log->set_task_id(log.task_id());
				new_log->set_device_id(log.device_id());
				new_log->set_msg(log.msg());
				new_log->set_tokens(log.tokens());
				new_log->set_aiagent(log.aiagent());

				last_new_log = new_log;
			}
			undelivered_logs_.clear();
		}
		protobuf::write_sha1pb(logs_pb_type_, logs_pb_backup_type_);

		logs_pb_log_added(count, *last_new_log);
	}
}

static bool logtype_from_me(int logtype)
{
	return logtype == logtype_center_chat || logtype == logtype_speech_recognition;
}

static void logs_pb_log_2_msg2(const std::map<std::string, const aplt::tapplet*>& id_aplt_map, const pb2::tlog& log, bool first, std::string& result)
{
	VALIDATE(log.type() == logtype_aplt_task, null_str);

	const std::string ts_str = utils::format_time_date(log.timestamp());
	if (!first) {
		const std::string& msg = log.msg();
		const int max_extra_bytes = 64;
		int bytes = result.size() + ts_str.size() + msg.size() + max_extra_bytes;
		char* buf = (char*)malloc(bytes);
	
		// msg_len doesn't include '\0'
		int msg_len = SDL_snprintf(buf, bytes, "%s\n%s %s", result.c_str(), ts_str.c_str(), msg.c_str());
		result.assign(buf);
		free(buf);

		return;
	}

	result.clear();

	const std::string& timing_id = log.task_id();
	std::string aplt_name;
	std::string timing_name = timing_id;

	const aplt::tapplet* hit_aplt = nullptr;
	if (id_aplt_map.count(log.aplt_id()) != 0) {
		hit_aplt = id_aplt_map.find(log.aplt_id())->second;
		aplt_name = hit_aplt->name2();

	} else {
		aplt_name = log.aplt_id() + _("[Unloaded]");
	}

	if (hit_aplt != nullptr && hit_aplt->tasks.count(timing_id) != 0) {
		timing_name = hit_aplt->tasks.find(timing_id)->second.name;
	}

	if (!log.device_id().empty()) {
		timing_name += "-" + log.device_id();
	}

	const std::string& msg = log.msg();
	const int max_extra_bytes = 64;
	int bytes = aplt_name.size() + timing_name.size() + ts_str.size() + msg.size() + max_extra_bytes;
	char* buf = (char*)malloc(bytes);
	
	// msg_len doesn't include '\0'
	int msg_len = SDL_snprintf(buf, bytes, "%s(%s)\n%s %s", timing_name.c_str(), aplt_name.c_str(), ts_str.c_str(), msg.c_str());
	result.assign(buf);
	free(buf);
}

static void insert_log_row(gui2::tlistbox& list, int log_type, int64_t log_ts, const std::string& msg2, int start_at, int end_at, std::map<std::string, std::string>& data)
{
	VALIDATE(end_at >= start_at, null_str);

	const bool me = logtype_from_me(log_type);

	data["time"] = utils::format_time_date(log_ts);

	// list_item_item.insert(std::make_pair("lportrait", "misc/robot_icon.png"));

	// list_item_item.insert(std::make_pair("rportrait", "misc/me_icon.png"));

	data["msg"] = msg2;

	{
		// SDL_Log("msg: %s", msg2.c_str());
	}

	gui2::ttoggle_panel& panel = list.insert_row(data);

	panel.set_cookie(posix_mku64(start_at, end_at));

	gui2::twidget& portrait = gui2::find_widget<gui2::twidget>(&panel, me? "lportrait": "rportrait", false);
	portrait.set_visible(gui2::twidget::INVISIBLE);


	// tgrid& grid_msg = find_widget<tgrid>(&panel, "_grid_msg", false);
	// tgrid::tchild& child = grid_msg.child(0, 0);
	// child.flags_ &= ~tgrid::HORIZONTAL_MASK;
	// child.flags_ |= me? tgrid::HORIZONTAL_ALIGN_RIGHT: tgrid::HORIZONTAL_ALIGN_LEFT;

	gui2::tgrid& grid_msg = gui2::find_widget<gui2::tgrid>(&panel, "_msg_grid", false);
	gui2::tgrid::tchild& child = grid_msg.child(0, 1);
	child.flags_ &= ~gui2::tgrid::HORIZONTAL_MASK;
	child.flags_ |= me? gui2::tgrid::HORIZONTAL_ALIGN_RIGHT: gui2::tgrid::HORIZONTAL_ALIGN_LEFT;

	gui2::tlabel& msg = gui2::find_widget<gui2::tlabel>(&panel, "msg", false);
	msg.set_canvas_variable("border", variant(me? "border6": "border5"));
	panel.set_canvas_highlight(false, true);
}

void base_instance::join_msg_tokens(const std::string& msg, uint64_t tokens, std::string& msg2) const
{
	msg2 = msg;

	int input_tokens = 0;
	int output_tokens = 0;
	uint16_t flags = aplt::split_log_tokens(tokens, &input_tokens, &output_tokens);
	if (input_tokens != 0 || output_tokens != 0 || flags != 0) {
		// msg2 = msg;
		char buf[128] = {'\0'};
		if (flags & aplt::tokensflag_new_conversation) {
			if (input_tokens != 0 || output_tokens != 0) {
				SDL_snprintf(buf, sizeof(buf), "\n%s tokens: %i(%i+%i)", tokens_new_msgstr_.c_str(), input_tokens + output_tokens, input_tokens, output_tokens);

			} else {
				SDL_snprintf(buf, sizeof(buf), "\n%s", tokens_new_msgstr_.c_str());
			}

		} else {
			if (input_tokens != 0 || output_tokens != 0) {
				SDL_snprintf(buf, sizeof(buf), "\ntokens: %i(%i+%i)", input_tokens + output_tokens, input_tokens, output_tokens);
			}
		}
		if (buf[0] != '\0') {
			msg2.append(ht::generate_format(buf, 0xff808080, font::SIZE_SMALLER));
		}
	}
}

std::string base_instance::get_log_msg2(const pb2::tlog& log) const
{
	std::string result;

	const std::string& msg = log.msg();
	uint64_t tokens = log.tokens();

	join_msg_tokens(msg, tokens, result);
	return result;
}

void base_instance::logs_reload_log_list(gui2::tlistbox& list, bool aiagent_dlg)
{
	list.clear();

	std::map<std::string, std::string> data;
	data["lportrait"] = "misc/robot_icon.png";
	data["rportrait"] = "misc/me_icon.png";

	pb2::tlogs& logs_pb = instance->logs_pb();
	const int size = logs_pb.logs_size();

	struct ttiming_task2 {
		ttiming_task2()
			: at(nposm)
			, logs(0)
		{}

		bool valid() const { return at != nposm; }

		void set(int _at, const std::string& _aplt, const std::string& _id, const std::string& _pin)
		{
			at = _at;
			aplt = _aplt;
			id = _id;
			pin = _pin;
			VALIDATE(logs == 0, null_str);
		}

		void clear()
		{
			at = nposm;
			aplt.clear();
			id.clear();
			pin.clear();
			logs = 0;
		}

		int at;
		std::string aplt;
		std::string id;
		std::string pin;
		int logs;
	};
	ttiming_task2 curr_timing_task;

	std::map<std::string, const aplt::tapplet*> id_aplt_map;
	aplt::calculate_id_aplt_map(applets_, id_aplt_map);

	std::stringstream ss;
	std::string msg2;
	const int max_logs_per_line = 25;
	for (int at = 0; at < size; at ++) {
		// const pb2::tlog& log = logs_pb.logs(size - 1 - at);
		const pb2::tlog& log = logs_pb.logs(at);
		const int type = log.type();
		
		if (aiagent_dlg) {
			if (log.aiagent()) {
				// const std::string& msg = log.msg();
				// uint64_t tokens = log.tokens();
				insert_log_row(list, type, log.timestamp(), get_log_msg2(log), at, at, data);
			}
			continue;
		}

		if (type == logtype_aplt_task) {
			bool first = !curr_timing_task.valid();
			bool same = true;
			if (first) {
				curr_timing_task.set(at, log.aplt_id(), log.task_id(), log.device_id());

			} else if (log.aplt_id() != curr_timing_task.aplt || log.task_id() != curr_timing_task.id || log.device_id() != curr_timing_task.pin) {
				same = false;

			} else if (curr_timing_task.logs >= max_logs_per_line) {
				same = false;
			}

			int end_at = at;
			if (same) {
				logs_pb_log_2_msg2(id_aplt_map, log, first, msg2);
				curr_timing_task.logs ++;
				if (at != size - 1) {
					continue;
				}
			} else {
				// this is different timing_task
				end_at --;
			}

			insert_log_row(list, logtype_aplt_task, logs_pb.logs(curr_timing_task.at).timestamp(), 
				msg2, curr_timing_task.at, end_at, data);

			if (!same) {
				curr_timing_task.clear();
				// this is different timing_task, this log go back to 'for' again
				VALIDATE(at > 0, null_str);
				at --;
				
			}
			continue;

		} else if (curr_timing_task.valid()) {
			insert_log_row(list, logtype_aplt_task, logs_pb.logs(curr_timing_task.at).timestamp(), 
				msg2, curr_timing_task.at, at - 1, data);

			curr_timing_task.clear();
		}

		insert_log_row(list, type, log.timestamp(), get_log_msg2(log), at, at, data);
	}
	logs_validate_log_list(list, aiagent_dlg);
}

void base_instance::logs_validate_log_list(gui2::tlistbox& list, bool aiagent_dlg)
{
	// const bool validate = game_config::os == os_windows;
	const bool validate = true;
	if (!validate) {
		return;
	}

	const int rows = list.rows();
	int next_should_at = 0;
	for (int at = 0; at < rows; at ++) {
		gui2::ttoggle_panel& row = list.row_panel(at);

		uint64_t cookie = row.cookie();
		int first_at = posix_lo32(cookie);
		int end_at = posix_hi32(cookie);

		if (first_at != next_should_at) {
			char buf[64];
			SDL_snprintf(buf, sizeof(buf), "at(%i): first_at(%i) != next_should_at(%i), end_at(%i)",
				at, first_at, next_should_at, end_at);
			if (!aiagent_dlg) {
				VALIDATE(first_at == next_should_at, buf);
			}
		}
		VALIDATE(end_at >= first_at, null_str);

		next_should_at = end_at + 1;
	}

	pb2::tlogs& logs_pb = instance->logs_pb();
	if (!aiagent_dlg) {
		VALIDATE(next_should_at == logs_pb.logs_size(), null_str);
	}
}

void base_instance::logs_pb_log_added2(gui2::tlistbox& list, int count, const pb2::tlog& log, bool aiagent_dlg)
{
	VALIDATE(count > 0, null_str);

	std::map<std::string, std::string> data;
	data["lportrait"] = "misc/robot_icon.png";
	data["rportrait"] = "misc/me_icon.png";

	std::map<std::string, const aplt::tapplet*> id_aplt_map;

	pb2::tlogs& logs_pb = instance->logs_pb();
	const int size = logs_pb.logs_size();

	const int rows = list.rows();
	if (rows != size - count) {
		// Previously triggered: VALIDATE(first_at == next_should_at, ...), but I couldn't find the reason.
		// This is a method of guessing. But on Windows, I still hope to get an 'VALIDATE'. 
		// non-windows will execute it, for example: android.
		if (game_config::os != os_windows) {
			logs_reload_log_list(list, aiagent_dlg);
			list.scroll_to_row2(list.rows() - 1);
			return;
		}
	}

	std::string msg2;
	for (int at = size - count; at < size; at ++) {
		const pb2::tlog& log = logs_pb.logs(at);
		const int log_type = log.type();

		if (aiagent_dlg) {
			if (log.aiagent()) {
				// const std::string& msg = log.msg();
				// uint64_t tokens = log.tokens();
				insert_log_row(list, log_type, log.timestamp(), get_log_msg2(log), at, at, data);
			}
			continue;
		}

		if (log_type == logtype_aplt_task) {
			if (id_aplt_map.empty()) {
				aplt::calculate_id_aplt_map(applets_, id_aplt_map);
			}
		}

		if (at == 0) {
			if (log_type == logtype_aplt_task) {
				logs_pb_log_2_msg2(id_aplt_map, log, true, msg2);

			} else {
				msg2 = get_log_msg2(log);
			}

			insert_log_row(list, log.type(), log.timestamp(), msg2, at, at, data);
			continue;
		}

		const pb2::tlog& front_log = logs_pb.logs(at - 1);
		const int front_log_type = front_log.type();

		if (front_log_type == logtype_aplt_task && log_type == logtype_aplt_task) {
			// maybe combine
			bool same = true;
			if (log.aplt_id() != front_log.aplt_id() || log.task_id() != front_log.task_id() || log.device_id() != front_log.device_id()) {
				same = false;
			}
			if (same) {
				VALIDATE(list.rows() > 0, null_str);
				gui2::ttoggle_panel& row = list.row_panel(list.rows() - 1);
				gui2::tlabel& msg_widget = gui2::find_widget<gui2::tlabel>(&row, "msg", false);

				msg2 = msg_widget.label();
				logs_pb_log_2_msg2(id_aplt_map, log, false, msg2);
				uint64_t cookie = row.cookie();
				int first_at = posix_lo32(cookie);
				int end_at = posix_hi32(cookie);
				VALIDATE(end_at == at - 1, null_str);
				end_at ++;

				row.set_cookie(posix_mku64(first_at, end_at));
				msg_widget.set_label(msg2);
				continue;
			}
		}
		if (log_type == logtype_aplt_task) {
			logs_pb_log_2_msg2(id_aplt_map, log, true, msg2);
		} else {
			msg2 = get_log_msg2(log);
		}

		insert_log_row(list, log.type(), log.timestamp(), msg2, at, at, data);
	}

	logs_validate_log_list(list, aiagent_dlg);
	list.scroll_to_row2(list.rows() - 1);
}

#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/dns/host_resolver.h"
#include <net/socket/client_socket_factory.h>
#include <net/socket/stream_socket.h>
#include "net/socket/tcp_server_socket.h"
#include "net/base/net_errors.h"

bool check_ip_valid(uint32_t ip, uint16_t check_port)
{
	VALIDATE(check_port != 0, null_str);
	// check this ip is valid or not.

	// If in window and use WLAN. require disable WLAN adapter. 
	// if disconect from all ap, but enable WLAN adapter, ListenWithAddressAndPort still return net::OK.
	// In order to improve the accuracy, this method needs to be revised in the future.
/*
	net::IPAddress address((const uint8_t*)&ip, 4);
	net::AddressList addresses_(net::IPEndPoint(address, 6554));

	net::NetLogWithSource net_log2_;
	net::ClientSocketFactory* const socket_factory_ = net::ClientSocketFactory::GetDefaultFactory();
	std::unique_ptr<net::StreamSocket> socket = socket_factory_->CreateTransportClientSocket(addresses_, nullptr, net_log2_.net_log(), net_log2_.source());
*/
	uint32_t addrNBO = htonl(ip);
	std::stringstream addr_ss;
	addr_ss << (int)((addrNBO >> 24) & 0xFF) << "." << (int)((addrNBO>>16) & 0xFF);
	addr_ss << "." << (int)((addrNBO>>8)&0xFF) << "." << (int)(addrNBO & 0xFF);

	std::unique_ptr<net::ServerSocket> server_socket(new net::TCPServerSocket(NULL, net::NetLogSource()));
	int ret = server_socket->ListenWithAddressAndPort(addr_ss.str(), check_port, 1);
	if (ret != net::OK) {
		SDL_Log("check_ip, ListenWithAddressAndPort(%s, %i), ret: %i", addr_ss.str().c_str(), check_port, ret);
	}

	return ret == net::OK;
}

void base_instance::make_ip_invalid()
{
	SDL_Log("base_instance::make_ip_invalid, original ip_require_invalid_: %s", ip_require_invalid_? "true": "false");
	ip_require_invalid_ = true;
}

uint32_t base_instance::current_ip() const
{ 
	return ReceivingInterfaceAddr; 
}

void base_instance::check_ip_slice()
{
	if (disable_check_ip_ || servers_.empty()) {
		return;
	}
	const uint32_t now = SDL_GetTicks();
	if (now >= next_check_ip_ticks_) {
		// SDL_Log("%u [%s]check_ip, now is in %s, check... [%s]", now, foreground_? "foreground": "background", 
		//	ReceivingInterfaceAddr == INADDR_ANY? "no-ip": "has-ip", slice? "slice": "normal");

		if (ReceivingInterfaceAddr == INADDR_ANY) {
			// now think no valid ip
			ReceivingInterfaceAddr = get_local_ipaddr();
			if (ReceivingInterfaceAddr != INADDR_ANY) {
				SDL_Log("check_ip, thie time get valid ipaddr: 0x%08x", ReceivingInterfaceAddr);
				start_servers_internal(ReceivingInterfaceAddr);
			} else {
				for (std::map<int, tserver_*>::const_iterator it = servers_.begin(); it != servers_.end(); ++ it) {
					const tserver_* server = it->second;
					VALIDATE(!server->started(), null_str);
				}
			}
		} else {
			// now think valid ip
			bool ok = false;
			if (!ip_require_invalid_) {
				ok = check_ip_valid(ReceivingInterfaceAddr, server_check_port_);

			} else {
				SDL_Log("check_ip, 0x%08x, ip_require_invalid_ is true, stop servers", ReceivingInterfaceAddr);
				ip_require_invalid_ = false;
			}
			
			if (!ok) {
				stop_servers_internal();
			} else {
				// there is maybe some server require it start.
				start_servers_internal(ReceivingInterfaceAddr);
			}
		}
		next_check_ip_ticks_ = SDL_GetTicks() + check_ip_threshold_;
	}
}

void base_instance::block_slice()
{
	check_ip_slice();

	sdl_thread_.pump();
	lobby->pump();

	if (current_ble_ != nullptr) {
		current_ble_->pump();
	}

	undelivered_logs_pump();

	bg_task_.pump();
	backup_pump();

	camera_.main_OnFrame_when_idle();
	bool nosignal = camera_.nosignal();
	if (camera_.tasking()) {
		if (nosignal && (SDL_GetTicks() > last_report_camera_nosignal_ticks_)) {
			int threshold = camera_.tasking_check_singal_threshold();
			last_report_camera_nosignal_ticks_ = SDL_GetTicks() + threshold;

			utils::string_map symbols;
			symbols["duration"] = utils::format_elapse_hms(threshold / 1000);
			const std::string msg = vgettext2("The camera has not received an image for more than $duration", symbols);
			chinese::curr_pinyin.speak(msg);
		}

	} else if (nosignal || camera_.expired()) {
		// SDL_Log("%u {dbg_camera}block_slice[1/2] pre call camera_.stop_avcapture()", SDL_GetTicks());
		camera_.stop_avcapture();
		// SDL_Log("%u {dbg_camera}block_slice[2/2] post call camera_.stop_avcapture()", SDL_GetTicks());
	}

	app_aplt_slice();
}

void base_instance::nonblock_slice()
{
	chinese::curr_pinyin.pump();

	app_ros_slice();
}