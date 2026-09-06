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

#include "rose_string_utils.hpp"

#include <SDL.h>
#include <algorithm>
#include <iomanip>
#include <vector>
#include "rose_exception.hpp"
#include "rose_config_3rdparty.hpp"
#include "gettext.hpp"

namespace utils {

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

// find the pos that non-blank character at it.
const char* skip_blank_characters(const char* start)
{
	while (start[0] == '\r' || start[0] == '\n' || start[0] == '\t' || start[0] == ' ') {
		start ++;
	}
	return start;
}

// find the pos that non-blank character at it.
const char* skip_blank_lines(const char* start, int lines)
{
	VALIDATE(lines > 0, null_str);

	int skiped_line_feeds = 0;
	while (start[0] == '\r' || start[0] == '\n' || start[0] == '\t' || start[0] == ' ') {
		if (start[0] == '\n') {
			skiped_line_feeds ++;
			if (skiped_line_feeds == lines) {
				start ++;
				if (start[0] == '\r' && start[1] != '\n') {
					start ++;
				}
				return start;
			}
		}
		start ++;
	}
	return start;
}

// find the pos that blank character(\r\n\t ) at it.
const char* until_blank_characters(const char* start, bool include_space)
{
	if (include_space) {
		while (start[0] != '\r' && start[0] != '\n' && start[0] != '\0' && start[0] != '\t' && start[0] != ' ') {
			start ++;
		}
	} else {
		while (start[0] != '\r' && start[0] != '\n' && start[0] != '\0' ) {
			start ++;
		}
	}
	return start;
}

// find the pos that c-style word terminate character(\r\n\t ;) at it.
const char* until_c_style_characters(const char* start)
{
	while (start[0] != '\r' && start[0] != '\n' && start[0] != '\t' && start[0] != ' ' && start[0] != ';') {
		start ++;
	}
	return start;
}

// if cstr is NULL, 'return cstr' directly will result 'access violation'.
std::string cstr_2_str(const char* cstr)
{
	return (cstr)? (cstr): null_str;
}

}