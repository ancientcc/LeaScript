/* $Id: string_utils.cpp 56274 2013-02-10 18:59:33Z boucman $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Copyright (C) 2005 - 2013 by Philippe Plantier <ayin@anathas.org>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file
 * Various string-routines.
 */
#define GETTEXT_DOMAIN "rose-lib"

#include "rose_string_utils_dll.hpp"
#include "rose_util.hpp"
#include "rose_exception.hpp"
#include "rose_config_3rdparty.hpp"
#include "gettext.hpp"
#include <algorithm>
#include <iomanip>
#include <SDL.h>

#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>

namespace utils {

bool isnewline(const char c)
{
	return c == '\r' || c == '\n';
}

// Make sure that we can use Mac, DOS, or Unix style text files on any system
// and they will work, by making sure the definition of whitespace is consistent
bool portable_isspace(const char c)
{
	// returns true only on ASCII spaces
	if (static_cast<unsigned char>(c) >= 128)
		return false;
	return isnewline(c) || isspace(c);
}

// Make sure we regard '\r' and '\n' as a space, since Mac, Unix, and DOS
// all consider these differently.
bool notspace(const char c)
{
	return !portable_isspace(c);
}

// if @str is empty(), return false.
bool has_portable_space_2end(const std::string& str)
{
	const char* c_str = str.c_str();
	int size = str.size();

	if (size == 0) {
		return false;
	}

	return portable_isspace(c_str[0]) || portable_isspace(c_str[size - 1]);
}

bool is_empty_or_all_portable_space(const std::string& str)
{
	int size = str.size();

	if (size == 0) {
		return true;
	}
	const char* c_str = str.c_str();

	for (int at = 0; at < size; at ++) {
		if (!portable_isspace(c_str[at])) {
			return false;
		}
	}
	return true;
}

std::string& strip(std::string &str)
{
	// If all the string contains is whitespace,
	// then the whitespace may have meaning, so don't strip it
	std::string::iterator it = std::find_if(str.begin(), str.end(), notspace);
	if (it == str.end()) {
		return str;
	}

	str.erase(str.begin(), it);
	str.erase(std::find_if(str.rbegin(), str.rend(), notspace).base(), str.end());

	return str;
}

std::string &strip_end(std::string &str)
{
	str.erase(std::find_if(str.rbegin(), str.rend(), notspace).base(), str.end());

	return str;
}

std::vector< std::string > split(std::string const &val, const char c, const int flags)
{
	std::vector< std::string > res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
			
	while (i2 != val.end()) {
		if (*i2 == c) {
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip_end(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty())
				res.push_back(new_val);
			++i2;
			if (flags & STRIP_SPACES) {
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}

			i1 = i2;
		} else {
			++i2;
		}
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip_end(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	return res;
}

bool isinteger(const std::string& str)
{
	const int len = (int)str.size();
	if (!len) {
		return false;
	}

	const char* c_str = str.c_str();
	char ch = c_str[0];
	int at = 0;
	if (ch == '-' || ch == '+') {
		if (len == 1) {
			return false;
		}
		at = 1;
	}
	for (; at < len; at ++) {
		if (c_str[at] < '0' || c_str[at] > '9') {
			return false;
		}
	}
	return true;
}

bool to_bool(const std::string& str, bool def, bool must_true)
{
	if (str.empty()) {
		return def;
	}

	//  for same as bool type in c/c++, priority of use 'true'. 
	if (str == "true") {
		return true;
	}

	return !must_true && str == "yes";
}

int64_t to_int64(const std::string& str, bool hex)
{
	if (str.empty()) {
		return 0;
	}
	const char* c_str = str.c_str();
	int size = str.size();

	int ret = 0;
	if (hex || (size >= 2 && c_str[0] == '0' && (c_str[1] == 'x' || c_str[1] == 'X'))) {
		// hex
		for (int at = hex? 0: 2; at < size; at ++) {
			const char ch = c_str[at];
			ret <<= 4;
			if (ch >= '0' && ch <= '9') {
				ret |= ch - '0';
			} else if (ch >= 'A' && ch <= 'F') {
				ret |= ch - 'A' + 10;
			} else if (ch >= 'a' && ch <= 'f') {
				ret |= ch - 'a' + 10;
			} else {
				return 0;
			}
		}
	} else {
		// decimal
		int at = 0, flag = 1;
		if (c_str[0] == '-') {
			at = 1;
			flag = -1;
		} else if (c_str[0] == '+') {
			at = 1;
		}
		for (; at < size; at ++) {
			const char ch = c_str[at];
			if (ch >= '0' && ch <= '9') {
				ret = ret * 10 + ch - '0';
			} else {
				return 0;
			}
		}
		ret *= flag;
	}

	return ret;
}

int to_int(const std::string& str, bool hex)
{
	return (int)to_int64(str, hex);
}

uint64_t to_uint64(const std::string& str, bool hex)
{
	if (str.empty()) {
		return 0;
	}
	const char* c_str = str.c_str();
	int size = str.size();

	uint64_t ret = 0;
	if (hex || (size >= 2 && c_str[0] == '0' && (c_str[1] == 'x' || c_str[1] == 'X'))) {
		// hex
		for (int at = hex? 0: 2; at < size; at ++) {
			const char ch = c_str[at];
			ret <<= 4;
			if (ch >= '0' && ch <= '9') {
				ret |= ch - '0';
			} else if (ch >= 'A' && ch <= 'F') {
				ret |= ch - 'A' + 10;
			} else if (ch >= 'a' && ch <= 'f') {
				ret |= ch - 'a' + 10;
			} else {
				return 0;
			}
		}
	} else {
		// decimal
		int at = 0, flag = 1;
		if (c_str[0] == '-') {
			return 0;
		} else if (c_str[0] == '+') {
			at = 1;
		}
		for (; at < size; at ++) {
			const char ch = c_str[at];
			if (ch >= '0' && ch <= '9') {
				ret = ret * 10 + ch - '0';
			} else {
				return 0;
			}
		}
		ret *= flag;
	}

	return ret;
}

uint32_t to_uint32(const std::string& str, bool hex)
{
	return (uint32_t)to_uint64(str, hex);
}

// if @str is empty, return @def.
double to_double(const std::string& str, double def, double invalid)
{
	if (str.empty()) {
		return def;
	}

	// Attempt to convert to a number.
	char* eptr;

	// must not use SDL_strtod(v.c_str(), &eptr)
	// -9223372036854775807 ==> -4294967295.0
	// double d = SDL_strtod(v.c_str(), &eptr);
	double d = strtod(str.c_str(), &eptr);
	if (*eptr == '\0') {
		// ok
		return d;
	}
	return invalid;
}

void test_from_double()
{
	std::vector<double> vec;
	vec.push_back(-9328.000019);
	vec.push_back(-9328.00001);
	vec.push_back(-9328.00002);
	vec.push_back(-9328.0001);
	vec.push_back(-9328.001);
	vec.push_back(-9328.01);
	vec.push_back(-9328.1);
	vec.push_back(-9328.00009);
	vec.push_back(-9328.999999);
	vec.push_back(0);
	vec.push_back(9328.000019);
	vec.push_back(9328.00001);
	vec.push_back(9328.00002);
	vec.push_back(9328.0001);
	vec.push_back(9328.001);
	vec.push_back(9328.01);
	vec.push_back(9328.1);
	vec.push_back(9328.00009);
	vec.push_back(9328.999999);

	vec.push_back(0.000019);
	vec.push_back(0.00001);
	vec.push_back(0.00002);
	vec.push_back(0.0001);
	vec.push_back(0.001);
	vec.push_back(0.01);
	vec.push_back(0.1);
	vec.push_back(0.00009);
	vec.push_back(0.0101);
	vec.push_back(0.999999);
	vec.push_back(-0.000019);
	vec.push_back(-0.00001);
	vec.push_back(-0.00002);
	vec.push_back(-0.0001);
	vec.push_back(-0.001);
	vec.push_back(-0.01);
	vec.push_back(-0.1);
	vec.push_back(-0.00009);
	vec.push_back(-0.0101);
	vec.push_back(-0.999999);

	for (std::vector<double>::const_iterator it = vec.begin(); it != vec.end(); ++ it) {
		double v = *it;
		std::string str = utils::from_double(v);
		SDL_Log("%.7f ==> %s", v, str.c_str());
	}
}

std::string from_double(double v)
{
	// max 5 decimal, but #5 maybe -1.
	// -9328.00002(memory:-9328.0000199) ==> -9328.00001
	char buf[40];
	char* ptr = buf;
	size_t maxlen = sizeof(buf);

	double integered = 0.0;
	int decimal = 0;
	if (v >= 0.0) {
		integered = SDL_floor(v);
		decimal = (v - integered) * 1000000;
	} else {
		integered = SDL_ceil(v);
		decimal = (v - integered) * -1000000;
		ptr[0] = '-';
		ptr ++;
		maxlen --;
		// if v is -0.01, intergered will be 0. but it is negative, and 0 hasn't -0.
		// so print (-1 + intergered)
		integered *= -1;
	}

	if ((decimal % 10 == 9) && decimal != 999999) {
		// Although the literal value is -9328.00002, the value stored in the memory may be -9328.000019.
		// 000019 + 1 ==> 00002;
		decimal ++;
	}
	decimal /= 10;

	if (decimal == 0) {
		SDL_snprintf(ptr, maxlen, "%.0f", integered);
		return buf;
	}
	// 99999
	if (decimal % 10) {
		SDL_snprintf(ptr, maxlen, "%.0f.%05d", integered, decimal);
		return buf;
	}
	decimal /= 10;
	// 9999
	if (decimal % 10) {
		SDL_snprintf(ptr, maxlen, "%.0f.%04d", integered, decimal);
		return buf;
	}
	decimal /= 10;
	// 999
	if (decimal % 10) {
		SDL_snprintf(ptr, maxlen, "%.0f.%03d", integered, decimal);
		return buf;
	}
	decimal /= 10;
	// 99
	if (decimal % 10) {
		SDL_snprintf(ptr, maxlen, "%.0f.%02d", integered, decimal);
		return buf;
	}
	decimal /= 10;
	// 9
	SDL_snprintf(ptr, maxlen, "%.0f.%d", integered, decimal);
	return buf;
}

bool bom_magic_started(const uint8_t* data, int size)
{
	return size >= BOM_LENGTH && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf;
}

std::string replace_all_char(const std::string& s, char oldchar, char newchar)
{
	if (s.empty() || oldchar == newchar) {
		return s; // if empty, return the given string.
	}

	int size = s.size();
	char* tmpstr = (char*)malloc(size + 1);
	memcpy(tmpstr, s.c_str(), size);
	tmpstr[size] = '\0';

	char* ptr = tmpstr;
	do {
		ptr = strchr(ptr, oldchar);
		if (ptr == nullptr) {
			break;
		}
		ptr[0] = newchar;
		ptr ++;
	} while (true);

	std::string result(tmpstr, size);
	free(tmpstr);
	return result;
}

bool is_short_app_dir(const std::string& str)
{
	const std::string prefix = "app-";
	const int should_connectors = 2;
	const int max_chars = 24;
	const int min_chars_one_segment = 2;

	int chars = 0;
	int connectors = 0;
	int size = str.size();
	if (size <= (int)prefix.size()) {
		return false;
	}
	const char* c_str = str.c_str();
	const char* ptr = strstr(c_str, prefix.c_str());
	if (ptr != c_str) {
		return false;
	}
	if (size - prefix.size() > max_chars) {
		return false;
	}
	for (int i = prefix.size(); i < size; i ++) {
		char ch = c_str[i];
		if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z')) {
/*
			if (connectors == 0 && (ch >= '0' && ch <= '9')) {
				// <a> must not digit
				return false;
			}
*/
			chars ++;

		} else if (ch == '_') {
			if (connectors == should_connectors) {
				return false;
			}
			if (chars < min_chars_one_segment) {
				return false;
			}
			connectors ++;
			chars = 0;
		} else {
			return false;
		}
	}
	return connectors <= should_connectors && chars >= min_chars_one_segment;
}

bool is_rose_bundleid(const std::string& str, const char separator)
{
	const int should_connectors = 2;
	const int max_chars = 24;
	const int min_chars_one_segment = 2;

	int chars = 0;
	int connectors = 0;
	int size = str.size();
	if (size > max_chars) {
		return false;
	}
	const char* c_str = str.c_str();
	for (int i = 0; i < size; i ++) {
		char ch = c_str[i];
		if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z')) {
			if (connectors == 0 && (ch >= '0' && ch <= '9')) {
				// <a> must not digit
				return false;
			}
			chars ++;

		} else if (ch == separator) {
			if (connectors == should_connectors) {
				return false;
			}
			if (chars < min_chars_one_segment) {
				return false;
			}
			connectors ++;
			chars = 0;
		} else {
			return false;
		}
	}
	return connectors == should_connectors && chars >= min_chars_one_segment;
}

bool is_bundleid2(const std::string& bundleid, const std::set<std::string>& exclude, char exclude_separator)
{
	if (!is_bundleid(bundleid)) {
		return false;
	}
	if (!exclude.empty()) {
		if (exclude_separator != '.') {
			const std::string label = utils::replace_all_char(bundleid, '.', exclude_separator);
			return exclude.count(label) == 0;
		} else {
			return exclude.count(bundleid) == 0;
		}
	}
	return true;
}

std::string join_app_prefix_id(const std::string& app, const std::string& id)
{
	// VALIDATE(!app.empty(), null_str);

	if (!app.empty()) {
		return app + "__" + id;
	}
	return id;
}

std::pair<std::string, std::string> split_app_prefix_id(const std::string& id2)
{
	size_t pos = id2.find("__");
	if (pos == std::string::npos) {
		return std::make_pair(null_str, id2);
	}
	return std::make_pair(id2.substr(0, pos), id2.substr(pos + 2));
}

// Convert a UCS-2 value to a UTF-8 string. Taken from SDL_UCS4ToUTF8()
std::string UCS2_to_UTF8(const wchar_t ch)
{
	uint8_t dst[4] = {0, 0, 0, 0};

    if (ch <= 0x7F) {
        dst[0] = (Uint8) ch;
    } else if (ch <= 0x7FF) {
        dst[0] = 0xC0 | (Uint8) ((ch >> 6) & 0x1F);
        dst[1] = 0x80 | (Uint8) (ch & 0x3F);
    } else {
        dst[0] = 0xE0 | (Uint8) ((ch >> 12) & 0x0F);
        dst[1] = 0x80 | (Uint8) ((ch >> 6) & 0x3F);
        dst[2] = 0x80 | (Uint8) (ch & 0x3F);
    }
	return (char*)dst;
}

// for utf8 continuation bytes must be rule: b7 must 1, b6 must 0.
// caller must define some variable:
// const uint8_t* data_ptr = (const uint8_t*)utf8str.c_str();
// int pos; when call, pos is offset utf8-char's first byte. after macro, pos will be offset first byte after uftf8-char.
// int continuation_pos;
#define check_utf8_continuation_byte(c_bytes, fail_res)	\
	for (++ pos, continuation_pos = 0; continuation_pos < c_bytes; continuation_pos ++, pos ++) { \
		if (((uint8_t)(data_ptr[pos]) & 0xc0) != 0x80) { \
			return fail_res; \
		} \
	}

size_t utf8str_len(const std::string& utf8str)
{
	size_t size = 0;
	const int len = utf8str.size();
	const uint8_t* data_ptr = (const uint8_t*)utf8str.c_str();
	int pos = 0;
	int continuation_pos;
	for (; pos < len; ) {
		uint8_t ch = data_ptr[pos];
		if ((ch & 0x80) == 0) {
			pos += 1;
		} else if ((ch & 0xE0) == 0xC0) {
			check_utf8_continuation_byte(1, 0);
			// pos += 2;
		} else if ((ch & 0xF0) == 0xE0) {
			check_utf8_continuation_byte(2, 0);
			// pos += 3;
		} else if ((ch & 0xF8) == 0xF0) {
			check_utf8_continuation_byte(3, 0);
			// pos += 4;
		} else if ((ch & 0xFC) == 0xF8) {
			check_utf8_continuation_byte(4, 0);
			// pos += 5;
		} else if ((ch & 0xFE) == 0xFC) {
			check_utf8_continuation_byte(5, 0);
			// pos += 6;
		} else {
			return 0;
		}
		size ++;
	}
	if (pos != len) {
		// utf8str isn't valid utf-8 format string
		size = 0;
	}

	return size;
}

SDL_Point utf8str_len2(const char* data, int len)
{
	// SDL_Point.x: valid chars, SDL_Point.y: valid bytes(must be <= @len).
	VALIDATE(len >= 0, null_str);

	size_t size = 0;
	const uint8_t* data_ptr = (const uint8_t*)data;
	int pos = 0;
	int continuation_pos;
	SDL_Point result{0, 0};
	for (; pos < len; ) {
		uint8_t ch = data_ptr[pos];
		if ((ch & 0x80) == 0) {
			pos += 1;
		} else if ((ch & 0xE0) == 0xC0) {
			check_utf8_continuation_byte(1, result);
			// pos += 2;
		} else if ((ch & 0xF0) == 0xE0) {
			check_utf8_continuation_byte(2, result);
			// pos += 3;
		} else if ((ch & 0xF8) == 0xF0) {
			check_utf8_continuation_byte(3, result);
			// pos += 4;
		} else if ((ch & 0xFC) == 0xF8) {
			check_utf8_continuation_byte(4, result);
			// pos += 5;
		} else if ((ch & 0xFE) == 0xFC) {
			check_utf8_continuation_byte(5, result);
			// pos += 6;
		} else {
			return result;
		}
		size ++;
		if (pos <= len) {
			result.x = size;
			result.y = pos;
		}
	}
	if (pos != len) {
		// utf8str isn't valid utf-8 format string
		// size = 0;
	}

	return result;
}

int utf8str_bytes(const std::string& utf8str, int chars)
{
	VALIDATE(chars >= 0, null_str);
	const int len = utf8str.size();
	if (chars >= len) {
		return len;
	}

	const uint8_t* c_str = (const uint8_t*)utf8str.c_str();
	int pos = 0, this_bytes = 0, chars2 = 0;
	for (; pos < len; ) {
		uint8_t ch = c_str[pos];
		if ((ch & 0x80) == 0) {
			this_bytes = 1;
		} else if ((ch & 0xE0) == 0xC0) {
			this_bytes = 2;
		} else if ((ch & 0xF0) == 0xE0) {
			this_bytes = 3;
		} else if ((ch & 0xF8) == 0xF0) {
			this_bytes = 4;
		} else if ((ch & 0xFC) == 0xF8) {
			this_bytes = 5;
		} else if ((ch & 0xFE) == 0xFC) {
			this_bytes = 6;
		} else {
			return 0;
		}
		pos += this_bytes;
		if (++ chars2 == chars) {
			break;
		}
	}

	return pos;
}

bool is_utf8str(const char* data, int64_t len)
{
	VALIDATE(len >= 0, null_str);
	const uint8_t* data_ptr = (const uint8_t*)data;
	int64_t pos = 0;
	int continuation_pos;
	for (; pos < len; ) {
		uint8_t ch = data_ptr[pos];
		if ((ch & 0x80) == 0) {
			pos += 1;
		} else if ((ch & 0xE0) == 0xC0) {
			check_utf8_continuation_byte(1, false);
			// pos += 2;
		} else if ((ch & 0xF0) == 0xE0) {
			check_utf8_continuation_byte(2, false);
			// pos += 3;
		} else if ((ch & 0xF8) == 0xF0) {
			check_utf8_continuation_byte(3, false);
			// pos += 4;
		} else if ((ch & 0xFC) == 0xF8) {
			check_utf8_continuation_byte(4, false);
			// pos += 5;
		} else if ((ch & 0xFE) == 0xFC) {
			check_utf8_continuation_byte(5, false);
			// pos += 6;
		} else {
			return false;
		}
	}

	return pos == len;
}

std::string truncate_to_max_bytes(const char* utf8str, const int utf8str_size, int max_bytes)
{
	VALIDATE((utf8str_size == nposm || utf8str_size >= 0) && max_bytes >= 1, null_str);

	const int len = utf8str_size == nposm? INT32_MAX: utf8str_size;
	if (len <= max_bytes) {
		return std::string(utf8str, len);
	}

	const uint8_t* c_str = (const uint8_t*)utf8str;
	int pos = 0, this_bytes = 0;
	for (; pos < len; ) {
		uint8_t ch = c_str[pos];
		if (ch == '\0') {
			break;
		} else if ((ch & 0x80) == 0) {
			this_bytes = 1;
		} else if ((ch & 0xE0) == 0xC0) {
			this_bytes = 2;
		} else if ((ch & 0xF0) == 0xE0) {
			this_bytes = 3;
		} else if ((ch & 0xF8) == 0xF0) {
			this_bytes = 4;
		} else if ((ch & 0xFC) == 0xF8) {
			this_bytes = 5;
		} else if ((ch & 0xFE) == 0xFC) {
			this_bytes = 6;
		} else {
			return std::string((const char*)c_str, pos);
		}
		if (pos + this_bytes > max_bytes) {
			break;
		}
		pos += this_bytes;
	}

	return std::string((const char*)c_str, pos);
}

// Truncate with ellipsis at end.
std::string truncate_to_max_bytes2(const std::string& str, const int max_bytes, bool ellipsis)
{
	VALIDATE(max_bytes > 0, null_str);

	bool use_new_str = false;
	int orig_size = str.size();
	std::string new_str;
	if (orig_size > max_bytes) {
		use_new_str = true;
		new_str = utils::truncate_to_max_bytes(str.c_str(), orig_size, max_bytes);

		if (ellipsis && new_str.size() != orig_size) {
			new_str += ellipsis_str;
		}
	}
	return use_new_str? new_str: str;
}

// Truncate with ellipsis at end.
std::string truncate_to_max_chars(const char* utf8str, const int utf8str_size, int max_chars)
{
	VALIDATE((utf8str_size == nposm || utf8str_size >= 0) && max_chars >= 1, null_str);

	const int len = utf8str_size == nposm? INT32_MAX: utf8str_size;
	if (len <= max_chars) {
		return std::string(utf8str, len);
	}

	const uint8_t* c_str = (const uint8_t*)utf8str;
	int pos = 0, this_bytes = 0, chars = 0;
	for (; pos < len; ) {
		uint8_t ch = c_str[pos];
		if (ch == '\0') {
			break;
		} else if ((ch & 0x80) == 0) {
			this_bytes = 1;
		} else if ((ch & 0xE0) == 0xC0) {
			this_bytes = 2;
		} else if ((ch & 0xF0) == 0xE0) {
			this_bytes = 3;
		} else if ((ch & 0xF8) == 0xF0) {
			this_bytes = 4;
		} else if ((ch & 0xFC) == 0xF8) {
			this_bytes = 5;
		} else if ((ch & 0xFE) == 0xFC) {
			this_bytes = 6;
		} else {
			return std::string((const char*)c_str, pos);
		}
		pos += this_bytes;
		if (++ chars == max_chars) {
			break;
		}
	}

	return std::string((const char*)c_str, pos);
}

// Truncate with ellipsis at end.
std::string truncate_to_max_chars2(const std::string& str, const int max_chars, bool ellipsis)
{
	VALIDATE(max_chars > 0, null_str);

	const int orig_size = str.size();
	std::string new_str = utils::truncate_to_max_chars(str.c_str(), orig_size, max_chars);

	if (ellipsis && new_str.size() != orig_size) {
		new_str += ellipsis_str;
	}

	return new_str;
}

// Truncate with ellipsis at start.
std::string truncate_to_max_chars2_at_start(const std::string& str, const int max_chars, const int max_discard_bytes)
{
	VALIDATE(max_chars > 0, null_str);

	const int orig_size = str.size();
	const int chars = utils::utf8str_len(str);
	if (chars <= max_chars) {
		return str;
	}

	const int discard_chars = chars - max_chars;
	int discard_bytes = utils::utf8str_bytes(str, discard_chars);
	if (max_discard_bytes != nposm && max_discard_bytes < discard_bytes) {
		SDL_Point valid = utils::utf8str_len2(str.c_str(), max_discard_bytes);
		discard_bytes = valid.y;
	}

	std::string new_str;
	if (discard_bytes != 0) {
		new_str = ellipsis_str;
	}
	new_str.append(str.substr(discard_bytes));

	return new_str;
}

static int byte_size_from_utf8_first(unsigned char ch)
{
	int count;

	if ((ch & 0x80) == 0)
		count = 1;
	else if ((ch & 0xE0) == 0xC0)
		count = 2;
	else if ((ch & 0xF0) == 0xE0)
		count = 3;
	else if ((ch & 0xF8) == 0xF0)
		count = 4;
	else if ((ch & 0xFC) == 0xF8)
		count = 5;
	else if ((ch & 0xFE) == 0xFC)
		count = 6;
	else
		throw invalid_utf8_exception(); // Stop on invalid characters

	return count;
}

utf8_iterator::utf8_iterator(const std::string& str) :
	current_char(0),
	string_end(str.end()),
	current_substr(std::make_pair(str.begin(), str.begin()))
{
	update();
}

utf8_iterator::utf8_iterator(std::string::const_iterator const &beg,
		std::string::const_iterator const &end) :
	current_char(0),
	string_end(end),
	current_substr(std::make_pair(beg, beg))
{
	update();
}

utf8_iterator utf8_iterator::begin(std::string const &str)
{
	return utf8_iterator(str.begin(), str.end());
}

utf8_iterator utf8_iterator::end(const std::string& str)
{
	return utf8_iterator(str.end(), str.end());
}

bool utf8_iterator::operator==(const utf8_iterator& a) const
{
	return current_substr.first == a.current_substr.first;
}

utf8_iterator& utf8_iterator::operator++()
{
	current_substr.first = current_substr.second;
	update();
	return *this;
}

wchar_t utf8_iterator::operator*() const
{
	return current_char;
}

bool utf8_iterator::next_is_end()
{
	if(current_substr.second == string_end)
		return true;
	return false;
}

const std::pair<std::string::const_iterator, std::string::const_iterator>& utf8_iterator::substr() const
{
	return current_substr;
}

void utf8_iterator::update()
{
	// Do not try to update the current unicode char at end-of-string.
	if (current_substr.first == string_end) {
		return;
	}

	size_t size = byte_size_from_utf8_first(*current_substr.first);
	current_substr.second = current_substr.first + size;

	current_char = static_cast<unsigned char>(*current_substr.first);
	// Convert the first character
	if (size != 1) {
		current_char &= 0xFF >> (size + 1);
	}

	// Convert the continuation bytes
	for (std::string::const_iterator c = current_substr.first+1; c != current_substr.second; ++c) {
		// If the string ends occurs within an UTF8-sequence, this is bad.
		if (c == string_end) {
			throw invalid_utf8_exception();
		}

		if ((*c & 0xC0) != 0x80) {
			throw invalid_utf8_exception();
		}

		current_char = (current_char << 6) | (static_cast<unsigned char>(*c) & 0x3F);
	}
}

//
// encrypt, decrypt
//
void aes256_encrypt(const uint8_t* key, const uint8_t* iv, const uint8_t* in, const int bytes, uint8_t* out)
{
	// In java, corresponding algorihm string: CIPHER_ALGORITHM = "AES/CBC/NoPadding"
	// if aes256, key must 32bytes. for aes256/192/128, all size are 16(AES_BLOCK_SIZE).
	VALIDATE(bytes > 0 && (bytes % AES_BLOCK_SIZE) == 0, null_str);

	AES_KEY enckey;
	AES_set_encrypt_key(key, 256, &enckey);

	// The IV used to encrypt block[n] is ciphertext(block[n-1]) unless n == 0, which is when the input IV is used. 
	// This copy makes sense if you think about calling this API in a loop. 
	// The output of the previous block is the IV for the next block. 
	uint8_t iv2[AES_BLOCK_SIZE];
    memcpy(iv2, iv, AES_BLOCK_SIZE);
	AES_cbc_encrypt(in, out, bytes, &enckey, iv2, AES_ENCRYPT);
}

void aes256_decrypt(const uint8_t* key, const uint8_t* iv, const uint8_t* in, const int bytes, uint8_t* out)
{
	// In java, corresponding algorihm string: CIPHER_ALGORITHM = "AES/CBC/NoPadding"
	// if aes256, key must 32bytes. for aes256/192/128, all size are 16(AES_BLOCK_SIZE).
	VALIDATE(bytes > 0 && (bytes % AES_BLOCK_SIZE) == 0, null_str);

	AES_KEY deckey;
	AES_set_decrypt_key(key, 256, &deckey);

    // memset(iv, 0x00, AES_BLOCK_SIZE);
	uint8_t iv2[AES_BLOCK_SIZE];
    memcpy(iv2, iv, AES_BLOCK_SIZE);
	AES_cbc_encrypt(in, out, bytes, &deckey, iv2, AES_DECRYPT);
}

std::unique_ptr<uint8_t[]> sha1(const uint8_t* in, const int len)
{
	VALIDATE(in && len >= 0, null_str);
	std::unique_ptr<uint8_t[]> md(new uint8_t[SHA_DIGEST_LENGTH]);
	uint8_t* mdptr = md.get();
    SHA1(in, len, mdptr);

	// std::string str = rtc::hex_encode((const char*)mdptr, SHA_DIGEST_LENGTH);

	return md;
}

std::unique_ptr<uint8_t[]> sha256(const uint8_t* in, const int len)
{
	// len maybe 0. but in must not nullptr.
	VALIDATE(in && len >= 0, null_str);
	std::unique_ptr<uint8_t[]> md(new uint8_t[SHA256_DIGEST_LENGTH]);
    SHA256(in, len, md.get());
	return md;
}

bool verify_sha1(const uint8_t* in, int len, const uint8_t* desire_md)
{
	VALIDATE(in != nullptr && len >= 0, null_str);

	uint8_t md[SHA_DIGEST_LENGTH];
	memset(md, 0, sizeof(md));

	SHA1(in, len, md);
	return memcmp(md, desire_md, SHA_DIGEST_LENGTH) == 0; 
}


int hmac_md_sha(const EVP_MD* md, const uint8_t* key, int key_len, const uint8_t* msg, int msg_len, uint8_t* result)
{
	// HMAC_CTX *ctx = HMAC_CTX_new();
	
	// bssl::ScopedHMAC_CTX include HMAC_CTX_new() and HMAC_CTX_free()
	bssl::ScopedHMAC_CTX ctx2;
	HMAC_CTX* ctx = ctx2.get();

    HMAC_Init_ex(ctx, key, key_len, md /*EVP_sha256()*/, NULL);
    HMAC_Update(ctx, msg, msg_len);

    unsigned int result_len;
    HMAC_Final(ctx, result, &result_len);
    // HMAC_CTX_free(ctx);

	// VALIDATE(result_len == SHA256_DIGEST_LENGTH, null_str);
	return result_len;
}

void resize_uint8data(tuint8data2_C& data, int size, int vsize)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0 && vsize >= 0, null_str);

	if (size > data.size) {
		uint8_t* tmp = (uint8_t*)malloc(size);
		if (data.ptr != nullptr) {
			if (vsize != 0) {
				memcpy(tmp, data.ptr, vsize);
			}
			free(data.ptr);
		}
		data.ptr = tmp;
		data.size = size;
	}
}

