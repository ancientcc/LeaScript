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

#ifndef LIBROSE_UTIL_H_INCLUDED
#define LIBROSE_UTIL_H_INCLUDED

#include <SDL_rect.h>
#include <rose_util.hpp>

template<typename T>
inline bool is_even(T num) { return num % 2 == 0; }

template<typename T>
inline bool is_odd(T num) { return !is_even(num); }

/**
 * Returns base + increment, but will not increase base above max_sum, nor
 * decrease it below min_sum.
 * (If base is already beyond the applicable limit, base will be returned.)
 */
inline int bounded_add(int base, int increment, int max_sum, int min_sum=0) {
	if ( increment >= 0 )
		return std::min(base+increment, std::max(base, max_sum));
	else
		return std::max(base+increment, std::min(base, min_sum));
}

/** Guarantees portable results for division by 100; round towards 0 */
inline int div100rounded(int num) {
	return (num < 0) ? -(((-num) + 50) / 100) : (num + 50) / 100;
}

/**
 *  round (base_damage * bonus / divisor) to the closest integer,
 *  but up or down towards base_damage
 */
inline int round_damage(int base_damage, int bonus, int divisor) {
	if (base_damage==0) return 0;
	const int rounding = divisor / 2 - (bonus < divisor || divisor==1 ? 0 : 1);
	return std::max<int>(1, (base_damage * bonus + rounding) / divisor);
}

// not guaranteed to have exactly the same result on different platforms
inline int round_double(double d) {
#ifdef HAVE_ROUND
	return static_cast<int>(round(d)); //surprisingly, not implemented everywhere
#else
	return static_cast<int>((d >= 0.0)? std::floor(d + 0.5) : std::ceil(d - 0.5));
#endif
}

// Guaranteed to have portable results across different platforms
inline double round_portable(double d) {
	return (d >= 0.0) ? std::floor(d + 0.5) : std::ceil(d - 0.5);
}


template<typename Cmp>
bool in_ranges(const Cmp c, const std::vector<std::pair<Cmp, Cmp> >&ranges) {
	typename std::vector<std::pair<Cmp,Cmp> >::const_iterator range,
		range_end = ranges.end();
	for (range = ranges.begin(); range != range_end; ++range) {
		if(range->first <= c && c <= range->second) {
			return true;
		}
	}
	return false;
}

inline bool chars_equal_insensitive(char a, char b) { return tolower(a) == tolower(b); }
inline bool chars_less_insensitive(char a, char b) { return tolower(a) < tolower(b); }

/**
 * Returns the size, in bits, of an instance of type `T`, providing a
 * convenient and self-documenting name for the underlying expression:
 *
 *     sizeof(T) * std::numeric_limits<unsigned char>::digits
 *
 * @tparam T The return value is the size, in bits, of an instance of this
 * type.
 *
 * @returns the size, in bits, of an instance of type `T`.
 */
template<typename T>
inline std::size_t bit_width() {
	return sizeof(T) * std::numeric_limits<unsigned char>::digits;
}

/**
 * Returns the size, in bits, of `x`, providing a convenient and
 * self-documenting name for the underlying expression:
 *
 *     sizeof(x) * std::numeric_limits<unsigned char>::digits
 *
 * @tparam T The type of `x`.
 *
 * @param x The return value is the size, in bits, of this object.
 *
 * @returns the size, in bits, of an instance of type `T`.
 */
template<typename T>
inline std::size_t bit_width(const T& x) {
	//msvc 2010 gives an unused parameter warning otherwise
	(void)x;
	return sizeof(x) * std::numeric_limits<unsigned char>::digits;
}

/**
 * Returns the quantity of `1` bits in `n` population count.
 *
 * Algorithm adapted from:
 * <https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetKernighan>
 *
 * This algorithm was chosen for relative simplicity, not for speed.
 *
 * @tparam N The type of `n`. This should be a fundamental integer type no
 * greater than `UINT_MAX` bits in width; if it is not, the return value is
 * undefined.
 *
 * @param n An integer upon which to operate.
 *
 * @returns the quantity of `1` bits in `n`, if `N` is a fundamental integer
 * type.
 */
