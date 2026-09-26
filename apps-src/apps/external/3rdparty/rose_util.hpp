/*
   Copyright (C) 2003 - 2015 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  Templates and utility-routines for strings and numbers.
 */

#ifndef LIBROSE2_UTIL_HPP_INCLUDED
#define LIBROSE2_UTIL_HPP_INCLUDED

#include "rose_global.hpp"
#include <SDL_rwops.h>
#include <SDL_rect.h>
#include <SDL_timer.h>
#include <cmath>
#include <cstddef>
#include <limits>
#include <math.h> // cmath may not provide round()
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <algorithm>
#include <functional>
#include "3rdparty_decl.h"

extern LIB3RDPARTY_DECL const std::string null_str;
extern LIB3RDPARTY_DECL const std::string ellipsis_str;
extern LIB3RDPARTY_DECL const std::string uuid_nposm;
extern LIB3RDPARTY_DECL const std::string formatted_uuid_nposm;
extern LIB3RDPARTY_DECL const std::string str_nposm;
extern LIB3RDPARTY_DECL const SDL_Rect empty_rect;
extern LIB3RDPARTY_DECL const SDL_Rect null_rect;

#define is_empty_rect(rect)		(!(rect).w || !(rect).h)
#define is_null_rect(rect)		((rect).w < 0 || (rect).h < 0)

// (<SDL2>/SDL_thread.h)typedef unsigned long SDL_threadID;
extern LIB3RDPARTY_DECL unsigned long main_tid;

extern LIB3RDPARTY_DECL const std::string charge_pos_uuid;
extern LIB3RDPARTY_DECL std::string charge_pos_name;
extern LIB3RDPARTY_DECL std::string privacy_protect_msgstr;

extern LIB3RDPARTY_DECL const tcharcdata_C rose_image_exts[];
extern LIB3RDPARTY_DECL const int nb_rose_image_exts;

extern LIB3RDPARTY_DECL const std::string LOG_IMG_PREFIX;

extern LIB3RDPARTY_DECL const std::string widget_id_main_map;
extern LIB3RDPARTY_DECL const std::string widget_id_mini_map;


#ifndef GENERIC_READ
#define GENERIC_READ        (0x80000000L)
#define GENERIC_WRITE       (0x40000000L)

#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#endif

typedef SDL_RWops*		posix_file_t;
#define INVALID_FILE	NULL

#define posix_fopen(name, desired_access, create_disposition, file)	do { \
    char __mode[5];	\
	int __mode_pos = 0;	\
    if (create_disposition == CREATE_ALWAYS) {  \
        __mode[__mode_pos ++] = 'w';	\
        __mode[__mode_pos ++] = '+';	\
    } else {	\
		__mode[__mode_pos ++] = 'r';	\
        if (desired_access & GENERIC_WRITE) {    \
            __mode[__mode_pos ++] = '+';	\
        }   \
	}	\
	__mode[__mode_pos ++] = 'b';	\
	__mode[__mode_pos] = 0;	\
	file = SDL_RWFromFile(name, __mode);	\
} while(0)

#define posix_fseek(file, offset)	\
	SDL_RWseek(file, offset, RW_SEEK_SET)		

#define posix_fwrite(file, ptr, size)	\
	SDL_RWwrite(file, ptr, 1, size)

#define posix_fread(file, ptr, size)	\
	SDL_RWread(file, ptr, 1, size)

#define posix_fclose(file)			\
	SDL_RWclose(file)

#define posix_fsize(file)	\
	SDL_RWsize(file)

// api required for some scenarios
#define posix_fseek2(file, offset, whence)	\
	SDL_RWseek(file, offset, whence)

#define posix_ftell(file)	\
	SDL_RWtell(file)

class LIB3RDPARTY_DECL tfile
{
public:
	tfile(const std::string& file, uint32_t desired_access, uint32_t create_disposition);
	explicit tfile(posix_file_t fp);
	virtual ~tfile() { close();	}

	bool valid() const { return fp != INVALID_FILE; }

	int64_t read_2_data(const int reserve_pre_bytes = 0, const int reserve_post_bytes = 0);
	void resize_data(int size, int vsize = 0);
	int replace_span(const int start, int original_size, const char* new_data, int new_size, const int fsize);
	int replace_string(int fsize, const std::vector<std::pair<std::string, std::string> >& replaces, bool* dirty);