std::string extract_file(const std::string& file)
{
	static const std::string dir_separators = game_config::os == os_windows? "\\/:": "/";
	std::string::size_type pos = file.find_last_of(dir_separators);

	if (pos == std::string::npos) {
		return file;
	}
	if (pos >= file.size() - 1) {
		return "";
	}

	return file.substr(pos + 1);
}

std::string extract_directory(const std::string& file)
{
	static const std::string dir_separators = game_config::os == os_windows? "\\/:": "/";
	std::string::size_type pos = file.find_last_of(dir_separators);

	if (pos == std::string::npos) {
		return "";
	}

	return file.substr(0, pos);
}

std::string file_stem_name(const std::string& file)
{
	std::string::size_type pos = file.find_last_of(".");

	if (pos == std::string::npos) {
		return file;
	}

	return file.substr(0, pos);
}

std::string file_ext_name(const std::string& file)
{
	std::string::size_type pos = file.find_last_of(".");

	if (pos == std::string::npos) {
		return "";
	}
	if (pos >= file.size() - 1) {
		return "";
	}

	return file.substr(pos + 1);
}


std::string lowercase(const std::string& src)
{
	const int s = (int)src.size();
	if (!s) {
		return null_str;
	}
	const char* c_str = src.c_str();

	std::string dst;
	dst.resize(s);

	int diff = 'a' - 'A';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'A' && c_str[i] <= 'Z') {
			dst[i] = c_str[i] + diff;
		} else {
			dst[i] = c_str[i];
		}
	}
	return dst;
}

