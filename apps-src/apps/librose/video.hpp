/* $Id: video.hpp 47615 2010-11-21 13:57:02Z mordante $ */
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
#ifndef VIDEO_HPP_INCLUDED
#define VIDEO_HPP_INCLUDED

#include "events.hpp"
#include "exceptions.hpp"
#include "lua_jailbreak_exception.hpp"

// #include <boost/utility.hpp>
#include "rose_sdl_utils.hpp"

// class surface;
// class texture;

// In SDL, this window uses SDL_WINDOW_TOOLTIP. 
// From the user's perspective, it's called a floating window. 
// I want this window to be associated with tray usage ¡ª meaning it will only appear when the tray is in use. 
// Therefore, it's called the tray window.
struct ttray_window
{
public:
	struct tslot
	{
		virtual void tray_handle_WINDOWEVENT(const SDL_WindowEvent& windowevt) {}
		virtual void tray_handle_MOUSEMOTION(int x, int y) {}
		virtual void tray_handle_MOUSEBUTTONDOWN(int x, int y) {}
		virtual void tray_handle_MOUSEBUTTONUP(int x, int y) {}
	};


	ttray_window()
		: window(nullptr)
		, renderer2(nullptr)
		, frameTexture2(nullptr)
		, width(0)
		, height(0)
		, dragging(false)
		, offset({0, 0})
		, is_hidden(false)
		, slot_(nullptr)
	{}

	~ttray_window();

	void create(SDL_Window& parent, int x, int y, int w, int h);
	void close();
	bool valid() const;

	void set_slot(tslot* slot);

	void handle_WINDOWEVENT(const SDL_WindowEvent& windowevt)
	{
		if (slot_ != nullptr) {
			slot_->tray_handle_WINDOWEVENT(windowevt);
		}
	}

	void handle_MOUSEMOTION(int x, int y) 
	{	 
		if (slot_ != nullptr) {
			slot_->tray_handle_MOUSEMOTION(x, y);
		}
	}
	void handle_MOUSEBUTTONDOWN(int x, int y)
	{	 
		if (slot_ != nullptr) {
			slot_->tray_handle_MOUSEBUTTONDOWN(x, y); 
		}
	}
	void handle_MOUSEBUTTONUP(int x, int y)
	{	 
		if (slot_ != nullptr) {
			slot_->tray_handle_MOUSEBUTTONUP(x, y); 
		}
	}

	void flip();

private:
	// Many functions or variables have the suffix '2' added,
	// to distinguish them from those used by the get_sdl_window().
	void create_frameTexture2(int w, int h);

public:
    SDL_Window* window;
    SDL_Renderer* renderer2;
	texture frameTexture2;
    int width;
    int height;
    bool dragging;
	SDL_Point offset;
	bool is_hidden;

private:
	tslot* slot_;
};

namespace tray
{
extern int window_id;
extern ttray_window* window;
}

texture& get_screen_texture();
texture& get_white_texture();
int frameTexture_width();
int frameTexture_height();
SDL_Rect frameTexture_rect();
SDL_Window* get_sdl_window();
const SDL_PixelFormat& get_screen_format();
SDL_Point SDL_GetNotchSizeRose();
void SDL_RaiseWindow2();

class CVideo
{
public:
	CVideo();
	~CVideo();

	posix_noncopyable(CVideo);

	void create_frameTexture(int w, int h, bool reset_image_zoom);
	bool setMode(int x, int y, int flags);

	//functions to get the dimensions of the current video-mode
	int getx() const;
	int gety() const;
	SDL_Rect bound() const;

	ttray_window& tray_window() { return tray_window_; }

	void sdl_set_window_size(int width, int height);
	void flip();

	texture& getTexture();
	SDL_Window* getWindow();
	const SDL_PixelFormat& getformat() const;

	bool isFullScreen() const;
	void set_force_render_present();

	struct error : public game::error
	{
		error() : game::error("Video initialization failed") {}
	};

	class quit
		: public lua_jailbreak_exception
	{
	public:

		quit()
			: lua_jailbreak_exception()
		{
		}

	private:

		IMPLEMENT_LUA_JAILBREAK_EXCEPTION(quit)
	};

	void set_flip_only_dirty(bool value) { flip_only_dirty_ = value; }
	bool flip_only_dirty() const { return flip_only_dirty_; }

	//function to stop the screen being redrawn. Anything that happens while
	//the update is locked will be hidden from the user's view.
	//note that this function is re-entrant, meaning that if lock_updates(true)
	//is called twice, lock_updates(false) must be called twice to unlock
	//updates.
	void lock_updates(bool value);
	bool update_locked() const;

private:
	void initSDL();

private:
	int updatesLocked_;
	bool flip_only_dirty_;
	uint32_t force_render_present_;
	ttray_window tray_window_;
};

#endif
