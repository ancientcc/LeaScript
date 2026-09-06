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

#include "rose_global.hpp"

#include "gettext.hpp"
#include "serialization/string_utils.hpp"
#include "serialization/parser.hpp"
#include "util.hpp"
#include "integrate.hpp"
#include "formula_string_utils.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"

#include <algorithm>
#include <iomanip>

#include <url/gurl.h>

#include <openssl/aes.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>

#include <boost/array.hpp>
#include <boost/lexical_cast.hpp>

#ifndef _WIN32
void CoCreateGuid(GUID* pguid)
{
	VALIDATE(false, null_str); // now CoCreateGuid only call when windows.
	memset(pguid, 0, sizeof(GUID));
}
#endif

std::string trtsp_settings::to_preferences() const
{
	VALIDATE(valid(), null_str);
	std::stringstream ss;
	ss << name << "," << (tcp? "tcp": "udp") << "," << url;
	return ss.str();
}

namespace utils {

const std::string ellipsis = "...";

const std::string unicode_minus = "-";
const std::string unicode_en_dash = "-";
const std::string unicode_em_dash = "-";
const std::string unicode_figure_dash = "-";
const std::string unicode_multiplication_sign = "-";
const std::string unicode_bullet = "-";

config to_config(const std::string& str)
{
	config cfg;
	try {
		::read(cfg, str);
	} catch(config::error& e) {
		SDL_Log("%s", e.message.c_str());
		cfg.clear();
	} catch (twml_exception& e) {
		SDL_Log("%s", e.user_message.c_str());
		cfg.clear();
	} 
	return cfg;
}

std::wstring to_wstring(const std::string& src)
{
	const char* tocode = sizeof(wchar_t) == 2? "UTF-16LE": "UTF-32LE";
	// param4 Inbytesleft indicates the number of input bytes to be used for conversion. 
	// '+1' make sure that converted string ends with '\0'. 
	// For std::string, the string returned by the c_str() always ends in a \0.
	// if result isn't nullptr, use SDL_free to free.
	wchar_t* wchar_ptr = (wchar_t *)SDL_iconv_string(tocode, "UTF-8", src.c_str(), src.size() + 1);
	std::wstring result(wchar_ptr);
	SDL_free(wchar_ptr);
	return result;
}

bool is_hexstring(const char* c_str, int size)
{
	if (size % 2 != 0) {
		return false;
	}
	for (int at = 0; at < size; at ++) {
		const char ch = c_str[at];
		if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) {
		} else {
			return false;
		}
	}
	return true;
}

// It is required to ensure that [data,size] is a valid hex-string, 
// To meet this requirement, call is_hexstring(data, size) before it.
// @to_ptr: 1)caller allocate memory
//          2)make sure can contain more size/2 bytes.
// return value: alway return size/2.
int hex_decode_prealloc(const char* data, int size, uint8_t* to_ptr)
{
	VALIDATE(size > 0 && size % 2 == 0, null_str);
	int hsize = size / 2;
	int index = 0;
	for (int at = 0; at < size; at += 2) {
		char ch0 = data[at];
		char ch1 = data[at + 1];
		uint8_t val = 0;
		if (ch0 >= '0' && ch0 <= '9') {
			val = ch0 - '0';
		} else if (ch0 >= 'A' && ch0 <= 'F') {
			val = ch0 - 'A' + 10;
		} else if (ch0 >= 'a' && ch0 <= 'f') {
			val = ch0 - 'a' + 10;
		} else {
			VALIDATE(false, null_str);
		}
		val <<= 4;
		if (ch1 >= '0' && ch1 <= '9') {
			val |= ch1 - '0';
		} else if (ch1 >= 'A' && ch1 <= 'F') {
			val |= ch1 - 'A' + 10;
		} else if (ch1 >= 'a' && ch1 <= 'f') {
			val |= ch1 - 'a' + 10;
		} else {
			VALIDATE(false, null_str);
		}
		to_ptr[index] = val;
		index ++;
	}
	VALIDATE(index == hsize, null_str);
	return hsize;
}

// 0x7301a8c0 => 192.168.1.115
std::string from_ipv4(uint32_t ipv4)
{
	char buf[32];
	SDL_snprintf(buf, sizeof(buf), "%i.%i.%i.%i",
		posix_lo8(posix_lo16(ipv4)), posix_hi8(posix_lo16(ipv4)),
		posix_lo8(posix_hi16(ipv4)), posix_hi8(posix_hi16(ipv4)));
	return buf;
}

// 192.168.1.115 => 0x7301a8c0
uint32_t to_ipv4(const std::string& str)
{
	std::vector<std::string> vstr = utils::split(str, '.', 0);
	if (vstr.size() != 4) {
		return 0;
	}
	uint32_t ret = 0;
	for (int n = 0; n < (int)vstr.size(); n ++) {
		const std::string& s = vstr[n];
		if (!utils::isinteger(s)) {
			return 0;
		}
		uint32_t val = to_int(s);
		if (val > 255) {
			return 0;
		}
		ret |= val << (n * 8);
	}
	return ret;
}

// @ip1, ip2: 192.168.1.1 => 0x0101a8c0
bool is_same_net(uint32_t ip1, uint32_t ip2, int prefixlen)
{
	VALIDATE(prefixlen >= 1 && prefixlen <= 31, null_str);
	uint32_t netmask_le = ~((1u << (32 - prefixlen)) - 1);
	ip1 = SDL_Swap32(ip1);
	ip2 = SDL_Swap32(ip2);
	return (ip1 & netmask_le) == (ip2 & netmask_le);
}

// @24 => 255.255.255.0
std::string from_prefixlen(int prefixlen)
{
	if (prefixlen == 0) {
		return "0.0.0.0";
	} else if (prefixlen < 0 || prefixlen > 32) {
		return str_cast(prefixlen);
	}
	VALIDATE(prefixlen >= 1 && prefixlen <= 32, null_str);
	// 24 => netmask_le: 0xffffff00;
	uint32_t netmask_le = ~((1u << (32 - prefixlen)) - 1);
	// 24 => netmask_be: 0x00ffffff;
	uint32_t netmask_be = SDL_Swap32(netmask_le);
	const uint8_t* ptr = (const uint8_t*)&netmask_be;
	std::stringstream ss;
	for (int n = 0; n < 4; n ++) {
		if (n != 0) {
			ss << ".";
		}
		ss << (int)(ptr[n]);
	}
	// SDL_Log("from_refixlen, %i => %s(0x%08x)", prefixlen, ss.str().c_str(), netmask_be);
	return ss.str();
}