void lowercase2(std::string& str)
{
	const int s = (int)str.size();
	if (!s) {
		return;
	}
	const char* c_str = str.c_str();

	int diff = 'a' - 'A';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'A' && c_str[i] <= 'Z') {
			str[i] = c_str[i] + diff;
		}
	}
}

std::string uppercase(const std::string& src)
{
	const int s = (int)src.size();
	if (!s) {
		return null_str;
	}
	const char* c_str = src.c_str();

	std::string dst;
	dst.resize(s);

	int diff = 'A' - 'a';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'a' && c_str[i] <= 'z') {
			dst[i] = c_str[i] + diff;
		} else {
			dst[i] = c_str[i];
		}
	}
	return dst;
}

void uppercase2(std::string& str)
{
	const int s = (int)str.size();
	if (!s) {
		return;
	}
	const char* c_str = str.c_str();

	int diff = 'A' - 'a';
	for (int i = 0; i < s; i ++) {
		if (c_str[i] >= 'a' && c_str[i] <= 'z') {
			str[i] = c_str[i] + diff;
		}
	}
}

bool is_uuid(const std::string& str, bool line)
{
	// d1e09a45-9413-4ffc-986a-aaa41615faf6
	const int digits = 32;
	const int lines = 4;
	const int require_size = digits + (line? 4: 0);
	int size = str.size();
	if (size != require_size) {
		return false;
	}
	const char* c_str = str.c_str();
	char c_str2[digits + lines]; // 32 + 4
	memcpy(c_str2, c_str, size);
	if (line) {
		int pos[lines] = {8, 13, 18, 23};
		for (int i = 0; i < lines; i ++) {
			char* ptr = c_str2 + pos[i];
			if (ptr[0] != '-') {
				return false;
			}
			*ptr = '0';
		}
	}

	for (int i = 0; i < size; i ++) {
		const char ch = c_str2[i];
		if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
		} else {
			return false;
		}
	}
	return true;
}

