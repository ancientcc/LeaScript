/* $Id: video.cpp 46847 2010-10-01 01:43:16Z alink $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>


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
 *  Video-testprogram, standalone
 */
#define GETTEXT_DOMAIN "rose-lib"

#include "rose_global.hpp"

#include "font.hpp"
#include "image.hpp"
#include "preferences.hpp"
#include "preferences_display.hpp"
#include "sdl_utils.hpp"
#include "video.hpp"
#include "gettext.hpp"
#include "base_instance.hpp"
#include <boost/foreach.hpp>
#include <vector>
#include <map>
#include <algorithm>

namespace {
	bool fullScreen = false;
	// if 'windows_use_opengl == true', SDL will 'opengles2' renderer. else use "direct3d".
	// use 'opengles2', you can understand opengles2 logic on windows. 
	// But it will appear 'white' when displayed on win10-22H2.
	bool windows_use_opengl = true;
}

static unsigned int get_flags(unsigned int flags)
{
	if (!game_config::mobile) {
		if (!(flags & SDL_WINDOW_FULLSCREEN)) {
			flags |= SDL_WINDOW_RESIZABLE;
		}
	}

	flags |= SDL_WINDOW_ALLOW_HIGHDPI;

	if (game_config::os == os_windows && windows_use_opengl) {
		flags |= SDL_WINDOW_OPENGL;
	}

	return flags;
}

namespace {
struct event {
	int x, y, w, h;
	bool in;
	event(const SDL_Rect& rect, bool i) : x(i ? rect.x : rect.x + rect.w), y(rect.y), w(rect.w), h(rect.h), in(i) { }
};
bool operator<(const event& a, const event& b) {
	if (a.x != b.x) return a.x < b.x;
	if (a.in != b.in) return a.in;
	if (a.y != b.y) return a.y < b.y;
	if (a.h != b.h) return a.h < b.h;
	if (a.w != b.w) return a.w < b.w;
	return false;
}
bool operator==(const event& a, const event& b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.in == b.in;
}

struct segment {
	int x, count;
	segment() : x(0), count(0) { }
	segment(int x, int count) : x(x), count(count) { }
};

}

namespace {
Uint32 current_format = SDL_PIXELFORMAT_UNKNOWN;
SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;
texture frameTexture = NULL;
texture whiteTexture;
int frame_width = 0;
int frame_height = 0;
}

SDL_Renderer* get_renderer()
{
	return renderer;
}

texture& get_screen_texture()
{
	return frameTexture;
}

texture& get_white_texture()
{
	return whiteTexture;
}

int frameTexture_width()
{
	return frame_width;
}

int frameTexture_height()
{
	return frame_height;
}

SDL_Rect frameTexture_rect()
{
	return create_rect(0, 0, frame_width, frame_height);
}

SDL_Window* get_sdl_window()
{
	return window;
}


static void clear_textures()
{
	image::flush_cache(true);
	if (frameTexture.get() == NULL) {
		return;
	}
	VALIDATE(frameTexture.use_count() == 1 && whiteTexture.use_count() == 1, null_str);

	frameTexture = NULL;
	whiteTexture = NULL;
}

const SDL_PixelFormat& get_screen_format()
{
	return get_neutral_pixel_format();
}

SDL_Point SDL_GetNotchSizeRose()
{
	VALIDATE(window != nullptr, null_str);
	SDL_Point ret = SDL_GetNotchSize(window);
	if (game_config::os == os_ios && ret.y > 0) {
		// on ios, statusbar height is 0 when full-screen. but app call often SDL_GetNotchSize when full-screen.
		// here fill right value.
		ret.x = game_config::statusbar_height;
	}
	return ret;
}