std::string from_wchar_ptr(const wchar_t* src)
{
	if (src == nullptr || src[0] == 0) {
		return null_str;
	}
	// param4 Inbytesleft indicates the number of input bytes to be used for conversion. 
	// '+1' make sure that converted string ends with '\0'. 
	// if result isn't nullptr, use SDL_free to free.
	const char* fromcode = sizeof(wchar_t) == 2? "UTF-16LE": "UTF-32LE";
	char* utf8_ptr = SDL_iconv_string("UTF-8", fromcode, (const char*)src, (SDL_wcslen(src) + 1) * sizeof(wchar_t));
	std::string result(utf8_ptr);
	SDL_free(utf8_ptr);
	return result;
}

// WCHAR* == > utf8. sizeof(WCHAR) always be 2.
std::string from_uint16_ptr(const uint16_t* src, int wchars)
{
	if (src == nullptr || src[0] == 0) {
		return null_str;
	}

	if (wchars == nposm) {
		wchars = 0;
		while (src[wchars] != 0) {
			wchars ++;
		}
	}
	char* utf8_ptr = SDL_iconv_string("UTF-8", "UTF-16LE", (const char*)src, (wchars + 1) * sizeof(uint16_t));
	std::string result(utf8_ptr);
	SDL_free(utf8_ptr);
	return result;
}

// @dst, src: caller must make sure them allocated.
// @maxlen: include terminal '\0'.
size_t wchar_ptr_2_uint16_ptr(uint16_t *dst, const wchar_t *src, size_t maxlen)
{
	if (sizeof(wchar_t) == sizeof(uint16_t)) {
		return SDL_wcslcpy((wchar_t*)dst, src, maxlen);
	}

    size_t srclen = SDL_wcslen(src);
    if (maxlen > 0) {
        size_t len = SDL_min(srclen, maxlen - 1);
		for (size_t n = 0; n < len; n ++) {
			dst[n] = src[n];
		}
        dst[len] = '\0';
    }
    return srclen;
}

std::string join_with_null(const std::vector<std::string>& vstr)
{
	int len = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& str = *it;
		if (str.empty()) {
			continue;
		}
		len += str.size() + 1;
	}
	if (len != 0) {
		len ++;
	} else {
		return null_str;
	}

	char* buf = (char*)malloc(len);
	memset(buf, 0, len);
	int pos = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& str = *it;
		if (str.empty()) {
			continue;
		}
		memcpy(buf + pos, str.c_str(), str.size());
		pos += str.size() + 1;
	}
	VALIDATE(pos == len - 1, null_str);
	return std::string(buf, len);
}

std::string join_int_string(int n, const std::string& str, const char connector)
{
	size_t str_size = str.size();
	const int max_int_size = 32;
	char* ptr = (char*)malloc(max_int_size + 1 + str_size);
	SDL_itoa(n, ptr, 10);

	size_t na_size = strlen(ptr);
	ptr[na_size] = connector;
	if (str_size > 0) {
		memcpy(ptr + na_size + 1, str.c_str(), str_size);
	}
	ptr[na_size + 1 + str_size] = '\0';
	std::string ret = ptr;
	free(ptr);
	return ret;
}

std::vector< std::string > square_parenthetical_split(std::string const &val,
		const char separator, std::string const &left,
		std::string const &right,const int flags)
{
	std::vector< std::string > res;
	std::vector<char> part;
	bool in_parenthesis = false;
	std::vector<std::string::const_iterator> square_left;
	std::vector<std::string::const_iterator> square_right;
	std::vector< std::string > square_expansion;

	std::string lp=left;
	std::string rp=right;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	std::string::const_iterator j1;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
	j1=i1;

	if (i1 == val.end()) return res;
	
	if (!separator) {
		// "Separator must be specified for square bracket split funtion.\n";
		return res;
	}

	if(left.size()!=right.size()){
		// "Left and Right Parenthesis lists not same length\n";
		return res;
	}

	while (true) {
		if(i2 == val.end() || (!in_parenthesis && *i2 == separator)) {
			//push back square contents
			size_t size_square_exp = 0;
			for (size_t i=0; i < square_left.size(); i++) {
				std::string tmp_val(square_left[i]+1,square_right[i]);
				std::vector< std::string > tmp = split(tmp_val);
				std::vector<std::string>::const_iterator itor = tmp.begin();
				for(; itor != tmp.end(); ++itor) {
					size_t found_tilde = (*itor).find_first_of('~');
					if (found_tilde == std::string::npos) {
						size_t found_asterisk = (*itor).find_first_of('*');
						if (found_asterisk == std::string::npos) {
							std::string tmp = (*itor);
							square_expansion.push_back(strip(tmp));
						}
						else { //'*' multiple expansion
							std::string s_begin = (*itor).substr(0,found_asterisk);
							s_begin = strip(s_begin);
							std::string s_end = (*itor).substr(found_asterisk+1);
							s_end = strip(s_end);
							for (int ast=atoi(s_end.c_str()); ast>0; --ast)
								square_expansion.push_back(s_begin);
						}
					}
					else { //expand number range
						std::string s_begin = (*itor).substr(0,found_tilde);
						s_begin = strip(s_begin);
						int begin = atoi(s_begin.c_str());
						size_t padding = 0;
						while (padding<s_begin.size() && s_begin[padding]=='0') {
							padding++;
						}
						std::string s_end = (*itor).substr(found_tilde+1);
						s_end = strip(s_end);
						int end = atoi(s_end.c_str());
						if (padding==0) {
							while (padding<s_end.size() && s_end[padding]=='0') {
								padding++;
							}
						}
						int increment = (end >= begin ? 1 : -1);
						end+=increment; //include end in expansion
						for (int k=begin; k!=end; k+=increment) {
							std::string pb = boost::lexical_cast<std::string>(k);
							for (size_t p=pb.size(); p<=padding; p++)
								pb = std::string("0") + pb;
							square_expansion.push_back(pb);
						}
					}
				}
				if (i*square_expansion.size() != (i+1)*size_square_exp ) {
					std::string tmp(i1, i2);
					// "Square bracket lengths do not match up: "+tmp+"\n";
					return res;
				}
				size_square_exp = square_expansion.size();
			}
			
			//combine square contents and rest of string for comma zone block
			size_t j = 0;
			size_t j_max = 0;
			if (square_left.size() != 0)
				j_max = square_expansion.size() / square_left.size();
			do {
				j1 = i1;
				std::string new_val;
				for (size_t i=0; i < square_left.size(); i++) {
					std::string tmp_val(j1, square_left[i]);
					new_val.append(tmp_val);
					size_t k = j+i*j_max;
					if (k < square_expansion.size())
						new_val.append(square_expansion[k]);
					j1 = square_right[i]+1;
				}
				std::string tmp_val(j1, i2);
				new_val.append(tmp_val);
				if (flags & STRIP_SPACES)
					strip_end(new_val);
				if (!(flags & REMOVE_EMPTY) || !new_val.empty())
					res.push_back(new_val);
				j++;
			} while (j<j_max);
			
			if (i2 == val.end()) //escape loop
				break;
			++i2;
			if (flags & STRIP_SPACES) { //strip leading spaces
				while (i2 != val.end() && portable_isspace(*i2))
					++i2;
			}
			i1=i2;
			square_left.clear();
			square_right.clear();
			square_expansion.clear();
			continue;
		}
		if(!part.empty() && *i2 == part.back()) {
			part.pop_back();
			if (*i2 == ']') square_right.push_back(i2);
			if (part.empty())
				in_parenthesis = false;
			++i2;
			continue;
		}
		bool found=false;
		for(size_t i=0; i < lp.size(); i++) {
			if (*i2 == lp[i]){
				if (*i2 == '[')
					square_left.push_back(i2);
				++i2;
				part.push_back(rp[i]);
				found=true;
				break;
			}
		}
		if(!found){
			++i2;
		} else
			in_parenthesis = true;
	}

	if (!part.empty()) {
		// "Mismatched parenthesis:\n"<<val<<"\n";;
	}

	return res;
}