bool is_uuid2(const std::string& str, bool line)
{
	if (!is_uuid(str, line)) {
		return false;
	}
	if (line) {
		return str != formatted_uuid_nposm;
	}
	return str != uuid_nposm;
}

std::string create_uuid(bool line)
{
	char* guid = SDL_CreateGUID(line? SDL_TRUE: SDL_FALSE);
	// all alpha are lowercase.
	std::string result = guid;
	SDL_free(guid);
	return result;
}

std::string format_uuid(const GUID& guid)
{
	// ==> 3641E31E-36BF-4E03-8879-DE33ADC07D68
	std::stringstream ss;

	ss << std::setw(8) << std::setfill('0') << std::setbase(16) << guid.Data1 << "-";
	ss << std::setw(4) << std::setfill('0') << std::setbase(16) << guid.Data2 << "-";
	ss << std::setw(4) << std::setfill('0') << std::setbase(16) << guid.Data3 << "-";
	ss << std::setw(2) << std::setfill('0') << std::setbase(16) << (int)guid.Data4[0];
	ss << std::setw(2) << std::setfill('0') << std::setbase(16) << (int)guid.Data4[1] << "-";
	for (int at = 2; at < 8; at ++) {
		ss << std::setw(2) << std::setfill('0') << std::setbase(16) << (int)guid.Data4[at];
	}

	return uppercase(ss.str());
}

std::string hex_encode_cstyle(const char* c_str, size_t srclen, char delimiter)
{
	if (srclen == 0) {
		return "";
	}

	static const char HEX[] = "0123456789abcdef";
	unsigned char* buffer = (unsigned char*)malloc(srclen * 3);

	size_t srcpos = 0, bufpos = 0;
	unsigned char val;
	while (srcpos < srclen) {
		unsigned char ch = c_str[srcpos++];
		val = (ch >> 4) & 0xF;
		buffer[bufpos  ] = (val < 16)? HEX[val] : '!';
		val = (ch     ) & 0xF;
		buffer[bufpos+1] = (val < 16)? HEX[val] : '!';
		bufpos += 2;

		// Don't write a delimiter after the last byte.
		if (delimiter && (srcpos < srclen)) {
			buffer[bufpos] = delimiter;
			++bufpos;
		}
	}

	// Null terminate.
	buffer[bufpos] = '\0';
	std::string ret((char*)buffer, bufpos);
	free(buffer);
	return ret;
}

// @dosstyle: windows dos style. false-->'/', true-->'\'
// get rid of all /.. and /. and //
std::string normalize_path(const std::string& src, bool dosstyle)
{
	// because of thread-safe, must not use static variable.
	char* data = NULL;
	int size = (int)src.size();

	const char separator = dosstyle? '\\': '/';
	const char require_modify_separator = dosstyle? '/': '\\';
	const char* c_str = src.c_str();
	bool has_dot = false;
	bool has_continue_separator = false;
	int sep = -2;

	// 1. replace all require_modify_separator to separator.
	for (int at = 0; at < size; at++) {
		const char ch = c_str[at];
		if (ch == separator) {
			if (data) {
				data[at] = ch;
			}
			if (!has_continue_separator && sep == at - 1) {
				has_continue_separator = true;
			}
			sep = at;
		} else if (ch == require_modify_separator) {
			if (!data) {
				data = (char*)malloc(posix_align_ceil(size + 1, 128));
				if (at) {
					memcpy(data, c_str, at);
				}
				data[size] = '\0';
			}
			if (!has_continue_separator && sep == at - 1) {
				has_continue_separator = true;
			}
			data[at] = separator;
			sep = at;
		} else {
			if (!has_dot && ch == '.' && (sep + 1 == at)) {
				// only consider '.' next to /.
				// filename maybe exist '.', ex: _main.cfg
				has_dot = true;
			}
			if (data) {
				data[at] = ch;
			}
		}
	}

	if (!has_dot && !has_continue_separator) {
		if (!data) {		
			return src;
		}
		std::string ret(data, size);
		free(data);
		return ret;
	}

	if (!data) {
		data = (char*)malloc(posix_align_ceil(size + 1, 128));
		memcpy(data, c_str, size);
		data[size] = '\0';
	}
	
	// 2. delete all /.. and /. and //
	while (1) {
		sep = -1;
		bool exit = true;
		for (int at = 0; at < size - 1; at++) {
			if (data[at] == separator) {
				if (data[at + 1] == '.') {
					if (data[at + 2] == '.') {
						// /..
						VALIDATE(sep != -1, null_str);
						if (size - at - 3) {
							SDL_memcpy(data + sep, data + at + 3, (size - at - 3));
						}
						size -= at - sep + 3;
						data[size] = '\0';
						exit = false;

					} else if (at + 2 >= size || data[at + 2] == '/') {
						// /.    ===>next is '/', or '.' is last char. delete these tow chars.
						if (size - at - 2) {
							SDL_memcpy(data + at, data + at + 2, (size - at - 2));
						}
						size -= 2;
						data[size] = '\0';
						exit = false;
					}
					
				} else if (data[at + 1] == '/') {
					// //    ===>delete second '/'
					if (size - at - 1) {
						SDL_memcpy(data + at, data + at + 1, (size - at - 1));
					}
					size -= 1;
					data[size] = '\0';
					exit = false;
				}

				if (!exit) {
					break;
				}
				sep = at;
			}
		}
		if (exit) {
			break;
		}
	}

	std::string ret(data, size);
	free(data);
	return ret;
}

std::string hex_encode_cstyle(const std::vector<uint8_t>& c_str, char delimiter)
{
	size_t srclen = c_str.size();
	if (srclen == 0) {
		return "";
	}

	static const char HEX[] = "0123456789abcdef";
	unsigned char* buffer = (unsigned char*)malloc(srclen * 3);

	size_t srcpos = 0, bufpos = 0;
	unsigned char val;
	while (srcpos < srclen) {
		unsigned char ch = c_str[srcpos++];
		val = (ch >> 4) & 0xF;
		buffer[bufpos  ] = (val < 16)? HEX[val] : '!';
		val = (ch     ) & 0xF;
		buffer[bufpos+1] = (val < 16)? HEX[val] : '!';
		bufpos += 2;

		// Don't write a delimiter after the last byte.
		if (delimiter && (srcpos < srclen)) {
			buffer[bufpos] = delimiter;
			++bufpos;
		}
	}

	// Null terminate.
	buffer[bufpos] = '\0';
	std::string ret((char*)buffer, bufpos);
	free(buffer);
	return ret;
}

bool localtime_clone(time_t t, struct tm& result)
{
	// localtime(t) returns a pointer, and this value can be changed at any time. 
	// If you need to use it later, you need to store it in an object.
	struct tm* ptr = localtime(&t);
	if (ptr == nullptr) {
		// if @t is too large, for error with ms unit, timeptr maybe nullptr.
		memset(&result, 0, sizeof(result));
		return false;
	}

	memcpy(&result, ptr, sizeof(struct tm));
	return true;
}

std::string posix_strftime(const std::string& format, const struct tm& timeptr)
{
	char ret[128] = {'\0'};

#ifdef _WIN32
	// on windows, should use wcsftime, not strftime.
	/*
	cbMultiByte [in]
	  Size, in bytes, of the string indicated by the lpMultiByteStr parameter. 
	  Alternatively, this parameter can be set to -1 if the string is null-terminated. 
	  Note that, if cbMultiByte is 0, the function fails.

	  If this parameter is -1, the function processes the entire input string, including the terminating null character. 
	  Therefore, the resulting Unicode string has a terminating null character, and the length returned by the function includes this character.

	  If this parameter is set to a positive integer, the function processes exactly the specified number of bytes. 
	  If the provided size does not include a terminating null character, the resulting Unicode string is not null-terminated, 
	  and the returned length does not include this character.
	*/
	int len = MultiByteToWideChar(CP_UTF8, 0, format.c_str(), -1, nullptr, 0);
	std::wstring wstr(len, 0);
	MultiByteToWideChar(CP_UTF8, 0, format.c_str(), -1, &wstr[0], len);

	wchar_t strDest[64];
	/*
	maxsize
	  Size of the strDest buffer, measured in characters (char or wchart_t).
	*/
	if (wcsftime(strDest, 64, wstr.c_str(), &timeptr) > 0) {
		WideCharToMultiByte(CP_UTF8, 0, strDest, -1, ret, sizeof(ret), NULL, NULL);
	}
#else
	strftime(ret, sizeof(ret), format.c_str(), &timeptr);
#endif

	return ret;
}