template<typename N>
inline unsigned int count_ones(N n) {
	unsigned int r = 0;
	while (n) {
		n &= n-1;
		++r;
	}
	return r;
}

// Support functions for `count_leading_zeros`.
#if defined(__GNUC__) || defined(__clang__)
inline unsigned int count_leading_zeros_impl(
		unsigned char n, std::size_t w) {
	// Returns the result of the compiler built-in function, adjusted for
	// the difference between the width, in bits, of the built-in
	// function parameter type (which is `unsigned int`, at the
	// smallest) and the width, in bits, of the input to this function, as
	// specified at the call-site in `count_leading_zeros`.
	return static_cast<unsigned int>(__builtin_clz(n))
		- static_cast<unsigned int>(
			bit_width<unsigned int>() - w);
}
inline unsigned int count_leading_zeros_impl(
		unsigned short int n, std::size_t w) {
	return static_cast<unsigned int>(__builtin_clz(n))
		- static_cast<unsigned int>(
			bit_width<unsigned int>() - w);
}
inline unsigned int count_leading_zeros_impl(
		unsigned int n, std::size_t w) {
	return static_cast<unsigned int>(__builtin_clz(n))
		- static_cast<unsigned int>(
			bit_width<unsigned int>() - w);
}
inline unsigned int count_leading_zeros_impl(
		unsigned long int n, std::size_t w) {
	return static_cast<unsigned int>(__builtin_clzl(n))
		- static_cast<unsigned int>(
			bit_width<unsigned long int>() - w);
}
inline unsigned int count_leading_zeros_impl(
		unsigned long long int n, std::size_t w) {
	return static_cast<unsigned int>(__builtin_clzll(n))
		- static_cast<unsigned int>(
			bit_width<unsigned long long int>() - w);
}
inline unsigned int count_leading_zeros_impl(
		char n, std::size_t w) {
	return count_leading_zeros_impl(
		static_cast<unsigned char>(n), w);
}
inline unsigned int count_leading_zeros_impl(
		signed char n, std::size_t w) {
	return count_leading_zeros_impl(
		static_cast<unsigned char>(n), w);
}
inline unsigned int count_leading_zeros_impl(
		signed short int n, std::size_t w) {
	return count_leading_zeros_impl(
		static_cast<unsigned short int>(n), w);
}
inline unsigned int count_leading_zeros_impl(
		signed int n, std::size_t w) {
	return count_leading_zeros_impl(
		static_cast<unsigned int>(n), w);
}
inline unsigned int count_leading_zeros_impl(
		signed long int n, std::size_t w) {
	return count_leading_zeros_impl(
		static_cast<unsigned long int>(n), w);
}
inline unsigned int count_leading_zeros_impl(
		signed long long int n, std::size_t w) {
	return count_leading_zeros_impl(
		static_cast<unsigned long long int>(n), w);
}
#else
template<typename N>
inline unsigned int count_leading_zeros_impl(N n, std::size_t w) {
	// Algorithm adapted from:
	// <http://aggregate.org/MAGIC/#Leading%20Zero%20Count>
	for (unsigned int shift = 1; shift < w; shift *= 2) {
		n |= (n >> shift);
	}
	return static_cast<unsigned int>(w) - count_ones(n);
}
#endif

/**
 * Returns the quantity of leading `0` bits in `n` i.e., the quantity of
 * bits in `n`, minus the 1-based bit index of the most significant `1` bit in
 * `n`, or minus 0 if `n` is 0.
 *
 * @tparam N The type of `n`. This should be a fundamental integer type that
 *  (a) is not wider than `unsigned long long int` (the GCC
 *   count-leading-zeros built-in functions are defined for `unsigned int`,
 *   `unsigned long int`, and `unsigned long long int`), and
 *  (b) is no greater than `INT_MAX` bits in width (the GCC built-in functions
 *   return instances of type `int`);
 * if `N` does not satisfy these constraints, the return value is undefined.
 *
 * @param n An integer upon which to operate.
 *
 * @returns the quantity of leading `0` bits in `n`, if `N` satisfies the
 * above constraints.
 *
 * @see count_leading_ones()
 */