std::vector< std::string > parenthetical_split(std::string const &val,
		const char separator, std::string const &left,
		std::string const &right,const int flags)
{
	std::vector< std::string > res;
	std::vector<char> part;
	bool in_parenthesis = false;

	std::string lp=left;
	std::string rp=right;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2;
	if (flags & STRIP_SPACES) {
		while (i1 != val.end() && portable_isspace(*i1))
			++i1;
	}
	i2=i1;
	
	if (left.size()!=right.size()) {
		// "Left and Right Parenthesis lists not same length\n";
		return res;
	}

	while (i2 != val.end()) {
		if(!in_parenthesis && separator && *i2 == separator){
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
			i1=i2;
			continue;
		}
		if(!part.empty() && *i2 == part.back()){
			part.pop_back();
			if(!separator && part.empty()){
				std::string new_val(i1, i2);
				if (flags & STRIP_SPACES)
					strip(new_val);
				res.push_back(new_val);
				++i2;
				i1=i2;
			}else{
				if (part.empty())
					in_parenthesis = false;
				++i2;
			}
			continue;
		}
		bool found=false;
		for(size_t i=0; i < lp.size(); i++){
			if (*i2 == lp[i]){
				if (!separator && part.empty()){
					std::string new_val(i1, i2);
					if (flags & STRIP_SPACES)
						strip(new_val);
					res.push_back(new_val);
					++i2;
					i1=i2;
				}else{
					++i2;
				}
				part.push_back(rp[i]);
				found=true;
				break;
			}
		}
		if(!found){
			++i2;
		} else
			in_parenthesis = true;
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	if (!part.empty()) {
		// "Mismatched parenthesis:\n"<<val<<"\n";;
	}

	return res;
}

// Modify a number by string representing integer difference, or optionally %
int apply_modifier( const int number, const std::string &amount, const int minimum ) {
	// wassert( amount.empty() == false );
	int value = atoi(amount.c_str());
	if(amount[amount.size()-1] == '%') {
		value = div100rounded(number * value);
	}
	value += number;
	if (( minimum > 0 ) && ( value < minimum ))
	    value = minimum;
	return value;
}

int apply_modifier( const int number, const std::string &amount, const int minimum, const int maximum)
{
	// wassert( amount.empty() == false );
	int value = atoi(amount.c_str());
	if(amount[amount.size()-1] == '%') {
		value = div100rounded(maximum * value);
	}
	value += number;
	if (value < minimum)
	    value = minimum;
	if (value > maximum)
	    value = maximum;
	return value;
}
/*
std::string escape(const std::string &str, const char *special_chars)
{
	std::string::size_type pos = str.find_first_of(special_chars);
	if (pos == std::string::npos) {
		// Fast path, possibly involving only reference counting.
		return str;
	}
	std::string res = str;
	do {
		res.insert(pos, 1, '\\');
		pos = res.find_first_of(special_chars, pos + 2);
	} while (pos != std::string::npos);
	return res;
}

std::string unescape(const std::string &str)
{
	std::string::size_type pos = str.find('\\');
	if (pos == std::string::npos) {
		// Fast path, possibly involving only reference counting.
		return str;
	}
	std::string res = str;
	do {
		res.erase(pos, 1);
		pos = res.find('\\', pos + 1);
	} while (pos != std::string::npos);
	return str;
}
*/
bool string_bool(const std::string& str, bool def) {
	if (str.empty()) return def;

	// yes/no is the standard, test it first
	if (str == "yes") return true;
	if (str == "no"|| str == "false" || str == "off" || str == "0" || str == "0.0")
		return false;

	// all other non-empty string are considered as true
	return true;
}

std::string signed_value(int val)
{
	std::ostringstream oss;
	oss << (val >= 0 ? "+" : unicode_minus) << abs(val);
	return oss.str();
}

std::string half_signed_value(int val)
{
	std::ostringstream oss;
	if (val < 0)
		oss << unicode_minus;
	oss << abs(val);
	return oss.str();
}

static void si_string_impl_stream_write(std::stringstream &ss, double input) {
#ifdef _MSC_VER
	// Visual C++ makes 'precision' set the number of decimal places.
	// Other platforms make it set the number of significant figures
	ss.precision(1);
	ss << std::fixed
	   << input;
#else
	// Workaround to display 1023 KiB instead of 1.02e3 KiB
	if (input >= 1000)
		ss.precision(4);
	else
		ss.precision(3);
	ss << input;
#endif
}

std::string si_string(double input, bool base2, std::string unit) {
	const double multiplier = base2 ? 1024 : 1000;

	typedef boost::array<std::string, 9> strings9;

	strings9 prefixes;
	strings9::const_iterator prefix;
	if (input < 1.0) {
		strings9 tmp = { {
			"",
			_("prefix_milli^m"),
			_("prefix_micro^u"),
			_("prefix_nano^n"),
			_("prefix_pico^p"),
			_("prefix_femto^f"),
			_("prefix_atto^a"),
			_("prefix_zepto^z"),
			_("prefix_yocto^y")
		} };
		prefixes = tmp;
		prefix = prefixes.begin();
		while (input < 1.0  && *prefix != prefixes.back()) {
			input *= multiplier;
			++prefix;
		}
	} else {
		strings9 tmp = { {
			"",
			(base2 ?
				_("prefix_kibi^K") :
				_("prefix_kilo^k")
			),
			_("prefix_mega^M"),
			_("prefix_giga^G"),
			_("prefix_tera^T"),
			_("prefix_peta^P"),
			_("prefix_exa^E"),
			_("prefix_zetta^Z"),
			_("prefix_yotta^Y")
		} };
		prefixes = tmp;
		prefix = prefixes.begin();
		while (input > multiplier && *prefix != prefixes.back()) {
			input /= multiplier;
			++prefix;
		}
	}

	std::stringstream ss;
	si_string_impl_stream_write(ss, input);
	ss << ' '
	   << *prefix
	   << (base2 && (*prefix != "") ? _("infix_binary^i") : "")
	   << unit;
	return ss.str();
}

bool word_completion(std::string& text, std::vector<std::string>& wordlist) {
	std::vector<std::string> matches;
	const size_t last_space = text.rfind(" ");
	// If last character is a space return.
	if (last_space == text.size() -1) {
		wordlist = matches;
		return false;
	}

	bool text_start;
	std::string semiword;
	if (last_space == std::string::npos) {
		text_start = true;
		semiword = text;
	} else {
		text_start = false;
		semiword.assign(text, last_space + 1, text.size());
	}

	std::string best_match = semiword;
	for (std::vector<std::string>::const_iterator word = wordlist.begin();
			word != wordlist.end(); ++word)
	{
		if (word->size() < semiword.size()
		|| !std::equal(semiword.begin(), semiword.end(), word->begin(),
				chars_equal_insensitive))
		{
			continue;
		}
		if (matches.empty()) {
			best_match = *word;
		} else {
			int j = 0;
			while (toupper(best_match[j]) == toupper((*word)[j])) j++;
			if (best_match.begin() + j < best_match.end()) {
				best_match.erase(best_match.begin() + j, best_match.end());
			}
		}
		matches.push_back(*word);
	}
	if(!matches.empty()) {
		text.replace(last_space + 1, best_match.size(), best_match);
	}
	wordlist = matches;
	return text_start;
}

static bool is_word_boundary(char c) {
	return (c == ' ' || c == ',' || c == ':' || c == '\'' || c == '"' || c == '-');
}

bool word_match(const std::string& message, const std::string& word) {
	size_t first = message.find(word);
	if (first == std::string::npos) return false;
	if (first == 0 || is_word_boundary(message[first - 1])) {
		size_t next = first + word.size();
		if (next == message.size() || is_word_boundary(message[next])) {
			return true;
		}
	}
	return false;
}

bool wildcard_string_match(const std::string& str, const std::string& match) {
	const bool wild_matching = (!match.empty() && match[0] == '*');
	const std::string::size_type solid_begin = match.find_first_not_of('*');
	const bool have_solids = (solid_begin != std::string::npos);
	// Check the simple case first
	if(str.empty() || !have_solids) {
		return wild_matching || str == match;
	}
	const std::string::size_type solid_end = match.find_first_of('*', solid_begin);
	const std::string::size_type solid_len = (solid_end == std::string::npos)
		? match.length() - solid_begin : solid_end - solid_begin;
	std::string::size_type current = 0;
	bool matches;
	do {
		matches = true;
		// Now try to place the str into the solid space
		const std::string::size_type test_len = str.length() - current;
		for(std::string::size_type i=0; i < solid_len && matches; ++i) {
			char solid_c = match[solid_begin + i];
			if(i > test_len || !(solid_c == '?' || solid_c == str[current+i])) {
				matches = false;
			}
		}
		if(matches) {
			// The solid space matched, now consume it and attempt to find more
			const std::string consumed_match = (solid_begin+solid_len < match.length())
				? match.substr(solid_end) : "";
			const std::string consumed_str = (solid_len < test_len)
				? str.substr(current+solid_len) : "";
			matches = wildcard_string_match(consumed_str, consumed_match);
		}
	} while(wild_matching && !matches && ++current < str.length());
	return matches;
}

std::vector< std::string > quoted_split(std::string const &val, char c, int flags, char quote)
{
	std::vector<std::string> res;

	std::string::const_iterator i1 = val.begin();
	std::string::const_iterator i2 = val.begin();

	while (i2 != val.end()) {
		if (*i2 == quote) {
			// Ignore quoted character
			++i2;
			if (i2 != val.end()) ++i2;
		} else if (*i2 == c) {
			std::string new_val(i1, i2);
			if (flags & STRIP_SPACES)
				strip(new_val);
			if (!(flags & REMOVE_EMPTY) || !new_val.empty())
				res.push_back(new_val);
			++i2;
			if (flags & STRIP_SPACES) {
				while(i2 != val.end() && *i2 == ' ')
					++i2;
			}

			i1 = i2;
		} else {
			++i2;
		}
	}

	std::string new_val(i1, i2);
	if (flags & STRIP_SPACES)
		strip(new_val);
	if (!(flags & REMOVE_EMPTY) || !new_val.empty())
		res.push_back(new_val);

	return res;
}

std::pair< int, int > parse_range(std::string const &str)
{
	const std::string::const_iterator dash = std::find(str.begin(), str.end(), '-');
	const std::string a(str.begin(), dash);
	const std::string b = dash != str.end() ? std::string(dash + 1, str.end()) : a;
	std::pair<int,int> res(atoi(a.c_str()), atoi(b.c_str()));
	if (res.second < res.first)
		res.second = res.first;

	return res;
}

std::vector< std::pair< int, int > > parse_ranges(std::string const &str)
{
	std::vector< std::pair< int, int > > to_return;
	std::vector<std::string> strs = utils::split(str);
	std::vector<std::string>::const_iterator i, i_end=strs.end();
	for(i = strs.begin(); i != i_end; ++i) {
		to_return.push_back(parse_range(*i));
	}
	return to_return;
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
/*
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
	if(current_substr.first == string_end)
		return;

	size_t size = byte_size_from_utf8_first(*current_substr.first);
	current_substr.second = current_substr.first + size;

	current_char = static_cast<unsigned char>(*current_substr.first);
	// Convert the first character
	if(size != 1) {
		current_char &= 0xFF >> (size + 1);
	}

	// Convert the continuation bytes
	for(std::string::const_iterator c = current_substr.first+1;
			c != current_substr.second; ++c) {
		// If the string ends occurs within an UTF8-sequence, this is bad.
		if (c == string_end)
			throw invalid_utf8_exception();

		if ((*c & 0xC0) != 0x80)
			throw invalid_utf8_exception();

		current_char = (current_char << 6) | (static_cast<unsigned char>(*c) & 0x3F);
	}
}
*/

std::string wstring_to_string(const wide_string &src)
{
	std::string ret;

	try {
		for(wide_string::const_iterator i = src.begin(); i != src.end(); ++i) {
			unsigned int count;
			wchar_t ch = *i;

			// Determine the bytes required
			count = 1;
			if(ch >= 0x80)
				count++;

			Uint32 bitmask = 0x800;
			for(unsigned int j = 0; j < 5; ++j) {
				if(static_cast<Uint32>(ch) >= bitmask) {
					count++;
				}

				bitmask <<= 5;
			}

			if(count > 6) {
				throw invalid_utf8_exception();
			}

			if(count == 1) {
				ret.push_back(static_cast<char>(ch));
			} else {
				for(int j = static_cast<int>(count) - 1; j >= 0; --j) {
					unsigned char c = (ch >> (6 * j)) & 0x3f;
					c |= 0x80;
					if(j == static_cast<int>(count) - 1) {
						c |= 0xff << (8 - count);
					}
					ret.push_back(c);
				}
			}

		}

		return ret;
	}
	catch (invalid_utf8_exception&) {
		// "Invalid wide character string\n";
		return ret;
	}
}

std::string wchar_to_string(const wchar_t c)
{
	wide_string s;
	s.push_back(c);
	return wstring_to_string(s);
}

wide_string string_to_wstring(const std::string &src)
{
	wide_string res;

	try {
		utf8_iterator i1(src);
		const utf8_iterator i2(utf8_iterator::end(src));

		// Equivalent to res.insert(res.end(),i1,i2) which doesn't work on VC++6.
		while(i1 != i2) {
			res.push_back(*i1);
			++i1;
		}
	}
	catch (invalid_utf8_exception&) {
		// "Invalid UTF-8 string: \"" << src << "\"\n";
		return res;
	}

	return res;
}

std::string utf8str_insert(const std::string& src, const int from, const std::string& insert)
{
	if (insert.empty()) {
		return src;
	}
	VALIDATE(from >= 0, null_str);
	std::string ret;

	utils::utf8_iterator it(src);
	utils::utf8_iterator end = utils::utf8_iterator::end(src);
	int at = 0;
	for (; it != end; ++ it, at ++) {
		if (at == from) {
			ret.append(insert);
		}
		ret.append(it.substr().first, it.substr().second);
	}
	if (at == from) {
		ret.append(insert);
	}
	return ret;
}

std::string utf8str_erase(const std::string& src, const int from, const int len)
{
	VALIDATE(from >= 0 && len > 0, null_str);
	std::string ret;

	utils::utf8_iterator it(src);
	utils::utf8_iterator end = utils::utf8_iterator::end(src);
	for (int at = 0; it != end; ++ it, at ++) {
		if (at >= from && at < from + len) {
			continue;
		}
		ret.append(it.substr().first, it.substr().second);
	}
	return ret;
}

std::string utf8str_substr(const std::string& src, const int from, const int len)
{
	VALIDATE(from >= 0 && len > 0, null_str);
	std::string ret;

	utils::utf8_iterator it(src);
	utils::utf8_iterator end = utils::utf8_iterator::end(src);
	for (int at = 0; it != end; ++ it, at ++) {
		if (at >= from && at < from + len) {
			ret.append(it.substr().first, it.substr().second);
			continue;
		}
	}
	return ret;
}

// replace with truncate_to_max_chars()/truncate_to_max_chars2()
/*
void ellipsis_truncate(std::string& str, const size_t max_chars)
{
	VALIDATE(max_chars > 0, null_str);

	const size_t prev_size = str.size();

	if (prev_size > max_chars) {
		str = truncate_to_max_chars(str.c_str(), prev_size, max_chars);
	}

	if (str.size() != prev_size) {
		str += ellipsis;
	}
}
*/
void truncate_as_wstring(std::string& str, const size_t size)
{
	wide_string utf8_str = utils::string_to_wstring(str);
	if(utf8_str.size() > size) {
		utf8_str.resize(size);
		str = utils::wstring_to_string(utf8_str);
	}
}

bool utf8str_compare(const std::string& str1, const std::string& str2)
{
	utils::utf8_iterator itor1(str1);
	utils::utf8_iterator itor1_end = utils::utf8_iterator::end(str1);
	utils::utf8_iterator itor2(str2);
	utils::utf8_iterator itor2_end = utils::utf8_iterator::end(str2);

	for (; itor1 != itor1_end; ++ itor1, ++ itor2) {
		if (itor2 == itor2_end) {
			return false;
		}
		wchar_t key1 = *itor1;
		wchar_t key2 = *itor2;
		if (key1 < key2) {
			return true;
		} else if (key1 > key2) {
			return false;
		}
	}
	return true;
}

uint32_t check_color(const std::string& color_str)
{
	std::vector<std::string> fields = utils::split(color_str);
	if (fields.size() != 4) {
		return 0;
	}

	int val;
	uint32_t result = 0;
	for (int i = 0; i < 4; ++i) {
		// shift the previous value before adding, since it's a nop on the
		// first run there's no need for an if.
		result = result << 8;
		
		val = utils::to_int(fields[i]);
		if (val < 0 || val > 255) {
			return 0;
		}
		result |= utils::to_int(fields[i]);
	}
	if (!(result & 0xff000000)) {
		// avoid to confuse with special color.
		return 0;
	}
	return result;
}

std::set<int> to_set_int(const std::string& from, bool fail_0, bool negative_fail, bool clear_on_fail, const char c)
{
	std::set<int> ret;
	const std::vector<std::string> vstr = utils::split(from, c);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		int val = utils::to_int(*it);
		bool fail = false;
		if (val == 0) {
			if (fail_0) {
				fail = true;
			}
		} else if (val < 0) {
			if (negative_fail) {
				fail = true;
			}
		}
		if (fail) {
			if (clear_on_fail) {
				ret.clear();
			}
			break;
		}
		ret.insert(val);
	}
	return ret;
}

std::vector<int> to_vector_int(const std::string& value)
{
	std::vector<std::string> vstr = split(value);
	std::vector<int> ret;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		ret.push_back(lexical_cast_default<int>(*it));
	}
	return ret;
}

