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

#ifndef SERIALIZATION_STRING_UTILS_HPP_INCLUDED
#define SERIALIZATION_STRING_UTILS_HPP_INCLUDED

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <set>

#include "SDL_types.h"
#include "config.hpp"
#include "util.hpp"
#include <rtc_base/stringencode.h>

#include <rose_string_utils.hpp>

/** The type we use to represent Unicode strings. */
typedef std::vector<wchar_t> wide_string;

/** If we append a 0 to that one, we can pass it to SDL_ttf as a const Uint16*. */
typedef std::vector<Uint16> ucs2_string;
typedef std::vector<Uint32> ucs4_string;

class t_string;

struct trtsp_settings
{
	trtsp_settings(const std::string& name, bool tcp, const std::string& url)
		: name(name)
		, tcp(tcp)
		, url(url)
        , sendrtcp(false)
		, ipv4(0)
		, id(nposm)
		, width(nposm)
		, height(nposm)
	{}

	bool valid() const { return !name.empty() && !url.empty(); }

    std::string to_preferences() const;

public:
	std::string name;
	bool tcp;
	std::string url;
    bool sendrtcp;
	std::string httpdurl;
	uint32_t ipv4;

	// evaluate after parse string.
	int id;
	int width;
	int height;
};

namespace utils {

extern const std::string unicode_minus;
extern const std::string unicode_en_dash;
extern const std::string unicode_em_dash;
extern const std::string unicode_figure_dash;
extern const std::string unicode_multiplication_sign;
extern const std::string unicode_bullet;

config to_config(const std::string& str);

std::wstring to_wstring(const std::string& src);
std::string from_wchar_ptr(const wchar_t* src);
std::string from_uint16_ptr(const uint16_t* src, int wchars);
size_t wchar_ptr_2_uint16_ptr(uint16_t *dst, const wchar_t *src, size_t maxlen);

// 0x7301a8c0 => 192.168.1.115
std::string from_ipv4(uint32_t ipv4);
// 192.168.1.115 => 0x7301a8c0
uint32_t to_ipv4(const std::string& str);
// @ip1, ip2: 192.168.1.1 => 0x0101a8c0
bool is_same_net(uint32_t ip1, uint32_t ip2, int prefixlen);
// @24 => 255.255.255.0
std::string from_prefixlen(int prefixlen);

std::string join_with_null(const std::vector<std::string>& vstr);
std::string join_int_string(int n, const std::string& str, const char connector);
// bool isnewline(const char c);
// bool portable_isspace(const char c);
// bool notspace(const char c);

// enum { REMOVE_EMPTY = 0x01,	/**< REMOVE_EMPTY : remove empty elements. */
//	  STRIP_SPACES  = 0x02	/**< STRIP_SPACES : strips leading and trailing blank spaces. */
// };

// std::vector< std::string > split(std::string const &val, const char c = ',', const int flags = REMOVE_EMPTY | STRIP_SPACES);

/**
 * Splits a string based either on a separator where text within parenthesis
 * is protected from splitting (Note that one can use the same character for
 * both the left and right parenthesis. In this mode it usually makes only
 * sense to have one character for the left and right parenthesis.)
 * or if the separator == 0 it splits a string into an odd number of parts:
 * - The part before the first '(',
 * - the part between the first '('
 * - and the matching right ')', etc ...
 * and the remainder of the string.
 * Note that this will find the first matching char in the left string
 * and match against the corresponding char in the right string.
 * In this mode, a correctly processed string should return with
 * an odd number of elements to the vector and
 * an empty elements are never removed as they are placeholders.
 * hence REMOVE EMPTY only works for the separator split.
 *
 * parenthetical_split("a(b)c{d}e(f{g})h",0,"({",")}") should return
 * a vector of <"a","b","c","d","e","f{g}","h">
 */
std::vector< std::string > parenthetical_split(std::string const &val,
	const char separator = 0 , std::string const &left="(",
	std::string const &right=")",const int flags = REMOVE_EMPTY | STRIP_SPACES);

/**
 * Similar to parenthetical_split, but also expands embedded square brackets.
 * Separator must be specified and number of entries in each square bracket
 * must match in each section.
 * Leading zeros are preserved if specified between square brackets.
 * An asterisk as in [a*n] indicates to expand 'a' n times
 * 
 * This is useful to expand animation WML code.
 * Examples:
 * square_parenthetical_split("a[1-3](1,[5,6,7]),b[8,9]",",") should return
 * <"a1(1,5)","a2(1,6)","a3(1,7)","b8","b9">
 * square_parenthetical_split("abc[07-10]") should return
 * <"abc07","abc08","abc09","abc10">
 * square_parenthetical_split("a[1,2]b[3-4]:c[5,6]") should return
 * <"a1b3:c5","a2b4:c6">
 * square_parenthetical_split("abc[3,1].png") should return
 * <"abc3.png","abc2.png","abc1.png">
 * square_parenthetical_split("abc[de,xyz]") should return
 * <"abcde","abcxyz">
 * square_parenthetical_split("abc[1*3]") should return
 * <"abc1","abc1","abc1">
 */
std::vector< std::string > square_parenthetical_split(std::string const &val,
	const char separator = ',' , std::string const &left="([",
	std::string const &right=")]",const int flags = REMOVE_EMPTY | STRIP_SPACES);


/**
 * Generates a new string containing a bullet list.
 *
 * List items are preceded by the indentation blanks, a bullet string and
 * another blank; all but the last item are followed by a newline.
 *
 * @param v A container with elements.
 * @param indent Number of indentation blanks.
 * @param bullet The leading bullet string.
 */
template<typename T>
std::string bullet_list(const T& v, size_t indent = 4, const std::string& bullet = unicode_bullet)
{
	std::ostringstream str;

	for(typename T::const_iterator i = v.begin(); i != v.end(); ++i) {
		if(i != v.begin()) {
			str << '\n';
		}

		str << std::string(indent, ' ') << bullet << ' ' << *i;
	}

	return str.str();
}

/**
 * This function is identical to split(), except it does not split
 * when it otherwise would if the previous character was identical to the parameter 'quote'.
 * i.e. it does not split quoted commas.
 * This method was added to make it possible to quote user input,
 * particularly so commas in user input will not cause visual problems in menus.
 *
 * @todo Why not change split()? That would change the methods post condition.
 */
std::vector< std::string > quoted_split(std::string const &val, char c= ',',
                                        int flags = REMOVE_EMPTY | STRIP_SPACES, char quote = '\\');
std::pair< int, int > parse_range(std::string const &str);
std::vector< std::pair< int, int > > parse_ranges(std::string const &str);
int apply_modifier( const int number, const std::string &amount, const int minimum = 0);
int apply_modifier( const int number, const std::string &amount, const int minimum, const int maximum);

/* add a "+" or replace the "-" par Unicode minus */
inline std::string print_modifier(const std::string &mod)
{ return mod[0] == '-' ?
	(unicode_minus + std::string(mod.begin()+1, mod.end())) : ("+" + mod);}

/** Prepends a configurable set of characters with a backslash */
// std::string escape(const std::string &str, const char *special_chars);

/**
 * Prepend all special characters with a backslash.
 *
 * Special characters are:
 * #@{}+-,\*=
 */
// inline std::string escape(const std::string &str)
// { return escape(str, "#@{}+-,\\*="); }

/** Remove all escape characters (backslash) */
// std::string unescape(const std::string &str);

/** Remove whitespace from the front and back of the string 'str'. */
// std::string &strip(std::string &str);

/** Remove whitespace from the back of the string 'str'. */
// std::string &strip_end(std::string &str);

/** Convert no, false, off, 0, 0.0 to false, empty to def, and others to true */
bool string_bool(const std::string& str,bool def=false);

/** Convert into a signed value (using the Unicode "-" and +0 convention */
std::string signed_value(int val);

/** Sign with Unicode "-" if negative */
std::string half_signed_value(int val);

/** Convert into a percentage (using the Unicode "-" and +0% convention */
inline std::string signed_percent(int val) {return signed_value(val) + "%";}

/**
 * Convert into a string with an SI-postfix.
 *
 * If the unit is to be translatable,
 * a t_string should be passed as the third argument.
 * _("unit_byte^B") is suggested as standard.
 *
 * There are no default values because they would not be translatable.
 */
std::string si_string(double input, bool base2, std::string unit);

/**
 * Try to complete the last word of 'text' with the 'wordlist'.
 *
 * @param[in]  'text'     Text where we try to complete the last word of.
 * @param[out] 'text'     Text with completed last word.
 * @param[in]  'wordlist' A vector of strings to complete against.
 * @param[out] 'wordlist' A vector of strings that matched 'text'.
 *
 * @return 'true' iff text is just one word (no spaces)
 */
bool word_completion(std::string& text, std::vector<std::string>& wordlist);

/** Check if a message contains a word. */
bool word_match(const std::string& message, const std::string& word);

/**
 * Match using '*' as any number of characters (including none), and '?' as any
 * one character.
 */
bool wildcard_string_match(const std::string& str, const std::string& match);

/**
 * Check if the username contains only valid characters.
 *
 * (all alpha-numeric characters plus underscore and hyphen)
 */
bool isvalid_username(const std::string &login);

/**
 * Check if the username pattern contains only valid characters.
 *
 * (all alpha-numeric characters plus underscore, hyphen,
 * question mark and asterisk)
 */
bool isvalid_wildcard(const std::string &login);

bool isvalid_filename(const std::string& filename);


/**
 * Functions for converting Unicode wide-char strings to UTF-8 encoded strings,
 * back and forth.
 */
/*
class invalid_utf8_exception : public std::exception {
};

class utf8_iterator
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
*/
std::string wstring_to_string(const wide_string &);
wide_string string_to_wstring(const std::string &);
std::string wchar_to_string(const wchar_t);

std::string utf8str_insert(const std::string& src, const int from, const std::string& insert);
std::string utf8str_erase(const std::string& src, const int from, const int len);
std::string utf8str_substr(const std::string& src, const int from, const int len);
// size_t utf8str_len(const std::string& utf8str);
// int utf8str_bytes(const std::string& utf8str, int chars);

// whether [data, len] is utf8 encoding or not
// bool is_utf8str(const char* data, int64_t len);
// @param[in] utf8str_size: if cannot sure size, use nposm.
// std::string truncate_to_max_bytes(const char* utf8str, const int utf8str_size, int max_bytes);

// @param[in] utf8str_size: if cannot sure size, use nposm.
// std::string truncate_to_max_chars(const char* utf8str, const int utf8str_size, int max_chars);

/**
 * Truncates a string.
 *
 * If the string send has more than size utf-8 characters it will be truncated
 * to this size.
 * No assumptions can be made about the actual size of the string.
 *
 * @param[in]  str     String which can be converted to utf-8.
 * @param[out] str     String which contains maximal size utf-8 characters.
 * @param size         The size to truncate at.
 */
void truncate_as_wstring(std::string& str, const size_t size);

/**
 * Truncates a string to a given utf-8 character count and then appends an ellipsis.
 */
void ellipsis_truncate(std::string& str, const size_t size);

/** Compare tow utf8 string. */
bool utf8str_compare(const std::string& str1, const std::string& str2);

// check color_str whether valid or not. 0: invalid, or color value.
uint32_t check_color(const std::string& color_str);

std::set<int> to_set_int(const std::string& value, bool fail_0 = false, bool negative_fail = false, bool clear_on_fail = true, const char c = ',');

std::vector<int> to_vector_int(const std::string& value);

bool is_single_cfg(const std::string& str, std::string* element_name = NULL);

struct tcfg_string_pair 
{
	tcfg_string_pair()
		: iscfg(true)
	{}