template<typename N>
inline unsigned int count_leading_zeros(N n) {
#if defined(__GNUC__) || defined(__clang__)
	// GCC `__builtin_clz` returns an undefined value when called with 0
	// as argument.
	// [<http://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html>]
	if (n == 0) {
		// Return the quantity of zero bits in `n` rather than
		// returning that undefined value.
		return static_cast<unsigned int>(bit_width(n));
	}
#endif
	// Dispatch, on the static type of `n`, to one of the
	// `count_leading_zeros_impl` functions.
	return count_leading_zeros_impl(n, bit_width(n));
	// The second argument to `count_leading_zeros_impl` specifies the
	// width, in bits, of `n`.
	//
	// This is necessary because `n` may be widened (or, alas, shrunk),
	// and thus the information of `n` true width may be lost.
	//
	// At least, this *was* necessary before there were so many overloads
	// of `count_leading_zeros_impl`, but I've kept it anyway as an extra
	// precautionary measure, that will (I hope) be optimized out.
	//
	// To be clear, `n` would only be shrunk in cases noted above as
	// having an undefined result.
}

/**
 * Returns the quantity of leading `1` bits in `n` i.e., the quantity of
 * bits in `n`, minus the 1-based bit index of the most significant `0` bit in
 * `n`, or minus 0 if `n` contains no `0` bits.
 *
 * @tparam N The type of `n`. This should be a fundamental integer type that
 *  (a) is not wider than `unsigned long long int`, and
 *  (b) is no greater than `INT_MAX` bits in width;
 * if `N` does not satisfy these constraints, the return value is undefined.
 *
 * @param n An integer upon which to operate.
 *
 * @returns the quantity of leading `1` bits in `n`, if `N` satisfies the
 * above constraints.
 *
 * @see count_leading_zeros()
 */
template<typename N>
inline unsigned int count_leading_ones(N n) {
	// Explicitly specify the type for which to instantiate
	// `count_leading_zeros` in case `~n` is of a different type.
	return count_leading_zeros<N>(~n);
}

enum tristate {t_false, t_true, t_unset};

class tdisable_idle_lock
{
public:
	tdisable_idle_lock();
	~tdisable_idle_lock();
private:
	SDL_bool original_;
};

class thome_indicator_lock
{
public:
	thome_indicator_lock(int level);
	~thome_indicator_lock();

private:
	int original_;
};
/*
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
*/
#define font_cfg_reference_size	1000
#define font_max_cfg_size_diff	2
#define font_min_cfg_size	(font_cfg_reference_size - 2)

#define MAGIC_COORDINATE_X	-999999 // x,y of widget maybe < 0
#define is_magic_coordinate(coordinate)			((coordinate).x == MAGIC_COORDINATE_X)

#define set_null_coordinate(coordinate)	\
	(coordinate).x = MAGIC_COORDINATE_X;	\
	(coordinate).y = -1
#define construct_null_coordinate()	MAGIC_COORDINATE_X, -1
#define is_null_coordinate(coordinate)			((coordinate).x == MAGIC_COORDINATE_X && (coordinate).y == -1)

#define set_mouse_leave_window_event(coordinate)	\
	(coordinate).x = MAGIC_COORDINATE_X; \
	(coordinate).y = 0
#define is_mouse_leave_window_event(coordinate)	((coordinate).x == MAGIC_COORDINATE_X && (coordinate).y == 0)

#define set_focus_lost_event(coordinate)	\
	(coordinate).x = MAGIC_COORDINATE_X; \
	(coordinate).y = 1
#define is_focus_lost_event(coordinate)	((coordinate).x == MAGIC_COORDINATE_X && (coordinate).y == 1)

#define set_multi_gestures_up(coordinate)	\
	(coordinate).x = MAGIC_COORDINATE_X;	\
	(coordinate).y = 2
