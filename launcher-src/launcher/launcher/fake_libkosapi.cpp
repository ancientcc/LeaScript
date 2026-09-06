/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <SDL_log.h>
#include <utility>

// #include <kosapi/camera.h>
#include <kosapi/gui.h>
// #include <kosapi/mediacodec.h>
#include <kosapi/net.h>
#include <kosapi/sys.h>

#include "rose_string_utils.hpp"
#include "rose_exception.hpp"

#include "game_config.hpp"


#ifdef FAKE_LIBKOSAPI_SO
// This file is impletement of fake libkosapi.so
// Used to obtain mediapipe time without libkosapi.so.

// Do not use remote desktop during operation

// ---windows---
// don't compile <4librose>/libkosapi/camera2.cpp, gui2.cpp, net2.cpp, sys2.cpp
 
// ---android---
// remove '-lkosapi' from 'LOCAL_LDLIBS'

// 
//
// for <kosapi/net.h>
//
static fkosNetReceiveBroadcast kosNetReceiveBroadcast = nullptr;
static void* kosNetReceiveBroadcastUser = nullptr;

void kosNetSetReceiveBroadcast(fkosNetReceiveBroadcast did, void* user)
{
	kosNetReceiveBroadcast = did;
	kosNetReceiveBroadcastUser = user;
}

int kosNetSendMsg(const char* msg, char* result, int maxBytes)
{
	VALIDATE(false, "must not call");
	return 0;
}

void kosNetGetCfg(char* result, int max_bytes)
{
	VALIDATE(false, "must not call");
}

bool kosNetSetCfg(const char* msg)
{
	VALIDATE(false, "must not call");
	return true;
}

//
// for <kosapi/gui.h>
//
int kosRecordScreenLoop(uint32_t bitrate_kbps, uint32_t max_fps_to_encoder, uint8_t* pixel_buf, fdid_gui2_screen_captured did, void* user)
{
	VALIDATE(false, "must not call");
	return 0;
}

void kosStopRecordScreen()
{
	VALIDATE(false, "must not call");
}

void kosPauseRecordScreen(bool pause)
{
	VALIDATE(false, "must not call");
}

bool kosRecordScreenPaused()
{
	VALIDATE(false, "must not call");
	return true;
}

void kosGetDisplayInfo(KosDisplayInfo* info)
{
	memset(info, 0, sizeof(KosDisplayInfo));

	SDL_Point screen_size = SDL_Point{1920, 1080};
	info->w = screen_size.x;
	info->h = screen_size.y;
	info->orientation = KOS_DISPLAY_ORIENTATION_0;

	info->xdpi = 11.23f;
	info->ydpi = 12.24f;
	info->fps = 25.1f;
	info->density = 23.12f;
	info->secure = true;
}

//
// for <kosapi/sys.h>
//
void kosGetVersion(char* ver, int max_bytes)
{
    strcpy(ver, "1.0.3-20220801");
}

static int screen_width = 0;
static int screen_height = 0;
bool kosCreateInput(bool keyboard, int _screen_width, int _screen_height)
{
    VALIDATE(false, "must not call");
    return true;
}

void kosDestroyInput()
{
    VALIDATE(false, "must not call");
}

uint32_t kosSendInput(uint32_t input_count, KosInput* inputs)
{
    VALIDATE(false, "must not call");
    return 0;
}

#endif