std::string format_time_ymd(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime(_("%b %d %y"), *tm_l);
	}

	return null_str;
}

std::string format_time_ymd2(time_t t, const char separator, bool align)
{
	tm* timeptr = localtime(&t);
	if (timeptr == NULL) {
		return null_str;
	}

	char buf[64];
	if (align) {
		if (separator != '\0') {
			SDL_snprintf(buf, sizeof(buf), "%04d%c%02d%c%02d", 1900 + timeptr->tm_year, separator, timeptr->tm_mon + 1, separator, timeptr->tm_mday);
		} else {
			SDL_snprintf(buf, sizeof(buf), "%04d%02d%02d", 1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday);
		}
	} else {
		if (separator != '\0') {
			SDL_snprintf(buf, sizeof(buf), "%d%c%d%c%d", 1900 + timeptr->tm_year, separator, timeptr->tm_mon + 1, separator, timeptr->tm_mday);
		} else {
			SDL_snprintf(buf, sizeof(buf), "%d%d%d", 1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday);
		}
	}
	return buf;
}

std::string format_time_ymd3(time_t t)
{
	// yyyy-mm-dd => 2018-12-06
	const struct tm* timeptr = localtime(&t);
	if (timeptr == nullptr) {
		// if t is too lareg, for error with ms unit, timeptr maybe nullptr.
		return null_str;
	}
	char buf[64];
	SDL_snprintf(buf, sizeof(buf), "%04d-%02d-%02d", 1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday);
	return buf;
}

std::string format_time_ymd4(time_t t, bool year, bool mon, bool day)
{
	const struct tm* timeptr = localtime(&t);
	if (timeptr == nullptr) {
		// if @t is too large, for error with ms unit, timeptr maybe nullptr.
		return null_str;
	}
	utils::string_map symbols;
	if (year) {
		symbols["year"] = str_cast(1900 + timeptr->tm_year);
	}
	if (mon) {
		symbols["mon"] = str_cast(timeptr->tm_mon + 1);
	}
	if (day) {
		symbols["day"] = str_cast(timeptr->tm_mday);
	}
	if (year && mon && day) {
		return vgettext2("$yearY$monM$dayD", symbols);
	}
	if (mon && day) {
		return vgettext2("$monM$dayD", symbols);
	}
	if (day) {
		return vgettext2("$dayD", symbols);
	}
	return "unsupport format";
}

std::string format_time_hms(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime(_("%H:%M:%S"), *tm_l);
	}

	return null_str;
}

std::string format_time_hm(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime(_("%H:%M"), *tm_l);
	}

	return null_str;
}

std::string format_time_date(time_t t)
{
	time_t curtime = time(NULL);
	const struct tm* timeptr = localtime(&curtime);
	if (timeptr == NULL) {
		return "";
	}

	const struct tm current_time = *timeptr;

	timeptr = localtime(&t);
	if(timeptr == NULL) {
		return "";
	}

	const struct tm save_time = *timeptr;

	const char* format_string = _("%b %d %y");

	if (current_time.tm_year == save_time.tm_year) {
		const int days_apart = current_time.tm_yday - save_time.tm_yday;

		if (days_apart == 0) {
			// save is from today
			format_string = _("%H:%M:%S");
		} else if (days_apart > 0 && days_apart == 1) {
			// save is from this week. On chinese, %A cannot display. use %m%d instead.
			format_string = _("YDAY %H:%M");
		} else if (days_apart > 0 && days_apart == 2) {
			// save is from this week. On chinese, %A cannot display. use %m%d instead.
			format_string = _("DBY %H:%M");
		} else if (days_apart > 0 && days_apart <= current_time.tm_wday) {
			// save is from this week. On chinese, %A cannot display. use %m%d instead.
			format_string = _("%A, %H:%M");
		} else {
			// save is from current year
			// format_string = _("%b %d");
			format_string = _("%A, %H:%M");
		}
	} else {
		// save is from a different year
		format_string = _("%b %d %y");
	}

	return posix_strftime(format_string, save_time);
}

std::string format_time_local(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime(_("%a %b %d %H:%M %Y"), *tm_l);
	}

	return null_str;
}

std::string format_time_ymdhms(time_t t)
{
	tm* tm_l = localtime(&t);
	if (tm_l) {
		return posix_strftime("%Y-%m-%d %H:%M:%S", *tm_l);
	}

	return null_str;
}

std::string format_time_ymdhms2(time_t t)
{
	// yyyymmddhhmmss => 20181206154225
	const struct tm* timeptr = localtime(&t);
	char buf[64];
	SDL_snprintf(buf, sizeof(buf), "%04d%02d%02d%02d%02d%02d", 1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday,
		timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);
	return buf;
}

std::string format_time_ymdhms3(time_t t)
{
	// yyyy-mm-dd hh:mm:ss => 2018-12-06 15:42:25
	const struct tm* timeptr = localtime(&t);
	if (timeptr == nullptr) {
		// if t is too lareg, for error with ms unit, timeptr maybe nullptr.
		return null_str;
	}
	char buf[64];
	SDL_snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", 1900 + timeptr->tm_year, timeptr->tm_mon + 1, timeptr->tm_mday,
		timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);
	return buf;
}

std::string format_time_dms(time_t t)
{
	// dd hh:mm => 06 15:42
	const struct tm* timeptr = localtime(&t);
	if (timeptr == nullptr) {
		// if t is too lareg, for error with ms unit, timeptr maybe nullptr.
		return null_str;
	}
	
	utils::string_map symbols;
	symbols["mday"] = str_cast(timeptr->tm_mday);
	symbols["hour"] = str_cast(timeptr->tm_hour);
	symbols["min"] = str_cast(timeptr->tm_min);

	return vgettext2("number $mday hour $hour min $min", symbols);
}

std::string format_second_24hoursys(int elapse)
{
	int one_day_seconds = 24 * 3600;
	elapse = elapse % one_day_seconds;

	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;

	char buf[24];
	SDL_snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, min, sec);
	return buf;
}

std::string format_elapse_hms(int elapse, int separator, bool m2)
{
	bool is_nagtive = false;
	if (elapse < 0) {
		is_nagtive = true;
		elapse *= -1;
	}

	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream ss;
	if (is_nagtive) {
		ss << "-";
	}

	if (day != 0) {
		if (separator == timesep_i18n) {
			utils::string_map symbols;
			symbols["d"] = str_cast(day);
			ss << vgettext2("$d days", symbols);

		} else {
			ss << day << "day";
		}
	}

	const bool sep_need_pad0 = separator == timesep_colon;
	bool need_pad0 = false;

	if (hour != 0) {
		need_pad0 = sep_need_pad0;
		ss << hour;
		if (separator == timesep_i18n) {
			ss << _("time^h");
		} else if (separator == timesep_colon) {
			ss << ":";
		} else {
			ss << "h";
		}
	}
	if (min != 0 || need_pad0) {
		if (!need_pad0) {
			need_pad0 = sep_need_pad0;
		}

		ss << min;
		if (separator == timesep_i18n) {
			if (m2 && sec == 0) {
				// fen zhong
				ss << _("time^m2");
			} else {
				// fen
				ss << _("time^m");
			}
		} else if (separator == timesep_colon) {
			ss << ":";
		} else {
			ss << "m";
		}
	}

	if (separator == timesep_colon && hour == 0 && min == 0) {
		// for example 32 second. 0:32, avoid 32
		ss << "0:";
	}

	if (sec != 0 || (day == 0 && hour == 0 && min == 0) || need_pad0) {
		ss << sec;
		if (separator == timesep_i18n) {
			ss << _("time^s");
		} else if (separator == timesep_colon) {
			// ss << ":";
		} else {
			ss << "s";
		}
	}
	
	return ss.str();
}

std::string format_elapse_hm_or_ms(int elapse, bool ceil_m, int separator, bool m2)
{
	// x<h>x<m> or x<m>x<s>
	elapse = elapse % ONE_DAY_SECONDS;
	if (elapse >= 3600) {
		if (ceil_m) {
			elapse = posix_align_ceil2(elapse, 60);
		} else {
			elapse = posix_align_floor(elapse, 60);
		}
	}

	return format_elapse_hms(elapse, separator, m2);
}

std::string format_elapse_hms2(int elapse, bool align)
{
	VALIDATE(!align, null_str);

	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream strstr;
	if (day) {
		strstr << day << "-";
	}
	strstr << std::setfill('0') << std::setw(2) << hour << ":";
	strstr << std::setfill('0') << std::setw(2) << min << ":";
	strstr << std::setfill('0') << std::setw(2) << sec;
	
	return strstr.str();
}

std::string format_elapse_hm(int elapse, bool align)
{
	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream ss;
	if (align) {
		ss << std::setfill('0') << std::setw(2);
	}
	ss << hour << _("time^h");
	ss << std::setfill('0') << std::setw(2) << min << _("time^m");
	
	return ss.str();
}

std::string format_elapse_hm2(int elapse, bool align)
{
	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream ss;
	if (align) {
		ss << std::setfill('0') << std::setw(2);
	}
	ss << hour << ":";
	ss << std::setfill('0') << std::setw(2) << min;
	
	return ss.str();
}

std::string format_elapse_ms2(int elapse, bool align)
{
	if (elapse < 0) {
		elapse = 0;
	}
	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream strstr;
/*
	if (day) {
		utils::string_map symbols;
		symbols["d"] = str_cast(day);
		strstr << vgettext2("$d days", symbols) << " ";
	}
*/
	if (hour) {
		strstr << std::setfill('0') << std::setw(2) << hour << ":";
	}
	strstr << std::setfill('0') << std::setw(2) << min << ":";
	strstr << std::setfill('0') << std::setw(2) << sec;
	
	return strstr.str();
}

std::string format_elapse_smsec(int elapse_ms, bool show_unit)
{
	if (elapse_ms < 0) {
		elapse_ms = 0;
	}
	int sec = elapse_ms / 1000;
	int msec = elapse_ms % 1000;

	std::stringstream ss;
	ss << sec << ".";
	ss << std::setfill('0') << std::setw(3) << msec;
	if (show_unit) {
		ss << _("time^s");
	}
	
	return ss.str();
}

