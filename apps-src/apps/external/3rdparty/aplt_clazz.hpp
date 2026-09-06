/* $Id: dialog.hpp 50956 2011-08-30 19:41:22Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE2_APLT_CLAZZ_HPP_INCLUDED
#define LIBROSE2_APLT_CLAZZ_HPP_INCLUDED

#include <SDL.h>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include <map>
#include "3rdparty_decl.h"
#include "rose_util.hpp"
#include "rose_thread.hpp"

namespace cv {
class Mat;
};

namespace aplt {

class treq_task;

enum {cpp_id_sys_dlg_closed, cpp_id_sys_count, cpp_id_aplt_min = 100};


//
// pinyin
//
#define PINYIN_DEF_TONE				0
#define PINYIN_DEF_ENG_LOWERCASE	true

#define speakid_nposm				0
#define speakid_repeat				9999

enum {
	ampmode_1x,		// 1m (1.0x)
	ampmode_1_25x,	// 3~4m (1.25x)
	ampmode_1_6x,	// 5m   (1.6x)
	ampmode_2x,		// Outdoor / noisy environment. (2.0x)
	ampmode_count,
};

extern LIB3RDPARTY_DECL std::map<int, tcode3> amp_modes;
LIB3RDPARTY_DECL int amp_mode_from_str(const std::string& str, bool nposm_to_1x);

class LIB3RDPARTY_DECL tpinyin
{
public:
	tpinyin();
	virtual ~tpinyin();

	posix_noncopyable(tpinyin);

	virtual uint32_t speak(const std::string& text) = 0;
	virtual bool is_speaking() const = 0;
	virtual const std::string& get_text() = 0;

	virtual void repeat_speak(const std::string& _text) = 0;
	virtual bool is_repeat_speaking() = 0;

	void stop_speak2()
	{
		if (is_repeat_speaking()) {
			repeat_speak(null_str);
		} else {
			speak(null_str);
		}
	}

	virtual void set_amp_mode(int mode) {}
	virtual int get_amp_mode() const { return ampmode_1x; }

	virtual std::string from_unicodes(const wchar_t* unicodes, int count, int tone, bool eng_lowercase, tint32data_C* pos_data, bool for_match) = 0;
	virtual std::string from_utf8str(const std::string& str, int tone, bool eng_lowercase, tint32data_C* pos_data, bool for_match) = 0;

	std::string from_utf8str2(const std::string& str, int tone, bool eng_lowercase)
	{
		return from_utf8str(str, tone, eng_lowercase, nullptr, true);
	}

protected:
	uint32_t next_id();
	virtual void did_speak_stopped(uint32_t id, const std::string& text, int unplayed_start) {}

protected:
	uint32_t sid_;
};

LIB3RDPARTY_DECL tpinyin& get_curr_pinyin();

class LIB3RDPARTY_DECL tvaluex
{
public:
	tvaluex()
		: battery_level(float_nposm)
		, NMTHREAD_battery_level_(float_nposm)
		, last_NMTHREAD_battery_level_ticks_(0)
	{
		memset(euler, 0, sizeof(euler));
		memset(angular_vel, 0, sizeof(angular_vel));
	}

	void nposm_NMTHREAD_battery_level();
	void set_NMTHREAD_battery_level(double level);
	void flip_battery_level();
	uint32_t last_NMTHREAD_battery_level_ticks();

public:
	// batery level use to in main thread. non-main thread must not use it.
	// 1. (main thread)call initial_battery_level_non_main_thread()
	// 1. (non-main thread)[base driver]call set_battery_level_non_main_thread()
	// 2. (main thread)call flip_battery_level() every N seconds.
	// 3. (main thread)free to use battery_level without mutex_.
	double battery_level; // voltage
	double euler[3]; // radian
	double angular_vel[3]; // radian/s

private:
	threading::mutex mutex_;

	// battery level in non-main thread
	double NMTHREAD_battery_level_; // voltage
	uint32_t last_NMTHREAD_battery_level_ticks_;
};

extern LIB3RDPARTY_DECL tvaluex valuex;

#define HAS_BATTERY()	(!is_float_nposm(aplt::valuex.battery_level))

class LIB3RDPARTY_DECL trpy_sensor
{
public:
    trpy_sensor();
    ~trpy_sensor();
    
	// Initialize the sensor.
    bool init();

	bool valid() const { return accel_sensor_ != nullptr; }
    
    // Update sensor data (call in the main loop).
    void update();
    
    // Determine whether it is perpendicular to the ground.
	enum {level_ok, level_warn, level_fail, level_count};
    int pitch_vertical_level(float* pitch_ptr) const;
    
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

LIB3RDPARTY_DECL trpy_sensor& get_rpy_sensor();

}

#endif