void SDL_RaiseWindow2()
{
	if (game_config::os != os_windows) {
		return;
	}
	if (window == nullptr) {
		return;
	}
	// 1. Check whether the current window already has input focus.
    uint32_t flags = SDL_GetWindowFlags(window);
    if (flags & SDL_WINDOW_INPUT_FOCUS) {
        // SDL_Log("Already in the active state, no action required.");
        return;
    }
    
    // First, raise the window above all other windows.
    SDL_RaiseWindow(window);
    
    // Explicitly request input focus.
    SDL_SetWindowInputFocus(window);
/*
    // Optional: Check the flag again to confirm whether the operation succeeded.
    flags = SDL_GetWindowFlags(get_sdl_window());
    if (flags & SDL_WINDOW_INPUT_FOCUS) {
         SDL_Log("Window successfully activated.");
    } else {
         SDL_Log("Warning: The window failed to gain focus, possibly restricted by the operating system.");
    }
*/
}

ttray_window::~ttray_window()
{
	VALIDATE(window == nullptr, null_str);
	VALIDATE(renderer2 == nullptr, null_str);
}

void ttray_window::create(SDL_Window& parent, int x, int y, int w, int h)
{
	VALIDATE(!valid(), null_str);

	window = SDL_CreateFloatWindow(
        NULL,
        x, y, w, h,
        SDL_WINDOW_TOOLTIP |           
        SDL_WINDOW_BORDERLESS |        
        SDL_WINDOW_ALWAYS_ON_TOP |     
        SDL_WINDOW_SKIP_TASKBAR |      
        // SDL_WINDOW_SHOWN,
		// To avoid a momentary mouse-focus change during startup, 
		// it is initially set to hidden. Accordingly, the underlying should 'is_hidden = true'.
		SDL_WINDOW_HIDDEN,
        &parent
    );
    
	VALIDATE(window != nullptr, null_str);

    // create renderer
    // renderer2 = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	renderer2 = SDL_CreateRenderer(window, -1, 0);
	VALIDATE(renderer2 != nullptr, null_str);

	bool use_blend = false;
	if (use_blend) {
		SDL_SetRenderDrawBlendMode(renderer2, SDL_BLENDMODE_BLEND);
	}

    width = w;
    height = h;
	create_frameTexture2(width, height);
    dragging = false;
	is_hidden = true;

	tray::window_id = SDL_GetWindowID(window);
}

void ttray_window::close()
{
	if (!valid()) {
		return;
	}
	VALIDATE(frameTexture2.use_count() == 1, null_str);
	frameTexture2 = nullptr;

	SDL_DestroyRenderer(renderer2);
	renderer2 = nullptr;
	SDL_DestroyWindow(window);
	window = nullptr;

	tray::window_id = nposm;
}

bool ttray_window::valid() const 
{
	if (window != nullptr) {
		VALIDATE(renderer2 != nullptr, null_str);
		return true;
	}
	VALIDATE(renderer2 == nullptr, null_str);
	return false;
}

void ttray_window::set_slot(ttray_window::tslot* slot)
{
	if (slot != nullptr) {
		VALIDATE(slot_ == nullptr, null_str);

	} else {
		VALIDATE(slot_ != nullptr, null_str);
	}
	slot_ = slot;
}

void ttray_window::flip()
{	
	// copy from 'CVideo::flip()'
	texture null_tex;
	trender_target_lock lock(renderer2, null_tex);

	SDL_RenderCopy(renderer2, frameTexture2.get(), NULL, NULL);

	SDL_RenderPresent(renderer2);
}

void ttray_window::create_frameTexture2(int w, int h)
{
	// copy from 'create_frameTexture'
	VALIDATE(current_format == SDL_PIXELFORMAT_ARGB8888, null_str);
	VALIDATE(renderer2 != nullptr, null_str);

	frameTexture2 = SDL_CreateTexture(renderer2, current_format, SDL_TEXTUREACCESS_TARGET, w, h);	
	VALIDATE(frameTexture2.get() != NULL, null_str);
	SDL_SetRenderTarget(renderer2, frameTexture2.get());
	SDL_RenderSetFrameTexture(renderer2, frameTexture2.get());

	bool use_blend = false;
	if (use_blend) {
		SDL_SetTextureBlendMode(frameTexture2.get(), SDL_BLENDMODE_BLEND);
	} else {
		SDL_SetTextureBlendMode(frameTexture2.get(), SDL_BLENDMODE_NONE);
	}
}