// Quickly format a millisecond value from 0 to 999 into a three-digit string with trailing zeros removed.
// @ms   Input millisecond value (0-999)
// @buf  Output buffer (at least 4 bytes required)
// @len returned don't include '\n'.
int fast_ms_to_str(int ms, char* buf)
{
	VALIDATE(ms >= 0 && ms <= 999, null_str);
    if (ms == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    
    // Directly generate a three-digit string (including leading zeros).
    buf[0] = '0' + (ms / 100);
    buf[1] = '0' + ((ms / 10) % 10);
    buf[2] = '0' + (ms % 10);
    
    int idx = 3;
    // Remove trailing zeros.
    while (idx > 1 && buf[idx - 1] == '0') {
        idx --;
		buf[idx] = '\0';
    }
    
    buf[idx] = '\0';
    return idx;
}

std::string format_mselapse_hms(int mselapse, int separator, bool m2)
{
	VALIDATE(separator >= 0 && separator < timesep_count, null_str);

	bool is_nagtive = false;
	if (mselapse < 0) {
		is_nagtive = true;
		mselapse *= -1;
	}

	const int elapse = mselapse / 1000;
	int ms = mselapse % 1000;

	int sec = elapse % 60;
	int min = (elapse / 60) % 60;
	int hour = (elapse / 3600) % 24;
	int day = elapse / (3600 * 24);

	std::stringstream ss;
	if (is_nagtive) {
		ss << "-";
	}

	if (day != 0) {
		if (separator == timesep_i18n) {
			utils::string_map symbols;
			symbols["d"] = str_cast(day);
			ss << vgettext2("$d days", symbols);
		} else {
			ss << day << "day";
		}
	}

	const bool sep_need_pad0 = separator == timesep_colon;
	bool need_pad0 = false;

	if (hour != 0) {
		need_pad0 = sep_need_pad0;
		ss << hour;
		if (separator == timesep_i18n) {
			ss << _("time^h");
		} else if (separator == timesep_colon) {
			ss << ":";
		} else {
			ss << "h";
		}
	}
	if (min != 0 || need_pad0) {
		if (!need_pad0) {
			need_pad0 = sep_need_pad0;
		}
		ss << min;
		if (separator == timesep_i18n) {
			if (m2 && sec == 0 && ms == 0) {
				// fen zhong
				ss << _("time^m2");
			} else {
				// fen
				ss << _("time^m");
			}
		} else if (separator == timesep_colon) {
			ss << ":";
		} else {
			ss << "m";
		}
	}

	bool add_intl_s = false;
	if (ms == 0) {
		if (sec != 0 || (hour == 0 && min == 0) || need_pad0) {
			ss << sec;
			if (separator == timesep_i18n) {
				add_intl_s = true;
			} else if (separator == timesep_colon) {
				// ss << ":";
			} else {
				ss << "s";
			}
		}
	} else {
		char buf[32] = {'\0'};

		int msg_len = SDL_snprintf(buf, sizeof(buf), "%i.", sec);
		int len2 = fast_ms_to_str(ms, buf + msg_len);
		if (separator == timesep_i18n) {
			add_intl_s = true;
		} else if (separator == timesep_colon) {
			// ss << ":";
		} else {
			buf[msg_len + len2] = 's';
		}
		ss << buf;
	}
	if (add_intl_s) {
		ss << _("time^s");
	}

	return ss.str();
}

std::string format_mselapse_hm_or_ms_or_dotms(int mselapse, bool ceil_m, int separator, bool m2, int show_ms_below_sec)
{
	VALIDATE(separator >= 0 && separator < timesep_count, null_str);
	// x<h>x<m> or x<m>x<s> or x.xxx<s>
	int elapse = mselapse / 1000;
	if (elapse >= show_ms_below_sec) {
		return format_elapse_hm_or_ms(elapse, ceil_m, separator, m2);
	}

	return format_mselapse_hms(mselapse, separator, m2);
}

std::string weekday_name_form_tm_wday(int tm_wday, bool abbr)
{
	VALIDATE(tm_wday >= 0 && tm_wday < 7, null_str);
	if (tm_wday == 1) {
		return abbr? _("week^Mon"): _("Monday");
	}
	if (tm_wday == 2) {
		return abbr? _("week^Tues"): _("Tuesday");
	}
	if (tm_wday == 3) {
		return abbr? _("week^Wed"): _("Wednesday");
	}
	if (tm_wday == 4) {
		return abbr? _("week^Thurs"): _("Thursday");
	}
	if (tm_wday == 5) {
		return abbr? _("week^Fri"): _("Friday");
	}
	if (tm_wday == 6) {
		return abbr? _("week^Sat"): _("Saturday");
	}
	return abbr? _("week^Sun"): _("Sunday");
}

int get_tm_wday(time_t t)
{
	const struct tm* timeptr = localtime(&t);
	if (timeptr == nullptr) {
		// if @t is too large, for error with ms unit, timeptr maybe nullptr.
		return nposm;
	}
	return timeptr->tm_wday;
}

std::string get_weekday_name(time_t t, bool abbr)
{
	int tm_wday = get_tm_wday(t);
	if (tm_wday == nposm) {
		return "get weekday fail";
	}
	return weekday_name_form_tm_wday(tm_wday, abbr);
}

std::string format_i64size2(int64_t size)
{
	std::stringstream ss;

	const int K = 1 << 10;
	const int M = 1 << 20;
	const int64_t G = 1 << 30;

	int integer, decimal;
	std::string unit;
	if (size >= 10 * G) {
		// 4,98 GB
		size = size / G + ((size % G)? 1: 0);
		integer = size / 1000;
		decimal = size % 1000;
		unit = "GB";

	} else if (size >= 10 * M) {
		// 4,98 MB
		size = size / M + ((size % M)? 1: 0);
		integer = size / 1000;
		decimal = size % 1000;
		unit = "MB";

	} else if (size >= 10 * K) {
		// 4,98 KB
		size = size / K + ((size % K)? 1: 0);
		integer = size / 1000;
		decimal = size % 1000;
		unit = "KB";

	} else {
		// 4,98 B
		integer = size / 1000;
		decimal = size % 1000;
		unit = "B";
	}

	if (integer) {
		ss << integer << "," << std::setfill('0') << std::setw(3);
	}
	ss << decimal << " " << unit;
	return ss.str();
}

std::string format_i64size(int64_t size)
{
	std::stringstream ss;

	const int K = 1 << 10;
	const int M = 1 << 20;
	const int64_t G = 1 << 30;

	int integer, decimal;
	bool less_1k = false;
	std::string unit;
	if (size >= G) {
		// 49.08 GB
		size = (int64_t)(1.0 * size / G * 1000);
		integer = size / 1000;
		decimal = (size % 1000) / 10 + ((size % 10)? 1: 0);
		unit = "GB";

	} else if (size >= M) {
		// 49.08 MB
		size = size * 1000 / M;
		integer = size / 1000;
		decimal = (size % 1000) / 10 + ((size % 10)? 1: 0);
		unit = "MB";

	} else if (size >= K) {
		// 49.08 KB
		size = size * 1000 / K;
		integer = size / 1000;
		decimal = (size % 1000) / 10 + ((size % 10)? 1: 0);
		unit = "KB";

	} else {
		// 498 B
		integer = size / 1000;
		decimal = size % 1000;
		unit = "B";
		less_1k = true;
	}

	if (integer) {
		if (!less_1k) {
			ss << integer << "." << std::setfill('0') << std::setw(2);
		} else {
			ss << integer << "," << std::setfill('0') << std::setw(3);
		}
	}
	ss << decimal << " " << unit;
	return ss.str();
}

int days_in_month(int year, int month)
{
	VALIDATE(year > 0 && month >= 1 && month <= 12, null_str);
	if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) {
		return 31;
	} else if (month == 4 || month == 6 || month == 9 || month == 11) {
		return 30;
	} else if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
		return 29;
	}
	return 28;
}

int64_t interval_seconds_by_months(int months)
{
	// Using @months, calculate how many seconds are now away from that time
	if (months == 0) {
		return 0;
	}

	time_t curtime = time(nullptr);
	struct tm current_time = *localtime(&curtime);

	int abs_months = posix_abs(months);
	int year_increase = abs_months / 12;
	int month_increase = abs_months % 12;
	struct tm desire_time = current_time;
	// tm_mon: [0,11]
	if (months > 0) {
		desire_time.tm_mon += month_increase;
		desire_time.tm_year += year_increase;
		if (desire_time.tm_mon >= 12) {
			desire_time.tm_year ++;
			desire_time.tm_mon -= 12;
		}
	} else {
		desire_time.tm_mon -= month_increase;
		desire_time.tm_year -= year_increase;
		if (desire_time.tm_mon < 0) {
			desire_time.tm_year --;
			desire_time.tm_mon += 12;
		}
	}
	time_t desiretime = mktime(&desire_time);
	return desiretime - curtime;
}

int64_t datetime_str_2_ts(const std::string& datetime)
{
	if (datetime.empty()) {
		return 0;
	}

	struct tm tm1;
	memset(&tm1, 0, sizeof(tm1));
	bool align = false;

	if (align) {
		const std::string example = "2018-04-20 19:54:12";
		if (datetime.size() != example.size()) {
			return 0;
		}
		sscanf(datetime.c_str(), "%4d-%2d-%2d %2d:%2d:%2d", &tm1.tm_year, &tm1.tm_mon, &tm1.tm_mday, &tm1.tm_hour, &tm1.tm_min, &tm1.tm_sec);        

	} else {
		std::vector<std::string> vstr = utils::split(datetime, ' ');
		if (vstr.size() != 2) {
			return 0;
		}
		std::vector<std::string> vstr2 = utils::split(vstr[0], '-');
		if (vstr2.size() != 3) {
			return 0;
		}
		tm1.tm_year = utils::to_int(vstr2[0]);
		tm1.tm_mon = utils::to_int(vstr2[1]);
		tm1.tm_mday = utils::to_int(vstr2[2]);
		if (tm1.tm_year <= 1900 || tm1.tm_mon <= 0 || tm1.tm_mon > 12 || tm1.tm_mday <= 0) {
			return 0;
		}
		const int days = days_in_month(tm1.tm_year, tm1.tm_mon);
		if (tm1.tm_mday > days) {
			return 0;
		}

		vstr2 = utils::split(vstr[1], ':');
		if (vstr2.size() != 3) {
			return 0;
		}
		tm1.tm_hour = utils::to_int(vstr2[0]);
		tm1.tm_min = utils::to_int(vstr2[1]);
		tm1.tm_sec = utils::to_int(vstr2[2]);
		if (tm1.tm_hour < 0 || tm1.tm_hour >= 24 || tm1.tm_min < 0 || tm1.tm_min >= 60 || tm1.tm_sec < 0 || tm1.tm_sec >= 60) {
			return 0;
		}
	}
	tm1.tm_year -= 1900;
	tm1.tm_mon --;
	tm1.tm_isdst =-1;
  
	return mktime(&tm1);
}

