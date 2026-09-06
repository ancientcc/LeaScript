/* $Id: string_utils.hpp 56274 2013-02-10 18:59:33Z boucman $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2013 by Guillaume Melquiond <guillaume.melquiond@gmail.com>


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE2_STRING_UTILS_DLL_HPP_INCLUDED
#define LIBROSE2_STRING_UTILS_DLL_HPP_INCLUDED

#include "rose_global.hpp"
#include "3rdparty_decl.h"

#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <iterator>
// #include <boost/next_prior.hpp>
#include <openssl/digest.h>
#include <SDL_rect.h>

#ifndef _WIN32
// <freerdp>/out/winpr/include/winpr/wtypes.h define GUID.
#ifndef POSIX_GUID
#define POSIX_GUID
typedef struct {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;
#endif
void CoCreateGuid(GUID* pguid);
#else
#include <combaseapi.h>
#endif

namespace utils {

/**
 * Generates a new string joining container items in a list.
 *
 * @param v A container with elements.
 * @param s List delimiter.
 */
template <typename T>
std::string join(T const &v, const std::string& s = ",")
{
        std::stringstream str;
        for(typename T::const_iterator i = v.begin(); i != v.end(); ++i) {
                str << *i;
                if (std::next(i) != v.end())
                        str << s;
        }

        return str.str();
}

LIB3RDPARTY_DECL bool isnewline(const char c);
LIB3RDPARTY_DECL bool portable_isspace(const char c);
LIB3RDPARTY_DECL bool notspace(const char c);
LIB3RDPARTY_DECL bool has_portable_space_2end(const std::string& str);
LIB3RDPARTY_DECL bool is_empty_or_all_portable_space(const std::string& str);

// Remove whitespace from the front and back of the string 'str'.
LIB3RDPARTY_DECL std::string& strip(std::string& str);
// Remove whitespace from the back of the string 'str'.
LIB3RDPARTY_DECL std::string& strip_end(std::string& str);

enum { REMOVE_EMPTY = 0x01,	// REMOVE_EMPTY : remove empty elements.
	  STRIP_SPACES  = 0x02	// STRIP_SPACES : strips leading and trailing blank spaces.
};

LIB3RDPARTY_DECL std::vector<std::string> split(std::string const &val, const char c = ',', const int flags = REMOVE_EMPTY | STRIP_SPACES);

LIB3RDPARTY_DECL bool isinteger(const std::string& str);

LIB3RDPARTY_DECL bool to_bool(const std::string& str, bool def = false, bool must_true = false);
LIB3RDPARTY_DECL int64_t to_int64(const std::string& str, bool hex = false);
LIB3RDPARTY_DECL int to_int(const std::string& str, bool hex = false);
LIB3RDPARTY_DECL uint64_t to_uint64(const std::string& str, bool hex = false);
LIB3RDPARTY_DECL uint32_t to_uint32(const std::string& str, bool hex = false);

LIB3RDPARTY_DECL double to_double(const std::string& str, double def = 0.0, double invalid = float_nposm);
LIB3RDPARTY_DECL std::string from_double(double v);

#define BOM_LENGTH		3
LIB3RDPARTY_DECL bool bom_magic_started(const uint8_t* data, int size);

LIB3RDPARTY_DECL std::string replace_all_char(const std::string& s, char oldchar, char newchar);
#define bundleid_2_lua_bundleid(s)	utils::replace_all_char(s, '.', '_')
#define lua_bundleid_2_bundleid(s)	utils::replace_all_char(s, '_', '.')

LIB3RDPARTY_DECL bool is_short_app_dir(const std::string& str);
LIB3RDPARTY_DECL bool is_rose_bundleid(const std::string& str, const char separator);
LIB3RDPARTY_DECL bool is_bundleid2(const std::string& bundleid, const std::set<std::string>& exclude, char exclude_separator);
#define is_bundleid(str)		utils::is_rose_bundleid(str, '.')
#define is_lua_bundleid(str)	utils::is_rose_bundleid(str, '_')

LIB3RDPARTY_DECL std::string join_app_prefix_id(const std::string& app, const std::string& id);
LIB3RDPARTY_DECL std::pair<std::string, std::string> split_app_prefix_id(const std::string& id2);

//
// utf8
//
#define ASCII_2_UCS2(ch)		(0x0000 + (ch))

// Convert a UCS-2 value to a UTF-8 string. Taken from SDL_UCS4ToUTF8()
LIB3RDPARTY_DECL std::string UCS2_to_UTF8(const wchar_t ch);