static bool is_username_char(char c) {
	return ((c == '_') || (c == '-'));
}

static bool is_wildcard_char(char c) {
    return ((c == '?') || (c == '*'));
}

bool isvalid_username(const std::string& username) 
{
	size_t alnum = 0, valid_char = 0, chars = 0;
	try {
		utils::utf8_iterator itor(username);
		for (; itor != utils::utf8_iterator::end(username); ++ itor) {
			wchar_t w = *itor;
			
			if ((w & 0xff00) || isalnum(w)) {
				alnum ++;
			} else if (is_username_char(w)) {
				valid_char ++;
			}
			chars ++;
		}
		if ((alnum + valid_char != chars) || valid_char == chars || !chars) {
			isvalid_errstr = _("Contains invalid characters!");
			return false;
		}
	} catch (utils::invalid_utf8_exception&) {
		isvalid_errstr = _("Invalid UTF-8 string!");
		return false;
	}
	return true;
}

bool isvalid_wildcard(const std::string& username) 
{
    const size_t alnum = std::count_if(username.begin(), username.end(), isalnum);
	const size_t valid_char =
			std::count_if(username.begin(), username.end(), is_username_char);
    const size_t wild_char =
            std::count_if(username.begin(), username.end(), is_wildcard_char);
	if ((alnum + valid_char + wild_char != username.size())
			|| valid_char == username.size() || username.empty() )
	{
		return false;
	}
	return true;
}