int64_t date_str_2_ts(const std::string& date)
{
	struct tm tm1;
	memset(&tm1, 0, sizeof(tm1));
	bool align = false;

	if (align) {
		const std::string example = "2018-04-20";
		if (date.size() != example.size()) {
			return 0;
		}
		sscanf(date.c_str(), "%4d-%2d-%2d", &tm1.tm_year, &tm1.tm_mon, &tm1.tm_mday);        
	} else {
		std::vector<std::string> vstr2 = utils::split(date, '-');
		if (vstr2.size() != 3) {
			return 0;
		}
		tm1.tm_year = utils::to_int(vstr2[0]);
		tm1.tm_mon = utils::to_int(vstr2[1]);
		tm1.tm_mday = utils::to_int(vstr2[2]);
		if (tm1.tm_year <= 1900 || tm1.tm_mon <= 0 || tm1.tm_mon > 12 || tm1.tm_mday <= 0) {
			// why not return, see 'yyyymmddhhmmss_2_ts(...)'
			// return 0;
		}
		const int days = days_in_month(tm1.tm_year, tm1.tm_mon);
		if (tm1.tm_mday > days) {
			// return 0;
		}
	}
	tm1.tm_year -= 1900;
	tm1.tm_mon --;
	tm1.tm_isdst =-1;
  
	return mktime(&tm1);
}

int64_t yyyymmddhhmmss_2_ts(const std::string& str, bool* invalid_format)
{
	if (invalid_format != nullptr) {
		*invalid_format = true;
	}

	int size = str.size();
	if (size != 14) {
		return 0;
	}

	struct tm tm1;
	memset(&tm1, 0, sizeof(tm1));

	const char* c_str = str.c_str();
	int val = 0;
	enum {year, month, day, hour, min, second};
	int step = year;
	// 20241128140345
	for (int at = 0; at < size; at ++) {
		const char ch = c_str[at];
		if (ch < '0' || ch > '9') {
			return 0;
		}
		if (step == year) {
			val = val * 10 + ch - '0';
			if (at == 3) {
				tm1.tm_year = val;
				step = month;
				val = 0;
			}

		} else if (step == month) {
			val = val * 10 + ch - '0';
			if (at == 5) {
				tm1.tm_mon = val;
				step = day;
				val = 0;
			}
		} else if (step == day) {
			val = val * 10 + ch - '0';
			if (at == 7) {
				tm1.tm_mday = val;
				step = hour;
				val = 0;
			}
		} else if (step == hour) {
			val = val * 10 + ch - '0';
			if (at == 9) {
				tm1.tm_hour = val;
				step = min;
				val = 0;
			}
		} else if (step == min) {
			val = val * 10 + ch - '0';
			if (at == 11) {
				tm1.tm_min = val;
				step = second;
				val = 0;
			}
		} else if (step == second) {
			val = val * 10 + ch - '0';
			if (at == 13) {
				tm1.tm_sec = val;
				step = nposm;
				val = 0;
			}
		}

	}

	// Don't have to judge the validity of each field of tm1, 
	// it will be guaranteed to be valid by 'carrying'
	
	// 20241128146712 ==> 20241128150712   (146712 -> 150712)tm_min is invalid, but carrying '1' to tm_hour.
	// 20241128140075 ==> 20241128140115   (0075 -> 0115)tm_sec is invalid, but carrying '1' to tm_min.

	tm1.tm_year -= 1900;
	tm1.tm_mon --;
	tm1.tm_isdst =-1;

	if (invalid_format != nullptr) {
		*invalid_format = false;
	}

	return mktime(&tm1);
}

bool from_hh_mm_ss(const std::string& time_str, char separator, int& hour, int& minute, int& second)
{
	std::vector<std::string> vstr = utils::split(time_str, separator);
	int vstr_size = vstr.size();
	if (vstr_size != 2 && vstr_size != 3) {
		return false;
	}

	hour = nposm;
	minute = nposm;
	second = 0;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it, at ++) {
		const std::string& str = *it;
		if (at == 0) {
			hour = utils::to_int(str);

		} else if (at == 1) {
			minute = utils::to_int(str);

		} else if (at == 1) {
			second = utils::to_int(str);
		}
	}
	if (hour < 0 || hour >= 24 || minute < 0 || minute >= 60 || second < 0 || second >= 60) {
		return false;
	}
	return true;
}

// month: [1, 12], day[1, 31]
int64_t mktime2(int year, int month, int day, int hour, int minute, int second)
{
	VALIDATE(month >= 1 && month <= 12, null_str);
	VALIDATE(day >= 1 && day <= 31, null_str);

	struct tm tm1;
	memset(&tm1, 0, sizeof(tm1));

	tm1.tm_year = year - 1900;
	tm1.tm_mon = month - 1;
	tm1.tm_mday = day;

	tm1.tm_hour = hour;
	tm1.tm_min = minute;
	tm1.tm_sec = second;

	// Don't have to judge the validity of each field of tm1, 
	// it will be guaranteed to be valid by 'carrying'
	
	// tm1.tm_year -= 1900;
	// tm1.tm_mon --;
	tm1.tm_isdst = -1;

	return mktime(&tm1);
}

int calculate_hour24_time(int64_t ts)
{
	return (ts + game_config::equation_of_time) % ONE_DAY_SECONDS;
}

int64_t calculate_0h0m0s_ts(int64_t ts)
{
	return ts - calculate_hour24_time(ts);
}

// below to replace_all is copy from StringReplace(<protobuf>/src/google/protobuf/stubs/strutil.cc)
std::string replace_all(const std::string &s, const std::string& oldsub, const std::string& newsub)
{
	if (oldsub.empty()) {
		return s; // if empty, return the given string.
	}

	std::string ret;
	std::string::size_type start_pos = 0;
	std::string::size_type pos;
	do {
		pos = s.find(oldsub, start_pos);
		if (pos == std::string::npos) {
			break;
		}
		ret.append(s, start_pos, pos - start_pos);
		ret.append(newsub);
		start_pos = pos + oldsub.size();  // start searching again after the "old"
	} while (true);
	ret.append(s, start_pos, s.length() - start_pos);
	return ret;
}

void replace_all2(std::string& s, const std::string& oldsub, const std::string& newsub)
{
	while (true) {
		std::string::size_type pos(0);
		if ((pos = s.find(oldsub)) != std::string::npos) {
			s.replace(pos, oldsub.length(), newsub);
		} else {
			break;
		}
	}
}

std::string unique_untitle_or_uuid7_id(const std::set<std::string>& existed_ids, const std::string& _prefix, bool uuid7, const std::string& postfix, int start_number)
{
	VALIDATE(start_number >= 0, null_str);
	std::string prefix = _prefix;
	if (prefix.empty()) {
		if (uuid7) {
			const std::string uuid = utils::create_uuid(false);
			prefix = uuid.substr(0, 7);
		} else {
			prefix = "untitle";
		}
	}

	int number = start_number;
	std::string result;

	while (true) {
		result = prefix;
		if (!postfix.empty()) {
			result.append("_").append(postfix);
		}
		if (number != 0) {
			result.append(str_cast(number));
		}

		if (existed_ids.count(result) == 0) {
			break;
		}
		number ++;
	}

	return result;
}

std::string unique_untitle_name(const std::set<std::string>& existed_names, const std::string& _prefix, int start_number)
{
	VALIDATE(start_number >= 0, null_str);
	const std::string prefix = _prefix.empty()? _("Untitle"): _prefix;
	int number = start_number;

	std::string result;

	while (true) {
		result = prefix;
		if (number != 0) {
			result.append(str_cast(number));
		}

		if (existed_names.count(result) == 0) {
			break;
		}
		number ++;
	}

	return result;
}

const std::string& landmark_name(int at)
{
	// VALIDATE(at >= 0 && at < mediapipe::kNumPoseLandmarks, null_str);
	// VALIDATE(is_valid_lmk_all(at), null_str);
	static std::vector<std::string> names;
	if (names.empty()) {
		names.push_back(_("lmk^nose"));
		names.push_back(_("lmk^left_eye_inner"));
		names.push_back(_("lmk^left_eye"));
		names.push_back(_("lmk^left_eye_outer"));
        names.push_back(_("lmk^right_eye_inner"));
		names.push_back(_("lmk^right_eye"));
		names.push_back(_("lmk^right_eye_outer"));
		names.push_back(_("lmk^left_ear"));
		names.push_back(_("lmk^right_ear"));
		names.push_back(_("lmk^mouth_left"));
		names.push_back(_("lmk^mouth_right"));
		names.push_back(_("lmk^left_shoulder"));
		names.push_back(_("lmk^right_shoulder"));
		names.push_back(_("lmk^left_elbow"));
		names.push_back(_("lmk^right_elbow"));
        names.push_back(_("lmk^left_wrist"));
		names.push_back(_("lmk^right_wrist"));
		names.push_back(_("lmk^left_pinky"));
		names.push_back(_("lmk^right_pinky"));
		names.push_back(_("lmk^left_index"));
		names.push_back(_("lmk^right_index"));
		names.push_back(_("lmk^left_thumb"));
		names.push_back(_("lmk^right_thumb"));
        names.push_back(_("lmk^left_hip"));
		names.push_back(_("lmk^right_hip"));
		names.push_back(_("lmk^left_knee"));
		names.push_back(_("lmk^right_knee"));
		names.push_back(_("lmk^left_ankle"));
		names.push_back(_("lmk^right_ankle"));
		names.push_back(_("lmk^left_heel"));
		names.push_back(_("lmk^right_heel"));
		names.push_back(_("lmk^left_foot_index"));
		names.push_back(_("lmk^right_foot_index"));
		VALIDATE((int)names.size() == mediapipe::kNumPoseLandmarks, null_str);

		names.push_back(_("lmk^fake_center"));
		names.push_back(_("lmk^fake_tl"));
		names.push_back(_("lmk^fake_tr"));
		names.push_back(_("lmk^fake_br"));
		names.push_back(_("lmk^fake_bl"));
		VALIDATE((int)names.size() == mediapipe::kNumPoseLandmarks + fake_lmk_count, null_str);
	}
	if (is_mediapipe_lmk(at)) {
		return names.at(at);
	}

	VALIDATE(is_fake_lmk(at), null_str);
	return names.at(mediapipe::kNumPoseLandmarks + at - fake_lmk_min);
}

SDL_DPoint transform_xy(double x, double y, double theta, const SDL_DPoint& src)
{
	// reference to transformFootprint in <libros>/costmap_2d/src/footprint.cpp
	double cos_th = cos(theta);
	double sin_th = sin(theta);

	SDL_DPoint result;
	result.x = x + src.x * cos_th - src.y * sin_th;
	result.y = y + src.x * sin_th + src.y * cos_th;

	return result;
}