	void truncate(int64_t size) { truncate_size_ = size; }
	void close();

public:
	posix_file_t fp;
	char* data;
	int data_size;

protected:
	int64_t truncate_size_;
	bool can_truncate_;
};

LIB3RDPARTY_DECL void posix_touch_i32(int x, int y, int* min_x, int* min_y, int* max_x, int* max_y);
LIB3RDPARTY_DECL bool is_cpu_saver();
LIB3RDPARTY_DECL int posix_align_ceil2(int dividend, int divisor);

typedef std::function<bool (bool)> fn_navigation_bh;

struct bad_lexical_cast : std::exception
{
public:
	const char *what() const throw()
	{
		return "bad_lexical_cast";
	}
};

template<typename To, typename From>
To lexical_cast(From a)
{
	To res;
	std::stringstream str;

	if(str << a && str >> res) {
		return res;
	} else {
		throw bad_lexical_cast();
	}
}

template<typename To, typename From>
To lexical_cast_default(From a, To def=To())
{
	To res;
	std::stringstream str;

	if(str << a && str >> res) {
		return res;
	} else {
		return def;
	}
}

template<>
size_t lexical_cast<size_t, const std::string&>(const std::string& a);

template<>
size_t lexical_cast<size_t, const char*>(const char* a);

template<>
size_t lexical_cast_default<size_t, const std::string&>(const std::string& a, size_t def);

template<>
size_t lexical_cast_default<size_t, const char*>(const char* a, size_t def);

template<>
long lexical_cast<long, const std::string&>(const std::string& a);

template<>
long lexical_cast<long, const char*>(const char* a);

template<>
long lexical_cast_default<long, const std::string&>(const std::string& a, long def);

template<>
long lexical_cast_default<long, const char*>(const char* a, long def);

template<>
int lexical_cast<int, const std::string&>(const std::string& a);

template<>
int lexical_cast<int, const char*>(const char* a);

template<>
int lexical_cast_default<int, const std::string&>(const std::string& a, int def);

template<>
int lexical_cast_default<int, const char*>(const char* a, int def);

template<>
double lexical_cast<double, const std::string&>(const std::string& a);

template<>
double lexical_cast<double, const char*>(const char* a);

template<>
double lexical_cast_default<double, const std::string&>(const std::string& a, double def);

template<>
double lexical_cast_default<double, const char*>(const char* a, double def);

template<>
float lexical_cast<float, const std::string&>(const std::string& a);

template<>
float lexical_cast<float, const char*>(const char* a);

template<>
float lexical_cast_default<float, const std::string&>(const std::string& a, float def);

template<>
float lexical_cast_default<float, const char*>(const char* a, float def);

template<typename From>
std::string str_cast(From a)
{
	return lexical_cast<std::string,From>(a);
}

template<typename To, typename From>
To lexical_cast_in_range(From a, To def, To min, To max)
{
	To res;
	std::stringstream str;

	if(str << a && str >> res) {
		if(res < min) {
			return min;
		}
		if(res > max) {
			return max;
		}
		return res;
	} else {
		return def;
	}
}

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
		// VALIDATE(alpha > 0.05f, null_str);
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

typedef int32_t fixed_t;
# define fxp_shift 8
# define fxp_base (1 << fxp_shift)

// IN: float or int - OUT: fixed_t
# define ftofxp(x) (fixed_t((x) * fxp_base))

// IN: unsigned and fixed_t - OUT: unsigned
# define fxpmult(x,y) (((x)*(y)) >> fxp_shift)

// IN: unsigned and int - OUT: fixed_t
# define fxpdiv(x,y) (((x) << fxp_shift) / (y))

// IN: fixed_t - OUT: int
# define fxptoi(x) ( ((x)>0) ? ((x) >> fxp_shift) : (-((-(x)) >> fxp_shift)) )