static bool is_filename_char(char c) {
	return c == '_' || c == '-' || c == ' ' || c == '.' || c == '(' || c == ')';
}

bool isvalid_filename(const std::string& filename)
{
	if (filename.empty()) {
		return false;
	}
	bool first = true;
	size_t alnum = 0, valid_char = 0, chars = 0;
	wchar_t last_w = 0;
	try {
		utils::utf8_iterator itor(filename);
		for (; itor != utils::utf8_iterator::end(filename); ++ itor) {
			wchar_t w = *itor;
			last_w = w;

			if (first) {
				if ((w & 0xff00) == 0) {
					uint8_t c = posix_lo8(w);
					if (c == '.' || c == ' ') {
						return false;
					}
				}
				first = false;
			}

			if ((w & 0xff00) || isalnum(w)) {
				alnum ++;
			} else if (is_filename_char(w)) {
				valid_char ++;
			} else {
				return false;
			}
			chars ++;
		}
		if (valid_char == chars) {
			return false;
		}
		if ((last_w & 0xff00) == 0) {
			uint8_t c = posix_lo8(last_w);
			if (c == '.' || c == ' ') {
				return false;
			}
		}
	} catch (utils::invalid_utf8_exception&) {
		return false;
	}
	return true;
}

// 1. single block. cannot only attribute.
// 2. may exist space char before [name].
// 3. may exist space char after [/name].
bool is_single_cfg(const std::string& str, std::string* element_name)
{
	const int len = str.size();
	const char* cstr = str.c_str();
	int ch;

	if (len < 7) { // [1][1/]
		return false;
	}
	int pos = 0;
	int name_start = -1;
	for (pos = 0; pos < len; pos ++) {
		ch = cstr[pos];
		if (ch == '\t' || ch == '\r' || ch == '\n' || ch == ' ') {
			continue;
		}
		if (name_start == -1) {
			if (ch != '[') {
				// non-space char must be begin with [.
				return false;
			}
			name_start = pos;
		} else if (ch == ']') {
			break;
		}
	}
	if (pos == len) {
		return false;
	}
	std::string key(cstr + name_start, pos + 1 - name_start); // key = [name].
	key.insert(1, "/");
	const char* ptr = strstr(cstr, key.c_str());
	if (ptr == NULL) {
		// cannot find [/name]
		return false;
	}
	for (pos = ptr + key.size() - cstr; pos < len; pos ++) {
		ch = cstr[pos];
		if (ch == '\t' || ch == '\r' || ch == '\n' || ch == ' ') {
			continue;
		}
		// all characters after [name] must space char.
		return false;
	}
	if (element_name) {
		*element_name = key.substr(2, key.size() - 3);
	}
	return true;
}