	tcfg_string_pair(const config& cfg)
		: iscfg(true)
		, cfg(cfg)
	{}

	tcfg_string_pair(const std::string& text)
		: iscfg(false)
		, text(text)
	{}

	bool iscfg;
	config cfg;
	std::string text;
};
// Parse a text string. Return a vector with the different parts of the
// text. Each markup item is a separate part while the text between
// markups are separate parts.
void split_integrate_src(const std::string& text, const std::set<std::string>& support_markups, std::map<int, tcfg_string_pair>& ret);
void verify_splited_intergate(const std::string& text, const std::map<int, tcfg_string_pair>& segments);
std::string ht_drop_markup(const std::string& text, const std::set<std::string>& support_markups, const std::string& text_markup);


bool is_hexstring(const char* c_str, int size);
int hex_decode_prealloc(const char* data, int size, uint8_t* to_ptr);

inline std::string hex_encode(const char* source, size_t srclen) {
	return rtc::hex_encode_with_delimiter(source, srclen, 0);
}

inline size_t hex_decode(char* buffer, size_t buflen, const std::string& source) {
	return rtc::hex_decode_with_delimiter(buffer, buflen, source, 0);
}

void hex_encode2(uint8_t* data, int len, char* out);

// chinese idnumber
bool is_valid_idnumber(const std::string& value);
bool is_valid_phone(const std::string& phone);
bool is_valid_plateno(const std::string& plateno);
std::string desensitize_idnumber(const std::string& idnumber);
std::string desensitize_phone(const std::string& phone);
std::string desensitize_name(const std::string& name);

std::vector<trtsp_settings> parse_rtsp_string(const std::string& str, bool check_0, bool unique_url);

std::string index_2_tstr(int index);

} // end namespace utils

#endif