LIB3RDPARTY_DECL size_t utf8str_len(const std::string& utf8str);
LIB3RDPARTY_DECL SDL_Point utf8str_len2(const char* data, int len);
LIB3RDPARTY_DECL int utf8str_bytes(const std::string& utf8str, int chars);

// whether [data, len] is utf8 encoding or not
LIB3RDPARTY_DECL bool is_utf8str(const char* data, int64_t len);
// @param[in] utf8str_size: if cannot sure size, use nposm.
LIB3RDPARTY_DECL std::string truncate_to_max_bytes(const char* utf8str, const int utf8str_size, int max_bytes);
LIB3RDPARTY_DECL std::string truncate_to_max_bytes2(const std::string& str, const int max_bytes, bool ellipsis);

// @param[in] utf8str_size: if cannot sure size, use nposm.
LIB3RDPARTY_DECL std::string truncate_to_max_chars(const char* utf8str, const int utf8str_size, int max_chars);
LIB3RDPARTY_DECL std::string truncate_to_max_chars2(const std::string& str, const int max_chars, bool ellipsis);
LIB3RDPARTY_DECL std::string truncate_to_max_chars2_at_start(const std::string& str, const int max_chars, const int max_discard_bytes = nposm);

class LIB3RDPARTY_DECL invalid_utf8_exception : public std::exception
{};

class LIB3RDPARTY_DECL utf8_iterator
{
public:
	typedef std::input_iterator_tag iterator_category;
	typedef wchar_t value_type;
	typedef ptrdiff_t difference_type;
	typedef wchar_t* pointer;
	typedef wchar_t& reference;

	utf8_iterator(const std::string& str);
	utf8_iterator(std::string::const_iterator const &begin, std::string::const_iterator const &end);

	static utf8_iterator begin(const std::string& str);
	static utf8_iterator end(const std::string& str);

	bool operator==(const utf8_iterator& a) const;
	bool operator!=(const utf8_iterator& a) const { return ! (*this == a); }
	utf8_iterator& operator++();
	wchar_t operator*() const;
	bool next_is_end();
	const std::pair<std::string::const_iterator, std::string::const_iterator>& substr() const;
private:
	void update();

	wchar_t current_char;
	std::string::const_iterator string_end;
	std::pair<std::string::const_iterator, std::string::const_iterator> current_substr;
};

//
// encrypt, decrypt
//
LIB3RDPARTY_DECL void aes256_encrypt(const uint8_t* key, const uint8_t* iv, const uint8_t* in, const int bytes, uint8_t* out);
LIB3RDPARTY_DECL void aes256_decrypt(const uint8_t* key, const uint8_t* iv, const uint8_t* in, const int bytes, uint8_t* out);
LIB3RDPARTY_DECL std::unique_ptr<uint8_t[]> sha1(const uint8_t* in, const int len);
LIB3RDPARTY_DECL std::unique_ptr<uint8_t[]> sha256(const uint8_t* in, const int len);
LIB3RDPARTY_DECL bool verify_sha1(const uint8_t* in, int len, const uint8_t* desire_md);
// if you don't known @result bytes, use @result[EVP_MAX_MD_SIZE]
LIB3RDPARTY_DECL int hmac_md_sha(const EVP_MD* md, const uint8_t* key, int key_len, const uint8_t* msg, int msg_len, uint8_t* result);

LIB3RDPARTY_DECL void resize_uint8data(tuint8data2_C& data, int size, int vsize);

//
// Returns the base file name of a file, with directory name stripped.
// Equivalent to a portable basename() function.
//
LIB3RDPARTY_DECL std::string extract_file(const std::string& file);

//
// Returns the directory name of a file, with filename stripped.
// Equivalent to a portable dirname()
//
LIB3RDPARTY_DECL std::string extract_directory(const std::string& file);

LIB3RDPARTY_DECL std::string file_stem_name(const std::string& file);

LIB3RDPARTY_DECL std::string file_ext_name(const std::string& file);

// Returns a lowercased version of the string.
LIB3RDPARTY_DECL std::string lowercase(const std::string& str);
LIB3RDPARTY_DECL void lowercase2(std::string& str);

// Returns a uppercased version of the string.
LIB3RDPARTY_DECL std::string uppercase(const std::string& str);
LIB3RDPARTY_DECL void uppercase2(std::string& str);