void verify_splited_intergate(const std::string& text, const std::map<int, tcfg_string_pair>& segments)
{
	if (text.empty()) {
		VALIDATE(segments.empty(), null_str);
		return;
	}
	VALIDATE(!segments.empty(), null_str);

	const char* c_str = text.c_str();
	const int size = text.size();
	int next_item_start = 0;
	for (std::map<int, tcfg_string_pair>::const_iterator it = segments.begin(); it !=segments.end(); ++ it) {
		const int item_start = it->first;
		const tcfg_string_pair& segment = it->second;
		VALIDATE(item_start >= 0 && item_start < size, null_str);
		if (next_item_start != -1) {
			VALIDATE(item_start == next_item_start, null_str);
		}

		if (segment.iscfg) {
			const int text_size = (int)segment.text.size();
			VALIDATE(text_size > 0 && item_start + (text_size + 2) * 2 < size, null_str);
			std::stringstream s;
			s << "<" << segment.text << ">";
			VALIDATE(!SDL_memcmp(c_str + item_start, s.str().c_str(), s.str().size()), null_str); 
			next_item_start = -1;
		} else {
			const int text_size = (int)segment.text.size();
			VALIDATE(text_size > 0 && item_start + text_size <= size, null_str);
			VALIDATE(!SDL_memcmp(c_str + item_start, segment.text.c_str(), text_size), null_str);
			next_item_start = item_start + text_size;
		}
	}
}

static config convert_to_wml(const char* contents, const int size)
{
	std::stringstream ss;
	tpoint quotes_pos(-1, -1);
	bool in_quotes = false;
	bool last_char_escape = false;
	const char escape_char = '\\';

	VALIDATE(contents, null_str);

	if (!size || !notspace(contents[0]) || !notspace(contents[size - 1])) {
		return config::empty_cfg;
	}

	std::vector<std::pair<std::string, tpoint> > attributes;
	// Find the different attributes.
	// No checks are made for the equal sign or something like that.
	// Attributes are just separated by spaces or newlines.
	// Attributes that contain spaces must be in single quotes.
	for (int pos = 0; pos < size; ++ pos) {
		const char c = contents[pos];
		if (c == escape_char && !last_char_escape) {
			last_char_escape = true;
		} else {
			if (c == '"' && !last_char_escape) {
				if (in_quotes) {
					quotes_pos.y = ss.str().size();
				} else {
					quotes_pos.x = ss.str().size();
				}
				ss << '"';
				in_quotes = !in_quotes;
			} else if ((c == ' ' || c == '\n') && !last_char_escape && !in_quotes) {
				// Space or newline, end of attribute.
				attributes.push_back(std::make_pair(ss.str(), quotes_pos));
				quotes_pos.x = quotes_pos.y = -1;
				ss.str("");
			} else {
				ss << c;
			}
			last_char_escape = false;
		}
	}
	if (in_quotes) {
		// Unterminated single quote after: 'ss.str()'
		return config::empty_cfg;
	}
	if (ss.str() != "") {
		attributes.push_back(std::make_pair(ss.str(), quotes_pos));
	}

	config ret;
	for (std::vector<std::pair<std::string, tpoint> >::const_iterator it = attributes.begin(); it != attributes.end(); ++ it) {
		const std::string& text = it->first;
		const tpoint quotes_pos = it->second;
		const char* c_str = text.c_str();
		const int size = text.size();
		if (size <= 2) {
			return config::empty_cfg;
		}
		size_t equal_pos = text.find('=');
		if (equal_pos == std::string::npos) {
			return config::empty_cfg;
		}
		if (quotes_pos.x != -1 && quotes_pos.y <= (int)equal_pos) {
			return config::empty_cfg;
		}
		std::string key = text.substr(0, equal_pos);
		strip(key);

		if (key.empty() || !notspace(key[0])) {
			// If all the string contains is whitespace,
			// then the whitespace may have meaning, strip will don't strip it
			return config::empty_cfg;
		}
		
		std::string value;
		if (quotes_pos.x != -1) {
			value.assign(text, quotes_pos.x + 1, quotes_pos.y - quotes_pos.x - 1);
		} else {
			value = text.substr(equal_pos + 1);
			strip(value);
		}
		ret[key] = value;
	}

	return ret;
}

