/* $Id: global.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_GLOBAL_HPP_INCLUDED
#define LIBROSE_GLOBAL_HPP_INCLUDED

#ifdef _MSC_VER

// #undef snprintf
// #define snprintf _snprintf

// Disable warnig about source encoding not in current code page.
#pragma warning(disable: 4819)

// Disable warning about deprecated functions.
#pragma warning(disable: 4996)

//disable some MSVC warnings which are useless according to mordante
#pragma warning(disable: 4244)
#pragma warning(disable: 4345)
#pragma warning(disable: 4250)
#pragma warning(disable: 4251)
#pragma warning(disable: 4355)
#pragma warning(disable: 4800)
#pragma warning(disable: 4351)
#pragma warning(disable: 4200)

#endif
/*
#ifdef _WIN32
// for memory lead detect begin
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
// for memory lead detect end
#endif
*/
enum {os_windows, os_ios, os_android};

enum var_type_t {var_type_nposm = -1, var_type_bool, var_type_integer, var_type_double, var_type_string, 
	var_type_tstring, var_type_array, var_type_count};

// npos macro
#define nposm				-1
// some scenario, -1 is valid. they maybe use secondary_nposm
#define secondary_nposm		-12345

// 16777215, https://blog.csdn.net/albertsh/article/details/92385277
// think float value range is < 16777205.
#define float_nposm			(10.0 + 16777205)
#define is_float_nposm(xy)	((xy) >= 16777205)

#define uid_nposm			-1
#define is_valid_uid(uid)	((uid) > 0)

#define rose_u16_nan		0

#define MAXLEN_APP			31
#define MAXLEN_TEXTDOMAIN	63	// MAXLEN_TEXTDOMAIN must >= MAXLEN_APP + 4. default textdomain is <app>-lib

#define MIN_WIN_WIDTH		480
#define MIN_WIN_HEIGHT		320

#define ONE_DAY_HOURS		24
#define ONE_DAY_SECONDS		86400 // 24 * 3600

#define CONSTANT_1G			1073741824
#define CONSTANT_64M		67108864
#define CONSTANT_1M			1048576
#define CONSTANT_1K			1024

void SDL_SimplerMB(const char* fmt, ...);

// 1. mask must be power's 2, and not 0.
// 2. if val is 0, it will return 0. even if mask is any value.
// 3. if mask is power's 2, use posix_align_ceil, or use posix_align_ceil2.
// 4. if mask isn't power's 2, must use posix_align_ceil2
#define posix_align_ceil(val, mask)     (((val) + (mask) - 1) & ~((mask) - 1))

#define posix_align_floor(dividend, divisor)	((dividend) - ((dividend) % (divisor)))
int posix_pages(int dividend, int divisor);

#define size_t_2_int(val)		(int)(long)(val)
#define uint64_2_int(val)		(int)(long)(val)

#define BIT_IDX_MASK(bit)	(UINT32_C(1) << (bit))			// 4 --> 0x10000
#define BIT_MASK(nbit)		((UINT32_C(1) << (nbit)) - 1)	// 4--> 0x1111

inline int get_max_bit(unsigned int mask)
{
    if (mask == 0) return -1;
    int pos = 0;
    while (mask >>= 1) pos++;
    return pos;
}

//
// KDL_Equal is same as KDL::Equal{in <libros>/include/kdl/utilities/utility.h}
//
#define KDL_KDL_epsilon	1e-6

// compares whether 2 doubles are equal in an eps-interval.
// Does not check whether a or b represents numbers
// On VC6, if a/b is -INF, it returns false;
inline bool KDL_Equal(double a,double b,double eps=KDL_KDL_epsilon/*epsilon*/)
{
    double tmp=(a-b);
    return ((eps>tmp)&& (tmp>-eps) );
}

#ifndef DEG2RAD
#define DEG2RAD(x) ((x)*0.01745329252f) // ((x)*M_PI / 180.)
#endif

#ifndef RAD2DEG
#define RAD2DEG(x) ((x)*57.29579513f) // ((x)*180. / M_PI)
#endif

#ifndef container_of
#ifdef _WIN32
#define container_of(ptr, type, field) CONTAINING_RECORD(ptr, type, field)
#else
// android
#define container_of(ptr, type, field) \
   (type*)((char*)ptr - offsetof(type, field))
#endif
#endif

// must not: 1)tclazz clazz2(that);
//           2)clazz2 = tclazz(...);
#define posix_noncopyable(tclazz) \
	tclazz (const tclazz & that) = delete; \
	const tclazz & operator=(const tclazz & that) = delete

#define MEDIAPIPE_POSTURE_MIN_INTERVAL	150 // 150ms
namespace mediapipe {
inline constexpr int kNumPoseLandmarks = 33;
}
#define is_mediapipe_lmk(at) ((at) >= 0 && (at) < mediapipe::kNumPoseLandmarks)

//
// fake landmark. fake_lmk_min: starting index of pseudo-landmark points.
// 
enum {fake_lmk_min = 1000, fake_lmk_center = fake_lmk_min, fake_lmk_tl, fake_lmk_tr,
	fake_lmk_br, fake_lmk_bl, fake_lmk_max = fake_lmk_bl, fake_lmk_count = fake_lmk_max - fake_lmk_min + 1};