#define UUID_STR_LEN	32
#define UUID_FORMATTED_STR_LEN	36
LIB3RDPARTY_DECL bool is_uuid(const std::string& str, bool line);
LIB3RDPARTY_DECL bool is_uuid2(const std::string& str, bool line);

LIB3RDPARTY_DECL std::string create_uuid(bool line);
LIB3RDPARTY_DECL std::string format_uuid(const GUID& guid);


LIB3RDPARTY_DECL std::string normalize_path(const std::string& src, bool dosstyle = false);
#ifdef _WIN32
#define os_normalize_path(src)	utils::normalize_path(src, true)
#else
#define os_normalize_path(src)	utils::normalize_path(src, false)
#endif

LIB3RDPARTY_DECL std::string hex_encode_cstyle(const char* c_str, size_t srclen, char delimiter);

LIB3RDPARTY_DECL bool localtime_clone(time_t t, struct tm& result);
LIB3RDPARTY_DECL std::string posix_strftime(const std::string& format, const struct tm& timeptr);
LIB3RDPARTY_DECL std::string format_time_ymd(time_t t);
LIB3RDPARTY_DECL std::string format_time_ymd2(time_t t, const char separator, bool align);
LIB3RDPARTY_DECL std::string format_time_ymd3(time_t t);
LIB3RDPARTY_DECL std::string format_time_ymd4(time_t t, bool year, bool mon, bool day);
LIB3RDPARTY_DECL std::string format_time_hms(time_t t);
LIB3RDPARTY_DECL std::string format_time_hm(time_t t);
LIB3RDPARTY_DECL std::string format_time_date(time_t t);
LIB3RDPARTY_DECL std::string format_time_local(time_t t);
LIB3RDPARTY_DECL std::string format_time_ymdhms(time_t t);
LIB3RDPARTY_DECL std::string format_time_ymdhms2(time_t t);
LIB3RDPARTY_DECL std::string format_time_ymdhms3(time_t t);
LIB3RDPARTY_DECL std::string format_time_dms(time_t t);

LIB3RDPARTY_DECL std::string format_second_24hoursys(int elapse);

enum {timesep_i18n, timesep_unit, timesep_colon, timesep_count};
LIB3RDPARTY_DECL std::string format_elapse_hms(int elapse, int separator = timesep_i18n, bool m2 = false);
LIB3RDPARTY_DECL std::string format_elapse_hm_or_ms(int elapse, bool ceil_m, int separator, bool m2 = false);
LIB3RDPARTY_DECL std::string format_elapse_hms2(int elapse, bool align = false);
LIB3RDPARTY_DECL std::string format_elapse_hm(int elapse, bool align = false);
LIB3RDPARTY_DECL std::string format_elapse_hm2(int elapse, bool align = false);
LIB3RDPARTY_DECL std::string format_elapse_ms2(int elapse, bool align);
LIB3RDPARTY_DECL std::string format_elapse_smsec(int elapse_ms, bool show_unit);

LIB3RDPARTY_DECL int fast_ms_to_str(int ms, char* buf);
LIB3RDPARTY_DECL std::string format_mselapse_hms(int mselapse, int separator, bool m2);
LIB3RDPARTY_DECL std::string format_mselapse_hm_or_ms_or_dotms(int mselapse, bool ceil_m, int separator, bool m2, int show_ms_below_sec = 10);
LIB3RDPARTY_DECL std::string weekday_name_form_tm_wday(int index, bool abbr);
LIB3RDPARTY_DECL int get_tm_wday(time_t t);
LIB3RDPARTY_DECL std::string get_weekday_name(time_t t, bool abbr);
LIB3RDPARTY_DECL std::string format_i64size(int64_t size);

LIB3RDPARTY_DECL int days_in_month(int year, int month);
LIB3RDPARTY_DECL int64_t interval_seconds_by_months(int months);
LIB3RDPARTY_DECL int64_t datetime_str_2_ts(const std::string& datetime);
LIB3RDPARTY_DECL int64_t date_str_2_ts(const std::string& date);
LIB3RDPARTY_DECL int64_t yyyymmddhhmmss_2_ts(const std::string& str, bool* fail = nullptr);

LIB3RDPARTY_DECL bool from_hh_mm_ss(const std::string& time_str, char separator, int& hour, int& minute, int& second);
LIB3RDPARTY_DECL int64_t mktime2(int year, int month, int day, int hour, int minute, int second);

LIB3RDPARTY_DECL int calculate_hour24_time(int64_t ts);
LIB3RDPARTY_DECL int64_t calculate_0h0m0s_ts(int64_t ts);