namespace tray
{
int window_id = nposm;
ttray_window* window = nullptr;
}

CVideo::CVideo()
	: updatesLocked_(0)
	// fix rdp client is android, delay bug
	, flip_only_dirty_(game_config::os != os_android)
	, force_render_present_(0)
{
	tray::window = &tray_window_;

	// on ios and android, disable screensaver will result to disable entering idle.
	// in order to control idle, advice to use SDL_HINT_IDLE_TIMER_DISABLED.
	SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

	// on android, rose want Android_PumpEvents_NonBlocking always.
	SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "false");

	// on android, use AVAudioSessionCategoryPlayback as default category.
	// SDL's default category is AVAudioSessionCategoryAmbient, it maybe result no-sound.
	SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "AVAudioSessionCategoryPlayback");

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	if (game_config::os == os_windows) {
		if (windows_use_opengl) {
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
			// SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
		}
	} else if (game_config::os == os_ios) {
		// on ios, SDL's default is METAL_RenderDriver.
		// in 'metal' driver, 2th libyuv::I420ToARGB(...) in trtc_client::VideoRenderer::OnFrame will ACCESS_BAD_ADDRESS.
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
	}

	initSDL();
}

CVideo::~CVideo()
{
	SDL_Log("CVideo::~CVideo()---");
	if (tray_window_.valid()) {
		tray_window_.close();
	}

	// since rendere will be destroy, texture associated it should be release.
	clear_textures();
	if (renderer != nullptr) {
		SDL_DestroyRenderer(renderer);
	}
	// SDL_DestroyTexture/SDL_DestroyRenderer requrie EGLContext, and SDL_DestroyWindow will delete EGLContext
	// so call SDL_DestroyWindow after them.
	if (window != nullptr) {
		SDL_DestroyWindow(window);
		window = NULL;
	}
	SDL_Quit();

	SDL_Log("---CVideo::~CVideo() X");
}

void CVideo::initSDL()
{
	const int res = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE | SDL_INIT_SENSOR);
	if (res < 0) {
		throw CVideo::error();
	}

	int sensorCount = SDL_NumSensors();
    SDL_Log("Found %d sensors", sensorCount);
/*
	// In some cases, it is necessary to receive SDL_TEXTINPUT before popup soft-keyboard. 
	// For example, as soon as enter the window, need to swipe QR code
	if (SDL_GetEventState(SDL_TEXTINPUT) != SDL_ENABLE) {
		// in SDL, ScreenKeyboard is soft keyboard.
		// to non-screenkeyboard(for exmaple windows), SDL_VideoInit(SDL_video.c) is call it by default
		SDL_EventState(SDL_TEXTINPUT, SDL_ENABLE);
	}
*/
}

void CVideo::create_frameTexture(int w, int h, bool reset_image_zoom)
{
	VALIDATE(current_format == SDL_PIXELFORMAT_ARGB8888, null_str);
	VALIDATE(renderer != nullptr, null_str);

	frame_width = w;
	frame_height = h;
	frameTexture = SDL_CreateTexture2(renderer, current_format, SDL_TEXTUREACCESS_TARGET, w, h);	
	VALIDATE(frameTexture.get() != NULL, null_str);
	SDL_SetRenderTarget(renderer, frameTexture.get());
	SDL_RenderSetFrameTexture(renderer, frameTexture.get());

	// white texture. use to brighten.
	surface surf = image::get_image("misc/bg_ffffff.png");
	whiteTexture = SDL_CreateTextureFromSurface2(renderer, surf);

	SDL_SetTextureBlendMode(frameTexture.get(), SDL_BLENDMODE_NONE);
	image::set_pixel_format(get_neutral_pixel_format());
	game_config::svga = frame_width >= 800 * gui2::twidget::hdpi_scale && frame_height >= 600 * gui2::twidget::hdpi_scale;
	if (reset_image_zoom) {
		int zoom = preferences::zoom();
		image::set_zoom(zoom);
	}

	// SDL_Texture* bug_tex = SDL_CreateTexture2(renderer, current_format, SDL_TEXTUREACCESS_TARGET, w, h);	
}