#define is_fake_lmk_range(at) ((at) >= fake_lmk_min)
#define is_fake_lmk(at) ((at) >= fake_lmk_min && (at) <= fake_lmk_max)

#define is_valid_lmk_all(at) (is_mediapipe_lmk(at) || is_fake_lmk(at))

// assistant macro of byte,word,dword,qword opration. windef.h has this definition.
// for make macro, first parameter is low part, second parameter is high part.
#ifndef posix_mku64
	#define posix_mku64(l, h)		((uint64_t)(((uint32_t)(l)) | ((uint64_t)((uint32_t)(h))) << 32))    
#endif

#ifndef posix_mku32
	#define posix_mku32(l, h)		((uint32_t)(((uint16_t)(l)) | ((uint32_t)((uint16_t)(h))) << 16))
#endif

#ifndef posix_mku16
	#define posix_mku16(l, h)		((uint16_t)(((uint8_t)(l)) | ((uint16_t)((uint8_t)(h))) << 8))
#endif

#ifndef posix_mku8
	#define posix_mku8(l, h)		(((uint8_t)(l) & 0xf) | (((uint8_t)(h) & 0xf) << 4))
#endif

#ifndef posix_mki64
	#define posix_mki64(l, h)		((int64_t)(((uint32_t)(l)) | ((int64_t)((uint32_t)(h))) << 32))    
#endif

#ifndef posix_mki32
	#define posix_mki32(l, h)		((int32_t)(((uint16_t)(l)) | ((int32_t)((uint16_t)(h))) << 16))
#endif

#ifndef posix_mki16
	#define posix_mki16(l, h)		((int16_t)(((uint8_t)(l)) | ((int16_t)((uint8_t)(h))) << 8))
#endif

#ifndef posix_lo32
	#define posix_lo32(v64)			((uint32_t)(v64))
#endif

#ifndef posix_hi32
	#define posix_hi32(v64)			((uint32_t)(((uint64_t)(v64) >> 32) & 0xFFFFFFFF))
#endif

#ifndef posix_lo16
	#define posix_lo16(v32)			((uint16_t)(v32))
#endif

#ifndef posix_hi16
	#define posix_hi16(v32)			((uint16_t)(((uint32_t)(v32) >> 16) & 0xFFFF))
#endif

#ifndef posix_lo8
	#define posix_lo8(v16)			((uint8_t)(v16))
#endif

#ifndef posix_hi8
	#define posix_hi8(v16)			((uint8_t)(((uint16_t)(v16) >> 8) & 0xFF))
#endif

#ifndef posix_clip
#define	posix_clip(x, min, max)	  SDL_max(SDL_min((x), (max)), (min))
#endif

#ifndef posix_abs
#define posix_abs(a)            (((a) >= 0)? (a) : (-(a)))
#endif


typedef struct SDL_2Point
{
    int x1;
    int y1;
	int x2;
	int y2;
} SDL_2Point;

typedef struct SDL_Point3
{
    int x;
    int y;
	int z;
} SDL_Point3;

typedef struct SDL_FPoint3
{
    float x;
    float y;
	float z;
} SDL_FPoint3;

typedef struct SDL_U16Point
{
    unsigned short x;
    unsigned short y;
} SDL_U16Point;

typedef struct SDL_DPoint
{
    double x;
    double y;
} SDL_DPoint;

typedef struct SDL_DPoint3
{
    double x;
    double y;
	double z;
} SDL_DPoint3;

typedef struct SDL_Size
{
    int w;
	int h;
} SDL_Size;

typedef struct SDL_FSize
{
    float w;
	float h;
} SDL_FSize;

typedef struct SDL_DSize
{
    double w;
	double h;
} SDL_DSize;

typedef struct SDL_Size3
{
    int l; // length
    int w; // width
	int h; // height
} SDL_Size3;

typedef struct SDL_DSize3
{
    double l; // length
    double w; // width
	double h; // height
} SDL_DSize3;

typedef struct SDL_Range
{
    int min;
    int max;
} SDL_Range;

typedef struct SDL_FRange
{
    float min;
    float max;
} SDL_FRange;

typedef struct SDL_DRange
{
    double min;
    double max;
} SDL_DRange;

typedef struct SDL_DColor
{
    double r;
    double g;
    double b;
    double a;
} SDL_DColor;

struct tchardata_C {
	char* ptr;
	int len;
};

struct tcharcdata_C {
	const char* ptr;
	int len;
};

struct tuint8data_C {
	unsigned char* ptr;
	int len;
};

struct tuint8cdata_C {
	const unsigned char* ptr;
	int len;
};

struct tint32data_C {
	int* ptr;
	int len;
};

struct tvoiddata_C {
	void* ptr;
	int len;
};

struct tvoidcdata_C {
	const void* ptr;
	int len;
};

struct tchardata2_C {
	char* ptr;
	int vsize;
	int size;
};

struct tuint8data2_C {
	unsigned char* ptr;
	int vsize;
	int size;
};

//
// workout
//
#define WKO_MAX_POSES_PER_TRACK	24
struct twko_operand
{
	int lmk0;
	int lmk1;
};

#endif // LIBROSE_GLOBAL_HPP_INCLUDED
