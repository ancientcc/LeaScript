/* $Id: sdl_utils.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

/** @file */

#ifndef SDL_UTILS_INCLUDED
#define SDL_UTILS_INCLUDED

#include "rose_sdl_utils.hpp"
// #include "serialization/string_utils.hpp"
// #include "wml_exception.hpp"

class trose_dbg_texture
{
public:
	struct titem_C
	{
		bool is_surf;
		const SDL_Texture* tex;
		int w;
		int h;
		char file[64];
		char function[32];
		int line;
	};

	trose_dbg_texture(const std::string& key)
		: items_(nullptr)
		, item_size_(0)
		, item_vsize_(0)
		, file_start_key_(key)
	{}

	~trose_dbg_texture();

	SDL_Texture* CreateTexture(SDL_Renderer* renderer, Uint32 format, int access, int w, int h, 
		const std::string& file, const std::string& func, int line)
	{
		SDL_Texture* tex = SDL_CreateTexture(renderer, format, access, w, h);
		insert_texture(false, tex, w, h, file, func, line);
		return tex;
	}

	SDL_Texture* CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface, const std::string& file, const std::string& func, int line)
	{
		VALIDATE(surface != nullptr, null_str);
		SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
		insert_texture(true, tex, surface->w, surface->h, file, func, line);
		return tex;
	}

	void did_DestroyTexture(const SDL_Texture* tex)
	{
		erase_texture(tex);
	}

	void log_items(const std::string& scene) const;

	void reset();

private:
	void resize_items(int size)
	{
		size = posix_align_ceil(size, 32);
		VALIDATE(size >= 0, null_str);

		if (size > item_size_) {
			titem_C* tmp = (titem_C*)malloc(size * sizeof(titem_C));
			if (items_ != nullptr) {
				if (items_ != nullptr) {
					VALIDATE(item_vsize_ != 0, null_str);
					memcpy(tmp, items_, item_vsize_ * sizeof(titem_C));
				}
				free(items_);
			}
			items_ = tmp;
			item_size_ = size;
		}
	}

	void insert_texture(bool is_surf, const SDL_Texture* tex, int w, int h, const std::string& file, const std::string& func, int line);
	void erase_texture(const SDL_Texture* tex);

public:
	const std::string file_start_key_;
	titem_C* items_;
	int item_size_;
	int item_vsize_;
};

extern trose_dbg_texture rose_dbg_texture;

// For 'launcher', in addition to the main window, there is also a tray window, 
// and debugging cannot be started. Those with only the main window can enable ENABLE_ROSE_DBG_TEXTURE.
// #define ENABLE_ROSE_DBG_TEXTURE
#if defined(_WIN32) && defined(ENABLE_ROSE_DBG_TEXTURE)
#define SDL_CreateTexture2(renderer, format, access, w, h)	\
	rose_dbg_texture.CreateTexture(renderer, format, access, w, h, __FILE__, __FUNCTION__, __LINE__)

#define SDL_CreateTextureFromSurface2(renderer, surface)	\
	rose_dbg_texture.CreateTextureFromSurface(renderer, surface, __FILE__, __FUNCTION__, __LINE__)

#else
#define SDL_CreateTexture2(renderer, format, access, w, h)	\
	SDL_CreateTexture(renderer, format, access, w, h)

#define SDL_CreateTextureFromSurface2(renderer, surface)	\
	SDL_CreateTextureFromSurface(renderer, surface)
#endif

void render_points(SDL_Renderer* renderer, Uint32 argb, const SDL_Point* points, int count);
void render_line(SDL_Renderer* renderer, Uint32 argb, int x1, int y1, int x2, int y2);
void render_rect_frame(SDL_Renderer* renderer, const SDL_Rect& rect, Uint32 argb, int thickness);
void render_rect(SDL_Renderer* renderer, const SDL_Rect& rect, Uint32 argb);
void render_rects(SDL_Renderer* renderer, Uint32 argb, const SDL_Rect* rects, int count);