#define is_multi_gestures_up(coordinate)		((coordinate).x == MAGIC_COORDINATE_X && (coordinate).y == 2)

#define construct_up_result_leave_coordinate()	MAGIC_COORDINATE_X, 3
#define is_up_result_leave(coordinate)			((coordinate).x == MAGIC_COORDINATE_X && (coordinate).y == 3)

std::ostream &operator<<(std::ostream &stream, const tpoint& point);

extern const tpoint null_point;

struct trect
{
	trect(const int x, const int y, const int w, const int h)
		: x(x)
		, y(y)
		, w(w)
		, h(h)
		{}

	trect(const SDL_Rect& rect)
		: x(rect.x)
		, y(rect.y)
		, w(rect.w)
		, h(rect.h)
		{}

	int x;
	int y;
	int w;
	int h;

	trect& operator=(const SDL_Rect& rect) 
	{ 
		x = rect.x; 
		y = rect.y;
		w = rect.w;
		h = rect.h;
		return *this;
	}

	bool operator==(const trect& rect) const 
	{ 
		return x == rect.x && y == rect.y && w == rect.w && h == rect.h;
	}

	bool operator!=(const trect& rect) const 
	{ 
		return x != rect.x || y != rect.y || w != rect.w || h != rect.h;
	}

	bool operator<(const trect& rect) const 
	{ 
		if (x != rect.x) {
			return x < rect.x;
		}
		if (y != rect.y) {
			return y < rect.y;
		}
		if (w != rect.w) {
			return w < rect.w;
		}
		return h < rect.h;
	}

	bool point_in(const int _x, const int _y) const
	{
		return _x >= x && _y >= y && _x < x + w && _y < y + h;
	}

	bool rect_overlap(const trect& rect) const
	{
		return rect.x < x + w && x < rect.x + rect.w && rect.y < y + h && y < rect.y + rect.h;
	}

	bool rect_overlap2(const SDL_Rect& rect) const
	{
		return rect.x < x + w && x < rect.x + rect.w && rect.y < y + h && y < rect.y + rect.h;
	}

	SDL_Rect to_SDL_Rect() const { return SDL_Rect{x, y, w, h}; }
};

// from SDL_Rect to uint64_t, (x, y, w, h) range: [(SHRT_MIN)-32768, (SHRT_MAX)32767]
#define lua_pack_rect(x, y, w, h)	posix_mku64(posix_mku32(x, y), posix_mku32(w, h))
// from uint64_t to SDL_Rect
SDL_Rect lua_unpack_rect(uint64_t u64);
// from (x, y) to uint64_t
#define lua_pack_point(x, y)	posix_mku64(x, y)
// from uint64_t to SDL_Point
#define lua_unpack_point(u64)	SDL_Point{(int)posix_lo32(u64), (int)posix_hi32(u64)}

// lua --- rose
// ==> vdata:to_file
enum {PATHTYPE_ABS, PATHTYPE_RES, PATHTYPE_USERDATA};

// ==> rose.integer_tostr/is_format_string
enum {FMT_UUID, FMT_IPV4, FMT_TIME, FMT_TIME_HHcMMcSS, FMT_ELAPSE};

// ==> rose.change_string
enum {STRCT_EXTRACT_DIRECTORY};

// ==> rose.aes_encrypt/rose.aes_decrypt
enum {AES128, AES192, AES256};


bool uint16_lt(uint16_t s1, uint16_t s2);
uint16_t uint16_minus(uint16_t s1, uint16_t s2);

uint32_t get_local_ipaddr();

//
// server
//
enum {
	// value is bit flag
	server_httpd = 0x1,
	server_rtspd = 0x2,
	server_tcpd = 0x4,
	server_rdpd = 0x8,
	server_min_app = 0x10000,
};

class tserver_
{
public:
	tserver_(int flag)
		: flag_(flag)
	{}

	virtual void start(uint32_t ipaddr) = 0;
	virtual void stop() = 0;
	virtual const std::string& url() const = 0;

	bool started() const { return !url().empty(); }

protected:
	int flag_;
};

#endif