LIB3RDPARTY_DECL std::string replace_all(const std::string& s, const std::string &oldsub, const std::string &newsub);
LIB3RDPARTY_DECL void replace_all2(std::string& s, const std::string& oldsub, const std::string& newsub);

LIB3RDPARTY_DECL std::string unique_untitle_or_uuid7_id(const std::set<std::string>& existed_ids, const std::string& _prefix, bool uuid7, const std::string& postfix, int start_number);
#define unique_untitle_id(existed_ids, _prefix, postfix, start_number)	\
	unique_untitle_or_uuid7_id(existed_ids, _prefix, false, postfix, start_number)

#define unique_uuid7_id(existed_ids, postfix)	\
	unique_untitle_or_uuid7_id(existed_ids, null_str, true, postfix, 1)

LIB3RDPARTY_DECL std::string unique_untitle_name(const std::set<std::string>& existed_names, const std::string& _prefix, int start_number);
LIB3RDPARTY_DECL const std::string& landmark_name(int at);

LIB3RDPARTY_DECL SDL_DPoint transform_xy(double x, double y, double theta, const SDL_DPoint& src);
LIB3RDPARTY_DECL SDL_DPoint3 transform_xyz_2D(double x, double y, double theta, const SDL_DPoint3& src);

LIB3RDPARTY_DECL double imgcoor_calculate_angle_2p(double start_x, double start_y, double end_x, double end_y);
#define imgcoor_calculate_angle_2SDL_Point(start, end) \
	imgcoor_calculate_angle_2p((start).x, (start).y, (end).x, (end).y)

LIB3RDPARTY_DECL double imgcoor_calculate_angle_3p_pi(double a_x, double a_y, double b_x, double b_y, double c_x, double c_y);
#define imgcoor_calculate_angle_3SDL_Point_pi(a, b, c) \
	imgcoor_calculate_angle_3p_pi((a).x, (a).y, (b).x, (b).y, (c).x, (c).y)

LIB3RDPARTY_DECL double imgcoor_calculate_angle_3p_2pi(double a_x, double a_y, double b_x, double b_y, double c_x, double c_y);
#define imgcoor_calculate_angle_3SDL_Point_2pi(a, b, c) \
	imgcoor_calculate_angle_3p_2pi((a).x, (a).y, (b).x, (b).y, (c).x, (c).y)

//
// isvalid_xxxx
//
extern LIB3RDPARTY_DECL std::string isvalid_errstr;
LIB3RDPARTY_DECL bool isvalid_underline_id(const std::string& id, bool first_must_alpha, int min, int max);
#define MIN_SHORT_ID_OR_VAR_NAME_BYTES		2
#define MAX_SHORT_ID_OR_VAR_NAME_BYTES		16
#define isvalid_short_id_or_var_name216(id)	utils::isvalid_underline_id(id, true, MIN_SHORT_ID_OR_VAR_NAME_BYTES, MAX_SHORT_ID_OR_VAR_NAME_BYTES)

#define MIN_NORMAL_ID_OR_VAR_NAME_BYTES		2
#define MAX_NORMAL_ID_OR_VAR_NAME_BYTES		24
#define isvalid_normal_id_or_var_name224(id)	utils::isvalid_underline_id(id, true, MIN_NORMAL_ID_OR_VAR_NAME_BYTES, MAX_NORMAL_ID_OR_VAR_NAME_BYTES)

LIB3RDPARTY_DECL bool isvalid_id(const std::string& id, bool first_must_alpha, int min, int max);
LIB3RDPARTY_DECL bool isvalid_nick(const std::string& nick);

LIB3RDPARTY_DECL bool isvalid_utf8_name(const std::string& name, int min_chars, int max_chars);
#define MIN_SHORT_UTF8_NAME_CHARS	2
#define MAX_SHORT_UTF8_NAME_CHARS	16
#define isvalid_short_utf8_name216(name)  utils::isvalid_utf8_name(name, MIN_SHORT_UTF8_NAME_CHARS, MAX_SHORT_UTF8_NAME_CHARS)

#define MIN_NORMAL_UTF8_NAME_CHARS	2
#define MAX_NORMAL_UTF8_NAME_CHARS	24
#define isvalid_normal_utf8_name224(name)  utils::isvalid_utf8_name(name, MIN_NORMAL_UTF8_NAME_CHARS, MAX_NORMAL_UTF8_NAME_CHARS)

}

#endif