SDL_DPoint3 transform_xyz_2D(double x, double y, double theta, const SDL_DPoint3& src)
{
	// reference to transformFootprint in <libros>/costmap_2d/src/footprint.cpp
	double cos_th = cos(theta);
	double sin_th = sin(theta);

	SDL_DPoint3 result;
	result.x = x + src.x * cos_th - src.y * sin_th;
	result.y = y + src.x * sin_th + src.y * cos_th;
	result.z = src.z;

	return result;
}

double imgcoor_calculate_angle_2p(double start_x, double start_y, double end_x, double end_y)
{
	double deltax = end_x - start_x;
	double deltay = end_y - start_y;

	if (fabs(deltax) < 1e-10 && fabs(deltay) < 1e-10) {
		// if both x and y are 0, atan2's result is undefined.
		// The result depends on the specific library implementation. Like MSVC, it is likely to be 0.
		return 0;		
	}

	double theta = atan2(deltay, deltax);

	// atan2's return value: >0(anticlockwise), <0(clockwise). 
	// It is what mathematics teaches
	// but @start_x/start_y, @end_x/end_y are image-coor, it' Y is opsitive of mathematics-coor.
	// Multiplying by -1 corrects this, making the final angle follow math convention:
	// >0 means anticlockwise, <0 means clockwise.
	return -1 * theta;
}

double imgcoor_calculate_angle_3p_pi(double a_x, double a_y, double b_x, double b_y, double c_x, double c_y)
{
	const SDL_DPoint BA{a_x - b_x, a_y - b_y}; // ABC, BA
	const SDL_DPoint BC{c_x - b_x, c_y - b_y}; // ABC, BC

	// a.b = ||a|| ||b|| cos(theat) => theat = acos((a.b) / (||a|| ||b||))
	double dot_product = BA.x * BC.x + BA.y * BC.y;

	const double EPSILON = 1e-10; // 1e-6
	double mod_ba = sqrt(BA.x * BA.x + BA.y * BA.y);
	double mod_bc = sqrt(BC.x * BC.x + BC.y * BC.y);

	if (mod_ba < EPSILON || mod_bc < EPSILON) {
        // Error: Points are coincident(duplicate points) or colinear
		// ==> mode_product = 0 ==> cos_theta is maximum value ==> 1
		// acos(1) is 0.
		return 0;		
    }

	double mod_product = mod_ba * mod_bc;

	double cos_theta = dot_product / mod_product;
    cos_theta = fmax(fmin(cos_theta, 1.0), -1.0);

    double angle_rad = acos(cos_theta);
	return angle_rad;
}

// 返回 [0, 360) 的逆时针有向角度 (适配图像坐标系)
// 按照已有 angle2p 的约定，将 atan2 的结果乘以 -1，使正数对应画面中的逆时针旋转。
double imgcoor_calculate_angle_3p_2pi(double a_x, double a_y, double b_x, double b_y, double c_x, double c_y)
{
    const double EPSILON = 1e-10;

    // 向量 BA = A - B
    double ba_x = a_x - b_x;
    double ba_y = a_y - b_y;

    // 向量 BC = C - B
    double bc_x = c_x - b_x;
    double bc_y = c_y - b_y;

    // 防重合兜底
    if (fabs(ba_x) < EPSILON && fabs(ba_y) < EPSILON) {
		return 0.0;
	}
    if (fabs(bc_x) < EPSILON && fabs(bc_y) < EPSILON) {
		return 0.0;
	}

    // 计算向量 BA 和 BC 相对于正X轴的方位角
    // 因为图像坐标系Y向下，这里先取负号，让计算出的角度符合数学约定（正数代表画面逆时针）
    double ba_angle = -1.0 * atan2(ba_y, ba_x);
    double bc_angle = -1.0 * atan2(bc_y, bc_x);

    // 求差值：从 BA 逆时针旋转到 BC 的净旋转量
    // 结果范围在 (-2π, 2π) 之间
    double angle_rad = bc_angle - ba_angle;

    // 归一化到 [0, 2π)
    // 如果小于 0，加上 2π
    if (angle_rad < 0) {
        angle_rad += 2.0 * M_PI;
    }

    // 转换为角度 [0, 360)
    double angle_deg = angle_rad * (180.0 / M_PI);

    // 清除浮点误差导致的 360.0
    if (angle_deg >= 360.0) {
        angle_deg -= 360.0;
    }

    return DEG2RAD(angle_deg);
}

// condition id(only ASCII)/variable(only ASCII)/username

static bool is_underline_char(char c)
{
	return (c == '_');
}

static bool is_other_id_char_no_whitespace(char c)
{
	return ((c == '_') || (c == '-'));
}

static bool is_other_id_char_with_whitespace(char c)
{
	return ((c == '_') || (c == '-') || (c == ' '));
}

static bool is_variable_char(char c) 
{
	return ((c == '_') || (c == '-') || (c == '.'));
}

typedef bool (*is_xxx_char)(char c);

std::string isvalid_errstr;
bool isvalid_id_base(const std::string& id, bool first_must_alpha, is_xxx_char fn, int min, int max)
{
	utils::string_map symbols;
	int s = (int)id.size();
	if (s == 0) {
		if (min > 0) {
			isvalid_errstr = _("Can not empty!");
 			return false;
		}
		return true;
	}
	if (s < min) {
		symbols["min"] = str_cast(min);
		isvalid_errstr = vgettext2("At least $min characters!", symbols);
		return false;
	}
	if (s > max) {
		symbols["max"] = str_cast(max);
		isvalid_errstr = vgettext2("Can not be larger than $max characters!", symbols);
		return false;
	}
	const char* c_str = id.c_str();
	char c = c_str[0];
	if (c == ' ') {
		isvalid_errstr = _("First character can not empty!");
		return false;
	}
	if (first_must_alpha && !isalpha(c)) {
		isvalid_errstr = _("First character must be alpha!");
		return false;
	}
	if (id == "null") {
		symbols["str"] = id;
		isvalid_errstr = vgettext2("$str is reserved string!", symbols);
		return false;
	}

	const size_t alnum = std::count_if(id.begin(), id.end(), isalnum);
	const size_t valid_char = std::count_if(id.begin(), id.end(), fn);
	if ((alnum + valid_char != s) || valid_char == id.size()) {
		isvalid_errstr = _("Contains invalid characters!");
		return false;
	}
	return true;
}

bool isvalid_underline_id(const std::string& id, bool first_must_alpha, int min, int max)
{
	return isvalid_id_base(id, first_must_alpha, is_underline_char, min, max);
}

// widget's id, aplt task's id
bool isvalid_id(const std::string& id, bool first_must_alpha, int min, int max)
{
	return isvalid_id_base(id, first_must_alpha, is_underline_char, min, max);
	// return isvalid_id_base(id, first_must_alpha, is_other_id_char_no_whitespace, min, max);
}

bool isvalid_nick(const std::string& nick)
{ 
	const int max_nick_size = 31;
	return isvalid_id_base(nick, false, is_other_id_char_with_whitespace, 1, max_nick_size);
}

bool isvalid_utf8_name(const std::string& name, int min_chars, int max_chars)
{
	int name_chars = utils::utf8str_len(name);
	if (name_chars == 0 && !name.empty()) {
		// name isn't empty, but not a utf8 format.
		return false;
	}
	if (name_chars < min_chars || name_chars > max_chars) {
		return false;
	}
	return true;
}

}

namespace i18n {

std::string freq_msgstr(int msgid)
{
	if (msgid == msgid_notempty_and_utf8str) {
		return _("Value must not be empty, and utf-8 format string.");
	} else if (msgid == msgid_empty_or_utf8str) {
		return _("Value is empty, or utf-8 format string.");
	} else if (msgid == msgid_greater_than_0) {
		return _("Value must be greater than 0.");
	} else if (msgid == msgid_greater_than_equal_to_0) {
		return _("Value must be greater than or equal to 0.");
	} else if (msgid == msgid_empty_or_greater_than_0) {
		return _("Value is empty, or greater than 0.");
	} else if (msgid == msgid_isvalid_normal_id_or_var_name) {
		return _("Cannot be empty. Can only contain letters, numbers, and underscores, and must start with a letter.");
	} else if (msgid == msgid_open_file) {
		return _("Open File");
	} else if (msgid == msgid_save_file_as_from_no) {
		return _("from_no^Save File As");
	} else if (msgid == msgid_save_file_as) {
		return _("Save File As");
	}

	return null_str;
}

std::string freq_msgstr_2str(int msgid, const std::string& str1, const std::string& str2)
{
	utils::string_map symbols;
	std::string msgstr;
	if (msgid == msgid_confirm_delete_2str) {
		const std::string& type = str1;
		const std::string& name = str2;
		symbols["type"] = type;
		symbols["name"] = name;

		msgstr = vgettext2("Are you sure you want to delete $type '$name'?", symbols);
	}
	return msgstr;
}

std::string freq_msgstr_3str(int msgid, const std::string& str1, const std::string& str2, const std::string& str3)
{
	utils::string_map symbols;
	std::string msgstr;
	if (msgid == msgid_load_file_fail) {
		// str1: file_type, str2: filename, str3: reason
		const std::string& file_type = str1;
		const std::string filename = os_normalize_path(str2);
		const std::string& reason = str3;

		std::string full_filename = filename;
		std::string short_filename;

		if (SDL_IsFromRootPath(full_filename.c_str())) {
			short_filename = utils::extract_file(full_filename);

		} else {
			short_filename = full_filename;
			full_filename.clear();
		}

		if (!reason.empty()) {
			symbols["reason"] = reason;
		} else {
			symbols["reason"] = _("The file format is invalid or corrupted.");
		}

		if (full_filename.empty()) {
			symbols["type"] = str1;
			symbols["filename"] = filename;

			msgstr = vgettext2("load file fail. $type, $filename, $reason", symbols);

		} else {
			symbols["type"] = file_type;
			symbols["short_filename"] = short_filename;
			symbols["full_filename"] = full_filename;
			msgstr = vgettext2("load file fail. $type, $short_filename, $reason, $full_filename", symbols);
		}
	}
	return msgstr;
}

std::string freq_msgstr_1str_2int(int msgid, const std::string& str, int int1, int int2)
{
	utils::string_map symbols;
	std::string msgstr;
	if (msgid == msgid_value_range) {
		// str: value, int1: min, int2: max
		const std::string& val = str;
		SDL_Range r{int1, int2};
		
		symbols["min"] = str_cast(r.min);
		symbols["max"] = str_cast(r.max);
		symbols["val"] = val;
		msgstr = vgettext2("$val range is [$min, $max].", symbols);
	}
	return msgstr;
}

}