void split_integrate_src(const std::string& text, const std::set<std::string>& support_markups, std::map<int, tcfg_string_pair>& ret)
{
	ret.clear();

	int item_start = 0;
	int maybe_cfg_item_start = 0;
	const char* c_str = text.c_str();
	const int size = text.size();
	enum { ELEMENT_NAME, OTHER } state = OTHER;
	for (int pos = 0; pos < size; ++ pos) {
		const char c = c_str[pos];
		if (state == OTHER) {
			if (c == '<') {
				maybe_cfg_item_start = pos;
				state = ELEMENT_NAME;
			}

		} else if (state == ELEMENT_NAME) {
			if (c == '/') {
				// Erroneous / in element name.
				// maybe_cfg_item_start = -1;
				state = OTHER;

			} else if (c == '<') {
				// overload
				maybe_cfg_item_start = pos;

			} else if (c == '>') {
				// End of this name.
				std::stringstream s;
				const std::string element_name(text.substr(maybe_cfg_item_start + 1, pos - maybe_cfg_item_start - 1));
				if (support_markups.find(element_name) != support_markups.end()) {
					s << "</" << element_name << ">";
					const std::string end_element_name = s.str();
					size_t end_pos = text.find(end_element_name, pos);
					if (end_pos != std::string::npos) {
						const int size = end_pos - pos - 1;
						const config cfg = convert_to_wml(text.c_str() + pos + 1, size);
						if (!cfg.empty()) {
							std::pair<std::map<int, tcfg_string_pair>::iterator, bool> ins;
							if (item_start != maybe_cfg_item_start) {
								VALIDATE(item_start < maybe_cfg_item_start, null_str);
								// previous
								ins = ret.insert(std::make_pair(item_start, tcfg_string_pair()));
								ins.first->second.iscfg = false;
								ins.first->second.text = text.substr(item_start, maybe_cfg_item_start - item_start);

								item_start = maybe_cfg_item_start;
							}

							ins = ret.insert(std::make_pair(item_start, tcfg_string_pair()));
							ins.first->second.iscfg = true;
							ins.first->second.text = element_name;
							ins.first->second.cfg = cfg;

							pos = end_pos + end_element_name.size() - 1;
							item_start = pos + 1;
						}
					}
				}

				state = OTHER;
			}
		}
	}
	if (item_start != size) {
		std::pair<std::map<int, tcfg_string_pair>::iterator, bool> ins;
		ins = ret.insert(std::make_pair(item_start, tcfg_string_pair()));
		ins.first->second.iscfg = false;
		ins.first->second.text = text.substr(item_start, size - item_start);
	}

	verify_splited_intergate(text, ret);
}

// when tintegrate::insert_str, string will insert in <format>...</format>
// because must not markup-nest, inserted string shoulu be drop markup.
std::string ht_drop_markup(const std::string& text, const std::set<std::string>& support_markups, const std::string& text_markup)
{
	if (text.empty()) {
		return null_str;
	}
	std::map<int, tcfg_string_pair> splited_items;
	split_integrate_src(text, support_markups, splited_items);
	if (splited_items.size() == 1 && !splited_items[0].iscfg) {
		return text;
	}

	std::stringstream ss;
	std::map<int, utils::tcfg_string_pair>::const_iterator it = splited_items.begin();
	std::map<int, utils::tcfg_string_pair>::const_iterator before_it = it;
	for (++ it; ; before_it = it, ++ it) {
		if (before_it->second.iscfg) {
			if (before_it->second.text == text_markup) {
				// string shoulde be in <format>...</format>, require escape.
				std::string stuffed = tintegrate::stuff_escape(before_it->second.cfg["text"].str(), true);
				ss << stuffed;
			}

		} else {
			if (it != splited_items.end()) {
				ss << text.substr(before_it->first, it->first - before_it->first);
			} else {
				ss << text.substr(before_it->first);
			}
		}
		if (it == splited_items.end()) {
			break;
		}
	}
	return ss.str();
}

void hex_encode2(uint8_t* data, int len, char* out)
{
	int pos = 0;
	for (int at = 0; at < len; at ++) {
		uint8_t u8 = data[at];
		uint8_t lo;
		uint8_t hi = (u8 & 0xf0) >> 4;
		if (hi <= 9) {
			out[pos ++] = hi + '0';
		} else {
			out[pos ++] = (hi - 10) + 'a';
		}
		lo = u8 & 0xf;
		if (lo <= 9) {
			out[pos ++] = lo + '0';
		} else {
			out[pos ++] = (lo - 10) + 'a';
		}
		out[pos ++] = ' ';
	}
	out[pos] = '\0';
}