bool CVideo::setMode(int w, int h, int flags)
{
	tcamera& camera = instance->camera();
	if (camera.is_avcapture_started()) {
		VALIDATE(!camera.tasking(), null_str);
		camera.stop_avcapture();
	}

	bool reset_image_zoom = frameTexture.get()? false: true;

	flags = get_flags(flags);

	if (current_format == SDL_PIXELFORMAT_UNKNOWN) {
		current_format = SDL_PIXELFORMAT_ARGB8888;
	}

	fullScreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
	instance->pre_create_renderer();

	// since rendere will be destroy, texture associated it should be release.
	clear_textures();
	if (renderer != nullptr) {
		SDL_DestroyRenderer(renderer);
		renderer = nullptr;
	}

	// SDL_DestroyTexture/SDL_DestroyRenderer requrie EGLContext, and SDL_DestroyWindow will delete EGLContext
	// so call SDL_DestroyWindow after them.
	if (window) {
		SDL_DestroyWindow(window);
		window = nullptr;
	}

	int x = game_config::os != os_ios? SDL_WINDOWPOS_UNDEFINED: 0;
	int y = game_config::os != os_ios? SDL_WINDOWPOS_UNDEFINED: 0;

	SDL_Log("setMode, (%i, %i, %i, %i)", x, y, w, h);
	if (game_config::os == os_ios) {
		VALIDATE((flags & SDL_WINDOW_BORDERLESS) == 0, null_str);

	} else if (game_config::os == os_android) {
		VALIDATE((flags & SDL_WINDOW_FULLSCREEN) == 0, null_str);

	} else if (game_config::os == os_windows) {
		if (flags & SDL_WINDOW_FULLSCREEN) {
			VALIDATE((flags & SDL_WINDOW_MAXIMIZED) == 0, null_str);
		} else if (flags & SDL_WINDOW_MAXIMIZED) {
			VALIDATE((flags & SDL_WINDOW_FULLSCREEN) == 0, null_str);
		}
	}
	VALIDATE(window == nullptr && renderer == nullptr, null_str);
	window = SDL_CreateWindow(game_config::get_app_msgstr(null_str).c_str(), x, y, w, h, flags);
    if (game_config::os == os_windows) {
		game_config::statusbar_height = SDL_GetStatusBarHeight(window);
	}
	if (game_config::corner_height == nposm) {
		game_config::corner_height = SDL_GetNotchSizeRose().y;
	}

	rose_dbg_texture.reset();
	renderer = SDL_CreateRenderer(window, -1, 0);
/*
#if (defined(__APPLE__) && TARGET_OS_IPHONE)
	{
		// Fix Bug!
		int w2, h2;
		SDL_GetWindowSize(window, &w2, &h2);
		SDL_Log("setMode, create size: %ix%i, requrie: %ix%i", w2, h2, w, h);

		SDL_SetWindowSize(window, w, h);
	}
#endif
*/
	// for android os, status bar effect desired and actual size.
	// as if window, if desire w or h is too large, sdl will downsize automaticly.
	int new_w = 0;
	int new_h = 0;
	SDL_GetWindowSize(window, &new_w, &new_h);

	w = posix_align_floor(new_w, gui2::hdpi_scale_4_align_floor(gui2::twidget::hdpi_scale));
	h = posix_align_floor(new_h, gui2::hdpi_scale_4_align_floor(gui2::twidget::hdpi_scale));
	if (w != new_w || h != new_h) {
		SDL_Log("hdpi_scale: %.5f, w(%i) != new_w(%i) || h(%i) != new_h(%i), call SDL_SetWindowSize(w, h)", gui2::twidget::hdpi_scale,
			w, new_w, h, new_h);
		// VALIDATE(game_config::os == os_windows, null_str);
		// on ms-windows/android, if desire w or h is too large, sdl will downsize automaticly.
		// result size maybe not divided by hdpi_scale.
		// on android, height of bottom toolbar maybe not divied by hdpi_scale. and new_h is get rid of it. for example 1920x997.
		// (hdpi_scale = 2)1920x997 => 1920x996
		SDL_SetWindowSize(window, w, h);
	} else {
		SDL_Log("hdpi_scale: %.5f, w(%i) == new_w(%i) || h(%i) == new_h(%i)", gui2::twidget::hdpi_scale,
			w, new_w, h, new_h);
	}

	create_frameTexture(w, h, reset_image_zoom);

	instance->post_create_renderer();

	return true;
}

