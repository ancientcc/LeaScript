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

#ifndef LIBROS_APLT_COMMON_HPP
#define LIBROS_APLT_COMMON_HPP


#include "rose_util.hpp"
#include "config.hpp"
#include <string>
#include <memory>
#include <SDL.h>

#ifdef _WIN32
#define LIBROSEAPLT_SO	"libroseaplt.dll"
#define LIBROSEAPLT2_SO	"libroseaplt2.dll"
#define LIBROSEAPLT3_SO	"libroseaplt3.dll"
#define LIBROSEAPLT4_SO	"libroseaplt4.dll"
#else
#define LIBROSEAPLT_SO	"libroseaplt.so"
#define LIBROSEAPLT2_SO	"libroseaplt2.so"
#define LIBROSEAPLT3_SO	"libroseaplt3.so"
#define LIBROSEAPLT4_SO	"libroseaplt4.so"
#endif	

enum {apltsotype_base, apltsotype_laser, apltsotype_moveit, apltsotype_dcamera, apltsotype_iot, apltsotype_speech, apltsotype_ai, apltsotype_maxdriver = apltsotype_ai, 
	apltsotype_fgaplt, apltsotype_base2th, apltsotype_aiagent_task, apltsotype_count};

typedef void (*faplt_load)(int src, const char* bundleid);
extern "C" DECLSPEC void aplt_load(int src, const char* bundleid);

typedef void (*faplt_unload)();
extern "C" DECLSPEC void aplt_unload();

// @driver_type[IN]
// @path[OUT]: this type serial path. Attation: Isn't node.
// @maxlen: always 260(MAX_PATH)(include '\0'). I think there must not be path > 260 bytes.
// @return value: this type serial baudrate. 

typedef void (*faplt_serial_driver_main)(bool& exit, const char* node, int baudrate);

typedef void* (*faplt_create_base_slot)(const char* sn, const char* cpuid);
extern "C" DECLSPEC void* aplt_create_base_slot(const char* sn, const char* cpuid);

typedef void* (*faplt_create_moveit_slot)();
extern "C" DECLSPEC void* aplt_create_moveit_slot();

typedef void* (*faplt_create_laser_slot)();
extern "C" DECLSPEC void* aplt_create_laser_slot();

typedef void* (*faplt_create_dcamera_slot)();
extern "C" DECLSPEC void* aplt_create_dcamera_slot();

typedef void* (*faplt_create_iot_slot)(void* subscriber);
extern "C" DECLSPEC void* aplt_create_iot_slot(void* subscriber);

typedef void* (*faplt_create_speech_slot)(void* subscriber);
extern "C" DECLSPEC void* aplt_create_speech_slot(void* subscriber);

typedef void* (*faplt_create_ai_slot)(void* subscriber);
extern "C" DECLSPEC void* aplt_create_ai_slot(void* subscriber);


typedef void* (*faplt_create_task_api)(void* aplt1);
extern "C" DECLSPEC void* aplt_create_task_api(void* aplt1);

#endif