namespace float_2_u16 {

#define N05P15_FLOAT_MIN -0.5f
#define N05P15_FLOAT_MAX 1.5f
#define N05P15_FLOAT_RANGE (N05P15_FLOAT_MAX - N05P15_FLOAT_MIN)  // 2.0f
#define N05P15_U16_MAX_VALUE_FALOT 65535.0f
#define N05P15_U16_MAX_VALUE_U16	65535

// Quantization function: convert float coordinates to uint16
inline uint16_t quantize_n05p15(float value)
{
    float normalized = (value - N05P15_FLOAT_MIN) / N05P15_FLOAT_RANGE;  // Map to [-0.5, 1.5]
    float scaled = normalized * N05P15_U16_MAX_VALUE_FALOT; // Map to [0, 65535]
    
    // Round and clamp to valid range
    int32_t int_val = (int32_t)(scaled + 0.5f);
    if (int_val < 0) int_val = 0;
    if (int_val > 65535) int_val = 65535;
    
    return (uint16_t)int_val;
}

// Dequantization function: restore uint16 coordinates to float
inline float dequantize_n05p15(uint16_t quantized)
{
    float normalized = quantized / N05P15_U16_MAX_VALUE_FALOT;   // Map to [0, 1]
    return N05P15_FLOAT_MIN + normalized * N05P15_FLOAT_RANGE;          // Map to [QUANT_MIN, QUANT_MAX]
}
/*
void test()
{   
    // Demonstrate boundary clamp effect
    SDL_Log("---float to uint16_t test---");
	float test_val_min = -1.0f;
	float test_val_max = 2.0f;
    for (float val = test_val_min; val < test_val_max; val += 0.01f) {
        uint16_t q = quantize_n05p15(val);
        float dq = dequantize_n05p15(q);
        SDL_Log("  %.4f -> %5u -> %.4f (Error: %.6f)", val, q, dq, fabsf(val - dq));
    }
	SDL_Log("------");
}
*/
}

namespace sound {

enum channel_group
{
	NULL_CHANNEL = -1,
	SOUND_SOURCES = 0,
	SOUND_BELL,
	SOUND_TIMER,
	SOUND_UI,
	SOUND_FX
};

}

#define PIXELS_PER_METER20  20
#define Y16_NO_DEPTH		INT16_MAX
#define MAX_CARTOGRAPHER_CELL_VAL	100
#define MOVEIT_SECTOR_HALF_ANGLE_DEG	30

enum {dctask_color, dctask_depth, dctask_d2c, dctask_count};

enum {dcframetype_color, dcframetype_depth, dcframetype_count};
enum {dcformat_y16, dcformat_y14, dcformat_rgb, dcformat_mjpeg, dcformat_count};
enum {dcframeidx_color, dcframeidx_depth, dcframeidx_count};

struct tdcframe_C
{
	int type;
	int format;
	int width;
	int height;
	const uint8_t* data;
	int data_size;
	float scale;
};

struct tdcintrinsics_C
{
	double cx;
	double cy;
	double fx;
	double fy;
	// It is not an value of intrinsics. 
	// It is only used to determine whether 4-values are valid or not.
	bool valid;
};

struct tbattery_info_C
{
	double max;
	double charge;
	double cutoff;
};

struct tspace4
{
	int l; // left
	int r; // right
	int t; // top
	int b; // bottom
};

struct tcode2
{
	tcode2()
		: code(nposm)
	{}

	tcode2(int code, const std::string& id)
		: code(code)
		, id(id)
	{}

	bool operator<(const tcode2& that) const { return code < that.code; }

	int code;
	std::string id;
};

struct tcode3
{
	tcode3()
		: code(nposm)
	{}

	tcode3(int code, const std::string& id, const std::string& name)
		: code(code)
		, id(id)
		, name(name)
	{}

	bool operator<(const tcode3& that) const { return code < that.code; }

	int code;
	std::string id;
	std::string name;
};

enum bool_set_t {bool_set_none, bool_set_false, bool_set_true, bool_set_count};
extern LIB3RDPARTY_DECL std::map<bool_set_t, tcode3> bool_set_types;

enum {
	ampmode_1x,		// 1m (1.0x)
	ampmode_1_25x,	// 3~4m (1.25x)
	ampmode_1_6x,	// 5m   (1.6x)
	ampmode_2x,		// Outdoor / noisy environment. (2.0x)
	ampmode_count,
};

// Holds a 2D point.
struct tpoint
{
	tpoint(const int x_, const int y_) :
		x(x_),
		y(y_)
		{}

	// x coodinate.
	int x;

	// y coodinate.
	int y;

	tpoint& operator=(const SDL_Point& point) 
	{ 
		x = point.x; 
		y = point.y; 
		return *this;
	}

