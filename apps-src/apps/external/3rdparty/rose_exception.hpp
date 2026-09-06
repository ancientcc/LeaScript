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

#ifndef LIBROSE2_UTILS_HPP_INCLUDED
#define LIBROSE2_UTILS_HPP_INCLUDED

#include "rose_util.hpp"

typedef void (*fnros_wml_exception)(const char* cond, const char* file, const int line, const char *function, const std::string& message);

DECLSPEC void rose_set_wml_exception(fnros_wml_exception fexception);
// DECLSPEC fnros_wml_exception rose_fwml_exception();

// don't dllexport it.
extern LIB3RDPARTY_DECL fnros_wml_exception s_fwml_exception;


#ifndef __func__
 #ifdef __FUNCTION__
  #define __func__ __FUNCTION__
 #endif
#endif

#define VALIDATE(cond, message)                                           \
	do {                                                                  \
		if (!(cond) && s_fwml_exception != nullptr) {                \
            fnros_wml_exception f = s_fwml_exception;                \
			f(#cond, __FILE__, __LINE__, __func__, message);              \
		}                                                                 \
	} while(0)

#define IN_MAIN_THREAD()	(SDL_ThreadID() == main_tid)

#define VALIDATE_IN_MAIN_THREAD()                                   \
	do {                                                                  \
		if (!IN_MAIN_THREAD()) {                \
            fnros_wml_exception f = s_fwml_exception;                \
			std::stringstream err;										\
			err << "main_tid:" << main_tid << " this_tid:" << SDL_ThreadID();	\
			f("VALIDATE_IN_MAIN_THREAD", __FILE__, __LINE__, __func__, err.str().c_str());              \
		}                                                                 \
	} while(0)

#define VALIDATE_NOT_MAIN_THREAD()                                   \
	do {                                                                  \
		if (IN_MAIN_THREAD()) { \
            fnros_wml_exception f = s_fwml_exception; \
			std::stringstream err; \
			err << "main_tid:" << main_tid << " this_tid:" << SDL_ThreadID(); \
			f("VALIDATE_NOT_MAIN_THREAD", __FILE__, __LINE__, __func__, err.str().c_str());              \
		}                                                                 \
	} while(0)

#define VALIDATE_IN_THIS_THREAD(tid)                                   \
	do {                                                                  \
		if ((tid) != SDL_ThreadID()) { \
            fnros_wml_exception f = s_fwml_exception; \
			std::stringstream err; \
			err << "desire_tid:" << (tid) << " this_tid:" << SDL_ThreadID(); \
			f("VALIDATE_IN_THIS_THREAD", __FILE__, __LINE__, __func__, err.str().c_str());              \
		}                                                                 \
	} while(0)


#endif

