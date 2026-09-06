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

#ifndef LIBROSE2_STRING_UTILS_HPP_INCLUDED
#define LIBROSE2_STRING_UTILS_HPP_INCLUDED

#include "rose_string_utils_dll.hpp"
#include <algorithm>
#include <map>
#include <set>

#include <SDL_types.h>


namespace utils {

/** Prepends a configurable set of characters with a backslash */
std::string escape(const std::string &str, const char *special_chars);

/**
 * Prepend all special characters with a backslash.
 *
 * Special characters are:
 * #@{}+-,\*=
 */
inline std::string escape(const std::string &str)
{ return escape(str, "#@{}+-,\\*="); }

/** Remove all escape characters (backslash) */
std::string unescape(const std::string &str);

// #define BOM_LENGTH		3
// bool bom_magic_started(const uint8_t* data, int size);
const char* skip_blank_characters(const char* start);
const char* skip_blank_lines(const char* start, int lines);
const char* until_blank_characters(const char* start, bool include_space);
const char* until_c_style_characters(const char* start);

std::string cstr_2_str(const char* cstr);

}

#endif