	bool operator==(const tpoint& point) const { return x == point.x && y == point.y; }
	bool operator!=(const tpoint& point) const { return x != point.x || y != point.y; }
	bool operator<(const tpoint& point) const { return x < point.x || (x == point.x && y < point.y); }

	bool operator<=(const tpoint& point) const { return x < point.x || (x == point.x && y <= point.y); }

	tpoint operator+(const tpoint& point) const { return tpoint(x + point.x, y + point.y); }
	tpoint& operator+=(const tpoint& point) {
		x += point.x;
		y += point.y;
		return *this;
	}

	tpoint operator-(const tpoint& point) const { return tpoint(x - point.x, y - point.y); }
	tpoint& operator-=(const tpoint& point) {
		x -= point.x;
		y -= point.y;
		return *this;
	}
};

struct tpose2d_C {
	double x;
	double y;
	double yaw;
	bool valid;
};

struct tpose2d
{
	tpose2d()
		: x(0)
		, y(0)
		, yaw(0)
		, valid(false)
	{}

	tpose2d(double x, double y, double yaw)
		: x(x)
		, y(y)
		, yaw(yaw)
		, valid(true)
	{}

	tpose2d(const tpose2d_C& that)
		: x(that.x)
		, y(that.y)
		, yaw(that.yaw)
		, valid(that.valid)
	{}

	void set(double _x, double _y, double _yaw, bool _valid)
	{
		x = _x;
		y = _y;
		yaw = _yaw;
		valid = _valid;
	}

	tpose2d_C to_pose2d_C() const
	{
		return tpose2d_C{x, y, yaw, valid};
	}

	std::string to_string(bool with_valid = false) const
	{
		char buf[64];
		if (with_valid) {
			SDL_snprintf(buf, sizeof(buf), "{t[%.3f, %.3f]q:%.3f, %s}", x, y, RAD2DEG(yaw), valid? "true": "false"); 
		} else {
			SDL_snprintf(buf, sizeof(buf), "{t[%.3f, %.3f]q:%.3f}", x, y, RAD2DEG(yaw)); 
		}
		return buf;
	}

	double x;
	double y;
	double yaw;
	bool valid;
};

// if no roll/pitch/yaw, use SDL_FPoint3 or SDL_DPoint3
struct tpose3d
{
	tpose3d()
		: x(0)
		, y(0)
		, z(0)
		, roll(0)
		, pitch(0)
		, yaw(0)
		, valid(false)
	{}

	tpose3d(double x, double y, double z, double roll, double pitch, double yaw)
		: x(x)
		, y(y)
		, z(z)
		, roll(roll)
		, pitch(pitch)
		, yaw(yaw)
		, valid(true)
	{}

	void set(double _x, double _y, double _z, double _roll, double _pitch, double _yaw, bool _valid)
	{
		x = _x;
		y = _y;
		z = _z;
		roll = _roll;
		pitch = _pitch;
		yaw = _yaw;
		valid = _valid;
	}

	std::string to_string(bool with_valid = false) const
	{
		char buf[128];
		if (with_valid) {
			SDL_snprintf(buf, sizeof(buf), "{t[%.3f %.3f %.3f]q[%.3f %.3f %.3f] %s}", x, y, z, RAD2DEG(roll), RAD2DEG(pitch), RAD2DEG(yaw), valid? "true": "false"); 
		} else {
			SDL_snprintf(buf, sizeof(buf), "{t[%.3f %.3f %.3f]q[%.3f %.3f %.3f]}", x, y, z, RAD2DEG(roll), RAD2DEG(pitch), RAD2DEG(yaw)); 
		}
		return buf;
	}

	double x;
	double y;
	double z;
	double roll;
	double pitch;
	double yaw;
	bool valid;
};

enum {rpy_roll, rpy_pitch, rpy_yaw, rpy_count};
struct trpy
{
	trpy()
		: roll(0)
		, pitch(0)
		, yaw(0)
		, valid(false)
	{}

	double roll;
	double pitch;
	double yaw;
	bool valid;
};

class tauto_destruct_executor
{
public:
	tauto_destruct_executor(const std::function<void ()>& did)
		: require_execute_(true)
		, did_(did)
	{}
	~tauto_destruct_executor()
	{
		if (require_execute_) {
			did_();
		}
	}
	void cancel_execute() { require_execute_ = false; }

private:
	bool require_execute_;
	std::function<void ()> did_;
};

#endif