texture create_neutral_texture(const int w, const int h, const int access);
// I cannot use render_rect(renderer, dstrect, 0x80000000) to get alpha-mask effect, so use below.
texture create_alpha_texture(int width, int height, uint32_t color);
texture create_texture_from_text(const std::string& text, int font_size, const SDL_Color& color);

void render_surface(SDL_Renderer* renderer, const surface& surf, const SDL_Rect* srcrect, const SDL_Rect* dstrect);
void texture_from_texture(const texture& src, texture& dst, const SDL_Rect* srcrect, const int dstwidth, const int dstheight);
texture clone_texture(const texture& src, const uint8_t r = NO_MODULATE_ALPHA, const uint8_t g = NO_MODULATE_ALPHA, const uint8_t b = NO_MODULATE_ALPHA);
void brighten_texture(const texture& tex, const uint8_t r, const uint8_t g, const uint8_t b);
void brighten_renderer(SDL_Renderer* renderer, const uint8_t r, const uint8_t g, const uint8_t b, const SDL_Rect* dstrect);
void grayscale_renderer(SDL_Renderer* renderer);
texture target_texture_from_surface(const surface& surf);

// @path: always append prefix: game_config::preferences_dir + "/"
void imwrite(const texture& tex, const std::string& path);
surface save_texture_to_surface(const texture& tex);

/** Get a portion of the screen.
 *  Send NULL if the portion is outside of the screen.
 *  @param surf              The source surface.
 *  @param rect              The portion of the source surface to copy.
 *  @param optimize_format   Optimize by converting to result to display format.
 *                           Only useful if the source is not the screen and you
 *                           plan to blit the result on screen several times.
 *  @return                  A surface containing the portion of the source.
 *                           No RLE or Alpha bits are set.
 *  @retval 0                if error or the portion is outside of the surface.
 */
surface get_surface_portion(const texture& tex, SDL_Rect &rect);

void blit_integer_surface(int integer, surface& to, int x, int y);
surface generate_integer_surface(int integer);
surface generate_pip_surface(surface& bg, surface& fg);
surface generate_pip_surface(int width, int height, const std::string& bg, const std::string& fg);
surface generate_surface(int width, int height, const std::string& img, int integer, bool greyscale);

class tsurface_blend_none_lock
{
public:
	tsurface_blend_none_lock(const surface& surf)
		: surf_(surf)
	{
		SDL_GetSurfaceBlendMode(surf, &original_);
		if (original_ != SDL_BLENDMODE_NONE) {
			SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE);
		}
	}
	~tsurface_blend_none_lock()
	{
		if (original_ != SDL_BLENDMODE_NONE) {
			SDL_SetSurfaceBlendMode(surf_, original_);
		}
	}

private:
	const surface& surf_;
	SDL_BlendMode original_;
};

class ttexture_blend_none_lock
{
public:
	ttexture_blend_none_lock(const texture& tex)
		: tex_(tex)
	{
		SDL_GetTextureBlendMode(tex.get(), &original_);
		if (original_ != SDL_BLENDMODE_NONE) {
			SDL_SetTextureBlendMode(tex.get(), SDL_BLENDMODE_NONE);
		}
	}
	~ttexture_blend_none_lock()
	{
		if (original_ != SDL_BLENDMODE_NONE) {
			SDL_SetTextureBlendMode(tex_.get(), original_);
		}
	}

private:
	const texture& tex_;
	SDL_BlendMode original_;
};

SDL_Renderer* get_renderer();