int CVideo::getx() const
{
	return frame_width;
}

int CVideo::gety() const
{
	return frame_height;
}

SDL_Rect CVideo::bound() const
{
	SDL_Rect rc;
	int display = window? SDL_GetWindowDisplayIndex(window): 0;
	SDL_GetDisplayBounds(display, &rc);
	return rc;
}

void CVideo::sdl_set_window_size(int width, int height)
{
	SDL_SetWindowSize(window, width, height);
}

const SDL_PixelFormat& CVideo::getformat() const
{
	return get_neutral_pixel_format();
}

void CVideo::set_force_render_present() 
{ 
	const uint32_t threshold = 5; 
	force_render_present_ = threshold; 
}

void CVideo::flip()
{
	// static uint32_t last_verbose_ticks = 0;
	// const int threshold = 3000;
	const uint32_t now = SDL_GetTicks();

    // when enable background audio, will enter it during background.
    if (!instance->foreground()) {
/*
		if (now >= last_verbose_ticks + threshold) {
			SDL_Log("%u CVideo::flip(), background, do nothing", now);
			last_verbose_ticks = now;
		}
*/
        return;

    } /* else if (now >= last_verbose_ticks + threshold) {
		SDL_Log("%u CVideo::flip(), foreground", now);
		last_verbose_ticks = now;
	}
*/
	// on android, switch foreground/background will result to black-screen or vertical-flip.
	if (!SDL_RenderGetFrameTextureDirty(renderer, SDL_TRUE) && flip_only_dirty_) {
		if (force_render_present_ == 0) {
			return;
		} else {
			// SDL_Log("%u CVideo::flip, frame_dirty is false, force_render_present(%u) result SDL_RenderPresent", 
			//	SDL_GetTicks(), force_render_present_);
		}
	}
	if (force_render_present_ != 0) {
		force_render_present_ --;
	}

	// static uint32_t time = 0;
	// SDL_Log("[#%u]%u CVideo::flip, SDL_RenderCopy", time ++, SDL_GetTicks());

	texture null_tex;
	trender_target_lock lock(renderer, null_tex);
	SDL_RenderCopy(renderer, frameTexture.get(), NULL, NULL);

	SDL_RenderPresent(renderer);
}

void CVideo::lock_updates(bool value)
{
	if (value == true) {
		++ updatesLocked_;
	} else {
		-- updatesLocked_;
	}
}

bool CVideo::update_locked() const
{
	return updatesLocked_ > 0;
}

texture& CVideo::getTexture()
{
	return frameTexture;
}

SDL_Window* CVideo::getWindow()
{
	return window;
}

bool CVideo::isFullScreen() const { return fullScreen; }