bool is_valid_idnumber(const std::string& value)
{
	const int size = value.size();
	if (size != 18) {
		return false;
	}
	const char* c_str = value.c_str();

	int N = c_str[size - 1] != 'X'? size: size - 1;
	int at = 0;
	for (at = 0; at < N; at ++) {
		char ch = c_str[at];
		if (ch < '0' || ch > '9') {
			return false;
		}
	}

	// https://wenku.baidu.com/view/3078a435cf7931b765ce0508763231126edb7774.html
	const int provinces[] = {
		11, 12, 13, 14, 15,
		21, 22, 23,
		31, 32, 33, 34, 35, 36, 37,
		41, 42, 43, 44, 45, 46,
		50, 51, 52, 53, 54,
		61, 62, 63, 64, 65,
		71, 81, 82, 91 // foreign
	};
	N = sizeof(provinces) / sizeof(provinces[0]);
	int province = utils::to_int(value.substr(0, 2));
	for (at = 0; at < N; at ++) {
		if (province == provinces[at]) {
			break;
		}
	}
	if (at == N) {
		return false;
	}

	int year = utils::to_int(value.substr(6, 4));
	if (year < 1900 || year > 2100) {
		return false;
	}

	int month = utils::to_int(value.substr(10, 2));
	if (month < 1 || month > 12) {
		return false;
	}

	int day_limit[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if ((year % 400 == 0) || ((year % 4 == 0) && (year % 100 != 0))) {
		day_limit[1] = 29;
	}

	int day = utils::to_int(value.substr(12, 2));
	N = sizeof(day_limit) / sizeof(day_limit[0]);
	for (at = 0; at < N; at ++) {
		if (day < 1 || day > day_limit[month - 1]) {
			return false;
		}
	}

	// verify check-charactor
	int t = 0;
	const int factor[] = {7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2};
	for (at = 0; at < 17; at ++) {
		t = t + utils::to_int(value.substr(at, 1)) * factor[at];
	}
	const int check_code[] = {'1', '0', 'X', '9', '8', '7', '6', '5', '4', '3', '2'};
	N = sizeof(check_code) / sizeof(check_code[0]);
	int factor_code = check_code[t % N];
	int factor_code_in_idnumber = value.c_str()[17];
	if (factor_code != factor_code_in_idnumber) {
		return false;
	}
	return true;
}

bool is_valid_phone(const std::string& phone)
{
	const int size = phone.size();
	const int min_chars = 5; // 10086
	if (size < min_chars) {
		return false;
	}
	const char* c_str = phone.c_str();
	for (int at = 0; at < size; at ++) {
		if (!isdigit(c_str[at]) && c_str[at] != '-') {
			return false;
		}
	}
	return true;
}

bool is_valid_plateno(const std::string& plateno)
{
	if (plateno.empty()) {
		return false;
	}
	const char* c_str = plateno.c_str();
	if (strchr(c_str, '|') != nullptr) {
		return false;
	}
	if (strchr(c_str, ',') != nullptr) {
		return false;
	}
	return true;
}

std::string edge_desensitize_str(const std::string& str, int prefix_len, int postfix_len)
{
	VALIDATE(prefix_len >= 0 && postfix_len >= 0 && prefix_len + postfix_len > 0, null_str);
	int stars = 0;

	const int len = utils::utf8str_len(str);
	if (len <= prefix_len + postfix_len) {
		return str;
	} else {
		stars = len - prefix_len - postfix_len;
	}

	std::string ret;
	if (prefix_len > 0) {
		ret = utils::utf8str_substr(str, 0, prefix_len);
	}
	ret.append(std::string(stars, '*'));
	if (postfix_len > 0) {
		ret.append(utils::utf8str_substr(str, prefix_len + stars, postfix_len));
	}
	return ret;
}

std::string desensitize_idnumber(const std::string& idnumber)
{
	return edge_desensitize_str(idnumber, 3, 4);
}

std::string desensitize_phone(const std::string& phone)
{
	if (!is_valid_phone(phone)) {
		// if it is invaoid phone, not blur.
		return phone;
	}
	// chinese mobile is 11.
	return edge_desensitize_str(phone, 3, 4);
}

std::string desensitize_name(const std::string& name)
{
	int len = utils::utf8str_len(name);
	if (len < 2) {
		return name;
	}

	const char* stars = "*";
	const int stars_len = 1;
	size_t size = 0;
	len = name.size();
	const uint8_t* data_ptr = (const uint8_t*)name.c_str();
	int pos = 0;
	for (; pos < len; ) {
		uint8_t ch = data_ptr[pos];
		if ((ch & 0x80) == 0) {
			pos += 1;
		} else if ((ch & 0xE0) == 0xC0) {
			pos += 2;
		} else if ((ch & 0xF0) == 0xE0) {
			pos += 3;
		} else if ((ch & 0xF8) == 0xF0) {
			pos += 4;
		} else if ((ch & 0xFC) == 0xF8) {
			pos += 5;
		} else if ((ch & 0xFE) == 0xFC) {
			pos += 6;
		} else {
			VALIDATE(false, null_str);
		}
		size ++;
		// now only first is orignal.
		break;
	}
	std::string ret = name.substr(0, pos);
	ret.append(stars, stars_len);

	return ret;
}

std::vector<trtsp_settings> parse_rtsp_string(const std::string& str, bool check_0, bool unique_url)
{
	std::vector<trtsp_settings> ret;
	if (str.empty()) {
		return ret;
	}
	std::vector<std::string> vstr = utils::split(str, ',', STRIP_SPACES);
	const int fields_per_settings = 3;
	if (vstr.empty() || vstr.size() % fields_per_settings != 0) {
		return ret;
	}

	std::set<std::string> parsed_url;
	bool has_error = false;
	int at = 0;
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); at ++) {
		const std::string name = *it;
		++ it;

		bool require_check = check_0 || at != 0;
		const std::string tcp_str = utils::lowercase(*it);
		bool tcp = true;
		if (require_check) {
			if (tcp_str == "tcp") {
				tcp = true;
			} else if (tcp_str == "udp") {
				tcp = false;
			} else {
				has_error = true;
				break;
			}
		}
		++ it;

		std::string url = *it;
		if (require_check) {
			std::replace(url.begin(), url.end(), ' ', '+');
			if (url.find("rtsp://") != 0 || (unique_url && parsed_url.count(url) > 0)) {
				has_error = true;
				break;
			}
			GURL gurl(url);
			if (!gurl.is_valid()) {
				has_error = true;
				break;
			}
		}
		parsed_url.insert(url);
		++ it;

		ret.push_back(trtsp_settings(name, tcp, url));
	}
	if (has_error) {
		ret.clear();
	}
	return ret;
}

std::string index_2_tstr(int index)
{
    if (index == 0) {
        return _("zero");
    } else if (index == 1) {
        return _("first");
    } else if (index == 2) {
        return _("second");
    } else if (index == 3) {
        return _("third");
    } else if (index == 4) {
        return _("fourth");
    } else if (index == 5) {
        return _("fifth");
    } else if (index == 6) {
        return _("sixth");
    } else if (index == 7) {
        return _("seventh");
    } else if (index == 8) {
        return _("eighth");
    } else if (index == 9) {
        return _("ninth");
    } else if (index == 10) {
        return _("tenth");
    }
    return "N";
}

} // end namespace utils