class trender_target_lock
{
public:
	trender_target_lock(SDL_Renderer* renderer, const texture& target)
		: renderer_(renderer)
		, original_(SDL_GetRenderTarget(renderer))
		, render_pause_(false)
	{
		// if desire to back-fb, tex is NULL.
		SDL_Texture* tex = target.get();
		
		if (tex) {
			// if want texture to target, this texture must be SDL_TEXTUREACCESS_TARGET.
			int access;
			SDL_QueryTexture(tex, NULL, &access, NULL, NULL);
			VALIDATE(access == SDL_TEXTUREACCESS_TARGET, null_str);
		}

		int result = SDL_SetRenderTarget(renderer, tex);
		if (result != 0) {
			render_pause_ = true;
			SDL_SetRenderAppPause(renderer, SDL_TRUE);
		}
	}
	~trender_target_lock()
	{
		if (!render_pause_) {
			SDL_SetRenderTarget(renderer_, original_);
		} else {
			SDL_SetRenderAppPause(renderer_, SDL_FALSE);
		}
	}

private:
	SDL_Renderer* renderer_;
	SDL_Texture* original_;
	bool render_pause_;
};

class trender_draw_color_lock
{
public:
	trender_draw_color_lock(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
		: renderer_(renderer)
	{
		SDL_GetRenderDrawColor(renderer, &original_r_, &original_g_, &original_b_, &original_a_);
		SDL_SetRenderDrawColor(renderer, r, g, b, a);
	}
	~trender_draw_color_lock()
	{
		SDL_SetRenderDrawColor(renderer_, original_r_, original_g_, original_b_, original_a_);
	}

private:
	SDL_Renderer* renderer_;
	Uint8 original_r_;
	Uint8 original_g_;
	Uint8 original_b_;
	Uint8 original_a_;
};

struct texture_clip_rect_setter
{
	// if r is NULL, clip to the full size of the surface.
	texture_clip_rect_setter(const SDL_Rect* r) 
		: original_()
	{
		SDL_Renderer* renderer = get_renderer();
		SDL_RenderGetClipRect(renderer, &original_);
		SDL_RenderSetClipRect(renderer, SDL_RectEmpty(r)? nullptr: r);
	}

	~texture_clip_rect_setter() 
	{
		SDL_RenderSetClipRect(get_renderer(), SDL_RectEmpty(&original_)? nullptr: &original_);
	}

private:
	SDL_Rect original_;
};

class ttexture_alpha_mod_lock
{
public:
	ttexture_alpha_mod_lock(const texture& tex, const uint8_t alpha)
		: tex_(tex)
	{
		SDL_Texture* t = tex_.get();
		SDL_GetTextureAlphaMod(t, &original_modulation_alpha_);
		if (alpha != original_modulation_alpha_) {
			SDL_SetTextureAlphaMod(t, alpha);
		}
	}
	~ttexture_alpha_mod_lock()
	{
		SDL_Texture* t = tex_.get();
		uint8_t modulation_alpha;
		SDL_GetTextureAlphaMod(t, &modulation_alpha);
		if (modulation_alpha != original_modulation_alpha_) {
			SDL_SetTextureAlphaMod(t, original_modulation_alpha_);
		}
	}

private:
	texture tex_;
	uint8_t original_modulation_alpha_;
};

class ttexture_color_mod_lock
{
public:
	ttexture_color_mod_lock(const texture& tex, const uint8_t r, const uint8_t g, const uint8_t b)
		: tex_(tex)
	{
		SDL_Texture* t = tex_.get();
		SDL_GetTextureColorMod(t, &original_modulation_r_, &original_modulation_g_, &original_modulation_b_);
		if (r != original_modulation_r_ || g != original_modulation_g_ || b != original_modulation_b_) {
			SDL_SetTextureColorMod(t, r, g, b);
		}
	}
	~ttexture_color_mod_lock()
	{
		SDL_Texture* t = tex_.get();
		uint8_t modulation_r, modulation_g, modulation_b;
		SDL_GetTextureColorMod(t, &modulation_r, &modulation_g, &modulation_b);
		if (modulation_r != original_modulation_r_ || modulation_g != original_modulation_g_ || modulation_b != original_modulation_b_) {
			SDL_SetTextureColorMod(t, original_modulation_r_, original_modulation_g_, original_modulation_b_);
		}
	}

private:
	texture tex_;
	uint8_t original_modulation_r_;
	uint8_t original_modulation_g_;
	uint8_t original_modulation_b_;
};

#endif
